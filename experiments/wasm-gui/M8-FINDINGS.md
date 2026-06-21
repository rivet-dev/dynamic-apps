# M8 (full GTK desktop stack to wasm32-wasip1) — spike findings

M8 asks for the GTK stack (GLib/GObject/Pango/Cairo/GdkPixbuf/harfbuzz/ATK/GTK) cross-compiled to
`wasm32-wasip1`, all running as wasm in secure-exec. This documents a real spike (not an estimate).

## Spike: cross-compile GLib (the stack's foundation)

GLib is the base of the entire stack (GObject → Pango/ATK/GdkPixbuf/GTK all depend on it). Using the
same toolchain that built the X stack (`toolchain/cross-env.sh`, wasi-sdk clang, meson cross file),
`scripts/spike-m8-glib.sh` runs:

    meson setup build-wasm --cross-file $CROSS_INI -Dtests=false -Dnls=disabled ... -Ddefault_library=static

Result: meson proceeds — it fetches the **libffi** subproject and runs the wasm32-wasi compiler checks
(all pass: sizeof types, visibility, headers) — then **fails**:

    subprojects/libffi/meson.build:259: ERROR: Unsupported pair: system "wasi", cpu family "wasm32"

## The foundational blocker: libffi has no wasm/wasi port

GObject (part of GLib) requires **libffi** for closure marshalling (`g_cclosure_marshal_generic`,
signal/closure invocation, GObject introspection). libffi implements `ffi_call`/closures with
**per-architecture assembly trampolines** — its meson build has branches for x86/x86_64/arm/aarch64/
riscv/etc., EACH supplying an arch-specific `sysv.S`. **There is no wasm branch**, because wasm has no
inline assembly and no way to synthesize a callable function pointer at runtime from a signature (the
core thing libffi/closures need). So `TARGET` stays empty and the build errors out.

This is not a "patch a few stubs" issue like the X stack's `wasi-compat` gaps. It is fundamental:

- **Every GObject-based library needs libffi** → all of GTK, Pango, ATK, GdkPixbuf, GLib's GObject.
- **wasm cannot do dynamic FFI trampolines** the way native arches do. The only working wasm libffi is
  Emscripten's, which uses a **JavaScript-side trampoline shim** (and dynamic `addFunction`/table
  growth) — it requires the Emscripten JS runtime and is NOT a pure-wasi solution, so it does not apply
  to secure-exec's `wasm32-wasip1` guests.

## Implication for M8

M8 is blocked at its foundation. Making the GTK stack run on wasi requires FIRST solving wasm FFI for
wasi-without-Emscripten, by one of:
1. Porting libffi to wasm32-wasi with a trampoline mechanism (e.g. a fixed dispatch table + the wasm
   `funcref`/`call_indirect` + a generated set of signature shims, or a host-side trampoline import).
   This is a substantial, novel piece of systems work.
2. Building GObject WITHOUT libffi (replace the generic closure marshaller). This is invasive surgery
   on GObject internals and breaks introspection/dynamic signals.
3. A different toolkit that does not need runtime FFI closures (none of the GTK-family does).

After that, the rest of the stack (Pango/Cairo/GdkPixbuf/harfbuzz/GTK) is still a multi-week
cross-compile with its own wasi blockers (threads/GThread, dlopen/GModule, dbus/GIO, fontconfig
already done for M6.2). So M8 = "solve wasm FFI for wasi" (research-level) + a multi-week port. It is
not completable in a session, and the FFI piece is a genuine prerequisite, not just effort.

Reproduce: `scripts/spike-m8-glib.sh` (downloads GLib 2.78.4, runs the meson wasi cross setup).
