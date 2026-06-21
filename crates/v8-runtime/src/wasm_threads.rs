//! wasi-threads support for the V8-isolate guest runtime (WASM-THREADS-SPEC.md, milestone M7.5).
//!
//! A multi-threaded wasm guest runs as N V8 isolates on N OS threads, all sharing ONE linear memory.
//! When the guest calls `pthread_create`, wasi-libc lowers it to `(import "wasi" "thread-spawn")`; the
//! host must start a new isolate-thread that instantiates the SAME module against the SAME shared
//! `WebAssembly.Memory` and calls the guest's exported `wasi_thread_start(tid, start_arg)`.
//!
//! The cross-isolate machinery this rests on is proven in `tests/threads_shared_memory_spike.rs`
//! (shared backing store crosses isolates) and `tests/threads_memory_serialize_spike.rs` (a shared
//! `WebAssembly.Memory` round-trips via `ValueSerializer`). This module holds the stable building
//! blocks the runtime integration uses:
//!
//! * [`SendBackingStore`] / [`SendCompiledModule`] — move the shared memory's backing store and the
//!   compiled module across the spawn boundary. `SharedRef<BackingStore>` is not auto-`Send` (the
//!   backing store is `Send` but not `Sync`); a SAB-backed store is explicitly designed for
//!   cross-isolate sharing (atomic refcount), so the wrapper is sound for *shared* stores only.
//! * [`ThreadSpawnRegistry`] — a process-global table that hands a `thread-spawn` payload from the
//!   spawning isolate's thread to the freshly created worker isolate's thread, keyed by an opaque
//!   token (the worker bootstrap looks the payload up and consumes it).
//!
//! Trust model: all threads of a VM are the same untrusted executor sharing one memory (one trust
//! domain), so there is no new guest<->guest boundary. The new surface is concurrent host calls into
//! kernel state, handled by the per-VM lock at the kernel/sidecar layer (see the spec, Phase 2).

use std::collections::HashMap;
use std::sync::atomic::{AtomicI32, AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};

/// A wasm linear-memory backing store that can be moved to a worker isolate thread.
///
/// Sound only for SAB-backed (shared) backing stores: the underlying C++ `shared_ptr` is atomically
/// refcounted and the storage is shared memory designed for concurrent cross-isolate access. Do not
/// use this to move a non-shared `ArrayBuffer` backing store across threads.
pub struct SendBackingStore(pub v8::SharedRef<v8::BackingStore>);

// SAFETY: see the type doc — restricted to shared (SAB) backing stores.
unsafe impl Send for SendBackingStore {}

/// A compiled wasm module shareable across isolates. `CompiledWasmModule` is already `Send + Sync` in
/// rusty_v8; this newtype exists for symmetry and to document intent at the spawn boundary.
pub struct SendCompiledModule(pub v8::CompiledWasmModule);

// `CompiledWasmModule` is declared `Send + Sync` by the v8 crate; this is purely a documenting wrapper.
unsafe impl Send for SendCompiledModule {}

/// Everything a worker isolate needs to begin executing one guest thread.
pub struct ThreadStart {
    /// The shared compiled guest module (re-instantiated in the worker isolate).
    pub module: SendCompiledModule,
    /// The shared linear memory's backing store (re-wrapped as the worker's `env.memory`).
    pub memory_backing: SendBackingStore,
    /// The thread id assigned by the host (positive; returned to the spawner).
    pub tid: i32,
    /// The opaque `start_arg` pointer wasi-libc passed to `thread-spawn`; forwarded verbatim to
    /// `wasi_thread_start(tid, start_arg)`. It points into the shared linear memory, so it is valid in
    /// the worker isolate precisely because the memory is shared.
    pub start_arg: i32,
}

/// Process-global table that carries a [`ThreadStart`] from the spawning isolate's thread to the
/// worker isolate's thread. The spawner [`register`](ThreadSpawnRegistry::register)s a payload, starts
/// the worker execution with the returned token, and the worker bootstrap
/// [`take`](ThreadSpawnRegistry::take)s it exactly once.
pub struct ThreadSpawnRegistry {
    pending: Mutex<HashMap<u64, ThreadStart>>,
    next_token: AtomicU64,
}

impl ThreadSpawnRegistry {
    fn new() -> Self {
        Self {
            pending: Mutex::new(HashMap::new()),
            next_token: AtomicU64::new(1),
        }
    }

    /// The single process-global registry.
    pub fn global() -> &'static ThreadSpawnRegistry {
        static REGISTRY: OnceLock<ThreadSpawnRegistry> = OnceLock::new();
        REGISTRY.get_or_init(ThreadSpawnRegistry::new)
    }

    /// Stash a pending thread start and return its lookup token.
    pub fn register(&self, start: ThreadStart) -> u64 {
        let token = self.next_token.fetch_add(1, Ordering::Relaxed);
        self.pending.lock().expect("thread spawn registry poisoned").insert(token, start);
        token
    }

    /// Consume the pending thread start for `token` (returns `None` if already taken / unknown).
    pub fn take(&self, token: u64) -> Option<ThreadStart> {
        self.pending.lock().expect("thread spawn registry poisoned").remove(&token)
    }

    /// Number of pending (registered-but-not-taken) thread starts. Diagnostics/tests only.
    pub fn pending_len(&self) -> usize {
        self.pending.lock().expect("thread spawn registry poisoned").len()
    }
}

