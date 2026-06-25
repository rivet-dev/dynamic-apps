#!/usr/bin/env bash
# XU1 FULL acceptance (visual): the complete xfconf->xfsettingsd->X-XSETTINGS->GTK chain, all wasm.
#   dbus-daemon (session bus) + xfconfd (serves the xsettings channel Net/ThemeName=Greybird over D-Bus)
#   + xfsettingsd (reads xfconf, publishes XSETTINGS to the X server) + a GTK client (gtk-hello) that
#   reads the XSETTINGS manager selection and themes itself with the real Xubuntu Greybird gtk-3.0 theme.
# PASS = gtk-hello's "XU1-XSETTINGS:" readback reports gtk-theme-name=Greybird (the push landed) AND a
# non-trivial framebuffer PNG is produced (the themed window rendered). All from UNMODIFIED upstream
# Xfce/GTK/D-Bus; only the test client (gtk-hello) is ours.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
for f in "$HOST" "$SIDECAR" "$XVFB" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfsettingsd.wasm" "$EXP/guest-xclient/gtk-hello.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done

FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true

FIX=/tmp/vmxu1gb
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
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
    <property name="DPI" type="int" value="98304"/>
    <property name="Antialias" type="int" value="1"/>
  </property>
</channel>
XML

W=800; H=600
FB="$(mktemp /tmp/xu1-greybird-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu1-greybird.log}"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu1-greybird-gtk.png}"
mkdir -p "$(dirname "$PNG")"

echo "running XU1 Greybird chain -> fb=$FB png=$PNG log=$OUT"
timeout 130 env -u DISPLAY NO_AT_BRIDGE=1 G_MESSAGES_DEBUG="" "$HOST" --xdemo --timeout "${TIMEOUT:-70}" \
  --server "$XVFB" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfsettingsd.wasm" \
  --client "$EXP/guest-xclient/gtk-hello.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$THEMES" --vm-tree "$XFT" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "$OUT" 2>&1 || true

echo "=== XU1 XSETTINGS readback (from gtk-hello) ==="
grep -aE "XU1-XSETTINGS:|M8-GTK: after gtk_init|Unable to open display" "$OUT" | head
# Convert the framebuffer to PNG proof.
if [ -s "$FB" ]; then
  python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1 || true
fi

THEME="$(grep -aoE "gtk-theme-name=[A-Za-z0-9_-]+" "$OUT" | head -1 | cut -d= -f2)"
if grep -aqiE "Unable to open display" "$OUT"; then
  echo "XU1 GREYBIRD: FAIL — gtk-hello could not open the display — see $OUT"; exit 1
elif [ "$THEME" = "Greybird" ]; then
  echo "XU1 GREYBIRD: PASS — GTK received XSETTINGS push (gtk-theme-name=Greybird) + rendered; png=$PNG"; exit 0
else
  echo "XU1 GREYBIRD: INCOMPLETE — gtk-theme-name=${THEME:-<none>} (expected Greybird); see $OUT"; exit 1
fi
