//! VM lifecycle functions: create, configure, dispose, bootstrap, snapshot.
//!
//! Extracted from service.rs as part of the service.rs split (Step 0a).
//! Contains VM lifecycle methods on NativeSidecar<B> and associated helpers.

use crate::bootstrap::{
    apply_root_filesystem_entry, discover_command_guest_paths, root_snapshot_entries,
    root_snapshot_entry, root_snapshot_from_entries,
};
use crate::bridge::{bridge_permissions, MountPluginContext};
use crate::protocol::{
    ConfigureVmRequest, CreateLayerRequest, CreateOverlayRequest, DisposeReason, EventFrame,
    ExportSnapshotRequest, ImportSnapshotRequest, LinkPackageRequest, MountDescriptor,
    MountPluginDescriptor, RootFilesystemDescriptor, RootFilesystemEntry,
    RootFilesystemEntryEncoding, RootFilesystemLowerDescriptor, SealLayerRequest,
    SnapshotRootFilesystemRequest, VmLifecycleState,
};
use crate::service::{
    audit_fields, dirname, emit_security_audit_event, emit_structured_event, kernel_error,
    normalize_path, plugin_error, root_filesystem_error, validate_permissions_policy, vfs_error,
};
use crate::state::{
    BridgeError, VmConfiguration, VmDnsConfig, VmListenPolicy, VmState, DISPOSE_VM_SIGKILL_GRACE,
    DISPOSE_VM_SIGTERM_GRACE, EXECUTION_DRIVER_NAME, JAVASCRIPT_COMMAND, PYTHON_COMMAND,
    WASM_COMMAND,
};
use crate::{DispatchResult, NativeSidecar, NativeSidecarBridge, SidecarError};

use base64::Engine;
use secure_exec_bridge::{
    FilesystemSnapshot, FlushFilesystemStateRequest, LifecycleState, LoadFilesystemStateRequest,
};
use secure_exec_kernel::command_registry::CommandDriver;
use secure_exec_kernel::kernel::{KernelVm, KernelVmConfig};
use secure_exec_kernel::mount_plugin::OpenFileSystemPluginRequest;
use secure_exec_kernel::mount_table::{MountOptions, MountTable, MountedFileSystem};
use secure_exec_kernel::permissions::filter_env;
use secure_exec_kernel::resource_accounting::ResourceLimits;
use secure_exec_kernel::root_fs::{
    decode_snapshot_with_import_limits, encode_snapshot as encode_root_snapshot,
    is_supported_root_filesystem_snapshot_format, FilesystemEntryKind as KernelFilesystemEntryKind,
    RootFilesystemImportLimits, ROOT_FILESYSTEM_SNAPSHOT_FORMAT,
};
use secure_exec_sidecar_core::permissions::{allow_all_policy, deny_all_policy};
use secure_exec_sidecar_core::{
    layer_created_response, layer_sealed_response, overlay_created_response,
    package_linked_response, protocol_root_filesystem_mode, root_filesystem_bootstrapped_response,
    root_filesystem_protocol_descriptor_from_config, root_filesystem_snapshot_response,
    snapshot_exported_response, snapshot_imported_response, vm_configured_response,
    vm_created_response, vm_disposed_response, VmLayerStore,
};
use secure_exec_vm_config as vm_config;
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::fmt;
use std::fs;
use std::net::{IpAddr, SocketAddr};
use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

const SHADOW_ROOT_BOOTSTRAP_DIRS: &[(&str, u32)] = &[
    ("/dev", 0o755),
    ("/proc", 0o755),
    ("/tmp", 0o1777),
    ("/bin", 0o755),
    ("/lib", 0o755),
    ("/sbin", 0o755),
    ("/boot", 0o755),
    ("/etc", 0o755),
    ("/root", 0o755),
    ("/run", 0o755),
    ("/srv", 0o755),
    ("/sys", 0o755),
    ("/opt", 0o755),
    ("/mnt", 0o755),
    ("/media", 0o755),
    ("/home", 0o755),
    ("/home/agentos", 0o755),
    ("/usr", 0o755),
    ("/usr/bin", 0o755),
    ("/usr/games", 0o755),
    ("/usr/include", 0o755),
    ("/usr/lib", 0o755),
    ("/usr/libexec", 0o755),
    ("/usr/man", 0o755),
    ("/usr/local", 0o755),
    ("/usr/local/bin", 0o755),
    ("/usr/sbin", 0o755),
    ("/usr/share", 0o755),
    ("/usr/share/man", 0o755),
    ("/var", 0o755),
    ("/var/cache", 0o755),
    ("/var/empty", 0o755),
    ("/var/lib", 0o755),
    ("/var/lock", 0o755),
    ("/var/log", 0o755),
    ("/var/run", 0o755),
    ("/var/spool", 0o755),
    ("/var/tmp", 0o1777),
    ("/etc/agentos", 0o755),
    // Non-Alpine default agent working directory (also present in the base
    // filesystem snapshot); scaffold it here so it exists even when the
    // default base layer is disabled. It is the default cwd and mount root,
    // kept separate from $HOME (/home/agentos).
    ("/workspace", 0o755),
];

pub(crate) const DEFAULT_GUEST_PATH_ENV: &str =
    "/usr/local/sbin:/usr/local/bin:/opt/agentos/bin:/usr/sbin:/usr/bin:/sbin:/bin";
#[cfg(test)]
const KERNEL_COMMAND_STUB: &[u8] = b"#!/bin/sh\n# kernel command stub\n";
// ---------------------------------------------------------------------------
// NativeSidecar VM lifecycle methods
// ---------------------------------------------------------------------------

