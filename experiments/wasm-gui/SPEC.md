# Spec: WASM GUI desktop — from software-rendered frame to native surface

Living spec (v4 — M8 split into per-component sub-milestones; changelog at bottom). Status legend:
⬜ todo · 🟡 in progress · ✅ done · ❌ blocked. Companion research:
`../../WASM-GUI-DESKTOP-RESEARCH.md`. Progress log + proof: `~/tmp/gui-progress/progress.html`.

## 1. North star & strategy

Run a Raspberry-Pi-class Linux GUI desktop where the GUI software is **cross-compiled from
source to `wasm32-wasip1`** (our toolchain family) and **rendered on a native host surface**
(the runtime owns a `winit`/`softbuffer` window; browser later). Research verdict drives the
sequencing:

- **Software rasterization → framebuffer → host blits it.** No GL/EGL/GPU in the guest. The
  guest computes pixels with ordinary compute + WASI; frames cross the sandbox boundary; the
  native host displays them. This single data path is the spine of every later milestone.
- **X11 over Wayland for the first real desktop** because core X11 runs entirely over a socket
  and `MIT-SHM` is optional with a clean `XPutImage` fallback — so we avoid implementing shared
  memory. (Wayland's `wl_shm` has no fallback.)
- **Avoid `dlopen` and shared memory.** Static-link everything; keep multi-process boundaries at
  the socket/module level. Threads are acceptable later (`wasm32-wasip1-threads`) but the first
  milestones are single-threaded.

## 1a. Strict constraints (NON-NEGOTIABLE)

1. **We compile the GUI software to wasm ourselves**, from source, with our own toolchain
   (`wasm32-wasip1` / wasi-sdk family). No off-the-shelf pre-built wasm GUI ports.
2. **The process that executes the guest AND renders the GUI is a native app built on the
   STANDARD secure-exec Rust client** (`crates/secure-exec-client`). That app:
   - drives the guest wasm **through the real secure-exec runtime (V8 sidecar)** — the guest runs
     inside secure-exec, exactly like any other guest workload;
   - pulls frames out via the client and blits them to a native window it owns;
   - injects input back into the guest through the client.
   **Forbidden in the execution/render path:** `wasmer`, `node:wasi`, the TypeScript client, or any
   `Command::new`-style direct spawn. If it runs guest pixels, it goes through secure-exec via the
   Rust client. (The M0 spike that used `wasmer`/`node:wasi` is **superseded** by this rule — it
   stays only as evidence the renderer is deterministic; it is not the product path.)
3. Everything is **end-to-end and automatically tested**, with a **manually runnable example**, and
   the spec + `~/tmp/gui-progress/progress.html` are kept current with proof/screenshots **as we go** —
   every milestone (including the M7.5 wasm-threads runtime + Phase-0 build milestones) gets a dated
   `progress.html` entry with proof (passing test output / built-artifact sizes / a screenshot under
   `~/tmp/gui-progress/`) **in the same change that lands it**. Do not let it go stale.
4. **Build observability tools that parallel native debugging** (see `INTERNAL-TOOLING.md` for the catalog + checklist) — when stuck, ask "what would I do on a
   native host?"** Most of the hard time on this project is not fixing bugs, it is *seeing* them: the
   guest runs as wasm inside a sidecar with none of the native toolkit. So when a milestone is blocked
   on a hang / crash / wrong behavior, first name the native technique you'd reach for (`gdb bt`,
   `strace -f`, `/proc/<pid>/wchan`, `GDK_DEBUG`/`GTK_DEBUG`/`GDK_SYNCHRONIZE`, `xtrace`, `ltrace`,
   `perf`), then ask **can we build a parallel tool for the secure-exec/wasm runtime?** If yes, build it
   — it is reusable, it pays for itself immediately, and developing it is an expected part of the work,
   not a detour. If there is no reasonable parallel, don't over-invest — fall back to ad-hoc probes and
   move on. Prefer general, reusable instrumentation over one-off `fprintf`-and-rebuild bisection.
   High-leverage parallels already identified (build the cheap, high-payoff ones first):
   - **`gdb bt` → on-demand guest stack dump.** Guests are wasm built with `-g` (DWARF) in V8 isolates;
     a `--dump-stacks-on-timeout` that walks each isolate's wasm frames and symbolizes via the `.wasm`
     DWARF turns a multi-hour bisection into one line. The X server's `XMARK:` prints are a hand-rolled
     version of this.
   - **`strace -f` → a sync-RPC trace.** Every guest↔sidecar call (net.poll/net.send/fd_read/
     thread-spawn/kernel ops) funnels through one dispatch point; a `SECURE_EXEC_TRACE` logging
     `[pid] rpc args → ret (Nms)` per process exposes cross-process liveness (e.g. "client loops
     net.poll while the X server does nothing" = a scheduling deadlock). Highest ROI; build first.
   - **`top`/`/proc` + scheduler view → process-table + pump-decision dump.** The sidecar owns the
     `ActiveProcess` table + pump; dumping per-process run-state / `last_pumped_at` and the pump's
     per-cycle "advanced X / yielded because Y" makes scheduling/liveness bugs visible.
   - **app debug env → env passthrough to guests.** Nearly free (`--client-env KEY=VAL`); unlocks GTK's
     own `GDK_DEBUG`/`G_MESSAGES_DEBUG` and `GDK_SYNCHRONIZE` (which pinpoints a hanging X call).
   - **`xtrace` → built-in X wire tracer.** The X transport *is* the kernel socket table, so the sidecar
     can optionally log X11 request/reply/error bytes between client and server — distinguishes
     "request not sent" vs "reply not produced" vs "reply not read."

## 2. Hard constraints (from runtime survey + research, verified against the codebase)

| Constraint | Source | Consequence |
|---|---|---|
| Guest is `wasm32-wasip1`, executed in **V8** over a sync-RPC bridge | runtime survey, confirmed `crates/execution/src/wasm.rs`, `node_import_cache.rs` (`new WASI({version:'preview1'})`) | host engine for fidelity = V8 family. **Note:** the secure-exec WASM runner is its *own* JS-polyfill WASI over sync-RPC, **not** stock `node:wasi`. So the M0 `node:wasi` harness is a *proxy* for the V8 family; true product-path parity is deferred to **M5**. |
| No shared memory / mmap / dlopen / threads exposed to guests | runtime survey, confirmed `registry/native/crates/wasi-ext/` exposes only `host_process`/`host_net`/`host_user` | M0 uses none of them. Each is a later, explicit milestone. |
| AF_UNIX (path sockets) + TCP work | confirmed `wasi-ext` `host_net` supports `/path` sockets | X11 wire transport feasible (later). |
| No GPU / framebuffer / native window exists yet | runtime survey | native surface is greenfield; built host-side in Rust. |
| `mmap` "unsupported" claim is **overstated** — must be measured | research (refuted claim) | M1 spike measures real mmap before font/fbdev work. |
| This dev box is **headless** (no DISPLAY) + only `wasmer` CLI, `node`, `ffmpeg`, `cargo` present (no `wasmtime`/`clang`/`emcc`/Xvfb) | environment probe | automated proof = raw framebuffer → PNG via `ffmpeg`; two engines = `node:wasi` + `wasmer` CLI (no heavy Rust wasm-engine build); windowed host is the user's manual demo. |

**Trust-model constraint (from root `CLAUDE.md`, boundary = sidecar ↔ executor):** anything that
parses untrusted guest traffic (the X server, toolkit, app) runs **in the executor as a guest**.
The new trusted host code is *only* the native-surface shuttle, which must do **no protocol
parsing** — it blits an opaque pixel buffer and forwards input events. This keeps the TCB minimal.

## 3. Architecture (the product path — secure-exec Rust client end to end)

```
┌─ guest GUI app (wasm32-wasip1, compiled by our toolchain) ───────────────┐
│  software-renders RGBA frames; speaks frame protocol v0 over WASI fds      │
│  (later milestones: a real toolkit, then an X server + WM as guests)       │
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   │  runs INSIDE secure-exec
                    ┌──────────────┴───────────────┐
                    │  secure-exec sidecar (V8)     │  ← the real runtime; NOT wasmer/node:wasi
                    │  WASM exec + VFS + sockets     │
                    └──────────────┬───────────────┘
                                   │  secure-exec WIRE protocol (stdio)
┌──────────────────────────────────┴─────────────────────────────────────────┐
│  NATIVE HOST APP (Rust) on the STANDARD secure-exec Rust client             │
│  crates/secure-exec-client:                                                  │
│   • spawn/connect sidecar, create VM, load guest.wasm, start WASM exec       │
│   • pull frames out (process stdout stream / VFS read) → decode protocol     │
│   • own a winit+softbuffer window, blit frames                               │
│   • inject keyboard/mouse via the client back into the guest                 │
└──────────────────────────────────────────────────────────────────────────────┘
```

The frame protocol is transport-agnostic; what changes per milestone is only what the *guest* is
(hand renderer → real toolkit → X server + WM). The guest's "render pixels with no host graphics"
contract and the "execute+render via the secure-exec Rust client" contract never change.

**Superseded M0 spike (kept as evidence only):** the original two-engine path (`node:wasi` +
`wasmer`, SHA-256 byte-equality, ffmpeg PNG) proved the renderer is deterministic and the data path
works. It violates §1a.2 (uses non-secure-exec runtimes), so it is **not** the product path — the
Rust-client host below replaces it.

## 4. Milestones

- **M0 — Framebuffer renderer + data path (SPIKE, superseded).** Rust→`wasm32-wasip1` guest
  software-renders a deterministic desktop-looking frame; proven byte-identical under two engines;
  golden-pixel + cross-engine tests; PNG proof. ✅ **done as a spike** — but it executes via
  `node:wasi`/`wasmer`, which **§1a.2 forbids** for the product path. Kept only as evidence the
  renderer + protocol work. Superseded by M1.
- **M1 — Rust-client native host (THE foundation).** A native Rust app on
  `crates/secure-exec-client` that: builds/locates + launches the sidecar, creates a VM, runs
  `guest.wasm` **through secure-exec (V8)**, reads the framebuffer back via the client (chunked
  PREAD), and blits it in a `winit`+`softbuffer` window (with `--capture` headless mode for
  automated proof). This **replaces wasmer/node:wasi entirely**. ✅ **done** — `./tests/run.sh`
  green; the frame rendered by the guest *inside the real secure-exec sidecar* is byte-identical
  (SHA-256) to the spike output and passes golden pixels; window host compiles with `--features
  window`. Host is a member of the repo root workspace (shares `Cargo.lock` with the sidecar);
  guest stays a standalone wasm workspace. Notes: the sidecar loads the wasm from the trusted-client
  HOST path given as `entrypoint`; the VM is created with an allow-all fs/process policy (trusted
  config); the client must use the sidecar-ALLOCATED `connection_id` from the auth response.
- **M2 — mmap reality spike.** ✅ **done.** Findings (verified by `probes/mmap-probe.c` compiled
  with the vendored wasi-sdk and run *through secure-exec* via the M1 host):
  - Rust's `wasm32-wasip1` self-contained libc defines **no** `mmap`; wasi-sdk's `sys/mman.h`
    `#error`s by default ("WASI lacks a true mmap").
  - wasi-sdk ships an opt-in emulation: **`-D_WASI_EMULATED_MMAN -lwasi-emulated-mman`** (mmap over
    malloc + pread). With it, **anonymous mmap (rw)** and **file-backed `MAP_PRIVATE` read** both
    work inside secure-exec — no host mmap implementation needed (the runtime already serves the
    underlying pread/file ops).
  - Limitation: no `MAP_SHARED` write-back, no cross-process shared memory (consistent with the
    "no shared memory" constraint). Fine for single-process toolkits and read-only font access.
  - **Consequence for M3:** freetype/fontconfig font `mmap` is satisfiable via the emulation; no
    stream-I/O patch strictly required. The `_WASI_EMULATED_MMAN` flags must be in the toolkit build.
  - Bonus: this proved the **C-source → wasm32-wasip1 (wasi-sdk clang) → run-in-secure-exec**
    pipeline end-to-end, which is exactly the M3 build path.
- **M3 — Real framebuffer-native toolkit (Nuklear).** ✅ **done.** A pre-implementation design
  review established that FLTK/Tk cannot software-render without X (they'd need a from-scratch
  screen/graphics driver — weeks of authoring, not a cross-compile), so they belong *after* the X
  server (see M4b). M3 is instead a real, standard, framebuffer-native toolkit: **Nuklear**
  (single-header immediate-mode GUI shipping a software RGBA backend; no X/GL/dlopen/threads, no
  font files). Real third-party widgets (window chrome, buttons, checkbox, radio buttons, slider,
  progress bar, labels, a second window) software-rasterized and run **inside secure-exec** via the
  M1 host. Cross-compiled from source with the vendored wasi-sdk (`scripts/build-nuklear.sh`).
  Tested end-to-end (`tests/run-nuklear.sh`): header + golden-pixel checks, exact frame size,
  regression-proof. Reuses the M0 `SXFB` protocol and the M1 host with zero host changes.
- **M4 — X server to wasm (target pivoted to `Xvfb`).** 🟡 **in progress (frontier — never done by
  anyone per the research).** Findings + concrete progress:
  - **Target pivot:** modern Xorg **dropped `Xfbdev`** (kdrive now only ships Xephyr, which needs a
    host X server). **`Xvfb` (the virtual-framebuffer X server, `hw/vfb`, `-Dxvfb=true`) is the
    right target** — it renders into an in-memory framebuffer with no fbdev device and no input
    hardware, matching our blit-to-host model directly. Update M4 to Xvfb.
  - **Toolchain proven:** meson+ninja installed; wasi-sdk **meson cross file**
    (`toolchain/wasi-sdk-cross.ini`) and the wasi-sdk **CMake** toolchain both work for X-stack C.
  - **Five X-stack components cross-compiled/installed to wasm** in `third_party/wasm-prefix`:
    `pixman` (`libpixman-1.a`), `freetype` (`libfreetype.a`), `xorgproto` (headers + all proto
    `.pc`s), `xtrans` (source headers + `.pc`), `libXau` (`libXau.a`, `XauReadAuth`). The meson
    cross file now declares `pkg-config` + a wasm-only `pkg_config_libdir`, so cross dependency
    resolution works.
  - **New constraint discovered:** C libs using **setjmp/longjmp** (freetype, likely the xserver)
    need `-mllvm -wasm-enable-sjlj` at compile time + an EH-capable engine (V8 qualifies).
  - **Seven components now on wasm:** + `util-macros` (autoconf macros) and `xcb-proto`
    (codegen data) installed; autotools cross-compile env wired (CC/AR/RANLIB/CFLAGS with
    `--host=wasm32-wasi`).
  - **xserver configure runs** (past all compiler checks) and needs `x11` (libX11 → libxcb →
    xcb-proto ✓ + libxdmcp), `xfont2`, libxkbfile/font-util, then the `os/` core.
  - **BREAKTHROUGH — the socket wall is solved by the patched sysroot.** Vanilla wasi-libc lacks
    `recvfrom`/`sendto`/etc., but the repo's **patched wasi-libc sysroot** (`registry/native/c/sysroot`,
    built by `patch-wasi-libc.sh`) provides them, backed by secure-exec's `host_net` imports. Building
    the X stack with `--sysroot=registry/native/c/sysroot` gives it working POSIX sockets in-sidecar.
    The toolchain (meson cross file + autotools `--host=wasm32-wasi` env) now targets that sysroot,
    plus a force-included `toolchain/wasi-compat.h` (no-op `flockfile`/etc.).
  - **THE ENTIRE X CLIENT + FONT STACK NOW CROSS-COMPILED TO WASM (14 components):** pixman,
    freetype, zlib, xorgproto, xtrans, libXau, util-macros, xcb-proto, libxdmcp, libxcb, **libX11**
    (`XOpenDisplay`), libfontenc, libXfont2, libxkbfile. Small per-lib patches were needed and are the
    pattern for the rest: disable xtrans TCP/local transports (`inet_addr`/`sys/wait.h`), patch
    `ioctl(FIONREAD)`→`poll()` in libX11, no-op stdio locking. **libX11 done means M4b (FLTK/Tk/twm as
    X clients) is now unblocked too.**
  - **THE X SERVER (Xvfb) IS CONFIGURED AND COMPILING TO WASM.** 16 X-stack libs now on wasm (added
    libxcvt, libXext, libsha1 [a tiny vendored SHA1], plus a meson `clang` wrapper that strips
    ELF-only linker args wasm-ld rejects, and stub `sys/wait.h`/`net/if.h` + no-op
    `flockfile`/`getpgrp`/`setpgid`/`umask`/`pthread_sigmask`/`uname` in
    `toolchain/wasi-compat.{h,c}`, and `-D_WASI_EMULATED_PROCESS_CLOCKS` for getrusage). `meson setup`
    succeeds ("Build targets: 30") and `ninja` compiles **~118/314 objects**, advancing as each
    `os/`-layer POSIX gap is shimmed (patched `os/access.c` utsname guard; `utils.c` is the current
    edge — `struct rlimit`/`RLIMIT_CORE` feature-macro + wasi's pointer `clockid_t`).
  - **Remaining (finite, well-understood — but more than one session):** finish the `os/`-layer
    compile shims, link the `Xvfb` wasm binary (will surface more host-function stubs), supply an XKB
    keymap (or run `-noxkb`), then RUN Xvfb inside secure-exec listening on an AF_UNIX `/tmp/.X11-unix`
    socket, wire its framebuffer out through the M1 host, and connect a client (M4b) + WM (M5).
  - **🎉 `Xvfb` IS NOW A WASM BINARY.** The full X.Org Xvfb server **compiled and linked to
    `wasm32-wasip1`** — `experiments/wasm-gui/Xvfb.wasm` (8.25 MB, valid `wasm32` module). 314/314
    targets; all os/-layer POSIX gaps shimmed (utsname guard, `~0L`→pointer `clockid_t` cast,
    `struct rlimit`/`RLIMIT_CORE`, `-D_GNU_SOURCE`); final link fixed by stripping `-pthread`/
    `--start-group`/`-rpath`/`-ldl` in the clang wrapper (wasi is single-threaded; pthread stubs come
    from libc). **This is the thing the research said nobody had ever done.** It imports only 9
    standard WASI functions (args/fd/proc_exit) — a clean surface secure-exec provides.
  - **`Xvfb` cross-compiled+linked to wasm and INSTANTIATES + EXECUTES in secure-exec** (verified:
    valid 12 MB `wasm32` module; imports only standard WASI + secure-exec's `host_net`/`host_process`/
    `host_user`; loads in the V8 sidecar and runs for seconds with no instantiation/trap error).
    Getting it to instantiate required, in the runner: a no-op `sock_shutdown` + a full **`net_poll`**
    in `host_net`; and at link: forcing `__main_argc_argv` (wasi crt only weak-refs main → GC'd) +
    appending libfontenc/freetype/**libsetjmp** (freetype's setjmp needs `libsetjmp.a`).
  - **Verified init progress (via stderr-streamed markers — the reliable probe):** Xvfb runs through
    `ProcessCommandLine` → **full `OsInit`** → **`CreateWellKnownSockets` succeeds (it CREATES A
    LISTENING AF_UNIX SOCKET — the X server is listening for clients in the sandbox)** → `InitOutput`
    / **screen init succeeds at depth 24** → enters the screens loop → **traps in `CreateRootWindow`**.
  - **Real fixes that got it this far (all needed):** run with `-nolock` (the X display lock dance
    loops on wasi); `-listen local` (modern Xorg defaults AF_UNIX transports to NOLISTEN); disable
    xtrans **abstract sockets** (host_net only does path sockets — exactly the research's call);
    patch `trans_mkdir` to skip the root/sticky-bit ownership dance; **patch xtrans's
    `fd >= sysconf(_SC_OPEN_MAX)` check** (host_net fds are intentionally `0x40000000+`, and the
    server uses `poll()` not `FD_SET`); depth **24** not 32; runner gained `sock_shutdown` + `net_poll`.
  - **Function-pointer-cast wall (SOLVED):** `CreateRootWindow`/`SetDefaultFontPath` trapped with wasm
    `RuntimeError: null function or function signature mismatch` — the X server calls procs through
    **cast function pointers**, and wasm's `call_indirect` enforces exact type signatures. Solved with
    **`wasm-opt --fpcast-emu`** (binaryen's equivalent of Emscripten `EMULATE_FUNCTION_POINTER_CASTS`),
    applied as a post-link pass in `scripts/link-xvfb.sh` + a manual `wasm-opt` step.
  - **Reaching the dispatch loop (DONE):** past the function-pointer wall, the server hit XKB keymap
    compilation (needs `xkbcomp` fork/exec, unavailable on wasi) — made non-fatal in `dix/devices.c`
    and `Xext/xtest.c` (keyboard activation warns + continues). Xvfb now runs `ProcessCommandLine →
    OsInit → CreateWellKnownSockets (listening AF_UNIX socket) → screen init (depth 24) → root window →
    InitInput → **Dispatch main loop**`, blocking there serving, verified by `XMARK` stderr markers.
  - **✅ Status: the never-before-done core (real X server → wasm) fully starts and serves in its
    dispatch loop inside secure-exec.** "One X client on the native surface" (M4 goal) is proven by
    M4b below.
- **M4b — X client over AF_UNIX (DONE for raw-protocol client; toolkit next). ✅** A minimal raw-X11
  client (`guest-xclient/xfill.c`, **no libX11** — pure X11 wire protocol over POSIX sockets) is
  cross-compiled to wasm against the patched sysroot and run **as a second guest in the SAME VM** as
  Xvfb. It connects to `/tmp/.X11-unix/X0`, completes the **full X11 connection-setup handshake**,
  parses the screen reply, and draws an orange fill across the 640×480 root window
  (`CreateGC` + `PolyFillRectangle`), then a `GetInputFocus` round-trip barrier. The host reads the
  framebuffer back out (`-fbdir /data`, patched `pwrite`) → **99.8% of pixels are `0x00FF8800`**, the
  exact color the client set (proof: `~/tmp/gui-progress/m4b-xfill.png`). Run it with
  `wasm-gui-host --xdemo --server Xvfb.wasm --client xfill.wasm --fb-out frame.bin`.
  - **Sidecar/runtime fixes this required (all in `crates/execution/src/node_import_cache.rs`):**
    (1) `net_connect` now handles **AF_UNIX path** addresses (routes to the sidecar's path-based
    `net.connect` → host-backed unix socket shared across guests in one VM), stripping trailing NUL
    padding from `sizeof(sockaddr_un)`. (2) `net_accept` for unix sockets no longer calls the TCP-only
    `address:port` formatter (which threw). (3) **`net_poll` listener readiness is now accurate** (a
    buffered non-blocking accept) and **`net_accept` is non-blocking** (returns `EAGAIN`) — previously
    the optimistic listener `POLLIN` + a blocking accept busy-loop starved connected clients.
  - **X-server-side fixes (vendored copies):** `os/xserver_poll.c` `xserver_poll()` rewritten to call
    real `poll()` (routed to host_net) instead of the `fd_set`/`select()` emulation, which silently
    dropped our `0x40000000+` host_net fds (> `FD_SETSIZE`). `Xtranssock.c` `SocketRead/Write/Readv/
    Writev` rewritten to use `recv()`/`send()` (the patched libc routes only those to host_net, not
    `read`/`write`/`recvmsg`/`writev`).
  - **Next:** swap the raw client for a **stock toolkit X client** (FLTK/Tk over the already-built
    libX11) to land the "real standard toolkit" intent, and blit the framebuffer through the M1
    winit/softbuffer native window live. 🟡
- **M5 — Standard WM + multi-window desktop. ✅ DONE.** The standard X.Org window manager **`twm`**,
  cross-compiled from source to `wasm32-wasip1`, manages and decorates a real **libX11** client window,
  all running as wasm guests inside secure-exec. twm grabs `SubstructureRedirect`, receives the client's
  `MapRequest`, reparents it into a decoration frame with a **title bar** (the client's `WM_NAME`), and
  the client draws into it with real Xlib calls. Proof: `~/tmp/gui-progress/m5-twm-window.png`, test
  `scripts/test-m5-twm.sh`. The **multi-client substrate** is also proven: three raw-X11 clients composite
  on one Xvfb (`scripts/test-m5-multiclient.sh`, `~/tmp/gui-progress/m5-multiclient.png`).
  - **Full toolkit/WM stack cross-compiled to wasm:** libICE, libSM, **libXt** (Intrinsics), libXmu, plus
    a locale-enabled libX11 and the patched libxcb, then **twm** itself (`scripts/build-xlib.sh`,
    `scripts/link-twm.sh`). A real libX11 client (`guest-xclient/xwin.c`) creates+maps+draws a top-level
    window (`scripts/...`); `guest-xclient/xopen.c` is a minimal libX11 smoke client.
  - **Key fixes that unblocked the toolkit stack:** the effective wasi **`POLLOUT` is `0x2`** (POLLWRNORM),
    not `0x4` — `net_poll` was using `0x4` so libxcb's poll-for-reply never saw the socket writable and
    `XOpenDisplay` hung; a host_net **`net_set_nonblock`** import + non-blocking `net_recv` (both libxcb's
    poll helpers and the X server's multi-client dispatch require non-blocking sockets, and
    `fcntl(O_NONBLOCK)` cannot reach host_net fds); `fcntl(F_SETFD)`-failure tolerance in libxcb/twm; a
    `.twmrc` with `RandomPlacement` (twm's default placement is interactive); libXt's
    `_XtWaitForSomething` `drop_lock` signature reconciliation; host-built `makestrs`; pregenerated
    `gram.c`; newer `config.sub`.
  - **Remaining polish (not blocking):** JWM as a richer Pi-class desktop, real fonts (Xft/core-font
    files) for crisper title text, live winit blit of the framebuffer, and making concurrent libX11 init
    robust (today the client connects after twm is idle to avoid flaky concurrent startup over the single
    sync-RPC bridge). **Real, standard WMs only (no custom WM):** the original plan was twm then JWM —
  with **`twm`** (X.Org tree; deps `libX11`/`libXt`/`libXmu`; no dlopen), then **`JWM`** as the
  Raspberry-Pi-class desktop (single C binary with built-in panel/menu/systray, builds against just
  `libX11`, no dlopen; `IceWM` fallback). Avoid `openbox`/`fluxbox` (libxml2/glib/pango/cairo or
  C++/imlib2). **Build WM + first apps on X CORE FONTS (no Xft)** so clients need no
  `freetype`/`fontconfig`/`mmap`; flip Xft on after M2. Multiple guest clients over AF_UNIX, the WM
  as another guest, everything inside secure-exec, rendered by the M1 host. ⬜

### ⭐ TOP PRIORITY (2026-06-20): a RUNNABLE, INTERACTIVE desktop window

Per direct user direction, the immediate next priority — ahead of M6.3 xterm, M7, M8 — is a desktop
the user can **run and interact with using a normal cursor**: open a native window, see the live
framebuffer (clock ticking, windows), **move a real mouse cursor**, **drag windows**, click, and type.
Get this to a genuinely working, runnable state, then resume the M6→M8 sequence.

**This is milestone `M6-INTERACTIVE`.** Status (2026-06-20):
1. **Live window — DONE (cross-platform).** `host/src/main.rs` `window::run_desktop` (behind
   `--features window`, winit + softbuffer, builds on macOS AND Linux) runs the desktop (Xvfb + twm +
   xclock + a libX11 window + the XTEST agent) in one VM and streams the live X framebuffer
   (`<shadow>/data/Xvfb_screen0`, BGRX→RGBA) into a native window ~30fps. One-command launch:
   `experiments/wasm-gui/scripts/run-desktop.sh` (needs a machine with a display). The framebuffer
   read is solid (proven headlessly via the screenshot tests); the on-screen window itself can only be
   verified on a display (this dev box is headless). winit mouse/keyboard already map to the agent's
   command vocabulary (motion / buttondn / buttonup / key<keycode>), so drag/click/type are wired.
2. **Live input forwarding — SOLVED (cursor + click + keyboard).** The host speaks **X11 + XTEST
   directly** to the X server's host-backed AF_UNIX socket (`<shadow>/tmp/.X11-unix/X0`) via `x11rb`
   (`xinput::XInput` in the host), bypassing the guest entirely. winit mouse/keyboard → XTEST
   FakeInput. VERIFIED HEADLESSLY: `test-m6-input.sh` (host injects motion + ButtonPress, the libX11
   client repaints orange). Pointer motion warps the software cursor (proven). This replaced the dead
   ends: host→guest **stdin** never reaches the guest (two-pipe mismatch) and host→guest **file**
   updates aren't seen by guest re-reads (VFS-coherence gap) — both abandoned for the x11rb path.
3. **Drag windows — DONE.** Root cause was twm's `XGrabServer` during interactive move
   (`twm/src/menus.c:1477`: it server-grabs unless `NoGrabServer && OpaqueMove`); the server grab froze
   ALL other clients, so the host's XTEST motion stream never reached the server until button-up (by
   which point twm had already placed the window back). Fix: the host's `.twmrc` now sets BOTH
   `NoGrabServer` AND `OpaqueMove`, so twm does an opaque, ungrabbed move and the host's XTEST motions
   flow through and drive it. `test-m6-drag.sh` PASSES (host drags titlebar (160,71)→(430,330); the
   window's body moves from ~(42,83) to ~(312,342)). Proof: `~/tmp/gui-progress/m6-interactive-drag.png`.
   (Host-side opaque move via `ConfigureWindow` is also wired in `xinput::XInput` as a fallback for
   unmanaged windows, but twm's own move now does the work for managed windows.)

**M6-INTERACTIVE COMPLETE:** live view + real cursor + click + keyboard + drag, all host-driven via
X11/XTEST, cross-platform (winit/softbuffer on macOS + Linux). Tests: `test-m6-input.sh`,
`test-m6-drag.sh`, plus `test-m6-desktop.sh`. Launch: `scripts/run-desktop.sh`.
4. **Status:** runnable interactive desktop (`scripts/run-desktop.sh`) — live framebuffer + real cursor
   + click + keyboard, cross-platform (winit/softbuffer on macOS + Linux). Remaining: the drag/grab
   fix above; 4th-concurrent-client starvation (residual M6.4) to harden before twm+xclock+window run
   together reliably.

### GOAL (post-M5): a full, well-known desktop environment — no half measures

M1-M5 proved the stack (X server + WM + libX11, all wasm in secure-exec). The goal now is a **real,
interactive desktop running a standard, well-known project**, in three sequenced milestones. "No half
measures" is the bar: each milestone must be **genuinely interactive** (live window + real input, not a
screenshot read-back), **robust** (no "connect after the WM is idle" hack — concurrent client startup
must work), use **real fonts**, be **fully automated-tested**, and have a **manually runnable example**.

- **M6 — Make what we have REAL.** 🟡 **in progress.** Done: a real stock app (X.Org **xclock**,
  analog + digital, with libXaw/libXmu/libXt/libXrender cross-compiled), **real X core fonts** served
  by the server (`-fp /fonts`; digital-clock text + twm title text render; host installs PCF fonts via
  `--fonts-dir`), and a **robust multi-app desktop** (twm + xclock + a libX11 window, 3/3 identical
  runs). Robustness came from (a) WM-ready-gated sequential launch in the host (session-manager style,
  no in-client sleep) and (b) making clients **event-driven** (block in the X loop, draw on Expose) so
  continuous redraw traffic stops flooding the sidecar's single sync-RPC thread. Proof:
  `~/tmp/gui-progress/m6-xclock.png`, `m6-xclock-digital.png`, `m6-desktop.png`.

  **Post-wipe rebuild (2026-06-20) status:** all 26 cross-compiled X libs + Xvfb/twm rebuilt and
  M4b/M5-twm/M5-multiclient PASS again (full recovery proven). Work now runs in an isolated jj
  workspace (`/home/nathan/secure-exec-wasmgui`) with a workspace-relative toolchain. **M6.2 locale
  fix landed:** Xt apps were failing `XCreateFontSet` ("Unable to load any usable fontset") fatally
  because libX11 is built without `XLOCALEDIR` env support on wasi (no `getresuid`/`issetugid`, so
  configure leaves it disabled) and the compiled-in locale path is a host path absent in the VM. Fix:
  host `--locale-dir` (+ `scripts/prepare-locale.sh`) installs the C-locale DB into the VM at the
  exact paths libX11 has compiled in; the fontset error is gone.

  **M6.4 FIXED (2026-06-20) — robust multi-app desktop works.** Two root causes, both fixed:
  (1) **sync-RPC fairness** — `net.poll` blocked the single sidecar sync-RPC service thread up to 50ms
  (`JAVASCRIPT_NET_POLL_MAX_WAIT`), so a chatty guest (an Xt app's poll loop) starved the WM and other
  clients; lowered the cap to 3ms so the thread round-robins across guests (M5-twm/M5-multiclient still
  pass). (2) **WASM execution budget** — every WASM guest is killed at the 30s default wall-clock
  "fuel" budget (`DEFAULT_WASM_EXECUTION_TIMEOUT_MS`), so the long-running X server died ~30s in
  ("WebAssembly fuel budget exhausted") and the desktop collapsed; the host now sets
  `limits.resources.maxWasmFuel` (1h) on the trusted VM. Result: `test-m6-desktop.sh` now asserts twm
  **concurrently** managing a real libX11 window AND a stock **xclock** (live analog face), running
  past 30s, 3/3 deterministic. Proof: `~/tmp/gui-progress/m6-desktop-robust.png` (two decorated apps),
  `m6-xclock-analog.png`. Also: xclock rebuilt from 1.0.7 (analog skips XCreateFontSet); host reads the
  framebuffer from the host-backed shadow fs (a wire readback starves while guests are live).
  **Remaining for M6:** live window + input (built path pending; needs a machine with a display to
  verify — this box is headless); a terminal (xterm needs fork/exec/PTY → a kernel-PTY-spawn shim);
  Xft/fontconfig for antialiased/i18n text. Original sub-tasks:
  1. **Live rendering + input.** Stream the X server framebuffer to the M1 `winit`/`softbuffer` window
     continuously (not a one-shot PNG), and inject host mouse/keyboard back through the client as X
     input events (so you can actually click/type into the wasm desktop). Replace `xdemo`'s PNG
     read-back with a live loop.
     **INPUT half DONE (2026-06-20):** input injection through the wasm X server is proven headlessly.
     `guest-xclient/xtest-agent.c` (XTEST, libXtst) synthesizes X input events; `xinput-target.c`
     repaints green on KeyPress / orange on ButtonPress. `test-m6-input.sh` injects a ButtonPress and
     asserts the target turns orange == the event was delivered to a real libX11 client. Proof:
     `~/tmp/gui-progress/m6-input-button.png` (orange window + pointer at the injection point). The host
     drives the agent (it launches it with the injection as args, or via `--inject "<pid>=<cmd>"`).
     Note: the dynamic `--inject` (host→agent **stdin** at runtime) is currently blocked by a separate
     WASM-guest stdin-delivery gap (the guest's `fgets(stdin)` doesn't receive host `write_stdin`); the
     argv path proves the X/XTEST input chain regardless. The **live winit blit** half still needs a
     machine with a display to verify (this box is headless).
  2. **Real fonts. DONE (2026-06-20).** Cross-compiled `expat` 2.6.4 + `fontconfig` 2.14.2 +
     `libXft` 2.3.8 (+ freetype/Xrender). DejaVu/Liberation TTFs installed into the VM via the host
     `--vm-tree` (+ `/etc/fonts/fonts.conf`, `prepare-xftfonts.sh`); Xft clients get `FONTCONFIG_PATH`.
     `xftdemo` renders antialiased "DejaVu Sans-22" text, verified by `test-m6-xft.sh` (white window +
     grey antialias edges). Proof: `~/tmp/gui-progress/m6-xft-text.png`. **Key fix:** freetype's
     file-stream `FT_New_Face` returns Cannot_Open_Resource under wasi (per-access fseek+fread
     streaming misbehaves) even though `fopen`/`fread`/`FT_New_Memory_Face` all work; patched
     `src/base/ftsystem.c` `FT_Stream_Open` to read the whole font into memory and present a memory
     stream. Build helper: `build-xclient.sh`. This unblocks xterm-with-Xft and the M8 GTK stack.
  3. **Stock apps as content.** Cross-compile and run standard X apps (`xclock`, `xterm`, maybe `xcalc`)
     as guests so the desktop has real, interactive programs.
     **M6.3 PTY primitive DONE (2026-06-20):** the kernel-PTY-spawn syscall surface a terminal needs is
     implemented and proven end-to-end through the FULL wasm stack (`test-m6-3-pty.sh`). A wasm
     "terminal" guest (`guest-xclient/pty-term.c`) spawns a wasm child (`pty-shell.c`) over a real
     kernel PTY and round-trips data: writes the master, reads the line-discipline echo back. Stack:
       - kernel `open_pty_split(parent_pid, child_pid)` — allocates a PTY pair, master fd into the
         parent (terminal) fd table, slave fd into the child; unit-tested in `kernel/tests/api_surface`.
       - sidecar `configure_child_stdio` 'pty' stdio mode on `child_process.spawn` (dups the slave onto
         the child's 0/1/2, returns `ptyMasterFd`) + `__pty_read`/`__pty_write` sync-RPC handlers.
       - bridge `_ptyReadRaw`/`_ptyWriteRaw` facades (v8-bridge.source.js + `SYNC_BRIDGE_FNS` +
         v8_runtime map + wasm runner switch); `host_net.pty_spawn`/`pty_read`/`pty_write` wasm imports
         in `node_import_cache.rs` (ASSET_VERSION 72). Host `--pty-test` mode drives it.
     **M6.3 INTERACTIVE SHELL SESSION DONE (2026-06-20):** `test-m6-3-pty.sh` drives a SUSTAINED
     multi-command interactive session between two wasm guests over the kernel PTY: the terminal sends
     `echo hello`->`hello`, `ping`->`pong`, `exit`->`bye` (clean shutdown), asserting
     `PTY_CHILD_REPLY_OK`/`PTY_CHILD_PING_OK`/`PTY_CHILD_EXIT_OK`/`PTY_SESSION_OK`. `pty-shell.c` is a
     real line-oriented interpreter loop (prompt -> read line from slave stdin -> respond), proving
     repeated bidirectional terminal I/O, not a one-shot echo. (A real shell like dash/bash needs
     fork/exec/job-control, which wasi lacks; this interpreter is faithful to what a terminal emulator
     drives over the PTY.)
       - The "child never runs" gap was fixed earlier by driving the nested child with the standard
         `child_process.poll` from the `pty_read` shim (`PTY_CHILD_RAN`).
       - The "stdin never reaches the child" gap (long mis-diagnosed) is now fixed. Real root cause: the
         v8-runtime session intercepts `_kernelStdinReadRaw` IN-SESSION (`javascript.rs` ~2931) from a
         `LocalKernelStdinBridge` that, for a pty child, was never fed (its input lives in the kernel PTY
         slave). Fix: `pump_pty_child_stdin` (`crates/sidecar/src/execution.rs`) drains the kernel PTY
         slave (fd 0) on each `poll_descendant` tick and forwards it to `child.execution.write_stdin`,
         mirroring the pipe-child stdin pump. Sidecar-side only, no runner/asset change. See
         `M6.3-FINDINGS.md` (the earlier "two-pipe mismatch"/"node:wasi fd_read" theories were wrong —
         corrected there). Regression: sidecar lib+service suites (1 pre-existing unrelated loopback-port
         test aside) + M1–M7 + `test-m6-3-pty.sh` green.
     **M6.3 REAL TERMINAL EMULATOR DONE (2026-06-20):** the suckless terminal **`st` 0.9.2**,
     cross-compiled from source to `wasm32-wasip1` (`scripts/build-st.sh`), runs as a wasm guest in
     secure-exec, spawns the wasm shell (`/pty-shell.wasm`) over a real kernel PTY, and renders the
     shell's terminal output via Xft to the wasm X server. `scripts/test-m6-3-st.sh` asserts the
     framebuffer shows the shell's prompt as antialiased Xft text; proof PNG `~/tmp/gui-progress/m6-3-st.png`
     (shows `wsh ready` + `$` prompt + cursor). Integration: st's forkpty/openpty + select/read/write PTY
     backend is replaced by `third_party/st/wasmpty.c` (host_net `pty_spawn`/`pty_read`/`pty_write`); the
     `pselect` event loop in `x.c` becomes a non-blocking poll loop (ttyread via pty_read + XPending +
     nanosleep idle throttle); `stty`/`termios`/`TIOCSWINSZ` neutralized (wasi has no termios). Host
     `run_xdemo` gained `--pty-shell` to install the child shell into the VM. The terminal emulation core
     + Xft rendering compile unchanged. A real shell (dash/bash) needs fork/exec, which wasi lacks, so
     the child is our line-oriented interpreter `wsh` (pty-shell.c).
     **X KEYBOARD DONE (2026-06-21):** the wasm X server now has a working keyboard device, so
     host-driven KeyPress/XTEST events are delivered AND translated to characters for real libX11
     clients. `test-m6-keyboard.sh` asserts a host-injected key repaints the input-target green
     (`green=39888`). Root cause it fixes: wasi has no fork/exec, so Xvfb cannot run `xkbcomp` to compile
     a keymap (its keyboard device never activated -> "XTest keyboard not activated" -> all key events
     dropped). Fix: compile a US keymap on the HOST (`scripts/prepare-xkb.sh` -> `/xkb/default.xkm`, a
     standard `.xkm`), install it in the VM (`--vm-tree`), and patch the server's `XkbCompileKeymap`
     (`third_party/xserver/xkb/ddxLoad.c`) to load it directly via `fmemopen` instead of forking xkbcomp.
     KEY GOTCHA: VFS-backed files don't support per-access `fseek`/`fread` streaming under wasi (the same
     limitation freetype hit), and `XkmReadFile` fseeks to each section offset, so the file must be
     slurped into memory and read via `fmemopen`. Also: host `--inject "host=focus"` sets X input focus
     (window-under-pointer + PointerRoot) so keys reach a client when no WM owns focus; st builds with
     XIM disabled + core `XLookupString` (no XIM server exists under wasi, and an XIC makes
     `XFilterEvent` swallow every KeyPress). Xvfb must be re-`wasm-opt --fpcast-emu`'d after relinking
     (link-xvfb.sh does not do it).
     **Live typing into st (MAJOR CORRECTION 2026-06-21): input path PROVEN; remaining gap is a
     timing-sensitive render/pump stall, not a recv bug.** The previous multi-pass investigation logged
     here concluded "st never receives KeyPress / client-side host_net recv bug". THAT CONCLUSION WAS
     WRONG -- it was an artifact of the twm click-to-focus path plus hot-path instrumentation that
     perturbs the very timing under test. With direct input focus (`--inject "host=focus"`) and the new
     `type` inject command, st RELIABLY receives keystrokes. Verified with NON-perturbing counters
     (dumped once per activity tick, read from the VM shadow /data) for a "ping" + "echo hello" type
     sequence: kp=16 (every injected key delivered to st), XLookupString translated them, st wrote all 16
     bytes to the kernel PTY (wr=16), read 48 reply bytes back (rd=48), and its terminal MODEL absorbed
     them (termglyphs jumps from ~11 to 33). So the full input chain works: host XTEST -> wasm X server
     -> st kpress -> XLookupString -> kernel PTY -> wasm shell -> reply -> st terminal model.
     THE ACTUAL REMAINING GAP is downstream of input, in st's RENDER/pump path: st's X DRAW requests
     after the initial frame do not reliably reach the server's framebuffer. Proven server-side
     (non-perturbing counter in vfb's BlockHandler): the block handler flushes the framebuffer file 679x
     during a run, yet a 220x220 green rectangle st draws DIRECTLY to its window after the first frame
     yields ~0 green pixels in the server's own framebuffer -- so the server is not drawing st's later
     requests. By contrast the minimal `xftpoll-target.c` (same poll loop, Xft, pty_spawn) fills its
     window green and that DOES reach the framebuffer. The difference is load: st emits dense Xft/pixmap
     traffic, and after enough of it the X server stops servicing st's connection, so st blocks in its
     draw/X path, stops draining the PTY, the shell blocks writing its reply, and never reads the next
     command. Net effect: sustained end-to-end typing is FLAKY -- a short first command often reaches the
     shell, longer/later ones stall (independently verified by a /data/shell_in.txt sentinel the wasm
     shell writes for every command line it receives).
     META-FINDING (reconfirmed): this is genuinely timing-sensitive. Hot-path file/ErrorF instrumentation
     in st's loop perturbs it enough to break even the previously-working case, and outcomes vary
     run-to-run with identical binaries. A `net_poll` level-triggered-refill change in the runner was
     tried and REVERTED -- measured with the shell sentinel it made typing strictly WORSE (0/5 runs
     landed any command vs the baseline's flaky first-command), because the extra per-poll sync-RPC ADDS
     to the very service-thread contention that is the root cause. So the fix DIRECTION is now known:
     REDUCE sync-RPC overhead / add input-vs-render scheduling fairness for a heavy client over the
     single sync-RPC service thread (same class as M2.3/M6.4 for net.poll), NOT add poll refills and NOT
     a delivery-function or recv fix. Also confirmed: the guests run autonomously inside the sidecar
     (the host does not pump guest execution -- it only receives wire-event notifications), so the
     earlier "host stops pumping during inject" theory is also wrong. The X server, focus, keymap,
     XLookupString, kernel PTY, and terminal model are all verified correct. WHAT IS RELIABLE AND SHIPPED: host keyboard input reaching a real libX11 client
     (`test-m6-keyboard.sh`), st rendering the shell prompt (`test-m6-3-st.sh`), deterministic
     terminal<->shell PTY round-trips (`test-m6-3-pty.sh`), and the `type <string>` host inject command
     (maps ASCII to keycodes via the server's keyboard mapping) for manual typing. No flaky end-to-end
     typing test is shipped.
  4. **Robust concurrency. DONE (2026-06-21).** Concurrent libX11 init over the single sync-RPC bridge
     now works without the settle/ordering hack: host `--xdemo --concurrent` launches every client at
     once (no per-client settle gating), and `scripts/test-m2-3-concurrent.sh` starts twm + xclock +
     xftdemo SIMULTANEOUSLY -- twm reaches its event loop and decorates both concurrently-launched
     windows, xftdemo opens its Xft font, 0 clients crash, and the framebuffer shows the managed desktop
     (proof `~/tmp/gui-progress/m2-3-concurrent.png`). 5/5 reliability runs: 0 failures, consistent
     render. The enabling fix was M6.4's net.poll fairness (JAVASCRIPT_NET_POLL_MAX_WAIT 50ms->3ms),
     which removed the bridge starvation that made concurrent init flaky; this milestone adds the
     concurrent launch path + test to lock it in. (Note: the WM is somewhat slower to finish decorating
     under concurrent contention than when launched first, but it is reliable, not flaky.)
  Acceptance: a live window showing twm managing xterm + xclock, typing into xterm works, all started
  concurrently, with an automated test + manual example.

- **M7 — JWM: a complete lightweight desktop from one standard project. ✅ DONE (2026-06-20).**
  Cross-compiled **JWM 2.4.6** (Joe's Window Manager) to `wasm32-wasip1` against our libX11 + libXft +
  fontconfig + freetype + libXrender + libXmu (configure: Xft+Xrender+Icon on; xinerama/png/jpeg/cairo/
  rsvg off). Two missing libc symbols stubbed in `wasi-compat.c` (`setsid`, `tzset`). Runs as the
  desktop shell in one VM: it **decorates client windows** (titlebar + minimize/maximize/close
  buttons), renders a **bottom panel/taskbar** with a window list and a **live clock**, and a root
  menu. `test-m7-jwm.sh` PASSES (panel renders at the bottom; JWM manages a real libX11 window). Proof:
  `~/tmp/gui-progress/m7-jwm.png` (decorated window + taskbar entry + clock "00:44:34"). Config staged
  via `prepare-jwm.sh` (`.jwmrc`). Build: `scripts/build-xclient`-style link in
  `third_party/jwm` → `jwm.wasm`. So a brand-name lightweight desktop shell runs entirely in wasm,
  rendered by the native Rust client and driveable by the host's X11/XTEST input.

- **M7.5 — Multi-threaded WASM runtime (wasi-threads). ⬜ HARD PREREQUISITE FOR M8.** GTK is blocked on
  threads (GLib spawns a worker thread on init). This is a **production runtime feature** specified in
  full in **[`WASM-THREADS-SPEC.md`](./WASM-THREADS-SPEC.md)** — shared-memory build, a `wasi.thread-spawn`
  host over a Node `worker_threads` pool, a multi-channel sync-RPC bridge with a per-VM serialization
  lock, kernel-owned WASI state, and a conformance + race + flake test suite. **M8 may not start until
  the threads Definition of Done (that spec §9, including the human TCB security sign-off) is green.**

- **M8 — A brand-name GTK desktop environment (LXDE).** ⬜ The big one (multi-week; NOT done — only the
  M8.0 spike is done). **Split into per-component sub-milestones M8.0–M8.5; see "M8 decomposition" below
  for the build order, the cross-cutting specs, and the debugging tools to build first.**
  **Was blocked on M7.5 (`WASM-THREADS-SPEC.md`); threads DoD is green and the GTK runtime now works.**
  Cross-compile the GTK stack (`GLib`/`GObject`/`Pango`/`Cairo`/`GdkPixbuf`/`harfbuzz`/`fontconfig`/
  `freetype`) and resolve the wasi blockers (`dlopen` for modules/themes — static-link or shim; `dbus`
  session bus; threads). **Spike first:** get a single GTK3 app (`gtk3-demo`) running on our X server to
  prove the stack before committing. Then build up to **LXDE** (lighter, GTK2/openbox/lxpanel/pcmanfm)
  or **XFCE** (xfwm4/xfce4-panel/thunar). Acceptance: the named DE's shell (panel + menu + a file
  manager or settings app) running live + interactive, automated test + manual example.
  **Foundational FFI blocker — keystone UNBLOCKED via a secure-exec-native path (2026-06-21):** GObject
  needs libffi, which has no wasm32-wasi port (wasm has no runtime trampolines). The previous finding
  called this "blocked, no pure-wasi solution." That is corrected: our guests run in the V8 sidecar, so
  `ffi_call` is implementable as a host import that uses V8's WebAssembly reflection to call a guest
  function by `__indirect_function_table` index with dynamically-typed args. **PROVEN** by
  `scripts/test-m8-ffi-spike.sh` (PASS): `host_net.ffi_call` + `guest-xclient/ffi-spike.c` call three
  functions purely by pointer with a runtime-built arg list (i32/f64/pointer). The other primitive,
  `ffi_closure` (runtime callbacks), is ALSO proven — `scripts/test-m8-closure-spike.sh` (PASS) — on
  pure wasm via a trampoline pool (no host import, no engine flag); a generic-signature closure would
  need V8's `WebAssembly.Function`, which is a core-engine flag deliberately left untouched. See
  M8-FINDINGS.md. A real libffi-ABI shim exists (`libffi-wasm/`) and — the decisive result —
  the **FULL GLib stack now CROSS-COMPILES to wasm32-wasip1** (`scripts/build-glib-stack.sh`,
  reproducible): GLib 2.78.4 configures (resolving `dependency('libffi')` against the shim, plus
  cross-compiled PCRE2, an intl stub, resolv/socket/grp stubs, emulated pthreads — the threads answer,
  no `-threads` target needed), and `libglib-2.0.a` (4.5 MB), `libgobject-2.0.a`, `libgthread-2.0.a`,
  `libgmodule-2.0.a`, and `libgio-2.0.a` (10.7 MB) all build. `libgobject-2.0.a` references
  `ffi_call`/`ffi_prep_cif` (gclosure.c/gmarshal.c compiled against the shim) — proof the libffi dead
  end is gone in a REAL GObject build. GIO needed compat `<sys/socket.h>`/`<netdb.h>`/`<grp.h>` + two
  small wasi stub patches (sin_zero, gunixmounts), all reproducible. The GTK **rendering stack** is now
  in flight (`scripts/build-gtk-deps.sh`): **libpng**, **fribidi**, **harfbuzz** (C++), **Cairo** (8 MB,
  xlib backend), and **Pango** (pango/pangocairo/pangoft2) all cross-compile — C++ works via wasi-sdk
  libc++ + `-wasm-enable-sjlj`; cairo needed compat sys/ipc.h+sys/shm.h (XShm stubs), pango needed
  flockfile decls + fontconfig/X-proto .pc fixes. **GdkPixbuf + ATK + GTK 3.24 ITSELF now cross-compile**
  (`scripts/build-gtk3.sh`): `libgtk-3.a` (41 MB, 506/507 TUs) builds, plus the X libs GTK needs
  (Xrandr/Xcursor/Xcomposite/Xdamage), a stub atk-bridge, and host code-gen tools (gdbus-codegen wrapped
  to our GLib 2.78.4 module). A GTK 3 app (`guest-xclient/gtk-hello.c`) now LINKS into a single wasm
  guest (`scripts/build-gtk-app.sh`, ~15 MB) and RUNS on the wasm X server: GLib/GObject init + the GDK
  X11 backend connects and sets up the display/screens/devices/seats.
  **✅ M8 "spike first" step DONE + the gtk_init runtime blocker CLEARED (2026-06-22). M8 ACCEPTANCE
  (a brand-name LXDE/XFCE DE shell: panel + menu + file manager/settings, live + interactive, with an
  automated test) is NOT yet met — this is the spike, not M8.**
  Proof of the spike: `~/tmp/gui-progress/proof-m8-gtk-window.png` — a live GTK 3 window with a label
  ("Hello from GTK 3 on wasm32-wasip1") and a themed "Click me" button, painted via cairo through the
  wasm X server (`Xvfb.wasm`); `gtk-hello` runs the full path `gtk_init` -> `gtk_widget_show_all` ->
  draw signal (cairo paint) -> `gtk_main` -> clean exit. The two `gtk_init` runtime hangs are RESOLVED,
  and the fixes are **entirely in the guest's vendored X libraries; the TCB (sidecar `crates/`) is
  unchanged, so there is NO security-boundary delta** (surfaced for human confirmation; nothing to
  self-approve). Root cause + fix (both stem from building libxcb/libX11 with the wasi-threads ABI while
  the guest runs single-threaded, so the threaded lock/cond machinery has no sibling thread to
  signal/contend):
    1. **Lost-wakeup cond deadlock at `XOpenDisplay`** (the original blocker): libxcb's and libX11's
       infinite `pthread_cond_wait`s (socket-handoff + reply-notify paths) park forever when an
       intra-process broadcast is missed. Fix: route them through MONOTONIC `pthread_cond_timedwait`
       with a 4ms bound (`_xcb_cond_*` in `libxcb-threads/src/xcb_conn.c`; `_X_xcond_*` in
       `libX11-threads/src/locking.c` via `xorgproto`'s `Xthreads.h`), so a missed wakeup degrades to
       bounded latency. CLOCK_REALTIME is frozen in the runtime, hence MONOTONIC.
    2. **Mutex self-deadlock at `XRRGetOutputInfo`** (`init_multihead`/`init_randr15`): libX11's
       display-lock `LockDisplay` re-enters on the one thread (no real contention), self-deadlocking a
       non-recursive mutex. Fix: make X mutexes RECURSIVE (`_X_xmutex_init_recursive`, same files).
  Verified: the `xinitthreads-probe` (XInitThreads + round-trips) passes, and `gtk-hello` renders with
  no regression. Diagnosis used the prior session's `SECURE_EXEC_TRACE` (sync-RPC strace) + `/proc`
  thread-state reads + sidecar-side `net.write`/`net.poll` byte counters (decisive: writes frozen =
  deadlock-not-latency; zero client RPCs at the hang = parked on an in-guest futex = mutex, not a
  reply wait). The remaining M8 ambition (a full brand-name DE: LXDE/XFCE shell with panel + menu +
  file manager, live + interactive) is still open and builds on this now-working GTK runtime.

### M8 decomposition — per-component sub-milestones (target: **LXDE**, GTK3 ports)

**DE choice: LXDE, not XFCE.** Rationale: LXDE is lighter and largely dbus-optional, whereas XFCE has a
*hard* dbus dependency (`xfconf` is the config store for `xfwm4`/`xfce4-panel`/`thunar`) plus `garcon`,
`exo`, `libxfce4ui`, `libxfce4util` — a much deeper closure to port and a session-bus daemon we do not
want in-sandbox. LXDE's pieces (`openbox`, `lxpanel`, `pcmanfm`) talk to `libfm`/`menu-cache` and read
freedesktop files from the VFS; config is keyfiles, not a bus. Build the **GTK3** ports of the LXDE
pieces (lxpanel/pcmanfm/libfm have GTK3 builds) since M8 already has the GTK3 stack; do **not** pull in
GTK2. XFCE stays the documented fallback only if an LXDE component proves unportable.

**Commitment: real LXDE with real `openbox` — no substitutes.** We build the actual brand-name DE:
`openbox` (not our existing twm/JWM standing in), `lxpanel`, `pcmanfm`. Do NOT relax "brand-name DE" by
swapping in an already-working lighter WM; the point of M8 is the real stack running unmodified. XFCE is
a documented last resort *only* if a specific LXDE component proves genuinely unportable to wasm (not
merely hard).

**Ordering: debugging tools FIRST, then the desktop.** M8.1 builds the out-of-band debugging toolchain
before any DE component, because the single-app spike (M8.0) cost multiple sessions purely for lack of
native-equivalent observability, and the multi-guest DE work will be far harder to reason about. Each
later sub-milestone is its own cross-compile against the threaded sysroot (`build-xlib.sh`-style) with
its own wasi blockers; per working-rule #4 and constraint #5, build/keep observability before guessing,
`jj diff` the vendored trees for stray diagnostics, and fix breakage in the **native/platform layer, not
the component source**. Strict order; each must hit its acceptance bar (live + interactive + automated
test + manual-example screenshot in `~/tmp/gui-progress/`) before the next starts.

- **M8.0 — GTK3 app spike. ✅ DONE (2026-06-22).** `gtk-hello` renders a live GTK 3 window with widgets
  on the wasm X server (proof above). This is the "spike first" step; it is NOT M8 acceptance.

- **M8.1 — Out-of-band debugging toolchain. 🟡 core deliverable DONE; rest build-on-demand.** The only
  guest-visible probes today are synchronous host calls that *perturb the race they measure*; build
  host-side observers that watch without participating. **DONE (2026-06-22):** tool 1's decisive half —
  the per-thread **deadlock-vs-livelock verdict** on `SECURE_EXEC_STACKDUMP` (`classify_stack` in
  `crates/v8-runtime/src/isolate.rs`: PARKED-ON-FUTEX / BLOCKED-IN-HOST / RUNNING), proven both ways
  (`~/tmp/gui-progress/proof-m8.1-stackdump-verdict.txt`). **Re-scope (evidence-based):** the built wasm
  **already retains a name section** (C function names survive; only `byn$fpcast` indirect-call thunks are
  nameless), so v8prof already names most frames and the DWARF symbolizer's marginal value is low — and
  the verdict tool + existing `SECURE_EXEC_TRACE` (RPC + byte flow) cover the thread-state and
  liveness axes that were decisive in M8.0. So tools 2-3 below are reclassified **build-on-first-demand
  during M8.2+** (just-in-time at the point of a real hang — still "observability before guessing,"
  applied where it pays), not speculative blockers. Catalog/status in `INTERNAL-TOOLING.md`. The tools, by
  value:
    1. **Thread-state / lock-holder dump (≈`gstack` + lock tracing) — highest value.** For every guest
       pthread, report what it is parked on (which futex/cond/mutex) and, for mutexes, the **current
       holder thread id**. Owns-the-thread-table read; extend `SECURE_EXEC_STACKDUMP`. This one tool
       makes both M8.0-class deadlocks (lost-wakeup cond vs. self-held `LockDisplay` mutex) obvious in
       seconds instead of inferred from wire silence.
    2. **X11 protocol decoder on the loopback socket (≈`x11trace`/`xtrace`).** The sidecar already sees
       the guest↔Xvfb byte stream; add a request/reply/event decoder behind an env flag. Names the exact
       stalling round-trip (e.g. `XRRGetOutputInfo`) directly. Exactly on-point for X-protocol hangs.
    3. **DWARF line-symbolizer for the pre-`--fpcast-emu` `.wasm` (≈symbols for `gdb`/`perf`).** Map
       `wasm-function[N]`+offset → `func:file:line` host-side off the DWARF in the un-fpcast module, so
       the native stack-dump (`crates/v8-runtime`) and the V8 tick profiler name guest C frames.
       `--fpcast-emu` (required for GTK's cross-signature fn-ptr casts) erases the name section, leaving
       only `byn$fpcast` thunks — this is what bottoms out every stack today.
    4. **Permanent per-`kernel_pid` sync-RPC trace view** (generalize `SECURE_EXEC_TRACE`): per-guest
       write/read byte totals + rate, so "deadlock vs. latency" and "which guest went silent" are one
       grep, not a hand-edited `eprintln!` (replaces the ad-hoc `net.write`/`net.poll` counters used in
       M8.0).
    5. **TSan-compiled guest builds (optional, for the threading bug class).** LLVM TSan targets wasm; a
       TSan build of a threaded guest flags pthread/cond/futex races directly. Higher setup cost; reach
       for it only if a multi-guest race resists tools 1-4.

- **M8.2 — `openbox` (the LXDE window manager). ⬜** Cross-compile openbox + its config/theme dep
  `libxml2` (new to the tree; XML parser, no network), using a pixmap/PNG theme so no `librsvg` closure
  is needed initially (theme choice is a staged fixture, not a source change). Acceptance: openbox
  decorates and manages the GTK `gtk-hello` window (titlebar, borders), interactive move/resize via
  injected pointer events works, and the openbox **root menu** opens. Test `scripts/test-m8-openbox.sh`.
  The standing caution (line ~289) to avoid openbox was about its `libxml2`/glib/pango/cairo deps — now
  acceptable because M8 built glib/pango/cairo and `libxml2` is the only new closure (pure-C, no net).
  Any wasi breakage is fixed in the platform layer, not by editing openbox.

- **M8.3 — `menu-cache` + `libfm` (the LXDE data layer). ⬜** Cross-compile `menu-cache` (freedesktop
  `.menu`/`.desktop` aggregation) and `libfm`/`libfm-extra` (GTK3) — the shared engine under both lxpanel
  and pcmanfm. wasi blockers expected, all fixed NATIVELY (constraint #5): `gio` file monitoring (provide
  it from the platform / inert monitor, do not edit libfm), volume/mount backends (present an empty
  static gio volume monitor — the truth in-sandbox — do not compile them out), and `dbus` (native stub
  or `--disable` configure flag). Acceptance: `test-m8-libfm.sh` enumerates a staged
  `/usr/share/applications` menu fixture from the VFS and lists a directory through `libfm` with no hang.

- **M8.4 — `lxpanel` (the panel/taskbar/menu). ⬜** Cross-compile lxpanel (GTK3) on top of M8.3.
  Acceptance: the panel renders at a screen edge with (a) an **application menu** that opens and lists the
  staged `.desktop` apps, (b) a **taskbar** that shows the managed `gtk-hello` window and reflects
  focus/minimize, and (c) a live **clock** plugin. Live + interactive (inject a menu click that launches
  an app). Plugins load via the M8.1/`dlopen` decision (native wasm `dlopen` preferred; static-link is
  the build-config fallback). Test `scripts/test-m8-lxpanel.sh`, screenshot proof.

- **M8.5 — `pcmanfm` (the file manager). ⬜** Cross-compile pcmanfm (GTK3) on top of M8.3. Acceptance:
  pcmanfm opens a window showing a **real directory listing of the kernel VFS**, navigates into a
  subdirectory and back, and (if desktop mode) draws desktop icons on the root window. Test
  `scripts/test-m8-pcmanfm.sh`, screenshot proof. Mounting/trash/volume features need no work — the
  empty native volume monitor (M8.3) makes unmodified pcmanfm correctly show no removable media; do not
  edit pcmanfm or fake devices.

- **M8.6 — LXDE session integration (= M8 ACCEPTANCE). ⬜** Bring up the full shell together via a
  minimal session launcher (`lxsession`, or a hand-written launcher if lxsession's dbus/autostart proves
  heavy): `openbox` + `lxpanel` + `pcmanfm` (desktop) running as one coherent, live, interactive LXDE
  desktop. Acceptance (the spec's M8 bar): the named DE's shell — panel + menu + file manager — live +
  interactive, with an automated end-to-end test (`scripts/test-m8-lxde.sh`) and a manual example +
  screenshot. Only when M8.6 is green is **M8 done**.

### M8 — overriding constraint #5: UNMODIFIED upstream; fix in the native/platform layer

The DE components (`openbox`, `lxpanel`, `pcmanfm`, `libfm`, `menu-cache`, …) and the toolkit/X
libraries (`GTK`, `GLib`, `libxcb`, `libX11`, …) must build and run from **stock upstream source** —
the same rule the repo applies to npm packages ("must work unmodified") and to tools generally ("Fix
runtime compatibility in secure-exec instead of patching callers around runtime quirks"). When a
component breaks, the fix goes in the **native/platform layer**, never in the component's source. The
boundary:

- **Platform (ours to change — fix here):** the Rust runtime/sidecar/kernel + the wasm host imports
  (`crates/*`); the VFS and the wasm X server (`Xvfb.wasm`); the **toolchain layer** — the patched
  wasi-libc/musl sysroot, `wasi-compat.{c,h}`, and `toolchain/compat-include/*` shims. A fix in the
  libc/sysroot is preferred to a per-library fix because **one** platform change makes **every**
  unmodified upstream consumer work.
- **Upstream (do NOT patch source):** the DE component repos and the GTK/X library repos. Allowed
  knobs are **build-time configure flags** (standard packaging, e.g. `--disable-foo`) and **runtime
  config/data/fixtures staged into the VFS** — these are not source modifications. Editing a `.c`/`.h`
  in `third_party/<lib|app>/` to work around a runtime quirk is the anti-pattern.

> **Known deviation to repay (tech-debt):** the M8.0 gtk_init fixes currently live as edits to
> `third_party/libxcb-threads` + `libX11-threads` + `xorgproto/Xthreads.h` (timed-cond + recursive
> mutex). Per this constraint the correct home is the **platform layer** — most likely the wasi-libc
> pthread implementation in the sysroot (make the default `pthread_mutex` self-deadlock-safe and give
> `pthread_cond_wait` a bounded internal re-check) so unmodified `libxcb`/`libX11`/openbox/GTK all work
> with no per-repo patches. Risk to weigh: a global libc pthread change affects genuinely-multithreaded
> guests (GLib worker, wasi-threads tests) and `pthread_cond_wait`-with-recursive-mutex semantics —
> validate against the M7.5 thread suite before adopting. Until moved, the per-lib edits are a
> documented exception (backed up in `~/tmp/gui-progress/fixes/`), not the pattern to copy for M8.2+
> (the DE components). Ideally repay this as part of M8.1 (the platform/observability work).

### M8 — additional things to specify (resolve in the native/platform layer per constraint #5)

- **Settings/config backend.** GSettings must NOT use dconf (dconf needs dbus). Select the **keyfile**
  backend via env + a staged schema dir / writable keyfile in the VFS (`GSETTINGS_BACKEND=keyfile`).
  This is runtime config, not a source change. Same spirit as the bucket-3 config rule: per-VM, on the
  wire/VFS, not an ambient daemon.
- **dbus session bus.** Prefer to **provide the capability natively** so components built with dbus run
  unmodified: a tiny sidecar/VFS unix-socket dbus *stub* that answers `Hello`/`RequestName` and no-ops
  the rest. `--disable-dbus` configure flags are an acceptable fallback (packaging knob, not a source
  patch) where a component supports it. Decide which components actually open a bus before building the
  stub. Do NOT run a real `dbus-daemon`.
- **`dlopen` (modules/plugins/theme engines).** Known M8 blocker: GTK theme/IM modules, GIO modules,
  panel plugins (`lxpanel` uses `g_module`), openbox theme loaders. **Preferred native fix: implement
  `dlopen` of guest wasm modules in the runtime** (load a sibling `.wasm` and resolve symbols), so
  unmodified components load their plugins as they expect. Build-config static-linking (link the
  needed plugins in, register at init) is the acceptable fallback — it's a packaging choice, not a
  source edit — but the native `dlopen` is the faithful answer and unblocks the general case.
- **`gio` volume/mount backends.** No removable media exists in the sandbox. **Native answer: present
  the gio/gvfs interfaces from the platform reporting an empty/static volume monitor** (which is the
  truth), so unmodified `libfm`/`pcmanfm` just see "no volumes". Do NOT compile the backends out of the
  component and do NOT stub fake devices.
- **VFS fixtures (staged via `--vm-tree`, like the Xft fonts — pure data, no source change).** A
  freedesktop **menu fixture** (`/etc/xdg/menus/*.menu` + `/usr/share/applications/*.desktop`), an
  **icon theme** (`hicolor` + one fallback so menu/taskbar/file icons resolve; else GTK draws blanks),
  and **MIME data** (`/usr/share/mime`) for pcmanfm typing. Check in `prepare-lxde-fixtures.sh` for
  reproducibility.
- **Process model / launching apps.** Menu entries and pcmanfm "open" must spawn new guest processes in
  the same VM (host `execute_env`→one-Session model, like the M5 multi-client twm substrate). Specify
  how a guest `g_spawn`/`exec` of a `.desktop` `Exec=` maps to launching another wasm guest — fix in the
  runtime's process layer; do not shell out to the host and do not patch the component's spawn code.
- **Resource/CPU budget.** WM + panel + fm + apps share the sidecar sync-RPC thread; carry forward the
  M5 net.poll round-robin (3ms clamp) and verify it scales to 4-5 concurrent GTK guests without starving
  any (the panel clock must keep ticking while pcmanfm loads). Runtime tuning, not a component change.
- **SVG icons/themes (clarified — a fixture choice, not a modification).** Choosing a **raster (PNG)
  icon theme** as the staged VFS fixture is normal Linux configuration and needs no source/build change
  to any component — make that the default for M8 to keep the closure small. If a genuinely SVG-only
  asset is required, the native answer is to **render SVG in the platform stack** (port `librsvg`, which
  is Rust→wasm-capable, as a normal dependency), NOT to compile SVG support out of a component.

Sequencing is strict: M6 → M7 → **M7.5 (threads)** → M8. Don't start the next until the previous fully
meets its acceptance bar (interactive, robust, tested, real fonts, no hacks). Within M8: **M8.1 tools →
M8.2 openbox → M8.3 menu-cache/libfm → M8.4 lxpanel → M8.5 pcmanfm → M8.6 LXDE session = acceptance.**

Threads (`wasm32-wasip1-threads` + `wasi_thread_spawn`) are now a first-class prerequisite milestone
(M7.5), not an ad-hoc per-milestone add: M8 (GTK) needs them, so they are specified and tested to a
production bar in **[`WASM-THREADS-SPEC.md`](./WASM-THREADS-SPEC.md)** before M8 begins.
GL passthrough is out of scope; software rasterization to the framebuffer remains the single data path.

## 5. M0 detailed design

Directory `experiments/wasm-gui/` — **standalone Cargo workspace** (its own `[workspace]` to halt
cargo's upward walk into the repo root workspace; root `Cargo.toml` has an explicit no-glob
`members` list, so without this the experiment crates fail to build):

```
experiments/wasm-gui/
  Cargo.toml               ← [workspace] members = ["guest","host"], resolver="2"
  SPEC.md  README.md
  guest/  Cargo.toml  src/main.rs   ← software renderer + frame protocol (target wasm32-wasip1)
  host/   Cargo.toml  src/main.rs   ← winit+softbuffer window; spawns `wasmer run guest --loop`
                                       (feature `window`, OFF by default)
  host-node/ run.mjs               ← node:wasi runner → raw framebuffer file
  tests/  run.sh  golden.json      ← build + run both engines + assert; writes RESULTS.txt
  scripts/ make-proof.sh           ← ffmpeg raw→PNG + copy artifacts to ~/tmp/gui-progress/assets
```

### Frame protocol v0 (pinned)
- Header: `magic = "SXFB"` (4 bytes) · `width: u32 LE` · `height: u32 LE`. Little-endian because
  it's WASM-native (no swap in guest).
- Payload: `width*height*4` bytes, **row-major, `[R,G,B,A]` byte order**, no stride/padding.
  Defined as a byte stream, so there is zero endianness ambiguity on pixels.
- Capture mode (`guest --out <path>`): write `header || payload` to the preopened file, exit.
- Window mode (`guest --loop`): read newline-delimited JSON events from stdin
  (`{"t":"pointer","x":..,"y":..}` / `{"t":"key","code":..}` / `{"t":"quit"}`), write
  `header || payload` frames to stdout. Used only by the Rust window host (wasmer handles stdio).
- **softbuffer note:** the window present step must repack `[R,G,B,A]` bytes → softbuffer's
  `0x00RRGGBB` native-endian u32. (Only the window path; not the compared bytes.)

### Guest determinism contract (REQUIRED — the raw-byte equality depends on it)
- No clock/time reads affecting pixels: the panel "clock" is a **hardcoded string** ("12:34").
- No RNG, or fixed-seed only; never `random_get`.
- Integer/fixed-point layout math; **no host-imported math**; avoid NaN-producing ops (plain
  WASM FP is deterministic across V8/wasmer, but keep transcendentals in-guest if used at all).
- No argv/env/cwd/locale/`$TZ`/font-file reads influencing pixels.
- Capture mode renders a **single fixed frame** with a **hardcoded pointer position**, so the
  cursor is deterministic.
- Guest writes **nothing** to stdout except frames; all diagnostics go to **stderr**. (A stray
  `println!` corrupts the binary stream.)

### node:wasi host (`host-node/run.mjs`)
- `new WASI({ version: 'preview1', returnOnExit: true })`, command (`_start`) model, run once.
- I/O via **preopened directory + file**, not stdio pipes: preopen a temp dir as `/out`, pass
  argv `["guest","--out","/out/frame.bin"]`, then read the host temp file back as a raw `Buffer`.
  (Preopened files are the robust node:wasi channel; stdio-as-pipe is the fragile path.)
- Expect/suppress the experimental-WASI stderr warning so the harness doesn't misparse it.

## 6. Testing strategy (fully automated, headless, no network/display)

`tests/run.sh` — non-zero exit on any failure:
1. `cargo build --target wasm32-wasip1 --release -p wasm-gui-guest` → `guest.wasm` exists.
2. Engine A: `node host-node/run.mjs … → /tmp/frame_node.bin`.
3. Engine B: `wasmer run guest.wasm --dir <tmp> -- --out /out/frame.bin → /tmp/frame_wasmer.bin`.
4. **Validate each header** independently: `magic=="SXFB"`, `w`/`h` == expected constants — fail
   with a clear message *before* comparing payloads (catches truncation/short reads).
5. **Cross-engine equality:** `sha256(frame_node.bin) == sha256(frame_wasmer.bin)` over the full
   `header||payload`. This is the honest engine-independence test (no PNG encoder in the loop).
6. **Golden-pixel checks** (`golden.json`): sample known coords on the raw payload (wallpaper,
   panel, title bar, a glyph pixel, cursor tip) and assert exact RGBA — deterministic regression
   guard.
7. Headless winit guard: run the capture path under `env -u DISPLAY -u WAYLAND_DISPLAY` and assert
   it never initializes a display (it can't pull in winit — the `window` feature is off).
8. `ffmpeg` encodes one `.bin` → `frame.png` (human proof only; never asserted).
9. Emit `tests/RESULTS.txt` consumed by the progress.html generator.

## 7. Manual example (run on your own machine, with a display)

`README.md` documents:
```
cd experiments/wasm-gui
./tests/run.sh                              # headless: builds + verifies + writes frame.png
cargo build -p wasm-gui-guest --target wasm32-wasip1 --release
cargo run -p wasm-gui-host --features window -- \
    --guest target/wasm32-wasip1/release/guest.wasm
```
Opens a real OS window showing the rendered desktop frame; mouse moves the cursor, Esc quits.
(This headless dev box can't open it; the PNG in progress.html is the byte-identical frame.)

## 8. Risks tracked

- R1 crate build time over cargo network. **Mitigated:** automated path uses installed `wasmer`
  CLI + `node:wasi` (no Rust wasm-engine crate). Only the manual window host builds
  winit/softbuffer, and it's not on the automated path.
- R2 node:wasi is experimental + ≠ the secure-exec bridge. Pin `version:'preview1'`,
  `returnOnExit:true`, node 24; treat as a V8-family proxy, real parity at M5.
- R3 Scope creep — M0 is a hand-rolled renderer, NOT a real toolkit (that's M2+). Keep it small.
- R4 "Looks like a desktop" is cosmetic at M0; fidelity comes from real toolkits at M2+.
- R5 Determinism — without §5's contract the raw-byte compare isn't guaranteed. Enforce it.

## Changelog
- **v2** (post-review): added nested-Cargo-workspace fix (build-breaking otherwise); switched
  cross-engine test from PNG-byte to raw-RGBA SHA-256 equality; pinned protocol endianness/pixel
  order; switched node:wasi I/O to preopened files + `returnOnExit`; added guest determinism
  contract; feature-gated winit; swapped wasmtime crate → installed `wasmer` CLI + `node:wasi`;
  added trust-model constraint (X/parsing stays in executor, host shuttle does no parsing).
- **v3** (M8 threaded GTK closure): added strict constraint #4 — build observability tools that
  parallel native debugging (`gdb`/`strace`/`xtrace`/`GDK_DEBUG`/`/proc`). Motivation: the threaded
  GTK stack builds + the threaded cairo+X stack renders to the wasm Xvfb (proof in progress.html), but
  diagnosing the remaining `gtk_init` hang (a cross-VM poll-scheduling deadlock: a threaded guest
  blocked in `net.poll` doesn't let the sibling X-server process run) cost far more than the fix would,
  purely for lack of native-equivalent observability. When blocked, name the native tool and build the
  parallel if feasible (sync-RPC trace + guest stack dump + env passthrough are the high-ROI ones).
- **v4** (M8 decomposition, 2026-06-22): the GTK-app **spike (M8.0) is done** and the two `gtk_init`
  runtime deadlocks are fixed (lost-wakeup cond → MONOTONIC timed-wait; `LockDisplay` self-deadlock →
  recursive X mutex). Corrected the earlier overstatement that this "met M8" — per the spec the single
  app is only the "spike first" step. Added **constraint #5: unmodified upstream; fix in the
  native/platform layer** (the npm "must work unmodified" rule applied to the DE/X/GTK repos — fix the
  runtime/sidecar/VFS/X-server or the toolchain sysroot/shim, never patch a component's source; the
  M8.0 per-lib X edits are flagged as a deviation to repay in the libc/sysroot pthread layer).
  Split M8 into sub-milestones with **debugging tools FIRST**: M8.1 out-of-band debug toolchain
  (thread/lock-holder dump, X11 protocol decoder, DWARF symbolizer for fpcast'd wasm, permanent per-pid
  sync-RPC trace, optional TSan) → M8.2 `openbox` → M8.3 `menu-cache`/`libfm` → M8.4 `lxpanel` →
  M8.5 `pcmanfm` → **M8.6 LXDE session integration = M8 acceptance**. Committed firmly to **real LXDE
  with real openbox** (no JWM/twm substitute; XFCE only a last resort if a component is truly
  unportable). Cross-cutting decisions all framed as native/config (keyfile GSettings, native dbus stub
  over `--disable`, native wasm `dlopen` over static-link, native empty volume monitor instead of
  compiling backends out, staged VFS menu/icon/MIME fixtures, in-VM app launching, N-guest CPU budget,
  PNG-theme fixture over librsvg).
- (v2 note, retained) node:wasi ≠ product bridge and M1-blocks-M2 dependency.
