# Full Xubuntu boot — reliable render first, reasonable launch times second

**This file is the working database.** Read it top-to-bottom each session. Keep it current: append every
theory, every fix attempt (with before/after numbers + approve/reject), and every learning. The `/goal`
points here.

**★ REFRAMED 2026-07-02 (determinism-first).** The 2026-07-02 investigation proved the blocker is NOT
incremental speed — it is **DETERMINISM**: the full desktop renders only ~1/3 of runs (2/3 total-black),
and the cause is **host-thread scheduling contention during concurrent boot** (the X server's notify/reader
threads are starved by the many per-isolate + per-bridge + per-socket threads oversubscribing the cores, so
it re-polls every 50 ms per request and flakily stalls — measured: Xvfb 90.9 % deadline-driven). So this is
ONE focused architectural fix, not a loop of small perf experiments.

---

## Objective + acceptance (PHASED)

**Goal: full Xubuntu (xfwm4 + xfce4-panel + xfdesktop + Thunar + apps) rendering RELIABLY, then at a
reasonable launch time.**

- **★ PHASE 1 — DETERMINISM (top priority, the real blocker):** the full desktop renders every time.
  ACCEPTANCE: a FULL render (fb coverage ≥ 40 %, all launched components painted) on **≥ 5 consecutive runs
  with ZERO total-black**, at any reasonable time. Measured with `scripts/measure-boot.sh` (FULL_MIN=40).
- **PHASE 2 — REASONABLE SPEED (only after Phase 1 holds):** drive launch time down. Target **~≤ 15 s**
  deterministic (best-effort toward ≤ 10× native ≈ 5.4 s, which becomes reachable once concurrent boot
  works: wall ≈ slowest single app ~2-4 s ≈ 4-7× native). Re-measure native (`scripts/native-ir`,
  2026-07-02 = 0.54 s) the same session so any multiple is honest.
- **Focus the work on the ONE root:** concurrent-boot thread contention / the X-server wakeup (below).
  This is not a "try many small levers" loop — it is a specific engineering fix. Don't chase launch-timing
  tuning or per-app parity until Phase 1 render is reliable (both were already rejected — see Known-nulls).

