//! D16 hopsplit probe (`SECURE_EXEC_HOPSPLIT=1`, default-OFF).
//!
//! D15 decomposed one applySync hop into ser / rt / de and found the whole floor
//! lives in `rt` (= `BridgeCallContext::sync_call`), the cross-thread roundtrip. R2
//! reframed `rt` as lumping THREE thread handoffs: guest isolate thread → per-session
//! event-bridge thread → the single shared sync-RPC service loop → back to the guest.
//! This probe stamps a real monotonic clock (`Instant`, process-wide monotonic on
//! Linux) at each handoff, keyed by `call_id`, so we can see which handoff owns the
//! ~707us. That decides whether the remaining ir floor is attackable transport
//! (forward/deliver scheduling) or intrinsic peer-guest work (service span), which in
//! turn gates F10-REFRAMED vs the F3-DEFERRED concurrency redesign.
//!
//! Stamp points (keyed by `call_id`):
//!   0 = guest enqueued the BridgeCall            (`host_call::sync_call`, guest thread)
//!   1 = event-bridge thread emitted SyncRpcRequest (`javascript::spawn_v8_event_bridge`)
//!   2 = service loop began servicing            (`execution::service_javascript_sync_rpc`)
//!   3 = service loop sent the response          (`respond_javascript_sync_rpc_success`)
//!   4 = guest `recv_response` returned          (`host_call::sync_call`, guest thread)
//!
//! Per-hop deltas: d01 forward-to-bridge wake, d12 bridge→service-loop forward + wake,
//! d23 service span (kernel + peer-guest), d34 response delivery wake. Aggregated per
//! method, flushed to stderr every 200 fully-stamped hops. Calls handled inline on the
//! bridge thread (module/log) never reach points 2/3 and are counted as `miss`.

use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};
use std::time::Instant;

const POINTS: usize = 5;

/// `true` only when `SECURE_EXEC_HOPSPLIT=1`. Cached once; when off every entry point
/// below is a single branch + return so the default benchmark path is untouched.
pub fn hopsplit_enabled() -> bool {
    static GATE: OnceLock<bool> = OnceLock::new();
    *GATE.get_or_init(|| std::env::var("SECURE_EXEC_HOPSPLIT").as_deref() == Ok("1"))
}

type Stamps = [Option<Instant>; POINTS];

fn table() -> &'static Mutex<HashMap<u64, Stamps>> {
    static T: OnceLock<Mutex<HashMap<u64, Stamps>>> = OnceLock::new();
    T.get_or_init(|| Mutex::new(HashMap::new()))
}

/// Record the timestamp for `point` (0..=4) of `call_id`. No-op unless enabled.
pub fn hopsplit_stamp(call_id: u64, point: usize) {
    if !hopsplit_enabled() || point >= POINTS {
        return;
    }
    let now = Instant::now();
    if let Ok(mut t) = table().lock() {
        let entry = t.entry(call_id).or_insert([None; POINTS]);
        entry[point] = Some(now);
    }
}

/// Drop a leaked entry (e.g. a send/recv error path that never reaches finish).
pub fn hopsplit_drop(call_id: u64) {
    if !hopsplit_enabled() {
        return;
    }
    if let Ok(mut t) = table().lock() {
        t.remove(&call_id);
    }
}

#[derive(Default)]
struct HopAgg {
    n: u64,
    miss: u64,
    d01: u64,
    d12: u64,
    d23: u64,
    d34: u64,
    total: u64,
    max_total: u64,
    max_d23: u64,
}

/// Finalize `call_id`: remove its stamps, compute per-hop deltas, aggregate per method,
/// and flush every 200 fully-stamped hops. Call once from the guest thread after
/// `recv_response` (point 4 already stamped). No-op unless enabled.
pub fn hopsplit_finish(call_id: u64, method: &str) {
    if !hopsplit_enabled() {
        return;
    }
    let stamps = match table().lock() {
        Ok(mut t) => t.remove(&call_id),
        Err(_) => return,
    };
    let Some(s) = stamps else { return };

    static MAP: OnceLock<Mutex<HashMap<String, HopAgg>>> = OnceLock::new();
    let map = MAP.get_or_init(|| Mutex::new(HashMap::new()));
    let Ok(mut guard) = map.lock() else { return };
    let agg = guard.entry(method.to_string()).or_default();

    // Need all five stamps to attribute a full roundtrip; otherwise it was serviced
    // inline (module/log) or short-circuited — count it as a miss and bail.
    let (Some(t0), Some(t1), Some(t2), Some(t3), Some(t4)) = (s[0], s[1], s[2], s[3], s[4]) else {
        agg.miss += 1;
        return;
    };
    let d01 = t1.saturating_duration_since(t0).as_micros() as u64;
    let d12 = t2.saturating_duration_since(t1).as_micros() as u64;
    let d23 = t3.saturating_duration_since(t2).as_micros() as u64;
    let d34 = t4.saturating_duration_since(t3).as_micros() as u64;
    let total = t4.saturating_duration_since(t0).as_micros() as u64;
    agg.n += 1;
    agg.d01 += d01;
    agg.d12 += d12;
    agg.d23 += d23;
    agg.d34 += d34;
    agg.total += total;
    if total > agg.max_total {
        agg.max_total = total;
    }
    if d23 > agg.max_d23 {
        agg.max_d23 = d23;
    }
    if agg.n % 200 == 0 {
        eprintln!(
            "[hopsplit] {} hops={} miss={} avgTotalUs={} | d01(fwd-wake)={} d12(svc-wake)={} d23(svc-span)={} d34(deliver-wake)={} | maxTotalUs={} maxD23Us={}",
            method,
            agg.n,
            agg.miss,
            agg.total / agg.n,
            agg.d01 / agg.n,
            agg.d12 / agg.n,
            agg.d23 / agg.n,
            agg.d34 / agg.n,
            agg.max_total,
            agg.max_d23,
        );
    }
}
