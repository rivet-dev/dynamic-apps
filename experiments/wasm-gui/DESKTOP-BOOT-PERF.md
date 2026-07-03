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

## ★ TOP PRIORITY (Phase 1 root fix) — kill the concurrent-boot thread-contention stall

The X server is ~91 % deadline-driven: client requests do not wake it (it re-polls every 50 ms), and under
concurrent boot that latency compounds → flaky total-black. The reader-thread and a per-socket data-notifier
BOTH fail because they are extra host threads that get starved (data-notifier A/B: notify count froze while
deadline % stayed ~90 % — REJECTED). The wakeup must NOT depend on a thread being scheduled.

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
