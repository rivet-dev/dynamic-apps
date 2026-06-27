// V8 isolate lifecycle: platform init, create, configure, destroy

use std::collections::HashMap;
use std::ffi::c_void;
use std::sync::Once;

use crate::ipc::ExecutionError;

static V8_INIT: Once = Once::new();
const MAX_UNHANDLED_PROMISE_REJECTIONS: usize = 1024;

#[repr(align(16))]
struct AlignedBytes<const N: usize>([u8; N]);

static ICU_COMMON_DATA: AlignedBytes<
    { include_bytes!(concat!(env!("OUT_DIR"), "/icudtl.dat")).len() },
> = AlignedBytes(*include_bytes!(concat!(env!("OUT_DIR"), "/icudtl.dat")));

#[derive(Default)]
pub struct PromiseRejectState {
    pub unhandled: HashMap<i32, ExecutionError>,
    overflow_count: usize,
}

impl PromiseRejectState {
    fn record_unhandled(&mut self, promise_id: i32, error: ExecutionError) {
        use std::collections::hash_map::Entry;
        // Cache the length before taking the entry, since `Entry` borrows the
        // map mutably and we cannot read `len()` while it is held.
        let under_limit = self.unhandled.len() < MAX_UNHANDLED_PROMISE_REJECTIONS;
        match self.unhandled.entry(promise_id) {
            // Existing rejection for this promise — overwrite with latest error.
            Entry::Occupied(mut entry) => {
                entry.insert(error);
            }
            // New rejection: store it if under the cap, otherwise count overflow.
            Entry::Vacant(entry) => {
                if under_limit {
                    entry.insert(error);
                } else {
                    self.overflow_count = self.overflow_count.saturating_add(1);
                }
            }
        }
    }

    fn mark_handled(&mut self, promise_id: i32) {
        if self.unhandled.remove(&promise_id).is_none() && self.overflow_count > 0 {
            self.overflow_count -= 1;
        }
    }

    pub fn take_next_unhandled(&mut self) -> Option<ExecutionError> {
        if self.overflow_count > 0 {
            self.overflow_count = 0;
            self.unhandled.clear();
            return Some(ExecutionError {
                error_type: "Error".into(),
                message: format!(
                    "unhandled promise rejection registry exceeded limit of {MAX_UNHANDLED_PROMISE_REJECTIONS} rejections"
                ),
                stack: String::new(),
                code: Some("ERR_AGENT_OS_UNHANDLED_REJECTION_LIMIT".into()),
            });
        }
        self.unhandled.drain().next().map(|(_, err)| err)
    }
}

extern "C" fn promise_reject_callback(msg: v8::PromiseRejectMessage) {
    let scope = &mut unsafe { v8::CallbackScope::new(&msg) };
    let promise_id = msg.get_promise().get_identity_hash().get();
    match msg.get_event() {
        v8::PromiseRejectEvent::PromiseRejectWithNoHandler => {
            let error = {
                let scope = &mut v8::HandleScope::new(scope);
                let value = msg
                    .get_value()
                    .unwrap_or_else(|| v8::undefined(scope).into());
                crate::execution::extract_error_info(scope, value)
            };
            if let Some(state) = scope.get_slot_mut::<PromiseRejectState>() {
                state.record_unhandled(promise_id, error);
            }
        }
        v8::PromiseRejectEvent::PromiseHandlerAddedAfterReject => {
            if let Some(state) = scope.get_slot_mut::<PromiseRejectState>() {
                state.mark_handled(promise_id);
            }
        }
        _ => {}
    }
}

pub fn configure_isolate(isolate: &mut v8::OwnedIsolate) {
    isolate.set_slot(PromiseRejectState::default());
    isolate.set_promise_reject_callback(promise_reject_callback);
}