impl<B> NativeSidecar<B>
where
    B: NativeSidecarBridge + Send + 'static,
    BridgeError<B>: fmt::Debug + Send + Sync + 'static,
{
    pub(crate) async fn create_vm(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: crate::protocol::CreateVmRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let __t = Instant::now();
        let (connection_id, session_id) = self.session_scope_for(&request.ownership)?;
        self.require_owned_session(&connection_id, &session_id)?;
        let create_config: vm_config::CreateVmConfig = serde_json::from_str(&payload.config)
            .map_err(|error| {
                SidecarError::InvalidState(format!("invalid create VM config JSON: {error}"))
            })?;
        create_config
            .validate(self.config.max_frame_bytes)
            .map_err(|error| {
                SidecarError::InvalidState(format!("invalid create VM config: {error}"))
            })?;
        let root_filesystem =
            root_filesystem_protocol_descriptor_from_config(&create_config.root_filesystem);
        let permissions_policy = create_config
            .permissions
            .clone()
            .unwrap_or_else(deny_all_policy);
        validate_permissions_policy(&permissions_policy)?;

        self.next_vm_id += 1;
        let vm_id = format!("vm-{}", self.next_vm_id);
        let cwd = create_vm_shadow_root(&vm_id)?;
        let (guest_cwd, host_cwd) = resolve_vm_cwds(create_config.cwd.as_ref(), &cwd)?;
        fs::create_dir_all(&host_cwd)
            .map_err(|error| SidecarError::Io(format!("failed to create VM cwd: {error}")))?;
        let limits = crate::limits::vm_limits_from_config(
            create_config.limits.as_ref(),
            self.config.max_frame_bytes,
        )?;
        let resource_limits = limits.resources.clone();
        let dns = vm_dns_config_from_config(create_config.dns.as_ref())?;
        let listen_policy = vm_listen_policy_from_config(create_config.listen.as_ref())?;
        let create_loopback_exempt_ports: BTreeSet<u16> = create_config
            .loopback_exempt_ports
            .iter()
            .copied()
            .collect();
        self.bridge
            .set_vm_permissions(&vm_id, &permissions_policy)?;
        let permissions = bridge_permissions(self.bridge.clone(), &vm_id);
        let mut guest_env = filter_env(&vm_id, &create_config.env, &permissions);
        // Sidecar-owned bootstrap work still needs to reconcile command stubs and the root
        // filesystem before the guest-visible policy takes effect.
        self.bridge
            .set_vm_permissions(&vm_id, &allow_all_policy())?;
        let native_root = native_root_plugin_from_config(create_config.native_root.as_ref())?;
        let loaded_snapshot = if native_root.is_some() {
            None
        } else {
            self.bridge.with_mut(|bridge| {
                bridge.load_filesystem_state(LoadFilesystemStateRequest {
                    vm_id: vm_id.clone(),
                })
            })?
        };
        if native_root.is_none() {
            materialize_shadow_root_snapshot_entries(
                &cwd,
                &root_filesystem,
                loaded_snapshot.as_ref(),
                &resource_limits,
            )?;
        }

        let mut config = KernelVmConfig::new(vm_id.clone());
        config.cwd = guest_cwd.clone();
        config.env = guest_env.clone();
        config.permissions = permissions;
        config.dns = secure_exec_kernel::dns::DnsConfig {
            name_servers: dns.name_servers.clone(),
            overrides: dns.overrides.clone(),
        };
        config.loopback_exempt_ports = create_loopback_exempt_ports.clone();
        let root_mount_table = if let Some(native_root) = native_root.as_ref() {
            build_native_root_mount_table(
                &self.mount_plugins,
                native_root,
                &root_filesystem,
                MountPluginContext {
                    bridge: self.bridge.clone(),
                    connection_id: connection_id.clone(),
                    session_id: session_id.clone(),
                    vm_id: vm_id.clone(),
                    sidecar_requests: self.sidecar_requests.clone(),
                    max_pread_bytes: resource_limits.max_pread_bytes,
                },
            )?
        } else {
            secure_exec_sidecar_core::build_root_mount_table_with_loaded_snapshot(
                &create_config.root_filesystem,
                loaded_snapshot.as_ref(),
                &resource_limits,
            )
            .map_err(|error| SidecarError::InvalidState(error.to_string()))?
        };
        config.resources = resource_limits;
        let mut kernel = KernelVm::new(root_mount_table, config);
        let command_guest_paths = discover_command_guest_paths(&mut kernel);
        refresh_guest_command_path_env(&mut guest_env, &command_guest_paths);
        let mut execution_commands = vec![
            String::from(JAVASCRIPT_COMMAND),
            String::from(PYTHON_COMMAND),
            // `python3` resolves to the same Pyodide runtime; register it so the
            // guest shell can find `/bin/python3` on PATH (the command resolver
            // already rewrites the alias to `python`).
            String::from("python3"),
            String::from(WASM_COMMAND),
        ];
        execution_commands.extend(command_guest_paths.keys().cloned());
        kernel
            .register_driver(CommandDriver::new(
                EXECUTION_DRIVER_NAME,
                execution_commands,
            ))
            .map_err(kernel_error)?;
        if let Some(root) = kernel.root_filesystem_mut() {
            root.finish_bootstrap();
        }
        self.bridge
            .set_vm_permissions(&vm_id, &permissions_policy)?;

        self.bridge
            .emit_lifecycle(&vm_id, LifecycleState::Starting)?;
        self.bridge.emit_lifecycle(&vm_id, LifecycleState::Ready)?;
        self.bridge.emit_log(
            &vm_id,
            format!("created VM {vm_id} for session {session_id}"),
        )?;

        self.sessions
            .get_mut(&session_id)
            .expect("owned session should exist")
            .vm_ids
            .insert(vm_id.clone());
        self.vms.insert(
            vm_id.clone(),
            VmState {
                connection_id: connection_id.clone(),
                session_id: session_id.clone(),
                limits,
                dns,
                listen_policy,
                create_loopback_exempt_ports,
                guest_env,
                requested_runtime: payload.runtime,
                root_filesystem_mode: protocol_root_filesystem_mode(root_filesystem.mode),
                guest_cwd,
                cwd,
                host_cwd,
                kernel,
                loaded_snapshot,
                configuration: VmConfiguration {
                    permissions: permissions_policy,
                    js_runtime: create_config.js_runtime.clone(),
                    ..VmConfiguration::default()
                },
                layers: VmLayerStore::default(),
                command_guest_paths,
                command_permissions: BTreeMap::new(),
                toolkits: BTreeMap::new(),
                active_processes: BTreeMap::new(),
                exited_process_snapshots: VecDeque::new(),
                detached_child_processes: BTreeSet::new(),
                signal_states: BTreeMap::new(),
                packages_staging_root: None,
            },
        );

        // Pre-warm the agent-SDK snapshot (when the trusted client supplied one via
        // jsRuntime.snapshotUserlandCode) so the FIRST session is already warm — the
        // one-per-sidecar build happens here at VM create, off the session-create
        // critical path. The V8 platform is already initialized on the main thread at
        // startup (ensure_runtime_initialized), so building on this worker thread is
        // safe. Best-effort: on failure the runtime falls back to the lazy build.
        if let Some(userland) = create_config
            .js_runtime
            .as_ref()
            .and_then(|cfg| cfg.snapshot_userland_code.clone())
        {
            let _ = tokio::task::spawn_blocking(move || {
                if let Err(error) =
                    secure_exec_execution::v8_host::pre_warm_agent_snapshot(&userland)
                {
                    eprintln!("agent snapshot pre-warm failed: {error}");
                }
            })
            .await;
        }

        let events = vec![
            self.vm_lifecycle_event(
                &connection_id,
                &session_id,
                &vm_id,
                VmLifecycleState::Creating,
            ),
            self.vm_lifecycle_event(&connection_id, &session_id, &vm_id, VmLifecycleState::Ready),
        ];

        tracing::info!(target: "secure_exec_sidecar::perf", phase = "create_vm", elapsed_ms = __t.elapsed().as_millis() as u64, "vm phase");
        Ok(DispatchResult {
            response: vm_created_response(request, vm_id),
            events,
        })
    }

    pub(crate) async fn dispose_vm(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: crate::protocol::DisposeVmRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        let events = self
            .dispose_vm_internal(&connection_id, &session_id, &vm_id, payload.reason)
            .await?;

        Ok(DispatchResult {
            response: vm_disposed_response(request, vm_id),
            events,
        })
    }

    pub(crate) async fn bootstrap_root_filesystem(
        &mut self,
        request: &crate::protocol::RequestFrame,
        entries: Vec<RootFilesystemEntry>,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let root = vm.kernel.root_filesystem_mut().ok_or_else(|| {
            SidecarError::InvalidState(String::from("VM root filesystem is unavailable"))
        })?;
        for entry in &entries {
            apply_root_filesystem_entry(root, entry)?;
        }

        Ok(DispatchResult {
            response: root_filesystem_bootstrapped_response(request, entries.len() as u32),
            events: Vec::new(),
        })
    }

    pub(crate) async fn configure_vm(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: ConfigureVmRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let __t = Instant::now();
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let mount_plugins = &self.mount_plugins;
        let bridge = self.bridge.clone();
        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let max_pread_bytes = vm.kernel.resource_limits().max_pread_bytes;
        let original_permissions = vm.configuration.permissions.clone();
        let configured_permissions = payload
            .permissions
            .clone()
            .map(crate::wire::permissions_policy_config_from_wire)
            .unwrap_or_else(|| original_permissions.clone());
        validate_permissions_policy(&configured_permissions)?;
        bridge.set_vm_permissions(&vm_id, &allow_all_policy())?;
        let mut effective_mounts = payload.mounts.clone();
        append_module_access_mount(&mut effective_mounts, payload.module_access_cwd.as_ref())?;
        // Build the `/opt/agentos` package projection in a sidecar-owned staging dir
        // and register its read-only host_dir mount ourselves (the client forwards the
        // package descriptors over the wire instead of staging them host-side). The
        // mount is always created — even with no boot packages — so runtime
        // `LinkPackage` can append to the live, host-backed staging dir.
        if let Some(old) = vm.packages_staging_root.take() {
            let _ = fs::remove_dir_all(&old);
        }
        let package_descriptors = package_descriptors_from_wire(&payload.packages)?;
        let (packages_staging_root, packages_mount) =
            build_packages_projection(&vm_id, &package_descriptors, &payload.packages_mount_at)?;
        vm.packages_staging_root = Some(packages_staging_root);
        effective_mounts.push(packages_mount);
        apply_package_provides_env(&mut vm.guest_env, &package_descriptors);
        append_package_provides_mounts(&mut effective_mounts, &package_descriptors)?;
        let reconfigure_result = reconcile_mounts(
            mount_plugins,
            vm,
            &effective_mounts,
            MountPluginContext {
                bridge: bridge.clone(),
                connection_id: connection_id.clone(),
                session_id: session_id.clone(),
                vm_id: vm_id.clone(),
                sidecar_requests: self.sidecar_requests.clone(),
                max_pread_bytes,
            },
        )
        .and_then(|()| {
            vm.command_guest_paths = discover_command_guest_paths(&mut vm.kernel);
            refresh_guest_command_path_env(&mut vm.guest_env, &vm.command_guest_paths);
            let mut execution_commands =
                vec![String::from(JAVASCRIPT_COMMAND), String::from(WASM_COMMAND)];
            execution_commands.extend(vm.command_guest_paths.keys().cloned());
            vm.kernel
                .register_driver(CommandDriver::new(
                    EXECUTION_DRIVER_NAME,
                    execution_commands,
                ))
                .map_err(kernel_error)?;
            vm.command_permissions = payload.command_permissions.clone().into_iter().collect();
            let mut loopback_exempt_ports = vm.create_loopback_exempt_ports.clone();
            loopback_exempt_ports.extend(payload.loopback_exempt_ports.iter().copied());
            vm.kernel.set_loopback_exempt_ports(loopback_exempt_ports);
            vm.configuration = VmConfiguration {
                mounts: effective_mounts.clone(),
                software: payload.software.clone(),
                permissions: configured_permissions.clone(),
                module_access_cwd: payload.module_access_cwd.clone(),
                instructions: payload.instructions.clone(),
                projected_modules: payload.projected_modules.clone(),
                command_permissions: payload.command_permissions.clone().into_iter().collect(),
                // jsRuntime is create-time only; preserve what create_vm stored.
                js_runtime: vm.configuration.js_runtime.clone(),
                loopback_exempt_ports: payload.loopback_exempt_ports.clone(),
            };
            Ok(())
        });
        match reconfigure_result {
            Ok(()) => bridge.set_vm_permissions(&vm_id, &configured_permissions)?,
            Err(error) => {
                match bridge.restore_vm_permissions_fail_closed(
                    &vm_id,
                    &original_permissions,
                    "configure_vm rollback",
                    &error,
                ) {
                    Ok(()) => return Err(error),
                    Err(rollback_error) => {
                        self.vms
                            .get_mut(&vm_id)
                            .expect("owned VM should exist")
                            .configuration
                            .permissions = deny_all_policy();
                        return Err(rollback_error);
                    }
                }
            }
        }

        tracing::info!(target: "secure_exec_sidecar::perf", phase = "configure_vm", elapsed_ms = __t.elapsed().as_millis() as u64, applied_mounts = effective_mounts.len() as u64, "vm phase");
        Ok(DispatchResult {
            response: vm_configured_response(
                request,
                effective_mounts.len() as u32,
                payload.software.len() as u32,
            ),
            events: Vec::new(),
        })
    }

    /// Runtime dynamic `linkSoftware`: project one package into the live
    /// `/opt/agentos` staging dir. The host-dir mount reflects host writes, so the
    /// package's `bin/` commands appear under `/opt/agentos/bin` (on `$PATH`)
    /// immediately, with no reboot. Returns the linked command names.
    pub(crate) async fn link_package(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: LinkPackageRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let staging_root = vm.packages_staging_root.clone().ok_or_else(|| {
            SidecarError::InvalidState(String::from(
                "link_package: VM has no /opt/agentos projection (configure_vm was not called)",
            ))
        })?;
        let descriptor = crate::package_projection::read_package_manifest(&payload.package.dir)?;
        let commands = crate::package_projection::link_package(&descriptor, &staging_root)?;

        Ok(DispatchResult {
            response: package_linked_response(request, commands),
            events: Vec::new(),
        })
    }

    pub(crate) async fn create_layer(
        &mut self,
        request: &crate::protocol::RequestFrame,
        _payload: CreateLayerRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let layer_id = vm
            .layers
            .create_writable_layer()
            .map_err(sidecar_core_error)?;

        Ok(DispatchResult {
            response: layer_created_response(request, layer_id),
            events: Vec::new(),
        })
    }

    pub(crate) async fn seal_layer(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: SealLayerRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let layer_id = vm
            .layers
            .seal_layer(&payload.layer_id)
            .map_err(sidecar_core_error)?;

        Ok(DispatchResult {
            response: layer_sealed_response(request, layer_id),
            events: Vec::new(),
        })
    }

    pub(crate) async fn import_snapshot(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: ImportSnapshotRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let layer_id = vm
            .layers
            .import_snapshot(root_snapshot_from_entries(&payload.entries)?)
            .map_err(sidecar_core_error)?;

        Ok(DispatchResult {
            response: snapshot_imported_response(request, layer_id),
            events: Vec::new(),
        })
    }

    pub(crate) async fn export_snapshot(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: ExportSnapshotRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let snapshot = vm
            .layers
            .export_snapshot(&payload.layer_id)
            .map_err(sidecar_core_error)?;

        Ok(DispatchResult {
            response: snapshot_exported_response(
                request,
                payload.layer_id,
                root_snapshot_entries(&snapshot),
            ),
            events: Vec::new(),
        })
    }

    pub(crate) async fn create_overlay(
        &mut self,
        request: &crate::protocol::RequestFrame,
        payload: CreateOverlayRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let layer_id = vm
            .layers
            .create_overlay_layer(
                protocol_root_filesystem_mode(payload.mode),
                payload.upper_layer_id,
                payload.lower_layer_ids,
            )
            .map_err(sidecar_core_error)?;

        Ok(DispatchResult {
            response: overlay_created_response(request, layer_id),
            events: Vec::new(),
        })
    }

    pub(crate) async fn snapshot_root_filesystem(
        &mut self,
        request: &crate::protocol::RequestFrame,
        _payload: SnapshotRootFilesystemRequest,
    ) -> Result<DispatchResult, SidecarError> {
        let (connection_id, session_id, vm_id) = self.vm_scope_for(&request.ownership)?;
        self.require_owned_vm(&connection_id, &session_id, &vm_id)?;

        let vm = self.vms.get_mut(&vm_id).expect("owned VM should exist");
        let snapshot = vm.kernel.snapshot_root_filesystem().map_err(kernel_error)?;

        Ok(DispatchResult {
            response: root_filesystem_snapshot_response(
                request,
                snapshot.entries.iter().map(root_snapshot_entry).collect(),
            ),
            events: Vec::new(),
        })
    }

    pub(crate) async fn dispose_vm_internal(
        &mut self,
        connection_id: &str,
        session_id: &str,
        vm_id: &str,
        _reason: DisposeReason,
    ) -> Result<Vec<EventFrame>, SidecarError> {
        self.require_owned_vm(connection_id, session_id, vm_id)?;

        let mut events = vec![self.vm_lifecycle_event(
            connection_id,
            session_id,
            vm_id,
            VmLifecycleState::Disposing,
        )];
        // Process termination needs the VM live in `self.vms` (it looks up and
        // signals the VM's active processes). Capture its result but keep tearing
        // down: a process that refuses to die must not strand the VM's tracking
        // entries for the process lifetime.
        let terminate_result = self.terminate_vm_processes(vm_id, &mut events).await;

        // Detach the VM from `self.vms` BEFORE the remaining fallible teardown so
        // no `?` below can leave the registry entry (or any per-VM map) behind.
        let mut vm = self
            .vms
            .remove(vm_id)
            .expect("owned VM should exist before disposal");

        // `continue_on_error = true` => `shutdown_configured_mounts` never returns
        // `Err` on the dispose path (it logs and presses on), so its result is
        // intentionally discarded rather than `?`-ed.
        let mount_context = MountPluginContext {
            bridge: self.bridge.clone(),
            connection_id: connection_id.to_owned(),
            session_id: session_id.to_owned(),
            vm_id: vm_id.to_owned(),
            sidecar_requests: self.sidecar_requests.clone(),
            max_pread_bytes: vm.kernel.resource_limits().max_pread_bytes,
        };
        let _ = shutdown_configured_mounts(&mut vm, &mount_context, "dispose_vm", true);

        // Snapshot/flush/kernel-dispose/permission-reset can each fail; run them
        // in a helper whose result is captured so cleanup below is unconditional.
        let teardown_result = self.finish_vm_teardown(vm_id, &mut vm).await;

        // Reclaim EVERY per-VM tracking entry on EVERY exit path — even when a
        // teardown step above errored. Pre-fix these ran only after the fallible
        // steps' `?`, so any failure stranded the engine/extension maps (H1) and
        // the output-buffer map was never reclaimed at all (M6).
        self.reclaim_vm_tracking(session_id, vm_id);
        let _ = fs::remove_dir_all(&vm.cwd);
        if let Some(staging_root) = vm.packages_staging_root.take() {
            let _ = fs::remove_dir_all(&staging_root);
        }

        // Surface the first failure only AFTER cleanup has completed.
        terminate_result?;
        teardown_result?;

        events.push(self.vm_lifecycle_event(
            connection_id,
            session_id,
            vm_id,
            VmLifecycleState::Disposed,
        ));
        Ok(events)
    }

    /// Run the fallible second half of VM disposal (root-filesystem snapshot +
    /// flush, lifecycle event, kernel dispose, permission reset) against a VM
    /// that has already been detached from `self.vms`. Kept separate so its
    /// `?`-propagated errors are captured by the caller and the per-VM tracking
    /// maps are still reclaimed afterward.
    async fn finish_vm_teardown(
        &mut self,
        vm_id: &str,
        vm: &mut VmState,
    ) -> Result<(), SidecarError> {
        let snapshot = if vm.kernel.root_filesystem_mut().is_some() {
            Some(FilesystemSnapshot {
                format: String::from(ROOT_FILESYSTEM_SNAPSHOT_FORMAT),
                bytes: encode_root_snapshot(
                    &vm.kernel.snapshot_root_filesystem().map_err(kernel_error)?,
                )
                .map_err(root_filesystem_error)?,
            })
        } else {
            None
        };

        self.bridge
            .emit_lifecycle(vm_id, LifecycleState::Terminated)?;
        vm.kernel.dispose().map_err(kernel_error)?;
        if let Some(snapshot) = snapshot {
            self.bridge.with_mut(|bridge| {
                bridge.flush_filesystem_state(FlushFilesystemStateRequest {
                    vm_id: vm_id.to_owned(),
                    snapshot,
                })
            })?;
        }
        self.bridge.clear_vm_permissions(vm_id)?;
        Ok(())
    }

    pub(crate) async fn terminate_vm_processes(
        &mut self,
        vm_id: &str,
        events: &mut Vec<EventFrame>,
    ) -> Result<(), SidecarError> {
        let process_ids = self
            .vms
            .get(vm_id)
            .map(|vm| vm.active_processes.keys().cloned().collect::<Vec<_>>())
            .unwrap_or_default();
        if process_ids.is_empty() {
            return Ok(());
        }

        for process_id in process_ids {
            if self
                .vms
                .get(vm_id)
                .is_some_and(|vm| vm.active_processes.contains_key(&process_id))
            {
                self.kill_process_internal(vm_id, &process_id, "SIGTERM")?;
            }
        }
        self.wait_for_vm_processes_to_exit(vm_id, DISPOSE_VM_SIGTERM_GRACE, events)
            .await?;

        if !self.vm_has_active_processes(vm_id) {
            return Ok(());
        }

        let remaining = self
            .vms
            .get(vm_id)
            .map(|vm| vm.active_processes.keys().cloned().collect::<Vec<_>>())
            .unwrap_or_default();
        for process_id in remaining {
            if self
                .vms
                .get(vm_id)
                .is_some_and(|vm| vm.active_processes.contains_key(&process_id))
            {
                self.kill_process_internal(vm_id, &process_id, "SIGKILL")?;
            }
        }
        self.wait_for_vm_processes_to_exit(vm_id, DISPOSE_VM_SIGKILL_GRACE, events)
            .await?;

        if self.vm_has_active_processes(vm_id) {
            return Err(SidecarError::Execution(format!(
                "failed to terminate active guest executions for VM {vm_id}"
            )));
        }

        Ok(())
    }

    pub(crate) async fn wait_for_vm_processes_to_exit(
        &mut self,
        vm_id: &str,
        timeout: Duration,
        events: &mut Vec<EventFrame>,
    ) -> Result<(), SidecarError> {
        let ownership = self.vm_ownership(vm_id)?;
        let deadline = Instant::now() + timeout;

        while self.vm_has_active_processes(vm_id) && Instant::now() < deadline {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if let Some(event) = self
                .poll_event(&ownership, remaining.min(Duration::from_millis(10)))
                .await?
            {
                events.push(event);
            }
        }

        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Free functions — VM lifecycle helpers
// ---------------------------------------------------------------------------

fn native_root_plugin_from_config(
    config: Option<&vm_config::NativeRootFilesystemConfig>,
) -> Result<Option<NativeRootPluginConfig>, SidecarError> {
    let Some(config) = config else {
        return Ok(None);
    };
    let plugin_config = serde_json::to_string(&config.plugin.config).map_err(|error| {
        SidecarError::InvalidState(format!(
            "failed to serialize nativeRoot.plugin.config: {error}"
        ))
    })?;
    Ok(Some(NativeRootPluginConfig {
        plugin: MountPluginDescriptor {
            id: config.plugin.id.clone(),
            config: plugin_config,
        },
        read_only: config.read_only,
    }))
}

fn vm_dns_config_from_config(
    config: Option<&vm_config::VmDnsConfig>,
) -> Result<VmDnsConfig, SidecarError> {
    let Some(config) = config else {
        return Ok(VmDnsConfig::default());
    };
    let name_servers = config
        .name_servers
        .iter()
        .map(|entry| parse_vm_dns_nameserver(entry))
        .collect::<Result<Vec<_>, _>>()?;
    let mut overrides = BTreeMap::new();
    for (hostname, addresses) in &config.overrides {
        let normalized_hostname = normalize_dns_hostname(hostname)?;
        let parsed_addresses = addresses
            .iter()
            .map(|entry| {
                entry.parse::<IpAddr>().map_err(|error| {
                    SidecarError::InvalidState(format!(
                        "invalid DNS override {hostname}={entry}: {error}"
                    ))
                })
            })
            .collect::<Result<Vec<_>, _>>()?;
        overrides.insert(normalized_hostname, parsed_addresses);
    }
    Ok(VmDnsConfig {
        name_servers,
        overrides,
    })
}

fn vm_listen_policy_from_config(
    config: Option<&vm_config::VmListenPolicyConfig>,
) -> Result<VmListenPolicy, SidecarError> {
    let mut policy = VmListenPolicy::default();
    let Some(config) = config else {
        return Ok(policy);
    };
    if let Some(port_min) = config.port_min {
        policy.port_min = port_min;
    }
    if let Some(port_max) = config.port_max {
        policy.port_max = port_max;
    }
    if policy.port_min > policy.port_max {
        return Err(SidecarError::InvalidState(format!(
            "invalid listen port range {} exceeds {}",
            policy.port_min, policy.port_max
        )));
    }
    if let Some(allow_privileged) = config.allow_privileged {
        policy.allow_privileged = allow_privileged;
    }
    Ok(policy)
}

#[derive(Debug, Clone)]
struct NativeRootPluginConfig {
    plugin: MountPluginDescriptor,
    read_only: bool,
}

fn build_native_root_mount_table<B>(
    mount_plugins: &secure_exec_kernel::mount_plugin::FileSystemPluginRegistry<
        MountPluginContext<B>,
    >,
    native_root: &NativeRootPluginConfig,
    descriptor: &RootFilesystemDescriptor,
    context: MountPluginContext<B>,
) -> Result<MountTable, SidecarError>
where
    B: NativeSidecarBridge + Send + 'static,
    BridgeError<B>: fmt::Debug + Send + Sync + 'static,
{
    if !descriptor.lowers.is_empty() {
        return Err(SidecarError::InvalidState(String::from(
            "native root filesystems do not support rootFilesystem.lowers",
        )));
    }

    let config_value: serde_json::Value = serde_json::from_str(&native_root.plugin.config)
        .map_err(|error| {
            SidecarError::InvalidState(format!(
                "root native plugin config for {} is not valid JSON: {error}",
                native_root.plugin.id
            ))
        })?;
    let mut filesystem = mount_plugins
        .open(
            &native_root.plugin.id,
            OpenFileSystemPluginRequest {
                vm_id: &context.vm_id,
                guest_path: "/",
                read_only: native_root.read_only,
                config: &config_value,
                context: &context,
            },
        )
        .map_err(plugin_error)?;

    bootstrap_native_root_filesystem(filesystem.as_mut(), descriptor)?;

    Ok(MountTable::new_boxed_root(
        filesystem,
        MountOptions::new(native_root.plugin.id.clone()).read_only(native_root.read_only),
    ))
}

fn bootstrap_native_root_filesystem(
    filesystem: &mut dyn MountedFileSystem,
    descriptor: &RootFilesystemDescriptor,
) -> Result<(), SidecarError> {
    for (guest_path, mode) in SHADOW_ROOT_BOOTSTRAP_DIRS {
        filesystem.mkdir(guest_path, true).map_err(vfs_error)?;
        filesystem.chmod(guest_path, *mode).map_err(vfs_error)?;
    }

    for entry in &descriptor.bootstrap_entries {
        apply_native_root_filesystem_entry(filesystem, entry)?;
    }

    Ok(())
}

fn apply_native_root_filesystem_entry(
    filesystem: &mut dyn MountedFileSystem,
    entry: &RootFilesystemEntry,
) -> Result<(), SidecarError> {
    let snapshot = root_snapshot_from_entries(std::slice::from_ref(entry))?;
    let kernel_entry = snapshot
        .entries
        .into_iter()
        .next()
        .expect("root snapshot from one entry should contain one entry");
    ensure_mounted_parent_directories(filesystem, &kernel_entry.path)?;

    match kernel_entry.kind {
        KernelFilesystemEntryKind::Directory => filesystem
            .mkdir(&kernel_entry.path, true)
            .map_err(vfs_error)?,
        KernelFilesystemEntryKind::File => filesystem
            .write_file(&kernel_entry.path, kernel_entry.content.unwrap_or_default())
            .map_err(vfs_error)?,
        KernelFilesystemEntryKind::Symlink => filesystem
            .symlink(
                kernel_entry.target.as_deref().ok_or_else(|| {
                    SidecarError::InvalidState(format!(
                        "root filesystem bootstrap for symlink {} requires a target",
                        entry.path
                    ))
                })?,
                &kernel_entry.path,
            )
            .map_err(vfs_error)?,
    }

    if !matches!(kernel_entry.kind, KernelFilesystemEntryKind::Symlink) {
        filesystem
            .chmod(&kernel_entry.path, kernel_entry.mode)
            .map_err(vfs_error)?;
        filesystem
            .chown(&kernel_entry.path, kernel_entry.uid, kernel_entry.gid)
            .map_err(vfs_error)?;
    }

    Ok(())
}

fn ensure_mounted_parent_directories(
    filesystem: &mut dyn MountedFileSystem,
    path: &str,
) -> Result<(), SidecarError> {
    let parent = dirname(path);
    if parent != "/" && !filesystem.exists(&parent) {
        ensure_mounted_parent_directories(filesystem, &parent)?;
        filesystem.mkdir(&parent, true).map_err(vfs_error)?;
    }
    Ok(())
}

fn reconcile_mounts<B>(
    mount_plugins: &secure_exec_kernel::mount_plugin::FileSystemPluginRegistry<
        MountPluginContext<B>,
    >,
    vm: &mut VmState,
    mounts: &[crate::protocol::MountDescriptor],
    context: MountPluginContext<B>,
) -> Result<(), SidecarError>
where
    B: NativeSidecarBridge + Send + 'static,
    BridgeError<B>: fmt::Debug + Send + Sync + 'static,
{
    shutdown_configured_mounts(vm, &context, "configure_vm", false)?;

    for mount in mounts {
        let config_value: serde_json::Value =
            serde_json::from_str(&mount.plugin.config).map_err(|error| {
                SidecarError::InvalidState(format!(
                    "mount plugin config for {} is not valid JSON: {error}",
                    mount.plugin.id
                ))
            })?;
        let filesystem = mount_plugins
            .open(
                &mount.plugin.id,
                OpenFileSystemPluginRequest {
                    vm_id: &context.vm_id,
                    guest_path: &mount.guest_path,
                    read_only: mount.read_only,
                    config: &config_value,
                    context: &context,
                },
            )
            .map_err(plugin_error)?;

        vm.kernel
            .mount_boxed_filesystem(
                &mount.guest_path,
                filesystem,
                MountOptions::new(mount.plugin.id.clone()).read_only(mount.read_only),
            )
            .map_err(kernel_error)?;
        emit_security_audit_event(
            &context.bridge,
            &context.vm_id,
            "security.mount.mounted",
            audit_fields([
                (String::from("guest_path"), mount.guest_path.clone()),
                (String::from("plugin_id"), mount.plugin.id.clone()),
                (String::from("read_only"), mount.read_only.to_string()),
            ]),
        );
    }

    Ok(())
}

fn shutdown_configured_mounts<B>(
    vm: &mut VmState,
    context: &MountPluginContext<B>,
    phase: &str,
    continue_on_error: bool,
) -> Result<(), SidecarError>
where
    B: NativeSidecarBridge + Send + 'static,
    BridgeError<B>: fmt::Debug + Send + Sync + 'static,
{
    for existing in vm.configuration.mounts.clone() {
        match vm.kernel.unmount_filesystem(&existing.guest_path) {
            Ok(()) => emit_security_audit_event(
                &context.bridge,
                &context.vm_id,
                "security.mount.unmounted",
                audit_fields([
                    (String::from("guest_path"), existing.guest_path.clone()),
                    (String::from("plugin_id"), existing.plugin.id.clone()),
                    (String::from("read_only"), existing.read_only.to_string()),
                ]),
            ),
            Err(error) if error.code() == "EINVAL" => {}
            Err(error) => {
                let _ = emit_structured_event(
                    &context.bridge,
                    &context.vm_id,
                    "filesystem.mount.shutdown_failed",
                    audit_fields([
                        (String::from("guest_path"), existing.guest_path.clone()),
                        (String::from("plugin_id"), existing.plugin.id.clone()),
                        (String::from("read_only"), existing.read_only.to_string()),
                        (String::from("phase"), String::from(phase)),
                        (String::from("error_code"), String::from(error.code())),
                        (String::from("error"), error.to_string()),
                    ]),
                );

                if !continue_on_error {
                    return Err(kernel_error(error));
                }
            }
        }
    }

    Ok(())
}

/// Build the `/opt/agentos` package projection for `configure_vm`: create a
/// sidecar-owned staging dir, project each parsed package descriptor into it, and
/// return the staging path plus the read-only `host_dir` mount that exposes it in
/// the guest. Always returns a (possibly empty) projection so runtime `LinkPackage`
/// has a live host-backed dir to append to.
fn build_packages_projection(
    vm_id: &str,
    packages: &[crate::package_projection::PackageDescriptor],
    mount_at: &str,
) -> Result<(PathBuf, MountDescriptor), SidecarError> {
    let mount_at = if mount_at.is_empty() {
        crate::package_projection::OPT_AGENTOS_ROOT
    } else {
        mount_at
    };
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|error| {
            SidecarError::Io(format!("failed to compute packages-staging nonce: {error}"))
        })?
        .as_nanos();
    let staging = std::env::temp_dir().join(format!("agentos-opt-{vm_id}-{nonce}"));
    crate::package_projection::init_projection(&staging)?;
    for pkg in packages {
        crate::package_projection::link_package(pkg, &staging)?;
    }
    let mount = MountDescriptor {
        guest_path: mount_at.to_string(),
        read_only: true,
        plugin: MountPluginDescriptor {
            id: String::from("host_dir"),
            config: serde_json::json!({
                "hostPath": staging,
                "readOnly": true,
            })
            .to_string(),
        },
    };
    Ok((staging, mount))
}

fn package_descriptors_from_wire(
    packages: &[crate::protocol::PackageDescriptor],
) -> Result<Vec<crate::package_projection::PackageDescriptor>, SidecarError> {
    packages
        .iter()
        .map(|package| crate::package_projection::read_package_manifest(&package.dir))
        .collect()
}

fn apply_package_provides_env(
    guest_env: &mut BTreeMap<String, String>,
    packages: &[crate::package_projection::PackageDescriptor],
) {
    for package in packages {
        let Some(provides) = package.provides.as_ref() else {
            continue;
        };
        for (key, value) in &provides.env {
            guest_env
                .entry(key.clone())
                .or_insert_with(|| value.clone());
        }
    }
}

fn append_package_provides_mounts(
    mounts: &mut Vec<MountDescriptor>,
    packages: &[crate::package_projection::PackageDescriptor],
) -> Result<(), SidecarError> {
    for package in packages {
        let Some(provides) = package.provides.as_ref() else {
            continue;
        };
        for file in &provides.files {
            let source = package_provides_source_path(&package.dir, &file.source);
            match fs::metadata(&source) {
                Ok(metadata) if metadata.is_dir() => {
                    mounts.push(MountDescriptor {
                        guest_path: file.target.clone(),
                        read_only: true,
                        plugin: MountPluginDescriptor {
                            id: String::from("host_dir"),
                            config: serde_json::json!({
                                "hostPath": source,
                                "readOnly": true,
                            })
                            .to_string(),
                        },
                    });
                }
                Ok(_) => {
                    tracing::warn!(
                        package = %package.name,
                        source = %source.display(),
                        target = %file.target,
                        "package provides file source is not a directory; skipping"
                    );
                }
                Err(error) if error.kind() == std::io::ErrorKind::NotFound => {
                    return Err(SidecarError::InvalidState(format!(
                        "package provides file source is missing: package `{}` source `{}` target `{}`",
                        package.name, file.source, file.target
                    )));
                }
                Err(error) => {
                    tracing::warn!(
                        package = %package.name,
                        source = %source.display(),
                        target = %file.target,
                        error = %error,
                        "package provides file source could not be inspected; skipping"
                    );
                }
            }
        }
    }
    Ok(())
}

fn package_provides_source_path(package_dir: &str, source: &str) -> PathBuf {
    let source = Path::new(source);
    if source.is_absolute() {
        source.to_path_buf()
    } else {
        Path::new(package_dir).join(source)
    }
}

fn append_module_access_mount(
    mounts: &mut Vec<MountDescriptor>,
    module_access_cwd: Option<&String>,
) -> Result<(), SidecarError> {
    if mounts
        .iter()
        .any(|mount| mount.guest_path == "/root/node_modules")
    {
        return Ok(());
    }

    let Some(module_access_cwd) = module_access_cwd else {
        return Ok(());
    };
    let root = resolve_host_path(Some(module_access_cwd))?.join("node_modules");
    if !root.is_dir() {
        return Ok(());
    }

    mounts.push(MountDescriptor {
        guest_path: String::from("/root/node_modules"),
        read_only: true,
        plugin: MountPluginDescriptor {
            id: String::from("module_access"),
            config: serde_json::json!({
                "hostPath": root,
            })
            .to_string(),
        },
    });
    append_module_access_symlink_mounts(mounts, &root)?;
    Ok(())
}

fn append_module_access_symlink_mounts(
    mounts: &mut Vec<MountDescriptor>,
    node_modules_root: &Path,
) -> Result<(), SidecarError> {
    for entry in fs::read_dir(node_modules_root)
        .map_err(|error| SidecarError::Io(format!("failed to read module_access root: {error}")))?
    {
        let entry = entry.map_err(|error| {
            SidecarError::Io(format!("failed to inspect module_access root: {error}"))
        })?;
        let file_name = entry.file_name();
        let name = file_name.to_string_lossy();
        if name.starts_with('.') {
            continue;
        }
        let path = entry.path();
        let metadata = fs::symlink_metadata(&path).map_err(|error| {
            SidecarError::Io(format!("failed to stat module_access entry: {error}"))
        })?;
        if metadata.file_type().is_symlink() {
            append_module_access_symlink_mount(
                mounts,
                &format!("/root/node_modules/{name}"),
                &path,
            )?;
            continue;
        }
        if !metadata.is_dir() || !name.starts_with('@') {
            continue;
        }
        for scoped_entry in fs::read_dir(&path).map_err(|error| {
            SidecarError::Io(format!("failed to read module_access scope: {error}"))
        })? {
            let scoped_entry = scoped_entry.map_err(|error| {
                SidecarError::Io(format!("failed to inspect module_access scope: {error}"))
            })?;
            let scoped_name = scoped_entry.file_name().to_string_lossy().into_owned();
            if scoped_name.starts_with('.') {
                continue;
            }
            let scoped_path = scoped_entry.path();
            let scoped_metadata = fs::symlink_metadata(&scoped_path).map_err(|error| {
                SidecarError::Io(format!(
                    "failed to stat module_access scoped entry: {error}"
                ))
            })?;
            if scoped_metadata.file_type().is_symlink() {
                append_module_access_symlink_mount(
                    mounts,
                    &format!("/root/node_modules/{name}/{scoped_name}"),
                    &scoped_path,
                )?;
            }
        }
    }

    Ok(())
}

fn append_module_access_symlink_mount(
    mounts: &mut Vec<MountDescriptor>,
    guest_path: &str,
    symlink_path: &Path,
) -> Result<(), SidecarError> {
    if mounts.iter().any(|mount| mount.guest_path == guest_path) {
        return Ok(());
    }

    let target = fs::canonicalize(symlink_path).map_err(|error| {
        SidecarError::Io(format!(
            "failed to resolve module_access package symlink {}: {error}",
            symlink_path.display()
        ))
    })?;
    if !target.is_dir() {
        return Ok(());
    }

    mounts.push(MountDescriptor {
        guest_path: guest_path.to_owned(),
        read_only: true,
        plugin: MountPluginDescriptor {
            id: String::from("host_dir"),
            config: serde_json::json!({
                "hostPath": target,
                "readOnly": true,
            })
            .to_string(),
        },
    });
    Ok(())
}

fn sidecar_core_error(error: secure_exec_sidecar_core::SidecarCoreError) -> SidecarError {
    SidecarError::InvalidState(error.to_string())
}

fn resolve_guest_cwd(value: Option<&String>) -> String {
    value
        .map(|path| normalize_guest_path(path))
        .unwrap_or_else(|| String::from("/workspace"))
}

fn resolve_vm_cwds(
    metadata_cwd: Option<&String>,
    shadow_root: &Path,
) -> Result<(String, PathBuf), SidecarError> {
    if let Some(raw_cwd) = metadata_cwd {
        let candidate = PathBuf::from(raw_cwd);
        if candidate.is_absolute() || raw_cwd.starts_with('.') {
            let resolved_host_cwd = resolve_host_path(Some(raw_cwd))?;
            return Ok((String::from("/"), resolved_host_cwd));
        }
    }

    let guest_cwd = resolve_guest_cwd(metadata_cwd);
    let host_cwd = shadow_path_for_guest(shadow_root, &guest_cwd);
    Ok((guest_cwd, host_cwd))
}

fn resolve_host_path(value: Option<&String>) -> Result<PathBuf, SidecarError> {
    match value {
        Some(path) => {
            let cwd = PathBuf::from(path);
            let resolved = if cwd.is_absolute() {
                cwd
            } else {
                std::env::current_dir()
                    .map_err(|error| {
                        SidecarError::Io(format!("failed to resolve current directory: {error}"))
                    })?
                    .join(cwd)
            };
            Ok(resolved)
        }
        None => std::env::current_dir().map_err(|error| {
            SidecarError::Io(format!("failed to resolve current directory: {error}"))
        }),
    }
}

fn create_vm_shadow_root(vm_id: &str) -> Result<PathBuf, SidecarError> {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|error| SidecarError::Io(format!("failed to compute shadow-root nonce: {error}")))?
        .as_nanos();
    let root = std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-{vm_id}-{nonce}"));
    fs::create_dir_all(&root)
        .map_err(|error| SidecarError::Io(format!("failed to create VM shadow root: {error}")))?;
    // macOS: `std::env::temp_dir()` lives under `/var/folders/…`, but `/var` is a
    // symlink to `/private/var`, and macOS fd→path recovery (`fcntl(F_GETPATH)`)
    // reports the resolved `/private/var/…` form. Canonicalize the shadow root up
    // front so the stored host-root matches those resolved paths; otherwise the
    // mapped-runtime confinement prefix checks (`strip_prefix(host_root)`) reject
    // every child and guest `readdir` of a populated dir returns empty. host_dir
    // mounts already canonicalize their root for the same reason.
    #[cfg(target_os = "macos")]
    let root = fs::canonicalize(&root).map_err(|error| {
        SidecarError::Io(format!("failed to canonicalize VM shadow root: {error}"))
    })?;
    bootstrap_shadow_root(&root)?;
    Ok(root)
}

