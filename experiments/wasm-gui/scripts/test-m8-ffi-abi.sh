#!/usr/bin/env bash
# M8: the libffi-wasm SHIM exercised through the REAL libffi public ABI (the interface GObject links).
# Builds libffi-wasm/src/ffi.c (the shim) + libffi-wasm/test/ffi-abi-test.c and runs it: ffi_prep_cif +
# ffi_call for int/double/pointer signatures, and ffi_closure_alloc/ffi_prep_closure_loc + invoking a
# runtime-built callback. This is the SPEC's next M8 step after the raw primitive spikes: a real ffi.h
# shim that GLib's build can link. (GLib/GObject/GTK cross-compile is the remaining multi-week work.)
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"

[ -f "$HOST" ] || { echo "MISSING: $HOST"; exit 1; }
[ -f "$SIDECAR" ] || { echo "MISSING: $SIDECAR"; exit 1; }
[ -f "$EXP/guest-xclient/ffi-abi-test.wasm" ] || bash "$EXP/scripts/build-ffi-abi-test.sh" >/dev/null || { echo "build failed"; exit 1; }

echo "== libffi-wasm shim via the real libffi ABI (ffi_prep_cif + ffi_call + ffi_closure) =="
OUT="$(timeout 60 env -u DISPLAY "$HOST" --exec --guest "$EXP/guest-xclient/ffi-abi-test.wasm" \
  --timeout 20 --sidecar "$SIDECAR" 2>&1)"
echo "$OUT" | grep -E "ffi_call|ffi_closure|ABI" | sed 's/^\(\[out\] \)*/  /'

echo "$OUT" | grep -q "M8-FFI-ABI: PASS" || { echo "FAIL: libffi-wasm ABI test did not pass"; echo "$OUT" | tail -20; exit 1; }
echo "PASS: libffi-wasm shim works through the real libffi ABI (call + closure)"
echo "== M8 libffi-wasm ABI test PASS =="
