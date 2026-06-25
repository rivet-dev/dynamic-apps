#!/usr/bin/env bash
# XU4: xfdesktop renders the desktop WALLPAPER under xfwm4, all wasm. Stack: dbus + xfconfd (serves the
# xfce4-desktop + xfwm4 channels) + xfwm4 (so the desktop window is mapped) + xfdesktop. The backdrop is a
# staged image keyed by the Xvfb monitor name "monitorscreen" (NOT "monitor0" -- discovered via
# G_DBUS_DEBUG). xfdesktop is NOT deadlocked (idle main loop after init); it just needs a WM + the right
# monitor key + a staged image. PASS = the framebuffer is filled with the wallpaper color.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfwm4.wasm" "$EXP/xfdesktop.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"; WMDATA="${VMXFWM4:-/tmp/vmxfwm4}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-xfwm4.sh" "$WMDATA" >/dev/null 2>&1 || true

FIX=/tmp/vmxu4sess; rm -rf "$FIX"
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus" "$FIX/root/.cache" "$FIX/usr/share/backgrounds/xfce"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"; cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"
python3 -c "from PIL import Image; Image.new('RGB',(800,600),(40,92,158)).save('$FIX/usr/share/backgrounds/xfce/wallpaper.png')"
CHDIR="$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml"; mkdir -p "$CHDIR"
cat > "$CHDIR/xfce4-desktop.xml" <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty"><property name="screen0" type="empty"><property name="monitorscreen" type="empty"><property name="workspace0" type="empty">
    <property name="color-style" type="int" value="0"/>
    <property name="image-style" type="int" value="5"/>
    <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/wallpaper.png"/>
  </property></property></property></property>
</channel>
X
cat > "$CHDIR/xfwm4.xml" <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0"><property name="general" type="empty"><property name="theme" type="string" value="Greybird"/><property name="use_compositing" type="bool" value="false"/></property></channel>
X

# Icon theme: stage Adwaita (has user-home/user-trash/folder...) + point GTK at it via settings.ini
# (no xfsettingsd in this test, so GTK would otherwise fall back to bare hicolor -> no desktop icons).
ICONS="${VMICONS:-/tmp/vmicons}"; [ -d "$ICONS" ] || bash "$EXP/scripts/prepare-icons.sh" "$ICONS" >/dev/null 2>&1 || true
GTKCFG="$FIX/root/.config/gtk-3.0"; mkdir -p "$GTKCFG"
printf '[Settings]\ngtk-icon-theme-name=Adwaita\n' > "$GTKCFG/settings.ini"
W=800; H=600
FB="$(mktemp /tmp/xu4-fb.XXXXXX.bin)"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu4-xfdesktop.png}"; mkdir -p "$(dirname "$PNG")"
echo "running XU4 xfdesktop -> png=$PNG"
WM_SETTLE_QUIET_MS=2500 WM_SETTLE_CAP_S=45 APP_SETTLE_MS=4000 \
timeout 180 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-100}" \
  --server "$EXP/Xvfb.wasm" --dbus "$EXP/dbus-daemon.wasm" --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" --client "$EXP/xfdesktop.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$THEMES" --vm-tree "$WMDATA" --vm-tree "$XFT" --vm-tree "$ICONS" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > "${OUT:-/tmp/xu4.log}" 2>&1 || true
[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
python3 - "$PNG" <<'PY'
from PIL import Image
import sys, os
p=sys.argv[1]
if not os.path.exists(p): print("XU4: no png"); sys.exit(1)
im=Image.open(p).convert("RGB"); W,H=im.size; px=im.load()
nb=sum(1 for y in range(0,H,4) for x in range(0,W,4) if max(px[x,y])>25)
filled = nb > (W//4)*(H//4)*0.5
print(f"backdrop coverage: {nb} cells; XU4 xfdesktop: {'PASS (wallpaper fills the desktop)' if filled else 'FAIL (black)'}")
sys.exit(0 if filled else 1)
PY