fn bootstrap_shadow_root(root: &Path) -> Result<(), SidecarError> {
    for (guest_path, mode) in SHADOW_ROOT_BOOTSTRAP_DIRS {
        let host_path = shadow_path_for_guest(root, guest_path);
        fs::create_dir_all(&host_path).map_err(|error| {
            SidecarError::Io(format!(
                "failed to create shadow directory {}: {error}",
                host_path.display()
            ))
        })?;
        fs::set_permissions(&host_path, fs::Permissions::from_mode(*mode)).map_err(|error| {
            SidecarError::Io(format!(
                "failed to set shadow directory mode {mode:o} on {}: {error}",
                host_path.display()
            ))
        })?;
    }
    Ok(())
}

fn materialize_shadow_root_snapshot_entries(
    shadow_root: &Path,
    descriptor: &RootFilesystemDescriptor,
    loaded_snapshot: Option<&FilesystemSnapshot>,
    resource_limits: &ResourceLimits,
) -> Result<(), SidecarError> {
    let import_limits = RootFilesystemImportLimits::from_resource_limits(resource_limits);
    if let Some(snapshot) = loaded_snapshot
        .filter(|snapshot| is_supported_root_filesystem_snapshot_format(&snapshot.format))
        .map(|snapshot| {
            decode_snapshot_with_import_limits(&snapshot.bytes, &import_limits)
                .map_err(root_filesystem_error)
        })
        .transpose()?
    {
        return materialize_shadow_entries(shadow_root, &root_snapshot_entries(&snapshot));
    }

    validate_shadow_descriptor_import_limits(descriptor, &import_limits)?;
    for lower in &descriptor.lowers {
        if let RootFilesystemLowerDescriptor::SnapshotRootFilesystemLower(inner) = lower {
            materialize_shadow_entries(shadow_root, &inner.entries)?;
        }
    }
    materialize_shadow_entries(shadow_root, &descriptor.bootstrap_entries)?;
    Ok(())
}

