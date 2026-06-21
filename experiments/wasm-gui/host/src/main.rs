//! Native host for the wasm GUI guest, built on the STANDARD secure-exec Rust client
//! (`crates/secure-exec-client`). It runs `guest.wasm` INSIDE the real secure-exec V8 sidecar and
//! renders the frames it produces. Per SPEC §1a.2 this is the product path: no wasmer, no
//! node:wasi, no TypeScript client, no `Command::new` in the execution/render path — the only
//! process spawn is the sidecar itself, done by the Rust client's transport.
//!
//! Modes:
//!   wasm-gui-host --capture <out.bin> --guest <guest.wasm> [--sidecar <bin>]
//!       Run the guest once through secure-exec, read back the framebuffer it wrote, save raw bytes.
//!       Headless; this is the automated-proof path.
//!   wasm-gui-host --window --guest <guest.wasm> [--sidecar <bin>]   (needs `--features window`)
//!       Stream frames from the guest (run in --loop mode in the sidecar) into a native winit
//!       window; forward input back through the client. Manual demo.

use std::collections::HashMap;
use std::sync::Arc;

use base64::prelude::*;
use secure_exec_client::transport::SidecarProcess;
use secure_exec_client::wire;

/// PREAD chunk size: stays well under the 1 MiB default max frame even after base64 (4/3) blowup.
const READ_CHUNK: u64 = 256 * 1024;

/// Trusted VM config: default bundled-base filesystem + allow-all permission policy (fs reads are
/// denied by default, which would block loading the wasm from /tmp). `"allow"` maps to the untagged
/// `FsPermissionScope::Mode(Allow)` etc.
const VM_CONFIG_JSON: &str = r#"{"permissions":{"fs":"allow","network":"allow","childProcess":"allow","process":"allow","env":"allow","binding":"allow"}}"#;

type Result<T> = std::result::Result<T, String>;

/// A connected secure-exec session bound to one VM, over the standard Rust client transport.
struct Session {
    t: Arc<SidecarProcess>,
    connection_id: String,
    session_id: String,
    vm_id: String,
}

impl Session {
    async fn connect(sidecar_bin: Option<String>) -> Result<Self> {
        let t = SidecarProcess::spawn(sidecar_bin)
            .await
            .map_err(|e| format!("spawn sidecar: {e}"))?;

        // Authenticate. The sidecar ALLOCATES the real connection id and returns it; the id we send
        // in the ownership scope here is a bootstrap placeholder it ignores. The bridge_version must
        // match the bridge contract the sidecar was built against.
        let auth = request(
            &t,
            conn_scope("bootstrap"),
            wire::RequestPayload::AuthenticateRequest(wire::AuthenticateRequest {
                client_name: "wasm-gui-host".into(),
                auth_token: "secure-exec-core-client-token".into(),
                protocol_version: wire::PROTOCOL_VERSION,
                bridge_version: secure_exec_bridge::bridge_contract().version,
            }),
        )
        .await?;
        let connection_id = match auth {
            wire::ResponsePayload::AuthenticatedResponse(a) => {
                t.set_max_frame_bytes(a.max_frame_bytes as usize);
                a.connection_id
            }
            other => return Err(format!("expected Authenticated, got {other:?}")),
        };

        // Open a session (using the sidecar-allocated connection id).
        let sess = request(
            &t,
            conn_scope(&connection_id),
            wire::RequestPayload::OpenSessionRequest(wire::OpenSessionRequest {
                placement: wire::SidecarPlacement::SidecarPlacementShared(
                    wire::SidecarPlacementShared { pool: None },
                ),
                metadata: HashMap::new(),
            }),
        )
        .await?;
        let session_id = match sess {
            wire::ResponsePayload::SessionOpenedResponse(s) => s.session_id,
            other => return Err(format!("expected SessionOpened, got {other:?}")),
        };

        // Create a WebAssembly VM with the default (bundled base) filesystem.
        let vm = request(
            &t,
            wire::OwnershipScope::SessionOwnership(wire::SessionOwnership {
                connection_id: connection_id.clone(),
                session_id: session_id.clone(),
            }),
            wire::RequestPayload::CreateVmRequest(wire::CreateVmRequest {
                runtime: wire::GuestRuntimeKind::WebAssembly,
                // Trusted VM config (we own this VM): grant the guest fs/process access so the
                // sidecar can load the wasm from /tmp and the guest can write/read /data. The
                // default policy denies fs reads.
                config: VM_CONFIG_JSON.into(),
            }),
        )
        .await?;
        let vm_id = match vm {
            wire::ResponsePayload::VmCreatedResponse(v) => v.vm_id,
            other => return Err(format!("expected VmCreated, got {other:?}")),
        };

        Ok(Session { t, connection_id, session_id, vm_id })
    }

