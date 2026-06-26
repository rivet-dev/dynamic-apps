#!/usr/bin/env bash
# Build libwasmeh-full.a: the C++ wasm exception-handling runtime the wasi-sdk omits
# (its libc++abi is built -fno-exceptions). Produces ONE consistent EH-enabled libc++abi
# + libunwind for wasm32-wasip1-threads so C++ try/catch (typed catches included) works.
#
# THE KEY FIX (constraint #5, toolchain layer): libunwind's Unwind-wasm.c declares
#   _Thread_local struct _Unwind_LandingPadContext __wasm_lpad_context;
# but clang 19.1.5 -fwasm-exceptions emits the compiler-generated landing pad's accesses to
# __wasm_lpad_context as NON-TLS (R_WASM_MEMORY_ADDR_LEB), while the _Thread_local definition
# makes libunwind access it as TLS (R_WASM_MEMORY_ADDR_TLS_SLEB). The two then read/write
# DIFFERENT memory: the landing pad writes the non-TLS lsda/lpad_index, the personality reads
# the TLS copy (== 0) -> scan_eh_tab sees a null LSDA -> every TYPED catch returns
# _URC_CONTINUE_UNWIND -> the throw escapes to JS -> std::terminate. (catch(...) works because
# the wasm `catch_all` instruction catches directly without calling the personality.)
# FIX: define __wasm_lpad_context as a plain (non-TLS) global so libunwind matches the
# compiler's non-TLS access. Verified: lsda becomes non-zero and `catch (int)` catches.
set -uo pipefail
cd "$(dirname "$0")/.."
export SECURE_EXEC_WASM_THREADS=1; source toolchain/cross-env.sh 2>/dev/null
CXXABI="$REPO/registry/native/c/vendor/llvm-project/libcxxabi"
CXX="$REPO/registry/native/c/vendor/llvm-project/libcxx"
LU="$REPO/registry/native/c/vendor/llvm-project/libunwind"
CXXF=$(echo "${CXXFLAGS:-}" | sed 's/-fno-exceptions//g')
CF=$(echo "${CFLAGS:-}" | sed 's/-fno-exceptions//g')
INCS="-I$CXXABI/include -I$CXXABI/src -I$CXX/src -I$WSDK/share/wasi-sysroot/include/c++/v1"
DEFS="-D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY -DLIBCXXABI_SILENT_TERMINATE -D_LIBCXXABI_HAS_NO_THREADS"
OBJ=$(mktemp -d /tmp/libwasmeh.XXXXXX)
echo "[libwasmeh] compiling libc++abi EH sources with -fwasm-exceptions ..."
FAIL=""
for f in "$CXXABI"/src/*.cpp; do
  b=$(basename "$f" .cpp)
  # cxa_noexception.cpp is the -fno-exceptions STUB; it duplicates cxa_exception.cpp's symbols. Skip it.
  [ "$b" = "cxa_noexception" ] && continue
  "$WSDK/bin/clang++" $CXXF -std=c++20 -fwasm-exceptions $DEFS $INCS -c "$f" -o "$OBJ/$b.o" 2>/dev/null || FAIL="$FAIL $b"
done
echo "[libwasmeh] compiled $(ls "$OBJ"/*.o 2>/dev/null | wc -l) libc++abi objs; FAILED:$FAIL"
echo "[libwasmeh] compiling libunwind Unwind-wasm.c with the non-TLS __wasm_lpad_context fix ..."
sed 's/_Thread_local struct _Unwind_LandingPadContext __wasm_lpad_context;/struct _Unwind_LandingPadContext __wasm_lpad_context; \/* non-TLS: match clang -fwasm-exceptions MEMORY_ADDR access *\//' \
  "$LU/src/Unwind-wasm.c" > "$OBJ/Unwind-wasm.c"
grep -q 'non-TLS: match' "$OBJ/Unwind-wasm.c" || { echo "[libwasmeh] ERROR: non-TLS patch did not apply"; exit 1; }
"$WSDK/bin/clang" $CF -DNDEBUG -fwasm-exceptions -I"$LU/include" -I"$LU/src" -c "$OBJ/Unwind-wasm.c" -o "$OBJ/Unwind-wasm.o" || exit 1
OUT="$PREFIX/lib/libwasmeh-full.a"
rm -f "$OUT"
"$WSDK/bin/llvm-ar" crs "$OUT" "$OBJ/Unwind-wasm.o" "$OBJ"/*.o
echo "[libwasmeh] wrote $OUT ($(du -h "$OUT" | cut -f1))"
echo "[libwasmeh] link C++ apps with: -nostdlib++ ... -lwasmeh-full   (NO --import-undefined on the type_info/LSDA path)"
rm -rf "$OBJ"