fn validate_shadow_descriptor_import_limits(
    descriptor: &RootFilesystemDescriptor,
    limits: &RootFilesystemImportLimits,
) -> Result<(), SidecarError> {
    let mut explicit_entry_count = descriptor.bootstrap_entries.len();
    let mut inode_paths = BTreeSet::new();
    collect_root_protocol_entry_paths(&descriptor.bootstrap_entries, &mut inode_paths);
    let mut bytes = root_protocol_entry_content_bytes(&descriptor.bootstrap_entries)?;

    for lower in &descriptor.lowers {
        match lower {
            RootFilesystemLowerDescriptor::SnapshotRootFilesystemLower(inner) => {
                let entries = &inner.entries;
                explicit_entry_count = explicit_entry_count.saturating_add(entries.len());
                collect_root_protocol_entry_paths(entries, &mut inode_paths);
                bytes = bytes.saturating_add(root_protocol_entry_content_bytes(entries)?);
            }
            RootFilesystemLowerDescriptor::BundledBaseFilesystemLower => {}
        }
    }

    if let Some(limit) = limits.max_inode_count {
        if explicit_entry_count > limit {
            return Err(root_filesystem_error(format!(
                "root filesystem descriptor contains {explicit_entry_count} entries, exceeding limit {limit}"
            )));
        }

        let entry_count = inode_paths.len();
        if entry_count > limit {
            return Err(root_filesystem_error(format!(
                "root filesystem descriptor contains {entry_count} entries, exceeding limit {limit}"
            )));
        }
    }

    if let Some(limit) = limits.max_filesystem_bytes {
        if bytes > limit {
            return Err(root_filesystem_error(format!(
                "root filesystem descriptor contains {bytes} bytes, exceeding limit {limit}"
            )));
        }
    }

    Ok(())
}

