//! Shared state types used across sidecar domain modules.
//!
//! Contains VM state, session state, configuration types, active process/socket
//! types, and other shared data structures extracted from service.rs.

use crate::protocol::{
    EventFrame, GuestRuntimeKind, MountDescriptor, PermissionsPolicy, ProjectedModuleDescriptor,
    RegisterHostCallbacksRequest, ResponseFrame, SidecarRequestFrame, SidecarRequestPayload,
    SidecarResponseFrame, SidecarResponsePayload, SignalHandlerRegistration, SoftwareDescriptor,
    WasmPermissionTier,
};
use crate::wire::DEFAULT_MAX_FRAME_BYTES;
use rusqlite::Connection;
use rustls::{ClientConnection, ServerConnection, StreamOwned};
use secure_exec_bridge::{BridgeTypes, FilesystemSnapshot};
use secure_exec_execution::{
    DeferredSyncRpcResponder, JavascriptExecution, JavascriptSyncRpcRequest, PythonExecution,
    PythonVfsRpcRequest, WasmExecution,
};
use secure_exec_kernel::kernel::{KernelProcessHandle, KernelVm};
use secure_exec_kernel::mount_table::MountTable;
use secure_exec_kernel::root_fs::{RootFileSystem, RootFilesystemMode, RootFilesystemSnapshot};
use secure_exec_kernel::socket_table::SocketId;
use secure_exec_vm_config as vm_config;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::error::Error;
use std::fmt;
use std::fs::File;
use std::net::{IpAddr, SocketAddr, TcpListener, TcpStream, UdpSocket};
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicI64, Ordering};
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, Instant};
use tokio::sync::mpsc::UnboundedSender;

// ---------------------------------------------------------------------------
// Type aliases
// ---------------------------------------------------------------------------

pub(crate) type BridgeError<B> = <B as BridgeTypes>::Error;
pub(crate) type SidecarKernel = KernelVm<MountTable>;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

pub(crate) const EXECUTION_DRIVER_NAME: &str = "secure-exec-sidecar-execution";
pub(crate) const JAVASCRIPT_COMMAND: &str = "node";
pub(crate) const PYTHON_COMMAND: &str = "python";
pub(crate) const WASM_COMMAND: &str = "wasm";
pub(crate) const PYTHON_VFS_RPC_GUEST_ROOT: &str = "/workspace";
pub(crate) const EXECUTION_SANDBOX_ROOT_ENV: &str = "AGENT_OS_SANDBOX_ROOT";
pub(crate) const WASM_STDIO_SYNC_RPC_ENV: &str = "AGENT_OS_WASI_STDIO_SYNC_RPC";
#[cfg(test)]
#[allow(dead_code)]
pub(crate) const HOST_REALPATH_MAX_SYMLINK_DEPTH: usize = 40;
pub(crate) const DISPOSE_VM_SIGTERM_GRACE: std::time::Duration =
    std::time::Duration::from_millis(100);
pub(crate) const DISPOSE_VM_SIGKILL_GRACE: std::time::Duration =
    std::time::Duration::from_millis(100);
pub(crate) const VM_DNS_SERVERS_METADATA_KEY: &str = "network.dns.servers";
#[cfg(test)]
#[allow(dead_code)]
pub(crate) const VM_LISTEN_PORT_MIN_METADATA_KEY: &str = "network.listen.port_min";
#[cfg(test)]
#[allow(dead_code)]
pub(crate) const VM_LISTEN_PORT_MAX_METADATA_KEY: &str = "network.listen.port_max";
pub(crate) const VM_LISTEN_ALLOW_PRIVILEGED_METADATA_KEY: &str = "network.listen.allow_privileged";
pub(crate) const DEFAULT_JAVASCRIPT_NET_BACKLOG: u32 = 511;
pub(crate) const LOOPBACK_EXEMPT_PORTS_ENV: &str = "AGENT_OS_LOOPBACK_EXEMPT_PORTS";
pub(crate) const TOOL_DRIVER_NAME: &str = "secure-exec-host-callbacks";
pub(crate) const MAPPED_HOST_FD_START: u32 = 1_000_000_000;

// ---------------------------------------------------------------------------
// Public API types
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub struct NativeSidecarConfig {
    pub sidecar_id: String,
    pub max_frame_bytes: usize,
    pub compile_cache_root: Option<PathBuf>,
    pub expected_auth_token: Option<String>,
    pub acp_termination_grace: Duration,
}

impl Default for NativeSidecarConfig {
    fn default() -> Self {
        Self {
            sidecar_id: String::from("secure-exec-sidecar"),
            max_frame_bytes: DEFAULT_MAX_FRAME_BYTES,
            compile_cache_root: None,
            expected_auth_token: None,
            acp_termination_grace: Duration::from_secs(3),
        }
    }
}

#[derive(Debug, Clone)]
pub struct DispatchResult {
    pub response: ResponseFrame,
    pub events: Vec<EventFrame>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SidecarError {
    InvalidState(String),
    ProtocolVersionMismatch(String),
    BridgeVersionMismatch(String),
    Conflict(String),
    Unauthorized(String),
    Unsupported(String),
    FrameTooLarge(String),
    Kernel(String),
    Plugin(String),
    Execution(String),
    Bridge(String),
    Io(String),
}

impl fmt::Display for SidecarError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidState(message)
            | Self::ProtocolVersionMismatch(message)
            | Self::BridgeVersionMismatch(message)
            | Self::Conflict(message)
            | Self::Unauthorized(message)
            | Self::Unsupported(message)
            | Self::FrameTooLarge(message)
            | Self::Kernel(message)
            | Self::Plugin(message)
            | Self::Execution(message)
            | Self::Bridge(message)
            | Self::Io(message) => f.write_str(message),
        }
    }
}

impl Error for SidecarError {}

pub trait SidecarRequestTransport: Send + Sync {
    fn send_request(
        &self,
        request: SidecarRequestFrame,
        timeout: Duration,
    ) -> Result<SidecarResponseFrame, SidecarError>;
}

#[derive(Clone)]
pub(crate) struct SharedSidecarRequestClient {
    transport: Option<Arc<dyn SidecarRequestTransport>>,
    next_request_id: Arc<AtomicI64>,
}

impl Default for SharedSidecarRequestClient {
    fn default() -> Self {
        Self {
            transport: None,
            next_request_id: Arc::new(AtomicI64::new(-1)),
        }
    }
}

impl SharedSidecarRequestClient {
    pub(crate) fn set_transport(&mut self, transport: Arc<dyn SidecarRequestTransport>) {
        self.transport = Some(transport);
    }

    pub(crate) fn invoke(
        &self,
        ownership: crate::protocol::OwnershipScope,
        payload: SidecarRequestPayload,
        timeout: Duration,
    ) -> Result<SidecarResponsePayload, SidecarError> {
        let transport = self.transport.as_ref().ok_or_else(|| {
            SidecarError::Unsupported(String::from("sidecar request transport is not configured"))
        })?;
        let request_id = self.next_request_id.fetch_sub(1, Ordering::Relaxed);
        let request = SidecarRequestFrame::new(request_id, ownership.clone(), payload);
        let response = transport.send_request(request, timeout)?;
        if response.request_id != request_id {
            return Err(SidecarError::InvalidState(format!(
                "sidecar response {} did not match request {request_id}",
                response.request_id
            )));
        }
        if response.ownership != ownership {
            return Err(SidecarError::InvalidState(String::from(
                "sidecar response ownership did not match request ownership",
            )));
        }
        Ok(response.payload)
    }
}

// ---------------------------------------------------------------------------
// Bridge wrapper
// ---------------------------------------------------------------------------

pub(crate) struct SharedBridge<B> {
    pub(crate) inner: Arc<Mutex<B>>,
    pub(crate) permissions: Arc<Mutex<BTreeMap<String, PermissionsPolicy>>>,
    #[cfg(test)]
    pub(crate) set_vm_permissions_outcomes: Arc<Mutex<VecDeque<Option<SidecarError>>>>,
}

impl<B> Clone for SharedBridge<B> {
    fn clone(&self) -> Self {
        Self {
            inner: Arc::clone(&self.inner),
            permissions: Arc::clone(&self.permissions),
            #[cfg(test)]
            set_vm_permissions_outcomes: Arc::clone(&self.set_vm_permissions_outcomes),
        }
    }
}

// ---------------------------------------------------------------------------
// Connection / session / VM state
// ---------------------------------------------------------------------------

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct ConnectionState {
    pub(crate) auth_token: String,
    pub(crate) sessions: BTreeSet<String>,
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) struct SessionState {
    pub(crate) connection_id: String,
    pub(crate) placement: crate::protocol::SidecarPlacement,
    pub(crate) metadata: BTreeMap<String, String>,
    pub(crate) vm_ids: BTreeSet<String>,
}

#[allow(dead_code)]
#[derive(Debug, Default, Clone)]
pub(crate) struct VmConfiguration {
    pub(crate) mounts: Vec<MountDescriptor>,
    pub(crate) software: Vec<SoftwareDescriptor>,
    pub(crate) permissions: PermissionsPolicy,
    pub(crate) module_access_cwd: Option<String>,
    pub(crate) instructions: Vec<String>,
    pub(crate) projected_modules: Vec<ProjectedModuleDescriptor>,
    pub(crate) command_permissions: BTreeMap<String, WasmPermissionTier>,
    /// Guest JavaScript host-environment config (platform / module resolution /
    /// builtin allow-list). Set at `create_vm` from `CreateVmConfig.jsRuntime`
    /// and preserved across `configure_vm`. `None` => full Node.js emulation.
    pub(crate) js_runtime: Option<vm_config::JsRuntimeConfig>,
    pub(crate) loopback_exempt_ports: Vec<u16>,
}

#[allow(dead_code)]
pub(crate) struct VmLayerStore {
    pub(crate) next_layer_id: u64,
    pub(crate) layers: BTreeMap<String, VmLayer>,
}

