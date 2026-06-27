# XU7 simultaneity — implementation plan (handoff)

Status: 4 of 5 Xubuntu components render **individually** in secure-exec (proof screenshots in
`~/tmp/gui-progress/2026-06-27T*/`): xfwm4 (decorates), Mousepad (decorated window), xfce4-panel (clock),
xfdesktop (wallpaper + icons). The **only** unmet XU7 criterion is running them **simultaneously, live,
responsive**. This doc is the precise spec for the remaining work.

## The 2nd-guest starvation is INHERENT, not a shared-box/scheduler artifact (tested)

I hypothesized the failure was scheduler-wakeup latency under this shared box's load (~10) and **tested it**: at the
quietest observed window (**load 8, ~12 of 20 cores free**), `mousepad+xfdesktop` STILL rendered only Mousepad
(67.5%, wallpaper 0%) — identical to the load-10 runs. So with ample free cores the 2nd guest still starves → the
cost is the **inherent wake-chain latency × Xvfb's single-thread serialization** (Xvfb is one thread serving N
clients; per-X-round-trip wake doesn't parallelize across cores), **not** core contention. A quieter/dedicated box
does NOT fix multi-app simultaneity (it may give ~2× single-app throughput per the earlier contamination finding,
but that does not lift a 0%-starved 2nd guest). **The deep fix is genuinely required — there is no quiet-box
shortcut.** (This corrects an earlier draft of this doc that recommended trying a quiet box first.)

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

## ★ Which path? (settled by the threading model — read first)