    fn vm_scope(&self) -> wire::OwnershipScope {
        wire::OwnershipScope::VmOwnership(wire::VmOwnership {
            connection_id: self.connection_id.clone(),
            session_id: self.session_id.clone(),
            vm_id: self.vm_id.clone(),
        })
    }

    async fn fs_call(&self, req: wire::GuestFilesystemCallRequest) -> Result<wire::GuestFilesystemResultResponse> {
        let r = request(&self.t, self.vm_scope(), wire::RequestPayload::GuestFilesystemCallRequest(req)).await?;
        match r {
            wire::ResponsePayload::GuestFilesystemResultResponse(res) => Ok(res),
            other => Err(format!("expected GuestFilesystemResult, got {other:?}")),
        }
    }

    async fn mkdir(&self, path: &str) -> Result<()> {
        let mut req = fs_req(wire::GuestFilesystemOperation::Mkdir, path, None, None);
        req.recursive = true;
        self.fs_call(req).await?;
        Ok(())
    }

    async fn write_file(&self, path: &str, data: &[u8]) -> Result<()> {
        let req = fs_req(
            wire::GuestFilesystemOperation::WriteFile,
            path,
            Some(BASE64_STANDARD.encode(data)),
            Some(wire::RootFilesystemEntryEncoding::Base64),
        );
        self.fs_call(req).await?;
        Ok(())
    }

    async fn execute(&self, process_id: &str, entrypoint: &str, args: &[&str]) -> Result<()> {
        self.execute_env(process_id, entrypoint, args, HashMap::new()).await
    }

    async fn execute_env(
        &self,
        process_id: &str,
        entrypoint: &str,
        args: &[&str],
        env: HashMap<String, String>,
    ) -> Result<()> {
        let r = request(
            &self.t,
            self.vm_scope(),
            wire::RequestPayload::ExecuteRequest(wire::ExecuteRequest {
                process_id: process_id.into(),
                command: None,
                runtime: Some(wire::GuestRuntimeKind::WebAssembly),
                // The sidecar loads the wasm module from this HOST path (trusted client input).
                entrypoint: Some(entrypoint.into()),
                args: args.iter().map(|s| s.to_string()).collect(),
                env,
                cwd: Some("/".into()),
                wasm_permission_tier: Some(wire::WasmPermissionTier::Full),
            }),
        )
        .await?;
        match r {
            wire::ResponsePayload::ProcessStartedResponse(_) => Ok(()),
            other => Err(format!("expected ProcessStarted, got {other:?}")),
        }
    }

    /// Read a guest file fully via repeated PREAD chunks (each chunk fits inside one wire frame).
    async fn read_file_chunked(&self, path: &str) -> Result<Vec<u8>> {
        let mut out = Vec::new();
        let mut offset = 0u64;
        loop {
            let mut req = fs_req(wire::GuestFilesystemOperation::Pread, path, None, None);
            req.len = Some(READ_CHUNK);
            req.offset = Some(offset);
            let res = self.fs_call(req).await?;
            let chunk = decode_fs_content(&res)?;
            let n = chunk.len() as u64;
            out.extend_from_slice(&chunk);
            if n < READ_CHUNK {
                break;
            }
            offset += n;
        }
        Ok(out)
    }

    async fn write_stdin(&self, process_id: &str, data: &[u8]) -> Result<()> {
        request(
            &self.t,
            self.vm_scope(),
            wire::RequestPayload::WriteStdinRequest(wire::WriteStdinRequest {
                process_id: process_id.into(),
                chunk: data.to_vec(),
            }),
        )
        .await?;
        Ok(())
    }

    fn shutdown(&self) {
        self.t.kill_child();
    }
}

fn abs_path(p: &str) -> Result<String> {
    std::fs::canonicalize(p)
        .map_err(|e| format!("resolve guest path {p}: {e}"))?
        .to_str()
        .map(|s| s.to_string())
        .ok_or_else(|| "guest path is not valid UTF-8".to_string())
}