**Constraints:** fix in CORE only (`crates/{sidecar,execution,v8-runtime,kernel,bridge}`) or the app
*build*/fixtures. **Never modify the core X server (Xvfb) source or build.** No guest-binary edits. Commit
wins to the `wasm-gui-desktop` branch (PR #104).

---

## ★ STATUS — PHASE 1 (determinism) ACHIEVED ✓; now on PHASE 2 (speed)

**★ 2026-07-02: PHASE 1 IS MET.** `SX_SERIAL_LAUNCH=1` = 5/5 zero-black FULL renders (95-99.9 %), ~61 s.
Root CONFIRMED = concurrent-boot CPU/thread oversubscription (serial boot converges every time). See the
top Fix-Log entry. **Now driving Phase 2 (reasonable launch time, ~≤15 s):** (1) binary-search the minimum
reliable `SX_SERIAL_SETTLE_MS` (6 s black, 12 s reliable → find the floor); (2) trim fixed dbus 2+4 s sleeps
→ readiness polls; (3) carefully re-introduce LIMITED concurrency (launch in pairs / cap concurrent
wasm-compiles) to break the ~11 s/app serial floor toward wall ≈ slowest app.

### (historical) TOP PRIORITY that led here — the OVERSUBSCRIPTION root

**★ UPDATED 2026-07-02: the X-server-wakeup theory below (lever 1) is DISPROVEN — see the top Fix-Log
entry.** Lever 1 was fully implemented + measured: the inline peer-notify fires 1500+ times/boot, all clients
pair correctly, and it changes NOTHING (still 1/3, deadline% unchanged). The 90.9 % Xvfb deadline% is normal
IDLE-polling, not a stall. So the wakeup graph is NOT the bug.

**THE LEAD IS NOW LEVER 2 — CPU / host-thread OVERSUBSCRIPTION.** In a black run the CLIENTS' deadline%
jumps (25 %→62-72 %): the apps are starved in their OWN init, not waiting on X. We run 6+ heavy wasm isolates
(GTK ~15-22× native compute) + ~30-40 host threads on the cores; native runs 5 procs with ~0 contention.
Validate first (cheap): render reliability vs guest count (3 vs 5 apps), and live host-thread count in a
black vs rendered boot. Then reduce pressure: fold per-socket reader threads into ONE shared poll loop per
VM, and/or cap/serialize the concurrent wasm COMPILE (the CPU-heaviest boot phase — "4 large guests
compiling at once", node_import_cache.rs). Everything below (lever 1) is kept for the record but is DONE.

### (DISPROVEN, kept for record) lever 1 — inline/synchronous cross-process peer-notify

### ★ THE PLAN — lever 1: inline/synchronous cross-process peer-notify (the direction we're going)
When a guest writes to a guest↔guest AF_UNIX socket, wake the PEER process INLINE, in the write handler
itself (no notifier thread, so nothing to starve — this is exactly how native "writer wakes reader" works).
Concrete implementation:

1. **Add `peer_readiness: Option<Arc<SocketReadiness>>` to `ActiveUnixSocket`** (state.rs). It holds the
   readiness of the process on the OTHER end of this socket (whom to wake when WE write).
2. **Establish the pairing at connect/accept** (execution.rs, the sidecar has `&mut self` → all processes):
   - CLIENT→SERVER (the confirmed-broken direction, do FIRST): at `net.connect` to a host path, find the
     process that owns a `unix_listeners` entry at that path (the X server), and set the client socket's
     `peer_readiness = server.socket_readiness`. `SocketReadiness` is per-PROCESS, so waking the server
     process is enough — its `poll_wait` re-scans all its sockets and finds the request.
   - SERVER→CLIENT (already ~OK — clients are 25 % deadline — do as a follow-up): pair the accepted socket
     with the connecting client's readiness (record the client readiness on the pending connection at
     connect, read it at accept).
3. **Notify inline on write:** in the `net.write` unix branch (execution.rs ~21935), after `write_all`
   succeeds, call `peer_readiness.notify()` (cheap: atomic gen bump + condvar). Happens exactly when the
   client produces the request → the server's blocked `poll_wait` wakes immediately, scheduling-independent.
4. **Gate it** (`SECURE_EXEC_INLINE_PEER_NOTIFY=1`, default off) so it can A/B against the WAKEPROF baseline;
   flip default-on once validated.

**Validate:** `SECURE_EXEC_WAKEPROF=1` — success = **Xvfb deadline % collapses toward the clients' ~25 %**
(the leading indicator the wake now lands before the 50 ms deadline), AND render becomes reliable
(**≥5 consecutive zero-black runs**, FULL coverage ≥40 %). If deadline % drops but render is still flaky,
the remaining stall is elsewhere (re-diagnose with WAKEPROF on the black run).

### Fallback — lever 2: reduce host-thread count during boot
If lever 1 is insufficient: the per-socket reader threads + per-isolate threads oversubscribe the cores
(native runs procs on cores with ~0 contention). Fold per-socket reader threads into ONE shared poll/epoll
loop per VM so a handful of guests don't spawn dozens of contending threads.

---

## The premise (why this is fixable, not physics) — 2026-07-02 three-agent investigation

Native boots this exact desktop in **0.54 s / 0.17 CPU-s on ONE thread**, with **zero degradation** as
clients are added (16 GTK apps started at once → all painted in 43 ms). Our 60-170 s + total-black is **~10×
above even the compute floor** — so it is NOT the compute floor. The cost splits three ways:

| bucket | our cost | native | nature |
|--|--|--|--|
| **Runtime liveness/scheduling** (the catastrophe) | ~10× above floor; total-black | 0 | **CORE-fixable, sidecar-only** |
| **Environment-parity bugs** | ~500-650 ms/app | ~0 | **fixable, no toolchain** |
| **Compute floor** (wasm exec of GTK/GObject, already TurboFan) | single-app first-paint ~1.5-1.7 s irreducible; ir ~30 ms | 0.12 s / 5 ms | toolchain only (last ~10×) |

**Interaction (typing/clicking a live window) is near-native (~30 ms, flat under load).** The brokenness is
entirely boot: liveness stalls + starvation + parity, all above the compute floor.

Full evidence: `~/progress/secure-exec-wasmgui/2026-07-02-wasm-compute-perf/`:
- `2026-07-02-total-stop-findings.md` — the total-black root cause (below).
- `2026-07-02-native-baseline-expectations.md` — native numbers + the floor arithmetic.
- `2026-07-02-startup-profile-findings.md` — single-app 2.4 s phase breakdown + parity bugs.
Related memory: `wasm-gui-m8.6-futex-storm-rootcause.md` (an OLDER, now-absent spin mode — don't confuse).

---

## Theory backlog (prioritized — attack top-down; re-rank as we learn)

### T1 — Total-stop: head-of-line blocking on the single sync-RPC dispatch task ★ HIGHEST
**Measured mechanism:** the sidecar has ONE cooperative dispatch task (stdio `select!` + `pump_process_events`)
that conveys every guest's every RPC. Two ops block it synchronously:
- **`ExecuteRequest`** (launch next app) waits up to the full **30 s wasm-prewarm timeout**
  (`DEFAULT_WASM_PREWARM_TIMEOUT_MS`, `crates/execution/src/wasm.rs:57`) — measured 30.1 s holds.
- **`Exited`** teardown of a guest wasi-thread child blocks **~52 s** — a circular wait (joining a thread
  parked on an RPC the blocked task must itself deliver).
While held, ALL guests (incl. Xvfb + dbus) park **untimed** in `recv_response` at 0% CPU → nothing paints;
holds chain (30+54+30 ≈ 114 s) = the 150 s+ black runs.
**Things to try:** (a) move the prewarm wait OFF the dispatch task (async ExecuteRequest); (b) make `Exited`
teardown async + bounded — find the 52 s blocker; (c) time-bound `recv_response` with a loud WARN so an
untimed wait can never stop the world; (d) promote the `[select-block]`/`[pump-gap]` probes (behind
`SECURE_EXEC_PERFCLOCK=1`) to default-on WARN. Interim proof knob: `SECURE_EXEC_ASYNC_POLL=0` never freezes
(but serializes ~34 ms/op).

### T2 — Serial boot vs native parallel boot ★ (the ≤10× enabler)
Native boots the 5 apps IN PARALLEL (procs on cores) → wall ≈ slowest app, not the sum. If our guests boot
serially (single dispatch task + settle-gating), boot = Σ per-app. To reach ≤10× (≤5.4 s) the guests must
boot concurrently so wall ≈ one app's first-paint (~1.6-2.4 s) + contention. **Investigate:** does the
single dispatch task force serial RPC service across guests even when they could run in parallel isolates?
Can dispatch be sharded/parallelized per-VM or per-guest so N guests boot at once? (Depends on T1.)

### T3 — Notify-graph completion (the re-poll starvation) ★
**Measured:** 82-99% of `net.poll_wait` completions are **50 ms deadline expiries, not notifies** — the guest
polls, nothing notified it, it waits the whole clamp, re-polls. This is the idle/starvation re-poll storm
(distinct from T1). The accept-notifier (commit 617c2152) closed the accept edge; the agent says the rest
are the majority. **Things to try:** find the remaining event sources that change fd readiness without
`SocketReadiness::notify()` (data on already-drained sockets, writable/POLLOUT drain, kernel-VFS fb readiness,
glib timer wakeups), and notify them; then idle guests block instead of re-polling, freeing X-server
throughput. Verify via the deadline-vs-notify ratio (wakeprof) dropping.

### T4 — Environment-parity bugs (per-app startup, ~500-650 ms) 
- **Fontconfig rescans every font every boot:** vm-tree staging gives files fresh mtimes → `FcDirCacheValid`
  always fails → full scan (~400 ms). Fix: preserve/normalize mtimes in staging, or run `fc-cache` once and
  persist the cache into the fixture.
- **No `icon-theme.cache`:** 6,799 host syscalls under `/usr/share/icons` vs native 26 (~110 ms). Fix: ship
  a built `icon-theme.cache` in the icon fixture.
- **`settings.conf` EFAULT write loop** (GIO keyfile save → "Bad address"): ~60-120 ms + a correctness bug.
  Fix in the wasi fd_write/GIO path.

### T5 — Toolchain (LAST — separate project, only if a proven floor blocks ≤10×)
fpcast-emu is REQUIRED (mousepad traps without it) + already tuned @64; V8 already TurboFan. The remaining
~10× single-app compute is toolchain: eliminate fpcast-emu at the source (fix fn-ptr casts), typed function
references, LTO (currently off), SIMD. Record here IF the loop proves runtime fixes can't reach ≤10×.

---

## Known-nulls (disproven — do NOT retry)
- **Multi-threading the X server** — X dispatch is single-threaded BY DESIGN (native too); input_thread only
  reads input devices. Not the bug.
- **V8 wasm tiering** — compute already TurboFan; `--no-liftoff` is WORSE; low tiering-budget unchanged.
- **fpcast max-func-params @64→@25** — no first-paint change (padding already tuned at @64).
- **Dropping fpcast-emu** — mousepad traps (real signature mismatches).
- **Bigger poll clamp (150 ms / 2000 ms)** — larger just exposes more notify-graph gaps (cascade timeout);
  50 ms is the current sweet spot.
- **--desktop overhead (XTEST client delay, fb-streamer rate)** — no effect on the stall.
- Old M8.6 timeout=0 / futex-storm spin — ABSENT in all 7 total-stop runs (different, 0%-CPU mode now).

---

## Fix log (append-only; newest at top)

_(template)_
### <date> — <theory id>: <one-line change> — APPROVED / REJECTED
- Hypothesis: …
- Change: …(files)…
- Boot-time before → after (≥3 runs): … ; native this session: … ; multiple: …×
- Single-app first-paint / ir regression check: …
- Verdict + why: …
- Learned / new theories: …

### 2026-07-02 — SESSION-2 measured baseline (RELEASE build, new harness) — no fix yet
- **Harness built this session:** `scripts/measure-boot.sh` + `scripts/window-test/inside-measure.sh`
  measure desktop-boot-time = wall from host launch until GUEST-fb coverage settles (all apps painted),
  mirroring native `mode_desktop` coverage-settle. Emits `BOOT_MS`. Runs N× for determinism. Native
  denominator via `scripts/native-ir` Dockerfile.desktop (`MODE=desktop`).
- **Native this session (Docker, 1024×768):** coverage settled **548 ms** (99.9%), ir median 6.4 ms.
  → ACCEPTANCE target ≤10× = **≤5.48 s**.
- **Ours (RELEASE, 800×600, 1 run):** `BOOT_MS=83100` (95.1% coverage — fully renders, NOT ∞) = **151.6×**.
  Note: total-black/∞ from session-1 (debug) did NOT reproduce on release — release boots deterministically
  finite. (Still need ≥3 runs to confirm determinism; 1 run so far.)
- **★ROOT CAUSE CONFIRMED (T1), directly measured** via `SECURE_EXEC_PERFCLOCK=1` select-block probe:
  `[select-block] stdin/request branch held 30161066us (Request::ExecuteRequest)` — exactly ONE app's
  `ExecuteRequest` held the single dispatch task for **30.16 s** = the 30 s `DEFAULT_WASM_PREWARM_TIMEOUT_MS`.
  All other pump-gaps ≤265 ms. Mechanism: `execute()` (execution.rs:3336) calls
  `start_execution_with_net_drain` SYNCHRONOUSLY on the async dispatch task → `prewarm_wasm_path`
  (wasm.rs:908) runs the module's warmup `_start` (GTK main loop never returns) and burns the full 30 s
  prewarm-timeout poll (wasm.rs:4920) with ZERO warm benefit (a timed-out warmup returns `None` metrics).
  Only ONE app timed out (others warmed <1 s, below the 1 s probe threshold). mousepad (17 MB) already
  skips prewarm (>16 MB `MAX_SYNC_WASM_PREWARM_MODULE_BYTES`); the 30 s is one of xfwm4/panel/xfdesktop/thunar.
- Committed so far (pre-loop): 50 ms poll clamp restore (f2d512f0), accept-notifier (617c2152).
- Next: T1.a — cap the wasm WARMUP-EXECUTION poll timeout short (the block is the warmup run, not
  materialization). Expected: remove ~28 s of the one 30 s block → boot ~83 s → ~55 s (~1.5×), 151× → ~100×.

### 2026-07-02 — T1 diagnosis (probes): thunar prewarm = a 30 s CIRCULAR DEADLOCK
- `[prewarm-probe]` (per module, Rust timing): Xvfb 229ms, dbus 45ms, xfconfd 56ms, xfwm4 241ms,
  xfce4-panel 202ms, xfdesktop 225ms → all `ok(none)` (exit fast at runner line 9526, `prewarmOnly`
  honored); **thunar 30143ms → timeout**; mousepad 0ms (>16 MB skip).
- `[prewarm-stuck-none]` dump for thunar: **blocked (poll None) after 30 000 ms with 0 rpcs, empty
  stderr** — the prewarm ISOLATE never ran a single line of guest code. It is stuck in
  instantiation, not in the app.
- `[pump-starve]` probe: **cumulative 38.8 s of dispatch-pump-NOT-running over 551 gaps** during an
  ~81 s boot (≈48 % of boot the pump is blocked). The single thunar 30 s block is the dominant chunk;
  ~8.8 s more is spread over ~550 smaller gaps (50 ms clamp waits etc.).
- Mechanism = **circular deadlock**: `execute()` (execution.rs:3336) runs `start_execution_with_net_drain`
  SYNCHRONOUSLY on the single dispatch task → `prewarm_wasm_path` `poll_event_blocking(30 s)` blocks that
  task → thunar's prewarm isolate stalls during instantiate (a C++ static-init `pthread_create` / GIO
  thread needs a sync-RPC the blocked dispatch task must itself deliver) → neither side progresses → 30 s
  timeout. The 4 lighter GTK apps hit `process.exit(0)` at runner line 9526 before any thread spawn, so
  they exit in ~200 ms. mousepad skips (>16 MB). Only thunar spawns a thread during instantiate → only
  thunar deadlocks.
- Fix direction: prewarm's compile-cache warmup NEVER pays off for one-shot long-lived GUI/server guests
  (invoked once) and can deadlock. T1.b = skip the prewarm EXECUTION cleanly (never start the isolate →
  no 30 s block, no deadlock, no mid-init terminate corruption). Real run compiles fresh (~200 ms/app,
  overlaps boot). Measure render + boot.

### 2026-07-02 — T1.b: SKIP the prewarm execution (env toggle) — ⚠️ PARTIAL (removes 30 s block; flaky)
- Change: `SECURE_EXEC_WASM_SKIP_PREWARM=1` skips `prewarm_wasm_path` entirely (never starts the isolate).
  Why this works where T1.a didn't: prewarm + real run SHARE `javascript_context_id`; T1.a TERMINATED the
  prewarm mid-setup → corrupted the shared context → black. Skipping never creates/terminates it.
- Boot-time: **1/3 rendered at 53.3 s** (vs 83 s baseline — removes exactly the ~30 s thunar block),
  **2/3 total-black (BOOT_TIMEOUT, cov 0.0)**.
- Verdict: ⚠️ removes the 30 s deadlock (real win when it renders) but exposes a **pre-existing total-black
  FLAKINESS** that is the true acceptance blocker. Also: prewarm's compile-cache MAY have been masking
  concurrent-compile contention → skip could worsen the black rate (measuring baseline black rate now).
- Also confirmed net-negative reasoning: prewarm compiles ON the dispatch task (blocking) while the real
  run compiles on its own isolate thread anyway → prewarm's "compile-cache benefit" is largely illusory
  and it is a pure dispatch-task cost. Principled gate for a real fix = skip prewarm when the module
  imports SHARED memory (threaded, wasi-threads) — those are exactly the ones that deadlock the sync
  prewarm; `validate_module_limits`/`extract_wasm_module_limits` already computes `memory_shared`.

### 2026-07-02 — baseline (prewarm ON) black rate: 1/3 render (83.4 s) — flakiness is PREWARM-INDEPENDENT
- Ran baseline (prewarm ON, no changes) 3×: 1/3 rendered @83.4 s, 2/3 total-black. IDENTICAL reliability to
  T1.b (skip prewarm, 1/3). ⇒ the total-black flakiness is INDEPENDENT of prewarm. So skip-prewarm is a
  strict win (53 s vs 83 s when it renders, same black rate) and safe to adopt.
- **Finalized skip-prewarm as a principled CORE fix (not just the env toggle):** skip the prewarm execution
  when the module imports SHARED memory (threaded / wasi-threads) — `wasm_module_is_threaded()` via the
  existing `extract_wasm_module_limits().memory_shared`. Those are exactly the modules that deadlock the
  synchronous prewarm; single-threaded CLI tools still prewarm (no regression). Env override
  `SECURE_EXEC_WASM_SKIP_PREWARM=1` forces it for any module. (Build pending.)

### 2026-07-02 — T0/paint-gate: gate launches on guest-fb coverage rise — ❌ REJECTED + ★metric bug found
- Change (host, PAINT_GATE): launch next client only after the previous PAINTED (fb coverage rose).
- Result: 2 runs "settled" but at only **4.8 % coverage = PANEL ONLY** (xfdesktop background never painted),
  launches spread to ~30 s each (all launched at 114 s). ❌ REJECTED (made default OFF, kept opt-in).
- Root flaw: the WM (xfwm4) produces ZERO coverage on its own (a WM paints only decorations around OTHER
  windows), so the coverage-rise signal never fires for the WM hop → 30 s fallback → slow serial launch →
  xfdesktop left unrendered.
- **★MEASUREMENT BUG FOUND (fixed):** `COV_MIN=3 %` counted panel-only (~5 %) as a successful boot. A FULL
  Xfce desktop settles ~95 % (xfdesktop light background + panel + windows); panel-only ~4.8 %. Added
  `FULL_MIN=40 %`: settle now requires ≥40 % coverage, so degraded renders are NOT scored as success.
  (Baseline/T1.b good runs were genuinely 95.1 %, so their scores stand; only paint-gate's 4.8 % was bogus.)
- Corrected picture: silence-gate FULL-renders at 95 % when it works (1/3), skip-prewarm makes that 53 s vs
  83 s. Paint-gate degraded to panel-only. So the SILENCE gate + skip-prewarm is the current best default.

### ★★2026-07-02 — the 30 s prewarm deadlock was ACCIDENTALLY STAGGERING launches (reliability confound)
- Corrected-metric (FULL_MIN=40 %) 3-run of the default (silence-gate + skip-prewarm-threaded): runs 1-2 =
  TOTAL black (maxcov **0.0 %** — the X server produced ZERO framebuffer content; all 5 clients launched at
  normal spacing 12/24/34/43/44 s, all silent). vs baseline (prewarm ON) = 1/3 full render.
- **Insight:** thunar's 30 s prewarm block was serialising the launch (thunar's `execute` held the dispatch
  task 30 s → the other apps' launches spread out), which REDUCED concurrent-boot contention. Removing it
  (skip-prewarm) makes launches tighter → MORE contention → the X-server-stuck race fires more often → worse
  reliability. So skip-prewarm trades 30 s of ACCIDENTAL stagger for speed, and the stagger was a (bad,
  deadlock-based) reliability crutch. Relying on a deadlock for stagger is unacceptable; the real fix is
  (a) the X-server-stuck ROOT and (b) DELIBERATE staggering, not a prewarm deadlock.
- ⇒ skip-prewarm-threaded is still a correct CORRECTNESS fix (no threaded guest should deadlock prewarm 30 s)
  but it is NOT a desktop reliability win on its own. Numbers are small-sample (33 %-ish success needs ~10
  runs to measure); treat the direction, not the exact rate.
- **Root to fix next (T0-root): the X server flakily gets STUCK producing 0 % coverage** — all clients block
  on it. Consistent with a lost wakeup on the X server's `net.poll_wait` (T3 notify-graph: 82-99 % of
  poll_wait completions are deadline expiries, so the server relies on the 50 ms re-poll; if a re-poll also
  misses readiness under load, it stalls). Diagnose with SECURE_EXEC_WAKEPROF on a black run.

### 2026-07-02 — heavy deliberate staggering (APP_SETTLE_MS=15000) — ❌ REJECTED (panel-only)
- Hypothesis: replace the accidental prewarm stagger with a deliberate long silence-gate → reliable render.
- Result: maxcov **4.8 % = PANEL ONLY** (xfdesktop background never paints), consistently. Same failure as
  paint-gate. ⇒ SLOW serial launch (15 s/app or paint-gate's 30 s) makes xfdesktop fail to paint its
  background even given 180 s. So there is a bad "sweet spot": too-fast launch → total black (0 %),
  too-slow launch → panel-only (4.8 %); the 30 s-prewarm + 6 s-settle combo happened to land in the narrow
  band that sometimes fully renders (1/3). Launch-timing tuning is NOT the fix.
- ⇒ The desktop render is genuinely, deeply flaky (X-server/concurrent-boot). The ONLY robust fix is the
  T0-root (X server data-arrival wakeup / dispatch concurrency), not launch gating. Confirmed the T3 gap:
  a guest writing to a peer's AF_UNIX socket notifies only the WRITER's own `SocketReadiness`
  (execution.rs:12852 host-TCP notifier; the guest↔guest data edge is missing), so the X server re-polls
  every 50 ms for client requests — under concurrent boot this stalls it flakily. NEXT-ITERATION ROOT FIX:
  notify the PEER process's `SocketReadiness` on a guest→guest socket write (a real kernel/socket-table
  change; would give BOTH determinism AND speed).

### ★2026-07-02 — Part-B code trace: the "missing data-arrival notify" hypothesis was WRONG
- Traced the guest↔guest AF_UNIX path in the merged tree. The X client↔server connection is a HOST-backed
  `ActiveUnixSocket` (real host `UnixStream`), NOT the kernel socket table. `ActiveUnixSocket::from_stream`
  (execution.rs:1912, used by BOTH `connect` and `accept`) spawns a per-socket READER THREAD
  (`spawn_unix_socket_reader`, 12872) that, on data arrival, ALREADY calls `wake(readiness)` →
  `readiness.notify()` (12895) for the READER's process. So the X server IS woken the instant a client
  writes — the data-arrival edge is wired. (The un-notified `kernel.socket_write` at 1687 is the TCP /
  kernel-socket path, which the X server does NOT use.)
- ⇒ The flaky total-black is NOT a missing data-notify. Candidate real mechanisms (need WAKEPROF/data to
  disambiguate): (a) the per-socket reader THREAD is starved under concurrent boot (host thread contention)
  so the notify is delayed/lost; (b) a race between `readiness.snapshot()` in the inline poll and the
  reader-thread `notify()`; (c) coalesced/lost notify across many fds (per-PROCESS readiness generation is
  coarse); (d) something upstream (the X server itself stalls before serving). NEXT: run one boot with
  SECURE_EXEC_WAKEPROF=1 to get the X server's poll_wait wake-cause breakdown in a black vs a rendered run.

### ★★★2026-07-02 — WAKEPROF DATA: the X server is 90.9% DEADLINE-driven (root confirmed)
- Ran a desktop boot with SECURE_EXEC_WAKEPROF=1 (per-process net.poll_wait wake-cause histogram). Result:
  | guest | total wakes | deadline% | notify |
  |--|--|--|--|
  | **Xvfb.wasm (X SERVER)** | 782 | **90.9 %** | 65 |
  | xfwm4 (WM client) | 2350 | 29.7 % | 1130 |
  | xfdesktop | 1130 | 24.6 % | 673 |
  | xfce4-panel | 1235 | 24.9 % | 749 |
  | dbus-daemon | 1227 | 49.4 % | 621 |
  | xfconfd | 3276 | 50.6 % | 1229 |
- **★ROOT CONFIRMED:** the X SERVER is ~91% DEADLINE-driven (woken by the 50 ms re-poll, not by a notify)
  while its CLIENTS are ~25% deadline (well notify-driven). So client REQUESTS do not wake the server —
  it discovers each request up to 50 ms late via re-poll. Under concurrent boot (many requests) this 50 ms
  per-request latency starves the single-threaded X server → it falls behind → flaky total-black.
- **Code location:** the server's accepted client sockets (`net.server_accept` unix branch, execution.rs
  ~21840) ARE created with `ActiveUnixSocket::from_stream(.., process.socket_readiness)` (a reader thread
  that would notify) AND `register_inline_unix_socket` (the C-lite inline drain). The inline drain consumes
  incoming data on-poll, so the reader thread rarely sees data to notify → the server is not proactively
  woken. The asymmetry: CLIENTS (connect side) are notified of server replies by their reader thread;
  the SERVER (accept side, inline-registered) is not notified of client requests.
