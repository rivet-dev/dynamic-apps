# Parallel Dispatch Servicing — the completion plan for Xubuntu ≤15s boot

The single remaining lever to take the Xubuntu desktop boot from ~56s to ≤15s. This is the per-VM
parallel-servicing re-shard: eliminate the single dispatch task's per-hop **pickup latency** so concurrent
guest init stops serializing. Everything else has been shipped or refuted (see `DESKTOP-BOOT-PERF.md`).

## Why this, and only this (the airtight diagnosis)

- Runtime notify/wake overhead is already **~20µs** (`[hopprof]`: wakeLag 11-13µs + respond 9µs). NOT the bottleneck.
- The ~56s boot is genuine serial X-protocol work: ~9-10ms per cross-guest round-trip × hundreds of round-trips.
- That 9-10ms is dominated by **per-hop pickup latency**: the in-code D16 measurement is **636µs of a 742µs hop
  is just waiting to be picked up by the single dispatch task** (`crates/execution/src/javascript.rs:443`).
- `[rpc-profile]` (serial boot, 40k pump calls): **~14,000 un-inlined hops/boot** — net.write 4.7k,
  __kernel_fd_read 5.6k, __kernel_fd_write 3.7k — each paying ~636µs pickup ≈ **9s cumulative**, amplified
  under concurrent overlap.
- Concurrent boot **collapses** (0% render) because apps init-overlap and the single dispatch task can't
  service them concurrently; the 12s serial settle is the workaround that keeps only one app initing at a time.
- **Per-op off-broker inlining does NOT work** — inline `net.write` was measured SLOWER (73s) + flaky (races +
  per-op overhead negate the pickup-latency win). The fix must be WHOLESALE: service each VM on its own task.

## What's already banked (do not redo)

- **D2.1**: blocking `recv()` off-broker (gated `SECURE_EXEC_RECV_OFFBROKER`, determinism 5/5).
- **Dedup bugfix** (gated `SX_READY_GATE`): dbus/xfconfd were launched twice (colliding on the bound socket);
  fixing it took concurrent boot from "1 app launches" to "all 5 launch".
- **Profiling toolkit** (all forwarded through `measure-boot.sh`): `TEE_GUEST_STDERR` (routes guest stderr to
  host.log — unblocks ALL guest probes), `HOPPROF`, `RPC_PROFILE`, `RPC_BLOCK_US`, `DEADLINE_PROBE`,
  `POLLWAITPROF`, `POLL_TRACE`, `SX_SERIAL_SETTLE_MS`, `SECURE_EXEC_POLL_MAX_WAIT_MS`.
- Inline `net.write` scaffold (gated OFF `SECURE_EXEC_INLINE_SOCKET_DATA`) — refuted, kept inert.

## Architecture facts (verified this session)

- Kernel state is FULLY per-VM partitioned: `vms: BTreeMap<String, VmState>` (`crates/sidecar/src/service.rs:860`),
  each `VmState` owns its whole `kernel` (`crates/sidecar/src/state.rs:292`). No cross-VM shared kernel state.
- Genuinely-global fields are already `Arc`/channel: `poll_waiter: Arc<PollWaiterPool>` (`service.rs:864`),
  `bridge: SharedBridge` (`service.rs:848`), the `process_event` mpsc (`service.rs:866-868`), `SharedDnsResolver`.
- Guest compute already parallel: one OS thread per session (`crates/v8-runtime/src/session.rs:300`).
- Cross-guest sockets are OUTSIDE any VM lock: per-socket reader threads hold only `Arc<SocketReadiness>` +
  buffer, and `SocketTable`/`PipeManager`/`PtyManager`/`ProcessTable`/`fd_tables` are already `Arc<Mutex>`
  interior-mutable. So one VM's socket write to another VM never needs the peer's VM lock.

## Scope (measured)

- **~82 `self.vms.{get,get_mut,insert,remove,values,iter}` sites** across: `service.rs` (12-13), `execution.rs`
  (53), `vm.rs` (9), `filesystem.rs` (4), `tools.rs` (3).
