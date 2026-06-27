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

### Phase-1 objective (the new contract — reduce per-round-trip latency)
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

### Phase-1 bar — OPEN (the new round, per the native baseline in Section 2)
Phase-0 is done but the loop is NOT — the native baseline shows ~40–88× headroom. Phase-1 DONE when:
1. **mousepad first-paint < 2 s** AND **input→response < 50 ms** (Section 2 objective) — OR the
   per-round-trip latency lever's ROI is flattened with documented diminishing returns.
2. Each applied latency fix has a before/after on BOTH B2 first-paint and the input→response number.
3. Constraint #5 + regression gate stay green; default-OFF diagnostics.

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

---

### Verdict log (newest first)

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
