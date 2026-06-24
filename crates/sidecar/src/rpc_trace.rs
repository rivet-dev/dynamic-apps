//! Out-of-band observability for the sidecar's sync-RPC dispatch — an `strace`/lock-holder-dump for
//! the guest↔sidecar boundary.
//!
//! Why this exists: every guest host call funnels through ONE sidecar dispatch path
//! ([`crate::execution`] `service_javascript_sync_rpc`). When a multi-guest VM (a wasm X server + a
//! window manager + GTK apps) stalls, the only black-box signal is "the wire went quiet", which can't
//! distinguish a *hard stall* (the main thread is stuck inside one op) from *idleness* (a guest stopped
//! making calls because its own render loop never re-armed). Adding `fprintf` probes is a synchronous
//! host call through the same path, so it perturbs the very timing under test (a heisenbug).
//!
//! The fix mirrors how `strace` would work if we owned the kernel — which we do: the SIDECAR is the
//! kernel, so we instrument the dispatch chokepoint and record **out-of-band on the native side**, into
//! process-local mutex-guarded state that NEVER touches the guest sync path. A dedicated watchdog OS
//! thread (which no guest can starve) periodically inspects that state and dumps:
//!   - the op the dispatch thread is currently INSIDE and for how long (→ a hard stall shows here), and
//!   - a per-process activity table: each guest's last op + idle time + op count (→ a guest whose render
//!     loop died shows as "went silent" while the dispatch thread is NOT stuck).
//!
//! Zero behaviour change: this only reads/records timing. With the watchdog off (default) the hooks are
//! a couple of mutex updates per RPC; with it on, all I/O happens on the watchdog thread.
//!
//! Env:
//!   `SECURE_EXEC_RPC_WATCHDOG_MS=<n>`  enable the watchdog; warn when the dispatch thread is inside one
//!                                      op for >= n ms (default threshold 750 when set to empty/`1`).
//!   `SECURE_EXEC_RPC_WATCHDOG_DUMP_MS=<n>` also print the full per-process activity table every n ms
//!                                      (heartbeat), to watch per-guest RPC rates and spot a silent guest.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, Instant};

/// The op the single dispatch thread is currently servicing (None when between ops).
struct InFlight {
    pid: u32,
    method: String,
    started: Instant,
    seq: u64,
}

/// Rolling per-process activity, keyed by kernel pid (one per guest process / wasi-thread group).
struct ProcInfo {
    last_method: String,
    last_at: Instant,
    count: u64,
    /// The last few ops (method + ms-since-base), so a dump shows what a now-idle guest did right
    /// before it parked — the pre-stall op sequence, not just the final poll.
    recent: std::collections::VecDeque<(String, u128)>,
}

struct TraceState {
    base: Instant,
    in_flight: Mutex<Option<InFlight>>,
    procs: Mutex<HashMap<u32, ProcInfo>>,
    seq: AtomicU64,
    /// Cumulative time the dispatch thread spent INSIDE an op (microseconds). Busy-fraction =
    /// busy_us / wall-elapsed distinguishes throughput-saturation (≈100%) from per-round-trip latency
    /// or idle-waiting (≪100%).
    busy_us: AtomicU64,
    /// Total ops completed (for mean op duration).
    done: AtomicU64,
    /// Stall threshold; `None` = watchdog disabled.
    watchdog_ms: Option<u64>,
    dump_ms: Option<u64>,
}

fn parse_ms(name: &str) -> Option<u64> {
    match std::env::var(name) {
        Err(_) => None,
        Ok(v) if v.is_empty() || v == "1" => Some(0), // sentinel: use default
        Ok(v) => v.parse::<u64>().ok(),
    }
}

fn state() -> &'static TraceState {
    static STATE: OnceLock<TraceState> = OnceLock::new();
    STATE.get_or_init(|| {
        let watchdog_ms = parse_ms("SECURE_EXEC_RPC_WATCHDOG_MS").map(|n| if n == 0 { 750 } else { n });
        let dump_ms = parse_ms("SECURE_EXEC_RPC_WATCHDOG_DUMP_MS").map(|n| if n == 0 { 3000 } else { n });
        let s = TraceState {
            base: Instant::now(),
            in_flight: Mutex::new(None),
            procs: Mutex::new(HashMap::new()),
            seq: AtomicU64::new(0),
            busy_us: AtomicU64::new(0),
            done: AtomicU64::new(0),
            watchdog_ms,
            dump_ms,
        };
        if watchdog_ms.is_some() || dump_ms.is_some() {
            spawn_watchdog();
        }
        s
    })
}

/// True when any out-of-band tracking is requested (cheap gate kept for the dispatch hot path).
pub(crate) fn watchdog_active() -> bool {
    state().watchdog_ms.is_some() || state().dump_ms.is_some()
}

