# Linux Divergence Ledger — Xubuntu desktop on secure-exec

Living tracker of every observed divergence from native Linux behavior, with its native baseline, the
faithful fix, and status. Under the load-bearing **"behave like native Linux"** invariant, each divergence
is a runtime bug to fix in core (`crates/{sidecar,execution,v8-runtime,kernel,bridge}`) — never a guest-side
workaround — unless it is one of the three documented isolation concessions (single-threaded execution of
`registry/native` commands, bounded memory/CPU, default-deny egress).

Detailed evidence: `DESKTOP-BOOT-PERF.md` (2026-07-03 equivalence-audit entries) + artifacts in
`~/progress/secure-exec/2026-07-03-native-equivalence-verification/`.

## Target (definition of done)

- **Determinism (ACHIEVED, hold it):** full desktop renders EVERY time — guest-fb coverage ≥40%, all
  components painted, ≥5 consecutive runs, ZERO total-black, via `scripts/measure-boot.sh` (FULL_MIN=40).
- **Performance:** deterministic full boot in **≤15s**, best-effort toward **≤10× native ≈ 5.4s**
  (native full-session baseline = 0.548s). NO regression of determinism to get there.
- **Equivalence:** the running desktop behaves like native Linux — **idles at ~0 host CPU**, and
  **concurrent app launch is reliable without the serial-settle crutch**.
- **Every performance win comes from closing a divergence below**, not from tuning the settle timer or any
  other guest-side crutch. The serial-settle / `SX_SERIAL_LAUNCH` path is a temporary determinism scaffold to
  be REMOVED once D2 lands, not a permanent knob.

## Status legend

`OPEN` unfixed · `WIP` in progress · `FIXED` code landed · `VERIFIED` re-audited native-equivalent + determinism held

## Divergence table

| ID | Divergence | Native baseline | secure-exec now | Verdict | Rank | Status |
|----|------------|-----------------|-----------------|---------|------|--------|
| **D2.1** | Blocking recv() on broker | native recv() blocks only its own thread | FIXED (gated SECURE_EXEC_RECV_OFFBROKER): off-broker net.poll_wait; det 5/5, net.poll freezes 400ms→0 | 1 | FIXED |
| **D1** | Idle busy-poll | `poll(-1)` parks at ~0 CPU (strace: 1 syscall, 3s, 0 wakeups) | 50ms clamp force-wakes idle guests → **0.56 idle cores** | faithfulness bug | **1** | OPEN |
| **D2** | Single-threaded host syscall broker | per-core parallel, fair (8 procs all `R`, ~1.5s CPU each) | ALL guest RPCs funnel 1 `&mut NativeSidecar` (current-thread tokio); co-boot HOL-starves → needs 12s settle | faithfulness bug (incidental, NOT the concession) | **1** | OPEN |
| **D3** | No-snapshot isolate per thread | `pthread_create` 10.5µs, shares address space | fresh no-snapshot V8 isolate per wasi-thread (~53 for 8 guests) | perf gap (semantics faithful) | 3 | OPEN |
| **D4** | wasm recompile per launch | `execve`: no recompile, shared text pages via page cache | recompiles wasm each launch, no cross-launch code cache | mechanism-only; **magnitude negligible** (Liftoff ~12ms; L-X measured null) | 4 (log-only) | OPEN |
| — | Thread semantics | pthreads: shared mem + futex | real OS threads + shared `WebAssembly.Memory` + `atomic.wait/notify` | **EQUIVALENT** | — | VERIFIED ✓ |

## Fix detail + code anchors

- **D1 — complete the notify-graph, then unclamp.** Guest requests only 1000ms for an infinite poll
  (`crates/execution/src/node_import_cache.rs:11922`); sidecar clamps to 50ms
  (`crates/sidecar/src/execution.rs:20738` `JAVASCRIPT_NET_POLL_MAX_WAIT`; clamp `:20787`; deadline `:21575`).
  The clamp is a safety net for the INCOMPLETE readiness notify-graph (comment `execution.rs:20734`): some
  readiness edges never call `notify()`, so idle guests only catch them by rescanning. Fix = make EVERY
  socket/pipe/listener/timer readiness edge call `notify()`, then let a guest `poll(-1)` block indefinitely on
  the `PollWaiterPool` (`state.rs:945`) and remove the 50ms clamp (keep only a coarse seconds-scale
  dispose/shutdown wake). Existing partial notifiers: accept `execution.rs:12962`, data `:13018`.
  **DoD:** idle desktop → `SIDECAR_IDLE_CPU_CORES` ≈ 0 (native ~0), determinism held.