impl Default for VmLayerStore {
    fn default() -> Self {
        Self {
            next_layer_id: 1,
            layers: BTreeMap::new(),
        }
    }
}

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) enum VmLayer {
    Writable(RootFileSystem),
    Snapshot(RootFilesystemSnapshot),
    Overlay(VmOverlayLayer),
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub(crate) struct VmOverlayLayer {
    pub(crate) mode: RootFilesystemMode,
    pub(crate) upper_layer_id: Option<String>,
    pub(crate) lower_layer_ids: Vec<String>,
}

#[allow(dead_code)]
pub(crate) struct VmState {
    pub(crate) connection_id: String,
    pub(crate) session_id: String,
    /// Operator-tunable VM-scoped runtime limits. Immutable for the VM's lifetime;
    /// `ConfigureVm` does not mutate limits.
    pub(crate) limits: crate::limits::VmLimits,
    pub(crate) dns: VmDnsConfig,
    pub(crate) listen_policy: VmListenPolicy,
    pub(crate) create_loopback_exempt_ports: BTreeSet<u16>,
    pub(crate) guest_env: BTreeMap<String, String>,
    pub(crate) requested_runtime: GuestRuntimeKind,
    pub(crate) root_filesystem_mode: RootFilesystemMode,
    pub(crate) guest_cwd: String,
    pub(crate) cwd: PathBuf,
    pub(crate) socket_root: PathBuf,
    pub(crate) host_cwd: PathBuf,
    pub(crate) kernel: SidecarKernel,
    pub(crate) loaded_snapshot: Option<FilesystemSnapshot>,
    pub(crate) configuration: VmConfiguration,
    pub(crate) layers: VmLayerStore,
    pub(crate) command_guest_paths: BTreeMap<String, String>,
    pub(crate) command_permissions: BTreeMap<String, WasmPermissionTier>,
    pub(crate) toolkits: BTreeMap<String, RegisterHostCallbacksRequest>,
    pub(crate) active_processes: BTreeMap<String, ActiveProcess>,
    pub(crate) exited_process_snapshots: VecDeque<ExitedProcessSnapshot>,
    pub(crate) detached_child_processes: BTreeSet<String>,
    pub(crate) signal_states: BTreeMap<String, BTreeMap<u32, SignalHandlerRegistration>>,
    /// Guest host_net fd -> (socket_id, nonblock) registry, shared across all of this VM's processes
    /// (threads). A wasm thread runs in its own V8 isolate with its own per-isolate runner socket
    /// table, so a worker thread cannot see a socket the main thread opened. This lets the worker's
    /// runner resolve the fd to the owning process's socket id (net.resolve_guest_fd) so its socket
    /// ops can be serviced on the owning process. Populated by net.register_guest_fd. Keyed by the
    /// owning (root) process id (the part of the caller's id before `~thread~`) then guest fd.
    pub(crate) guest_net_fds: BTreeMap<String, BTreeMap<u32, GuestNetFdEntry>>,
}

#[derive(Debug, Clone)]
pub(crate) struct GuestNetFdEntry {
    pub(crate) socket_id: String,
    pub(crate) nonblock: bool,
}

#[derive(Debug, Clone)]
pub(crate) struct ExitedProcessSnapshot {
    pub(crate) captured_at: Instant,
    pub(crate) process: crate::protocol::ProcessSnapshotEntry,
}

// ---------------------------------------------------------------------------
// DNS configuration
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Default)]
pub(crate) struct VmDnsConfig {
    pub(crate) name_servers: Vec<SocketAddr>,
    pub(crate) overrides: BTreeMap<String, Vec<IpAddr>>,
}

#[derive(Debug, Clone)]
pub(crate) struct JavascriptSocketPathContext {
    pub(crate) socket_root: PathBuf,
    pub(crate) mounts: Vec<MountDescriptor>,
    pub(crate) listen_policy: VmListenPolicy,
    pub(crate) loopback_exempt_ports: BTreeSet<u16>,
    pub(crate) tcp_loopback_guest_to_host_ports: BTreeMap<(JavascriptSocketFamily, u16), u16>,
    pub(crate) udp_loopback_guest_to_host_ports: BTreeMap<(JavascriptSocketFamily, u16), u16>,
    pub(crate) udp_loopback_host_to_guest_ports: BTreeMap<(JavascriptSocketFamily, u16), u16>,
    pub(crate) used_tcp_guest_ports: BTreeMap<JavascriptSocketFamily, BTreeSet<u16>>,
    pub(crate) used_udp_guest_ports: BTreeMap<JavascriptSocketFamily, BTreeSet<u16>>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum JavascriptSocketFamily {
    Ipv4,
    Ipv6,
}

impl JavascriptSocketFamily {
    pub(crate) fn from_ip(ip: IpAddr) -> Self {
        match ip {
            IpAddr::V4(_) => Self::Ipv4,
            IpAddr::V6(_) => Self::Ipv6,
        }
    }
}

impl From<JavascriptUdpFamily> for JavascriptSocketFamily {
    fn from(value: JavascriptUdpFamily) -> Self {
        match value {
            JavascriptUdpFamily::Ipv4 => Self::Ipv4,
            JavascriptUdpFamily::Ipv6 => Self::Ipv6,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct VmListenPolicy {
    pub(crate) port_min: u16,
    pub(crate) port_max: u16,
    pub(crate) allow_privileged: bool,
}

impl Default for VmListenPolicy {
    fn default() -> Self {
        Self {
            port_min: 1,
            port_max: u16::MAX,
            allow_privileged: false,
        }
    }
}

// ---------------------------------------------------------------------------
// Active process state
// ---------------------------------------------------------------------------

/// Process-scoped "some socket became readable" signal. Each socket reader thread calls `notify()`
/// after delivering data/EOF/error to its per-socket channel; a guest `poll()` that found no fd ready
/// blocks on this (via the `net.poll_wait` sync RPC) until the generation advances. The guest captures
/// the generation BEFORE its non-blocking readiness scan and passes it back, so data that arrived
/// between the scan and the wait is never missed (no lost wakeup). This lets the guest's `net_poll`
/// block on ALL its fds at once (wake-on-any-ready) instead of round-robin-blocking each fd in turn —
/// the round-robin otherwise serializes a multi-client X server's poll and stalls cross-VM rendering.
/// A `net.poll_wait` registered for DATA-driven completion directly by the reader thread (the
/// `SECURE_EXEC_POLL_DIRECT` path). On data, `notify()` completes these inline instead of waking a
/// poll-waiter-pool thread first, removing one scheduler hop from the per-wakeup latency chain. A
/// matching pool entry still handles the deadline; the shared `claimed` flag guarantees exactly one
/// completer (reader OR deadline) fires.
struct DirectPollWaiter {
    responder: DeferredSyncRpcResponder,
    call_id: u64,
    claimed: Arc<std::sync::atomic::AtomicBool>,
}

// ===================================================================================================
// D1 wake-cause profiler (SECURE_EXEC_WAKEPROF=1). Default-OFF, zero-cost when disabled.
//
// Buckets every `net.poll_wait` completion by CAUSE, keyed by the per-process `SocketReadiness`
// pointer. A bimodal split across keys (some processes deadline-dominated = lost/throttled wakes,
// others notify-dominated = healthy) discriminates the lost-wake theories (T-A/T-B) from
// latency/serialization (T-C/T-D/T-E/T-F). This only FLAGS/LOGS; it never completes a wait (that
// would be the banned poll fallback — see CLAUDE.md "Wakeups are event-driven").
// ===================================================================================================

pub(crate) const WAKE_IMMEDIATE: usize = 0; // zero-wait poll (timeout==0)
pub(crate) const WAKE_PRE_ADVANCED: usize = 1; // generation already moved past the guest's scan
pub(crate) const WAKE_DIRECT_NOTIFY: usize = 2; // reader thread completed it inline via notify()
pub(crate) const WAKE_POOL_NOTIFY: usize = 3; // pool worker woke on a real generation change
pub(crate) const WAKE_POOL_DEADLINE: usize = 4; // pool worker woke on timeout (no notify arrived)
pub(crate) const WAKE_INLINE_NOTIFY: usize = 5; // inline-fallback path woke on a generation change
pub(crate) const WAKE_INLINE_DEADLINE: usize = 6; // inline-fallback path woke on timeout

const WAKE_LABELS: [&str; 7] = [
    "immediate",
    "pre_adv",
    "direct_notify",
    "pool_notify",
    "pool_DEADLINE",
    "inline_notify",
    "inline_DEADLINE",
];

pub(crate) fn wakeprof_enabled() -> bool {
    static EN: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *EN.get_or_init(|| std::env::var("SECURE_EXEC_WAKEPROF").map(|v| v == "1").unwrap_or(false))
}

type WakeProfRegistry = std::sync::Mutex<BTreeMap<usize, [u64; 7]>>;

fn wakeprof_registry() -> &'static WakeProfRegistry {
    static REG: std::sync::OnceLock<WakeProfRegistry> = std::sync::OnceLock::new();
    REG.get_or_init(|| std::sync::Mutex::new(BTreeMap::new()))
}

fn wakeprof_pidmap() -> &'static std::sync::Mutex<BTreeMap<usize, u32>> {
    static MAP: std::sync::OnceLock<std::sync::Mutex<BTreeMap<usize, u32>>> =
        std::sync::OnceLock::new();
    MAP.get_or_init(|| std::sync::Mutex::new(BTreeMap::new()))
}

/// Associate a readiness pointer with the owning guest's kernel pid (called from the poll_wait handler
/// where `process` is in scope), so the dump can name each process. Guests are created in launch order,
/// so the pids identify which guest is which (Xvfb, dbus-daemon, xfconfd, xfwm4, ...).
pub(crate) fn wakeprof_set_pid(key: usize, pid: u32) {
    if !wakeprof_enabled() {
        return;
    }
    if let Ok(mut map) = wakeprof_pidmap().lock() {
        map.entry(key).or_insert(pid);
    }
}

fn wakeprof_namemap() -> &'static std::sync::Mutex<BTreeMap<u32, String>> {
    static MAP: std::sync::OnceLock<std::sync::Mutex<BTreeMap<u32, String>>> =
        std::sync::OnceLock::new();
    MAP.get_or_init(|| std::sync::Mutex::new(BTreeMap::new()))
}

/// Associate a kernel pid with its guest entrypoint basename (called at spawn). Lets the dump name
/// each guest (Xvfb.wasm, xfwm4.wasm, …) instead of relying on launch-order pid guesses.
pub(crate) fn wakeprof_set_name(pid: u32, entrypoint: &str) {
    if !wakeprof_enabled() {
        return;
    }
    let name = entrypoint.rsplit('/').next().unwrap_or(entrypoint).to_string();
    if let Ok(mut map) = wakeprof_namemap().lock() {
        map.insert(pid, name);
    }
}

/// Record one poll_wait completion. `key` = `Arc::as_ptr(&readiness) as usize` (stable per process);
/// `cause_idx` = one of the `WAKE_*` constants. Prints a per-process histogram every 5000 wakes.
pub(crate) fn wakeprof_record(key: usize, cause_idx: usize) {
    if !wakeprof_enabled() {
        return;
    }
    static TOTAL: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);
    let n = TOTAL.fetch_add(1, std::sync::atomic::Ordering::Relaxed) + 1;
    if let Ok(mut reg) = wakeprof_registry().lock() {
        let entry = reg.entry(key).or_insert([0u64; 7]);
        if cause_idx < 7 {
            entry[cause_idx] = entry[cause_idx].saturating_add(1);
        }
        if n % 5000 == 0 {
            wakeprof_dump(&reg);
        }
    }
}