/// Record entry into a sync-RPC op on the dispatch thread. Returns a sequence id to pass to
/// [`on_exit`] so a stale exit (after the watchdog already moved on) can't clear a newer op.
pub(crate) fn on_enter(pid: u32, method: &str) -> u64 {
    let st = state();
    let seq = st.seq.fetch_add(1, Ordering::Relaxed);
    if let Ok(mut guard) = st.in_flight.lock() {
        *guard = Some(InFlight {
            pid,
            method: method.to_string(),
            started: Instant::now(),
            seq,
        });
    }
    if let Ok(mut procs) = st.procs.lock() {
        let base = st.base;
        let e = procs.entry(pid).or_insert_with(|| ProcInfo {
            last_method: String::new(),
            last_at: Instant::now(),
            count: 0,
            recent: std::collections::VecDeque::with_capacity(8),
        });
        e.last_method = method.to_string();
        e.last_at = Instant::now();
        e.count += 1;
        // Keep the last few NON-poll ops (polls dominate and obscure the real pre-stall work).
        if !method.starts_with("net.poll") {
            e.recent.push_back((method.to_string(), base.elapsed().as_millis()));
            while e.recent.len() > 6 {
                e.recent.pop_front();
            }
        }
    }
    seq
}

/// Record exit from the op identified by `seq` (clears in-flight if it is still the current op).
pub(crate) fn on_exit(seq: u64) {
    let st = state();
    if let Ok(mut guard) = st.in_flight.lock() {
        if let Some(f) = guard.as_ref() {
            if f.seq == seq {
                st.busy_us
                    .fetch_add(f.started.elapsed().as_micros() as u64, Ordering::Relaxed);
                st.done.fetch_add(1, Ordering::Relaxed);
                *guard = None;
            }
        }
    }
}

fn spawn_watchdog() {
    std::thread::Builder::new()
        .name("se-rpc-watchdog".into())
        .spawn(watchdog_loop)
        .expect("spawn rpc watchdog");
}

fn watchdog_loop() {
    let st = state();
    let stall = st.watchdog_ms.map(Duration::from_millis);
    let dump_every = st.dump_ms.map(Duration::from_millis);
    // Tick fast enough to catch the threshold but never busier than 50ms.
    let tick = stall
        .map(|d| (d / 2).max(Duration::from_millis(50)))
        .unwrap_or(Duration::from_millis(250))
        .min(Duration::from_millis(500));
    let mut last_dump = Instant::now();
    let mut warned_seq: Option<u64> = None;
    loop {
        std::thread::sleep(tick);
        // Hard-stall detection: the dispatch thread sitting inside one op past the threshold.
        if let (Some(stall), Ok(guard)) = (stall, st.in_flight.lock()) {
            if let Some(f) = guard.as_ref() {
                let held = f.started.elapsed();
                if held >= stall && warned_seq != Some(f.seq) {
                    warned_seq = Some(f.seq);
                    let ms = st.base.elapsed().as_millis();
                    eprintln!(
                        "[rpc-watchdog +{ms}ms] DISPATCH STUCK pid={} op={} held={}ms seq={}",
                        f.pid,
                        f.method,
                        held.as_millis(),
                        f.seq
                    );
                    drop(guard);
                    dump_activity("stuck-context");
                    continue;
                }
            } else {
                warned_seq = None;
            }
        }
        if let Some(every) = dump_every {
            if last_dump.elapsed() >= every {
                last_dump = Instant::now();
                dump_activity("heartbeat");
            }
        }
    }
}

/// Dump the per-process activity table out-of-band: each guest's last op + idle time + op count.
/// A guest whose render loop stalled shows up as a long idle time while the dispatch thread is NOT
/// stuck — the signal that distinguishes "blocked in an op" from "stopped making calls".
fn dump_activity(why: &str) {
    let st = state();
    let ms = st.base.elapsed().as_millis();
    let Ok(procs) = st.procs.lock() else { return };
    let mut rows: Vec<(u32, String, u128, u64)> = procs
        .iter()
        .map(|(pid, info)| {
            (
                *pid,
                info.last_method.clone(),
                info.last_at.elapsed().as_millis(),
                info.count,
            )
        })
        .collect();
    rows.sort_by_key(|r| r.0);
    let busy_us = st.busy_us.load(Ordering::Relaxed);
    let done = st.done.load(Ordering::Relaxed).max(1);
    let wall_us = st.base.elapsed().as_micros() as u64;
    let busy_pct = if wall_us > 0 { busy_us * 100 / wall_us } else { 0 };
    eprintln!(
        "[rpc-watchdog +{ms}ms] activity ({why}): {} process(es) | DISPATCH BUSY {busy_pct}% \
         (busy={}ms wall={}ms ops={done} mean={}us/op)",
        rows.len(),
        busy_us / 1000,
        wall_us / 1000,
        busy_us / done,
    );
    for (pid, method, idle_ms, count) in &rows {
        eprintln!("    pid={pid:<6} last={method:<28} idle={idle_ms:>7}ms ops={count}");
    }
    // For a guest that has gone idle (parked), print its last non-poll ops: the pre-stall trail.
    for (pid, _, idle_ms, _) in &rows {
        if *idle_ms < 1500 {
            continue;
        }
        if let Some(info) = procs.get(pid) {
            if info.recent.is_empty() {
                continue;
            }
            let trail: Vec<String> = info
                .recent
                .iter()
                .map(|(m, at)| format!("{m}@{at}ms"))
                .collect();
            eprintln!("      pid={pid} pre-park trail: {}", trail.join(" -> "));
        }
    }
}
