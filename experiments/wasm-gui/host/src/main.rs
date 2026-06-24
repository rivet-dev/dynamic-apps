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
use secure_exec_client::transport::SidecarTransport;
use secure_exec_client::wire;

/// PREAD chunk size: stays well under the 1 MiB default max frame even after base64 (4/3) blowup.
const READ_CHUNK: u64 = 256 * 1024;

/// Paths libX11 has the locale database compiled into (it ignores XLOCALEDIR on wasi). The locale
/// tree is staged at these exact in-VM paths so XCreateFontSet / XSupportsLocale work. These match
/// the `XLOCALEDIR`/`XLOCALELIBDIR` baked into the current libX11.a build.
const LIBX11_COMPILED_LOCALE_DIRS: &[&str] = &[
    "/home/nathan/secure-exec/experiments/wasm-gui/third_party/wasm-prefix/share/X11/locale",
    "/home/nathan/secure-exec/experiments/wasm-gui/third_party/wasm-prefix/lib/X11/locale",
];

/// Trusted VM config: default bundled-base filesystem + allow-all permission policy (fs reads are
/// denied by default, which would block loading the wasm from /tmp). `"allow"` maps to the untagged
/// `FsPermissionScope::Mode(Allow)` etc.
// Trusted VM config. `maxWasmFuel` raises the per-WASM-execution budget (enforced as a wall-clock
// timeout in ms) far above the 30s default: the X server and a desktop's clients are LONG-RUNNING
// guests, so the default would kill the server ~30s in ("WebAssembly fuel budget exhausted") and the
// desktop would collapse. 1 hour is generous for a session while still bounding a runaway guest.
const VM_CONFIG_JSON: &str = r#"{"permissions":{"fs":"allow","network":"allow","childProcess":"allow","process":"allow","env":"allow","tool":"allow"},"limits":{"resources":{"maxWasmFuel":3600000,"maxWasmMemoryBytes":536870912}}}"#;

type Result<T> = std::result::Result<T, String>;

/// A connected secure-exec session bound to one VM, over the standard Rust client transport.
struct Session {
    t: Arc<SidecarTransport>,
    connection_id: String,
    session_id: String,
    vm_id: String,
    /// The host-backed shadow dir this VM created (for direct framebuffer readback). Identified at
    /// create time so concurrent sessions' same-named (vm-1) shadow dirs don't get mixed up.
    shadow_dir: Option<std::path::PathBuf>,
}

/// List all current sidecar VM shadow dirs under the system temp dir.
fn list_shadow_dirs() -> Vec<std::path::PathBuf> {
    let mut out = Vec::new();
    if let Ok(rd) = std::fs::read_dir(std::env::temp_dir()) {
        for e in rd.flatten() {
            if e.file_name()
                .to_string_lossy()
                .starts_with("secure-exec-sidecar-shadow-")
            {
                out.push(e.path());
            }
        }
    }
    out
}

impl Session {
    async fn connect(sidecar_bin: Option<String>) -> Result<Self> {
        let t = SidecarTransport::spawn(sidecar_bin)
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

        // Snapshot pre-existing sidecar shadow dirs so we can identify the one OUR CreateVm makes.
        // Multiple concurrent sidecar processes each assign vm ids like "vm-1", so matching on vm_id
        // alone is ambiguous; the dir that newly appears after our CreateVm is unambiguously ours.
        let pre_shadows = list_shadow_dirs();

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

        // The shadow dir that appeared after CreateVm (matching our vm_id) is ours; pick the newest
        // such new one. This is robust to other sessions' concurrently-active vm-1 shadow dirs.
        let shadow_dir = list_shadow_dirs()
            .into_iter()
            .filter(|p| !pre_shadows.contains(p))
            .filter(|p| {
                p.file_name()
                    .and_then(|n| n.to_str())
                    .map(|n| n.starts_with(&format!("secure-exec-sidecar-shadow-{vm_id}-")))
                    .unwrap_or(false)
            })
            .max_by_key(|p| std::fs::metadata(p).and_then(|m| m.modified()).ok());

        Ok(Session { t, connection_id, session_id, vm_id, shadow_dir })
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

    /// Recursively install a host directory tree into the VM filesystem (mkdir dirs, write files).
    /// Used to stage runtime data the guest libraries expect at fixed paths: libX11 locale data
    /// (XLOCALEDIR), and later fontconfig configs / theme data.
    fn install_tree<'a>(
        &'a self,
        host_root: &'a std::path::Path,
        vm_root: &'a str,
    ) -> std::pin::Pin<Box<dyn std::future::Future<Output = Result<usize>> + 'a>> {
        Box::pin(async move {
            self.mkdir(vm_root).await.ok();
            let mut count = 0usize;
            let entries = std::fs::read_dir(host_root)
                .map_err(|e| format!("read tree {}: {e}", host_root.display()))?;
            for entry in entries.flatten() {
                let name = entry.file_name();
                let name = name.to_string_lossy();
                let vm_path = format!("{vm_root}/{name}");
                let ft = entry.file_type().map_err(|e| format!("stat {name}: {e}"))?;
                if ft.is_dir() {
                    count += self.install_tree(&entry.path(), &vm_path).await?;
                } else if ft.is_file() {
                    let bytes = std::fs::read(entry.path())
                        .map_err(|e| format!("read {name}: {e}"))?;
                    self.write_file(&vm_path, &bytes).await?;
                    count += 1;
                }
            }
            Ok(count)
        })
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
    t: &Arc<SidecarTransport>,
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

// ---- host-side X11 + XTEST input ---------------------------------------------------------------
// The host connects DIRECTLY to the wasm X server's host-backed AF_UNIX socket and injects real input
// via the XTEST extension. This is the working input path: host->guest stdin/file delivery does not
// propagate live data, but the X server's listening socket is a real host unix socket the host can
// speak X11 to. Cross-platform (pure-Rust x11rb; unix sockets on macOS + Linux).
mod xinput {
    use std::os::unix::net::UnixStream;
    use std::path::Path;

    use x11rb::connection::Connection;
    use x11rb::protocol::xproto::{ConfigureWindowAux, ConnectionExt as _, Window};
    use x11rb::protocol::xtest::ConnectionExt as _;
    use x11rb::rust_connection::{DefaultStream, RustConnection};

    // XTEST event types (xproto): KeyPress=2, KeyRelease=3, ButtonPress=4, ButtonRelease=5, Motion=6.
    const KEY_PRESS: u8 = 2;
    const KEY_RELEASE: u8 = 3;
    const BUTTON_PRESS: u8 = 4;
    const BUTTON_RELEASE: u8 = 5;
    const MOTION: u8 = 6;

    pub struct XInput {
        conn: RustConnection,
        root: Window,
        last: (i16, i16),
        // While a host-side window drag is active: (frame window, cursor->frame offset x, y).
        drag: Option<(Window, i16, i16)>,
    }

    impl XInput {
        pub fn connect(socket_path: &Path) -> Result<Self, String> {
            let us = UnixStream::connect(socket_path)
                .map_err(|e| format!("connect X socket {}: {e}", socket_path.display()))?;
            let (stream, _peer_auth) =
                DefaultStream::from_unix_stream(us).map_err(|e| format!("x stream: {e}"))?;
            let conn = RustConnection::connect_to_stream(stream, 0)
                .map_err(|e| format!("X11 handshake: {e}"))?;
            let root = conn.setup().roots[0].root;
            // Confirm XTEST is present (Xvfb builds it in).
            conn.xtest_get_version(2, 2)
                .map_err(|e| format!("xtest query: {e}"))?
                .reply()
                .map_err(|e| format!("xtest version: {e}"))?;
            Ok(Self { conn, root, last: (0, 0), drag: None })
        }