fn conn_scope(connection_id: &str) -> wire::OwnershipScope {
    wire::OwnershipScope::ConnectionOwnership(wire::ConnectionOwnership {
        connection_id: connection_id.into(),
    })
}

fn fs_req(
    operation: wire::GuestFilesystemOperation,
    path: &str,
    content: Option<String>,
    encoding: Option<wire::RootFilesystemEntryEncoding>,
) -> wire::GuestFilesystemCallRequest {
    wire::GuestFilesystemCallRequest {
        operation,
        path: path.into(),
        destination_path: None,
        target: None,
        content,
        encoding,
        recursive: false,
        mode: None,
        uid: None,
        gid: None,
        atime_ms: None,
        mtime_ms: None,
        len: None,
        offset: None,
    }
}

fn decode_fs_content(res: &wire::GuestFilesystemResultResponse) -> Result<Vec<u8>> {
    match (&res.content, &res.encoding) {
        (Some(c), Some(wire::RootFilesystemEntryEncoding::Base64)) => {
            BASE64_STANDARD.decode(c).map_err(|e| format!("base64 decode: {e}"))
        }
        (Some(c), _) => Ok(c.clone().into_bytes()),
        (None, _) => Ok(Vec::new()),
    }
}

async fn request(
    t: &Arc<SidecarProcess>,
    ownership: wire::OwnershipScope,
    payload: wire::RequestPayload,
) -> Result<wire::ResponsePayload> {
    let r = t
        .request_wire(ownership, payload)
        .await
        .map_err(|e| format!("transport: {e}"))?;
    if let wire::ResponsePayload::RejectedResponse(rej) = &r {
        return Err(format!("rejected [{}]: {}", rej.code, rej.message));
    }
    Ok(r)
}

async fn wait_for_exit(
    events: &mut tokio::sync::broadcast::Receiver<(wire::OwnershipScope, wire::EventPayload)>,
    process_id: &str,
) -> Result<i32> {
    loop {
        match events.recv().await {
            Ok((_, wire::EventPayload::ProcessExitedEvent(e))) if e.process_id == process_id => {
                return Ok(e.exit_code);
            }
            Ok(_) => continue,
            Err(tokio::sync::broadcast::error::RecvError::Lagged(_)) => continue,
            Err(e) => return Err(format!("event stream closed: {e}")),
        }
    }
}

// ---- capture mode (headless, automated proof) --------------------------------------------------

async fn run_capture(sidecar: Option<String>, guest: &str, out: &str) -> Result<()> {
    let s = Session::connect(sidecar).await?;
    // Run the capture, then always kill the sidecar child regardless of success/failure.
    let result = capture_inner(&s, guest, out).await;
    s.shutdown();
    result
}

async fn capture_inner(s: &Session, guest: &str, out: &str) -> Result<()> {
    let guest_abs = abs_path(guest)?;
    s.mkdir("/data").await?;

    let mut events = s.t.subscribe_wire_events();
    s.execute("proc-capture", &guest_abs, &["--out", "/data/frame.bin"])
        .await?;
    // Bound the wait so a wedged guest can't hang the host forever.
    let code = tokio::time::timeout(
        std::time::Duration::from_secs(60),
        wait_for_exit(&mut events, "proc-capture"),
    )
    .await
    .map_err(|_| "timed out waiting for guest to exit".to_string())??;
    if code != 0 {
        return Err(format!("guest exited with code {code}"));
    }

    let bytes = s.read_file_chunked("/data/frame.bin").await?;
    std::fs::write(out, &bytes).map_err(|e| format!("write {out}: {e}"))?;
    eprintln!("secure-exec: captured {} bytes -> {out}", bytes.len());
    Ok(())
}

// ---- exec mode (run a long-lived guest, e.g. Xvfb, streaming its output for a timeout) ---------

