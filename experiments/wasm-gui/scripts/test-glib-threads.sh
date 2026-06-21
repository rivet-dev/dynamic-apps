#!/usr/bin/env bash
# Threaded GLib smoke (WASM-THREADS-SPEC.md DoD 9.7): GLib's thread API (g_thread_new + GThreadPool)
# running on the wasm-threads runtime through the real sidecar. The direct proof GLib's worker threads
# (the M8/GTK blocker) work. Requires the threaded GLib stack (build-glib-stack.sh w/ threads).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO_WS="$(cd "$EXP/../.." && pwd)"
HOST="$REPO_WS/target/debug/wasm-gui-host"; SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/glib-threads-test.wasm"
REPS="${1:-3}"
"$EXP/scripts/build-glib-threads-smoke.sh" >/dev/null || { echo "BUILD FAILED"; exit 1; }
[ -x "$HOST" ] && [ -x "$SIDECAR" ] || { echo "missing host/sidecar"; exit 1; }
pass=0
for i in $(seq 1 "$REPS"); do
  out="$(env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout 40 --sidecar "$SIDECAR" 2>&1)"
  line="$(echo "$out" | grep -iE 'GLIB-THREADS' | tr '\n' ' ')"
  if echo "$out" | grep -q "GLIB-THREADS: PASS"; then pass=$((pass+1)); echo "run $i: PASS  $line"; else echo "run $i: FAIL  $line"; fi
done
echo "reliability: $pass/$REPS"
[ "$pass" -eq "$REPS" ] && { echo "GLIB-THREADS: PASS"; exit 0; } || { echo "GLIB-THREADS: FAIL"; exit 1; }
