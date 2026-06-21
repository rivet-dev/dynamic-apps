#!/usr/bin/env bash
# M8 keystone spike: a secure-exec-native DYNAMIC FFI CALL on wasm32-wasip1.
#
# M8 (the GTK desktop) is blocked at libffi: wasm32-wasi has no libffi because wasm has no runtime
# trampolines / inline asm, which GObject's closure marshalling (g_cclosure_marshal_generic -> ffi_call)
# needs. BUT our guests run inside the V8 sidecar, whose WebAssembly reflection CAN call a guest
# function by its indirect-function-table index with dynamically-typed args. The `host_net.ffi_call`
# import (crates/execution/src/node_import_cache.rs) implements exactly that. ffi-spike.c calls three
# functions purely by POINTER + a runtime-built arg list (no static call site for the callee), proving
# the core libffi capability GObject needs is achievable here without Emscripten. This is the keystone
# that unblocks the M8 GTK port (the rest is a multi-week cross-compile, plus ffi_closure).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"

for f in "$HOST" "$SIDECAR" "$EXP/guest-xclient/ffi-spike.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f (build: cargo build -p wasm-gui-host -p secure-exec-sidecar; scripts/build-ffi-spike.sh)"; exit 1; }
done

echo "== secure-exec dynamic FFI call (libffi keystone) on wasm32-wasip1 =="
OUT="$(timeout 60 env -u DISPLAY "$HOST" --exec --guest "$EXP/guest-xclient/ffi-spike.wasm" \
  --timeout 20 --sidecar "$SIDECAR" 2>&1)"
echo "$OUT" | grep -E "ffi_call|SPIKE" | sed 's/^\(\[out\] \)*/  /'

echo "$OUT" | grep -q "M8-FFI-SPIKE: PASS" || { echo "FAIL: dynamic FFI call did not pass"; echo "$OUT" | tail -20; exit 1; }
echo "PASS: dynamic FFI call (i32 + f64 + pointer args, called by pointer) works via V8 reflection"
echo "== M8 FFI keystone spike PASS =="