        fn fake(&self, ty: u8, detail: u8, x: i16, y: i16) -> Result<(), String> {
            self.conn
                .xtest_fake_input(ty, detail, 0, self.root, x, y, 0)
                .map_err(|e| format!("xtest_fake_input: {e}"))?;
            self.conn.flush().map_err(|e| format!("x flush: {e}"))?;
            Ok(())
        }

        pub fn motion(&self, x: i16, y: i16) -> Result<(), String> {
            self.fake(MOTION, 0, x, y)
        }
        pub fn button(&self, n: u8, press: bool) -> Result<(), String> {
            self.fake(if press { BUTTON_PRESS } else { BUTTON_RELEASE }, n, 0, 0)
        }
        pub fn key(&self, code: u8, press: bool) -> Result<(), String> {
            self.fake(if press { KEY_PRESS } else { KEY_RELEASE }, code, 0, 0)
        }

        /// The top-level window (twm frame) directly under the pointer, with its root-relative origin.
        fn frame_under_pointer(&self) -> Option<(Window, i16, i16)> {
            let qp = self.conn.query_pointer(self.root).ok()?.reply().ok()?;
            let child = qp.child;
            if child == 0 || child == self.root {
                return None;
            }
            let g = self.conn.get_geometry(child).ok()?.reply().ok()?;
            Some((child, g.x, g.y))
        }

        fn move_window(&self, w: Window, x: i16, y: i16) -> Result<(), String> {
            self.conn
                .configure_window(w, &ConfigureWindowAux::new().x(x as i32).y(y as i32))
                .map_err(|e| format!("configure_window: {e}"))?;
            self.conn.flush().map_err(|e| format!("x flush: {e}"))?;
            Ok(())
        }

        /// Run one agent-vocabulary command line (motion/button/buttondn/buttonup/key). Button 1
        /// presses begin a host-side opaque window drag (the frame under the pointer follows the
        /// cursor on subsequent motions) so dragging works without relying on the WM's own move grab
        /// — XTEST FakeMotion does not warp the pointer while the WM holds a pointer grab in our Xvfb.
        /// Build a keysym -> (keycode, needs-shift) lookup from the server's keyboard mapping, so we can
        /// translate an ASCII string into XTEST keystrokes against whatever keymap the server loaded.
        fn keysym_map(&self) -> Result<std::collections::HashMap<u32, (u8, bool)>, String> {
            let setup = self.conn.setup();
            let min = setup.min_keycode;
            let max = setup.max_keycode;
            let count = max - min + 1;
            let m = self
                .conn
                .get_keyboard_mapping(min, count)
                .map_err(|e| format!("get_keyboard_mapping: {e}"))?
                .reply()
                .map_err(|e| format!("get_keyboard_mapping reply: {e}"))?;
            let per = m.keysyms_per_keycode as usize;
            let mut map = std::collections::HashMap::new();
            for kc in 0..count as usize {
                let base = kc * per;
                let keycode = min + kc as u8;
                if let Some(&ks0) = m.keysyms.get(base) {
                    if ks0 != 0 {
                        map.entry(ks0).or_insert((keycode, false));
                    }
                }
                if per > 1 {
                    if let Some(&ks1) = m.keysyms.get(base + 1) {
                        if ks1 != 0 {
                            map.entry(ks1).or_insert((keycode, true));
                        }
                    }
                }
            }
            Ok(map)
        }

        /// Type an ASCII string as XTEST keystrokes (KeyPress/KeyRelease, with Shift for shifted glyphs).
        fn type_text(&mut self, text: &str) -> Result<(), String> {
            let map = self.keysym_map()?;
            // Shift_L keysym = 0xffe1.
            let shift_kc = map.get(&0xffe1).map(|&(kc, _)| kc);
            for ch in text.chars() {
                // ASCII printables map keysym==codepoint; a couple of control chars map to named keysyms.
                let ks: u32 = match ch {
                    '\n' => 0xff0d, // Return
                    '\t' => 0xff09, // Tab
                    c if (0x20..=0x7e).contains(&(c as u32)) => c as u32,
                    _ => continue,
                };
                let Some(&(code, shift)) = map.get(&ks) else { continue };
                if shift {
                    if let Some(sk) = shift_kc {
                        self.key(sk, true)?;
                    }
                }
                self.key(code, true)?;
                self.key(code, false)?;
                if shift {
                    if let Some(sk) = shift_kc {
                        self.key(sk, false)?;
                    }
                }
                // Pace keystrokes like a human typist so the terminal's polling loop drains each one
                // (a zero-delay burst can outrun the guest's read/redraw cycle).
                std::thread::sleep(std::time::Duration::from_millis(15));
            }
            Ok(())
        }