// [deadprobe] D1 Phase-1 mechanism confirmation (SECURE_EXEC_DEADLINE_PROBE=1, default-OFF; run alongside
// SECURE_EXEC_WAKEPROF=1 so the pid/name maps are populated). At the moment a net.poll_wait is ABOUT TO BLOCK
// (guest drained nothing, readiness unchanged), the handler non-blocking-polls this process's socket OS
// buffers. If data is ALREADY readable there, the per-socket reader thread is BEHIND — a client's bytes sit
// unread in the kernel buffer, so the notify will land after the 50ms deadline (a = LATE notify). If empty,
// the block is a genuine no-data wait (b). Per-process, reuses the wakeprof pid/name maps. Diagnostic only.
pub(crate) fn deadline_probe_enabled() -> bool {
    static EN: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *EN.get_or_init(|| {
        std::env::var("SECURE_EXEC_DEADLINE_PROBE").map(|v| v == "1").unwrap_or(false)
    })
}

pub(crate) fn deadline_probe_record(key: usize, os_readable: bool) {
    if !deadline_probe_enabled() {
        return;
    }
    static REG: std::sync::OnceLock<std::sync::Mutex<BTreeMap<usize, [u64; 2]>>> =
        std::sync::OnceLock::new();
    let reg = REG.get_or_init(|| std::sync::Mutex::new(BTreeMap::new()));
    static TOTAL: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);
    let n = TOTAL.fetch_add(1, std::sync::atomic::Ordering::Relaxed) + 1;
    if let Ok(mut reg) = reg.lock() {
        let entry = reg.entry(key).or_insert([0u64; 2]); // [empty, os_readable]
        entry[usize::from(os_readable)] = entry[usize::from(os_readable)].saturating_add(1);
        if n % 2000 == 0 {
            let pids = wakeprof_pidmap().lock().map(|m| m.clone()).unwrap_or_default();
            let names = wakeprof_namemap().lock().map(|m| m.clone()).unwrap_or_default();
            eprintln!(
                "[deadprobe] === block-entry OS-buffer readability (a=LATE-notify if OSdata%% high) ==="
            );
            for (k, c) in reg.iter() {
                let tot = c[0] + c[1];
                if tot == 0 {
                    continue;
                }
                let pid = pids.get(k).copied().unwrap_or(0);
                let name = names.get(&pid).map(|s| s.as_str()).unwrap_or("?");
                eprintln!(
                    "[deadprobe] {:<16} pid={} block_entry_with_OSdata={} empty={} ({:.1}% had-data)",
                    name,
                    pid,
                    c[1],
                    c[0],
                    (c[1] as f64) * 100.0 / (tot as f64)
                );
            }
        }
    }
}

fn wakeprof_dump(reg: &BTreeMap<usize, [u64; 7]>) {
    let pids = wakeprof_pidmap().lock().map(|m| m.clone()).unwrap_or_default();
    let names = wakeprof_namemap().lock().map(|m| m.clone()).unwrap_or_default();
    eprintln!("[wakeprof] === per-process wake-cause histogram (guest, pid, readiness ptr) ===");
    for (key, counts) in reg.iter() {
        let total: u64 = counts.iter().sum();
        if total == 0 {
            continue;
        }
        let pid = pids.get(key).copied().unwrap_or(0);
        let name = names.get(&pid).map(|s| s.as_str()).unwrap_or("?");
        let deadline = counts[WAKE_POOL_DEADLINE] + counts[WAKE_INLINE_DEADLINE];
        let notify =
            counts[WAKE_DIRECT_NOTIFY] + counts[WAKE_POOL_NOTIFY] + counts[WAKE_INLINE_NOTIFY];
        let pct_deadline = (deadline as f64) * 100.0 / (total as f64);
        let parts: Vec<String> = counts
            .iter()
            .enumerate()
            .filter(|(_, c)| **c > 0)
            .map(|(i, c)| format!("{}={}", WAKE_LABELS[i], c))
            .collect();
        eprintln!(
            "[wakeprof] {:<16} pid={} proc@{:#018x} total={} deadline%={:.1} notify={} | {}",
            name,
            pid,
            key,
            total,
            pct_deadline,
            notify,
            parts.join(" ")
        );
    }
}

/// D12 hop-profiler gate (SECURE_EXEC_HOPPROF=1, default-OFF). Decomposes ONE productive
/// `net.poll_wait` hop into its host-side segments on the shared `perf_now_micros` clock so the FAT
/// segment of the ~1.5-2ms hop is visible: (a) register->notify = legitimate cross-guest peer wait
/// (NOT overhead), (b) notify->pool-resume = condvar wake latency (overhead suspect), (c)
/// resume->responded = respond_success/channel-send cost (overhead). Anything in the guest-observed
/// RTPROBE total NOT covered by a+b+c is the residual guest send + response-channel wake. Cheap
/// OnceLock gate keeps the default path byte-for-byte unchanged.
pub(crate) fn hopprof_enabled() -> bool {
    static EN: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *EN.get_or_init(|| std::env::var("SECURE_EXEC_HOPPROF").map(|v| v == "1").unwrap_or(false))
}

/// GAPTRACE (SECURE_EXEC_GAPTRACE=1, default-OFF): per-event (NOT aggregated) log of each productive
/// blocking poll_wait's segments with a `perf_now_micros` stamp, so an offline decoder can window the
/// stream to the keystroke render (vs the [ir-mark] inject) and see, round-trip by round-trip, where one
/// ~8ms gap goes: `peerWait` (register→notify = waiting for the peer guest to produce data — legitimate
/// cross-guest causality) vs `wakeLag` (notify→resume) + `resp` (respond/channel-send) = the wake-delivery
/// overhead a CORE lever could cut. If peerWait dominates, the fix is the peer's turnaround (recursive
/// wasm overhead); if wakeLag/resp dominate, the wake path is the lever.
pub(crate) fn gaptrace_enabled() -> bool {
    static EN: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *EN.get_or_init(|| std::env::var("SECURE_EXEC_GAPTRACE").map(|v| v == "1").unwrap_or(false))
}

/// L-L fd-scoped poll wakeups (`SECURE_EXEC_FD_SCOPED_POLL=1`, default-OFF). When on, a `net.poll_wait`
/// whose awaited set is purely host-net data sockets only completes when one of THOSE sockets fires
/// (per-key generation), eliminating the ~59% spurious cross-fd wakes the process-wide generation
/// otherwise delivers. Off => the global-generation path, byte-for-byte unchanged.
pub(crate) fn fd_scoped_poll_enabled() -> bool {
    static EN: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *EN.get_or_init(|| std::env::var("SECURE_EXEC_FD_SCOPED_POLL").map(|v| v == "1").unwrap_or(false))
}

/// Allocate a process-unique source key for a socket's fd-scoped readiness (L-L). Monotonic, never 0
/// (0 means "no key" in scoped snapshots), wraps harmlessly far beyond any realistic socket count.
pub(crate) fn next_readiness_key() -> u64 {
    static NEXT: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(1);
    NEXT.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
}

#[derive(Default)]
struct HopProfAcc {
    n: u64,
    wait_us: u128,
    wake_us: u128,
    resp_us: u128,
    wait_max: u64,
    wake_max: u64,
    resp_max: u64,
    /// Productive wakes whose registered_us/notify_us were both stamped (clean segment data).
    clean: u64,
}

fn hopprof_acc() -> &'static Mutex<HopProfAcc> {
    static ACC: std::sync::OnceLock<Mutex<HopProfAcc>> = std::sync::OnceLock::new();
    ACC.get_or_init(|| Mutex::new(HopProfAcc::default()))
}

/// Record one productive poll_wait hop's segments (µs). `wait` = register->notify (peer wait),
/// `wake` = notify->resume (condvar wake), `resp` = resume->responded (send). Prints every 200.
fn hopprof_record(wait: u64, wake: u64, resp: u64, clean: bool) {
    if let Ok(mut a) = hopprof_acc().lock() {
        a.n += 1;
        a.wake_us += wake as u128;
        a.resp_us += resp as u128;
        if wake > a.wake_max {
            a.wake_max = wake;
        }
        if resp > a.resp_max {
            a.resp_max = resp;
        }
        if clean {
            a.clean += 1;
            a.wait_us += wait as u128;
            if wait > a.wait_max {
                a.wait_max = wait;
            }
        }
        if a.n % 200 == 0 {
            let c = a.clean.max(1) as u128;
            let n = a.n as u128;
            eprintln!(
                "[hopprof] n={} clean={} | peerWait avgUs={} maxUs={} | wakeLag(notify->resume) avgUs={} maxUs={} | respond avgUs={} maxUs={}",
                a.n,
                a.clean,
                a.wait_us / c,
                a.wait_max,
                a.wake_us / n,
                a.wake_max,
                a.resp_us / n,
                a.resp_max,
            );
        }
    }
}

