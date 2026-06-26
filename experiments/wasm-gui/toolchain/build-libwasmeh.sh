#!/usr/bin/env bash
# Build libwasmeh-full.a: the C++ wasm exception-handling runtime the wasi-sdk omits
# (its libc++abi is built -fno-exceptions). Produces ONE consistent EH-enabled libc++abi
# + libunwind for wasm32-wasip1-threads so C++ try/catch (typed catches included) works.
#
# THIS BUILDS THE ALL-THREADS variant (-pthread -matomics -mbulk-memory), which is what every
# real guest uses. Two facts make threads non-negotiable here and a clean build mandatory:
#
# 1. __wasm_lpad_context TLS-ness must match. clang -fwasm-exceptions emits the compiler-generated
#    landing pad's accesses to __wasm_lpad_context. In a THREADED TU (-matomics) those are TLS
#    (R_WASM_MEMORY_ADDR_TLS_SLEB); libunwind's Unwind-wasm.c declares __wasm_lpad_context
#    `_Thread_local`, so KEEP it _Thread_local (do NOT patch it non-TLS) to match. The landing pad
#    writes lpad_index+lsda; _Unwind_CallPersonality reads them back; if the TLS-ness disagrees they
#    touch different memory, the personality reads lsda==0, scan_eh_tab returns _URC_CONTINUE_UNWIND,
#    and every TYPED catch escapes -> std::terminate. (catch(...) still works: the wasm `catch_all`
#    catches without the personality.) Verified: threaded eh-tref reports lsda non-zero, catch(int) catches.
#
# 2. ALL objects in the final link must be threaded. A single NON-threads object makes wasm-ld demote
#    the TLS data segment to non-TLS .bss, which then rejects the TLS reloc / yields lsda==0 at runtime.
#    So compile every cxa source AND the unwind with the threads flags below. (A clean cross-object
#    _Thread_local links + runs fine; mixing a non-threads object is what breaks it.)
#
# Link C++ apps with: -nostdlib++ ... -lc++ (libc++ for std::, WITHOUT the sysroot's -fno-exceptions
# libc++abi) -lwasmeh-full (this lib supplies the EH-enabled libc++abi). Do NOT also pull the default
# libc++abi (it overlaps and is -fno-exceptions).
set -uo pipefail
cd "$(dirname "$0")/.."
export SECURE_EXEC_WASM_THREADS=1; source toolchain/cross-env.sh 2>/dev/null
CXXABI="$REPO/registry/native/c/vendor/llvm-project/libcxxabi"
CXX="$REPO/registry/native/c/vendor/llvm-project/libcxx"
LU="$REPO/registry/native/c/vendor/llvm-project/libunwind"
SR="$WSDK/share/wasi-sysroot"
T="--target=wasm32-wasip1-threads --sysroot=$SR -pthread -matomics -mbulk-memory"
INCS="-I$CXXABI/include -I$CXXABI/src -I$CXX/src -isystem $SR/include/wasm32-wasip1-threads/c++/v1"
DEFS="-D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY -DLIBCXXABI_SILENT_TERMINATE -D_LIBCXXABI_HAS_NO_THREADS"
OBJ=$(mktemp -d /tmp/libwasmeh.XXXXXX)
echo "[libwasmeh] compiling libc++abi EH sources (-fwasm-exceptions, threads) ..."
FAIL=""
for f in "$CXXABI"/src/*.cpp; do
  b=$(basename "$f" .cpp)
  # cxa_noexception.cpp is the -fno-exceptions STUB; it duplicates cxa_exception.cpp's symbols. Skip it.
  [ "$b" = "cxa_noexception" ] && continue
  "$WSDK/bin/clang++" $T -std=c++20 -fwasm-exceptions $DEFS $INCS -c "$f" -o "$OBJ/$b.o" 2>/dev/null || FAIL="$FAIL $b"
done
echo "[libwasmeh] compiled $(ls "$OBJ"/*.o 2>/dev/null | wc -l) libc++abi objs; FAILED:$FAIL"
echo "[libwasmeh] compiling libunwind Unwind-wasm.c (threads, _Thread_local KEPT) ..."
"$WSDK/bin/clang" $T -DNDEBUG -fwasm-exceptions -I"$LU/include" -I"$LU/src" -c "$LU/src/Unwind-wasm.c" -o "$OBJ/Unwind-wasm.o" || exit 1
[ "$("$WSDK/bin/llvm-objdump" -r "$OBJ/Unwind-wasm.o" 2>/dev/null | grep -ac TLS_SLEB)" -gt 0 ] || { echo "[libwasmeh] ERROR: unwind __wasm_lpad_context is not TLS"; exit 1; }
OUT="$PREFIX/lib/libwasmeh-full.a"
rm -f "$OUT"
"$WSDK/bin/llvm-ar" crs "$OUT" "$OBJ/Unwind-wasm.o" "$OBJ"/*.o
echo "[libwasmeh] wrote $OUT ($(du -h "$OUT" | cut -f1)) -- all-threads, TLS __wasm_lpad_context"
rm -rf "$OBJ"
