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

## Direct-link investigation (2026-06-26 T63) -- confirms focused-session
Probed the "direct non-libtool clang link" option: libtool does NOT emit the full clang link command to
/tmp/make-thunar.log (only "CCLD thunar"), so it can't be lifted-and-modified from the log; and under
`--enable-static --disable-shared` the thunar program objects are non-PIC scattered (`thunar/.libs/*.o` empty).
Reconstructing the link by hand (objects + lib order) is exactly the interactive focused-session work.
UNTRIED OPTION worth a shot in that session (simplest first): pass the SBR's individual `.o` files DIRECTLY in
LIBS (not the `.a`) -- `thunar_sbr_la-*.o` -- since directly-listed objects are linked unconditionally where
archive members are pulled-on-reference; pair with `-Wl,--no-gc-sections` or `-Wl,-u,thunar_extension_initialize`
if wasm-ld still GCs them. If that fails, use `libtool --mode=link --dry-run`/`-n` to print the real clang
command, then run it directly with a genuine `--whole-archive $SBR_A`.

## Full chain status + CONFIRMED root cause (2026-06-26 T64) -- focused session needed
Pushed the thunar-sbr proof case far. PROVEN WORKING (committed shims + wiring in build-thunar.sh):
- SBR builds + statically LINKS: toolchain/thunar-sbr-keep.c (a constructor referencing the entry points)
  defeats wasm-ld --gc-sections (the breakthrough; --whole-archive/-u/direct-objects all failed to GC-survive).
  Surfaced the SBR's libexif dep -> added -lexif. The binary links clean and renders the dialog.
- All 3 entry points EXPORT (objdump confirms thunar_extension_initialize/shutdown/list_types as global CODE).
- The marker .so (/tmp/vmthunarx at THUNARX_DIRECTORY=<prefix>/lib/thunarx-3) is FOUND by the factory dir-scan.
- -Wl,--wrap=g_module_open FIRES (6 hits) and returns the main module.
BLOCKED at the LAST link: thunarx_provider_module_load asserts list_types==NULL, i.e. g_module_symbol(main,
"thunar_extension_*") returns NULL -- the runtime's main-module dlsym does not resolve the wasm export table.
The fix (a --wrap=g_module_symbol shim returning the addresses directly, toolchain/thunar-sbr-symwrap.c) is
written and wired, BUT confirmed NOT APPLIED: __wrap_g_module_symbol / __real_g_module_symbol are 0 in the
binary. ROOT CAUSE (now certain): **libtool's `make -C thunar` link strips/ignores the extra linker flags** --
it dropped --whole-archive, -u, and now --wrap=g_module_symbol (only --wrap=g_module_open, passed via the make's
own LDFLAGS path, took). THE FIX is the already-documented DIRECT non-libtool clang link of thunar: do the final
link by hand (objects + libs + ALL the -Wl wraps/exports), which applies every flag at once. Then the chain
completes (symwrap resolves the symbols -> renamers register -> infobar clears). All shims are committed and
correct; only the libtool->direct-link swap remains, and it closes the whole chain at once.

## ✅ SOLVED (2026-06-26 T65) -- gmodule static plugins work end-to-end
The thunar-sbr renamer loads and registers; the bulk-rename dialog shows the live renamer-rule controls
("Insert / Overwrite", "Name only", Insert/Text/At-position) instead of the "No renamer modules" infobar.
Proof: `~/tmp/gui-progress/2026-06-26T17/xu3-thunar-sbr-static.png`.