/// D13 drain host-service profiler gate (SECURE_EXEC_DRAINHOSTPROF=1, default-OFF). The guest-side
/// DRAINPROF (D8) measures the WHOLE `net.poll` drain round-trip wall (~717us for a tiny X event),
/// while D12 found the poll_wait host bridge plumbing is only ~31us. This gate stamps the host-side
/// service time of the `net.poll` drain handler (entry->exit on the shared `perf_now_micros` clock)
/// so the residual = guest-wall - host-service = V8 sync-call suspend/resume + the time the request
/// queued behind OTHER guests' sync RPCs on the single main thread (the contention F3 attacks). If
/// host-service is ~tens of us, the ~717us is queue+boundary (reduce RPC COUNT, F3's real value); if
/// host-service is most of it, the socket.poll/encode path is the lever. Cheap OnceLock gate.
pub(crate) fn drainhostprof_enabled() -> bool {
    static EN: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *EN.get_or_init(|| {
        std::env::var("SECURE_EXEC_DRAINHOSTPROF").map(|v| v == "1").unwrap_or(false)
    })
}

#[derive(Default)]
struct DrainHostProfAcc {
    n: u64,
    svc_us: u128,
    svc_max: u64,
    data_n: u64,
    data_svc_us: u128,
    data_bytes: u128,
}

fn drainhostprof_acc() -> &'static Mutex<DrainHostProfAcc> {
    static ACC: std::sync::OnceLock<Mutex<DrainHostProfAcc>> = std::sync::OnceLock::new();
    ACC.get_or_init(|| Mutex::new(DrainHostProfAcc::default()))
}

/// Record one `net.poll` drain handler's host-side service time (µs). `had_data`/`bytes` split out
/// the productive drains (the ones F3 would fold away) from empty re-scans. Prints every 200.
pub(crate) fn drainhostprof_record(svc_us: u64, had_data: bool, bytes: usize) {
    if let Ok(mut a) = drainhostprof_acc().lock() {
        a.n += 1;
        a.svc_us += svc_us as u128;
        if svc_us > a.svc_max {
            a.svc_max = svc_us;
        }
        if had_data {
            a.data_n += 1;
            a.data_svc_us += svc_us as u128;
            a.data_bytes += bytes as u128;
        }
        if a.n % 200 == 0 {
            let n = a.n as u128;
            let dn = a.data_n.max(1) as u128;
            eprintln!(
                "[drainhostprof] drains={} | hostSvc avgUs={} maxUs={} | withData={} dataSvcAvgUs={} dataAvgBytes={}",
                a.n,
                a.svc_us / n,
                a.svc_max,
                a.data_n,
                a.data_svc_us / dn,
                a.data_bytes / dn,
            );
        }
    }
}

#[derive(Debug)]
pub(crate) struct SocketReadiness {
    generation: std::sync::Mutex<u64>,
    signal: std::sync::Condvar,
    /// Direct-completion registry (SECURE_EXEC_POLL_DIRECT). Empty/unused on the default path.
    direct: Mutex<Vec<DirectPollWaiter>>,
    /// D12 (SECURE_EXEC_HOPPROF): perf-clock µs of the most recent `notify()`. Only written when the
    /// hop-profiler is on; a pool worker reads it on a productive wake to size the notify->resume
    /// condvar-wake latency. `0` = never notified under the profiler.
    last_notify_us: std::sync::atomic::AtomicU64,
    /// L-L (fd-scoped wakeups, `SECURE_EXEC_FD_SCOPED_POLL`): per-source generation map keyed by a
    /// socket's `readiness_key`. A reader thread bumps its source's gen via `notify_key` BEFORE the
    /// global `notify()` bump, so a scoped waiter woken by the global condvar can tell whether one of
    /// the *specific* fds it awaits actually fired (vs an unrelated event in the same process — the 59%
    /// spurious-wake case). Empty/untouched on the default (non-scoped) path: zero default-path cost.
    key_gens: std::sync::Mutex<std::collections::HashMap<u64, u64>>,
}

impl std::fmt::Debug for DirectPollWaiter {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DirectPollWaiter").field("call_id", &self.call_id).finish()
    }
}

impl SocketReadiness {
    pub(crate) fn new() -> Self {
        Self {
            generation: std::sync::Mutex::new(0),
            signal: std::sync::Condvar::new(),
            direct: Mutex::new(Vec::new()),
            last_notify_us: std::sync::atomic::AtomicU64::new(0),
            key_gens: std::sync::Mutex::new(std::collections::HashMap::new()),
        }
    }

    /// A reader delivered an event: advance the generation and wake any blocked `wait_changed`. On the
    /// direct path, also complete every registered direct waiter inline (the generation just advanced, so
    /// each waiter's `last_seen` differs — same semantics as `wait_changed` returning on any change). The
    /// `claimed` CAS makes this race-safe against the deadline path; responses are sent after the locks
    /// drop so the response delivery never runs under the readiness lock.
    pub(crate) fn notify(&self) {
        // D12: stamp the notify time BEFORE waking waiters so a freshly-woken pool worker reads the
        // moment data became ready (gated; no clock read on the default path).
        if hopprof_enabled() || gaptrace_enabled() {
            self.last_notify_us.store(
                secure_exec_bridge::perf_now_micros(),
                std::sync::atomic::Ordering::Relaxed,
            );
        }
        let new_generation = {
            let mut generation = match self.generation.lock() {
                Ok(generation) => generation,
                Err(_) => return,
            };
            *generation = generation.wrapping_add(1);
            self.signal.notify_all();
            *generation
        };
        // Fast exit when the direct path is unused (default): no allocation, no extra lock contention.
        let to_complete: Vec<(DeferredSyncRpcResponder, u64)> = {
            let mut direct = match self.direct.lock() {
                Ok(direct) => direct,
                Err(_) => return,
            };
            if direct.is_empty() {
                return;
            }
            let drained = std::mem::take(&mut *direct);
            drained
                .into_iter()
                .filter(|w| {
                    w.claimed
                        .compare_exchange(
                            false,
                            true,
                            std::sync::atomic::Ordering::AcqRel,
                            std::sync::atomic::Ordering::Acquire,
                        )
                        .is_ok()
                })
                .map(|w| (w.responder, w.call_id))
                .collect()
        };
        let wake_key = self as *const SocketReadiness as usize;
        for (responder, call_id) in to_complete {
            wakeprof_record(wake_key, WAKE_DIRECT_NOTIFY);
            let _ = responder
                .respond_success(call_id, serde_json::json!({ "generation": new_generation }));
        }
    }

    /// Direct path: atomically (w.r.t. `notify`) either observe that the generation already advanced past
    /// `last_seen` (returns `Some(current)` — the caller answers inline, nothing registered), or register
    /// `waiter` for reader-driven completion (returns `None`). Closes the scan→register lost-wakeup race
    /// because the same `generation` lock that `notify` advances under is held here. Also sweeps already-
    /// claimed (deadline-completed) entries so the registry stays bounded.
    pub(crate) fn register_direct_or_current(
        &self,
        responder: DeferredSyncRpcResponder,
        call_id: u64,
        claimed: Arc<std::sync::atomic::AtomicBool>,
        last_seen: u64,
    ) -> Option<u64> {
        let generation = match self.generation.lock() {
            Ok(generation) => generation,
            Err(_) => return Some(last_seen),
        };
        if *generation != last_seen {
            return Some(*generation);
        }
        if let Ok(mut direct) = self.direct.lock() {
            direct.retain(|w| !w.claimed.load(std::sync::atomic::Ordering::Acquire));
            direct.push(DirectPollWaiter { responder, call_id, claimed });
        }
        None
    }

    /// Non-blocking read of the current readiness generation (e.g. to decide, on the sync-RPC main
    /// thread, whether a `net.poll_wait` can return immediately or must be deferred to a waiter).
    pub(crate) fn snapshot(&self) -> u64 {
        self.generation.lock().map(|g| *g).unwrap_or(0)
    }

    /// Block until the generation differs from `last_seen` or `timeout` elapses; return the generation
    /// observed at return. Returns immediately if it already differs (covers an event delivered between
    /// the caller's scan and this call). Spurious early returns are harmless: the caller rescans.
    pub(crate) fn wait_changed(&self, last_seen: u64, timeout: std::time::Duration) -> (u64, bool) {
        // Returns (generation_observed, changed). `changed == true` means a real notify advanced the
        // generation (a productive wake); `changed == false` means we returned without a generation
        // change (timeout or spurious) — a non-productive wake. The D1 wake-cause profiler maps
        // changed → notify and !changed → deadline.
        let guard = match self.generation.lock() {
            Ok(guard) => guard,
            Err(_) => return (last_seen, false),
        };
        if *guard != last_seen {
            return (*guard, true);
        }
        match self.signal.wait_timeout(guard, timeout) {
            Ok((guard, _)) => {
                let generation = *guard;
                (generation, generation != last_seen)
            }
            Err(_) => (last_seen, false),
        }
    }

    /// L-L fd-scoped wakeups: like `notify()`, but also advance the per-source generation for `key`
    /// FIRST, so a scoped waiter woken by the global bump observes the source change with happens-before
    /// (key write → global bump under its lock → waiter reads global then key). The global `notify()`
    /// still wakes non-scoped waiters and the global condvar, so this is strictly additive.
    pub(crate) fn notify_key(&self, key: u64) {
        if let Ok(mut keys) = self.key_gens.lock() {
            let e = keys.entry(key).or_insert(0);
            *e = e.wrapping_add(1);
        }
        self.notify();
    }