- **★FIX DESIGN (next: implement + validate):** a NON-CONSUMING data-notifier per accepted inline socket,
  modelled EXACTLY on `spawn_unix_listener_accept_notifier` (execution.rs:12950): a thread that
  `nix::poll`s the accepted socket fd for POLLIN and calls `readiness.notify()` on the readable edge
  (throttled/backoff so a not-yet-drained socket can't storm), WITHOUT reading the bytes (the inline
  drain still does the real read). This wakes the X server the instant a client request lands → its
  deadline% should collapse like the clients' (~25%), removing the 50 ms-per-request latency → should fix
  BOTH the flaky stall (determinism) AND the boot speed. Gate behind an env flag for A/B, measure Xvfb
  deadline% + render reliability with it on vs off. Wire the notifier's stop into the socket's Drop (as
  the accept-notifier does via ActiveUnixListener).

### 2026-07-02 — data-notifier (non-consuming POLLIN wake per accepted socket) — ❌ REJECTED (thread-starved)
- Implemented `spawn_unix_socket_data_notifier` (execution.rs, gated `SECURE_EXEC_DATA_NOTIFIER=1`, default
  OFF): a per-accepted-socket thread that `nix::poll`s the fd for POLLIN and `notify()`s the server's
  readiness on the readable edge, non-consuming (inline drain still reads). Modeled on the accept-notifier.
  Kept gated-off as a documented negative result + Drop-based stop wiring.
- A/B (DATA_NOTIFIER=1, WAKEPROF): Xvfb deadline% stayed **~90-94 %** (unchanged from the 90.9 % baseline);
  run still total-black. Smoking gun: the Xvfb **notify count FROZE at 107** while total wakes grew
  550→1863 — i.e. the notifier fires early then can't keep up: its notifies land AFTER the 50 ms deadline
  already completed the poll, so each still counts as a deadline wake.
