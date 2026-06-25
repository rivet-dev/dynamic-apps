# Xubuntu-on-wasm: handoff — two deep platform roots block the last milestones

**Status (2026-06-25):** XU0–XU4 DONE; XU6 = three bundled apps render with verified real text
(xfce4-appfinder, mousepad, ristretto) + the xfce4-notifyd daemon's full build + D-Bus chain works. The
remaining items (XU5 Thunar, the notifyd popup, the xfce4-terminal, XU7 full session) are gated on **two
deep, thoroughly-characterized platform roots**, both needing a focused effort / human sign-off — they
are NOT app-side bugs and are NOT fixable by patching components (constraint #5).

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

**Consequence per app:** standalone apps pay the ~13s **once** and render (appfinder/mousepad/ristretto
do). But notifyd's **synchronous** `Notify` D-Bus call blocks the sender during it, and notifyd's popup
has an *additional* widget-specific cost (>134s in its run — its custom XfceNotifyWindow CSS + Pango
markup + icon scaling). Thunar's `ThunarPathEntry` similarly amplifies it.

**Proper fix (deep, needs sign-off):** a faster wasm indirect-call path for GObject's
mismatched-signature function pointers — wasm **typed-function-references / GC** instead of
`--fpcast-emu -pa max-func-params@128`. This is a toolchain + runtime change (clang/wasm-ld emit + V8
support + the cross-env build) and speeds up **every** GTK guest. It is in-scope per constraint #5
(toolchain layer) but is a multi-day effort, not a 5-minute-cron change.

## Root 2 — concurrent-guest scheduling ceiling (gates XU7 full session)

The full Xubuntu session needs xfwm4 + xfce4-panel + xfdesktop + Thunar + apps as 4+ concurrent heavy
guests. A 4th heavy guest starves (the host scheduler favors the busy guest). This is a separate runtime
scheduling/TCB item, surfaced earlier (see `M8-STATUS-LOG.md`). Independent of Root 1.

## xfce4-terminal — process-spawn (separate, surfaced)

VTE needs `fork`/process-spawn → a host wasi-spawn bridge decision (TCB). Build is ready (`build-vte.sh`,
no icu/gnutls + the TIOCGWINSZ shim).

---

## Decision needed

1. **Undertake the typed-function-references toolchain/runtime change** (Root 1) — unblocks XU5 + the
   notifyd popup and accelerates all GTK guests; or accept the ~13s first-widget cost.
2. **Address the concurrent-guest scheduling ceiling** (Root 2) — unblocks XU7.
3. **Sign off the process-spawn bridge** for xfce4-terminal.

None of these are self-approvable (they are toolchain/runtime/TCB architecture decisions). The app-side
work that does not require them is complete.