/// Initialize the V8 platform (once per process).
/// Safe to call multiple times; only the first call takes effect.
pub fn init_v8_platform() {
    V8_INIT.call_once(|| {
        // SECURE_EXEC_V8PROF: enable V8's tick profiler (~ perf). V8 writes per-isolate `isolate-*.log`
        // (cwd) with code-creation (incl. wasm function names if the name section is present) + ticks;
        // symbolize the spinning function with scripts/v8prof-top.py. See INTERNAL-TOOLING.md.
        if std::env::var("SECURE_EXEC_V8PROF").map(|v| v != "0" && !v.is_empty()).unwrap_or(false) {
            v8::V8::set_flags_from_string(
                "--prof --no-logfile-per-isolate --logfile=/tmp/secure-exec-v8.log --prof-sampling-interval=200",
            );
        }
        // NOTE (L-U, REFUTED 2026-06-29): `--wasm-lazy-compilation` was A/B-tested as a launch-latency
        // fix (the eager compile of the large GTK module holds the sidecar ~0.9s at ExecuteRequest). It
        // is NULL for first-paint (eager ~9.7s vs lazy ~9.8s): lazy just defers per-function compile to
        // first call, and GTK init calls most of the module during init, so the cost is paid either way.
        // Left out (eager default) — lazy can also slow steady-state. Opt-in only if a future need arises.
        if std::env::var("SECURE_EXEC_WASM_LAZY").map(|v| v == "1").unwrap_or(false) {
            v8::V8::set_flags_from_string("--wasm-lazy-compilation");
        }
        // A/B knob for V8 wasm execution-tier flags (diagnostic). mousepad first-paint is COMPUTE-bound
        // (~7s on-CPU wasm), so the suspect is single-pass GTK-init code staying in the Liftoff baseline
        // tier (unoptimized) instead of tiering up to TurboFan. Pass flags like `--no-liftoff` (TurboFan
        // only) or `--wasm-tiering-budget=<n>` (tier up sooner) to measure the compute-vs-compile tradeoff.
        if let Ok(flags) = std::env::var("SECURE_EXEC_V8_WASM_FLAGS") {
            if !flags.is_empty() {
                v8::V8::set_flags_from_string(&flags);
            }
        }
        v8::icu::set_common_data_74(&ICU_COMMON_DATA.0)
            .expect("failed to initialize V8 ICU common data");
        let platform = v8::new_default_platform(0, false).make_shared();
        v8::V8::initialize_platform(platform);
        v8::V8::initialize();
    });
}

// Headroom granted to V8 when the near-heap-limit callback fires. V8 fatal-aborts
// the whole process (SIGTRAP) if the callback does not raise the limit, so we must
// hand back a larger limit to give the engine room to unwind. Termination has
// already been requested, so this extra budget only covers propagation of the
// uncatchable termination exception, not continued guest allocation.
const NEAR_HEAP_LIMIT_HEADROOM_BYTES: usize = 16 * 1024 * 1024;

/// Invoked by V8 when heap usage approaches the configured limit. Instead of
/// letting V8 fatal-abort the (process-global) runtime, request termination of the
/// offending isolate and return a raised limit so V8 can propagate the uncatchable
/// termination exception cleanly. `data` is a leaked `Box<v8::IsolateHandle>` for
/// the isolate this callback was registered on.
extern "C" fn near_heap_limit_callback(
    data: *mut c_void,
    current_heap_limit: usize,
    initial_heap_limit: usize,
) -> usize {
    if !data.is_null() {
        // Safety: `data` is the pointer produced by `Box::into_raw` in
        // `install_heap_limit_guard` and lives for the entire lifetime of the
        // isolate.
        let handle = unsafe { &*(data as *const v8::IsolateHandle) };
        // Terminate any JS currently running on this isolate. This unwinds the
        // guest with an uncatchable exception rather than crashing the process.
        handle.terminate_execution();
    }
    // Grant headroom so V8 does not immediately fatal-abort before the termination
    // takes effect. We never shrink below the current limit.
    current_heap_limit
        .max(initial_heap_limit)
        .saturating_add(NEAR_HEAP_LIMIT_HEADROOM_BYTES)
}

