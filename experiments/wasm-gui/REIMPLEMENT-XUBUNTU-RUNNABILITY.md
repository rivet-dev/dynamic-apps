# Xubuntu-on-WASM Runnability Verdict — post-reimpl on clean secure-exec main

_Synthesis of 8 adversarial subsystem reports (X spine, GTK/FFI substrate, D-Bus/xfconf, FS+fonts, threads/XU7, perf parity, host harness, build reproducibility). De-duplicated and ranked._

---

## 1. TOP-LINE VERDICT

**RUNS-AFTER-ADDING-FEATURES — currently AT-RISK / will NOT run if the plan is executed as the K1–K15 list literally reads.** The runtime levers (K1–K15) are largely sound, but the desktop is **not buildable and not driveable** on a clean main without a set of items that exist only in the gaps-audit (H/M/D series) or only as untracked on-disk hand-edits — not as actionable KEEP items. The single hardest wall is **build reproducibility**: the X server + threaded X client libs (`Xvfb.wasm`, `libxcb-threads`, `libX11-threads`) have **no tracked from-scratch recipe and no tracked patch files** — the threaded-cond patches live only in this machine's working tree (gitignored), so a clean checkout reconstructs them pristine and **every GTK guest hangs forever at `gtk_init`**. Right behind it: the **libffi-wasm guest shim** (the FFI keystone the whole GLib→GTK chain links) is outside `third_party/` and in no KEEP item, so no GTK binary builds at all; the **host driver crate** (the only thing that creates the VM, injects input, and scrapes frames) is in no KEEP item; and **`dbus_creds.c` has no source anywhere** (only a `.o` blob), so EXTERNAL SASL auth fails and every session blanks. **Performance will NOT match by default** either: at least five perf levers (K6 raw-module delivery, K1 worker-site inline dispatch, K8 thread-exit sync-skip, F1b cold-boot ingest, K10 fb-delta) re-seat into heavily-rewritten hot files and **regress silently** (correct pixels, wrong speed) if mis-applied — K6 in particular looks "already done" on main because main resolves the host path but still base64s the 17–23 MB module over the bridge. Net: with the ~15 additions below the desktop runs and roughly matches the achieved baseline at **~3-app render scale**; the **4–5 app XU7 milestone stays RED** until the isolate-divergence generalizing fix is actually built (it is only *named*, never built).

---

## 2. MISSING FEATURES TO ADD TO THE PLAN (the main deliverable)

These are desktop dependencies **not covered by any actionable KEEP item** (or covered only by an audit note the plan never folds in). Ordered by severity.

### BLOCKERS — desktop will not build / boot / be driveable without these

#### M-1 (new **K-host**): Host driver harness re-implementation
- **Dependency:** `experiments/wasm-gui/host/src/main.rs` is the *entire* consumer side — it `CreateVm`s, stages every fixture (`GuestFilesystemCall` mkdir/write/install_tree), launches xserver+WM+panel+filemanager+settings+dbus via per-process `ExecuteRequest` env, injects XTEST over the host-backed X socket (`:622`), and scrapes frames from the shadow dir (`:1579-1637`). Every screenshot/bench runs through its `--capture/--exec/--xdemo` modes.
- **Fails if omitted:** plan stops at K15 with no host item (H4 is KEEP-AS-DESIGN only; L8 is a Cargo footnote). The runtime boots but there is **no supported way to create the VM, feed input, or read a single frame** — black screen to the host, zero observability, zero benches.
- **Severity:** BLOCKER
- **Add:** New `K-host` keeper that ports `experiments/wasm-gui/host` (or an equivalent supported AgentOs driver) as a workspace member, enumerating its required wire surface (all present on main): `CreateVmRequest{config}`, `ExecuteRequest{...,env,wasm_permission_tier}`, `GuestFilesystemCallRequest{Mkdir/WriteFile/Pread}`, `ProcessOutputEvent` subscription for launch gating, plus the two out-of-band host-fs couplings (shadow-dir frame scrape, host-backed X-socket connect). Register the crate (L8).

