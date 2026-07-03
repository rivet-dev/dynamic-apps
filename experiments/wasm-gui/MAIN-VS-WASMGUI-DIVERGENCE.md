# MAIN ↔ WASMGUI Behavioral Divergence Ledger

Capability-level map of where secure-exec `main` and the `wasmgui` branch evolved
the **same behavior in different directions**, so you can decide which to take when
re-implementing wasmgui's GUI work on a clean `main`.

Scope: 8 subsystems, 48 distinct capability-level divergences.

---

## 1. TL;DR

**The perf-lever substrate did NOT move. This is the single most important fact for the re-impl.**

Main left the sync-RPC bridge transport intact: the IPC frame format
(`[MSG_* opcode][session-id flag][u32-BE-len][bytes]`, `BridgeCall`/`BridgeResponse`
opcodes), the CBOR value serialize/deserialize path, the `register_sync_bridge_fns`
callback shape, and the net-timeout sentinel are all unchanged. Main's only additive
transport edit is a `userland_code` field appended to the Execute/WarmSnapshot frames.
Likewise the "funnel" (V8→host event channel → sidecar stdout frames) is the **same
channel**, just with better disposition (see below).

Consequence: **every wasmgui headline perf lever — InlineNetDrain inline-dispatch,
bulk-SAB, `host_net.ffi_call`, `_perfNow`, the bridge-method additions — attaches at
the same substrate and ports without re-derivation against a new transport.** The GUI
work is largely a clean graft. The conflicts that exist are concentrated and named below.

**Divergence counts by type:**

| Type | Count | Meaning |
|---|---|---|
| TRUE-CONFLICT | 5 | Same capability, opposite implementation; adopting wasmgui fights a main change |
| MAIN-BLOCKS-WASMGUI | 1 | Main deliberately closed a capability wasmgui needs (TCB sign-off) |
| INTERACTION-RISK | 5 | No textual conflict, but the two interact and must be validated together |
| COMPLEMENTARY | 10 | Different layers of the same story; merge both |
| WASMGUI-SUPERSEDED-BY-MAIN | 13 | Main already did it (often better); **do NOT re-port** — free wins |
| INDEPENDENT | 14 | Purely additive; grafts cleanly onto main |

**Headline takeaways:**
- The substrate is stable → port the levers as designed.
- 13 wasmgui "features" are redundant — main already shipped them (often as a strict superset). That is less to port.
- Only **6 items need real care**: 5 true conflicts + 1 TCB-blocked capability. Everything else is merge-both or graft-clean.
- The dominant *mechanical* hazard is the global `AGENT_OS_*`/`__agentOs*` → `AGENTOS_*`/`__agentOS*` rename on main: it breaks silently (undefined env lookup, mis-decoded Buffer payloads), not at compile time.

---

## 2. Ranked divergence table (most-severe first)