/// Register the near-heap-limit OOM guard on an isolate that was created with a
/// configured heap cap. Without this guard, V8 fatal-aborts the whole (process-
/// global) runtime with a SIGTRAP when the cap is reached, taking down every
/// concurrent tenant; with it, the offending isolate is terminated instead.
///
/// Must be called for every isolate created with a non-`None` heap limit,
/// regardless of whether it was built fresh or restored from a snapshot.
pub fn install_heap_limit_guard(isolate: &mut v8::OwnedIsolate) {
    // The callback needs a thread-safe handle to request termination of this very
    // isolate. The handle is leaked so it outlives the callback registration; the
    // number of isolates per process is bounded, so this is not an unbounded leak,
    // and the memory is reclaimed when the process exits.
    let handle = Box::new(isolate.thread_safe_handle());
    let data = Box::into_raw(handle) as *mut c_void;
    isolate.add_near_heap_limit_callback(near_heap_limit_callback, data);
}

// --- SECURE_EXEC_STACKDUMP: the wasm `gdb thread apply all bt`. When SECURE_EXEC_STACKDUMP_AFTER_MS
// is set, a watchdog interrupts every registered session isolate after that delay and prints its
// current JS/wasm stack to stderr, repeating SECURE_EXEC_STACKDUMP_SAMPLES times (default 3, 600ms
// apart) so a busy-spin (stack varies) is distinguishable from a clean block (stack identical). Pure
// instrumentation; off (zero cost) unless the env var is set. See experiments/wasm-gui/INTERNAL-TOOLING.md.
static DIAG_ISOLATES: std::sync::OnceLock<std::sync::Mutex<Vec<(String, v8::IsolateHandle)>>> =
    std::sync::OnceLock::new();
static DIAG_WATCHDOG: std::sync::Once = std::sync::Once::new();

fn diag_stackdump_after_ms() -> Option<u64> {
    std::env::var("SECURE_EXEC_STACKDUMP_AFTER_MS")
        .ok()
        .and_then(|v| v.parse::<u64>().ok())
        .filter(|&n| n > 0)
}

/// Classify an interrupted guest OS thread from its native backtrace. This is the cheap
/// deadlock-vs-livelock verdict: a guest `pthread_mutex`/`pthread_cond`/raw `memory.atomic.wait`
/// parks inside V8's `FutexEmulation` (C++), a guest blocked in a kernel/sync-RPC call sits in the
/// sidecar's poll/recv, and otherwise the thread is actively running JIT'd wasm (a CPU spin if it
/// stays here across rounds). We can't recover the futex ADDRESS or lock owner from the native frames
/// (those live in wasm linear memory) — that needs the threaded-libc futex trace (a follow-up); this
/// verdict is the part that was the hard call in the M8.0 gtk_init debugging.
fn classify_stack(bt: &str) -> &'static str {
    if bt.contains("FutexEmulation")
        || bt.contains("AtomicWait") // V8 Runtime_WasmI32/I64AtomicWait
        || bt.contains("AtomicsWait")
        || bt.contains("atomic_wait")
    {
        "PARKED-ON-FUTEX (memory.atomic.wait — pthread mutex/cond or raw atomics; deadlock candidate)"
    } else if bt.contains("recv_timeout")
        || bt.contains("epoll")
        || bt.contains("::poll")
        || bt.contains("read_generic")
        || bt.contains("condvar")
        || bt.contains("Condvar")
        || bt.contains("parking_lot")
    {
        "BLOCKED-IN-HOST (sidecar/kernel wait — e.g. net.poll/recv; normal if it makes progress)"
    } else {
        "RUNNING (JIT/wasm or a V8 builtin — livelock/CPU-spin candidate if it stays here across rounds)"
    }
}

// --- Tool #3: wasm-frame symbolizer for a LIVELOCKED guest -----------------------------------------
// The native backtrace (above) stops at the V8 C++/JIT boundary — it cannot unwind JIT'd wasm frames
// (no frame-pointer/unwind info), and the interrupt callback cannot open a HandleScope to ask V8 for
// the JS/wasm stack. So we NAME the spinning wasm function two ways that DON'T need a scope:
//   1. SetJitCodeEventHandler (pure-C ABI; callable straight from Rust) records every JIT'd code blob's
//      [address range, name] as V8 compiles it — for wasm, V8 names them `wasm-function[N]` (or the
//      real name if the module kept its name section).
//   2. At the interrupt safepoint we run on the spinning thread, so a CONSERVATIVE SCAN of this thread's
//      own stack for return addresses that land in a recorded wasm range reveals the live call chain.
// Both are gated on SECURE_EXEC_STACKDUMP_AFTER_MS, so there is zero cost in normal runs.

