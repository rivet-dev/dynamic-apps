#![forbid(unsafe_code)]

//! Native sidecar scaffold that composes the kernel and execution crates.

pub(crate) mod bootstrap;
pub(crate) mod bridge;
pub(crate) mod execution;
pub mod extension;
pub(crate) mod filesystem;
pub mod generated_protocol;
#[allow(dead_code)]
pub(crate) mod json_rpc;
pub mod limits;
pub(crate) mod plugins;
pub(crate) mod sab_ring;
pub mod protocol;
pub(crate) mod rpc_trace;
pub mod service;
pub(crate) mod state;
pub mod stdio;
pub(crate) mod tools;
pub(crate) mod vm;
pub mod wire;

pub use extension::{
    Extension, ExtensionContext, ExtensionFuture, ExtensionInterruptRequest,
    ExtensionInterruptResponse, ExtensionResponse,
};
pub use service::{DispatchResult, NativeSidecar, NativeSidecarConfig, SidecarError};
pub use state::SidecarRequestTransport;

use wire::{DEFAULT_MAX_FRAME_BYTES, PROTOCOL_NAME, PROTOCOL_VERSION};

pub trait NativeSidecarBridge: secure_exec_bridge::HostBridge {}

impl<T> NativeSidecarBridge for T where T: secure_exec_bridge::HostBridge {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SidecarScaffold {
    pub package_name: &'static str,
    pub binary_name: &'static str,
    pub kernel_package: &'static str,
    pub execution_package: &'static str,
    pub protocol_name: &'static str,
    pub protocol_version: u16,
    pub max_frame_bytes: usize,
}

pub fn scaffold() -> SidecarScaffold {
    let kernel = secure_exec_kernel::scaffold();
    let execution = secure_exec_execution::scaffold();

    SidecarScaffold {
        package_name: env!("CARGO_PKG_NAME"),
        binary_name: env!("CARGO_PKG_NAME"),
        kernel_package: kernel.package_name,
        execution_package: execution.package_name,
        protocol_name: PROTOCOL_NAME,
        protocol_version: PROTOCOL_VERSION,
        max_frame_bytes: DEFAULT_MAX_FRAME_BYTES,
    }
}