fn collect_root_protocol_entry_paths(
    entries: &[RootFilesystemEntry],
    paths: &mut BTreeSet<String>,
) {
    for entry in entries {
        collect_root_protocol_path(&entry.path, paths);
    }
}

fn collect_root_protocol_path(path: &str, paths: &mut BTreeSet<String>) {
    let normalized = normalize_guest_path(path);
    paths.insert(normalized.clone());

    let mut parent = String::new();
    let segments = normalized
        .split('/')
        .filter(|segment| !segment.is_empty())
        .collect::<Vec<_>>();
    for segment in segments.iter().take(segments.len().saturating_sub(1)) {
        parent.push('/');
        parent.push_str(segment);
        paths.insert(parent.clone());
    }
}

fn root_protocol_entry_content_bytes(entries: &[RootFilesystemEntry]) -> Result<u64, SidecarError> {
    entries.iter().try_fold(0_u64, |total, entry| {
        let bytes = match entry.kind {
            crate::protocol::RootFilesystemEntryKind::Directory => 0,
            crate::protocol::RootFilesystemEntryKind::File => {
                root_protocol_file_content_bytes(entry)?
            }
            crate::protocol::RootFilesystemEntryKind::Symlink => entry
                .target
                .as_ref()
                .map(|target| usize_to_u64(target.len()))
                .unwrap_or(0),
        };
        Ok(total.saturating_add(bytes))
    })
}

