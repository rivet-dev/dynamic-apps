#!/usr/bin/env bash
# Desktop-boot-time measurement driver for the DESKTOP-BOOT-PERF loop. Stages the Xfce session fixtures,
# builds the sx-wintest image, then runs the full multi-app desktop headless N times (RUNS, default 3)
# and reports desktop-boot-time (wall from host launch until guest-fb coverage settles = all apps
# painted). Determinism = the spread across runs + how many timed out.
#
# Env: RUNS (default 3), BINDIR (target/release default; target/debug for fast iteration),
#      XFCE_CLIENTS, SX_BOOT_TIMEOUT (default 180), SECURE_EXEC_PERFCLOCK (1 = capture select-block).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
command -v docker >/dev/null || { echo "docker required"; exit 1; }
: "${RUNS:=3}"; : "${BINDIR:=target/release}"; : "${SX_BOOT_TIMEOUT:=180}"
: "${SECURE_EXEC_PERFCLOCK:=1}"

HOST_BIN="$REPO/$BINDIR/wasm-gui-host"; SIDECAR_BIN="$REPO/$BINDIR/secure-exec-sidecar"
if [ ! -x "$HOST_BIN" ] || [ ! -x "$SIDECAR_BIN" ]; then
  echo "MISSING $HOST_BIN or $SIDECAR_BIN — build first:"
  echo "  CARGO_HOME=/home/nathan/sx-wg-cargo cargo build ${BINDIR##*/} -p wasm-gui-host --features window -p secure-exec-sidecar"
  exit 1
fi
for f in Xvfb dbus-daemon xfconfd xfwm4 xfce4-panel xfdesktop thunar mousepad; do
  [ -f "$EXP/$f.wasm" ] || { echo "MISSING $f.wasm"; exit 1; }
done

# --- stage the session fixtures on the host (identical to verify-desktop-headless.sh) ---
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

echo "building sx-wintest image..."
docker build -t sx-wintest "$EXP/scripts/window-test" >/dev/null || exit 1
OUT="$EXP/scripts/window-test/out"; mkdir -p "$OUT"

RESULTS=()
for run in $(seq 1 "$RUNS"); do
  echo "=================== boot run $run/$RUNS (BINDIR=$BINDIR) ==================="
  LOG="$OUT/boot-run${run}.log"
  docker run --rm \
    -e W="${W:-800}" -e H="${H:-600}" -e BINDIR="$BINDIR" \
    -e XFCE_CLIENTS="${XFCE_CLIENTS:-xfwm4 xfce4-panel xfdesktop thunar mousepad}" \
    -e SX_BOOT_TIMEOUT="$SX_BOOT_TIMEOUT" -e APP_SETTLE_MS="${APP_SETTLE_MS:-6000}" \
    -e SX_SERIAL_LAUNCH="${SX_SERIAL_LAUNCH:-0}" \
    -e SECURE_EXEC_PERFCLOCK="$SECURE_EXEC_PERFCLOCK" \
    -e SECURE_EXEC_WASM_SKIP_PREWARM="${SECURE_EXEC_WASM_SKIP_PREWARM:-0}" \
    -e SECURE_EXEC_WAKEPROF="${SECURE_EXEC_WAKEPROF:-0}" \
    -e SECURE_EXEC_DATA_NOTIFIER="${SECURE_EXEC_DATA_NOTIFIER:-0}" \
    -e SECURE_EXEC_INLINE_PEER_NOTIFY="${SECURE_EXEC_INLINE_PEER_NOTIFY:-0}" \
    -e SECURE_EXEC_KEEP_NAMES="${SECURE_EXEC_KEEP_NAMES:-0}" \
    -v "$REPO:/repo" \
    -v "$FONTS:/tmp/vmfonts:ro" -v "$LOCALE:/tmp/vmlocale:ro" -v "$XFT:/tmp/vmxft:ro" \
    -v "$THEMES:/tmp/vmthemes:ro" -v "$WMDATA:/tmp/vmxfwm4:ro" -v "$XKB:/tmp/vmxkb:ro" \
    -v "$SCHEMAS:/tmp/vmschemas:ro" -v "$SESS:/tmp/vmxu3sess:ro" -v "$FIX:/tmp/vmxu3sess-dbus:ro" \
    -v "$OUT:/out" \
    -v "$EXP/scripts/window-test/inside-measure.sh:/inside-measure.sh:ro" \
    sx-wintest bash /inside-measure.sh 2>&1 | tee "$LOG"
  BOOT=$(grep -aoE "BOOT_MS=[0-9]+" "$LOG" | tail -1 | cut -d= -f2)
  if [ -n "$BOOT" ]; then RESULTS+=("$BOOT"); else RESULTS+=("TIMEOUT"); fi
  # Preserve the per-run sidecar host.log for forensics (esp. total-black runs).
  cp -f "$OUT/host.log" "$OUT/host-run${run}.log" 2>/dev/null || true
done

echo "======================== SUMMARY ========================"
echo "BINDIR=$BINDIR  RUNS=$RUNS  clients=${XFCE_CLIENTS:-xfwm4 xfce4-panel xfdesktop thunar mousepad}"
printf 'run boot-times (ms): %s\n' "${RESULTS[*]}"
python3 - "${RESULTS[@]}" <<'PY'
import sys
vals = [int(x) for x in sys.argv[1:] if x.isdigit()]
to = [x for x in sys.argv[1:] if not x.isdigit()]
if vals:
    vals.sort()
    med = vals[len(vals)//2]
    print(f"finite runs: {len(vals)}/{len(sys.argv)-1}  min={vals[0]}ms  median={med}ms  max={vals[-1]}ms  ({med/1000:.1f}s median)")
if to:
    print(f"TIMEOUTS: {len(to)}/{len(sys.argv)-1}")
PY
