# Spec: Multi-threaded WASM in the secure-exec runtime (wasi-threads)

Status: **DRAFT v2** (2026-06-21, post adversarial review). Bar: **production runtime feature** (lands in
core crates, full TCB security review, conformance + race test suite, ships in the real sidecar). This is
a **hard prerequisite for M8** (the GTK desktop): see `SPEC.md` M8. M8 cannot start until the Definition
of Done (§10) is green.

The implementation lands in **core runtime crates** — primarily **`crates/v8-runtime`** (isolate +
shared-memory spawning), plus `crates/execution`, `crates/kernel`, `crates/sidecar` — because the GTK
runtime that needs threads runs in the real secure-exec **V8 isolate** path, not in any experiment-local
harness or the opt-in Node path.

> **PROGRESS LOG (do this as we go — REQUIRED).** Keep `~/tmp/gui-progress/progress.html` current as each
> milestone lands: add a dated entry with PROOF — for runtime/build work the passing test output or the
> built-artifact sizes (e.g. `libgio-2.0.a 10.7MB threaded`), and for any GUI/render work a screenshot
> saved under `~/tmp/gui-progress/`. Do not let it go stale; update it in the same change that lands the
> milestone. (Same rule as `SPEC.md`'s progress logging.)

> **v2 changelog (why this differs from the first draft).** A four-lens adversarial review found the v1
> design was built on the wrong execution model. v1 assumed guests run under `node:wasi` with a single
> `SharedArrayBuffer` + `worker_threads` synchronous-RPC bridge, and proposed "multi-channel that
> bridge." **That bridge is opt-in (`AGENT_OS_NODE_SYNC_RPC_ENABLE`, off by default) and is not the
> wasm path.** The real path: guest wasm runs in a **V8 isolate** (`crates/v8-runtime`), one isolate per
> guest on a dedicated OS thread; host imports are **synchronous, in-context** functions that read/write
> the instance's memory directly and call the host inline (`__agentOsSyncRpc.callSync`; `ffi_call`
> reflects on `instance.exports.__indirect_function_table`). So threads are **additional V8 isolates on
> additional OS threads sharing one `WebAssembly.Memory`**, driven from Rust — NOT node Workers. v2 also
> corrects the wasi-threads ABI, the lock granularity, the (false) deadlock argument, and the build/test
> plan. See §11 for the per-finding ledger.

---

## 0. TL;DR

GTK is blocked because GLib's first act is to spawn a worker thread (`pthread_create`), and our runtime
has no thread support: the guest runs as a **single, non-shared-memory `WebAssembly.Instance` in one V8
isolate**, with **synchronous in-context host imports**. To unblock it we implement the **wasi-threads**
proposal end to end:

0. **Spike (M7.5.0, gate-before-build):** prove the core V8 isolate path can spawn a second isolate on
   another OS thread bound to one shared `WebAssembly.Memory` and run `wasi_thread_start`, using a tiny
   hand-written module — *before* recompiling any of GTK.
1. **Phase 0 — build:** retarget wasi-libc to the prebuilt **`wasm32-wasip1-threads`** sysroot (already
   vendored) and rebuild the GTK dependency closure with shared, growable memory; emit
   `(import "wasi" "thread-spawn")` and export `wasi_thread_start`.
2. **Phase 1 — spawn host (Rust):** implement `wasi.thread-spawn` as a host import that spawns a new V8
   isolate on a new OS thread, re-instantiates the same module against the **one** host-created shared
   `WebAssembly.Memory` + shared function table, and runs `wasi_thread_start(tid, start_arg)`.
3. **Phase 2 — concurrency safety:** make concurrent **synchronous host calls** from N isolate-threads
   into shared kernel/sidecar state safe with a **per-VM critical-section lock** that (a) copies syscall
   args out of shared guest memory first, (b) does check+act atomically, and (c) is **dropped while a
   blocking op parks** so threads don't starve or deadlock.
4. **Phase 3 — coherence:** ensure all *stateful* WASI calls are kernel-owned so the thread isolates
   share one coherent view (fd table, cwd, file offsets).
5. **Phase 4 — handoff:** GLib's worker thread starts; hand back to M8 for GDK device setup + render.

The work is bounded by one fact that de-risks the security review: **all threads live inside a single
VM's executor — one trust domain.** Thread↔thread isolation is a non-goal (they already share one linear
memory). The *only* new attack surface is concurrent access to **kernel/sidecar state** (§6).

---

## 0a. Threading primitives: PARK correctly (verified 2026-06-21). GTK hang reclassified to SPEC.md M8.

> **History/correction.** An earlier draft of this section claimed the root bug was "contended locks
> busy-spin instead of parking." **That claim is DISPROVEN** by three minimal reproducers run under the
> in-tree tools (`/proc` thread states + `SECURE_EXEC_TRACE`). The threading primitives park correctly.
> Kept here as the negative result so the wrong theory is not re-investigated.

**What was verified (proof in `~/tmp/gui-progress/proof-parking-evidence.txt`):**

- **`test-threads-contend`** (`guest-xclient/test-threads-contend.c`) — leader holds one `pthread_mutex`
  across a 3 s sleep while two consumer threads contend it. During the hold, **all three
  `session-v8-exec` threads are state `S` / `futex_wait_queue`** (the two consumers are *parked*, not
  spinning at `R`/100%); the run completes with the correct count. Contended `pthread_mutex` **parks**.
- **`threads-atomicwait`** (`guest-xclient/threads-atomicwait.c`) — the **leader** isolate calls
  `memory.atomic.wait32` on a never-notified address (6 s timeout); `/proc` shows it state `S` /
  `futex_wait_queue` and it returns `2` (timeout). **`atomic.wait` parks on the leader isolate**, not
  just on worker isolates — refuting the "atomic.wait is a no-op on the leader" hypothesis.
- **`threads-condwait`** — `pthread_cond_wait` on the leader blocks and is woken by a worker's
  `pthread_cond_signal`. Cond-var wait/signal works across isolates.
- Earlier: spawn, one-shot `pthread_join`, atomics, loopback recv/send split across threads — all pass.

**Conclusion.** `pthread_mutex`, `pthread_cond`, and raw `memory.atomic.wait/notify` all **block-and-yield
(park, state `S`)** on *both* the leader and worker isolates, via real `atomic.wait`/`notify` over shared
linear memory. The §0 step-3(c) / §Phase-2 "blocking op parks" invariant **holds under contention**. The
WASM-THREADS-SPEC threading model is sound; its DoD deadlock/starvation gate **passes** (see §10).

**The original GTK `gtk_init` hang at `XOpenDisplay` was NOT a threading-primitive defect and NOT a
runtime bug at all — it was self-inflicted diagnostic corruption (now fixed); see §0b.** After the fix,
`gtk_init` runs through `XOpenDisplay` + RANDR; a *new, deeper* GLib-worker spin is the live M8 frontier.

## 0b. RESOLVED root cause + current M8 frontier

**Root cause of the original hang (FIXED 2026-06-21):** a previous debugging session left **unbraced
`fprintf` diagnostics in the working-copy (uncommitted) `third_party/libxcb-threads/src/xcb_out.c`** that
destroyed two `while(...) pthread_cond_wait(...)` loops (in `get_socket_back()` and `_xcb_out_flush_to()`):

```c
while(c->out.return_socket && c->out.socket_moving)
    fprintf(stderr,"XCBWAIT...");   /* loop body is now ONLY the fprintf  */
    pthread_cond_wait(...);          /* now runs UNCONDITIONALLY -> single-threaded deadlock */
```