async fn run_exec(sidecar: Option<String>, guest: &str, args: &[String], timeout_s: u64) -> Result<()> {
    let s = Session::connect(sidecar).await?;
    let guest_abs = abs_path(guest)?;
    s.mkdir("/data").await.ok();
    s.mkdir("/tmp/.X11-unix").await.ok();
    let mut events = s.t.subscribe_wire_events();
    let argv: Vec<&str> = args.iter().map(|x| x.as_str()).collect();
    s.execute("proc-exec", &guest_abs, &argv).await?;
    eprintln!("secure-exec: started {guest_abs} {args:?}");
    let deadline = tokio::time::Instant::now() + std::time::Duration::from_secs(timeout_s);
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        match tokio::time::timeout(remaining, events.recv()).await {
            Ok(Ok((_, wire::EventPayload::ProcessOutputEvent(o)))) if o.process_id == "proc-exec" => {
                let txt = String::from_utf8_lossy(&o.chunk);
                let ch = if matches!(o.channel, wire::StreamChannel::Stderr) { "err" } else { "out" };
                eprint!("[{ch}] {txt}");
            }
            Ok(Ok((_, wire::EventPayload::ProcessExitedEvent(e)))) if e.process_id == "proc-exec" => {
                eprintln!("\nsecure-exec: guest exited with code {}", e.exit_code);
                s.shutdown();
                return Ok(());
            }
            Ok(Ok(_)) => continue,
            Ok(Err(tokio::sync::broadcast::error::RecvError::Lagged(_))) => continue,
            Ok(Err(_)) => break,
            Err(_) => break,
        }
    }
    eprintln!("\nsecure-exec: timeout ({timeout_s}s) reached");
    // Best-effort: read back a guest file (e.g. Xvfb's -fbdir framebuffer) before disposing.
    if let Ok(rb) = std::env::var("READBACK") {
        if let Some((gpath, hpath)) = rb.split_once(':') {
            match s.read_file_chunked(gpath).await {
                Ok(bytes) => {
                    let _ = std::fs::write(hpath, &bytes);
                    eprintln!("secure-exec: read back {} ({} bytes) -> {hpath}", gpath, bytes.len());
                }
                Err(e) => eprintln!("secure-exec: readback {gpath} failed: {e}"),
            }
        }
    }
    s.shutdown();
    Ok(())
}

