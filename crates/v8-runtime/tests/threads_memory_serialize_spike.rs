//! M7.5.0 Phase-1 primitive (WASM-THREADS-SPEC.md §11a): round-trip a *shared* `WebAssembly.Memory`
//! across two isolates via `ValueSerializer`/`ValueDeserializer`, so the second (worker) isolate gets a
//! real `WebAssembly.Memory` object backed by the SAME memory — which it needs to instantiate the guest
//! module's `env.memory` import. `WasmMemoryObject` has no constructor in rusty_v8, so structured-clone
//! (the node:worker_threads mechanism) is the route; V8 serializes a shared wasm memory by referencing
//! its SharedArrayBuffer via the `get_shared_array_buffer_id` delegate, which we transfer out-of-band.
//!
//! Proven here: isolate B observes A's write through B's deserialized memory, and A observes B's write —
//! i.e. the two `WebAssembly.Memory` objects in two isolates share one backing store. Combined with the
//! backing-store keystone (threads_shared_memory_spike.rs) and `CompiledWasmModule` sharing, this is all
//! the cross-isolate machinery Phase 1's real thread-spawn needs.

use std::cell::RefCell;
use v8::ValueDeserializerHelper;
use v8::ValueSerializerHelper;

type BackingStores = Vec<v8::SharedRef<v8::BackingStore>>;

/// Sound for SAB-backed (shared) backing stores moved between isolate threads (atomic refcount).
struct SendBackingStores(BackingStores);
unsafe impl Send for SendBackingStores {}

/// SerDes delegate that transfers shared array buffers (and thus shared wasm memories) by collecting
/// their backing stores into a side table, mirroring the v8 crate's own `Custom1Value` test delegate.
struct MemoryTransfer<'a> {
    stores: RefCell<&'a mut BackingStores>,
}

impl<'a> v8::ValueSerializerImpl for MemoryTransfer<'a> {
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
        let mut stores = self.stores.borrow_mut();
        stores.push(v8::SharedArrayBuffer::get_backing_store(&shared_array_buffer));
        Some((stores.len() as u32) - 1)
    }
}

impl<'a> v8::ValueDeserializerImpl for MemoryTransfer<'a> {
    fn get_shared_array_buffer_from_id<'s>(
        &self,
        scope: &mut v8::HandleScope<'s>,
        transfer_id: u32,
    ) -> Option<v8::Local<'s, v8::SharedArrayBuffer>> {
        let store = self.stores.borrow().get(transfer_id as usize)?.clone();
        Some(v8::SharedArrayBuffer::with_backing_store(scope, &store))
    }
}

fn run<'s>(scope: &mut v8::HandleScope<'s>, src: &str) -> v8::Local<'s, v8::Value> {
    let code = v8::String::new(scope, src).unwrap();
    let script = v8::Script::compile(scope, code, None).unwrap();
    script.run(scope).unwrap()
}

#[test]
fn shared_wasm_memory_round_trips_across_isolates() {
    secure_exec_v8_runtime::isolate::init_v8_platform();

    let mut stores = BackingStores::new();

    // --- Isolate A: create a shared WebAssembly.Memory, write byte[0]=11, serialize it. ---
    let mut isolate_a = secure_exec_v8_runtime::isolate::create_isolate(None);
    let context_a = secure_exec_v8_runtime::isolate::create_context(&mut isolate_a);
    let serialized: Vec<u8> = {
        let scope = &mut v8::HandleScope::new(&mut isolate_a);
        let context = v8::Local::new(scope, &context_a);
        let scope = &mut v8::ContextScope::new(scope, context);
        let memory = run(
            scope,
            "globalThis.m = new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true });\n\
             new Uint8Array(globalThis.m.buffer)[0] = 11;\n\
             globalThis.m",
        );
        let serializer =
            v8::ValueSerializer::new(scope, Box::new(MemoryTransfer { stores: RefCell::new(&mut stores) }));
        serializer.write_header();
        let wrote = serializer.write_value(context, memory);
        assert_eq!(wrote, Some(true), "V8 should serialize a shared WebAssembly.Memory");
        serializer.release()
    };
    assert!(!stores.is_empty(), "the memory's shared backing store should have been transferred");

    // --- Isolate B on another OS thread: deserialize -> a WebAssembly.Memory sharing the backing store. ---
    let sendable = SendBackingStores(stores);
    let serialized_for_b = serialized.clone();
    let worker = std::thread::spawn(move || {
        secure_exec_v8_runtime::isolate::init_v8_platform();
        let mut stores_b = sendable.0;
        let mut isolate_b = secure_exec_v8_runtime::isolate::create_isolate(None);
        let scope = &mut v8::HandleScope::new(&mut isolate_b);
        let context = v8::Context::new(scope, Default::default());
        let scope = &mut v8::ContextScope::new(scope, context);
        let value = {
            let deserializer = v8::ValueDeserializer::new(
                scope,
                Box::new(MemoryTransfer { stores: RefCell::new(&mut stores_b) }),
                &serialized_for_b,
            );
            assert_eq!(deserializer.read_header(context), Some(true));
            deserializer.read_value(context).expect("deserialize WebAssembly.Memory in isolate B")
        };
        let global = context.global(scope);
        let key = v8::String::new(scope, "m2").unwrap();
        global.set(scope, key.into(), value);
        // Confirm it really is a WebAssembly.Memory and shares A's bytes; then write byte[1]=22.
        let seen = run(
            scope,
            "if (!(globalThis.m2 instanceof WebAssembly.Memory)) throw new Error('not a Memory');\n\
             const v = new Uint8Array(globalThis.m2.buffer)[0];\n\
             new Uint8Array(globalThis.m2.buffer)[1] = 22;\n\
             v",
        );
        seen.uint32_value(scope).unwrap()
    });
    let seen_from_a = worker.join().expect("worker thread panicked");
    assert_eq!(seen_from_a, 11, "isolate B's deserialized memory must see A's write");

    // --- Isolate A: observe B's write through the original memory (same context as the serialize). ---
    let seen_from_b = {
        let scope = &mut v8::HandleScope::new(&mut isolate_a);
        let context = v8::Local::new(scope, &context_a);
        let scope = &mut v8::ContextScope::new(scope, context);
        let value = run(scope, "new Uint8Array(globalThis.m.buffer)[1]");
        value.uint32_value(scope).unwrap()
    };
    assert_eq!(
        seen_from_b, 22,
        "isolate A must observe isolate B's write through the shared wasm memory"
    );
}
