# XU7 simultaneity — implementation plan (handoff)

Status: 4 of 5 Xubuntu components render **individually** in secure-exec (proof screenshots in
`~/tmp/gui-progress/2026-06-27T*/`): xfwm4 (decorates), Mousepad (decorated window), xfce4-panel (clock),
xfdesktop (wallpaper + icons). The **only** unmet XU7 criterion is running them **simultaneously, live,
responsive**. This doc is the precise spec for the remaining work.

## Empirically-established root cause (not assumption)

- Each component renders **solo** (xfwm4 + that one component) at 100%.
- **Any two heavy components together → only one paints; the second starves to ~0%.** Proven across:
  panel+xfdesktop (→ black, panel's GDBus connect times out), mousepad+xfdesktop (→ Mousepad only),
  full 5-client (→ Mousepad only).
- The starvation is **not slowness**: mousepad+xfdesktop at a **450s** timeout still rendered only Mousepad,
  neither client exited. More time does not help → the 2nd guest makes ~no rendering progress while the 1st runs.
- The sidecar sync-RPC **service thread is ~99% idle** (host profiler: ~909ms serviced over a 120s run). So the
  bottleneck is **not service-thread saturation** — it is **per-round-trip cross-thread wake latency** (~180µs ×
  hundreds of thousands of X/D-Bus round-trips) combined with the **single-threaded servicing path**: when two
  guests' round-trips interleave, each round-trip's latency rises until the second guest can't progress to map+paint.
- V8 constraint (verified): `Atomics.wait` is isolate-internal, so Rust cannot directly wake a blocked guest — the
  wake must traverse the deferred-responder → response-pipe → guest worker `readSync` → `Atomics.notify` chain.
  Bounded hop-reductions (the committed, gated `SECURE_EXEC_POLL_DIRECT` reader-direct change) give only ~25–33%.

## What is already committed (perf-pivot-work)

- Bulk-SAB large-binary-arg transport (JS+WASM, hostile-validated; sidecar stays `forbid-unsafe`).
- Host-side sync-RPC profiler: `SECURE_EXEC_RPCPROF=1` (per-method servicing time, real host clock).
- Kernel-pipe-write readiness notify (cross-thread GWakeup wakes `net.poll_wait` immediately).
- `SECURE_EXEC_POLL_DIRECT` reader-direct deferred-poll completion (gated, default off; marginal +signal at light load).
- Harness: `test-xu7-full.sh` outer-timeout = inner+45s (fixes empty-fb readback); `XU7_LITE`, `XU7_PANEL_ONLY`,
  `XU7_ONLY="<components>"` diagnostic modes.

## Verified feasibility fact

`SidecarKernel` (= `KernelVm<MountTable>`) **is `Send`** (compile-proved). So sharing VM state across threads is
type-feasible — but `Send` is necessary, not sufficient (see Path A constraints).

---

## Path A — intra-VM per-process concurrency (the direct fix)

Make the VM service concurrent guests' sync-RPCs in parallel (20 cores here, ~10 free), so a busy/active guest
doesn't starve co-tenants.

**Scope (why it's multi-day):** all guest sync-RPCs funnel through a single-threaded `&mut self` event loop
(`crates/sidecar/src/execution.rs:~4130` drains `ActiveExecutionEvent::JavascriptSyncRpcRequest` →
`handle_javascript_sync_rpc_request(&mut self)` → `self.vms.get_mut(vm_id)` →
`service_javascript_sync_rpc(&mut kernel, &mut process)`). There are **~137 `&mut self` methods** on the servicing
path (execution.rs 87 / service.rs 45 / filesystem.rs 5). The desktop is **one VM, many processes**, so the
concurrency must be **per-process inside one VM**, not per-VM.

**Plan:**
1. Move the per-VM mutable state (`kernel` + process table + socket table) behind one `Arc<Mutex<VmState>>`
   (coarse lock — kernel ops are ~0.5µs, so contention is negligible; correctness by mutual exclusion, no fine-grained
   race surface).
2. Drain **each guest process's** execution events on its own task/thread (not one serial loop over all processes).
3. Each task services one request under a **brief** lock; **all blocking stays outside the lock** (`net.poll_wait`
   is already deferred to `PollWaiterPool` — keep it; audit `net.poll` drain and fs ops to confirm they don't block
   under the lock).
4. Gate behind `SECURE_EXEC_SERVICE_THREADS` (default 1 = today's exact behavior, zero regression).
5. **Validate on a quiet (non-shared) box** — this machine is shared (~10 load) and noisy (~6× render variance), so
   concurrency correctness and the simultaneity win cannot be trusted here.

**Risk:** this is the security TCB. A coarse single `Mutex` avoids data races by construction, but the change is
pervasive and must be reviewed. Do NOT ship a half-converted state.

## Path B — co-locate the X server + clients (sidesteps the kernel refactor)

Run Xvfb + its X clients in **one isolate** and short-circuit their loopback X socket **in-isolate** so X-protocol
round-trips become in-process function calls (~µs) instead of cross-thread IPC (~180µs) — a ~100× cut on the
dominant cost. Legitimate under the trust model (the sandbox boundary is sidecar↔executor; the user's desktop apps
are mutually-untrusted-together already — co-location only couples their crash-fate, not the sandbox).

**Scope:** requires the wasm runner to host multiple wasm modules in one isolate with a cooperative scheduler
(when one module's event loop blocks on poll, run the other), plus detecting + short-circuiting intra-isolate
loopback sockets. Deep runtime change; deadlock-prone; also needs a quiet box to validate.

## The 5th component — thunar

Fails even **solo**: a worker thread exits before the window maps. The `GIO_USE_VOLUME_MONITOR=null` warning is
benign (host sets it deliberately to avoid GIO's native volume monitor, which deadlocks on wasm). The real blocker
is the **file-manager async-job model** (glib async + wasm-threads) — the same hard area the M8.5 pcmanfm work left
PARTIAL. Needs the wasm-threads effort.

## Validation bar (for whoever does A or B)

On a quiet box: `XU7_ONLY="xfce4-panel xfdesktop"` (then full 5-client) must show **both/all** components painted
together (panel clock strip + xfdesktop wallpaper + decorated app windows), stable, with input (type/click/switch)
demonstrated. Measure before/after with `SECURE_EXEC_RPCPROF=1` and render nonblack%/component-visibility.
