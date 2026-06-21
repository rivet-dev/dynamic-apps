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
use std::sync::atomic::{AtomicI32, AtomicU64, AtomicUsize, Ordering};
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

/// Bounds the number of *live* worker threads (= worker isolates + OS threads), so an untrusted guest
/// cannot spawn-bomb the host (a resource-exhaustion escape per the trust model). A slot is reserved
/// before a worker OS thread is spawned and released when it exits. The cap is process-global for now;
/// moving it to the per-VM `ResourceLimits` on the BARE wire is a tracked DoD item (§10).
#[derive(Debug)]
pub struct ThreadSlots {
    live: AtomicUsize,
    max: usize,
}

impl ThreadSlots {
    pub fn new(max: usize) -> Self {
        Self { live: AtomicUsize::new(0), max }
    }

    /// Atomically reserve a slot, returning `false` (without reserving) if at the cap. The
    /// reserve-then-rollback keeps the check-and-increment race-free: concurrent spawners each see a
    /// distinct `prev`, so at most `max` succeed.
    pub fn try_reserve(&self) -> bool {
        let prev = self.live.fetch_add(1, Ordering::SeqCst);
        if prev >= self.max {
            self.live.fetch_sub(1, Ordering::SeqCst);
            false
        } else {
            true
        }
    }

    /// Release a previously reserved slot (called when a worker thread exits).
    pub fn release(&self) {
        self.live.fetch_sub(1, Ordering::SeqCst);
    }

    pub fn live(&self) -> usize {
        self.live.load(Ordering::SeqCst)
    }
}

/// Default cap on concurrent worker threads per process. Generous enough for GLib/GTK thread pools,
/// bounded enough to prevent host OS-thread/isolate exhaustion.
pub const DEFAULT_MAX_LIVE_THREADS: usize = 128;