| Capability | Main direction | wasmgui direction | Type | WHICH TO TAKE | Concern |
|---|---|---|---|---|---|
| **net_poll POLLOUT bit value** | POLLOUT = `0x002` (POLLWRNORM); guests compiled against headers using 0x002 | POLLOUT = `0x004`; whole GTK/X toolchain compiled against `<poll.h>` with 0x004 | **TRUE-CONFLICT** | **needs-human-decision** — gate on guest ABI / unify toolchain header, not a single global const | Same const, opposite value, both cite X/libxcb. Wrong bit ⇒ writers block forever. Compounded by K3c merging socket+kernel-pipe POLLOUT bits into one revents word |
| **start_execution seam: `guest_reader` vs `net_drain` 3rd arg** | `start_execution_with_module_reader(req, module_reader, guest_reader)` installs a direct V8-thread module reader (bridge bypass) | `start_execution_with_bridges(req, module_reader, net_drain)` adds InlineNetDrain; no guest_reader | **TRUE-CONFLICT** | **merge-both** — unified signature must carry BOTH `guest_reader` AND `net_drain`; LocalBridgeState holds both | Porting wasmgui's shape verbatim silently deletes main's direct-module-reader fast path; compiles because both share the `has_module_reader()` short-circuit |
| **Host VM-config `PermissionsPolicy` schema** | renamed scope field `tool` → `binding`; `deny_unknown_fields` | still `pub tool`; host driver sends `"tool":"allow"` in VM_CONFIG_JSON | **TRUE-CONFLICT** | **take-main** — rename host config key `tool`→`binding` (or drop it) | `deny_unknown_fields` ⇒ stale `tool` key makes serde reject the ENTIRE config ⇒ CreateVm fails ⇒ black screen before any guest. Single highest-impact schema break |
| **net.poll blocking-wait ceiling** | unchanged from fork: `JAVASCRIPT_NET_POLL_MAX_WAIT = 50ms`, fixed, no override | lowered default to `3ms` + env-overridable `SECURE_EXEC_POLL_MAX_WAIT_MS` | **TRUE-CONFLICT** | **merge-both / needs-human-decision** — prefer wiring K3c's event-driven `SocketReadiness.notify()` (makes the timer redundant); drop the 3ms global default + env lever | wasmgui's 3ms lowers the busy-poll floor for EVERY net.poll caller in every VM; the clamp bounds guest-controlled waits so one VM can't stall shutdown |
| **Watchdog env-var name (X-server opt-out knob)** | renamed `AGENT_OS_V8_CPU_TIME_LIMIT_MS` → `AGENTOS_V8_CPU_TIME_LIMIT_MS` (+ wall-clock) | host harness still sets `AGENT_OS_V8_CPU_TIME_LIMIT_MS=0` to opt the long-lived X server out | **TRUE-CONFLICT** | **take-main** — emit `AGENTOS_V8_CPU_TIME_LIMIT_MS` in the ported harness | Verbatim port ⇒ opt-out silently no-ops. Main is off-by-default so usually harmless, but if any config arms the budget the X server is killed mid-session (~70s). Pure rename, high blast radius |
| **Guest-side PTY allocation (`pty_open`)** | DELIBERATE hard stub: `pty_open → WASI_ERRNO_FAULT`; guest cannot mint kernel PTYs (read/observe-only on inherited tty) | UN-stubs it: `pty_open → __pty_open → kernel.open_pty`, range-encoded master+slave into guest fd table; gated `permissionTier==='full'` | **MAIN-BLOCKS-WASMGUI** | **needs-human-decision** — fold into the SAME TCB sign-off as `proc_spawn`; do not silently un-stub | Re-enabling reverses a deliberate main security posture; clobbering the FAULT stub without sign-off ships an unreviewed guest capability (sidecar↔executor expansion) |
| **Bridge-method allowlist (4 registration sites)** | appends `_networkHttpServerRequestRaw`, `_kernelIsattyRaw`, `_kernelTtySizeRaw` to the same arrays | appends 11 of its own (`_wasmThreadSpawn`, net guest-fd, kernel-pipe/pty, `_perfNowRaw`, `_ptyOpenRaw`, …) | **INTERACTION-RISK** | **merge-both** — UNION across all 4 sites (bridge-contract.json + wasm.rs switch + map_bridge_method + session.rs global list) | Both edit the same contiguous arrays ⇒ naive rebase yields line conflicts; every method must appear in ALL FOUR places or the bridge rejects it at runtime |
| **Framebuffer file backing model + sandbox-root env name** | host-shadow write-through via real host fd; env `AGENTOS_SANDBOX_ROOT` | same host-shadow model (inherited) but env `AGENT_OS_SANDBOX_ROOT` (extra underscore) | **INTERACTION-RISK** | **take-main** backing model; graft K10 fb-delta run-diff onto the host real-fd write path (`_fdPwrite`/`writeSync`), **NOT** crates/vfs; reconcile env name | Plan's premise that fb "may route through crates/vfs" is wrong — fb never touches vfs. `AGENT_OS_` vs `AGENTOS_` is a concrete silent break if carried verbatim |
| **Internal env-var / JS-global naming convention** | global rename `AGENT_OS_*`→`AGENTOS_*` and `__agentOs*`→`__agentOS*` (Os→OS) | retains OLD fork convention everywhere (env keys + `__agentOsType`/`__agentOsVirtualOs`) | **INTERACTION-RISK** | **take-main** convention; mechanically reseat EVERY wasmgui addition before porting | **Silent breakage, not a compile error.** `__agentOSType:'bytes'` vs `__agentOsType` ⇒ byte/base64 marshalling mismatch ⇒ Buffer payloads mis-decoded (data corruption). Dominant porting hazard |
| **PTY / TTY surface (wire + bridge contract)** | new `__kernel_isatty`/`__kernel_tty_size` + ResizePty/PtyResized wire op | `_ptyOpenRaw`→`__pty_open` + kernel `open_pty_split` (master/slave across 2 procs) | **INTERACTION-RISK** | **merge-both** — drive ONE PtyManager/readiness path, not parallel plumbings | Two independently-evolved PTY plumbings could double-implement or fight over fd readiness |
| **JS event-channel overflow policy** | backpressure + warn-on-approach via queue_tracker (no session teardown) | `try_send` Full ⇒ `destroy()` session (fork behavior); cap shrunk to 64 | **INTERACTION-RISK** | **take-main** policy; graft only F1b coalesced-wake onto main's Ok arm | wasmgui GUI workload (framebuffer deltas, ~60fps) is the canonical channel saturator; reverting to destroy-on-full self-kills the session on a transient spike. Verify inline net_drain doesn't deadlock against a backpressured channel |
| **Bulk-SAB large-fs-write substrate** | absent; but session bootstrap rewritten (+221), SessionBuffers handed as raw `*const RefCell<SessionBuffers>` | full bulk-SAB (RingBacking.bulk, 8 MiB per-guest SAB, read_bulk_arg) | INTERACTION-RISK (re-seat) | **take-wasmgui** — re-seat against main's rewritten `SessionBuffers::new()` + raw-pointer handoff; give bulk its OWN flag | No semantic conflict, but the re-seat is mechanical-but-fiddly against changed code; coordinate with the session.rs rewrite owner |
| **max_threads / maxThreads VM limit** | absent; but ResourceLimits restructured (trait impl + ResourceGauges) | added end-to-end (`max_threads`, DEFAULT 64, wire, sidecar set, EAGAIN enforce, ThreadSlots backstop 128) | COMPLEMENTARY | **merge-both** — port onto main's restructured ResourceLimits + ADD `TrackedLimit::VmThreads` gauge | wasmgui diff authored against flat ResourceLimits; main's now has trait+gauges. Add the gauge when you add the field or you violate the bounded+warn+host-visible invariant |
| **K4 drain-fairness cap (`MAX_EVENT_DRAIN_PASSES`)** | UNBOUNDED event_ready drain; now calls BLOCKING send_output_frame | caps at 64 passes, re-arms + yields to select! so other guests' BridgeCalls aren't starved | COMPLEMENTARY | **merge-both** — re-apply K4 onto main's drain loop | K4 is ABSENT from main and MORE needed there: unbounded blocking drain can park the whole event loop on a full 4096 queue during a framebuffer burst. queue_tracker only WARNS, K4 BOUNDS |
| **PTY-attached child spawn (master fd owner)** | host-owned master (controlling-terminal model); resize via wire op | `open_pty_split` puts master in PARENT GUEST table (in-VM emulator drives in-VM shell) | COMPLEMENTARY | **merge-both** — add wasmgui's split-master as a distinct stdio mode beside main's host-master path | main's `resize_pty` assumes master = kernel_stdin_writer_fd (false in split model); naive wiring mis-resizes |
| **F1/F1b event-driven cold-boot ingest** | timer-only (250µs pump); no inbound notify branch | adds `process_event_notify().notified()` branch + producer poke | COMPLEMENTARY | **merge-both** — graft notify branch onto main's blocking_send Ok arm (not wasmgui's try_send body) | F1b producer poke lives in the SAME send_javascript_event main rewrote. Omitting costs ~1.8s first-paint |
| **socket-id leak fix on full-backlog connect** | validates listener + backlog capacity BEFORE consuming a socket id | did not touch socket_table.rs; but drives far more connect/accept churn | COMPLEMENTARY | **take-main** verbatim (zero conflict) | wasmgui GUI is the workload MOST likely to hit the leak (thousands of loopback connects); main hardened the exact primitive wasmgui leans on |
| **Backpressure/starvation observability** | structured edge-triggered queue_tracker warnings → host `limit_warning` event | ad-hoc `eprintln '[select-block] …'`; no host-visible event | COMPLEMENTARY | **take-main** surface; register the drain/funnel under queue_tracker when re-applying K4 | Two parallel observability mechanisms would drift; consolidate on queue_tracker |
| **Guest host-call registration surface** | grew allowlist via same points (tuple shape unchanged) | adds 11 net-new methods via identical points | COMPLEMENTARY | **merge-both** — append, don't replace | A wholesale copy of wasmgui's array drops main's new tty methods |
| **crates/bridge perf-clock vs queue_tracker** | added 673-line queue_tracker.rs (bounded-queue usage tracker) | adds `perf_now_micros` to lib.rs + `__perf_now` RPC; K4 drain bound | COMPLEMENTARY | **merge-both** — append perf_now_micros (clean), keep K4, both sit beside queue_tracker | Thematic overlap, not conflict; ideally register the stdio queue as a queue_tracker gauge so both agree on capacity |
| **Guest WASM module delivery (K6: host-path readFileSync)** | bakes module as base64 literal into runner source; host fallback re-base64s | hands leader the real host path → `readFileSync` into Buffer (drops ~31MB base64 source literal) | COMPLEMENTARY | **take-wasmgui** (apply K6) — replaces a pre-fork slow path; reseat env name to `AGENTOS_WASM_MODULE_HOST_PATH` | **Partial win only:** the readFileSync still routes through the wasm.rs sync-rpc handler that base64-encodes over the bridge (identical on both). The 17–23MB module STILL crosses as base64. Don't claim K6 "done" |
| **Base64 module decode (K5)** | hand-rolled `atob()`+charCodeAt loop (~1.5s on 17MB) | native `Buffer.from(…,'base64')`, loop as fallback | COMPLEMENTARY | **take-wasmgui** — drop-in faster, no main behavior to clobber | Benign perf; largely moot once K6 host-path applies |
| (remaining 14 INDEPENDENT + 13 SUPERSEDED items in §3, §6) | | | | | |