/// V8 130 `JitCodeEvent` ABI (x86_64). Only the prefix fields + the `name` arm of the trailing union
/// are read; layout must match v8/include/v8-callbacks.h exactly.
#[repr(C)]
struct JitCodeEventRaw {
    event_type: i32,            // 0: CODE_ADDED=0, CODE_MOVED=1, ...
    code_type: i32,             // 4: BYTE_CODE=0, JIT_CODE=1, WASM_CODE=2
    code_start: *const u8,      // 8
    code_len: usize,            // 16
    _script: *const c_void,     // 24: Local<UnboundScript>
    _user_data: *const c_void,  // 32
    _wasm_source_info: *const c_void, // 40
    name_str: *const u8,        // 48: union { name_t { str, len } } arm
    name_len: usize,            // 56
    _isolate: *const c_void,    // 64
}

/// Recorded JIT code blobs: (start_addr, end_addr, name). Process-global — JIT addresses are unique
/// across isolates in one address space, so a single map serves all guest VMs.
static JIT_CODE_MAP: std::sync::OnceLock<std::sync::Mutex<Vec<(usize, usize, String)>>> =
    std::sync::OnceLock::new();

extern "C" fn jit_code_event_handler(event: *const JitCodeEventRaw) {
    if event.is_null() {
        return;
    }
    let ev = unsafe { &*event };
    if ev.event_type != 0 {
        return; // only CODE_ADDED
    }
    let start = ev.code_start as usize;
    if start == 0 || ev.code_len == 0 {
        return;
    }
    let name = if !ev.name_str.is_null() && ev.name_len > 0 && ev.name_len < 4096 {
        let bytes = unsafe { std::slice::from_raw_parts(ev.name_str, ev.name_len) };
        String::from_utf8_lossy(bytes).into_owned()
    } else {
        // BYTE_CODE/JIT_CODE/WASM_CODE without a name string.
        format!("<{}@{:#x}>", ["bytecode", "jit", "wasm"].get(ev.code_type as usize).unwrap_or(&"code"), start)
    };
    if let Ok(mut map) = JIT_CODE_MAP
        .get_or_init(|| std::sync::Mutex::new(Vec::new()))
        .lock()
    {
        map.push((start, start + ev.code_len, name));
    }
}

extern "C" {
    // v8::Isolate::SetJitCodeEventHandler(JitCodeEventOptions, JitCodeEventHandler) — a non-static member,
    // so the SysV ABI passes `this` (the isolate) as the first integer arg. Pure-C signature: no std types.
    #[link_name = "_ZN2v87Isolate22SetJitCodeEventHandlerENS_19JitCodeEventOptionsEPFvPKNS_12JitCodeEventEE"]
    fn v8_isolate_set_jit_code_event_handler(
        isolate: *mut c_void,
        options: i32,
        handler: extern "C" fn(*const JitCodeEventRaw),
    );
}

/// Install the JIT-code recorder on this isolate (only when the stackdump watchdog is armed).
pub(crate) fn install_jit_code_recorder(isolate: &mut v8::Isolate) {
    if diag_stackdump_after_ms().is_none() {
        return;
    }
    let isolate_ptr = isolate as *mut v8::Isolate as *mut c_void;
    // kJitCodeEventDefault = 0: report code as it is added. wasm is compiled during guest execution
    // (after this call), so its functions are captured.
    unsafe { v8_isolate_set_jit_code_event_handler(isolate_ptr, 0, jit_code_event_handler) };
}

