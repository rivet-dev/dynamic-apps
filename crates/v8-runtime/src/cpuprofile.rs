//! P2 — V8 CPU profiler for the guest isolate (default-OFF, `SECURE_EXEC_CPUPROFILE=<path>`).
//!
//! rusty_v8 does not expose V8's `CpuProfiler` C++ class, so this drives the V8 **Inspector**
//! Profiler domain (the same path Chrome DevTools uses, which auto-symbolizes wasm frames). We create
//! a `V8Inspector` over the guest isolate, connect a session with a capturing `Channel`, and dispatch
//! the CDP messages `Profiler.enable` + `Profiler.start` when the guest context is created, then
//! `Profiler.stop` at isolate teardown. The `Profiler.stop` response carries the profile as
//! `result.profile`, which we write verbatim to the configured path as a `.cpuprofile` (loadable in
//! Chrome DevTools → Performance/Profiler).
//!
//! The whole thing is inert unless the env var is set: `enabled_path()` returns `None` and no inspector
//! objects are created, so the production guest-execution path is byte-for-byte unchanged.

use std::ptr::addr_of;
use std::sync::OnceLock;

use v8::inspector::{
    ChannelBase, ChannelImpl, StringBuffer, StringView, V8Inspector, V8InspectorClientBase,
    V8InspectorClientImpl, V8InspectorClientTrustLevel, V8InspectorSession,
};
use v8::UniquePtr;
use v8::UniqueRef;

/// `Some(path)` when `SECURE_EXEC_CPUPROFILE` names an output file; `None` (default) disables P2.
pub fn enabled_path() -> Option<&'static str> {
    static P: OnceLock<Option<String>> = OnceLock::new();
    P.get_or_init(|| std::env::var("SECURE_EXEC_CPUPROFILE").ok().filter(|v| !v.is_empty()))
        .as_deref()
}

struct ProfilerClient {
    base: V8InspectorClientBase,
}
impl ProfilerClient {
    fn new() -> Self {
        Self {
            base: V8InspectorClientBase::new::<Self>(),
        }
    }
}
impl V8InspectorClientImpl for ProfilerClient {
    fn base(&self) -> &V8InspectorClientBase {
        &self.base
    }
    fn base_mut(&mut self) -> &mut V8InspectorClientBase {
        &mut self.base
    }
    unsafe fn base_ptr(this: *const Self) -> *const V8InspectorClientBase {
        // SAFETY: `this` points at a live ProfilerClient; project to its `base` field.
        unsafe { addr_of!((*this).base) }
    }
}

/// Channel that captures every CDP `send_response` message so we can recover the Profiler.stop result.
struct ProfilerChannel {
    base: ChannelBase,
    responses: Vec<(i32, String)>,
}
impl ProfilerChannel {
    fn new() -> Self {
        Self {
            base: ChannelBase::new::<Self>(),
            responses: Vec::new(),
        }
    }
}
impl ChannelImpl for ProfilerChannel {
    fn base(&self) -> &ChannelBase {
        &self.base
    }
    fn base_mut(&mut self) -> &mut ChannelBase {
        &mut self.base
    }
    unsafe fn base_ptr(this: *const Self) -> *const ChannelBase {
        // SAFETY: `this` points at a live ProfilerChannel; project to its `base` field.
        unsafe { addr_of!((*this).base) }
    }
    fn send_response(&mut self, call_id: i32, message: UniquePtr<StringBuffer>) {
        if let Some(buf) = message.as_ref() {
            self.responses.push((call_id, buf.string().to_string()));
        }
    }
    fn send_notification(&mut self, _message: UniquePtr<StringBuffer>) {}
    fn flush_protocol_notifications(&mut self) {}
}

/// A live CPU-profiling session bound to one guest isolate + context. Created at context setup;
/// `stop_and_write` (consuming) is called at isolate teardown to emit the `.cpuprofile`.
pub struct GuestCpuProfiler {
    // Drop order matters: `session` (holds C++ pointers into `inspector` + `channel`) must drop first,
    // then `inspector` (points into `client` + the isolate), then the boxed `channel`/`client`. Rust
    // drops fields top-to-bottom, so this declaration order is the required teardown order.
    session: UniqueRef<V8InspectorSession>,
    inspector: UniqueRef<V8Inspector>,
    // Boxed so their heap addresses stay stable while the session/inspector hold raw pointers to them.
    channel: Box<ProfilerChannel>,
    #[allow(dead_code)]
    client: Box<ProfilerClient>,
    path: String,
}

fn dispatch(session: &mut V8InspectorSession, msg: &str) {
    let bytes = msg.as_bytes();
    session.dispatch_protocol_message(StringView::from(bytes));
}

