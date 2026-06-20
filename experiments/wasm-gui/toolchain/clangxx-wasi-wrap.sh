#!/usr/bin/env bash
REAL="/home/nathan/secure-exec/registry/native/c/vendor/wasi-sdk/bin/clang++"
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
