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
