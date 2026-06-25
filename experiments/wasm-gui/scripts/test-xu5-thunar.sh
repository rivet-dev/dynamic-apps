#!/usr/bin/env bash
# XU5: run UNMODIFIED Thunar (file manager) on the wasm X server under xfwm4, browsing a home folder.
# Validates the file-view gate fix (g_vfs_get_default -> g_vfs_get_local wrap): Thunar's folder model
# enumerates each entry via g_file_query_info, which was the gate. Also exercises Thunar's
# GtkApplication startup (previously suspected to not map a window).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfwm4.wasm" "$EXP/thunar.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"; WMDATA="${VMXFWM4:-/tmp/vmxfwm4}"; ICONS="${VMICONS:-/tmp/vmicons}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
[ -d "$ICONS" ]  || bash "$EXP/scripts/prepare-icons.sh" "$ICONS" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-xfwm4.sh" "$WMDATA" >/dev/null 2>&1 || true
FIX=/tmp/vmxu5sess; rm -rf "$FIX"
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus" "$FIX/root/.cache"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"; cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"
# a home folder for Thunar to enumerate (its default view = the user home, /root)
mkdir -p "$FIX/root/Documents" "$FIX/root/Pictures" "$FIX/root/Downloads"
printf 'hello from the wasm Xubuntu desktop\n' > "$FIX/root/readme.txt"
printf 'notes\n' > "$FIX/root/notes.txt"
# GTK icon theme (no xfsettingsd here -> settings.ini; icon-theme-name only, gtk-theme-name stalls the render)
GTKCFG="$FIX/root/.config/gtk-3.0"; mkdir -p "$GTKCFG"
printf '[Settings]\ngtk-icon-theme-name=Adwaita\n' > "$GTKCFG/settings.ini"
mkdir -p "$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml"
cat > "$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml" <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0"><property name="general" type="empty"><property name="theme" type="string" value="Greybird"/><property name="use_compositing" type="bool" value="false"/></property></channel>
X
W=900; H=650
FB="$(mktemp /tmp/xu5-fb.XXXXXX.bin)"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu5-thunar.png}"; mkdir -p "$(dirname "$PNG")"
echo "running XU5 thunar -> png=$PNG"
WM_SETTLE_QUIET_MS=2500 WM_SETTLE_CAP_S=50 APP_SETTLE_MS=5000 \
timeout 200 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-120}" \
  --server "$EXP/Xvfb.wasm" --dbus "$EXP/dbus-daemon.wasm" --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" --client "$EXP/thunar.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$THEMES" --vm-tree "$WMDATA" --vm-tree "$XFT" --vm-tree "$ICONS" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > "${OUT:-/tmp/xu5.log}" 2>&1 || true
[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
python3 - "$PNG" <<'PY'
from PIL import Image
import sys, os
p=sys.argv[1]
if not os.path.exists(p): print("no png"); sys.exit()
im=Image.open(p).convert("RGB");W,H=im.size;px=im.load()
nonblack=sum(1 for y in range(0,H,3) for x in range(0,W,3) if max(px[x,y])>30)
total=(H//3+1)*(W//3+1)
print(f"non-black cells: {nonblack}/{total} ({100*nonblack//total}%)")
print("XU5 thunar: PASS (a Thunar window renders)" if nonblack>total//5 else "XU5 thunar: window did not render")
PY
