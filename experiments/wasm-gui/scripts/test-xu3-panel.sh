#!/usr/bin/env bash
# XU3 (in progress): render the xfce4-panel bar, all wasm. Stack: dbus + xfconfd (serves the staged
# xfce4-panel config) + xfce4-panel. PASS-so-far = the panel bar renders (a horizontal band) without the
# fork-migration crash. Plugins (Whisker/tasklist/clock/systray) are the remaining work (they load via
# gmodule/dlopen, which the sandbox lacks). A WM (xfwm4) can be added once the full layout works.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/dbus-daemon.wasm" "$EXP/xfconfd.wasm" "$EXP/xfce4-panel.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done

FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; THEMES="${VMTHEMES:-/tmp/vmthemes}"; PANELCFG="${VMPANEL:-/tmp/vmxfce4panel}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-xfce4-panel.sh" "$PANELCFG" >/dev/null 2>&1 || true

FIX=/tmp/vmxu3
bash "$EXP/scripts/prepare-dbus-fixtures.sh" "$FIX" >/dev/null
mkdir -p "$FIX/etc" "$FIX/var/lib/dbus"
printf '0123456789abcdef0123456789abcdef\n' > "$FIX/etc/machine-id"
cp -f "$FIX/etc/machine-id" "$FIX/var/lib/dbus/machine-id"

W=800; H=600
FB="$(mktemp /tmp/xu3-panel-fb.XXXXXX.bin)"
OUT="${OUT:-/tmp/xu3-panel.log}"
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu3-panel.png}"
mkdir -p "$(dirname "$PNG")"

echo "running XU3 panel -> fb=$FB png=$PNG log=$OUT"
timeout 110 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-50}" \
  --server "$EXP/Xvfb.wasm" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  --client "$EXP/xfce4-panel.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --vm-tree "$FIX" --vm-tree "$PANELCFG" --vm-tree "$THEMES" --vm-tree "$XFT" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "$OUT" 2>&1 || true

echo "=== panel evidence ==="
grep -aiE "fork|migration|Gtk-ERROR|unreachable|No window manager|plugin|exited with code" "$OUT" | grep -aviE "NETWRITE|NETREAD" | head -8
[ -s "$FB" ] && python3 "$EXP/scripts/fb2png.py" "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
python3 - "$PNG" <<'PY'
from PIL import Image
import sys, os
p=sys.argv[1]
if not os.path.exists(p): print("XU3 PANEL: no png"); sys.exit(1)
im=Image.open(p).convert("RGB"); W,H=im.size; px=im.load()
nb=[(x,y) for y in range(0,H,2) for x in range(0,W,2) if max(px[x,y])>25]
if not nb: print("XU3 PANEL: nothing rendered"); sys.exit(1)
xs=[q[0] for q in nb]; ys=[q[1] for q in nb]
x0,x1,y0,y1=min(xs),max(xs),min(ys),max(ys)
print(f"rendered bbox: x[{x0}..{x1}] y[{y0}..{y1}] ({x1-x0}x{y1-y0})")
bar = (x1-x0) > W*0.8 and (y1-y0) < 60
print("XU3 PANEL:", "BAR renders" if bar else "rendered (not a clear bar)")
sys.exit(0 if bar else 1)
PY