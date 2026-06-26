# Xubuntu-on-wasm: handoff — two deep platform roots block the last milestones

**Status (2026-06-25):** **XU0–XU5 DONE; XU6 = appfinder + mousepad + ristretto + the notifyd notification
popup all RENDER** (verified screenshots, real text/icons, all wasm). Every single-guest desktop
component works. The only remaining milestones are **xfce4-terminal** (blocked at VTE's hard `fork`
requirement) and **XU7 full session** (4 guests at once). Both are gated on **deep platform/runtime
decisions** needing human sign-off — NOT app-side bugs, NOT fixable by patching components (constraint #5).

This document is the decision point. Everything below the line is reproducible from the committed repros
(`guest-xclient/{popup-repro,pango-bench,xsync-bench,map-bench}.c`) and `M8-STATUS-LOG.md`.

---

## Root 1 — GTK first-text-widget performance (gates XU5 Thunar + the notifyd popup)

**Symptom:** Thunar's `ThunarPathEntry` and notifyd's popup window appear to "hang" during construction.

**What it actually is (measured, not guessed):**
- The wasm X server is fast: X round-trips ~5ms (`xsync-bench`), async events MapNotify ~21ms / Expose
  ~28ms (`map-bench`). `gtk_widget_realize` ~42ms. An empty `GtkWindow` `show_all` ~44ms.
- Rendering **one `GtkLabel`** makes `show_all` take **~13.4s**. Eight labels ≈ 14.6s → the cost is
  **one-time**, not per-widget.
- Direct pango/cairo text (no GTK) is only **~1.3s** (fontconfig+freetype+harfbuzz shape = 1284ms;
  rasterize = 4ms); a second label is 0ms. So the font path is ~1.3s one-time.
- Therefore GTK adds **~12s on top of pango** = the GtkWidget CSS/style machinery (theme CSS parse +
  first style cascade), one-time on the first text widget.

**Why:** the ~12s is GObject/CSS **indirect-call density** under the wasm `--fpcast-emu` cast emulation
(needed because GObject function pointers have mismatched wasm-table signatures). pango, far less
indirect-call-heavy, is only 1.3s. The GTK libs are already compiled `-O2`; the fpcast `-O0`-vs-`-O2`
pass-1 level is moot because the pipeline runs a `-Oz` second pass that already optimizes the wrappers.
So opt-level tuning is NOT the fix.

**Sub-localized further (`css-bench`):** the CSS *parser* is NOT the bottleneck — it parses 4000 synthetic
rules in **1256ms (0.32ms/rule)**. The ~12s lands in the **draw-time style cascade**: it appears in
`show_all`'s first paint (`gtk_render_layout` → the style context → the CSS-value computation for the
label's color/font), not in `gtk_widget_get_preferred_size` (which measured 0ms). So the cost is the
GObject CSS-value machinery executed on the first text paint — exactly the indirect-call-heavy path that
the fpcast-emu penalizes, confirming (not changing) the typed-function-references fix direction. (Open,
non-actionable lead: whether the cascade scales with theme rule-count — a smaller theme would change the
required authentic Greybird/Adwaita look, so it is not a usable fix.)

**Consequence per app:** standalone apps pay the ~13s **once** and render (appfinder/mousepad/ristretto
do). But notifyd's **synchronous** `Notify` D-Bus call blocks the sender during it, and notifyd's popup
has an *additional* widget-specific cost (>134s in its run — its custom XfceNotifyWindow CSS + Pango
markup + icon scaling). Thunar's `ThunarPathEntry` similarly amplifies it.

**Proper fix (deep, needs sign-off):** a faster wasm indirect-call path for GObject's
mismatched-signature function pointers — wasm **typed-function-references / GC** instead of
`--fpcast-emu -pa max-func-params@128`. This is a toolchain + runtime change (clang/wasm-ld emit + V8
support + the cross-env build) and speeds up **every** GTK guest. It is in-scope per constraint #5
(toolchain layer) but is a multi-day effort, not a 5-minute-cron change.

## Root 2 — the single sidecar sync-RPC service thread saturates under concurrent heavy guests (gates XU7)

**UPDATE (precise root found, 2026-06-25).** The vague "scheduling ceiling" is now pinned. The sidecar
has ONE service thread that processes ALL guests' syscalls (the sync-RPC bridge) AND the host's control
RPCs. `host/src/main.rs:1340` documents it: "while the X server and its clients are alive they keep the
single sidecar service thread busy, so a wire readback gets starved and never returns."

The full Xubuntu session (xfwm4 + xfce4-panel + xfdesktop + Thunar = 4 heavy guests) saturates that one
thread, manifesting two ways — both empirically observed:
- **Staggered launch** (each guest gated on the previous going idle): the host's `execute_env` control
  RPC to spawn the next guest STARVES behind a still-busy guest → the host blocks at ~114s, never
  launching the 3rd guest, then the outer timeout SIGKILLs it (no error, no FB).
- **Concurrent launch** (`--concurrent`, all up front): no launch-RPC starvation, but under sustained
  3-heavy-guest load the single thread saturates (X traffic + framebuffer writes + the Root-1 perf
  cascade's syscall flood) → the session collapses at ~211s, still before the slow guests render.

Ruled out as causes (with evidence): CPU/fuel limit (xserver+clients+services all set
`AGENT_OS_V8_CPU_TIME_LIMIT_MS=0`), OOM (`cgroup oom_kill=0`, no memory.max), per-process limits (ulimit
threads 255k / fds 1M / mem unlimited), and the system-bus connect (returns ENOENT, handled fine).
Single-guest sessions are stable to 240s+ and capture fine — this is strictly a MULTI-heavy-guest issue.

**Launch-RPC starvation FIXED (in-harness), but the timing wall remains.** A long (35s) inter-guest
settle -- launch the next guest only after the previous goes truly IDLE -- makes all guests launch
cleanly (the 18s settle launched mid-cascade, starving the launch RPC -> ~114s block). But the session
still collapses at ~237-251s (load-dependent; a smaller screen does not help -- the framebuffer is already
delta-optimized). Concrete numbers: each heavy guest takes ~100s to construct (perf cascade + X
round-trips on the one thread), so 3-guest STAGGERED construction is ~245s and 4-guest ~345s -- both
EXCEED the ~237s session-life. There is no launch window (earlier = panel-still-busy starvation; later =
no time before the death). So no in-harness knob (settle, screen, concurrent-vs-staggered) bridges the
gap; XU7 needs the session to live longer (Root 2 fix) OR the guests to construct faster (Root 1 fix).

**Root 1 COMPOUNDS Root 2:** the perf cascade is what floods the single thread with syscalls, so even
the concurrent workaround can't finish rendering before ~211s. The two are entangled for XU7.

**Fix space (sign-off, runtime/architecture):** multiplex the sidecar service thread / a per-guest
service thread / off-thread control-RPC handling. Same family as the M8.6 single-thread framebuffer
bottleneck. The in-harness workarounds (stagger settle tuning, `--concurrent`, capture-timing) are all
exhausted and ruled out.

**Note:** XU5 Thunar and the XU6 notifyd popup, once thought blocked, now BOTH RENDER (they were just
slow from Root 1, not deadlocked) — see the spec. So Root 1 no longer *blocks* those; it now governs
SPEED and, via the syscall flood, compounds Root 2 for the full session.

## xfce4-terminal — process-spawn (separate, surfaced)

VTE 0.70.6 configures past every check EXCEPT `meson.build:442` which HARD-ASSERTS "fork not found"
(`Checking if 'fork' compiles: NO`). wasi-libc has no `fork()` (no process duplication in the wasm
model). Constraint #5 forbids patching VTE, so the fix is platform-layer: declare `fork()` so the
compile check passes, then intercept the fork+exec PATTERN (VTE's child does setsid/dup2/exec) and map
it to the EXISTING wasi-spawn bridge (spawn a sandboxed shell guest, e.g. `pty-shell.wasm`, via the
wasi-pty seam). A deep fork/spawn-sequence intercept = the process-spawn architecture decision. Does NOT
unblock 100% (XU7 does); parked behind this decision.

---

## Decision needed

Status: **XU0–XU5 DONE; XU6 = appfinder + mousepad + ristretto + notifyd notification all RENDER**
(verified screenshots). The only remaining milestones are XU6's terminal and XU7's full session.

1. **Multiplex the sidecar sync-RPC service thread** (Root 2) — the direct blocker for XU7's full
   session; one service thread cannot carry 4 concurrent heavy GTK guests + the host's control RPCs.
2. **Undertake the typed-function-references toolchain change** (Root 1) — cuts the ~13s GTK first-widget
   cost; on its own it speeds everything up, and it RELIEVES Root 2 (less syscall flood per guest). XU5 +
   notifyd already render slowly without it, so it is now a speed/XU7-enabler, not a hard blocker.
3. **Sign off the process-spawn bridge** for xfce4-terminal (VTE fork → wasi-spawn).

None are self-approvable (toolchain/runtime/TCB architecture decisions). Everything achievable from the
harness/build/fixture layer is done: XU0–XU6 single-guest components all render with real text/icons.
XU7 (4 guests at once) is the one milestone that fundamentally needs the runtime concurrency change.
