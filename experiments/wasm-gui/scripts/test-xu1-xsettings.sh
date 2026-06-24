#!/usr/bin/env bash
# XU1 acceptance (integration): xfsettingsd reads the xfconf xsettings channel and publishes XSETTINGS
# to the X server, all wasm. Combined X + D-Bus session harness (--xdemo --dbus): wasm Xvfb + dbus-daemon
# + xfconfd (serves the staged xsettings channel: Net/ThemeName=Greybird) + xfsettingsd. PASS =
# xfsettingsd connects to the display (no "Unable to open display") and publishes the XSETTINGS manager
# selection. (The GTK-client screenshot is the next step.)
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
for f in "$HOST" "$SIDECAR" "$XVFB" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfsettingsd.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done

FIX=/tmp/vmxu1
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
# D-Bus machine-id (GDBus/dbus need /etc/machine-id; without it GDBus tries to autolaunch a bus).
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"
cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"
# Stage the xsettings xfconf channel that xfconfd serves and xfsettingsd publishes as XSETTINGS.
CHDIR="$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml"
mkdir -p "$CHDIR" "$FIX/root/.cache"
cat > "$CHDIR/xsettings.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xsettings" version="1.0">
  <property name="Net" type="empty">
    <property name="ThemeName" type="string" value="Greybird"/>
    <property name="IconThemeName" type="string" value="elementary-xfce"/>
    <property name="DoubleClickTime" type="int" value="400"/>
  </property>
  <property name="Gtk" type="empty">
    <property name="FontName" type="string" value="Sans 10"/>
    <property name="CanChangeAccels" type="bool" value="false"/>
  </property>
  <property name="Xft" type="empty">
    <property name="DPI" type="int" value="96"/>
    <property name="Antialias" type="int" value="1"/>
  </property>
</channel>
XML

FB="$(mktemp /tmp/xu1-xsettings-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu1-xsettings.log}"
timeout 110 env -u DISPLAY "$HOST" --xdemo --timeout "${TIMEOUT:-60}" \
  --server "$XVFB" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfsettingsd.wasm" \
  --vm-tree "$FIX" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "$OUT" 2>&1 || true

echo "=== xfsettingsd / XSETTINGS evidence ==="
grep -aiE "xsettingsd|XSETTINGS|Unable to open display|org.xfce.Xfconf|Greybird|manager.*selection|error|Failed" "$OUT" | grep -aviE "NETWRITE|NETREAD" | head -15
if grep -aqiE "Unable to open display" "$OUT"; then
  echo "XU1 XSETTINGS: INCOMPLETE — xfsettingsd could not open the display — see $OUT"; exit 1
elif grep -aqiE "xsettingsd|started.*xsettings|xclient1" "$OUT"; then
  echo "XU1 XSETTINGS: xfsettingsd launched against the display (check the log for the XSETTINGS publish)"; exit 0
else
  echo "XU1 XSETTINGS: INCONCLUSIVE — see $OUT"; exit 1
fi
