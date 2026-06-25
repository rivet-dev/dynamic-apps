#!/usr/bin/env bash
# XU2 interaction (completes the DoD): xfwm4 MOVES a window via an XTEST titlebar drag, all wasm. Stack:
# dbus + xfconfd (xfwm4 channel theme=Greybird) + xfwm4 (WM) + gtk-hello (decorated window). The host
# injects a real XTEST drag on the Greybird titlebar and we assert the window's content bbox relocated.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
for f in "$HOST" "$SIDECAR" "$XVFB" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfwm4.wasm" "$EXP/guest-xclient/gtk-hello.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done

FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"; WMDATA="${VMXFWM4:-/tmp/vmxfwm4}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-xfwm4.sh" "$WMDATA" >/dev/null 2>&1 || true

FIX=/tmp/vmxu2move
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
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
FB="$(mktemp /tmp/xu2-move-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu2-move.log}"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu2-xfwm4-move.png}"
mkdir -p "$(dirname "$PNG")"

# The decorated gtk-hello window sits centered ~ x[220..580] y[187..411]; titlebar ~ y=199. Grab the
# title text area (350,199) -- left of the min/max/close buttons -- and drag down-left to (210,360).
INJECT=(--inject "h=motion 350 199" --inject "h=buttondn 1"
        --inject "h=motion 300 250" --inject "h=motion 250 310" --inject "h=motion 210 360"
        --inject "h=buttonup 1")

echo "running XU2 xfwm4 MOVE -> fb=$FB png=$PNG log=$OUT"
WM_SETTLE_QUIET_MS=3000 WM_SETTLE_CAP_S=45 APP_SETTLE_MS=3000 INJECT_DELAY_MS="${INJECT_DELAY_MS:-72000}" \
timeout 180 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-120}" \
  --server "$XVFB" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfwm4.wasm" \
  --client "$EXP/guest-xclient/gtk-hello.wasm" \
  "${INJECT[@]}" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$THEMES" --vm-tree "$WMDATA" --vm-tree "$XFT" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "$OUT" 2>&1 || true

[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
python3 - "$PNG" <<'PY'
from PIL import Image
import sys, os
p=sys.argv[1]
if not os.path.exists(p): print("XU2 MOVE: no png"); sys.exit(1)
im=Image.open(p).convert("RGB"); W,H=im.size; px=im.load()
def iswhite(c): return c[0]>235 and c[1]>235 and c[2]>235
xs=[x for y in range(H) for x in range(0,W,2) if iswhite(px[x,y])]
ys=[y for y in range(H) for x in range(0,W,3) if iswhite(px[x,y])]
if not xs: print("XU2 MOVE: no window mapped"); sys.exit(1)
x0,x1,y0,y1=min(xs),max(xs),min(ys),max(ys)
print(f"window bbox after drag: x[{x0}..{x1}] y[{y0}..{y1}] (centered start was ~x[220..580] y[187..411])")
# A successful move shifts the window left (x0 well below 220) and down (y0 above 187).
moved = x0 < 180 and y0 > 210
print("XU2 MOVE:", "PASS (window relocated by the titlebar drag)" if moved else "INCONCLUSIVE (did not relocate as expected)")
sys.exit(0 if moved else 1)
PY