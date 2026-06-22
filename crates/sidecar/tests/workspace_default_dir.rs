mod support;

use secure_exec_sidecar::wire::{
    DisposeReason, DisposeVmRequest, GuestRuntimeKind, RequestPayload,
};
use std::time::Duration;
use support::{
    assert_node_available, authenticate_wire, collect_process_output_wire_with_timeout,
    create_vm_wire, execute_wire, new_sidecar, open_session_wire, temp_dir, wire_request, wire_vm,
    write_fixture,
};

/// The non-Alpine `/workspace` directory (the default agent working directory)
/// must ship in the default base filesystem snapshot. A guest that stats it in a
/// fresh VM, without bootstrapping the directory itself, proves the base layer
/// provides it by default with the base user's ownership (uid/gid 1000).
///
/// This test runs a guest V8 isolate, so it lives in its own integration-test
/// binary (one isolate per process) to avoid the multi-isolate process-teardown
/// segfault that surfaces when several guest-executing tests share a binary.
#[test]
fn default_base_layer_provides_workspace_directory() {
    assert_node_available();

    let mut sidecar = new_sidecar("workspace-default");
    let cwd = temp_dir("workspace-default-cwd");
    let js_entry = cwd.join("entry.mjs");

    write_fixture(
        &js_entry,
        r#"
import { statSync } from "node:fs";
const stat = statSync("/workspace");
console.log(`workspace:${stat.isDirectory()}:${stat.uid}:${stat.gid}`);
"#,
    );

    let connection_id = authenticate_wire(&mut sidecar, "conn-1");
    let session_id = open_session_wire(&mut sidecar, 2, &connection_id);

    let (vm_id, _) = create_vm_wire(
        &mut sidecar,
        3,
        &connection_id,
        &session_id,
        GuestRuntimeKind::JavaScript,
        &cwd,
    );

    execute_wire(
        &mut sidecar,
        4,
        &connection_id,
        &session_id,
        &vm_id,
        "proc-workspace",
        GuestRuntimeKind::JavaScript,
        &js_entry,
        Vec::new(),
    );
    let (stdout, stderr, exit) = collect_process_output_wire_with_timeout(
        &mut sidecar,
        &connection_id,
        &session_id,
        &vm_id,
        "proc-workspace",
        Duration::from_secs(10),
    );

    assert_eq!(stdout.trim(), "workspace:true:1000:1000", "stderr: {stderr}");
    assert_eq!(exit, 0);

    sidecar
        .dispatch_wire_blocking(wire_request(
            5,
            wire_vm(&connection_id, &session_id, &vm_id),
            RequestPayload::DisposeVmRequest(DisposeVmRequest {
                reason: DisposeReason::Requested,
            }),
        ))
        .expect("dispose vm");
}