/// Conservatively scan THIS thread's stack for return addresses that land inside a recorded wasm
/// code range, and print the matching function names innermost-first. Heuristic (may include
/// stale/false-positive addresses), but the cluster nearest the stack pointer names the live frame.
fn scan_stack_for_wasm_frames(label: &str) {
    let map_lock = match JIT_CODE_MAP.get() {
        Some(m) => m,
        None => return,
    };
    let map = match map_lock.lock() {
        Ok(m) => m,
        Err(_) => return,
    };
    if map.is_empty() {
        return;
    }
    // Bound the scan by this thread's actual stack extent so we never read unmapped memory.
    let (stack_lo, stack_hi) = match current_thread_stack_bounds() {
        Some(b) => b,
        None => return,
    };
    // Sort by start for a quick range summary; report map size + the code span it covers.
    let mut ranges: Vec<(usize, usize, &str)> =
        map.iter().map(|(s, e, n)| (*s, *e, n.as_str())).collect();
    ranges.sort_by_key(|r| r.0);
    let span_lo = ranges.first().map(|r| r.0).unwrap_or(0);
    let span_hi = ranges.last().map(|r| r.1).unwrap_or(0);
    let probe = 0usize;
    let sp = (&probe as *const usize) as usize;
    eprintln!(
        "[stackdump] {label} wasm-frame scan: {} code blobs in [{:#x},{:#x}), stack [{:#x},{:#x}), sp={:#x}:",
        ranges.len(),
        span_lo,
        span_hi,
        stack_lo,
        stack_hi,
        sp
    );
    let mut addr = sp & !7; // 8-byte aligned
    let top = stack_hi;
    let mut printed = 0usize;
    while addr + 8 <= top && addr >= stack_lo {
        let word = unsafe { (addr as *const usize).read() };
        if word >= span_lo && word < span_hi {
            // binary search the sorted ranges
            if let Ok(i) | Err(i) = ranges.binary_search_by(|r| {
                if word < r.0 {
                    std::cmp::Ordering::Greater
                } else if word >= r.1 {
                    std::cmp::Ordering::Less
                } else {
                    std::cmp::Ordering::Equal
                }
            }) {
                if let Some((start, end, name)) = ranges.get(i) {
                    if word >= *start && word < *end {
                        eprintln!(
                            "[stackdump]   +{:#08x}  ra={:#x}  {} (+{:#x})",
                            addr - sp,
                            word,
                            name,
                            word - start
                        );
                        printed += 1;
                    }
                }
            }
        }
        if printed >= 80 {
            break;
        }
        addr += 8;
    }
    if printed == 0 {
        eprintln!("[stackdump]   (no JIT return addresses found on stack — frame may be in a V8 C++ builtin)");
    }
}

/// (lowest, highest) address of the current thread's stack, via pthread, so the scan stays in-bounds.
fn current_thread_stack_bounds() -> Option<(usize, usize)> {
    unsafe {
        let mut attr: libc::pthread_attr_t = std::mem::zeroed();
        if libc::pthread_getattr_np(libc::pthread_self(), &mut attr) != 0 {
            return None;
        }
        let mut base: *mut c_void = std::ptr::null_mut();
        let mut size: libc::size_t = 0;
        let rc = libc::pthread_attr_getstack(&attr, &mut base, &mut size);
        libc::pthread_attr_destroy(&mut attr);
        if rc != 0 || base.is_null() || size == 0 {
            return None;
        }
        let lo = base as usize;
        Some((lo, lo + size))
    }
}

extern "C" fn diag_stack_dump_callback(_isolate: &mut v8::Isolate, data: *mut c_void) {
    // `data` is a leaked Box<String> label produced per request; reclaim it here.
    let label = if data.is_null() {
        String::from("?")
    } else {
        *unsafe { Box::from_raw(data as *mut String) }
    };
    // We canNOT create a rusty_v8 HandleScope here: this runs at a safepoint while the guest's own
    // scope chain is active, and a second root scope aborts ("active scope can't be dropped"). So we
    // capture the NATIVE backtrace of the interrupted guest OS thread instead — symbolizes V8/Rust
    // frames (reveals a spin inside a V8 builtin: atomics/GC/futex) and shows raw addresses for JIT'd
    // wasm (a busy-spin in pure guest code). See experiments/wasm-gui/INTERNAL-TOOLING.md.
    let bt = std::backtrace::Backtrace::force_capture();
    let bt_str = format!("{bt}");
    // One-line per-thread verdict first (greppable: `[stackdump] ... => `), then the full backtrace.
    eprintln!("[stackdump] {label} => {}", classify_stack(&bt_str));
    eprintln!("[stackdump] {label} native backtrace:\n{bt_str}");
    // Tool #3: when the verdict is RUNNING (a wasm spin), name the live wasm frames by scanning this
    // thread's stack against the JIT code map. (For PARKED/BLOCKED the native frames already suffice.)
    if classify_stack(&bt_str).starts_with("RUNNING") {
        scan_stack_for_wasm_frames(&label);
    }
}

