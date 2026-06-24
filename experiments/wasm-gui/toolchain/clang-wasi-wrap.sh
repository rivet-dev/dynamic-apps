#!/usr/bin/env bash
# clang wrapper that strips linker args wasm-ld (lld-wasm) rejects, which meson emits for
# executables (ELF-isms: --start-group/--end-group, -rpath, $ORIGIN). Lets the X stack's
# meson/autotools executable links (cvt tool, eventually Xvfb) succeed.
REAL="${SECURE_EXEC_TOOLCHAIN_HOME:-/home/nathan/secure-exec}/registry/native/c/vendor/wasi-sdk/bin/clang"
args=()
for a in "$@"; do
  case "$a" in
    -Wl,--start-group|-Wl,--end-group|--start-group|--end-group) continue ;;
    -Wl,-rpath,*|-Wl,-rpath|-rpath) continue ;;
    -Wl,--enable-new-dtags|-Wl,-soname,*) continue ;;
    -Wl,--as-needed|-Wl,--no-as-needed|--as-needed|--no-as-needed) continue ;;
    -Wl,--export-dynamic|--export-dynamic|-Wl,--version-script,*) continue ;;
    -ldl) continue ;;
    -pthread|-lpthread)
      # Under the wasi-threads (Phase 0) profile KEEP -pthread: wasi-libc <pthread.h> static_asserts
      # "threads not enabled" without it. Strip only in the non-threaded profile.
      [ "${SECURE_EXEC_WASM_THREADS:-0}" = "1" ] && args+=("$a"); continue ;;
    *'$ORIGIN'*) continue ;;
  esac
  args+=("$a")
done
exec "$REAL" "${args[@]}"
