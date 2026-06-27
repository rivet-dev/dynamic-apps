# Secure-Exec Runtime Performance Optimization (startup / render throughput)

Continues from `XU7-STARVATION-INVESTIGATION.md`. That investigation fixed the **T-J starvation
spin** (the desktop went from 0% black to rendering). It surfaced a *separate* root — **T-H: the
single kernel service thread serializes all guests' syscalls** — as the reason rendering is *slow*
(single GTK app ~90-120s, xfce4-panel ~250-300s, 5-app desktop never finishes its D-Bus handshakes).
This spec drives that startup/render-throughput frontier to a measured target.

## 1. Problem statement

Rendering is correct but far too slow. Working mechanism (hypothesized, **unmeasured** — measuring it
is Phase 0): every guest syscall is one *synchronous* RPC serviced by a single kernel thread, GObject
dispatch routes through a JS `ffi_call` shim, payloads are JSON/base64-marshaled, and startup repeats
redundant scans (fontconfig / icon-theme / gschemas). Rough model: **cost ≈ (syscalls + GObject
calls) × per-op overhead**, all serialized. Idle is already cheap (the T-J fix made waits
event-driven); pure compute is near-native (JIT wasm). The slowness is the **boundary-crossing tax**,
densest at startup.

## 2. Targets (definition of done)

- **Single real GTK app (mousepad) first-paint: < 10s.**
- **5-app Xfce desktop painted + responsive (type/click): < 30s.**
- Stop when the targets hold **OR** every remaining lever's ROI has flattened (documented diminishing
  returns), with before/after numbers and a profile for each applied lever.

"Near-native" is the direction, not a literal bar — the wasm-in-V8 + per-syscall-RPC model has a
floor; the 10s/30s numbers are the contract.

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
- **P2 — V8 CPU profile of the guest isolate. ALREADY BUILT (`SECURE_EXEC_V8PROF=1`).** Enables V8's
  `--prof` tick profiler (`crates/v8-runtime/src/isolate.rs` `init_v8_platform`): V8 writes
  `/tmp/secure-exec-v8.log` (code-creation incl. wasm function names + sampled ticks); symbolize with
  `experiments/wasm-gui/scripts/v8prof-top.py` to get top self-time functions. This is the CPU profile
  — captures wasm/`ffi_call`/JS self-time. (Earlier wrong note: rusty_v8 v130 has no `v8::CpuProfiler`,
  so I assumed an Inspector build was needed — but the `--prof` tick profiler was already wired and is
  sufficient. The Inspector `Profiler` domain is an unnecessary alternative.) It's SIDECAR-side env, so
  it sidesteps the guest-env-gate gap entirely.
  - **Guest-env-gate gap (blocker for guest-side probes):** `SECURE_EXEC_*` debug vars set
    `globalThis.__rpcprof`/`__pollstat` in the runner (node_import_cache.rs ~8817), but they did NOT
    activate for **X-client** guests even after adding them to the host cenv allowlist + rebuilding —
    `__pollstat` never fired. The host-side `[rpcprof-host]` aggregator works (it reads the RPC stream
    host-side), which is why the RPC numbers exist. Fix the guest `process.env` plumbing for X clients
    (or lower the dump thresholds + add a startup "[gate] enabled" confirmation to diagnose) before
    relying on guest-side counters.

**The first number to get: service-thread-wait vs isolate-compute.** It decides RPC-bound vs
CPU-bound, which picks the first lever. Adding targeted logs to get more info is fine.

> **Determinism guardrail (verified):** profiling MUST NOT weaken guest determinism/isolation. The
> guest-facing clock stays frozen by default — `globalThis.performance.now() === 0`, and
> `Date`/`process.hrtime` are virtualized. The real monotonic clock (`originalPerformance`, captured
> once before the freeze) is **module-scope**, is **never placed on `globalThis`** or any
> guest-reachable object, and is bound **only** when the opt-in `SECURE_EXEC_RPCPROF` debug flag is set.
> So the guest cannot read a real clock by default; exposing one is a deliberate, debug-only opt-in.

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
- **L-A — Single kernel service thread serializes all RPCs.** Concurrency / multiplex. **DOWN for
  single-app** — the service thread is **97% idle**, NOT the single-app bottleneck (refuted by baseline
  #1). May still matter for the **5-app contention** case (the D-Bus-timeout ceiling). Risk: races
  across shared kernel state in a `#![forbid(unsafe_code)]` crate. _status: DEFERRED to the 5-app phase._
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
- **Caching is a fallback, not a crutch.** Allowed only when there's no clear runtime speedup; keep it
  simple, no overfitting the desktop workload.
- **Regression gate green every iteration.** A fixed smoke suite (the GUI render tests + the GWakeup
  probe + a conformance subset) must stay green after each lever.
- **Default-OFF diagnostics**, committed on `perf-pivot-work`, cataloged in `INTERNAL-TOOLING.md`.
- **Profiling never weakens guest determinism/isolation** (see the §4 guardrail): the real clock
  (`originalPerformance`) stays module-scope, never on `globalThis`/any guest-reachable object, bound
  only under the opt-in `SECURE_EXEC_RPCPROF` flag; the default guest clock stays frozen.
- **Never-self-approve** (require explicit sign-off): D-Bus-to-host, host-fd, GPU, host-network.

## 8. Completion bar (DONE only when ALL hold)

1. Benchmarks B0-B3 built + baselined (one number each, repeatable).
2. Profiler P1 + P2 built; the RPC-bound-vs-CPU-bound split measured (not assumed).
3. Targets met — single app < 10s, 5-app desktop < 30s painted + responsive — **OR** ROI flattened
   with documented diminishing returns per remaining lever.
4. Every applied lever has a **before/after number + a profile artifact**.
5. Regression gate green; Constraint #5 verified; recursion drained (no OPEN lever with clear ROI).

---

### Verdict log (newest first)

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