- The single dispatch loop: `crates/sidecar/src/stdio.rs:123` (`new_current_thread`), loop `:211-320`, which
  holds `&mut NativeSidecar` and fans out `for session in active_sessions` sequentially in every branch
  (`:254-262`, `:295-299`, `:307-311`).
- **`pump_process_events` (`execution.rs:3906`) holds VM/process state across `.await`** (`:4013`
  `process.execution.poll_event(...).await`). So a std `Mutex<VmState>` guard cannot be held across the await —
  the plan must use `tokio::Mutex` for the VM lock OR restructure the pump to drop the lock around each await.

## Increments (each independently BUILDABLE + determinism-verified; the type change is NOT gateable, so land it
## behind a compile that keeps single-thread behavior first, then flip to multi-thread behind a runtime gate)

### Increment 1 — Convert `vms` to `Arc<tokio::Mutex<VmState>>`, KEEP single-thread behavior.
- Change `vms: BTreeMap<String, VmState>` → `BTreeMap<String, Arc<tokio::Mutex<VmState>>>` (`service.rs:860`).
- Mechanically update all ~82 access sites: `self.vms.get(id)` → clone the `Arc`, then `.lock().await` (async
  ctx) at the point of use, scoping the guard so it is NOT held across an unrelated `.await`. Where a site is
  in a sync fn, use `try_lock`/`blocking_lock` carefully OR thread the guard in. Expect this to be the bulk of
  the work; do it in small batches, `cargo build` after each file.
- Runtime stays `new_current_thread`; tasks still run one-at-a-time, so locks are uncontended and behavior is
  identical. **DoD: builds clean; serial 5/5 zero-black; boot time unchanged (~56s).** This de-risks the type
  change before any concurrency.

### Increment 2 — Multi-thread runtime + per-session servicing tasks, behind `SX_PARALLEL_VMS`.
- `stdio.rs:123` `new_current_thread` → `new_multi_thread` (gate: when `SX_PARALLEL_VMS!=1`, cap worker threads
  to 1 to preserve single-thread behavior for the default/baseline).
- Replace the serial fan-out (`stdio.rs:254-262,295-299,307-311`) with one `tokio::spawn` servicing task per
  session, each locking ONLY its own VM's `Arc<Mutex<VmState>>`. So VM A's fs/socket/spawn RPCs run concurrently
  with VM B's — killing the cross-guest pickup latency.
- **Per-VM event routing:** the single global `process_event` mpsc (`service.rs:866-868`, drained
  `execution.rs:3948`) must be split/keyed per VM (or per-VM pending queues) so a session task drains only its
  own events. Preserve the F1 event-driven notify (`stdio.rs:283`) + the timer safety net (`stdio.rs:302`).
- **DoD: serial 5/5 zero-black held; CONCURRENT boot (SX_SERIAL_LAUNCH=0) converges (was 0/3) and BOOT_MS drops.**

#### Increment 2 — CONCRETE DESIGN (mapped 2026-07-04)
The pickup latency is in the RPC SERVICING (`service_javascript_sync_rpc`), serialized because the whole
dispatch runs `&mut NativeSidecar` on one task. Parallelizing only event-POLLING would not help — the servicing
(the expensive part) must run per-VM. Good news: the hot RPCs (net.*, fs.*, __kernel_fd_*) only touch the VM's
`kernel` (now `Arc<Mutex<VmState>>`) + the Arc-shared `bridge` + `poll_waiter`. So the surgical shape:
1. **Route the HOT per-VM RPCs to per-session tasks.** Extract the hot-RPC servicing into a path callable with
   `&self` + Arc handles (VM `Arc<Mutex>`, `bridge`, `poll_waiter`). Spawn one `tokio::spawn` task per session
   that drains that session's guest sync-RPCs and services them locking ONLY its own VM. Keep the RARE RPCs that
   touch shared engines/connections/sessions (`wasm.thread_spawn`, `child_process.spawn`, vm lifecycle) routed
   back to the main dispatch task (they are infrequent, not on the pickup-latency hot path).