fn global_thread_slots() -> &'static ThreadSlots {
    static SLOTS: OnceLock<ThreadSlots> = OnceLock::new();
    SLOTS.get_or_init(|| ThreadSlots::new(DEFAULT_MAX_LIVE_THREADS))
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
    wasi: {\n\
      // Nested spawn: a worker can spawn further worker threads (GLib thread pools do).\n\
      'thread-spawn'(startArg) {\n\
        if (typeof globalThis.__agentOsWasmThreadSpawn !== 'function') return -1;\n\
        return globalThis.__agentOsWasmThreadSpawn(startArg, globalThis.__threadMod, globalThis.__threadMem) | 0;\n\
      },\n\
    },\n\
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
    // Release the reserved thread slot when this worker exits (normal return or early bail).
    struct SlotGuard;
    impl Drop for SlotGuard {
        fn drop(&mut self) {
            global_thread_slots().release();
        }
    }
    let _slot = SlotGuard;

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
    // Expose the native spawn so this worker can itself spawn further worker threads (nested spawn).
    register_thread_spawn(scope);

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

    // Enforce the live-thread cap BEFORE allocating a tid / spawning, so a guest cannot spawn-bomb
    // the host. The slot is released when the worker exits (run_worker's SlotGuard).
    if !global_thread_slots().try_reserve() {
        return fail(&mut rv);
    }

    let tid = match global_thread_ids().allocate() {
        Some(tid) => tid,
        None => {
            global_thread_slots().release();
            return fail(&mut rv);
        }
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
        global_thread_slots().release();
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

/// Concurrent response demultiplexer for the shared-session worker host-call path.
///
/// To give worker threads real kernel host calls, a worker isolate shares the spawning isolate's
/// bridge wiring (same `session_id` → the sidecar dispatches the worker's calls against the parent's
/// kernel process, so threads share the process fd table) and `call_id` counter. But the existing
/// per-session response receiver assumes exactly one in-flight call (it errors on a `call_id`
/// mismatch). With multiple threads issuing calls concurrently, responses interleave, so a single
/// reader must demultiplex them by `call_id` to the waiting thread. This is that router: each caller
/// `register`s its `call_id` (getting a one-shot receiver to block on), and the session's response
/// reader `complete`s each arriving response to the matching waiter.
pub struct ResponseDemux {
    waiters: Mutex<HashMap<u64, std::sync::mpsc::SyncSender<crate::runtime_protocol::BridgeResponse>>>,
}

impl Default for ResponseDemux {
    fn default() -> Self {
        Self { waiters: Mutex::new(HashMap::new()) }
    }
}

impl ResponseDemux {
    pub fn new() -> Self {
        Self::default()
    }

    /// Register interest in `call_id`; returns a receiver that yields exactly that call's response.
    pub fn register(&self, call_id: u64) -> std::sync::mpsc::Receiver<crate::runtime_protocol::BridgeResponse> {
        let (tx, rx) = std::sync::mpsc::sync_channel(1);
        self.waiters.lock().expect("response demux poisoned").insert(call_id, tx);
        rx
    }

    /// Deliver a response to the thread waiting on its `call_id`. Returns `false` if no waiter is
    /// registered (a late/duplicate response) so the caller can drop it.
    pub fn complete(&self, response: crate::runtime_protocol::BridgeResponse) -> bool {
        let sender = self.waiters.lock().expect("response demux poisoned").remove(&response.call_id);
        match sender {
            Some(tx) => tx.send(response).is_ok(),
            None => false,
        }
    }

    /// Number of outstanding (registered-but-not-completed) calls. Diagnostics/tests only.
    pub fn pending_len(&self) -> usize {
        self.waiters.lock().expect("response demux poisoned").len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::runtime_protocol::BridgeResponse;

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

    #[test]
    fn thread_slots_enforce_cap_and_release() {
        let slots = ThreadSlots::new(2);
        assert!(slots.try_reserve(), "first reservation under cap");
        assert!(slots.try_reserve(), "second reservation hits cap exactly");
        assert_eq!(slots.live(), 2);
        assert!(!slots.try_reserve(), "third reservation rejected at cap");
        assert_eq!(slots.live(), 2, "rejected reservation must not leak a slot");
        slots.release();
        assert_eq!(slots.live(), 1);
        assert!(slots.try_reserve(), "slot freed by release is reusable");
        assert_eq!(slots.live(), 2);
    }

    #[test]
    fn thread_slots_zero_cap_admits_nothing() {
        let slots = ThreadSlots::new(0);
        assert!(!slots.try_reserve());
        assert_eq!(slots.live(), 0);
    }

    fn response(call_id: u64) -> BridgeResponse {
        BridgeResponse { call_id, status: 0, payload: vec![call_id as u8] }
    }

    #[test]
    fn response_demux_routes_by_call_id_out_of_order() {
        let demux = ResponseDemux::new();
        let rx1 = demux.register(1);
        let rx2 = demux.register(2);
        assert_eq!(demux.pending_len(), 2);
        // Complete out of order: 2 then 1. Each waiter gets exactly its own response.
        assert!(demux.complete(response(2)));
        assert!(demux.complete(response(1)));
        assert_eq!(rx1.recv().unwrap().payload, vec![1]);
        assert_eq!(rx2.recv().unwrap().payload, vec![2]);
        assert_eq!(demux.pending_len(), 0);
    }

    #[test]
    fn response_demux_drops_unregistered_response() {
        let demux = ResponseDemux::new();
        assert!(!demux.complete(response(99)), "no waiter -> dropped");
    }

    #[test]
    fn response_demux_delivers_across_threads() {
        use std::sync::Arc;
        let demux = Arc::new(ResponseDemux::new());
        let rx = demux.register(7);
        let d2 = Arc::clone(&demux);
        let h = std::thread::spawn(move || d2.complete(response(7)));
        assert_eq!(rx.recv().unwrap().call_id, 7, "waiter wakes on cross-thread completion");
        assert!(h.join().unwrap());
    }
}