#### M-2 (new KEEP, sibling to K14/K15): Guest-side **libffi-wasm** shim + `build-libffi-wasm.sh`
- **Dependency:** every GLib/GObject/GTK guest links `third_party/wasm-prefix/lib/libffi.a`, produced ONLY by `experiments/wasm-gui/scripts/build-libffi-wasm.sh` compiling `experiments/wasm-gui/libffi-wasm/{src/ffi.c,include/ffi.h,ffitarget.h}` into a fake `libffi.a`+`libffi.pc` (v3.4.4). This C shim is the **guest half of D5** (calls `extern __se_ffi_call` → import `host_net.ffi_call`).
- **Fails if omitted:** `libffi-wasm/` lives **outside `third_party/`**, and K15's preserve note is scoped to `third_party/*`. A re-impl that preserves `third_party/*` and re-seats the JS host import still never carries `build-libffi-wasm.sh`/`libffi-wasm/` → `build-glib-stack.sh:23` fails → GObject's `dependency('libffi')` unresolved → **the whole GLib→cairo/pango/harfbuzz→GTK3 chain fails to build → no GTK binary exists to run.**
- **Severity:** BLOCKER
- **Add:** KEEP item carrying `libffi-wasm/{src/ffi.c,include/ffi.h,ffitarget.h}` + `build-libffi-wasm.sh`; list `bash build-libffi-wasm.sh` as explicit step 1 of the GLib-stack build in K15; **correct K15's preserve-sources note to include `libffi-wasm/`** (it is not under `third_party/`).

#### M-3 (correct **K13**): `dbus_creds.c` from-scratch reconstruction
- **Dependency:** dbus-daemon EXTERNAL SASL auth needs `__wrap_getsockopt(SO_PEERCRED)` → uid 0 + strong `recvmsg/sendmsg/__wrap_read` for the credential nul-byte over `host_net` AF_UNIX. `build-dbus.sh:31-51` links `libdbuscreds.a` from `registry/native/patches/wasi-libc-overrides/dbus_creds.c`.
- **Fails if omitted:** **`dbus_creds.c` exists in NO git/jj commit and is absent from main**; `wasi-compat.c` only weak-stubs `recvmsg/sendmsg`→-1. K13 says "pull the canonical `.c` from MAIN or `wasi-compat.c`" — **both lack it**; only a prebuilt `dbus_creds.o` blob exists. Following K13 literally → undefined `__wrap_getsockopt` → dbus's `#ifdef SO_PEERCRED` block compiles out → EXTERNAL auth returns "no credentials" → **xfconfd + every GTK app gets a blank session.**
- **Severity:** BLOCKER
- **Add:** Reconstruct `dbus_creds.c` from scratch (or disassemble the committed `.o`) into `wasi-libc-overrides/dbus_creds.c`: `__wrap_getsockopt` answering `SO_PEERCRED` with `ucred{uid=0,gid=0}`, strong `recvmsg/sendmsg` consuming the leading credential nul, `__wrap_read` forwarding non-socket fds to `__real_read`. Mark K13 a **from-scratch reconstruction**, not a "pull from main"; ship `-DSO_PEERCRED=17 -D_GNU_SOURCE`.