    /// Non-blocking read of a source's per-key generation (0 if never fired). Snapshotted at poll_wait
    /// registration so the pool worker can detect whether THIS source advanced.
    pub(crate) fn snapshot_key(&self, key: u64) -> u64 {
        self.key_gens
            .lock()
            .map(|k| k.get(&key).copied().unwrap_or(0))
            .unwrap_or(0)
    }

    /// L-L scoped block: wake only when one of the AWAITED sources (`keys`, snapshotted at `snaps`)
    /// actually advances, or the deadline elapses. Driven by the global condvar (`wait_changed`): each
    /// global bump is a candidate; if none of the awaited keys changed it was a spurious cross-fd wake,
    /// so we re-block on the remaining timeout instead of returning to the guest. Returns the same
    /// `(generation, changed)` contract as `wait_changed` (changed=true => an awaited source fired).
    pub(crate) fn wait_changed_scoped(
        &self,
        last_seen: u64,
        keys: &[u64],
        snaps: &[u64],
        deadline: std::time::Instant,
    ) -> (u64, bool) {
        let mut seen = last_seen;
        loop {
            let remaining = deadline.saturating_duration_since(std::time::Instant::now());
            if remaining.is_zero() {
                return (seen, false);
            }
            let (generation, changed) = self.wait_changed(seen, remaining);
            if !changed {
                // Global generation did not advance within the slice => deadline/timeout: non-productive.
                return (generation, false);
            }
            seen = generation;
            // Global advanced: did one of OUR sources fire? (Compare current per-key gen vs the snapshot
            // taken at registration.) If so, this is a productive wake; else it was a spurious cross-fd
            // event and we loop to re-block on the remaining time.
            let awaited_fired = keys
                .iter()
                .zip(snaps.iter())
                .any(|(k, s)| self.snapshot_key(*k) != *s);
            if awaited_fired {
                return (generation, true);
            }
        }
    }
}

/// A deferred `net.poll_wait` whose blocking wait runs on a [`PollWaiterPool`] thread instead of the
/// sidecar's single sync-RPC main thread.
pub(crate) struct PendingPollWait {
    /// Off-thread handle that completes the originating sync RPC for the guest.
    pub(crate) responder: DeferredSyncRpcResponder,
    /// The sync-RPC call id to complete.
    pub(crate) call_id: u64,
    /// The originating process's socket-readiness signal to wait on.
    pub(crate) readiness: Arc<SocketReadiness>,
    /// Generation the guest observed before its readiness scan (lost-wakeup guard).
    pub(crate) last_seen: u64,
    /// Hard deadline: complete with the current generation no later than this (the poll clamp).
    pub(crate) deadline: Instant,
    /// D12 (SECURE_EXEC_HOPPROF): perf-clock µs when this wait was registered, else `0`. Used to size
    /// the register->notify "peer wait" segment of a productive hop.
    pub(crate) registered_us: u64,
    /// SECURE_EXEC_POLL_DIRECT: shared single-completer flag. `Some` => the reader thread may complete
    /// this wait directly (data path); the pool worker here only serves the deadline/fallback and must
    /// CAS-win `claimed` before responding so exactly one of {reader, pool} fires. `None` => legacy path
    /// (the pool always completes), byte-for-byte unchanged.
    pub(crate) claimed: Option<Arc<std::sync::atomic::AtomicBool>>,
    /// L-L fd-scoped wakeups: the awaited sources' `readiness_key`s and their generation snapshots taken
    /// at registration. Non-empty => the pool worker completes only when one of these sources fires
    /// (`wait_changed_scoped`), filtering spurious cross-fd wakes. Empty => global-generation wait.
    pub(crate) scoped_keys: Vec<u64>,
    pub(crate) scoped_snaps: Vec<u64>,
}

/// Thread pool that owns the *blocking* part of `net.poll_wait`. The sidecar's single sync-RPC main
/// thread MUST NOT block waiting for a socket to become readable: with several concurrent guests
/// (e.g. a wasm X server plus a window manager plus GTK apps) that block serializes every guest's
/// X round-trips and starves the slowest. Instead the main thread registers the wait here, returns
/// immediately, and a pool worker blocks on the process's [`SocketReadiness`] and delivers the
/// response off-thread the instant a reader notifies (or the poll deadline elapses). Each blocked
/// guest needs at most one in-flight poll, so a modest pool covers a desktop; a backed-up queue only
/// adds bounded latency since every wait is clamped to a few ms.
pub(crate) struct PollWaiterPool {
    queue: Arc<PollQueue>,
}

struct PollQueue {
    inner: Mutex<VecDeque<PendingPollWait>>,
    signal: Condvar,
}

impl PollWaiterPool {
    pub(crate) fn new(workers: usize) -> Self {
        let queue = Arc::new(PollQueue {
            inner: Mutex::new(VecDeque::new()),
            signal: Condvar::new(),
        });
        let workers = workers.max(1);
        for i in 0..workers {
            let queue = Arc::clone(&queue);
            std::thread::Builder::new()
                .name(format!("se-poll-waiter-{i}"))
                .spawn(move || poll_waiter_loop(queue))
                .expect("spawn poll-waiter thread");
        }
        Self { queue }
    }

    /// Default pool size: scale with cores but keep a generous floor so a small VM's X server, WM and
    /// apps never queue behind one another. Tunable via `SECURE_EXEC_POLL_WAITERS`.
    pub(crate) fn with_default_size() -> Self {
        let n = std::env::var("SECURE_EXEC_POLL_WAITERS")
            .ok()
            .and_then(|v| v.parse::<usize>().ok())
            .unwrap_or_else(|| {
                std::thread::available_parallelism()
                    .map(|p| p.get().saturating_mul(2))
                    .unwrap_or(8)
                    .max(16)
            });
        Self::new(n)
    }

    /// Register a deferred poll; a pool worker will complete it.
    pub(crate) fn register(&self, wait: PendingPollWait) {
        let mut q = self.queue.inner.lock().expect("poll-waiter queue poisoned");
        q.push_back(wait);
        self.queue.signal.notify_one();
    }
}

fn poll_waiter_loop(queue: Arc<PollQueue>) {
    loop {
        let wait = {
            let mut q = queue.inner.lock().expect("poll-waiter queue poisoned");
            loop {
                if let Some(w) = q.pop_front() {
                    break w;
                }
                q = queue.signal.wait(q).expect("poll-waiter queue poisoned");
            }
        };
        let (generation, changed) = if wait.scoped_keys.is_empty() {
            let remaining = wait.deadline.saturating_duration_since(Instant::now());
            wait.readiness.wait_changed(wait.last_seen, remaining)
        } else {
            // L-L: complete only when one of the awaited host-net sockets actually fired.
            wait.readiness.wait_changed_scoped(
                wait.last_seen,
                &wait.scoped_keys,
                &wait.scoped_snaps,
                wait.deadline,
            )
        };
        // Direct path: the reader thread may have already completed this wait on data. CAS-claim before
        // responding so we never double-complete; if we lost, the reader served it — skip silently.
        if let Some(claimed) = &wait.claimed {
            if claimed
                .compare_exchange(
                    false,
                    true,
                    std::sync::atomic::Ordering::AcqRel,
                    std::sync::atomic::Ordering::Acquire,
                )
                .is_err()
            {
                continue;
            }
        }
        // Best-effort: a torn-down guest just no-ops inside the responder.
        wakeprof_record(
            std::sync::Arc::as_ptr(&wait.readiness) as usize,
            if changed { WAKE_POOL_NOTIFY } else { WAKE_POOL_DEADLINE },
        );
        // D12 hop decomposition (productive wakes only): resume_us is now; notify_us is when the data
        // became ready; registered_us is when the guest's poll was registered. Capture BEFORE respond
        // so `resp` covers only the respond_success/channel-send cost.
        if gaptrace_enabled() {
            // Count EVERY pool completion (changed=notify vs deadline) so we see whether blocking
            // poll_waits complete by data or by timeout, and how many there are at all.
            use std::sync::atomic::{AtomicU64, Ordering};
            static N_NOTIFY: AtomicU64 = AtomicU64::new(0);
            static N_DEADLINE: AtomicU64 = AtomicU64::new(0);
            if changed {
                N_NOTIFY.fetch_add(1, Ordering::Relaxed);
            } else {
                N_DEADLINE.fetch_add(1, Ordering::Relaxed);
            }
            let tot = N_NOTIFY.load(Ordering::Relaxed) + N_DEADLINE.load(Ordering::Relaxed);
            if tot % 200 == 0 {
                eprintln!(
                    "[gaptrace-count] pool completions: notify={} deadline={}",
                    N_NOTIFY.load(Ordering::Relaxed),
                    N_DEADLINE.load(Ordering::Relaxed)
                );
            }
        }
        let hopprof = changed && (hopprof_enabled() || gaptrace_enabled());
        let resume_us = if hopprof {
            secure_exec_bridge::perf_now_micros()
        } else {
            0
        };
        let _ = wait
            .responder
            .respond_success(wait.call_id, serde_json::json!({ "generation": generation }));
        if hopprof {
            let notify_us = wait
                .readiness
                .last_notify_us
                .load(std::sync::atomic::Ordering::Relaxed);
            let responded_us = secure_exec_bridge::perf_now_micros();
            // Clean segment data needs both a registered_us and a notify_us that bracket this wake.
            let clean = wait.registered_us != 0 && notify_us >= wait.registered_us;
            let wait_seg = if clean {
                notify_us - wait.registered_us
            } else {
                0
            };
            // Guard against a stale/overlapping notify stamp (multiple waiters): clamp to non-negative.
            let wake_seg = resume_us.saturating_sub(notify_us);
            let resp_seg = responded_us.saturating_sub(resume_us);
            if gaptrace_enabled() {
                // Per-event line (raw, no clean filter), epoch-stamped so it can be windowed to the
                // [ir-mark] inject (epoch wall, cross-process comparable — perf_now is per-process).
                let epoch = std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .map(|d| d.as_micros())
                    .unwrap_or(0);
                eprintln!(
                    "[gaptrace] epoch={} call={} reg={} notify={} resume={} peerWaitUs={} wakeLagUs={} respUs={} clean={}",
                    epoch, wait.call_id, wait.registered_us, notify_us, resume_us, wait_seg, wake_seg, resp_seg, clean
                );
            }
            if hopprof_enabled() {
                hopprof_record(wait_seg, wake_seg, resp_seg, clean);
            }
        }
    }
}

