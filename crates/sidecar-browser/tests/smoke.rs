#[path = "../../bridge/tests/support.rs"]
mod bridge_support;

use bridge_support::RecordingBridge;
use secure_exec_kernel::kernel::KernelVmConfig;
use secure_exec_sidecar_browser::{
    scaffold, BrowserExtension, BrowserExtensionContext, BrowserExtensionRequest, BrowserSidecar,
    BrowserSidecarConfig, BrowserWorkerBridge, BrowserWorkerHandle, BrowserWorkerHandleRequest,
    BrowserWorkerSpawnRequest,
};

struct SmokeExtension(&'static str);

impl BrowserExtension for SmokeExtension {
    fn namespace(&self) -> &str {
        self.0
    }

    fn handle_request(
        &self,
        context: &mut BrowserExtensionContext<'_>,
        payload: &[u8],
    ) -> Result<Vec<u8>, secure_exec_sidecar_browser::BrowserSidecarError> {
        if payload == b"context-fs" {
            context.mkdir("vm-ext", "/workspace", true)?;
            context.write_file("vm-ext", "/workspace/context.txt", b"from-context")?;
            return context.read_file("vm-ext", "/workspace/context.txt");
        }
        let mut response = self.0.as_bytes().to_vec();
        response.push(b':');
        response.extend_from_slice(payload);
        Ok(response)
    }
}

impl BrowserWorkerBridge for RecordingBridge {
    fn create_worker(
        &mut self,
        request: BrowserWorkerSpawnRequest,
    ) -> Result<BrowserWorkerHandle, Self::Error> {
        Ok(BrowserWorkerHandle {
            worker_id: format!("smoke-worker-{}", request.context_id),
            runtime: request.runtime,
        })
    }

    fn terminate_worker(
        &mut self,
        _request: BrowserWorkerHandleRequest,
    ) -> Result<(), Self::Error> {
        Ok(())
    }
}

#[test]
fn browser_sidecar_scaffold_stays_on_main_thread_with_shared_kernel() {
    let scaffold = scaffold();

    assert_eq!(scaffold.package_name, "secure-exec-sidecar-browser");
    assert_eq!(scaffold.kernel_package, "secure-exec-kernel");
    assert_eq!(scaffold.execution_host_thread, "main");
    assert_eq!(scaffold.guest_worker_owner_thread, "main");
}

#[test]
fn browser_sidecar_accepts_extension_signature() {
    let mut sidecar = BrowserSidecar::with_extensions(
        RecordingBridge::default(),
        BrowserSidecarConfig::default(),
        vec![Box::new(SmokeExtension("dev.rivet.agentos.browser-smoke"))],
    )
    .expect("construct browser sidecar with extension");

    assert_eq!(sidecar.extension_count(), 1);
    assert!(sidecar.has_extension("dev.rivet.agentos.browser-smoke"));

    let error = sidecar
        .register_extension(Box::new(SmokeExtension("dev.rivet.agentos.browser-smoke")))
        .expect_err("duplicate extension namespace should fail");
    assert!(error
        .to_string()
        .contains("browser extension namespace already registered"));
}

#[test]
fn browser_sidecar_dispatches_extension_requests_by_namespace() {
    let mut sidecar = BrowserSidecar::with_extensions(
        RecordingBridge::default(),
        BrowserSidecarConfig::default(),
        vec![Box::new(SmokeExtension("dev.rivet.agentos.browser-smoke"))],
    )
    .expect("construct browser sidecar with extension");

    let response = sidecar
        .dispatch_extension_request(BrowserExtensionRequest {
            namespace: String::from("dev.rivet.agentos.browser-smoke"),
            payload: b"ping".to_vec(),
        })
        .expect("dispatch extension request");
    assert_eq!(response.namespace, "dev.rivet.agentos.browser-smoke");
    assert_eq!(response.payload, b"dev.rivet.agentos.browser-smoke:ping");

    let error = sidecar
        .dispatch_extension_request(BrowserExtensionRequest {
            namespace: String::from("missing"),
            payload: Vec::new(),
        })
        .expect_err("unknown extension namespace should fail");
    assert!(error
        .to_string()
        .contains("no browser extension registered for namespace missing"));
}

#[test]
fn browser_extension_context_exposes_vm_filesystem_primitives() {
    let mut sidecar = BrowserSidecar::with_extensions(
        RecordingBridge::default(),
        BrowserSidecarConfig::default(),
        vec![Box::new(SmokeExtension("dev.rivet.agentos.browser-smoke"))],
    )
    .expect("construct browser sidecar with extension");
    sidecar
        .create_vm(KernelVmConfig::new("vm-ext"))
        .expect("create vm for extension context");

    let response = sidecar
        .dispatch_extension_request(BrowserExtensionRequest {
            namespace: String::from("dev.rivet.agentos.browser-smoke"),
            payload: b"context-fs".to_vec(),
        })
        .expect("dispatch extension request through context");

    assert_eq!(response.payload, b"from-context");
}