`get_socket_back` runs on the first buffer flush during `XOpenDisplay`, so the client **parked forever**.
The `/proc` "96% CPU state R" was a *lifetime-average* misread (`ps pcpu`); the thread was actually
`S`/`futex_wait_queue` = PARKED. The `--prof` "net_poll spin" was the **X server's** normal accept loop,
not the client. The committed sources were always clean; **the fix = restore the working copy to
committed-clean** (`jj diff` for those files is now 0; also reverted a `/* DIAG */ if(0)` that had
disabled the real BigRequests round-trip). **Lesson: before theorizing a deep runtime bug, `jj diff` the
vendored/gitignored working tree for stray uncommitted diagnostics.**

**Verification:** `guest-xclient/xinitthreads-probe.c` (single-threaded: `XInitThreads` + `XOpenDisplay`
+ `XInternAtom` round-trip + `XNoOp` + `XAllocID` + `XCreateGC` + `XSync`) = **"ALL OK -> no hang"**
(`~/tmp/gui-progress/proof-xinitthreads-clean.log`). Real `gtk-hello` now passes the original blocker:
`gtk_init` → `gdk_display_open_default` → **`XOpenDisplay` COMPLETES** → `precache_atoms` →
`gdk_screen_new` → RANDR `XRRGetScreenResourcesCurrent`/`XRRGetMonitors` round-trips **succeed**.

**Current M8 frontier (genuine, not corruption — verified glib/gmain.c == pristine):** during
`_gdk_x11_screen_new` → `init_multihead`, a **GLib worker thread busy-spins (150% R) on its main
context** while the main thread parks at `xcb_take_socket`'s `pthread_mutex_lock(&c->iolock)` (the worker
holds `iolock`). This is the genuine always-ready-GSource livelock (the originally-suspected issue, now
correctly located *past* `XOpenDisplay`). Remaining work: identify which GSource on the GLib worker
context reports ready / 0-timeout so `g_main_context_iterate` spins without reaching `poll`, and fix its
readiness wiring (candidate: a poll-fd that always reports ready — the same POLLOUT-always-ready class as
`net_poll` line ~11458, or a GLib wakeup-pipe fd). **Also discovered (in-scope runtime bug, not the
spin cause):** the runtime's `clock_gettime(CLOCK_REALTIME)` is frozen at a constant while
`CLOCK_MONOTONIC` advances. This is M8 desktop-integration work; the WASM-THREADS-SPEC threading model
itself is complete (§0a).

---

## 1. Background: the real execution model (corrected) and why threads don't work today

Verified against the wasmgui workspace and `crates/v8-runtime` / `crates/execution`:

- **Guest wasm runs in a V8 isolate**, not under Node. `crates/execution/src/wasm.rs` drives execution
  through `crates/v8-runtime` (`execute_module`, `run_event_loop`, `session.rs` — "isolates on dedicated
  threads"). One isolate per guest, on its own OS thread. (Per `CLAUDE.md`: guest code must run in V8
  isolates; never `Command::new("node")`.)
- **Host imports are synchronous and in-context.** `host_net` / `host_fs` / `host_user` /
  `host_process` and `ffi_call` are plain JS functions in the materialized wasm-runner
  (`node_import_cache.rs` renders it; `wasm_runner_path()`), invoked **inline** on the same thread as the
  guest. They read/write `instanceMemory.buffer` directly and call the host via
  `globalThis.__agentOsSyncRpc.callSync(...)` (synchronous bridge handled in Rust) or, for `ffi_call`, by
  reflecting on `instance.exports.__indirect_function_table`. There is **one instance, one thread, no
  Worker**.
- **The `worker_threads` + `SharedArrayBuffer` sync-RPC bridge is opt-in and OFF by default.**
  `NODE_SYNC_RPC_ENABLE = HOST_PROCESS_ENV.AGENT_OS_NODE_SYNC_RPC_ENABLE === '1'`
  (`node_import_cache.rs:2092`). It exists for running the runner under a real Node host. **The wasm-gui
  path does not use it. Do not build threads on it.** (This is the v1 mistake.)
- **WASI shim:** the runner provides preview1; the *exact* shim differs across checkouts (the experiment
  workspace wires `node:wasi`; core/main carries a custom in-isolate preview1 shim). **The spike (§0)
  must confirm which the core target uses and wire thread-spawn into that one.** Either way, neither
  exposes the wasi-threads `thread-spawn` import.
