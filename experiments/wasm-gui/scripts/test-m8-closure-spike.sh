#!/usr/bin/env bash
# M8 keystone spike, part 2: ffi_closure (runtime-created callbacks) on wasm32-wasip1 via the pure-wasm
# TRAMPOLINE-POOL technique. Together with test-m8-ffi-spike.sh (ffi_call), this demonstrates BOTH
# libffi primitives GObject needs — the foundational blocker M8-FINDINGS.md called a dead end. The pool
# hands out pre-generated per-slot trampolines as distinct runtime closures, each forwarding to a
# generic dispatcher with its captured data. No host import, no V8 engine flag.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"

for f in "$HOST" "$SIDECAR" "$EXP/guest-xclient/closure-spike.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f (build: cargo build -p wasm-gui-host -p secure-exec-sidecar; scripts/build-closure-spike.sh)"; exit 1; }
done

echo "== ffi_closure (runtime callbacks via trampoline pool) on wasm32-wasip1 =="
OUT="$(timeout 60 env -u DISPLAY "$HOST" --exec --guest "$EXP/guest-xclient/closure-spike.wasm" \
  --timeout 20 --sidecar "$SIDECAR" 2>&1)"
echo "$OUT" | grep -E "closure[12]|SPIKE" | sed 's/^\(\[out\] \)*/  /'

echo "$OUT" | grep -q "M8-CLOSURE-SPIKE: PASS" || { echo "FAIL: closure dispatch did not pass"; echo "$OUT" | tail -20; exit 1; }
echo "PASS: two distinct runtime closures each dispatched with their captured data"
echo "== M8 ffi_closure spike PASS =="
