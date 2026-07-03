#!/usr/bin/env bash
# Headless end-to-end verification of the INTERACTIVE multi-app Xfce desktop (scripts/run-desktop.sh) on a
# box with no display, using Docker + Xvfb. Builds the window host + sidecar, stages the session fixtures,
# then runs the full Xfce stack in a container, injects REAL mouse+keyboard into the winit window via
# xdotool, and captures before/after screenshots proving the input round-trip changes the desktop (the
# XU7 IN-BAND acceptance). Outputs: scripts/window-test/out/desktop-{before,after}.png.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
command -v docker >/dev/null || { echo "docker required"; exit 1; }

HOST_BIN="$REPO/target/debug/wasm-gui-host"; SIDECAR_BIN="$REPO/target/debug/secure-exec-sidecar"
if [ ! -x "$HOST_BIN" ] || [ ! -x "$SIDECAR_BIN" ]; then
  echo "building host (window) + sidecar... (fresh checkout: pnpm install first)"
  test -d /home/nathan/sx-wg-cargo && CH=/home/nathan/sx-wg-cargo || CH="$HOME/.cargo"
  ( cd "$REPO" && CARGO_HOME="$CH" cargo build -p wasm-gui-host --features window -p secure-exec-sidecar ) || exit 1
fi

for f in Xvfb dbus-daemon xfconfd xfwm4 xfce4-panel xfdesktop thunar mousepad; do
  [ -f "$EXP/$f.wasm" ] || { echo "MISSING $f.wasm — build the Xfce guests first (XUBUNTU-SPEC.md)"; exit 1; }
done

# --- stage the session fixtures on the host (mirrors scripts/test-xu7-full.sh) ---
FONTS=/tmp/vmfonts; LOCALE=/tmp/vmlocale; XFT=/tmp/vmxft; THEMES=/tmp/vmthemes; WMDATA=/tmp/vmxfwm4
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-themes.sh" "$THEMES" >/dev/null 2>&1 || true
bash "$EXP/scripts/prepare-xfwm4.sh" "$WMDATA" >/dev/null 2>&1 || true
XKB=/tmp/vmxkb
[ -f "$XKB/xkb/default.xkm" ] || bash "$EXP/scripts/prepare-xkb.sh" "$XKB" >/dev/null 2>&1 || true
SCHEMAS=/tmp/vmschemas; mkdir -p "$SCHEMAS"
SESS=/tmp/vmxu3sess
PLUGINS="${PLUGINS:-clock tasklist systray separator}" bash "$EXP/scripts/prepare-xfce4-panel.sh" "$SESS" >/dev/null 2>&1
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

echo "building sx-wintest image (adds xdotool + python3)..."
docker build -t sx-wintest "$EXP/scripts/window-test" >/dev/null || exit 1
mkdir -p "$EXP/scripts/window-test/out"

echo "running the interactive Xfce desktop under a virtual display + injecting real input..."
docker run --rm \
  -e SX_WAIT="${SX_WAIT:-150}" -e APP_SETTLE_MS="${APP_SETTLE_MS:-6000}" \
  -e SX_XTEST_DELAY_S="${SX_XTEST_DELAY_S:-0}" -e SX_FB_STREAM_MS="${SX_FB_STREAM_MS:-33}" \
  -e PAINT_SETTLE="${PAINT_SETTLE:-150}" \
  -v "$REPO:/repo" \
  -v "$FONTS:/tmp/vmfonts:ro" -v "$LOCALE:/tmp/vmlocale:ro" -v "$XFT:/tmp/vmxft:ro" \
  -v "$THEMES:/tmp/vmthemes:ro" -v "$WMDATA:/tmp/vmxfwm4:ro" -v "$XKB:/tmp/vmxkb:ro" \
  -v "$SCHEMAS:/tmp/vmschemas:ro" -v "$SESS:/tmp/vmxu3sess:ro" -v "$FIX:/tmp/vmxu3sess-dbus:ro" \
  -v "$EXP/scripts/window-test/out:/out" \
  -v "$EXP/scripts/window-test/inside-xfce.sh:/inside-xfce.sh:ro" \
  sx-wintest bash /inside-xfce.sh

B="$EXP/scripts/window-test/out/desktop-before.png"; A="$EXP/scripts/window-test/out/desktop-after.png"
[ -s "$A" ] && echo "VERIFIED artifacts: $B , $A" || { echo "FAILED: no after screenshot"; exit 1; }
