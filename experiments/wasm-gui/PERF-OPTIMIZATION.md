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
- **P2 — V8 CPU profile of the guest isolate.** Capture the isolate self-time split (wasm exec vs JS
  shims vs `ffi_call`/GObject vs encoders vs the RPC bridge). The *only* thing that captures the
  GObject/JS-side cost. (V8 Inspector / `--cpu-prof`-style; reuse the debugger seam if present.)

**The first number to get: service-thread-wait vs isolate-compute.** It decides RPC-bound vs
CPU-bound, which picks the first lever. Adding targeted logs to get more info is fine.

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

- **L-A — Single kernel service thread serializes all RPCs.** Concurrency / multiplex. Highest
  *suspected* value (the T-H ceiling, and the 5-app D-Bus-timeout cause). Risk: races across shared
  kernel state (VFS / socket / process tables) in a `#![forbid(unsafe_code)]` crate — needs careful
  locking / sharding. _status: OPEN, unprofiled._
- **L-B — GObject `ffi_call` dispatch via JS shim.** Per-call cost on every signal/closure; GTK init
  fires huge numbers. _status: OPEN, unprofiled (B1 isolates it)._
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

_(empty — seed Phase 0: build B0-B3 + P1/P2, then baseline.)_