fn root_protocol_file_content_bytes(entry: &RootFilesystemEntry) -> Result<u64, SidecarError> {
    let Some(content) = entry.content.as_deref() else {
        return Ok(0);
    };

    let bytes = match entry
        .encoding
        .clone()
        .unwrap_or(RootFilesystemEntryEncoding::Utf8)
    {
        RootFilesystemEntryEncoding::Utf8 => content.len(),
        RootFilesystemEntryEncoding::Base64 => estimated_base64_decoded_len(content),
    };
    Ok(usize_to_u64(bytes))
}

fn estimated_base64_decoded_len(content: &str) -> usize {
    let padding = content
        .as_bytes()
        .iter()
        .rev()
        .take_while(|byte| **byte == b'=')
        .count()
        .min(2);
    content
        .len()
        .div_ceil(4)
        .saturating_mul(3)
        .saturating_sub(padding)
}

fn usize_to_u64(value: usize) -> u64 {
    u64::try_from(value).unwrap_or(u64::MAX)
}

fn materialize_shadow_entries(
    shadow_root: &Path,
    entries: &[RootFilesystemEntry],
) -> Result<(), SidecarError> {
    let mut ordered = entries.iter().collect::<Vec<_>>();
    ordered.sort_by_key(|entry| {
        let depth = entry.path.matches('/').count();
        let kind_rank = match entry.kind {
            crate::protocol::RootFilesystemEntryKind::Directory => 0,
            crate::protocol::RootFilesystemEntryKind::File => 1,
            crate::protocol::RootFilesystemEntryKind::Symlink => 2,
        };
        (kind_rank, depth, entry.path.as_str())
    });

    for entry in ordered {
        let shadow_path = shadow_path_for_guest(shadow_root, &entry.path);
        if let Some(parent) = shadow_path.parent() {
            fs::create_dir_all(parent).map_err(|error| {
                SidecarError::Io(format!(
                    "failed to create shadow parent for {}: {error}",
                    entry.path
                ))
            })?;
        }

        match entry.kind {
            crate::protocol::RootFilesystemEntryKind::Directory => {
                fs::create_dir_all(&shadow_path).map_err(|error| {
                    SidecarError::Io(format!(
                        "failed to materialize shadow directory {}: {error}",
                        entry.path
                    ))
                })?;
            }
            crate::protocol::RootFilesystemEntryKind::File => {
                let bytes = decode_root_entry_content(entry)?;
                fs::write(&shadow_path, bytes).map_err(|error| {
                    SidecarError::Io(format!(
                        "failed to materialize shadow file {}: {error}",
                        entry.path
                    ))
                })?;
            }
            crate::protocol::RootFilesystemEntryKind::Symlink => {
                let _ = fs::remove_file(&shadow_path);
                let _ = fs::remove_dir_all(&shadow_path);
                std::os::unix::fs::symlink(
                    entry.target.as_deref().ok_or_else(|| {
                        SidecarError::InvalidState(format!(
                            "root filesystem symlink {} requires a target",
                            entry.path
                        ))
                    })?,
                    &shadow_path,
                )
                .map_err(|error| {
                    SidecarError::Io(format!(
                        "failed to materialize shadow symlink {}: {error}",
                        entry.path
                    ))
                })?;
                continue;
            }
        }

        let mode = entry.mode.unwrap_or(match entry.kind {
            crate::protocol::RootFilesystemEntryKind::Directory => 0o755,
            crate::protocol::RootFilesystemEntryKind::File => {
                if entry.executable {
                    0o755
                } else {
                    0o644
                }
            }
            crate::protocol::RootFilesystemEntryKind::Symlink => 0o777,
        });
        fs::set_permissions(&shadow_path, fs::Permissions::from_mode(mode & 0o7777)).map_err(
            |error| {
                SidecarError::Io(format!(
                    "failed to set shadow mode on {}: {error}",
                    entry.path
                ))
            },
        )?;
    }

    Ok(())
}