/// Register an isolate so the stack-dump watchdog can interrupt it. No-op (and no handle retained)
/// unless SECURE_EXEC_STACKDUMP_AFTER_MS is set, so there is zero cost in normal runs.
pub fn register_isolate_for_diag(isolate: &mut v8::OwnedIsolate) {
    let after_ms = match diag_stackdump_after_ms() {
        Some(ms) => ms,
        None => return,
    };
    let tid = unsafe { libc::syscall(libc::SYS_gettid) };
    let name = std::thread::current()
        .name()
        .map(|s| s.to_owned())
        .unwrap_or_else(|| String::from("isolate"));
    let label = format!("{name}#tid{tid}");
    DIAG_ISOLATES
        .get_or_init(|| std::sync::Mutex::new(Vec::new()))
        .lock()
        .unwrap()
        .push((label, isolate.thread_safe_handle()));

    DIAG_WATCHDOG.call_once(|| {
        let samples: u64 = std::env::var("SECURE_EXEC_STACKDUMP_SAMPLES")
            .ok()
            .and_then(|v| v.parse().ok())
            .unwrap_or(3);
        let interval: u64 = std::env::var("SECURE_EXEC_STACKDUMP_INTERVAL_MS")
            .ok()
            .and_then(|v| v.parse().ok())
            .unwrap_or(600);
        std::thread::Builder::new()
            .name("secure-exec-stackdump".into())
            .spawn(move || {
                std::thread::sleep(std::time::Duration::from_millis(after_ms));
                for round in 0..samples.max(1) {
                    eprintln!("[stackdump] === round {round} (t={after_ms}ms+{round}x{interval}ms) ===");
                    if let Some(reg) = DIAG_ISOLATES.get() {
                        let guard = reg.lock().unwrap();
                        for (label, handle) in guard.iter() {
                            let data = Box::into_raw(Box::new(label.clone())) as *mut c_void;
                            // If the isolate is gone, request_interrupt returns false and the leaked
                            // label would leak; reclaim it in that case.
                            if !handle.request_interrupt(diag_stack_dump_callback, data) {
                                drop(unsafe { Box::from_raw(data as *mut String) });
                            }
                        }
                    }
                    std::thread::sleep(std::time::Duration::from_millis(interval));
                }
                eprintln!("[stackdump] === done ===");
            })
            .ok();
    });
}

/// Create a new V8 isolate with an optional heap limit in MB.
pub fn create_isolate(heap_limit_mb: Option<u32>) -> v8::OwnedIsolate {
    let mut params = v8::CreateParams::default();
    if let Some(limit) = heap_limit_mb {
        let limit_bytes = (limit as usize) * 1024 * 1024;
        params = params.heap_limits(0, limit_bytes);
    }
    let mut isolate = v8::Isolate::new(params);
    configure_isolate(&mut isolate);
    if heap_limit_mb.is_some() {
        install_heap_limit_guard(&mut isolate);
    }
    install_jit_code_recorder(&mut isolate);
    register_isolate_for_diag(&mut isolate);
    isolate
}

/// Create a new V8 context on the given isolate.
/// Returns a Global handle so the context can be reused across scopes.
pub fn create_context(isolate: &mut v8::OwnedIsolate) -> v8::Global<v8::Context> {
    let global = {
        let scope = &mut v8::HandleScope::new(isolate);
        let context = v8::Context::new(scope, Default::default());
        v8::Global::new(scope, context)
    };
    global
}

// V8 lifecycle tests are consolidated in execution::tests to avoid
// inter-test SIGSEGV from V8 global state issues.