Three established facts settle this:
1. **Each guest isolate runs on its own dedicated thread** (`crates/v8-runtime/src/session.rs:1` — "V8 isolates on
   dedicated threads"). So guests already execute **concurrently** on separate cores (20 cores, ~10 free).
2. The sidecar sync-RPC **service path is ~99% idle** (~909ms serviced over 120s).
3. The 2nd guest still starves to 0% even at a 450s timeout (starvation, not slowness).

Together these mean the bottleneck is **per-round-trip cross-thread WAKE LATENCY**, not any thread being saturated.
Each X/D-Bus round-trip hops guest-thread → sidecar dispatch → Xvfb-thread → back, ~180µs each; **Xvfb (one thread)
serving N clients** is throughput-limited by that latency, so a 2nd client's startup request volume never drains.

- **Path A (service-thread multiplex) does NOT fix this** — the service path is idle and guests are already on
  separate threads; multiplexing an idle path adds no concurrency where the bottleneck is latency. **Do not invest
  the multi-day refactor in path A.** (This corrects task #19's framing.)
- **Path C (adaptive spin-poll transport) — RECOMMENDED FIRST.** The whole problem is the kernel can't wake a blocked
  guest, forcing the slow wake chain. Have the kernel write responses to the per-guest SAB and guests **spin-poll**
  it (adaptive: spin a short budget, then fall back to today's blocking path). Cuts the hot-path round-trip ~180µs →
  ~µs with NO co-location, NO Asyncify, NO TCB-concurrency refactor — just a gated transport change on the SAB infra
  already in tree. Lowest-cost, lowest-risk shot at the real fix. (Details below.)
- **Path B (co-location) — fallback if C's spin can't be tuned** — co-locate Xvfb + clients so X round-trips are
  in-process; eliminates the wake entirely but needs Asyncify/fibers + a cooperative scheduler (heavier). (Details below.)

**Verify on a quiet box** with `SECURE_EXEC_RPCPROF=1` before building, but the threading model already rules out A.

## Path A — intra-VM per-process concurrency

Make the VM service concurrent guests' sync-RPCs in parallel (20 cores here, ~10 free), so a busy/active guest
doesn't starve co-tenants. (Caveat above: the idle service thread makes this less certain to help than B.)

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

## ★ The contention point is EVERY shared single-threaded service guest (scope co-location accordingly)

The starvation generalizes beyond Xvfb. A guest gets *stuck* (0%, not just slow) when it issues a synchronous
request to a **shared single-threaded service guest** that is contended by another client:
- **Xvfb** (X server) — serves all X clients on one thread.
- **dbus-daemon** (session bus) — serves all bus clients on one thread. *Direct evidence:* the panel failed with
  "Failed to connect to the D-BUS session bus: Timeout" under contention; xfdesktop's xfconf calls route over this bus.
- **xfconfd** (config) — a bus service, same pattern.

Each is throughput-limited by the per-round-trip wake latency, so with 2+ clients the 2nd client's synchronous
calls hang past their (GLib, un-tunable per Constraint #5) timeouts → stuck. **Implication for Path B:** co-location
must put the **service daemons (Xvfb, dbus-daemon, xfconfd) in the same isolate as the clients** (in-process IPC for
X *and* D-Bus), or the bottleneck simply moves from Xvfb to the bus. Co-locate the whole desktop session, not just
X.

## Path B — co-locate the X server + clients (sidesteps the kernel refactor)

Run Xvfb + its X clients in **one isolate** and short-circuit their loopback X socket **in-isolate** so X-protocol
round-trips become in-process function calls (~µs) instead of cross-thread IPC (~180µs) — a ~100× cut on the
dominant cost. Legitimate under the trust model (the sandbox boundary is sidecar↔executor; the user's desktop apps
are mutually-untrusted-together already — co-location only couples their crash-fate, not the sandbox).

**Scope:** requires the wasm runner to host multiple wasm modules in one isolate with a cooperative scheduler
(when one module's event loop blocks on poll, run the other), plus detecting + short-circuiting intra-isolate
loopback sockets. Deep runtime change; deadlock-prone; also needs a quiet box to validate.

**★ Critical dependency / hard part:** each guest is a *synchronous, blocking* event loop (Xvfb `WaitForSomething`,
the client GTK main loop). When module A's wasm calls `poll()` it synchronously enters the V8 host and blocks the
isolate thread — you cannot "pause A and run B" in one thread without stack-switching. So co-location needs ONE of:
(a) **Asyncify** the guest wasm (build-time transform so blocking host calls can suspend/resume) — guests are NOT
currently built with it; or (b) a **fiber/coroutine** mechanism for the runner. This is the real cost of Path B
(not the socket short-circuit, which is comparatively easy once scheduling exists). Evaluate (a) vs (b) first.

## Path C — adaptive spin-poll transport (likely the MOST tractable; no TCB-concurrency, no Asyncify)

The whole problem is that the kernel **can't wake a blocked guest** (V8 `Atomics.wait` is isolate-internal), forcing
the slow multi-hop wake chain (deferred responder → pipe → worker `readSync` → `Atomics.notify` → main). **Sidestep
the wake entirely:** have the kernel write each sync-RPC response into the per-guest **SAB** (the T1 ring SABs are
already allocated — `__secure_exec_t1_req/resp/bulk` in session.rs), and have the guest **spin-poll** the SAB for the
response instead of blocking. No pipe, no worker, no `notify` — hot-path round-trip drops from ~180µs to ~µs, so the
single-threaded service guests (Xvfb, dbus-daemon) can serve multiple clients. Guests already on dedicated threads;
this changes only the *transport* (gated, default off = today's pipe+Atomics path → no regression, no TCB races).

**The known objection + how to handle it:** a *pure* busy-spin "burns a core per blocked guest and starves under
contention" (the earlier T1-ring analysis's reason for skepticism). Resolve with **ADAPTIVE spin**: spin for a small
budget (catches the hot path — during the startup burst responses arrive in µs), then **fall back to the existing
blocking wake chain** for idle waits (no CPU burn when truly idle). With 20 cores (~12 free during the burst), the
brief multi-guest spin is affordable exactly when it's needed. **Crucially, the SERVER guests (Xvfb, dbus-daemon)
must spin-poll their client sockets too** — else they're still slow to *produce* replies (a client spinning for a
reply Xvfb hasn't sent yet doesn't help).

**Why this may beat B:** no Asyncify/guest rebuild, no cooperative scheduler, no co-location, no kernel-concurrency
refactor — just a gated transport change + adaptive backoff, built on the SAB infra already in tree. **Validate on a
quiet box** and tune the spin budget; measure hot-path round-trip latency and multi-client render before/after.
Risk: getting the adaptive backoff wrong (too-short → no win; too-long → CPU burn) — measure to tune. This is the
recommended FIRST implementation attempt before the heavier A/B.

**Hybrid (kernel-side loopback short-circuit) — likely INSUFFICIENT, here's why:** the intra-VM unix-socket X
round-trip latency is the **cross-thread WAKE**, not the socket *delivery* (delivery = a fast buffer copy). Waking
Xvfb's blocked poll requires the chain: kernel notify → deferred responder → response-pipe write → Xvfb's worker
`readSync` → `Atomics.notify` → Xvfb main thread. The one reducible hop (the poll-waiter-pool) is **already**
removed by the committed `SECURE_EXEC_POLL_DIRECT` reader-direct change; the remaining hops are **V8-constrained**
(`Atomics.wait` is isolate-internal, so Rust cannot skip the pipe+worker). So a further loopback short-circuit
buys little. **Only co-location removes the wake entirely** (in-process call, no cross-thread hop). Don't expect
the hybrid to reach the multi-client ceiling; it's a measure-and-confirm step, not the fix.

## The 5th component — thunar

Fails even **solo**: a worker thread exits before the window maps. The `GIO_USE_VOLUME_MONITOR=null` warning is
benign (host sets it deliberately to avoid GIO's native volume monitor, which deadlocks on wasm). The real blocker
is the **file-manager async-job model** (glib async + wasm-threads) — the same hard area the M8.5 pcmanfm work left
PARTIAL. Needs the wasm-threads effort.

## Validation bar (for whoever does A or B)

On a quiet box: `XU7_ONLY="xfce4-panel xfdesktop"` (then full 5-client) must show **both/all** components painted
together (panel clock strip + xfdesktop wallpaper + decorated app windows), stable, with input (type/click/switch)
demonstrated. Measure before/after with `SECURE_EXEC_RPCPROF=1` and render nonblack%/component-visibility.