/// Run an X server guest and an X client guest concurrently in the SAME VM, so they share the
/// kernel socket table and the client can connect to the server's AF_UNIX socket
/// (/tmp/.X11-unix/X0). After the client finishes, read the server's framebuffer file back out.
async fn run_xdemo(
    sidecar: Option<String>,
    server: &str,
    clients: &[String],
    server_args: &[String],
    fb_out: Option<&str>,
    timeout_s: u64,
    fonts_dir: Option<&str>,
) -> Result<()> {
    let s = Session::connect(sidecar).await?;
    let server_abs = abs_path(server)?;
    // Each client spec is "wasm_path arg1 arg2 ...": resolve the path, keep the args.
    let mut client_specs: Vec<(String, Vec<String>)> = Vec::new();
    for spec in clients {
        let mut parts = spec.split_whitespace();
        let path = parts.next().ok_or_else(|| "empty --client spec".to_string())?;
        let path_abs = abs_path(path)?;
        let cargs: Vec<String> = parts.map(|x| x.to_string()).collect();
        client_specs.push((path_abs, cargs));
    }
    s.mkdir("/data").await.ok();
    s.mkdir("/tmp/.X11-unix").await.ok();
    // Provide a twm config that auto-places windows (twm's default placement is interactive and
    // would never map a window without user input). Harmless for non-twm runs.
    s.mkdir("/root").await.ok();
    s.write_file(
        "/root/.twmrc",
        b"RandomPlacement\nUsePPosition \"on\"\nNoGrabServer\nNoTitleFocus\n",
    )
    .await
    .ok();

    // Install X core fonts into the VM (so the X server can serve real fonts via -fp /fonts).
    if let Some(fdir) = fonts_dir {
        s.mkdir("/fonts").await.ok();
        let entries = std::fs::read_dir(fdir).map_err(|e| format!("read fonts dir {fdir}: {e}"))?;
        let mut n = 0;
        for entry in entries.flatten() {
            if !entry.path().is_file() {
                continue;
            }
            let name = entry.file_name();
            let name = name.to_string_lossy();
            let bytes = std::fs::read(entry.path()).map_err(|e| format!("read font {name}: {e}"))?;
            s.write_file(&format!("/fonts/{name}"), &bytes).await?;
            n += 1;
        }
        eprintln!("secure-exec: installed {n} font files into /fonts");
    }

    let mut events = s.t.subscribe_wire_events();

    // Start the X server. It binds /tmp/.X11-unix/X0 and blocks in its dispatch loop.
    let sargv: Vec<&str> = server_args.iter().map(|x| x.as_str()).collect();
    s.execute("xserver", &server_abs, &sargv).await?;
    eprintln!("secure-exec: started X server {server_abs} {server_args:?}");

    let deadline = tokio::time::Instant::now() + std::time::Duration::from_secs(timeout_s);
    let mut server_ready = false;
    let mut wm_ready = false;                    // first client (the WM) is up + idle in its loop
    let mut launched = 0usize;                  // number of clients launched so far
    let mut last_activity = tokio::time::Instant::now(); // last output from the latest client
    let mut last_launch = tokio::time::Instant::now();
    let mut exited_ok = 0usize;
    let mut exited_bad = 0usize;
    // A client is "settled" once it has produced no output for this long after launch — i.e. it
    // finished initializing and is idle in its event loop. We launch the NEXT client only then, so
    // heavy libX11 startups never contend on the sidecar's single sync-RPC thread. Event-driven,
    // not a fixed sleep (this mirrors a session manager waiting for the WM before starting apps).
    let settle = std::time::Duration::from_millis(1500);
    let min_after_launch = std::time::Duration::from_millis(800);
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        // Launch the next client once the previous one is ready. The first client is the window
        // manager: gate apps on it being fully up (its "WM ready" signal, or a generous fallback),
        // not just a quiet pause — twm has quiet stretches mid-init while awaiting X replies.
        if server_ready
            && launched > 0
            && !wm_ready
            && tokio::time::Instant::now().duration_since(last_launch)
                >= std::time::Duration::from_secs(12)
        {
            wm_ready = true; // fallback: assume the WM is up if it has run a while
        }
        let can_launch_next = launched == 0 || wm_ready;
        if server_ready && launched < client_specs.len() && can_launch_next {
            let now = tokio::time::Instant::now();
            let prev_settled = launched == 0
                || (now.duration_since(last_activity) >= settle
                    && now.duration_since(last_launch) >= min_after_launch);
            if prev_settled {
                let (path, cargs) = &client_specs[launched];
                let id = format!("xclient{launched}");
                let argv: Vec<&str> = cargs.iter().map(|x| x.as_str()).collect();
                let mut cenv = HashMap::new();
                cenv.insert("DISPLAY".to_string(), ":0".to_string());
                cenv.insert("HOME".to_string(), "/root".to_string());
                s.execute_env(&id, path, &argv, cenv).await?;
                eprintln!("secure-exec: launched {id} ({path})");
                launched += 1;
                last_launch = tokio::time::Instant::now();
                last_activity = tokio::time::Instant::now();
            }
        }
        let poll = remaining.min(std::time::Duration::from_millis(300));
        match tokio::time::timeout(poll, events.recv()).await {
            Ok(Ok((_, wire::EventPayload::ProcessOutputEvent(o)))) => {
                let txt = String::from_utf8_lossy(&o.chunk);
                let who = if o.process_id == "xserver" { "srv" } else { &o.process_id };
                let ch = if matches!(o.channel, wire::StreamChannel::Stderr) { "err" } else { "out" };
                eprint!("[{who}/{ch}] {txt}");
                if !server_ready && o.process_id == "xserver" && txt.contains("m_pre_dispatch") {
                    server_ready = true;
                    eprintln!("secure-exec: server is serving; launching {} X client(s) as each settles", client_specs.len());
                }
                // Track activity of the most-recently-launched client to detect when it settles.
                if launched > 0 && o.process_id == format!("xclient{}", launched - 1) {
                    last_activity = tokio::time::Instant::now();
                }
                // The first client (window manager) announces readiness when it enters its event
                // loop (twm prints "handleevents"; JWM/others similar). Gate apps on this — a real
                // session manager waits for the WM before starting clients.
                if !wm_ready && o.process_id == "xclient0" && txt.contains("handleevents") {
                    wm_ready = true;
                    eprintln!("secure-exec: window manager is ready; starting apps");
                }
            }
            Ok(Ok((_, wire::EventPayload::ProcessExitedEvent(e)))) => {
                eprintln!("\nsecure-exec: {} exited with code {}", e.process_id, e.exit_code);
                if e.process_id.starts_with("xclient") {
                    if e.exit_code == 0 { exited_ok += 1; } else { exited_bad += 1; }
                    if exited_ok + exited_bad >= client_specs.len() {
                        break;
                    }
                }
                if e.process_id == "xserver" {
                    break;
                }
            }
            Ok(Ok(_)) => continue,
            Ok(Err(tokio::sync::broadcast::error::RecvError::Lagged(_))) => continue,
            Ok(Err(_)) => break,
            Err(_) => continue, // poll timeout: loop to re-check the settle condition
        }
    }

    if !server_ready {
        eprintln!("\nsecure-exec: server never reached dispatch; clients not launched");
    }
    eprintln!(
        "secure-exec: {exited_ok}/{} X client(s) completed successfully ({exited_bad} failed)",
        client_specs.len()
    );

    // Read the server framebuffer out via the kernel VFS (works while the server still blocks).
    if let Some(hpath) = fb_out {
        match s.read_file_chunked("/data/Xvfb_screen0").await {
            Ok(bytes) => {
                let _ = std::fs::write(hpath, &bytes);
                eprintln!("secure-exec: read back framebuffer ({} bytes) -> {hpath}", bytes.len());
            }
            Err(e) => eprintln!("secure-exec: framebuffer readback failed: {e}"),
        }
    }
    s.shutdown();
    Ok(())
}

