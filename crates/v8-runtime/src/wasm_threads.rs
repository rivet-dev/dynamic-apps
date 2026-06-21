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
    /// `ValueSerializer` bytes for the shared memory (paired with `memory_backing` to reconstruct a
    /// real `WebAssembly.Memory` in the worker isolate — `WasmMemoryObject` has no constructor).
    pub serialized_memory: Vec<u8>,
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

/// The process-global tid allocator used by the in-runtime `thread-spawn` path. (Per-VM allocation is
/// a refinement; global monotonic tids are a valid superset of per-VM uniqueness.)
fn global_thread_ids() -> &'static ThreadIdAllocator {
    static IDS: OnceLock<ThreadIdAllocator> = OnceLock::new();
    IDS.get_or_init(ThreadIdAllocator::default)
}

/// JS run in the worker isolate: instantiate the shared module against the shared memory and enter the
/// guest at `wasi_thread_start(tid, start_arg)`. The wasi imports are no-op stubs: a worker that makes
/// real host calls is the GTK path (Phase 2, routed through the sidecar); the minimal threads spike's
/// worker only touches shared memory, so stubs suffice and instantiation still needs every import
/// present (a Proxy supplies a no-op for any name).
const WORKER_BOOTSTRAP_JS: &str = "(() => {\n\
  const wasiStub = new Proxy({}, { get: () => (() => 0) });\n\
  const inst = new WebAssembly.Instance(globalThis.__threadMod, {\n\
    env: { memory: globalThis.__threadMem },\n\
    wasi_snapshot_preview1: wasiStub,\n\
    wasi_unstable: wasiStub,\n\
    wasi: { 'thread-spawn': () => -1 },\n\
  });\n\
  inst.exports.wasi_thread_start(globalThis.__threadTid, globalThis.__threadStartArg);\n\
})();";

fn set_global<'s>(
    scope: &mut v8::HandleScope<'s>,
    context: v8::Local<'s, v8::Context>,
    name: &str,
    value: v8::Local<'s, v8::Value>,
) {
    let global = context.global(scope);
    if let Some(key) = v8::String::new(scope, name) {
        global.set(scope, key.into(), value);
    }
}

/// Body of a worker OS thread: create a fresh isolate, reconstruct the shared memory + module from the
/// transferred handles, and run the worker bootstrap (which calls `wasi_thread_start`). Best-effort:
/// errors are swallowed (a trapping guest thread would, in the production path, fault the whole VM —
/// see the spec; for the spike a failed worker simply never flips the join flag and the test fails).
fn run_worker(start: ThreadStart) {
    crate::isolate::init_v8_platform();
    let mut isolate = crate::isolate::create_isolate(None);
    let context = crate::isolate::create_context(&mut isolate);
    let scope = &mut v8::HandleScope::new(&mut isolate);
    let context = v8::Local::new(scope, &context);
    let scope = &mut v8::ContextScope::new(scope, context);

    let memory = match deserialize_shared_memory(scope, context, &start.serialized_memory, &start.memory_backing.0) {
        Some(memory) => memory,
        None => return,
    };
    let module = match v8::WasmModuleObject::from_compiled_module(scope, &start.module.0) {
        Some(module) => module,
        None => return,
    };
    set_global(scope, context, "__threadMem", memory);
    set_global(scope, context, "__threadMod", module.into());
    let tid_value = v8::Integer::new(scope, start.tid).into();
    set_global(scope, context, "__threadTid", tid_value);
    let start_arg_value = v8::Integer::new(scope, start.start_arg).into();
    set_global(scope, context, "__threadStartArg", start_arg_value);

    let try_catch = &mut v8::TryCatch::new(scope);
    if let Some(code) = v8::String::new(try_catch, WORKER_BOOTSTRAP_JS) {
        if let Some(script) = v8::Script::compile(try_catch, code, None) {
            let _ = script.run(try_catch);
        }
    }
}

/// Native `__agentOsWasmThreadSpawn(start_arg, module, memory)` callback. Invoked by the wasm runner's
/// `wasi.thread-spawn` import. Captures the shared module + memory from the spawning isolate, allocates
/// a tid, spawns a worker OS thread that runs the guest's `wasi_thread_start`, and returns the tid
/// synchronously (a negative return tells wasi-libc the spawn failed -> EAGAIN).
fn wasm_thread_spawn_callback<'s>(
    scope: &mut v8::HandleScope<'s>,
    args: v8::FunctionCallbackArguments<'s>,
    mut rv: v8::ReturnValue,
) {
    let fail = |rv: &mut v8::ReturnValue| rv.set_int32(-1);

    let start_arg = args.get(0).int32_value(scope).unwrap_or(0);

    let module = match v8::Local::<v8::WasmModuleObject>::try_from(args.get(1)) {
        Ok(module) => module,
        Err(_) => return fail(&mut rv),
    };
    let compiled = module.get_compiled_module();

    let memory_value = args.get(2);
    let context = scope.get_current_context();
    let (serialized_memory, backing) = match serialize_shared_memory(scope, context, memory_value) {
        Some(parts) => parts,
        None => return fail(&mut rv),
    };

    let tid = match global_thread_ids().allocate() {
        Some(tid) => tid,
        None => return fail(&mut rv),
    };

    let start = ThreadStart {
        module: SendCompiledModule(compiled),
        memory_backing: SendBackingStore(backing),
        serialized_memory,
        tid,
        start_arg,
    };
    if std::thread::Builder::new()
        .name(format!("wasm-thread-{tid}"))
        .spawn(move || run_worker(start))
        .is_err()
    {
        return fail(&mut rv);
    }
    rv.set_int32(tid);
}

/// Register `globalThis.__agentOsWasmThreadSpawn` on the current context so the wasm runner's
/// `wasi.thread-spawn` import can reach it. Inert for non-threaded guests (they never call it).
pub fn register_thread_spawn(scope: &mut v8::HandleScope) {
    let context = scope.get_current_context();
    let global = context.global(scope);
    let template = v8::FunctionTemplate::builder(wasm_thread_spawn_callback).build(scope);
    if let Some(func) = template.get_function(scope) {
        if let Some(key) = v8::String::new(scope, "__agentOsWasmThreadSpawn") {
            global.set(scope, key.into(), func.into());
        }
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