2. **Interior-mutability scope (the refactor cost):** the fields a servicing task or the main dispatch both
   touch must become interior-mutable/Arc: `pending_process_events` (VecDeque → `Arc<Mutex>` or per-VM queue),
   the sidecar-response tracker, and the event queue. Most other NativeSidecar fields (engines, connections,
   sessions, next_* counters) are touched only by the MAIN dispatch (frame handling, vm lifecycle), so they can
   stay `&mut self` on the main task — do NOT convert them; keep the per-session tasks to the VM + Arc handles.
3. **Per-VM event routing:** split `pending_process_events`/`process_event` so a session task drains only its
   own VM's events; the main dispatch still assembles outbound frames. Preserve F1 notify + the timer net.
4. **lock_vm under multi-thread:** each VM is serviced by ONE task, so its lock is normally uncontended — BUT
   the main dispatch may also touch a VM (lifecycle). So change `lock_vm` from try_lock+panic to a BLOCKING
   `lock()` for genuine cross-thread contention PLUS a thread-local held-set that panics only on SAME-thread
   reentry (the reentrant bug we actually hunt). Do this as the first Increment-2 step.
5. **Gate everything behind `SX_PARALLEL_VMS`** (default = single-thread, current behavior byte-identical), so
   the determinism baseline is one env-flip away and the concurrency is A/B-testable + instantly revertible.
This is a LARGE refactor (dispatch restructure + interior mutability of the event path + per-session task
spawning). Land it in sub-revisions: (2a) lock_vm multi-thread-safe + `SX_PARALLEL_VMS` multi_thread flag;
(2b) `pending_process_events` interior-mutable + hot-RPC servicing extracted to an Arc-handle path; (2c) spawn
per-session servicing tasks + per-VM event routing. Determinism-gate after each.

### Increment 3 — Delete the serial-settle scaffold; flip defaults ON.
- Once concurrent boot is 5/5 FULL, remove `SX_SERIAL_LAUNCH`/settle (`host/src/main.rs:1305`, `:2404`), and
  flip `SX_READY_GATE`, `SECURE_EXEC_RECV_OFFBROKER`, `SX_PARALLEL_VMS` defaults ON.
- Add precise readiness gating for launch order (X serving + dbus serving + xfconfd serving + WM managing),
  then launch the mutually-independent apps concurrently. Boot ≈ infra + max(app-init).
- **DoD: concurrent boot 5/5 FULL, ZERO total-black, BOOT_MS ≤15s (best-effort ≤10× native ≈5.4s).**

## Verify protocol (run after EVERY increment, in this order)

1. **Determinism gate FIRST (revert-gate):** `RUNS=5 SX_SERIAL_LAUNCH=1 SX_READY_GATE=1
   SECURE_EXEC_RECV_OFFBROKER=1 experiments/wasm-gui/scripts/measure-boot.sh` → must be 5/5 FULL, coverage
   ≥40% (FULL_MIN=40), ZERO total-black, median ≈56s (no regression). Any regression → revert the increment.
2. **Concurrent convergence:** `RUNS=5 SX_SERIAL_LAUNCH=0 SX_READY_GATE=1 SX_PARALLEL_VMS=1` → must converge
   (was 0/3) and BOOT_MS trend toward ≤15s.