// ---- arg parsing + entrypoint ------------------------------------------------------------------

struct Args {
    mode_window: bool,
    capture_out: Option<String>,
    guest: String,
    sidecar: Option<String>,
    exec: bool,
    exec_args: Vec<String>,
    timeout: u64,
    xdemo: bool,
    server: Option<String>,
    clients: Vec<String>,
    fb_out: Option<String>,
    fonts_dir: Option<String>,
}

fn parse_args() -> Args {
    let argv: Vec<String> = std::env::args().collect();
    let mut a = Args {
        mode_window: false,
        capture_out: None,
        guest: "target/wasm32-wasip1/release/guest.wasm".into(),
        sidecar: std::env::var("SECURE_EXEC_SIDECAR_BIN").ok(),
        exec: false,
        exec_args: Vec::new(),
        timeout: 8,
        xdemo: false,
        server: None,
        clients: Vec::new(),
        fb_out: None,
        fonts_dir: None,
    };
    let mut i = 1;
    while i < argv.len() {
        match argv[i].as_str() {
            "--window" => a.mode_window = true,
            "--exec" => a.exec = true,
            "--xdemo" => a.xdemo = true,
            "--server" => {
                i += 1;
                a.server = argv.get(i).cloned();
            }
            "--client" => {
                i += 1;
                if let Some(c) = argv.get(i) {
                    a.clients.push(c.clone());
                }
            }
            "--fb-out" => {
                i += 1;
                a.fb_out = argv.get(i).cloned();
            }
            "--fonts-dir" => {
                i += 1;
                a.fonts_dir = argv.get(i).cloned();
            }
            "--capture" => {
                i += 1;
                a.capture_out = argv.get(i).cloned();
            }
            "--guest" => {
                i += 1;
                if let Some(g) = argv.get(i) {
                    a.guest = g.clone();
                }
            }
            "--sidecar" => {
                i += 1;
                a.sidecar = argv.get(i).cloned();
            }
            "--timeout" => {
                i += 1;
                a.timeout = argv.get(i).and_then(|s| s.parse().ok()).unwrap_or(8);
            }
            "--" => {
                // everything after `--` is passed to the guest
                a.exec_args = argv[i + 1..].to_vec();
                break;
            }
            _ => {}
        }
        i += 1;
    }
    a
}