#[allow(dead_code)]
pub(crate) struct ActiveProcess {
    pub(crate) kernel_pid: u32,
    pub(crate) kernel_handle: KernelProcessHandle,
    pub(crate) kernel_stdin_writer_fd: Option<u32>,
    /// For a child spawned with stdio "pty": the PTY master fd (in the *parent's* kernel fd table)
    /// that pairs with this child's slave stdin/stdout/stderr. The terminal emulator drives it via
    /// __pty_read/__pty_write; tracked here so the parent can release it when the child is reaped.
    pub(crate) pty_master_fd: Option<u32>,
    pub(crate) runtime: GuestRuntimeKind,
    pub(crate) detached: bool,
    /// True for a wasi-threads worker: this process SHARES another process's `kernel_pid` (the thread
    /// group leader). On exit it must only end its own session — it must NOT finish/reap the shared
    /// kernel process or terminate its children, or it would kill the leader and its sibling threads.
    pub(crate) is_thread: bool,
    pub(crate) execution: ActiveExecution,
    pub(crate) guest_cwd: String,
    pub(crate) env: BTreeMap<String, String>,
    pub(crate) host_cwd: PathBuf,
    pub(crate) mapped_host_fds: BTreeMap<u32, ActiveMappedHostFd>,
    pub(crate) next_mapped_host_fd: u32,
    pub(crate) pending_execution_events: VecDeque<ActiveExecutionEvent>,
    pub(crate) pending_self_signal_exit: Option<i32>,
    pub(crate) child_processes: BTreeMap<String, ActiveProcess>,
    pub(crate) next_child_process_id: usize,
    pub(crate) http_servers: BTreeMap<u64, ActiveHttpServer>,
    pub(crate) pending_http_requests: BTreeMap<(u64, u64), Option<String>>,
    pub(crate) http2: ActiveHttp2State,
    pub(crate) tcp_listeners: BTreeMap<String, ActiveTcpListener>,
    pub(crate) next_tcp_listener_id: usize,
    pub(crate) tcp_sockets: BTreeMap<String, ActiveTcpSocket>,
    pub(crate) next_tcp_socket_id: usize,
    pub(crate) tcp_port_reservations: BTreeMap<String, (JavascriptSocketFamily, u16)>,
    pub(crate) next_tcp_port_reservation_id: usize,
    pub(crate) unix_listeners: BTreeMap<String, ActiveUnixListener>,
    pub(crate) next_unix_listener_id: usize,
    pub(crate) unix_sockets: BTreeMap<String, ActiveUnixSocket>,
    pub(crate) next_unix_socket_id: usize,
    pub(crate) udp_sockets: BTreeMap<String, ActiveUdpSocket>,
    pub(crate) next_udp_socket_id: usize,
    pub(crate) cipher_sessions: BTreeMap<u64, ActiveCipherSession>,
    pub(crate) next_cipher_session_id: u64,
    pub(crate) diffie_hellman_sessions: BTreeMap<u64, ActiveDiffieHellmanSession>,
    pub(crate) next_diffie_hellman_session_id: u64,
    pub(crate) sqlite_databases: BTreeMap<u64, ActiveSqliteDatabase>,
    pub(crate) next_sqlite_database_id: u64,
    pub(crate) sqlite_statements: BTreeMap<u64, ActiveSqliteStatement>,
    pub(crate) next_sqlite_statement_id: u64,
    /// Per-process module resolution cache, persisted across module sync-RPCs
    /// (`__resolve_module` / `__load_file` / `__module_format` /
    /// `__batch_resolve_modules`) for the lifetime of this process so cold-start
    /// resolution does not rebuild it on every dispatch. The resolver reads the
    /// kernel VFS; the node_modules tree is mounted read-only, so cached
    /// stat/exists/package.json results under it stay valid for the process run.
    pub(crate) module_resolution_cache: secure_exec_execution::LocalModuleResolutionCache,
    /// Shared "some socket became readable" signal; cloned into each TCP/unix socket reader thread so
    /// a blocked guest `net.poll_wait` wakes the instant any of this process's sockets has data.
    pub(crate) socket_readiness: std::sync::Arc<SocketReadiness>,
    /// Inline-net-drain registry mirroring `unix_sockets`, shared with this
    /// process's event-bridge thread so it can service the hot non-blocking
    /// `net.poll` drain off the single service loop (F10-INLINE). Populated on
    /// every unix-socket insert and cleared on every remove.
    pub(crate) unix_inline_registry: UnixInlineRegistry,
    /// Inline-accept registry mapping this process's unix `listener_id` → the host
    /// listener's raw fd, shared with this process's event-bridge thread (and its
    /// worker threads) so the non-blocking `net.server_accept` "nothing pending"
    /// case is serviced off the single service loop (T-accept). Populated on every
    /// unix-listener insert and cleared on every remove.
    pub(crate) unix_listener_fd_registry: UnixListenerFdRegistry,
}

pub(crate) struct ActiveMappedHostFd {
    pub(crate) file: File,
    pub(crate) path: PathBuf,
}

pub(crate) struct ActiveCipherSession {
    pub(crate) algorithm: String,
    pub(crate) auth_tag_len: usize,
    pub(crate) context: openssl::symm::Crypter,
}

pub(crate) struct ActiveSqliteDatabase {
    pub(crate) connection: Connection,
    pub(crate) host_path: Option<PathBuf>,
    pub(crate) vm_path: Option<String>,
    pub(crate) dirty: bool,
    pub(crate) transaction_depth: usize,
    pub(crate) read_only: bool,
}

#[derive(Clone)]
pub(crate) struct ActiveSqliteStatement {
    pub(crate) database_id: u64,
    pub(crate) sql: String,
    pub(crate) return_arrays: bool,
    pub(crate) read_bigints: bool,
    pub(crate) allow_bare_named_parameters: bool,
    pub(crate) allow_unknown_named_parameters: bool,
}

pub(crate) enum ActiveDiffieHellmanSession {
    Dh(ActiveDhSession),
    Ecdh(ActiveEcdhSession),
}

pub(crate) struct ActiveDhSession {
    pub(crate) params: openssl::dh::Dh<openssl::pkey::Params>,
    pub(crate) key_pair: Option<openssl::dh::Dh<openssl::pkey::Private>>,
}

pub(crate) struct ActiveEcdhSession {
    pub(crate) curve: String,
    pub(crate) key_pair: Option<openssl::ec::EcKey<openssl::pkey::Private>>,
}

#[derive(Debug, Clone, Copy, Default)]
pub(crate) struct NetworkResourceCounts {
    pub(crate) sockets: usize,
    pub(crate) connections: usize,
}

#[derive(Debug)]
pub(crate) struct ActiveHttpServer {
    pub(crate) listener: TcpListener,
    pub(crate) guest_local_addr: SocketAddr,
    pub(crate) next_request_id: u64,
}

#[derive(Clone, Default)]
pub(crate) struct ActiveHttp2State {
    pub(crate) shared: Arc<Mutex<Http2SharedState>>,
}

#[derive(Default)]
pub(crate) struct Http2SharedState {
    pub(crate) next_session_id: u64,
    pub(crate) next_stream_id: u64,
    pub(crate) servers: BTreeMap<u64, ActiveHttp2Server>,
    pub(crate) sessions: BTreeMap<u64, ActiveHttp2Session>,
    pub(crate) streams: BTreeMap<u64, ActiveHttp2Stream>,
    pub(crate) server_events: BTreeMap<u64, VecDeque<Http2BridgeEvent>>,
    pub(crate) session_events: BTreeMap<u64, VecDeque<Http2BridgeEvent>>,
}

#[derive(Debug)]
pub(crate) struct ActiveHttp2Server {
    pub(crate) actual_local_addr: SocketAddr,
    pub(crate) guest_local_addr: SocketAddr,
    pub(crate) secure: bool,
    pub(crate) tls: Option<JavascriptTlsBridgeOptions>,
    pub(crate) closed: Arc<AtomicBool>,
}

#[derive(Debug, Clone)]
pub(crate) struct ActiveHttp2Session {
    pub(crate) command_tx: UnboundedSender<Http2SessionCommand>,
}

#[derive(Debug, Clone)]
pub(crate) struct ActiveHttp2Stream {
    pub(crate) session_id: u64,
    pub(crate) paused: Arc<AtomicBool>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub(crate) struct Http2SocketSnapshot {
    pub(crate) encrypted: bool,
    pub(crate) allow_half_open: bool,
    pub(crate) local_address: Option<String>,
    pub(crate) local_port: Option<u16>,
    pub(crate) local_family: Option<String>,
    pub(crate) remote_address: Option<String>,
    pub(crate) remote_port: Option<u16>,
    pub(crate) remote_family: Option<String>,
    pub(crate) servername: Option<String>,
    pub(crate) alpn_protocol: Option<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub(crate) struct Http2RuntimeSnapshot {
    pub(crate) effective_local_window_size: u32,
    pub(crate) local_window_size: u32,
    pub(crate) remote_window_size: u32,
    pub(crate) next_stream_id: u32,
    pub(crate) outbound_queue_size: u32,
    pub(crate) deflate_dynamic_table_size: u32,
    pub(crate) inflate_dynamic_table_size: u32,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub(crate) struct Http2SessionSnapshot {
    pub(crate) encrypted: bool,
    pub(crate) alpn_protocol: Option<String>,
    pub(crate) origin_set: Vec<String>,
    pub(crate) local_settings: BTreeMap<String, Value>,
    pub(crate) remote_settings: BTreeMap<String, Value>,
    pub(crate) state: Http2RuntimeSnapshot,
    pub(crate) socket: Http2SocketSnapshot,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub(crate) struct Http2BridgeEvent {
    pub(crate) kind: String,
    pub(crate) id: u64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) data: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) extra: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) extra_number: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) extra_headers: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) flags: Option<u64>,
}