3. **No spin / native idle:** `SIDECAR_IDLE_CPU_CORES` must not regress (native ~0).
4. Commit each green increment to `wasm-gui-desktop` (PR #104) with the before/after numbers.

## Risks + mitigations

- **Determinism regression (top risk):** determinism gate re-run before AND after each increment; Increment 1
  keeps single-thread behavior so it can't change semantics; Increment 2 is behind `SX_PARALLEL_VMS`.
- **Held-guard-across-await deadlock:** use `tokio::Mutex`; audit every `pump_process_events` await to ensure the
  VM guard is dropped around awaits, or the await is on per-process state cloned out of the guard.
- **Cross-VM lock deadlock:** a session task locks ONLY its own VM; all cross-guest IO goes through the lock-free
  socket buffers/readiness (never lock the peer VM). Any genuine two-VM op acquires locks in `vm_id` order.
- **Event routing loss/reorder (Increment 2):** keep the notify + timer safety nets; unit/scenario-test that no
  process event is dropped or reordered under the per-VM split.
- **Not gateable type change:** Increment 1 lands the type change with identical single-thread behavior first, so
  the risky concurrency (Increment 2) is isolated behind a runtime gate.

## Done criteria (the goal)

- Concurrent boot renders 5/5 FULL (coverage ≥40%, zero total-black) with the serial-settle scaffold removed.
- BOOT_MS ≤15s median (best-effort toward ≤10× native ≈5.4s; native baseline 0.548s).
- Serial determinism unaffected; `SIDECAR_IDLE_CPU_CORES` at ~native idle.
- All wins committed to `wasm-gui-desktop`, `DESKTOP-BOOT-PERF.md` + `LINUX-DIVERGENCES.md` updated.

---
## STATUS LOG

### 2026-07-04 — Increment 1 LANDED (committed, deadlock-free) — with a 4/5 flakiness to resolve
- `vms` → `Arc<Mutex<VmState>>` converted across ~96 sites; builds clean; 85/85 lib + VM-lifecycle/kill/signal
  integration tests pass.
- **Deadlock hunt (the hard part):** the naive conversion deadlocked the desktop (5/5 timeout, 7.7s CPU,
  launched=False). Root causes found via a `lock_vm()` try_lock+backtrace detector:
  1. pump held a std guard across `poll_event(ZERO).await` → replaced with the sync `poll_event_blocking(ZERO)`.
  2. `spawn_wasm_thread` held a value-lifetime guard across a later `active_processes.insert` re-lock — the
     conversion turned NLL-scoped `get_mut` borrows (end at last-use) into value guards (end at scope-end),
     so sequential locks now OVERLAP. Scoped the extract-guard to drop before the re-lock.
  → after the fixes: desktop RENDERS (95%), normal CPU/threads, no reentrant panic.
- **Determinism: 4/5 FULL at 55.1s median** (matches baseline speed), 1/5 app-overlap panel-only. Two gates
  both 4/5. Baseline was 5/5 (2 gates) — so either a small Increment-1 timing effect or the boot's inherent
  ~90% reliability sampled unluckily. TODO before Increment 2: confirm vs a baseline re-gate; if real, tighten
  the remaining value-guard lifetimes (drop at last-use like the original NLL borrows) to cut lock-hold time.
- lock_vm() detector kept (free on the happy path; panics-with-backtrace on any future reentrant regression).

### 2026-07-04 — Step 0 RESOLVED: Increment 1 is clean (baseline is ALSO 4/5)
Re-gated the pre-Increment-1 parent (ef9f96cf) serial ×5: 4/5 FULL, 55.8s median, 1/5 app-overlap panel-only
timeout — IDENTICAL to Increment 1's 4/5/55.1s. ∴ Increment 1 did NOT regress determinism; it is behavior-
identical (single-thread, uncontended). The ~20% app-overlap flakiness is INHERENT to the serial-settle boot
(the earlier 5/5 gates were lucky samples of an ~80%-reliable process). The fragile settle-gate is the
flakiness source → removed in Increment 3 (concurrent launch). No guard-tightening needed. Proceed to Increment 2.
NOTE: the "5/5 determinism gate" is really ~80% inherent reliability today; treat "matches baseline (~4/5, ~55s,
zero HARD deadlock/black-on-all-runs)" as the pass bar per increment, with true 5/5 the Increment-3 outcome.