#[tokio::main(flavor = "multi_thread", worker_threads = 2)]
async fn main() {
    let args = parse_args();

    if args.xdemo {
        let server = args.server.clone().unwrap_or_else(|| {
            eprintln!("--xdemo requires --server <Xvfb.wasm>");
            std::process::exit(2);
        });
        if args.clients.is_empty() {
            eprintln!("--xdemo requires at least one --client \"<xclient.wasm> [args...]\"");
            std::process::exit(2);
        }
        if let Err(e) = run_xdemo(
            args.sidecar.clone(),
            &server,
            &args.clients,
            &args.exec_args,
            args.fb_out.as_deref(),
            args.timeout,
            args.fonts_dir.as_deref(),
        )
        .await
        {
            eprintln!("xdemo failed: {e}");
            std::process::exit(1);
        }
        return;
    }

    if args.exec {
        if let Err(e) = run_exec(args.sidecar.clone(), &args.guest, &args.exec_args, args.timeout).await {
            eprintln!("exec failed: {e}");
            std::process::exit(1);
        }
        return;
    }

    if let Some(out) = args.capture_out.clone() {
        if let Err(e) = run_capture(args.sidecar.clone(), &args.guest, &out).await {
            eprintln!("capture failed: {e}");
            std::process::exit(1);
        }
        return;
    }

    if args.mode_window {
        #[cfg(feature = "window")]
        {
            if let Err(e) = window::run(args.sidecar.clone(), args.guest.clone()).await {
                eprintln!("window failed: {e}");
                std::process::exit(1);
            }
            return;
        }
        #[cfg(not(feature = "window"))]
        {
            eprintln!(
                "built without the `window` feature.\n\
                 Build the interactive demo with:  cargo run -p wasm-gui-host --features window -- \\\n\
                     --window --guest target/wasm32-wasip1/release/guest.wasm"
            );
            std::process::exit(0);
        }
    }

    eprintln!(
        "usage:\n  wasm-gui-host --capture <out.bin> --guest <guest.wasm> [--sidecar <bin>]\n  \
         wasm-gui-host --window --guest <guest.wasm> [--sidecar <bin>]   (needs --features window)"
    );
    std::process::exit(2);
}

// ---- window mode (manual demo; needs a display) ------------------------------------------------

#[cfg(feature = "window")]
mod window {
    use super::*;
    use std::rc::Rc;
    use std::sync::mpsc as std_mpsc;

    use softbuffer::{Context, Surface};
    use winit::application::ApplicationHandler;
    use winit::event::{ElementState, WindowEvent};
    use winit::event_loop::{ActiveEventLoop, EventLoop};
    use winit::keyboard::{Key, NamedKey};
    use winit::window::{Window, WindowId};

    const MAGIC: &[u8; 4] = b"SXFB";

    pub struct Frame {
        pub w: u32,
        pub h: u32,
        pub rgba: Vec<u8>,
    }

    /// Accumulates guest stdout chunks and yields whole v0 frames.
    struct FrameParser {
        buf: Vec<u8>,
    }
    impl FrameParser {
        fn new() -> Self {
            Self { buf: Vec::new() }
        }
        fn push(&mut self, chunk: &[u8], out: &mut Vec<Frame>) {
            self.buf.extend_from_slice(chunk);
            loop {
                if self.buf.len() < 12 {
                    return;
                }
                if &self.buf[0..4] != MAGIC {
                    // Resync: drop one byte until magic aligns.
                    self.buf.remove(0);
                    continue;
                }
                let w = u32::from_le_bytes(self.buf[4..8].try_into().unwrap());
                let h = u32::from_le_bytes(self.buf[8..12].try_into().unwrap());
                // The guest output is untrusted: reject implausible dimensions (OOM guard) and
                // resync rather than buffering gigabytes waiting for a frame that never completes.
                const MAX_DIM: u32 = 8192;
                let need = (w >= 1 && h >= 1 && w <= MAX_DIM && h <= MAX_DIM)
                    .then(|| (w as usize).checked_mul(h as usize).and_then(|p| p.checked_mul(4)))
                    .flatten()
                    .map(|p| p + 12);
                let Some(need) = need else {
                    self.buf.remove(0);
                    continue;
                };
                if self.buf.len() < need {
                    return;
                }
                let rgba = self.buf[12..need].to_vec();
                out.push(Frame { w, h, rgba });
                self.buf.drain(0..need);
            }
        }
    }

