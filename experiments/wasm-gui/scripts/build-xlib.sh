#!/usr/bin/env bash
# Cross-compile one autotools X.Org library/app to wasm32-wasip1 against the patched sysroot.
# Usage: build-xlib.sh <dir-under-third_party> [extra configure args...]
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
DIR="$1"; shift || true
SRC="$EXP/third_party/$DIR"
[ -d "$SRC" ] || { echo "no such source dir: $SRC"; exit 1; }
cd "$SRC"

echo "== configuring $DIR =="
./configure $CROSS_CONFIGURE_ARGS "$@" > /tmp/conf-$DIR.log 2>&1
if [ $? -ne 0 ]; then echo "CONFIGURE FAILED ($DIR); tail:"; tail -25 /tmp/conf-$DIR.log; exit 1; fi

echo "== building $DIR =="
make -j4 > /tmp/make-$DIR.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED ($DIR); tail:"; tail -30 /tmp/make-$DIR.log; exit 1; fi

echo "== installing $DIR =="
make install > /tmp/install-$DIR.log 2>&1 || { echo "INSTALL FAILED ($DIR)"; tail -15 /tmp/install-$DIR.log; exit 1; }
echo "OK: $DIR built + installed to wasm-prefix"