#### M-4 (correct **K13**): host_net `fcntl`/`writev`/`ioctl` libc overrides are **NOT on main**
- **Dependency:** dbus sets listen+accepted sockets non-blocking via `fcntl(F_SETFL,O_NONBLOCK)` on host_net fds and flushes auth "OK" via `writev`; build links `--wrap=writev --wrap=getsockopt`.
- **Fails if omitted:** commits `1977d19e2`(fcntl)/`c369688af`(writev)/`81b2bd45c`(ioctl FIONREAD) resolve **only on branch `nathan/workspace-default-dir`** — `::main &` those is **empty**. main's `fcntl.c` has no `IS_HOST_NET`/`0x40000000` branch and no `writev`/`ioctl` override. Without the fcntl host_net branch `O_NONBLOCK` is silently dropped on the dbus listen fd; without writev host_net the daemon can't flush the auth reply → **handshake stalls past `auth_timeout` → blank session.** _(Note: the working wasmgui desktop ships WITHOUT these — its functional path is the K3f guest-side `--wrap` layer + `0008-sockets.patch`, already on main via K12. So this is required only if K13's libc-layer convergence form is chosen; otherwise K13 must be redocumented as optional cleanup, not a link prerequisite.)_
- **Severity:** BLOCKER (as written — K13 instructs "verify main already has these"; verification result is **NOT-PRESENT**)
- **Add:** Treat K13 as fully un-landed. Either cherry-pick `1977d19e/c369688a/81b2bd45` (+`dbus_creds`, + `sockaddr_storage __ss_data[126]` widening) onto clean main, keeping `--wrap` names lockstep with `crates/wasi-ext`; OR document that the shipping path is K3f + `0008-sockets.patch` and demote K13 to optional. **Remove the false "already on main" framing.**

#### M-5 (new tracked patches → fold into **K15**): extract the **H7 threaded-X-client patches** to tracked files
- **Dependency:** every threaded GTK guest links `libX11-threads`/`libxcb-threads`; the fix (MONOTONIC bounded `pthread_cond_timedwait` + `_X_xmutex_init_recursive`) lives in `third_party/libxcb-threads/src/xcb_conn.c` and `libX11-threads/src/locking.c`.
- **Fails if omitted:** those files are **NOT-TRACKED** (caught by `.gitignore:51 third_party/*-threads/`, never force-added). The patch bodies exist **only in this machine's working tree** — no script, no `.patch` file reconstructs them; `rebuild-all.sh:36` does a plain `cp -r` of the pristine base and applies **no patch**. On clean main the trees are reconstructed pristine → missed intra-process cond broadcast → **infinite `pthread_cond_wait` → `gtk_init` hangs forever → no window ever appears.** _(Same patch family is also the X-spine BLOCKER "missing from K15's sanctioned-patch list".)_ Also: `libICE-threads/src/locking.c` IS tracked but its committed copy has **0 patch-hits** while the on-disk copy has the edit — even "tracked" trees diverge from a clean checkout.
- **Severity:** BLOCKER
- **Add:** Before this working tree is lost, extract `patches/xcb-threads-monotonic-cond-timedwait.patch` + `patches/libx11-threads-recursive-mutex-monotonic-cond.patch` (+ libICE), add them to K15's sanctioned-patch list as a **client-side set sibling to the Xvfb server patches**, and wire a `patch -p1` step into `rebuild-all.sh`/`build-xclient.sh` after the `cp -r`. Mark `CLOCK_MONOTONIC` mandatory (CLOCK_REALTIME is frozen, L1).

#### M-6 (new tracked build scripts; extend **K15**): from-scratch X-stack reproduction recipe
- **Dependency:** desktop needs `Xvfb.wasm` + `libxcb/libX11/libXrandr/libICE/libXt/libXpm/...`.
- **Fails if omitted:** `third_party/{libxcb,libX11,libICE,xserver}` have **0 tracked source files**; `build-xclient.sh` has no fetcher; `rebuild-all.sh` is a `config.log` **replay** tool that prints `SKIP (no dir)` when the trees are absent. On clean main it builds **nothing** for the X stack → **no display server → nothing renders.** The only tracked X artifacts are the 2 `xserver-*.patch` files (and the xserver revision they apply to, plus configure flags, are uncaptured). K15 understates this as "HIGH reproduction cost" when it is currently **impossible**.
- **Severity:** BLOCKER
- **Add:** Author tracked from-scratch scripts mirroring `build-gtk-deps.sh`: pin each upstream version+URL (libxcb, libX11, libXau, libXdmcp, libICE, libSM, libXt, libXpm, libXrandr/Xcursor/Xext/Xi/Xtst/Xrender/Xfixes, xorgproto, xtrans, exact xserver/Xvfb revision), download, apply tracked patches (2 existing xserver + M-5 threaded + the `Xtranssock.c` recv/send patch at `rebuild-all.sh:70`), capture configure flags explicitly. Verify `rm -rf third_party/{libxcb*,libX11*,xserver} && build` reproduces a working `Xvfb.wasm`.

#### M-7 (extend host-config DoD): re-anchor host `VM_CONFIG_JSON` to main's `PermissionsPolicy` schema
- **Dependency:** `Session::connect` sends `VM_CONFIG_JSON` (host `main.rs:40`) into `CreateVmRequest`, parsed by `crates/vm-config` `PermissionsPolicy` which is `deny_unknown_fields`.
- **Fails if omitted:** the host config has `"permissions":{...,"tool":"allow"}`, but main's policy has `fs/network/childProcess/process/env/binding` and **no `tool` key** → serde **rejects the entire config** → `CreateVm` fails → **VM never created, black screen at VM creation.** A verbatim port black-screens before anything launches.
- **Severity:** BLOCKER
- **Add:** Drop/replace the obsolete `"tool":"allow"` key (use `"binding"` if needed); validate the full config string round-trips through `crates/vm-config` on main; add a host-driver smoke assert that `CreateVm` succeeds with the ported config.

#### M-8 (promote **M8/H9** into plan KEEP): `session.conf auth_timeout=600000` fixture
- **Dependency:** the whole SASL handshake runs over the single sidecar service thread (Root-2 multiplex unsolved, D1). `prepare-dbus-fixtures.sh` stages `session.conf` with `auth_timeout/pending_fd_timeout=600000` + EXTERNAL+ANONYMOUS+allow_anonymous, **regenerated every run** (`rm -rf`, not `[ -d ] ||`).
- **Fails if omitted:** under a 3-app desktop the funnel starves each guest's handshake past the **30s default** → "not authenticated soon enough" → connection dropped → **xfconf init fails → blank render.** The fixture is documented only in audit M8/H9, never folded into a K-item.
- **Severity:** BLOCKER
- **Add:** Promote `prepare-dbus-fixtures.sh` into the mandatory fixture-staging KEEP set (mounted `--vm-tree vmdbus`): `auth_timeout=600000`, `pending_fd_timeout=600000`, EXTERNAL+ANONYMOUS+allow_anonymous, fixed `unix:path=/tmp/.dbus/session`, regenerate every run via `rm -rf`. Note: **ANONYMOUS is NOT a fallback** — stock GDBus `g_bus_get()` only attempts EXTERNAL, so M-3 creds remain mandatory.

### PERF-REGRESSIONS — desktop runs but is silently slow

#### M-9 (promote **M2** to fixture-build DoD): prebake fontconfig + GTK icon-theme caches
- **Dependency:** `vmxft/vmicons` ship font/icon files but **no `*.cache`/`icon-theme.cache`**.
- **Fails if omitted:** first render cold-rebuilds fc-cache + gtk icon cache by scanning/hashing every font/icon → **~1.4s first-paint regression every run** (single biggest remaining fp chunk). Silent: pixels correct, only slow.
- **Severity:** PERF-REGRESSION
- **Add:** Couple to H9 — the fixture step that builds `vmxft/vmicons` MUST run `fc-cache` + `gtk-update-icon-cache` so caches are baked into the vm-tree. Pure build-time; no core fix. Add an fp-budget assert (`fp < ~2s`) to the Phase-3 gate.

#### M-10 (promote **H2** to new **K18**): F1b cold-boot event-driven request-ingest
- **Dependency:** achieved `fp ~2.5s` depends on replacing the 250µs timer-polled **request ingest** with an event-driven `select!/Notify` branch during cold boot.
- **Fails if omitted:** F1b appears only parenthetically in the plan + audit H2; main's existing `event_ready select!` (`stdio.rs:236`) polls execution **events**, not inbound request ingest — it *looks* like coverage but isn't. Skipped → cold-boot RPCs wait on the 250µs cadence → **fp regresses ~1.8s silently.**
- **Severity:** PERF-REGRESSION
- **Add:** New numbered K18 (sibling to K4): cold-boot-windowed event-driven ingest on the REQUEST channel, windowed to boot so steady-state `ir` is not regressed; distinguish explicitly from the existing event pump. Gate with a cold-boot fp regression check.

### DEGRADE — multi-app interactivity / robustness gaps

#### M-11 (build **H5** under K3a, don't just name): `syntheticFdEntries` cross-isolate kernel-resolve fallback
- **Dependency:** K2 spawns each guest OS-thread as its own V8 isolate; module-scope `new Map()` (`syntheticFdEntries`) is isolate-local while the kernel fd-table is `kernel_pid`-shared. `lookupFdHandle` chain ends in `null` with **no kernel call**.
- **Fails if omitted:** a worker thread (every Xfce app spawns one) reading/writing a guest file the main thread opened gets `null → WASI_ERRNO_BADF`. Concrete breaks: **Mousepad Save, Thunar GIO copy/load, gdk-pixbuf sniff-on-GIO-thread** silently EBADF. Initial paint survives; interactive multi-app file I/O (the XU7 milestone) breaks. H5 only **names** it; K3a's reimpl notes build host_net sharing, not the guest-file path.
- **Severity:** DEGRADE
- **Add (BUILD, under K3a):** give `lookupFdHandle` a final kernel-resolve fallback on miss (mirror `getHostNetSocket`'s `net.resolve_guest_fd`), OR range-encode the kernel `targetFd` like `KERNEL_PIPE_FD_BASE` so guest-file fds are isolate-independent. One edge fixes every `wasiImport.*fd*` handler.

#### M-12 (build **H5** with proc_spawn): `proc_waitpid` kernel-table routing
- **Dependency:** `proc_waitpid` reads per-isolate `spawnedChildren` with no kernel fallback → under K2 a worker isolate sees an empty map though children are live → spurious `ECHILD/ESRCH`.
- **Fails if omitted:** lower blast radius today (desktop routes around fork via H9 fixtures), but the instant `proc_spawn` lands or any threaded `waitpid` path is hit, threaded process management breaks.
- **Severity:** DEGRADE
- **Add (BUILD, paired with proc_spawn):** route `proc_waitpid` (and finish `proc_kill`) through the kernel process table by pid (already cross-isolate-shared), like `proc_kill`'s `process.kill` fallthrough half-does.

#### M-13 (explicit MISSING milestone): 5-app concurrent-startup ceiling stays RED
- **Dependency:** XU7's milestone = xfwm4 + panel + xfdesktop + Thunar + several apps responsive.
- **Fails if omitted:** ~5 heavy guests serialize their **blocking** D-Bus startup handshakes through the single kernel service thread (T-H). K1 inline-dispatch offloads only the 3 hottest **non-blocking** sync-RPCs — **not** the blocking handshake. Root-2 multiplex was re-scoped DESIGN-ONLY. So post-reimpl the 5-app desktop still times out (`xfce4-panel: Failed to connect to D-BUS session bus: Timeout`). **Parity, not regression — but the plan does NOT deliver XU7's acceptance milestone; only ~3-app render is reachable.**
- **Severity:** DEGRADE (milestone-blocking, not runtime-blocking)
- **Add:** Carry T-H as an explicit **MISSING milestone item**, not a closed design note: a named, scheduled effort on bounded service-thread/kernel concurrency for the blocking D-Bus path, or per-guest startup round-trip reduction (GObject ffi_call/closure cost). State bluntly that XU7 ships at ~3-app render scale only.

#### M-14 (extend host-config DoD): CPU-time env-var rename + WASM-watchdog decision
- **Dependency:** host opts the ~70s X server out of the CPU-time watchdog via `AGENT_OS_V8_CPU_TIME_LIMIT_MS=0` on the separate empty-env `xserver` `execute_env`.
- **Fails if omitted:** main reads a **differently named** env `AGENTOS_V8_CPU_TIME_LIMIT_MS` (`javascript.rs:73`). If the WASM CPU-time watchdog is ported (security model requires bounded CPU) AND the host crate uses the wasmgui string, the opt-out **silently no-ops** → X server killed at default 30s → **full-black at 30–70s, no diagnostic.** H3 cites the wrong env string, so copying H3 reproduces the no-op. _(Also K17/H3: the watchdog/fuel/memory raises must be applied to the `xserver` execute_env specifically, not just a global default — promote to host-config DoD.)_
- **Severity:** DEGRADE (black-screens the X server, but a config-only fix)
- **Add:** Use main's actual `AGENTOS_V8_CPU_TIME_LIMIT_MS`; decide+document whether the wasmgui WASM CPU-time watchdog (`v8-runtime/session.rs` per-thread CPU clock) is re-implemented; if yes the xserver launch carries `=0`, if no record the bounded-CPU gap. Apply `maxWasmFuel=3600000` + `maxWasmMemoryBytes=536870912` + CPU opt-out to **every long-lived guest env**.

#### M-15 (extend host-config DoD): `maxThreads` headroom for a full session
- **Dependency:** `VM_CONFIG_JSON` sets only fuel+memory; `maxThreads` defaults (per-VM 64, process-global 128). A full Xubuntu session (X + WM + panel + filemanager + settings + dbus + services) each spawns wasi-thread workers, all in one VM.
- **Fails if omitted:** if live wasi-thread count exceeds the cap, `wasm.thread_spawn` → EAGAIN → GTK worker startup fails → **affected guest renders blank** (silent slot exhaustion under heaviest load).
- **Severity:** DEGRADE
- **Add:** Add `maxThreads` to host-config DoD with measured headroom; document the observed peak live-thread count. Lower confidence (defaults render 3-app today) — verify before raising.

---

## 3. HIGHEST RE-SEAT RISKS (covered in plan, fragile on main)

These have KEEP items but re-seat into **rewritten hot files** (`crates/vfs`, rewritten `execution.rs`/`stdio.rs`/`node_import_cache.rs`). Validate each at impl time.

| Risk | Where it re-seats | Validate at impl time |
|---|---|---|
| **K6 module delivery (silent half-done)** | main `wasm.rs:1009-1013` resolves `module_host_path` then **base64-encodes** it via `base64_encode_pub` into a `Value::String` | "path-based" is NOT sufficient — assert the module crosses the bridge as **raw bytes**, not base64. Confirm `readFileSync`/`promises.readFile` handler does **not** call `base64_encode_pub`. Deliver via Buffer/typed-array frame or K11 SAB channel. |
| **K3g positional read** (gdk-pixbuf/FreeType sniff-then-rewind) | **ALREADY ON MAIN** (`wasm.rs` `_fdRead` uses explicit `entry.offset`, `_fdPread`, `_fdSeek` all present; `__agentOSFs()=node:fs` positional pread) | **Downgrade to a regression test**, not a port: open font, read 8 bytes, seek 0, re-read, assert byte-identical. Don't waste re-seat effort. |
| **K3g/K10 pwrite-forward through crates/vfs** | `crates/vfs/posix/vfs.rs:335` keeps offset-preserving RMW pwrite; root/overlay/mount layers do **not** override it | **Already covered** — byte-identical to wasmgui's own pwrite; in-place pwrite stays a D3 MAYBE. Add regression: full-frame then sparse-delta writes reproduce byte-identical host `Xvfb_screen0`. |
| **K10 fb-delta re-seat** | main has plain `fd_pwrite`; **fb file may now be backed by `crates/vfs`** after the kernel→vfs split | First confirm where the fb file is backed on main; hook the run-diff/skip-identical-frame logic there, not the old `kernel/vfs.rs`. Gate with Xvfb futex-count (~1552/4s target, not ~32k). |
| **K1 worker-site inline dispatch** | `spawn_wasm_thread` is **new** (K2); easy to wire the JS site and miss the worker site | After K2, assert the worker session installs `InlineNetDrain` (fd_poll+accept, registry=None). Post-reimpl HOPSPLIT assert: `_kernelFdPollRaw` ABSENT from the service loop. Validate on a **threaded** guest. |
| **K8 thread-exit sync-skip ordering** | main calls `sync_process_host_writes_to_kernel` **unconditionally** at `execution.rs:4662` + `:6965`; **no `is_thread` concept exists** | Sequence K8 strictly **after** K2's `is_thread`; guard both sites with `if !process.is_thread`; re-seat against main's rewritten reap path (now → crates/vfs). 3-app max-funnel-stall regression (a single-app test cannot catch the ~20s walk). |
| **Frame-scrape write-through gate** | host reads `<shadow>/data/Xvfb_screen0` off disk; after vfs split the fb file may be `MemoryFileSystem` (in-memory) | Host-driver DoD: fb file MUST be host-shadow-backed (write-through), verified by an out-of-band read returning **live changing pixels** during a render. Explicit dependency edge host→K10/K3g. |
| **XKB keymap fixture coupling** | K15 lists `xserver-keymap-no-xkbcomp.patch` decoupled from `prepare-xkb.sh` → `/xkb/default.xkm` | Couple the patch to the fixture as a **required pair**; the patch `fopen("/xkb/default.xkm")` is inert without the precompiled keymap → **input silently dead while pixels render** (high coverage masks it). Assert `default.xkm` presence before declaring X up. |

**Crates/vfs fs-fidelity re-seat — overall:** the audit-traced good news is that positional read (K3g) and offset-preserving pwrite are **already correct on main / in the vfs default trait** and survive the Root→Overlay→engine→S3 layering byte-identically. The risks are concentrated in (a) where the **framebuffer file** is backed after the split (K10) and (b) the **char-device synthesis** (K3e: main only special-cases `/dev/null`; `/dev/urandom|zero|full|random` → ENOENT, which degrades dbus/xfconf → blank icon theme + wrong Xft). Validate both explicitly.

---

## 4. PERFORMANCE PARITY CHECKLIST

| Lever | Default-ON post-reimpl? | Trap / action |
|---|---|---|
| **K6** raw-module delivery | ⚠️ **silent-loss** | main is half-done (path-resolved but base64'd). Assert no base64 on the module read path. |
| **K1** inline dispatch (net.poll, `__kernel_fd_poll`, accept) | ⚠️ worker-site easily dropped | wire BOTH JS and worker (`spawn_wasm_thread`) sites; assert `_kernelFdPollRaw` absent from service loop on a threaded guest. |
| **K4** funnel fairness cap (64 passes / 250ms) | ✅ self-contained | re-apply to main's rewritten `stdio.rs` drain loop + add warn-on-cap. |
| **K8** thread-exit fs-sync skip | ⚠️ ordering | must land in the **same increment as K2**, guarded by `is_thread`, both exit sites. |
| **F1b / M-10** cold-boot ingest | ❌ **MISSING** (only H2) | promote to K18; distinct from existing `event_ready` event pump; ~1.8s fp loss if dropped. |
| **K10** fb-delta + batched drain | ⚠️ vfs re-route | re-seat into rewritten `fd_pwrite`; confirm fb backing location; futex regression gate. |
| **K9** `--fpcast-emu @64` (GTK) | ✅ verbatim | re-apply `build-gtk-app.sh:99 @64`; **keep Xvfb at @128** — do NOT "fix" it to @64 (would diverge from baseline X server). |
| **K11** bulk-SAB / `SECURE_EXEC_T1_RING` | ✅ correctly default-OFF | **NOT a regression** — baseline was measured with T1_RING OFF and bulk-SAB fb-write measured NULL. Don't treat default-off as a parity loss. Real fb win is K10 (default-on). Re-evaluate only if K6 raw-module is later routed through the bulk channel. |
| **M-9** fc-cache / icon-cache prebake | ❌ **MISSING** (only M2) | bake at fixture-build time; ~1.4s fp loss if dropped. |

**Bottom line:** four levers (**K6, K1-worker, F1b/K18, K10**) regress *silently* — correct pixels, wrong numbers — and **K6 is the most likely to silently not-apply** because main looks done. Two MISSING perf items (M-9, M-10) cost ~3.2s combined fp if forgotten. The bulk-SAB "headline regression" hinted in the prompt is a **red herring** — verified NULL.

---

## 5. MULTI-APP (XU7) HONEST STATUS

**The desktop does NOT yet compose 4–5 apps post-reimpl — it is effectively a ~3-app render target until two things are built that the plan only *names*.**

Two independent walls:

1. **Isolate-divergence generalizing fix (DEGRADE, named-never-built):** under K2 each guest OS-thread is its own V8 isolate with isolate-local `syntheticFdEntries`/`spawnedChildren` Maps over a `kernel_pid`-shared kernel table. H5 **names** the two divergence members (guest-file fds → spurious EBADF; waitpid → spurious ECHILD) but **builds neither** — K3a's reimpl notes only build host_net sharing. Until M-11 (kernel-resolve fallback in `lookupFdHandle`) and M-12 (proc_waitpid via kernel table) are **built**, interactive multi-app file I/O (Mousepad Save, Thunar copy, gdk-pixbuf on the GIO thread pool) silently fails on worker threads. Initial paint survives, so this hides behind a render-only gate.

2. **Concurrent-startup ceiling (DEGRADE, re-scoped design-only):** the 5-app set fails on wasmgui with D-Bus/xfconf **timeouts** (T-H) because ~5 heavy guests serialize their blocking D-Bus handshakes through the single service thread. K1 inline-dispatch offloads only non-blocking sync-RPCs, **not** the blocking handshake; Root-2 multiplex was re-scoped DESIGN-ONLY. So 5-app stays **RED — parity with wasmgui, not a regression, but not the XU7 acceptance milestone.**

Also gating multi-app correctness (covered, but ordering-critical): the **10ms poll-cap removal (C2) must land in ONE increment with the owner-targeted kernel-pipe write/close `socket_readiness.notify()` edge (K3c) and K3a `owner_socket_readiness`/`~thread~` resolution.** If the cap is removed (mandatory) before owner-routing is green, a worker→owner GWakeup is dropped and the reader **parks forever — a HARD HANG, strictly worse than wasmgui's busy-spin**, on every threaded GDBus/GLib app. Ship the default-OFF missed-wake **detector** as the standing regression guard.

**Required to actually compose multi-app:** build M-11 + M-12; land C2+K3c+K3a atomically with the missed-wake detector; land K8 with K2; then schedule the M-13 concurrent-startup-ceiling effort for 4+ apps. **State in the plan that XU7 ships at ~3-app render scale; 4–5 app is a separate, named, unscheduled milestone.**

---

## 6. ADD-TO-PLAN PATCH LIST (append to `REIMPLEMENT-ON-CLEAN-MAIN.md`)

```
# --- NEW KEEP ITEMS (desktop dependencies with no existing coverage) ---
K-host  Host driver harness: port experiments/wasm-gui/host as a workspace
        member (L8). Wire surface (all on main): CreateVmRequest{config},
        ExecuteRequest{...env,wasm_permission_tier}, GuestFilesystemCall
        {Mkdir/WriteFile/Pread}, ProcessOutputEvent launch gating, + out-of-band
        shadow-dir frame scrape and host-backed X-socket connect. [BLOCKER]
K16-ffi Guest libffi-wasm shim: keep libffi-wasm/{src/ffi.c,include/ffi.h,
        ffitarget.h} + scripts/build-libffi-wasm.sh (GUEST half of D5). Add
        `bash build-libffi-wasm.sh` as step 1 of the GLib-stack build. CORRECT
        K15 preserve-note: libffi-wasm/ is NOT under third_party/. [BLOCKER]
K18     F1b cold-boot event-driven request-ingest (promote H2): Notify/select!
        on the REQUEST channel, windowed to boot; distinct from main's existing
        event_ready EVENT pump. ~1.8s fp if omitted. [PERF]
# --- CORRECTIONS TO EXISTING ITEMS ---
K6      Verification criterion is RAW BYTES, not "path-based". main resolves
        module_host_path but still base64_encode_pub's it (wasm.rs:1009-1013).
        Confirm no base64 on the module read path; deliver via Buffer/typed-array
        or K11 SAB. [PERF, silent-loss]
K13     dbus_creds.c is in NO commit — reconstruct from scratch (not "pull from
        main"). fcntl/writev/ioctl host_net overrides are on branch
        nathan/workspace-default-dir (1977d19e/c369688a/81b2bd45), NOT main —
        cherry-pick OR redocument K13 as optional (shipping path = K3f +
        0008-sockets.patch already on main). Remove "verify main already has
        these". [BLOCKER]
K15     Add the H7 threaded-X-client patch set (extract xcb_conn.c + locking.c
        edits from the working tree to tracked patches/*.patch BEFORE it is lost;
        +libICE) and wire `patch -p1` after the cp -r in rebuild-all.sh.
        Add tracked from-scratch X-stack download+build scripts (pin versions/
        URLs/configure flags for libxcb/libX11/libICE/libXt/libXpm/xserver);
        rebuild-all.sh is a replay tool and builds nothing on a clean checkout.
        Couple xserver-keymap-no-xkbcomp.patch to prepare-xkb.sh -> /xkb/
        default.xkm + --vm-tree vmxkb as a required pair. [BLOCKER]
# --- FIXTURE / CONFIG DoD ADDITIONS ---
H9+M8   Promote prepare-dbus-fixtures.sh into the mandatory fixture KEEP set
        (--vm-tree vmdbus): session.conf auth_timeout=600000,
        pending_fd_timeout=600000, EXTERNAL+ANONYMOUS+allow_anonymous, fixed
        unix:path=/tmp/.dbus/session, regenerate every run via rm -rf. [BLOCKER]
H9+M2   fixture-build DoD: run fc-cache + gtk-update-icon-cache when building
        vmxft/vmicons so *.cache files are baked into the vm-tree. fp<~2s. [PERF]
H3/K17  Host-config DoD: re-anchor VM_CONFIG_JSON to main's PermissionsPolicy
        (drop "tool" key -> deny_unknown_fields rejects it); use main's env name
        AGENTOS_V8_CPU_TIME_LIMIT_MS (=0 on the xserver execute_env); apply
        maxWasmFuel/maxWasmMemoryBytes + CPU opt-out per long-lived guest env;
        add maxThreads headroom; decide+document the WASM CPU-time watchdog.
        [BLOCKER schema; DEGRADE rest]
# --- BUILD (multi-app), pair with K2/K3a/proc_spawn ---
K3a+    BUILD (not name) H5: give lookupFdHandle a kernel-resolve fallback on
        miss (mirror getHostNetSocket/net.resolve_guest_fd) OR range-encode the
        kernel targetFd like KERNEL_PIPE_FD_BASE. Fixes worker-thread EBADF on
        guest files (Mousepad Save, Thunar copy, gdk-pixbuf GIO). [DEGRADE]
K3a+    BUILD H5: route proc_waitpid (and finish proc_kill) through the kernel
        process table by pid, not the per-isolate spawnedChildren Map. Pair with
        proc_spawn. [DEGRADE]
C2/K3c  ORDERING INVARIANT: do NOT remove the 10ms poll cap until the owner-
        targeted kernel-pipe write/close socket_readiness.notify() edge + K3a
        owner_socket_readiness/~thread~ resolution are landed and green. Land
        cap-removal + owner-notify in ONE increment; ship default-OFF missed-
        wake detector. (Else HARD HANG, worse than spin.) [PERF/correctness]
K8      ORDERING: land in the SAME increment as K2's is_thread; guard
        sync_process_host_writes_to_kernel at execution.rs:4662 AND :6965 with
        `if !process.is_thread`. [PERF]
T-H     MISSING MILESTONE (not a closed design note): XU7 ships at ~3-app render
        scale only. 4-5 app needs a named, scheduled effort on the concurrent-
        startup ceiling (bounded service-thread concurrency for the blocking
        D-Bus handshake, or fewer per-guest startup round-trips). [DEGRADE]
```
