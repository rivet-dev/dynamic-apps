# Secure-Exec Runtime Performance Optimization (startup / render throughput)

Continues from `XU7-STARVATION-INVESTIGATION.md`. That investigation fixed the **T-J starvation
spin** (the desktop went from 0% black to rendering). It surfaced a *separate* root — **T-H: the
single kernel service thread serializes all guests' syscalls** — as the reason rendering is *slow*
(single GTK app ~90-120s, xfce4-panel ~250-300s, 5-app desktop never finishes its D-Bus handshakes).
This spec drives that startup/render-throughput frontier to a measured target.

## 1. Problem statement

Rendering is correct but far too slow. Working mechanism (hypothesized, **unmeasured** — measuring it
is Phase 0): every guest syscall is one *synchronous* RPC serviced by a single kernel thread, GObject
dispatch routes through a JS `ffi_call` shim, payloads are JSON/base64-marshaled, and startup repeats
redundant scans (fontconfig / icon-theme / gschemas). Rough model: **cost ≈ (syscalls + GObject
calls) × per-op overhead**, all serialized. Idle is already cheap (the T-J fix made waits
event-driven); pure compute is near-native (JIT wasm). The slowness is the **boundary-crossing tax**,
densest at startup.

## 2. Targets (definition of done)

- **Single real GTK app (mousepad) first-paint: < 10s.**
- **5-app Xfce desktop painted + responsive (type/click): < 30s.**
- Stop when the targets hold **OR** every remaining lever's ROI has flattened (documented diminishing
  returns), with before/after numbers and a profile for each applied lever.

"Near-native" is the direction, not a literal bar — the wasm-in-V8 + per-syscall-RPC model has a
floor; the 10s/30s numbers are the contract.

## 3. Benchmarks — build FIRST, each emits ONE easy-to-measure number

Deterministic, repeatable, machine-readable (wall-ms to a defined milestone). Build before any
optimization so every lever has a before/after.

- **B0 — empty app.** A bare-minimum guest (no GTK/X; do nothing, exit). Measures the floor: instance
  init + the minimum syscall set. Isolates pure runtime startup overhead.
- **B1 — pure-GObject workload.** A guest that creates GObjects + emits signals/closures in a tight
  loop (no GTK/X/D-Bus). Isolates the `ffi_call`/GObject dispatch cost from everything else.
- **B2 — single real app.** Mousepad first-paint wall-time (the user-facing single-app number).
- **B3 — 5-app desktop.** Time-to-all-painted + responsive (xfwm4 + panel + mousepad + xfdesktop +
  thunar). Reuses the `INJECT`+XKB harness in `test-xu7-full.sh` for the responsiveness check.

A `make bench` / one script that runs B0-B3 and prints the four numbers is the loop's dashboard.

## 4. Profiler — build FIRST (default-OFF, env-gated), evidence before action

The per-cause split is currently a *hypothesis*. These two artifacts replace the guess with numbers:

- **P1 — sidecar per-method service-time accumulator.** Accumulate µs per RPC method on the service
  thread and dump a histogram at exit (**accumulate, do NOT `eprintln` per-call** — avoid the
  observer effect). Answers: which syscalls dominate the RPC wait, and total time-in-service-thread.
- **P1-guest — guest-side RPC time** (`SECURE_EXEC_RPCPROF`, default-OFF). Per-method (count,
  blocking-µs) of `callSyncRpc` measured *guest-side*, dumped every 20k calls with per-isolate
  attribution. The DELTA vs P1 (sidecar service time) = bridge/marshaling overhead; total guest-RPC-µs
  vs wall = round-trip-bound vs compute-bound. Uses the real clock **only** under the determinism
  guardrail in §7. (`crates/execution/src/node_import_cache.rs` `callSyncRpc`, `__rpcprof`.)
- **P2 — V8 CPU profile of the guest isolate.** Capture the isolate self-time split (wasm exec vs JS
  shims vs `ffi_call`/GObject vs encoders vs the RPC bridge). The *only* thing that captures the
  GObject/JS-side cost. **BUILD NOTE (verified 2026-06-27):** rusty_v8 v130 does **NOT** expose
  `v8::CpuProfiler` (no `cpu_profiler.rs`); only `inspector.rs` exists. So P2 must drive the **V8
  Inspector `Profiler` domain** (Profiler.enable / setSamplingInterval / start / stop over an
  `InspectorSession` in `crates/v8-runtime/`, then parse the returned CPU-profile JSON) — a substantial
  build, not a quick attach. Alternative cheaper-but-narrower probe: instrument the `ffi_call` host
  import directly (count + time, like P1-guest) to size GObject dispatch (L-B) without a full profile —
  but it needs the guest-side env gate fixed first (see below).
  - **Guest-env-gate gap (blocker for guest-side probes):** `SECURE_EXEC_*` debug vars set
    `globalThis.__rpcprof`/`__pollstat` in the runner (node_import_cache.rs ~8817), but they did NOT
    activate for **X-client** guests even after adding them to the host cenv allowlist + rebuilding —
    `__pollstat` never fired. The host-side `[rpcprof-host]` aggregator works (it reads the RPC stream
    host-side), which is why the RPC numbers exist. Fix the guest `process.env` plumbing for X clients
    (or lower the dump thresholds + add a startup "[gate] enabled" confirmation to diagnose) before
    relying on guest-side counters.

