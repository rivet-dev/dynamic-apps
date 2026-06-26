#!/usr/bin/env bash
# XU4 completion: xfdesktop's ROOT MENU (right-click the desktop -> the garcon applications menu), all wasm.
# Builds on test-xu4-xfdesktop.sh (dbus + xfconfd + xfwm4 + xfdesktop + wallpaper) and adds: the garcon
# root-menu fixture (prepare-rootmenu-fixtures.sh: xfce-applications.menu + .desktop apps) mounted into the
# VM, then a host XTEST RIGHT-CLICK (button 3) at the desktop center to pop the menu, captured.
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
MENUFIX=/tmp/vmrootmenu; bash "$EXP/scripts/prepare-rootmenu-fixtures.sh" "$MENUFIX" >/dev/null

FIX=/tmp/vmxu4menu; rm -rf "$FIX"
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
  <property name="desktop-menu" type="empty"><property name="show" type="bool" value="true"/></property>
</channel>
X
cat > "$CHDIR/xfwm4.xml" <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0"><property name="general" type="empty"><property name="theme" type="string" value="Greybird"/><property name="use_compositing" type="bool" value="false"/></property></channel>
X
ICONS="${VMICONS:-/tmp/vmicons}"; [ -d "$ICONS" ] || bash "$EXP/scripts/prepare-icons.sh" "$ICONS" >/dev/null 2>&1 || true
GTKCFG="$FIX/root/.config/gtk-3.0"; mkdir -p "$GTKCFG"
printf '[Settings]\ngtk-icon-theme-name=Adwaita\n' > "$GTKCFG/settings.ini"
W=800; H=600
FB="$(mktemp /tmp/xu4menu-fb.XXXXXX.bin)"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu4-rootmenu.png}"; mkdir -p "$(dirname "$PNG")"
# Right-click the desktop centre to pop the root menu (button 3 = right). Press-and-HOLD (buttondn, no
# buttonup) so the menu pops on press and stays grabbed open for the capture (a full click's release can
# dismiss/select). Override INJECT_MODE=click to test the press+release variant.
if [ "${INJECT_MODE:-hold}" = click ]; then
  INJECT=(--inject "h=motion 400 300" --inject "h=button 3 400 300")
else
  INJECT=(--inject "h=motion 400 300" --inject "h=buttondn 3")
fi
echo "running XU4 root menu -> png=$PNG"
WM_SETTLE_QUIET_MS=2500 WM_SETTLE_CAP_S=45 APP_SETTLE_MS=4000 \
timeout 200 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-130}" \
  --server "$EXP/Xvfb.wasm" --dbus "$EXP/dbus-daemon.wasm" --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" --client "$EXP/xfdesktop.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$MENUFIX" --vm-tree "$THEMES" --vm-tree "$WMDATA" --vm-tree "$XFT" --vm-tree "$ICONS" \
  "${INJECT[@]}" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > "${OUT:-/tmp/xu4menu.log}" 2>&1 || true
[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
echo "PNG: $PNG"
echo "menu refs in log: $(grep -acE 'garcon|menu|applications.menu' "${OUT:-/tmp/xu4menu.log}" 2>/dev/null)"
grep -aiE "garcon|could not|failed to|menu" "${OUT:-/tmp/xu4menu.log}" 2>/dev/null | head -5