fn decode_root_entry_content(entry: &RootFilesystemEntry) -> Result<Vec<u8>, SidecarError> {
    let content = entry.content.as_deref().unwrap_or_default();
    match entry
        .encoding
        .clone()
        .unwrap_or(crate::protocol::RootFilesystemEntryEncoding::Utf8)
    {
        crate::protocol::RootFilesystemEntryEncoding::Utf8 => Ok(content.as_bytes().to_vec()),
        crate::protocol::RootFilesystemEntryEncoding::Base64 => {
            base64::engine::general_purpose::STANDARD
                .decode(content)
                .map_err(|error| {
                    SidecarError::InvalidState(format!(
                        "invalid base64 root filesystem content for {}: {error}",
                        entry.path
                    ))
                })
        }
    }
}

fn shadow_path_for_guest(shadow_root: &std::path::Path, guest_path: &str) -> PathBuf {
    let normalized = normalize_guest_path(guest_path);
    let relative = normalized.trim_start_matches('/');
    if relative.is_empty() {
        return shadow_root.to_path_buf();
    }
    shadow_root.join(relative)
}

fn normalize_guest_path(path: &str) -> String {
    let mut segments = Vec::new();
    let absolute = path.starts_with('/');
    for segment in path.split('/') {
        match segment {
            "" | "." => {}
            ".." => {
                segments.pop();
            }
            other => segments.push(other),
        }
    }

    if !absolute {
        return format!("/{}", segments.join("/"));
    }
    if segments.is_empty() {
        String::from("/")
    } else {
        format!("/{}", segments.join("/"))
    }
}

fn parse_vm_dns_nameserver(value: &str) -> Result<SocketAddr, SidecarError> {
    use crate::state::VM_DNS_SERVERS_METADATA_KEY;

    if let Ok(address) = value.parse::<SocketAddr>() {
        return Ok(address);
    }
    if let Ok(ip) = value.parse::<IpAddr>() {
        return Ok(SocketAddr::new(ip, 53));
    }
    Err(SidecarError::InvalidState(format!(
        "invalid {} entry {value}; expected IP or IP:port",
        VM_DNS_SERVERS_METADATA_KEY
    )))
}

fn refresh_guest_command_path_env(
    guest_env: &mut BTreeMap<String, String>,
    command_guest_paths: &BTreeMap<String, String>,
) {
    let mut merged = Vec::new();
    let mut seen = BTreeSet::new();

    for guest_path in command_guest_paths.values() {
        let Some(parent) = Path::new(guest_path)
            .parent()
            .and_then(|path| path.to_str())
        else {
            continue;
        };
        let normalized = normalize_path(parent);
        if normalized == "/" {
            continue;
        }
        if seen.insert(normalized.clone()) {
            merged.push(normalized);
        }
    }

    for segment in DEFAULT_GUEST_PATH_ENV.split(':') {
        let normalized = normalize_path(segment);
        if seen.insert(normalized.clone()) {
            merged.push(normalized);
        }
    }

    if let Some(existing_path) = guest_env.get("PATH") {
        for segment in existing_path.split(':') {
            let trimmed = segment.trim();
            if trimmed.is_empty() {
                continue;
            }
            let normalized = if trimmed.starts_with('/') {
                normalize_path(trimmed)
            } else {
                trimmed.to_owned()
            };
            if seen.insert(normalized.clone()) {
                merged.push(normalized);
            }
        }
    }

    guest_env.insert(String::from("PATH"), merged.join(":"));
}

pub(crate) fn normalize_dns_hostname(hostname: &str) -> Result<String, SidecarError> {
    let normalized = hostname.trim().trim_end_matches('.').to_ascii_lowercase();
    if normalized.is_empty() {
        return Err(SidecarError::InvalidState(String::from(
            "DNS hostname must not be empty",
        )));
    }
    Ok(normalized)
}

