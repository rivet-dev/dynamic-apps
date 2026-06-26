#!/usr/bin/env bash
# XU7 ACCEPTANCE: the full Xubuntu session -- xfwm4 (WM) + xfce4-panel + xfdesktop (wallpaper) + Thunar,
# all wasm, under one X server. Tests the 4-heavy-guest "ceiling" WITH PATIENCE (construction is slow due
# to the perf root, NOT deadlocked -- per the Thunar/notifyd findings). Constraint #5: all unmodified.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="${SIDECAR:-$REPO/target/debug/secure-exec-sidecar}"
for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfwm4.wasm" "$EXP/xfce4-panel.wasm" "$EXP/xfdesktop.wasm" "$EXP/thunar.wasm"; do
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
[ -d /tmp/vmschemas ] || bash "$EXP/scripts/stage-gschemas.sh" >/dev/null 2>&1 || true
SESS=/tmp/vmxu7sess
PLUGINS="${PLUGINS:-clock tasklist systray separator}" bash "$EXP/scripts/prepare-xfce4-panel.sh" "$SESS" >/dev/null 2>&1
FIX=/tmp/vmxu7sess-dbus; rm -rf "$FIX"
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus" "$FIX/root/.cache" "$FIX/usr/share/backgrounds/xfce" "$FIX/root/.config/gtk-3.0"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"; cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"
python3 -c "from PIL import Image; Image.new('RGB',(800,600),(40,92,158)).save('$FIX/usr/share/backgrounds/xfce/wallpaper.png')"
printf '[Settings]\ngtk-icon-theme-name=Adwaita\n' > "$FIX/root/.config/gtk-3.0/settings.ini"
CHDIR="$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml"; mkdir -p "$CHDIR"
cat > "$CHDIR/xfwm4.xml" <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0"><property name="general" type="empty"><property name="theme" type="string" value="Greybird"/><property name="title_font" type="string" value="Sans Bold 9"/><property name="use_compositing" type="bool" value="false"/></property></channel>
X
cat > "$CHDIR/xfce4-desktop.xml" <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty"><property name="screen0" type="empty"><property name="monitorscreen" type="empty"><property name="workspace0" type="empty">
    <property name="color-style" type="int" value="0"/><property name="image-style" type="int" value="5"/>
    <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/wallpaper.png"/>
  </property></property></property></property>
</channel>
X
W=${W:-800}; H=${H:-600}
FB="$(mktemp /tmp/xu7-full-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu7-full.log}"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu7-shell-session.png}"; mkdir -p "$(dirname "$PNG")"
echo "running XU7 SHELL (xfwm4+panel+xfdesktop) -> png=$PNG log=$OUT"
WM_SETTLE_QUIET_MS=${WM_SETTLE_QUIET_MS:-18000} WM_SETTLE_CAP_S=${WM_SETTLE_CAP_S:-150} APP_SETTLE_MS=${APP_SETTLE_MS:-18000} \
timeout "${OUTER:-400}" env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo ${CONCURRENT_FLAG:-} --timeout "${TIMEOUT:-340}" \
  --server "$EXP/Xvfb.wasm" --dbus "$EXP/dbus-daemon.wasm" --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" --client "$EXP/xfce4-panel.wasm" --client "$EXP/xfdesktop.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$SESS" --vm-tree "$THEMES" --vm-tree "$WMDATA" --vm-tree "$XFT" --vm-tree "$ICONS" --vm-tree /tmp/vmschemas \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > "$OUT" 2>&1 || true
echo "clients completed: $(grep -aoE '[0-9]+/[0-9]+ X client' "$OUT" | tail -1)"
[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
python3 - "$PNG" <<'PY'
from PIL import Image
import sys, os
p=sys.argv[1]
if not os.path.exists(p): print("XU7: no png"); sys.exit(1)
im=Image.open(p).convert("RGB"); W,H=im.size; px=im.load()
blue=sum(1 for y in range(0,H,4) for x in range(0,W,4) if px[x,y][2]>px[x,y][0]+20 and px[x,y][2]>80)
white=sum(1 for y in range(0,H,4) for x in range(0,W,4) if min(px[x,y])>230)
toprow=sum(1 for x in range(0,W,2) if any(max(px[x,y])>40 for y in range(0,30)))
print(f"wallpaper-blue cells: {blue}; white(window) cells: {white}; panel top-cover: {toprow}/{W//2}")
print("XU7 FULL:", "wallpaper="+("Y" if blue>500 else "n"), "window="+("Y" if white>200 else "n"), "panel="+("Y" if toprow>W//2*0.6 else "n"))
PY
