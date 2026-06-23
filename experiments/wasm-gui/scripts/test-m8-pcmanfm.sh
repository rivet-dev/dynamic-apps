#!/usr/bin/env bash
# M8.5: run the real LXDE file manager (pcmanfm) headless on the wasm X server and capture the
# framebuffer. Acceptance: pcmanfm opens a window showing a real directory listing of the kernel VFS.
# All wasm in secure-exec (Xvfb + optional openbox + pcmanfm). Mirrors test-m8-lxpanel.sh.
#
#   WITH_WM=1   also launch openbox so the window is decorated (default off: 2-party pcmanfm<->Xvfb).
#   PCMANFM_DIR the VFS path pcmanfm should open (default /usr — a populated base-fs dir).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
FONTS="${VMFONTS:-/tmp/vmfonts}"
LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"
PCMANFM_DIR="${PCMANFM_DIR:-/usr/share}"
OUT="${OUT:-/tmp/pcmanfm-run.log}"
FB="${FB:-$HOME/tmp/gui-progress/proof-m8.5-pcmanfm-lists-vfs.png}"
XTRACE_LOG="${XTRACE_LOG:-/tmp/pcmanfm-xtrace.log}"
TIMEOUT="${TIMEOUT:-30}"

[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true

HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$HOST" ]    || ( cd "$REPO" && cargo build -p wasm-gui-host )
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar )

for f in "$EXP/Xvfb.wasm" "$EXP/pcmanfm.wasm"; do
  [ -f "$f" ] || { echo "MISSING $f — build the wasm artifacts first"; exit 1; }
done

# each --client value is a single space-separated "wasm_path arg1 arg2 ..." spec
CLIENTS=(--client "$EXP/pcmanfm.wasm --new-win $PCMANFM_DIR")
if [ "${WITH_WM:-0}" = "1" ]; then
  [ -f "$EXP/openbox.wasm" ] || { echo "MISSING openbox.wasm"; exit 1; }
  CLIENTS=(--client "$EXP/openbox.wasm" "${CLIENTS[@]}")
fi

VMTREES=()
[ -d "$XFT" ] && VMTREES+=(--vm-tree "$XFT")

echo "running pcmanfm (dir=$PCMANFM_DIR WITH_WM=${WITH_WM:-0}) -> fb=$FB  log=$OUT"
env -u DISPLAY SECURE_EXEC_XTRACE=2000000 \
  ${SECURE_EXEC_STACKDUMP_AFTER_MS:+SECURE_EXEC_STACKDUMP_AFTER_MS=$SECURE_EXEC_STACKDUMP_AFTER_MS} \
  "$HOST" --xdemo \
  --server "$EXP/Xvfb.wasm" \
  "${CLIENTS[@]}" \
  --fb-out "$FB" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  "${VMTREES[@]}" \
  --sidecar "$SIDECAR" \
  --timeout "$TIMEOUT" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  >"$OUT" 2>&1

grep -a "\[xtrace\]" "$OUT" > "$XTRACE_LOG" || true
echo "done. xtrace lines: $(wc -l < "$XTRACE_LOG" 2>/dev/null || echo 0)  (full log: $OUT)"