impl GuestCpuProfiler {
    /// Begin profiling `isolate` for the guest `context_global`. Returns `None` if P2 is disabled.
    pub fn start(
        isolate: &mut v8::Isolate,
        context_global: &v8::Global<v8::Context>,
    ) -> Option<Self> {
        // Each guest runs in its own isolate; suffix the configured path with a per-isolate index so
        // concurrent guests (X server, WM, apps) each get their own `.cpuprofile.<n>` instead of
        // clobbering one file.
        use std::sync::atomic::{AtomicU32, Ordering};
        static SEQ: AtomicU32 = AtomicU32::new(0);
        let base = enabled_path()?;
        let n = SEQ.fetch_add(1, Ordering::Relaxed);
        let path = format!("{base}.{n}");
        let mut client = Box::new(ProfilerClient::new());
        // `create` only borrows the isolate for the duration of the call (returns an owned UniqueRef).
        let mut inspector = V8Inspector::create(isolate, client.as_mut());
        let mut channel = Box::new(ProfilerChannel::new());
        let session;
        {
            // Register the guest context + connect a session inside a scope so the inspector has an
            // entered context; the CDP dispatches run here too (Profiler is isolate-scoped but V8 wants
            // the isolate current, which it is on this thread).
            let scope = &mut v8::HandleScope::new(isolate);
            let ctx = v8::Local::new(scope, context_global);
            let name = b"secure-exec-guest";
            inspector.context_created(
                ctx,
                1,
                StringView::from(&name[..]),
                StringView::from(&b"{}"[..]),
            );
            let mut s = inspector.connect(
                1,
                channel.as_mut(),
                StringView::from(&b"{}"[..]),
                V8InspectorClientTrustLevel::FullyTrusted,
            );
            let _cscope = &mut v8::ContextScope::new(scope, ctx);
            dispatch(&mut s, r#"{"id":1,"method":"Profiler.enable"}"#);
            // 1000 us/sample = V8's default; explicit so the artifact is reproducible.
            dispatch(
                &mut s,
                r#"{"id":2,"method":"Profiler.setSamplingInterval","params":{"interval":1000}}"#,
            );
            dispatch(&mut s, r#"{"id":3,"method":"Profiler.start"}"#);
            session = s;
        }
        eprintln!("[cpuprofile] started (Profiler.start); will write {path} at teardown");
        Some(Self {
            session,
            inspector,
            channel,
            client,
            path,
        })
    }

    /// Stop profiling and write the `.cpuprofile` (the CDP `result.profile` object) to the path.
    /// Must be called while `isolate` is still alive (before it is dropped).
    pub fn stop_and_write(mut self, isolate: &mut v8::Isolate) {
        {
            let scope = &mut v8::HandleScope::new(isolate);
            let _ = scope;
            dispatch(&mut self.session, r#"{"id":9,"method":"Profiler.stop"}"#);
        }
        let stop_resp = self
            .channel
            .responses
            .iter()
            .find(|(id, _)| *id == 9)
            .map(|(_, body)| body.clone());
        let Some(body) = stop_resp else {
            eprintln!("[cpuprofile] ERROR: no Profiler.stop response captured; nothing written");
            return;
        };
        // The response is `{"id":9,"result":{"profile":{...}}}`. A `.cpuprofile` is exactly the value
        // of `profile` (the CDP Profiler.Profile object). Extract the balanced-brace substring after the
        // `"profile":` key (no JSON dependency; the profile is well-formed V8 output).
        let Some(profile) = extract_profile_object(&body) else {
            eprintln!("[cpuprofile] ERROR: Profiler.stop response had no result.profile object");
            return;
        };
        let n = profile.len();
        if let Err(e) = std::fs::write(&self.path, profile.as_bytes()) {
            eprintln!("[cpuprofile] ERROR writing {}: {e}", self.path);
        } else {
            eprintln!(
                "[cpuprofile] wrote {} ({n} bytes) — load in Chrome DevTools (Performance/Profiler)",
                self.path
            );
        }
    }
}

/// Extract the balanced `{...}` value that follows the first `"profile":` key in a CDP response body.
fn extract_profile_object(body: &str) -> Option<String> {
    let key = "\"profile\":";
    let start_key = body.find(key)?;
    let rest = &body[start_key + key.len()..];
    let open = rest.find('{')?;
    let bytes = rest.as_bytes();
    let mut depth = 0i32;
    let mut in_str = false;
    let mut escaped = false;
    for (i, &b) in bytes.iter().enumerate().skip(open) {
        if in_str {
            if escaped {
                escaped = false;
            } else if b == b'\\' {
                escaped = true;
            } else if b == b'"' {
                in_str = false;
            }
            continue;
        }
        match b {
            b'"' => in_str = true,
            b'{' => depth += 1,
            b'}' => {
                depth -= 1;
                if depth == 0 {
                    return Some(rest[open..=i].to_string());
                }
            }
            _ => {}
        }
    }
    None
}
