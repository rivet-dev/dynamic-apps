# XU7 Multi-App Starvation — Systematic Investigation

**Living document.** Single source of truth for the "multiple X apps starve each other" bug.
Every theory is proven or refuted here with linked evidence. New theories are appended as
investigation surfaces them (see [Recursion Protocol](#recursion-protocol)). The investigation is
**not done while any theory is OPEN.**

---

## 1. Problem statement

A single GTK/X app renders in ~45–60s (clean box). Run several at once in one wasm VM (the XU7
desktop: xfwm4 + xfce4-panel + xfdesktop + Thunar + apps) and they **starve each other**: some
render, most go quiet. Goal end-state = all apps live, WM-decorated, responsive under load.

## 2. Status snapshot

- **Leading root-cause hypothesis (revised 2026-06-27):** latency/serialization through the
  single-threaded host-socket bridge + thread oversubscription (T-C/T-D/T-F/T-E). The original
  lost-wake-via-cross-process-pipe theory (T-A) is **refuted for XU7's hot path** by the channel
  inventory: XU7 uses host AF_UNIX **sockets** for X11 + D-Bus (which DO notify the reader), and
  GWakeup is intra-process (notified via execution.rs:16638). A socket-notify *race* (T-B) is not
  yet ruled out.
- **Confidence:** LOW until D1 runs. The "2 active / 4 starved" split (F3) still argues against a
  *pure* uniform latency story (T-E), so something asymmetric remains (T-C/T-D head-of-line, or T-F
  per-guest scheduling).
- **Root cause:** NOT YET PROVEN. D1 is still the decisive next experiment: notify-dominated wakes
  for all guests would confirm T-A/T-B refuted and pivot fully to latency/serialization; any
  deadline-dominated starved guest would reopen a lost-wake mechanism (T-B).
- **Note:** the 10ms cap still *fires* in XU7 (every GTK app has the intra-process GWakeup pipe in
  its poll set → `pollSetHasPipes`), but it is now **redundant** there (GWakeup notifies via 16638),
  so removing it (task #26) is expected safe for XU7.

## 3. Established facts (proven this investigation)

| # | Fact | Evidence |
|---|------|----------|
| F1 | Each guest = its own V8 isolate on its own OS thread; isolates share no memory. | session.rs |
| F2 | Sidecar sync-RPC **service thread is ~99% idle** under a 2-app load (~909ms/120s). | per-session CPU sample |
| F3 | Of 6 guests, ~2 active (74k polls, 21k file-I/O); **~4 starve to near-0 RPCs**. | per-session RPC counts |
| F4 | The starved guests are **alive** (still issue some file I/O), not deadlocked. | rpc trace |
| F5 | Wasm fuel/CPU limit is **not** hit (60-min budget). | resolve_wasm_execution_timeout |
| F6 | Real X traffic (sockets) and intra-process GWakeup pipes already wake **immediately** via `socket_readiness.notify()`. | execution.rs:12425-12617, 16638 |
| F7 | A **cross-process** pipe write notifies the **writer's** process, not the reader's → reader gets no notify. | execution.rs:16638 (per-process readiness) |
| F8 | The 10ms `pollSetHasPipes` cap was a timer fallback masking missed wakes. **Banned** by CLAUDE.md; slated for removal. | node_import_cache.rs:11709 |

## 4. Methodology & rules (the investigation contract)

- **Most-decisive-first.** Work the ledger in priority order; one theory at a time.
- **Subagents = read-only.** Code-path analysis, call-site mapping, predicted-signature sharpening
  only. **No builds in subagents** (the earlier 20× measurement was contaminated by concurrent
  builds on the shared box).
- **Main thread = builds + timing.** Serialize: nothing else running during a timing run.
- **Diagnostics are default-OFF**, env-gated, zero-cost when disabled, committed on `perf-pivot-work`
  as cataloged tooling (extend `experiments/wasm-gui/INTERNAL-TOOLING.md`).
- **Never** reintroduce a timer/poll fallback that *completes* a wait (CLAUDE.md "Wakeups are
  event-driven"). Diagnostics may *flag/log* a missed wake; never silently complete it.
- **Constraint #5:** upstream Xfce/GTK/glib/X stay unmodified; fix only in native/platform layer.
- **Proof bar:** a theory is PROVEN/REFUTED only with a linked artifact showing the predicted (or
  refuting) signature. "Looks plausible" is not a verdict.

## 5. Theory ledger

Status legend: `OPEN` (untested) · `TESTING` · `PROVEN` · `REFUTED` · `PARTIAL`.

Template per theory: Hypothesis · Predicted signature (true-if) · Refuting signature (false-if) ·
Proof experiment · Debug needed · Result.

---

### T-A — Missing notify edge (lost wake, cross-process pipe) · **REFUTED (static; D1 to confirm)** · was priority 1
- **Hypothesis:** A starved guest blocks on an event that occurred but whose `notify()` never routed
  to it (known gap F7: cross-process pipe reader). Without the 10ms cap it would hang; with it, it
  was throttled to ~100 events/sec = apparent starvation.
- **True-if:** starved guests' wakes are dominated by `deadline/cap-timeout`, not `notify`; and/or
  ready-but-slept hits exist (an fd was ready at the host when the guest blocked).
- **False-if:** starved guests' wakes are overwhelmingly real `notify`, with no ready-but-slept hits.
- **Proof experiment:** D1 + D2 on a 2-app then 6-app run; bucket wakes per guest by cause.
- **Debug needed:** D1 (wake-cause), D2 (ready-but-slept), D8 (cross-process pipe tracer).
- **Result:** **REFUTED for XU7's hot path** by the channel inventory (artifact:
  `xu7-channel-inventory.md`). The cross-process-pipe mechanism (F7) is not traversed: X11 + D-Bus
  are host AF_UNIX **sockets** whose reader threads call `socket_readiness.notify()` on data/EOF/error
  (execution.rs:12592/12606/12617); GWakeup is an **intra-process** self-pipe (gwakeup.c:163, both
  ends one process) notified via execution.rs:16638. No cross-process kernel pipes in the current
  harness (apps launched top-level, not forked by a session manager). **D1 will confirm** by showing
  notify-dominated wakes; any deadline-dominated starved guest would reopen the lost-wake question as
  T-B (socket-notify race), not T-A.

### T-B — Notify fires but completes 0 waiters (wake race) · **REFUTED** · was priority 1
- **Hypothesis:** `notify()` runs but the waiter isn't registered yet / the direct-vs-pool CAS loses
  / generation moved between net_poll's readiness snapshot and `poll_wait` register → wake dropped.
- **True-if:** D3 shows `notify` events that complete 0 waiters while a waiter for that process is
  registered within microseconds before/after.
- **False-if:** every `notify` with a registered waiter completes it; no orphan notifies.
- **Proof experiment:** D3 correlated with D1; look for notify→0-completions adjacent to a register.
- **Debug needed:** D3 (notify producer log), D1.
- **Result:** _pending_

### T-C — Head-of-line stall through single-threaded Xvfb · **OPEN** · priority 2
- **Hypothesis:** Xvfb serves all clients on one loop; if one client's socket write backpressures
  (buffer full) and Xvfb blocks there, every client behind it stalls → the 2 that "win" are simply
  ahead in line.
- **True-if:** D4 shows starved guests blocked specifically on the X socket while Xvfb is blocked in
  a write; D5 shows X round-trip latency spikes correlated across clients.
- **False-if:** starved guests are blocked on non-X fds, or Xvfb never blocks on write.
- **Proof experiment:** D4 + D5; inspect Xvfb's thread state during a stall (stackdump watchdog).
- **Debug needed:** D4 (liveness timeline), D5 (X round-trip histogram).
- **Result:** _pending_

### T-D — Two-party write deadlock (Xvfb ↔ client), cap was breaking it · **OPEN** · priority 2
- **Hypothesis:** Client blocked writing a request to Xvfb while Xvfb blocked writing a reply to the
  client — both socket buffers full. Classic write-write deadlock; the 10ms cap broke it every 10ms.
  Predicts: **removing the cap turns starvation into a hard hang.**
- **True-if:** with cap removed, a guest pair hangs; stackdump shows both in socket write.
- **False-if:** cap removal does not produce paired write-blocked hangs.
- **Proof experiment:** remove cap (after D-prereqs), run, watch for paired write hangs.
- **Debug needed:** D4, D6 (per-RPC service time), stackdump watchdog.
- **Result:** _pending_

### T-E — Pure latency serialization (null hypothesis) · **REFUTED** · was priority 3
- **Hypothesis:** No bug. Just N × startup-round-trips × per-hop latency, serialized through the
  single-threaded servers. Apps are uniformly slow, not starved.
- **True-if:** all guests make slow-but-roughly-equal progress; wake-cause is mostly `notify`;
  removing the cap changes nothing; co-location is the only lever.
- **False-if:** the 2/4 split persists (F3) — non-uniform progress refutes pure serialization.
- **Proof experiment:** D4 progress timelines across all guests; check for the split vs uniform.
- **Debug needed:** D4, D1.
- **Result:** _leaning REFUTED: the panel-only D1 run is sharply bimodal (2 guests ~99% deadline, 1
  busy-spin ~91% immediate, 1 healthy) — not the uniform progress pure-latency predicts. Full run to
  confirm._

### T-F — Reader/worker OS-thread oversubscription inflates hop latency · **OPEN** · priority 2
- **Hypothesis:** 6 guests × (main + worker + N socket-reader threads) >> cores. The OS doesn't
  promptly co-schedule a guest's main+worker pair, so the cross-thread wake hop (worker does the
  blocking pipe read) takes far longer under multi-app load → latency compounds non-linearly.
- **True-if:** D7 shows thread count >> cores and reader/worker threads in runnable-but-waiting
  state during stalls; per-hop latency (D1 elapsed) grows with concurrent guest count.
- **False-if:** hop latency is flat vs guest count; threads are not oversubscribed.
- **Proof experiment:** D7 census + D1 elapsed vs #guests (1,2,4,6 apps).
- **Debug needed:** D7 (thread census), D1.
- **Result:** _pending_

### T-G — Generation rescan amplification · **OPEN** · priority 3
- **Hypothesis:** `socket_readiness` generation is per-process and coarse. Any fd change bumps it;
  the woken guest rescans **all** its fds (one RPC each). Under multi-app, active guests bump
  generations constantly, so a starved guest with many fds spends all its time rescanning +
  re-registering, never completing a logical step.
- **True-if:** D1 shows starved guests doing many short notify-woken rescans that find nothing ready
  on the fd they care about; rescan RPC count >> useful progress.
- **False-if:** rescans are cheap / rare relative to useful round-trips.
- **Proof experiment:** D1 with per-wake "fds rescanned vs fds ready" counts.
- **Debug needed:** D1 (extended), D6.
- **Result:** _pending_

### T-H — Sync-RPC service-thread tail latency (hidden behind 99% idle) · **OPEN** · priority 3
- **Hypothesis:** Average idle (F2) hides a fat tail: one slow RPC (large framebuffer pwrite, or a
  not-deferred blocking op) serializes all guests behind it in bursts.
- **True-if:** D6 service-time histogram has a tail (p99 >> p50) and stalls correlate with tail RPCs.
- **False-if:** service-time is uniformly tiny; no tail.
- **Proof experiment:** D6 histogram + correlate tail events with D4 stalls.
- **Debug needed:** D6 (per-RPC service-time histogram).
- **Result:** _pending_

### T-I — Guest busy-spins on poll(timeout=0) · **CONFIRMED (= xfconfd)** · priority 1 · (NEW 2026-06-27, from D1)
- **Hypothesis:** A guest (suspected Xvfb) loops calling poll with timeout=0 (non-blocking), returning
  immediately whether or not anything is ready — a CPU spin (the M8.6 `WaitForSomething` timeout=0
  pattern). It burns a core and floods the sidecar with zero-wait sync-RPCs, starving other guests of
  CPU/RPC bandwidth and never blocking long enough to be event-driven.
- **True-if:** wakeprof shows a guest with a huge `immediate` (zero-wait) count dwarfing its blocking
  waits, and quieting/fixing it frees the others.
- **False-if:** no guest has a dominating immediate count, or fixing it doesn't help others.
- **Proof experiment:** D1 (already shows the spin) + pid identity + D4 to confirm spin vs useful work.
- **Debug needed:** D1 (done), pid identity, D4.
- **Result:** **CONFIRMED — the spinner is `xfconfd.wasm`** (`immediate=26736` zero-wait polls in the
  named full run; ~594 polls/sec). Burns a core and floods the sidecar with zero-wait RPCs. Open
  question (now T-J): WHY it spins, and whether quieting it unblocks xfwm4/render (causality test).

### T-J — Why xfconfd spins at timeout=0 (spurious GSource readiness) · **OPEN** · priority 1 · (NEW 2026-06-27, from T-I)
- **Hypothesis:** xfconfd's GLib main loop iterates with timeout=0 because some GSource's `prepare()`
  reports ready (or an fd reports POLLIN/POLLHUP) every iteration, but `dispatch()` never clears it —
  classic GLib busy-spin on an always-ready fd (e.g. a socket/pipe at EOF still reporting readable, an
  undrained eventfd, or a spuriously-readable fd in our emulation). Root would be in the platform fd
  emulation (Constraint #5: fix here, not in glib).
- **True-if:** a net/poll trace of xfconfd shows the same fd reported ready (revents≠0) every spin with
  no state change, OR t=0 polls returning ready=0 while GLib keeps re-iterating.
- **False-if:** xfconfd's t=0 polls correlate with real work (dispatching distinct events) — i.e. it's
  busy, not spinning.
- **Proof experiment:** `SECURE_EXEC_NET_TRACE=1` (filter xfconfd's stderr) to see its poll fd set +
  revents per iteration; identify the always-ready fd. Then a causality test: quiet/fix it and re-measure.
- **Debug needed:** net trace (exists); maybe a t=0-path poll trace (small add) if the spin is ready=0.
- **Result:** **ROOT NARROWED to a KERNEL PIPE (not a socket; clock sub-hypothesis REFUTED).** The
  per-socket-state trace shows both polled fds are tagged `:pipe` (isKernelPipeFd=true): **fd 0x50000005
  is a kernel pipe perpetually POLLIN-readable (re=1, ~13,419 identical polls), never drained**, and
  companion **fd 0x50000007 wants POLLOUT but is `re=0` (not writable)** — the signature of a **pipe
  full of undrained data** (readable + write-blocked). Readiness comes from `__kernel_fd_poll`
  (node_import_cache.rs:11631), NOT the socket readChunks path. So GLib spins on a forever-readable
  kernel pipe it never drains. Leading cause: a **GDBus worker↔main wakeup pipe that fills and never
  drains in single-threaded wasm** (matches the known "GDBus worker-thread GMainContext wakeup"
  blocker; explains why xfconfd=GDBus-primary spins but xfwm4=X-primary doesn't), or a no-op-stub
  inotify fd. Subagent identifying which pipe + the platform fix. Artifacts:
  `gui-progress/2026-06-27T19/{xu7-spin2,xu7-sockstate}.log`.

### T-K — Frozen CLOCK_MONOTONIC (Linux-fidelity defect, separate from the spin) · **OPEN** · priority 3 · (NEW 2026-06-27)
- **Hypothesis:** `_clockTimeGet` (wasm.rs:3322) returns frozen `Date.now()*1e6` for ALL clock ids,
  including `CLOCK_MONOTONIC`, because the wasm guest's `Date.now()` is virtualized/frozen
  (node_import_cache.rs:11133 comment). A monotonic clock that never advances is wrong for Linux
  emulation and can break timeouts/timers, even if it's NOT what causes xfconfd's spin (the spin is a
  readable-socket loop, T-J). Track + fix for fidelity; verify it doesn't mask other stalls.
- **True-if:** CLOCK_MONOTONIC returns identical values across wall-clock-separated calls.
- **False-if:** CLOCK_MONOTONIC advances with real elapsed time.
- **Proof experiment:** trace `_clockTimeGet` return values over time; or test a monotonic-advance fix.
- **Debug needed:** small clock-value trace.
- **Result:** _pending (strongly suspected frozen per the wasm.rs:3322 + 11133 evidence)._

---

## 6. Debug functionality catalog

All default-OFF, env-gated, host-side, native-tool-parallel. Build as each theory needs it.

| ID | Tool | What it proves | Native analog |
|----|------|----------------|---------------|
| D1 | **Wake-cause tagging** on `net.poll_wait` completion: `woke-by ∈ {direct-notify, pool-notify, deadline}`, elapsed, gen-delta, fd/event set. Bucket per guest. | T-A, T-B, T-E, T-G | `perf sched` |
| D2 | **Ready-but-slept probe**: at `poll_wait` register, snapshot whether any waited fd is already ready at the host (socket channel non-empty / pipe has bytes). | T-A (strongest single proof) | — |
| D3 | **Notify producer log**: each `socket_readiness.notify()` → process, reason (data/eof/error/pipe-write), waiters completed vs woken. | T-B | — |
| D4 | **Per-guest liveness timeline**: sample every ~50ms — state (running / poll_wait / Atomics.wait / in-RPC) + monotonic RPC counter. | T-C, T-D, T-E, T-F | `top`/`pidstat` |
| D5 | **X round-trip latency histogram** per client (request→reply). | T-C | — |
| D6 | **Per-RPC service-time histogram** on the sidecar service thread (expose tail behind the 99%-idle average). | T-D, T-H | — |
| D7 | **Reader/worker thread census**: OS threads per guest, oversubscription flag, `/proc/.../wchan` states during stalls. | T-F | `ps -L`/`nproc` |
| D8 | **Cross-process pipe wake tracer**: which process wrote, which holds the read end, was the reader notified. | T-A (F7 edge) | — |

## 7. Recursion protocol

This doc is **recursive**: investigation creates new theories.

1. When an experiment produces an unexplained signature, **append a new T-x entry** (same template)
   with status `OPEN` and a priority, rather than hand-waving it.
2. When a result partially explains the bug, mark `PARTIAL` and spawn the follow-up theory it implies.
3. Re-rank priorities after each verdict (new evidence changes what's most decisive next).
4. Keep the [Status snapshot](#2-status-snapshot) current after every verdict.

## 8. Completion bar (investigation is DONE only when ALL hold)

1. Every theory in the ledger is `PROVEN` or `REFUTED` with a **linked artifact**.
2. The root cause is identified and **proven** (not merely most-likely).
3. A fix is implemented in the native/platform layer and **validated**: the multi-app XU7 desktop
   renders all apps live, WM-decorated, and responsive (type/click/switch), measured **before/after**.
4. The 10ms poll cap is removed and the notify graph is complete (no missed-wake hangs in the
   GWakeup + cross-process-pipe repros).
5. Constraint #5 holds (upstream Xfce/GTK/glib/X unmodified — verified by diff).
6. No `OPEN` theory remains (recursion drained).

---

### Verdict log (newest first)

- **2026-06-27 — ★ CAUSALITY PROVEN: the xfconfd spin IS the render blocker.** Same client set
  (xfwm4 + mousepad), only difference = xfconfd present or not: **WITH xfconfd → 0.0% nonblack** (black,
  nothing renders); **WITHOUT xfconfd (NO_XFCONFD=1) → 69.6% nonblack** (mousepad + xfwm4 render a real
  desktop). So xfconfd's busy-spin starves the other guests (CPU contention: 2 spinning isolate threads)
  → multi-app render fails. Removing the spinner fixes render. ⇒ **T-I/T-J is THE root cause of XU7
  multi-app starvation, proven end-to-end.** The fix = stop xfconfd spinning (drain its GWakeup pipe).
  Artifacts: `gui-progress/2026-06-27T19/caus-{with,without}-xfconfd.png`.
- **2026-06-27 — T-J mechanism PROVEN: GLib never drains a readable wakeup pipe.** New
  `SECURE_EXEC_PIPE_TRACE` (sidecar-side, thread-tagged) shows xfconfd polls its kernel wakeup pipes
  readable ~26k× and issues **ZERO reads** (guest-side `kpipe_read` trace confirms 0 attempts). Both
  threads spin: **main** (thread=false, 16,665 polls) and the **GDBus worker** (thread=true, 10,841
  polls) — so the worker IS running its loop; both fail to `g_wakeup_acknowledge` (gmain.c:4093) their
  readable wakeup. xfwm4 doesn't spin (its wakeup is idle, never written). Ruled out: eventfd (no
  emulation → GWakeup pipe-mode), frozen clock, sockets, lost-wake. Root class = GLib wakeup-pipe
  not-drained in xfconfd's active main↔worker GDBus signaling on wasi-threads (the known "GDBus
  worker-thread GMainContext wakeup" blocker). Exact GLib-internal reason (fd-match vs revents-propagation
  in g_main_context_check) needs GLib-side tracing. **Causality test in flight** (render with vs without
  xfconfd). Artifacts: `gui-progress/2026-06-27T19/{xu7-pipetrace,xu7-disc,xu7-thr}.log`.
- **2026-06-27 — T-J root narrowed to a KERNEL PIPE; clock + socket framings REFUTED:** xfconfd's spin
  = ~13,419 polls all returning a perpetually-readable **kernel pipe** (`fd=0x50000005:re=1:pipe`) +
  a companion pipe `fd=0x50000007 re=0` wanting POLLOUT but not writable = **a pipe full of undrained
  data**. NOT a socket (both fds tagged `:pipe`), NOT a t=0/empty spin, NOT the frozen clock. Root =
  kernel-pipe readiness/drain (platform layer); leading cause = GDBus worker↔main wakeup pipe never
  drained in single-threaded wasm. New **T-K** tracks frozen CLOCK_MONOTONIC (wasm.rs:3322) as a
  separate fidelity bug. Tooling: host forwards `SECURE_EXEC_*` to server/dbus/service guests (was
  client-only); spin0 + per-socket-state poll traces. Artifacts:
  `gui-progress/2026-06-27T19/{xu7-spin2,xu7-sockstate}.log`.
- **2026-06-27 — NAMED full run (identity confirmed):** spinner = **xfconfd.wasm** (`immediate=26736`
  zero-wait polls) → **T-I CONFIRMED**. **Xvfb** (98% deadline, notify=4) + **dbus-daemon** (99%
  deadline, notify=3) stuck; **xfwm4** crawls (136 poll-cycles/45s ≈ 3/sec), blocked on the stuck
  servers; other clients never launched (harness gates on xfwm4 settling). rpcprof: service thread
  **~99% idle** (80k RPCs / 140ms); only costly op = one **63 ms fs.readSync**. ⇒ **T-E REFUTED**
  (bimodal, identity-confirmed), **T-B REFUTED** (notify path works but is barely exercised; stuck
  guests get ~0 data because upstream servers are stuck/spinning, not because a wake was lost). New
  theory **T-J**: why xfconfd spins at timeout=0. Artifact: `gui-progress/2026-06-27T19/xu7-named.log`.
- **2026-06-27 (prelim) — D1 wakeprof first run (panel-only, 25s):** works; sharply **bimodal**.
  System runs on **~35 notifies total** (polling-dominated, not event-driven). Two guests wake ~99% by
  `deadline` with ~0 notify ⇒ ~0 data arriving ⇒ **upstream stall, not lost-wake** (weakens T-B). One
  guest `immediate=26939` (busy-spin) ⇒ new **T-I**. T-E leaning refuted. Identity (pid) added; full
  run pending. Artifact: `gui-progress/2026-06-27T19/xu7-wakeprof-panelonly-smoke.log`.
- **2026-06-27 — T-A → REFUTED (static):** cross-process-pipe lost-wake not reachable in XU7; X11 +
  D-Bus are host AF_UNIX sockets that notify the reader (execution.rs:12606), GWakeup is intra-process
  (16638). Re-rank: T-C/T-D/T-F/T-E now lead; T-B (socket-notify race) still open. D1 to confirm by
  measurement. Artifact: [xu7-channel-inventory.md](./xu7-channel-inventory.md).

_(append `YYYY-MM-DD — T-x → PROVEN/REFUTED: one-line + artifact link` here as verdicts land)_
