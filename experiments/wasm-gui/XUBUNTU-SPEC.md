# Spec: Xubuntu desktop compatibility — a full Linux desktop, all-wasm in secure-exec

Living spec (**DRAFT v1**, 2026-06-24). Status legend: ⬜ todo · 🟡 in progress · ✅ done · ❌ blocked.

**Relationship to the existing specs (read these first):**
- [`SPEC.md`](./SPEC.md) — the original DE spec. Milestones M1–M8 took the runtime from a
  software-rendered frame to a **live LXDE desktop** (openbox + lxpanel + pcmanfm), all wasm. **This
  spec is the direct successor to M8**: it reuses M8's entire spine (software raster → framebuffer →
  host blit; X11-over-socket; static-link, avoid `dlopen`/SHM; the wasm X server; the XTEST input
  path; the fixture/`--vm-tree` harness) and raises the target from minimalist LXDE to a **faithful
  default-Xubuntu desktop shell**.
- [`MEMORY-MODEL-SPEC.md`](./MEMORY-MODEL-SPEC.md) — the `mmap`/virtual-memory model (no MMU in wasm;
  the platform layer is the page cache). The framebuffer `MAP_SHARED` writeback + dirty-page diff it
  describes is already done; for Xubuntu, implement more **only if a component actually breaks** (see §5 item 10).
- [`WASM-THREADS-SPEC.md`](./WASM-THREADS-SPEC.md) — the wasi-threads runtime feature M8 depends on.
  Still the threading substrate here.
- Research backing the desktop-target decision: see this repo's report under `~/tmp/gui-progress/`
  and the inline analysis that produced this spec (Xfce/X11 is the only full-desktop target that fits
  software-rendered, no-GL wasm).

Progress log + proof: `~/tmp/gui-progress/progress.html`. Proof for every closed item goes in
`~/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/` (screenshots for visual milestones, logs otherwise).

---

## 1. North star

Reproduce the **default Xubuntu 24.04 LTS desktop shell** — visually and behaviorally
indistinguishable from a real Xubuntu session — with **every DE/X/GTK component built and run from
UNMODIFIED upstream source**, all wasm in secure-exec. This is a **Linux-compatibility conformance
milestone**: the deliverable is the *platform surface* (Linux syscalls, AF_UNIX/IPC, fs/VFS,
process/thread/PTY/signal model, the X11 wire protocol, and now **D-Bus**) implemented well enough
that the real Xfce stack boots unmodified. The desktop is the test; the platform layer is the work.

**Why Xubuntu/Xfce** (vs GNOME/Cinnamon/Plasma): Xfce is the most complete *traditional X11* desktop
that does **not** require a GPU/GL compositor, and it is a real mainstream distro default (Xubuntu, MX
Linux). GNOME Shell / Mutter / Cinnamon / KDE Plasma are walled off by their **mandatory OpenGL
requirement** — a hardware/driver concern orthogonal to ABI compatibility, deliberately out of scope
(see §7).

**Mirror target:** Xubuntu 24.04 LTS = **Xfce 4.18**, X11, Greybird theme + elementary-Xfce icons.

---

## 2. Inherited architecture constraints (from SPEC.md)

- **Software rasterization → framebuffer → host blits.** No GL/EGL/GPU in the guest.
- **X11 over a socket.** Core X11 + `XPutImage` fallback (no MIT-SHM).
- **Avoid `dlopen` and shared memory.** Static-link everything; build loadable plugins **static**
  (the lxpanel `--disable-plugins-loading` pattern — no dlopen in the sandbox).
- **Constraint #5 (overriding):** components build/run from unmodified upstream; fix breakage in the
  **native/platform layer** (runtime / sidecar / VFS / X-server / toolchain sysroot+shim), never by
  patching a component's source. Repay any per-lib patches into the libc/sysroot layer.
- **Constraint #4:** build native-parallel observability before guessing; `jj diff` the vendored
  `third_party/*-threads` trees for stray diagnostics before blaming the runtime.
- **TCB security sign-off is a human gate** — surface it, never self-approve. (New surface here: the
  **D-Bus session bus** — see §6.1 and §7.)

---

## 3. Target software — the Xubuntu desktop shell

