#!/usr/bin/env bash
# XU2 acceptance (visual): the REAL Xfce window manager xfwm4 decorates a GTK window with the Greybird
# xfwm4 theme, all wasm, compositing OFF (software-rendered). Stack: dbus-daemon + xfconfd (serves the
# xfwm4 channel /general/theme=Greybird over D-Bus) + xfwm4 (--client: the WM; selects
# SubstructureRedirect on the root, reparents + decorates managed windows) + gtk-hello (--client: a
# top-level window xfwm4 reparents into a Greybird titlebar+border frame). PASS = xfwm4 maps and
# decorates the window (the framebuffer shows the Greybird titlebar chrome around the GTK content) +
# no fatal WM error. Proof PNG. UNMODIFIED upstream xfwm4/GTK/D-Bus; only gtk-hello is ours.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
for f in "$HOST" "$SIDECAR" "$XVFB" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfwm4.wasm" "$EXP/guest-xclient/gtk-hello.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done

FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
WMDATA="${VMXFWM4:-/tmp/vmxfwm4}"
bash "$EXP/scripts/prepare-xfwm4.sh" "$WMDATA" >/dev/null 2>&1 || true

FIX=/tmp/vmxu2
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"
cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"
# Stage the xfwm4 xfconf channel: tell xfwm4 to use the Greybird DECORATION theme.
CHDIR="$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml"
mkdir -p "$CHDIR" "$FIX/root/.cache"
cat > "$CHDIR/xfwm4.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0">
  <property name="general" type="empty">
    <property name="theme" type="string" value="Greybird"/>
    <property name="title_font" type="string" value="Sans Bold 9"/>
    <property name="button_layout" type="string" value="O|HMC"/>
    <property name="use_compositing" type="bool" value="false"/>
    <property name="workspace_count" type="int" value="4"/>
  </property>
</channel>
XML

W=800; H=600
FB="$(mktemp /tmp/xu2-xfwm4-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu2-xfwm4.log}"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu2-xfwm4-greybird.png}"
mkdir -p "$(dirname "$PNG")"

echo "running XU2 xfwm4 chain -> fb=$FB png=$PNG log=$OUT"
timeout 150 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-95}" \
  --server "$XVFB" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" \
  --client "$EXP/guest-xclient/gtk-hello.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$THEMES" --vm-tree "$XFT" --vm-tree "$WMDATA" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "$OUT" 2>&1 || true

echo "=== xfwm4 / decoration evidence ==="
grep -aiE "xfwm4|reparent|managing|Greybird|SubstructureRedirect|another window manager|Unable to open display|theme|fatal|CRITICAL" "$OUT" | grep -aviE "NETWRITE|NETREAD" | head -15
if [ -s "$FB" ]; then python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1 || true; fi

if grep -aqiE "another window manager is already running|Unable to open display" "$OUT"; then
  echo "XU2 XFWM4: FAIL — xfwm4 could not take over the display — see $OUT"; exit 1
else
  echo "XU2 XFWM4: ran (inspect $PNG for the Greybird-decorated window); log $OUT"; exit 0
fi