**The first number to get: service-thread-wait vs isolate-compute.** It decides RPC-bound vs
CPU-bound, which picks the first lever. Adding targeted logs to get more info is fine.

> **Determinism guardrail (verified):** profiling MUST NOT weaken guest determinism/isolation. The
> guest-facing clock stays frozen by default — `globalThis.performance.now() === 0`, and
> `Date`/`process.hrtime` are virtualized. The real monotonic clock (`originalPerformance`, captured
> once before the freeze) is **module-scope**, is **never placed on `globalThis`** or any
> guest-reachable object, and is bound **only** when the opt-in `SECURE_EXEC_RPCPROF` debug flag is set.
> So the guest cannot read a real clock by default; exposing one is a deliberate, debug-only opt-in.

## 5. Optimization loop (profile-guided, one lever at a time)

1. Run B0-B3 → record baselines (+ a profile).
2. Profile → identify the **single biggest cost**. *Never optimize without a measurement proving it.*
3. Implement the fix in **CORE secure-exec** (sidecar / kernel / runtime / bridge). Big + risky is
   fine if it's focused on secure-exec.
4. Re-measure the affected benchmark(s), before/after.
5. **Regression gate green** (§7) + Constraint #5 + never-self-approve check.
6. Record the lever + before/after + a PROVEN/REFUTED-style verdict in §6. Re-rank. Repeat.

## 6. Lever ledger (candidates — order is DATA-DRIVEN, not pre-committed)

Seeded from the architecture + the runtime-perf notes; profiling decides the real order. Append new
levers as profiling surfaces new costs (recursion).

- **L-B — Guest isolate compute (GObject `ffi_call` / wasm / JS shims). [TOP — profiled compute-bound]**
  Single-app startup is **96% isolate compute** (178s/185s); RPCs (~1s) + service thread (~5s) are
  noise. The dominant sub-cost — `ffi_call` (GObject dispatch) vs wasm exec vs the JS poll-loop
  machinery — needs **P2** to split. _status: TOP; P2 to break down (B1 isolates ffi_call)._
- **L-F — Main-loop busy-poll. [NEW, from baseline #1]** 640k poll iterations during startup;
  `net.poll_wait` returns immediately (~1µs, not blocking) = the glib loops spinning. Unknown how much
  of the 178s is loop machinery vs real GTK compute (P2 splits it). If it's a spin, fixing it (à la
  T-J) could be a big win. _status: OPEN; P2 to quantify._
