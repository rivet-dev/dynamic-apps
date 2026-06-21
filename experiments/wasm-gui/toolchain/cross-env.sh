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

# Phase 0 (WASM-THREADS-SPEC.md): SECURE_EXEC_WASM_THREADS=1 switches the cross-compile to the
# wasi-threads ABI for the GTK closure rebuild — wasm32-wasip1-threads target, the VANILLA threaded
# sysroot, a SEPARATE threaded prefix (so threaded .a's never mix with non-threaded ones), shared/
# imported/growable memory, and -pthread. NOTE: the X-stack patches live in the non-threaded $SYSROOT
# (registry/native/c/sysroot); porting them onto the threaded sysroot is the remaining mechanical Phase
# 0 work, so this profile builds the patch-independent stack (GLib/PCRE2/...) first.
if [ "${SECURE_EXEC_WASM_THREADS:-0}" = "1" ]; then
  WASM_TARGET="wasm32-wasip1-threads"
  THREADS_SYSROOT="$WSDK/share/wasi-sysroot"
  PREFIX="$EXP/third_party/wasm-prefix-threads"
  THREADS_MEM_MAX=$((512*1024*1024))
  THREADS_FLAGS="-pthread -matomics -mbulk-memory -DSECURE_EXEC_WASM_THREADS -D_WASI_EMULATED_SIGNAL"
  THREADS_LDFLAGS="-Wl,--shared-memory -Wl,--import-memory -Wl,--export-memory -Wl,--max-memory=$THREADS_MEM_MAX -Wl,--export=wasi_thread_start -lwasi-emulated-signal"
else
  WASM_TARGET="wasm32-wasip1"
  THREADS_SYSROOT="$SYSROOT"
  THREADS_FLAGS=""
  THREADS_LDFLAGS=""
fi

export CC="$WSDK/bin/clang"
export CXX="$WSDK/bin/clang++"
export AR="$WSDK/bin/llvm-ar"
export RANLIB="$WSDK/bin/llvm-ranlib"
export CFLAGS="--target=$WASM_TARGET --sysroot=$THREADS_SYSROOT -O2 $THREADS_FLAGS -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -D_GNU_SOURCE -mllvm -wasm-enable-sjlj -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-int-conversion -I$EXP/toolchain/compat-include -include $EXP/toolchain/wasi-compat.h"
export CPPFLAGS="--target=$WASM_TARGET --sysroot=$THREADS_SYSROOT -I$PREFIX/include -I$EXP/toolchain/compat-include -include $EXP/toolchain/wasi-compat.h"
# NOTE: wasi-compat.o (stub symbols) is NOT here — libtool rejects non-libtool objects when
# building .la static libraries. It is appended only at the final executable link.
export LDFLAGS="--target=$WASM_TARGET --sysroot=$THREADS_SYSROOT $THREADS_LDFLAGS -L$PREFIX/lib -L$WSDK/share/wasi-sysroot/lib/$WASM_TARGET -lwasi-emulated-mman -lwasi-emulated-process-clocks"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
export PKG_CONFIG_PATH=""
export ACLOCAL_PATH="$PREFIX/share/aclocal"
# Wrapper scripts (clang-wasi-wrap.sh) inherit the toolchain home so their REAL clang resolves.
export SECURE_EXEC_TOOLCHAIN_HOME="$REPO"

CROSS_CONFIGURE_ARGS="--host=wasm32-wasi --prefix=$PREFIX --enable-static --disable-shared --disable-malloc0returnsnull"

