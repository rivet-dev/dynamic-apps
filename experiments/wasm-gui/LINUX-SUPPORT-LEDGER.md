# Linux-Support Ledger

A living, auto-synthesized catalog of every **platform / native-layer** change secure-exec
made so that **UNMODIFIED** Linux/POSIX software (X.Org, GTK/GLib/cairo/Pango, D-Bus, Xfce,
coreutils, shells, terminal emulators) runs inside the wasm sandbox.

**The overriding constraint (SPEC.md constraint #5):** upstream components stay byte-identical.
Every compatibility fix lives in the native/platform layer — the runtime (`crates/`), the
sidecar, the VFS, the wasm X server's *build glue* (never its `.c`), the toolchain, or the
libc shims. This ledger catalogs those fixes so the surface is auditable and re-findable.

**How to read an entry.** Each is `{Linux feature expected · what broke under wasm/secure-exec ·
the native-layer fix · where it lives (file / commit)}`. Trees marked *(vendored/gitignored)*
are rebuildable build inputs, not version-controlled; their fixes live in build scripts or on-disk.

**Source provenance.** Synthesized from the commit history of `/home/nathan/secure-exec` and
`/home/nathan/secure-exec-wasmgui`, the `~/progress/secure-exec/*` logs, the wasm-gui design docs
(`PERF-ARCHITECTURE-RESOLVED.md`, `ROOT-2-MULTIPLEX-DESIGN.md`, `T1-SAB-RING-SECURITY-DESIGN.md`,
`PS-TOP-STRACE-SCOPE.md`, `XUBUNTU-SPEC.md`), and the session memory notes. **This is a starting
point — append to it as new platform fixes land.**

**Cross-repo note.** Two jj working copies. The **main repo** (`/home/nathan/secure-exec`) owns
the runtime (`crates/`), `registry/native/patches/` (wasi-libc + libc overrides), and the
canonical toolchain. The **wasmgui workspace** (`/home/nathan/secure-exec-wasmgui`) owns
`experiments/wasm-gui/` (X-lib trees, build scripts, the wasm-gui host, the spec). Many fixes
require a commit in **both**. `cross-env.sh` points `$REPO` at the main repo.

---

## Trust-model framing

These fixes are *platform compatibility*, not security relaxation. The sidecar↔executor boundary
is unchanged: host_net/host_fs/host_process imports are kernel-brokered, host_net is
`permissionTier==='full'` only, and the never-self-approve list (D-Bus-to-host, host-fd, GPU,
host-network egress) is untouched. The one boundary-relevant addition is **wasm-threads** (shared
memory across worker isolates) — that landed with its own runtime DoD and is flagged in §F.

---

## A. libc / wasi-libc shims

wasip1 omits sockets, process control, and much of POSIX by design (Workers-style isolation).
These restore the libc surface UNMODIFIED upstream expects. Overrides live in
`registry/native/patches/wasi-libc-overrides/*.c` (linked via `-Wl,--wrap=` into `libhostcompat.a`)
and `registry/native/patches/wasi-libc/*.patch`; broader gaps in
`experiments/wasm-gui/toolchain/wasi-compat.c`.

- **`fcntl(F_GETFL/F_SETFL/F_GETFD/F_SETFD)` on sockets** — Expected: set `O_NONBLOCK` on an fd ·
  Broke: host_net fds (`>=0x40000000`) aren't in the WASI fd table, so `fcntl` can't reach them;
  libxcb/glib non-blocking setup failed · Fix: `IS_HOST_NET_FD` branch routing F_SETFL→`net_set_nonblock`
  import, F_GETFL→`O_RDWR` · Where: `registry/native/patches/wasi-libc-overrides/fcntl.c`
  (`--wrap=fcntl`), commit `1977d19e2` main / `f23d51b1` wasmgui. **Repaid** libxcb's `set_fd_flags` patch.

- **`writev()` on sockets** — Expected: vectored write of the X11/D-Bus connection setup · Broke:
  wasi-libc `writev` fails on host_net fds; setup write silently dropped → "Unable to open display" ·
  Fix: `__wrap_writev` loops host_net fds through `send`+EAGAIN, else `__real_writev` · Where:
  `wasi-libc-overrides/writev_hostnet.c` (`--wrap=writev`), commit `c369688af` main / `7a0bfe20` wasmgui.
  **Repaid** libxcb's `xcb_conn.c` writev patch (libxcb now fully stock).

- **`ioctl(FIONREAD)` on sockets** — Expected: bytes-readable count (libX11 `SocketBytesReadable`) ·
  Broke: unsupported on host_net fds → libX11 fell back wrong · Fix: `wasi-libc-overrides/ioctl.c`
  answers FIONREAD on host_net fds via host_net `poll()` readable estimate, ENOTTY otherwise · Where:
  commit `81b2bd45c` main / `fdaaea67` wasmgui. **Repaid** libX11's `XlibInt.c` ioctl→poll patch.

- **`recvmsg()` / `sendmsg()`** — Expected: ancillary-data socket I/O (D-Bus credential passing) ·
  Broke: wasi has neither → D-Bus auth couldn't read the credential byte · Fix: strong `recvmsg`
  (reads iovecs via host_net `recv`) + strong `sendmsg` (send-per-iovec) · Where:
  `wasi-libc-overrides/dbus_creds.c`, XU0 (commit per `wasm-gui-xubuntu-xu0-dbus`).

- **`read()` on sockets** — Expected: plain `read()` works on a socket fd (D-Bus `_dbus_read`) ·
  Broke: host_net fds aren't in the WASI fd table → EBADF → fatal disconnect; and host_net
  would-block errno is unreliable · Fix: `__wrap_read` routes host_net fds to `recv`, forces
  `errno=EAGAIN` on `<0` · Where: `dbus_creds.c` (`--wrap=read`), XU0.

- **`getsockopt(SO_PEERCRED)`** — Expected: peer credentials for D-Bus EXTERNAL auth · Broke:
  wasi hides `SO_PEERCRED`/`struct ucred` behind `__wasilibc_unmodified_upstream` → compiled out ·
  Fix: build `-DSO_PEERCRED=17 -D_GNU_SOURCE`; `__wrap_getsockopt` returns the single sandbox
  identity `ucred{pid=1,uid=0,gid=0}` · Where: `dbus_creds.c` (`--wrap=getsockopt`), XU0.

- **`struct sockaddr_storage` too small for AF_UNIX** — Expected: `sizeof(sockaddr_storage) >=
  sizeof(sockaddr_un)` (=110) so GLib's `g_socket_connect` can write a unix address · Broke: stock
  wasi `__ss_data[32]` ≈34B < 110 → **every GIO AF_UNIX connect** fails `G_IO_ERROR_NO_SPACE` ·
  Fix: `__ss_data[126]` (128B, Linux `_SS_SIZE`) · Where: `__struct_sockaddr_storage.h` (vendored
  sysroot, all wasm32-wasi* variants); durably belongs in `wasi-libc/0008-sockets.patch`. XU1.

- **`pipe()` / `pipe2()` / `socketpair()`** — Expected: anonymous pipes & socketpairs (GWakeup,
  D-Bus reload) · Broke: wasi lacks them · Fix: `pipe`/`pipe2` over the `host_process.fd_pipe`
  import; `socketpair`→`pipe()` emulation · Where: `toolchain/wasi-compat.c`, kernel-pipe path
  (commit `2b45e734` GWakeup chain); dbus forces `ac_cv_func_socketpair=yes`.

- **`getpwuid` / `getpwuid_r`** — Expected: a passwd entry · Broke: no `/etc/passwd` → NULL deref
  in openbox `find_uid_gid` · Fix: strong stub returning a valid identity (user/`/`), forced over
  the weak wasi-compat def via `--whole-archive` · Where: `toolchain/openbox-compat.c`, M8.2.

- **group DB API (`getgrgid`/`getgrnam`/`setgrent`/`getgrent`/`endgrent`) + `grp.h` signatures** —
  Expected: group lookups · Broke: wasi omits the enum API; bad implicit-int signatures caused
  **wasm direct-call ABI traps** · Fix: correct static-inline decls/stubs · Where: compat `grp.h`
  + `openbox-compat.c`/GIO build patch, M8.2 / GLib stack.

- **`setgroups()`** — Expected: privilege-drop no-op · Broke: absent → D-Bus startup failure ·
  Fix: no-op stub (single-identity sandbox) · Where: `toolchain/wasi-compat.c`, XU0.

- **`getrlimit`/`setrlimit`** — Expected: fd-limit query · Broke: uninitialized → "cannot change
  fd limit" · Fix: fill `RLIM_INFINITY`; force `ac_cv_func_{getrlimit,setrlimit}=yes` (autotools
  link-test false-negatives) · Where: `wasi-compat.c` + `build-dbus.sh`, XU0.

