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
| **xfce4-panel** | panel / taskbar / systray / clock | https://gitlab.xfce.org/xfce/xfce4-panel | ⬜ |
| **xfce4-whiskermenu-plugin** | the iconic Xubuntu app menu | https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin | ⬜ |
| **xfdesktop4** | wallpaper + desktop icons + root menu | https://gitlab.xfce.org/xfce/xfdesktop | ⬜ |
| **Thunar** (+ thunar-volman) | file manager | https://gitlab.xfce.org/xfce/thunar | ⬜ (reuse pcmanfm/libfm work) |
| **xfce4-settings** (xfsettingsd) | settings daemon → XSETTINGS push (theme/font/cursor) | https://gitlab.xfce.org/xfce/xfce4-settings | ⬜ (**the "feels right" linchpin**) |
| **xfconf** | settings store (D-Bus service) | https://gitlab.xfce.org/xfce/xfconf | ⬜ |

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
2. **xfconf + xfsettingsd bring-up.** ⬜ With the bus up, xfconf (settings store) and xfsettingsd
   (XSETTINGS push) must run; stage the **xubuntu-default-settings** xfconf channels so themes/fonts/
   wallpaper/`xfwm4 compositing=off` are applied. Without this the desktop renders default-ugly.
3. **Static panel plugins.** ⬜ Build xfce4-panel + whiskermenu + any plugins **statically** (no
   dlopen), mirroring the lxpanel `--disable-plugins-loading` approach.
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
- **XU1 — xfconf + xfsettingsd.** 🟡 IN PROGRESS (2026-06-24). DoD: xfconf stores/serves a value over
  D-Bus; xfsettingsd pushes XSETTINGS to a GTK client (theme/font visibly applied). Proof: a GTK window
  in Greybird, not default. **Foundation de-risked first (constraint #4):** xfconf/xfsettingsd reach the
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
  - **REMAINING for XU1 acceptance:** the combined visual harness — run X server + dbus-daemon + xfconfd
    (with a staged xsettings channel: Net/ThemeName=Greybird, Gtk/FontName) + xfsettingsd + a GTK client,
    and screenshot the GTK window themed by Greybird (not default). Needs a combined X+D-Bus harness mode
    (currently `--xdemo` and `--bus-test` are separate) + the Greybird gtk-3.0 theme staged as a fixture.
- **XU2 — xfwm4 (the real Xfce WM).** ⬜ xfwm4 (compositing off) decorates a GTK window with the
  Greybird theme; move/resize + workspaces via XTEST. Proof screenshot. (Supersedes M8.2's openbox as
  the Xubuntu WM.)
- **XU3 — xfce4-panel + whiskermenu.** ⬜ The panel renders with the Whisker app menu, taskbar, clock,
  systray; the menu opens and lists apps. Proof screenshot.
- **XU4 — xfdesktop.** ⬜ Wallpaper + desktop icons + right-click root menu render. Proof screenshot at
  ≥1024×768.
- **XU5 — Thunar.** ⬜ Thunar (Xfce file manager, gvfs/tumbler off) lists a real VFS dir, decorated by
  xfwm4. Proof screenshot. (Reuses the M8.3/M8.5 libfm + listing work.)
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
