# Architecture & Direction Review — secure-exec wasm-gui (Xubuntu-compat + perf pivot)

**Date:** 2026-06-26 · **Scope:** read-only review of the Linux/Xubuntu-compatibility effort and the perf
pivot. No code changed. **Workspace:** `/home/nathan/secure-exec-wasmgui` @ `perf-pivot-work`.

**Method.** Direct reads of the design docs + the load-bearing code, then a 5-dimension multi-agent sweep
(convergence, constraint-#5, perf direction, debt, security) with adversarial verification of 15 load-bearing
claims (14 verified: 12 confirmed, 2 partial-with-correction, 0 refuted). Every claim below is cited to
`file:line`. Where I am uncertain I say so.

---

## 1. Direction verdict: **ADJUST**

The foundation is genuinely good and the right structural problems have been *identified*. But the perf pivot
is mis-sequenced: its named first lever (T1) rests on a **misattributed cost** and an **unmeasured saturation
number**, and the session banked a half-wired T1 feature that now allocates memory into a void. None of this is
dangerous (so not RETHINK); none of it is clean enough to just continue (so not GO).

> **The structural question, answered plainly:** *Is the path to a fast multi-app Xubuntu desktop sound, or
> does something structural need to change first?* **Something structural needs to change first, and it is not
> T1.** The measured wall is single-thread serialization of all guest syscalls (Root-2). Making each poll
> cheaper (T1) does not unblock 4 guests that serialize on one thread. **Root-2 (kernel service-thread
> multiplex) is the prerequisite**, and it should be gated on **one free measurement** (`busy_pct`, already
> computed by the existing tracer) that the project has never actually read off.

Three things are true at once, and the docs blur them:
- **The components work** — XU0–XU6 render solo from unmodified upstream, with real platform-layer convergence.
- **XU7 has never happened** — every "done" is a *single* static guest; the multi-app desktop has never composed.
- **The two changes that would unblock XU7 (Root-2 multiplex, Root-1 marshaller root-cause) are both unstarted
  ("design only").** The ladder ends at a cliff, not a ramp.

### The bottleneck and where each lever actually acts

```
                          ONE current-thread tokio runtime  (stdio.rs:106-109)
                          one `let mut sidecar`             (stdio.rs:118)
   guest A ─┐             one service loop &mut sidecar     (stdio.rs:179-248)
   guest B ─┤  ~70k       ┌───────────────────────────────────────────────┐
   guest C ─┼─ polls/s ──▶│  service_javascript_sync_rpc  (execution.rs:13814)  │──▶ kernel VFS / socket table
   Xvfb   ─┘   (serialized)│  CBOR enc/dec · base64 ONLY on Cbor::Bytes        │     (single owner)
                          └───────────────────────────────────────────────┘
                              ▲ T1 shaves per-op transport (base64/CBOR)   ← NOT on the 58k integer polls
                              ▲ Root-1 cuts guest compute (the ~13s cascade) BEFORE the syscall flood
                              ▲▲ Root-2 makes THIS BOX parallel            ← the measured wall for XU7
```
<svg viewBox="0 0 760 250" xmlns="http://www.w3.org/2000/svg" font-family="sans-serif">
  <rect x="0" y="0" width="760" height="250" fill="#fbfbfd"/>
  <!-- guests -->
  <g font-size="12">
    <rect x="20" y="40" width="90" height="26" rx="4" fill="#e6f0ff" stroke="#3b6fb0"/><text x="65" y="57" text-anchor="middle">guest A</text>
    <rect x="20" y="74" width="90" height="26" rx="4" fill="#e6f0ff" stroke="#3b6fb0"/><text x="65" y="91" text-anchor="middle">guest B</text>
    <rect x="20" y="108" width="90" height="26" rx="4" fill="#e6f0ff" stroke="#3b6fb0"/><text x="65" y="125" text-anchor="middle">guest C</text>
    <rect x="20" y="142" width="90" height="26" rx="4" fill="#e6f0ff" stroke="#3b6fb0"/><text x="65" y="159" text-anchor="middle">Xvfb</text>
  </g>
  <!-- serialization funnel -->
  <path d="M110 53 L210 95 M110 87 L210 100 M110 121 L210 105 M110 155 L210 110" stroke="#b35900" stroke-width="1.3" fill="none"/>
  <text x="150" y="190" font-size="11" fill="#b35900">~70k polls/s, serialized</text>
  <!-- single service thread box -->
  <rect x="215" y="55" width="330" height="100" rx="6" fill="#fdf0e0" stroke="#b35900" stroke-width="2"/>
  <text x="380" y="78" text-anchor="middle" font-size="12" font-weight="bold">ONE service thread (single &amp;mut sidecar)</text>
  <text x="380" y="98" text-anchor="middle" font-size="11">service_javascript_sync_rpc · execution.rs:13814</text>
  <text x="380" y="116" text-anchor="middle" font-size="11">CBOR enc/dec · base64 ONLY on Cbor::Bytes</text>
  <text x="380" y="134" text-anchor="middle" font-size="10.5" fill="#777">stdio.rs:106-109 / 118 / 179-248</text>
  <!-- kernel -->
  <rect x="615" y="70" width="125" height="70" rx="6" fill="#e6f6ec" stroke="#1a9e4b"/>
  <text x="677" y="100" text-anchor="middle" font-size="11">kernel VFS /</text>
  <text x="677" y="116" text-anchor="middle" font-size="11">socket table</text>
  <text x="677" y="132" text-anchor="middle" font-size="10.5" fill="#777">single owner</text>
  <path d="M545 105 L613 105" stroke="#1a9e4b" stroke-width="1.5" marker-end="url(#ar)"/>
  <defs><marker id="ar" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="#1a9e4b"/></marker></defs>
  <!-- levers -->
  <g font-size="11">
    <text x="215" y="180" fill="#3b6fb0">T1: shaves per-op transport — but NOT the 58k integer polls</text>
    <text x="215" y="198" fill="#7a4fb0">Root-1: cuts the ~13s guest CSS cascade BEFORE the syscall flood</text>
    <text x="215" y="216" fill="#c0392b" font-weight="bold">Root-2: makes this box PARALLEL — the measured XU7 wall</text>
  </g>
</svg>

---

## 2. The central perf finding — T1 is mis-scoped, and the deciding number was never read

This is the most important analytical result of the review. The two perf docs **contradict each other** and the
code adjudicates in favor of the demoting one.

**The contradiction.** `PERF-ARCHITECTURE-RESOLVED.md:46-48` names T1 the "CONFIRMED primary lever" that "cuts
the round-trip overhead across 70k kernel-forwarded polls (base64 + postMessage + synthetic-wait)."
`PERF-FINDINGS.md:7-23` says the opposite: postMessage is browser-only and the native in-process path has no
base64 on the hot syscall path.

**The code says PERF-FINDINGS is right.** `cbor_to_json` emits base64 **only** in the `Cbor::Bytes` arm
(`crates/execution/src/v8_runtime.rs:447-451`); the Integer/Array/Map arms never call it. The dominant polls —
`__kernel_fd_poll` (28k, returns a revents bitmask) and `net.poll_wait` (30k, returns a generation integer),
≈58k of the ~70k total per `PERF-ARCHITECTURE-RESOLVED.md:39-44` — carry **zero bytes in both directions**, so
they never touch base64. postMessage is the browser worker path only (`packages/browser/src/worker.ts:809`).
**T1's headline justification (kill base64+postMessage across 70k polls) is misattributed.** T1 Phase 1 actually
only helps *binary* control payloads (X-socket writes, the ~12k data-carrying `net.poll` responses, DNS) — a
real but much smaller target than advertised.

**The deciding number was never measured.** The baseline reports only `count × servicing-µs` (721,539 µs total,
`PERF-ARCHITECTURE-RESOLVED.md:38-52`) and *infers* "cost = COUNT × round-trip overhead." But the tracer already
computes `busy_pct = busy_us / wall_us` (`crates/sidecar/src/rpc_trace.rs:56-59, 225`) — the one number that
distinguishes **throughput-saturation (~100% → Root-2 / count-reduction dominate)** from **per-round-trip
latency / idle-waiting (≪100% → T1 Phase-2 doorbell or Root-1 dominate)**. It was never reported. **The entire
lever ordering rests on a quantity the existing instrumentation produces for free and nobody read off.**

**What this means for sequencing:**
- The structural multi-guest lever is **Root-2** (parallelize the single service thread). The measured collapse
  (4 guests dead at ~211–251s, `ROOT-2-MULTIPLEX-DESIGN.md:3`) is serialization, and making each poll cheaper
  cannot unblock work that serializes on one thread.
- The part of T1 that *would* cut poll round-trip latency is **Phase 2 (a real `Atomics.notify` doorbell)**,
  which the design doc itself defers as "optional, only if measured" (`T1-SAB-RING-SECURITY-DESIGN.md:106-109`).
  And the X server "polls almost exclusively with timeout 0" (`node_import_cache.rs:11560`) — those spins are
  guest-event-loop-driven and a doorbell can't cut them without touching the X/GTK loop (forbidden by #5). The
  *blockable* polls already have a generation-based wakeup (`PollWaiterPool`, `node_import_cache.rs:11688`). So
  "attack the count" is largely already done for what's reducible and irreducible for the rest.
- **Root-1 is the best-evidenced lever and is orthogonal.** @128→@64 fpcast arity is a measured 3.7× on
  first-widget (`GOBJECT-FPCAST-INTEGRATION.md:64-66`, 0-pixel diff). It is *already applied* to the heavy GTK
  guests (`scripts/build-gtk-app.sh:99`) — contradicting `PERF-FINDINGS.md:88`'s claim that no arity tuning
  exists. The remaining Root-1 work is the **per-signature marshaller root-cause** (kill `g_cclosure_marshal_generic`,
  scope `--fpcast-emu`), which is a toolchain-layer project and unstarted.

> **Recommended re-sequence:** (0) run the existing trace, report `busy_pct` + wall-clock — *free, do first*;
> (1) if saturated → Root-2 multiplex; (2) Root-1 marshaller root-cause in the toolchain (compounds Root-2 by
> cutting the syscall flood); (3) T1 only after measurement justifies it, re-scoped to binary payloads, and
> wired at the empirically-confirmed servicing site — **not** sold as the 70k-poll fix.

---

## 3. Constraint-#5 audit — discipline is real; the debt is *inside* correctly-located shims

**Good news, verified.** No GTK/glib/Xfce/D-Bus/pango/cairo **component source is byte-modified** for compat.
The only `.patch` files under those trees are upstream-vendored glib subproject patches (libiconv/gnulib/pcre2)
and a libxml2 test fixture that is never applied; the only `sed`/`patch` in build scripts edit *generated*
artifacts (`.pc`/`.la`/meson cross-ini, and the generated Xvfb link command at `scripts/link-xvfb.sh:23-24`),
never component `.c`. All compat is `-Wl,--wrap=` linker shims compiled from `registry/native/patches/` or
`toolchain/` — the sanctioned native/libc/toolchain layer — and the LEDGER tracks *repaying* per-library patches
back into the sysroot (libxcb writev, libX11 ioctl, sockaddr_storage sizing, POLLOUT value, kernel-pipe GWakeup).
**The two X-server patches are sanctioned** (they touch only the wasm Xvfb, which #5 names as the platform "wasm
X server"). The admitted inferior-duplicate thunar gmodule shims **were actually deleted** (verified absent on
disk). The gmodule static-plugin name-table is **not** the forbidden package-name special-casing — wasm has no
`dlopen`, so enumerating linked plugins is structurally required, and the table is *generated* from the linked
set keyed on the gmodule ABI entry-point.

**The real debt (all in the right layer, but fragile):**

| # | Finding | Evidence | Correct fix |
|---|---------|----------|-------------|
| B1 | **VTE fork→posix_spawn shim has fidelity gaps.** `__wrap_fork` is setjmp+return-0; only `dup2` is recorded into `posix_spawn_file_actions`; `__se_fork_record_close` exists but has **no caller** (no `__wrap_close`), and `__wrap_setsid(){return getpid();}` is a no-op — so child `close()`/`setsid()` are dropped. A `fork()` *not* followed by `execve()` never longjmps and runs in-process with corrupt state. | `toolchain/vte-syscompat/fork_shim.c:25,27-53`; `pty_proc_shim.c:78`; `compat-include/unistd.h:11` | Keep the layer. Close the gap: wrap `close()` into the deferred-child window, fold `setsid`/`sigaction` into the recorded setup, and `abort()` loudly on a non-exec fork. Gate the terminal on the `proc_spawn` TCB sign-off. This is **idiom-only (fork+exec) emulation** and should be documented as such. |
| B2 | **Always-correct POSIX shims are opt-in per-app build flags.** The empty-path→ENOENT shim (universally-correct libc behavior; glibc rejects empty paths, wasi-libc wrongly resolves to cwd and hangs Thunar) links only when `SECURE_EXEC_EMPTY_PATH_SHIM` is set. Same for `gio-vfs-local`, `gmodule-softfail`. | `scripts/build-gtk-app.sh:51-69`; `toolchain/wasi-empty-path-shim.c` | Promote unconditionally-correct shims into the default link set (`cross-env.sh` / a `libhostcompat` archive); keep only genuinely app-dependent ones conditional. These are bucket-1 build knobs (legitimately env) — the issue is *fragility of opt-in*, not the channel. |
| B3 | **Debug `fprintf` on the hot `open()` path** in a production shim (`PTYDIAG __wrap_open` for every `/dev` path). | `toolchain/wasi-empty-path-shim.c` `__wrap_open` | Gate behind a `-D` debug define (like `GMODSHIM_DEBUG`) or remove. |
| B4 | **`AGENT_OS_V8_CPU_TIME_LIMIT_MS` on the ambient env channel** is the dead-cap pattern the sidecar CLAUDE.md warns about — but **only in the experiment's host harness**, not the production wire path. | `HANDOFF-PERF-AND-CEILING.md` Root-2; host harness | Low. If the wasm-gui host ever drives real VMs, route the CPU limit through `CreateVmConfig` limits. No action for the experiment. |

**Net:** constraint-#5 is *not* being violated by guest-side workarounds. The one genuinely un-converged Linux
primitive is **fork/process-spawn** — currently routed-around via fixtures (pre-staged `xfce4-panel.xml` to skip
the fork-based migrate; VTE parked). Landing `host_process.proc_spawn` is the real fix, not more config staging.

---

## 4. Tech-debt / cleanup inventory — the half-wired T1 feature

The session left **contained but real** debt centered on an unfinished "T1 SAB ring." Finish it or revert it;
the current half-state is the worst option.

| # | Item | Status (file:line) | Action |
|---|------|--------------------|--------|
| D1 | **`sab_ring.rs` (463 lines, 15 tests) + `shm_registry.rs` (257 lines, 7 tests) are dead-pending-wiring** | Zero callers outside their own `#[cfg(test)]` modules + the `mod` decls. `shm_registry` even carries `#[allow(dead_code)]` at `lib.rs:16` — the author knows. | **Wire or remove.** Tested-but-unconstructed cores pass CI and read as "done" while exercising nothing real. |
| D2 | **T1 ring is a half-built feature: producer allocates into a void, no consumer exists** | `session.rs:965-973` allocates `__secure_exec_t1_req`/`_resp` SABs (2×256 KiB/guest) + `t1_handoff_set` under `SECURE_EXEC_T1_RING`. But `t1_handoff_get` (`session.rs:2142`) has **zero** sidecar callers; `session.rs:712` carries `#[allow(unused_variables)] t1_handoff`. No guest `RingChannel` consumes the globals (`node_import_cache.rs` has zero `__secure_exec_t1_req` refs). | **Decide finish-or-revert.** When the flag is on today, the sidecar allocates real per-guest memory that nothing reads. Worst possible state. Note this is also blocked on the §2 measurement — don't finish it before the data justifies it. |
| D3 | **`experiments/wasm-gui/t1-ring/*.mjs` are standalone prototypes** | Reference only each other; the real runner (`wasm.rs`) never imports them. | Tie to the D2 decision: revert→delete; finish→port `runner-route` into the embedded runtime JS verbatim, keep these as reference only (don't maintain a parallel impl that drifts). |
| D4 | **`SECURE_EXEC_ROOT2_TRACE` is a dead env var** | Forwarded by the host (`host/src/main.rs:1124,1208`) but **zero** crate readers; the doc itself notes it produced 0 lines. | **Remove** both forwardings + the doc reference (forwarded-but-unread = dead-cap). The live profiler is `SECURE_EXEC_RPCPROF` + `rpc_trace.rs`. |
| D5 | **`SECURE_EXEC_TRACE` is done right — keep** | `OnceLock`-gated, zero-cost when off (`wasm.rs:976`, `execution.rs:13763`). | No action. (Correction to the review brief: it is **not** in `isolate.rs` — that file hosts the unrelated STACKDUMP feature.) |
| D6 | **Build-env isolation is mostly legit hygiene** | Isolated `CARGO_HOME`/`--target-dir`/pnpm-store prevent real registry-corruption races across parallel agent sessions on a shared host. | **Keep** the isolation. The one smell is `env -u CC/CXX/CFLAGS` — it means the wasm cross-compiler is exported into the *interactive* shell globally and leaks into native host builds. Fix at the dotfile/profile layer (scope wasm toolchain vars to wasm invocations), not by unsetting per-build forever. Neither touches shipped runtime source. |

---

## 5. Security of the new shared-memory code — sound by construction, with two caveats

I read `crates/sidecar/src/sab_ring.rs` in full. **The validation is genuinely correct, not cosmetic:**

- `ring_size` and `consumer_index` are kernel-owned **struct fields** (`sab_ring.rs:36-37`), set in `new()`,
  never re-sourced from the SAB; `consumer_index` advances only by kernel arithmetic (`:112`). ✓ matches the
  threat model.
- Each hostile control field is read **once** into a local then bounded: `len` is checked against
  `MAX_RECORD_BYTES` (`:99`, *reject* `RecordTooLarge`) and an available-bytes check (`:104`, *reject*
  `IncompleteRecord`) — **reject/teardown, not clamp-and-continue**. The payload is **copied out** via
  `read_data_wrapped` before any use (no double-fetch / TOCTOU). The advisory `producer_index` is the only thing
  clamped (`% ring_size`, `:88`), then re-validated downstream. ✓
- Overflow-safe (`4u32.saturating_add(len)`, `:104`); wrap-split bounded both halves.
- **Strong hostile-fuzz tests** (2000 iters × 5 ring sizes, random producer/len/payload — `:417-462`) assert no
  OOB/panic.
- Sidecar stays `#![forbid(unsafe_code)]` (`lib.rs:1`); `sab_ring.rs`/`shm_registry.rs` contain **zero** unsafe.
  The only `from_raw_parts_mut` lives in `v8-runtime/session.rs:2085-2086` — exactly the crate that owns raw V8
  memory. All four new caps are catalogued in `limits-inventory.json` (`MAX_RECORD_BYTES`, three shm caps,
  `T1_RING_BYTES`). ✓ (limits discipline honored even for unwired code.)

**Two reasons this is ADJUST not GO on security:**

- **E1 (medium) — non-atomic access to guest-shared memory.** The kernel reads/writes the SAB via plain slice
  ops (`read_header_u32` `:50-53`, `read_data_wrapped` `:63/:65`, `publish_consumer` `:119-124`) over memory the
  guest isolate can mutate concurrently from another thread. It's OOB-safe regardless of value, but it's a formal
  data race with no acquire/release ordering — so a *benign* guest could be spuriously torn down
  (`IncompleteRecord`) under weak ordering. **Fix when wiring:** read/write header words through atomic/volatile
  accesses to match the JS-side `Atomics`. x86-benign today, but close it before the transport goes live.
- **E2 (medium) — the entire consumer/servicing side is inert dead code,** so the escape-relevant wiring is
  **unbuilt and unreviewable**: the drain-loop fairness/bound (the *caller* loop is unwritten — `read_record`
  itself is bounded), the `ring_size` passed to `SabRingEndpoint::new` at the unsafe boundary (must be the
  kernel-owned `T1_RING_BYTES` matching the SAB length, never guest-derived), and per-VM `ShmRegistry`
  instantiation (each VM needs its own instance so intra-VM stays intra-VM). **This review certifies the
  foundation only; the dangerous parts must be re-reviewed at wiring time.**
- **E3 (low, not an escape) — shm segment ids are sequential and any in-VM guest may attach to any live id**
  (`shm_registry.rs:84-88`, no owner/ACL gate). This is fine *if* intra-VM guests (X server + clients) are one
  trust domain, which matches MIT-SHM semantics and the Trust Model (the enforced boundary is sidecar↔executor
  and cross-VM, not guest↔guest within a VM). **Make that decision explicit when wiring**, and confirm the
  registry is per-VM.

---

## 6. Convergence assessment — coherent, not a pile of one-offs

The LEDGER fixes cluster into **genuine categories** (libc / X11-glib / net / fs / D-Bus / threads / toolchain),
and the healthy majority are **repaid into the wasi-libc/sysroot/`--wrap` layer** rather than left as per-app
band-aids. That is real convergence. The tail of component-specific shims is small and mostly justified
(whiskermenu register, the `XfceTitledDialog` type-ensure wrap, gio-vfs-local). The honest caveat is that
"renders" is proven by an **out-of-band framebuffer scrape** from the host shadow dir
(`host/src/main.rs:1357-1359`) precisely *because* an in-band wire readback starves under load — so the
screenshots are static frames, not evidence of interactivity. The XU7 DoD ("responsive enough to type/click/
switch") is **unproven by construction** until there's an in-band input→visible-response proof measured over the
wire.

---

## 7. Ranked recommendations

1. **[do first, free] Measure `busy_pct`.** Re-run the existing `SECURE_EXEC_TRACE` with the watchdog and report
   `busy_us/wall` + session wall-clock (`rpc_trace.rs:225`). This single number reorders the whole roadmap
   (saturation→Root-2 primary; latency→T1 Phase-2; compute→Root-1). No code. *Don't wire anything else until this
   is in hand.*
2. **[unblocks XU7] Commit to Root-2 as the structural prerequisite** — the dedicated-X-server-IO-thread → bounded
   per-subsystem-locks plan (`ROOT-2-MULTIPLEX-DESIGN.md:16-22`) in `crates/sidecar`/`crates/kernel`. Re-baseline
   the XU7 ETA as a multi-week TCB effort; stop reporting it as "one milestone away" when zero concurrency code
   has landed. Land the host-side service-thread observability (step 1 of that doc) first so "the multiplex
   helped" is measurable.
3. **[decide now] Finish-or-revert the T1 ring (D1–D3).** Given §2, the honest call is **revert the producer-side
   allocation behind the flag now** (`session.rs:965-973` + `allocate_t1_ring_sab`/`with_ring_backing_slices`/
   `T1RingHandoff`/`t1_handoff_set/get`) and **keep `sab_ring.rs`/`shm_registry.rs` as the reviewed foundation**,
   re-introducing the producer only when the measurement justifies T1 and with the §5 atomic-access fix. Do not
   leave the producer live allocating into a void.
4. **[debt, cheap] Remove the dead `SECURE_EXEC_ROOT2_TRACE` forwardings (D4)** and gate/remove the `PTYDIAG`
   `fprintf` (B3).
5. **[correctness] Close the VTE fork-shim fidelity gaps (B1)** and **promote universally-correct POSIX shims to
   default-on (B2)** — both in the toolchain layer, components untouched. Gate the terminal on the
   `host_process.proc_spawn` TCB sign-off.
6. **[perf root-cause] Treat Root-1 Lever B (per-signature marshallers / vendored-binaryen `SelectiveFpcastEmulation`)
   as the load-bearing single-guest perf project** in `registry/native`/toolchain. Don't burn cycles on Lever A
   arity micro-tuning (@64→@28 is ~2.6%, self-admittedly marginal). Keep Root-1 — it's the best-evidenced lever
   and orthogonal to the RPC debate.
7. **[doc hygiene] Reconcile the perf docs.** `PERF-ARCHITECTURE-RESOLVED.md` and `PERF-FINDINGS.md` contradict
   each other on base64 and on whether arity tuning exists. Correct the resolving doc to stop calling T1 the
   "primary 70k-poll lever," and fix the stale "@128 tuning doesn't exist" / "max-func-params is an un-done quick
   win" banners (it's applied at `build-gtk-app.sh:99`).
8. **[acceptance] Replace the out-of-band FB scrape as XU7 proof** with an in-band input→response measurement over
   the wire. Until then XU7 is unproven regardless of how many components render solo.

---

### Appendix — verification ledger

15 load-bearing claims tested adversarially; 14 returned (1 verifier hit an auth error, re-confirmed manually).
**12 confirmed, 2 partial, 0 refuted.** The two partials sharpened precision without changing conclusions:
(a) "only two glib patches / three sed scripts" → the patch inventory is slightly broader (pcre2 packagefile,
a libxml2 fixture) and there's a 4th `sed` on a *generated* link command — **still zero component-source
modification**; (b) "the only unsafe in v8-runtime is `from_raw_parts_mut`" → v8-runtime has many other unsafe
blocks, but the security-load-bearing part holds — **the sidecar TCB is unsafe-free and the ring's raw slices
are isolated to v8-runtime.** I independently re-read `sab_ring.rs`, the `cbor_to_json` base64 path, the T1
wiring greps, and the limits inventory; all align with the agents' findings.