        pub fn run(&mut self, line: &str) -> Result<(), String> {
            // `type <string>` keeps the remainder verbatim (it may contain spaces).
            if let Some(rest) = line.strip_prefix("type ") {
                return self.type_text(rest);
            }
            let parts: Vec<&str> = line.split_whitespace().collect();
            let Some(&cmd) = parts.first() else { return Ok(()) };
            let num = |i: usize| -> i32 { parts.get(i).and_then(|s| s.parse().ok()).unwrap_or(0) };
            match cmd {
                "motion" => {
                    let (x, y) = (num(1) as i16, num(2) as i16);
                    self.last = (x, y);
                    self.motion(x, y)?;
                    if let Some((w, ox, oy)) = self.drag {
                        self.move_window(w, x - ox, y - oy)?;
                    }
                    Ok(())
                }
                "button" => {
                    let (x, y) = (num(2) as i16, num(3) as i16);
                    self.last = (x, y);
                    self.motion(x, y)?;
                    self.button(num(1) as u8, true)?;
                    self.button(num(1) as u8, false)
                }
                "buttondn" => {
                    let n = num(1) as u8;
                    self.button(n, true)?;
                    if n == 1 {
                        if let Some((frame, fx, fy)) = self.frame_under_pointer() {
                            self.drag = Some((frame, self.last.0 - fx, self.last.1 - fy));
                        }
                    }
                    Ok(())
                }
                "buttonup" => {
                    let n = num(1) as u8;
                    if n == 1 {
                        self.drag = None;
                    }
                    self.button(n, false)
                }
                "key" => {
                    self.key(num(1) as u8, true)?;
                    self.key(num(1) as u8, false)
                }
                "focus" => {
                    // Assign keyboard input focus to a client window so XTEST KeyPress events reach it
                    // (needed when no WM manages focus). Prefer the window under the pointer; if that is
                    // the root, fall back to the last (most recently mapped) viewable child of root.
                    let mut target = self.frame_under_pointer().map(|(w, _, _)| w);
                    if target.is_none() {
                        if let Some(tree) = self.conn.query_tree(self.root).ok().and_then(|c| c.reply().ok()) {
                            for &child in tree.children.iter().rev() {
                                if let Some(attrs) = self.conn.get_window_attributes(child).ok().and_then(|c| c.reply().ok()) {
                                    if attrs.map_state == x11rb::protocol::xproto::MapState::VIEWABLE {
                                        target = Some(child);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if let Some(w) = target {
                        self.conn
                            .set_input_focus(
                                x11rb::protocol::xproto::InputFocus::PARENT,
                                w,
                                x11rb::CURRENT_TIME,
                            )
                            .map_err(|e| format!("set_input_focus: {e}"))?;
                    }
                    // Also enable PointerRoot focus (window 1) so KeyPress routes to whatever window is
                    // under the pointer, robust when no WM owns focus. Caller positions the pointer first.
                    self.conn
                        .set_input_focus(
                            x11rb::protocol::xproto::InputFocus::POINTER_ROOT,
                            1u32,
                            x11rb::CURRENT_TIME,
                        )
                        .map_err(|e| format!("set_input_focus(ptrroot): {e}"))?;
                    self.conn.flush().map_err(|e| format!("x flush: {e}"))?;
                    Ok(())
                }
                _ => Ok(()),
            }
        }
    }

    /// Find the X server's host-backed unix socket inside a VM shadow dir.
    pub fn server_socket(shadow_dir: &Path) -> std::path::PathBuf {
        shadow_dir.join("tmp/.X11-unix/X0")
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

async fn run_exec(
    sidecar: Option<String>,
    guest: &str,
    args: &[String],
    timeout_s: u64,
    vm_trees: &[String],
    guest_env: &[(String, String)],
) -> Result<()> {
    let s = Session::connect(sidecar).await?;
    let guest_abs = abs_path(guest)?;
    s.mkdir("/data").await.ok();
    s.mkdir("/tmp/.X11-unix").await.ok();
    // Stage host fixture trees at the VM root (e.g. freedesktop .menu + .desktop dirs for menu-cache-gen).
    for tree in vm_trees {
        let n = s.install_tree(std::path::Path::new(tree), "").await?;
        eprintln!("secure-exec: installed {n} files from {tree} into the VM root");
    }
    let mut events = s.t.subscribe_wire_events();
    let argv: Vec<&str> = args.iter().map(|x| x.as_str()).collect();
    let env: HashMap<String, String> = guest_env.iter().cloned().collect();
    s.execute_env("proc-exec", &guest_abs, &argv, env).await?;
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
                // Read back a guest output file (e.g. menu-cache-gen's generated cache) on clean exit.
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

/// XU0: D-Bus session-bus round-trip. Launch the unmodified dbus-daemon as a long-lived guest (it binds
/// the session bus AF_UNIX socket in the kernel socket table), then launch the client guests (e.g.
/// dbus-monitor then dbus-send) with DBUS_SESSION_BUS_ADDRESS so they connect to it — exactly how the X
/// clients reach the wasm X server. Streams all output so a test can assert a method-call reply +
/// signal round-trip over the bus.
async fn run_bus_roundtrip(
    sidecar: Option<String>,
    server: &str,
    clients: &[String],
    server_args: &[String],
    timeout_s: u64,
    vm_trees: &[String],
    bus_address: &str,
) -> Result<()> {
    let s = Session::connect(sidecar).await?;
    s.mkdir("/data").await.ok();
    s.mkdir("/tmp").await.ok();
    s.mkdir("/tmp/.dbus").await.ok();
    for tree in vm_trees {
        let n = s.install_tree(std::path::Path::new(tree), "").await?;
        eprintln!("secure-exec: installed {n} files from {tree} into the VM root");
    }
    let mut events = s.t.subscribe_wire_events();

    // Start the bus daemon (longest-lived; opt out of the CPU-time limit like the X server).
    let server_abs = abs_path(server)?;
    let sargv: Vec<&str> = server_args.iter().map(|x| x.as_str()).collect();
    let mut denv = HashMap::new();
    denv.insert("AGENT_OS_V8_CPU_TIME_LIMIT_MS".to_string(), "0".to_string());
    s.execute_env("dbusd", &server_abs, &sargv, denv).await?;
    eprintln!("secure-exec: started dbus-daemon {server_abs} {server_args:?}");
    // Give it time to bind + listen before any client connects.
    tokio::time::sleep(std::time::Duration::from_secs(3)).await;

    // Launch each client with the bus address, staggered (e.g. monitor first so it sees the signals).
    for (i, spec) in clients.iter().enumerate() {
        let parts: Vec<&str> = spec.split_whitespace().collect();
        if parts.is_empty() {
            continue;
        }
        let path = abs_path(parts[0])?;
        let argv: Vec<&str> = parts[1..].to_vec();
        let mut cenv = HashMap::new();
        cenv.insert(
            "DBUS_SESSION_BUS_ADDRESS".to_string(),
            bus_address.to_string(),
        );
        let id = format!("dbus-client{i}");
        s.execute_env(&id, &path, &argv, cenv).await?;
        eprintln!("secure-exec: launched {id} ({})", parts[0]);
        tokio::time::sleep(std::time::Duration::from_millis(1500)).await;
    }

    let deadline = tokio::time::Instant::now() + std::time::Duration::from_secs(timeout_s);
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        match tokio::time::timeout(remaining, events.recv()).await {
            Ok(Ok((_, wire::EventPayload::ProcessOutputEvent(o)))) => {
                let txt = String::from_utf8_lossy(&o.chunk);
                let ch = if matches!(o.channel, wire::StreamChannel::Stderr) {
                    "err"
                } else {
                    "out"
                };
                eprint!("[{}/{ch}] {txt}", o.process_id);
            }
            Ok(Ok((_, wire::EventPayload::ProcessExitedEvent(e)))) => {
                eprintln!("secure-exec: {} exited with code {}", e.process_id, e.exit_code);
            }
            Ok(Ok(_)) => continue,
            Ok(Err(tokio::sync::broadcast::error::RecvError::Lagged(_))) => continue,
            Ok(Err(_)) => break,
            Err(_) => break,
        }
    }
    eprintln!("\nsecure-exec: bus-test timeout ({timeout_s}s) reached");
    s.shutdown();
    Ok(())
}

/// M6.3 PTY end-to-end test: install the "shell" wasm into the VM fs, then run the "terminal" wasm,
/// which spawns the shell over a kernel PTY (host_net.pty_spawn) and echoes data through it. Streams
/// the terminal's output so a test can assert it read the shell's reply back from the PTY master.
async fn run_pty_test(
    sidecar: Option<String>,
    term: &str,
    shell: &str,
    timeout_s: u64,
) -> Result<()> {
    let s = Session::connect(sidecar).await?;
    let term_abs = abs_path(term)?;
    s.mkdir("/data").await.ok();
    let shell_bytes = std::fs::read(shell).map_err(|e| format!("read {shell}: {e}"))?;
    s.write_file("/pty-shell.wasm", &shell_bytes).await?;
    eprintln!(
        "secure-exec: installed /pty-shell.wasm ({} bytes)",
        shell_bytes.len()
    );
    let mut events = s.t.subscribe_wire_events();
    s.execute("proc-pty", &term_abs, &[]).await?;
    eprintln!("secure-exec: started pty-term {term_abs}");
    let deadline = tokio::time::Instant::now() + std::time::Duration::from_secs(timeout_s);
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        match tokio::time::timeout(remaining, events.recv()).await {
            Ok(Ok((_, wire::EventPayload::ProcessOutputEvent(o)))) if o.process_id == "proc-pty" => {
                let txt = String::from_utf8_lossy(&o.chunk);
                let ch = if matches!(o.channel, wire::StreamChannel::Stderr) {
                    "err"
                } else {
                    "out"
                };
                eprint!("[{ch}] {txt}");
            }
            Ok(Ok((_, wire::EventPayload::ProcessExitedEvent(e)))) if e.process_id == "proc-pty" => {
                eprintln!("\nsecure-exec: pty-term exited with code {}", e.exit_code);
                s.shutdown();
                return Ok(());
            }
            Ok(Ok(_)) => continue,
            Ok(Err(tokio::sync::broadcast::error::RecvError::Lagged(_))) => continue,
            Ok(Err(_)) => break,
            Err(_) => break,
        }
    }
    eprintln!("\nsecure-exec: pty-test timeout ({timeout_s}s) reached");
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
    locale_dir: Option<&str>,
    vm_trees: &[String],
    inject: &[(String, String)],
    pty_shell: Option<&str>,
    concurrent_launch: bool,
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
    // Pre-create the input-command file BEFORE any client launches. Guests can read /data files that
    // existed when they started (e.g. the X server's Xvfb_screen0) but not ones the host write_file's
    // afterwards, so the XTEST agent's `follow` mode only sees /data/input-cmds if it predates it.
    s.write_file("/data/input-cmds", b"").await.ok();
    // Provide a twm config that auto-places windows (twm's default placement is interactive and
    // would never map a window without user input). Harmless for non-twm runs.
    s.mkdir("/root").await.ok();
    s.write_file(
        "/root/.twmrc",
        b"RandomPlacement\nUsePPosition \"on\"\nNoGrabServer\nOpaqueMove\nNoTitleFocus\nButton1 = : title : f.move\nButton1 = : frame : f.move\n",
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

    // Install libX11 locale data into the VM so XCreateFontSet works (Xt apps like xclock/xterm fail
    // their widget realize without a usable fontset). Our libX11 is built without XLOCALEDIR env
    // support (wasi lacks getresuid/issetugid, so configure leaves it disabled), so it reads ONLY the
    // path compiled into the library. We install the data at that exact compiled path inside the VM
    // (plus /locale for builds that do honor XLOCALEDIR).
    if let Some(ldir) = locale_dir {
        let n = s.install_tree(std::path::Path::new(ldir), "/locale").await?;
        for compiled in LIBX11_COMPILED_LOCALE_DIRS {
            s.install_tree(std::path::Path::new(ldir), compiled).await?;
        }
        eprintln!("secure-exec: installed {n} locale files into /locale (+ compiled libX11 paths)");
    }

    // Install arbitrary host trees at the VM root (e.g. /etc/fonts + /usr/share/fonts for Xft, or
    // theme/config data later). Each tree mirrors the in-VM layout it should land at.
    for tree in vm_trees {
        let n = s.install_tree(std::path::Path::new(tree), "").await?;
        eprintln!("secure-exec: installed {n} files from {tree} into the VM root");
    }

    // Install the PTY child shell so a terminal-emulator client (st) can pty_spawn("/pty-shell.wasm").
    if let Some(shell) = pty_shell {
        let shell_abs = abs_path(shell)?;
        let bytes = std::fs::read(&shell_abs).map_err(|e| format!("read pty-shell {shell_abs}: {e}"))?;
        s.write_file("/pty-shell.wasm", &bytes).await?;
        eprintln!("secure-exec: installed /pty-shell.wasm ({} bytes)", bytes.len());
    }

    let mut events = s.t.subscribe_wire_events();

    // Start the X server. It binds /tmp/.X11-unix/X0 and blocks in its dispatch loop.
    let sargv: Vec<&str> = server_args.iter().map(|x| x.as_str()).collect();
    // The X server is the longest-lived guest of all (it outlives every client). The 30s CPU-time
    // default kills it mid-session, collapsing the whole desktop. Trusted long-lived guest -> opt out.
    let mut srv_env = HashMap::new();
    srv_env.insert("AGENT_OS_V8_CPU_TIME_LIMIT_MS".to_string(), "0".to_string());
    s.execute_env("xserver", &server_abs, &sargv, srv_env).await?;
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
    // Default raised from 1.5s: a heavy GTK app (lxpanel/pcmanfm) initializes SLOWLY under
    // concurrent-guest contention and emits sporadic output with multi-second gaps, so a short
    // settle window is fooled into launching the next app mid-init (then all of them contend and
    // none renders). Require a long sustained quiet so the previous app has truly finished and gone
    // idle in its event loop before the next launches. Env-tunable (APP_SETTLE_MS).
    let settle = std::time::Duration::from_millis(
        std::env::var("APP_SETTLE_MS").ok().and_then(|v| v.parse().ok()).unwrap_or(9000),
    );
    let min_after_launch = std::time::Duration::from_millis(800);
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        // Launch the next client once the previous one is ready. The first client is the window
        // manager: gate apps on it being fully up (its "WM ready" signal, or a generous fallback),
        // not just a quiet pause — twm has quiet stretches mid-init while awaiting X replies.
        if server_ready && launched == 1 && !wm_ready {
            // A heavyweight WM (openbox) inits slowly under concurrent-guest contention: libxml2
            // rc.xml/theme parsing plus hundreds of X round-trips. If we launch the app at a fixed
            // mid-init moment, the app's own heavy init contends with the WM's, neither settles, and
            // the WM never selects SubstructureRedirect in time to manage the app's window. So gate
            // the app on the WM having SETTLED — run a bit AND gone quiet (idle in its event loop,
            // redirect active) — with a generous hard cap as a safety net. (openbox prints no
            // "handleevents"-style ready marker, so detect readiness by quiescence.)
            // The WM's init output is itself starved/slow under contention, so a short quiet window
            // can be fooled by the gaps between its progress messages. Require a long sustained quiet
            // (it has truly stopped initializing and is idle in its event loop). Env-tunable.
            let quiet_ms: u64 = std::env::var("WM_SETTLE_QUIET_MS")
                .ok().and_then(|v| v.parse().ok()).unwrap_or(9000);
            let cap_s: u64 = std::env::var("WM_SETTLE_CAP_S")
                .ok().and_then(|v| v.parse().ok()).unwrap_or(120);
            let now = tokio::time::Instant::now();
            let ran = now.duration_since(last_launch);
            let quiet = now.duration_since(last_activity);
            if (ran >= std::time::Duration::from_secs(4)
                && quiet >= std::time::Duration::from_millis(quiet_ms))
                || ran >= std::time::Duration::from_secs(cap_s)
            {
                wm_ready = true;
            }
        }
        // M2.3 concurrency mode: once the server is up, launch ALL clients back-to-back without the
        // settle/WM gating, to exercise concurrent libX11 init over the sync-RPC bridge.
        if concurrent_launch && server_ready && launched < client_specs.len() {
            while launched < client_specs.len() {
                let (path, cargs) = &client_specs[launched];
                let id = format!("xclient{launched}");
                let argv: Vec<&str> = cargs.iter().map(|x| x.as_str()).collect();
                let mut cenv = HashMap::new();
                // Desktop guests (X server, WM, GTK apps) are TRUSTED and LONG-LIVED — unlike a short
                // adapter script they legitimately accumulate CPU over a multi-minute session. The 30s
                // CPU-time default (AGENT_OS_V8_CPU_TIME_LIMIT_MS) kills the X server mid-session (~70s
                // wall), tearing down every client's display -> black screen. 0 = the explicit trusted
                // opt-out (wall-clock backstops still apply). Trust-model: the host configures its own VMs.
                cenv.insert("AGENT_OS_V8_CPU_TIME_LIMIT_MS".to_string(), "0".to_string());
                cenv.insert("DISPLAY".to_string(), ":0".to_string());
                cenv.insert("HOME".to_string(), "/root".to_string());
                // GIO cannot dlopen its native volume-monitor module on wasm; the union monitor's
                // native init (g_volume_monitor_get) then deadlocks the main thread inside pcmanfm's
                // places side pane. The sandbox has no removable drives, so use the null monitor.
                cenv.insert("GIO_USE_VOLUME_MONITOR".to_string(), "null".to_string());
                // Force GDK's CORE device manager (Virtual core pointer/keyboard) instead of XI2.
                // Xvfb.wasm advertises XInputExtension but its XI2 device enumeration yields broken
                // master/slave associations, so GDK's XI2 manager builds NULL GdkDevices. Standalone
                // those are only non-fatal "GDK_IS_DEVICE" criticals, but once a WM (openbox) sends
                // the window focus/crossing events, GDK dereferences the NULL device -> wasm trap
                // (memory access out of bounds). The sandbox has a single synthetic XTEST seat, so the
                // core pointer/keyboard model is the honest one. (Standard GDK knob; constraint #5
                // runtime config, like GIO_USE_VOLUME_MONITOR above.)
                cenv.insert("GDK_CORE_DEVICE_EVENTS".to_string(), "1".to_string());
                // Forward SE_DIAG_* diagnostic toggles from the host env to the guest (temporary).
                for (k, v) in std::env::vars() {
                    if k.starts_with("SE_DIAG_") {
                        cenv.insert(k, v);
                    }
                }
                if locale_dir.is_some() {
                    cenv.insert("XLOCALEDIR".to_string(), "/locale".to_string());
                }
                if !vm_trees.is_empty() {
                    cenv.insert("FONTCONFIG_PATH".to_string(), "/etc/fonts".to_string());
                    cenv.insert("FONTCONFIG_FILE".to_string(), "/etc/fonts/fonts.conf".to_string());
                }
                s.execute_env(&id, path, &argv, cenv).await?;
                eprintln!("secure-exec: launched {id} ({path}) [concurrent]");
                launched += 1;
            }
        }
        let can_launch_next = launched == 0 || wm_ready;
        if !concurrent_launch && server_ready && launched < client_specs.len() && can_launch_next {
            let now = tokio::time::Instant::now();
            let prev_settled = launched == 0
                || (now.duration_since(last_activity) >= settle
                    && now.duration_since(last_launch) >= min_after_launch);
            if prev_settled {
                let (path, cargs) = &client_specs[launched];
                let id = format!("xclient{launched}");
                let argv: Vec<&str> = cargs.iter().map(|x| x.as_str()).collect();
                let mut cenv = HashMap::new();
                // Desktop guests (X server, WM, GTK apps) are TRUSTED and LONG-LIVED — unlike a short
                // adapter script they legitimately accumulate CPU over a multi-minute session. The 30s
                // CPU-time default (AGENT_OS_V8_CPU_TIME_LIMIT_MS) kills the X server mid-session (~70s
                // wall), tearing down every client's display -> black screen. 0 = the explicit trusted
                // opt-out (wall-clock backstops still apply). Trust-model: the host configures its own VMs.
                cenv.insert("AGENT_OS_V8_CPU_TIME_LIMIT_MS".to_string(), "0".to_string());
                cenv.insert("DISPLAY".to_string(), ":0".to_string());
                cenv.insert("HOME".to_string(), "/root".to_string());
                // GIO cannot dlopen its native volume-monitor module on wasm; the union monitor's
                // native init (g_volume_monitor_get) then deadlocks the main thread inside pcmanfm's
                // places side pane. The sandbox has no removable drives, so use the null monitor.
                cenv.insert("GIO_USE_VOLUME_MONITOR".to_string(), "null".to_string());
                // Force GDK's CORE device manager (Virtual core pointer/keyboard) instead of XI2.
                // Xvfb.wasm advertises XInputExtension but its XI2 device enumeration yields broken
                // master/slave associations, so GDK's XI2 manager builds NULL GdkDevices. Standalone
                // those are only non-fatal "GDK_IS_DEVICE" criticals, but once a WM (openbox) sends
                // the window focus/crossing events, GDK dereferences the NULL device -> wasm trap
                // (memory access out of bounds). The sandbox has a single synthetic XTEST seat, so the
                // core pointer/keyboard model is the honest one. (Standard GDK knob; constraint #5
                // runtime config, like GIO_USE_VOLUME_MONITOR above.)
                cenv.insert("GDK_CORE_DEVICE_EVENTS".to_string(), "1".to_string());
                // Forward SE_DIAG_* diagnostic toggles from the host env to the guest (temporary).
                for (k, v) in std::env::vars() {
                    if k.starts_with("SE_DIAG_") {
                        cenv.insert(k, v);
                    }
                }
                if locale_dir.is_some() {
                    cenv.insert("XLOCALEDIR".to_string(), "/locale".to_string());
                }
                // Point fontconfig at the in-VM config we may have staged via --vm-tree, so Xft
                // clients resolve fontconfig patterns to the installed TTFs.
                if !vm_trees.is_empty() {
                    cenv.insert("FONTCONFIG_PATH".to_string(), "/etc/fonts".to_string());
                    cenv.insert("FONTCONFIG_FILE".to_string(), "/etc/fonts/fonts.conf".to_string());
                    if let Ok(dbg) = std::env::var("WASMGUI_FC_DEBUG") {
                        cenv.insert("FC_DEBUG".to_string(), dbg);
                    }
                }
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
                // Stamp a relative-ms timestamp so guest "BC:" breadcrumbs (fprintf checkpoints) line up
                // on the same timeline as the sidecar rpc-watchdog dumps (both go to this log).
                static DEMO_T0: std::sync::OnceLock<std::time::Instant> = std::sync::OnceLock::new();
                let t = DEMO_T0.get_or_init(std::time::Instant::now).elapsed().as_millis();
                eprint!("[+{t:>7}ms {who}/{ch}] {txt}");
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
                // Only count the MAIN client process, never its wasi worker threads. Worker threads
                // are named "<client>~thread~<id>" (which still starts_with "xclient"), so a glib/GIO
                // pool thread exiting (e.g. pcmanfm's folder/icon loader) must not be mistaken for the
                // client completing — doing so tears down the VM before the main thread renders.
                if e.process_id.starts_with("xclient") && !e.process_id.contains("~thread~") {
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

    // Host-driven input injection (SPEC M6.1): the host connects DIRECTLY to the X server's
    // host-backed AF_UNIX socket and injects via XTEST. This is the working path (host->guest stdin /
    // file delivery does not propagate live data). The `pid` of each --inject entry is ignored.
    if !inject.is_empty() {
        match s.shadow_dir.as_ref() {
            Some(dir) => {
                let sock = xinput::server_socket(dir);
                match xinput::XInput::connect(&sock) {
                    Ok(mut xi) => {
                        // Let clients finish mapping/realizing their windows before the first inject so
                        // focus targets a real window and early keystrokes are not dropped. Tunable via
                        // INJECT_DELAY_MS for slow desktops (e.g. the LXDE session, where pcmanfm's libfm
                        // directory load only finishes ~140s in under 3-client contention).
                        let inject_delay = std::env::var("INJECT_DELAY_MS")
                            .ok().and_then(|v| v.parse().ok()).unwrap_or(800);
                        tokio::time::sleep(std::time::Duration::from_millis(inject_delay)).await;
                        for (_, cmd) in inject {
                            if let Err(e) = xi.run(cmd) {
                                eprintln!("secure-exec: XTEST inject '{cmd}' failed: {e}");
                            } else {
                                eprintln!("secure-exec: XTEST injected: {cmd}");
                            }
                            // Pace events so the WM/clients process each one (a real drag needs the
                            // buttondn -> motion -> buttonup sequence spaced out, not instantaneous).
                            tokio::time::sleep(std::time::Duration::from_millis(120)).await;
                        }
                        tokio::time::sleep(std::time::Duration::from_millis(800)).await;
                    }
                    Err(e) => eprintln!("secure-exec: XTEST connect failed: {e}"),
                }
            }
            None => eprintln!("secure-exec: no shadow dir; cannot connect for XTEST inject"),
        }
    }

    // Read the server framebuffer directly from the host-backed VM shadow filesystem.
    //
    // /data is shadowed by the sidecar to <tmp>/secure-exec-sidecar-shadow-<vm_id>-<nonce>/data,
    // and Xvfb writes its framebuffer there continuously. We read that host path directly instead
    // of issuing a kernel-VFS Pread over the wire: while the X server and its clients are alive they
    // keep the single sidecar service thread busy, so a wire readback gets starved and never returns.
    // A direct host-fs read needs no wire round-trip and so cannot be starved by the live guests.
    if let Some(hpath) = fb_out {
        // Prefer the exact shadow dir this VM created (robust under concurrent same-id sessions);
        // fall back to the vm_id glob if it wasn't captured.
        let read = s
            .shadow_dir
            .as_ref()
            .and_then(|d| {
                let p = d.join("data/Xvfb_screen0");
                std::fs::read(&p).ok().map(|b| (p, b))
            })
            .or_else(|| read_shadow_framebuffer(&s.vm_id, "data/Xvfb_screen0"));
        match read {
            Some((src, bytes)) => {
                let _ = std::fs::write(hpath, &bytes);
                eprintln!(
                    "secure-exec: read back framebuffer ({} bytes) from {} -> {hpath}",
                    bytes.len(),
                    src.display()
                );
            }
            None => eprintln!("secure-exec: framebuffer readback failed: no shadow Xvfb_screen0 for vm {}", s.vm_id),
        }
    }
    s.shutdown();
    Ok(())
}

/// Locate and read a host-backed VM shadow file (e.g. the X framebuffer) directly from disk.
/// The sidecar names each VM's shadow root `secure-exec-sidecar-shadow-<vm_id>-<nonce>` under the
/// system temp dir; we match on `<vm_id>` and pick the newest matching root.
fn read_shadow_framebuffer(vm_id: &str, rel: &str) -> Option<(std::path::PathBuf, Vec<u8>)> {
    let prefix = format!("secure-exec-sidecar-shadow-{vm_id}-");
    let tmp = std::env::temp_dir();
    let mut best: Option<(std::time::SystemTime, std::path::PathBuf)> = None;
    for entry in std::fs::read_dir(&tmp).ok()?.flatten() {
        let name = entry.file_name();
        let name = name.to_string_lossy();
        if !name.starts_with(&prefix) {
            continue;
        }
        let candidate = entry.path().join(rel);
        if let Ok(meta) = std::fs::metadata(&candidate) {
            let mtime = meta.modified().unwrap_or(std::time::UNIX_EPOCH);
            if best.as_ref().map(|(t, _)| mtime > *t).unwrap_or(true) {
                best = Some((mtime, candidate));
            }
        }
    }
    let (_, path) = best?;
    let bytes = std::fs::read(&path).ok()?;
    Some((path, bytes))
}

// ---- arg parsing + entrypoint ------------------------------------------------------------------

struct Args {
    mode_window: bool,
    mode_desktop: bool,
    capture_out: Option<String>,
    guest: String,
    sidecar: Option<String>,
    exec: bool,
    exec_args: Vec<String>,
    timeout: u64,
    xdemo: bool,
    bus_test: bool,
    server: Option<String>,
    clients: Vec<String>,
    fb_out: Option<String>,
    fonts_dir: Option<String>,
    locale_dir: Option<String>,
    vm_trees: Vec<String>,
    inject: Vec<(String, String)>,
    guest_env: Vec<(String, String)>,
    pty_test: bool,
    concurrent: bool,
    pty_shell: Option<String>,
}

fn parse_args() -> Args {
    let argv: Vec<String> = std::env::args().collect();
    let mut a = Args {
        mode_window: false,
        mode_desktop: false,
        capture_out: None,
        guest: "target/wasm32-wasip1/release/guest.wasm".into(),
        sidecar: std::env::var("SECURE_EXEC_SIDECAR_BIN").ok(),
        exec: false,
        exec_args: Vec::new(),
        timeout: 8,
        xdemo: false,
        bus_test: false,
        server: None,
        clients: Vec::new(),
        fb_out: None,
        fonts_dir: None,
        locale_dir: None,
        vm_trees: Vec::new(),
        inject: Vec::new(),
        guest_env: Vec::new(),
        pty_test: false,
        concurrent: false,
        pty_shell: None,
    };
    let mut i = 1;
    while i < argv.len() {
        match argv[i].as_str() {
            "--window" => a.mode_window = true,
            "--desktop" => a.mode_desktop = true,
            "--exec" => a.exec = true,
            "--xdemo" => a.xdemo = true,
            "--bus-test" => a.bus_test = true,
            "--concurrent" => a.concurrent = true,
            "--pty-test" => a.pty_test = true,
            "--pty-shell" => {
                i += 1;
                a.pty_shell = argv.get(i).cloned();
            }
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
            "--locale-dir" => {
                i += 1;
                a.locale_dir = argv.get(i).cloned();
            }
            // --vm-tree <hostdir>: install that host directory tree at the VM root (repeatable).
            "--vm-tree" => {
                i += 1;
                if let Some(d) = argv.get(i) {
                    a.vm_trees.push(d.clone());
                }
            }
            // --inject "<process_id>=<command>"  (e.g. --inject "xclient2=key 38")
            // After the desktop is up, the host writes "<command>\n" to that client's stdin.
            "--inject" => {
                i += 1;
                if let Some(spec) = argv.get(i) {
                    if let Some((pid, cmd)) = spec.split_once('=') {
                        a.inject.push((pid.to_string(), cmd.to_string()));
                    }
                }
            }
            // --guest-env KEY=VAL: set an env var in the --exec guest's environment (repeatable).
            "--guest-env" => {
                i += 1;
                if let Some(spec) = argv.get(i) {
                    if let Some((k, v)) = spec.split_once('=') {
                        a.guest_env.push((k.to_string(), v.to_string()));
                    }
                }
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

    if args.mode_desktop {
        let server = args.server.clone().unwrap_or_else(|| {
            eprintln!("--desktop requires --server <Xvfb.wasm>");
            std::process::exit(2);
        });
        #[cfg(feature = "window")]
        {
            if let Err(e) = window::run_desktop(
                args.sidecar.clone(),
                server,
                args.clients.clone(),
                args.exec_args.clone(),
                args.fonts_dir.clone(),
                args.locale_dir.clone(),
                args.vm_trees.clone(),
                640,
                480,
            )
            .await
            {
                eprintln!("desktop failed: {e}");
                std::process::exit(1);
            }
            return;
        }
        #[cfg(not(feature = "window"))]
        {
            let _ = server;
            eprintln!(
                "built without the `window` feature.\n\
                 Build the interactive desktop with:  cargo run -p wasm-gui-host --features window -- --desktop ..."
            );
            std::process::exit(0);
        }
    }

    if args.bus_test {
        let server = args.server.clone().unwrap_or_else(|| {
            eprintln!("--bus-test requires --server <dbus-daemon.wasm>");
            std::process::exit(2);
        });
        let bus_addr = std::env::var("DBUS_BUS_ADDRESS")
            .unwrap_or_else(|_| "unix:path=/tmp/.dbus/session".to_string());
        if let Err(e) = run_bus_roundtrip(
            args.sidecar.clone(),
            &server,
            &args.clients,
            &args.exec_args,
            args.timeout,
            &args.vm_trees,
            &bus_addr,
        )
        .await
        {
            eprintln!("bus-test failed: {e}");
            std::process::exit(1);
        }
        return;
    }

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
            args.locale_dir.as_deref(),
            &args.vm_trees,
            &args.inject,
            args.pty_shell.as_deref(),
            args.concurrent,
        )
        .await
        {
            eprintln!("xdemo failed: {e}");
            std::process::exit(1);
        }
        return;
    }

    if args.pty_test {
        let shell = args.pty_shell.clone().unwrap_or_else(|| {
            eprintln!("--pty-test requires --pty-shell <pty-shell.wasm> (and --guest <pty-term.wasm>)");
            std::process::exit(2);
        });
        if let Err(e) =
            run_pty_test(args.sidecar.clone(), &args.guest, &shell, args.timeout).await
        {
            eprintln!("pty-test failed: {e}");
            std::process::exit(1);
        }
        return;
    }

    if args.exec {
        if let Err(e) = run_exec(
            args.sidecar.clone(),
            &args.guest,
            &args.exec_args,
            args.timeout,
            &args.vm_trees,
            &args.guest_env,
        )
        .await
        {
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
            cursor: (0, 0),
            _session: s,
        };
        event_loop
            .run_app(&mut app)
            .map_err(|e| format!("run_app: {e}"))
    }

    /// Read the X server's framebuffer (BGRX, with a small leading header) from the host-backed VM
    /// shadow file and convert it to an RGBA Frame the winit blit understands. Returns None until the
    /// server has produced a full screen's worth of pixels.
    fn read_x_frame(path: &std::path::Path, w: u32, h: u32) -> Option<Frame> {
        let bytes = std::fs::read(path).ok()?;
        let need = (w as usize) * (h as usize) * 4;
        if bytes.len() < need {
            return None;
        }
        let pix = &bytes[bytes.len() - need..]; // skip the leading header; pixels are the tail
        let mut rgba = vec![0u8; need];
        for i in (0..need).step_by(4) {
            // X dumps BGRX; the blit reads rgba[s], [s+1], [s+2] as R,G,B.
            rgba[i] = pix[i + 2];
            rgba[i + 1] = pix[i + 1];
            rgba[i + 2] = pix[i];
            rgba[i + 3] = 0xff;
        }
        Some(Frame { w, h, rgba })
    }

    /// Interactive desktop window: runs the X server + window manager + apps in one VM, streams the
    /// live framebuffer into a native winit window, and forwards mouse/keyboard back through the XTEST
    /// `follow` agent (via the /data/input-cmds file the agent tails). Cross-platform (winit +
    /// softbuffer build on macOS and Linux); the .wasm guests are platform-independent.
    #[allow(clippy::too_many_arguments)]
    pub async fn run_desktop(
        sidecar: Option<String>,
        server: String,
        clients: Vec<String>,
        server_args: Vec<String>,
        fonts_dir: Option<String>,
        locale_dir: Option<String>,
        vm_trees: Vec<String>,
        width: u32,
        height: u32,
    ) -> Result<()> {
        let s = Session::connect(sidecar).await?;
        // --- VM setup (mirrors the headless xdemo path) ---
        s.mkdir("/data").await.ok();
        s.mkdir("/tmp/.X11-unix").await.ok();
        s.write_file("/data/input-cmds", b"").await.ok(); // must predate the agent (see findings)
        s.mkdir("/root").await.ok();
        s.write_file(
            "/root/.twmrc",
            b"RandomPlacement\nUsePPosition \"on\"\nNoGrabServer\nOpaqueMove\nNoTitleFocus\nButton1 = : title : f.move\nButton1 = : frame : f.move\n",
        )
        .await
        .ok();
        if let Some(fdir) = fonts_dir.as_deref() {
            s.mkdir("/fonts").await.ok();
            if let Ok(entries) = std::fs::read_dir(fdir) {
                for entry in entries.flatten() {
                    if entry.path().is_file() {
                        if let Ok(bytes) = std::fs::read(entry.path()) {
                            let name = entry.file_name();
                            let _ = s
                                .write_file(&format!("/fonts/{}", name.to_string_lossy()), &bytes)
                                .await;
                        }
                    }
                }
            }
        }
        if let Some(ldir) = locale_dir.as_deref() {
            let _ = s.install_tree(std::path::Path::new(ldir), "/locale").await;
            for compiled in LIBX11_COMPILED_LOCALE_DIRS {
                let _ = s.install_tree(std::path::Path::new(ldir), compiled).await;
            }
        }
        for tree in &vm_trees {
            let _ = s.install_tree(std::path::Path::new(tree), "").await;
        }

        let shadow_fb = s
            .shadow_dir
            .clone()
            .map(|d| d.join("data/Xvfb_screen0"))
            .ok_or_else(|| "no shadow dir captured for framebuffer streaming".to_string())?;

        let mut events = s.t.subscribe_wire_events();
        let server_abs = abs_path(&server)?;
        let srv_argv: Vec<&str> = server_args.iter().map(|x| x.as_str()).collect();
        let mut srv_env = HashMap::new();
        srv_env.insert("AGENT_OS_V8_CPU_TIME_LIMIT_MS".to_string(), "0".to_string());
        s.execute_env("xserver", &server_abs, &srv_argv, srv_env).await?;

        let s = Arc::new(s);

        // --- background launcher: wait for the server, then launch clients sequentially ---
        let (input_tx, mut input_rx) = tokio::sync::mpsc::unbounded_channel::<String>();
        let s_launch = s.clone();
        let clients_owned = clients.clone();
        tokio::spawn(async move {
            let mut server_ready = false;
            let mut wm_ready = false;
            let mut launched = 0usize;
            let mut last_launch = tokio::time::Instant::now();
            loop {
                let timed = tokio::time::timeout(std::time::Duration::from_millis(300), events.recv());
                match timed.await {
                    Ok(Ok((_, wire::EventPayload::ProcessOutputEvent(o)))) => {
                        let txt = String::from_utf8_lossy(&o.chunk);
                        if !server_ready && o.process_id == "xserver" && txt.contains("m_pre_dispatch") {
                            server_ready = true;
                        }
                        if !wm_ready && o.process_id == "xclient0" && txt.contains("handleevents") {
                            wm_ready = true;
                        }
                    }
                    Ok(Ok(_)) | Err(_) => {}
                    Ok(Err(_)) => break,
                }
                // Gate first app on the WM; fall back after 12s like the headless path.
                if server_ready && launched > 0 && !wm_ready
                    && last_launch.elapsed() >= std::time::Duration::from_secs(12)
                {
                    wm_ready = true;
                }
                let can = launched == 0 || wm_ready;
                if server_ready && launched < clients_owned.len() && can
                    && last_launch.elapsed() >= std::time::Duration::from_millis(1500)
                {
                    let spec = &clients_owned[launched];
                    let mut parts = spec.split_whitespace();
                    if let Some(path) = parts.next() {
                        if let Ok(path_abs) = abs_path(path) {
                            let cargs: Vec<String> = parts.map(|x| x.to_string()).collect();
                            let argv: Vec<&str> = cargs.iter().map(|x| x.as_str()).collect();
                            let mut cenv = HashMap::new();
                            // Trusted long-lived desktop guest -> opt out of the 30s CPU-time default
                            // (see the X server launch); else a multi-minute session is killed mid-run.
                            cenv.insert("AGENT_OS_V8_CPU_TIME_LIMIT_MS".to_string(), "0".to_string());
                            cenv.insert("DISPLAY".to_string(), ":0".to_string());
                            cenv.insert("HOME".to_string(), "/root".to_string());
                            cenv.insert("XLOCALEDIR".to_string(), "/locale".to_string());
                            let id = format!("xclient{launched}");
                            let _ = s_launch.execute_env(&id, &path_abs, &argv, cenv).await;
                            eprintln!("secure-exec: launched {id} ({path})");
                            launched += 1;
                            last_launch = tokio::time::Instant::now();
                        }
                    }
                }
            }
        });

        // --- input forwarder: host speaks X11+XTEST directly to the server's host-backed socket ---
        // A blocking thread owns the (sync) x11rb connection; a tokio task bridges winit tokens to it.
        let x_socket = s
            .shadow_dir
            .clone()
            .map(|d| crate::xinput::server_socket(&d))
            .ok_or_else(|| "no shadow dir for X11 input socket".to_string())?;
        let (xtx, xrx) = std_mpsc::channel::<String>();
        tokio::spawn(async move {
            while let Some(line) = input_rx.recv().await {
                if xtx.send(line).is_err() {
                    break;
                }
            }
        });
        std::thread::spawn(move || {
            // The server may not be accepting yet; retry the connect for a few seconds.
            let mut xi = None;
            for _ in 0..200 {
                match crate::xinput::XInput::connect(&x_socket) {
                    Ok(c) => {
                        eprintln!("secure-exec: XTEST input connected to {}", x_socket.display());
                        xi = Some(c);
                        break;
                    }
                    Err(_) => std::thread::sleep(std::time::Duration::from_millis(100)),
                }
            }
            let Some(mut xi) = xi else {
                eprintln!("secure-exec: could not connect XTEST input to {}", x_socket.display());
                return;
            };
            while let Ok(tok) = xrx.recv() {
                let _ = xi.run(&tok);
            }
        });

        // --- framebuffer streamer: read the shadow file ~30fps and push frames to the window ---
        let (frame_tx, frame_rx) = std_mpsc::channel::<Frame>();
        std::thread::spawn(move || loop {
            if let Some(frame) = read_x_frame(&shadow_fb, width, height) {
                if frame_tx.send(frame).is_err() {
                    return;
                }
            }
            std::thread::sleep(std::time::Duration::from_millis(33));
        });

        // --- winit takes the main thread ---
        let event_loop = EventLoop::new().map_err(|e| format!("event loop: {e}"))?;
        let mut app = App {
            frame_rx,
            input_tx,
            window: None,
            surface: None,
            last: None,
            cursor: (0, 0),
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
        cursor: (i32, i32),
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
                WindowEvent::CloseRequested => event_loop.exit(),
                // Emit the XTEST agent's command vocabulary (see xtest-agent.c `follow` mode):
                // pointer motion, button down/up (so window drags work), and key press by keycode.
                WindowEvent::CursorMoved { position, .. } => {
                    self.cursor = (position.x as i32, position.y as i32);
                    let _ = self
                        .input_tx
                        .send(format!("motion {} {}", self.cursor.0, self.cursor.1));
                }
                WindowEvent::MouseInput { state, button, .. } => {
                    let n = match button {
                        winit::event::MouseButton::Left => 1,
                        winit::event::MouseButton::Middle => 2,
                        winit::event::MouseButton::Right => 3,
                        _ => 1,
                    };
                    // Move the pointer to the current spot first so the press lands where expected.
                    let _ = self
                        .input_tx
                        .send(format!("motion {} {}", self.cursor.0, self.cursor.1));
                    let verb = if state == ElementState::Pressed { "buttondn" } else { "buttonup" };
                    let _ = self.input_tx.send(format!("{verb} {n}"));
                }
                WindowEvent::KeyboardInput { event, .. } => {
                    if event.state == ElementState::Pressed {
                        if let Key::Named(NamedKey::Escape) = event.logical_key {
                            event_loop.exit();
                            return;
                        }
                        // X keycode = evdev/winit scancode + 8 (the standard Xorg offset).
                        if let winit::keyboard::PhysicalKey::Code(_) = event.physical_key {
                            let scancode = winit_scancode(&event);
                            if let Some(sc) = scancode {
                                let _ = self.input_tx.send(format!("key {}", sc + 8));
                            }
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

    /// Best-effort winit -> evdev scancode for the X keycode mapping (keycode = scancode + 8). winit
    /// 0.30 doesn't expose the raw scancode portably, so map the common KeyCode variants we care about.
    fn winit_scancode(event: &winit::event::KeyEvent) -> Option<u32> {
        use winit::keyboard::{KeyCode, PhysicalKey};
        let code = match event.physical_key {
            PhysicalKey::Code(c) => c,
            _ => return None,
        };
        // evdev scancodes (linux input-event-codes.h); X keycode adds 8.
        Some(match code {
            KeyCode::KeyA => 30, KeyCode::KeyB => 48, KeyCode::KeyC => 46, KeyCode::KeyD => 32,
            KeyCode::KeyE => 18, KeyCode::KeyF => 33, KeyCode::KeyG => 34, KeyCode::KeyH => 35,
            KeyCode::KeyI => 23, KeyCode::KeyJ => 36, KeyCode::KeyK => 37, KeyCode::KeyL => 38,
            KeyCode::KeyM => 50, KeyCode::KeyN => 49, KeyCode::KeyO => 24, KeyCode::KeyP => 25,
            KeyCode::KeyQ => 16, KeyCode::KeyR => 19, KeyCode::KeyS => 31, KeyCode::KeyT => 20,
            KeyCode::KeyU => 22, KeyCode::KeyV => 47, KeyCode::KeyW => 17, KeyCode::KeyX => 45,
            KeyCode::KeyY => 21, KeyCode::KeyZ => 44,
            KeyCode::Digit1 => 2, KeyCode::Digit2 => 3, KeyCode::Digit3 => 4, KeyCode::Digit4 => 5,
            KeyCode::Digit5 => 6, KeyCode::Digit6 => 7, KeyCode::Digit7 => 8, KeyCode::Digit8 => 9,
            KeyCode::Digit9 => 10, KeyCode::Digit0 => 11,
            KeyCode::Enter => 28, KeyCode::Space => 57, KeyCode::Backspace => 14, KeyCode::Tab => 15,
            _ => return None,
        })
    }
}