---

## 3. TAKE MAIN — wasmgui is already superseded (do NOT re-port)

These 13 capabilities are **free wins**: main already shipped them, frequently as a strict
superset, and several are listed in the wasmgui plan as graft items on a **false premise**
(they predate the branch or main already did them). Re-porting wastes effort and, worse,
can shadow/revert main's landed work.

| Capability | Why it's superseded | Hazard if re-ported |
|---|---|---|
| **JS/agent-SDK module delivery** | main built GuestModuleReader (in-process source reads) + userland-snapshot bake | Re-applying K6 to the JS path duplicates/shadows main's reader, wasting the round-trip elimination. Scope K6 to WASM only |
| **V8→host event-channel overflow** | main parks producer (blocking_send, cap 64→512, 80% gauge) instead of `destroy()` | Verbatim port reintroduces the session-kill; GUI framebuffer burst is its worst case. Graft only F1b onto main's Ok arm |
| **Sidecar stdout frame channel overflow** | main blocking backpressure + cap 4096; wasmgui try_send + cap 128 ⇒ tears down whole sidecar | A framebuffer burst hard-kills the sidecar under wasmgui's version |
| **Event-pump cadence** | main independently set 250µs (same value); wasmgui's only delta is a diagnostic env knob | None — convergent value; keep main, optionally keep the env knob as dev diagnostic |
| **Positional fd read (`pread`/offset)** | identical, INHERITED from fork (not a wasmgui add); plan K3g premise wrong | Wasted re-graft; downgrade to a regression test (gdk-pixbuf sniff-then-rewind) |
| **O_EXCL atomic exclusive create** | same host mapping, inherited from fork; also covered by vfs `create_file_exclusive` | None |
| **Offset-preserving pwrite** | present + equivalent on both sub-paths; fb is host real-fd, never touches vfs | The in-place clone-avoidance "lever" is NOT built on wasmgui — it's a proposed MAYBE |
| **procfs (/proc + per-pid)** | byte-for-byte equivalent ProcNode set already on main (kernel-resident) | Verify no wasmgui-only /proc node before deleting from graft list |
| **Per-thread CPU watchdog (CpuBudgetGuard)** | shared pre-fork ancestry; main ADDED macOS Mach impl (strict superset) | Copying wasmgui's (=fork) timeout.rs silently drops macOS CPU-budget enforcement |
| **Limit observability gauges** | main net-new wired watchdogs + every saturating limit into queue_tracker | Overwriting resource_accounting.rs/timeout.rs clobbers the entire observability layer |
| **Pyodide / Python runtime** | wasmgui made ZERO changes vs fork; identical pyodide 0.28.0.dev0/375pkgs. The `60→111` bump premise doesn't exist | None — GUI work doesn't touch Python |
| **Wire RequestPayload/ResponsePayload framing** | wasmgui added no variants; re-impl inherits main's renumbered protocol (lockstep) | Only if an artifact hardcodes old Ext=27; none found |
| **undici/fetch global dispatcher** | wasmgui never touched fetch; main added configured dispatcher | Don't let a whole-file v8-bridge.source.js port overwrite main's undici block — port only the 3 PTY facades |

