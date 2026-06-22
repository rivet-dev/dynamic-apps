#!/usr/bin/env bash
# Run a guest under the secure-exec debugging tools (see INTERNAL-TOOLING.md / root CLAUDE.md).
# Usage:
#   diag.sh trace      <guest.wasm>              # SECURE_EXEC_TRACE (~ strace): sync-RPC stream
#   diag.sh stackdump  <guest.wasm> [after_ms]   # SECURE_EXEC_STACKDUMP (~ gdb bt): isolate backtraces
#   diag.sh v8prof     <guest.wasm>              # V8 --prof (~ perf): names the hot wasm function
#   diag.sh threads    <guest.wasm>              # ps -L on the sidecar (~ top): thread run-states
# A guest path ending in a known X client is run on the wasm Xvfb (--xdemo); otherwise plain --exec.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
MODE="${1:?mode: trace|stackdump|v8prof|threads}"; GUEST="${2:?guest .wasm path}"
AFTER="${3:-14000}"
LOG="$(mktemp /tmp/diag-$MODE.XXXXXX.log)"; FB="$(mktemp /tmp/diag-fb.XXXXXX.bin)"
[ -f "$GUEST" ] || GUEST="$EXP/guest-xclient/$GUEST"
for f in "$HOST" "$SIDECAR" "$GUEST"; do [ -f "$f" ] || { echo "missing: $f"; exit 1; }; done
[ -d /tmp/vmfonts ] || bash scripts/prepare-fonts.sh /tmp/vmfonts >/dev/null 2>&1
[ -d /tmp/vmxft ]   || bash scripts/prepare-xftfonts.sh /tmp/vmxft >/dev/null 2>&1
[ -d /tmp/vmlocale ]|| bash scripts/prepare-locale.sh /tmp/vmlocale >/dev/null 2>&1

ENVV=(); case "$MODE" in
  trace)     ENVV=(SECURE_EXEC_TRACE=1) ;;
  stackdump) ENVV=(SECURE_EXEC_STACKDUMP_AFTER_MS="$AFTER" SECURE_EXEC_STACKDUMP_SAMPLES=2 SECURE_EXEC_STACKDUMP_INTERVAL_MS=800) ;;
  v8prof)    ENVV=(SECURE_EXEC_V8PROF=1) ;;
  threads)   ENVV=() ;;
  *) echo "unknown mode $MODE"; exit 1 ;;
esac

run_xdemo() {
  env "${ENVV[@]}" -u DISPLAY "$HOST" --xdemo --timeout 22 \
    --server "$EXP/Xvfb.wasm" --client "$GUEST" \
    --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale --vm-tree /tmp/vmxft \
    --fb-out "$FB" --sidecar "$SIDECAR" \
    -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts
}

if [ "$MODE" = threads ]; then
  ( timeout 40 bash -c "$(declare -f run_xdemo); EXP='$EXP' HOST='$HOST' GUEST='$GUEST' FB='$FB' SIDECAR='$SIDECAR' run_xdemo" >"$LOG" 2>&1 ) &
  sleep "$(( AFTER/1000 ))"
  SPID=$(ps -eo pid,comm | awk '$2 ~ /secure-exec-sid/ {print $1; exit}')
  echo "== sidecar pid=$SPID threads (R+high%CPU = busy-spin; futex_wait_queue = blocked) =="
  ps -L -p "$SPID" -o tid,pcpu,state,wchan:28,comm 2>/dev/null
  wait 2>/dev/null
  exit 0
fi

timeout 50 bash -c "$(declare -f run_xdemo); EXP='$EXP' HOST='$HOST' GUEST='$GUEST' FB='$FB' SIDECAR='$SIDECAR' run_xdemo" >"$LOG" 2>&1
echo "== $MODE -> $LOG =="
case "$MODE" in
  trace)     grep 'rpc-trace' "$LOG" | tail -40; echo "-- pid activity --"; grep -oE 'pid=[0-9]+' "$LOG" | sort | uniq -c ;;
  stackdump) grep -A30 'native backtrace' "$LOG" | head -120 ;;
  v8prof)    echo "(v8.log written next to the sidecar cwd; symbolize with scripts/v8prof-top.py)"; ls -t /tmp/*v8.log isolate-*.log 2>/dev/null | head ;;
esac
