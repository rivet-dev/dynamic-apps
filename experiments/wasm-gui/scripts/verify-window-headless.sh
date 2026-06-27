#!/usr/bin/env bash
# Headless end-to-end verification of the interactive window (scripts/run-desktop.sh) on Linux, using a
# Docker-provided virtual display (Xvfb). Builds the window host, runs the wasm desktop inside a container
# with a virtual X display, and screenshots the winit window to prove the full pipeline (guest Xvfb + WM +
# apps -> framebuffer -> native window). Output PNG: scripts/window-test/out/window.png.
#
# This is the CI/no-desktop path. On a real macOS/Linux desktop, just run scripts/run-desktop.sh instead.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"

command -v docker >/dev/null || { echo "docker required for the headless verify path"; exit 1; }

FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true

HOST_BIN="$REPO/target/debug/wasm-gui-host"; SIDECAR_BIN="$REPO/target/debug/secure-exec-sidecar"
if [ -x "$HOST_BIN" ] && [ -x "$SIDECAR_BIN" ]; then
  echo "using pre-built binaries in target/debug (build them yourself to pick up code changes:"
  echo "  cargo build -p wasm-gui-host --features window -p secure-exec-sidecar   # fresh checkout: pnpm install first)"
else
  echo "building host (window feature) + sidecar... (a fresh checkout needs 'pnpm install' first — see CLAUDE.md)"
  ( cd "$REPO" && cargo build -p wasm-gui-host --features window -p secure-exec-sidecar ) || {
    echo "build failed and no pre-built binaries present"; exit 1; }
fi

echo "building the sx-wintest virtual-display image..."
docker build -t sx-wintest "$EXP/scripts/window-test" >/dev/null || exit 1

mkdir -p "$EXP/scripts/window-test/out"
echo "running the window demo under a virtual display..."
docker run --rm \
  -e SX_WAIT="${SX_WAIT:-30}" \
  -v "$REPO:/repo" \
  -v "$FONTS:/tmp/vmfonts:ro" -v "$LOCALE:/tmp/vmlocale:ro" \
  -v "$EXP/scripts/window-test/out:/out" \
  -v "$EXP/scripts/window-test/inside.sh:/inside.sh:ro" \
  sx-wintest bash /inside.sh

OUT="$EXP/scripts/window-test/out/window.png"
if [ -s "$OUT" ]; then echo "VERIFIED: $OUT ($(du -h "$OUT" | cut -f1))"; else echo "FAILED: no screenshot produced"; exit 1; fi