- **Single, non-shared memory.** The guest exports its own non-shared linear memory. We deliberately
  build non-shared (we dropped wasm-opt `--enable-threads` earlier when fixed-max shared memory OOM'd).
  wasi-threads requires a **shared** memory so a spawned isolate-thread sees the same heap.

So the blocker is **not** a V8 limitation (V8 supports wasm threads + growable `SharedArrayBuffer`-backed
memory + multiple isolates), and **not** "the RPC mechanism" (there is no central RPC mechanism in the
default path — host calls are synchronous and in-context). It is, in order: no `thread-spawn` import
(easy — we own the imports), no machinery to spawn a second isolate-thread bound to a shared memory (the
real gap, in `crates/v8-runtime`), and then concurrent synchronous host calls into shared kernel state
must be made safe (the follow-on).

> **Checkout note.** The wasm-gui experiment lives in an isolated jj workspace
> (`/home/nathan/secure-exec-wasmgui`) that has diverged from `main`'s wasm wiring. Threads is a core
> feature, so it targets `main`'s architecture; the spike (§0) is run against the core target, and the
> experiment is rebased/reconciled onto it before M8 build work.

---

## 2. The ABI we target (the contract) — corrected

We target the **wasi-threads** proposal exactly as emitted by wasi-libc built for the
**`wasm32-wasip1-threads`** target (NOT the older WAMR host-pthread ABI; the two are incompatible — §3).
The vendored wasi-sdk (25.0) **already ships** this sysroot
(`registry/native/c/.../wasi-sysroot/lib/wasm32-wasip1-threads/`), so wasi-libc itself needs **no
recompile** — only retargeting (§4 Phase 0). The contract:

- **Guest imports** `(import "wasi" "thread-spawn" (func (param i32) (result i32)))`.
  - Param: an **opaque i32** the host passes through unchanged to `wasi_thread_start`. wasi-libc points
    it at its own per-thread start args; the host **must not interpret it**.
  - Result: a positive **thread id** on success, or a **negative value** on failure. The specific
    negative is not a propagated errno; wasi-libc surfaces `EAGAIN` to `pthread_create`.
- **Guest exports** `(export "wasi_thread_start" (func (param $tid i32) (param $start_arg i32)))`. The
  host runs this on a fresh instance bound to the shared memory. **Inside `wasi_thread_start`**, wasi-libc
  sets the new thread's stack-pointer global and initializes TLS from `start_arg`. The host does **not**
  allocate the guest stack or set the stack pointer; wasi-libc allocated the stack/TLS *region* from the
  shared heap before the spawn call, but activation happens inside `wasi_thread_start`.
- **Imported memory (shared).** A `-pthread` build **imports** memory: `(import "env" "memory" (memory
  (shared) initial maximum))`. The host creates one `WebAssembly.Memory({ shared: true, initial,
  maximum })` and supplies it to **every** instance (spawner + all threads) under `env.memory`.
- **Imported, shared function table.** A shared-memory module **imports** its
  `__indirect_function_table` (`env.__indirect_function_table`) so all thread instances share **one**
  table — function pointers are coherent because there is one table object, not because indices
  "happen to match." (v1 wrongly said the table is per-instance; passive segments are not auto-applied per
  instance, so a fresh instance would otherwise have an empty table.)
- **Imported mutable globals.** The threaded module imports/needs mutable globals (`__stack_pointer`,
  TLS base); the host import object must satisfy them. `-pthread` implies `-matomics -mbulk-memory` at
  compile time; the linker needs `--shared-memory --import-memory --max-memory` and
  `--export=wasi_thread_start`.

Anti-requirement: we do **not** implement Emscripten's pthread model (main-thread proxying). Emscripten is
a mechanics reference only (§3).

---

## 3. Prior art (what we take from each) — re-weighted for the V8-isolate model

- **`wasmtime-wasi-threads`** (crates.io) — **now the closest structural analogue.** It is a *host*
  (Rust) spawning OS threads, each re-instantiating the module against one shared memory — exactly our
  model (V8 isolates from Rust sharing a `WebAssembly.Memory`), minus the engine. Take from it: tid
  allocation, the `wasi_thread_start` invocation contract, instance-per-thread sharing memory+table, and
  its explicit **"not ready for multi-tenant embeddings" warning** — which we mitigate via single-VM /
  single-trust-domain + the per-VM lock (§6) and cite as the thing we are deliberately addressing.
- **`@emnapi/wasi-threads`** (npm, MIT) — **demoted to semantic reference.** It targets Node
  `worker_threads` + browser Workers, which is the opt-in path we are *not* using for the V8-isolate
  product runtime. Useful only for the JS-side shape (tid bookkeeping, `wasi_thread_start` calling) if/when
  a Node-host or browser variant is built (§11 Q5); not the structural template for core.
- **Emscripten pthreads docs** — **mechanics only.** Growable `SharedArrayBuffer` semantics, the
  buffer-detach-on-grow hazard, atomics gotchas. Pthread *model* is not our ABI.
- **WAMR wasi-threads** — **ABI-fork confirmation.** Old host-pthread vs new wasi-threads are
  incompatible; confirms we target wasi-threads (what `wasm32-wasip1-threads` wasi-libc emits).
- **wasi-threads proposal** — the normative ABI (§2).

---

## 4. Architecture

### Phase 0 spike → M7.5.0 (gate before any GTK rebuild)

**Do this first.** A hand-written `.wat` (or ~20-line `-pthread` C) that: imports a shared memory +
`(import "wasi" "thread-spawn")`, exports `wasi_thread_start`, and has the spawned thread flip a flag in
shared memory that the main thread then reads. Run it through the **real sidecar / core V8 isolate path**.

- **Negative gate:** assert it fails *today* (no thread-spawn) — proving the test bites.
- **Positive gate:** after Phase 1's minimal isolate-spawn primitive, the flag flips and both threads
  join.

This resolves the single biggest unknown — *can the core V8 embedding spawn a second isolate-thread
bound to one shared growable memory and run `wasi_thread_start`* — **before** the multi-week GTK rebuild.
Mirrors the repo's own libffi-spike-before-GTK precedent. It also settles the WASI-shim question (§1).

### Phase 0 — Build: retarget to the threaded sysroot + rebuild the closure shared

- **Retarget, don't recompile wasi-libc.** Use the vendored `wasm32-wasip1-threads` sysroot. Add a
  threads build profile to `toolchain/cross-env.sh`: `-pthread` (implies `-matomics -mbulk-memory`),
  `-Wl,--shared-memory -Wl,--import-memory -Wl,--max-memory=$N -Wl,--export=wasi_thread_start`.
- **Fork the toolchain stubs that fight threads** (concrete link-time breakage, not hand-waving):
  - `toolchain/wasi-compat.c` **defines its own `pthread_create` (returns 1), `pthread_join`,
    `pthread_attr_*`, `pthread_setname_np`, `pthread_sigmask`, and `flockfile`/`funlockfile`/
    `ftrylockfile`.** These **duplicate-symbol** against `libpthread.a` and the fake `pthread_create`
    is the exact no-op-thread→deadlock failure the file's own comment rejects. Produce a **separate
    threaded compat object** (or `#ifdef __wasi_threads__` guards) that drops all of these.
  - The clang wrappers (`clang-wasi-wrap.sh`, `clangxx-wasi-wrap.sh`) and `build-gtk-app.sh` **strip
    `-pthread`/`-lpthread`** (`sed 's/-pthread//g'`). The threaded profile needs the opposite —
    threads-aware wrapper/sed variants that preserve `-pthread`.
- **Rebuild the whole prefix closure shared.** Mixed shared/non-shared objects do not link. Every
  already-built `.a` in `third_party/wasm-prefix/lib` (PCRE2, `libffi-wasm`, intl-stub, fribidi,
  harfbuzz, cairo, pango, gdk-pixbuf, atk, GLib stack, GTK, the X libs) must be rebuilt with the threaded
  profile. CI builds both trees (non-threaded for guests that don't need threads; threaded for the GTK
  closure). Document the full list in the build scripts.
- **Memory.** Host creates `WebAssembly.Memory({ shared:true, initial, maximum })`. The existing runtime
  memory-cap clamp (wasm-runner clamps a module's declared max to a runtime limit; the GTK build note
  says "runtime caps wasm memory max at 128 MiB") must move to apply to the **host-created** max and be
  raised. Start `--max-memory` at **512 MiB**, tune. The raised cap is a **per-VM resource limit on the
  BARE wire** (`ResourceLimits`), never an env knob (dead-cap rule).
- **wasm-opt.** The current pass set uses `--fpcast-emu` (function-pointer cast emulation — GTK casts fn
  pointers across signatures) and explicitly **not** `--enable-threads`. Re-validate every pass under
  shared+imported memory: confirm `--fpcast-emu`'s side table is coherent with the shared imported table,
  and that memory growth survives. Drop any pass that fixes/forbids growth.

### Phase 1 — `wasi.thread-spawn` host (spawn a V8 isolate-thread), in Rust

In `crates/v8-runtime` (+ wiring through `crates/execution`):

- The host creates the shared `WebAssembly.Memory` and shared `WebAssembly.Table` once per VM and
  instantiates the main module against them (`env.memory`, `env.__indirect_function_table`, mutable
  globals). **Gate on the module importing a *shared* memory** (the `shared` bit / presence of the
  `wasi.thread-spawn` import) — not merely "imports memory." Non-threaded guests keep the exported-memory
  path unchanged.
- Register the `wasi: { 'thread-spawn'(start_arg) }` import. It:
  1. Allocates a fresh **tid** (monotonic, never reused within a VM; tid for main reserved).
  2. **Inside the per-VM lock**, checks the live-thread count against `ResourceLimits.max_threads`
     (check-and-increment atomically — else two threads at limit−1 both pass), rejecting with a negative
     return at the cap.
  3. Spawns a **new V8 isolate on a new OS thread**, re-instantiating the same compiled module against
     the **same** shared memory + table + a fresh set of synchronous host imports bound to this thread.
  4. **Happens-before:** the thread's host-import wiring is live before the isolate executes any guest
     code that can call a host import.
  5. Calls `instance.exports.wasi_thread_start(tid, start_arg)` on that isolate's thread. Returns `tid`
     synchronously to the spawner (the new thread runs concurrently). On spawn failure, return a negative
     value and decrement the count.
- **Memory-grow hazard.** After `memory.grow` on shared memory the backing `ArrayBuffer` is replaced;
  **every** host-side accessor (in every isolate-thread) that caches a `Uint8Array`/`DataView` over
  `memory.buffer` must re-derive it per call (or on a grow signal). Audit the existing cached-view sites
  (`ffi_call`, the `host_*` import bodies).
- **Lifecycle / teardown.**
  - Thread exit: the joinee's wasi-libc exit-notify (in-wasm `memory.atomic.notify` on the join futex)
    must complete **before** the host tears down / reuses the isolate; tid + thread-count budget release
    happens at `wasi_thread_start` return regardless of join state (so detached threads are reclaimed).
  - **No instance reuse across logical threads** unless a full per-thread reset is specified (a pooled
    isolate carries prior WASI/errno state). Default: fresh isolate per thread; pool only with a proven
    reset.
  - **Trap → VM fault.** Any thread trap corrupts shared memory → fault the whole VM. Mechanism: the
    isolate-thread's error/exit surfaces to the host, which runs the VM-fault teardown: stop all
    thread-isolates, then free shared memory last (so no thread observes freed memory).
  - **`proc_exit` is VM-global** (NOT a per-instance pass-through): any thread's `proc_exit` tears down
    all sibling thread-isolates and the VM.

### Phase 2 — Concurrency safety for synchronous host calls (the real "RPC fix")

> **MAJOR FINDING (2026-06-21, from mapping the execution path) — the existing architecture already
> provides most of Phase 2.** Kernel-touching host calls do NOT mutate kernel state in-isolate; each one
> marshals its args to **CBOR (a copy)** and round-trips via `RuntimeEvent::BridgeCall`
> (`host_call.rs:sync_call`) to the **single-threaded sidecar event loop**
> (`service.rs:handle_javascript_sync_rpc_request`, a `new_current_thread` runtime). Multiple isolates of
> one VM **already coexist** today (parent + `child_process.spawn` children), and their host calls are
> **serialized at that one loop**, which accesses `vm.kernel` via `&mut` on a single thread. Consequences
> for a worker isolate (which routes BridgeCalls to the same loop):
> - **Races: already prevented.** One event is processed to completion before the next → kernel access is
>   single-threaded by construction. No new lock needed for memory-safety.
> - **Check-then-act atomicity: already provided.** A whole host-call handler runs uninterrupted on the
>   sidecar loop → permission/resource check + action + commit are atomic w.r.t. other host calls.
> - **TOCTOU-on-args: already prevented.** Args are CBOR-copied before crossing to the sidecar; a sibling
>   thread mutating shared guest memory can't change the already-copied request.
> - **`pthread_join`/futex: no sidecar round-trip.** wasi-libc uses in-wasm `memory.atomic.wait/notify`
>   on shared memory, so the joiner parks in its *isolate*, not in a host call → no sidecar deadlock edge.
> So the "per-VM critical-section lock" below is **largely subsumed by the existing single-threaded
> sidecar**. The remaining real Phase-2 work is narrower: (i) **fairness** for *blocking* worker host
> calls (the M6.4 class — a worker's blocking `poll` must not starve the main thread; bounded poll
> quantum already exists), and (ii) an **audit** to confirm no kernel-touching path bypasses the sidecar
> loop (e.g. anything handled directly in v8-runtime). The original lock-design notes below are retained
> as the fallback / audit checklist if any unsynchronized path is found.

Because host imports are synchronous and in-context per isolate-thread, the *potential* hazard is
**N isolate-threads making concurrent synchronous host calls into shared kernel/sidecar state** — but per
the finding above, the existing single-threaded sidecar loop is that critical section. The notes below
define what to verify / how to harden if a bypass exists.

> **CONCRETE DESIGN — worker→kernel host-call routing (the GTK-enabling block, scoped 2026-06-21).**
> Today worker isolates run with **stub** wasi imports (sound for compute threads — the spike/multi/
> nested guests make no host calls). GLib's worker does real I/O, so worker host calls must reach the
> **same VM's kernel, operating on the shared process fd table** (threads share fds — the X socket fd is
> opened by the main thread and used by workers). Verified constraints from the code:
> - Kernel: per-process `ProcessFdTable` (`crates/kernel/src/fd_table.rs`), fds are `SharedFileDescription`
>   (Arc). A thread must share its parent's `ProcessFdTable` (not a cloned one).
> - Sidecar: host calls dispatch by `(vm_id, process_id)` against `vm.active_processes`
>   (`service.rs:1780+`); responses route by `session_id` (via the call_id→session_id router + a
>   per-session response channel). One session per process today.
>
> Required (a real threading-model feature, TCB-touching → needs the §10 human design/review):
> 1. **Kernel thread = a process sharing the parent's fd table.** Add a `spawn_thread`/`SpawnOptions{
>    share_fd_table: parent_pid}` that `Arc`-shares the parent's `ProcessFdTable` instead of cloning, and
>    a thread id within the process. (Or a first-class thread abstraction under the process.)
> 2. **Worker = a registered execution/session in the same VM**, created via the sidecar (the
>    `spawn_javascript_child_process` template, `execution.rs:5174`) in **thread mode**, mapped to the
>    shared-fd-table kernel thread, with its **own** response channel (so per-session response routing
>    works) but the **same** process fd table.
> 3. **Thread-mode runner**: take the registered `ThreadStart` by token (the `ThreadSpawnRegistry` is
>    built for this), `deserialize_shared_memory` + `from_compiled_module`, instantiate with the **full**
>    host imports (wired to the worker session's bridge), call `wasi_thread_start(tid, start_arg)`.
> 4. Spawn flow: parent `wasi.thread-spawn` registers the `ThreadStart` → token, then `callSync` a new
>    sidecar method `wasm.thread_spawn{token}` which creates the worker session+kernel-thread.
> The existing single-threaded sidecar loop already serializes the resulting concurrent kernel access
> (the Phase-2 finding above), so no new lock is needed — only the threading-model plumbing + fairness.
> This is the next implementation block and the gate for GTK threads.

- **Per-VM critical-section lock.** The unit of mutual exclusion is the **entire host-call servicing
  routine**, not "one kernel method." Concretely, each host call:
  1. **Copies its arguments out of shared guest memory into host memory first** (path strings, addrs,
     lengths). A sibling thread can mutate pinned syscall args between read and use; operate only on the
     copy. (TOCTOU-on-args defense.)
  2. Acquires the per-VM lock.
  3. Performs **permission/egress/resource check + the action + the kernel-state commit as one
     uninterrupted critical section** — so no sibling thread runs between a check and its action. This
     covers multi-step host sequences (e.g. TCP connect = policy check → DNS resolve → connect; loopback
     = create → bind → connect) that are *not* single kernel methods.
  4. Releases the lock.
- **Blocking ops MUST drop the lock while parked.** `poll`, `accept`, blocking `read`/`recv` on a
  socket/pipe/PTY, `pthread_join`, and futex-style waits must **not** hold the per-VM lock while waiting
  — they acquire only to mutate state, then release and park, then re-acquire to commit. Holding the lock
  across a blocking wait recreates the **exact intra-VM starvation** this project already fought
  (`JAVASCRIPT_NET_POLL_MAX_WAIT` was lowered 50ms→3ms in M6.4 precisely because one guest's blocking
  poll starved others). Keep the bounded-poll quantum per thread.
- **Deadlock model (corrected — the v1 "no guest→guest edge" claim was false).** Threads of one VM **do**
  depend on each other through kernel objects: pipe/socketpair backpressure (A's blocking write drains
  only when B reads), loopback recv/send between two threads, PTY master/slave across threads, and
  `pthread_join`/futex. The deadlock-freedom requirement is therefore: **no host call may block while
  holding any resource another thread needs to make the unblocking progress.** That is exactly why
  blocking ops drop the lock (above). Blocking IPC reads/writes/joins must be implemented as
  **retryable / lock-free-while-parked** so the unblocking thread can always run.
- **Enumerate ALL per-VM TCB state under the lock, not just kernel tables.** Beyond the kernel (fd /
  socket / process / pipe / pty tables, VFS), the sidecar holds security-relevant mutable state the
  servicer touches: host-mount link state (`bridge.rs` `HostFilesystemLinkState`), the loopback-TLS
  transport registry (keyed by vm/socket ids), per-listener backlog `active_connection_ids`, and HTTP/2
  session maps. Each must be **either** under the per-VM lock **or** on a documented bypass allow-list
  with a written argument that it is internally check-and-act-atomic. Define a global **lock-acquisition
  order** (per-VM lock outermost; existing fine-grained mutexes always under it, fixed order) to avoid
  introducing a lock-ordering deadlock.
- **Resource snapshot+check+act is one critical section.** Resource gates read a *cross-subsystem*
  snapshot (processes+sockets+pipes+ptys+inodes) then check then mutate. This is atomic only if held in
  one lock span. **Rule out** the per-subsystem-lock optimization (Open Q §11.4) unless each gate's
  snapshot is first narrowed to a single subsystem.
- **Lock poisoning / partial state on trap.** If a servicing closure panics or a thread vanishes
  mid-call, define behavior: the VM is faulting anyway, so a poisoned per-VM lock that fails all
  subsequent calls is acceptable **by design** (not by accident); in-flight kernel mutations must be
  committed-or-rolled-back deterministically (document which), and thread-count / fd / socket accounting
  reclaimed.

### Phase 3 — WASI state coherence

Each thread = its own isolate = its own WASI shim instance. Any WASI call still backed by the shim's
*internal* state becomes per-thread-inconsistent. We already kernel-back most fd ops; audit the
remaining pass-throughs (`fd_readdir`, `path_filestat_get`, `path_link`/`symlink`/`rename` on preopen
fds, `fd_advise`, file offsets) and move any with cross-thread-visible state to **kernel-owned**, so the
kernel is the single source of truth. Reclassify `proc_exit` and thread-exit as **kernel-coordinated /
VM-global**, not "stateless per-instance pass-through" (v1 error). Deliverable: a documented split of
"kernel-owned stateful WASI" vs "safe stateless pass-throughs" (`args_*`, `environ_*`, `random_get`,
`clock_*`, `sched_yield`).

### Phase 4 — Handoff to M8

With threads working, `g_system_thread_new` → `pthread_create` succeeds and GLib's worker runs. Control
returns to `SPEC.md` M8: GDK device/seat setup, CPU-budget tuning under multi-thread load, the libffi
closure-pool thread-safety (§6 R5), then LXDE bring-up. **Threads is done when §10 is green; M8 resumes.**

---

## 5. Trust model (unchanged boundary, new internal surface)

Per the repo trust model the boundary is **sidecar ↔ executor**; the executor (all guest threads) is
untrusted. Threads do not move this boundary:

- All threads of a VM are the **same untrusted executor**, sharing one linear memory. They can already
  corrupt each other arbitrarily — in-scope-by-design (one trust domain), **not** a new vulnerability. No
  guest↔guest isolation requirement.
- The **only** new attack surface is **concurrent guest-driven access to sidecar/kernel state**. A TCB
  race a multi-threaded guest can drive into memory-unsafety or policy-bypass **is** in scope.
- Mitigation is structural: the **per-VM critical-section lock** (§Phase 2). The review must confirm it
  covers every state-mutating host call **and all enumerated non-kernel TCB state**, that arg copies
  happen before checks, that permission/limit checks are inside the lock (no cross-thread TOCTOU), and
  that blocking ops drop the lock.
- **Resource limits** are the second new vector: thread count is unbounded host OS-thread/isolate/CPU
  consumption (spawn-bomb). Add `max_threads` to `ResourceLimits` (BARE wire), enforce check-and-increment
  inside the lock before spawn. Memory bounded by `--max-memory` + the wire cap. Note the spec enforces
  "guest cannot **exceed** the applied cap" (in-scope); it does **not** add validation of the trusted
  configured cap (out-of-scope per the model).

Explicitly out of scope: multi-client / VM-to-VM (single-client transport), host egress.

---

## 6. Risks

- **R1 — Build blast radius.** Whole prefix closure rebuilt shared; `wasi-compat.c` pthread/flockfile
  stubs and the `-pthread`-stripping wrappers actively conflict (§Phase 0). Mitigation: separate threaded
  compat object + wrappers; CI builds both trees; enumerate the closure.
- **R2 — Growable shared memory + wasm-opt `--fpcast-emu`.** Prior OOM + fpcast side-table coherence.
  Mitigation: import-memory growable from host; re-validate each pass; memgrow conformance test on a
  `--fpcast-emu`-processed binary gates it.
- **R3 — WASI shim under shared memory / per-isolate state.** A per-thread shim instance may duplicate
  state or fight shared memory. Mitigation: Phase 3 kernel-owns stateful WASI; the §0 spike confirms the
  core shim before committing. If the shim fights shared memory, replace it on the threaded path (flag).
- **R4 — Kernel concurrency / missed unlocked path.** Mitigated by the per-VM lock + the full TCB-state
  enumeration; residual risk caught by the lock-disabled negative test (§9).
- **R5 — libffi closure pool in shared memory.** The M8 libffi shim uses a trampoline pool in guest
  memory; once shared, pool allocation must be thread-safe (atomic bump or guest lock), AND the host must
  never derive a *host-side* capability (fd/socket/callback) from a guest-pool-controlled index without
  re-validating against kernel-owned tables under the lock. The shim is currently built non-shared (R1).
  **Circular-gate caveat:** see §9 — this work moves into Phase 0, not the M8 gate.
- **R6 — Deadlock/starvation.** Mitigated by lock-drop-on-block + the corrected deadlock model;
  validated by inter-thread-dependency deadlock tests + a fairness assertion (§9).
- **R7 — Isolate/thread overhead.** GLib thread pools create many threads; one OS thread + isolate each
  is heavy. Mitigation: bounded by `max_threads`; measure under the GLib smoke test; consider an isolate
  pool with a proven reset (Phase 1) if needed.

---

## 7. RPC-channel-corruption note

In the V8-isolate default path there is no per-thread SAB control channel to forge (host calls are
in-context synchronous). The corresponding invariant to preserve: **the synchronous bridge dispatch and
its argument buffers are host-owned and not addressable from guest linear memory by pointer.** The new
shared-memory hazard is that syscall *arguments* live in shared guest memory and a sibling thread can
mutate them mid-call — handled by the copy-args-first rule (§Phase 2). If a Node/SAB variant is ever
built (§11 Q5), per-thread control/data SABs must be host-owned, exclusively owned by one thread +
servicer, never shared/reused, with release-store/acquire-load on the state word and notify-per-channel.

---

## 8. (reserved)

---

## 9. Test plan (production bar — fully automated, real sidecar)

All tests run headless against the **real secure-exec sidecar / V8 isolate path** (no wasmer, no
`Command::new("node")` for guest execution), per `SPEC.md` §1a. Every test bites (a negative case that
must fail), per the M0 golden-pixel precedent.

0. **Spike (`test-threads-spike.sh`)** — §0: the hand-written module; negative (fails today) + positive
   (flag flips, threads join) after Phase 1. Gates all of Phase 0.
1. **ABI conformance (`test-threads-abi.sh`)** — `-pthread` C guest: spawn 1 thread + join (shared flag
   flips); spawn N → N distinct positive tids, all join; exceed `max_threads` → negative return →
   `pthread_create` fails. Negative: assert the join-flag is *unset* if the thread didn't run.
2. **Shared-memory growth (`test-threads-memgrow.sh`)** — allocate past `initial` to force `memory.grow`
   across threads; all threads observe the grown heap. Run on a **`--fpcast-emu`-processed** binary
   (R2). Negative: a thread caching a pre-grow view reads stale/detached → must fail (proves the
   re-derive rule is needed).
3. **Concurrent kernel I/O (`test-threads-kernel-io.sh`)** — multiple threads doing concurrent VFS +
   socket ops through the per-VM lock; correctness + **no panic/UB** over many iterations. **The keystone
   negative test: with the per-VM lock DISABLED, this stress MUST fail** — proving the lock is what
   bites, not luck.
4. **Race/stress flake gate (`test-threads-stress.sh`)** — the above run **≥200 iterations**, **0
   failures**. Nondeterminism must be *real*, not "vary by index" (deterministic theater, since
   `Math.random`/`Date.now` are unavailable): inject **host-side jitter** (variable delay before the
   lock releases) and seed a PRNG from `clock_time_get`. **Mandatory under the sidecar's
   sanitizer/debug build where supported.** Bar set above 50 because this project's races (M2.3, M6.3,
   M6.4) were timing-sensitive and survived small iteration counts.
5. **Deadlock/liveness (`test-threads-deadlock.sh`)** — adversarial **inter-thread dependency**
   topologies (a bare independent-work loop does not bite): (a) full-pipe producer/consumer split across
   two threads; (b) loopback recv/send split across two threads; (c) `pthread_join` of a thread parked in
   a blocking host call; (d) a barrier where N threads block and the (N+1)th releases them; (e)
   **fairness assertion** — while thread A is in a long blocking poll, thread B's unrelated `write`
   completes within a tight bound (the M6.4 round-robin invariant). Bounded wall-clock; no hang.
6. **Teardown/trap (`test-threads-teardown.sh`)** — N threads parked in blocking host calls while one
   calls `proc_exit` → bounded clean shutdown, no orphaned isolate/thread; and a thread that deliberately
   traps → all parked peers wake and the VM faults within a bound (not a hang); accounting reclaimed.
7. **GLib smoke (`test-threads-glib.sh`)** — the threaded GLib build initializes its worker thread and a
   `GThreadPool` runs a job to completion — the direct M8 unblock proof.
8. **Wire-parity (`test-threads-wire-parity.sh`)** — `max_threads` (and the raised memory cap) reachable
   from **both** the Rust and TS client, wire-carried, enforced from the wire value (not an env fallback)
   — per the dead-cap rule. (Repo precedent: the prior limits/identity wire-migration tests.)
9. **Regression** — a named aggregate: the `test-m*.sh` milestone suite + `cargo test -p
   secure-exec-sidecar -p secure-exec-execution` (+ v8-runtime), green with the threaded runtime
   **present but unused** by non-threaded guests. Test **both** branches: a non-threaded guest still
   takes the exported-memory path; a threaded guest takes the imported-shared-memory path.

`tests/RESULTS.txt` records the run; proof artifacts land in `~/tmp/gui-progress/`.

**libffi-under-threads — resolving the circular gate.** A naive "DoD requires `test-threads-ffi` green,
but the fix lives in M8" is circular. Resolution: the libffi-shim **shared rebuild + atomic trampoline
pool** (R5) moves into **Phase 0** (it's part of the closure rebuild anyway), and `test-threads-ffi.sh`
(concurrent `ffi_call` + closures from multiple threads, pool stays consistent) is a **threads DoD test**.
Anything genuinely downstream (GTK render correctness) stays an **M8-entry** task, not an M8 gate.

---

## 10. Definition of Done (the M8 gate)

Threads is **DONE** — and M8 may start — only when **all** hold:

- [ ] Spike: a hand-written threaded module spawns + joins on the real core V8 path (test-threads-spike).
- [ ] A `-pthread` wasi-libc guest spawns and joins threads (test-threads-abi).
- [ ] Shared, growable memory works across threads on a `--fpcast-emu` binary (test-threads-memgrow).
- [ ] Concurrent kernel I/O is correct and crash-free under the per-VM lock, **and fails with the lock
      disabled** (test-threads-kernel-io).
- [ ] Race/stress gate passes **0/≥200** with real nondeterminism, under sanitizer where supported
      (test-threads-stress).
- [x] **No deadlock/starvation under inter-thread dependencies** — contended `pthread_mutex`/
      `pthread_cond` **block-and-yield** (park in `memory.atomic.wait`, state `S`/`futex_wait_queue`), not
      busy-spin. **✅ VERIFIED — see §0a.** `test-threads-contend` (2 consumers park on a held mutex,
      all `S`, correct count), `threads-atomicwait` (leader isolate parks in `atomic.wait`), and
      `threads-condwait` (cond wait/signal across isolates) all pass. Proof:
      `~/tmp/gui-progress/proof-parking-evidence.txt`. (The GTK `gtk_init` hang is an app-level GLib
      GSource livelock, reclassified to SPEC.md M8 §0b — not a primitive defect.)
- [ ] Clean teardown + trap→VM-fault with parked threads, no leaks (test-threads-teardown).
- [ ] libffi call+closure thread-safe (test-threads-ffi; R5 work done in Phase 0).
- [ ] A threaded **GLib** build runs its worker thread + a thread-pool job (test-threads-glib).
- [ ] Wire-parity: `max_threads`/memory cap on the BARE wire, both clients, enforced from the wire
      (test-threads-wire-parity).
- [ ] Full regression green, both memory branches exercised (regression aggregate).
- [ ] **TCB security review signed off** by a human: per-VM lock covers all state-mutating host calls
      **and all enumerated non-kernel TCB state**; args copied before checks; permission/resource checks
      inside the lock (no cross-thread TOCTOU); blocking ops drop the lock; lock-ordering defined;
      `max_threads` enforced; thread-trap → VM fault; `proc_exit` VM-global.

Until every box is checked, `SPEC.md` M8 stays blocked.

---

## 11b. DoD scorecard (live)

Against §10's gates (✅ done / 🟡 partial / ⬜ not started / 👤 human-gated):

- ✅ Spike: threaded module spawns + joins on the real V8 path (`test-threads-spike.sh`, 5/5).
- ✅ `-pthread` guest spawns/joins; multi-thread atomics correct (`test-threads-multi.sh`, 8 threads ×
  2000 atomic incs = 16000, 5/5); nested spawn (`threads-nested`, worker spawns workers, 3/3).
- ✅ Shared, growable memory across threads (imported shared `env.memory`; proven in spikes).
- ✅ **Concurrent kernel I/O — worker→kernel host calls WORK** (the GTK-enabling block, DONE
  2026-06-21). Worker threads are real sidecar wasm sessions sharing the parent's kernel process
  (shared fd table); their host calls route to the kernel, serialized by the single-threaded sidecar
  loop. `threads-io` (a worker does `write()` to stdout) PASSES; spike/multi/nested also pass via this
  path; worker exit no longer kills the leader (`ActiveProcess.is_thread` guard). Lifecycle: worker =
  top-level active_process (autonomously pumped) sharing `kernel_pid` + handle clone.
- ✅ Stress + lifecycle (`test-threads-stress.sh`, 200 spawn/join cycles/run, exact count, also proves
  slot reclamation since 200 > cap). ✅ teardown/trap → VM fault (`test-threads-trap.sh`: a worker
  `__builtin_trap` faults the VM in ~2s via `fault_thread_group`, not a hang). ⬜ ≥200-iteration
  *cross-run* flake gate under sanitizer; ⬜ libffi-under-threads (needs the shared-built libffi shim,
  part of Phase 0).
- ✅ `max_threads` on the **BARE wire**: `limits.resources.maxThreads` (vm-config macro → TS-exported,
  both clients) → kernel `ResourceLimits.max_threads` (default 64) → enforced per-VM in
  `spawn_wasm_thread`; process-global `ThreadSlots` backstop too.
- ✅ Full regression aggregate + cross-run flake gate (`test-threads-all.sh`: spike/multi/io/stress/
  trap/nested each ×N, 0 failures). ⬜ a **sanitizer** build of the gate (infra follow-up).
- 🟡 Phase 0 threaded GTK closure rebuild (in progress): threaded toolchain DONE + proven; **the ENTIRE
  GLib stack (libglib/gobject/gthread/gmodule/gio) now builds for `wasm32-wasip1-threads`** — Phase 0's
  hardest dependency. Remaining: harfbuzz/cairo/pango/gdk-pixbuf/atk/gtk + X libs threaded (same
  toolchain + compat-header pattern).
- ✅ Threaded **GLib** smoke (`test-glib-threads.sh`): GLib's `g_thread_new` (×4) + `GThreadPool` (6
  jobs) run on the wasm-threads runtime through the real sidecar, 3/3 — the direct proof GLib's worker
  threads (the M8 blocker) work. ⬜ libffi-under-threads (next).
- 👤 **TCB security sign-off** — a human gate by design (threads touch the sandbox boundary); cannot be
  satisfied autonomously.

**Honest completion note (RE-CORRECTED 2026-06-21).** This note has swung twice; here is the
evidence-backed state. (1) An early draft over-claimed the threading risk was "retired / runs reliably".
(2) A second draft over-corrected, asserting a "root bug" that contended locks busy-spin. **Both were
wrong.** The verified truth (§0a, proof in `~/tmp/gui-progress/`): `pthread_mutex`/`pthread_cond`/raw
`memory.atomic.wait` **park correctly (state `S`) under sustained contention on both leader and worker
isolates** — `test-threads-contend`, `threads-atomicwait`, `threads-condwait` all pass. The "blocking op
parks" invariant **holds**. What is *not* yet done: the full conformance/stress/teardown suite (§10
remaining boxes), and the **human TCB security sign-off** (a gate by design). The GTK `gtk_init` hang is
a real, separate problem but it is an **application-level GLib GSource livelock** (§0b), tracked under
SPEC.md M8 — it does **not** indicate a threading-primitive defect and does not block the threading
model itself.

## 11a. Implementation status (M7.5.0 spike — in progress)

- **DONE — threaded build profile.** `scripts/build-threads-spike.sh` produces a correct wasi-threads
  ABI: imported + shared + growable + re-exported `env.memory`, `wasi.thread-spawn` import,
  `wasi_thread_start`/`_start` exports. Confirmed ground truth: **only memory is shared; the function
  table is per-instance** (rebuilt from elem segments) — so no cross-isolate table sharing is needed
  (this corrects the v2 §2 "table imported/shared" claim).
- **DONE — negative gate.** The threaded guest now **instantiates and runs `_start` in the real
  sidecar V8 isolate** with a host-created shared `WebAssembly.Memory` (env.memory) and a reachable
  `wasi.thread-spawn`; `pthread_create` returns EAGAIN (spawn stub). Runner: `parseWasmThreadInfo` +
  shared-memory creation + `createWasiThreadsImport` in `node_import_cache.rs` (asset v76). Validation:
  `wasm.rs` now allows a shared imported memory (still rejects plain imported memory). Test:
  `scripts/test-threads-spike.sh` (SPIKE NEGATIVE-GATE). All 30 `wasm::` unit tests still pass.
- **FEASIBILITY CONFIRMED — cross-isolate shared memory (the spike's central unknown).** rusty_v8
  130.0.7 exposes everything needed: `WasmMemoryObject::buffer()` → `(Shared)ArrayBuffer::get_backing_store`
  (a `Send` `SharedRef<BackingStore>`), `SharedArrayBuffer::with_backing_store` to rebuild it in the
  second isolate, `WasmModuleObject::{get_compiled_module, from_compiled_module}` (`CompiledWasmModule`
  is `Send`) to share compiled code, and `ValueSerializer`/`ValueDeserializer` with SAB + wasm transfer
  delegates — the structured-clone path that reconstructs a shared `WebAssembly.Memory` in isolate B
  (the same mechanism node `worker_threads` uses). So Phase 1 is buildable in `crates/v8-runtime`.
- **DONE — all cross-isolate primitives proven in code.** Two passing rusty_v8 tests:
  `threads_shared_memory_spike.rs` (a backing store is genuinely shared across two isolates on two OS
  threads) and `threads_memory_serialize_spike.rs` (a shared `WebAssembly.Memory` round-trips via
  `ValueSerializer`, so the worker isolate gets a real `Memory` to instantiate `env.memory` with).
- **DONE — coordinator module.** `crates/v8-runtime/src/wasm_threads.rs`: `SendBackingStore` /
  `SendCompiledModule` (sound cross-thread move wrappers), `ThreadSpawnRegistry` (process-global token
  table handing a `ThreadStart` from spawner thread to worker thread), `ThreadIdAllocator`, and
  `serialize_shared_memory` / `deserialize_shared_memory` (the reusable round-trip). Unit-tested.
- **DONE — SPIKE PASS (Phase 1 minimal real thread-spawn).** A multi-threaded wasm guest
  (`threads-test.wasm`) runs end to end in the real sidecar: `pthread_create` spawns a V8 isolate on a
  new OS thread sharing the guest's memory, the worker runs `wasi_thread_start`, writes shared memory,
  and `pthread_join` (in-wasm `atomic.wait`/`notify`) completes → `M8-THREADS: PASS`, **5/5 reliable**.
  Wiring: native `__agentOsWasmThreadSpawn` (`wasm_threads.rs`) captures the shared module+memory,
  allocates a tid, spawns a worker OS thread (`run_worker` reconstructs both and calls
  `wasi_thread_start`); `session.rs` registers it per-session; `wasi.thread-spawn` (runner, asset v77)
  calls it. No regressions (3 spikes + 2 unit + 30 `wasm::` tests green). Test: `test-threads-spike.sh`
  → SPIKE PASS.
- **NEXT — remaining M7.5 toward the full DoD (§10):**
  - **Phase 0** build: a threaded-profile GTK closure (retarget `wasm32-wasip1-threads`, fork
    `wasi-compat.c`/wrappers, rebuild prefix shared) — needed before GTK can use threads.
  - **Phase 2** for *worker host calls* (GLib's worker does real kernel I/O): route the worker
    isolate's host calls through the sidecar bridge to the same VM (currently the spike worker uses
    stub wasi imports since the minimal thread makes no host calls) + the fairness audit.
  - **Phase 3** WASI coherence; **conformance/stress/teardown tests** (DoD §9.1–§9.8), `max_threads`
    on the wire, and the **human TCB security sign-off**.
- **(superseded) earlier "Phase 1 integration" plan — now done for the minimal spike:**
  1. Make `wasi.thread-spawn` (runner JS) call a native host callback that, in the spawning isolate,
     grabs the shared memory + compiled module, allocates a tid, `register`s a `ThreadStart`, and starts
     a worker execution carrying the token; returns tid synchronously.
  2. A worker execution = a session/isolate on a new OS thread (mirror `session.rs:287`) running the
     runner in **thread mode**: `take` the `ThreadStart`, `deserialize_shared_memory` into `env.memory`,
     instantiate the shared module, call `wasi_thread_start(tid, start_arg)` instead of `wasi.start`.
  3. Route the worker isolate's host calls to the same VM's kernel behind the **per-VM lock** (Phase 2:
     copy args first, check+act atomic, DROP lock while blocking). Then the spike flips to SPIKE PASS.

## 11. Open questions (resolve in review)

1. **(RESOLVED — feasible)** Isolate spawning in `crates/v8-runtime`: create a new isolate on a new OS
   thread (mirroring `session.rs:287` `session_thread`), reconstruct the shared memory via the
   backing-store + `ValueSerializer` mechanism above, share the compiled module via `CompiledWasmModule`.
   No table sharing needed (per-instance). Open sub-question: reuse `SessionManager` or a dedicated
   lighter-weight thread-isolate type?
2. `--max-memory` for GTK (512 MiB?) and `max_threads` default (64?). How do they compose with the
   existing runtime memory cap clamp?
3. **(RESOLVED)** The WASI shim is a **custom in-isolate `WASI` class** (`crates/execution/src/wasm.rs`
   ~`:2244`, injected as `globalThis.__agentOsWasiModule`) — fully ours, not node:wasi. Each thread
   isolate gets its own shim instance bound to the shared memory; Phase 3 still kernel-owns stateful WASI
   for coherence.
4. Is the single per-VM lock fine-grained enough that GTK is not serialized to uselessness? If not, what
   is the minimal set of independently-lockable kernel subsystems — and how do cross-subsystem resource
   snapshots stay atomic (§Phase 2)?
5. Node-host and browser variants: the SAB + `worker_threads` / Web-Worker model (the demoted emnapi
   template) — separate follow-up after the core V8-isolate landing. Note that synchronous-`Atomics.wait`
   servicers are Worker-only (not page-main-thread), so a browser variant needs `waitAsync` regardless.

---

## References

### Reference implementations (source to read before/while building)

The adversarial review reasoned about these from ABI knowledge but did **not** read their source; read the
actual code below before Phase 1, especially wasmtime's thread lifecycle and lock-free-while-parked logic.

- **`wasmtime-wasi-threads`** (Rust) — **primary structural reference.** A host spawning OS threads that
  re-instantiate a module against one shared memory: our exact model (V8 isolates from Rust sharing a
  `WebAssembly.Memory`), minus the engine. Study: tid allocation, the `wasi_thread_start` invocation,
  instance-per-thread sharing memory+table, teardown, and its multi-tenant caveat.
  - Source: https://github.com/bytecodealliance/wasmtime/tree/main/crates/wasi-threads
  - Crate page: https://crates.io/crates/wasmtime-wasi-threads
- **wasi-libc threads** (C) — **the source of truth for what the guest emits.** This is what `-pthread`
  compiles to: the `thread-spawn` import, the `wasi_thread_start` export, stack/TLS setup, `pthread_join`
  + futex. Read this to get the host contract exactly right (§2).
  - Source: https://github.com/WebAssembly/wasi-libc (see `libc-top-half/musl/.../pthread`, `__wasi_thread_start`)
- **`@emnapi/wasi-threads`** (JS, MIT) — **semantic reference only (demoted).** Targets Node
  `worker_threads` + browser Workers = the opt-in path we are *not* using for the core V8-isolate runtime.
  Relevant only for a future Node/browser variant (§11 Q5): tid bookkeeping, `wasi_thread_start` calling.
  - Source: https://github.com/toyobayashi/emnapi/tree/main/packages/wasi-threads
  - npm: https://www.npmjs.com/package/@emnapi/wasi-threads
- **WAMR wasi-threads** (C) — **ABI-fork guardrail.** Confirms old host-pthread vs new wasi-threads are
  incompatible (we target the latter). Implementation lives in WAMR's `core/iwasm` lib-wasi-threads.
  - Source: https://github.com/bytecodealliance/wasm-micro-runtime
  - Intro: https://bytecodealliance.github.io/wamr.dev/blog/introduction-to-wamr-wasi-threads/
- **Emscripten pthreads** (C/JS) — **mechanics only.** Growable `SharedArrayBuffer` semantics + the
  buffer-detach-on-grow hazard (§Phase 1). Pthread *model* is not our ABI.
  - Docs: https://emscripten.org/docs/porting/pthreads.html
  - Source: https://github.com/emscripten-core/emscripten (`system/lib/pthread`)

### Specs / docs

- **wasi-threads proposal** (normative ABI, §2) — https://github.com/WebAssembly/wasi-threads

### Runtime evidence

- Runtime evidence (wasmgui workspace): guest runs in a V8 isolate via `crates/v8-runtime`
  (`execute_module`, `session.rs`); synchronous host imports in the materialized wasm-runner
  (`crates/execution/src/node_import_cache.rs`, `wasm_runner_path()`; `ffi_call` reflects on
  `__indirect_function_table`; `__agentOsSyncRpc.callSync`); the SAB+`worker_threads` bridge is gated by
  `AGENT_OS_NODE_SYNC_RPC_ENABLE` (off by default). Vendored threaded sysroot:
  `registry/native/c/.../wasi-sysroot/lib/wasm32-wasip1-threads/`.
- `M8-FINDINGS.md` (threads-gap reproduction), `SPEC.md` M8.

---

## Appendix: review-finding ledger (v1 → v2)

Four adversarial reviewers (ABI, security/TCB, RPC/concurrency, test/build). Disposition:

- **Execution-model correction (test/build reviewer, BLOCKER):** v1 targeted the opt-in
  `node:wasi`+`worker_threads`+SAB bridge; real path is V8-isolate + synchronous in-context imports.
  → §1 rewritten; Phases 1–2 rewritten (isolate-spawn from Rust; critical-section lock, not multi-channel
  SAB); emnapi demoted, wasmtime promoted (§3).
- **ABI errors (ABI reviewer, 2 BLOCKER/4 MAJOR):** start_arg is opaque; stack/TLS activate inside
  `wasi_thread_start`; table is imported+shared; memory imported under `env`; `--export=wasi_thread_start`;
  `wasm32-wasip1-threads` target; post-grow view re-derive. → §2, §4 corrected.
- **Security (TCB reviewer, 3 BLOCKER/…):** lock unit = whole servicing routine, not one kernel op;
  copy args before checks; enumerate non-kernel TCB state + lock order; resource snapshot+check+act
  atomic, rule out per-subsystem locks; `max_threads` on wire, check-and-increment in lock; trap/partial
  state. → §Phase 2, §5, §6 R4/R5.
- **Concurrency (RPC reviewer, 3 BLOCKER):** the per-VM lock must drop on blocking ops or it recreates
  M6.4 starvation intra-VM; the "no guest→guest edge" deadlock argument is false (pipe/socket/PTY/join/
  futex edges); the deadlock test needs real inter-thread dependencies. → §Phase 2 deadlock model, §9.5.
- **Test/build (test reviewer):** spike-before-GTK; threaded sysroot already ships; `wasi-compat.c`
  pthread-stub collisions + `-pthread`-stripping wrappers; rebuild whole closure shared; flake gate was
  deterministic theater → real jitter + sanitizer + ≥200; lock-disabled negative test; libffi circular
  gate → Phase 0; wire-parity + named regression aggregate. → §0, §4 Phase 0, §9, §10.