- **D2 — parallelize the host syscall broker. [PROFILED 2026-07-03 — targets revised from the plan's guess.]**
  Per-method `[rpc-block]` histogram of a concurrent boot (env-tunable `SECURE_EXEC_RPC_BLOCK_US`, service.rs)
  shows the ACTUAL funnel freezers are: **(a) blocking `net.poll` — 8×~50ms**, a blocking socket `recv()`
  (node_import_cache.rs:12485-12490) that sleeps the 50ms clamp ON the dispatch task, freezing all guests; and
  **(b) `wasm.thread_spawn` — 18×~17ms**, worker-isolate bootstrap (`start_execution_with_net_drain`,
  execution.rs:5875-5894) run synchronously on `&mut self`. Socket data (net.write/read) never hit 2ms
  (plan Increment 1 REFUTED); `[select-block]` empty → framebuffer not a monopolizer (plan Increment 3
  DEPRIORITIZED). Revised increments: **(D2.1)** blocking `recv()` → defer via the proven off-broker
  `net.poll_wait` pattern the `net_poll` loop (node_import_cache.rs:11738) already uses (drain-0 → snapshot gen →
  poll_wait), so recv blocks only its own guest; **(D2.2)** `wasm.thread_spawn` → bootstrap the worker isolate
  off the dispatch task and/or via snapshot (overlaps D3); **(D2.3, wholesale)** per-VM sharded servicing for the
  sub-2ms long tail. Original architecture notes below still hold.
- **D2 (original architecture) — parallelize the host syscall broker.** Guest COMPUTE already runs parallel (isolate per host thread,
  `crates/v8-runtime/src/session.rs:300`). Only the HOST broker serializes: one `select!` loop
  (`crates/sidecar/src/stdio.rs:123` current-thread tokio, `:211-320`) holds `&mut NativeSidecar` and services
  every guest's `SyncRpcRequest` one at a time (`pump_process_events` → `execution.rs:3904`). In-code comment
  `crates/execution/src/javascript.rs:447` measures ~636µs of a ~742µs hop is just waiting to be picked up.
  Fix = fine-grained, per-subsystem, per-VM-sharded interior locking of kernel state (VFS / process table /
  socket table) — or a multi-threaded runtime over sharded state — extending the `InlineNetDrain`
  (`javascript.rs:456`) + `PollWaiterPool` pattern from the hot poll paths to the whole syscall surface.
  **DoD:** concurrent (non-serial) boot renders 5/5 FULL, zero-black → **delete the 12s settle / `SX_SERIAL_LAUNCH`**;
  target ~1-2s guest-visible convergence.
- **D3 — snapshot the worker isolates.** `create_isolate` uses `CreateParams::default()` (no snapshot,
  `crates/v8-runtime/src/isolate.rs:520`); worker path `create_isolate(None)` (`wasm_threads.rs:320`). A snapshot
  facility already exists and the main session isolate uses it (`session.rs:897`); extend it to the worker path.
  Compatible with `__threadMod` compiled-module reuse (module is a separate artifact). **DoD:** measurable drop
  in per-thread isolate-construction time; determinism held. Secondary — do after D1/D2.
- **D4 — cross-launch wasm code cache (log-only).** `new WebAssembly.Module` recompiles each launch
  (`node_import_cache.rs:9499`), no persisted code cache. Faithful fix = `WebAssembly.Module` serialize/deserialize
  cache keyed by module hash. But magnitude is ~12-14ms/app and repo experiment L-X already measured it ≈ null,
  so this is a faithfulness footnote, NOT a perf lever. Record only.

## Verify-equivalence protocol (run after EACH fix)

1. **Determinism gate first (non-negotiable):** `scripts/measure-boot.sh` with the serial scaffold still in
   place → must stay 5/5 FULL, zero-black (FULL_MIN=40). A fix that regresses determinism is reverted.
2. **Native-equivalence re-audit** for the touched divergence: re-establish the native baseline (strace / repro /
   `/proc` CPU) and compare the secure-exec behavior. Save artifacts to
   `~/progress/secure-exec/{date}-{slug}/`. Flip status to `VERIFIED` only when the measured behavior matches
   native within reason.
3. **Perf measure:** record `BOOT_MS` + `SIDECAR_IDLE_CPU_CORES` in `DESKTOP-BOOT-PERF.md`. For D2, the real
   test is CONCURRENT (non-serial) boot reliability + time, since D2's DoD is removing the settle.
4. Update this ledger's status column + commit to `wasm-gui-desktop`.

## Ordered work queue

1. **D2 — parallel host syscall broker.** Biggest lever: unlocks reliable concurrent boot and deletes the 12s
   settle (the ~60s → few-seconds win). Do first; it also relieves the dispatch pressure behind D1.
2. **D1 — notify-graph completion + unclamp.** Kills the 0.56-core idle spin → native-like idle. Complements D2
   (both are the single-broker + incomplete-delivery root).
3. **D3 — worker-isolate snapshot.** Secondary startup polish once D1/D2 land.
4. **D4 — record only.** No action unless a future measurement changes its magnitude.

> Re-run the 4-way native-equivalence audit (the 2026-07-03 subagent method) after the queue drains to catch any
> NEW divergence surfaced by the fixes, until the whole table reads VERIFIED and the perf target is met.