- **★KEY LEARNING (reframes the root):** the wakeup bottleneck is **host-THREAD SCHEDULING**, not a missing
  notify edge. Under concurrent boot there are already many host threads (per-guest isolate + bridge +
  per-socket reader threads); the X server's notifier/reader threads are STARVED and cannot notify faster
  than the 50 ms re-poll. Adding MORE notifier threads (this fix) makes contention worse, not better. So
  neither the reader-thread notify nor a data-notifier can fix it while it is thread-based.
- **New root-fix directions (next):** (a) SYNCHRONOUS/INLINE cross-process notify — when a client's net.write
  runs, look up the paired server socket → server readiness and `notify()` INLINE (no thread), so the wake
  is immediate and scheduling-independent (needs a client-socket↔server-socket peer registry, since the X
  path is host-backed UnixStreams); (b) REDUCE host-thread count during boot (the per-socket reader threads
  + isolate threads oversubscribe the cores) so the existing notifies aren't starved; (c) revisit whether
  the single dispatch task + N isolates is simply oversubscribing CPU (native uses procs on cores, ~0
  contention). (a) is the most principled and matches native's "writer wakes reader synchronously".

### ★★★2026-07-02 — LEVER 1 (inline peer-notify) — ❌ REJECTED, and it DISPROVES the X-server-wakeup root
- Implemented lever 1 fully: `peer_readiness` on `ActiveUnixSocket`, client→server pairing via a global
  `host_path → Weak<SocketReadiness>` registry (register at `net.listen`, look up at `net.connect`), inline
  `peer.notify()` in the `net.write` unix handler. Gated `SECURE_EXEC_INLINE_PEER_NOTIFY=1` (default off).
- **The mechanism works perfectly** (diagnostic probes): both servers register (X0 + dbus), ALL 5 X clients
  + 4 dbus clients pair `paired=true`, and the inline notify **fires 1500+ times per boot**.
- **But it changes nothing:** 3-run = 1/3 render (unchanged), Xvfb deadline% ~88-94 % (unchanged), Xvfb
  notify-wakes only ~128 despite 1500+ `notify()` calls (most fire while the server is NOT blocked → no-op).
- **★★ROOT CORRECTION — the X-server-wakeup was a RED HERRING.** The X server IS woken fine; the 90.9 %
  deadline% is just normal IDLE-polling (the X server polls with a 50 ms timeout and is mostly caught-up /
  waiting for the next request — that is not a stall). Making the wakeup instant (lever 1) does NOT fix the
  flaky total-black. So the whole "X-server request-wakeup latency" theory is disproven by direct measurement.
- **★ WHERE THE STALL ACTUALLY IS (new lead):** the flaky black is CPU / host-THREAD OVERSUBSCRIPTION. In a
  black run the CLIENTS' deadline% jumps (25 % → 62-72 %) — the apps are starved in their OWN init, not
  waiting on X. We run 6+ heavy wasm ISOLATES (each GTK at ~15-22× native compute) + ~30-40 host threads
  (per-isolate + bridge + per-socket reader + accept/data notifier threads) on the cores; native runs 5
  PROCESSES on cores with ~0 contention. Under this oversubscription some guests intermittently make no
  progress → flaky black. This is **lever 2 territory (reduce host-thread count / concurrency pressure)**,
  now the lead — NOT the wakeup graph.
- Lever 1 code kept gated-off (correct mechanism, zero-cost when off; useful reference). Debug eprintlns
  only fire when the feature is on.