THE REAL ROOT CAUSE (the T63/T64 "libtool strips the link flags" diagnosis was WRONG -- corrected by adding
SHIMLOG fprintf observability to the wraps, per constraint #4). The captured SHIMLOG showed:
- BOTH `--wrap=g_module_open` AND `--wrap=g_module_symbol` DO fire (the nm "0 __wrap_*" checks were unreliable
  on wasm; libtool's dry-run confirmed it KEEPS every flag -- the strip theory was a red herring).
- The actual bug: `__wrap_g_module_open(thunar-sbr.so)` did `__real_g_module_open(NULL)` and got back
  **`-> main module = 0`**. The runtime has no self/main-module handle (glib `_g_module_self()` returns NULL
  here), so g_module_open(NULL) is NULL -> thunarx bailed "unknown dl-error" before ever calling g_module_symbol.

THE FIX (toolchain/gmodule-static-shim.c): for a plugin path, return a non-NULL **sentinel** handle (the address
of a static int) instead of `g_module_open(NULL)`. The companion symwrap (toolchain/thunar-sbr-symwrap.c)
resolves the 3 SBR entry points BY NAME, ignoring the handle, so the sentinel never needs to be a real module;
GTypeModules are use-counted and never unloaded, so it is never passed to g_module_close.

The complete proven mechanism (generalizes to the xfce4-panel plugin family for XU3):
1. keep-shim constructor (thunar-sbr-keep.c) -> defeats wasm-ld --gc-sections, keeps the statically-linked init.
2. -Wl,--export=<entry points> -> the symbols are real exports.
3. -Wl,--wrap=g_module_open -> SENTINEL non-NULL handle for plugin paths.
4. -Wl,--wrap=g_module_symbol -> resolve the plugin entry points by name (the addresses).
5. marker .so staged at the THUNARX_DIRECTORY path -> the factory dir-scan enumerates it and calls g_module_open.
NEXT for XU3: generalize the path-match (already covers xfce4/panel, panel-plugins) + the symwrap name-map to the
xfce4-panel plugin entry points (xfce_panel_module_init / construct), link each panel plugin's objects + keep-shim.

## ⚠ CORRECTION (2026-06-26 T66) -- scope + duplication, read before trusting T65 above
The T65 "SOLVED -- XU3 core blocker" framing OVERCLAIMED. Accurate picture:
- **XU3 (xfce4-panel + plugins) was already DONE 2026-06-25** via the EXISTING `toolchain/gmodule-shim.c`
  (named-handle from the .so path + generated `gmodule-plugins.gen.c` table + `--wrap=g_module_open/open_full/symbol`).
  The cron seed's "XU3 blocked on gmodule" is STALE. This session did NOT unblock XU3.
- What this session actually did: made the **thunar-sbr renamers (XU5 Thunar)** load -- a DIFFERENT plugin family
  (thunarx providers, 3 entry points: initialize/shutdown/list_types) not covered by the panel shim. Real result
  (the bulk-rename "No renamer modules" infobar clears), but it is an XU5 enhancement, not the XU3 blocker.
- **My `gmodule-static-shim.c` (sentinel handle) + `thunar-sbr-symwrap.c` (hardcoded names) DUPLICATE the existing
  `gmodule-shim.c` mechanism, worse.** The existing shim already returns a name-carrying handle (so it never hits
  g_module_open(NULL)=0 -- the thing I spent ~12 fires "discovering") and resolves via a TABLE (general, no
  hardcoded symbol names). The proper implementation is to EXTEND the existing shim, not add a parallel one.
- LESSON (own it): I did not read the existing gmodule-shim.c or re-read the authoritative spec (XU3 🟢) before a
  long build effort. constraint #4 is "observe before guessing" -- that includes observing existing solutions.

## FOLLOW-UP (the right consolidation, hard work, do next)
Unify thunar-sbr onto the existing `gmodule-shim.c`:
1. Generalize `__wrap_g_module_open`'s accept-check: accept the open if the parsed name is in the table for ANY
   registered symbol (today it hard-checks `xfce_panel_module_init/construct`), so thunarx names pass.
2. Have build-thunar.sh generate a table entry: thunar-sbr -> {thunar_extension_initialize, _shutdown,
   _list_types} (the existing `panel_static_plugin_lookup(name, symbol)` is already generic over the symbol).
3. Link `gmodule-shim.c` + the thunar table into thunar; DROP `gmodule-static-shim.c` + `thunar-sbr-symwrap.c`.
4. Re-verify BOTH the panel (regression) and the thunar bulk-rename render. One shim, one mechanism.
