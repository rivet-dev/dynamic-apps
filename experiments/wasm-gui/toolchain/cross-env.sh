#!/usr/bin/env bash
# Source this to get the wasm32-wasip1 cross-compile environment used for the X stack
# (autotools --host=wasm32-wasi against the patched secure-exec sysroot + host_net sockets).
#
# Workspace-aware: the experiment outputs (PREFIX, build dirs, compat objects) are located relative
# to THIS script, so the cross-compile targets whatever jj workspace it is sourced from. The toolchain
# INPUTS (vendored wasi-sdk + patched sysroot) are large shared read-only build infrastructure that is
# not duplicated per workspace; they resolve from SECURE_EXEC_TOOLCHAIN_HOME (default: the canonical
# /home/nathan/secure-exec checkout).
EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="${SECURE_EXEC_TOOLCHAIN_HOME:-/home/nathan/secure-exec}"
WSDK="$REPO/registry/native/c/vendor/wasi-sdk"
SYSROOT="$REPO/registry/native/c/sysroot"
PREFIX="$EXP/third_party/wasm-prefix"

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
# Wrapper scripts (clang-wasi-wrap.sh) inherit the toolchain home so their REAL clang resolves.
export SECURE_EXEC_TOOLCHAIN_HOME="$REPO"

CROSS_CONFIGURE_ARGS="--host=wasm32-wasi --prefix=$PREFIX --enable-static --disable-shared --disable-malloc0returnsnull"

# Generate a workspace-local meson cross file with this workspace's compat/prefix paths baked in
# (meson cross files cannot self-locate). sysroot/wasi-sdk still point at the toolchain home.
CROSS_INI="$EXP/toolchain/wasi-sdk-cross.gen.ini"
cat > "$CROSS_INI" <<INI
[constants]
wasi_sdk = '$WSDK'
sysroot = '$SYSROOT'
vanilla_lib = wasi_sdk / 'share/wasi-sysroot/lib/wasm32-wasip1'

[binaries]
c = '$EXP/toolchain/clang-wasi-wrap.sh'
cpp = '$EXP/toolchain/clangxx-wasi-wrap.sh'
ar = wasi_sdk / 'bin/llvm-ar'
strip = wasi_sdk / 'bin/llvm-strip'
nm = wasi_sdk / 'bin/llvm-nm'
pkg-config = '/usr/bin/pkg-config'

[properties]
pkg_config_libdir = '$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig'

[host_machine]
system = 'wasi'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'

[built-in options]
default_library = 'static'
c_args = ['--target=wasm32-wasip1', '--sysroot=' + sysroot, '-D_WASI_EMULATED_MMAN', '-D_GNU_SOURCE', '-D_WASI_EMULATED_PROCESS_CLOCKS', '-DHAVE_SYS_RESOURCE_H', '-Wno-error=implicit-function-declaration', '-Wno-implicit-function-declaration', '-I$EXP/toolchain/compat-include', '-include', '$EXP/toolchain/wasi-compat.h']
c_link_args = ['--target=wasm32-wasip1', '--sysroot=' + sysroot, '-L' + vanilla_lib, '-lwasi-emulated-mman', '-lwasi-emulated-process-clocks', '$EXP/toolchain/wasi-compat.o']
cpp_args = ['--target=wasm32-wasip1', '--sysroot=' + sysroot, '-D_WASI_EMULATED_MMAN', '-D_GNU_SOURCE', '-fno-exceptions']
cpp_link_args = ['--target=wasm32-wasip1', '--sysroot=' + sysroot, '-L' + vanilla_lib, '-lwasi-emulated-mman']
INI

export CROSS_CONFIGURE_ARGS PREFIX REPO WSDK SYSROOT EXP CROSS_INI