Authoritative source: the Xubuntu seeds
([`desktop-minimal`](https://github.com/Xubuntu/xubuntu-seed/blob/noble/desktop-minimal) +
[`desktop`](https://github.com/Xubuntu/xubuntu-seed/blob/noble/desktop)). We mirror the **shell + light
bundled apps**, NOT the heavy app payload (see §7). Versions = Xfce 4.18 / Xubuntu 24.04.

### 3.1 Core shell — build these (the "it looks/feels like Xubuntu" set)
| Component | Role | Upstream repo | Status |
|---|---|---|---|
| **xfwm4** | window manager (compositing **OFF** → software) | https://gitlab.xfce.org/xfce/xfwm4 | ⬜ |
| **xfce4-panel** | panel / taskbar / systray / clock | https://gitlab.xfce.org/xfce/xfce4-panel | 🟢 (XU3: panel + clock + tasklist (live window button) + systray + separator + **applicationsmenu** all render, all wasm, via the gmodule static-plugin shim; Greybird theming proven. The applicationsmenu DRAWS (2026-06-25, 23200 bar px) after task#11 was fixed: the hang was GPollFileMonitor's async query_info because wasi had no inotify; fixed by adding sys/inotify.h + no-op inotify runtime stubs so GLib's UNMODIFIED inotify backend compiles + GIO uses the no-op inotify monitor. whiskermenu (C++, the 6th plugin) now also DRAWS -- see its row) |
| **xfce4-whiskermenu-plugin** | the iconic Xubuntu app menu | https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin | 🟢 (XU3: UNMODIFIED whiskermenu 2.8.3 (C++, 24 .cpp) cross-compiled to wasm + integrated as the 6th static panel plugin; the panel bar DRAWS with whiskermenu (2026-06-25, 23200 bar px). Built via `scripts/build-whiskermenu.sh` (direct clang++ compile, no CMake cross-toolchain) + an EXTERNAL_PLUGINS path in build-xfce4-panel.sh. Loads garcon lazily so it avoids the binaryen fpcast file-view gate. The one platform fix: `toolchain/whiskermenu-register.c` (XFCE_PANEL_PLUGIN_REGISTER -> the proper xfce_panel_module_construct that DEFERS the construct to a realize handler, after the panel sets the plugin name) -- mapping the raw construct directly crashed in xfce_panel_plugin_lookup_rc_file on a garbage name) |
| **xfdesktop4** | wallpaper + desktop icons + root menu | https://gitlab.xfce.org/xfce/xfdesktop | 🟢 (XU4: builds + the desktop WALLPAPER + DESKTOP ICONS both render under xfwm4, all wasm. Wallpaper full-screen (Xvfb monitor key "monitorscreen"); the file-icons (Home/Filesystem/Trash special icons) render after the file-view gate fix (g_vfs_get_default -> g_vfs_get_local wrap unblocks the per-icon g_file_query_info enumeration) + the Adwaita icon theme staged + a gtk settings.ini pointing GTK at it. 0 "could not find the icon" errors. Proof 2026-06-25 xu4-xfdesktop.png. Root menu (garcon) still to wire) |
| **Thunar** (+ thunar-volman) | file manager | https://gitlab.xfce.org/xfce/thunar | ⬜ (reuse pcmanfm/libfm work) |
| **xfce4-settings** (xfsettingsd) | settings daemon → XSETTINGS push (theme/font/cursor) | https://gitlab.xfce.org/xfce/xfce4-settings | ✅ (XU1: GTK window themed Greybird via the XSETTINGS push) |
| **xfconf** | settings store (D-Bus service) | https://gitlab.xfce.org/xfce/xfconf | ✅ (XU1: round-trip over GDBus) |

### 3.2 Xfce core libraries (dependencies of the above)
| Lib | Role | Repo |
|---|---|---|
| libxfce4util | base utils | https://gitlab.xfce.org/xfce/libxfce4util |
| libxfce4ui | shared GTK widgets | https://gitlab.xfce.org/xfce/libxfce4ui |
| **libxfce4windowing** | X11/Wayland window abstraction (new 4.18+, used by panel/xfdesktop) | https://gitlab.xfce.org/xfce/libxfce4windowing |
| garcon | freedesktop menu library (we have menu-cache; garcon is the Xfce one) | https://gitlab.xfce.org/xfce/garcon |
| exo | helper widgets / `exo-open` | https://gitlab.xfce.org/xfce/exo |
| thunarx | Thunar extension interface | (in Thunar) |

### 3.3 Light bundled apps — build later (Phase B)
| App | Role | Repo |
|---|---|---|
| xfce4-terminal | terminal (VTE) — reuse the kernel-PTY path | https://gitlab.xfce.org/apps/xfce4-terminal |
| xfce4-appfinder | app launcher | https://gitlab.xfce.org/xfce/xfce4-appfinder |
| xfce4-notifyd | notifications daemon (`org.freedesktop.Notifications`) | https://gitlab.xfce.org/apps/xfce4-notifyd |
| mousepad | text editor (GTK3) | https://gitlab.xfce.org/apps/mousepad |
| ristretto | image viewer | https://gitlab.xfce.org/apps/ristretto |
| catfish | file search | https://gitlab.xfce.org/apps/catfish |
| xfce4-screenshooter | screenshots | https://gitlab.xfce.org/apps/xfce4-screenshooter |
| VTE | terminal widget (xfce4-terminal dep) | https://gitlab.gnome.org/GNOME/vte |

### 3.4 Theming & data — **stage as fixtures, no code** (highest feel-per-effort)
| Asset | Role | Repo / source |
|---|---|---|
| **xubuntu-default-settings** | **the default xfconf channels** (panel layout, Greybird, elementary-xfce, wallpaper) — the single biggest "looks like Xubuntu" lever | https://launchpad.net/ubuntu/+source/xubuntu-default-settings |
| greybird-gtk-theme | default GTK/xfwm theme | https://github.com/shimmerproject/Greybird |
| elementary-xfce | icon theme (mostly PNG → avoids librsvg) | https://github.com/shimmerproject/elementary-xfce |
| xubuntu-artwork / wallpapers | wallpaper | https://launchpad.net/ubuntu/+source/xubuntu-artwork |
| dmz-cursor-theme, fonts-noto, fonts-symbola | cursors + fonts | (distro) |

### 3.5 Infrastructure
| Component | Role | Repo |
|---|---|---|
| **dbus** (`dbus-daemon`, dbus-x11) | **session message bus** — run as an untrusted guest | https://gitlab.freedesktop.org/dbus/dbus |

---

## 4. Reference docs (the protocols we must satisfy)

- **D-Bus specification** — https://dbus.freedesktop.org/doc/dbus-specification.html
- **XSETTINGS spec** (theme/font/DPI push) — https://specifications.freedesktop.org/xsettings-spec/xsettings-spec-latest.html
- **EWMH** (`_NET_*` WM hints, used by panel/taskbar/xfdesktop) — https://specifications.freedesktop.org/wm-spec/latest/
- **ICCCM** (base X11 client/WM conventions) — https://www.x.org/releases/X11R7.7/doc/xorg-docs/icccm/icccm.html
- **Desktop Entry spec** (`.desktop`) — https://specifications.freedesktop.org/desktop-entry-spec/latest/
- **Desktop Menu spec** (`.menu`, the data layer) — https://specifications.freedesktop.org/menu-spec/latest/
- **Icon Theme spec** — https://specifications.freedesktop.org/icon-theme-spec/latest/
- **Notification spec** — https://specifications.freedesktop.org/notification-spec/latest/
- **Autostart spec** — https://specifications.freedesktop.org/autostart-spec/latest/
- **Base Directory spec** (`XDG_*`) — https://specifications.freedesktop.org/basedir-spec/latest/
- **System Tray / StatusNotifier** — https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/
- **Xfce 4.18 docs** — https://docs.xfce.org/ · per-component docs under https://docs.xfce.org/xfce
- **Xubuntu seed (authoritative default set)** — https://github.com/Xubuntu/xubuntu-seed
- **GTK3 / GLib / GIO** (toolkit, already ported in M8) — https://gitlab.gnome.org/GNOME/gtk (branch `gtk-3-24`)

---

## 5. Things we need to IMPLEMENT (platform layer — the actual deliverable)

Everything here is in the runtime/sidecar/VFS/toolchain, NOT in the components (constraint #5).

1. **D-Bus session bus (NEW — the linchpin).** ⬜ Run stock `dbus-daemon` as an **untrusted guest**
   bound to an AF_UNIX socket in the kernel socket table; desktop apps connect over the same brokered
   socket path they use for X11. Provide `DBUS_SESSION_BUS_ADDRESS`. Keeps the bus **out of the TCB**
   (it routes guest↔guest messages, enforces nothing security-critical). **TCB note:** new IPC surface
   — surface for human sign-off (§7) even though running it as a guest is the conservative choice.
2. **xfconf + xfsettingsd bring-up.** ✅ (XU1) xfconf + xfsettingsd run as guests; the xsettings xfconf
   channel (Net/ThemeName=Greybird, IconThemeName=elementary-xfce) is staged and pushed as XSETTINGS so
   a GTK app/panel themes Greybird live. (xubuntu-default-settings full channel set still to stage.)
3. **Static panel plugins.** ✅ (XU3, mostly) The sandbox has no dlopen, so `toolchain/gmodule-shim.c`
   (linked + `-Wl,--wrap=g_module_*`) resolves each plugin's entry from a generated table; each plugin
   `.o` is rebuilt with a compile-time entry-symbol rename. Five plugins are static-linked + load:
   separator, clock, tasklist, systray, applicationsmenu. The panel + plugins stay UNMODIFIED upstream.
   whiskermenu (C++) is the 6th plugin via the EXTERNAL_PLUGINS path (external lib + C++ stdlib link + the
   realize-deferred construct shim). All 6 plugins draw.
4. **Hand-launched session harness (bypass xfce4-session).** ⬜ A launcher script
   (`scripts/test-xu-session.sh`) sequencing `dbus → xfsettingsd → xfwm4 → xfce4-panel → xfdesktop →
   thunar --daemon`, gated on readiness — the Xfce analogue of `test-m8-lxde.sh`. Avoids ICE/SM +
   autostart complexity (lose session save/restore, acceptable).
5. **Disable fork/exec helper daemons.** ⬜ Build/run Thunar with **gvfs and tumbler disabled**
   (local-only, no thumbnailer) to stay within the brokered-spawn model.
6. **xfdesktop wallpaper throughput.** ⬜ Full-screen wallpaper paint stresses the framebuffer budget;
   reuse + extend the M8.6 delta-encoding so a static wallpaper costs ~0/frame. Validate at desktop
   resolutions (≥1024×768), beyond M8's 800×600.
7. **VTE + kernel PTY** for xfce4-terminal. ⬜ Heavier than `st`; the PTY path exists.
8. **Icon-theme cache + gdk-pixbuf loaders.** ⬜ Run `gtk-update-icon-cache` over staged
   elementary-xfke; confirm PNG loaders cover it (avoid the librsvg closure where possible).
9. **Repay & generalize** any per-component breakage into the sysroot/runtime (constraint #5),
   continuing the libX11/libxcb/libxtrans host_net repayment already tracked in `SPEC.md`.
10. **`mmap` / virtual-memory model — implement ONLY if absolutely necessary.** ⬜ See
    [`MEMORY-MODEL-SPEC.md`](./MEMORY-MODEL-SPEC.md) for the full model: wasm has no MMU, so the
    platform layer stands in for the page cache. The framebuffer's `MAP_SHARED` writeback +
    dirty-page diff is **already done** (item 6 reuses it), and the high-volume rest (fontconfig /
    freetype / glib `gschemas.compiled` / GResource) is `MAP_PRIVATE` **read-only pass-through** that
    needs no new work. **Do NOT build anything more unless a component actually breaks on it.** The
    only genuinely-hard case — coherent cross-process `MAP_SHARED` shared-memory (dconf-style change
    notification) — is **designed out** by Xfce's xfconf-over-D-Bus (item 2), so don't pre-build it.
    If a stray GSettings/dconf-backend app forces the issue, implement the minimum from
    `MEMORY-MODEL-SPEC.md` §6, nothing speculative.
    **Performance trigger (the one case where you DO take the detour proactively):** if an operation
    is **markedly slower than it would be on a real OS** — a guest spends time the same call would not
    cost natively — suspect the missing page-cache / MMU dirty-tracking (a full-buffer writeback
    crossing the sandbox boundary, as in the M8.6 framebuffer spin) and **take the detour to implement
    the targeted mmap/diff optimization**. A perf gap vs native is the signal that justifies the work;
    correctness-only parity is not. See `MEMORY-MODEL-SPEC.md` §5–§7.

---

## 6. Milestones (DoD = the acceptance bar)

> Naming: **XU#** to distinguish from `SPEC.md`'s M#. Order is dependency-first.

- **XU0 — D-Bus session bus.** ✅ **DONE (2026-06-24).** `dbus-daemon` + `dbus-send` + `dbus-monitor`
  (dbus 1.14.10) cross-compile from UNMODIFIED upstream (`scripts/build-dbus.sh`) and run as wasm guests
  over the host_net AF_UNIX socket layer. **Acceptance met:** the round-trip harness (host `--bus-test` +
  `scripts/test-xu0-dbus.sh`) launches the daemon as a live session bus (binds `/tmp/.dbus/session` from
  `scripts/prepare-dbus-fixtures.sh`'s session.conf), and a guest `dbus-send ListNames` gets a
  `method_return` while a guest `dbus-monitor` observes the `NameAcquired`/`NameLost`/`NameOwnerChanged`
  signals — all wasm. **Result: `PASS (method_return=3 signals=4)`**; proof in
  `~/tmp/gui-progress/2026-06-24T20/xu0-dbus-roundtrip.txt`. Platform fixes (all constraint #5, dbus
  source untouched):
  - **Build/feature detection:** configure WITHOUT `--allow-undefined` for accurate link-tests (else
    false Solaris `getpeerucred`→`<ucred.h>`); force-detect `getrlimit/setrlimit/socketpair/poll` (their
    link-tests false-negative under no-`--allow-undefined`); `-DSO_PEERCRED=17 -D_GNU_SOURCE` so dbus's
    `#ifdef SO_PEERCRED` credential path (its only EXTERNAL-auth mechanism) compiles in.
  - **wasi-compat stubs:** `setgroups()` no-op; `getrlimit` fills `RLIM_INFINITY`; `socketpair` via pipe.
  - **Synthesized `/dev` char devices** in the wasm fs (`/dev/null`, `/dev/urandom`, …).
  - **`dbus_creds.c`** (`registry/native/patches/wasi-libc-overrides/`, linked into the three binaries):
    host_net AF_UNIX has no SCM/ancillary or real SO_PEERCRED, and its fds (≥0x40000000) aren't in the
    WASI fd table, so dbus's stock auth + message I/O stalled. It provides, for host_net fds only:
    `recvmsg()` (1-byte credential read), `getsockopt(SO_PEERCRED)`→uid 0 (the single sandbox identity,
    so EXTERNAL auth succeeds), `sendmsg()` (message writes via send-per-iovec), and `__wrap_read`
    (route `read()`→`recv()`, surfacing a blocked read as `EAGAIN` so dbus doesn't treat would-block as
    a fatal disconnect).
  - **Runtime `net_poll` POLLOUT fix** (`node_import_cache.rs`): corrected POLLOUT from the wrong 0x002
    to the real wasi `<poll.h>` value 0x004 (0x002 is POLLPRI), keeping 0x002 as a compat bit. dbus uses
    `poll()` (forced `HAVE_POLL`; its `select()` fallback can't represent fd 0x40000000 in an `fd_set`)
    and genuinely waits on write-readiness to flush its auth `OK`/replies, which the X libs never did.
  - The X server M8 spine still passes (twm decorates a window) — no regression. **Gates everything else;
    now unblocked.** TCB note: the daemon is an untrusted guest; no host-fd/privilege expansion.
- **XU1 — xfconf + xfsettingsd.** ✅ DONE (2026-06-25). DoD MET: xfconf stores/serves a value over
  D-Bus; xfsettingsd pushes XSETTINGS to a GTK client (theme/font visibly applied) — a GTK window
  rendered in Greybird (not default Adwaita), proven logically (gtk-theme-name=Greybird readback) and
  visually (30% `#cecece` Greybird grey in the framebuffer). **Foundation de-risked first (constraint #4):** xfconf/xfsettingsd reach the
  bus via **GDBus** (GIO's D-Bus client), not libdbus — so the gate is GDBus-over-host_net. Built a GDBus
  probe (`guest-xclient/gdbus-probe.c` + `scripts/build-gdbus-probe.sh`) linked against the threaded
  GLib/GIO stack + the dbus_creds host_net shims, run against the daemon via `--bus-test`. Progress:
  - **GIO AF_UNIX connect fixed (general platform bug):** GLib's `g_socket_connect` writes the native
    address into a `struct sockaddr_storage` and requires `destlen >= sizeof(struct sockaddr_un)` (110);
    wasi's `sockaddr_storage` was only ~34 bytes (`__ss_data[32]`) → every GIO AF_UNIX connect failed
    `G_IO_ERROR_NO_SPACE` ("Not enough space for socket address"). Enlarged it to 128 (Linux `_SS_SIZE`)
    in the wasi sysroot header + recompiled glib's gsocket objects.
  - **GDBus SASL auth works:** the probe now connects and authenticates over host_net (EXTERNAL is
    rejected because GIO's `getuid()`→-1 yields `GCredentials:unknown` so it asserts uid 4294967295 ≠ the
    daemon's SO_PEERCRED 0; it then falls back to **ANONYMOUS**, which the session.conf policy allows).
  - **Cross-thread socket sharing — IMPLEMENTED + VERIFIED.** The root cause was that the host_net
    socket table is per-isolate and each wasm thread is its own V8 isolate, so the GDBus worker thread
    couldn't see the socket the main thread opened (M8's X clients never hit this; X I/O stays on the
    main thread). Fixed across four layers (full writeup +
    [`XU1-SOCKET-SHARING-DESIGN.md`](./XU1-SOCKET-SHARING-DESIGN.md)): runner fd→socketId registry +
    resolve-on-miss; sidecar dispatches a thread's net.* ops against its owning ancestor process and
    waits `net.poll_wait` on the owner's readiness; and the three new RPCs registered in the WASM bridge
    allowlist (wasm.rs / v8_runtime.rs / session.rs / bridge-contract.json). **The worker thread now
    sends Hello and receives the daemon's replies over the main thread's socket; M8 stays green.** Proof
    `~/tmp/gui-progress/2026-06-24T22/xu1-socket-sharing-works.txt`.
  - **GDBus-over-host_net FULLY WORKS — foundation complete.** The last blocker was that `net_poll`
    (what GLib's `poll()` routes to) did not poll **kernel-pipe fds**, so a GMainContext **GWakeup pipe**
    was never readable and GDBus's cross-thread completion wakeup (worker → blocked `g_bus_get_sync`)
    never fired → livelock (M8's GTK never hit it; it polls the X socket). Fixed with a `__kernel_fd_poll`
    RPC (`kernel.poll_fds`, non-consuming) wired into net_poll. **The gdbus-probe now passes:
    `g_bus_get_sync(SESSION)` connects and `ListNames` returns 2 names, all wasm**; M8 (`test-m5-twm`)
    and XU0 (dbus 3/4) stay green. Proof `~/tmp/gui-progress/2026-06-24T23/xu1-gdbus-pass.txt`.
  - **xfconf half DONE ✅ (2026-06-24): "xfconf stores/serves a value over D-Bus", all wasm.** Built
    UNMODIFIED libxfce4util 4.18.2 + xfconf 4.18.3 (xfconfd + xfconf-query) via autotools
    (`scripts/build-libxfce4util.sh`, `scripts/build-xfconf.sh`). `scripts/test-xu1-xfconf.sh` →
    **PASS**: dbus-daemon + xfconfd (registers `org.xfce.Xfconf`) + `xfconf-query --set hello-xu1` then
    `xfconf-query` GET returns `hello-xu1`, a real xfconfd round-trip over GDBus. Platform fixes
    (constraint #5): gettext stubs + `--as-needed` stripping in the clang wrapper (build tooling); a
    `toolchain/glib-compat.c` shim (`g_variant_builder_init_static`→`g_variant_builder_init` for the
    host gdbus-codegen/target-glib-2.78 version skew, + BSD `err.h`/`daemon` stubs); runtime
    `fd_datasync` WASI import (xfconfd fsyncs its channel XML). M8 + XU0 stay green. Proof
    `~/tmp/gui-progress/2026-06-24T??/xu1-xfconf-pass.txt`.
  - **xfsettingsd BUILT (2026-06-24), all wasm.** Built UNMODIFIED libxfce4ui 4.18.6 + exo 4.18.0 +
    garcon 4.18.2 + xfce4-settings 4.18.4 → `xfsettingsd.wasm` (15.8 MB) via the proven Xfce recipe.
    xfsettingsd instantiates + runs (past ALL platform gaps), reaching `Unable to open display` (the
    expected failure with no X server in a bus-only smoke), proving the binary works and just needs an X
    server to publish XSETTINGS. Platform fixes: `libsetjmp.a` appended last (`__wasm_setjmp`,
    `-wasm-enable-sjlj` via GTK's gdk-pixbuf); GTK transitive libs (stub `atk-bridge-2.0` + epoxy + X ext
    libs) appended via LIBS (non-static pkg-config drops them); build only the xfsettingsd target (the
    GUI dialog binaries' huge links hit "argument list too long"). Scripts: `build-libxfce4ui.sh`,
    `build-exo.sh`, `build-garcon.sh`, `build-xfce4-settings.sh`.
  - **Combined X+D-Bus harness BUILT (2026-06-24).** `--xdemo` gained `--dbus <daemon>` (launch the
    dbus-daemon as a long-lived guest + auto-inject `DBUS_SESSION_BUS_ADDRESS` into clients) and
    `--dbus-service <svc>` (launch xfconfd-style services before the gated X-client loop, fully settled,
    so they don't starve a connecting X client) + forwards `GDK_DEBUG`/`NO_AT_BRIDGE`. Fixtures:
    `scripts/test-xu1-xsettings.sh` stages the dbus session.conf, a `/etc/machine-id`, and the xsettings
    xfconf channel (Net/ThemeName=Greybird). Verified the combined harness's X path works (twm+xwin map),
    and xfconfd connects to the bus (machine-id + DBUS injection).
  - **xfsettingsd RUNS over X + D-Bus (2026-06-24).** The "Unable to open display" blocker was TWO
    missing GTK link flags (found by tracing: both lxpanel and xfsettingsd poll the X socket `ev=3 re=2`,
    but lxpanel then SENDS the X setup while xfsettingsd never did): `-Wl,--wrap=writev` (libxcb writes
    the X setup via writev, which must route to the host_net override; without it the write silently
    fails on the X socket) and `-Wl,-z,stack-size=8388608` (GTK's deep init stack overflows the wasm
    default). With both, **xfsettingsd opens the display, connects to xfconf, and runs its settings
    helpers past gtk_init** (`test-xu1-xsettings.sh` → LIVE). These two flags are now mandatory for every
    GTK component (XU2 xfwm4, XU3 panel, …). Proof `~/tmp/gui-progress/2026-06-24T??/xu1-xfsettingsd-live.txt`.
  - **★ FULL XU1 ACCEPTANCE DONE ✅ (2026-06-25): the complete xfconf → xfsettingsd → X-XSETTINGS → GTK
    chain works end-to-end, all wasm, and a GTK window themes itself with the real Xubuntu Greybird.**
    `scripts/test-xu1-greybird.sh` → **PASS**: dbus-daemon + xfconfd (D-Bus service serving the
    `Net/ThemeName=Greybird` xsettings channel) + **xfsettingsd** (X-client: opens the display, reads
    xfconf over the bus, publishes the XSETTINGS manager selection to the X server) + **gtk-hello**
    (X-client: reads XSETTINGS, themes itself). Proof is twofold: (1) **logical** — gtk-hello's readback
    prints `XU1-XSETTINGS: gtk-theme-name=Greybird gtk-icon-theme-name=elementary-xfce gtk-font-name=Sans 10`,
    i.e. all three channel values flowed through xfsettingsd's X publish into GTK; (2) **visual** — the
    framebuffer PNG is **30% Greybird grey `#cecece`** (Greybird's exact `theme_bg_color`; default Adwaita
    would be near-white `#f6f5f4`), the GTK window rendered in the Greybird theme. The real prebuilt
    Greybird gtk-3.0 theme (Ubuntu noble `greybird-gtk-theme` deb, vendored `third_party/greybird-theme/`,
    staged by `scripts/prepare-themes.sh`) is loaded BY NAME from the XSETTINGS push, not pinned. **Key
    sequencing fix:** xfsettingsd is a `--client` (X-gated loop, settle-gated so it owns `_XSETTINGS_S0`
    before gtk-hello reads it), NOT a `--dbus-service` (those launch before the X server is up → "Unable
    to open display"). xfconfd stays a `--dbus-service` (pure D-Bus). gtk-hello build = `build-gtk-app.sh`
    (now fixed: links `libhostcompat.a` + the two mandatory GTK flags). M8/XU0 stay green. Proof
    `~/tmp/gui-progress/2026-06-25T00/xu1-greybird-{gtk.png,readback.txt,log}`. **XU1 COMPLETE.**
- **XU2 — xfwm4 (the real Xfce WM).** 🟢 DECORATION DONE (2026-06-25); XTEST move/resize remaining. DoD:
  xfwm4 (compositing off) decorates a GTK window with the Greybird theme; move/resize + workspaces via
  XTEST. Proof screenshot. (Supersedes M8.2's openbox as the Xubuntu WM.)
  - **★★ ACHIEVED (2026-06-25 iter6): xfwm4 fully decorates a GTK window with the Greybird theme, all
    wasm** — titlebar with the centered title, the min/max/close window buttons + the left menu button,
    and Greybird borders, all rendering. Unblocked by the stdio `fseek` fix (below). Proof
    `~/tmp/gui-progress/2026-06-25T02/xu2-ACHIEVED-greybird-decoration.png`.
  - **XTEST move/resize — harness follow-up (not an xfwm4/decoration defect):** `scripts/test-xu2-xfwm4-move.sh`
    injects an XTEST titlebar drag, but the host's `--inject` path HANGS against xfwm4 (the host XTEST
    connection vs xfwm4's server-side move grab — the same case M8's openbox needed a dedicated
    "host-assisted drag path" for; plain `buttondn`/motion/`buttonup` doesn't drive a grabbing WM and
    stalls the host's readback). The decoration proof above already establishes xfwm4 works as the Xfce
    WM; closing this needs the host-assisted-drag path extended to xfwm4's grab (a harness change, xfwm4
    unmodified). Tracked as a follow-up; XU2's substantive DoD (Greybird decoration) is met.
  - **★★ THE FIX (constraint #5, runtime — fixes XU2 AND a broad class of bugs): passthrough/host-backed
    file `fd_read` now reads POSITIONALLY from the tracked `entry.offset` (and advances it), not from the
    host fd's own offset (`position=null`).** `_fdSeek` only updated `entry.offset` and never moved the
    host fd, so a `null`-position read ignored the seek: `fseek(0)+fread` returned 0 (proven by
    `fread-probe.c`: sequential fread=315 but fseek-then-fread=0; after the fix, =16). That broke EVERY
    loader that sniffs-then-rewinds — `gdk_pixbuf_new_from_file` → "Read Error" on all decoration PNGs,
    xfwm4 `xpm_image_load` → "Cannot read Pixmap header" ×672 (now 0). Fix in `crates/execution/src/wasm.rs`
    `_fdRead` passthrough branch (asset bump 106→108). M8 regression green (openbox+xclock still decorates).
    Reusable probes `guest-xclient/{fread-probe,gtkcairo-a1,xrender-a1-test,pixman-a1-test,rfmt-probe}.c` +
    env-gated `SECURE_EXEC_FD_TRACE`. (cairo→A1 was a red herring — it always worked; the image just never loaded.)
  - **BUILT + RUNS as a WM ✅ (2026-06-25):** built UNMODIFIED libXinerama 1.1.5 + libwnck 3.24.1 (the
    staged libwnck was an ancient 3.4.9 < the `>= 3.14` xfwm4 needs; 3.24.1 is the last autotools series,
    3.32+ is meson) + xfwm4 4.18.0 → `xfwm4.wasm` (15.9 MB), all wasm, via `scripts/build-libwnck.sh` +
    `scripts/build-xfwm4.sh` (compositor/startup-notification/xpresent off — no GPU; the Xfce autotools +
    two mandatory GTK flags recipe). xfwm4 takes over the root, connects to xfconf over D-Bus, and
    **places the gtk-hello window centered** (exact WM placement). `scripts/test-xu2-xfwm4.sh` runs the
    full stack (dbus + xfconfd serving the `xfwm4` channel theme=Greybird + xfwm4 + gtk-hello).
  - **Two platform fixes landed:** (1) `--datadir=/usr/share` in the xfwm4 build (it loads its `defaults`
    file from compile-time `PACKAGE_DATADIR`, not XDG; the wasm-prefix path is absent in the VM → "Missing
    defaults file" → exit 1). `scripts/prepare-xfwm4.sh` stages that defaults file + the bundled Default
    fallback theme. (2) `prepare-themes.sh` transcodes XPM-only decoration images → PNG (our gdk-pixbuf is
    PNG-only; several Greybird borders ship only `.xpm`).
  - **★★ ACTUAL ROOT CAUSE FOUND (2026-06-25 iter5) — it is a stdio `fseek` bug, NOT cairo (this REVERSES
    the iter1–4 cairo conclusion below).** `fseek(0,SEEK_SET)` then `fread()` returns **0** on a stdio
    `FILE*` over a kernel-VFS (`--vm-tree`) file, while raw POSIX `open`/`lseek(0)`/`read` works (proven by
    `guest-xclient/fread-probe.c` on a valid 315-byte PNG: sequential fread=315, but fseek-then-fread=0;
    raw read-after-lseek=8). EVERY asset loader does sniff-then-rewind via stdio, so they all fail:
    `gdk_pixbuf_new_from_file` on every decoration PNG → "Fatal error in PNG image file: Read Error" (RGBA
    and LA alike), and xfwm4's `xpm_image_load` (`fread(1024)+fseek(0)+getc`) → "Cannot read Pixmap header".
    xfwm4 therefore loads NO decoration images → blank/shaped-away decorations. **cairo→A1 is FINE** —
    `guest-xclient/gtkcairo-a1.c` (a GTK app where cairo inits) shows cairo→depth-1(A1) solid fill AND
    xfwm4's exact clear+fill BOTH produce correct masks (`set=1024 left=1024 right=0`); the masks were
    empty only because the image never loaded. NEXT (constraint #5, runner / wasi-libc sysroot): instrument
    the runner's guest-file `fd_seek`/`fd_read` (`node_import_cache.rs` ~13357/13623; the code reads from
    `handle.position` which `fd_seek` sets to 0, so the fault is likely wasi-libc stdio's buffer/EOF
    bookkeeping above the WASI layer), fix it → gdk_pixbuf loads → xfwm4 decorations render → XU2 closes.
    This fseek fix also unblocks any component that seek-reads a VFS file. Probe tools
    `guest-xclient/{fread-probe,gtkcairo-a1}.c`. Proof `~/tmp/gui-progress/2026-06-25T01/xu2-ACTUAL-rootcause-fseek.txt`.
    *(Superseded cairo theory, kept for the record:)*
  - **★ OPEN BLOCKER — ROOT CAUSE NARROWED (2026-06-25, constraint #4 observability):** xfwm4 draws NO
    visible decoration (titlebar band black) even though it reparents + uploads real images. Proven via
    XTRACE + a new `guest-xclient/bgpixmap-test.c` repro on the bare wasm X server:
    - xfwm4 runs as a WM: reparents (3 ReparentNotify), creates the 326×32 titlebar, uploads REAL
      decoration pixel data (18 PutImage, 17–62% nonzero), ~30 CopyArea. Images LOAD fine.
    - The wasm X server (real X.Org Xvfb) passes EVERY primitive xfwm4 uses: solid bg color, background
      pixmap, **tiled** bg pixmap (16×16 tile on a 200×150 win), direct draw, **and SHAPE/`XShapeCombineMask`**
      (left-half mask → left orange, right black). openbox decorates fine in the same workspace.
    - **Root cause:** xfwm4 shapes its title/side/corner decoration windows with a mask via
      `XShapeCombineMask` (frame.c:512-544); the mask is a **depth-1 bitmap drawn with CAIRO**
      (`cairo_xlib_surface_create_for_bitmap`, `xfwmPixmapDrawFromGdkPixbuf`). An all-zero mask shapes a
      window to NOTHING (proven). Since even fully-opaque transcoded decoration PNGs (mask should be all-1s)
      render invisible, **cairo→depth-1(A1)-bitmap rendering produces EMPTY masks in the wasm cairo build**
      → every decoration window is shaped away → invisible (black root shows through). openbox/twm draw
      rectangular decorations directly and never shape, so they work.
    - **✓ CONFIRMED (2026-06-25 iter3) via the `rootcolor.c` discriminator:** painting the X root MAGENTA
      then running the full stack, the titlebar band + left border around the managed window show **MAGENTA
      (root) not black** → the decoration windows are **SHAPED AWAY to nothing** (empty mask), NOT painted
      black. So cairo→depth-1(A1) rendering is a NO-OP in the wasm build (the A1 XRender format exists —
      rfmt-probe — and core-X masks work — bgpixmap-test — so the gap is specifically the **cairo→A1 path**,
      likely the XRender PictOpSrc-to-A1-picture that cairo emits for mask draws). (`cairomask-test.c` is
      confounded — cairo crashes on first use in a minimal non-GTK binary — so the discriminator is authoritative.)
    - **✓✓ FULLY LOCALIZED to CAIRO (2026-06-25 iter4):** every layer tested in isolation as a standalone
      wasm guest — X-server A1 (core-X fill, `XRenderFillRectangle`, `XGetImage`+`XPutImage` round-trip),
      bg-pixmap+tiling+SHAPE, the `PictStandardA1`/depth-1 formats, **and pixman `pixman_image_fill_boxes`
      on a `PIXMAN_a1` image** (`set_bits=1024/2048`, exactly the left half) — ALL WORK. cairo→depth-24
      color pixmap works (xfwm4's 18 real PutImages). ONLY cairo→depth-1(A1) is broken. So it is NOT the X
      server, NOT pixman, NOT the XPutImage upload — it's specifically cairo's `cairo_xlib_surface_create_for_bitmap`
      + A1-fill orchestration (cairo has every working primitive but drops the A1 result; likely a wrong
      render/pixman-format pick for depth-1 in the wasm cairo build).
    - NEXT (constraint #5, platform = the wasm **cairo** build): first get a cairo test harness running
      (cairo crashes on first `cairo_xlib_surface_create` in a minimal non-GTK/non-threaded binary — a cairo
      init/TLS issue, NOT the A1 bug; it works inside GTK apps), then reproduce cairo→A1 and inspect
      `_cairo_xlib_surface_create_for_bitmap`'s render-format selection, or read cairo-xlib-surface.c's
      depth-1 path for a wasm-build miswire. xfwm4 stays UNMODIFIED. Repro tools
      `guest-xclient/{rootcolor,bgpixmap-test,rfmt-probe,xrender-a1-test,pixman-a1-test,cairomask-test}.c`. Proof
      `~/tmp/gui-progress/2026-06-25T01/xu2-{CONFIRMED-decorations-shaped-away,A1-primitives-all-work}.png`
      + `xu2-elimination-chain.txt`.
    - Side finding (real platform gap, non-fatal): xfwm4's `xpm_image_load` does `fread(1024)+fseek(0,SEEK_SET)+getc`
      → "Cannot read Pixmap header" ×672 = a wasi-libc fseek-after-fread stdio-buffer bug; PNG fallback covers it.
- **XU3 — xfce4-panel + whiskermenu.** 🟢 DONE (2026-06-25): panel core + applicationsmenu + **whiskermenu** all draw, all wasm. whiskermenu (UNMODIFIED 2.8.3, C++) cross-compiled + integrated as the 6th static plugin; bar DRAWS (23200 px). The realize-deferred construct shim (toolchain/whiskermenu-register.c) was the one platform fix.
  DoD: the panel renders with the app menu, taskbar, clock, systray. The panel bar with
  applicationsmenu + clock + separator DRAWS (proof: ~/tmp/gui-progress/2026-06-25T12/xu3-applicationsmenu-bar-*.png,
  23200 bar px, clean upstream GLib). task#11 (the applicationsmenu hang) CLOSED via the inotify fix
  (sys/inotify.h + no-op runtime stubs; see status log). REMAINING: whiskermenu (the Xubuntu-DEFAULT menu, a
  C++ xfce4-whiskermenu-plugin build, not yet fetched/built; should render like applicationsmenu since both
  load garcon lazily).
  - **Panel core BUILT (2026-06-25):** UNMODIFIED xfce4-panel 4.18.6 → `xfce4-panel.wasm` (15.9 MB) +
    `libxfce4panel-2.0.a` (the plugin SDK), via `scripts/build-xfce4-panel.sh` (the Xfce autotools + GTK
    recipe; deps cairo/exo/garcon/garcon-gtk3/gtk3/libwnck/libxfce4ui/util all already built; no
    libxfce4windowing needed for 4.18). `--datadir=/usr/share`.
  - **★ TWO platform blockers (both new, both general to Xfce static linking):**
    1. **libxfce4ui GResource not registered → Gtk-ERROR abort. ✅ FIXED (2026-06-25).** A libxfce4ui
       dialog's UI is a GResource (`/org/xfce/libxfce4ui/libxfce4ui-dialog-ui.ui`); its register-constructor
       lives in `libxfce4ui-resources.o` INSIDE `libxfce4ui-2.a`, but archive-pull drops it (nothing
       references it), so the ctor never runs → "resource does not exist" → `unreachable`. Fix: **extract
       `libxfce4ui_2_la-libxfce4ui-resources.o` from the archive and link it as a plain object** (always
       included → ctor runs → bundle registered). `--undefined`/`--whole-archive` via LIBS did NOT work
       (automake dedups the repeated `-l`); also `rm` the panel binary first (make won't relink it for a
       LIBS-string change). Baked into `build-xfce4-panel.sh`; libxfce4ui untouched. Helps XU4/XU5 too.
       Confirmed: the dialog renders correctly and the **panel bar renders** (empty — plugins still blocked).
    1b. **`fork()` for first-run config migration. ✅ FIXED (2026-06-25).** With no config the panel `fork`s
       `xfce4-panel-migrate` → "Failed to fork" (wasm has no fork). Fix: pre-stage an xfconf
       `xfce4-panel.xml` (configver=2) so the panel finds a valid config and skips migration entirely
       (`scripts/prepare-xfce4-panel.sh`; fixture, not a patch). **Result: the panel BAR renders** — a
       full-width 798×28 bar, no crash (`scripts/test-xu3-panel.sh` → "BAR renders"). Proof
       `~/tmp/gui-progress/2026-06-25T03/xu3-panel-bar-renders.png`. The bar is empty (no plugins yet).
    2. **gmodule/dlopen static plugins — ✅ MECHANISM PROVEN (2026-06-25): the separator plugin loads
       through the shim, no dlopen, reproducible.** `toolchain/gmodule-shim.c` (linked + `-Wl,--wrap=g_module_*`)
       intercepts the panel's `g_module_open(<.so>)`/`g_module_symbol`, parses the plugin name from the
       path, and resolves the entry from the generated `gmodule-plugins.gen.c` table. Each plugin `.o` is
       rebuilt with a compile-time entry-symbol rename (`-D<entry>=<name>_module_entry`, appended to the
       plugin's CPPFLAGS so the sysroot/`-include wasi-compat.h` survive) to avoid the all-plugins-export-
       the-same-symbol clash. Confirmed end-to-end via `GMODSHIM open separator` + `GMODSHIM resolved
       xfce_panel_module_init` (debug build) — the panel loads + constructs the plugin with no dlopen and
       no "no module found". Fully baked into `build-xfce4-panel.sh` (`STATIC_PLUGINS="name:entry"` list)
       + `prepare-xfce4-panel.sh` (stages a stub `.so` at the compile-time `PANEL_PLUGINS_LIB_DIR` so the
       panel's `g_file_test` passes, the plugin `.desktop` with `X-XFCE-Internal=TRUE`, and the config
       entry). `test-xu3-panel.sh` → "BAR renders" with the separator. ✅ clock + separator RENDER (the panel shows the live date/time "2026-06-25 03:48" + the separator
       line; proof gui-progress/2026-06-25T03/xu3-panel-clock-separator.png; the loop links all of a multi-
       file plugin's objects). NEXT: add tasklist (window-buttons) + systray to
       `STATIC_PLUGINS`/`PLUGINS`, then build + static-link
       xfce4-whiskermenu-plugin (a separate package) for the app menu = XU3 DoD. (Stub-`.so` paths are the
       absolute wasm-prefix path; a `--libdir=/usr/lib` rebuild would make them clean `/usr/lib/...`.)
    2z. **(historical) gmodule/dlopen static plugins.** All 13
       panel plugins are external `.so` loaded via `g_module_open(<path>)`+`g_module_symbol`; the sandbox
       has no dlopen, and they can't all be statically linked under their real names (every plugin exports
       the same entry symbol). **Approach (constraint #5; panel+plugins UNMODIFIED — toolchain shim + a
       compile-time `-D` rename):** `toolchain/gmodule-shim.c` (DONE, compiles, exports
       `__wrap_g_module_open/open_full/symbol/close/make_resident/error/supported`) intercepts the panel's
       g_module calls (link with `-Wl,--wrap=g_module_*`), parses the plugin name from the `.so` path, and
       resolves the entry from a generated static table `panel_static_plugin_lookup(name, symbol)`. Per
       plugin: compile its `.c` with the entry symbol renamed (`-Dxfce_panel_module_init=<name>_init` for
       GObject/TypeModule plugins like **separator**; `-Dxfce_panel_module_construct=<name>_construct` for
       simple plugins), link it into the panel, add `{name, symbol, fn}` to the gen table. **Remaining
       wiring (per plugin):** (a) build the plugin objects + deps into the panel; (b) gen-table entry;
       (c) stage a STUB `.so` at the compile-time `PANEL_PLUGINS_LIB_DIR` (the panel `g_file_test`s the
       file exists before opening — set `--libdir=/usr/lib` or stage at the wasm-prefix path); (d) stage
       the plugin `.desktop` with `X-XFCE-Internal=true` (→ INTERNAL mode, no wrapper fork) at the panel
       data dir; (e) add the plugin to the xfconf config. NEXT: prove the path with one plugin
       (separator → clock), then tasklist/systray/clock + build xfce4-whiskermenu-plugin.
- **XU4 — xfdesktop.** 🟢 WALLPAPER + DESKTOP ICONS DONE (2026-06-25). The icons unblocked once the file-view
  gate was solved (g_vfs_get_default->g_vfs_get_local wrap) + the Adwaita icon theme was staged + a GTK
  settings.ini pointed GTK at it (drop gtk-theme-name: the heavy Greybird CSS load stalled the render to black;
  icon-theme-name alone is enough). wallpaper 29727 cells + 273 icon cells, 0 missing-icon errors. Root menu (garcon) remains.
  Wallpaper renders full-screen under xfwm4 (proof: ~/tmp/gui-progress/2026-06-25T13/xu4-*.png; relinked
  against the inotify libgio, no regression). Desktop file-icons do NOT populate -- gated on the binaryen
  --fpcast-emu defect below (xfdesktop's icon view enumerates ~/Desktop eagerly via g_file_query_info).
- **XU5 — Thunar.** 🟡 Thunar 4.18.10 builds (rc=0 -> thunar.wasm 16.3MB) + runs, all wasm. CLEARED:
  file-monitor hang (task#11 inotify fix), D-Bus session bus, clock, AND the file-view gate (the
  g_vfs_get_default->g_vfs_get_local wrap is wired into build-thunar.sh; Thunar runs with NO unreachable
  trap now). Test: scripts/test-xu5-thunar.sh (xfwm4 + thunar browsing /root). ONE remaining blocker: the
  Thunar-specific GtkApplication-ACTIVATION block -- Thunar reaches GtkApplication startup (connects to the
  session manager, the GNOME/Xfce/inhibit session proxies fail benignly) + the volume-monitor init, then
  IDLES with no window: no "activate"/command-line -> window, spins=0 (not a busy-hang), 0 X CreateWindow.
  The minimal gtkapp-probe DID map a window, so this is Thunar's startup specifically. Suspects: the
  GtkApplication primary-instance DBus name acquisition (g_bus_own_name async completion) not advancing the
  activation, or Thunar blocking on the volume monitor (last log line before the idle is the
  GIO_USE_VOLUME_MONITOR='null' "can't find module 'null'" warning; xfdesktop emits the same but does not use
  the volume list, Thunar's window does). Build recovery note: the shared tree lost Thunar's 24 Makefile.in
  (untracked); restored from third_party/thunar.tar.bz2 (a dist tarball).
- **★ THE FILE-VIEW GATE -- ✅ SOLVED 2026-06-25 (was the single blocker for XU4 icons + XU5 folders + XU6 file
  dialogs).** FIX: `toolchain/gio-vfs-local-shim.c` + `-Wl,--wrap=g_vfs_get_default` (build-gtk-app.sh
  `SECURE_EXEC_GIO_VFS_LOCAL=1`) wraps g_vfs_get_default -> g_vfs_get_local. In a module-less sandbox (no
  dlopen -> no gvfs daemon backends) the default GVfs is ALWAYS the local vfs, so this is the semantically
  correct value AND bypasses the trapping `_g_io_modules_ensure_loaded` machinery. VALIDATED on fileview-probe:
  g_vfs_get_default / g_file_new_for_path / g_file_get_path / **g_file_query_info all PASS** (err=none). A
  toolchain --wrap shim (constraint #5, like gmodule-shim.c / the writev wrap), no component patched. Apply to
  xfdesktop/Thunar/file-dialog builds. (Root cause of the underlying trap -- a built-in GIO type registration in
  ensure_loaded -- is sidestepped, not yet pinpointed; only matters if a component needs GSettings/volume-monitor,
  which Xfce avoids via xfconf + GIO_USE_VOLUME_MONITOR=null.)
  <details><summary>diagnosis history</summary>
  Originally g_vfs_get_default() trapped `RuntimeError: unreachable`, blocking the ENTIRE GFile object path
  (g_file_new_for_path / get_path / query_info all call g_vfs_get_default first), while raw lstat + glib
  path-based g_file_get_contents (no GVfs) WORK. **REDIAGNOSED 2026-06-25 -- this OVERTURNED the prior "binaryen --fpcast-emu defect" theory:**
  a NO_FPCAST build (build-gtk-app.sh `SECURE_EXEC_NO_FPCAST=1`, drops --fpcast-emu entirely) STILL traps at
  the same spot, and the trap is `unreachable` (a REAL unreachable: abort / g_assert_not_reached / NULL-vtable
  call / an unimplemented stub) -- NOT a call_indirect "signature mismatch". So this is a GIO VFS-INITIALIZATION
  trap, not a codegen/binaryen issue. The earlier "shifts per build" was just function-index renumbering, not
  fpcast layout-dependence. Bisect (guest-xclient/fileview-probe.c, run via host --exec): lstat works, then
  g_vfs_get_default is the FIRST call to trap (before g_vfs_get_local / get_file_for_path / query_info).
  CANDIDATES for the unreachable: the GIO module scan (g_module_open dlopen stub), an unregistered GLocalVfs
  type (g_object_new on a 0 type -> abort), or g_assert_not_reached in the extension-point/module-load path.
  The panel/applicationsmenu/whiskermenu work because they DON'T create GFiles at startup (lazy), not because
  of fpcast. NEXT: (1) confirm guest env reaches --exec then test GIO_USE_VFS=local (force local vfs, skip the
  module scan); (2) disassemble the trapping libgio function; (3) ensure GLocalVfs is registered / the GIO
  module scan fails soft. Symbolization still blocked (lib funcs absent from the name section; DWARF low_pc
  zeroed). Trail in M8-STATUS-LOG.md (2026-06-25T17c..18h).
  </details>
- **XU6 — bundled apps.** ⬜ xfce4-terminal (live shell via PTY), mousepad, ristretto, appfinder,
  xfce4-notifyd (a notification pops). Proof screenshots.
- **XU7 — full Xubuntu session = ACCEPTANCE.** ⬜ One screenshot shows the FULL live Xubuntu desktop
  working together: Greybird-themed, elementary-xfce icons, xfdesktop wallpaper + icons, xfce4-panel +
  Whisker menu, an xfwm4-decorated Thunar showing a real listing, all interactive, captured in a normal
  run. Visually indistinguishable from a real Xubuntu 24.04 session. (The Xubuntu analogue of M8.6.)

---

## 7. Out of scope / deliberate trust & architecture decisions

These are NOT "more Linux compatibility" — they are hardware/GPU/privilege decisions a sandbox brokers
or omits. Surfacing them as human/TCB calls, not unattended work:

- **GPU / OpenGL (Mesa/DRI)** ❌ — the wall for GNOME Shell / Mutter / Cinnamon / KDE Plasma. Would
  need GPU passthrough or a wasm software-GL (softpipe; **llvmpipe is impossible** — it JITs native
  x86) port = a whole new rendering subsystem. Explicitly out per SPEC.md's software-raster spine.
- **Wayland** ❌ — default GNOME's display server; this whole spine is X11. Out unless/until a Wayland
  display is implemented in the runtime.
- **Real hardware** ❌ — audio (PipeWire), networking (NetworkManager), DRM/displays, USB, bluetooth,
  printers, scanners. No devices in the sandbox.
- **Login / init / privilege** ❌ — LightDM/GDM (we hand-launch the session), systemd-as-init, cgroups,
  polkit escalation, gnome-keyring secrets, real-fs mounts.
- **Heavy app payload** ❌ — Firefox, Thunderbird (snaps), LibreOffice, GIMP, rhythmbox/parole (media),
  transmission, snapd. They define the ISO but can't run here. We mirror the **shell**, not the ISO.
- **D-Bus session bus** ⚠️ **TCB gate** — running stock `dbus-daemon` as an untrusted guest is the
  conservative choice (no enforcement in the bus), but it is a new IPC surface; **surface for human
  sign-off** before XU0 lands in anything shipping.

---

## 8. Workflow

Per closed item: advance the lowest open XU#, drop proofs in the hour-bucket folder
(`~/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/`, name `proof-xu<N>-<what>.png`), capture BOTH render and
black/stall outcomes, update `progress.html`, append a one-line status log, and `jj commit`. Build WASM
only through the custom toolchain. Real Xfce/Xubuntu only — no substitutes. Stop only when all XU items
are green with artifacts.

---

## Changelog
- **v1 (2026-06-24):** initial draft. Successor to `SPEC.md` M8 (LXDE). Target: default Xubuntu 24.04
  desktop shell (Xfce 4.18, X11, software-rendered). Software list + reference docs/repos + platform
  work (D-Bus session bus is the new linchpin) + XU0–XU7 milestones.
