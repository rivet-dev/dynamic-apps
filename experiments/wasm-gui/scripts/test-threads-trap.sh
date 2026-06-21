#!/usr/bin/env bash
# Worker-trap -> VM fault (WASM-THREADS-SPEC.md DoD §9.6): a worker thread traps. The VM must FAULT
# promptly (the leader, hung in pthread_join the dead worker can't notify, is terminated) — not hang
# until the execution timeout, and the leader must NOT report "survived".
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO_WS="$(cd "$EXP/../.." && pwd)"
HOST="$REPO_WS/target/debug/wasm-gui-host"
SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/threads-trap.wasm"

"$EXP/scripts/build-threads-spike.sh" threads-trap >/dev/null || { echo "BUILD FAILED"; exit 1; }
[ -x "$HOST" ] && [ -x "$SIDECAR" ] || { echo "missing host/sidecar"; exit 1; }

GUEST_TIMEOUT=20
start=$(date +%s)
out="$(timeout $((GUEST_TIMEOUT + 10)) env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout "$GUEST_TIMEOUT" --sidecar "$SIDECAR" 2>&1)"
elapsed=$(( $(date +%s) - start ))
echo "$out" | grep -iE "M8-THREADS-TRAP|exited with code|timeout" | sed 's/^/  /'
echo "  elapsed=${elapsed}s"

if echo "$out" | grep -qi "survived"; then echo "THREADS-TRAP: FAIL (leader survived a worker trap)"; exit 1; fi
if echo "$out" | grep -qi "timeout"; then echo "THREADS-TRAP: FAIL (hung to timeout instead of faulting)"; exit 1; fi
if echo "$out" | grep -qiE "exited with code [1-9]"; then
  echo "THREADS-TRAP: PASS (worker trap faulted the VM promptly, elapsed=${elapsed}s)"; exit 0
fi
echo "THREADS-TRAP: FAIL (no fault observed)"; exit 1
