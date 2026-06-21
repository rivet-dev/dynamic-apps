//! M7.5.0 Phase-1 mechanism proof (WASM-THREADS-SPEC.md §11a): a *worker* isolate on another OS thread
//! reconstructs the shared memory + compiled module transferred from the spawning isolate, instantiates
//! the module against `env.memory`, and runs an exported function that mutates the shared memory — which
//! the spawning isolate then observes. This is exactly the sequence the real `wasi.thread-spawn` worker
//! bootstrap performs (only the exported entry differs: `wasi_thread_start(tid, start_arg)` instead of
//! `poke`). It exercises the `wasm_threads` coordinator helpers end to end across two isolates.
//!
//! The guest module (hand-assembled, 60 bytes) is:
//!   (module
//!     (import "env" "memory" (memory 1 2 shared))
//!     (func (export "poke") (param i32) (i32.store (i32.const 0) (local.get 0))))

use secure_exec_v8_runtime::wasm_threads::{
    deserialize_shared_memory, serialize_shared_memory, SendBackingStore, SendCompiledModule,
};

/// `poke.wasm`: imports a shared `env.memory`, exports `poke(v)` storing `v` at offset 0.
const POKE_WASM: &[u8] = &[
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x01, 0x7f, 0x00, 0x02,
    0x10, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x03, 0x01,
    0x02, 0x03, 0x02, 0x01, 0x00, 0x07, 0x08, 0x01, 0x04, 0x70, 0x6f, 0x6b, 0x65, 0x00, 0x00, 0x0a,
    0x0b, 0x01, 0x09, 0x00, 0x41, 0x00, 0x20, 0x00, 0x36, 0x02, 0x00, 0x0b,
];

fn run<'s>(scope: &mut v8::HandleScope<'s>, src: &str) -> v8::Local<'s, v8::Value> {
    let code = v8::String::new(scope, src).unwrap();
    let script = v8::Script::compile(scope, code, None).unwrap();
    script.run(scope).unwrap()
}

fn set_global<'s>(
    scope: &mut v8::HandleScope<'s>,
    context: v8::Local<'s, v8::Context>,
    name: &str,
    value: v8::Local<'s, v8::Value>,
) {
    let global = context.global(scope);
    let key = v8::String::new(scope, name).unwrap();
    global.set(scope, key.into(), value);
}

#[test]
fn worker_isolate_instantiates_module_over_shared_memory() {
    secure_exec_v8_runtime::isolate::init_v8_platform();

    // --- Spawning isolate A: create the shared memory, compile the module, capture both for transfer. ---
    let mut isolate_a = secure_exec_v8_runtime::isolate::create_isolate(None);
    let context_a = secure_exec_v8_runtime::isolate::create_context(&mut isolate_a);
    let (serialized, backing, compiled) = {
        let scope = &mut v8::HandleScope::new(&mut isolate_a);
        let context = v8::Local::new(scope, &context_a);
        let scope = &mut v8::ContextScope::new(scope, context);
        let memory = run(
            scope,
            "globalThis.m = new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true });\n\
             globalThis.m",
        );
        let (bytes, store) =
            serialize_shared_memory(scope, context, memory).expect("serialize shared memory");
        let module = v8::WasmModuleObject::compile(scope, POKE_WASM).expect("compile poke.wasm");
        let compiled = module.get_compiled_module();
        (bytes, store, compiled)
    };

    // --- Worker isolate B on another OS thread: reconstruct, instantiate, run the export. ---
    let send_store = SendBackingStore(backing);
    let send_module = SendCompiledModule(compiled);
    let serialized_for_b = serialized.clone();
    let worker = std::thread::spawn(move || {
        secure_exec_v8_runtime::isolate::init_v8_platform();
        let mut isolate_b = secure_exec_v8_runtime::isolate::create_isolate(None);
        let scope = &mut v8::HandleScope::new(&mut isolate_b);
        let context = v8::Context::new(scope, Default::default());
        let scope = &mut v8::ContextScope::new(scope, context);

        let memory = deserialize_shared_memory(scope, context, &serialized_for_b, &send_store.0)
            .expect("deserialize shared memory in worker");
        let module = v8::WasmModuleObject::from_compiled_module(scope, &send_module.0)
            .expect("reconstruct module in worker");

        set_global(scope, context, "__mem", memory);
        set_global(scope, context, "__mod", module.into());
        // Instantiate against the shared memory and run the export that writes 99 to offset 0.
        run(
            scope,
            "const inst = new WebAssembly.Instance(globalThis.__mod, { env: { memory: globalThis.__mem } });\n\
             inst.exports.poke(99);",
        );
    });
    worker.join().expect("worker thread panicked");

    // --- Spawning isolate A: observe the worker's write through the shared memory. ---
    let seen = {
        let scope = &mut v8::HandleScope::new(&mut isolate_a);
        let context = v8::Local::new(scope, &context_a);
        let scope = &mut v8::ContextScope::new(scope, context);
        let value = run(scope, "new Uint32Array(globalThis.m.buffer)[0]");
        value.uint32_value(scope).unwrap()
    };
    assert_eq!(
        seen, 99,
        "isolate A must observe the worker isolate's wasm write through the shared memory"
    );
}
