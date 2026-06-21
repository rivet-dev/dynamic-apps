//! M7.5.0 keystone spike (WASM-THREADS-SPEC.md §0/§11a): prove that ONE wasm/SAB backing store can be
//! shared across TWO V8 isolates on TWO OS threads in this rusty_v8 embedding. This is the central
//! unknown of the whole wasi-threads effort: a worker thread = a second isolate that must see the same
//! linear memory as the spawning isolate. A shared `WebAssembly.Memory`'s buffer is a SharedArrayBuffer,
//! so its backing store is the exact object we must move across isolates here.
//!
//! `SharedRef<BackingStore>` is not auto-`Send` (BackingStore is Send but not Sync), but the underlying
//! C++ shared_ptr is atomically refcounted and a SAB backing store is explicitly designed for
//! cross-isolate/thread sharing (V8 docs: "coordinates lifetime ... even across isolates"). So wrapping
//! it in a `Send` newtype is sound for shared backing stores specifically. This is the same move
//! node:worker_threads makes internally when it posts a shared WebAssembly.Memory to a worker.
//!
//! Run as its own test binary (one #[test]) to avoid the documented inter-test V8 global-state SIGSEGV.

/// Sound for SAB-backed (shared) backing stores: refcount is atomic, data is shared memory.
struct SendBackingStore(v8::SharedRef<v8::BackingStore>);
unsafe impl Send for SendBackingStore {}

#[test]
fn shared_backing_store_crosses_isolates() {
    secure_exec_v8_runtime::isolate::init_v8_platform();

    // --- Isolate A: create a shared backing store and write a sentinel into it. ---
    let mut isolate_a = secure_exec_v8_runtime::isolate::create_isolate(None);
    let backing_store_a = {
        let scope = &mut v8::HandleScope::new(&mut isolate_a);
        let context = v8::Context::new(scope, Default::default());
        let scope = &mut v8::ContextScope::new(scope, context);
        // A shared WebAssembly.Memory's .buffer is exactly this kind of object.
        let sab = v8::SharedArrayBuffer::new(scope, 16).expect("create SharedArrayBuffer in A");
        let backing_store = sab.get_backing_store();
        let data = backing_store
            .data()
            .expect("backing store data pointer")
            .as_ptr() as *mut u8;
        // A writes byte[0] = 11.
        unsafe { *data = 11 };
        backing_store
    };

    // --- Isolate B on another OS thread: attach to the SAME backing store, read A's write, write back. ---
    let bs_for_b = SendBackingStore(backing_store_a.clone());
    let worker = std::thread::spawn(move || {
        secure_exec_v8_runtime::isolate::init_v8_platform();
        let mut isolate_b = secure_exec_v8_runtime::isolate::create_isolate(None);
        let scope = &mut v8::HandleScope::new(&mut isolate_b);
        let context = v8::Context::new(scope, Default::default());
        let scope = &mut v8::ContextScope::new(scope, context);
        // Reconstruct a SharedArrayBuffer in isolate B over A's backing store.
        let sab_b = v8::SharedArrayBuffer::with_backing_store(scope, &bs_for_b.0);
        let bs_b = sab_b.get_backing_store();
        let data = bs_b.data().expect("backing store data pointer in B").as_ptr() as *mut u8;
        let seen_from_a = unsafe { *data };
        // B writes byte[1] = 22 for A to observe.
        unsafe { *data.add(1) = 22 };
        seen_from_a
    });

    let seen_from_a = worker.join().expect("worker thread panicked");
    assert_eq!(
        seen_from_a, 11,
        "isolate B must observe isolate A's write through the shared backing store"
    );

    // --- Back in isolate A: observe B's write through the same backing store. ---
    let data = backing_store_a
        .data()
        .expect("backing store data pointer")
        .as_ptr() as *mut u8;
    let seen_from_b = unsafe { *data.add(1) };
    assert_eq!(
        seen_from_b, 22,
        "isolate A must observe isolate B's write through the shared backing store"
    );
}
