#!/usr/bin/env bash
# XU1 (first half) acceptance: xfconf stores/serves a value over D-Bus, all wasm. Launches the
# unmodified dbus-daemon (session bus) + xfconfd (the Xfce settings daemon, registers org.xfce.Xfconf)
# + xfconf-query to SET then GET a property. PASS = the GET returns the value the SET wrote, proving a
# real xfconfd method-call round-trip over GDBus.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
for f in dbus-daemon.wasm xfconfd.wasm xfconf-query.wasm; do
  [ -f "$EXP/$f" ] || { echo "MISSING $EXP/$f"; exit 1; }
done
FIX="${VMDBUS:-/tmp/vmdbus}"
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
mkdir -p "$FIX/root/.config" "$FIX/root/.cache"   # xfconfd writes channel XML under $HOME/.config/xfce4/xfconf
OUT="${OUT:-/tmp/xu1-xfconf.log}"

env -u DISPLAY "$HOST" --bus-test \
  --server "$EXP/dbus-daemon.wasm" \
  --client "$EXP/xfconfd.wasm" \
  --client "$EXP/xfconf-query.wasm --channel test --property /greeting --type string --set hello-xu1 --create" \
  --client "$EXP/xfconf-query.wasm --channel test --property /greeting" \
  --vm-tree "$FIX" --sidecar "$SIDECAR" --timeout "${TIMEOUT:-30}" \
  -- --config-file=/etc/dbus-1/session.conf --nofork --nopidfile --print-address \
  > "$OUT" 2>&1

echo "=== xfconf evidence ==="
grep -aiE "hello-xu1|xfconf|org.xfce.Xfconf|error|failed" "$OUT" | grep -aviE "NETWRITE|NETREAD" | head -15
got=$(grep -ac "hello-xu1" "$OUT")
if [ "$got" -ge 1 ]; then
  echo "XU1 XFCONF ROUND-TRIP: PASS (xfconf-query GET returned the SET value)"; exit 0
else
  echo "XU1 XFCONF ROUND-TRIP: INCOMPLETE — see $OUT"; exit 1
fi
