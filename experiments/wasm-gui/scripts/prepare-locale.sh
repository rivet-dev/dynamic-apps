#!/usr/bin/env bash
# Stage the minimal libX11 locale database the VM needs for XCreateFontSet / XSupportsLocale to work
# (Xt apps like xclock/xterm abort their widget realize without a usable fontset). Output goes to
# /tmp/vmlocale (override with $1); the host installs it into the VM with --locale-dir.
set -uo pipefail
cd "$(dirname "$0")/.."
source toolchain/cross-env.sh
SRC="$PREFIX/share/X11/locale"
OUT="${1:-/tmp/vmlocale}"
[ -f "$SRC/locale.dir" ] || { echo "no locale data at $SRC (build libX11 first)"; exit 1; }
rm -rf "$OUT"; mkdir -p "$OUT"
# C/POSIX locale + the directory indexes XSupportsLocale consults. iso8859-1 is the C-locale charset
# and our X core fonts already cover it, so the C locale is enough for the desktop's Latin text.
cp "$SRC/locale.dir" "$SRC/locale.alias" "$SRC/compose.dir" "$OUT/" 2>/dev/null
cp -r "$SRC/C" "$OUT/"
echo "staged $(find "$OUT" -type f | wc -l) locale files into $OUT"
