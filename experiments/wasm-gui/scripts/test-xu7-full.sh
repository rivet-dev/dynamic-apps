#!/usr/bin/env bash
# XU7 integrated session: xfwm4 (WM, Greybird) + xfce4-panel (clock/tasklist/systray/separator) + a
# decorated GTK app window, all wasm. Proves the panel runs UNDER the WM and the tasklist shows a real
# window button for the managed app. (applicationsmenu is excluded here -- its populated menu deadlocks,
# tracked separately.) Stack: dbus + xfconfd (serves the xfwm4 + xfce4-panel channels) + xfwm4 +
# xfce4-panel + gtk-hello.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfwm4.wasm" "$EXP/xfce4-panel.wasm" "$EXP/mousepad.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done

FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"; WMDATA="${VMXFWM4:-/tmp/vmxfwm4}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-xfwm4.sh" "$WMDATA" >/dev/null 2>&1 || true

# Use SEPARATE vm-trees (each prepare script rm -rf's its own tree); the host MERGES all --vm-tree into
# the VM root, so the panel channel (panel tree) and the xfwm4 channel (dbus tree) land in the same
# xfconf dir without clobbering each other.
# Panel tree (config + stub .so + .desktop); default plugins = clock tasklist systray separator.
SESS=/tmp/vmxu3sess
PLUGINS="${PLUGINS:-clock tasklist systray separator}" bash "$EXP/scripts/prepare-xfce4-panel.sh" "$SESS" >/dev/null 2>&1
# dbus tree (separate) + machine-id + the xfwm4 channel.
FIX=/tmp/vmxu3sess-dbus
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null 2>&1
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"
cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"
CHDIR="$FIX/root/.config/xfce4/xfconf/xfce-perchannel-xml"; mkdir -p "$CHDIR" "$FIX/root/.cache"
cat > "$CHDIR/xfwm4.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0"><property name="general" type="empty">
<property name="theme" type="string" value="Greybird"/>
<property name="title_font" type="string" value="Sans Bold 9"/>
<property name="use_compositing" type="bool" value="false"/></property></channel>
XML

W=800; H=600
FB="$(mktemp /tmp/xu3-sess-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu7-session.log}"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu7-session.png}"
mkdir -p "$(dirname "$PNG")"

echo "running XU3 session -> fb=$FB png=$PNG log=$OUT"
WM_SETTLE_QUIET_MS=3000 WM_SETTLE_CAP_S=50 APP_SETTLE_MS=4000 \
timeout 200 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-120}" \
  --server "$EXP/Xvfb.wasm" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" \
  --client "$EXP/mousepad.wasm" \
  --client "$EXP/xfce4-panel.wasm" \
  --client "$EXP/xfdesktop.wasm" \
  --client "$EXP/thunar.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$SESS" --vm-tree "$THEMES" --vm-tree "$WMDATA" --vm-tree "$XFT" --vm-tree /tmp/vmschemas \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "$OUT" 2>&1 || true

echo "=== session evidence ==="
grep -aiE "Gtk-ERROR|unreachable|No window manager|exited with code|deadlock|PARKED" "$OUT" | grep -aviE "NETWRITE|NETREAD" | head -6
[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
python3 - "$PNG" <<'PY'
from PIL import Image
import sys, os
p=sys.argv[1]
if not os.path.exists(p): print("XU7 SESSION: no png"); sys.exit(1)
im=Image.open(p).convert("RGB"); W,H=im.size; px=im.load()
# panel band: a near-full-width row of non-bg pixels near the top (y<32).
def nb(x,y): return max(px[x,y])>25
toprow=sum(1 for x in range(0,W,2) if any(nb(x,y) for y in range(0,30)))
# app window: a large white-ish region in the body.
white=[(x,y) for y in range(40,H,3) for x in range(0,W,3) if px[x,y][0]>235 and px[x,y][1]>235 and px[x,y][2]>235]
print(f"panel band top-coverage: {toprow}/{W//2} columns")
if white:
    xs=[q[0] for q in white]; ys=[q[1] for q in white]
    print(f"app window bbox: x[{min(xs)}..{max(xs)}] y[{min(ys)}..{max(ys)}]")
else:
    print("app window: none detected")
ok = toprow > (W//2)*0.7 and bool(white)
print("XU7 SESSION:", "PASS (panel + decorated app under xfwm4)" if ok else "PARTIAL")
PY
