use crate::{
    BrowserSidecarBridge, BrowserWorkerEntrypoint, BrowserWorkerHandle, BrowserWorkerHandleRequest,
    BrowserWorkerSpawnRequest,
};
use secure_exec_bridge::{
    BridgeTypes, CreateJavascriptContextRequest, CreateWasmContextRequest, ExecutionEvent,
    ExecutionHandleRequest, GuestContextHandle, GuestRuntime, KillExecutionRequest,
    LifecycleEventRecord, LifecycleState, PollExecutionEventRequest, StartExecutionRequest,
    StartedExecution, StructuredEventRecord, WriteExecutionStdinRequest,
};
use secure_exec_kernel::bridge::LifecycleState as KernelLifecycleState;
use secure_exec_kernel::kernel::{KernelError, KernelVm, KernelVmConfig, VirtualProcessOptions};
use secure_exec_kernel::permissions::Permissions;
use secure_exec_kernel::vfs::MemoryFileSystem;
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt;
use std::time::Duration;

type BridgeError<B> = <B as BridgeTypes>::Error;
type BrowserKernel = KernelVm<MemoryFileSystem>;
const BROWSER_WORKER_DRIVER: &str = "browser.worker";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BrowserSidecarConfig {
    pub sidecar_id: String,
}

impl Default for BrowserSidecarConfig {
    fn default() -> Self {
        Self {
            sidecar_id: String::from("secure-exec-sidecar-browser"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BrowserSidecarError {
    InvalidState(String),
    Kernel(String),
    Bridge(String),
}

impl fmt::Display for BrowserSidecarError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidState(message) | Self::Kernel(message) | Self::Bridge(message) => {
                f.write_str(message)
            }
        }
    }
}

impl Error for BrowserSidecarError {}

struct VmState {
    kernel: BrowserKernel,
    contexts: BTreeSet<String>,
    active_executions: BTreeSet<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ContextState {
    vm_id: String,
    runtime: GuestRuntime,
    entrypoint: BrowserWorkerEntrypoint,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ExecutionState {
    vm_id: String,
    worker: BrowserWorkerHandle,
    kernel_pid: u32,
    stdin_write_fd: u32,
}

pub trait BrowserExtension: Send + Sync {
    fn namespace(&self) -> &str;

    fn handle_request(
        &self,
        context: &mut BrowserExtensionContext<'_>,
        payload: &[u8],
    ) -> Result<Vec<u8>, BrowserSidecarError> {
        let _ = context;
        let _ = payload;
        Err(BrowserSidecarError::InvalidState(format!(
            "browser extension {} does not handle requests",
            self.namespace()
        )))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BrowserExtensionRequest {
    pub namespace: String,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BrowserExtensionResponse {
    pub namespace: String,
    pub payload: Vec<u8>,
}

pub trait BrowserExtensionHost {
    fn write_file(
        &mut self,
        vm_id: &str,
        path: &str,
        contents: Vec<u8>,
    ) -> Result<(), BrowserSidecarError>;

    fn read_file(&mut self, vm_id: &str, path: &str) -> Result<Vec<u8>, BrowserSidecarError>;

    fn mkdir(
        &mut self,
        vm_id: &str,
        path: &str,
        recursive: bool,
    ) -> Result<(), BrowserSidecarError>;

    fn read_dir(&mut self, vm_id: &str, path: &str) -> Result<Vec<String>, BrowserSidecarError>;

    fn create_javascript_context(
        &mut self,
        request: CreateJavascriptContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError>;

    fn create_wasm_context(
        &mut self,
        request: CreateWasmContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError>;

    fn start_execution(
        &mut self,
        request: StartExecutionRequest,
    ) -> Result<StartedExecution, BrowserSidecarError>;

    fn write_stdin(
        &mut self,
        request: WriteExecutionStdinRequest,
    ) -> Result<(), BrowserSidecarError>;

    fn close_stdin(&mut self, request: ExecutionHandleRequest) -> Result<(), BrowserSidecarError>;

    fn kill_execution(&mut self, request: KillExecutionRequest) -> Result<(), BrowserSidecarError>;

    fn poll_execution_event(
        &mut self,
        request: PollExecutionEventRequest,
    ) -> Result<Option<ExecutionEvent>, BrowserSidecarError>;
}

pub struct BrowserExtensionContext<'a> {
    host: &'a mut dyn BrowserExtensionHost,
}

impl<'a> BrowserExtensionContext<'a> {
    pub fn new(host: &'a mut dyn BrowserExtensionHost) -> Self {
        Self { host }
    }

    pub fn write_file(
        &mut self,
        vm_id: &str,
        path: &str,
        contents: impl Into<Vec<u8>>,
    ) -> Result<(), BrowserSidecarError> {
        self.host.write_file(vm_id, path, contents.into())
    }

    pub fn read_file(&mut self, vm_id: &str, path: &str) -> Result<Vec<u8>, BrowserSidecarError> {
        self.host.read_file(vm_id, path)
    }

    pub fn mkdir(
        &mut self,
        vm_id: &str,
        path: &str,
        recursive: bool,
    ) -> Result<(), BrowserSidecarError> {
        self.host.mkdir(vm_id, path, recursive)
    }

    pub fn read_dir(
        &mut self,
        vm_id: &str,
        path: &str,
    ) -> Result<Vec<String>, BrowserSidecarError> {
        self.host.read_dir(vm_id, path)
    }

    pub fn create_javascript_context(
        &mut self,
        request: CreateJavascriptContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError> {
        self.host.create_javascript_context(request)
    }

    pub fn create_wasm_context(
        &mut self,
        request: CreateWasmContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError> {
        self.host.create_wasm_context(request)
    }

    pub fn start_execution(
        &mut self,
        request: StartExecutionRequest,
    ) -> Result<StartedExecution, BrowserSidecarError> {
        self.host.start_execution(request)
    }

    pub fn write_stdin(
        &mut self,
        request: WriteExecutionStdinRequest,
    ) -> Result<(), BrowserSidecarError> {
        self.host.write_stdin(request)
    }

    pub fn close_stdin(
        &mut self,
        request: ExecutionHandleRequest,
    ) -> Result<(), BrowserSidecarError> {
        self.host.close_stdin(request)
    }

    pub fn kill_execution(
        &mut self,
        request: KillExecutionRequest,
    ) -> Result<(), BrowserSidecarError> {
        self.host.kill_execution(request)
    }

    pub fn poll_execution_event(
        &mut self,
        request: PollExecutionEventRequest,
    ) -> Result<Option<ExecutionEvent>, BrowserSidecarError> {
        self.host.poll_execution_event(request)
    }
}

pub struct BrowserSidecar<B> {
    bridge: B,
    config: BrowserSidecarConfig,
    vms: BTreeMap<String, VmState>,
    contexts: BTreeMap<String, ContextState>,
    executions: BTreeMap<String, ExecutionState>,
    extensions: BTreeMap<String, Box<dyn BrowserExtension>>,
}

impl<B> BrowserSidecar<B>
where
    B: BrowserSidecarBridge,
    BridgeError<B>: fmt::Debug,
{
    pub fn new(bridge: B, config: BrowserSidecarConfig) -> Self {
        Self::with_extensions(bridge, config, Vec::new())
            .expect("empty browser extension registry should be valid")
    }

    pub fn with_extensions(
        bridge: B,
        config: BrowserSidecarConfig,
        extensions: Vec<Box<dyn BrowserExtension>>,
    ) -> Result<Self, BrowserSidecarError> {
        let mut sidecar = Self {
            bridge,
            config,
            vms: BTreeMap::new(),
            contexts: BTreeMap::new(),
            executions: BTreeMap::new(),
            extensions: BTreeMap::new(),
        };
        for extension in extensions {
            sidecar.register_extension(extension)?;
        }
        Ok(sidecar)
    }

    pub fn register_extension(
        &mut self,
        extension: Box<dyn BrowserExtension>,
    ) -> Result<(), BrowserSidecarError> {
        let namespace = extension.namespace();
        if namespace.is_empty() {
            return Err(BrowserSidecarError::InvalidState(String::from(
                "browser extension namespace must not be empty",
            )));
        }
        if self.extensions.contains_key(namespace) {
            return Err(BrowserSidecarError::InvalidState(format!(
                "browser extension namespace already registered: {namespace}",
            )));
        }
        self.extensions.insert(namespace.to_string(), extension);
        Ok(())
    }

    pub fn extension_count(&self) -> usize {
        self.extensions.len()
    }

    pub fn has_extension(&self, namespace: &str) -> bool {
        self.extensions.contains_key(namespace)
    }

    pub fn dispatch_extension_request(
        &mut self,
        request: BrowserExtensionRequest,
    ) -> Result<BrowserExtensionResponse, BrowserSidecarError> {
        let Some(extension) = self.extensions.remove(&request.namespace) else {
            return Err(BrowserSidecarError::InvalidState(format!(
                "no browser extension registered for namespace {}",
                request.namespace
            )));
        };
        let payload = {
            let mut context = BrowserExtensionContext::new(self);
            extension.handle_request(&mut context, &request.payload)
        };
        self.extensions.insert(request.namespace.clone(), extension);
        let payload = payload?;
        Ok(BrowserExtensionResponse {
            namespace: request.namespace,
            payload,
        })
    }

    pub fn sidecar_id(&self) -> &str {
        &self.config.sidecar_id
    }

    pub fn bridge(&self) -> &B {
        &self.bridge
    }

    pub fn bridge_mut(&mut self) -> &mut B {
        &mut self.bridge
    }

    pub fn into_bridge(self) -> B {
        self.bridge
    }

    pub fn vm_count(&self) -> usize {
        self.vms.len()
    }

    pub fn context_count(&self, vm_id: &str) -> usize {
        self.vms
            .get(vm_id)
            .map(|vm| vm.contexts.len())
            .unwrap_or_default()
    }

    pub fn active_worker_count(&self, vm_id: &str) -> usize {
        self.vms
            .get(vm_id)
            .map(|vm| vm.active_executions.len())
            .unwrap_or_default()
    }

    pub fn create_vm(&mut self, mut config: KernelVmConfig) -> Result<(), BrowserSidecarError> {
        let vm_id = config.vm_id.clone();
        if self.vms.contains_key(&vm_id) {
            return Err(BrowserSidecarError::InvalidState(format!(
                "browser sidecar VM already exists: {vm_id}"
            )));
        }

        // Browser-side host capabilities are already mediated at the JS host-bridge
        // boundary, so the in-browser kernel stays permissive and remains the single
        // source of truth for VM-local filesystem, process, and socket state.
        config.permissions = Permissions::allow_all();

        self.emit_lifecycle(
            &vm_id,
            LifecycleState::Starting,
            Some(String::from(
                "browser sidecar booting kernel on main thread",
            )),
        )?;
        self.vms.insert(
            vm_id.clone(),
            VmState {
                kernel: KernelVm::new(MemoryFileSystem::new(), config),
                contexts: BTreeSet::new(),
                active_executions: BTreeSet::new(),
            },
        );
        self.emit_lifecycle(
            &vm_id,
            LifecycleState::Ready,
            Some(String::from(
                "browser sidecar kernel is ready on the main thread",
            )),
        )?;
        Ok(())
    }

    pub fn write_file(
        &mut self,
        vm_id: &str,
        path: &str,
        contents: impl Into<Vec<u8>>,
    ) -> Result<(), BrowserSidecarError> {
        let vm = self.vm_mut(vm_id)?;
        vm.kernel
            .write_file(path, contents)
            .map_err(Self::kernel_error)
    }

    pub fn read_file(&mut self, vm_id: &str, path: &str) -> Result<Vec<u8>, BrowserSidecarError> {
        let vm = self.vm_mut(vm_id)?;
        vm.kernel.read_file(path).map_err(Self::kernel_error)
    }

    pub fn mkdir(
        &mut self,
        vm_id: &str,
        path: &str,
        recursive: bool,
    ) -> Result<(), BrowserSidecarError> {
        let vm = self.vm_mut(vm_id)?;
        vm.kernel.mkdir(path, recursive).map_err(Self::kernel_error)
    }

    pub fn read_dir(
        &mut self,
        vm_id: &str,
        path: &str,
    ) -> Result<Vec<String>, BrowserSidecarError> {
        let vm = self.vm_mut(vm_id)?;
        vm.kernel.read_dir(path).map_err(Self::kernel_error)
    }

    pub fn kernel_state(&self, vm_id: &str) -> Result<LifecycleState, BrowserSidecarError> {
        let vm = self.vm(vm_id)?;
        Ok(match vm.kernel.state() {
            KernelLifecycleState::Starting => LifecycleState::Starting,
            KernelLifecycleState::Ready => LifecycleState::Ready,
            KernelLifecycleState::Busy => LifecycleState::Busy,
            KernelLifecycleState::Terminated => LifecycleState::Terminated,
        })
    }

    pub fn read_execution_stdin(
        &mut self,
        vm_id: &str,
        execution_id: &str,
        length: usize,
        timeout: Duration,
    ) -> Result<Option<Vec<u8>>, BrowserSidecarError> {
        let execution = self.ensure_execution_state(vm_id, execution_id)?;
        let vm = self.vm_mut(vm_id)?;
        vm.kernel
            .read_process_stdin(
                BROWSER_WORKER_DRIVER,
                execution.kernel_pid,
                length,
                Some(timeout),
            )
            .map_err(Self::kernel_error)
    }

    pub fn dispose_vm(&mut self, vm_id: &str) -> Result<(), BrowserSidecarError> {
        // Remove the VM bookkeeping up front and take ownership of its state, so
        // that EVERY exit path below — including a mid-dispose `?` failure while
        // releasing executions or emitting lifecycle events — reclaims the
        // VmState (and the BrowserKernel it owns) instead of stranding it in the
        // `vms` map for the process lifetime.
        let Some(vm_state) = self.vms.remove(vm_id) else {
            return Err(BrowserSidecarError::InvalidState(format!(
                "unknown browser sidecar VM: {vm_id}"
            )));
        };

        // Dropping per-context bookkeeping is infallible, so do it
        // unconditionally; `contexts` can never retain an entry for a VM that
        // has already been removed from `vms`.
        for context_id in &vm_state.contexts {
            self.contexts.remove(context_id);
        }

        // Release every execution, attempting all of them and retaining only the
        // first error. A single worker-termination failure must not abandon the
        // remaining executions (their `ExecutionState`s would otherwise leak),
        // and `release_execution` already removes each entry from `executions`
        // before doing fallible bridge work, so the maps stay drained even when
        // the bridge reports an error.
        let mut first_error: Option<BrowserSidecarError> = None;
        for execution_id in &vm_state.active_executions {
            if let Err(error) = self.release_execution(execution_id, "browser.worker.disposed") {
                first_error.get_or_insert(error);
            }
        }

        // Emit the terminal lifecycle event regardless of the outcome above; the
        // VM is already gone from the registry either way.
        let terminated = self.emit_lifecycle(
            vm_id,
            LifecycleState::Terminated,
            Some(String::from(
                "browser sidecar VM disposed on the main thread",
            )),
        );

        match first_error {
            Some(error) => Err(error),
            None => terminated,
        }
    }

    pub fn create_javascript_context(
        &mut self,
        request: CreateJavascriptContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError> {
        self.ensure_vm(&request.vm_id)?;

        let vm_id = request.vm_id.clone();
        let entrypoint = BrowserWorkerEntrypoint::JavaScript {
            bootstrap_module: request.bootstrap_module.clone(),
        };
        let handle = self
            .bridge
            .create_javascript_context(request)
            .map_err(Self::bridge_error)?;

        self.register_context(vm_id, handle.clone(), entrypoint)?;
        Ok(handle)
    }

    pub fn create_wasm_context(
        &mut self,
        request: CreateWasmContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError> {
        self.ensure_vm(&request.vm_id)?;

        let vm_id = request.vm_id.clone();
        let entrypoint = BrowserWorkerEntrypoint::WebAssembly {
            module_path: request.module_path.clone(),
        };
        let handle = self
            .bridge
            .create_wasm_context(request)
            .map_err(Self::bridge_error)?;

        self.register_context(vm_id, handle.clone(), entrypoint)?;
        Ok(handle)
    }

    pub fn start_execution(
        &mut self,
        request: StartExecutionRequest,
    ) -> Result<StartedExecution, BrowserSidecarError> {
        self.ensure_vm(&request.vm_id)?;

        let context = self
            .contexts
            .get(&request.context_id)
            .cloned()
            .ok_or_else(|| {
                BrowserSidecarError::InvalidState(format!(
                    "unknown browser sidecar context: {}",
                    request.context_id
                ))
            })?;

        if context.vm_id != request.vm_id {
            return Err(BrowserSidecarError::InvalidState(format!(
                "browser sidecar context {} belongs to vm {}, not {}",
                request.context_id, context.vm_id, request.vm_id
            )));
        }

        let guest_cwd = request.cwd.clone();
        let (kernel_pid, stdin_write_fd) = {
            let vm = self.vm_mut(&request.vm_id)?;
            let kernel_handle = vm
                .kernel
                .create_virtual_process(
                    BROWSER_WORKER_DRIVER,
                    BROWSER_WORKER_DRIVER,
                    request
                        .argv
                        .first()
                        .map(String::as_str)
                        .unwrap_or("browser-worker"),
                    request.argv.clone(),
                    VirtualProcessOptions {
                        env: request.env.clone(),
                        cwd: Some(guest_cwd.clone()),
                        ..VirtualProcessOptions::default()
                    },
                )
                .map_err(Self::kernel_error)?;
            let kernel_pid = kernel_handle.pid();
            match Self::configure_process_stdio(&mut vm.kernel, kernel_pid) {
                Ok(stdin_write_fd) => (kernel_pid, stdin_write_fd),
                Err(error) => {
                    Self::cleanup_pending_kernel_process(&mut vm.kernel, kernel_pid)?;
                    return Err(error);
                }
            }
        };

        let worker = match self.bridge.create_worker(BrowserWorkerSpawnRequest {
            vm_id: request.vm_id.clone(),
            context_id: request.context_id.clone(),
            runtime: context.runtime,
            entrypoint: context.entrypoint.clone(),
        }) {
            Ok(worker) => worker,
            Err(error) => {
                let vm = self.vm_mut(&request.vm_id)?;
                Self::cleanup_pending_kernel_process(&mut vm.kernel, kernel_pid)?;
                return Err(Self::bridge_error(error));
            }
        };

        let started = match self.bridge.start_execution(request.clone()) {
            Ok(started) => started,
            Err(error) => {
                let cleanup_result = {
                    let vm = self.vm_mut(&request.vm_id)?;
                    Self::cleanup_pending_kernel_process(&mut vm.kernel, kernel_pid)
                };
                let terminate_result = self
                    .bridge
                    .terminate_worker(BrowserWorkerHandleRequest {
                        vm_id: request.vm_id,
                        execution_id: String::from("pending"),
                        worker_id: worker.worker_id,
                    })
                    .map_err(Self::bridge_error);
                cleanup_result?;
                terminate_result?;
                return Err(Self::bridge_error(error));
            }
        };

        let worker_id = worker.worker_id.clone();
        self.executions.insert(
            started.execution_id.clone(),
            ExecutionState {
                vm_id: request.vm_id.clone(),
                worker: worker.clone(),
                kernel_pid,
                stdin_write_fd,
            },
        );
        let vm_state = self
            .vms
            .get_mut(&request.vm_id)
            .expect("VM should exist after validation");
        vm_state
            .active_executions
            .insert(started.execution_id.clone());

        self.emit_structured(
            &request.vm_id,
            "browser.worker.spawned",
            BTreeMap::from([
                (String::from("context_id"), request.context_id),
                (String::from("execution_id"), started.execution_id.clone()),
                (
                    String::from("runtime"),
                    runtime_label(context.runtime).to_string(),
                ),
                (String::from("worker_id"), worker_id),
            ]),
        )?;
        self.emit_lifecycle(
            &request.vm_id,
            LifecycleState::Busy,
            Some(String::from(
                "browser sidecar is coordinating guest execution on the main thread",
            )),
        )?;

        Ok(started)
    }

    fn configure_process_stdio(
        kernel: &mut BrowserKernel,
        kernel_pid: u32,
    ) -> Result<u32, BrowserSidecarError> {
        let (stdin_read_fd, stdin_write_fd) = kernel
            .open_pipe(BROWSER_WORKER_DRIVER, kernel_pid)
            .map_err(Self::kernel_error)?;
        kernel
            .fd_dup2(BROWSER_WORKER_DRIVER, kernel_pid, stdin_read_fd, 0)
            .map_err(Self::kernel_error)?;
        let (_stdout_read_fd, stdout_write_fd) = kernel
            .open_pipe(BROWSER_WORKER_DRIVER, kernel_pid)
            .map_err(Self::kernel_error)?;
        kernel
            .fd_dup2(BROWSER_WORKER_DRIVER, kernel_pid, stdout_write_fd, 1)
            .map_err(Self::kernel_error)?;
        let (_stderr_read_fd, stderr_write_fd) = kernel
            .open_pipe(BROWSER_WORKER_DRIVER, kernel_pid)
            .map_err(Self::kernel_error)?;
        kernel
            .fd_dup2(BROWSER_WORKER_DRIVER, kernel_pid, stderr_write_fd, 2)
            .map_err(Self::kernel_error)?;
        Ok(stdin_write_fd)
    }

    fn cleanup_pending_kernel_process(
        kernel: &mut BrowserKernel,
        kernel_pid: u32,
    ) -> Result<(), BrowserSidecarError> {
        kernel
            .exit_process(BROWSER_WORKER_DRIVER, kernel_pid, 1)
            .map_err(Self::kernel_error)?;
        kernel.waitpid(kernel_pid).map_err(Self::kernel_error)?;
        Ok(())
    }

    pub fn write_stdin(
        &mut self,
        request: WriteExecutionStdinRequest,
    ) -> Result<(), BrowserSidecarError> {
        self.ensure_execution(&request.vm_id, &request.execution_id)?;
        let execution = self.ensure_execution_state(&request.vm_id, &request.execution_id)?;
        {
            let vm = self.vm_mut(&request.vm_id)?;
            vm.kernel
                .fd_write(
                    BROWSER_WORKER_DRIVER,
                    execution.kernel_pid,
                    execution.stdin_write_fd,
                    &request.chunk,
                )
                .map_err(Self::kernel_error)?;
        }
        self.bridge.write_stdin(request).map_err(Self::bridge_error)
    }

    pub fn close_stdin(
        &mut self,
        request: ExecutionHandleRequest,
    ) -> Result<(), BrowserSidecarError> {
        self.ensure_execution(&request.vm_id, &request.execution_id)?;
        let execution = self.ensure_execution_state(&request.vm_id, &request.execution_id)?;
        {
            let vm = self.vm_mut(&request.vm_id)?;
            vm.kernel
                .fd_close(
                    BROWSER_WORKER_DRIVER,
                    execution.kernel_pid,
                    execution.stdin_write_fd,
                )
                .map_err(Self::kernel_error)?;
        }
        self.bridge.close_stdin(request).map_err(Self::bridge_error)
    }

    pub fn kill_execution(
        &mut self,
        request: KillExecutionRequest,
    ) -> Result<(), BrowserSidecarError> {
        self.ensure_execution(&request.vm_id, &request.execution_id)?;
        let execution = self.ensure_execution_state(&request.vm_id, &request.execution_id)?;
        {
            let vm = self.vm_mut(&request.vm_id)?;
            vm.kernel
                .kill_process(
                    BROWSER_WORKER_DRIVER,
                    execution.kernel_pid,
                    execution_signal_to_kernel(request.signal),
                )
                .map_err(Self::kernel_error)?;
        }
        self.bridge
            .kill_execution(request)
            .map_err(Self::bridge_error)
    }

    pub fn poll_execution_event(
        &mut self,
        request: PollExecutionEventRequest,
    ) -> Result<Option<ExecutionEvent>, BrowserSidecarError> {
        self.ensure_vm(&request.vm_id)?;

        let event = self
            .bridge
            .poll_execution_event(request)
            .map_err(Self::bridge_error)?;

        match &event {
            Some(ExecutionEvent::Stdout(chunk)) => {
                let execution = self.ensure_execution_state(&chunk.vm_id, &chunk.execution_id)?;
                let vm = self.vm_mut(&chunk.vm_id)?;
                vm.kernel
                    .write_process_stdout(BROWSER_WORKER_DRIVER, execution.kernel_pid, &chunk.chunk)
                    .map_err(Self::kernel_error)?;
            }
            Some(ExecutionEvent::Stderr(chunk)) => {
                let execution = self.ensure_execution_state(&chunk.vm_id, &chunk.execution_id)?;
                let vm = self.vm_mut(&chunk.vm_id)?;
                vm.kernel
                    .write_process_stderr(BROWSER_WORKER_DRIVER, execution.kernel_pid, &chunk.chunk)
                    .map_err(Self::kernel_error)?;
            }
            Some(ExecutionEvent::Exited(exited)) => {
                let execution = self.ensure_execution_state(&exited.vm_id, &exited.execution_id)?;
                {
                    let vm = self.vm_mut(&exited.vm_id)?;
                    vm.kernel
                        .exit_process(
                            BROWSER_WORKER_DRIVER,
                            execution.kernel_pid,
                            exited.exit_code,
                        )
                        .map_err(Self::kernel_error)?;
                }
                self.release_execution(&exited.execution_id, "browser.worker.reaped")?;
            }
            Some(ExecutionEvent::GuestRequest(_)) | None => {}
        }

        Ok(event)
    }

    fn register_context(
        &mut self,
        vm_id: String,
        handle: GuestContextHandle,
        entrypoint: BrowserWorkerEntrypoint,
    ) -> Result<(), BrowserSidecarError> {
        self.contexts.insert(
            handle.context_id.clone(),
            ContextState {
                vm_id: vm_id.clone(),
                runtime: handle.runtime,
                entrypoint,
            },
        );
        let vm_state = self
            .vms
            .get_mut(&vm_id)
            .expect("VM should exist while registering a guest context");
        vm_state.contexts.insert(handle.context_id.clone());

        self.emit_structured(
            &vm_id,
            "browser.context.created",
            BTreeMap::from([
                (String::from("context_id"), handle.context_id),
                (
                    String::from("runtime"),
                    runtime_label(handle.runtime).to_string(),
                ),
            ]),
        )
    }

    fn release_execution(
        &mut self,
        execution_id: &str,
        event_name: &'static str,
    ) -> Result<(), BrowserSidecarError> {
        let Some(execution) = self.executions.remove(execution_id) else {
            return Ok(());
        };

        if let Some(vm_state) = self.vms.get_mut(&execution.vm_id) {
            vm_state.active_executions.remove(execution_id);
        }

        let vm_id = execution.vm_id;
        let runtime = execution.worker.runtime;
        let worker_id = execution.worker.worker_id;
        self.bridge
            .terminate_worker(BrowserWorkerHandleRequest {
                vm_id: vm_id.clone(),
                execution_id: execution_id.to_string(),
                worker_id: worker_id.clone(),
            })
            .map_err(Self::bridge_error)?;

        self.emit_structured(
            &vm_id,
            event_name,
            BTreeMap::from([
                (String::from("execution_id"), execution_id.to_string()),
                (String::from("runtime"), runtime_label(runtime).to_string()),
                (String::from("worker_id"), worker_id),
            ]),
        )?;

        let next_state = if self.active_worker_count(&vm_id) == 0 {
            LifecycleState::Ready
        } else {
            LifecycleState::Busy
        };
        self.emit_lifecycle(
            &vm_id,
            next_state,
            Some(String::from(
                "browser sidecar worker bookkeeping was updated on the main thread",
            )),
        )
    }

    fn ensure_vm(&self, vm_id: &str) -> Result<(), BrowserSidecarError> {
        if self.vms.contains_key(vm_id) {
            Ok(())
        } else {
            Err(BrowserSidecarError::InvalidState(format!(
                "unknown browser sidecar VM: {vm_id}"
            )))
        }
    }

    fn ensure_execution(&self, vm_id: &str, execution_id: &str) -> Result<(), BrowserSidecarError> {
        let execution = self.executions.get(execution_id).ok_or_else(|| {
            BrowserSidecarError::InvalidState(format!(
                "unknown browser sidecar execution: {execution_id}"
            ))
        })?;

        if execution.vm_id == vm_id {
            Ok(())
        } else {
            Err(BrowserSidecarError::InvalidState(format!(
                "browser sidecar execution {execution_id} belongs to vm {}, not {vm_id}",
                execution.vm_id
            )))
        }
    }

    fn ensure_execution_state(
        &self,
        vm_id: &str,
        execution_id: &str,
    ) -> Result<ExecutionState, BrowserSidecarError> {
        let execution = self.executions.get(execution_id).cloned().ok_or_else(|| {
            BrowserSidecarError::InvalidState(format!(
                "unknown browser sidecar execution: {execution_id}"
            ))
        })?;

        if execution.vm_id == vm_id {
            Ok(execution)
        } else {
            Err(BrowserSidecarError::InvalidState(format!(
                "browser sidecar execution {execution_id} belongs to vm {}, not {vm_id}",
                execution.vm_id
            )))
        }
    }

    fn vm(&self, vm_id: &str) -> Result<&VmState, BrowserSidecarError> {
        self.vms.get(vm_id).ok_or_else(|| {
            BrowserSidecarError::InvalidState(format!("unknown browser sidecar VM: {vm_id}"))
        })
    }

    fn vm_mut(&mut self, vm_id: &str) -> Result<&mut VmState, BrowserSidecarError> {
        self.vms.get_mut(vm_id).ok_or_else(|| {
            BrowserSidecarError::InvalidState(format!("unknown browser sidecar VM: {vm_id}"))
        })
    }

    fn emit_lifecycle(
        &mut self,
        vm_id: &str,
        state: LifecycleState,
        detail: Option<String>,
    ) -> Result<(), BrowserSidecarError> {
        self.bridge
            .emit_lifecycle(LifecycleEventRecord {
                vm_id: vm_id.to_string(),
                state,
                detail,
            })
            .map_err(Self::bridge_error)
    }

    fn emit_structured(
        &mut self,
        vm_id: &str,
        name: &str,
        fields: BTreeMap<String, String>,
    ) -> Result<(), BrowserSidecarError> {
        self.bridge
            .emit_structured_event(StructuredEventRecord {
                vm_id: vm_id.to_string(),
                name: name.to_string(),
                fields,
            })
            .map_err(Self::bridge_error)
    }

    fn bridge_error(error: BridgeError<B>) -> BrowserSidecarError {
        BrowserSidecarError::Bridge(format!("{error:?}"))
    }

    fn kernel_error(error: KernelError) -> BrowserSidecarError {
        BrowserSidecarError::Kernel(error.to_string())
    }
}

impl<B> BrowserExtensionHost for BrowserSidecar<B>
where
    B: BrowserSidecarBridge,
    BridgeError<B>: fmt::Debug,
{
    fn write_file(
        &mut self,
        vm_id: &str,
        path: &str,
        contents: Vec<u8>,
    ) -> Result<(), BrowserSidecarError> {
        BrowserSidecar::write_file(self, vm_id, path, contents)
    }

    fn read_file(&mut self, vm_id: &str, path: &str) -> Result<Vec<u8>, BrowserSidecarError> {
        BrowserSidecar::read_file(self, vm_id, path)
    }

    fn mkdir(
        &mut self,
        vm_id: &str,
        path: &str,
        recursive: bool,
    ) -> Result<(), BrowserSidecarError> {
        BrowserSidecar::mkdir(self, vm_id, path, recursive)
    }

    fn read_dir(&mut self, vm_id: &str, path: &str) -> Result<Vec<String>, BrowserSidecarError> {
        BrowserSidecar::read_dir(self, vm_id, path)
    }

    fn create_javascript_context(
        &mut self,
        request: CreateJavascriptContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError> {
        BrowserSidecar::create_javascript_context(self, request)
    }

    fn create_wasm_context(
        &mut self,
        request: CreateWasmContextRequest,
    ) -> Result<GuestContextHandle, BrowserSidecarError> {
        BrowserSidecar::create_wasm_context(self, request)
    }

    fn start_execution(
        &mut self,
        request: StartExecutionRequest,
    ) -> Result<StartedExecution, BrowserSidecarError> {
        BrowserSidecar::start_execution(self, request)
    }

    fn write_stdin(
        &mut self,
        request: WriteExecutionStdinRequest,
    ) -> Result<(), BrowserSidecarError> {
        BrowserSidecar::write_stdin(self, request)
    }

    fn close_stdin(&mut self, request: ExecutionHandleRequest) -> Result<(), BrowserSidecarError> {
        BrowserSidecar::close_stdin(self, request)
    }

    fn kill_execution(&mut self, request: KillExecutionRequest) -> Result<(), BrowserSidecarError> {
        BrowserSidecar::kill_execution(self, request)
    }

    fn poll_execution_event(
        &mut self,
        request: PollExecutionEventRequest,
    ) -> Result<Option<ExecutionEvent>, BrowserSidecarError> {
        BrowserSidecar::poll_execution_event(self, request)
    }
}

fn runtime_label(runtime: GuestRuntime) -> &'static str {
    match runtime {
        GuestRuntime::JavaScript => "javascript",
        GuestRuntime::WebAssembly => "webassembly",
    }
}

fn execution_signal_to_kernel(signal: secure_exec_bridge::ExecutionSignal) -> i32 {
    match signal {
        secure_exec_bridge::ExecutionSignal::Terminate => 15,
        secure_exec_bridge::ExecutionSignal::Interrupt => 2,
        secure_exec_bridge::ExecutionSignal::Kill => 9,
    }
}

#[cfg(test)]
impl<B> BrowserSidecar<B>
where
    B: BrowserSidecarBridge,
    BridgeError<B>: fmt::Debug,
{
    /// Test-only: number of entries still tracked in the global `contexts` map.
    pub(crate) fn test_total_context_count(&self) -> usize {
        self.contexts.len()
    }

    /// Test-only: number of entries still tracked in the global `executions` map.
    pub(crate) fn test_total_execution_count(&self) -> usize {
        self.executions.len()
    }

    /// Test-only: inject a context directly into both the global `contexts` map
    /// and the owning VM's context set, bypassing the bridge round-trip so a
    /// dispose-path test can exercise cleanup at the smallest seam.
    pub(crate) fn test_insert_context(&mut self, vm_id: &str, context_id: &str) {
        self.contexts.insert(
            context_id.to_string(),
            ContextState {
                vm_id: vm_id.to_string(),
                runtime: GuestRuntime::JavaScript,
                entrypoint: BrowserWorkerEntrypoint::JavaScript {
                    bootstrap_module: None,
                },
            },
        );
        if let Some(vm) = self.vms.get_mut(vm_id) {
            vm.contexts.insert(context_id.to_string());
        }
    }

    /// Test-only: inject an active execution directly into both the global
    /// `executions` map and the owning VM's active-execution set.
    pub(crate) fn test_insert_execution(&mut self, vm_id: &str, execution_id: &str) {
        self.executions.insert(
            execution_id.to_string(),
            ExecutionState {
                vm_id: vm_id.to_string(),
                worker: BrowserWorkerHandle {
                    worker_id: format!("worker-{execution_id}"),
                    runtime: GuestRuntime::JavaScript,
                },
                kernel_pid: 0,
                stdin_write_fd: 0,
            },
        );
        if let Some(vm) = self.vms.get_mut(vm_id) {
            vm.active_executions.insert(execution_id.to_string());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use secure_exec_bridge::{
        ChmodRequest, ClockRequest, CommandPermissionRequest, CreateDirRequest, DiagnosticRecord,
        DirectoryEntry, EnvironmentPermissionRequest, ExecutionHandleRequest, FileMetadata,
        FilesystemPermissionRequest, FilesystemSnapshot, FlushFilesystemStateRequest,
        LoadFilesystemStateRequest, LogRecord, NetworkPermissionRequest, PathRequest,
        PermissionDecision, RandomBytesRequest, ReadDirRequest, ReadFileRequest, RenameRequest,
        ScheduleTimerRequest, ScheduledTimer, SymlinkRequest, TruncateRequest, WriteFileRequest,
    };
    use secure_exec_bridge::{
        ClockBridge, EventBridge, ExecutionBridge, FilesystemBridge, PermissionBridge,
        PersistenceBridge, RandomBridge,
    };
    use secure_exec_kernel::kernel::KernelVmConfig;
    use std::time::SystemTime;

    #[derive(Debug, Clone, PartialEq, Eq)]
    struct TestBridgeError(String);

    /// Minimal bridge whose `terminate_worker` can be forced to fail, used to
    /// drive a mid-dispose error through `release_execution`.
    #[derive(Default)]
    struct TerminateFailingBridge {
        fail_terminate: bool,
    }

    impl BridgeTypes for TerminateFailingBridge {
        type Error = TestBridgeError;
    }

    impl FilesystemBridge for TerminateFailingBridge {
        fn read_file(&mut self, _request: ReadFileRequest) -> Result<Vec<u8>, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn write_file(&mut self, _request: WriteFileRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn stat(&mut self, _request: PathRequest) -> Result<FileMetadata, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn lstat(&mut self, _request: PathRequest) -> Result<FileMetadata, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn read_dir(
            &mut self,
            _request: ReadDirRequest,
        ) -> Result<Vec<DirectoryEntry>, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn create_dir(&mut self, _request: CreateDirRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn remove_file(&mut self, _request: PathRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn remove_dir(&mut self, _request: PathRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn rename(&mut self, _request: RenameRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn symlink(&mut self, _request: SymlinkRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn read_link(&mut self, _request: PathRequest) -> Result<String, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn chmod(&mut self, _request: ChmodRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn truncate(&mut self, _request: TruncateRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn exists(&mut self, _request: PathRequest) -> Result<bool, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
    }

    impl PermissionBridge for TerminateFailingBridge {
        fn check_filesystem_access(
            &mut self,
            _request: FilesystemPermissionRequest,
        ) -> Result<PermissionDecision, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn check_network_access(
            &mut self,
            _request: NetworkPermissionRequest,
        ) -> Result<PermissionDecision, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn check_command_execution(
            &mut self,
            _request: CommandPermissionRequest,
        ) -> Result<PermissionDecision, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn check_environment_access(
            &mut self,
            _request: EnvironmentPermissionRequest,
        ) -> Result<PermissionDecision, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
    }

    impl PersistenceBridge for TerminateFailingBridge {
        fn load_filesystem_state(
            &mut self,
            _request: LoadFilesystemStateRequest,
        ) -> Result<Option<FilesystemSnapshot>, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn flush_filesystem_state(
            &mut self,
            _request: FlushFilesystemStateRequest,
        ) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
    }

    impl ClockBridge for TerminateFailingBridge {
        fn wall_clock(&mut self, _request: ClockRequest) -> Result<SystemTime, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn monotonic_clock(&mut self, _request: ClockRequest) -> Result<Duration, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn schedule_timer(
            &mut self,
            _request: ScheduleTimerRequest,
        ) -> Result<ScheduledTimer, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
    }

    impl RandomBridge for TerminateFailingBridge {
        fn fill_random_bytes(
            &mut self,
            _request: RandomBytesRequest,
        ) -> Result<Vec<u8>, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
    }

    impl EventBridge for TerminateFailingBridge {
        fn emit_structured_event(
            &mut self,
            _event: StructuredEventRecord,
        ) -> Result<(), Self::Error> {
            Ok(())
        }
        fn emit_diagnostic(&mut self, _event: DiagnosticRecord) -> Result<(), Self::Error> {
            Ok(())
        }
        fn emit_log(&mut self, _event: LogRecord) -> Result<(), Self::Error> {
            Ok(())
        }
        fn emit_lifecycle(&mut self, _event: LifecycleEventRecord) -> Result<(), Self::Error> {
            Ok(())
        }
    }

    impl ExecutionBridge for TerminateFailingBridge {
        fn create_javascript_context(
            &mut self,
            _request: CreateJavascriptContextRequest,
        ) -> Result<GuestContextHandle, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn create_wasm_context(
            &mut self,
            _request: CreateWasmContextRequest,
        ) -> Result<GuestContextHandle, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn start_execution(
            &mut self,
            _request: StartExecutionRequest,
        ) -> Result<StartedExecution, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn write_stdin(&mut self, _request: WriteExecutionStdinRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn close_stdin(&mut self, _request: ExecutionHandleRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn kill_execution(&mut self, _request: KillExecutionRequest) -> Result<(), Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
        fn poll_execution_event(
            &mut self,
            _request: PollExecutionEventRequest,
        ) -> Result<Option<ExecutionEvent>, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }
    }

    impl crate::BrowserWorkerBridge for TerminateFailingBridge {
        fn create_worker(
            &mut self,
            _request: BrowserWorkerSpawnRequest,
        ) -> Result<BrowserWorkerHandle, Self::Error> {
            unimplemented!("not exercised by dispose test")
        }

        fn terminate_worker(
            &mut self,
            _request: BrowserWorkerHandleRequest,
        ) -> Result<(), Self::Error> {
            if self.fail_terminate {
                Err(TestBridgeError(String::from("forced terminate failure")))
            } else {
                Ok(())
            }
        }
    }

    // A mid-dispose worker-termination failure must still drain the VM, context,
    // and execution bookkeeping for that id — otherwise the VmState (holding a
    // BrowserKernel) and ContextState leak for the process lifetime.
    #[test]
    fn dispose_vm_drains_maps_even_when_worker_termination_fails() {
        let bridge = TerminateFailingBridge {
            fail_terminate: true,
        };
        let mut sidecar = BrowserSidecar::new(bridge, BrowserSidecarConfig::default());

        sidecar
            .create_vm(KernelVmConfig::new("vm-leak"))
            .expect("create vm");
        sidecar.test_insert_context("vm-leak", "ctx-leak");
        sidecar.test_insert_execution("vm-leak", "exec-leak");

        assert_eq!(sidecar.vm_count(), 1);
        assert_eq!(sidecar.test_total_context_count(), 1);
        assert_eq!(sidecar.test_total_execution_count(), 1);

        // The forced terminate_worker failure surfaces as an error, but the
        // dispose must still have reclaimed every entry for `vm-leak`.
        let result = sidecar.dispose_vm("vm-leak");
        assert!(result.is_err(), "forced terminate failure should surface");

        assert_eq!(sidecar.vm_count(), 0, "VmState leaked after failed dispose");
        assert_eq!(
            sidecar.test_total_context_count(),
            0,
            "ContextState leaked after failed dispose"
        );
        assert_eq!(
            sidecar.test_total_execution_count(),
            0,
            "ExecutionState leaked after failed dispose"
        );
    }
}
