#!/usr/bin/env bash
# Source this to get the wasm32-wasip1 cross-compile environment used for the X stack
# (autotools --host=wasm32-wasi against the patched secure-exec sysroot + host_net sockets).
REPO="/home/nathan/secure-exec"
WSDK="$REPO/registry/native/c/vendor/wasi-sdk"
SYSROOT="$REPO/registry/native/c/sysroot"
PREFIX="$REPO/experiments/wasm-gui/third_party/wasm-prefix"
EXP="$REPO/experiments/wasm-gui"

export CC="$WSDK/bin/clang"
export CXX="$WSDK/bin/clang++"
export AR="$WSDK/bin/llvm-ar"
export RANLIB="$WSDK/bin/llvm-ranlib"
export CFLAGS="--target=wasm32-wasip1 --sysroot=$SYSROOT -O2 -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -D_GNU_SOURCE -mllvm -wasm-enable-sjlj -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-int-conversion -I$EXP/toolchain/compat-include -include $EXP/toolchain/wasi-compat.h"
export CPPFLAGS="--target=wasm32-wasip1 --sysroot=$SYSROOT -I$PREFIX/include -I$EXP/toolchain/compat-include -include $EXP/toolchain/wasi-compat.h"
# NOTE: wasi-compat.o (stub symbols) is NOT here — libtool rejects non-libtool objects when
# building .la static libraries. It is appended only at the final executable link.
export LDFLAGS="--target=wasm32-wasip1 --sysroot=$SYSROOT -L$PREFIX/lib -L$WSDK/share/wasi-sysroot/lib/wasm32-wasip1 -lwasi-emulated-mman -lwasi-emulated-process-clocks"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
export PKG_CONFIG_PATH=""
export ACLOCAL_PATH="$PREFIX/share/aclocal"

CROSS_CONFIGURE_ARGS="--host=wasm32-wasi --prefix=$PREFIX --enable-static --disable-shared --disable-malloc0returnsnull"
export CROSS_CONFIGURE_ARGS PREFIX REPO WSDK SYSROOT EXP
