#![forbid(unsafe_code)]

//! Native execution plane scaffold for the secure-exec runtime migration.

mod common;
mod host_node;
mod node_import_cache;
mod runtime_support;
mod signal;
pub mod v8_host;
pub mod v8_ipc;
pub mod v8_runtime;

pub mod benchmark;
#[allow(dead_code, unused_imports)]
pub mod javascript;
pub mod python;
pub mod wasm;

pub use javascript::{
    CreateJavascriptContextRequest, GuestRuntimeConfig, JavascriptContext, JavascriptExecution,
    JavascriptExecutionEngine, JavascriptExecutionError, JavascriptExecutionEvent,
    JavascriptExecutionLimits, JavascriptExecutionResult, JavascriptSyncRpcRequest,
    LocalModuleResolutionCache, LocalResolvedModuleFormat, ModuleFsReader, ModuleResolveMode,
    ModuleResolver, StartJavascriptExecutionRequest,
};
pub use python::{
    CreatePythonContextRequest, PythonContext, PythonExecution, PythonExecutionEngine,
    PythonExecutionError, PythonExecutionEvent, PythonExecutionLimits, PythonExecutionResult,
    PythonVfsRpcMethod, PythonVfsRpcRequest, PythonVfsRpcResponsePayload, PythonVfsRpcStat,
    StartPythonExecutionRequest,
};
pub use secure_exec_bridge::GuestRuntime;
pub use signal::{NodeSignalDispositionAction, NodeSignalHandlerRegistration};
pub use wasm::{
    CreateWasmContextRequest, NativeBinaryFormat, StartWasmExecutionRequest, WasmContext,
    WasmExecution, WasmExecutionEngine, WasmExecutionError, WasmExecutionEvent,
    WasmExecutionLimits, WasmExecutionResult, WasmPermissionTier,
};

pub trait NativeExecutionBridge: secure_exec_bridge::ExecutionBridge {}

impl<T> NativeExecutionBridge for T where T: secure_exec_bridge::ExecutionBridge {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ExecutionScaffold {
    pub package_name: &'static str,
    pub kernel_package: &'static str,
    pub target: &'static str,
    pub planned_guest_runtimes: [GuestRuntime; 2],
}

pub fn scaffold() -> ExecutionScaffold {
    ExecutionScaffold {
        package_name: env!("CARGO_PKG_NAME"),
        kernel_package: "secure-exec-kernel",
        target: "native",
        planned_guest_runtimes: [GuestRuntime::JavaScript, GuestRuntime::WebAssembly],
    }
}