### 2026-07-04 — Increments 2a / 2c-1 / 2c-2 LANDED — CONCURRENT CONVERGENCE 0/3 → 5/5
- **2a** (committed): `lock_vm` made multi-thread-safe — blocking `lock()` for genuine cross-thread contention
  + a thread-local held-set that panics only on SAME-thread reentry (the reentrant bug we hunt). Gated
  `SX_PARALLEL_VMS` runtime flag. No default behavior change; SX_PARALLEL_VMS=1 boots FULL.
- **2c-1** (committed): extracted `service_hot_javascript_sync_rpc(vm, bridge, poll_waiter)` — the hot generic-arm
  servicing + response delivery as a free fn taking a single `&mut VmState` (one lock, no re-lock). Behavior-
  preserving (3/3 FULL, 85 lib tests). Prereq for the per-VM servicing threads.
- **2c-2** (committed): **per-VM OS-thread servicing.** Each VM gets a dedicated OS thread (`serve_vm_hot_rpcs`)
  locking only its own `VmState`, servicing that VM's hot sync-RPCs + draining its process events; the main
  dispatch skips VM polling under `SX_PARALLEL_VMS` and only drains the forwarded (non-hot) events. This kills the
  cross-VM pickup latency (the D16 diagnosis).
  - **Design pivot that mattered:** first tried `tokio::spawn` tasks on a `new_multi_thread` runtime → the
    servicing loop blocks on the VM's `std::sync::Mutex`, so N contending tasks **starve the shared worker pool**
    → 2/5 FULL, 3/5 all-black (cov=0.0%, main pump alive but per-VM tasks wedged). Switched to a **dedicated
    std::thread per VM** (blocking_send to the forwarded-event channel, 250µs idle poll) + reverted the main
    dispatch to `new_current_thread`. Dedicated threads mirror a native per-process scheduler and remove the pool
    starvation entirely → **5/5**. (An adaptive idle-backoff experiment reintroduced a black run without lowering
    idle CPU — the per-VM poll is NOT the idle-CPU driver; reverted to flat 250µs.)
  - **Unblocked by making the VFS mount boundary `Send`:** `MountedFileSystem: Any + Send`, `Send` bound on the
    `impl VirtualFileSystem` mount params, `Box<dyn MountedFileSystem + Send>`. Behavior-neutral; **no plugin was
    non-Send** (s3/host_dir/google_drive/sqlite/js_bridge/sandbox_agent/module_access all Send already).
  - **Results:** serial determinism gate **3/5 FULL @55.4s, zero hard-black** (matches baseline ~4/5, inherent
    panel-only flakiness — serial code path unchanged; the notify_waiters change is behind the OFF-by-default
    event-ingest flag). Concurrent `SX_PARALLEL_VMS=1` **5/5 FULL @53.0s (was 0/3)** — decisively converged, and
    now MORE reliable than the serial settle-gate. Idle CPU 0.60 median (~baseline 0.55).
- **Why boot is still ~53s (not ≤15s):** the serial-settle launch scaffold still serializes timing; Increment 2
  delivered *parallel servicing* (the enabler), not the concurrent-launch speedup. The ≤15s target is Increment 3
  (delete the settle scaffold + readiness-ordered concurrent launch + flip defaults ON) + replacing the 250µs
  per-VM poll with an event-driven wakeup graph.

### 2026-07-04 — ⚠️ CORRECTION: the "0/3 → 5/5" above was a MEASUREMENT CONFOUND. Premise in question.
The entry above is **retracted as validation**. Root cause: the boot harness never plumbed `SX_PARALLEL_VMS`
to the sidecar — it was absent from measure-boot.sh's docker `-e` list AND inside-measure.sh's host-env prefix
(both fixed 2026-07-04). So **every "concurrent 5/5" number above ran with per-VM threads OFF** (settle-gated
concurrent LAUNCH on the serial sidecar). Confirmed by the sidecar thread histogram: prior runs peaked ~151-154
threads with NO `sx-vmsvc-*` threads; only after the plumbing fix do 20+ `sx-vmsvc-vm-*` threads appear
(peak ~158-168).
- **First real per-VM-ON boot (plumbing fixed):** `SX_PARALLEL_VMS=1` concurrent → **5/5 systematic panel-only
  (cov=4.8%)** — the WM + panel come up but the heavy apps never render. Distinct, reproducible failure mode
  (all 5 identical), NOT the load-induced random all-black that a PV=0 control shows.
