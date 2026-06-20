#!/usr/bin/env bash
# clang wrapper that strips linker args wasm-ld (lld-wasm) rejects, which meson emits for
# executables (ELF-isms: --start-group/--end-group, -rpath, $ORIGIN). Lets the X stack's
# meson/autotools executable links (cvt tool, eventually Xvfb) succeed.
REAL="/home/nathan/secure-exec/registry/native/c/vendor/wasi-sdk/bin/clang"
args=()
for a in "$@"; do
  case "$a" in
    -Wl,--start-group|-Wl,--end-group|--start-group|--end-group) continue ;;
    -Wl,-rpath,*|-Wl,-rpath|-rpath) continue ;;
    -Wl,--enable-new-dtags|-Wl,-soname,*) continue ;;
    -pthread|-lpthread|-ldl) continue ;;
    *'$ORIGIN'*) continue ;;
  esac
  args+=("$a")
done
exec "$REAL" "${args[@]}"