- **NEXT:** validate the oversubscription hypothesis before a big lever-2 build — e.g. measure render
  reliability vs guest count / vs pinning fewer host threads, or count live host threads during a black vs a
  rendered boot. Then reduce threads (fold per-socket reader threads into one shared poll loop per VM;
  cap/serialize concurrent wasm COMPILE which is the CPU-heaviest boot phase — the "4 large guests compiling
  at once" note in node_import_cache.rs).

### 2026-07-02 — concurrency sweep (3 apps vs 5): oversubscription is PARTIAL, not the whole story
- 3 apps (xfwm4 + xfce4-panel + xfdesktop; still 6 guests incl. X + dbus + xfconfd): **2/3 render** @59s vs
  5 apps **1/3**. Fewer guests helps a bit → oversubscription CONTRIBUTES. But 3 apps is STILL 1/3 black, so
  it is not the sole cause: even a minimal 6-guest desktop stalls flakily. And boot ≈59s for BOTH 3 and 5
  apps → boot-time is dominated by FIXED overhead (dbus 2+4s sleeps + the settle-gating), not app count
  (a Phase-2/speed detail, not the Phase-1 blocker).
- ⇒ The flaky black is a timing-sensitive race present even at 6 guests — reducing threads (lever 2) may
  raise reliability but is unlikely to fully fix it alone. Open sub-questions before a big lever-2 build:
  (i) in a black run does the X SERVER keep serving after the initial accepts, or does IT stall? (ii) is
  there a specific EARLY guest (dbus/xfconfd/X) that intermittently fails to come up, blocking the rest?
  (iii) does the single dispatch task (pump_process_events, which still serves fs sync-RPCs) get monopolised
  by one guest in a black run? Instrument per-guest first-RPC / last-RPC timeline in a black vs rendered run
  to see WHICH guest stalls first, rather than assuming oversubscription.

### ★2026-07-02 — per-guest wakeprof, BLACK vs RENDERED: a CASCADING GTK-app stall (not one culprit, not X)
- Ran 3× with WAKEPROF (default 5-app), got 1 rendered + 2 black, compared per-guest deadline%:
  | guest | RENDERED | BLACK |
  |--|--|--|
  | xfdesktop (paints the bg = coverage) | **19 %** | **52-57 %** |
  | xfwm4 (WM) | 31 % | 53 % |
  | xfconfd (settings daemon) | 54 % | 65 % |
  | Xvfb (X server) | 88 % | 92 % (idle-polls the same either way) |
- **The X server is NOT the discriminator** (idle-polls ~90 % in both) — reconfirms lever 1's disproof. The
  discriminator is that the GTK apps + xfconfd **all stall together** in black runs (much higher deadline%),
  and race ahead together in rendered runs. It is a CASCADING stall, not one stuck guest (thunar/mousepad
  have low activity in BOTH — not the cause). xfdesktop stalling → no background paint → cov 0 → "black".
- So the flaky black is a **systemic concurrent-boot convergence problem**: the GTK apps depend on each
  other + xfconfd + the X server, and under contention the whole graph sometimes fails to converge to a
  painted state. There is no single wakeup/thread fix; it is emergent from the concurrency.
- **Pragmatic directions (need a steer / pick one):**
  - **A. Serialize-to-ready (determinism over speed):** launch each app only after the previous is CONFIRMED
    up (a real per-app ready signal — window mapped on X, or generous fixed dwell), never piling on. Trades
    launch time for reliability. (Naive paint-gate + heavy-stagger already failed — need a BETTER ready
    signal, e.g. query the X server for the app's mapped window, or gate xfdesktop specifically on the bg.)
  - **B. Cap the concurrent wasm-COMPILE storm** (node_import_cache.rs "4 large guests compiling at once"):
    serialize/limit concurrent `new WebAssembly.Module` so the CPU spike during co-boot doesn't starve the
    convergence. Cheapest lever-2 test.
  - **C. Reduce host-thread count** (fold per-socket reader threads into one shared poll loop per VM).
- Lever 1 (wakeup) DONE/disproven. Oversubscription PARTIAL (3 apps 2/3 vs 5 apps 1/3). Boot ~59 s is
  fixed-overhead-bound (dbus sleeps + settle-gate), independent of app count.

### 2026-07-02 — Phase 2: serial SETTLE is NOT the speed lever (8 s = 12 s = 61 s) — per-app init dominates
- `SX_SERIAL_SETTLE_MS=8000`: 5/5 render (95-99.9 %) but **median 60.9 s — identical to 12 s (61 s)**. So
  shortening the settle does NOT speed boot: each app stays CHATTY for ~11 s during its own init (resetting
  `last_activity`), so the quiet-window barely starts before the app is genuinely done. The ~61 s = ~5 apps
  × ~11 s serial init + ~6 s fixed dbus sleeps. The bottleneck is PER-APP INIT (wasm compute floor), not the
  settle. ⇒ settle-tuning is a dead-end for speed.
- **Real Phase-2 levers (reliability-vs-speed tension):** fully-serial is 5/5 but slow (61 s); ANY naive
  concurrency re-introduces the flaky black (3 concurrent = 2/3). To go faster AND stay reliable we must fix
  the OVERSUBSCRIPTION root itself, not serialize around it:
  1. **Cap concurrent wasm-COMPILE** (the CPU-spike) while letting the rest of init run concurrently — a
     cross-isolate compile semaphore (guests ask the sidecar before `new WebAssembly.Module`, one at a
     time). Lets apps co-boot (fast) without the compile-storm (reliable). The principled speed+reliability
     fix. CORE change.
  2. **Launch in small batches** (2-3 at a time) instead of fully serial — cheap host-side test of how much
     concurrency the graph tolerates. (Sweep showed 3 concurrent = 2/3, so pure batching may not be enough
     without #1.)
  3. **Trim the fixed dbus 2+4 s sleeps** → readiness polls (~6 s, cheap, orthogonal).

### ★★★2026-07-02 — SERIALIZE-TO-READY WORKS: Phase 1 (determinism) ACHIEVED + concurrency CONFIRMED as root
- Definitive test — heavy serial boot (`APP_SETTLE_MS=12000`, honest FULL_MIN=40 metric): **5/5 FULL renders
  at 95.1-95.2 % coverage, ~72 s, ZERO black.** vs concurrent (default) ~1/3. This CONFIRMS the root: the
  flaky total-black IS concurrent-boot oversubscription — booting the heavy wasm guests one-at-a-time
  removes the contention and the desktop converges every time.
- ⇒ **Phase 1 (reliable full render, ≥5 consecutive zero-black) is MET by serial launch.** Trades launch
  time (~72 s) for determinism — exactly the determinism-first goal. Phase 2 then drives 72 s down (it is
  ~60 s of settle-gate stagger + fixed dbus sleeps; can be tightened / partially re-parallelised carefully).
- ★ **`SX_SERIAL_LAUNCH=1` VALIDATED: 5/5 FULL renders (3× 95.1 %, 2× 99.9 %), ~61 s median, ZERO black.**
  Dedicated opt-in (host `run_desktop`): strictly one app at a time, gated on the current app having become
  ACTIVE (emitted output) AND then quiet for its own `SX_SERIAL_SETTLE_MS` (default 12 s — its OWN var, so
  the harness's APP_SETTLE_MS=6 s can't silently override it; 6 s was too short → 5/5 black, the bug found +
  fixed), with a `SX_SERIAL_APP_TIMEOUT_MS` safety cap. Slightly FASTER than the fixed-12 s proxy (61 vs
  72 s) because it advances as soon as each app settles. **This is the Phase-1 fix — shippable as an opt-in.**
- NOTE this also retro-explains the earlier "heavy-stagger → panel-only 4.8 %" reject: that was PRE-MERGE;
  codex's "Fix wasm GUI desktop default launch" changes (now merged) + serial launch together give the
  reliable 95 % render. Good example of why re-measuring on the current tree matters.
- **Phase 2 backlog (speed, only now that Phase 1 holds):** (a) trim the fixed dbus 2+4 s sleeps → readiness
  polls; (b) shorten the 12 s serial settle toward the minimum that still converges (binary-search it);
  (c) carefully re-introduce LIMITED concurrency (e.g. launch in pairs, or cap concurrent wasm-compiles)
  to approach wall ≈ slowest app; (d) the per-app compute floor (~2.2 s first-paint) is the toolchain floor.

### ★2026-07-02 — TOTAL-BLACK FLAKINESS is the acceptance blocker (new top theory T0)
- Forensics on a black run (skip-prewarm, cov 0.0 for 110 s, all 5 launched): every client emits only its
  2 launch lines then is SILENT; the WM (xclient0, launched +12 s) NEVER reaches its event loop / paints
  in the remaining ~98 s. `[pump-starve]` = only 8.1 s over 560 gaps (pump runs ~93 % — NOT gross
  dispatch-blocking). No traps/errors. So it is not a single block: under concurrent boot the guests
  collectively fail to progress to paint (concurrent-boot starvation / a cross-guest race), FLAKILY.
- The fb-write starvation (M8.6 futex-storm) is ALREADY fixed: `maybeBulkEncodeFsPayload` routes ≥64 KB fs
  writes (the 1.2 MB fb) through the bulk SAB via memcpy, not base64 (node_import_cache.rs:7843). So the
  current black is a DIFFERENT mechanism.
- Settle-gating misfire suspected: launch gate uses "X-client quiet ≥6 s" as the "settled/painted" proxy,
  but a STARVED client is ALSO quiet → the gate launches the next app onto a not-yet-painted WM → piles on
  → deeper starvation → black. A real session manager gates on the WM being UP (_NET_SUPPORTING_WM_CHECK /
  fb coverage rising), not on silence.
- **T0 (new, TOP): make concurrent boot render DETERMINISTICALLY.** Candidate smallest tests: (a) paint-gated
  launch (host gates next launch on guest-fb coverage rising, not silence); (b) concurrency sweep 1→5 apps
  to find the reliability ceiling; (c) reduce per-app fs-RPC load (T4 fontconfig/icon cache) to cut dispatch
  pressure. Then the perf levers (concurrency, ≤5.4 s) on top.

### 2026-07-02 — T1.a: cap wasm warmup-exec poll to 2 s (separate from 30 s materialize) — ❌ REJECTED
- Hypothesis: the one 30 s dispatch-task hold is `prewarm_wasm_path`'s warmup poll running the GUI app's
  `_start` (never returns) to the full 30 s PREWARM_TIMEOUT; capping the poll to 2 s removes ~28 s.
- Change: new `DEFAULT_WASM_WARMUP_EXEC_TIMEOUT_MS=2000` used only in `prewarm_wasm_path`'s poll loop
  (wasm.rs), + WARN on breach, + limits-inventory entry. (Also touched the inline WASI fallback `start()`
  — DEAD code: the active WASI comes from `globalThis.__agentOsWasiModule` in node_import_cache.rs.)
- Boot-time before → after: 83.1 s (1 run, 95 %) → **3/3 BOOT_TIMEOUT, cov 0.0** (nothing paints). Launch
  DID speed up (88 s → 54–61 s) = the 30 s block was removed as intended — but the desktop no longer renders.
- Verdict: ❌ the 2 s cap fires `prewarm_execution.terminate()` mid-init; killing the half-run prewarm app
  corrupts shared state the real run needs (fontconfig/icon cache mid-write, or the shared JS context /
  X handshake). At 30 s the app had finished init + gone idle, so terminate was clean → real run rendered.
- **★LEARNED (reframes T1):** the 30 s "waste" is load-bearing. Line 9526 of the runner
  (node_import_cache.rs) ALREADY does `if (prewarmOnly) process.exit(0)` BEFORE running `_start` — so IF
  `prewarmOnly` were honored, prewarm would exit in ~compile time, never 30 s. The observed 30 s proves
  **`prewarmOnly` is effectively FALSE in the runner** (env not honored) → prewarm runs the FULL app.
  Reverted T1.a. Next: probe whether `prewarmOnly` reaches the runner + how prewarm exits (Exit vs 30 s
  timeout). If false, the correct T1 fix = make `prewarmOnly` honored → clean fast exit at 9526 (no
  mid-init terminate, no double-run) and re-measure whether the real run still renders.

### ★★2026-07-02 — CORRECTED CPU/thread measurement: it's a DISPATCH-FUNNEL, box is 92% idle (not compute floor)
- (Earlier "72 CPU-s compute-bound" was a PROBE BUG — matched the HOST process, not the sidecar. Fixed the
  probe to match comm="secure-exec-sid".) Corrected, actual sidecar:
  | | CPU-s | wall | cores avg | threads | outcome |
  |--|--|--|--|--|--|
  | serial | 101 | 60.5 s | **1.67** | 162 | ✓ renders |
  | concurrent | 102 | 100 s (timeout) | **1.02** | 157 | ✗ black |
- **★ Findings:** ~100 CPU-s of REAL work (not idle-blocked), but the box is **92 % IDLE** (1.67/20 cores),
  with **162 threads mostly PARKED.** The guests do NOT parallelize. Concurrent boot does the SAME work on
  FEWER cores (1.02) and FAILS → adding concurrency makes it worse (guests stall/contend, not spread).
- **★ Mechanism = SINGLE-DISPATCH-TASK FUNNEL.** All guest sync-RPCs serialize through the one select!/pump
  task; ~1 of the 1.67 cores is likely that thread busy processing RPCs (base64 every syscall + VFS), only
  ~0.67 cores is actual guest compute. Serial (1-2 guests) → funnel keeps up → render. Concurrent (8 guests
  hammering it) → funnel is the bottleneck → guests stall → black. NOT the wasm compute floor (box idle).
- **★ Phase-2 is RUNTIME-fixable (not toolchain):** the cores are there. Levers: (a) cut per-RPC overhead
  (extend the binary-SAB path beyond fb-writes so every syscall isn't base64'd on the dispatch thread); (b)
  PARALLELIZE sync-RPC servicing so 8 guests don't serialize through one thread. Either lets guests spread
  onto the idle cores → concurrent boot fast AND reliable → ~≤15 s. This is the same single-dispatch-task
  root that PR #123 / the inline-dispatch levers chipped at — now the measured #1 Phase-2 target.
- Open: split the ~1.67 cores into dispatch-RPC-overhead vs guest-compute (SECURE_EXEC_RPCPROF) to decide if
  cutting RPC cost alone is a big win or full dispatch-parallelization is needed.

### ★★2026-07-03 — the CPU is largely BUSY-SPIN (50ms re-poll), + the thread/isolate model, measured
- **SIDECAR_IDLE_CPU_CORES=0.56** at a SETTLED/idle desktop (native ~0). So ~0.56 cores is pure busy-spin,
  and likely ~half the boot's 100 CPU-s is re-poll churn, NOT productive compute. (Earlier "compute-bound"
  was wrong twice — first the host-vs-sidecar probe bug, now: much of the sidecar CPU is spin.)
- **Thread histogram (162, all HOST/sidecar — the VM has no threads of its own):** 40 `se-poll-waiter-`
  (= cores×2, `PollWaiterPool::with_default_size`, tunable `SECURE_EXEC_POLL_WAITERS`), 53 V8-exec
  (`secure-exec-v8-`+`session-v8-exec` = guest ISOLATES incl. wasi-thread workers), 53 main sidecar/tokio,
  16 V8 DefaultWorker (compile/GC).
- **Spin mechanism:** the pool threads BLOCK (state.rs:1002 `poll_waiter_loop` = condvar wait + `wait_changed`).
  The spin is the GUESTS: each idle `net.poll_wait` hits the 50ms clamp (`JAVASCRIPT_NET_POLL_MAX_WAIT`) →
  times out → isolate wakes → re-polls → re-registers, ~20×/s per idle guest × ~8 guests ≈ 160 wakeups/s ≈
  0.56 cores. Root = the 50ms clamp is a re-poll SAFETY NET for an INCOMPLETE notify-graph.
- **Isolate model:** `create_isolate` = `CreateParams::default()` — NO SNAPSHOT; each of the ~53 exec
  isolates built fresh. GTK apps are wasi-THREADED so each GLib/GIO worker = another fresh isolate (why 53
  isolates for 8 guests).
- **★ Phase-2 levers (measured, prioritized):** (A) complete the notify-graph so the clamp can go/grow →
  idle guests BLOCK (native-like ~0 idle CPU), frees ~0.56+ cores AND unloads the dispatch funnel — BIGGEST.
  (B) snapshot isolates → faster creation for the ~53 fresh isolates. (C) parallelize the dispatch funnel.
  All RUNTIME fixes (box is idle; not the wasm compute floor).

### ★★★2026-07-03 — the 60s SERIAL boot is SETTLE-DOMINATED, not compute-dominated (Phase-2 reframe)
- The serial launch gate (host/src/main.rs:1305) launches the next app only once the previous app has been
  QUIET (no output) for `settle` = SX_SERIAL_SETTLE_MS (default 12000). Comment at :2396 = "12s is the
  measured 5/5-reliable value." So the 60.5s ≈ 5 apps × ~12s quiet-wait. The 72-101 CPU-s of real compute
  OVERLAPS INSIDE those windows — the wall time is the DELIBERATE settle delays, not a compute floor.
- **∴ Phase-2's dominant lever is SHRINKING THE SETTLE WINDOW (12s → target ~3s → 5×3=15s).** The 12s is a
  conservative "wait until the app is surely done initializing" heuristic; shorter was flaky (contention →
  black), which is WHY 12s was chosen. So shrinking it reliably requires REDUCING the contention that makes
  apps slow to settle: (a) the 50ms re-poll spin (0.56 idle cores), (b) the single dispatch funnel, (c) the
  wasm compile burst. Reduce those → apps settle faster → shorter settle window is reliable → faster boot.
- **First cheap experiment (respects Phase 1):** measure each app's ACTUAL active-init time (launch→quiet)
  vs the 12s quiet window. If apps go quiet in ~2s, the 12s is mostly conservatism → shrink SX_SERIAL_SETTLE_MS
  and find the reliability cliff via measure-boot.sh (must stay 5/5 FULL, zero-black). No core rewrite.
- Deeper levers unlock an even shorter settle: kill the re-poll spin (notify-graph), snapshot isolates
  (faster ~53-isolate creation), parallelize the dispatch funnel.

### ★★★2026-07-03 — NATIVE-LINUX EQUIVALENCE AUDIT (4 parallel subagents, evidence-backed)
Verified each observed behavior against native Linux under the "behave like native Linux" invariant. 3 of 4 DIVERGE;
none are covered by the allowed concessions (single-thread / bounded-CPU / default-deny egress) → all faithfulness
bugs. All four converged on ONE root: the single-threaded host broker + incomplete event delivery.

| # | Behavior | Native | secure-exec | Verdict | Phase-2 rank |
|---|----------|--------|-------------|---------|--------------|
| 1 | idle poll | poll(-1) parks 0 CPU (strace: 1 syscall, 3s, 0 wakeups) | force-wake every 50ms → 0.56 idle cores | **DIVERGENT bug** | **#1** |
| 2 | host syscall dispatch | per-core parallel, fair (8 procs measured all R, ~1.5s CPU each) | ALL guest RPCs funnel 1 `&mut NativeSidecar` (current-thread tokio), HOL starvation → needs 12s settle | **DIVERGENT bug (incidental, NOT the concession)** | **#1 (co-root)** |
| 3a | thread SEMANTICS | pthreads: shared mem, futex | real OS threads + shared WebAssembly.Memory + atomic.wait/notify | **EQUIVALENT** ✓ | — |
| 3b | thread/isolate COST | pthread_create 10.5µs, no runtime built | fresh NO-SNAPSHOT isolate per wasi-thread (~53 for 8 guests) | **DIVERGENT perf gap** | secondary |
| 4 | launch compile | execve: no recompile, shared text pages | recompile wasm per launch, no cross-launch code cache | **DIVERGENT mechanism, NEGLIGIBLE magnitude** | log only |

- **#1 detail:** guest requests only 1000ms for an infinite poll (node_import_cache.rs:11922), sidecar clamps to
  50ms (execution.rs:20738 JAVASCRIPT_NET_POLL_MAX_WAIT; clamp :20787; deadline :21575). Clamp is a SAFETY NET for
  the incomplete notify-graph (code comment execution.rs:20734). Fix: complete notify-graph (every readiness edge
  calls notify()) → guest poll(-1) blocks indefinitely on the PollWaiterPool → idle spin → ~0, like native.
- **#2 detail:** guest COMPUTE is already parallel (each isolate own host thread session.rs:300). Divergence is
  only the HOST syscall broker: single `select!` loop (stdio.rs:123 current-thread, :211-320) holds `&mut sidecar`;
  in-code comment javascript.rs:447 measures ~636µs of a ~742µs hop is just waiting to be picked up. Co-boot = all
  guests burst RPCs → HOL starvation → slow guest never converges → black. 12s settle = only-one-guest-bursts hack.
  Fix: shard kernel state / per-subsystem interior locking (extend InlineNetDrain/PollWaiterPool to whole surface)
  → concurrent boot reliable AND ~1-2s → **the 12s settle + serial-launch become UNNECESSARY** (the 60s→~few-s win).
- **#3b:** semantics faithful; the no-snapshot fresh-isolate cost is real but the concession ("no OS threads") is
  scoped to registry/native commands, NOT GTK wasi-threads guests (which DO spawn real OS-thread worker isolates).
  Snapshot facility already exists (session main isolate uses it, session.rs:897); extend to worker path
  (wasm_threads.rs:320 create_isolate(None)) — compatible w/ __threadMod reuse. SECONDARY lever, below #1.
- **#4:** REFUTED as a boot lever by repo's own measurement — Liftoff compiles the 17MB module in ~12ms; L-X
  (persist compiled module) already measured = ~14ms = null. Co-boot stall is NOT compile-bound. Log-only faithfulness note.

**∴ Phase-2 conclusion:** the 60s isn't a compute floor — it's TWO host-faithfulness bugs (a busy-poll clamp
papering over an incomplete notify-graph, and a single-threaded syscall broker forcing the 12s settle). Fixing
them makes the desktop behave like native Linux: idle at ~0 CPU, concurrent launch reliable + ~1-2s. Snapshot is a
secondary polish; compile-caching is negligible.

### ★★★2026-07-03 — D2 funnel profiling: the REAL freezers are blocking net.poll + thread_spawn, NOT socket data
Instrumented an env-tunable `[rpc-block]` threshold (SECURE_EXEC_RPC_BLOCK_US, default 300000 preserved) and ran a
concurrent (non-serial) boot at 2ms. Per-method cumulative time HOLDING the single dispatch task:
- **net.poll — 8 calls × EXACTLY ~50ms = 400ms.** Blocking poll (`socket.poll(clamp)` at execution.rs:21294/21307)
  SLEEPS up to the 50ms clamp ON the dispatch task → a single call FREEZES EVERY guest for 50ms. No deferral path
  (unlike net.poll_wait which defers to PollWaiterPool). This is D1's clamp manifesting as a D2 funnel freeze.
- **wasm.thread_spawn — 18 calls × ~12-23ms = 306ms.** `spawn_wasm_thread` (execution.rs:5772) runs
  `start_execution_with_net_drain` (worker-isolate bootstrap, NO snapshot) SYNCHRONOUSLY on `&mut self` → each spawn
  freezes all guests ~17ms. This is D3's no-snapshot isolate cost manifesting as a D2 funnel freeze.
- **net.listen — 2 × 21ms = 43ms.**
- **[select-block] EMPTY** (no branch held >1s) → the framebuffer event-drain is NOT a monopolizer. **Plan's
  Increment 3 (fb) DEPRIORITIZED.** And net.write/net.read never hit 2ms → **Plan's Increment 1 (socket data) REFUTED.**
- pump-starve = 8.1s cumulative; the >2ms blocks (~750ms) are the ACUTE harmful part (each freezes all guests); the
  rest is the sub-2ms long tail + legit idle.
- NOTE: this concurrent run CONVERGED at 59s / 95.2% cov — concurrent boot is FLAKY (earlier 3/3 timed out), and a
  few 50ms/17ms freezes at the wrong moment in the convergence window is a plausible flakiness tipping mechanism.

**∴ D2 first increment (data-driven, replaces the plan's socket-data guess):** stop BLOCKING/EXPENSIVE ops from
monopolizing the shared dispatch task. Two contained targets, both "one guest's op freezes all guests" = the D2
thesis: (1) blocking net.poll → defer off-funnel like net.poll_wait (or fix the guest caller to use drain+poll_wait);
(2) wasm.thread_spawn → bootstrap the worker isolate off the dispatch task and/or via snapshot (D3-mechanism).
Structural per-VM servicing (plan Increment 4) remains the wholesale fix for the long tail.

### ★★2026-07-03 — D2.1 LANDED (gated): blocking recv() now blocks off-broker — determinism 5/5, net.poll freezes gone
Fix (node_import_cache.rs recv loop, gated SECURE_EXEC_RECV_OFFBROKER, default-OFF): a blocking socket recv()
no longer sleeps net.poll(50) on the shared dispatch task; it mirrors net_poll's proven off-broker pattern
(snapshot readiness gen → non-blocking drain THIS socket → net.poll_wait deferred to PollWaiterPool). So a
blocking recv freezes only its own guest, like native.
- **Mechanism CONFIRMED:** concurrent-boot [rpc-block] histogram with the gate ON → `net.poll` GONE (was
  8×~50ms=400ms of all-guest freezes → 0). Only wasm.thread_spawn (313ms) + net.listen (46ms) remain.
- **Determinism gate PASSED:** serial ×5 with the gate ON = 5/5 FULL, zero total-black, cov 95.1-99.9%, median
  62.4s (no regression vs ~61s baseline). No lost-wakeup — the readiness change is safe.
- Still GATED default-OFF: D2.1 alone doesn't make concurrent boot reliable (thread_spawn + the sub-2ms long
  tail remain), so the settle stays until the freezer stack is cleared. Flip defaults ON once concurrent is 5/5.
- NEXT: D2.2 (wasm.thread_spawn — worker-isolate bootstrap off the dispatch task and/or snapshot).

### ★★★2026-07-03 — CORRECTED: boot is SERIAL-APP-INIT-bound (~N × app-init), settle must ≥ app-init-time
MEASUREMENT BUG FOUND + FIXED: SX_SERIAL_SETTLE_MS was NOT forwarded through measure-boot.sh/inside-measure.sh,
so an earlier "settle=8s" test silently ran at 12s (log showed settle=12000). Fixed the forwarding, re-measured:
- **settle=12s → 5/5 FULL, ~61s.** (5 apps → ~12.2s/app.)
- **settle=6s → 5/5 TOTAL-BLACK (cov 0.0%).** The gate launches the next app before the previous reaches
  quiet → co-init contention → total collapse. So the 12s is NOT pure conservatism: apps genuinely take
  ~6-12s to initialize/quiet, and the settle must be ≥ that. Reliability cliff is in (6s, 12s].
- ∴ **boot time ≈ SUM of serial per-app init times** (the launch gate runs apps one at a time by design to
  avoid contention). The settle just has to cover each app's init. Shaving the settle within the cliff gives
  only a modest win (~61s → ~45-50s at best) and can't approach ≤15s.
- **∴ The ≤15s target REQUIRES concurrent app launch (all at once) + the structural per-VM parallel servicing
  (D2.3) so concurrent init doesn't contend/collapse.** Then boot ≈ MAX(app-init) ≈ 10-15s, not the serial SUM.
  Contained freezer-removals (D2.1 ✓ net.poll, D2.2 thread_spawn) are correct native-faithfulness fixes and
  necessary groundwork (fewer freezes under concurrent load) but do NOT by themselves delete the serialization.
- D2.1 status: determinism-safe (5/5 at 12s), net.poll freezes gone — banked/gated. It does NOT lower the
  settle cliff alone (6s still collapses). The real lever is the structural change + concurrent launch.

### ★★★2026-07-03 — the 61s is a BLACK BARRIER (~20s/app DEAD-WAIT init), NOT settle or per-app-serial-CPU
Coverage-over-time of a full 5-app boot: first-paint(>3%)=61134ms, FULL(95%)=61435ms — only 300ms apart. So the
screen is BLACK for 61s then big-bangs to 95%. Decisive control: a 2-app boot (xfwm4+panel) first-paints at
**55.7s** — barely below the 5-app 61s. ∴ the cost is NOT per-app-serial (2 apps ≈ 5 apps); it's a largely-FIXED
~55s barrier dominated by the FIRST apps' cold init (~20s each: xfwm4 quiet ~22s, panel launched 34s → paint 55.7s
= ~22s). Each app takes ~20s launch→paint vs native <1s, and SIDECAR_CPU≈1.3 cores (box mostly idle) → the ~20s
is DEAD-WAIT, not compute.
- ∴ the boot-SPEED lever is killing the ~20s/app dead-wait, NOT shrinking the settle (already shown settle-cliff
  is 6-12s and time is init-bound) and NOT (only) the structural concurrent-launch change.
- **Prime suspect: D1.** An app doing thousands of poll_waits during init, each stalled up to the 50ms clamp by
  the INCOMPLETE notify-graph, = thousands×~50ms = many seconds of dead-wait per app. If so, D1 (complete the
  notify-graph, remove the clamp) is the BOOT-SPEED lever, not just the idle-spin lever — it would cut both the
  ~20s/app init AND the 0.56 idle cores. Tracing wake-cause (WAKEPROF deadline% vs notify%) to confirm.

### ★★★★2026-07-03 — WAKEPROF CONFIRMS D1 is the BOOT-SPEED lever: X server 90.7% deadline-wait
WAKEPROF on a 2-app boot — every infra guest is dominated by DEADLINE (50ms-clamp timeout) wakes, not notify():
  Xvfb 90.7% deadline | dbus 58.0% | xfconfd 57.8% | xfwm4 38.4% | xfce4-panel 41.1%
The X SERVER (the hub every client renders through) wakes 90.7% from the 50ms deadline, NOT from client data
arriving. So every X-protocol round-trip during app init waits up to 50ms for the server's poll to RESCAN
instead of being notified the instant a client writes → hundreds of round-trips/app × up-to-50ms = the ~20s/app
init barrier. This UNIFIES the whole problem: D1 (complete the readiness notify-graph so a socket write notifies
the PEER's poll_wait, then remove the 50ms clamp) collapses BOTH the ~20s/app boot barrier AND the 0.56 idle
cores. D1 is the primary lever; the structural per-VM change (D2.3) is secondary once round-trips are fast.
Existing mechanisms to build on: SECURE_EXEC_DATA_NOTIFIER (spawn_unix_socket_data_notifier),
SECURE_EXEC_INLINE_PEER_NOTIFY (net.write → peer readiness notify). Testing whether they cut Xvfb deadline%/boot.

### 2026-07-03 — existing notifiers DON'T fix the X-server wake gap (D1 edge located)
Ran the 2-app boot with SECURE_EXEC_DATA_NOTIFIER=1 + SECURE_EXEC_INLINE_PEER_NOTIFY=1: NO change —
first-paint 56.3s (was 55.7s), Xvfb deadline% = 92.0 (was 90.7). So the existing peer/data notifiers do NOT
wake the X server's client-connection poll. The precise D1 gap: a client writing X-protocol data to the X
server's socket does not call notify() on the readiness object the X server's poll_wait blocks on. Fixing THIS
edge (and the analogous dbus/xfconfd ones) is the D1 boot-speed work. Next: trace the X server's wait path
(net.poll_wait vs __kernel_fd_poll, which readiness object) and where a client write should notify it.

### ★★★2026-07-03 — the barrier is COMPOSITE: deadline-latency (D1) is real but ~22%, serial sequencing is the rest
Subagent pushback (well-cited): the client-write→server-notify edge is NOT missing — it's wired 3× (per-socket
reader thread execution.rs:12943 + 2 gated copies), all notifying the same process.socket_readiness. So "missing
notify" was WRONG. Its theory: host-thread oversubscription delays the (correct) notify past the 50ms clamp.
Tension: box is ~92% idle, so CPU-starvation is a weak explanation.
DECISIVE TEST — drop the clamp 50ms→5ms (SECURE_EXEC_POLL_MAX_WAIT_MS, now forwarded): 2-app first-paint
55.7s→**43.4s** (−12s, −22%). So deadline-latency IS on the critical path (reducing it speeds boot), but NOT
proportionally (10× less clamp ≠ 10× less barrier) → the barrier is COMPOSITE:
  ~33s serial launch sequencing (settle-bound; panel doesn't launch until 33s) + ~10-22s per-app init
  (deadline-latency-bound, clamp cuts it) + ~10s init floor.
∴ BOTH levers are real and needed for ≤15s: (1) D1 kill the deadline-latency on the critical path — but the
faithful fix isn't lowering the clamp (raises idle spin) nor adding a 4th notify (redundant); it's either the
subagent's shared-poll-loop (fold N reader threads → 1, if late-notify) OR notifying the critical unnotified
sources (POLLOUT/framebuffer/timers, if idle-source polling) — still to disambiguate; (2) concurrent launch +
structural per-VM servicing to remove the ~33s serial sequencing. Neither alone reaches ≤15s.

### ★★★★★2026-07-03 — clamp=5 REGRESSES full boot (3/3 timeout, 493 CPU-s) → D1 notify-graph is THE unifying lever
Full 5-app boot at clamp=5ms: 3/3 TIMEOUT (cov 4.84% panel-only), SIDECAR_CPU=493s (5× the ~100s at clamp=50).
So the clamp is a TRADEOFF not a free lever: lower clamp = less deadline-latency (helped 2-app 55.7→43.4s) BUT
10× more re-polls = a funnel-SATURATING storm that collapses the full boot. Latency and re-poll-storm are the
SAME clamp's two sides — clamp-tuning can't win.
∴ **D1 (complete the notify-graph so guests BLOCK on a real notify instead of re-polling at the clamp) is the
ONE lever that fixes everything at once:** kills deadline-latency (fast X round-trips → faster app-init), kills
the re-poll storm (low funnel load → concurrent boot viable → removes the serial-sequencing barrier too), kills
the 0.56 idle cores. The subagent showed the SOCKET-DATA notify is already wired (reader thread); the remaining
UNNOTIFIED sources that force the clamp re-polls are the NON-socket ones: POLLOUT/write-readiness, framebuffer/
VFS-write readiness, glib timers. Completing D1 = wire those to notify() (and give timed waits their real
deadline, not the 50ms clamp), then raise/remove the clamp. THIS is the primary work; concurrent launch +
structural D2 follows once the funnel load drops.
- D2.1 remains banked (recv off-broker, determinism-safe, gated). clamp stays at default 50ms (5ms regresses).

### ★★★★2026-07-03 — D1 mechanism CONFIRMED (b) GENUINE no-data — refutes late-notify / shared-poll-loop fix
Added [deadprobe] (SECURE_EXEC_DEADLINE_PROBE, sidecar-side): at the moment net.poll_wait is about to BLOCK
(guest drained nothing, gen unchanged), non-blocking-poll the process's socket OS buffers. Result on a 2-app
boot — OS buffers essentially EMPTY at block-entry for ALL guests: Xvfb 0.1% had-data (1/1074), dbus 0.0%,
xfconfd 0.3%, xfwm4 0.1%, panel 0.1%.
∴ the reader threads are NOT behind — data isn't sitting unread. The 90% deadline waits are GENUINE no-data
waits, NOT late/starved notifies. **This REFUTES the subagent's late-notify theory and its shared-per-VM-poll-
loop fix** (folding reader threads would fix a problem that doesn't exist). Investigating-before-implementing
saved a big black-screen-prone refactor built on a wrong mechanism.
Caveat: Probe B is entry-time; a reader never-behind at entry across ~10k idle-core samples won't be >50ms late
mid-wait, so (b) is well-supported (Probe A deadline-exit would make it airtight).
∴ D1 reduces to branch (b): the deadlines are genuine timed/no-data waits for NON-socket events. clamp=5 sped
the 2-app boot 22% by discovering some unnotified non-socket event faster (socket/pipe/POLLOUT already
notify/immediate) — so the critical unnotified events are timers / framebuffer / cross-thread. Faithful fix =
wire those specific critical non-socket edges to notify() (instant discovery, no re-poll storm) and give timer
waits their real deadline. Subtler + ~22% payoff; the bigger lever remains the ~33s serial sequencing
(concurrent launch + structural D2). Reader-thread notify graph for sockets is COMPLETE — leave it alone.

### ★★★2026-07-03 — D1(b) refined: waits are 99.9% INFINITE event-waits; socket+pipe notify graph is COMPLETE
POLLWAITPROF (requested pre-clamp timeout): 11986/12000 = 99.9% are [100+]ms (the 1000ms infinite sentinel).
So the deadline waits are INFINITE event-waits (block-until-event), NOT timers → "give timers real deadlines"
barely applies. Verified the kernel-pipe notify path: service_javascript_kernel_fd_write_sync_rpc
(execution.rs:17616) notifies owner_socket_readiness (the main thread's readiness for a worker's GWakeup
write) — so BOTH socket data (reader thread) AND kernel pipes (GWakeup) already notify the SAME readiness
net.poll_wait blocks on. The notify-graph for socket+pipe is COMPLETE.
∴ D1(b)'s clean targets are already wired. Yet raising the clamp historically caused cascade-timeouts
(exposed gaps), and clamp=5 sped the boot 22% — so SOME non-socket/pipe source IS unnotified and on the
critical path. Candidates (not yet pinned): framebuffer/VFS-write readiness, eventfd/timerfd, cross-VM
non-socket edges. Pinning it needs a targeted trace of what NON-socket/pipe fds sit in the deadline-ing poll
sets (net_poll's fd set at the block point). The 22% clamp=5 gain may also be partly GLib main-loop
ITERATION-RATE (internal idle-source state machines advancing per poll iteration), which is not a runtime
notify gap. ∴ D1(b) boot-speed payoff is more uncertain than the 22% suggested; the clean bounded win is the
idle-spin reduction (0.56→~0 cores) once the non-socket sources are wired so a longer clamp is safe.

### ★★★★★2026-07-03 — ROOT CAUSE: slow WAKE PROPAGATION (4-24ms per productive wake), not missing notify
Built a guest-stderr tee (SECURE_EXEC_TEE_GUEST_STDERR, javascript.rs _log/_error branch → sidecar stderr →
host.log) — unblocks ALL guest probes (9898 [guest] lines/boot; POLL_TRACE now in the forward list). Decisive
data:
- POLLTRACE: every deadline-ing poll blocks on GLib GWakeup PIPE fds (hasPipes=true, k5/k7/k9/k11), revents=0.
- **[rt] (guest-observed blocking net.poll_wait duration): notify DOES fire (productive wakes 25-887/guest),
  but productive wakes are SLOW — prodAvgUs=4000-24000µs (4-24ms), prodMaxUs up to 147ms. deadline waits =
  50ms clamp. [rt-outer]: blockingPolls avg 27-347ms, MAX 11 SECONDS.**
- Native notify→wake = microseconds. Here it's MILLISECONDS. So the problem was NEVER a missing notify (the
  socket+pipe notify graph IS complete, as verified) — it's SLOW WAKE PROPAGATION: notify() fires, but the path
  notify → PollWaiterPool worker → deferred-completion delivery → guest isolate RESUME takes ms, and across
  hundreds of X round-trips/app = the ~20s/app barrier.
- **This UNIFIES D1 and D2**: the deferred poll_wait completion is almost certainly delivered through the
  contended single dispatch funnel (or a slow cross-thread resume). The fix = make the wake completion reach the
  blocked guest isolate in ~µs, off the funnel. clamp=5's 22% gain is explained: more frequent re-polls catch
  the event without waiting for the slow propagation. Next: trace the deferred-completion → isolate-resume path.

### ★★★★★2026-07-03 — DEFINITIVE: runtime wake overhead is ~20µs; the 9-10ms/round-trip is GENUINE cross-guest causality
[hopprof] decomposition of productive wakes (2-app boot): peerWait avgUs=9124-10009 (max 50ms=clamp) |
wakeLag(notify->resume) avgUs=11-13 (max 2.3ms) | respond avgUs=9 (max 95µs).
∴ **the runtime notify→resume→deliver overhead is ~20µs — the wake path is already microsecond-fast.** The
entire 9-10ms per round-trip is peerWait = the PEER genuinely taking that long to produce the response. So
BOTH "missing notify" AND "slow wake propagation" are refuted. **D1(b) is a dead end for boot SPEED**: the
notify already fires in µs; the deadlines are genuine cross-guest/cross-thread waits for work-not-yet-produced.
The 22% clamp=5 gain = the peerWait TAIL (waits hitting the 50ms clamp for a cross-thread signal not produced
within 50ms) resolving faster on more-frequent re-poll — but that storms at scale (D1 idle-spin lever only).
**∴ the ONLY path to ≤15s is CONCURRENT LAUNCH (overlap the serial per-guest causality chains) via the
structural per-VM change — the runtime per-op overhead is already µs, not the bottleneck.** The ~9-10ms/round-
trip is the guest's genuine work (wasm X-protocol processing + cross-thread GLib) which is serial today because
apps launch one-at-a-time. D1 remaining value = idle-spin reduction only (not boot speed).

### 2026-07-03 — concurrent collapse is STUCK not STORM (clamp=500: CPU 493→48s but still 3/3 timeout 0% cov)
Tested the cheap hypothesis (raise clamp → less storm → concurrent converges). REFUTED: concurrent clamp=500
cut SIDECAR_CPU to ~48s (10× less than clamp=5's 493s — storm genuinely reduced) but STILL 3/3 timeout at 0.0%
coverage. So the concurrent-boot collapse is NOT the re-poll storm — the guests are STUCK (low CPU, nothing
renders), not thrashing. Neither clamp direction unlocks it (low=storm, high=slower-init fools the settle gate).
∴ concurrent boot has a correctness/scheduling STUCK-state under co-init (matches the old "4th heavy guest
starves, ceiling ~3" finding), not a throughput problem. The structural per-VM concurrent-servicing change is
confirmed necessary — but the collapse ROOT (a specific cross-guest deadlock/missed-wake vs funnel-serialization
starvation) must be diagnosed first, since a low-CPU stuck-state is not the signature of pure funnel contention.

### ★★★★2026-07-03 — concurrent-collapse ROOT (via tee): LIVELOCK — only xclient0 launches, guests freeze on GWakeups
Concurrent boot with the tee: only `secure-exec: launched xclient0 (xfwm4)` — the OTHER 4 apps never launch. All
guests freeze in an IDENTICAL repeating poll cycle on GLib GWakeup pipe fds (k5/k7/k9/k11, revents=0), many with
remain=298s/563s (waiting effectively forever). So concurrent boot LIVELOCKS: xfwm4 launches once the X server
is "serving" but BEFORE dbus/xfconfd finish initing → xfwm4 blocks on a cross-guest readiness signal that never
fires → xfwm4 never settles → the launch gate never releases the next app → 0% render. Root = DEPENDENCY-ORDERING
+ cross-guest starvation (matches "ceiling ~3 heavy guests"), NOT throughput/storm (CPU is low). Serial mode
works precisely because each dep is fully settled before the next launches (breaking the circular wait).
∴ two candidate fixes to scope: (1) SMARTER LAUNCH GATING (host) — gate each app on a PRECISE readiness signal
(X serving + dbus serving + xfconfd serving + WM managing) instead of the crude 12s quiet-settle, and launch the
mutually-INDEPENDENT apps (panel/xfdesktop/thunar/mousepad) together once deps are ready → boot ≈ infra +
max(app-init); (2) STRUCTURAL per-VM servicing (runtime) — raise the concurrent-guest ceiling so co-init doesn't
starve. (1) may be far cheaper and is where the ~33s waste actually lives (conservative settle, not required
ordering). Scope both.

### ★★2026-07-03 — Phase 2B increment 1: DEDUP infra launch (SX_READY_GATE) unblocks the launch-gate livelock
Found + fixed a real bug: dbus-daemon + xfconfd were each launched TWICE (synchronous main.rs:2293-2332 AND
background launcher :2350-2386); the second dbusd collides on the bound session socket. Gated the duplicate
synchronous block behind SX_READY_GATE (default-OFF preserves the banked serial-5/5 baseline). Result: concurrent
boot with SX_READY_GATE=1 now launches ALL 5 xclients (was only xclient0 — xfwm4 previously never reached
wm_ready). So the dedup breaks the FIRST livelock. Still 0% render (times out) because concurrent apps launch 6s
apart (APP_SETTLE_MS) and 6s-settle-too-short → co-init contention (the same cliff). ∴ increment 2 = precise
readiness gating (X/dbus/xfconfd/WM serving) so the settle can shrink + independent apps burst safely.

### 2026-07-03 — 6s-collapse is NOT funnel-HOL but pump-STARVE (pickup latency) → Phase 2A justified differently
6s-settle collapse diagnostic: NO large held RPC (max 22ms, only the usual thread_spawn 18×17ms) — so NOT
funnel head-of-line blocking. BUT [pump-starve]=7.6s over 537 gaps (the dispatch not running ~14% of the time).
So the funnel cost is per-hop PICKUP LATENCY (D16 in-code: 636µs of a 742µs hop is waiting to be picked up by
the single dispatch task), not held RPCs. Across the many hops of each ~9-10ms round-trip × hundreds of
round-trips, that accumulated cross-guest pickup latency IS the peerWait/boot cost. The plan's "held-RPC"
Phase-1 criterion was the WRONG test — parallel servicing (Phase 2A) eliminates the pickup latency (each guest's
hops serviced on its own task, not queued behind other guests). ∴ Phase 2A justified. Dedup (increment 1) is
banked (serial 5/5, 56.7s median). Shorter settle collapses on app-init OVERLAP (X server serializes concurrent
clients at ~9-10ms/round-trip) — the same pickup-latency root. Next: implement per-VM parallel servicing.

### 2026-07-03 — inline net.write (F10-INLINE-WRITE) REFUTED: slower (73s vs 56.7s) + flaky (1/2), gated OFF
Implemented net.write off-broker (SECURE_EXEC_INLINE_SOCKET_DATA): extend InlineSock with stream+peer_readiness,
try_socket_write on the bridge thread (write + peer.notify, byte-decode via javascript_sync_rpc_bytes_arg),
dispatch net.write inline. RPC_PROFILE justified it (net.write = 4.7k calls/boot, #1 non-poll on-pump op). But
serial+gate-ON = 1/2 (one FULL at 73s SLOWER than 56.7s baseline, one launched=False hard-fail). So it's a NET
NEGATIVE: the inline path's overhead (per-write base64 decode + extra peer-notify wake cycles on the bridge
thread, plus a possible double-write-on-error/race) outweighs the ~636µs pickup latency saved. Refuted; gated
default-OFF (inert — try_socket_write returns None → net.write falls through to the pump unchanged), kept as a
scaffold. Lesson: moving INDIVIDUAL hot ops off-broker piecemeal introduces races + overhead that negate the
pickup-latency win — the funnel pickup latency needs the WHOLESALE parallel-servicing re-shard (per-VM tasks),
not per-op inlining, to actually pay off. That (the big change) is the remaining lever for ≤15s.