- **Why (architectural):** ALL rendering funnels through ONE single-threaded Xvfb guest. Dispatch pickup latency
  (the D16 diagnosis) is NOT the boot's binding constraint — the Xvfb serial render throughput is. Giving every
  guest an always-polling (250µs) servicing thread oversubscribes CPU and contends the hot Xvfb lock, starving
  the render → panel-only. The per-VM design optimizes the wrong bottleneck for this workload.
- **Caveat:** the PV-ON run overlapped external CPU load (a 65-min test-fix subagent build; separately, another
  session's agentos-sidecar at ~150% CPU), so a definitive PV-ON vs PV-OFF A/B needs a QUIET machine. But the
  *systematic* panel-only signature (vs load's random black) already points at per-VM threads, not noise.
- **What still stands (unconditional wins, independent of the premise):** the VFS `Send` refactor (neutral,
  enables future threading), the `configure_vm` reentrant-deadlock fix, the full sidecar test binary compiling
  again (133 lock_vm repairs), and Increment 3's env-plumbing + tunable launch orchestration (SX_INFRA_*/
  SX_WM_FALLBACK_MS/SX_LAUNCH_STAGGER_MS + quiet-gate).
- **Open decision (needs owner input):** keep `SX_PARALLEL_VMS` OFF and pivot the ≤15s effort to the real
  bottleneck — Xvfb render / framebuffer-write throughput (the SAB dataBuffer lever, see the M8.6 futex-storm
  memory) — OR run a clean-machine A/B first to be certain per-VM servicing is a net regression before shelving it.

### 2026-07-05 — RESOLVED: controlled A/B confirms per-VM = regression; per-VM stays OFF; Increment 3 shipped as tunables
- **Interleaved A/B (PV=1 vs PV=0 alternating, same load window, RUNS=1 ×5 pairs):** PV=1 = **0/5 FULL, 5/5
  identical panel-only (4.8%)**; PV=0 = **4/5 FULL @~52s** (1 load-induced black). The perfectly-systematic
  panel-only under PV=1 vs the control's random variance is conclusive: **per-VM servicing is a net regression
  for the desktop boot.** The Xvfb single-threaded render is the binding constraint, not dispatch pickup latency.
  → `SX_PARALLEL_VMS` stays **OFF by default** (code default already false; the feature is preserved for
  many-independent-VM workloads, just not the shared-Xvfb desktop).
- **Increment 3 disposition:** the boot is inherently fragile to external scheduling jitter (single-threaded host
  broker) — every compressed launch config (C1, WM-only, "modest") collapsed to all-black under the concurrent
  external load (other sessions: agentos-sidecars + rustc) that the *generous* defaults survived. A validated
  faster DEFAULT needs a quiet machine. So Increment 3 ships the **mechanism** (env-tunable infra/WM/stagger/
  settle + the harness plumbing fix) with **robust defaults**; the aggressive compression toward ≤15s is left to
  the tunables + a quiet-machine window. **≤15s is not achievable via launch orchestration alone** — it is gated
  on the Xvfb render/framebuffer-write throughput lever (the real remaining work).
- **Deadlock fix verified:** the `configure_vm` reentrant re-lock is gone (net_poll_suite no longer panics
  `[REENTRANT-VM-LOCK]`; it now reaches a pre-existing fake-wasm-fixture `start_execution` failure, unrelated to
  this work). 85 lib + all kernel + 87/88 service integration tests pass.
- **Landed on `wasm-gui-desktop` (PR #104):** `rumlrwpk` (2c-2: per-VM servicing gated OFF + VFS Send refactor +
  deadlock fix + test-binary repair) and `yptupqtv` (Increment 3: tunable launch orchestration + plumbing fix).

### 2026-07-05 — Xvfb-throughput lever (T1 SAB fb-write) INVESTIGATED: a modest reliability win, NOT the ≤15s unlock
Pursued the framebuffer-write throughput lever. The SAB bulk fb-write path (guest `maybeBulkEncodeFsPayload` →
kernel `read_bulk_arg`, gated `SECURE_EXEC_T1_RING=1`, 8 MiB bulk buffer) is already **fully built** by a prior
session (`#[allow(dead_code)]`); a `[fb-delta]` diff also already keeps most Xvfb writes as small changed-block
runs on the cheap base64 path, so T1 only replaces the occasional **full-frame** (≥64 KiB) write. Plumbed
`SECURE_EXEC_T1_RING` (+ a new `SX_BURST_LAUNCH=1` fb-write stress option) through the harness (both were missing,
like `SX_PARALLEL_VMS`). Interleaved A/Bs under external load:
- **Settle-gated PV=0, T1 on vs off (4 pairs):** T1 ON **3/4 FULL**, T1 OFF **1/4 FULL** — in the same load
  moment T1 ON rendered while T1 OFF went all-black. → T1 is a **modest reliability win** (fewer fb-write-
  starvation blacks). No speedup (53s is launch-serialization bound).
- **Burst (all apps concurrent), T1 on vs off:** 8/8 all-black regardless of T1 → the burst's bottleneck is the
  **single dispatch thread (PV=0) overwhelmed by 5 concurrently-initializing guests**, not fb-write encoding.
- **PV=1, T1 on vs off (4 pairs):** 8/8 identical panel-only regardless of T1 → **T1 does NOT fix the per-VM
  regression** (that stall is CPU oversubscription from 20+ always-polling threads, not the fb-write lock-hold).
  The "PV=1 (parallel dispatch) + T1 (cheap fb-write)" combination also fails.
**Conclusion:** T1 is worth shipping as an opt-in reliability lever (validate on a quiet machine before flipping
the default), but it is **NOT the ≤15s unlock.** The real remaining blocker is **concurrent guest init**: 5 heavy
wasm guests cannot initialize at once (single dispatch overwhelmed under PV=0; CPU-oversubscribed under PV=1) —
a deeper concurrency-management problem than framebuffer-write throughput. `SECURE_EXEC_T1_RING` + `SX_BURST_LAUNCH`
stay OFF by default. Full artifacts: `~/progress/secure-exec/2026-07-05-xvfb-throughput-sab/`.

### 2026-07-05 — Cheaper-init + orchestration-waits INVESTIGATED: ≤15s is architecturally out of reach; ~53s is a load-bearing equilibrium
Profiled the boot end-to-end (RPCPROF per-guest syscall table + PATHOPENPROF paths + milestone timeline).
- **WASM compile is NOT the cost:** `new WebAssembly.Module` for a 13.8MB GTK app = **11ms** (V8 lazy compile);
  instantiate 1-4ms. So compile-cache / isolate-snapshot buy ~nothing. (The bridge-JS snapshot is already shared.)
- **Per-guest init = a FS SCAN STORM, but its DIRECT cost is cheap (~700ms total, 60k calls @ ~30µs).** The
  dominant consumer is **xfwm4** re-reading `/usr/share/themes/Greybird/xfwm4/*` — 113 assets × 13 reopens = 1469
  opens + ~7000 stats (NOT fontconfig: 18 font opens). Redundant immutable re-reads.
- **The ~53s is ORCHESTRATION WAITS, not work:** ~12s infra (dbus 2s + xfconfd 4s + Xvfb ~6s) + ~12s WM gate +
  ~27s per-app settle gaps. The fs storm matters only INDIRECTLY (keeps apps chatty → longer settles; overlapping
  storms overwhelm the single dispatch under concurrency).
- **WM-ready EWMH signal: dead end.** Replacing the 12s silent-xfwm4 fallback with a `_NET_SUPPORTING_WM_CHECK`
  probe: the property is set only ~49s in (after render, LATER than the 12s fallback) because xfwm4's full init
  under single-dispatch contention takes ~37s. No usable earlier signal; the 12s fallback is load-bearing. Reverted.
- **Every compressed launch / trimmed-wait config collapses under external CPU load** that the generous defaults
  survive — the boot is jitter-fragile (single-dispatch ceiling), so aggressive orchestration trims need a quiet host.

**Overall conclusion (6 levers taken to ground):** per-VM servicing (regression), launch orchestration (tunable
but load-fragile), Xvfb SAB throughput (modest reliability win), compile-cache (refuted, 11ms), fs-scan reduction
(cheap to service), WM-ready signal (EWMH too late) — NONE is the ≤15s unlock. The ~53s settle-gated boot is a
deeply-constrained equilibrium bound by the **single-owner `&mut sidecar` dispatch pump** serializing all guests'
syscalls (ROOT-2-MULTIPLEX-DESIGN.md) + load-fragility. ≤15s needs a fundamental dispatch re-architecture (a
BOUNDED servicing thread-pool — NOT per-VM-thread, which oversubscribed) — a major separate project, not a lever.
Shipped this session (all OFF/opt-in by default): tunable launch orchestration, T1/burst plumbing, RPCPROF/path
profiling plumbing, the `configure_vm` deadlock fix, the VFS Send refactor, test-binary repair.

### 2026-07-06 — ROOT CAUSE FOUND: the boot is NOTIFY-GAP bound, not dispatch-bound. Fix mechanism PROVEN.
Phase 1 (de-risk) OVERTURNED the whole premise:
- **The single dispatch pump is 99.7% IDLE** (DISPATCH BUSY 0.3-0.36%) in BOTH successful and collapsed boots
  (SECURE_EXEC_RPC_WATCHDOG_DUMP_MS). A dispatch thread-pool is REFUTED — it parallelizes an idle resource.
- **The boot = (thousands of X round-trips) × (peerWait ~8.3ms/hop).** HOPPROF: wakeLag notify→resume = 8µs,
  respond = 8µs (wake+delivery INSTANT), so the ~8.3ms is peerWait.
- **peerWait is a NOTIFY GAP, not compute:** 80-96% of poll_wait completions are DEADLINE (Xvfb = 96%),
  DEADLINE_PROBE 0% data-at-block-entry. The peer wakes on its ~8ms POLL CLAMP instead of on incoming data,
  because the writer does not notify the peer's readiness — so every hop pays the clamp.
- **Fix mechanism PROVEN:** `SECURE_EXEC_INLINE_PEER_NOTIFY` (lever 1) wakes the peer inline on write. It was
  wired ONE direction only (client→server, via the listener-host-path registry). Added the REVERSE direction
  (server→client): a per-guest-path FIFO where net.connect stashes the client's readiness and net.accept pops it
  to pair the accepted socket's peer_readiness. Result: **xfdesktop deadline 78%→34%, xfwm4 79%→51%** — the
  mechanism demonstrably cuts the deadline waits where it applies.
- **REMAINING (the dominant path):** Xvfb stays 90% deadline + peerWait unchanged, because the inline peer-notify
  is wired only into host-unix `net.write` (`process.unix_sockets`), but the X protocol's hot writes go through
  the KERNEL-fd socket path (`__kernel_fd_write` → `kernel.fd_write`) which does NOT notify the peer's readiness.
  NEXT: extend inline peer-notify to the kernel-socket write path (both directions) so client requests wake Xvfb
  and Xvfb's replies wake the clients — that should collapse peerWait from 8.3ms toward the 8µs wakeLag floor.
- Plumbed for this work: SECURE_EXEC_RPC_WATCHDOG_MS/_DUMP_MS, SECURE_EXEC_RPCPROF. All fixes gated OFF by default
  (INLINE_PEER_NOTIFY). Artifacts: ~/progress/secure-exec/2026-07-05-xvfb-throughput-sab/dispatch-decomposition.md.