pub(crate) enum Http2SessionCommand {
    Request {
        headers_json: String,
        options_json: String,
        respond_to: Sender<Result<Value, String>>,
    },
    Settings {
        settings_json: String,
        respond_to: Sender<Result<Value, String>>,
    },
    SetLocalWindowSize {
        size: u32,
        respond_to: Sender<Result<Value, String>>,
    },
    Goaway {
        error_code: u32,
        last_stream_id: u32,
        opaque_data: Option<Vec<u8>>,
        respond_to: Sender<Result<Value, String>>,
    },
    Close {
        abrupt: bool,
        respond_to: Sender<Result<Value, String>>,
    },
    StreamRespond {
        stream_id: u64,
        headers_json: String,
        respond_to: Sender<Result<Value, String>>,
    },
    StreamPush {
        stream_id: u64,
        headers_json: String,
        respond_to: Sender<Result<Value, String>>,
    },
    StreamWrite {
        stream_id: u64,
        chunk: Vec<u8>,
        end_stream: bool,
        respond_to: Sender<Result<Value, String>>,
    },
    StreamClose {
        stream_id: u64,
        error_code: Option<u32>,
        respond_to: Sender<Result<Value, String>>,
    },
    StreamRespondWithFile {
        stream_id: u64,
        body: Vec<u8>,
        headers_json: String,
        options_json: String,
        respond_to: Sender<Result<Value, String>>,
    },
}

// ---------------------------------------------------------------------------
// TCP types
// ---------------------------------------------------------------------------

#[derive(Debug)]
pub(crate) enum JavascriptTcpListenerEvent {
    Connection(PendingTcpSocket),
    Error {
        code: Option<String>,
        message: String,
    },
}

#[derive(Debug)]
pub(crate) struct PendingTcpSocket {
    pub(crate) stream: Option<TcpStream>,
    pub(crate) kernel_socket_id: Option<SocketId>,
    pub(crate) preallocated: bool,
    pub(crate) guest_local_addr: SocketAddr,
    pub(crate) guest_remote_addr: SocketAddr,
}

#[derive(Debug)]
pub(crate) enum JavascriptTcpSocketEvent {
    Data(Vec<u8>),
    End,
    Close {
        had_error: bool,
    },
    Error {
        code: Option<String>,
        message: String,
    },
}

#[derive(Debug)]
pub(crate) struct ActiveTcpSocket {
    pub(crate) stream: Option<Arc<Mutex<TcpStream>>>,
    pub(crate) pending_read_stream: Option<Arc<Mutex<Option<TcpStream>>>>,
    pub(crate) events: Option<Receiver<JavascriptTcpSocketEvent>>,
    pub(crate) event_sender: Option<Sender<JavascriptTcpSocketEvent>>,
    pub(crate) kernel_socket_id: Option<SocketId>,
    pub(crate) no_delay: bool,
    pub(crate) keep_alive: bool,
    pub(crate) keep_alive_initial_delay_secs: Option<u64>,
    pub(crate) guest_local_addr: SocketAddr,
    pub(crate) guest_remote_addr: SocketAddr,
    pub(crate) listener_id: Option<String>,
    pub(crate) tls_mode: Arc<AtomicBool>,
    pub(crate) tls_stream: Arc<Mutex<Option<ActiveTlsStream>>>,
    pub(crate) tls_state: Arc<Mutex<Option<ActiveTlsState>>>,
    pub(crate) saw_local_shutdown: Arc<AtomicBool>,
    pub(crate) saw_remote_end: Arc<AtomicBool>,
    pub(crate) close_notified: Arc<AtomicBool>,
    /// Process "socket became readable" signal; the lazily-spawned reader notifies it so a guest
    /// `net.poll_wait` blocked on this process's fds wakes on this socket's data. Defaults to a
    /// standalone signal and is overwritten with the owning process's signal at attach time.
    pub(crate) socket_readiness: Arc<SocketReadiness>,
}

pub(crate) struct LoopbackTlsTransportPair {
    pub(crate) state: Mutex<LoopbackTlsTransportPairState>,
    pub(crate) ready: Condvar,
}

#[derive(Debug, Default)]
pub(crate) struct LoopbackTlsTransportPairState {
    pub(crate) lower_to_higher: VecDeque<u8>,
    pub(crate) higher_to_lower: VecDeque<u8>,
    pub(crate) lower_write_closed: bool,
    pub(crate) higher_write_closed: bool,
    pub(crate) lower_closed: bool,
    pub(crate) higher_closed: bool,
}

pub(crate) struct LoopbackTlsEndpoint {
    pub(crate) pair: Arc<LoopbackTlsTransportPair>,
    pub(crate) is_lower_socket: bool,
}

impl fmt::Debug for LoopbackTlsEndpoint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("LoopbackTlsEndpoint")
            .field("is_lower_socket", &self.is_lower_socket)
            .finish()
    }
}

#[derive(Debug)]
pub(crate) enum ActiveTlsStream {
    Client(StreamOwned<ClientConnection, TcpStream>),
    Server(StreamOwned<ServerConnection, TcpStream>),
    LoopbackClient(StreamOwned<ClientConnection, LoopbackTlsEndpoint>),
    LoopbackServer(StreamOwned<ServerConnection, LoopbackTlsEndpoint>),
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub(crate) struct JavascriptTlsClientHello {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) servername: Option<String>,
    #[serde(
        rename = "ALPNProtocols",
        alias = "ALPNProtocols",
        skip_serializing_if = "Option::is_none"
    )]
    pub(crate) alpn_protocols: Option<Vec<String>>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub(crate) struct JavascriptTlsBridgeOptions {
    pub(crate) is_server: bool,
    pub(crate) servername: Option<String>,
    pub(crate) reject_unauthorized: Option<bool>,
    pub(crate) request_cert: Option<bool>,
    pub(crate) session: Option<String>,
    pub(crate) key: Option<JavascriptTlsMaterial>,
    pub(crate) cert: Option<JavascriptTlsMaterial>,
    pub(crate) ca: Option<JavascriptTlsMaterial>,
    pub(crate) passphrase: Option<String>,
    pub(crate) ciphers: Option<String>,
    #[serde(alias = "ALPNProtocols")]
    pub(crate) alpn_protocols: Option<Vec<String>>,
    pub(crate) min_version: Option<String>,
    pub(crate) max_version: Option<String>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(untagged)]
pub(crate) enum JavascriptTlsMaterial {
    Single(JavascriptTlsDataValue),
    Many(Vec<JavascriptTlsDataValue>),
}

#[derive(Debug, Clone, Deserialize)]
#[serde(tag = "kind", rename_all = "camelCase")]
pub(crate) enum JavascriptTlsDataValue {
    Buffer { data: String },
    String { data: String },
}

