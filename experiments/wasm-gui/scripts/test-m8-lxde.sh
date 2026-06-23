#!/usr/bin/env bash
# M8.6 (= M8 ACCEPTANCE): bring up the full LXDE shell as ONE coherent, live desktop, all wasm in
# secure-exec — openbox (WM) + lxpanel (panel/menu) + pcmanfm (file manager) against a single Xvfb.wasm
# X server, then capture the composite framebuffer. This is the hand-written session launcher the SPEC
# allows in lieu of lxsession (whose dbus/autostart machinery is heavy and unneeded here): the host's
# concurrent-guest runner already sequences clients like a session manager (WM first, gated on readiness,
# then each app once the previous settles), which is exactly an LXDE startup.
#
# Acceptance: the panel renders at the bottom AND a decorated pcmanfm window shows a real VFS directory
# listing — panel + menu + file manager, live, one screenshot.
#
#   PCMANFM_DIR  the VFS path pcmanfm opens (default / — a populated dir; /usr/share is empty in base fs).
#   TIMEOUT      overall budget; openbox has no "handleevents"-style ready print so the WM gate uses the
#                host's 12s fallback, then lxpanel + pcmanfm each settle (~1.5s) before the next — give it room.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
FONTS="${VMFONTS:-/tmp/vmfonts}"
LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"
ICONS="${VMICONS:-/tmp/vmicons}"
PANELCFG="${VMLXPANEL:-/tmp/vmlxpanel}"
OPENBOXCFG="${VMOPENBOX:-/tmp/vmopenbox}"
PCMANFM_DIR="${PCMANFM_DIR:-/}"
OUT="${OUT:-/tmp/lxde-run.log}"
FB="${FB:-$HOME/tmp/gui-progress/proof-m8.6-lxde-session.png}"
XTRACE_LOG="${XTRACE_LOG:-/tmp/lxde-xtrace.log}"
TIMEOUT="${TIMEOUT:-70}"

# --- fixtures (data/config staged into the VFS via --vm-tree; not source changes) ---
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
[ -d "$ICONS" ]  || bash "$EXP/scripts/prepare-icons.sh" "$ICONS" >/dev/null 2>&1 || true
# lxpanel's parser is line-based — always regenerate.
bash "$EXP/scripts/prepare-lxpanel.sh" "$PANELCFG" >/dev/null 2>&1 || true
[ -d "$OPENBOXCFG" ] || bash "$EXP/scripts/prepare-openbox-fixtures.sh" "$OPENBOXCFG" >/dev/null 2>&1 || true

HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$HOST" ]    || ( cd "$REPO" && cargo build -p wasm-gui-host )
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar )

for f in "$EXP/Xvfb.wasm" "$EXP/openbox.wasm" "$EXP/lxpanel.wasm" "$EXP/pcmanfm.wasm"; do
  [ -f "$f" ] || { echo "MISSING $f — build the wasm artifacts first"; exit 1; }
done

# The session: WM first (the host gates the rest on it), then the apps. APPS selects which apps follow
# the WM (default the full LXDE shell). Used to bisect the session (e.g. APPS="lxpanel").
APPS="${APPS:-lxpanel pcmanfm}"
WM="${WM:-openbox}"   # window manager (openbox default); set WM=twm to A/B against the M5 WM
CLIENTS=(--client "$EXP/$WM.wasm")
for app in $APPS; do
  case "$app" in
    lxpanel) CLIENTS+=(--client "$EXP/lxpanel.wasm") ;;
    pcmanfm) CLIENTS+=(--client "$EXP/pcmanfm.wasm --new-win $PCMANFM_DIR") ;;
    xclock) CLIENTS+=(--client "$EXP/xclock.wasm") ;;
    *) echo "unknown APP: $app" >&2; exit 1 ;;
  esac
done

VMTREES=()
for d in "$XFT" "$ICONS" "$PANELCFG" "$OPENBOXCFG"; do
  [ -d "$d" ] && VMTREES+=(--vm-tree "$d")
done

echo "running LXDE session (openbox+lxpanel+pcmanfm, dir=$PCMANFM_DIR) -> fb=$FB  log=$OUT"
env -u DISPLAY SECURE_EXEC_XTRACE="${SECURE_EXEC_XTRACE:-2000000}" \
  ${SECURE_EXEC_STACKDUMP_AFTER_MS:+SECURE_EXEC_STACKDUMP_AFTER_MS=$SECURE_EXEC_STACKDUMP_AFTER_MS} \
  "$HOST" --xdemo \
  --server "$EXP/Xvfb.wasm" \
  "${CLIENTS[@]}" \
  --fb-out "$FB" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  "${VMTREES[@]}" \
  --sidecar "$SIDECAR" \
  --timeout "$TIMEOUT" \
  -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
     ${XVFB_EXTRA:-} \
  >"$OUT" 2>&1

grep -a "\[xtrace\]" "$OUT" > "$XTRACE_LOG" || true
echo "done. clients launched: $(grep -ac 'secure-exec: launched' "$OUT")  xtrace lines: $(wc -l < "$XTRACE_LOG" 2>/dev/null || echo 0)  (full log: $OUT)"
echo "framebuffer: $FB"