// Retained for the native-root command-stub test; `python` is now a real
// command so production no longer prunes `/bin/python`.
#[cfg(test)]
fn prune_kernel_command_stub(
    kernel: &mut KernelVm<secure_exec_kernel::mount_table::MountTable>,
    path: &str,
) -> Result<(), SidecarError> {
    if !kernel.exists(path).map_err(kernel_error)? {
        return Ok(());
    }

    let content = kernel.read_file(path).map_err(kernel_error)?;
    if content == KERNEL_COMMAND_STUB {
        kernel.remove_file(path).map_err(kernel_error)?;
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::{
        bootstrap_native_root_filesystem, bootstrap_shadow_root,
        materialize_shadow_root_snapshot_entries, native_root_plugin_from_config,
        prune_kernel_command_stub, shadow_path_for_guest, KERNEL_COMMAND_STUB,
    };
    use crate::plugins::chunked_local::ChunkedLocalMountPlugin;
    use crate::protocol::{
        RootFilesystemDescriptor, RootFilesystemEntry, RootFilesystemEntryKind,
        RootFilesystemLowerDescriptor,
    };
    use secure_exec_bridge::FilesystemSnapshot;
    use secure_exec_kernel::kernel::{KernelVm, KernelVmConfig};
    use secure_exec_kernel::mount_plugin::{FileSystemPluginFactory, OpenFileSystemPluginRequest};
    use secure_exec_kernel::mount_table::{MountOptions, MountTable};
    use secure_exec_kernel::permissions::Permissions;
    use secure_exec_kernel::resource_accounting::ResourceLimits;
    use secure_exec_kernel::root_fs::{encode_snapshot, FilesystemEntry, RootFilesystemSnapshot};
    use secure_exec_kernel::vfs::VirtualFileSystem;
    use std::fs;
    use std::os::unix::fs::PermissionsExt;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn bootstrap_shadow_root_seeds_standard_directories() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let root = std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-test-{unique}"));
        fs::create_dir_all(&root).expect("temp shadow root should be created");

        bootstrap_shadow_root(&root).expect("shadow bootstrap should succeed");

        let tmp = shadow_path_for_guest(&root, "/tmp");
        let etc_agentos = shadow_path_for_guest(&root, "/etc/agentos");
        let usr_local_bin = shadow_path_for_guest(&root, "/usr/local/bin");

        assert!(tmp.is_dir(), "/tmp should exist in the shadow root");
        assert!(
            etc_agentos.is_dir(),
            "/etc/agentos should exist in the shadow root"
        );
        assert!(
            usr_local_bin.is_dir(),
            "/usr/local/bin should exist in the shadow root"
        );
        assert_eq!(
            fs::metadata(&tmp)
                .expect("/tmp metadata should be readable")
                .permissions()
                .mode()
                & 0o7777,
            0o1777,
            "/tmp should preserve its sticky-bit mode in the shadow root"
        );

        fs::remove_dir_all(&root).expect("temp shadow root should be removed");
    }

    #[test]
    fn native_root_config_opens_chunked_local_as_persistent_root() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let database_path =
            std::env::temp_dir().join(format!("secure-exec-native-root-{unique}.sqlite"));
        let block_root =
            std::env::temp_dir().join(format!("secure-exec-native-root-blocks-{unique}"));
        let native_root = native_root_plugin_from_config(Some(
            &secure_exec_vm_config::NativeRootFilesystemConfig {
                plugin: secure_exec_vm_config::MountPluginDescriptor {
                    id: "chunked_local".to_string(),
                    config: serde_json::json!({
                        "metadataPath": database_path.to_string_lossy(),
                        "blockRoot": block_root.to_string_lossy(),
                    }),
                },
                read_only: false,
            },
        ))
        .expect("native root config should parse")
        .expect("native root should be present");
        let config: serde_json::Value =
            serde_json::from_str(&native_root.plugin.config).expect("valid plugin config");
        let plugin = ChunkedLocalMountPlugin;
        let mut filesystem = plugin
            .open(OpenFileSystemPluginRequest {
                vm_id: "vm-test",
                guest_path: "/",
                read_only: false,
                config: &config,
                context: &(),
            })
            .expect("sqlite root should open");
        bootstrap_native_root_filesystem(
            filesystem.as_mut(),
            &RootFilesystemDescriptor {
                bootstrap_entries: vec![RootFilesystemEntry {
                    path: "/etc/agentos/boot.txt".to_string(),
                    kind: RootFilesystemEntryKind::File,
                    content: Some("booted".to_string()),
                    ..Default::default()
                }],
                ..Default::default()
            },
        )
        .expect("native root should bootstrap");

        let mut mount_table = MountTable::new_boxed_root(
            filesystem,
            MountOptions::new(native_root.plugin.id.clone()),
        );
        assert!(mount_table.exists("/home/agentos"));
        assert_eq!(
            mount_table
                .read_file("/etc/agentos/boot.txt")
                .expect("bootstrap file should be readable"),
            b"booted".to_vec()
        );
        mount_table
            .write_file("/home/agentos/persist.txt", b"persisted".to_vec())
            .expect("write through sqlite root should succeed");
        let mut kernel_config = KernelVmConfig::new("vm-test");
        kernel_config.permissions = Permissions::allow_all();
        let mut kernel = KernelVm::new(mount_table, kernel_config);
        kernel
            .write_file("/bin/python", KERNEL_COMMAND_STUB.to_vec())
            .expect("command stub should be writable");
        prune_kernel_command_stub(&mut kernel, "/bin/python")
            .expect("command stub prune should support native roots");
        assert!(
            !kernel.exists("/bin/python").expect("exists should succeed"),
            "stub should be pruned through the mounted root"
        );
        drop(kernel);

        let reopened = plugin
            .open(OpenFileSystemPluginRequest {
                vm_id: "vm-test",
                guest_path: "/",
                read_only: false,
                config: &config,
                context: &(),
            })
            .expect("chunked local root should reopen");
        let mut reopened = MountTable::new_boxed_root(reopened, MountOptions::new("chunked_local"));
        assert_eq!(
            reopened
                .read_file("/home/agentos/persist.txt")
                .expect("persisted file should survive reopen"),
            b"persisted".to_vec()
        );

        let _ = fs::remove_file(database_path);
        let _ = fs::remove_dir_all(block_root);
    }

    #[test]
    fn materialize_shadow_root_snapshot_entries_rejects_oversized_legacy_restored_snapshots() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let root = std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-limit-{unique}"));
        fs::create_dir_all(&root).expect("temp shadow root should be created");
        bootstrap_shadow_root(&root).expect("shadow bootstrap should succeed");

        let snapshot = RootFilesystemSnapshot {
            entries: vec![FilesystemEntry::file("/large.txt", b"four".to_vec())],
        };
        let loaded_snapshot = FilesystemSnapshot {
            format: String::from("agentos_filesystem_snapshot_v1"),
            bytes: encode_snapshot(&snapshot).expect("encode restored snapshot"),
        };
        let resource_limits = ResourceLimits {
            max_filesystem_bytes: Some(3),
            ..ResourceLimits::default()
        };

        let error = materialize_shadow_root_snapshot_entries(
            &root,
            &RootFilesystemDescriptor::default(),
            Some(&loaded_snapshot),
            &resource_limits,
        )
        .expect_err("oversized restored snapshot should be rejected");

        assert!(error.to_string().contains("exceeding limit 3"));
        fs::remove_dir_all(&root).expect("temp shadow root should be removed");
    }

    #[test]
    fn materialize_shadow_root_snapshot_entries_rejects_oversized_descriptor_before_writes() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-descriptor-{unique}"));
        fs::create_dir_all(&root).expect("temp shadow root should be created");
        bootstrap_shadow_root(&root).expect("shadow bootstrap should succeed");

        let descriptor = RootFilesystemDescriptor {
            lowers: vec![RootFilesystemLowerDescriptor::SnapshotRootFilesystemLower(
                crate::protocol::SnapshotRootFilesystemLower {
                    entries: vec![RootFilesystemEntry {
                        path: String::from("/large.txt"),
                        kind: RootFilesystemEntryKind::File,
                        mode: Some(0o644),
                        uid: Some(0),
                        gid: Some(0),
                        content: Some(String::from("four")),
                        encoding: Some(crate::protocol::RootFilesystemEntryEncoding::Utf8),
                        target: None,
                        executable: false,
                    }],
                },
            )],
            ..RootFilesystemDescriptor::default()
        };
        let resource_limits = ResourceLimits {
            max_filesystem_bytes: Some(3),
            ..ResourceLimits::default()
        };

        let error =
            materialize_shadow_root_snapshot_entries(&root, &descriptor, None, &resource_limits)
                .expect_err("oversized descriptor should be rejected");

        assert!(error.to_string().contains("exceeding limit 3"));
        assert!(
            !shadow_path_for_guest(&root, "/large.txt").exists(),
            "oversized descriptor must be rejected before materializing files"
        );
        fs::remove_dir_all(&root).expect("temp shadow root should be removed");
    }

    #[test]
    fn materialize_shadow_root_snapshot_entries_counts_implicit_parent_directories() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-parents-{unique}"));
        fs::create_dir_all(&root).expect("temp shadow root should be created");
        bootstrap_shadow_root(&root).expect("shadow bootstrap should succeed");

        let descriptor = RootFilesystemDescriptor {
            lowers: vec![RootFilesystemLowerDescriptor::SnapshotRootFilesystemLower(
                crate::protocol::SnapshotRootFilesystemLower {
                    entries: vec![RootFilesystemEntry {
                        path: String::from("/deep/nested/file.txt"),
                        kind: RootFilesystemEntryKind::File,
                        mode: Some(0o644),
                        uid: Some(0),
                        gid: Some(0),
                        content: Some(String::from("x")),
                        encoding: Some(crate::protocol::RootFilesystemEntryEncoding::Utf8),
                        target: None,
                        executable: false,
                    }],
                },
            )],
            ..RootFilesystemDescriptor::default()
        };
        let resource_limits = ResourceLimits {
            max_inode_count: Some(1),
            ..ResourceLimits::default()
        };

        let error =
            materialize_shadow_root_snapshot_entries(&root, &descriptor, None, &resource_limits)
                .expect_err("implicit parents should be rejected");

        assert!(error.to_string().contains("exceeding limit 1"));
        assert!(
            !shadow_path_for_guest(&root, "/deep").exists(),
            "implicit parents must not be materialized after rejection"
        );
        fs::remove_dir_all(&root).expect("temp shadow root should be removed");
    }

    #[test]
    fn materialize_shadow_root_snapshot_entries_rejects_duplicate_descriptor_entries() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-duplicates-{unique}"));
        fs::create_dir_all(&root).expect("temp shadow root should be created");
        bootstrap_shadow_root(&root).expect("shadow bootstrap should succeed");

        let duplicate_entry = RootFilesystemEntry {
            path: String::from("/dup.txt"),
            kind: RootFilesystemEntryKind::File,
            mode: Some(0o644),
            uid: Some(0),
            gid: Some(0),
            content: Some(String::new()),
            encoding: Some(crate::protocol::RootFilesystemEntryEncoding::Utf8),
            target: None,
            executable: false,
        };
        let descriptor = RootFilesystemDescriptor {
            lowers: vec![RootFilesystemLowerDescriptor::SnapshotRootFilesystemLower(
                crate::protocol::SnapshotRootFilesystemLower {
                    entries: vec![duplicate_entry.clone(), duplicate_entry],
                },
            )],
            ..RootFilesystemDescriptor::default()
        };
        let resource_limits = ResourceLimits {
            max_inode_count: Some(1),
            ..ResourceLimits::default()
        };

        let error =
            materialize_shadow_root_snapshot_entries(&root, &descriptor, None, &resource_limits)
                .expect_err("duplicate descriptor entries should be rejected");

        assert!(error.to_string().contains("exceeding limit 1"));
        assert!(
            !shadow_path_for_guest(&root, "/dup.txt").exists(),
            "duplicate descriptor must be rejected before materializing files"
        );
        fs::remove_dir_all(&root).expect("temp shadow root should be removed");
    }

    #[test]
    fn materialize_shadow_root_snapshot_entries_copies_custom_snapshot_files() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be monotonic")
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("secure-exec-sidecar-shadow-snapshot-{unique}"));
        fs::create_dir_all(&root).expect("temp shadow root should be created");
        bootstrap_shadow_root(&root).expect("shadow bootstrap should succeed");

        let descriptor = RootFilesystemDescriptor {
            lowers: vec![RootFilesystemLowerDescriptor::SnapshotRootFilesystemLower(
                crate::protocol::SnapshotRootFilesystemLower {
                    entries: vec![
                        RootFilesystemEntry {
                            path: String::from("/"),
                            kind: RootFilesystemEntryKind::Directory,
                            mode: Some(0o755),
                            uid: Some(0),
                            gid: Some(0),
                            content: None,
                            encoding: None,
                            target: None,
                            executable: false,
                        },
                        RootFilesystemEntry {
                            path: String::from("/hello.txt"),
                            kind: RootFilesystemEntryKind::File,
                            mode: Some(0o644),
                            uid: Some(0),
                            gid: Some(0),
                            content: Some(String::from("hello from snapshot\n")),
                            encoding: Some(crate::protocol::RootFilesystemEntryEncoding::Utf8),
                            target: None,
                            executable: false,
                        },
                    ],
                },
            )],
            ..RootFilesystemDescriptor::default()
        };

        materialize_shadow_root_snapshot_entries(
            &root,
            &descriptor,
            None,
            &ResourceLimits::default(),
        )
        .expect("snapshot entries should materialize into the shadow root");

        assert_eq!(
            fs::read_to_string(shadow_path_for_guest(&root, "/hello.txt"))
                .expect("shadow file should be readable"),
            "hello from snapshot\n"
        );

        fs::remove_dir_all(&root).expect("temp shadow root should be removed");
    }
}