---

## 4. TRUE CONFLICTS / DECISIONS

Five capabilities where adopting wasmgui's behavior **fights a main change**. Three need
a human call; two have a clear recommended resolution.

### 4.1 net_poll POLLOUT bit — `0x002` (main) vs `0x004` (wasmgui) — **HUMAN CALL**
Same constant, opposite value, both citing X/libxcb. The correct bit must match the
`<poll.h>` the guest binaries were **actually compiled against** — and the two branches
compiled their guests differently (main's net examples ⇒ 0x002; wasmgui's GTK/X toolchain
⇒ 0x004, matching the fork's `KERNEL_POLLOUT=0x0004`).
**Recommended resolution:** do NOT pick a single global constant. Gate the bit on guest
ABI or unify the toolchain header. Re-applying wasmgui reverts main's deliberate 0x002 and
breaks main's networking guests/tests; keeping main's 0x002 makes wasmgui's X/GTK guests
never see write-readiness. **Compounded by K3c**, which merges socket + kernel-pipe POLLOUT
bits into one revents word — on a mismatched bit the merged word is internally inconsistent.

### 4.2 `start_execution` seam — `guest_reader` vs `net_drain` — **MERGE BOTH**
Unify the signature to carry **both** main's `guest_reader` (direct V8-thread module reader)
and wasmgui's `net_drain` (InlineNetDrain); `LocalBridgeState` holds both fields. Porting
wasmgui's 3-arg `(req, module_reader, net_drain)` over main's 3-arg
`(req, module_reader, guest_reader)` silently deletes main's bridge-bypass module loading —
a real perf regression masked because both share the `has_module_reader()` short-circuit
(compiles, superficially works).

### 4.3 PermissionsPolicy `tool` → `binding` — **TAKE MAIN** (hard blocker)
`deny_unknown_fields` means a stale `tool` key makes serde reject the entire VM config ⇒
CreateVm fails ⇒ black screen. Rename the host harness key `tool`→`binding` (or drop it).
Mechanical, but it's the highest-impact break in the whole port if missed.

### 4.4 net.poll wait ceiling — 50ms (main) vs 3ms+env (wasmgui) — **MERGE / HUMAN CALL**
Prefer wiring K3c's event-driven `SocketReadiness.notify()`, which makes any timer ceiling a
redundant safety net. Do **not** lower the 50ms default globally — it bounds guest-controlled
waits so one VM can't stall dispose/shutdown, and 3ms raises wakeup/CPU churn for every
net.poll caller in every VM. The `SECURE_EXEC_POLL_MAX_WAIT_MS` env lever is on wasmgui's own
refuted list — drop it.

### 4.5 Watchdog env-var name — **TAKE MAIN** (pure rename)
Emit `AGENTOS_V8_CPU_TIME_LIMIT_MS` (+ wall-clock) in the ported harness. Already reconciled
in the plan. Low effort, high blast radius if missed (X-server opt-out silently no-ops).

### Plus one TCB-blocked capability (decision, not conflict)
**`pty_open` (MAIN-BLOCKS-WASMGUI)** — main keeps the guest `pty_open` a deliberate
`WASI_ERRNO_FAULT` stub. Un-stubbing exposes kernel-PTY allocation to untrusted guests
(sidecar↔executor expansion). **Fold into the same TCB sign-off as `proc_spawn`** — do not
silently un-stub.

---

## 5. INTERACTION RISKS — validate together (no textual conflict)

### 5.1 Main's queue_tracker backpressure vs wasmgui's funnel optimization — **synergistic, validate**
This is the central interaction. Mental model:
- **Main** made the funnel *recoverable + observable*: producer parks on overflow (blocking_send), caps raised (64→512, stdout 4096), edge-triggered 80% gauge warnings routed to the host as `limit_warning`.
- **wasmgui** made the funnel *carry less + drain fairer + ingest faster*: inline-dispatch moves hot fd-polls OFF the funnel (K1), K4 bounds the drain loop, F1b wakes the pump on RPC arrival.

These compose **without reconciliation and are strictly synergistic**: inline-dispatch lowers
funnel fill ⇒ main's gauge warns and blocking_send parks LESS often; faster draining ⇒ same.
**Validate:**
1. K4 is genuinely absent from main and MORE needed there — main's unbounded drain now calls a BLOCKING send, so a framebuffer burst can park the whole event loop on a full 4096 queue. **Re-apply K4 onto main's drain loop** (identical select! shape).
2. Register the drain/funnel under main's existing queue_tracker gauges (JavascriptEventChannel / SidecarStdoutFrames) instead of restoring wasmgui's `eprintln`.
3. Graft F1b's producer poke onto main's **blocking_send Ok arm**, not wasmgui's try_send body.
4. Verify wasmgui's inline net_drain interception doesn't deadlock against a backpressured event channel.

### 5.2 Main's bridge/session rewrite vs the perf-lever hooks — **re-seat, don't re-derive**
The transport is unchanged, so the levers port cleanly, but two of them must be **re-seated
against rewritten surrounding code**:
- **bulk-SAB** must allocate in main's rewritten `SessionBuffers::new()` and read via the new raw `*const RefCell<SessionBuffers>` handoff. Give bulk its own flag (decouple from dead `SECURE_EXEC_T1_RING`).
- **The bridge-method allowlist** — both branches edit the same contiguous arrays; UNION across all 4 registration sites, drop nothing.

### 5.3 The naming rename — **silent breakage surface**
Mechanically reseat EVERY wasmgui addition (`AGENT_OS_*` env keys, `__agentOs*` globals) onto
main's `AGENTOS_*`/`__agentOS*` before porting. Failure mode is silent: undefined env lookups
(feature disappears) and `__agentOSType` vs `__agentOsType` mismatch (Buffer payload corruption).
Also: `ERR_AGENT_OS_*` strings wasmgui emits must become `ERR_AGENTOS_*` to pass
`bridge_error_code`'s trusted-prefix allowlist.

### 5.4 Framebuffer fb-delta encoder — **wire to the right layer**
Hook K10's fb-delta run-diff onto the host real-fd write path (`_fdPwrite`/`writeSync` in
wasm.rs / node_import_cache), **NOT** crates/vfs — the framebuffer never routes through vfs.
Reconcile `AGENT_OS_SANDBOX_ROOT` → `AGENTOS_SANDBOX_ROOT` or the sandbox root silently fails.

