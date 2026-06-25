#!/usr/bin/env bash
# XU6 dep: cross-compile libexif 0.6.24 (ristretto's EXIF dep) to wasm32-wasip1-threads. Pure-C autotools.
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1; source "$EXP/toolchain/cross-env.sh"
SRC="$EXP/third_party/libexif"; [ -d "$SRC" ] || { echo "FATAL: libexif not fetched"; exit 1; }
cd "$SRC"; export CC="$EXP/toolchain/clang-wasi-wrap.sh"
./configure $CROSS_CONFIGURE_ARGS --disable-docs --disable-nls > /tmp/conf-libexif.log 2>&1 || { echo "CONFIGURE FAILED"; tail -15 /tmp/conf-libexif.log; exit 1; }
make -j4 >> /tmp/make-libexif.log 2>&1; make install >> /tmp/make-libexif.log 2>&1
echo "libexif.a: $(stat -c%s "$PREFIX/lib/libexif.a" 2>/dev/null) bytes"
