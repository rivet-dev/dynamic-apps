#!/usr/bin/env bash
# M8.2 acceptance — openbox WM interaction: decorate + interactive MOVE + root MENU, all wasm in
# secure-exec (Xvfb + openbox + a decorated X client), driven by injected XTEST pointer events.
#
# Two scenarios (each captures its FINAL framebuffer; the host injects after the WM+client settle):
#   SCENARIO=move  grab the titlebar and drag the window to a new location (asserts it moved).
#   SCENARIO=menu  right-click the root (button 3) to open the openbox root menu (asserts it rendered).
#
# The move uses the host-assisted drag path (the same path the live --desktop window uses; XTEST
# FakeMotion does not warp the pointer while the WM holds its move grab). The menu is PURE openbox
# behaviour: ButtonPress on the Root context -> ShowMenu root-menu, from the staged menu.xml fixture.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"   # fontconfig config + cache dir (menu/Xft text needs it)
OPENBOXCFG="${VMOPENBOX:-/tmp/vmopenbox}"
SCENARIO="${SCENARIO:-move}"
BUCKET="${BUCKET:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)}"; mkdir -p "$BUCKET"
FB="${FB:-$BUCKET/proof-m8.2-openbox-$SCENARIO.png}"
OUT="${OUT:-/tmp/ob-$SCENARIO.log}"
TIMEOUT="${TIMEOUT:-60}"

[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1 || true
[ -d "$OPENBOXCFG" ] || bash "$EXP/scripts/prepare-openbox-fixtures.sh" "$OPENBOXCFG" >/dev/null 2>&1 || true
[ -x "$HOST" ]    || ( cd "$REPO" && cargo build -p wasm-gui-host )
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar )
for f in "$EXP/Xvfb.wasm" "$EXP/openbox.wasm" "$EXP/xclock.wasm"; do
  [ -f "$f" ] || { echo "MISSING $f"; exit 1; }
done

VMTREES=()
for d in "$XFT" "$OPENBOXCFG"; do [ -d "$d" ] && VMTREES+=(--vm-tree "$d"); done

# The xclock window maps decorated at ~ x[318..482] y[206..392] @ 800x600 (titlebar ~ y=214, center x=400).
case "$SCENARIO" in
  move)  # grab titlebar (400,214) and drag down-left to (180,470)
    INJECT=(--inject "h=motion 400 214" --inject "h=buttondn 1"
            --inject "h=motion 320 330" --inject "h=motion 230 420" --inject "h=motion 180 470"
            --inject "h=buttonup 1") ;;
  menu)  # right-click an exposed root spot far from the window to open the openbox root menu
    INJECT=(--inject "h=motion 620 140" --inject "h=button 3 620 140") ;;
  *) echo "unknown SCENARIO=$SCENARIO (move|menu)"; exit 1 ;;
esac

echo "running openbox SCENARIO=$SCENARIO -> fb=$FB  log=$OUT"
env -u DISPLAY SECURE_EXEC_XTRACE="${SECURE_EXEC_XTRACE:-2000000}" \
  APP_SETTLE_MS="${APP_SETTLE_MS:-3000}" \
  WM_SETTLE_QUIET_MS="${WM_SETTLE_QUIET_MS:-3000}" \
  INJECT_DELAY_MS="${INJECT_DELAY_MS:-6000}" \
  SECURE_EXEC_POLL_WAITERS="${SECURE_EXEC_POLL_WAITERS:-4}" \
  "$HOST" --xdemo \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/openbox.wasm" \
  --client "$EXP/xclock.wasm" \
  "${INJECT[@]}" \
  --fb-out "$FB" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  "${VMTREES[@]}" \
  --sidecar "$SIDECAR" \
  --timeout "$TIMEOUT" \
  -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  >"$OUT" 2>&1

echo "done -> $FB"