- **Process/signal stubs** — `getpid`/`kill`/`waitpid`/`sig*`/`sigaction`/`fork`/`raise`/`alarm`/
  `setsid`/`tzset`/`strsignal`(weak)/`pthread_exit`/`execv`/`execve`/`daemon`/`setjmp`-via-sjlj —
  Expected: standard process control · Broke: wasi has no fork/exec/signals · Fix: stubs (no-op or
  VFS-backed) · Where: `wasi-compat.c`, `openbox-compat.c`, `glib-compat.c` (`err.h`/`daemon`), M7/M8.

- **`inet_addr`/`inet_aton`/`inet_ntoa`/`inet_ntop`** — Expected: address formatting (libXfont2,
  resolvers) · Fix: added to wasi-compat + `wasi-libc-overrides/inet_ntop.c` · Where: M6/glib stack.

- **`fmemopen` / memory streams** — Expected: `FILE*` over a buffer · used to work around
  per-access `fseek`/`fread` streaming gaps (XKB keymap, freetype) · Where: wasi-libc-overrides /
  `open_wmemstream.c`. See §D for the underlying VFS seek-read fix.

- **`tmpfile`, `getline`, `strlcpy`, `ns_get16/32`, `gethostbyaddr`, `getservbyname_r`,
  `pthread_attr_setinheritsched`** — assorted gap stubs surfaced one-at-a-time during GTK/Xfce
  builds · Where: `openbox-compat.c` / `wasi-compat.c` / `glib-compat.c` (find gaps by diffing a
  guest's `env` imports vs a known-good guest).

---

## B. X11 / GTK / GLib / cairo / Pango platform fixes

The wasm X server is real X.Org **Xvfb** cross-compiled to wasm; GTK3/GLib/cairo/Pango are real
upstream. Fixes are in the X-lib *build glue*, the GLib build patches (in `build-glib-stack.sh`,
not glib source), the runtime, or staged VM fixtures.

- **libxcb/libX11 cond-wait lost-wakeup deadlock at `XOpenDisplay`** — Expected: threaded X libs
  park/wake on socket handoff · Broke: built with the wasi-threads ABI but run single-threaded →
  `pthread_cond_wait` parks forever (no sibling to broadcast) · Fix: MONOTONIC
  `pthread_cond_timedwait` 4ms bound (`_X_xcond_init_monotonic`) · Where: `libxcb-threads/src/xcb_conn.c`,
  `libX11-threads/src/locking.c` via `Xthreads.h` macros *(vendored)*. (MONOTONIC because the
  runtime freezes CLOCK_REALTIME for determinism.)

- **libX11 non-recursive display mutex self-deadlock at `XRRGetOutputInfo`** — Expected: nested
  `LockDisplay` re-entry · Broke: single thread re-enters the non-recursive mutex → self-deadlock ·
  Fix: RECURSIVE X mutexes (`_X_xmutex_init_recursive`) · Where: `libX11-threads/src/locking.c` via
  `Xthreads.h` `xmutex_init` *(vendored)*.

- **GWakeup cross-thread main-loop wakeup** — Expected: a worker's `g_main_context_invoke` wakes
  the main thread's blocked `poll()` via a pipe · Broke: glib's `gwakeup.c` was (mis)stubbed to
  `{-1,-1}` fds on `__wasi__` ("assumed single-threaded"); the runner's synthetic JS pipe is
  per-isolate so it can't wake another thread's isolate · Fix: revert gwakeup to pristine; route
  `pipe()` through a **kernel pipe** (shared across thread isolates, range-encoded
  `0x50000000+kernelFd`) so a write notifies the kernel poll notifier → cross-thread `__kernel_poll`
  wakes · Where: `wasi-compat.c` pipe shim + sidecar `__kernel_pipe`/`__kernel_fd_*` sync-RPCs +
  runner kernel-pipe fd routing, commit `2b45e734` (M8.5). **This unblocked all GLib worker-thread async.**

- **GIO worker-context wakeup (main→worker direction)** — Expected: the GLib worker context polls
  its wakeup fd · Broke: the gmain worker blocks in its first `g_main_context_iteration` before
  polling its wakeup pipe (main→worker fails even though worker→main works) → `GVolumeMonitor` /
  libfm jobs hang · Fix (partial/workaround): patch `gunixvolumemonitor.c is_supported()`→FALSE on
  `__wasi__` (sandbox has no mounts) so the null monitor is used; `GIO_USE_VOLUME_MONITOR=null` ·
  Where: `build-glib-stack.sh` patch, M8.5. *(Proper constraint-#5 fix — runtime main→worker
  wakeup — still open.)*

- **`net_poll` ignored kernel-pipe (GWakeup) fds** — Expected: `poll()` on a GMainContext whose only
  source is a wakeup pipe returns readable when written · Broke: `net_poll` only handled host_net
  sockets; pipe revents stayed 0 → `g_bus_get_sync` spins · Fix: `__kernel_fd_poll` sync-RPC
  (`kernel.poll_fds`); `net_poll` batches kernel-pipe fds through it and caps `net.poll_wait`≈10ms
  when the set has pipe fds · Where: runner + sidecar, XU1 (GDBus enablement).

- **per-isolate host_net socket table breaks worker-thread socket I/O** — Expected: a worker thread
  does socket I/O on a socket the main thread opened (GDBus does async I/O on a worker) · Broke:
  `hostNetSockets` is a module-level Map and **each wasm thread is its own V8 isolate** → worker's
  `net_send(fd)` misses → EBADF → "connection closed" · Fix: a sidecar-side guest-fd registry
  (`net.register_guest_fd`/`net.resolve_guest_fd`, keyed `(owner_proc_id, fd)`); a thread's net.* op
  dispatches against its owning ancestor process; `net.poll_wait` waits on the owner's
  `SocketReadiness` · Where: `node_import_cache.rs` + sidecar `service.rs`, XU1
  (`XU1-SOCKET-SHARING-DESIGN.md`).

- **freetype file-stream font loading** — Expected: `FT_New_Face` streams a font via per-access
  `fseek`/`fread` · Broke: VFS-backed files don't support per-access streaming under wasi → "Unable
  to load fontset" · Fix: patch `ftsystem.c` `FT_Stream_Open` to slurp the whole font into a memory
  stream · Where: `freetype/src/base/ftsystem.c` *(vendored)*, M6.2. (Underlying VFS seek-read fix in §D.)

- **XKB keyboard with no fork/exec** — Expected: Xvfb runs `xkbcomp` to compile a keymap · Broke:
  wasi has no fork/exec → keyboard never activates → keys dropped · Fix: host-precompile a US keymap
  (`prepare-xkb.sh`→`/xkb/default.xkm`), patch the server's `XkbCompileKeymap` to load it directly,
  slurp the `.xkm` and read via `fmemopen` · Where: `patches/xserver-keymap-no-xkbcomp.patch`
  *(on-disk)*, M6.1.

- **libX11 compiled-in locale path absent in VM** — Expected: `XCreateFontSet` finds the locale DB ·
  Broke: libX11 built without `XLOCALEDIR` env support (wasi lacks `getresuid`/`issetugid`) → reads
  only the host build path · Fix: host `--locale-dir` + `prepare-locale.sh` install the C-locale DB
  at libX11's compiled-in paths · Where: host main.rs (`LIBX11_COMPILED_LOCALE_DIRS`), M6.2.

- **Xvfb RandR phantom 1280×1024** — Expected: `XRRGetMonitors` returns the `-screen` size · Broke:
  `InitOutput.c` seeds CRTC geometry from `defaultScreenInfo` before `-screen` is parsed; the parse
  updates the core screen but not `crtcs[]` → GTK sizes percent-width panels off-screen (lxpanel
  black) · Fix: drive CRTC geometry from the parsed `-screen` size · Where:
  `patches/xserver-vfb-randr-screen-geometry.patch` *(on-disk)*, M8.4.

- **gdk-pixbuf dynamic image loaders need dlopen** — Expected: load PNG/XPM via loadable modules ·
  Broke: wasm has no dlopen → no image format decodes → every icon load fails · Fix: build gdk-pixbuf
  `-Dbuiltin_loaders=png` (static PNG); stage a curated Adwaita/hicolor icon set via `--vm-tree`;
  **XPM→PNG transcode** for Xfce deco themes (`prepare-themes.sh`, PIL) since the build is PNG-only ·
  Where: `build-gtk3.sh` + `prepare-icons.sh`/`prepare-themes.sh`, M8.5 / XU2.

- **GDK NULL input devices trap under a WM** — Expected: a seat with core pointer/keyboard · Broke:
  Xvfb advertises XI2 but enumerates NULL master/slave devices; once a WM sends focus/crossing events
  GDK derefs the NULL device → wasm trap · Fix: `GDK_CORE_DEVICE_EVENTS=1` guest env (forces GDK core
  device manager, honest single-XTEST-seat) · Where: host guest-env, M8.6.

- **GTK deep-init stack overflow** — Expected: GTK's deep init / recursive `gtk_widget_show_all`
  call chain · Broke: overflows the wasm 64KB default stack → "memory access out of bounds" · Fix:
  `-Wl,-z,stack-size=8388608` (8MB) on every GTK component · Where: all GTK build scripts, M8.5/XU1.

- **`HAVE_XSYNC` cross-detection unreliable** — Expected: gdk-x11 detects libXext XSync · Broke:
  cross `has_function` link test is unreliable → XSync compiled out · Fix: force `HAVE_XSYNC` in
  gtk meson.build · Where: `build-gtk3.sh`, M8.

- **libepoxy GLX / XShm absent** — cairo XShm path needs `sys/ipc.h`/`sys/shm.h`; MIT-SHM
  unavailable (isolated guests, separate linear memory) → cairo falls back to `XPutImage`; libepoxy
  built standalone GLX-off · Where: compat headers + `build-gtk-deps.sh`, M8.

- **GtkUIManager / GLib idle starvation (low-priority idles never dispatch)** — Expected: a
  `g_idle_add` cleanup fires after a job completes · Broke: the GTK X11 event source stays
  perpetually-ready (a stray core FocusIn it can't drain) → starves the lower-priority idle → the
  dir-list "finish-loading" never dispatches (pcmanfm listing empty) · Fix: post that idle at
  `G_PRIORITY_DEFAULT` · Where: `build-libfm.sh` python patch on `fm-job.c`, M8.5.

- **libfm `fm_run_in_default_main_context` deadlock** — Expected: run a func in the default context ·
  Broke: GLIB>=2.32 path does `g_main_context_invoke(NULL)+g_cond_wait` unconditionally → deadlocks
  from the main thread before `gtk_main` iterates · Fix: the inline-run guard the older path has ·
  Where: `build-libfm.sh` python patch, M8.5.

- **`g_variant_builder_init_static` host-codegen / target-lib skew** — Expected: gdbus-codegen output
  links · Broke: the host's glib≥2.84 `gdbus-codegen` emits the static API the wasm target's glib 2.78
  lacks → LinkError · Fix: `glib-compat.c` shims `g_variant_builder_init_static`→`g_variant_builder_init`;
  prefer the *wasm build's own* codegen wrapper · Where: `toolchain/glib-compat.c` + build scripts, XU1.

- **`path_filestat_set_times`/`fd_filestat_set_times`/`fd_renumber`/`fd_datasync` imports missing** —
  Expected: standard WASI fs ops GTK/openbox/xfconfd import · Broke: node:wasi omits them → instantiate
  LinkError · Fix: runtime no-op/aliased imports (`fd_datasync`→`fd_sync`, `fd_renumber`→handle clone) ·
  Where: `node_import_cache.rs` (asset bumps), M8.2 / XU1.

---

## C. Networking / sockets

The kernel socket table brokers all guest sockets (AF_UNIX loopback between guests; host_net at
`permissionTier==='full'`). host_net fds are `>=0x40000000`.

- **`POLLOUT` value wrong** — Expected: wasi `<poll.h>` POLLOUT · Broke: `net_poll` hardcoded `0x2`
  (POLLPRI/POLLWRNORM) instead of `0x004` → libxcb's poll-for-reply never saw the socket writable →
  `XOpenDisplay` hung; later D-Bus (the first guest that genuinely parks on write-readiness) confirmed
  the bug · Fix: report POLLOUT=`0x004` (kept `0x002` as compat bit) · Where: `node_import_cache.rs`
  `net_poll`, XU0 (asset 100→101). (POLLIN=`0x1` was always correct.)

- **non-blocking sockets unreachable** — Expected: `fcntl(O_NONBLOCK)` makes a socket non-blocking ·
  Broke: can't reach host_net fds; both libxcb's poll helpers and the X server's multi-client dispatch
  need non-blocking sockets · Fix: host_net `net_set_nonblock(fd,en)` import + non-blocking `net_recv`
  (EAGAIN on empty) · Where: `node_import_cache.rs`, M5.

- **`setsockopt(SO_REUSEADDR/SO_REUSEPORT/SO_KEEPALIVE/SO_BROADCAST)`** — Expected: benign boolean
  socket options succeed (pcmanfm single-instance socket) · Broke: returned EINVAL → treated as fatal ·
  Fix: accept as a no-op (`'noop'` kind → SUCCESS) · Where: `node_import_cache.rs` `net_setsockopt`,
  M8.5 (asset 84→85).

- **AF_UNIX connect/accept paths** — `net_connect` AF_UNIX (strip trailing NUL), `net_accept` unix
  addresses + non-blocking, `net_poll` listener readiness via buffered non-blocking accept · Where:
  `node_import_cache.rs`, M4/M5.

- **threaded global vs thread-local errno on would-block** — Expected: recv/send set errno on
  would-block · Broke: prebuilt `host_socket.o` writes the non-threaded *global* errno but threaded
  wasi-libc reads *thread-local* errno → unreliable on would-block · Fix: treat host_net `recv/send==-1`
  as would-block unconditionally (host_net returns -1 only for would-block) · Where: libxcb
  `xcb_conn.c`/`xcb_in.c` *(vendored, threaded build)*.

- **X server poll uses real `poll()` not select** — Expected: `select()`/`fd_set` over X client fds ·
  Broke: host_net fds `>0x40000000` exceed `FD_SETSIZE`; `fd_set` can't represent them (D-Bus hit this
  too) · Fix: vendored `os/xserver_poll.c`→real `poll()`; force `ac_cv_func_poll=yes` for D-Bus ·
  Where: `xserver` build *(vendored)* + `build-dbus.sh`, M5 / XU0.

- **Xtranssock / xcb writev→send-loop** — `Xtranssock.c` Read/Write/Readv/Writev → recv/send;
  libxcb `xcb_conn.c` writev → send-loop · Where: vendored X transport (libxcb writev since **repaid**
  via §A `--wrap=writev`; remaining libxtrans read/write/readv/writev are the next repayment target).

---

## D. Filesystem / procfs / /dev

The kernel VFS presents normal Linux fs semantics; host-backed shadow mounts back `--exec`/`--vm-tree`
guests.

- **VFS seek-then-read returns 0 (the big one)** — Expected: `fseek(0)+fread` after a sequential read
  re-reads from the new offset (every image/asset/config loader sniffs-then-rewinds) · Broke: the base
  WASI passthrough `_fdRead` read at `position=null` (the host fd's own offset) but `_fdSeek` only
  updated the tracked `entry.offset` → the seek was ignored → `fread` returned 0; gdk-pixbuf loaded no
  decoration PNGs, xfwm4 drew no decoration · Fix: passthrough `_fdRead` reads **positionally** from
  `entry.offset` (the field `_fdSeek` maintains) and advances it · Where: `crates/execution/src/wasm.rs`
  `_fdRead`, XU2 (asset 106→108). **Unblocks any component that seek-reads a VFS file.**

- **`/dev/null`, `/dev/zero`, `/dev/full`, `/dev/random`, `/dev/urandom`** — Expected: device files
  openable · Broke: threaded guests use the host-backed wasm fs which bypasses the kernel device layer →
  ENOENT (GTK/X never noticed — they use `getrandom()`; D-Bus opens the device files) · Fix: `wasm.rs`
  `WasmCharDevice` table + `path_open` routes `/dev/{null,zero,full,random,urandom}` through the guest
  file path regardless of O_CREAT · Where: `wasm.rs` + `node_import_cache.rs`, commit `16a27519` (XU0).
  *(Note: non-O_CREAT opens previously skipped the guest-file path — that's why the device routing had to
  be added on both create and non-create.)*

- **`mkstemp` / `O_EXCL` exclusive-create gap** — Expected: `O_RDWR|O_CREAT|O_EXCL` (no O_TRUNC)
  creates the temp file · Broke: `fsOpenFlagForPathOpen` returned Node `'r+'` (no create!) for an
  exclusive create without O_TRUNC → ENOENT → every safe-create silently broke (menu-cache atomic save) ·
  Fix: return `'wx'`/`'wx+'` for any exclusive create; `precreatePathOpenTarget` skips O_EXCL; numeric
  `open_wasm_guest_file` maps O_CREAT|O_EXCL→`create_new` · Where: `node_import_cache.rs` + `wasm.rs`,
  commits `cc0c690b`+`104403ce` (M8.3).

- **`pwrite` not forwarded through the VFS layer stack** — Expected: positional write reaches the leaf
  store · Broke: `RootFileSystem`/`OverlayFileSystem` didn't forward `pwrite` to the in-place leaf · Fix:
  forward `pwrite` through to the in-place leaf · Where: `crates/vfs` (commit `f5b80f093e38`).

- **`fdatasync`** — Expected: durability flush (xfconfd) · Broke: import absent · Fix: `fd_datasync`→
  `fd_sync` (kernel VFS owns durability) · Where: `node_import_cache.rs` + `wasm.rs`, XU1.

- **`mmap(MAP_SHARED, fd)` writeback / `msync`** — Expected: a file-backed shared mapping writes back
  (Xvfb's framebuffer export, restored to stock upstream `mmap+msync`) · Broke: wasi-emulated-mman gives
  no write-back · Fix: a file-backed MAP_SHARED writeback registry behind `__wrap_mmap`/`__wrap_munmap` +
  a real `msync()` that pwrites tracked regions (runner delta-encodes) · Where: `toolchain/wasi-compat.c`
  (`--wrap=mmap,munmap`), commit `1bbae0b5` (M8.6, constraint-#5 repayment of the Xvfb fb patch).

- **procfs ready for `ps`/`top`** — `/proc` enumerates pids + `cpuinfo/loadavg/meminfo/mounts/self/
  uptime/version`; per-pid `stat`/`cmdline`/`environ`/`cwd`/`fd` nodes exist. A real `ps`/`top` works
  with no kernel change; only a wasm-command build is needed · Where: `crates/kernel/src/kernel.rs`
  (`PS-TOP-STRACE-SCOPE.md`). `strace` needs a new kernel syscall-trace capability (open).

---

## E. D-Bus

Running stock `dbus-daemon`/`dbus-send`/`dbus-monitor` as untrusted guests over the same AF_UNIX
kernel socket table as X — **no TCB expansion** (user-signed-off). XU0 = full session-bus round-trip,
all wasm. The EXTERNAL-auth handshake drove four platform fixes (all in §A/§C; the trace order
mattered — each exposed the next):

1. **`recvmsg`** so the daemon reads the 1-byte credential — §A `dbus_creds.c`.
2. **`SO_PEERCRED`** so EXTERNAL auth gets a uid — §A `--wrap=getsockopt` returning `uid=0`.
3. **`read`/`sendmsg` on host_net fds** + EAGAIN errno so messages flow — §A `dbus_creds.c`.
4. **`HAVE_POLL` + correct `POLLOUT`** so the daemon's queued "OK" actually sends — §C
   `ac_cv_func_poll=yes` + POLLOUT=`0x004`.

GDBus (GIO's D-Bus client, used by xfconf/xfsettingsd) added: §B per-isolate socket sharing
(worker-thread I/O), §B kernel-pipe GWakeup poll, §A `sockaddr_storage` sizing, and an ANONYMOUS-auth
fallback (GIO `getuid()`→-1 fails EXTERNAL; the permissive `session.conf` allows ANONYMOUS). Fixtures:
`session.conf`, `/etc/machine-id` (GDBus autolaunch needs it), `--bus-test`/`--dbus`/`--dbus-service`
harness modes. `libdbuscreds.a` is linked into the GDBus binaries.

---

## F. Threads / concurrency runtime (wasm-threads)

GLib/GTK require real threads (a GIO/GDBus/file-monitor worker spawned at startup). wasip1 has no
threads. This is the one TCB-relevant addition and landed with its own runtime DoD.

- **`wasi.thread-spawn` + shared memory** — Expected: `pthread_create` spawns a thread sharing memory ·
  Broke: wasip1 has no `(import "wasi" "thread-spawn")`; the JS runner has no Web Workers · Fix: Rust
  spawns a worker **OS thread + V8 isolate** re-instantiating the SAME compiled module on the SAME shared
  `WebAssembly.Memory` (SAB backing store moved cross-thread via a Send newtype), runs `wasi_thread_start`;
  only memory is shared, the indirect table is per-instance · Where: `crates/v8-runtime/src/wasm_threads.rs`,
  `session.rs`, `node_import_cache.rs` `parseWasmThreadInfo`; `validate_module_limits` allows shared
  imported memory. `WASM-THREADS-SPEC.md` (milestone M7.5). Asset v76→v84.

- **worker→kernel host calls** — worker threads are real sidecar wasm sessions sharing the parent
  `kernel_pid` (shared fd table); host calls reach the kernel. Lifecycle: `ActiveProcess.is_thread`;
  `finish_active_process_exit` skips reap for threads (else a worker exit kills the shared pid);
  trap→`fault_thread_group` terminates leader+siblings · Where: sidecar `execution.rs`.

- **`max_threads` on the wire** — `limits.resources.maxThreads`→kernel `ResourceLimits.max_threads`
  (default 64), enforced in `spawn_wasm_thread`; process-global `ThreadSlots` + per-VM cap.

- **C++ typed exceptions across threads** — Expected: `catch(int)`/`catch(T&)` works (`catch(...)`
  already did) · Broke: clang `-fwasm-exceptions` emits `__wasm_lpad_context` accesses as **TLS** in
  threaded TUs but **non-TLS** in non-threaded TUs; libunwind declares it `_Thread_local` — a mismatch
  makes the landing pad write `lsda` to one location and the personality read 0 → `scan_eh_tab` early-returns
  → typed catch escapes (blocked VTE/xfce4-terminal) · Fix: build the EH runtime (libc++abi EH sources +
  `Unwind-wasm.o`) **all-threads**, keeping `__wasm_lpad_context` TLS, so it matches the threaded guests ·
  Where: `experiments/wasm-gui/toolchain/build-libwasmeh.sh` → `libwasmeh-full.a`
  (`wasm-cpp-exceptions-lpad-tls-fix`).

- **libffi (`ffi_call` + closures)** — Expected: GObject's `g_cclosure_marshal_generic` does dynamic
  calls; libffi has no pure-wasi port · Broke: M8 "blocked at libffi" · Fix: a libffi-ABI-compatible shim —
  `ffi_call` via a host import reflecting on `instance.exports.__indirect_function_table.get(idx)(...)`;
  closures via a **pure-wasm trampoline pool** (no host import) · Where:
  `experiments/wasm-gui/libffi-wasm/` + `host_net.ffi_call` in `node_import_cache.rs` (asset 74), wired
  into the real GLib build via `build-glib-stack.sh`. Build with `--export-table`, **no** `--fpcast-emu`
  (it rewrites indirect-call indices).

---

## G. Performance levers (the desktop scaling work)

The multi-app desktop wall is **single-kernel-service-thread serialization + a base64 binary-RPC hop**.
Named roots (per `PERF-ARCHITECTURE-RESOLVED.md`):

- **Root-3: binary RPC byte-copy (base64)** — Expected: zero-copy binary I/O · Broke: the native WASM
  sync-RPC base64-encodes binary payloads (`cbor_to_json`→`{__type:Buffer,data:base64}`); Xvfb's
  `vfbBlockHandler` pwrote the whole 1.2MB framebuffer every block through it → 97% futex storm,
  black-spin at 640×480 (rendered fine at 160×120) · Fixes shipped: (a) **fb delta-encode** — `fd_pwrite`
  for large guest-file writes diffs vs the previous frame and writes only changed byte-ranges
  (`node_import_cache.rs`, commit `7f2e6261`); (b) **batched net_poll drain** — one `net.poll` over all
  of a process's sockets (`40a768de`, Xvfb futex 32308/4s→1552/4s, ~20×); (c) **lazy readReadyGen** on
  timeout=0 polls (`7d1b35f5`); (d) **accept-probe throttle** to once/50ms on spin-polls (`04bc635f`,
  `net.server_accept` −81%) · Where: runner + sidecar. **General fix (T1 below) supersedes base64.**

- **Root-2: single kernel-service-thread serialization** — Expected: N guests' syscalls run in
  parallel · Broke: the kernel is a single-owner `&mut sidecar` serviced by one thread; under 4 heavy
  GTK guests + Xvfb the X server starves (~1 window in 216s) · Status: **design only**
  (`ROOT-2-MULTIPLEX-DESIGN.md`): observability first, then a dedicated X-server IO thread (option D),
  then per-subsystem locks (B), then per-guest sharding (C). Genuine TCB concurrency review required.
  Live measurement must instrument the **WASM sidecar-serviced** RPC path
  (`WasmExecutionEvent::SyncRpcRequest`, `execution.rs:2685`), not the pump loop or the JS handler — the
  desktop's hot fs/fb RPCs are serviced executor-internal in `wasm.rs:933`.

- **Root-1: wasm indirect-call / GObject construction cost** — Expected: cheap fn-ptr dispatch · Broke:
  GObject's `fpcast-emu` thunks make construction expensive · Fix: fpcast arity tuning
  (`max-func-params@128`→@64 measured ~3.7x GObject construction); **not** typed-func-refs (that was the
  wrong lever) · Where: build-script wasm-opt args. Independent parallel track for solo-app snappiness.

- **T1: SAB ring transport (the recommended first build step)** — generalize the proven framebuffer
  SAB fast-path to **every** syscall: a guest↔kernel SharedArrayBuffer SPSC ring with an atomics
  doorbell, killing the base64 control-channel hop. Possibly autonomous (same data, faster channel) but
  the guest writes hostile bytes into the ring, so it needs the validation spec in
  `T1-SAB-RING-SECURITY-DESIGN.md` (kernel-owned `ring_size`, copy-out-then-validate to defeat
  double-fetch/TOCTOU, `MAX_RECORD_BYTES` cap, bounded `Atomics.wait`). Status: **security design written,
  no code yet.**

- **Other measured Node-parity perf levers** (`NODEJS-RUNTIME-COMPARISON.md`): timers spawn one OS thread
  per arm (→ single timer thread + BinaryHeap); 1ms busy-poll event loop (→ block until msg/deadline);
  blocking console RPC (→ fire-and-forget); pure-JS encoders (→ native simdutf/encodeInto); 128MB heap cap
  collapses V8 semi-space (→ decouple `--max-semi-space-size`).

---

## H. Toolchain / build (cross-compile recipe)

Unmodified autotools/meson upstream cross-compiles to `wasm32-wasip1[-threads]` only through this glue.
Build WASM **only** via the custom toolchain (`make -C registry/native wasm` / the wasm-gui build scripts),
never a raw `cargo build --target wasm32-wasip1`.

- **`--fpcast-emu` for cross-signature fn-ptr casts** — GTK/GObject/openbox call fn pointers across
  signatures; raw wasm traps "null function or function signature mismatch" · Fix: `wasm-opt --fpcast-emu`
  (separately — link scripts don't auto-apply it) · Caveat: erases the C name section and rewrites indirect
  indices (so libffi/ffi_call builds skip it). Tune `--pass-arg=max-func-params@N`.

- **Strip DWARF or the V8 isolate OOMs at compile** — linked X/glib/gtk libs carry huge `.debug_*`
  (lxpanel 44MB→5MB); the V8 isolate heap (not wasm linear memory) is the limit · Fix:
  `wasm-opt --strip-debug --strip-dwarf` on every guest · Where: all link scripts, M8.4.

- **C++ on wasm** — wasi-sdk libc++ (`-isystem .../c++/v1` + `-lc++ -lc++abi`) + `-mllvm
  -wasm-enable-sjlj` (freetype setjmp); `libsetjmp.a`/`libc.a` appended **last** via the `LIBS` make var
  (position matters with `--allow-undefined`) · Where: cross-ini + build scripts.

- **autotools cross-detection traps** — (a) **never** put `-Wl,--allow-undefined` in `./configure` LDFLAGS
  (makes every link feature-test false-pass → mis-detects `getpeerucred` etc.); configure with
  `-L$PREFIX/lib -lhostcompat`, add `--allow-undefined` only at the final link. (b) Functions in an object
  that references unresolved host imports **false-negative** → force `ac_cv_func_*=yes`. (c)
  `clang-wasi-wrap.sh` strips libtool's `--as-needed`/`--export-dynamic`/`--version-script` (wasm-ld
  rejects them). (d) gettext/intltool **host-bin stubs** satisfy `--version` probes (`--disable-nls`). (e)
  copy a newer `config.sub`/`config.guess` (into `build-aux/` too) so `wasm32-wasi` is recognized.

- **`-fcommon`/`-fno-common` tentative-def collisions** — clang defaults `-fno-common` → cross-TU duplicate
  symbols; `-fcommon` ICEs the wasi clang; wasm-ld has no `--allow-multiple-definition` · Fix: a guarded
  **unity TU** that `#include`s the unmodified `.c` files into one TU (menu-cache-gen) · Where: build temp dir, M8.3.

- **Two mandatory GTK link flags** — `-Wl,--wrap=writev` (route the X11 setup write to host_net) +
  `-Wl,-z,stack-size=8388608` — every GTK component, or it reports "Unable to open display".

- **`libhostcompat.a`** is the canonical host-import/compat archive (`host_socket` + `host_pipe_dup` +
  `override_fcntl` + `override_writev` + `override_ioctl` + `wasi-compat-threads` + per-component compat).
  libtool rejects a raw `.o` in its library stage → must be an archive. The `--wrap` override objects are
  regenerated from main-repo source in the build scripts (they're gitignored blobs prone to an ar race).

- **`SECURE_EXEC_WASM_THREADS=1`** selects the threaded profile (`wasm32-wasip1-threads`,
  `wasm-prefix-threads`, imported/shared/growable memory, real `-pthread`). **Dual trees** exist
  (`libX11` vs `libX11-threads`, etc.) — the GTK clients link the `-threads` variant; editing the wrong
  tree clobbers the threads prefix.

---

## I. Internal observability / tooling (constraint #4 — for diagnosing, reverted after)

Built to debug the above; flag-gated, never in the hot path of a shipped guest. Cataloged in
`INTERNAL-TOOLING.md`. Highlights:

- `SECURE_EXEC_TRACE` — sync-RPC strace (`crates/sidecar/src/execution.rs`).
- `SECURE_EXEC_STACKDUMP_AFTER_MS` — isolate-interrupt native backtrace (`crates/v8-runtime/src/isolate.rs`).
- `SECURE_EXEC_V8PROF` — V8 tick profiler (`scripts/v8prof-top.py`).
- `SECURE_EXEC_NET_TRACE` / `SECURE_EXEC_POLLDBG` / `SECURE_EXEC_FD_TRACE` — socket I/O, poll readiness,
  and fd-kind tracing.
- `SECURE_EXEC_KEEP_NAMES` — keep the wasm `name` section through link (lld drops it; `--fpcast-emu`
  leaves only `byn$fpcast-emu$N` thunks) so V8 reports real C names in stackdumps.
- `SECURE_EXEC_XTRACE=65536` + `scripts/xreassemble.py` — X11 protocol capture.
- `SECURE_EXEC_ROOT2_TRACE` / `SECURE_EXEC_RPCPROF` — per-RPC servicing timing (the Root-2 measurement seam).

**Recurring diagnostic lessons.** Hot-path `ErrorF`/`fprintf` instrumentation **perturbs the timing it
measures** (heisenbugs) — use non-perturbing `/data` counters read from the VM shadow. A LinkError
"Import requires a callable" = a missing import (stub it); a RuntimeError "unreachable" = a trap (ABI
mismatch / null-deref — bisect with markers). Before theorizing a deep runtime bug, `jj diff` the
gitignored vendored sources for stray uncommitted diagnostics.

---

## Milestone state (proof index)

- **M1–M7 + interactive desktop**: DONE — twm/JWM decorate real libX11 windows; live cursor/keyboard/drag
  via host XTEST; st terminal emulator + kernel-PTY shell.
- **M8 (LXDE)**: openbox + lxpanel + pcmanfm render together as a live desktop (all wasm); the threaded
  GTK/GLib/cairo/Pango stack runs. Remaining: Root-2 fair scheduling for >3 heavy guests.
- **XU0 (D-Bus session bus)**: DONE — full method-call + signal round-trip, all wasm.
- **XU1 (xfconf→xfsettingsd→XSETTINGS→GTK)**: DONE — a GTK window themed by real Greybird.
- **XU2 (xfwm4)**: DONE — xfwm4 decorates a GTK window with the Greybird theme (the VFS seek-read fix).
- **XU3+ (panel/desktop/Thunar/session)**: ahead.

Screenshot proof lives under `~/tmp/gui-progress/<date>/` and `~/progress/secure-exec/*/`.