# Generate a workspace-local meson cross file with this workspace's compat/prefix paths baked in
# (meson cross files cannot self-locate). Threads mode swaps target/sysroot/c++-include, drops the
# emulated single-threaded pthread for the real one + shared-memory link flags, and uses the threaded
# wasi-compat object (pthread/flockfile stubs #ifdef'd out under -DSECURE_EXEC_WASM_THREADS).
CROSS_INI="$EXP/toolchain/wasi-sdk-cross.gen.ini"
if [ "${SECURE_EXEC_WASM_THREADS:-0}" = "1" ]; then
  MESON_SYSROOT="$THREADS_SYSROOT"
  MESON_LIBSUBDIR="wasm32-wasip1-threads"
  MESON_C_THREADS="'-pthread', '-matomics', '-mbulk-memory', '-DSECURE_EXEC_WASM_THREADS', '-D_WASI_EMULATED_SIGNAL', "
  MESON_C_EMUL_PTHREAD=""
  # NOTE: --export=wasi_thread_start is intentionally NOT here — as a global link arg it fails every
  # meson cc.links() probe (the probe program has no such symbol to export), breaking dependency
  # detection (e.g. GIO's socket() check). It is added only at the final guest-app link.
  MESON_LINK_THREADS="'-Wl,--shared-memory', '-Wl,--import-memory', '-Wl,--export-memory', '-Wl,--max-memory=$THREADS_MEM_MAX', '-lwasi-emulated-signal', "
  MESON_LINK_EMUL_PTHREAD=""
  MESON_WASI_COMPAT_O="$EXP/toolchain/wasi-compat-threads.o"
  # Compile the threaded compat object (pthread/flockfile stubs compiled out) for the final link.
  "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$THREADS_SYSROOT" -O2 -DSECURE_EXEC_WASM_THREADS \
    -I"$EXP/toolchain/compat-include" -c "$EXP/toolchain/wasi-compat.c" -o "$MESON_WASI_COMPAT_O" 2>/dev/null || true
else
  MESON_SYSROOT="$SYSROOT"
  MESON_LIBSUBDIR="wasm32-wasip1"
  MESON_C_THREADS=""
  MESON_C_EMUL_PTHREAD="'-D_WASI_EMULATED_PTHREAD', "
  MESON_LINK_THREADS=""
  MESON_LINK_EMUL_PTHREAD="'-lwasi-emulated-pthread', "
  MESON_WASI_COMPAT_O="$EXP/toolchain/wasi-compat.o"
fi
cat > "$CROSS_INI" <<INI
[constants]
wasi_sdk = '$WSDK'
sysroot = '$MESON_SYSROOT'
vanilla_lib = wasi_sdk / 'share/wasi-sysroot/lib/$MESON_LIBSUBDIR'

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
c_args = ['--target=$WASM_TARGET', '--sysroot=' + sysroot, $MESON_C_THREADS '-D_WASI_EMULATED_MMAN', '-D_GNU_SOURCE', '-D_WASI_EMULATED_PROCESS_CLOCKS', ${MESON_C_EMUL_PTHREAD}'-mllvm', '-wasm-enable-sjlj', '-DHAVE_SYS_RESOURCE_H', '-Wno-error=implicit-function-declaration', '-Wno-implicit-function-declaration', '-Wno-error=format-security', '-Wno-error=format-nonliteral', '-I$PREFIX/include', '-I$EXP/toolchain/compat-include', '-include', '$EXP/toolchain/wasi-compat.h']
c_link_args = ['--target=$WASM_TARGET', '--sysroot=' + sysroot, $MESON_LINK_THREADS '-L$PREFIX/lib', '-L' + vanilla_lib, '-lwasi-emulated-mman', '-lwasi-emulated-process-clocks', ${MESON_LINK_EMUL_PTHREAD}'$MESON_WASI_COMPAT_O']
cpp_args = ['--target=$WASM_TARGET', '--sysroot=' + sysroot, $MESON_C_THREADS '-D_WASI_EMULATED_MMAN', '-D_GNU_SOURCE', ${MESON_C_EMUL_PTHREAD}'-fno-exceptions', '-mllvm', '-wasm-enable-sjlj', '-isystem', wasi_sdk / 'share/wasi-sysroot/include/$MESON_LIBSUBDIR/c++/v1', '-I$PREFIX/include', '-I$EXP/toolchain/compat-include']
cpp_link_args = ['--target=$WASM_TARGET', '--sysroot=' + sysroot, $MESON_LINK_THREADS '-L$PREFIX/lib', '-L' + vanilla_lib, '-lwasi-emulated-mman', ${MESON_LINK_EMUL_PTHREAD}'-lc++', '-lc++abi']
INI

export CROSS_CONFIGURE_ARGS PREFIX REPO WSDK SYSROOT EXP CROSS_INI
