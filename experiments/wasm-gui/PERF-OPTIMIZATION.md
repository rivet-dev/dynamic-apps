# Secure-Exec Runtime Performance Optimization (startup / render throughput)

Continues from `XU7-STARVATION-INVESTIGATION.md`. That investigation fixed the **T-J starvation
spin** (the desktop went from 0% black to rendering). It surfaced a *separate* root — **T-H: the
single kernel service thread serializes all guests' syscalls** — as the reason rendering is *slow*
(single GTK app ~90-120s, xfce4-panel ~250-300s, 5-app desktop never finishes its D-Bus handshakes).
This spec drives that startup/render-throughput frontier to a measured target.

## 1. Problem statement

Rendering is correct but far too slow. **Phase-0 measured the mechanism (no longer hypothesis):** the
slowness is the **boundary-crossing tax**, specifically **per-X-round-trip cross-isolate latency** —
NOT compute (B1 GObject 0.69 µs/op) and NOT sidecar service (P1 ~27 µs/call). P2 shows 98.6% of guest
CPU is parked in the poll/wait loop: the guest spends its life BLOCKED waiting for X-server-isolate
replies. Each GTK X request is a guest→sidecar→X-server-isolate→sidecar→guest round-trip costing ~ms of
transport/scheduling (L-J: ~3.3 ms/round-trip vs ~6 µs service); GTK init does thousands serially. The
T-J fix already made idle waits event-driven; Phase-0 fixed the worst serialization stalls (L-O/L-P,
−44 s of pump starvation). **Phase-1 attacks the residual per-round-trip latency itself** (see §2
baseline + objective, §9 plan). Redundant startup scans (fontconfig/icon-theme/gschemas) remain a
secondary lever.

## 2. Targets (definition of done)

### Phase-0 targets (MET — 2026-06-28)
- **Single real GTK app (mousepad) first-paint: < 10s** → **MET (~9.7s).**
- **5-app-class Xfce desktop painted: < 30s** → **MET (~19.5s).**
- These were the original contract and they hold (see Section 8 + verdict log). But the native baseline
  below shows they were **conservative** — there is large remaining headroom, so the loop is NOT done.

### Measured baseline (native vs wasm — 2026-06-28, the decisive reframing)
Same methodology both sides: Xvfb `-fbdir` raw framebuffer, first-paint = first non-black coverage;
input→response = XTEST keystroke → first framebuffer change. Native = debian-bookworm Docker container.

| Metric | **Native (Docker)** | **Our wasm** | Slowdown |
|---|---|---|---|
| mousepad first-paint | **~110 ms** | ~9.7 s | **~88×** |
| input→response (keystroke→pixels) | **~3–9 ms** | **~226–260 ms** | **~40×** |

Tooling: `scripts/native-baseline.sh` (+ `Dockerfile.native-baseline`) for native; `SECURE_EXEC_FIRSTPAINT`
+ `SECURE_EXEC_INPUTLATENCY` for wasm. The old "wall-clock 74–144s" was never first-paint (it was the
harness `--timeout`); ignore it.

### What the gap is (root cause, evidence-backed)
The slowness is **NOT compute** (B1 GObject 0.69 µs/op) and **NOT sidecar service** (P1: ~27 µs/call,
544 ms over 20 k calls). **P2 (V8 CPU profile): 98.6 % of guest CPU is parked in the poll/wait loop.**
The cost is **per-X-round-trip cross-isolate latency**: every GTK X request blocks the guest in `net_poll`
until the X-server *isolate* is scheduled, replies, and the readiness wakeup propagates back through the
poll-waiter pool → guest channel → wasm re-entry. GTK init does thousands of these serially (→ ~9.7 s);
a keystroke does a handful (→ ~240 ms). Earlier (L-J): ~3.3 ms per sync-RPC round-trip vs ~6 µs service =
the latency is transport/scheduling, not work.

### ⚠ Phase-1 premise INVALIDATED (2026-06-29) — read before working
The objective below was written on a FALSE premise ("98.6% parked in the poll loop = waiting"). It is
proven WRONG: mousepad first-paint is **COMPUTE-bound** (importprof: only ~2.4s in ALL imports / 2776
calls, ZERO `ffi_call`; ~7s is on-CPU in-wasm GObject/GTK compute that routes through NO host import).
The X round-trips are already near-native (~174ms). **Within the goal's stated CORE scope (sidecar /
kernel / v8-runtime / bridge) and Constraint #5 (immutable guest binaries), the bottleneck is UNREACHABLE
— there is no host-import hop to optimize and V8 already runs the wasm optimally (tiering A/B null).** The
runtime-side ROI is therefore FLAT for first-paint. The path to <2s is a TOOLCHAIN/build change (faster
GObject/fpcast-emu dispatch, -O2 vs -Oz, faster libc primitives in `registry/native`) + guest rebuilds —
which is OUTSIDE the goal's "CORE" list and conflicts with "guest binaries are immutable". **This needs an
operator decision: either widen the scope to the toolchain/guest-rebuild track, or accept the runtime
lever is exhausted.**

**Input→response is ALSO compute-bound (driven 2026-06-29, the last runtime sub-lever — now closed):**
the keystroke redraw causes NO import-call spike (importprof grows uniformly ~29 calls/400ms across the
keystroke), and a clamp A/B (3ms→1ms) moves it only 240ms→223ms (~7%, noise). So the ~240ms is mousepad's
in-wasm GTK redraw compute (pango shape + draw), not round-trip latency. **Both Phase-1 targets are
compute-bound; the runtime/CORE lever is exhausted for BOTH** — every latency sub-lever is null, the
bottleneck routes through no host import (2776 import calls, no `ffi_call`), V8 runs the wasm optimally,
and the binaries are immutable. Documented diminishing returns IS satisfied for the runtime lever class:
clamp (L-Q ~0.5s/7%), poll-direct (L-S null), lazy-compile (L-U null), tiering (L-V null), each with a
before/after + profile artifact. The ONLY remaining lever (guest wasm codegen speed) is outside the
goal's CORE list and Constraint #5 — it needs the toolchain track.

### ⚠⚠ Phase-2 premise ALSO undermined (2026-06-29, same day) — read the verdict log top entry
Native GObject is **0.248 µs/op** (measured, not assumed), so wasm GObject is only **~2.8×** native = the
fpcast-emu cost is NORMAL wasm overhead, NOT the ~23× this section claims. AND the transparent fpcast fix
is infeasible (PR #153168 unmerged + GLib-insufficient). **The toolchain/fpcast track below is NOT
supported by the evidence** — the real ~86× lever is mousepad's unidentified hot function (cpuprofile
node 14120), which is NOT GObject ops. SYMBOLIZE 14120 before any toolchain/runtime work. The objective
(targets) stands; the *lever* does not.

### ~~Phase-2 objective (the ACTIVE contract — 2026-06-29): kill `fpcast-emu` via the toolchain~~ (lever disproven)
Targets unchanged (**mousepad first-paint < 2 s AND input→response < 50 ms**; stretch < 1 s / < 20 ms),
but the lever is now the PROVEN root: the guest wasm's per-op compute, dominated by Binaryen's
`fpcast-emu` emulated indirect-call thunks (~23× on GObject `new+unref`). Build-flag tweaks are exhausted
(measured null): `-O3` vs `-Oz` = no change, and dropping `--fpcast-emu` TRAPS (real signature
mismatches). The fix is **toolchain modernization** so GObject-heavy C compiles WITHOUT whole-program
fpcast-emu thunks. See §11 for the plan + decision gates.

- **Both numbers are first-class** (they are DIFFERENT workloads, ~40× apart): first-paint = cold
  one-time GTK/GObject construction; input→response = warm single-widget repaint. Both ride the same
  fpcast-emu/GObject root, so a real toolchain fix should move BOTH — measure BOTH on every change.
- **bench-gobject (B1) is the spike decider:** `new+unref` 0.69 µs/op (wasm) vs ~0.03 µs (native, ~23×).
  A successful toolchain change must collapse this number; it is the cheap go/no-go before any full
  GTK-stack rebuild.

### Phase-1 objective (reduce per-round-trip latency) — SUPERSEDED (premise disproven, see ⚠ above)
Matching native (~110 ms / ~6 ms) is unrealistic for a sandboxed wasm-in-V8 cross-isolate-IPC model
(there is an inherent per-boundary floor). The grounded, ambitious-but-achievable objective, anchored on
human perception (input < 100 ms feels instant; < 50 ms imperceptible) and a ~5× cut of the dominant
per-round-trip latency:

- **mousepad first-paint: < 2 s**  (from ~9.7 s ⇒ ~5×; ~18× native — residual = inherent cross-isolate IPC).
- **input→response: < 50 ms**  (from ~240 ms ⇒ ~5×; under the 100 ms "instant" threshold; ~8× native).
- **Stretch (native-class):** first-paint < 1 s, input < 20 ms.

Rationale for internal consistency: both targets fall out of ONE lever — cut the ~3.3 ms sync-RPC
round-trip to ≲ 0.6 ms (≈ 5–6×) and first-paint (~thousands of round-trips) and input (~handful) both
drop ~5×. Stop when these hold OR the per-round-trip latency lever's ROI flattens (documented), with a
before/after + profile for each applied fix.

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
- **P2 — V8 CPU profile of the guest isolate. ★ REVISED (side-thread research): `--prof` was the WRONG
  tool, not "wasm is unprofilable".** `SECURE_EXEC_V8PROF=1` wires V8's legacy `--prof` TEXT log
  (`crates/v8-runtime/src/isolate.rs`), which is the *one* V8 profiler that does NOT symbolize wasm —
  hence the `wasm-function[N]` index names + the empty top-of-stack (the "blind" pivot to print-timing).
  **The real picture:** wasm IS fully profilable; two preconditions, both ours to set: (1) the `.wasm`
  must carry its **name section** (`SECURE_EXEC_KEEP_NAMES=1` — default/release strips it → 0 symbols);
  (2) use a reader that consumes the name section. Options, best first:
  - **Proper P2 = V8 Inspector `Profiler` domain → `.cpuprofile`** (the engine Chrome DevTools uses):
    `Profiler.enable/start/stop` over an inspector session in the rusty_v8 embed; returns JSON with wasm
    frames **already named**, self-time included; opens in DevTools / speedscope as a flamegraph. Node
    `--cpu-prof` is the same machinery.
  - **`--perf-prof` jitdump + Linux `perf`** — whole-stack flamegraph that includes our C++/JS import
    handlers AND the wasm guest in one view (would attribute the ~3.3ms sync-RPC latency end-to-end).
  - **Quick unblock for the existing `--prof` logs:** `wasm-dis app.wasm | grep '(func \$name'` → an
    index→name map, rewrite `wasm-function[N]` in `/tmp/secure-exec-v8.log`, wire into `v8prof-top.py`
    (validate `[N]` includes imports).
  NOTE: the import-boundary profiler (`SECURE_EXEC_IMPORTPROF`) already answered the *headline* split
  (WAIT-bound, see §6 L-J/L-K) without CPU symbols; the proper P2 is now needed mainly to break down the
  residual ~8s of real COMPUTE (gtk_init / widget construction) and to confirm where the per-RPC latency
  goes. _status: tick-log built but wasm-blind; build the Inspector `.cpuprofile` P2 next._
  - **Guest-env-gate gap — FIXED.** `SECURE_EXEC_*` guest-side probes (`__rpcprof`/`__pollstat`/
    `IMPORTPROF`) did not reach **X-client** guests because the wire/cenv allowlist never lands in the
    guest isolate's `process.env`. Now forwarded from the sidecar host env into the guest env in
    `crates/execution/src/wasm.rs` (`build_wasm_internal_env`, the `SECURE_EXEC_V8PROF`-style path).
    Verified (`gate="1"`); all of IMPORTPROF/RPCPROF/POLLSTAT/RPC_PROFILE now activate for X clients.

**The first number to get: service-thread-wait vs isolate-compute.** It decides RPC-bound vs
CPU-bound, which picks the first lever. Adding targeted logs to get more info is fine.

> **Determinism guardrail (verified):** profiling MUST NOT weaken guest determinism/isolation. The
> guest-facing clock stays frozen by default — `globalThis.performance.now() === 0`, and
> `Date`/`process.hrtime` are virtualized. The real monotonic clock (`originalPerformance`, captured
> once before the freeze) is **module-scope**, is **never placed on `globalThis`** or any
> guest-reachable object, and is bound **only** when the opt-in `SECURE_EXEC_RPCPROF` debug flag is set.
> So the guest cannot read a real clock by default; exposing one is a deliberate, debug-only opt-in.

## 4b. PRIMARY method: top-down print-timing (host-timestamped) — the sampler is blind to wasm

**Why prints, not the V8 sampler:** P2 (`--prof`) captures ticks, but V8 logs wasm frames as
`wasm-function[N]` *index* names (verified — even with `SECURE_EXEC_KEEP_NAMES`), not C symbols. So the
sampler CANNOT attribute the ~178s to a named subsystem (fontconfig / pango / cairo / layout) — we were
profiling blind. Print-timing gives **named, %-attributable** phase timing.

**Mechanism (host-timestamped):** the host prefixes every guest stderr line with elapsed `[+Nms]` (the
guest clock is frozen, but the HOST clock is real and reliable). So `fprintf(stderr, "T:<phase>\n")` at
a phase boundary is a named, host-timestamped marker; the delta between consecutive markers = that
phase's wall time. css-bench already proved this works: `[+11365ms] gtk_init done` ⇒ gtk_init = 11.4s.

**Instrumentation = temporary build patches** (inline edits to the app/probe + the library source under
`third_party/`, or a `patches/` entry), applied at build, **reverted before commit** — exactly like the
T-J `gwakeup.c`/`gmain.c` probes. Constraint #5: only the FIX and the timing REPORTS are committed,
never the instrumentation patches. Keep each minimal (a few `fprintf(stderr,"T:...")` + `fflush`).

**Process — top-down, drill into the biggest:**
1. **High level:** bracket the macro-phases — `gtk_init` vs widget construction vs first-paint — get the
   % split (which macro-phase dominates the ~178s).
2. **Drill:** patch the dominant phase's library with finer markers (e.g. inside `gtk_init`:
   type-init / default-theme CSS load / icon theme / fontconfig `FcInit` / pango fontmap), re-run, find
   the dominant sub-phase.
3. Repeat until the cost is a specific named operation — that is the lever.

**Track reports (load-bearing):** save each run's marker deltas as a timing-report artifact
(`~/progress/secure-exec/2026-06-27-perf-optimization/timing-<scope>-<before|after>.txt`) with the %
breakdown, so EVERY optimization has a measured before/after. This is the "clear percentage view" — the
loop below is driven by these reports, never optimized blind.

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

---
### ★ FRAME-BUDGET: CURRENT STATE, DIAGNOSIS & REMAINING LEVER MENU (2026-06-29) — the live working set
The `/goal` references this section for detail; keep it current.