### 5.5 Two PTY plumbings — **one manager**
Main's isatty/tty-size/resize and wasmgui's open_pty_split/`__pty_read` must share ONE
PtyManager/readiness path. And wasmgui's `__pty_read` must honor main's new canonical-EOF
`Ok(None)` marker, or a VEOF on the in-VM PTY is mis-mapped to EAGAIN and the in-VM shell hangs.

---

## 6. INDEPENDENT / take-wasmgui cleanly (the bulk)

14 capabilities are purely additive on main — nothing to clobber, the substrate is
fork-identical, the GUI work grafts without conflict. Briefly:

- **Core sync-RPC transport** — unchanged on main; the load-bearing reason the levers port cleanly.
- **WASM binary module delivery (raw bytes)** — main never improved it (still base64s); re-derive the raw-bytes path against main's rewritten wasm.rs (`wasm.rs:1008-1013`). Own path, no GuestModuleReader collision.
- **bulk-SAB substrate** — re-seat per §5.2.
- **Inline-dispatch (K1)** — services net.poll/fd_poll/accept off the funnel; lowers main's funnel pressure, strictly synergistic.
- **/dev char-device synthesis** (zero/full/random/urandom) — main has NONE; graft into the wasm runner's fs handler (NOT vfs). Without it dbus/xfconf degrade, blank icon theme.
- **mmap(MAP_SHARED) writeback** — guest-link-time `--wrap=mmap` shim (toolchain only); rides host-shadow pwrite. Lowest priority.
- **Host-backed mount confinement** — take main's hardening; if GUI mounts rely on symlinks/`..` they may newly EACCES — adjust mount layout, don't weaken the plugin.
- **Cross-isolate host_net socket sharing** (guest_net_fds/SocketReadiness/owner-routing) — entirely net-new; main's net.poll/server_accept handlers are fork-identical so owner-routing wraps cleanly. (Plan's "threads through main's rewritten dispatch" worry is overstated — main's rewrites were in bridge.rs/ipc_binary, not the net handlers.) Only coupling: the POLLOUT bit (§4.1).
- **DNS resolution** — neither side touched kernel dns.rs.
- **proc_waitpid kernel-table routing (H5)** — UNBUILT on both; build as planned (main's WNOHANG std fix stays underneath). Needed before open_pty_split + proc_spawn or multi-isolate waitpid ⇒ spurious ECHILD.
- **maxWasmFuel / maxWasmMemoryBytes** — trusted client config; main already enforces + observes. Just carry the values.
- **Ext extension envelope** — main enriched additively; GUI host-calls go through bridge dispatch, not Ext. No action.
- **Worker-thread (wasi-threads) module delivery (K7/K2)** — net-new; main has no threads. Port as a unit with K2 (token-based module/memory reuse).
- **Per-sidecar agent-SDK V8 snapshot** — take main as-is; wasmgui's runner is `inline_code`, orthogonal.

---

## 7. Net guidance — re-impl ordering

Resolve these **before** porting the dependent features:

1. **FIRST, repo-wide: apply the `AGENT_OS_*`→`AGENTOS_*` / `__agentOs*`→`__agentOS*` rename to every wasmgui addition** (§5.3). This underlies everything; doing it lazily produces silent data corruption. Also rename `ERR_AGENT_OS_*`→`ERR_AGENTOS_*` and `AGENT_OS_SANDBOX_ROOT`→`AGENTOS_SANDBOX_ROOT`.

2. **Fix the VM-config schema (`tool`→`binding`) before any guest launch attempt** (§4.3). Nothing renders until CreateVm succeeds.

3. **Delete the 13 superseded items from the graft list** (§3) before estimating work — they are free wins and several have false premises in the plan.

4. **Get the TCB sign-off batch** — `pty_open` un-stub (§4) + `proc_spawn` — before wiring the in-VM terminal/PTY features. They're gated capability expansions, not perf work.

5. **Decide the POLLOUT bit (§4.1) before porting K3c** (the merged socket+pipe poll path) — K3c's correctness depends on the bit being coherent.

6. **Build H5 (proc_waitpid kernel-table routing) before open_pty_split + proc_spawn** — otherwise the second isolate's waitpid surfaces spurious ECHILD.

7. **Unify the `start_execution` seam (§4.2) before porting InlineNetDrain** — the signature must carry both `guest_reader` and `net_drain` so you don't silently revert main's module-reader fast path.

8. **Then port the levers** (clean grafts, in this order): inline-dispatch (K1) → bulk-SAB re-seat (§5.2) → WASM raw-bytes delivery → K4 + F1b onto main's funnel (§5.1) → K6/K5 module decode → /dev synthesis → fb-delta encoder onto the host fd path (§5.4) → worker-thread (K2/K7) → max_threads with its gauge (§2).

9. **Throughout, UNION the bridge-method allowlist across all 4 sites** (§5.2) and register new funnel/drain queues under main's queue_tracker rather than restoring ad-hoc eprintln.

**Bottom line:** the substrate held, so this is mostly a disciplined graft, not a re-derivation.
The risk is concentrated in ~6 named items (5 true conflicts + the TCB-gated pty_open) and one
repo-wide rename. Get those right and the GUI work lands on main without fighting it.