    /// Runs the secure-exec session on the tokio runtime, streaming frames to the winit thread and
    /// receiving input tokens back. Spawned before the event loop takes the main thread.
    pub async fn run(sidecar: Option<String>, guest: String) -> Result<()> {
        let s = Session::connect(sidecar).await?;
        let guest_abs = abs_path(&guest)?;

        let (frame_tx, frame_rx) = std_mpsc::channel::<Frame>();
        let (input_tx, mut input_rx) = tokio::sync::mpsc::unbounded_channel::<String>();

        let mut events = s.t.subscribe_wire_events();
        s.execute("proc-window", &guest_abs, &["--loop"]).await?;

        let s = Arc::new(s);
        let s_in = s.clone();
        // Forward input tokens to the guest stdin.
        tokio::spawn(async move {
            while let Some(line) = input_rx.recv().await {
                let _ = s_in.write_stdin("proc-window", line.as_bytes()).await;
            }
        });
        // Pump guest stdout frames to the window.
        tokio::spawn(async move {
            let mut parser = FrameParser::new();
            loop {
                match events.recv().await {
                    Ok((_, wire::EventPayload::ProcessOutputEvent(o)))
                        if o.process_id == "proc-window"
                            && matches!(o.channel, wire::StreamChannel::Stdout) =>
                    {
                        let mut frames = Vec::new();
                        parser.push(&o.chunk, &mut frames);
                        for f in frames {
                            if frame_tx.send(f).is_err() {
                                return;
                            }
                        }
                    }
                    Ok((_, wire::EventPayload::ProcessExitedEvent(e)))
                        if e.process_id == "proc-window" =>
                    {
                        return;
                    }
                    Ok(_) => continue,
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(_)) => continue,
                    Err(_) => return,
                }
            }
        });

        // The winit event loop must own the main thread.
        let event_loop = EventLoop::new().map_err(|e| format!("event loop: {e}"))?;
        let mut app = App {
            frame_rx,
            input_tx,
            window: None,
            surface: None,
            last: None,
            _session: s,
        };
        event_loop
            .run_app(&mut app)
            .map_err(|e| format!("run_app: {e}"))
    }

    struct App {
        frame_rx: std_mpsc::Receiver<Frame>,
        input_tx: tokio::sync::mpsc::UnboundedSender<String>,
        window: Option<Rc<Window>>,
        surface: Option<Surface<Rc<Window>, Rc<Window>>>,
        last: Option<Frame>,
        _session: Arc<Session>,
    }

    impl App {
        fn redraw(&mut self) {
            while let Ok(f) = self.frame_rx.try_recv() {
                self.last = Some(f);
            }
            let (Some(surface), Some(frame)) = (self.surface.as_mut(), self.last.as_ref()) else {
                return;
            };
            surface
                .resize(
                    std::num::NonZeroU32::new(frame.w).unwrap(),
                    std::num::NonZeroU32::new(frame.h).unwrap(),
                )
                .unwrap();
            let mut buf = surface.buffer_mut().unwrap();
            for (i, px) in buf.iter_mut().enumerate() {
                let s = i * 4;
                let r = frame.rgba[s] as u32;
                let g = frame.rgba[s + 1] as u32;
                let b = frame.rgba[s + 2] as u32;
                *px = (r << 16) | (g << 8) | b;
            }
            buf.present().unwrap();
        }
    }

    impl ApplicationHandler for App {
        fn resumed(&mut self, event_loop: &ActiveEventLoop) {
            let attrs = Window::default_attributes().with_title("secure-exec — wasm GUI");
            let window = Rc::new(event_loop.create_window(attrs).unwrap());
            let context = Context::new(window.clone()).unwrap();
            let surface = Surface::new(&context, window.clone()).unwrap();
            self.surface = Some(surface);
            self.window = Some(window);
        }

        fn window_event(&mut self, event_loop: &ActiveEventLoop, _id: WindowId, event: WindowEvent) {
            match event {
                WindowEvent::CloseRequested => {
                    let _ = self.input_tx.send("q".into());
                    event_loop.exit();
                }
                WindowEvent::CursorMoved { position, .. } => {
                    let _ = self
                        .input_tx
                        .send(format!("p {} {}", position.x as i32, position.y as i32));
                    if let Some(w) = self.window.as_ref() {
                        w.request_redraw();
                    }
                }
                WindowEvent::KeyboardInput { event, .. } => {
                    if event.state == ElementState::Pressed {
                        if let Key::Named(NamedKey::Escape) = event.logical_key {
                            let _ = self.input_tx.send("q".into());
                            event_loop.exit();
                        }
                    }
                }
                WindowEvent::RedrawRequested => {
                    self.redraw();
                    if let Some(w) = self.window.as_ref() {
                        w.request_redraw();
                    }
                }
                _ => {}
            }
        }
    }
}
