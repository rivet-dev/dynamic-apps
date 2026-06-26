<!-- Scoping brief for the gmodule/no-dlopen static-plugin blocker. Written 2026-06-26 after the Thunar
     bulk-rename render surfaced it concretely. This is the cron's flagged XU3 blocker AND the highest-value
     remaining AUTONOMOUS work (a platform/toolchain fix, NOT a TCB sign-off). Substantial + multi-turn, so it
     is scoped here rather than started casually in a 5-minute cron fire. -->
# gmodule static-plugin support (the XU3 / Thunar-renamer blocker)

## What it blocks (one root cause, several visible symptoms)
- **XU3 xfce4-panel**: the default panel plugins (clock, tasklist, systray, separator, whiskermenu, ...) are
  gmodule `.so`s loaded at runtime. The static no-dlopen build can't load them, so the panel renders with only
  whatever is force-linked. (The cron "Current state" flags exactly this.)
- **Thunar renamers**: the bulk-rename dialog renders (T58) but its infobar says *"No renamer modules were
  found ... enable the Simple Builtin Renamers plugin"* -- the renamer rules (thunar-sbr) are gmodule plugins.
- Same shape elsewhere (Thunar `thunarx` providers, gio modules, gdk-pixbuf loaders already solved differently).

## Why it fails (observed, not guessed)
`thunarx/thunarx-provider-module.c:197` does `module->library = g_module_open(path, ...)` then `:207`
`g_module_symbol(library, "thunar_extension_initialize")`. The factory (`thunarx-provider-factory.c:182`) only
*tries* paths it found by scanning a directory for files ending in `.G_MODULE_SUFFIX` (`.so`). In the static
wasm build there are no `.so` files and `g_module_open` of a real path can't dlopen, so zero providers load.

## The fix shape (constraint #5: platform/toolchain, component source untouched)
A **static-plugin gmodule shim**, three parts:
1. **Link the plugin init symbols** into the host binary (e.g. thunar-sbr's `thunar_extension_initialize`, each
   panel plugin's `xfce_panel_module_init` / `construct`). They become normal statically-linked symbols.
2. **`--wrap=g_module_open`**: for a known plugin path, return a handle that resolves to the MAIN module's
   symbol table (gmodule already supports `g_module_open(NULL)` = "the main program"); for everything else call
   the real one. Then `g_module_symbol(handle, "thunar_extension_initialize")` finds the linked symbol.
3. **Fake `.so` directory entries** (empty marker files named `*.so`) staged in the module dir so the factory's
   `g_str_has_suffix(name, "."G_MODULE_SUFFIX)` scan actually enumerates them and calls `g_module_open` on each.
   (Alternatively wrap the dir-scan, but the marker-file route keeps the component code untouched.)

This is the same idea that already works for the V8 bridge / static gdk-pixbuf loaders, generalized to gmodule.

## Tractability / cost
- **Tractable**, and **autonomous** (no TCB sign-off -- it's toolchain/sysroot/shim, the constraint-#5 sanctioned
  layer). But it is **multi-turn**: the wrap shim + per-plugin init wiring + the fake-`.so` staging + a
  symbol-name map per plugin family (thunarx vs xfce4-panel use different init entry points) + test renders.
- **Highest leverage left**: it unblocks XU3 (the real panel), the Thunar renamers, and any other gmodule
  provider in one mechanism -- more than any single remaining dialog.
- **Recommendation**: do it as a focused multi-fire effort (or a dedicated session), starting with the *simplest*
  case to prove the shim end-to-end -- **thunar-sbr** (one init symbol, one fake `.so`, the bulk-rename dialog as
  the visible pass/fail) -- then generalize the symbol-map to the xfce4-panel plugin family for XU3.

*Visible proof of the blocker today: `~/tmp/gui-progress/2026-06-26T14/xu5-thunar-bulk-rename.png` (the "no
renamer modules" infobar). Status detail: `M8-STATUS-LOG.md` T58.*

## Progress + build-plumbing blockers (2026-06-26, steps 1-2 wired)
DONE: mechanism confirmed (gmodule g_module_open(NULL)=main module); toolchain/gmodule-static-shim.c written
(`__wrap_g_module_open` -> main module for thunarx/panel paths); build-thunar.sh wired to build thunar-sbr,
link it, compile+link the shim, and add `-Wl,--wrap=g_module_open`. The thunar BINARY links clean (54 MB).

BLOCKED on build plumbing (needs a FOCUSED session, not 5-min cron fires -- each rebuild is slow and the
shared workspace churns mid-build):
1. **libtool drops `-Wl,--whole-archive`**: `libthunar-sbr.a` reaches the link but the whole-archive wrapper is
   stripped by libtool, so the SBR objects are GC'd (the linked binary has 0 `thunar_extension_initialize`).
   Fix options: a direct (non-libtool) final clang link for thunar like the dialog recipe uses; or
   `-Wl,-u,thunar_extension_initialize -Wl,-u,<each renamer>_get_type`; or `-Wl,--whole-archive` passed in a way
   libtool preserves (`-Wl,` doubling / `-XCClinker`).
2. **thunar.wasm fpcast is separate**: build-thunar.sh only builds the native-wasm BINARY; the fpcast-emu+-Oz to
   thunar.wasm lives elsewhere (test-xu5-thunar.sh / a wrap step) and must be run after, or thunar.wasm is stale.
3. **workspace churn wipes `.libs`**: a concurrent session removes thunarx/thunar-sbr `.libs/*.a` between steps;
   building them in-script (added) mostly mitigates but a focused session with no concurrent churn is cleaner.

RECOMMENDATION: finish this in a focused session -- iterate the thunar relink interactively (resolve #1 with a
direct clang link, run the fpcast for #2), confirm the bulk-rename infobar clears, THEN generalize the shim's
path-match + the static link to the xfce4-panel plugin family for XU3. The shim + wiring are committed and ready.
