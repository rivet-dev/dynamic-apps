#!/usr/bin/env bash
# XU0 acceptance: D-Bus session-bus method-call + signal round-trip, all wasm. Launches the unmodified
# dbus-daemon as a guest (binds the session bus on the kernel socket table) + dbus-monitor and dbus-send
# as client guests (DBUS_SESSION_BUS_ADDRESS injected by the host --bus-test mode). PASS = dbus-send gets
# a method_return for ListNames AND dbus-monitor observes a signal.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$HOST" ]    || { echo "MISSING $HOST"; exit 1; }
[ -x "$SIDECAR" ] || { echo "MISSING $SIDECAR"; exit 1; }
for f in dbus-daemon.wasm dbus-send.wasm dbus-monitor.wasm; do
  [ -f "$EXP/$f" ] || { echo "MISSING $EXP/$f — run scripts/build-dbus.sh"; exit 1; }
done
FIX="${VMDBUS:-/tmp/vmdbus}"
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
OUT="${OUT:-/tmp/xu0-dbus.log}"

env -u DISPLAY "$HOST" --bus-test \
  --server "$EXP/dbus-daemon.wasm" \
  --client "$EXP/dbus-monitor.wasm --session" \
  --client "$EXP/dbus-send.wasm --type=method_call --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListNames" \
  --vm-tree "$FIX" --sidecar "$SIDECAR" --timeout "${TIMEOUT:-25}" \
  -- --config-file=/etc/dbus-1/session.conf --nofork --nopidfile --print-address \
  > "$OUT" 2>&1

echo "=== bus round-trip evidence ==="
grep -aE "method return|NameAcquired|NameOwnerChanged|signal |unix:path" "$OUT" | grep -avE "NETWRITE" | head
reply=$(grep -ac "method return" "$OUT")
sig=$(grep -acE "NameAcquired|NameOwnerChanged|signal " "$OUT")
if [ "$reply" -ge 1 ] && [ "$sig" -ge 1 ]; then
  echo "XU0 D-BUS ROUND-TRIP: PASS (method_return=$reply signals=$sig)"; exit 0
else
  echo "XU0 D-BUS ROUND-TRIP: INCOMPLETE (method_return=$reply signals=$sig) — see $OUT"; exit 1
fi