- **L-A — Single kernel service thread serializes all RPCs.** Concurrency / multiplex. **DOWN for
  single-app** — the service thread is **97% idle**, NOT the single-app bottleneck (refuted by baseline
  #1). May still matter for the **5-app contention** case (the D-Bus-timeout ceiling). Risk: races
  across shared kernel state in a `#![forbid(unsafe_code)]` crate. _status: DEFERRED to the 5-app phase._
- **L-C — Per-RPC JSON/base64 marshaling.** Binary fast-path for hot ops (framebuffer already on the
  bulk-SAB path; generalize). _status: OPEN._
- **L-D — Redundant startup scans** (fontconfig / icon-theme / gschemas), re-done every boot, each a
  stat/readdir storm. Cache / eliminate — *only if there's no clear runtime win*, and keep it simple
  (no overfitting the desktop case). _status: OPEN._
- **L-E — Misc runtime hot spots** from the perf-comparison notes: 1ms busy-poll, per-arm timer
  threads, blocking console RPC, pure-JS encoders. _status: OPEN._

## 7. Constraints (hard)

- **Evidence-first.** No optimization without a profile/measurement showing the cost it removes.
- **Fix in CORE secure-exec, never the guest layer.** No boutique Xubuntu / glib / GTK / app-source
  modifications (Constraint #5). The default diagnosis for slowness is "the runtime is slow," fixed in
  the sidecar / kernel / runtime / bridge.
- **Caching is a fallback, not a crutch.** Allowed only when there's no clear runtime speedup; keep it
  simple, no overfitting the desktop workload.
- **Regression gate green every iteration.** A fixed smoke suite (the GUI render tests + the GWakeup
  probe + a conformance subset) must stay green after each lever.
- **Default-OFF diagnostics**, committed on `perf-pivot-work`, cataloged in `INTERNAL-TOOLING.md`.
- **Profiling never weakens guest determinism/isolation** (see the §4 guardrail): the real clock
  (`originalPerformance`) stays module-scope, never on `globalThis`/any guest-reachable object, bound
  only under the opt-in `SECURE_EXEC_RPCPROF` flag; the default guest clock stays frozen.
- **Never-self-approve** (require explicit sign-off): D-Bus-to-host, host-fd, GPU, host-network.

## 8. Completion bar (DONE only when ALL hold)

1. Benchmarks B0-B3 built + baselined (one number each, repeatable).
2. Profiler P1 + P2 built; the RPC-bound-vs-CPU-bound split measured (not assumed).
3. Targets met — single app < 10s, 5-app desktop < 30s painted + responsive — **OR** ROI flattened
   with documented diminishing returns per remaining lever.
4. Every applied lever has a **before/after number + a profile artifact**.
5. Regression gate green; Constraint #5 verified; recursion drained (no OPEN lever with clear ROI).

---

### Verdict log (newest first)

- **2026-06-27 — L-F (loop iterations), from existing host-side data.** The glib main loops iterate
  **~168k times in 185s (~900/sec — busy, NOT blocking)** (≈ the `net.poll_wait` count). With the 178s
  of compute that is **~1ms of compute per loop iteration** — so the 178s ≈ 168k iterations × ~1ms (the
  glib prepare/check/dispatch machinery in wasm + per-iteration work). **Whether each iteration is real
  GTK work or a non-progressing spin is exactly the P2 question.** Two attack surfaces: cut the
  per-iteration cost (~1ms is high for a glib iteration) and/or cut the iteration count (168k is high if
  spinning). NOTE: the guest-side `__pollstat`/`__rpcprof` gates do NOT activate for X clients
  (`SECURE_EXEC_*` debug vars don't reach X-client `process.env` even after the cenv allowlist add — a
  separate plumbing gap); not chased because `[rpcprof-host]` (host-side) already carries the per-isolate
  poll counts. **Next: P2 (V8 CpuProfiler)** — break the ~1ms/iteration into glib machinery vs
  `ffi_call`/GObject vs GTK work. Artifact: `/tmp/p2lite-mousepad.log`.

- **2026-06-27 — DECISIVE: single-app startup is COMPUTE-BOUND (the RPC-vs-CPU split, measured).**
  mousepad dual-profile (P1 sidecar + P1-guest, ~185s wall): **guest-side RPC blocking = 952ms** (640k
  RPCs, mostly ~1µs *in-process* `__agentOsSyncRpc.callSync` — NOT cross-process round-trips); sidecar
  service = 5313ms (mostly poll-wait). So **~178s (96%) is guest ISOLATE COMPUTE** (wasm / GObject
  `ffi_call` / JS shims), not RPCs and not the service thread. Most-expensive guest RPCs: a few
  `fs.readSync` at 15-33ms each (large reads, ~390ms) + `fs.statSync` ×1957 (80ms, icon scans) — ~600ms
  total, noise vs 178s. **Ledger re-rank (evidence-driven):** L-B (GObject `ffi_call`/compute) → **TOP**;
  L-A (service-thread multiplex) → DOWN for single-app (thread 97% idle; keep for 5-app contention);
  L-C (marshaling) → DOWN (RPCs ~1µs). **New candidate L-F: main-loop busy-poll** — 640k poll
  iterations, `net.poll_wait` returning immediately (~1µs, not blocking) = the glib loops spinning;
  unknown how much of the 178s is loop machinery vs real GTK compute. **Next: P2 (V8 CPU profile of the
  guest isolate)** — the only tool that splits the 178s into wasm exec vs `ffi_call`/GObject vs JS
  poll-loop machinery. Artifacts: `/tmp/p2lite-mousepad.log`, `/tmp/p1-mousepad.log`.

- **2026-06-27 — Baseline #1 (mousepad startup+run, ~185s wall, 3 guests, P1): the service thread is
  NOT the bottleneck.** 456,000 RPCs total; `total_service_ms` = **4985ms (~5s)** → the single kernel
  service thread is **~97% idle**. Startup is **NOT RPC-work-bound** (refutes the T-H "service thread
  saturated" hypothesis *for single-app startup*; T-H may still bite the 5-app case via contention).
  **~99.8% of RPCs are POLLING:** `__kernel_fd_poll` 146k (38%), `net.poll_wait` 168k (31%), `net.poll`
  98k (13%), `net.server_accept` 42k (12%, the dbus-daemon accept loop). Real work RPCs are tiny: 157
  `__kernel_fd_read`, 500 `net.write`, 6 `fs.*` — **~30ms total**. `net.poll_wait` avg **9.2µs** =
  immediate return → a **busy-poll signature** (polls keep finding something ready, guest re-polls).
  `net.listen`: 2 calls, 263ms (131ms each — slow setup, minor). **Verdict: the ~120s is ISOLATE-SIDE**
  — guest compute (GObject `ffi_call` / wasm / JS) and/or the round-trip overhead of 456k poll RPCs
  (at ~100-200µs guest round-trip each, the polls alone are ~45-90s). **Next: P2 — split
  guest-RPC-blocked-time vs guest-compute-time** (decides poll-reduction vs compute-optimization).
  Artifact: `/tmp/p1-mousepad.log`. (Caveat: includes dbus-daemon + xfconfd; ~42k polls are the dbus
  accept loop — per-pid attribution + a clean single-app B2 will sharpen this.)
