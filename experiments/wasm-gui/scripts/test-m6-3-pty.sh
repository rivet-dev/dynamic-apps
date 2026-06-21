#!/usr/bin/env bash
# M6.3 test: a full INTERACTIVE shell session over a kernel PTY, end-to-end through the whole wasm
# stack. A wasm "terminal" guest (pty-term) spawns a wasm interactive shell (pty-shell) over a real
# kernel PTY via the host_net.pty_spawn import (open_pty_split + stdio 'pty'), then drives a sustained
# multi-command session: it writes command lines to the master (pty_write/__pty_write) and reads the
# shell's responses back from the master (pty_read/__pty_read). This exercises BOTH directions:
#   terminal -> shell stdin  (kernel PTY slave -> sidecar pump_pty_child_stdin -> in-session bridge)
#   shell stdout -> terminal (slave write -> kernel line discipline -> master read)
# across several command/response cycles (echo, ping, exit), proving sustained interactivity.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
TERM="$EXP/guest-xclient/pty-term.wasm"; SHELL_W="$EXP/guest-xclient/pty-shell.wasm"

for f in "$HOST" "$SIDECAR" "$TERM" "$SHELL_W"; do
  [ -f "$f" ] || { echo "MISSING: $f (build: cargo build -p wasm-gui-host -p secure-exec-sidecar; scripts/build-pty-guests.sh)"; exit 1; }
done

echo "== wasm terminal drives an interactive wasm shell session over a kernel PTY =="
OUT="$(timeout 70 "$HOST" --pty-test --guest "$TERM" --pty-shell "$SHELL_W" \
  --sidecar "$SIDECAR" --timeout 45 2>&1)"
echo "$OUT" | grep -E "PTY_(SPAWN|WRITE|CHILD|SESSION|NO_|.*FAIL)" | tr -d '\r'

echo "$OUT" | grep -q "PTY_SPAWN_OK"       || { echo "FAIL: pty_spawn did not return a master fd"; exit 1; }
echo "$OUT" | grep -q "PTY_CHILD_RAN"      || { echo "FAIL: the spawned wasm shell did not start over the PTY"; exit 1; }
echo "$OUT" | grep -q "PTY_WRITE_OK"       || { echo "FAIL: pty_write to the master failed"; exit 1; }
echo "$OUT" | grep -q "PTY_CHILD_REPLY_OK" || { echo "FAIL: 'echo hello' was not read from stdin and answered"; exit 1; }
echo "$OUT" | grep -q "PTY_CHILD_PING_OK"  || { echo "FAIL: second command ('ping') round-trip failed (not a sustained loop)"; exit 1; }
echo "$OUT" | grep -q "PTY_CHILD_EXIT_OK"  || { echo "FAIL: interactive 'exit' did not shut the shell down cleanly"; exit 1; }
echo "$OUT" | grep -q "PTY_SESSION_OK"     || { echo "FAIL: interactive session did not complete"; exit 1; }
echo "== M6.3 interactive PTY shell session PASS (echo + ping + exit round-trips) =="
