#!/usr/bin/env bash
# Stage a precompiled XKB keymap (.xkm) for the wasm X server. wasi has no fork/exec, so the server
# cannot run xkbcomp to compile a keymap at runtime (its keyboard device then never activates, so
# KeyPress/XTEST events are dropped). We compile a standard US keymap on the HOST with the system
# xkbcomp and install it at /xkb/default.xkm in the VM (via --vm-tree); the server's patched
# XkbCompileKeymap loads it directly (fmemopen, no exec). See third_party/xserver/xkb/ddxLoad.c.
# Output tree: $1 (default /tmp/vmxkb), mirroring the in-VM layout (xkb/default.xkm).
set -uo pipefail
OUT="${1:-/tmp/vmxkb}"
command -v xkbcomp >/dev/null || { echo "need xkbcomp (x11-xkb-utils)"; exit 1; }
stat_bytes() {
  if stat -c%s "$1" >/dev/null 2>&1; then
    stat -c%s "$1"
  else
    stat -f%z "$1"
  fi
}
mkdir -p "$OUT/xkb"
TMP="$(mktemp -d)"
cat > "$TMP/km.xkb" <<'EOF'
xkb_keymap {
    xkb_keycodes  { include "evdev+aliases(qwerty)" };
    xkb_types     { include "complete" };
    xkb_compat    { include "complete" };
    xkb_symbols   { include "pc+us+inet(evdev)" };
};
EOF
# -xkm produces the binary interchange keymap the server's XkmReadFile consumes. No geometry section
# (the server does not need it, and it keeps the file's component set to exactly what `need` requires).
xkbcomp -xkm "$TMP/km.xkb" "$OUT/xkb/default.xkm" >/dev/null 2>&1
rm -rf "$TMP"
[ -s "$OUT/xkb/default.xkm" ] || { echo "xkbcomp produced no .xkm"; exit 1; }
echo "staged $OUT/xkb/default.xkm ($(stat_bytes "$OUT/xkb/default.xkm") bytes)"