#[derive(Debug, Clone, Default)]
pub(crate) struct ActiveTlsState {
    pub(crate) client_hello: Option<JavascriptTlsClientHello>,
    pub(crate) local_certificates: Vec<Vec<u8>>,
    pub(crate) session_reused: bool,
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct ResolvedTcpConnectAddr {
    pub(crate) actual_addr: SocketAddr,
    pub(crate) guest_remote_addr: SocketAddr,
    pub(crate) use_kernel_loopback: bool,
}

#[derive(Debug)]
pub(crate) struct ActiveTcpListener {
    pub(crate) listener: Option<TcpListener>,
    pub(crate) kernel_socket_id: Option<SocketId>,
    pub(crate) local_addr: Option<SocketAddr>,
    pub(crate) guest_local_addr: SocketAddr,
    pub(crate) backlog: usize,
    pub(crate) active_connection_ids: BTreeSet<String>,
}

// ---------------------------------------------------------------------------
// Unix socket types
// ---------------------------------------------------------------------------

#[derive(Debug)]
pub(crate) enum JavascriptUnixListenerEvent {
    Connection(PendingUnixSocket),
    Error {
        code: Option<String>,
        message: String,
    },
}

#[derive(Debug)]
pub(crate) struct PendingUnixSocket {
    pub(crate) stream: UnixStream,
    pub(crate) local_path: Option<String>,
    pub(crate) remote_path: Option<String>,
}

#[derive(Debug)]
pub(crate) struct ActiveUnixSocket {
    pub(crate) stream: Arc<Mutex<UnixStream>>,
    /// Wrapped in `Arc<Mutex<..>>` so the per-session V8/wasm event-bridge thread
    /// can `try_recv` it INLINE (the F10-INLINE net.poll fast path) in parallel
    /// with the single sidecar service loop. A std `mpsc::Receiver` is neither
    /// `Clone` nor `Sync`, so the `Mutex` is what lets two threads reach it; only
    /// one ever holds the lock at a time, and `poll`/`try_poll` each take it only
    /// for the duration of a single `try_recv`/`recv_timeout`.
    pub(crate) events: Arc<Mutex<Receiver<JavascriptTcpSocketEvent>>>,
    pub(crate) event_sender: Sender<JavascriptTcpSocketEvent>,
    pub(crate) listener_id: Option<String>,
    pub(crate) local_path: Option<String>,
    pub(crate) remote_path: Option<String>,
    pub(crate) saw_local_shutdown: Arc<AtomicBool>,
    pub(crate) saw_remote_end: Arc<AtomicBool>,
    pub(crate) close_notified: Arc<AtomicBool>,
    /// L-L fd-scoped wakeups: a process-unique source id for this socket. The reader thread bumps
    /// `SocketReadiness::notify_key(readiness_key)` on data/EOF; the `net.poll_wait` handler maps the
    /// guest's awaited socket ids to these keys so a scoped wait only completes when THIS socket fires.
    pub(crate) readiness_key: u64,
    /// Data-notifier stop flag (T0-root). When a data-notifier thread is spawned for this socket (an
    /// accepted server-side socket, opt-in), it polls the fd for POLLIN and `notify()`s the owner's
    /// readiness on the readable edge so a blocked `net.poll_wait` wakes the instant a request lands
    /// (instead of discovering it up to 50ms late on the next re-poll). Set on Drop so the thread exits.
    pub(crate) data_notifier_stop: Arc<AtomicBool>,
    /// ★ Lever 1 (inline cross-process peer-notify): the readiness of the process on the OTHER end of this
    /// socket (whom to wake when WE write). Set at `net.connect` from the listener-owning process's
    /// readiness (client→server pairing). When present, the `net.write` handler `notify()`s it INLINE after
    /// a successful write, so the peer's blocked `net.poll_wait` wakes the instant the request lands —
    /// scheduling-independent (no notifier thread to starve). `None` when the feature is off / unpaired.
    pub(crate) peer_readiness: Option<Arc<SocketReadiness>>,
}

impl Drop for ActiveUnixSocket {
    fn drop(&mut self) {
        // Stop any data-notifier thread bound to this socket (no-op if none was spawned).
        self.data_notifier_stop.store(true, std::sync::atomic::Ordering::Relaxed);
    }
}

/// One entry in a process's inline-net-drain registry: just enough of an
/// [`ActiveUnixSocket`] for the event-bridge thread to service a non-blocking
/// `net.poll` drain WITHOUT reaching into sidecar process state. The select!
/// task (which owns socket lifecycle) registers an entry on every unix-socket
/// insert and removes it on every unix-socket remove, so the registry mirrors
/// `ActiveProcess::unix_sockets`. The bridge thread only ever `try_recv`s the
/// shared receiver and, on a terminal `Close`, re-queues it via `event_sender`
/// and falls back to the service loop (which owns the actual socket removal).
#[derive(Debug, Clone)]
pub(crate) struct InlineSock {
    pub(crate) events: Arc<Mutex<Receiver<JavascriptTcpSocketEvent>>>,
    pub(crate) event_sender: Sender<JavascriptTcpSocketEvent>,
    pub(crate) remote_path: Option<String>,
    /// The socket's write half (shared `Arc<Mutex<UnixStream>>`), so the per-session bridge thread can
    /// service `net.write` INLINE (F10-INLINE-WRITE) instead of queuing it on the single dispatch task —
    /// removing the ~636µs per-hop pickup latency for the 4.7k net.writes/boot (X requests).
    pub(crate) stream: Arc<Mutex<UnixStream>>,
    /// The PEER's readiness (e.g. the X server) to `notify()` after an inline write lands its request, so
    /// the peer's blocked `net.poll_wait` wakes immediately — the SAME object + notify the on-pump handler
    /// uses (lever 1), so no lost wakeup. `None` when unpaired.
    pub(crate) peer_readiness: Option<Arc<SocketReadiness>>,
}

/// Per-process registry of inline-drainable unix sockets, keyed by the
/// process-local socket id. Shared (`Arc`) between the select! task that
/// populates it and the [`InlineNetDrain`](secure_exec_execution::InlineNetDrain)
/// handed to that process's event-bridge thread.
pub(crate) type UnixInlineRegistry =
    Arc<Mutex<std::collections::HashMap<String, InlineSock>>>;

/// Per-process map of unix `listener_id` → a `try_clone`'d (dup'd) handle to the
/// host `UnixListener`, shared with this process's event-bridge thread (and its
/// worker threads) so the inline `net.server_accept` fast-path (T-accept) can do a
/// NON-CONSUMING readiness check (`poll(POLLIN, 0)`) on the listener directly, off
/// the single shared service loop. Storing an owned dup (rather than a bare raw
/// fd) keeps the poll fully safe (`as_fd()` → `BorrowedFd`, no `unsafe`) and
/// immune to fd-number reuse: the dup is closed only when removed from this map.
/// Populated when a unix listener is created and cleared when it is removed. The
/// same `Arc` is shared between a process and its worker threads so whichever
/// bridge thread services the accept can resolve the listener.
pub(crate) type UnixListenerFdRegistry =
    Arc<Mutex<std::collections::HashMap<String, UnixListener>>>;

#[derive(Debug)]
pub(crate) struct ActiveUnixListener {
    pub(crate) listener: UnixListener,
    pub(crate) path: String,
    pub(crate) backlog: usize,
    pub(crate) active_connection_ids: BTreeSet<String>,
    /// Stop flag for the accept-notifier thread. The notifier (spawned in `net.listen`) wakes the
    /// server's `net.poll_wait` the instant a client connects, so the server accepts immediately
    /// instead of waiting out the poll ceiling (event-driven connection setup — completes the
    /// readiness notify-graph on the accept edge). Set on drop so the thread exits within ~1s.
    pub(crate) accept_notifier_stop: Arc<std::sync::atomic::AtomicBool>,
}

impl Drop for ActiveUnixListener {
    fn drop(&mut self) {
        self.accept_notifier_stop
            .store(true, std::sync::atomic::Ordering::Relaxed);
    }
}

// ---------------------------------------------------------------------------
// UDP types
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum JavascriptUdpFamily {
    Ipv4,
    Ipv6,
}

impl JavascriptUdpFamily {
    pub(crate) fn from_socket_type(value: &str) -> Result<Self, SidecarError> {
        match value {
            "udp4" => Ok(Self::Ipv4),
            "udp6" => Ok(Self::Ipv6),
            other => Err(SidecarError::InvalidState(format!(
                "unsupported dgram socket type {other}"
            ))),
        }
    }

    pub(crate) fn socket_type(self) -> &'static str {
        match self {
            Self::Ipv4 => "udp4",
            Self::Ipv6 => "udp6",
        }
    }

    pub(crate) fn matches_addr(self, addr: &SocketAddr) -> bool {
        matches!(
            (self, addr),
            (Self::Ipv4, SocketAddr::V4(_)) | (Self::Ipv6, SocketAddr::V6(_))
        )
    }
}

#[derive(Debug)]
pub(crate) enum JavascriptUdpSocketEvent {
    Message {
        data: Vec<u8>,
        remote_addr: SocketAddr,
    },
    Error {
        code: Option<String>,
        message: String,
    },
}

#[derive(Debug)]
pub(crate) struct ActiveUdpSocket {
    pub(crate) family: JavascriptUdpFamily,
    pub(crate) socket: Option<UdpSocket>,
    pub(crate) kernel_socket_id: Option<SocketId>,
    pub(crate) guest_local_addr: Option<SocketAddr>,
    pub(crate) recv_buffer_size: usize,
    pub(crate) send_buffer_size: usize,
}

// ---------------------------------------------------------------------------
// Execution types
// ---------------------------------------------------------------------------

#[derive(Debug)]
pub(crate) enum ActiveExecution {
    Javascript(JavascriptExecution),
    Python(PythonExecution),
    Wasm(Box<WasmExecution>),
    Tool(ToolExecution),
}

#[derive(Debug, Clone)]
pub(crate) struct ToolExecution {
    pub(crate) cancelled: Arc<AtomicBool>,
    pub(crate) pending_events: Arc<Mutex<VecDeque<ActiveExecutionEvent>>>,
    pub(crate) events_overflowed: Arc<AtomicBool>,
}

impl Default for ToolExecution {
    fn default() -> Self {
        Self {
            cancelled: Arc::new(AtomicBool::new(false)),
            pending_events: Arc::new(Mutex::new(VecDeque::new())),
            events_overflowed: Arc::new(AtomicBool::new(false)),
        }
    }
}

#[derive(Debug)]
pub(crate) enum ActiveExecutionEvent {
    Stdout(Vec<u8>),
    Stderr(Vec<u8>),
    JavascriptSyncRpcRequest(JavascriptSyncRpcRequest),
    PythonVfsRpcRequest(Box<PythonVfsRpcRequest>),
    SignalState {
        signal: u32,
        registration: SignalHandlerRegistration,
    },
    Exited(i32),
}

#[derive(Debug)]
pub(crate) struct ProcessEventEnvelope {
    pub(crate) connection_id: String,
    pub(crate) session_id: String,
    pub(crate) vm_id: String,
    pub(crate) process_id: String,
    pub(crate) event: ActiveExecutionEvent,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum SocketQueryKind {
    TcpListener,
    UdpBound,
}

// ---------------------------------------------------------------------------
// Command resolution
// ---------------------------------------------------------------------------

#[derive(Debug)]
pub(crate) struct ResolvedChildProcessExecution {
    pub(crate) command: String,
    pub(crate) process_args: Vec<String>,
    pub(crate) runtime: GuestRuntimeKind,
    pub(crate) entrypoint: String,
    pub(crate) execution_args: Vec<String>,
    pub(crate) env: BTreeMap<String, String>,
    pub(crate) guest_cwd: String,
    pub(crate) host_cwd: PathBuf,
    pub(crate) wasm_permission_tier: Option<WasmPermissionTier>,
    pub(crate) tool_command: bool,
}

// ---------------------------------------------------------------------------
// Utility types
// ---------------------------------------------------------------------------

#[derive(Debug)]
pub(crate) struct ProcNetEntry {
    pub(crate) local_host: String,
    pub(crate) local_port: u16,
    pub(crate) state: String,
    pub(crate) inode: u64,
}

/// Increment 1 diagnostic (TEMPORARY): lock a per-VM mutex, detecting same-thread REENTRANT locks. On the
/// single-threaded dispatch runtime, `try_lock` fails ONLY when this thread already holds the guard (no other
/// thread locks the vms map), so a WouldBlock here IS a reentrant double-lock — the deadlock we are hunting.
/// Panic with a backtrace to pinpoint the exact call chain instead of hanging. Remove once Increment 1 is green.
pub(crate) fn lock_vm(
    arc: &std::sync::Arc<std::sync::Mutex<VmState>>,
) -> std::sync::MutexGuard<'_, VmState> {
    match arc.try_lock() {
        Ok(g) => g,
        Err(std::sync::TryLockError::WouldBlock) => {
            panic!(
                "[REENTRANT-VM-LOCK] same-thread double lock of vm@{:p}:\n{}",
                std::sync::Arc::as_ptr(arc),
                std::backtrace::Backtrace::force_capture()
            );
        }
        Err(std::sync::TryLockError::Poisoned(e)) => e.into_inner(),
    }
}