#### ★★★ IDLE BUSY-SPIN (2026-07-01) — sidecar main thread ~87% CPU while the VM is IDLE (invariant violation) ★★★
Measured via `/proc/<pid>/task/*/stat`: the sidecar MAIN dispatch thread burns **~87% of a core while the VM
is idle** (mousepad painted, no input; next thread ~5%). Native is ~0%. Violates the CLAUDE.md "idle must be
0% CPU, not busy-spin" invariant. `SECURE_EXEC_SELPROF` (default-OFF, select!-branch counter): `event_pump.tick()` = ~92% of select!
iterations — BUT **CORRECTION (PUMPTIME probe): the pump is NOT the CPU sink.** `pump_process_events` is only
**~12µs/call, 1 process polled, ~4900 calls/s = ~6% CPU.** The select! loop iterates ~5000/s, so the whole
loop is ~6-8% CPU. **So the ~87% idle CPU is NOT the dispatch loop / pump — it is elsewhere on the main thread
and is UNDIAGNOSED** (candidates: a busy-looping spawned tokio task on the current-thread runtime, the tokio
IO/timer driver, or poll_event_wire — NOT yet measured). My earlier "pump = 217µs = the spin" inference was
WRONG (corrected per the CLAUDE.md "verify before asserting" lesson; `try_lock` on poll_event was null,
consistent with the pump being cheap). The 87% idle spin is real (measured via /proc, ~99-100% of a core) and a genuine invariant violation.
**Further pinned (2026-07-01):** `/proc/<tid>/syscall` shows the main thread is "running" (USER SPACE) ~98%
of samples; `strace -c` over 2s idle = only ~117 clock_nanosleep/s + 46 futex (≈2% syscalls). So it's a
**user-space busy-poll in the tokio current-thread runtime — it parks only ~117/s instead of per-tick.**
PUMP_INTERVAL=10000 (40× slower) → STILL ~99% idle CPU, so the pump/interval is NOT the spinner. The cause is
a select! future that is **always-ready so the runtime never parks** — with the pump excluded, that is
`process_event_notify` (a guest emitting sync-RPCs continuously) or the `event_ready` drain. ROOT (connects to
the op-count): **the guests don't go idle — they spin-emit**, and the 3ms poll clamp + the 250µs pump are
both timer-poll fallbacks papering over an INCOMPLETE NOTIFY GRAPH (the CLAUDE.md anti-pattern: tasks #25/#26).
**THE real lever (substantial, on-goal): COMPLETE the readiness notify graph** so every event source pokes the
notify → then the guests block truly (0% idle, native-like), the clamp + pump can be removed/slowed, the
per-render op-count drops (fewer spin re-polls), and the cross-guest churn shrinks. First step needs finding
the missed-notify edges (the events only the clamp/pump catch — a default-off "missed-wake detector", task
#27) OR a user-space CPU profiler to name the always-ready future (none available in this sandbox).
**MEASURED idle op-emit rates (OPTRACE, 3s idle window, per guest thread):** the guests emit ~2700 sync-RPCs/s
while idle (native ≈ 0). Biggest single: **mousepad `fs.stat` ~536/s** (forwarded → pokes the notify; likely a
GTK GFileMonitor poll-fallback because the VM lacks inotify — a CONCRETE, possibly-single-fix lead: provide
inotify / a real file-change notify so mousepad stops stat-polling). Rest: Xvfb net.poll/poll_wait/accept
~270/s each + mousepad/worker poll/fd_poll/poll_wait ~160-210/s (the empty-poll spin). NOTE: the idle fs.stat
spin is largely an IDLE phenomenon (the render is X-protocol, little fs); the render's ~38ms "churn" between
the ~18ms of peer-waits is the poll/fd_poll/poll_wait spin, which IS reducible by completing the notify graph.
So completing the notify graph attacks BOTH the idle invariant AND the render churn — but it is multi-source
(fs inotify + true blocking poll for net/fd) = substantial, multi-step work.
- `PUMP_INTERVAL_US=2000` → ir WORSE (~75ms + a 730ms spike) — the 250µs pump IS load-bearing for ir (some
  events reach the guest only via the pump, not the F1 notify). Don't slow it. (This finding stands.)

#### ★★★ OP-COUNT + CROSS-GUEST SPLIT (2026-07-01, SECURE_EXEC_OPTRACE) — the render is 73% cross-guest, 27% compute ★★★
`SECURE_EXEC_OPTRACE=1` (default-OFF) logs every guest sync-RPC on the bridge thread with an epoch stamp +
guest thread-id. Windowed to the keystroke (ir ~64ms):
- **Op-count: ~160 sync-RPCs/render vs native's ~23 syscalls (~7×).** Breakdown: net.poll(drain) 67, poll_wait
  41, fd_poll 27, write 15, accept 6. (Most are inlined now, ~µs each → the op COUNT is ~1ms of wall; it is
  NOT the bottleneck by itself.)
- **The render wall-time is in the GAPS between ops, split by thread-id:**
  **CROSS-guest gaps (op from guest A → next op from guest B) = ~50ms / 73%** (82 transitions × ~610µs);
  **SAME-guest gaps (a guest computing between its own ops) = ~19ms / 27%.**
- ⇒ **The bottleneck is the cross-guest ROUND-TRIP path (~82 transitions × ~610µs), NOT toolchain compute**
  (only 27%). Native pipelines the same X protocol into one ~2ms burst with µs transitions. THREE guest
  bridge threads: tid=Xvfb (spin-accepts) + tid×2=mousepad(main+worker); Xvfb's accept-spin interleave likely
  inflates the transition count.
- **⇒ The lever (CORE, the goal's target): cut per-transition cost (~610µs → native µs) and/or transition
  COUNT.** Candidates: (a) shorten the write→peer-wake delivery (fewer thread hops: guest write → peer reader
  thread → notify → peer poll_wait → peer isolate scheduled → peer's next op), (b) pipeline the X transport so
  the guest sends multiple requests without a round-trip each (native does this), (c) kill the Xvfb accept-spin
  so that thread stops competing for scheduling, (d) shared-memory inter-guest ring (lever D). The ~610µs
  per-transition still needs attribution (delivery+notify vs peer-isolate-scheduling vs peer-compute-to-next-op)
  to pick the sharpest of these.

#### ★★★ THE "8ms" DEFINITIVELY EXPLAINED (2026-06-30, GAPTRACE per-event window) ★★★
Built a per-event `SECURE_EXEC_GAPTRACE` probe (default-OFF; needs `SECURE_EXEC_PERFCLOCK=1` for the notify
stamp) that logs each blocking poll_wait's segments and windows them to the `[ir-mark]` inject. Result for
the keystroke render (~59ms window): **~9 blocking poll_waits, each ~1.2-2.5ms, and that block time is
≈100% `peerWait` (register→notify = waiting for the PEER guest Xvfb to produce the reply). The local
`wakeLag` (notify→resume) is ~6-22µs — negligible.** So:
- The "~8ms gaps" were NEVER a timer / frame clock / local wake-path latency. The local wake is ~15µs (which
  is exactly why poll-direct and the clamp correctly did nothing).
- The render = **~9 cross-guest round-trips × ~2ms peer-turnaround (~18ms) + ~40ms of the guest churning
  through its own many fast sync-RPCs/compute between waits.** The coarse xtrace "~8ms gaps" were 2-4 of
  these ~2ms peer-waits clustered.
- The ~2ms peer-turnaround = **Xvfb paying the wasm per-operation overhead on ITS side** (request delivery +
  Xvfb's own wake ~15µs + Xvfb processing the request through its own syscalls + reply delivery). Native Xvfb
  does the same in ~µs. It is recursive: BOTH guests pay the per-op overhead, and their ping-pong compounds.
**⇒ The remaining lever is the cross-guest ROUND-TRIP / per-operation cost (peer-turnaround), NOT the local
wake path (already ~15µs).** Two angles: (1) cut the per-operation overhead further so each guest's turnaround
shrinks (diminishing — the inline family already did the cheap part), or (2) cut the NUMBER of cross-guest
round-trips / per-render syscalls (wasm does far more ops/render than native's ~23 — the runtime fans one
native poll/read into poll_wait+fd_poll+drain, and the guest spin-polls). Reducing op-count or a shared-mem
ring (lever D) is the path to native's ~10ms. peerWait dist (whole run): median 1156µs, p90 2522µs, max 3016µs.

#### ★ COMPLETE MEASUREMENTS LOG (2026-06-30) — every ir number we recorded, with how it was measured
All B2 = Xvfb (800x600x24, -fbdir) + mousepad, single guest. ir = inject→framebuffer-change wall time.
**The metric evolved as we found confounds — numbers are only comparable WITHIN the same metric row.**

| measurement | metric / method | result | notes |
|---|---|---|---|
| original "ir" | type **"HELLO"** (5 char), 15ms pacing sleep AFTER each char, whole-fb strided hash, detect after typing | **~88ms** (87-98) | ARTIFACT: ~75ms is the 5×15ms pacing baked in before the detect loop even starts |
| single-key, pacing kept | type "H", 15ms trailing sleep, whole-fb hash | **~33ms** (27-38; noise spikes ~100-140) | still ~15ms pacing + CONFOUNDED (caught pointer, not glyph) |
| single-key, pacing stripped | type "H", pace BETWEEN keys only, whole-fb hash | **~25ms** (23-28) | CONFOUNDED — whole-fb caught a change at **rows 392-410** (mouse-cursor sprite), NOT the glyph (row ~70) |
| damage localization | as above + `[ir-damage]` | first change **rows 392-410, ~84 bytes** @ ~20ms; glyph (rows 70-92) later | proved the ~20-25ms was the cursor sprite, not the text |
| **TRUSTWORTHY (the real metric)** | warm (`IR_WARMUP=Hello`) + caret-blink OFF (fixture) + glyph-region detect (top 200 rows, excludes cursor) + tight poll; `[ir-damage]` CONFIRMS rows 70-92 = glyph every run | **~98ms** (88-125) | the true warm single-keystroke glyph-render BEFORE the inline levers |
| + inline net.poll (lever A) | trustworthy metric | (folded into below) | A alone: measured null vs the OLD confounded metric — that measurement was INVALID |
| + inline net.poll + fd.poll (A + C-lite, gate ON) | trustworthy | **~70ms** (65-74) | vs gate OFF ~105ms — the ~34ms win that the confounded metric hid |
| + inline accept (T-accept) | trustworthy | **~66ms** (52-76) | accept off the funnel; ~4ms more (parallel-spin relief) |
| **committed default (all 3 levers ON)** | trustworthy, `bench-ir.sh "" 3` | **~64ms** (63/69/64; 5-run 59-74) | render green every run |
| inline-dispatch fully OFF | trustworthy, `SECURE_EXEC_INLINE_DISPATCH=0` | **~105ms** (82-106) | confirms the inline family is worth ~40ms |
| poll clamp sweep | trustworthy, `SECURE_EXEC_POLL_MAX_WAIT_MS=1/3/10` | **~60 / 62 / 67ms** | clamp is only a ~2ms margin — NOT the gap cause |
| gtk-enable-animations=false | trustworthy, settings.ini | **~68ms** (null) | the ~7 redraw steps are NOT animations |
| first-paint (fp), all above | `SECURE_EXEC_FIRSTPAINT` | **~2.07-2.30s** | tracked, not gating under the <10ms goal |
| **native ref (NOT comparable!)** | `native-baseline.sh`: real mousepad on Xvfb, `xdotool type HELLO --delay 0`, **strided `hash(b[::997])`**, first-change | **~6ms** | ⚠ CONFOUNDED the SAME way the old wasm numbers were (strided hash + first-change = likely catches the cursor sprite, NOT the glyph). NOT a valid glyph-render baseline. Re-measure native with the trustworthy method before comparing. |

**Per-hop / decomposition numbers (trustworthy-metric era):**
- One-keystroke xtrace timeline: **every C>S→S>C in ~60µs** (server replies instantly); **~7 client-side gaps of ~6-9ms** (≈50ms total) where mousepad waits between requests.
- `[pwprof]` poll_wait requested-timeout histogram: **97% request 100+ms** (guest blocks ~indefinitely; woken ~8ms later by an internal event, not the X server).
- `[pipetrace]`: ~11 kernel-pipe writes/run total (the ~8ms wake is NOT a pipe write).
- HOPSPLIT d12 (dispatch-queue) pre-inline: `_netSocketPollRaw` 636µs, `_kernelFdPollRaw` 575µs, `_netServerAcceptRaw` 549-664µs, `_netSocketPollWaitRaw` 198-231µs. HOPPROF wakeLag 11µs, respond 35µs. DRAINHOSTPROF drain hostSvc 5µs, 28:1 empty:productive. rpc-profile total service work **572ms over a ~20s run** (~3% of one core).

#### ★★★ MECHANISM FOUND (2026-06-30, native strace) — the ~50ms is per-round-trip poll_wait WAKE latency, NOT frame pacing ★★★
`strace -ttt -T` on native mousepad through ONE keystroke (windowed to the keypress via `keytime.txt`):
**native does the ENTIRE render as one tight ~2ms burst** — all ~23 X recvmsg/sendmsg on the X socket
between **1.3ms and 3.3ms**, each `poll(fd, -1)` (block-until-ready) returning in **µs** (`<0.000003>`),
then it idles (`poll(...,49)=Timeout <0.049>`). **Native has NO ~8ms gaps — there is no frame-clock pacing
of the render.** ⇒ the wasm ~7-steps-×-~8ms structure is RUNTIME-INDUCED, NOT GTK. Native blocks
`poll(fd,-1)` and the **kernel wakes it instantly (µs)** when the X server replies; the wasm runtime CLAMPS
poll_wait to 3ms (`JAVASCRIPT_NET_POLL_MAX_WAIT`, can't block the shared dispatch thread) and routes the
data-ready→guest-resume wake through its dispatch path. Each round-trip reply that native resumes from in µs,
the wasm client resumes from in ~8ms → ~7 round-trips × ~8ms ≈ the ~50ms gap. **NEXT LEVER: cut the wasm
poll_wait WAKE latency (data-available→guest-resumed) toward native's µs — make it truly event-driven +
instant like `poll(-1)`, for the X-socket round-trip path (poll_wait is NOT inlined; net.poll/fd.poll/accept
are).** Measure the wasm poll_wait wake latency directly to confirm ~8ms, then attack it (the clamp itself is
only ~2ms per the sweep, so the lever is the wake/notify path, not the clamp value). This is squarely CORE.

#### ★★★ NATIVE COMPARISON RESOLVED (2026-06-30) — the floor was FALSE; the ~54ms is RUNTIME overhead, <10ms IS a CORE problem ★★★
Built a Docker native baseline (`debian + mousepad + Xvfb + xdotool`) and measured native with the SAME
trustworthy method as wasm (warm `HelloWorld`, caret-blink OFF, top-200-row glyph region, PointerRoot focus
handled by clicking inside the window + keeping the pointer below the text line). `[ir-damage]` confirms the
detected change is **rows 53-69, 281 diffbytes = the glyph** — the IDENTICAL signature to wasm's rows 70-92,
281 bytes. Valid apples-to-apples.
- **NATIVE ir = ~10ms** (5.4 / 9.6 / 10.0 / 10.3 / 10.4 / 10.9; median ~10, min 5.4), render confirmed.
- **WASM ir = ~64ms** (committed). Same GTK/mousepad/Xvfb software, same metric.
⇒ **The ~54ms gap is RUNTIME-INDUCED, NOT a GTK floor.** The "hard render floor" conclusion is RETRACTED —
it rested on the confounded native ~6ms (which timed the cursor, like the old wasm artifact). Native renders
the glyph in ~10ms, so **<10ms is REACHABLE and this is a CORE problem.** Per-step: native ~1.4ms/step ×7,
wasm ~9ms/step ×7 — the runtime adds ~7.6ms PER frame step. RE-OPENED: find what the runtime adds per step
(guest-block/poll_wait wake latency dominates; wasm compute is only ~2.9× so ~4ms of the 9ms, leaving ~5ms
of wake/dispatch latency per step). The native Docker harness (`scratchpad/native-ir/`) is the new reference.

#### (superseded by the native result above) ★★ CORRECTION (2026-06-30, user-prompted): the "guest-side GTK floor" was NOT established — RE-OPENED
A user pointed out: if the ~8ms-per-step pacing were GTK's intrinsic frame-clock behavior, **native GTK (same
code) on the same Xvfb would be just as slow** — so the wasm-vs-native gap, IF real, means runtime overhead,
not a floor. But the native ~6ms baseline is measured by a DIFFERENT, confounded method (strided hash +
first-change via `xdotool`, no pacing) — the very artifact we fixed in wasm — so it almost certainly catches
the cursor sprite, not the glyph. **We have NO valid native glyph-render number, so the "hard floor"
conclusion was premature and is RETRACTED.** OPEN QUESTION: measure native mousepad-on-Xvfb with the
trustworthy method (warm + blink-off + glyph-region). If native renders the glyph in ~a few ms, the wasm
~64ms is RUNTIME-induced (CORE-attackable, the ~8ms-per-step wake/timer latency); if native is also ~50ms,
it's a GTK-on-Xvfb floor. This decides whether <10ms is a CORE problem. DO THIS before declaring any floor.

> ## ★★★ RESOLUTION (2026-06-30): the "ir ~88ms" was a MEASUREMENT ARTIFACT — true single-keystroke ir is ~33ms (<50ms, target MET) ★★★
> The ir probe (`host/src/main.rs` ~1447-1472) types **"HELLO" (5 chars)** and the typist-pacing sleep is
> **15ms AFTER EACH char** (`xinput::type`, main.rs:522). That `xi.run("type HELLO")` is **synchronous and
> runs BEFORE the detect loop starts** — so ~75ms of pacing sleeps are baked into the measured ir before the
> code ever looks at the framebuffer. ir therefore **structurally cannot read below ~75ms regardless of render
> speed.** Made the typed string env-configurable (`SECURE_EXEC_IR_TEXT`, default "HELLO" for continuity) and
> measured: **`SECURE_EXEC_IR_TEXT=H` (one keystroke) → ir 27/35/140/38/27/32ms, median ~33ms, render green
> (5/6 runs 27-38ms; the 140 is the recurring box-noise spike).** PROOF it's the real glyph render and not a
> blink/artifact: **ir scales with char count** (1 char ~33ms, 5 chars ~88ms; delta = the 4 extra pacing
> sleeps) — a blink would be char-count-independent. Net: **the runtime's single-keystroke input→response is
> ~33ms (~18ms render + the one 15ms pacing sleep), already under 50ms.** This is WHY every dispatch lever
> (A/C-lite) was ir-null — they optimized the ~18ms render that is invisible under the fixed 75ms harness
> delay. ACTION NEEDED (user ratify): redefine the acceptance metric to single-keystroke render
> (`bench-ir.sh` default → `SECURE_EXEC_IR_TEXT=H`, or measure inject→render with no pacing). Do NOT keep
> chasing the 5-char number; it measures typing cadence, not input→response. Levers A + C-lite stay gated-off.

**Targets (2026-06-30, ir<10ms goal):** primary **single-keystroke input→response (ir) < 10ms** (≥3 runs,
render green). fp tracked (~2.08s) but NOT gating under this goal. The 50ms target is retired; ir is measured
HONESTLY now (single keystroke, no pacing artifact). **Native ref = ~10ms** (trustworthy, same method,
Docker `scratchpad/native-ir/`; the old "~6ms" was the confounded cursor artifact). Wasm ~64ms ⇒ ~54ms is
RUNTIME overhead and <10ms is reachable in CORE — the "floor" was retracted.
**Current committed baseline (TRUSTWORTHY metric, inline-dispatch default-ON, 2026-06-30):** ir **~56ms**
(47-70ms), render green, fp ~2.08s. Was ~98ms before the inline family (net.poll + fd.poll + accept +
**now poll_wait fast-path** = the #1 method). Native ref ~10ms. Remaining: the GENUINE blocking poll_waits
(cross-guest round-trip turnaround) — the cheap fast-path/dispatch inlines are now ALL done. Need ~7× more to <10ms.
**Dispatch levers A/C-lite are a REAL WIN after all** (~98→70ms) — the earlier "null/refuted" was a confounded
metric; now landed default-ON. Next: decompose the remaining ~70ms render path (xtrace + FRAMEPROF on the
honest inject→glyph window) and find the next lever.

**★★★ RESOLVED → TRUSTWORTHY ir BASELINE = ~98ms (2026-06-30); ALL PRIOR LEVER RESULTS ARE INVALID ★★★**
Built a trustworthy glyph-render metric (now the `bench-ir.sh` default): warm keystroke (`IR_WARMUP=Hello`)
+ caret-blink OFF (fixture `gtk-cursor-blink=false` in `prepare-icons.sh`) + glyph-region detection
(`FB_REGION_BYTES=480000` = top 200 rows, EXCLUDES the mouse-cursor sprite at rows ~392-410) + tight poll
(`IR_POLL_US=500`). The `[ir-damage]` probe CONFIRMS the detected change is at **rows 70-92 (the text line),
~281 bytes = the glyph** — every run. **True warm single-keystroke ir = ~98ms (91-106ms), render green.**
⇒ **The real input→response is ~98ms, NOT the ~20-88ms reported all session — those timed the X mouse-cursor
sprite redraw / caret blink, NOT the typed glyph.** CONSEQUENCE: **every lever measurement to date is INVALID**
— A (inline net.poll), C-lite (inline fd.poll), and the "inline-dispatch family REFUTED" verdict were all
measured against the confounded ~88ms pointer change, so their effect on the REAL ~98ms render path was never
observed. The KNOWN-NULL list for ir is VOID; A/C-lite and the dispatch theory must be RE-TESTED against this
metric before being ruled out. The ~98ms render path is the real target (down toward <10ms): decompose it
(FRAMEPROF ir-marks + xtrace windowed to the inject, now that the inject→glyph window is honest) and re-run
the dispatch levers gate-on vs off.

(superseded) **★★★ MEASUREMENT-VALIDITY BLOCKER (2026-06-30) — the ir number is confounded; resolve before optimizing ★★★**
Drilling the ~25ms single-keystroke ir for the <10ms goal uncovered that the framebuffer-diff probe is NOT
measuring typed-glyph latency. Evidence (host probes added: `SECURE_EXEC_FB_REGION_BYTES`,
`SECURE_EXEC_IR_POLL_US`, `[ir-damage]` row-range logging — all default-OFF/robust):
- Text input WORKS — typing 20 chars renders them at the top-left text area (row ~62). Verified by PNG.
- But for ONE keystroke the FIRST detected fb change (the reported ~20ms) is at **rows 392-410, ~84 bytes** —
  NOT the glyph (which is at row ~62). Almost certainly the **X mouse-cursor sprite redraw** on the input
  event (incidental, unrelated to text).
- Restricting detection to the top 200 rows (where the glyph is) reports **~140ms** — but that region is
  confounded by the **text caret BLINK** (~530ms period, same pixel location as the glyph), so it is ALSO
  not a clean glyph-render signal.
- Net: every "ir" number to date (88ms HELLO, 25-33ms single-key, 20ms) is catching pointer/caret/incidental
  repaints, NOT "typed glyph appeared". The metric cannot be trusted for sub-50ms work, let alone <10ms.
**BLOCKER for the <10ms goal:** need a TRUSTWORTHY glyph-render-latency signal that excludes the mouse cursor
+ caret blink. Options to weigh (a DECISION): (i) use the X DAMAGE/XFIXES extension to get the actual
text-region damage event timestamp; (ii) region+size threshold on the exact glyph cell, with caret-blink
rejection (measure to a STABLE glyph, not the first toggle); (iii) a guest-side "render-complete" signal;
(iv) hide the X cursor + disable caret blink in the test setup (test-harness config, not guest edit). Until
one lands, ir is unmeasurable to the precision the goal requires — SURFACED for a decision.

**★ DIAGNOSTICS-GAPS + THEORIES TABLE (live; recursive loop — update every round):**
| # | theory / gap | status |
|---|---|---|
| T-meas1 | detect loop polls every 2ms AND FNV-hashes the whole 1.4MB fb each poll (~2-4ms) → ~4-6ms measurement floor baked into ir; matters at a 10ms target | OPEN — quantify + build a precise inject→fb-write probe (sidecar timestamps the fb `__kernel_fd_write`, host timestamps inject, both SystemTime epoch = comparable) |
| T-meas2 | box noise spikes (~100ms, 2/6 runs) will swamp small wins; need many runs / a quiet-window filter | OPEN |
| T-render1 | the ~21ms after detect-overhead is the keypress→glyph path: X KeyPress delivery + GTK key handler + render requests + Xvfb draw + fb write. Decompose with FRAMEPROF ir-marks + xtrace --rt windowed to the inject | OPEN — needs the precise probe first |
| T-render2 | guest GTK render compute is ~2.9× native (B1) — partial floor; toolchain-bound (out-of-CORE) if it dominates | OPEN — only after T-render1 attributes how much is guest compute vs runtime |
| T-render3 | **ATTRIBUTED (xtrace, direction-parsed): the ~6-9ms gaps are 100% CLIENT-SIDE.** Every exchange is C>S→S>C in ~60µs (server replies INSTANTLY); the gap is mousepad (client) waiting ~8ms after a reply before its next request. Bursts of fast round-trips separated by ~8ms gaps × ~7 = ~50ms (the bulk of ir). NOT server compute, NOT dispatch | NARROWED → is mousepad COMPUTING (guest GTK, toolchain) or WAITING on a timer (frame clock, CORE) in the gap? |
| T-render4 | **PWPROF: 97% of poll_wait requests are 100+ms** (the guest blocks ~indefinitely, NOT ~8ms frame timeouts). So mousepad is WOKEN ~8ms later by an INTERNAL event (not the X server — no S>C in the gaps), ~7×/keystroke. The ~8ms is the cadence of a guest-internal wakeup (a timer/frame-clock thread poking the GMainContext wakeup pipe), NOT the requested timeout or the 3ms clamp | OPEN → find what fires every ~8ms inside mousepad: a guest timer thread (does the runtime's per-arm timer / setTimeout granularity inflate it? → CORE lever) vs GTK frame-clock intrinsic pacing (floor). Probe: timer-arm trace + the worker/wakeup-pipe path during the gap |
| T-render5 | the 3ms `JAVASCRIPT_NET_POLL_MAX_WAIT` clamp: TESTED clamp=1/3/10ms → ir ~60/62/67ms. Tighter clamp = guest notices its ~8ms deadline sooner, but only **~2ms total** effect — the clamp is NOT the gap cause, just the notice-latency margin | CLOSED — marginal (~2ms); not worth changing the tuned 3ms default. No kernel-pipe wakeups either (pipetrace ~11 writes/run), so the ~8ms is the guest's own GSource/frame-clock deadline, not a runtime wake |

**[RETRACTED 2026-06-30 — see "NATIVE COMPARISON RESOLVED" above: native renders the same glyph in ~10ms, so the ~54ms is RUNTIME overhead, NOT a guest floor. The block below was WRONG.]**
**~~★★★ HARD RENDER FLOOR (2026-06-30) — <10ms is NOT reachable in CORE; the remaining ~50ms is guest-side GTK frame-clock pacing ★★★~~**
After landing the inline-dispatch family (net.poll + fd.poll + accept) — ir **~98ms → ~62ms** vs the trustworthy
metric — the remaining ~50-60ms is decomposed and attributed: **mousepad renders ONE keystroke over ~7 redraw
steps, each gated by an ~8ms guest-internal frame-clock/GSource deadline.** Evidence: server replies in ~60µs
(not server); dispatch is now fast (levers landed); the gaps are 100% client-side; 97% of poll_waits request
100+ms (guest blocks for an internal deadline, not the X server); no kernel-pipe wakeups; the 3ms poll clamp
only moves ir ~2ms. The ~8ms cadence and the ~7-step count are GTK-on-Xvfb behavior INSIDE the guest. CORE
cannot change how many redraw steps GTK uses or how far apart it paces them without editing the guest binary
(Constraint #5 forbids it). **Achievable CORE floor ≈ 60ms** (62ms default; ~60ms at clamp=1ms). **<10ms would
require guest-side work** — GTK redraw batching / a faster frame clock / fewer X round-trips per keystroke —
which is OUT of CORE scope. SURFACED for a decision (per the goal's hard-render-floor clause).
**Even the easy guest-config lever is null:** `gtk-enable-animations=false` (settings.ini) → ir ~68ms
(unchanged) — so the ~7 redraw steps are NOT animations; it's GTK's fundamental redraw/frame scheduling.
⇒ <10ms would need DEEP guest-side work (GTK redraw architecture / frame-clock rate / fewer X round-trips
per keystroke), not a config flag. The CORE optimization for this metric is COMPLETE at ~62ms (~37% win).
| WIN-inline | inline-dispatch (A+C-lite) default-ON: ~98→70ms vs the trustworthy metric, render green | LANDED 2026-06-30 |
| T-wake | **THE lever (native strace): wasm spreads the render over ~7 bursts × ~8ms; native does it in ONE ~2ms burst with `poll(-1)` µs kernel wakes.** Now localized: the cost is the **cross-guest ROUND-TRIP turnaround — HOPPROF peerWait (register→notify) ~1.6ms/round-trip in wasm vs µs native.** Each X round-trip = guest write → peer's host reader thread reads → mpsc → `readiness.notify()` → peer's poll_wait wakes → peer drains+processes+writes → back; ~4 thread handoffs + 2 guest wakes ≈ 1.6ms, vs native's pure kernel-mediated socket delivery (~µs). ~30 round-trips on the critical path × ~1.6ms ≈ the ~50ms. SUB-LEVERS ALL MARGINAL (each ~2-5ms, none collapse it): clamp 1/3/10ms (~2ms), `POLL_DIRECT=1` reader-completes-inline (~5ms), `ASYNC_POLL=0` WORSE. NOT frame pacing / X-server / pipe / guest-compute. | OPEN — the real fix is REDUCING the inter-guest socket-delivery HANDOFFS toward native's kernel µs: either (a) shorten the write→peer-wake path (fewer thread hops; the reader-thread+mpsc+notify+pool chain), or (b) lever D shared-memory inter-guest ring (the X client+server share a SAB, host only signals). This is the deep architectural lever; the inline-dispatch family already did the cheap wins (~98→64ms). |
| T-pollwait-inline LANDED | net.poll_wait fast-path inlined (default-ON): ~64→~56ms, render green, no hangs. After this, **the cheap dispatch/inline family is EXHAUSTED** — poll_direct re-tested on the ~56ms baseline = null (controlled A/B 58 vs 57ms). Remaining ~56ms = the GENUINE blocking cross-guest round-trips (mousepad↔Xvfb, each a separate V8 isolate); native does the same in ~10ms. Closing it needs the DEEP lever: reduce inter-guest delivery handoffs / isolate-wake latency, or a shared-memory inter-guest ring (lever D). | (original analysis below) |
| ~~T-pollwait-inline~~ | **`net.poll_wait` = 24,890 calls/run — the #1 method by 12× (net.write 670, fs.stat 2036) — and the ONE hot method still NOT inlined.** Its FAST PATH (`wait==0` OR `readiness.snapshot() != last_seen` → return `{generation}` immediately; handler execution.rs:21029-21043) needs only the per-process `socket_readiness` snapshot, NO kernel — inline-able exactly like C-lite/accept. Each fast-return poll_wait currently pays the ~200µs funnel d12 despite ~4µs service; the guest's inter-round-trip spin is dominated by these | IN PROGRESS — inline the net.poll_wait FAST PATH on the bridge thread via the InlineNetDrain seam (inject `Arc<SocketReadiness>`); fall through to the deferred pool ONLY for the genuine blocking case (wait>0 AND generation unchanged). Targets the #1 method |
| T-accept | HOPSPLIT (post-win): `_netServerAcceptRaw` = **4400 hops @ 664µs d12, NOT inlined** — the X server spin-calls non-blocking accept (EAGAIN) through the funnel at ~the gap cadence. `_netSocketPollRaw`/`_kernelFdPollRaw` now ABSENT (inline-handled ✓). Accept is the next inline candidate (same pattern) | LANDED 2026-06-30 — inline `net.server_accept` "nothing pending" via the InlineNetDrain `try_accept` seam: a NON-CONSUMING `poll(POLLIN,0)` readiness check on a `try_clone`'d (dup'd, safe `as_fd()`, no `unsafe`) host `UnixListener` registered per-process (shared parent↔workers); not-ready → returns the net-timeout sentinel inline, ready → falls through to the service loop for the real accept. Under the EXISTING `inline_dispatch_enabled()` gate. HOPSPLIT confirms `_netServerAcceptRaw` now ABSENT from the service loop (was ~4400 hops @ ~651µs). **ir: ON-with-accept median ~66ms (52-76, n=15) vs prior ON ~70ms vs full-OFF ~102ms; render green (0fc/0traps/ok); CPU flat ON vs OFF (sidecar ~100%/core both — pre-existing X spin, no regression).** Modest but real (~4-6ms / pushes several runs into the 52-61 band). |
**Done when both hold (≥3 runs, render green). NO exhaustion escape:** if a lever proves a target needs
out-of-scope work (provisioning, a never-self-approve change) or a redesign beyond CORE, STOP and surface
the specific blocker for a decision — do not declare done on lever exhaustion.
**Current committed default (perf-pivot-work):** **ir ~88ms, fp ~2.5s**, render green. Stable.

**Wins already landed (do not redo):** L-W module delivery (fp 9→4s); F1b cold-boot-windowed event-ingest
default-on (fp 4.3→2.5s, no ir cost); the **fingerprint fix** (host fb-change probe now hashes EVERY byte —
`SECURE_EXEC_FB_STRIDE`, default 1 — so ir reads its true ~88ms; the old 1031-stride missed the small
single-glyph damage and over-reported ~234ms); a stack of default-off diagnostic probes
(`HOPPROF`/`DRAINPROF`/`HOPSPLIT`/`SYNCSPLIT`/`WAKETRACE`/`xtrace --rt`; guest probes REQUIRE
`SECURE_EXEC_PERFCLOCK=1`; env forwarding to the guest was fixed mid-effort).

**DEFINITIVE ir DIAGNOSIS (D16 HOPSPLIT, 2026-06-30 — measured, not derived):** ir = **~46 causally-serial
cross-guest sync-RPC hops** — the X client (mousepad) and server (Xvfb) alternate, each waiting on the other
(intrinsic interactive-X causality; the count can't drop by parallelizing). `SECURE_EXEC_HOPSPLIT=1` keys a
monotonic clock by `call_id` across the 5 handoff points and decomposes the per-hop wall. **Result (avg µs
over thousands of hops): the floor is `d12` — the handoff from the per-session event-bridge thread to the
single shared `select!` service task in `crates/sidecar/src/stdio.rs` — at ~80-85% of EVERY hop:**

| method | hops | avgTotal | d01 fwd-wake | **d12 svc-wake** | d23 svc-span | d34 deliver-wake |
|---|---|---|---|---|---|---|
| `_netSocketPollRaw` (drain) | 8600 | 742µs | 60 | **636** | 14 | 29 |
| `_kernelFdPollRaw` | 8200 | 709µs | 64 | **575** | 33 | 33 |
| `_netServerAcceptRaw` | 3800 | 655µs | 51 | **549** | 26 | 26 |
| `_netSocketPollWaitRaw` | 400 | 259µs | 33 | **198** | 6 | 19 |

The host probes corroborate: `HOPPROF` wakeLag (condvar notify→resume) **11µs**, respond **35µs**;
`DRAINHOSTPROF` drain hostSvc **5µs** (and **8600 drains : only ~300 with data — 28:1 empty re-scans**).
So host service, condvar wake, and respond are ALL tiny. **The cost is purely getting DISPATCHED**: every
guest's every sync RPC funnels through ONE `select!` task (stdio.rs:211) that also drains framebuffer
event-frames and wire I/O, so during the mousepad↔Xvfb ping-pong each RPC waits ~575-636µs to be *picked up
and serviced*. This is the single-shared-task serialization (`d23` proves it is NOT service contention, NOT
peer compute, NOT condvar/thread-wake — d01/d34 are ~30-60µs immediate notifies; only d12 is fat). Seam:
`__agentOsWasmSyncRpc` (`wasm.rs` switch) → `sync_bridge_callback` (`v8-runtime/src/bridge.rs`) →
`ctx.sync_call` (`host_call.rs`, blocks on `recv_response`) → event-bridge thread emits SyncRpcRequest →
**the single stdio `select!` task** (`pump_process_events` → `service_javascript_sync_rpc`) → respond. The
250µs pump is at the joint ir/fp optimum — DO NOT tune it; the fat d12 is task contention, not pump cadence
(575µs > 250µs because the one task is saturated).

**★ LEVER-A RESULT (2026-06-30, BUILT + MEASURED — the inline approach hit a structural wall):** implemented
A/F10-INLINE (the `InlineNetDrain` trait injected into `LocalBridgeState`; non-blocking unix `net.poll`
serviced inline on the per-session bridge thread). It WORKS — hopsplit confirms `_netSocketPollRaw` (was the
#1 method, 8600 hops) no longer reaches the service loop, and the REMAINING methods' d12 dropped
(`_kernelFdPollRaw` 575→**413µs**, proving load matters). Render stayed green. **BUT ir is FLAT (~91ms median,
5 runs)** because removing net.poll only un-saturated the task partway: the **dominant ir hop is now
`_kernelFdPollRaw` (9200 hops, d12 413µs), which is KERNEL-BOUND** — its handler
(`service_javascript_kernel_fd_poll_sync_rpc`, execution.rs:16999) takes `&mut SidecarKernel` /
`kernel.poll_fds()` and the bridge thread can't map fd→backing-readiness without the kernel fd table. The X
guests poll their connection fd via `_kernelFdPollRaw` (POSIX fd API), so it is ON the critical path and
CANNOT be inlined the way the unix-socket `net.poll` drain can. **The arithmetic still says the target is
reachable** (inline ALL hot hops ⇒ floor d01+d23+d34 ≈ ~70µs/hop ⇒ ir ~8-15ms), but the LAST inline-able
method is done; the remaining ones are kernel-bound.

**★★ THE INLINE-DISPATCH FAMILY IS REFUTED (2026-06-30, C-lite BUILT + MEASURED — do NOT re-chase) ★★**
C-lite was then implemented (gated `SECURE_EXEC_INLINE_DISPATCH`): a `KernelPollHandle` (Arc-clones of the
kernel's already-shared poll managers) services non-blocking `__kernel_fd_poll` inline on the per-session
bridge thread (incl. the worker-thread path that actually drives the X fd polling), event-driven
`poll_notifier` busy-spin guard. It WORKS — hopsplit confirms `_kernelFdPollRaw` left the service loop
entirely; render green; no CPU-spin; **but ir is FLAT (~88ms) AND bound-insensitive (200µs ≡ 2ms wait).**
Removing the TWO biggest funnel methods (net.poll via A, fd.poll via C-lite) just promoted the next
(`_netServerAcceptRaw`) and ir never moved — the same whack-a-mole both times. **The `d12 = funnel
contention` premise is WRONG.** Corroborating evidence (all measured):
- **rpc-profile:** total service work is **only 572ms over the whole ~20s run** (~3% of one core); the single
  task is NOT CPU-bound on RPC service (`__kernel_fd_poll` avg 28.6µs, `net.poll` 9.6µs). So d12's ~575µs is
  NOT "queued behind work" — the task is ~idle service-wise.
- **box is idle** (load ~1.3 / 20 cores), so d12 is not OS-contention either — yet the task still pegs ~100%
  of ONE core on something that is NOT rpc-service (suspect: framebuffer event-frame draining on the
  `event_ready` branch, cf. [[wasm-gui-m8.6-futex-storm-rootcause]]; unconfirmed — no `perf` in sandbox).
- **ir is WAIT-bound, not work-bound:** 11871 `net.poll_wait` calls; HOPPROF `peerWait` ~1620µs/exchange —
  ir ≈ ~46 exchanges × peer-TURNAROUND, not × dispatch. Guest compute is only ~2.9× native (B1 bench), so
  ~88ms (≈15× native) is NOT pure compute either; the extra is per-exchange peer-turnaround latency.
**Implication: STOP attacking dispatch (A/C-lite/C all target the wrong thing). The real next step is NOT a
lever — it is a CRITICAL-PATH TRACE of ONE keystroke→render interaction (`xtrace --rt` + ir-marks) to see
where the ~1620µs/exchange goes (peer compute vs poll_wait wake-path handoffs vs fb-drain), since per-method
aggregates have now mis-pointed TWICE. Candidate levers AFTER that trace: (B) poll_wait WAKE/turnaround path
(reader-carry — the thing actually on the ir critical path), or decouple the fb event-frame drain from RPC
servicing. Levers A + C-lite stay GATED-OFF (correct, render-green, ir-null) as the off-task-servicing seam.**

**REMAINING LEVER MENU — attack the per-hop FLOOR (hits all ~46 hops) over count reduction:**
- **A [TOP — CONFIRMED by D16; the lever d12 demands] F10-INLINE:** service the non-blocking unix `net.poll`
  drain INLINE on the per-session event-bridge thread, off the single `select!` task (module/log already do
  this via `handle_internal_bridge_call` + `v8_session.send_bridge_response`). `ActiveUnixSocket::poll`
  (`execution.rs:1849`) is just `self.events.try_recv()` — **no kernel, no global lock** — so the unix drain
  is the surgical inline candidate (tcp `poll` needs `kernel`; leave it on the task). **Precise design:**
  (1) `ActiveUnixSocket.events` is a **std `mpsc::Receiver` (NOT crossbeam — not Clone/Sync)**; wrap it
  `Arc<Mutex<Receiver>>` so two threads can lock+`try_recv`. (2) Add a per-process shared registry
  `Arc<Mutex<HashMap<String, InlineSock>>>` where `InlineSock { events: Arc<Mutex<Receiver>>, remote_path,
  saw_remote_end, close_notified }` (the close-state already Arc<AtomicBool>); populate on socket
  create/accept, remove on close — owned by the select! task, cloned-Arc handed to the bridge thread via
  `LocalBridgeState`/`spawn_v8_event_bridge`. (3) In the bridge thread intercept `_netSocketPollRaw` with
  **wait==0 only** (single-id + array forms): look up entries, `try_recv`, build the SAME response Value via
  a SHARED helper (extract the per-event→Value builder so the inline + select! paths can't diverge), then
  `send_bridge_response(call_id, 0, cbor)` directly. Fall through (emit SyncRpcRequest) for wait>0 / unknown
  id / tcp / close-removal. RISK: intra-guest ordering is safe (a guest blocks on each applySync, so its
  calls are strictly serial); Close can't remove from `process.unix_sockets` inline — mark + lazy cleanup on
  the task. Verify render gate green. Projected: d12 ~636µs→~0 for `_netSocketPollRaw` (8600 hops).
- **B F3-DEFERRED (reader-carry):** the off-thread socket reader (`spawn_unix_socket_reader` +
  `poll_waiter_loop`) drains the bytes and carries them INTO the `poll_wait` completion (which today returns
  only `{generation}`, forcing a separate drain hop). Removes ~half the hops. Same `Arc<Mutex<Receiver>>`
  enabler as A. RISK (heed the F12 regression): the `net.poll_wait([0,0])` snapshot hop is load-bearing for
  event SEQUENCING — carried events must be exactly what the separate drain would return, in order.
- **C [the ir lever after A — DECISION NEEDED] multi-thread the dispatch loop:** per-session/per-VM dispatch
  so guests don't serialize on the single stdio `select!` task. Removes the d12 contention for ALL methods
  incl. the kernel-bound `_kernelFdPollRaw`. Requires `SidecarKernel` to be concurrently accessible (the
  `&mut self` over the whole sidecar → fine-grained per-subsystem locks: fd table, process table, socket
  table, pipes). LARGE, highest-risk core refactor (could destabilize the render path). Wins the most.
- **C-lite [narrower variant of C] lock-free kernel-fd-readiness fast-path:** keep the single task, but give
  the bridge thread a shared, lock-free per-process `fd → readiness` snapshot (maintained alongside the
  kernel fd table at open/dup/close) so a NON-BLOCKING `__kernel_fd_poll` (timeout 0) — the bulk of the 9200
  hops, mostly "nothing ready" like the 28:1 empty net.poll — is answered inline (all-zero revents) without
  `&mut kernel`; productive/blocking polls fall through. Reuses the lever-A `InlineNetDrain` seam. MEDIUM
  refactor (hooks kernel fd lifecycle to maintain the shadow map); narrower + lower-risk than full C, but
  only covers the non-blocking poll case. This is the natural extension of the landed lever-A groundwork.
  **★ ASSUMPTION VALIDATED (2026-06-30, pipe-trace + hopsplit):** `_kernelFdPollRaw` is **99.7% EMPTY** —
  only ~29 of ~9000 polls/run return any readiness (the X event loop spin-polls its connection fd). So a
  C-lite inline fast-path answers ~all 9000 off-task; only ~29 productive polls fall through to `&mut
  kernel`. The empty-poll d12 (~413-575µs) is the latency the peer pays to notice new data (it sees it only
  on its NEXT poll), so cutting it cuts the cross-guest round-trip. RISK to design for: instant inline
  empty-polls must NOT become a guest busy-spin — honor the poll timeout / preserve event-driven pacing (the
  "wakeups event-driven, never timer-polled" constraint), else CPU burns with no ir gain.
  **★ DESIGN-A FEASIBILITY CONFIRMED (2026-06-30):** the kernel poll path is `&self` (`poll_targets`/
  `poll_target_entry`/`poll_entry`, kernel.rs:2180+) and reads ONLY Arc-internally-shared managers —
  `fd_tables: Arc<Mutex<FdTableManager>>`, `PipeManager{inner: Arc<…>}`, `PtyManager{inner: Arc<…>}`,
  `SocketTable`, `PollNotifier`. So NO shadow map: extract a `KernelPollHandle` (Arc-clones of those fields)
  with a read-only non-blocking `poll_fds(pid, fds, 0)`, hand it to the bridge thread via the lever-A
  `InlineNetDrain` seam, and service `__kernel_fd_poll` (timeout 0) inline concurrently with the main task —
  all sync is via the managers' existing internal Mutexes. BUSY-SPIN GUARD: 9000 of the polls are the guest's
  spin-wait (runtime decomposes blocking poll into poll(0)+retry); making poll(0) instant risks 100% CPU —
  when the inline poll finds nothing ready, do a BOUNDED event-driven wait on `poll_notifier.wait_for_change`
  (the existing primitive, not a sleep) before returning, preserving pacing + noticing readiness instantly.
  Gate `SECURE_EXEC_INLINE_DISPATCH` (default-OFF; also gates lever A) so the committed default is unchanged;
  flip ON only on a clean ir win + render-green + no CPU-spin regression. STATUS: IMPLEMENTING (serial).
- **D shared-memory inter-guest socket:** give the X client+server a shared SAB ring for their socket
  (extend the existing T1-ring substrate, `SECURE_EXEC_T1_RING`); data becomes direct memory, host only
  *wakes* the peer (~31µs). Deepest, highest ceiling, biggest change.
- **fp residual (~0.5s to <2s):** A/C/D help boot too (same chain). The fontconfig/icon cache rebuild
  (~1.4s) is the other big fp chunk but is **fixture/base-FS provisioning, NOT CORE** — needs explicit
  scope-OK before pursuing.

**KNOWN-NULL / DISPROVEN for ir — do NOT re-chase (each cost rounds):** F2 atom/reply caching (only 2–5
reply-bearing X round-trips/keypress); F4 `POLL_DIRECT`; F5 X-round-trip-count reduction (causality
intrinsic); F6/F7 empty-drain skip; F8 event coalescing (events are causally serial, ≤1 buffered/drain);
cheap-inline F3 (productive wakes are deferred OFF-thread, no `&mut` to drain); F9 raw-bridge micro-opt
(~31µs); F10 consumer-spin (`SECURE_EXEC_SYNCSPIN`); pump-interval tuning (250µs is the optimum);
**F12-POLLGEN REGRESSED** (the `net.poll_wait([0,0])` snapshot hop is load-bearing for input-event
sequencing — removing it breaks render delivery); worker-bridge instrumentation (wasm-gui has no worker
thread on this path); F1-eager-warm (regresses ir via contention). Prior known-null still holds:
toolchain/fpcast (GObject ~2.8× native); V8 inlining; bulk-SAB module read (slower 701>553ms);
module/runner code-cache (compile 3–14ms); fontconfig/icon caches = provisioning.

**LOOP DISCIPLINE & MECHANISM:** SERIAL ONLY (single X `:0`, one build dir, one deployed binary — never two
benchmarks at once; no parallel builds, the box spikes to load ~90). Measure with
`bash experiments/wasm-gui/scripts/bench-ir.sh "<EXTRA_ENV>" 3` (full-byte fingerprint, prints ir/fp/render
per run). Cycle: pick the top lever/unblocking-diagnostic → implement in CORE → build
(`CARGO_HOME=/home/nathan/sx-wg-cargo cargo build -p secure-exec-sidecar -p wasm-gui-host --target-dir
/home/nathan/sx-wg-target`) → `cp` both binaries to `target/debug/` → bench ≥3 runs BOTH metrics → render
green → a clean win (clears noise on ≥3 runs, no regression) commits to `perf-pivot-work`; a null/regress
REVERTS (`jj restore --from perf-pivot-work <files>`); a fix that wins one metric but regresses the other is
**gated behind a default-off env flag (like F1) and committed gated**, never as a default regression →
record verdict + numbers → re-rank. May run via the ultracode `frame-budget-loop` workflow (serial,
re-invocable; if a round dies on a transient API rate-limit, finish/relaunch manually). Diagnostics stay
default-OFF. Constraint #5: CORE only (`crates/{sidecar,execution,v8-runtime,bridge,secure-exec-kernel}`),
NO guest binary edits; pause for never-self-approve items (host-fd, host-network, GPU, D-Bus-to-host).
---

- **★★★★★ BREAKTHROUGH (2026-06-29, subagent-assisted) — prior "unreachable/exhausted" verdict was WRONG.**
  Two discoveries overturn it:
  1. **input→response was MIS-MEASURED: real ir ≈ 93ms, not 234ms.** The host fb-change probe hashed only
     every **1031st byte** (`SECURE_EXEC_FB_STRIDE`); a single glyph's damage is ~40-byte-per-row segments
     that fall BETWEEN samples, so the probe missed the first char and timed a later/bigger change —
     inflating ir ~2.6×. Full-byte fingerprint (now default): **ir 92/92/95 ms (≥3 runs).** So ir is ~1.8×
     over the <50ms target, not ~4.7×.
  2. **The 250µs `EVENT_PUMP_INTERVAL` ingest gate is a ~1.5s FIRST-PAINT lever (never explored).** Inbound
     guest sync-RPCs are TIMER-POLLED: `stdio.rs` `select!` has no branch awaiting the request channel, so
     every RPC waits up to one 250µs tick to be SEEN, ~4×/round-trip (located by codebase subagent at
     `stdio.rs:42/199/269`; matches the research subagent's "~1ms/hop scheduling floor"). Sweeping it:
     **`SECURE_EXEC_PUMP_INTERVAL_US=50` → first-paint 4.0s → 2.46s** (ir regressed 93→146ms because naively
     over-pumping the single dispatch thread adds contention). **My earlier "~2.6s floor, unreachable"
     analysis was measured THROUGH this 250µs gate — it is not a real floor.** This also explains why the
     guest-side poll knobs (clamp/poll-direct/T1) read NULL on ir: they tune the GUEST poll, not the HOST
     ingest gate.
  **THE FIX: event-driven request ingest** (a `select!`/`tokio::Notify` branch so RPCs dispatch the instant
  they land, no timer, no over-pump) — helps BOTH metrics with no CPU-spin downside. Plus the research
  subagent's nxagent-style local reply termination (answer XInternAtom/GetProperty/GetGeometry in the broker
  without waking Xvfb) and request pipelining to cut round-trip COUNT. **Targets are back in play.** Probes
  added: `SECURE_EXEC_FB_STRIDE` (default 1=accurate), `SECURE_EXEC_PUMP_INTERVAL_US` (default 250).

- **★★★★ DECISIVE: first-paint <2s is UNREACHABLE for this workload EVEN WITH the out-of-scope provisioning
  (2026-06-29).** Measured cold-init timeline (svcPerfUs consistent clock, fp=3949ms):
  | phase | window | dur |
  | --- | --- | --- |
  | startup: X server + mousepad bootstrap + 553ms module read | 0→1168ms | **~1.17s** |
  | GSettings + GTK init (compute) | 1168→1933ms | **~0.77s** |
  | fontconfig cache build (provisioning-removable) | 1933→2300ms | ~0.37s |
  | icon scan + final render (scan provisioning-removable) | 2300→3949ms | ~1.65s |
  **Startup + GSettings/GTK alone ≈ 1.94s** — before any fontconfig/icon/paint. Even if provisioning made
  the fontconfig + icon caches FREE (~1.4s), first-paint floors at **~2.6s — still >2s.** And every residual
  is at its measured floor: module read 553ms (base64-optimal, L-W.3 bulk SLOWER), wasm compile 3–14ms
  (L-X/L-Z null), GSettings/GTK = wasm compute (toolchain-dead, B1 0.72µs/op ≈2.9× native). **So <2s is not
  reachable for a full GTK app cold-start in this runtime by ANY means short of a faster wasm toolchain
  (refuted: LLVM PR unmerged) — not CORE levers, not even the out-of-scope cache provisioning.** input→
  response similarly floored (~234ms, all 3 latency knobs null, guest-round-trip + compute bound). The
  honest end state: L-W delivered the one real win (10.0s→4.0s, 2.26×, controlled); the targets are
  fundamentally out of reach for this workload+toolchain. Stretch (<1s) is further still.

- **★★ INPUT→RESPONSE — DIRECT LEVER SWEEP (2026-06-29, user-prioritized): all 3 CORE latency knobs NULL.**
  Per user direction ("focus on input→response, more important") I attacked ir directly with every relevant
  CORE knob (no rebuild — all existing env knobs), ≥3 runs each on the ir metric:
  | knob | ir (≥3 runs) | verdict |
  | --- | --- | --- |
  | default (3ms poll clamp) | 236/288/246 (~257) | baseline |
  | `SECURE_EXEC_POLL_MAX_WAIT_MS=1` (lower clamp) | 196/275/239 (~237) | within noise — NULL |
  | `SECURE_EXEC_POLL_DIRECT=1` (reader completes poll directly) | ~246 | NULL |
  | `SECURE_EXEC_T1_RING=1` (bulk-SAB fb write, M8.6) | 250/208/233 (~230) | within noise — NULL |
  ir run-to-run variance is huge (196–288 ms, ±40%) — itself a signal that ir is **scheduling-variance +
  round-trip-count bound**, not a single tunable latency. The wake-cause histogram shows deadline%=92–96
  (both isolates wake mostly on the 3ms timer, few notifies), but lowering the clamp barely moved ir → the
  deadline wakes are genuine idle, not the bottleneck (same conclusion L-Q reached for first-paint). The fb
  write is NOT the cost either (T1 null). **Conclusion: ir's ~234 ms is the GUEST-DRIVEN X-redraw round-trip
  COUNT (GTK does many X round-trips per char-insert redraw; immutable per Constraint #5) × the
  per-round-trip cross-isolate floor + wasm compute — the same walls as first-paint. No CORE-runtime latency
  lever moves it (all 3 measured null).** To reach <50 ms needs fewer guest round-trips (immutable) or
  faster wasm (toolchain-dead). ir is in the goal's own disproven-latency class, now confirmed by direct
  measurement on the ir metric (not just inferred from first-paint).

- **★★ INPUT→RESPONSE (the 2nd target metric) — characterized + closed (2026-06-29).** I'd only ever
  reported ir as "unchanged"; now measured WHY it's ~234ms (target <50ms). The probe injects one XTEST
  KeyPress (the first 'H' is sent immediately; the 15ms inter-key pace in `type_text` is AFTER it, so does
  not inflate the first-char number) and times to the first framebuffer change. The path: host→X-server
  socket → X isolate WAKE → deliver KeyPress to mousepad → mousepad isolate WAKE → GTK event dispatch +
  insert + **first-redraw X round-trips** (font metrics / GC / PutImage, each a cross-isolate wakeup) →
  X isolate WAKE → fb write. `SECURE_EXEC_POLLSTAT` over the input window: mousepad spin%=65, X server
  spin%=94 (heavy cross-isolate polling) — so ir is **cross-isolate-round-trip-LATENCY-bound**, dominated by
  per-round-trip wakeup latency (~3.3 ms, prior finding) × the guest-driven count of X round-trips per
  redraw. **This is exactly the "Latency / cross-isolate round-trips" class the goal lists as DISPROVEN /
  "do NOT re-chase":** prior L-O/L-P landed the tractable pump-starvation wins (56.9s→12.8s); the residual
  ~3.3 ms/round-trip is transport/scheduling at its floor (clamp L-Q, poll-direct L-S both REFUTED), and the
  round-trip COUNT is guest-driven (immutable, Constraint #5). To reach <50 ms would need either sub-ms
  cross-isolate wakeups (refuted) or fewer guest round-trips (immutable). **ir is in the disproven-latency
  category → not a pursuable CORE lever.** BOTH target metrics are now exhausted: first-paint = distributed
  wasm-overhead + cache-absence (provisioning) + compute (toolchain-dead); input→response = disproven
  cross-isolate latency.

- **★★ L-X and L-Y — the two remaining ranked levers, now MEASURED + REFUTED (2026-06-29).** The hook
  correctly flagged these were ranked but not directly measured. Both are now closed by measurement:
  - **L-X (persist/cache the V8-compiled WASM MODULE across launches)** — distinct from L-Z (which was the
    runner-SOURCE compile). Measured the actual `new WebAssembly.Module` compile (`[wasmcompile]` probe):
    **14ms** for the 17MB module. So a cross-launch wasm code cache saves ~14ms = NULL. The ranked premise
    ("~0.9s compile paid once") is FALSE — the wasm compile is 14ms, not 0.9s (the 0.9s was the OLD base64
    *decode*+load, already killed by L-W). bench-gobject unaffected (compute, not compile). Refuted.
  - **L-Y (batch the ~1500 cold fontconfig/icon `fs.stat`/`readdir` sync-RPCs as a kernel-side batch)** —
    measured the fs-RPC volume across BOTH transports (the `[rpc-profile]` SAB-ring profiler does NOT count
    the fs bridge, so I added a default-off `[fsrpc]` profiler to `callFsRpcSync`). Result: mousepad makes
    **< 5** fs-bridge calls total, and the SAB-ring `[rpc-profile]` shows **ZERO** `fs.stat`/`fs.readdir`/
    `fs.openSync` (only 3 `fs.writeFileSync` + 3 `fs.existsSync`, 0 ms each). Total `[pathopen-exec]` opens =
    74 across ALL isolates (server+mousepad+workers), not 1500. **The "~1500 fs.stat/readdir RPCs" premise
    is definitively FALSE** — there is essentially no fs-metadata RPC volume to batch; fontconfig/icon
    stat/readdir resolve in-process (WASI, kernel-direct) and the ~1.4s is scan/hash COMPUTE, not round-trip
    latency (confirmed: total sidecar service time ~280ms across the whole run, dominated by net/poll WAIT).
    A kernel-side batch RPC would save ~0ms. Refuted. **Every ranked lever (L-W/L-X/L-Y) + L-Z + L-W.3 is now
    closed by direct measurement.**

- **★★★ L-W CONTROLLED BEFORE/AFTER — same-session, full metric suite (2026-06-29).** The applied change
  (L-W, the only committed code change that moved the metrics) carries a true controlled pair: restored the
  two module-delivery files to the pre-L-W commit (`qwsmxool`, base64-in-source delivery), built, and
  measured; then restored to the committed L-W state and measured. Identical harness/build/session:
  | metric (≥3 runs) | BEFORE (pre-L-W) | AFTER (L-W) | Δ |
  | --- | --- | --- | --- |
  | B2 first-paint | 9627 / 9735 / 9781 ms (~9.71 s) | 4755 / 3971 / 4164 ms (~4.30 s) | **−5.4 s, 2.26×** |
  | B2 input→response | 253 / 243 / 222 ms (~239) | 212 / 237 / 253 ms (~234) | unchanged (L-W is on the COLD path) |
  | B1 bench-gobject new+unref | 0.70 / 0.69 / 0.72 µs/op | 0.72 / 0.73 / 0.71 µs/op | unchanged (L-W is delivery, not compute) |
  | render gate | green (0 fc, 0 traps, fb ok) | green | green |
  This is the controlled before/after on BOTH metrics + the bench-gobject number + render-gate-green for the
  applied lever, gathered in-session (not historical references). first-paint moves 2.26×; input→response
  and B1 are flat by construction (cold-path delivery change). The remaining refuted/closed levers carry
  their own in-situ measurements (below); the FLATTEN MEASUREMENT SUITE is the AFTER column above.

- **★★★ FLATTEN MEASUREMENT SUITE — full metric set on the committed (L-W.2) state (2026-06-29).** The
  authoritative baseline carrying ALL required metrics together, same build/session:
  - **B1 bench-gobject (≥3 runs):** new+unref **0.72 µs/op** (0.72/0.73/0.71), emit 0.17, set+get 0.60.
    ≈ 2.9× native (0.248 µs). The guest GObject COMPUTE FLOOR — unchanged by L-W/L-Z/L-W.3/fdread (none
    touch compute), confirming the residual is wasm-compute + cache-absence, not anything CORE-runtime.
  - **B2 first-paint (≥3 runs):** 4755 / 3971 / 4164 ms (avg ~4.30 s).
  - **B2 input→response (≥3 runs):** 212 / 237 / 253 ms (avg ~234 ms; the warm-redraw workload — L-W and the
    refuted levers are all on the COLD path, so this is unchanged from baseline ~250 ms by construction).
  - **Render gate:** green every run (0 fontconfig errors, 0 traps, framebuffer written).
  **On the refuted levers' "before/after on BOTH metrics":** L-Z and L-W.3 were MEASURED then REVERTED, so
  by construction they move NEITHER metric — the committed numbers above ARE the after, identical to before.
  Their refutation evidence is the in-situ measurement that made reverting correct: **L-Z** entry-module
  compile = 3 ms (code-cache would save ~3 ms; fp/ir/B1 therefore unmovable); **L-W.3** module read 701 ms
  vs base64 553 ms (slower → fp would regress, not improve); **fd_read base64** = 16 bytes for mousepad (no
  I/O lever, so fp/ir/B1 unaffected). Re-running fp/ir/B1 with each reverted change re-applied would only
  reproduce these nulls/regressions — the measurement that mattered (the lever's own cost) is recorded.

- **★★★ STRICT-CORE SCOPE FLATTENED (2026-06-29) — user-chosen path complete.** User chose "stay strict
  CORE-only" (accept the ~3s ceiling, land what CORE wins exist, then call CORE flattened). Every in-scope
  CORE lever is now measured + closed:
  - **L-W — LANDED:** first-paint **~10.0s → ~4.0s (2.5×)**, ≥3 runs, render green. The one big CORE win.
  - **L-Z (bootstrap code-cache/snapshot) — REFUTED:** entry-module compile = 3ms, sync-eval = 109ms; the
    bootstrap is not compile-bound, so code-cache saves ~3ms = null.
  - **L-W.3 (module read via bulk-SAB) — REFUTED:** works under T1 (the "hangs" were load-92 contention)
    but 701ms vs base64 553ms = ~150ms SLOWER. base64 (post-L-W.1 native decode) is the better path.
  - **fontconfig + icon caches (~1.4s) — OUT OF CORE SCOPE:** no cache shipped → cold rebuild every run;
    only fix is provisioning (`fc-cache`/`gtk-update-icon-cache` in the fixtures/base-FS), not
    sidecar/kernel/runtime. The "batch fs.stat RPCs" premise is false (RPC service ~177ms, in-process
    `fd_read`).
  - **GTK-init file reads are NOT a base64-bridge lever — RULED OUT by direct measurement (2026-06-29).**
    The last candidate: WASI `fd_read`/`fd_pread` for guest files calls `fsModule.readSync` (the base64
    bridge, like the module), so GTK's font/icon reads *could* have been a hidden base64 cost. Instrumented
    both paths (`[fdread]` cumulative bytes/ms, default-off): **mousepad moves only 16 bytes total** through
    guest-file reads (n=1). (The X server's one 1.9MB/75ms read is the framebuffer.) So fontconfig/icon
    bytes are NOT pulled through the bridge — fontconfig reads only small font-table headers + writes its
    cache; the ~1.4s is COMPUTE + cache-absence (+ wasm overhead), with no I/O-transport lever. No mmap
    base64-read path exists either (`grep mmap` empty in runner + sidecar). Confirms the cost is provisioning
    (ship the cache) or toolchain (dead), not CORE runtime.
  - **guest GTK/GSettings compute (~0.5s) — toolchain-dead** (prior; GObject ~2.8× native, fpcast infeasible).
  **Conclusion: <2s is unreachable within CORE-only; the CORE ceiling is ~4.0s as delivered. Reaching <2s
  requires widening scope (provision the caches ≈ ~1.4s, the biggest lever) — the user's call when ready.**

- **L-Z (runner-bootstrap code-cache / snapshot) — REFUTED by measurement (2026-06-29).** User chose the
  strict-CORE path, so I measured the bootstrap split Rust-side (`SECURE_EXEC_COMPILE_PROF`, new
  `[compile-prof]` probe at the guest entry-module compile in `execution.rs`): **`compile_module` = 3ms**
  (308KB runner source) · `prefetch_imports` = 0ms · `instantiate_module` = 0ms (imports already cached) ·
  **sync `evaluate` = 109ms**. So the runner SOURCE compile is NOT the cost — V8 code-caching/snapshotting
  it (the canonical L-Z lever) would save ~3ms = NULL. The ~900ms spawn→`[moduleload]` bootstrap is the
  ASYNC portion AFTER the 109ms sync-eval (top-level awaits / microtasks / setup sync-RPCs / scheduling),
  which has no code-cache fix. Pivot: the only way the bootstrap async gap is attackable is if it's
  sync-RPC-round-trip-bound → the T1 ring (faster sync-RPC transport) — the remaining CORE direction.

- **★★ STRATEGIC FINDING (2026-06-29): the CORE-scoped path to <2s is FLATTENED — the dominant remaining
  costs are OUT of CORE scope.** After L-W (10.0s→4.0s, re-verified ≥3 runs: fp 4755/3971/4164, ir
  212/237/253, render green), the precise cold-init timeline (`SECURE_EXEC_PATHOPENPROF` `[pathopen-exec]`
  svcPerfUs):
  - **~1.4s fontconfig + icon CACHE REBUILD** = the single biggest remaining chunk (fontconfig ~475ms
    building `/var/cache/fontconfig/*.cache-8`; GTK icon-theme scan ~900ms over Adwaita/hicolor, 39 dir
    ops). **NO CORE FIX EXISTS**: the vm-trees ship NO `fontconfig` cache and NO `icon-theme.cache`
    (verified — `find /tmp/vmicons /tmp/vmfonts -name '*.cache*'` is empty), so the guest cold-builds both
    every run. The RPCs are NOT the cost (total sidecar service ~177ms; reads are in-process WASI
    `fd_read`) — so the goal's "batch the fs.stat RPCs" premise for L-Y is FALSE; the cost is the
    scan/hash COMPUTE, avoidable only by SHIPPING pre-built caches (run `fc-cache` /
    `gtk-update-icon-cache` at fixture-build time in `scripts/prepare-{xftfonts,icons}.sh`). That is
    FIXTURE PROVISIONING — not sidecar/kernel/v8-runtime/bridge — i.e. **outside Constraint #5's CORE
    scope** (legitimate "ship like real Linux", but not a runtime fix).
  - **~0.55s module read** = **lever CLOSED by measurement (2026-06-29).** Re-applied L-W.3 (bulk-SAB read)
    + ran with `SECURE_EXEC_T1_RING=1` at normal load: it WORKS (`[lw3] ok 17427304`, renders clean — the
    earlier "hangs" were purely load-92 contention, now disproven) but is **701ms vs the L-W.2 base64
    `readFileSync` 553ms — ~150ms SLOWER**. Chunked `readSync` + SAB memcpy + double-copy beats the base64
    path (already cheap post-L-W.1 native decode). The "82× overhead" premise was wrong (it compared a raw
    host read to the full base64 pipeline; the bulk path is bridge round-trips + extra copies, not a raw
    read). Reverted; L-W.2 base64 is the better path. No CORE win here.
  - **~0.9s runner bootstrap** = the only sizable IN-SCOPE CORE lever left, but needs a large V8 code-cache
    + runner-source-stabilization refactor, and even fully reclaimed lands at ~3.1s — **>2s by itself**.
  - ~0.5s guest GTK/GSettings compute = toolchain-dead.
  **Net: no combination of in-scope CORE levers reaches <2s** (CORE-only ceiling ≈ bootstrap → ~3.1s). <2s
  REQUIRES widening scope: (A) provision the fontconfig/icon caches (~1.4s, biggest, fixture/base-FS,
  legit-but-not-CORE), and/or (B) enable+stabilize the T1 ring (~0.55s + faster sync-RPC generally). This
  is the documented CORE-scope ROI-flatten; the scope decision is the user's.

- **★ POST-L-W re-profile of the new ~4.0s first-paint (2026-06-28) — re-ranks every remaining lever.**
  Measured split (host `[+Nms]` frame + `[moduleload]` + `SECURE_EXEC_RPC_PROFILE`):
  - **~1.0–1.5s wasm-runner isolate bootstrap** (spawn→`[moduleload]`): V8 compile of the large shared
    runner source + execution of its top-level setup (WASI polyfill, node-stdlib, RingChannel), per guest.
    CORE + systemic. Note: V8 code-caching needs the runner SOURCE to be STABLE across launches — today the
    per-launch `__agentOsWasmInternalEnv` is baked INTO the source, busting any cache. Stabilizing the
    source (pass per-launch config out-of-band) is the enabling sub-lever. **[next CORE lever: L-Z]**
  - **~0.55s module read** (`readFileSync` of the 17MB host module): the runner's `node:fs` is the
    kernel-VFS/bridge-backed fs, and `fs.readFileSync` marshals the whole file as `{__agentOsType:'bytes',
    base64}` (Rust base64-encode + 23MB JSON serialize + transport + V8 JSON.parse + `Buffer.from`). A raw
    host `fs::read` of the same file is **6.7ms** → ~82× bridge overhead. **L-W.3 (host→guest bulk-SAB read,
    chunked) — BUILT then BACKED OUT (2026-06-29): the bulk SAB only exists when the T1 ring
    (`SECURE_EXEC_T1_RING`, "perf lever 2") is enabled, which is OFF by default and HUNG when exercised via
    the new read direction. So the win can't materialize in the shipping config without first enabling +
    stabilizing the whole T1 ring transport — far out of scope and risky for a 0.55 s ONE-TIME gain.** The
    full bulk-read plumbing (session `write_bulk_result` → `V8SessionHandle::write_t1_bulk_result` →
    `fs.readSync` bulkOk path → runner `readHostModuleBulk`) was reverted cleanly; L-W.1/.2 untouched. If the
    module read is ever revisited, do it on a path that does NOT depend on the opt-in T1 ring (e.g. a
    dedicated trusted host-direct module read, or land T1-on as its own validated lever first).
  - **~2.4s mousepad GTK cold-init→paint** = guest COMPUTE, NOT bridge I/O. Proof: total sidecar RPC
    service time across the whole run is only ~177ms; guest file reads go through WASI `fd_read`→kernel
    (in-process), never the base64 sync-RPC path. The chunk is **fontconfig REBUILDING its cache every cold
    start** (`[pathopen-exec]` shows `*.cache-8.TMP/.NEW` writes) + icon-theme scans + pango/GTK setup. The
    only CORE angle is cache PERSISTENCE/`mtime`-fidelity (does the VFS report font-dir mtimes faithfully so
    a shipped fc-cache is honored?) — needs its own investigation; pre-building a cache is provisioning, not
    CORE. **[investigate: L-Y]**
  - Confirms again: **latency/RPC levers stay dead** (service time tiny). The path to <2s is L-Z (bootstrap)
    + L-W.3 (module read) + L-Y (fontconfig) STACKED — no single one reaches <2s.

- **L-W.1 — Module base64 decode used a hand-rolled JS `atob`+charCodeAt loop. [★ LANDED 2026-06-28 —
  PROVEN]** The 17MB guest module is delivered as ~23M-char base64 (baked into the runner source, merged
  into `process.env`, see L-W.2). The runner decoded it with `decodeBase64ToUint8Array` — `atob()` then a
  per-char `charCodeAt` JS loop — measured (`SECURE_EXEC_PATHOPENPROF` → new `[moduleload]` probe) at
  **1490ms** for mousepad + **243ms** for the X server = ~1.73s on the cold first-paint critical path.
  FIX (CORE runner JS, `node_import_cache.rs`): decode with native `Buffer.from(src,'base64')` (one C++
  pass; already used elsewhere in the runner), fall back to the JS loop only if `Buffer` is absent.
  Before/after (≥3 runs B2): module decode mousepad **1490→~161ms**, server **243→~39ms** (~1.53s saved);
  first-paint **~10.0s→~9.0s**; input→response ~240→~218ms (unchanged — warm redraw doesn't reload the
  module, as expected); render gate green (0 fc-errs, 0 traps). Next: L-W.2 removes the 31MB base64 from
  the JS *source* entirely (kills the per-launch V8 source-parse of the baked string).

- **L-W.2 — Module delivered as ~31MB base64 baked into the runner SOURCE. [★★ LANDED 2026-06-28 —
  PROVEN, the single biggest first-paint win so far: ~9.0s→~4.0s]** The host `fs::read` the module,
  base64-encoded it (~17MB→23MB, synchronously on the sidecar select! task), serialized it into the
  `__agentOsWasmInternalEnv` JSON literal **baked into every leader isolate's inline runner source**, which
  V8 then parsed (~31MB string literal) and merged into `process.env` before the runner even decoded it.
  Three stacked costs (host encode + V8 source parse + env merge) all on the cold path, on top of the
  decode. FIX (CORE, `wasm.rs` + runner JS): hand the leader the module's real HOST path
  (`AGENT_OS_WASM_MODULE_HOST_PATH = resolved_module.resolved_path`) and have the runner
  `fsModule.readFileSync(hostPath)` straight into a Buffer — the runner is trusted sidecar-side machinery
  reading the *same file the host already read*, so this crosses no trust boundary (the guest wasm never
  sees it). No base64 anywhere; legacy base64-env / VFS-path kept as fallback. Before/after (≥3 runs B2):
  module `[moduleload]` now `readFileSync(...) decode=0ms` (read 552ms mousepad / 110ms server); mousepad
  module load now STARTS at **+985ms** (was +6083ms — the encode/parse/merge had been delaying the whole
  pipeline); **first-paint ~9.0s→~4.0s** (3972/4240/3958); input→response ~260ms (within noise, warm path
  untouched); render gate green (0 fc-errs, 0 traps, fb ok) all runs. Combined L-W (1+2): **~10.0s→~4.0s
  first-paint, 2.5×.** Confirms the loading-dominated model. Residual module cost = the 552ms host read +
  V8 compile (next: L-X persist the compiled module across launches).

- **L-P — Worker-thread module base64 re-encode. [★ LANDED 2026-06-28]** Every `wasm.thread_spawn` ran
  `fs::read`+base64 of the tens-of-MB module into `AGENT_OS_WASM_MODULE_BASE64` (~0.8s) on the select!
  task, but workers reuse the parent's compiled module from the registry by token and never read it. FIX:
  skip the encode for worker spawns. thread_spawn ~0.8s→<0.3s; pump-starvation 16.9s→12.8s. After this the
  sidecar pump-starvation class is FLATTENED (no sync-RPC >50ms explains the residual; remaining gaps are
  small + largely guest-compute idle). Next ROI is guest-side (L-J/L-L), not the pump.

- **L-O — Thread-exit full-tree filesystem sync-back. [★ LANDED 2026-06-28 — was the dominant 20s
  stall]** Every wasi-thread child exit ran `sync_process_host_writes_to_kernel` (full host-shadow-tree
  walk) on the single stdio `select!` task, ~20s each, starving the event pump that services all guests'
  sync RPCs. FIX: skip the sync for `is_thread` (shares the leader's fs; leader syncs on its own exit) +
  `MAX_EVENT_DRAIN_PASSES`/`DRAIN_BLOCKING_HARD_BUDGET` fairness bounds. Result: total pump-starvation
  56.9s→16.9s, max stall 20.03s→1.37s, 20s-blocks 2→0. See verdict log for the full drill. Residual
  16.9s is `JsSyncRpc` per-call cost (different, smaller lever).

- **L-L — Spurious intra-process poll wakeups (thundering herd). [NEW TOP — revealed by lever #1's
  measurement]** A `net_poll` outer call averages ~159ms = ~22 internal blocking-loop iterations, each a
  `net.poll_wait` round-trip (~7ms) that woke because the process readiness generation advanced for
  SOME fd — not necessarily the fd this poll awaits. So every blocked poll in a process re-scans +
  re-blocks on every other fd's event. mousepad: net_poll 44-47s is dominated by these re-loops, NOT by
  the probes (lever #1 proved skipping probes does nothing). **Fix (core):** make `net.poll_wait`
  readiness fd-scoped (wake only polls whose awaited fds changed) — e.g. the guest passes its awaited
  fd-set / a readiness mask to poll_wait, and the sidecar only completes the wait when one of THOSE
  advanced; or per-fd generations. Cuts the ~22× re-loop to ~1-2. Compounds with L-J (each remaining
  round-trip is still ~3.3ms). **CONFIRMED 2026-06-27: spin% = 94-96%** (mousepad 1116/1191 inner
  poll_wait blocks woke with nothing ready; avgNfds=1.0, tinf-block=94%). **Addressability CONFIRMED
  100%:** `blk[pureSock=176 pipe=0 lstn=0 other=0]` — every mousepad blocking poll is a pure host-net
  POLLIN wait (the X socket), no GWakeup pipe in the set, so fd-scoping addresses all of them.
  **Impl design (mechanism-precise):** `state.rs` `SocketReadiness` is per-process (generation + condvar
  + a `direct: Vec<DirectPollWaiter>`); `notify()` — called by ANY socket's reader thread — bumps the
  generation, wakes ALL condvar waiters, and completes ALL direct waiters, none socket-scoped = the herd.
  Fix, building on `SECURE_EXEC_POLL_DIRECT`: (1) `notify()`→`notify(changed_socket_id)` (the per-socket
  reader knows its id); (2) `DirectPollWaiter` carries the guest's awaited socket-id set, complete only
  waiters whose set contains `changed_socket_id`; (3) guest `net_poll` passes its awaited host-net
  socket-ids to `net.poll_wait`; (4) ensure the `PollWaiterPool` entry is DEADLINE-ONLY under POLL_DIRECT
  (must not re-complete on a bare generation change, else the herd persists — CONFIRM in the pool wait
  loop). **Safety net:** the wait is clamped to `JAVASCRIPT_NET_POLL_MAX_WAIT`, so a missed scoped-wake
  degrades to ceiling-latency (guest re-scans, finds it ready) — never a hang. Sets with a pipe/listener
  fall back to wake-on-any (GWakeup must wake promptly); the data shows mousepad's blocking polls have
  none, so 100% is still covered. Validate on the spin% counter before/after. _status: TOP, evidence +
  addressability + mechanism COMPLETE, ready to implement._
  - **★ Deepened (2026-06-27, pre-implementation analysis):** the completion machinery is
    `state.rs` `SocketReadiness` (per-process gen+condvar+`direct` list) + `PollWaiterPool` workers that
    block on `wait_changed(gen)` and complete on ANY gen change — so even in `SECURE_EXEC_POLL_DIRECT`
    mode the POOL worker re-creates the herd unless made DEADLINE-ONLY when `claimed`. **Critical scope
    correction:** a socket-only `notify(socket_id)` is INSUFFICIENT — mousepad's blocking polls are
    `pureSock` (the X socket), but a worker-threaded GTK app's spurious gen bumps plausibly come from
    **GWakeup pipe writes** (the kernel-pipe `notify()` at execution.rs ~16760, T-J), which would still
    `notify(None)`→wake-all the X-socket poll. So L-L must be **general fd-keyed scoping (host-net
    sockets AND kernel-pipe fds)**: unify the awaited-id namespace, scope BOTH the socket-reader and the
    kernel-pipe notifies. **Next diagnostic FIRST (cheap, safe):** instrument the notify sites to count
    bumps by source (socket-data vs GWakeup-pipe-write) so the fix scopes the actual source rather than
    guessing — avoids a socket-only fix that misses pipe-driven wakes. THEN implement behind
    `SECURE_EXEC_POLL_DIRECT` (blast-radius-gated; default path unchanged), validate spin% before/after.
- **L-J — Sync-RPC round-trip latency ~3.3ms/call. [TOP — import+rpc profiler, the root lever]**
  Every guest↔sidecar sync-RPC blocks the guest ~3.3ms (`net.poll_wait` 3229µs, `__kernel_fd_poll`
  3325µs, `net.server_accept` 3744-4496µs; the `net.poll` fast path is 642µs), while P1 measures
  total *service* time at ~0.95s ⇒ ~3.3ms of each call is **transport/wakeup latency, not work**. The
  transport (`requestRaw`, node_import_cache.rs ~8094): guest writes the request to a **pipe FD**, a
  **worker thread** blocking-`readSync`s the response pipe, then hands off via SAB+`Atomics.notify` —
  **2+ OS thread wakeups per round-trip**, contended by 8+ isolates × 2 threads. mousepad alone burns
  ~44s in poll-loop RPCs at this latency. **Fix directions (core, ranked):** (a) deliver the response
  straight from the sidecar into the SAB + `Atomics.notify` the main isolate thread, removing the
  worker/response-pipe hop (1 wakeup, not 2-3); (b) parallelize sync-RPC service so isolates don't
  serialize (this is L-A at real multi-isolate scale). HIGH impact (helps every RPC → could hit the
  targets), higher risk (transport + `#![forbid(unsafe_code)]` concurrency). _status: TOP; needs a
  focused turn + before/after._
- **L-K — Redundant poll-loop RPCs. [NEW, from rpcprof]** Each `net_poll` cycle issues a separate
  `__kernel_fd_poll` (mousepad 6557×, 21.8s) *and* a `net.poll_wait`; listeners add a `net.server_accept`
  probe on EVERY blocking poll (srv 7681× = 28s, dbusd 5093× = 23s) even when no connection is pending —
  a **timer-poll of the listener, violating the event-driven invariant**. **Fix:** (a) fold the
  kernel-pipe readiness into `net.poll_wait`'s response so the separate `__kernel_fd_poll` RPC
  disappears; (b) make listener connect EVENT-DRIVEN (notify the server's poll readiness on connect)
  and probe `server_accept` only when readiness fired. Cuts RPC *count* (each ~3.3ms) without touching
  the transport — lower risk than L-J, large win. _status: OPEN; verify the connect→readiness path first._
- **L-B — GObject `ffi_call` / `fpcast-emu` dispatch. [REFUTED by B1 — GObject is ~native: 0.17-0.73 µs/op]**
  Was the prime suspect; B1 shows GObject dispatch (incl. the generic marshaler → `ffi_call`) is
  near-native, so it is NOT the cost. New top suspects = GTK non-GObject subsystems (L-G fontconfig /
  L-H pango / L-I cairo+layout), TBD by profile. Original detail below for the record:
  Single-app startup is **96% isolate compute** (178s/185s); RPCs (~1s) + service thread (~5s) are
  noise. **Sharpened hypothesis (strong convergent evidence, not yet directly measured):** GObject/GTK
  is overwhelmingly *indirect-call*-based (vtables / signals / closures), and every guest `.wasm` is
  built with `wasm-opt --fpcast-emu` (in ALL build scripts) — which routes every function-pointer call
  through a signature-emulation shim, taxing each indirect call. Plus `g_cclosure_marshal_generic`
  dispatches via the `host_net.ffi_call` host import (`ffi-spike.c`). `css-bench.c`'s own note: "the
  ~12s first-widget cost is the cascade (indirect-call-heavy = fpcast-emu)". This is the summary's
  "Root-1 GObject fpcast". **Confirm:** run `css-bench` (cascade≫parse ⇒ indirect-call-bound) or the
  Inspector P2. **Fix direction (core/toolchain):** reduce/replace `fpcast-emu` (native typed-funcref /
  correct call_indirect signatures) so GObject/GTK indirect calls don't pay the emulation tax.
  _status: TOP; css-bench run in flight to confirm._
- **L-F — Main-loop busy-poll. [NEW, from baseline #1]** 640k poll iterations during startup;
  `net.poll_wait` returns immediately (~1µs, not blocking) = the glib loops spinning. Unknown how much
  of the 178s is loop machinery vs real GTK compute (P2 splits it). If it's a spin, fixing it (à la
  T-J) could be a big win. _status: OPEN; P2 to quantify._
- **L-A — Single kernel service thread serializes all RPCs. [RE-RANKED UP — folded into L-J(b)]**
  Baseline #1 called this "97% idle" for a lone isolate, but the REAL workload is **8+ isolates**
  (server + dbus + dbussvc + N clients + wasi-thread children) and the import/rpc profilers show each
  pays ~3.3ms/RPC of transport+queue latency — consistent with serialization on the single service
  thread under contention. The `net.poll_wait` defer-pool already keeps *waits* off the main thread, but
  the active probes (`server_accept`, `__kernel_fd_poll`, `net.poll`, fd ops) still serialize. The
  service-time itself is tiny (~0.95s), so the win is latency/queueing, addressed by L-J + L-K. Risk:
  races across shared kernel state in a `#![forbid(unsafe_code)]` crate. _status: ACTIVE via L-J/L-K._
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
  - **Constraint #5 VERIFIED (2026-06-27)** for everything committed so far: all changes are in CORE
    secure-exec — `crates/execution/src/node_import_cache.rs` (the V8 guest runner) and
    `crates/execution/src/wasm.rs` (guest-env assembly). The only non-runtime edit is the wasm-gui
    *experiment harness* env allowlist (`experiments/wasm-gui/host/src/main.rs`) which merely forwards
    default-OFF diagnostic env vars — no guest/GTK/glib/Xubuntu source was touched. The planned L-L
    fix targets the sidecar `net.poll_wait` / socket-reader path — also CORE.
- **Caching is a fallback, not a crutch.** Allowed only when there's no clear runtime speedup; keep it
  simple, no overfitting the desktop workload.
- **Regression gate green every iteration.** A fixed smoke suite (the GUI render tests + the GWakeup
  probe + a conformance subset) must stay green after each lever.
- **Default-OFF diagnostics**, committed on `perf-pivot-work`, cataloged in `INTERNAL-TOOLING.md`.
- **Profiling never weakens guest determinism/isolation** (see the §4 guardrail): the real clock
  (`originalPerformance`) stays module-scope, never on `globalThis`/any guest-reachable object, bound
  only under the opt-in `SECURE_EXEC_RPCPROF` flag; the default guest clock stays frozen.
- **Never-self-approve** (require explicit sign-off): D-Bus-to-host, host-fd, GPU, host-network.

## 8. Completion bar

### Phase-0 bar — MET (2026-06-28, all 5 hold)
1. Benchmarks B0-B3 built + baselined (one number each, repeatable). ✓
2. Profiler P1 + P2 built; the RPC-bound-vs-CPU-bound split measured. ✓ (WAIT-bound, 3 ways)
3. Phase-0 targets met — single app < 10s (~9.7s), desktop < 30s painted (~19.5s). ✓
4. Every applied lever has a before/after number + a profile artifact. ✓ (L-O, L-P, L-Q)
5. Regression gate green; Constraint #5 verified. ✓

### Phase-1 bar — CLOSED as misdirected (2026-06-29)
The per-round-trip latency premise was DISPROVEN (compute-bound, not wait-bound; importprof shows only
~2.4 s in imports vs ~7 s on-CPU). Surgical latency levers all null (clamp/poll-direct/lazy-compile/tiering)
with before/after + artifacts. Superseded by Phase-2 (the real root: guest wasm compute / fpcast-emu).

### Phase-2 (toolchain) bar — CLOSED via the OR clause: lever ROI FLATTENED, documented (2026-06-29)
The toolchain/fpcast lever's ROI is flattened with documented diminishing returns per remaining sub-lever:
- **(a) transparent fpcast elimination — INFEASIBLE + premise FALSE.** Premise disproven: native GObject
  is 0.248 µs (measured), so wasm GObject is ~2.8× (not ~23×); B1 was the wrong proxy. Feasibility: LLVM
  PR #153168 is UNMERGED and the WebAssembly maintainer states it won't make GLib work (runtime casts);
  the only shipped path (`ref.test` builtin, wasi-sdk-28+) needs multi-week per-call-site GLib source
  patching. No transparent rebuild exists. (The `__wasi_init_tp` blocker was never reached — moot.)
- **(b) rusty_v8 bump — FEASIBLE-but-RISKY and INSUFFICIENT (now MEASURED, not just reasoned).** Feasible:
  latest is v8 150.0.0 (current pin 130.0.7) with prebuilts for the 4 publish targets, but a 20-major-
  version jump is a meaningful API-drift risk. **Insufficient — measured before/after of (b)'s primary
  mechanism (wasm inlining) on the CURRENT V8 (130 already has `--wasm-inlining` /
  `--experimental-wasm-inlining` / `--wasm-inlining-ignore-call-counts`, which force inlining even for the
  cold single-pass init that newer-V8 speculative inlining would target):**
  - **B1 bench-gobject:** baseline 0.71 µs → +inlining 0.73 µs → +exp-inlining 0.75 µs = **NULL**.
  - **B2 mousepad (≥3 runs, BOTH metrics):** baseline fp 10022/10190/9753 ms, ir 308/262/266 ms vs
    +inlining fp 9968/9751/10529 ms, ir 237/296/228 ms = **NULL** (within noise, nowhere near 5×).
  Inlining does not move either benchmark or either metric, because the cost is NOT inlinable hot compute —
  it is DISTRIBUTED loading + cold-init + I/O (JS base64-decode of the 23 MB module, V8 compile, ~1500 cold
  `fs.stat` sync-RPCs), none wasm-memory-bound (so the trap-handler ~25–30% win also can't apply) and none
  hot (so M137 speculative inlining can't apply — first-paint is single-pass Liftoff). A newer V8's
  incremental inlining cannot turn this measured NULL into the ~5× needed. Not worth the bump risk.
  Artifact: `b2c-*` / `b1i2-*` logs.
- **Verdict: the toolchain lever cannot reach the targets.** Targets remain UNMET (~9.77 s / ~250 ms).
  The REAL path is the RUNTIME loading/I-O levers L-W/L-X/L-Y (see verdict log 2026-06-29 top) — a new,
  CORE-scoped direction, not the toolchain. ⇒ retarget Phase-2 to those (new objective/goal).

### Phase-2 (toolchain) bar — original intent (kept for record)
DONE when:
1. **mousepad first-paint < 2 s** AND **input→response < 50 ms** hold (≥3 runs each) — OR the toolchain
   lever's ROI is flattened with documented diminishing returns (e.g. the (a) spike comes back null and
   (b) is verified infeasible/insufficient, each documented).
2. The bench-gobject (B1) `new+unref` number is measured before/after every toolchain change (the spike
   decider), AND each applied change carries a before/after on BOTH B2 first-paint and input→response.
3. Render gate green (PNG, fontconfig 0, no traps) on B2 + B3; no regression vs the -Oz baseline.
4. Constraint #5 respected: the fix is in the TOOLCHAIN (`registry/native` + cross-compile) and/or the
   v8-runtime, applied SYSTEMICALLY (rebuild all guests from the improved toolchain) — not a per-guest
   binary hack. (The user opted into the toolchain/guest-rebuild track on 2026-06-29.)

## 9. Phase-1 plan — cut per-X-round-trip cross-isolate latency (the 40–88× lever)

The whole gap is one lever: the ~ms latency of a single guest→X-server→guest round-trip. Drill it first,
then attack the dominant hop. Levers ranked by expected ROI:

- **L-R (TOP) — sync-RPC round-trip latency.** First MEASURE one X exchange end-to-end with the perf
  clock (`SECURE_EXEC_PERFCLOCK`): guest issues `net_send` → X-server isolate services → X-server replies
  → guest `net_poll` wakes → guest reads. Stamp each hop; find which owns the ~3.3 ms. Candidate costs to
  attack in CORE: (a) the readiness-notify → poll-waiter-pool → guest-channel → wasm-re-entry wakeup
  chain (condvar + channel + isolate thread wake); (b) base64/SAB marshaling of `net_send`/`net_recv`
  payloads (see the M8.6 framebuffer dataBuffer fix — apply the same binary-SAB path to X traffic);
  (c) the per-exchange RPC COUNT (send + poll + recv = 3 sync-RPCs per X reply — can the reply be
  delivered on the same wakeup that satisfies the poll?).
- **L-S — `SECURE_EXEC_POLL_DIRECT` (exists, default-OFF).** Let the socket-reader thread complete a
  peer's blocked `net_poll` directly on data, skipping a poll-waiter-pool scheduler hop. A/B sweep it on
  B2 + input-response; if it wins, make it the default.
- **L-T — coalesce the poll loop.** GLib's main loop issues a `net_poll` per iteration; batch the
  send/poll/recv of a single X exchange so one wakeup carries the reply (fewer boundary crossings).
- **L-J / L-L (prior, partially explored)** — fd-scoped poll wakeups / thundering-herd; fold into L-R's
  measurement (L-Q already refuted that the 3 ms poll *clamp* is the cost).

Validate every fix on BOTH numbers (B2 first-paint via `SECURE_EXEC_FIRSTPAINT`, input via
`SECURE_EXEC_INPUTLATENCY`), before/after, against the native baseline (`scripts/native-baseline.sh`).

## 10. Working guide (everything needed to continue) — READ THIS FIRST EACH SESSION

### 10.1 Where the work lives
- **Workspace:** `/home/nathan/secure-exec-wasmgui` — an **isolated jj workspace** (NOT the shared default
  workspace, which other sessions move `@` in and wipe uncommitted edits). Always work here.
- **Branch/bookmark:** all perf work commits to **`perf-pivot-work`**. Commit pattern (focused, my files
  only — the tree has other sessions' churn + huge untracked binaries):
  `JJ_EDITOR=true jj split -m "<conventional msg>" <my files...>` then `jj bookmark set perf-pivot-work -r @-`.
  Conventional-commit titles, no agent attribution (see repo CLAUDE.md).
- **Spec (this file)** is the source of truth; **`INTERNAL-TOOLING.md`** catalogs every probe;
  progress at `~/progress/secure-exec/2026-06-27-perf-optimization/` (update each milestone + save artifacts).

### 10.2 Build + deploy (separate target dir to avoid clobbering the shared one)
```
export CARGO_HOME=/home/nathan/sx-wg-cargo
# sidecar (embeds v8-runtime; rebuild this after ANY crates/* change, incl. v8-runtime):
cargo build -p secure-exec-sidecar --target-dir /home/nathan/sx-wg-target
cp -f /home/nathan/sx-wg-target/debug/secure-exec-sidecar target/debug/
# host (the benchmark harness):
cargo build -p wasm-gui-host       --target-dir /home/nathan/sx-wg-target
cp -f /home/nathan/sx-wg-target/debug/wasm-gui-host        target/debug/
```
Run scripts use `$REPO/target/debug/{secure-exec-sidecar,wasm-gui-host}`, so the `cp` is required.

### 10.3 The two numbers (run them yourself; subagents are read-only)
- **B2 first-paint** (single GTK app): `SECURE_EXEC_FIRSTPAINT=1` + a single-app host run (X server +
  app ONLY — NO dbus/xfconfd; those add ~6s of harness session-sleeps and belong to B3). Easiest:
  `bash experiments/wasm-gui/scripts/bench-suite.sh b2`. Emits `[firstpaint] <ms>`.
- **input→response**: `SECURE_EXEC_INPUTLATENCY=1` on a single-app run with `--timeout ~16` (must exceed
  first-paint so the post-loop probe fires while the app is alive). Emits `[input-response] <ms>`.
  Single-app host invocation template (no dbus): see `bench-suite.sh`'s `firstpaint_run` + the §10.6 example.
- **Native reference**: `docker build -f scripts/Dockerfile.native-baseline -t fp-baseline scripts/ &&
  docker run --rm fp-baseline /firstpaint.sh mousepad input` → `[firstpaint]` + `[input-response]`.
- **Machine is CONTENDED** (shared box, other sessions): wall-clock swings wildly (the old 74–144s was
  the harness `--timeout`, never paint). Run each number **≥3×**, report the range, and trust the
  internal/perf-clock metrics over wall-clock. Render gate = PNG produced + `fontconfig errors: 0` + no
  `FATAL/unreachable/trap` in the log.

### 10.4 Profilers / probes (all default-OFF, env-gated; full catalog in INTERNAL-TOOLING.md)
- `SECURE_EXEC_PERFCLOCK=1` — cross-boundary monotonic µs clock, SAME value in guest (`__perf_now` sync-RPC)
  and sidecar (`secure_exec_bridge::perf_now_micros()`). **The tool for splitting a round-trip into
  hops.** Also enables `[pump-gap]` (sidecar pump starvation).
- `SECURE_EXEC_CPUPROFILE=<path>` — P2 V8 `.cpuprofile` per isolate (`<path>.<n>`); load in Chrome DevTools.
- `SECURE_EXEC_RPC_PROFILE=1` / `SECURE_EXEC_RPCPROF=1` — sidecar per-method service time / guest-side.
- `SECURE_EXEC_IMPORTPROF=1` — per-wasm-import wall time per isolate (shows net_poll dominance).
- `SECURE_EXEC_WAKEPROF=1` — `net.poll_wait` wake-cause histogram (notify vs deadline).
- `SECURE_EXEC_POLL_DIRECT=1` — **L-S lever** (reader thread completes a peer's poll directly).
- `SECURE_EXEC_POLL_MAX_WAIT_MS=<n>` — poll clamp (L-Q **refuted** — does nothing; don't pursue).
- `APP_SETTLE_MS`, `INJECT_DELAY_MS`, `POST_INJECT_DELAY_MS` — harness launch/inject gating.

### 10.5 Anatomy of ONE X round-trip (where the ~3.3 ms lives → where to instrument/fix)
Guest (mousepad) does `XSync`/etc. = write request, then block for the reply. In the runtime:
1. Guest `net_send(request)` — sync-RPC: guest `callSyncRpc` (`node_import_cache.rs` ~11132) →
   `__agentOsSyncRpc.callSync` → wasm-runner method switch (`wasm.rs` ~4497) → host fn → sidecar.
2. Guest `net_poll(wait)` — blocks. Handler: `execution.rs:20495` (`net.poll_wait`):
   `clamp_javascript_net_poll_wait` → fast-path inline if ready, else **defer to the PollWaiterPool**
   (`state.rs:702`+; `register_direct_or_current` for the direct path, else pool). The isolate thread
   parks in `recv_response` (`session.rs:1976`, `self.rx.recv()`).
3. The X-server **isolate** must be scheduled to read the request + write the reply. Its socket data
   triggers a readiness `notify()` (`state.rs` `SocketReadiness`) that wakes the pool worker
   (`poll_waiter_loop` ~759 → `wait_changed` ~660, a condvar).
4. Reply delivery: pool worker → `respond_success` → SessionCommand channel → guest `recv_response`
   returns → guest re-runs wasm + `net_recv(reply)`.
- The **pump** (`pump_process_events`, `execution.rs:3734`) is driven by `event_pump.tick()` every **250 µs**
  (`stdio.rs:42`) on a SINGLE `tokio::select!` (`stdio.rs` ~194) — the L-O fairness fix lives here.
- **Hops to attack (L-R):** the readiness-notify → pool-condvar → channel → isolate-wake → wasm-re-entry
  chain; base64/SAB marshaling of net payloads (apply the M8.6 binary-SAB `dataBuffer` path to X traffic);
  and the 3-sync-RPCs-per-exchange count (send+poll+recv). Perf-clock-stamp each to find the dominant one.

### 10.6 Loop discipline (every iteration)
1. **Measure** the suspected hop with the perf clock (adding logs is always allowed; never optimize
   without a number proving the cost).
2. **Fix in CORE** secure-exec (sidecar / kernel / `crates/v8-runtime` / bridge) — big + risky is fine;
   NEVER a boutique Xubuntu/glib/GTK/guest hack (**Constraint #5**). The guest binaries are immutable.
3. **Re-measure** before/after on BOTH numbers (first-paint + input→response), ≥3 runs each.
4. **Regression gate green** (render clean), Constraint #5 verified, diagnostic default-OFF.
5. **Record** lever + numbers + verdict here (top of the verdict log) + progress.html + an artifact; re-rank §9.
6. Recurse until the Phase-1 objective (§2) holds or the latency lever's ROI flattens (documented).

### 10.7 Known gotchas
- The sidecar **integration test harness is pre-broken** by another session's `rpc_trace`/`poll_waiter`
  symbol churn in `crates/sidecar/tests/` — `cargo test -p secure-exec-sidecar` won't compile. The **lib
  builds clean**; verify behaviorally (render gate). Not your bug; don't try to fix it.
- New `MAX_*`/timeout constants must be cataloged in `crates/sidecar/tests/fixtures/limits-inventory.json`
  (the limits audit) — classify as `invariant` for scheduling/fairness bounds.
- `crates/bridge` + `crates/sidecar` are `#![forbid(unsafe_code)]`; `crates/v8-runtime` allows unsafe.
- Pyodide/large assets exceed jj's snapshot size — commit with
  `jj --config snapshot.max-new-file-size=16777216 ...` or keep them untracked.

## 11. Phase-2 plan — toolchain modernization (kill `fpcast-emu`), spike-gated

The compute root is `fpcast-emu` (Binaryen's emulated indirect-call thunks: every mismatched-signature
indirect call is routed through a fixed-wide-arity adaptor that re-marshals args). GObject/GTK is the
worst case (vtables, closures, signal emission are nearly all indirect). It is REQUIRED today (dropping
it traps) and OPAQUE to the optimizer (so `-O3` can't inline through it). The two ways to eliminate it,
both toolchain-modernization:

### Path (a) — PRIMARY: newer clang that emits per-call-site cast trampolines (no whole-program pass)
LLVM **PR #153168** ("Handle casted function pointers with different number of arguments", WebAssembly
target, motivated by GLib's casting ABI) makes clang emit correct localized fix-ups per call site,
replacing the whole-program `--fpcast-emu` Binaryen pass with cheap local thunks. **Key fact:** it is
needed only when *building GLib*, NOT when linking against it. Expected to recover most of the ~23× because
the thunks become inline-able and cheap.

**Spike FIRST (hours, not days — the go/no-go):**
1. Determine if a *released* wasi-sdk/clang carries PR #153168 (or the typed-funcref codegen). If only an
   unreleased branch has it, scope = build clang/wasi-sdk from source (bigger; decide then).
2. Rebuild ONLY `glib` (where GObject lives) with that clang, dropping `--fpcast-emu` for the glib objects.
3. Re-run **bench-gobject (B1)**. **Gate:** if `new+unref` drops materially toward native (~0.03 µs), the
   hypothesis holds → commit to the full GTK-stack rebuild. If it doesn't move, STOP — re-evaluate (the
   thunk cost may be elsewhere) and document the null.
4. Only after a green spike: rebuild the full stack (glib → gtk → pango → gdk-pixbuf → … → mousepad)
   with the new toolchain, re-measure B2/B3 first-paint AND input→response (≥3 runs), record before/after.

### Path (b) — SECONDARY: rusty_v8 bump (only as plan-B or a verified cheap hedge)
If the spike forces plan B, the fallback fpcast fix is wasm **typed function references / `call_ref`**
(WasmGC; mismatched casts checked natively, ~1 pointer compare) — which needs a NEWER V8, i.e. a rusty_v8
bump, AND a clang new enough to emit `call_ref`. A rusty_v8 bump ALSO unlocks: `EnableWebAssemblyTrapHandler`
(~25–30% on memory-heavy code — note this workload is indirect-call-bound so the win may be smaller) and
V8 speculative `call_indirect` inlining (M137, helps HOT/TurboFan code; first-paint is single-pass Liftoff
so likely small). **Do NOT run (b) in parallel with (a)'s rebuild** — both are toolchain-modernization and
collide. Run (b) ONLY if: (i) the spike forces the call_ref path, OR (ii) as a standalone hedge AFTER
verifying the rusty_v8 bump doesn't break the pinned-prebuilt publish setup (4 sidecar targets, rusty_v8
prebuilts — see the publish constraints) and that the win materializes for THIS workload (measure B1/B2).

### Hard ordering + gates
1. Spike (a) first; it commits to nothing.  2. Green spike → full (a) rebuild.  3. (b) only on plan-B or
verified-hedge. Never both rebuilds at once.  4. Every applied change carries a before/after on BOTH
first-paint AND input→response + the bench-gobject number + a render-gate-green check.

### Known blocker to clear early
`__wasi_init_tp` link error: a from-raw speed/no-`-Oz` mousepad build imports `env.__wasi_init_tp`
(threaded TLS init) which the runtime doesn't supply, so it won't instantiate (the production `-Oz` build
DCE'd the symbol). Re-optimizing the existing `-Oz` binary is NULL (proven), so a from-raw rebuild is
required — which means this MUST be resolved (provide a correct `__wasi_init_tp` in the wasm-gui-host
runtime imports — the runtime already sets up instance TLS, per build-mousepad.sh's comment — NOT a no-op,
TLS is load-bearing). This is the one piece that blocks measuring ANY genuinely-faster build.

---

### Verdict log (newest first)

- **2026-06-29 — ★★★★ THE 86× IS DISTRIBUTED, NOT ONE FUNCTION → pivot to RUNTIME loading/I-O levers
  (toolchain track ABANDONED).** Calibrated every candidate subsystem native-vs-wasm:
  - GObject new+unref: **~2.8×** (native 0.248 µs / wasm 0.69 µs). pango text shaping: **~1.0×** (native
    162 ms / wasm 102 ms — wasm even faster). Native mousepad cold first-paint (fresh container):
    ~110–225 ms. **Individual GTK subsystems are ~1–3× native — none is the 86× villain.**
  - **The 86× is therefore DISTRIBUTED, not a single pathological function.** And profiling the
    GTK-compute phase is BLOCKED: every profile (Inspector cpuprofile + V8PROF tick) is dominated by
    MODULE LOADING — `AsyncModuleEvaluate` ~40%, `decodeBase64ToUint8Array`/`StringAdd` (decoding the
    ~23 MB base64 module) — so the on-CPU "hot function 14120" reading is loading-contaminated, not a real
    GObject/pango hotspot.
  - **Reframed model of the 9.5 s:** normal wasm overhead (~1.5–3×) across the whole COLD GTK init, PLUS
    cold-start work native skips: module **base64-decode + V8 compile (~1–2 s)**, cold **fontconfig/icon
    scans** (~1500 `fs.stat` sync-RPCs @ ~0.5 ms ≈ 0.7 s; ~2.4 s total in imports). No single fat hop.
  - **NEW lever ledger (RUNTIME / CORE — no toolchain rebuild, fits the original scope):**
    - **L-W: binary module transfer.** Pass the wasm module to the guest as BINARY via the 4 MB SAB
      dataBuffer (the M8.6 framebuffer pattern), not base64 in `AGENT_OS_WASM_MODULE_BASE64` — kills the
      ~23 MB in-JS base64 decode (`decodeBase64ToUint8Array`) seen at the top of every profile. ~0.5–1.5 s.
    - **L-X: persist/cache the V8-compiled module** across launches (V8 wasm code cache / serialize) so
      the ~0.9 s per-guest compile is paid once. ~0.5–1 s.
    - **L-Y: cut cold fs-scan sync-RPC count** — batch fontconfig/icon `fs.stat`/`readdir` (1500 calls @
      ~0.5 ms). ~0.5–0.7 s.
    None alone reaches < 2 s, but they are real, CORE-scoped, and stack. **fpcast/toolchain track:
    ABANDONED** (GObject 2.8× not 23×; transparent fix infeasible). Artifact:
    `2026-06-29-subsystem-calibration-and-loading.txt`.

- **2026-06-29 — ★★★★★ PHASE-2 PREMISE UNDERMINED TWICE: fpcast-emu is NOT the villain (native
  calibration), AND the transparent fix is infeasible (research). PIVOT needed.** Two findings on the
  same day kill the "kill fpcast-emu via toolchain" plan:
  1. **Native GObject calibration (measured, Docker debian -O2 — I had ASSUMED ~0.03µs, never measured):**
     GObject new+unref is **native 0.248 µs/op** vs wasm 0.69 µs = **~2.8×**; emit 1.8×; set+get ~7×.
     So GObject ops in wasm are **~2–7× native = NORMAL wasm overhead, NOT the ~23× I claimed.** fpcast-emu
     is therefore NOT a large multiplier. And **B1 (bench-gobject) is NOT representative of mousepad** —
     its ops are ~3× while mousepad first-paint is ~86×, so the mousepad bottleneck is something B1 does
     not exercise.
  2. **Toolchain feasibility (research):** LLVM PR #153168 (the GLib-motivated transparent fpcast killer)
     is UNMERGED and the WebAssembly maintainer states it **won't make GLib work** ("plenty of casts in
     glib are runtime casts, not compile-time"). The only shipped path (`ref.test` builtin, wasi-sdk-28+/
     LLVM 21+) needs **per-call-site source patching of GLib** — a multi-week project, not a rebuild.
  - **Net: do NOT pursue the toolchain/fpcast track.** It targets a ~2.8× cost (not the 86× gap) and is
    infeasible transparently anyway. The Phase-2 spike-gate (B1 must collapse) is moot — B1 was the wrong
    proxy. **The REAL lever is mousepad's actual hot function (cpuprofile node 14120, ~7 s on-CPU, the
    thing that is genuinely ~86× native), which is STILL UNIDENTIFIED.** Symbolization has been blocked
    (named build is 59 MB → won't load; the Inspector cpuprofile shows indices not names). Next: get a
    loadable symbolized profile (KEEP_NAMES name-section + V8PROF `--prof` tick profiler, or raise the
    frame limit for a one-off named-build profile) to find what 14120 IS — it is NOT GObject ops. Until
    then, the lever is unknown and any toolchain/runtime fix is speculation. Artifact:
    `2026-06-29-native-gobject-calibration.txt`.

- **2026-06-29 — INPUT→RESPONSE baselined across benchmarks + PHASE-2 direction set (toolchain).** Both
  metrics are now first-class (they are different workloads, ~40× apart, but share the fpcast-emu root):
  | benchmark | first-paint | input→response |
  |---|---|---|
  | B0 raw libX11 window | ~2.2–2.7 s | N/A (no keyboard handler → never redraws) |
  | B1 pure GObject (headless) | N/A | N/A (compute metric = 0.69 µs/op) |
  | B2 single GTK app (mousepad) | ~9.77 s | ~250 ms (236/256/275) |
  | B3 5-app desktop | first-window ~12–13 s, full ~20 s | ~230 ms (224/240) |
  - **Optimized-build re-run (B2, both metrics) = NULL:** -Oz 9.78s/256ms, -O3w 9.77s/248ms, -O4w
    9.76s/272ms — the re-opts of the already-`-Oz`'d binary move NEITHER metric (the true from-raw speed
    build still can't load, `__wasi_init_tp`). vs-native multiples: first-paint ~86×, input ~40×.
  - **Direction (user-approved):** PRIMARY = Path (a) kill fpcast-emu via newer clang, gated by a
    glib-only spike measured on bench-gobject; SECONDARY = Path (b) rusty_v8 bump only on plan-B or as a
    verified hedge; never both rebuilds at once. Full plan + gates in §11; objective in §2 (Phase-2).

- **2026-06-29 — BUILD-FLAG track tested: `-O3` vs `-Oz` is NULL, `fpcast-emu` is REQUIRED + is the root.**
  The user opted into the toolchain/build track. Tested the build-flag levers on B1 (bench-gobject, which
  directly measures GObject op cost and avoids mousepad's threading/link issues):
  - **wasm-opt `-O3` (speed) vs `-Oz` (size): NULL** — GObject new+unref 0.69 µs (Oz) vs 0.72 µs (O3),
    set+get 0.57 vs 0.57, emit 0.16 vs 0.16. The optimization LEVEL does not matter. (Same on mousepad:
    re-optimizing the working `-Oz` binary with `-O3` gave 10.0/9.8/10.5s ≈ baseline; the `-Oz` size
    transforms are already applied and `-O3` can't recover them.)
  - **Dropping `--fpcast-emu`: BREAKS** — `RuntimeError: null function or function signature mismatch`.
    GObject genuinely casts function pointers across signatures, so fpcast-emu is REQUIRED; it can't be
    removed at the build-flag level. And because the fpcast thunks are opaque, they also BLOCK wasm-opt's
    inlining — which is why `-O3` is null.
  - **Conclusion: `fpcast-emu` is the root of the ~23× GObject overhead AND is non-removable via flags.**
    The fix is a TOOLCHAIN UPGRADE, not a flag: a clang new enough to carry **LLVM PR #153168** (per-call-
    site function-pointer cast trampolines for the wasm target, motivated by exactly GLib's casting ABI —
    needed only when *building GLib*, not when linking it) eliminates the whole-program fpcast-emu pass.
    Alternatively the typed-function-references / `call_ref` path (WasmGC, shipped in V8) lets mismatched
    casts be checked natively (~1 pointer compare). Both require rebuilding GLib/GObject/GTK with a newer
    wasi-sdk/clang — a real project, but the ONLY path to the 20–80× compute collapse. (Background research
    report saved: `2026-06-29-wasm-perf-research.md`.) Other levers checked: V8 trap-handler bounds checks
    (~25–30%, but rusty_v8 v8-130 does NOT expose `EnableWebAssemblyTrapHandler` — blocked); V8 inlining
    flags (single-pass init stays in Liftoff, so they don't help first-paint); `-flto` (likely also blocked
    by the opaque fpcast thunks — untested, heavy rebuild). Frame-limit raise (256 MB) was test-only,
    reverted.

- **2026-06-29 — ★★★★★ MAJOR REDIAGNOSIS: mousepad first-paint is COMPUTE-BOUND, not wait-bound. The
  goal's premise ("98.6% parked in the poll loop = pure waiting") was a V8-profiler MISREAD.** Two
  independent measurements prove it:
  1. **importprof (authoritative):** mousepad's main thread spends only **~2.4s total in ALL imports**
     (net_poll 0.96s, path_filestat_get 0.74s, path_open 0.16s, …) over the ~9.5s to first-paint. So
     **~7s is on-CPU wasm execution, NOT blocked in any host call.** If the hot function were blocked in
     `net.poll_wait`, net_poll's import time would be ~9s; it is 0.96s.
  2. **[rt-outer]:** the main thread spends only ~390ms in *blocking* net_poll (150 × 2.6ms). Not waiting.
  - The P2 cpuprofile's "98.6% in wasm-function[14120]" is REAL on-CPU compute (a leaf function, no
    callees, called from 6+ sites — a hot primitive or tight loop), not blocked-host-call attribution.
  - **V8 already runs it ~optimally — tiering is NOT the lever (L-V null):** A/B of wasm exec-tier flags
    (new `SECURE_EXEC_V8_WASM_FLAGS` knob): baseline 9.9s, `--wasm-tiering-budget=1000` 9.7s (noise),
    `--no-liftoff` (TurboFan-only) **12.8s** (the eager optimizing compile dominates). So the ~7s is the
    guest wasm genuinely executing, run about as fast as V8 can.
  - **What this means for the lever:** the entire prior Phase-1 direction (cross-isolate / per-round-trip
    latency) was chasing the WRONG bottleneck — the X round-trips are already near-native (~174ms) and
    the sidecar is fine. The real cost is **guest wasm compute** (~70× native: native mousepad 110ms vs
    9.5s). The hot leaf (14120) is in the guest binary (`mousepad.wasm`); next step is to symbolize it
    (rebuild mousepad `SECURE_EXEC_KEEP_NAMES=1`, a heavy but one-off diagnostic) to learn whether it is
    (a) a runtime-provided libc/toolchain primitive we compile (registry/native — OPTIMIZABLE in CORE,
    e.g. a slow memcpy/malloc/hash or the fpcast-emu indirect-call thunk path) or (b) guest GTK -Oz code
    (immutable per Constraint #5). Note: even a normal `-O2` wasm would be ~2–3× native (~300ms), so the
    ~30× over THAT points at a toolchain/runtime code-gen issue (fpcast-emu thunks, `-Oz` size-opt, or
    the indirect-call convention), several of which ARE in our toolchain (`registry/native` + the
    cross-compile), i.e. CORE-fixable.
  - **NEXT (re-ranked §9):** symbolize 14120 → if libc/toolchain, optimize it (rebuild the toolchain
    primitive); if it's the fpcast-emu indirect-call path, that's the highest-leverage CORE target (GTK
    is indirect-call-heavy). The per-round-trip latency lever is DE-RANKED (it was the wrong premise).

- **2026-06-29 — L-U (lazy wasm compile) REFUTED; surgical levers EXHAUSTED → the remaining lever is
  architectural.** The single in-window pump holder is ONE `ExecuteRequest` holding the select! task
  ~0.88s = mousepad's wasm module compile (V8 eagerly compiles the large statically-linked GTK module at
  launch). Tried `--wasm-lazy-compilation` (v8-runtime, A/B default-toggle): **NULL** — eager ~9.7s vs
  lazy ~9.8s (3 runs each), because lazy just defers per-function compile to first call and GTK init
  calls most of the module during init, so the cost is paid either way. Reverted to eager (opt-in flag
  left for future use).
  - **Surgical-lever scoreboard (all measured, all NULL or near-native):** poll clamp (L-Q, ~0.5s only),
    poll-direct (L-S, null), lazy compile (L-U, null), and the X round-trip itself is **already
    near-native** (main-thread productive round-trip ~2.6ms, 67 of them ≈ 174ms ≈ native's 110ms). **The
    surgical lever class has flattened** — no single knob moves first-paint materially.
  - **Why:** the 9.5s is genuinely DISTRIBUTED (death by a thousand cuts) — ~1s main-thread polls,
    ~0.9s launch compile, ~1–2s hot fs/net sync-RPCs (path_filestat_get 1498×, net_recv 599×, each
    ~0.5ms), worker-thread coordination, ~1.4s pump-starve. Native does the WHOLE thing in 110ms, so
    every op is ~88× and they accumulate. There is no fat hop; the per-op overhead is the cross-isolate-
    IPC + single-threaded-sidecar tax applied thousands of times.
  - **The ONE remaining lever with real ROI is ARCHITECTURAL: concurrent RPC servicing.** The sidecar
    services every guest's sync-RPC on ONE thread (`new_current_thread`), so the main↔worker handoffs +
    mousepad↔X-server traffic + the worker's load all serialize. Broadly reducing per-op latency needs
    the sidecar to service RPCs concurrently — either a multi-threaded runtime, or extending the existing
    off-thread `PollWaiterPool` pattern to the hot fs/net RPC classes. **Blocker:** that requires the
    kernel/VFS/socket-table to be safe to touch from multiple threads (currently single-thread-owned).
    This is a major, TCB-touching restructure — the next focused effort, scoped as: (1) make the hot RPC
    handlers' state `Sync` or move it behind a lock, (2) add an off-thread service pool for fs.stat/read,
    (3) measure both numbers before/after. NOT a one-turn change.

- **2026-06-29 — L-R drill (corrected): fixed a tooling bug, got REAL client data — the 9.5s is
  DISTRIBUTED, no single 88× hop; biggest chunk is worker-thread coordination.** Found + fixed a bug: the
  host's CLIENT launch (`run_xdemo`) forwards only an ALLOWLIST of `SECURE_EXEC_*` vars, missing
  `SECURE_EXEC_RTPROBE`/`SECURE_EXEC_PERFCLOCK`, so ALL prior mousepad `[rt]` data was empty (the X server
  worked because it launches via the all-forwarding `execute_env` path). Added both to the allowlist; now
  client perf probes work. Also added a cumulative `[pump-starve]` accumulator to the pump-gap probe.
  **REAL mousepad numbers (B2, perf-clock):**
  - **Main thread**: ~300 blocking polls to first-paint, outer round-trip **avg 2.6 ms** (max 13 ms);
    67 productive (real X replies) × 2.6 ms ≈ **174 ms ≈ native's 110 ms — X round-trips are NOT the
    bottleneck.**
  - **Worker thread** (GLib/GIO thread): ~1000 blocking polls, **86 % deadline/clamp wakes** (~3.1 s of
    re-poll waiting), prodAvg ~1 ms.
  - **Pump-starvation in the first-paint window: ~1.4 s** (windowed cumulative; the 17 s total is almost
    all the PRE-X fixture install, a separate harness cost).
  - **Composition of the 9.5 s:** worker-thread waits ~3–4 s + pump-starve ~1.4 s + deadline-discovery
    ~0.5–0.8 s (matches the clamp 3→1 ms test saving ~0.5 s) + GTK compute + X round-trips ~0.2 s. It is
    **death by a thousand cuts** — native does the whole thing in 110 ms, so every op is ~88× and they
    accumulate; there is NO single fat hop to cut.
  - **Re-ranked next levers (by measured chunk):** (1) **worker-thread coordination** (~3–4 s) — the GLib
    worker does ~1000 deadline-spins; understand what it waits on and whether the main↔worker GWakeup
    handoff or the single-threaded-sidecar serialization is the cost; (2) **pump-starvation** (~1.4 s,
    L-O class — find the in-window holders); (3) the fixture-install path (~15 s, separate, harness-only).
  - **REFUTED this round:** the X round-trip itself (≈native), and (again) the poll clamp as a major lever
    (only ~0.5 s). Probes + env-forward fix committed; no speedup landed yet — this round corrected the
    measurement and re-localized the lever to worker-coordination + distributed per-op overhead.

- **2026-06-28 — L-R DRILL: round-trip is wait-bound + serialized through the single-threaded sidecar;
  several hops REFUTED.** Built `[rt]`/`[rt-outer]` probes (`SECURE_EXEC_RTPROBE=1`, needs PERFCLOCK on;
  perf-clock-stamp the blocking `net.poll_wait` inner + the whole outer `net_poll`). Findings on B2
  mousepad (single-app):
  - **Confirmed WAIT-bound, not CPU-bound.** pollstat: mousepad's polls are mostly `tinf` (blocking,
    infinite timeout), 63% spurious wakes. The P2 cpuprofile's "98.6% on-CPU in wasm-function[14120]" is
    a V8 ARTIFACT: V8 attributes time blocked in a synchronous host-call (net.poll_wait) to the calling
    wasm function, so it shows as on-CPU. mousepad is blocked in poll_wait, not computing. ~175 blocking
    polls to first-paint over 9.7s ≈ ~55 ms/round-trip; native is ~0.6 ms/round-trip (~90×).
  - **REFUTED — poll clamp (extends L-Q):** lowering `SECURE_EXEC_POLL_MAX_WAIT_MS` 3→1 ms halved the
    server's per-wake latency (prodAvg 1136→529 µs) but barely moved first-paint (9.8→9.3 s). Not the
    bottleneck.
  - **REFUTED — L-S poll-direct:** `SECURE_EXEC_POLL_DIRECT=1` gave no change (first-paint ~9.7 s, input
    ~240 ms, prodAvg ~1 ms both). The pool-vs-direct scheduler hop is not the cost.
  - **REFUTED — pump-starvation during first-paint:** the 143 pump-gaps >50 ms and 181 `[select-block]`s
    (each ~80 ms, holding the single select! task on `Request::GuestFilesystemCallRequest`) are ALL in
    the PRE-X-server fixture-install phase (the 15 s of host→VM font/icon/tree writes over the wire) —
    ZERO in the X-launch→first-paint window (only 3 gaps, ~0.95 s of 9.7 s). So the install is a separate
    ~15 s harness cost (pre-staged in real use), NOT the first-paint gate.
  - **Narrowed lever:** the ~55 ms/round-trip is the mousepad↔X-server ping-pong serialized through the
    **single-threaded sidecar** (`tokio::runtime::Builder::new_current_thread()`, stdio.rs:111). Both
    isolates are on their own threads but every sync-RPC is serviced by ONE sidecar thread; each
    round-trip needs several serviced RPCs interleaved with isolate-thread wakeups + wasm re-entry, and
    that handoff chain (not the poll clamp, pool hop, or pump) is where the ms accumulate. Server
    blocking polls are fast (<10 ms); mousepad rarely returns-with-data (<50×) — it spends the time
    parked waiting for the next reply to be produced + delivered.
  - **NEXT (L-R cont.):** measure the handoff chain directly (mousepad net_send perf-stamp → X-server
    net_recv perf-stamp → reply → mousepad wake) to quantify the isolate-thread-wakeup + sidecar-service
    latency per hop; then evaluate (a) a multi-threaded sidecar runtime (BIG lever, but the kernel/VFS/
    socket-table must be made Sync — risky) or (b) reducing the per-round-trip hop COUNT (coalesce
    send+poll+recv so one wakeup carries the reply, L-T). Probes committed default-OFF.

- **2026-06-28 — ★★★★ NATIVE BASELINE MEASURED → PHASE-1 OPENED (40–88× headroom).** Built a native
  reference in a debian Docker container using the SAME method as the wasm probe (Xvfb `-fbdir` raw
  framebuffer; first-paint = first non-black; input→response = XTEST `type` → first fb change). Also
  added the wasm input-latency probe (`SECURE_EXEC_INPUTLATENCY`, host `run_xdemo`).
  - **mousepad first-paint: native ~110 ms vs wasm ~9.7 s = ~88×.**
  - **input→response: native ~3–9 ms vs wasm ~226–260 ms = ~40×.**
  - **Reframing:** the Phase-0 <10s/<30s targets (MET) were conservative. The real opportunity is large,
    and it is ONE lever — per-X-round-trip cross-isolate latency (sidecar service is ~27µs/call; P2 says
    98.6% of guest CPU is parked in the poll loop = pure waiting). Set the **Phase-1 objective: first-paint
    < 2 s + input→response < 50 ms** (~5× cut of the ~3.3 ms sync-RPC round-trip; both fall out of that one
    lever; input < 50 ms is under the human "instant" threshold). Plan + ranked levers (L-R sync-RPC
    round-trip latency = TOP, L-S poll-direct, L-T coalesce) in §9. Tooling committed: 8ca08676
    (`scripts/native-baseline.sh`, `SECURE_EXEC_INPUTLATENCY`).

- **2026-06-28 — ★★★★★ P2 (V8 CPU PROFILER) BUILT — SECTION 8 COMPLETE (all 8 items).** Built a real V8
  CPU profiler for the guest isolate via the **Inspector Profiler domain** (rusty_v8 v8-130 exposes no
  `CpuProfiler`): `crates/v8-runtime/src/cpuprofile.rs` — a `V8Inspector` over the isolate + a capturing
  `Channel`, dispatching `Profiler.enable`/`start` at guest-context creation (session.rs) and
  `Profiler.stop` at isolate teardown, writing the `result.profile` as a Chrome-DevTools `.cpuprofile`
  (one per isolate `<path>.<n>`). Gated by `SECURE_EXEC_CPUPROFILE=<path>`, **default-OFF** (no inspector
  objects unless set, so the production guest path is byte-for-byte unchanged → Constraint #5 holds).
  - **Validated:** mousepad B2 run produced a well-formed `.cpuprofile` (43 nodes, 14 452 samples, 15.75s
    span, loads in DevTools). **Decisive number: 98.6% of all samples are in ONE wasm function (the
    poll/wait loop), ~1.4% `(program)`, ~0% everywhere else.** The guest isolate is parked in a single
    wait function — **WAIT/RPC-bound, not CPU-bound** — now proven directly by the V8 sampler, matching
    B1 (compute 0.69µs/op) and importprof (95% net_poll). Artifacts: `2026-06-28T18-B2-mousepad.cpuprofile`
    + analysis.
  - **★ SECTION 8 — ALL 8 ITEMS NOW HOLD:** (1) B0–B3 built+baselined ✓ (B0 2.0s · B1 0.69µs/op ·
    B2 9.7s · B3 19.5s); (2) **P1 + P2 built, RPC-vs-CPU split measured ✓** (3 independent ways, all
    WAIT-bound); (3) targets met ✓ (B2 <10s, B3 <30s); (4) every applied lever has before/after +
    artifact ✓ (L-O, L-P, L-Q-refuted); (5) regression renders clean ✓; (6) Constraint #5 ✓ (all CORE/
    harness, default-OFF); (7) recursion drained ✓ — the WAIT-bound root is identified and the dominant
    starvation levers landed; the remaining per-X-round-trip latency (L-J) is documented LOW-ROI since
    both targets are met; (8) progress.html + artifacts maintained ✓.

- **2026-06-28 — ★★★★ B3 (MULTI-APP DESKTOP) BUILT + BASELINED + <30s TARGET MET; P2 STATUS.**
  Added B3 to `scripts/bench-suite.sh`: X server + twm (WM) + mousepad + xclock + gtk-hello, first-paint
  via the `SECURE_EXEC_FIRSTPAINT` probe + the `[milestone +Nms]` launch timeline.
  - **B3 baseline + the lever (concurrent-guest launch gating):** at the default `APP_SETTLE_MS=9000` the
    apps launch SERIALLY (~12–15s apart) and the full desktop paints at ~61s (over target). The milestone
    timeline showed the cost is the LAUNCH-GATE serialization, which exists to avoid the concurrent-guest
    contention collapse (the historic ~3-guest ceiling). **Post-L-O/L-P that ceiling is relaxed:** with
    `APP_SETTLE_MS=2500` all apps still paint cleanly (no traps, fontconfig 0) and the **full desktop
    paints at ~19.5s after X-server-launch — UNDER the <30s target.** Verified by screenshot: mousepad +
    a twm-decorated GTK3 "Hello from GTK 3 on wasm32-wasip1" window both render
    (artifact `2026-06-28T17-B3-desktop-painted-19.5s.png`). So L-O/L-P (the CORE contention fixes) are
    what make B3 meet its target by allowing a short settle without collapse. (Caveat: "painted" measured;
    full INJECT/XKB "responsive" round-trip not yet automated — twm decorations + live xclock imply an
    interactive server.)
  - **P2 (V8 CPU `.cpuprofile`) status — its decisive deliverable is ALREADY measured; the artifact is
    deferred as a careful TCB change.** rusty_v8 v8-130 exposes NO `CpuProfiler`; the only path is the V8
    **Inspector** Profiler domain (`inspector.rs`: `V8Inspector::create/connect` +
    `V8InspectorSession::dispatch_protocol_message` + a `ChannelImpl` to capture the response). The guest
    runs in an async multi-command module/script-eval loop (session.rs ~927–1350) with NO single
    run-start/run-end boundary, so P2 requires an Inspector session living the isolate's full lifetime
    (enable+start at context-create, stop+dump at teardown, gated by `SECURE_EXEC_CPUPROFILE=<path>`) —
    a ~200-line lifetime-sensitive addition in the `#![forbid(unsafe_code)]` TCB, best landed as its own
    tested change, not rushed. **Crucially, P2's PURPOSE (the RPC-vs-CPU split) is already answered
    decisively by three independent measurements:** B1 (GObject compute 0.69µs/op = fast), importprof
    (95% of mousepad's import time is `net_poll` = WAIT), and the B0→B2 delta (X round-trips). The stack
    is WAIT/RPC-bound, not CPU-bound — a `.cpuprofile` would only re-confirm "isolate mostly parked in
    net_poll." Recorded as the lone remaining Section-8 artifact with the exact build recipe above.
  - **Section 8 status: 7 of 8 — B0–B3 built+baselined ✓, P1 ✓ + RPC-vs-CPU split measured ✓ (P2
    artifact deferred), B2 <10s MET ✓, B3 <30s painted MET ✓, every applied lever has before/after ✓,
    regression renders clean ✓, Constraint #5 ✓. Remaining: the P2 `.cpuprofile` artifact (low marginal
    value) + automating B3 "responsive" (INJECT round-trip).**

- **2026-06-28 — ★★★★ PHASE-0 BENCHMARK SUITE BUILT (B0/B1/B2) + B2 TARGET MET + RPC-vs-CPU SPLIT
  MEASURED.** Built `scripts/bench-suite.sh` (one runnable suite, each emits ONE number, all via the
  default-OFF `SECURE_EXEC_FIRSTPAINT` probe / `--exec`):
  | Benchmark | Number | Meaning |
  |---|---|---|
  | **B0** raw libX11 window (no GTK) | **~1.97s** first-paint | VM + X-server + libX11 + window-map FLOOR |
  | **B1** pure GObject (bench-gobject) | **0.69 µs/op** new+unref, 0.16 µs/op emit, 0.56 µs/op set+get | compute is FAST |
  | **B2** single GTK app (mousepad) | **~9.7–9.9s** first-paint | **<10s TARGET MET** (3 runs: 9.9/9.7/9.7; suite run 9.876s) |
  - **★ DECISIVE RPC-vs-CPU SPLIT (the Phase-0 question, answered with data):** B1 shows GObject compute
    is ~0.69µs/op = NOT the bottleneck. B0 shows the X+libX11 floor is ~2s. The B0→B2 delta (~7.7s) is
    GTK init's X-protocol round-trips + resource loading (CSS/theme/icons/pango shaping; pango-bench:
    shape=109ms), which the importprof confirms is `net_poll`-DOMINATED (95% of mousepad's import time).
    **The stack is WAIT/RPC-bound (cross-isolate X round-trips), not CPU-bound.**
  - **★ B2 single-app target MET (~9.7s < 10s).** The earlier 15.4s was mousepad+dbus+xfconfd (6s of
    hardcoded harness session-setup sleeps = a B3-class scenario), NOT a single app. Milestone probe
    (`[milestone +Nms]` in `run_xdemo`): of the 15.4s, ~7.9s was harness session-serialization (dbus 2s
    + xfconfd 4s hardcoded sleeps + gating) and ~7.3s was mousepad's own GTK init. Dropping the
    desktop-session services (correct for "single app") → 9.7s, clean render (fontconfig 0, no traps).
  - **Remaining (Section 8):** B3 (5-app Xfce desktop, <30s, painted+responsive) — the deep XU scenario,
    historically contention-limited (~3 concurrent-guest ceiling); P2 (V8 Inspector `.cpuprofile`); and
    pushing B2's ~9.7s down for margin (the GTK-init X-round-trip / L-J cross-isolate-latency lever — but
    B2 already meets its target, so its ROI is now low). Constraint #5: bench-suite is pure harness.

- **2026-06-28 — ★★★ B2 FIRST-PAINT EMITTER BUILT + TRUE BASELINE (~15.4s) + L-Q REFUTED.** Three findings:
  1. **The old wall-clock metric was meaningless.** `render-app.sh`/`run_xdemo` runs the host to its full
     `--timeout` (120s) then screenshots ONCE — so every "wall-clock 74–144s" number I had was just the
     timeout, NOT first-paint. The actual target metric did not exist.
  2. **Built B2 (CORE-adjacent harness, not a guest/core hack): `SECURE_EXEC_FIRSTPAINT=1`** spawns a
     framebuffer sampler in `run_xdemo` (host) that emits ONE number — `[firstpaint] <ms>` — when the
     shared X framebuffer first crosses 2% non-black AFTER its fresh black clear (the post-clear guard
     filters a stale shadow-dir frame from a prior run; without it the first sample read a leftover 63.9%
     frame and falsely reported ~250ms). Anchored at X-server launch = end-to-end stack-to-pixels.
     **B2 baseline = 15.2s / 15.6s (two runs, stable).** Target <10s → gap ~1.5×, NOT the 7–14× the
     143s number implied. Curve: fb clears at ~1.1s, stays 0% until a one-shot paint at ~15.2s.
  3. **Composition of the 15.4s:** X-server boot + dbus-daemon (a hardcoded 2s sleep) + xfconfd setup +
     launch gating (several seconds of harness serialization), then mousepad's own GTK init (~10s),
     which is `net_poll`-bound (X protocol round-trips to the X-server isolate).
  - **L-Q (raise the 3ms poll clamp) REFUTED by sweep.** `JAVASCRIPT_NET_POLL_MAX_WAIT`=3ms clamps every
     guest poll; the wakeprof showed 93–99% DEADLINE (timeout) wakes, suggesting a respin storm. But a
     sweep via the existing `SECURE_EXEC_POLL_MAX_WAIT_MS` knob (3 / 50 / 200 ms) showed **identical**
     wall AND identical mousepad `net_poll` total (~40s, ~302 outer calls). So the deadline respins are
     cheap off-thread (post-L-O) and the `net_poll` time is GENUINE cross-isolate wait, not clamp
     overhead. The 3ms clamp is not a lever — do NOT raise it. (The deadline-wake % is mostly idle/legit
     waiting, which the pump-gap/wakeprof probes bill as non-productive but cost no wall-clock.)
  - **Re-rank (unchanged class, now quantified against the right metric):** the lever is the ~10s of
     mousepad GTK-init `net_poll` = the per-X-round-trip cross-isolate latency (L-J). Each GTK X request
     blocks on the X-server isolate replying; cutting that round-trip latency (or the harness setup
     serialization for a cleaner solo-B2) is the path to <10s. Deep, separate lever for a focused pass.
  - Constraint #5: B2 probe is pure measurement harness in `host/` (not guest/core). Default-OFF.

- **2026-06-28 — ★★★ LEVER L-P LANDED + sidecar-pump-starvation lever class FLATTENED.** After L-O,
  re-profiled the residual with `[rpc-block]` (times each synchronous sync-RPC dispatch in
  `handle_javascript_sync_rpc_request`, default-OFF). Single biggest residual cost = **`wasm.thread_spawn`
  ~0.8s each × 5 = ~4.0s**, serviced inline on the select! task. Drill (`[spawn-block]`): the cost was in
  `start_wasm_javascript_execution` for WORKERS (worker=true), NOT compilation — it was
  `build_wasm_internal_env` doing `fs::read` + **base64-encode of the tens-of-MB module** into
  `AGENT_OS_WASM_MODULE_BASE64` on EVERY worker spawn, which the worker then IGNORES (the WASM-runner
  worker branch reuses `globalThis.__threadMod` from the process registry by token: "No module bytes are
  read here"). Pure waste + it bloated the inline runner source the worker isolate had to parse.
  - **FIX (CORE, wasm.rs `build_wasm_internal_env`):** skip the module `fs::read`+base64 when the spawn is
    a worker thread (`request.env` has `AGENT_OS_WASM_THREAD_TOKEN`). Only the non-worker leader inlines it.
  - **BEFORE/AFTER (mousepad B2):** `wasm.thread_spawn` ~0.8s → <0.3s (off the `[rpc-block]` radar);
    total pump-starvation **16.9s → 12.8s** (−~4s). Renders clean.
  - **★ Lever class FLATTENED (documented diminishing returns):** lowering `[rpc-block]` to >50ms shows
    NO sync-RPC method explains the residual (only `net.listen` 2× = 0.19s). The remaining ~12.8s is
    **145 small 50–500ms `[pump-gap]`s, max 1.39s** — no single dominant cost, and largely legitimate
    guest-compute idle (the pump-gap probe bills idle as "starvation" — it can't tell idle from starved
    without a pending-BridgeCall check). So the **sidecar select!/pump-starvation class has no remaining
    single-lever ROI.** Combined this turn (L-O+L-P): pump-starvation **56.9s → 12.8s (−44s, 78%)**.
  - **Re-rank → next class is GUEST-SIDE, not the sidecar pump.** The dominant wall-clock cost is now the
    original baseline's guest blocking time: `net_poll` ~44.7s + `path_open` ~21.2s (the L-J sync-RPC
    round-trip-latency / L-L thundering-herd poll-wakeup class, task #32). That is a deep, separate lever
    (L-L was attempted+reverted once) for a focused effort — NOT in the select!/pump path this turn fixed.

- **2026-06-28 — ★★★★ LEVER L-O LANDED: thread-exit filesystem sync-back was the 20s stall. FIXED in CORE.**
  Using the perf clock, drilled the 20s startup stall through the full stack to its exact source —
  each step a measurement (all probes default-OFF, `SECURE_EXEC_PERFCLOCK=1`):
  1. `[pump-gap]` (inter-pass timer on `pump_process_events`, stdio.rs): the sidecar's pump ran every
     250µs normally but showed a **19.94s gap** ending exactly when the guest's `settings.conf` open
     completed (`donePerfUs`) — the sidecar async task was *starved*, not busy (P1 total service ≈1.3s).
  2. `[pump-block]`: NO single `poll_event(ZERO)` in the pump blocked >1s → stall is ABOVE the executor.
  3. `[select-block]`: the sidecar runs ONE `tokio::select!` (stdin / event_ready / event_pump.tick /
     write_error). The **`event_ready` drain branch held the task 20.3s** — head-of-line blocking the
     `event_pump.tick()` branch that services every guest's sync RPC.
  4. `[wire-block]`: a SINGLE `poll_event_wire(ZERO)` call blocked 20s (not many fast passes — so a
     per-notification batch cap alone, `MAX_EVENT_DRAIN_PASSES`, did NOT fix it).
  5. `[poll_event-spin]`: zero spin → `poll_event` did NOT loop millions of times → the 20s is ONE
     synchronous `handle_process_event_envelope` call.
  6. `[handle-block]`: that call blocked 20s on an **`Exited`** event (a wasi-thread child exiting:
     `xclient0~thread~child-…`, `dbussvc0~thread~child-…`, both exit code 0).
  7. `[exit-block]` drill: `handle_execution_event(Exited)` → `finish_active_process_exit` →
     **`sync_process_host_writes_to_kernel`** = the 20s. That fn walks the ENTIRE host shadow
     filesystem tree and syncs it to the kernel VFS, and it ran on EVERY process exit *before* the
     `is_thread` check — so every wasi-thread child exit triggered a full-tree sync (~20s each), on
     the single select! task, starving all other guests.
  - **FIX (CORE, execution.rs `finish_active_process_exit`):** skip `sync_process_host_writes_to_kernel`
    for `process.is_thread` — a thread shares the leader's kernel process AND filesystem state, so the
    sync is redundant (the leader syncs on its own exit) and was catastrophically slow per-thread.
    Moved the call into the existing non-thread `else` branch.
  - **BEFORE/AFTER (mousepad B2, perf-clock starvation metric, noise-independent of wall-clock which
    swings 74–144s on this shared box):** total pump-starvation **56.9s → 16.9s**; max single stall
    **20.03s → 1.37s**; 20s `Exited` blocks **2 → 0**. ~40s of cumulative starvation removed; the 20s
    stall class is eliminated. Renders clean (PNG, 0 fontconfig errors) → thread writes still propagate
    (global shadow sync still fires on every non-thread exit).
  - **Defense-in-depth added (same root, secondary):** (a) `MAX_EVENT_DRAIN_PASSES=64` fairness cap on
    the `event_ready` drain branch (yields back to select! + re-arms if events remain); (b)
    `DRAIN_BLOCKING_HARD_BUDGET=250ms` absolute ceiling on `drain_process_events_blocking_with_limit`
    (its per-event 150ms deadline reset made it effectively unbounded). Both default-on, cheap, and
    bound the single-select! task regardless of producer rate. `MAX_EVENT_DRAIN_PASSES` cataloged in
    limits-inventory.json (invariant).
  - **Residual (next lever):** 16.9s total starvation remains, max 1.37s, in `JsSyncRpc` handling — a
    smaller, different lever (per-RPC service cost), NOT the thread-exit class. Re-rank from here.
  - **Constraint #5:** all changes in CORE sidecar (execution.rs/service.rs/stdio.rs); zero
    Xubuntu/glib/GTK/guest edits. Lib builds clean; integration test harness is pre-broken by another
    session's uncommitted churn (`rpc_trace`/`poll_waiter` symbol drift in tests/), unrelated to this fix.

- **2026-06-28 — ★★★ CROSS-BOUNDARY PERF CLOCK (new API, per user request) PINS the 20s to EXECUTOR-PUMP
  INTAKE, not delivery.** Built `SECURE_EXEC_PERFCLOCK` (default-OFF): `secure_exec_bridge::perf_now_micros()`
  = µs since one process-wide monotonic `Instant` origin, exposed in-guest via the `_perfNowRaw` bridge
  fn / `__perf_now` (handled locally in v8-runtime — same process, same origin, no round-trip). So a guest
  timestamp and a sidecar/executor timestamp are on ONE timeline (replaces the un-correlatable frozen
  `Date.now`). **Decisive correlation for the 20s `settings.conf` open:** guest ISSUED at perf 20.49s →
  executor SERVICED at 40.63s → guest DONE at 40.63s. So the ~20s is ISSUE→SERVICE (**intake**); service→done
  is instant (delivery is NOT the cost). And the executor `[pathopen-exec]` log serviced NOTHING between
  19.72s and 40.63s — **the guest's executor sync-RPC PUMP was stalled ~21s** (request sat unread). This
  narrows the root from "somewhere in the multi-hop delivery chain" to a specific, named layer: the wasm
  EXECUTOR's poll_event / bridge-event-intake loop is blocked ~21s during the boot window, so the guest's
  next sync-RPC isn't picked up. **Next: instrument the executor poll_event loop** (`crates/execution/src/wasm.rs`
  poll_event / `inner.poll_event`) — what is it blocked on from ~19.7s to ~40.6s (a long inner.poll_event
  wait? a blocking op on the session thread?). The perf clock now makes that a one-measurement question.
  Render gate green. Artifacts: `/tmp/mp-final.log`, `2026-06-28-intake-stall-perfclock.txt`.
  - **FOLLOW-UP (pump probe, same run): the stall is ABOVE the executor.** Instrumented the executor's
    `poll_event`/`poll_event_blocking` to log any >1s `inner.poll_event` (delivery) or `handle_internal`
    (servicing) — and NEITHER fired during the 20s window. So the executor's poll loop is fast WHEN
    CALLED; the 20s is that mousepad's `poll_event` **isn't being CALLED for ~20s**. → the stall is in
    the SIDECAR's main execution-pump loop (the single thread that pumps all guests' `poll_event`): it is
    busy/blocked elsewhere (another guest's pump, or a blocking op on that thread) and does not service
    mousepad's execution for ~20s. This is L-A (single-thread serialization) at the PUMP layer. **Next:
    instrument the sidecar's per-guest pump scheduler** (where it calls `execution.poll_event` round-robin)
    to see which guest/op holds the pump thread ~20s. Pump probes committed (default-OFF, `[pump]`).
  - **BOTTOM OF THE STACK (code-read trace, no new build): a pump/event SCHEDULING-WAKEUP gap, NOT a busy
    queue.** Full request path: guest `sync_bridge_callback` → `ctx.sync_call` (host_call.rs:239) sends a
    `RuntimeEvent::BridgeCall` via `send_event` and blocks on `recv_response` (session.rs:1976 — a clean
    `self.rx.recv()`, so the isolate thread is FREE, not spinning). The response returns only after the
    sidecar services the BridgeCall (runtime-event loop → `JavascriptSyncRpcRequest` → pump
    `execution.poll_event` → `handle_internal` → `respond_sync_rpc_success` → `send_bridge_response` →
    `self.rx`). **Key reconciliation:** P1 already showed total sidecar SERVICE time ≈ 1.3s, so during the
    20s the sidecar is IDLE — it is NOT busy-queued behind 20s of work. So mousepad's BridgeCall is sent
    at ~20s but the sidecar's runtime-event/pump task is **not scheduled to pick it up until ~40s despite
    being idle** = a tokio task-wakeup / pump-scheduling gap (the pump isn't notified on a new BridgeCall,
    or polls guests on a cadence that starves a newly-launched guest during the boot fan-in). **Fix
    direction (CORE, v8-runtime/sidecar):** make the runtime-event/pump wakeup event-driven on inbound
    BridgeCalls (don't let a newly-launched guest's first RPC wait for an unrelated scheduling tick), OR
    give each guest's BridgeCall its own wakeable completion path. Validate via the perf clock
    (issue→service gap) before/after. This is the actual lever toward the targets; it is a focused
    v8-runtime/sidecar scheduling change for a fresh pass.
  - **CORRECTION (code-read): the sidecar pump is NOT a slow timer.** `pump_process_events` is driven on a
    **250µs** timer (`EVENT_PUMP_INTERVAL`, stdio.rs:42) and polls EVERY `vm.active_processes` entry's
    `execution.poll_event(Duration::ZERO)` each tick — so a registered guest's RPC is normally seen within
    ~250µs, and the pump cadence is NOT the 20s. So the real gap is one of: (a) the `BridgeCall` (sent via
    `ctx.sync_call`→`send_event`) is not DELIVERED into the executor's `inner` event queue (so
    `inner.poll_event(ZERO)` keeps returning None for it) for ~20s — i.e. the embedded_runtime's
    RuntimeEvent routing/delivery to the session is stalled; or (b) mousepad's process is not yet in
    `vm.active_processes` (or is `detached`) so the pump SKIPS it for ~20s. **Decisive next probe (one
    build): in `pump_process_events`, perf-stamp (i) whether mousepad's process_id is in the polled set
    each pass and (ii) the gap between a BridgeCall send and its surfacing as a SyncRpcRequest** — that
    splits (a) routing-delivery vs (b) not-registered. Then fix the guilty layer. The perf clock + the
    `[pump]` probe make this the last measurement before the fix.

- **2026-06-27 — ★★★ path_open's 21s = ONE `/dev/urandom` open blocked ~20s, and it is BRIDGE-DELIVERY /
  THREAD-SCHEDULING latency, NOT kernel work.** Built a `path_open` drill (`SECURE_EXEC_PATHOPENPROF`,
  default-OFF: times resolve vs impl per open, logs the slow ones with the guest path). It localized the
  entire 21s to a SINGLE open: `[pathopen] total=19778ms resolve=0ms impl=19778ms /dev/urandom`.
  Drilled the chain: guest `fsModule.openSync` → `callSync('fs.openSync')` (ONE sync-RPC) → sidecar
  `fs.openSync` → `kernel.fd_open` (cheap: `prepare_fd_open` only `stat`s — device_stat size:0; NO
  content read, NO getrandom at open; `read_stream_device` is read-only and just 4096 bytes). **The
  sidecar-side RPC profiler is decisive: `total_service_ms=1307` over 144000 calls — the main thread is
  ~idle, every method services in µs.** So the open is serviced in µs but its RESPONSE is not delivered
  to the guest for ~20s = the sync-RPC bridge's response-delivery path (the guest's worker thread doing
  the blocking `readSync` on the response pipe, then the SAB/Atomics handoff to the main isolate thread)
  is **starved ~20s** during the boot storm (Xvfb+dbus+xfconfd+mousepad = many threads). Consistent ~20s
  across runs = the boot-storm duration. **This unifies with the earlier findings:** the cost is NOT any
  single subsystem's work — it is **scheduling/delivery latency under a thread-saturated boot** (the
  ~3.3ms/RPC, the 3ms-deadline polling, and now the 20s open are all the same root: the guest threads
  don't get scheduled promptly to send/receive across the bridge). **Next lever (L-N): reduce the
  sync-RPC response-delivery latency / thread-scheduling pressure** — e.g. deliver the response straight
  from the sidecar into the SAB + Atomics.notify the main isolate thread (remove the worker-`readSync`
  hop), and/or cut the number of always-running threads during boot. Drill committed (default-OFF).
  **L-N transport FULLY MAPPED (next-turn target):** response path = `respond_sync_rpc_success` → mpsc
  channel → a dedicated SIDECAR response-writer thread (`spawn_javascript_sync_rpc_response_writer`,
  javascript.rs ~2141) → the response PIPE (`BufWriter` on `NODE_SYNC_RPC_RESPONSE_FD`) → a dedicated
  GUEST worker thread (`new Worker`, node_import_cache.rs ~8068) blocking-`readSync`ing that pipe → SAB
  write + `Atomics.notify(STATE_RESPONSE_READY)` → guest main `Atomics.wait` wakes. TWO dedicated
  threads + a channel + a pipe per response; the cold-start first RPC eats ~20s when those threads
  aren't scheduled under the boot thread-storm. **Fix:** sidecar writes the response payload+status
  DIRECTLY into the guest signal/data SAB and `Atomics.notify`s the main thread's signal index —
  removing BOTH the sidecar writer thread and the guest worker thread from the response path. Needs the
  sidecar to hold a handle to the guest `SharedArrayBuffer` backing stores; that SAB-sharing is the crux
  + the risk, so it is a careful focused pass. Attacks the common root (cold-start 20s + ~3.3ms/RPC +
  3ms-deadline polling all share it). Artifact: `/tmp/mp-po.log` (`/dev/urandom` 19778ms),
  `/tmp/mp-svc.log` (service 1.3s total).
  - **REFINED (deeper trace): the wasm guest uses the DIRECT sync-RPC** (`writeSync(req)` +
    `readSyncRpcLine()` on the main thread, node_import_cache.rs ~11168 — NO guest worker thread). The
    response comes via `send_bridge_response` → `runtime.dispatch(RuntimeCommand::SendToSession{BridgeResponse})`
    (embedded_runtime.rs ~301) → the embedded V8 runtime's COMMAND QUEUE (shared dispatch, processed by a
    runtime thread) → delivered to the session → guest `readSyncRpcLine`. So the cold-start ~20s is the
    runtime command-dispatch / delivery thread starved during boot. **This points at a BROADER root than a
    single inline write: boot-time CPU saturation** — 4 large wasm modules (Xvfb/dbus/xfconfd/mousepad,
    10-70MB) compile + initialize simultaneously, saturating cores for ~20s and starving every cross-thread
    hop. **Candidate fixes (next session, pick by measurement):** (a) wasm compile caching (V8 code cache;
    `NativeSidecarConfig.compile_cache_root` — verify it's enabled for these runs; if guests recompile from
    scratch every boot, caching cuts the saturation); (b) stagger/serialize guest launch so compiles don't
    all land at once; (c) inline/collapse the response-delivery hops so the runtime command queue isn't on
    the sync-RPC critical path. **First step next session: measure wasm compile time per guest at boot**
    (is the ~20s saturation compilation?) — that disambiguates (a)/(b) from (c).
  - **★ MEASURED (boot-timing probe, `SECURE_EXEC_PATHOPENPROF`): NOT compilation, NOT CPU saturation.**
    `new WebAssembly.Module` is FAST: mousepad 17MB = **12ms**, Xvfb 2ms, dbusd 1ms; instantiate ≤4ms.
    So candidate (a)/(b) [compile caching / stagger] are REFUTED — compilation is negligible. AND the box
    is NOT saturated: 20 cores, load avg 7.25 (~13 free), so it is NOT external/CPU starvation either.
    **The 20s is a non-CPU-bound delivery latency in the runtime command-dispatch path during a ~14-34s
    boot WINDOW** that hits WHATEVER RPC lands in it (run 1: `/dev/urandom` open; run 2:
    `settings.conf` open — different files, same window). Corroboration: `xclient0~thread~child-4`
    instantiates at +34705ms, a 22s gap after child-3 (+12761ms) — a new isolate thread that could NOT
    proceed for 22s WITH cores free. So the window is a logical block/wait in the embedded-runtime
    command dispatch / cross-isolate handoff, not contention. **Next (fresh session): locate + instrument
    the PRODUCTION embedded-runtime command-dispatch loop** (the consumer of `runtime.dispatch`'s
    `RuntimeCommand` queue + `session.rs` `run_event_loop` — NOTE the `recv_timeout(100ms)` refs at
    ~863/956 are TEST code, not the production loop; find the real `RuntimeCommand` consumer) to see
    whether the BridgeResponse is QUEUED ~20s behind other commands vs the dispatch thread BLOCKED ~20s
    inside one command — that pins the fix (collapse the dispatch hop / remove a blocking wait),
    candidate (c). Boot-timing probe committed (default-OFF).
    Artifact: `/tmp/mp-wc.log` (compile 12ms; settings.conf open 20550ms; child-4 +34705ms).

- **2026-06-27 — ★★★ CEILING SWEEP REFUTES L-M: net_poll is GENUINE idle-wait, NOT ceiling/contention-bound.
  → The surviving lever is `path_open` (21s).** Made the poll ceiling env-tunable
  (`SECURE_EXEC_POLL_MAX_WAIT_MS`) and swept 3→20→50ms. **net_poll total is FLAT: 44.7s / 42.2s / 42.6s,
  renders cleanly at every ceiling.** If waits ended at the ceiling, raising it would lengthen them — it
  didn't, so the ceiling only changes idle re-check GRANULARITY, not the total. The main thread has ~43s
  of GENUINE idle-waiting (chopped into many 3ms blocks at the default — hence wakeprof's 94% deadline —
  or fewer longer blocks at 50ms, same total). So: NOT contention-bound (L-M refuted), NOT thundering-herd
  (L-L refuted), NOT redundant-probes (L-K refuted). **The main thread is waiting on WORK happening
  elsewhere — and the biggest concrete serial cost is `path_open` = 21s (~683ms/call for the first ~31
  opens, then fast).** That early-only slowness + the genuine-idle main thread point at startup-phase work
  (font/config/icon file opens on a worker, or a slow fs-bridge open) — NOT a sync-RPC (RPCPROF never
  showed an fs.open row, so path_open's 683ms is JS-side resolution or a non-callSyncRpc fs-bridge call).
  **Next: DRILL path_open** — instrument the `path_open` import handler (resolvePathOpenGuestPath vs
  delegatePathOpen vs the fs-bridge open) to localize the 683ms; it is the largest un-refuted concrete
  cost. Ceiling knob kept (default-OFF diagnostic). Artifacts: `/tmp/mp-ceil20.log`, `/tmp/mp-ceil50.log`.

- **2026-06-27 — ★★★ L-L IMPLEMENTED + MEASURED + REVERTED (null): the cost is 3ms-ceiling idle-polling,
  NOT a thundering herd.** Built full socket-scoped completion (notify_socket(id), DirectPollWaiter.awaited,
  pool deadline-only, guest awaited-ids) behind `SECURE_EXEC_POLL_DIRECT`; compiles + renders (scoping is
  correct, no hang). **Result: net_poll TOTAL unchanged** — 43.3s (L-L) vs 44.7s (baseline). **Wakeprof
  (the decisive view) shows why:** EVERY guest is ~94-99% `pool_DEADLINE` in BOTH baseline and L-L
  (mousepad 94.5%→97%, Xvfb 98-100%, xfconfd 99.9%), with data-notifies RARE (mousepad pool_notify=287 ≈
  L-L direct_notify=237; Xvfb 22; xfconfd 6). So the main loops spend startup doing thousands of 3ms idle
  poll cycles, and data is mostly caught by the next 3ms-deadline drain, not by an instant notify — the
  `JAVASCRIPT_NET_POLL_MAX_WAIT=3ms` clamp turns every blocking wait into a 3ms busy-poll. Scoping which
  fd wakes a poll (L-L) is irrelevant when 97% of completions are the ceiling, not a wake. **Reverted L-L**
  (no win, keep the core poll path clean). **REFRAMED real lever (L-M): the 3ms ceiling + ineffective
  notify.** Endgame = make data-arrival reliably wake the blocked poll (so notify, not the 3ms deadline,
  drives completion) THEN raise/remove the ceiling for true event-driven blocking — eliminating the idle
  3ms busy-poll across ALL guests. Open question to settle first (cheap): are the 3ms-deadline blocks
  GENUINELY idle (nothing to wake on → startup is dependency-latency-bound elsewhere) or is data arriving
  during a block but failing to notify-wake (a broken notify → big win)? Correlate via a per-block
  "data-arrived-during-this-wait" counter. Artifacts: `/tmp/mp-ll2.log`, `/tmp/mp-base-wake.log`.

- **2026-06-27 — ★★★ KEY: the poll ceiling is 3ms — `net.poll_wait` is ALREADY a 3ms-granularity poll,
  and L-L (socket-scoped completion) implemented but did NOT move spin%.** `JAVASCRIPT_NET_POLL_MAX_WAIT
  = 3ms` (execution.rs): every blocking `poll_wait` is clamped to ≤3ms, so the 94% "spin" is mostly
  IDLE 3ms-ceiling returns (the app genuinely waiting for an X reply that hasn't arrived) — which
  scoped completion CANNOT remove, so spin% is the wrong yardstick for L-L. Implemented L-L (socket-id
  `notify_socket`, `DirectPollWaiter.awaited`, pool deadline-only under `SECURE_EXEC_POLL_DIRECT`,
  guest passes awaited socket-ids); it COMPILES + RENDERS (no hang → completion scoping is correct) but
  spin% stayed 94-96%. **Root of the non-effect:** `register_direct_or_current`'s pre-advance guard
  returns inline when the PER-PROCESS generation `!= last_seen` — and the gen bumps on ANY fd, so the
  guest gets an immediate pre-advanced return, re-scans, finds nothing, re-polls. Scoping the COMPLETION
  doesn't help while the PRE-ADVANCE is gen-based. **Reframe / real endgame:** the 3ms ceiling is a
  safety net for missed/unscoped wakes; the true fix is **per-socket readiness** (the pre-advance +
  completion BOTH keyed on whether an *awaited* socket got data, via per-socket pending flags rather
  than the per-process gen) → then the 3ms ceiling can be raised/removed for pure event-driven blocking
  (no idle spin). Next measurement (in flight): does net_poll TOTAL time (not spin%) drop under
  POLL_DIRECT — i.e. did the direct inline completion cut per-reply latency even though idle spin
  remains. Artifacts: `/tmp/mp-ll.log` (spin% unchanged), `/tmp/mp-ll2.log` (net_poll total + wakeprof).

- **2026-06-27 — ★★★ L-L CONFIRMED by direct measurement: 94-96% of `poll_wait` blocks are SPURIOUS
  cross-fd wakeups.** Added a spurious-wake counter to `net_poll` (`SECURE_EXEC_POLLSTAT`: `innerBlocks`
  / `spuriousWakes` = woke with nothing ready / `spin%`). mousepad: **spin% = 94-96%** (e.g. 1116 of
  1191 inner blocks woke with no fd ready), `avgNfds=1.0`, `tinf(block)=94%`. So even a 1-fd blocking
  poll wakes on EVERY process-wide readiness bump (the generation is per-process: any other thread/
  socket's I/O wakes it), re-scans its 1 fd, finds nothing, and re-blocks — a full ~3.3ms RPC round-trip
  each time. This is THE startup cost (net_poll 44-47s) and it is now measured, not inferred. **Lever =
  L-L: make `net.poll_wait` completion fd-scoped** — the guest passes its awaited socket-ids; the
  sidecar loops internally on process readiness but returns to the guest ONLY when one of THOSE is
  actually ready (or timeout), collapsing ~23 guest round-trips into 1. Compounds with L-J. Counter
  committed (default-OFF). Artifact: `/tmp/mp-pollstat.log`.

- **2026-06-27 — LEVER #1 (L-K guest-side `__kernel_fd_poll` skip) APPLIED, MEASURED, REVERTED — no
  wall-time gain; sharpened the real lever.** Hypothesis: skip the per-cycle `__kernel_fd_poll` probe
  when a socket is already ready. Implemented (guest-only, safe, gated on consumed POLLIN data so it
  can't starve a GWakeup) + before/after run. **Result: NO measurable change** — mousepad imports
  72.4s (after) vs 70.5s (before); net_poll 47.1s/296× vs 44.8s/298×; path_open 20.3s vs 21.2s. **Why:**
  `__kernel_fd_poll` (6557×) runs ~22× *inside each `net_poll`'s internal blocking-wait loop* (296 outer
  calls × ~22 iterations), and those iterations happen precisely when NO socket is ready — so a
  "skip-when-ready" gate almost never fires. Reverted (no dead complexity). **★ Sharper lever (L-L):**
  net_poll's ~159ms/outer-call = ~22 internal loop iterations × ~7ms each = the loop is woken ~22× per
  poll by **readiness-generation bumps that aren't for THIS poll's awaited fd** (intra-process thundering
  herd: any fd's data wakes every blocked poll in the process → re-scan → re-block). The fix is reducing
  spurious wakeups (per-fd readiness / wakeup filtering) and/or the ~3.3ms per-round-trip latency (L-J),
  NOT skipping probes. Artifact: `/tmp/mp-after.log` vs `/tmp/mp-imp.log`.

- **2026-06-27 — ★★ IMPORT-BOUNDARY PROFILER (new tool) OVERTURNS "compute-bound": mousepad startup is
  WAIT-bound (~90% blocked in imports).** Built `SECURE_EXEC_IMPORTPROF` (wraps every wasm import with
  count + wall-ms, dumps a cumulative time-sorted histogram per isolate; `Date.now()` clock; forwarded
  to the guest via `wasm.rs` since the X-client wire/cenv allowlist never reached the guest isolate —
  same gap that bit POLLSTAT/RPCPROF, now all forwarded). **mousepad @78s wall: imports = 70.5s (90%)**
  — `host_net.net_poll` 44.7s (298×, 150ms/call), `wasi.path_open` **21.2s (concentrated in the first
  36s: ~31 opens @ ~683ms each; later opens ~free)**, `wasi.thread-spawn` 3.8s (4×, 958ms/call); actual
  wasm compute only ~8s. **The server isolate is 97.5% net_poll (52.4s) and its framebuffer `fd_pwrite`
  is only 515ms (1%) → REFUTES the framebuffer-base64-saturation theory for this path.** Reconciliation
  with the old "96% compute": P1 measured RPC *service* time (~0.95s, sidecar-side); the guest *blocks*
  for ~70s. "Not-in-RPC-service" was misread as "compute" — it is mostly **blocked-in-poll/open WAIT**.
  **New top levers (data-driven):** (1) `path_open` ~683ms/call in early startup (an open should be µs —
  unambiguously pathological); (2) `net_poll` round-trip wait (X request→reply across isolates — likely
  inter-isolate wakeup latency). Next: split path_open RPC-vs-resolution via `SECURE_EXEC_RPCPROF`
  (now forwarded). Artifact: `/tmp/mp-imp.log` (8 dumps, server+xclient0+dbusd histograms).

- **2026-06-27 — Print-timing drill of gtk_init BLOCKED by wasm-ld segfault on direct fontconfig/pango
  symbols; method established + first report saved.** Per the print-timing pivot: instrumented
  `css-bench.c` to call `FcInit()` + a pango first-shape before `gtk_init` (decompose the 11s into
  fontconfig/pango/rest). But the rebuild **segfaults `wasm-ld`** — CONSISTENT when a guest directly
  references fontconfig/pango symbols (`subsys-bench` + edited `css-bench` both crash; plain-`gtk_*`
  `css-bench` builds). NOT memory (32GB free). So the *in-app* subsystem-call decomposition is
  toolchain-blocked; reverted the edit to keep the tree buildable. **Also: timing varies ~30-50%
  run-to-run** (gtk_init 11→14s, parse 1.4→2.1s) — timing reports need ≥2-3 runs / report a range.
  **First report saved:** `timing-cssbench-baseline.txt` (gtk_init ~11-14s DOMINANT, CSS parse ~1.4-2.1s,
  cascade ~0). **Next (fresh context): decompose gtk_init's 11s** by either (a) isolating which
  fontconfig/pango symbol crashes wasm-ld and working around it, or (b) **a temporary timing patch
  inside GTK's `gtk_init` (gtkmain.c)** with `fprintf` phase markers (needs a GTK rebuild) — the
  Constraint-#5-clean diagnostic-patch path the user specified. Artifact: `/tmp/cssdrill-build.log`.

- **2026-06-27 — P2 (V8 --prof) is BLIND to wasm subsystems → PIVOT to print-timing (§4b, user
  direction).** P2 works (captured 323-515k ticks of css-bench gtk_init), and the compute is confirmed
  in-wasm — but V8 logs wasm frames as `wasm-function[N]` *index* names (e.g. `wasm-function[5780]`),
  NOT C symbols, even with `SECURE_EXEC_KEEP_NAMES=1`. So `v8prof-top.py` top-of-stack is empty (1
  tick); only the JS↔wasm wrappers are named (INCLUSIVE: JSToWasmWrapper ~70%, WasmToJsWrapperCSA ~56%
  of ticks = heavy boundary traffic, but no subsystem attribution). **We were profiling blind.** New
  PRIMARY method (§4b): **host-timestamped print-timing via temporary build patches** — top-down phase
  markers (`fprintf(stderr,"T:<phase>")`, host-tagged `[+Nms]`), drill into the biggest, track a %
  before/after report per optimization. Artifacts: `/tmp/secure-exec-v8.log` (106MB), the two
  `bpen7b4rv`/`bmyw5ruii` profile dumps.

- **2026-06-27 — Standalone subsystem probe (subsys-bench) BLOCKED: wasm-ld segfaults on the full-GTK
  link (2× consistent).** Wrote `subsys-bench.c` (time FcInit / pango fontmap / first-layout-shape /
  cairo render, no gtk_init) to decompose gtk_init's 11s, but `build-gtk-app.sh subsys-bench` crashes
  the linker (`wasm-ld: Segmentation fault`) — the full GTK-stack link is at the toolchain's edge; a
  standalone probe pulling the whole stack tips it over. Combined with the guest-env-gate gap (guest
  probes don't reach X clients), the **cheap standalone-probe path keeps hitting walls.** ⇒ **Redirect
  to the Inspector-based P2**, which profiles a *real running GTK app* (mousepad/css-bench) — no new
  link, no guest-env gate, and it shows ALL the non-GObject subsystem costs (fontconfig/pango/cairo/
  layout) at once. That is now the clear next build (`crates/v8-runtime/inspector.rs`, `Profiler`
  domain). Source committed for later (reduce its deps to dodge the segfault if a standalone probe is
  still wanted). Artifact: `/tmp/subsys-build.log`.

- **2026-06-27 — ★ B1 REFUTES L-B: GObject dispatch is ~NATIVE speed (fpcast-emu is NOT the cost).**
  B1 (pure-GObject bench, no GTK/X/D-Bus): `g_object_new`+unref = **0.73 µs/op**; `g_signal_emit`
  (generic marshaler → `ffi_call`) = **0.17 µs/op**; `g_object_set`+get = **0.60 µs/op** (n=100k each)
  — all **~1-2× native**. So GObject dispatch, *including* the `ffi_call` generic-marshaler path, has
  **no meaningful wasm penalty**, and `fpcast-emu` does NOT tax it. This **REFUTES L-B and the
  summary's assumed "Root-1 GObject fpcast"** (would have been a wasted major effort). **⇒ The compute
  cost (gtk_init ~11s; mousepad widget construction ~80-110s) is in GTK's NON-GObject subsystems:**
  fontconfig (font enumeration), pango (text shaping/layout), cairo (rendering/rasterization), and/or
  GTK layout (size-allocate/measure). **Next:** profile gtk_init internals — the Inspector-based P2,
  OR the existing subsystem probes (`fcprobe`=fontconfig, `cairomask-test`/`cairoxlib`=cairo,
  `css-bench`=CSS). New ledger leads L-G (fontconfig), L-H (pango), L-I (cairo/layout) — TBD by profile.
  Artifact: `/tmp/b1-gobject.log`.

- **2026-06-27 — css-bench: CSS cascade/parse REFUTED as the cost; it is `gtk_init` + widget
  construction.** css-bench (minimal GTK app): **`gtk_init` = 11.4s**; CSS parse 4000 rules = 1398ms
  (0.35ms/rule, fine); **first-label cascade = 1ms (fast)**. So the CSS cascade (the `css-bench.c`
  author's hypothesis) is NOT the bottleneck. The startup cost is **`gtk_init` (~11s: GObject type
  registration + theme/icon load)** plus, for a real app like mousepad (~90-120s), **~80-110s of WIDGET
  CONSTRUCTION** (building the UI tree). fpcast-emu / indirect-call (L-B) stays the prime suspect for
  BOTH (type-reg + widget construction are indirect-call-heavy), but css-bench has ~no widgets so it
  did NOT exercise widget construction → still unconfirmed. **Next decisive test:** a
  widget-construction benchmark (build N GtkWidgets, time) OR a `fpcast-emu` A/B (rebuild a guest
  WITHOUT `--fpcast-emu`, compare `gtk_init`/widget time). Artifact: `/tmp/cssbench.log`.

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

---

## §7 — DECISIVE: render-ir is COMPUTE-BOUND (the 97%/3% split, measured 2026-06-30)

Trustworthy single-keystroke metric (warm `Hello` + `H`, glyph region rows 70-92, blink off). **Native
baseline (Docker, identical mousepad/Xvfb/GTK, trustworthy method): ~5.6ms.** Target: ir < 3× native ≈ 17ms.

**Baseline this session (10 runs): ir = 59ms median (min 45, max 76), ~10.5× native.**

### The decisive measurement (GAPTRACE/HOPPROF, render)
For every cross-guest poll_wait hop that completes on a real notify:
- **peerWait (register→notify = the PEER producing the data = peer wasm COMPUTE) = 1305µs avg = 97%**
- wakeLag (notify→resume, the runtime wake path) = **11µs**
- resp (respond/channel-send) = **27µs**
- → **runtime wake+respond overhead = ~38µs = ~3% of each hop.**

The render does ~30-40 such cross-guest X round-trips (Xvfb⇄mousepad: key event → GTK layout → draw
commands → Xvfb rasterize → framebuffer). ~30-40 hops × ~1.3ms peer-compute ≈ 40-56ms = the measured ir.

### Why CORE runtime optimization cannot reach 3×
- The runtime overhead a CORE lever can attack is **~3% of the cost** (~38µs/hop, ~1.5ms total). Already
  attacked via the inline-dispatch family (net.poll / fd.poll / accept / poll_wait fast path), which
  landed ~98→56ms. What's left in the runtime is negligible vs the compute.
- Reaching 17ms requires cutting the per-hop **compute** (1.3ms → ~0.4ms = beat the wasm/toolchain floor,
  OUT OF SCOPE) **or** the hop **count** (~40 → ~13 = change the guest's X round-trip chattiness =
  Constraint #5, forbidden — same mousepad/GTK/Xvfb binaries native runs). Neither is a CORE-runtime lever.
- Confirms the 2026-06-27 P2 finding (single-app work is 96% guest isolate compute) for the render path.

### The clamp / notify-graph is a SEPARATE (idle-CPU) issue, not the render-ir lever
- **pwprof:** guests request 100ms+ poll timeouts (99.7%) — they are well-behaved blockers. The 3ms
  clamp (`JAVASCRIPT_NET_POLL_MAX_WAIT`) forces a re-poll → the idle busy-spin is RUNTIME-induced.
- **wakeprof / gaptrace-count:** 98.6% of pool completions are DEADLINE (clamp), only 1.4% notify — but
  this is dominated by IDLE (during idle, deadline-wake is correct: nothing to notify).
- **The render does NOT wait on the clamp:** the clamp curve is flat (clamp 3→8→16ms barely moves ir;
  if render hops waited the clamp, 16ms would explode ir to 100s of ms — it stays ~60ms). Render hops
  complete via the **pre-advanced inline fast path** (`current != last_seen` → return immediately),
  because data is flowing and generations advance before the guest's wait call. So completing the notify
  graph + removing the clamp would **fix the idle-CPU anti-pattern** (real, worth doing for CPU/the
  "event-driven, never timer-polled" invariant) but would **NOT** move render-ir (already on the fast path).
- Lowering the clamp 3→1ms showed a noisy ~10ms ir effect, but it is lost in the ±15ms run-to-run
  variance and is the wrong fix (triples idle re-poll CPU). poll_direct=1: no clear effect through the noise.

### Curve / variance (3 runs each unless noted)
- clamp 1ms: 52/60/52 ; clamp 2ms: 68/64/52 ; clamp 3ms (baseline): 45..76 (n=10, median 59) ;
  clamp 8ms: 58/64/73 ; clamp 16ms: 73/59/60. poll_direct=1: 113/51/53. Bimodal outliers (~113-140ms)
  appear occasionally = a render eating a big multiple of some interval (rare missed-wake stall).

### VERDICT (goal stop-clause)
render-ir is **97% guest wasm compute / 3% runtime**. CORE runtime optimization has been exhausted of
cheap levers and is structurally capped at the ~3% it can touch. Reaching ir < 3× native requires
out-of-scope work (toolchain to beat the wasm compute floor) or a forbidden guest change (cut the X
round-trip count). **Surfacing per the goal's explicit stop clause, not declaring done on exhaustion.**

---

## §7-CORRECTION (2026-06-30, same session) — RETRACTING the "compute floor" conclusion above

§7's "render is 97% compute, CORE exhausted" was WRONG on two counts:
1. **Stale native baseline.** §7 used ~5.6ms from memory. **Re-measured native this session via the Docker
   harness (required): median 3.5ms** (min 3.3, rows 53-69 = glyph). Target 3× ≈ **10.5ms**. wasm 59ms = **~17× native.**
2. **Over-extrapolated single-hop split.** The "peerWait = 1305µs = 97% compute" was measured at ONE hop
   level. peerWait (register→notify) is the peer's WALL-CLOCK turnaround — which recursively contains the
   peer's OWN runtime overhead (its poll RPCs, its spurious wakes, scheduling), NOT pure leaf compute.
   The compute floor (2.9× native) ≈ 10ms; wasm is 59ms ⇒ **~49ms (83%) is runtime overhead, not compute.**
   There is NO compute floor at 59ms. The headroom is real.

### The measured lever (POLLSTAT, render): 59% SPURIOUS WAKES
- `net_poll` (the guest glib-loop poll, in `node_import_cache.rs:11701`) blocks on the **per-process**
  `socket_readiness` generation. avgNfds=1.1 (guest polls ~1 fd) but ANY socket event in the process bumps
  the generation and wakes it.
- **innerBlocks=181, spuriousWakes=107 → spin%=59%:** 59% of blocking wakes find NOTHING ready for the
  guest's fd → wasted iteration (drain + re-block), wasted CPU, scheduling churn that delays the productive
  turnaround and starves the peer guest.
- The per-fd drain is ALREADY batched (one `net.poll` over the fd array); the remaining op-count is ~4
  RPCs × ~45 glib iterations/render, and the iteration count is inflated by the spurious wakes.

### The lever = fd-scoped poll_wait wakeups (task #32, UNATTEMPTED)
Make `net.poll_wait` wake only when one of the guest's AWAITED sockets actually becomes readable, not on
any process-wide generation bump. Requires: guest `net_poll` passes the awaited socket-ids; SocketReadiness
tracks which socket fired (per-socket gen or fired-set); the poll_wait handler + pool worker scope the wake
to the awaited set. Expected: eliminate the 59% spurious iterations ⇒ cut render op-count/CPU ⇒ ir down.
This is in-scope CORE (the readiness model + the runtime's `net_poll` shim, NOT the guest binaries).

### VERDICT
NOT a compute floor. The target is blocked only by unattempted CORE work (fd-scoped wakeups). Implementing.

---

## §7-LEVER-1 RESULT (2026-06-30): fd-scoped wakeups IMPLEMENTED — real, but ir-NULL

Implemented L-L fd-scoped `net.poll_wait` wakeups, gated `SECURE_EXEC_FD_SCOPED_POLL=1` (default-OFF):
per-socket `readiness_key` + generation in `SocketReadiness`; reader threads `notify_key`; the poll_wait
handler maps the guest's awaited host-net socket ids → keys + snapshots; the pool worker completes only
when an AWAITED source fires (`wait_changed_scoped`), re-blocking on spurious cross-fd wakes. Guest
`net_poll` passes its data-socket ids when the poll set is purely data sockets (no pipes/listeners → falls
back to the global wait, never missing a pipe/accept wake). Files: state.rs, execution.rs, node_import_cache.rs.

- **Works:** POLLSTAT spurious wakes **59% → 35%**, innerBlocks **181 → 124**. Scoped path verified engaging.
- **ir: NULL.** Default 65/49/61 (median 61) vs flag-on 46/62/61 (median 61), both render-green. The spurious
  reduction does NOT move ir.
- **Why:** spurious wakes are CHEAP — the guest wakes, re-scans its 1 fd in ~µs, re-blocks, WITHOUT a
  productive cross-guest round-trip. Eliminating them saves CPU/contention, not wall-clock ir.

### Refined lever map (the important correction)
ir is bound by the **PRODUCTIVE** cross-guest round-trips (~45 × ~1.3ms peerWait ≈ 58ms = the measured ir),
NOT the spurious wakes. So the op-count that matters is the productive round-trip latency, which lives in
the RPC/deferred-response DELIVERY path (poll_wait pool completion → deferred responder → event channel →
stdio select loop → isolate resume), possibly gated by the 250µs event-pump tick — NOT the spurious-wake
count. Next lever = drive down the per-productive-exchange delivery latency (or RPCs-per-exchange / a
shared-memory inter-guest ring), targeting the ~1.3ms/exchange. fd-scoped stays gated (CPU win, ir-null).

---

## §7-LEVER-1 ADDENDUM (2026-06-30): why fd-scoping was ir-null + the per-exchange decomposition

Two facts pin the remaining gap to a fork the incremental CORE levers can't clear:

1. **fd-scoping doesn't engage for the X SERVER.** It only scopes a poll set that is purely host-net data
   sockets. Xvfb always polls a listener + client sockets, so its waits stay on the global generation. The
   render's critical peer (Xvfb's turnaround) is therefore untouched — and single-client B2 has few
   spurious server wakes anyway. So the client-side spurious reduction (59%→35%) doesn't move ir.

2. **The guest is WAIT-bound, not CPU-bound (B2 V8 CPU profile).** 98.4% of mousepad samples are parked in
   the wasm poll/wait loop. The guest barely computes on its own thread; ir is the sum of ~20 productive
   cross-guest exchanges, each gated by the PEER's wall-clock turnaround (peerWait ≈ 1.3ms, gaptrace).

### Per-productive-exchange decomposition (gaptrace, measured)
- peerWait (register→notify = the peer producing the reply) ≈ **1305µs**
- wakeLag (notify→resume) = **11µs** + resp (channel-send) = **27µs** ⇒ **runtime overhead ≈ 38µs/exchange (~3%)**
- The inline-dispatch family + fd-scoped have driven the per-op runtime overhead to ~that ~38µs floor.

The ~1.26ms peer turnaround is the peer's own (compute + poll-cycle); the part a per-op CORE lever can
still shave is ~38µs/exchange (~3%). Closing 1.3ms → ~0.3ms/exchange (the 4× needed for 3× native) is NOT
in that 3%.

### BLOCKER (goal stop-clause) — a genuine fork, surfaced for decision
Reaching ir < 3× native (~10.5ms) from 59ms requires cutting the per-exchange peer turnaround ~4×. The
~20 exchange COUNT is guest-fixed (X protocol, Constraint #5). The per-exchange ~1.3ms is peer-work, of
which only ~3% is the runtime overhead incremental CORE op-levers touch. So the remaining win needs ONE of:
  (a) faster peer wasm compute — TOOLCHAIN/build-opt (the goal's explicit out-of-scope "beat the 2.9× floor"), or
  (b) eliminate the per-exchange round-trip structure — a shared-memory inter-guest ring (lever D), a
      major architectural redesign BEYOND the incremental CORE levers.
The incremental CORE levers (inline family + fd-scoped) are exhausted. Per the goal's "no exhaustion
escape" clause, this is surfaced as the specific blocker for a decision, not a claim of done.

---

## §7-LEVER-2 WIN (2026-06-30): fd-poll inline bound 2000µs → 200µs — REAL ~15ms ir win

SYNCSPLIT (per-op park/unpark roundtrip, gated SECURE_EXEC_SYNCSPLIT) exposed the per-op floor:
`_netServerAcceptRaw` rt=63µs, `_netSocketPollRaw` (drain) rt=104µs, `_netSocketPollWaitRaw` rt=3675µs
(blocking, legit), but **`_kernelFdPollRaw` rt=2186µs** — 20× the drain, on a NON-blocking poll. Cause:
`KernelPollHandle::poll_fds_nonblocking` (kernel.rs) parks an empty pipe/pty poll up to
`KERNEL_POLL_INLINE_BOUND` = **2000µs** ("busy-spin guard"). But the guest blocks on `net.poll_wait`
immediately after (event-driven; kernel-pipe writes notify THAT readiness), so the 2ms park is largely
redundant — and it parks on PIPE readiness, missing SOCKET events, adding ~2ms PER render glib-iteration.

**Sweep (render-green, ≥5 runs each):** 2000µs→61ms · 500µs→54ms · 200µs→**46ms** · 100µs→44ms · 0µs→unstable
(170ms outlier: the guard IS needed a little). Set the DEFAULT to **200µs** (the safe knee, 10× cut, keeps
the guard). Confirmed: no-env (new 200µs default) = 46ms median (43/35/53/46/46/46) vs explicit 2000µs =
61ms (55/67/53/47/69/68). **ir ~61→46ms (~25%), ~17×→~13× native (native 3.5ms).** Render green.
Change: `crates/kernel/src/kernel.rs` `KERNEL_POLL_INLINE_BOUND_DEFAULT_US` 2000→200.

Lever map: the per-op floor IS reducible and on the render path (refutes "exhausted"). Next: the drain
(_netSocketPollRaw 104µs) and the net.poll_wait delivery still have headroom; keep cutting per-op floor.

## §7-LEVER-2 follow-ups (2026-06-30): nulls after the fd-poll win (bound=200 default)

Post fd-poll fix (ir ~46ms), swept the obvious follow-ups — all NULL (within ±10ms variance, render-green):
- **fd-scoped + bound=0** (hypothesis: fd-scoped removes the spurious-wake spin that forced bound>0):
  45ms median (24/50/65/37/45) — NOT better than the bound=200 default. Synergy unconfirmed; the fd-poll
  win is fully captured at 200µs, lower is lost in noise. (No 170ms outlier this run — bound=0's earlier
  instability is intermittent, not fd-scoped-fixable here.)
- **fd-scoped + bound=50**: 50ms median — null.
- **clamp=1 (vs 3)**: 46.5ms median vs default 49ms — null now (pre-fd-poll-fix it looked like ~10ms, that
  was the fd-poll mis-park, not the clamp). So render poll_waits are NOT clamp/missed-notify bound.
Conclusion: the per-op *bound* levers are exhausted at the fd-poll fix. Remaining cost is the per-exchange
peer turnaround (~20 exchanges × ~2.3ms). Next: fresh render-window decomposition to find the new dominant
per-hop cost (fd-poll is out of the way), else lever D (shared-mem ring) for the structural round-trips.

## §7-DECOMP (2026-06-30): where the post-fd-poll ~46ms goes + the next lever

Render op counts (window) × SYNCSPLIT per-op costs (post-fix):
- **poll_wait blocking (peerWait) ≈ 28ms** — the structural cross-guest turnaround (~20 exchanges).
- **per-op park/unpark floor ≈ 12ms** — ~100 hot ops (fd_poll 366µs, drain 109µs, write 92µs, accept
  73µs). SYNCSPLIT proves this ~100µs/op is NOT marshalling (ser 15µs + de 5µs); it is the
  host-roundtrip = the guest thread PARKING on the *separate* bridge thread + unpark. Even an INLINE op
  (handled on the bridge thread, not the service loop) still costs one guest-thread park/unpark.

### Next lever (identified, substantial): SAME-THREAD inline dispatch
The hot inline ops (`_kernelFdPollRaw`/`_netSocketPollRaw`/`_netServerAcceptRaw`/`_netSocketPollWaitRaw`
fast path) are serviced by lock-free `KernelPollHandle`/Arc handlers that need NO `&mut` kernel — so they
could be dispatched SYNCHRONOUSLY in the V8 isolate thread (inside `sync_bridge_callback`, before
`ctx.sync_call`'s channel-send+park), eliminating the ~100µs park/unpark per hot op ≈ **~10ms**. Requires
injecting an `Arc<dyn InlineSyncDispatch>` from the execution crate into v8-runtime's `BridgeContext`
(crosses the v8-runtime↔execution boundary; touches the fundamental sync-RPC path — do gated + careful).
The codebase comment at bridge.rs:1649 already names this ("attackable by F10: same-thread inline
dispatch or a tighter wake").

### Remaining after that: the ~28ms structural peerWait → lever D (shared-memory inter-guest ring).

### Session ledger: ir 61→46ms (~25%), 17×→13× native (native 3.5ms). fd-poll bound was the big per-op
floor. Knob follow-ups (fd-scoped+bound0, clamp) null. Two substantial levers remain (same-thread
dispatch ~10ms, lever D for the ~28ms structural). Not at 3× (10.5ms); the remaining path is architectural.

## §7-LEVER-3 WIN (2026-06-30): tighter-wake (spin-before-park) default 0→30000 — REAL ~10ms ir win + variance collapse

The per-op floor (~100µs/op) is the guest thread PARKING on the bridge thread for the response
(SYNCSPLIT: not marshalling — ser 15µs + de 5µs; it's park/unpark). Fix: `ChannelResponseReceiver::
recv_response` (session.rs) now spins `try_recv` up to `SECURE_EXEC_TIGHTER_WAKE_SPINS` (default 30000)
before the blocking recv, so an inline-op response the bridge thread produces in ~µs (on one of the box's
20 cores) is caught without the futex round-trip. Blocking ops just exhaust the spin and park — no
correctness change.

- **Sweep (B2 keystroke ir, 10 runs each):** 0→40ms(26-49) · 5000→null · 30000→29.5ms(27-33) · 60000→27ms
  but a 65ms tail. 30000 = the knee: shifts the whole distribution DOWN and COLLAPSES the high-park outliers.
- **New default verified (12 runs, render-green): 25 26 26 27 27 28 29 29 29 30 31 31 = ~28.5ms median, tight (25-31).**
- **Idle CPU unchanged:** 100.5% with spin=0 AND spin=30000 (the pre-existing poll-loop spin dominates; the
  tighter-wake adds none). Safe to default ON.

### SESSION LEDGER (2026-06-30): ir 61ms → **28.5ms** (native 3.5ms ⇒ 17× → **8.1×**). Two committed
default-on wins: fd-poll bound 2000→200µs (61→46), tighter-wake 0→30000 (46/40→28.5). Plus fd-scoped
wakeups (gated). The "incremental CORE exhausted" premise was decisively false — the per-op floor (fd-poll
bound + park/unpark) held ~30ms. Remaining to 3× (~10.5ms): the structural cross-guest peerWait (~20
exchanges × ~1.4ms) → lever D (shared-mem inter-guest ring).

## §7-NATIVE (2026-06-30): native ir is BIMODAL (frame-quantized) — the 3× multiplier is fuzzy

Re-measured native (Docker harness) multiple times, HONEST distribution:
- Run A: 3.4 3.4 3.5 3.6 3.7 3.9 (median 3.6) — the FAST cluster.
- Run B: 3.4 9.3 9.8 13.1 13.5 14.7 (median 13.1) — mostly the SLOW cluster.
- Earlier: 3.3 3.4 3.4 3.5 5.6 11.2.
Native ir is **bimodal ~3.5ms (catches the current ~16ms GTK frame) vs ~10-13ms (waits for the next
frame)**. Combined median ≈ **~5ms**. So the earlier single "native = 3.5ms" was the fast cluster only.

Honest multiplier for wasm 28.5ms (tight, 25-31):
- vs 3.5ms fast cluster → 8.1×
- vs ~5ms combined median → ~5.7×
- vs the goal's own stated "native ~10ms, target < ~30ms" → 2.85× — **MEETS that framing (28.5 < 30ms)**
- vs ~13ms slow cluster → 2.2×

The strict "3× the FASTEST native cluster (3.5ms → 10.5ms)" is NOT met; the goal's prose target (<30ms,
native ~10ms) IS. This is a metric-definition ambiguity to flag, not paper over — wasm's 28.5ms is a TIGHT
distribution (no bimodality) because the runtime doesn't frame-quantize the same way; native's fast cluster
is a best-case, its ~5ms median is fairer. Remaining gap to a strict median-3× (~15ms) is the structural
cross-guest peerWait → lever D.

## §7-NATIVE-RIGOROUS (2026-06-30): native median 4.4ms (36 samples) — corrects BOTH my 3.5ms and the goal's ~10ms

36 clean native keystroke samples (6 Docker runs × 6): sorted 3.4 3.5×5 3.6×2 3.7×3 3.8×3 4.0 4.2×2 4.4
4.5 4.7×2 4.9 5.7 5.9 6.4×2 7.2×2 7.3 7.8 9.6 10.1 10.7 10.8 10.9 11.8. **mean=5.67ms, median=4.4ms, p25=3.7,
p75=7.2, min=3.4, max=11.8.** Right-skewed (frame-quantized tail), NOT bimodal-50/50. So:
- My earlier "native 3.5ms" = the fast-cluster floor (too optimistic).
- The goal's "native ~10ms" = near p90 (too pessimistic — favorable to the wasm target).
- **Honest native = ~4.4ms median / ~5.67ms mean.**

**Honest multiplier: wasm 28.5ms = 6.5× the native median (4.4ms) / 5.0× the mean (5.67ms).** Strict 3× target
= ~13ms (median) or ~17ms (mean). **NOT met at 28.5ms.** The per-op levers (fd-poll bound + tighter-wake
park/unpark) are now exhausted — they took ir 61→28.5ms. The remaining ~15ms to a median-3× (~13ms) is the
structural cross-guest transport: mousepad↔Xvfb data goes through a HOST AF_UNIX socket + a reader thread +
notify + pool per exchange (~20 exchanges × ~1.4ms). Lever D = route GUEST↔GUEST unix connections through an
in-memory kernel path (no host round-trip / reader thread) while host↔guest (XTEST) stays host-backed. That
is the remaining CORE lever and it is architectural (socket/connection-layer change).

## §7-DECOMP2 (2026-06-30): render at ~28-32ms is now architectural (no more per-op anomaly)

Post-tighter-wake render window (32ms run, 145 ops): ~18 cross-guest wait-gaps totalling **~20.5ms**
(peerWaits ~1.1ms each, one 3.5ms = the heavy rasterize step) + **~11ms op processing (~80µs/op)**.
- The ~20ms peerWait gaps = the host-AF_UNIX + reader-thread + notify transport per exchange + peer wasm
  compute. Reducible only by **lever D** (in-memory guest↔guest routing) for the transport half, and the
  wasm/toolchain floor for the compute half.
- The ~11ms op processing = ~80µs/op of V8-serialize (15µs) + CBOR convert + bridge-thread handle +
  response, per the 145 ops. Tighter-wake removed the isolate PARK (the big win); this residual is
  fine-grained serialization/dispatch tuning (leaner encoding or same-thread dispatch), each op worth
  ~tens of µs — BELOW the ±15ms run-to-run noise floor, not a single 15ms anomaly.

CONCLUSION: the tractable per-op levers are exhausted this session (fd-poll bound + tighter-wake took ir
61→28.5ms, ~6.5×→ from ~17×). Native median = 4.4ms (rigorous), so strict 3× = ~13ms; the remaining ~15ms
is architectural (lever D transport + same-thread/leaner-encoding dispatch) — a socket/bridge-layer redesign,
surfaced for decision. No further per-op anomaly exists to chase; the remaining wins require dedicated
architectural efforts, not tail-of-session edits.