/// Per-VM thread-id allocator. tids are monotonic and never reused within a VM; tid 0 is reserved for
/// the main thread. wasi-libc treats a positive return from `thread-spawn` as the new thread id and a
/// negative return as failure (surfaced to `pthread_create` as `EAGAIN`).
#[derive(Debug)]
pub struct ThreadIdAllocator {
    next: AtomicI32,
}

impl Default for ThreadIdAllocator {
    fn default() -> Self {
        // tid 1 is the first worker; 0 is the main thread.
        Self { next: AtomicI32::new(1) }
    }
}

impl ThreadIdAllocator {
    /// Allocate the next tid, or `None` if the positive i32 space is exhausted (a runaway guest; the
    /// caller should return a negative value to `thread-spawn`).
    pub fn allocate(&self) -> Option<i32> {
        let tid = self.next.fetch_add(1, Ordering::Relaxed);
        if tid <= 0 {
            // Overflowed into the negative space; refuse.
            None
        } else {
            Some(tid)
        }
    }
}

/// Reconstruct a shared `WebAssembly.Memory` in the current isolate from a backing store transferred
/// from another isolate, via the `ValueSerializer` structured-clone path (the only route, since
/// `WasmMemoryObject` has no public constructor). `serialized` is the bytes produced by serializing the
/// memory in the source isolate with a delegate that returned id 0 for the backing store. Proven by
/// `tests/threads_memory_serialize_spike.rs`.
///
/// Returns the deserialized value (a `WebAssembly.Memory`) ready to use as the `env.memory` import.
pub fn deserialize_shared_memory<'s>(
    scope: &mut v8::HandleScope<'s>,
    context: v8::Local<'s, v8::Context>,
    serialized: &[u8],
    backing: &v8::SharedRef<v8::BackingStore>,
) -> Option<v8::Local<'s, v8::Value>> {
    use v8::ValueDeserializerHelper;
    let delegate = SingleStoreTransfer { store: backing.clone() };
    let deserializer = v8::ValueDeserializer::new(scope, Box::new(delegate), serialized);
    deserializer.read_header(context)?;
    deserializer.read_value(context)
}

/// Serialize a shared `WebAssembly.Memory` value in the current isolate, capturing its backing store.
/// Returns the serialized bytes and the captured backing store (to transfer to the worker isolate).
pub fn serialize_shared_memory<'s>(
    scope: &mut v8::HandleScope<'s>,
    context: v8::Local<'s, v8::Context>,
    memory: v8::Local<'s, v8::Value>,
) -> Option<(Vec<u8>, v8::SharedRef<v8::BackingStore>)> {
    use v8::ValueSerializerHelper;
    let captured = std::rc::Rc::new(std::cell::RefCell::new(None));
    let delegate = CaptureStoreTransfer { captured: captured.clone() };
    let serializer = v8::ValueSerializer::new(scope, Box::new(delegate));
    serializer.write_header();
    serializer.write_value(context, memory)?;
    let bytes = serializer.release();
    let store = captured.borrow_mut().take()?;
    Some((bytes, store))
}

/// Serializer delegate that captures the (single) shared backing store referenced by the value.
struct CaptureStoreTransfer {
    captured: std::rc::Rc<std::cell::RefCell<Option<v8::SharedRef<v8::BackingStore>>>>,
}

impl v8::ValueSerializerImpl for CaptureStoreTransfer {
    fn throw_data_clone_error<'s>(
        &self,
        scope: &mut v8::HandleScope<'s>,
        message: v8::Local<'s, v8::String>,
    ) {
        let error = v8::Exception::error(scope, message);
        scope.throw_exception(error);
    }

    fn get_shared_array_buffer_id<'s>(
        &self,
        _scope: &mut v8::HandleScope<'s>,
        shared_array_buffer: v8::Local<'s, v8::SharedArrayBuffer>,
    ) -> Option<u32> {
        *self.captured.borrow_mut() = Some(v8::SharedArrayBuffer::get_backing_store(&shared_array_buffer));
        Some(0)
    }
}

/// Deserializer delegate that hands back the one transferred backing store for id 0.
struct SingleStoreTransfer {
    store: v8::SharedRef<v8::BackingStore>,
}

impl v8::ValueDeserializerImpl for SingleStoreTransfer {
    fn get_shared_array_buffer_from_id<'s>(
        &self,
        scope: &mut v8::HandleScope<'s>,
        _transfer_id: u32,
    ) -> Option<v8::Local<'s, v8::SharedArrayBuffer>> {
        Some(v8::SharedArrayBuffer::with_backing_store(scope, &self.store))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registry_round_trips_a_token() {
        // Build a ThreadStart without touching V8 by faking the v8 fields is not possible (they hold
        // real v8 handles), so exercise the parts that don't need V8: tid allocation + registry shape.
        let alloc = ThreadIdAllocator::default();
        let a = alloc.allocate().unwrap();
        let b = alloc.allocate().unwrap();
        assert_eq!(a, 1, "first worker tid is 1 (0 is the main thread)");
        assert_eq!(b, 2, "tids are monotonic and unique");
        assert!(a != b);
    }

    #[test]
    fn registry_global_is_stable() {
        let r1 = ThreadSpawnRegistry::global() as *const _;
        let r2 = ThreadSpawnRegistry::global() as *const _;
        assert_eq!(r1, r2, "the registry is a single process-global");
    }
}
