#!/usr/bin/env bash
REAL="${SECURE_EXEC_TOOLCHAIN_HOME:-/home/nathan/secure-exec}/registry/native/c/vendor/wasi-sdk/bin/clang++"
args=()
for a in "$@"; do
  case "$a" in
    -Wl,--start-group|-Wl,--end-group|--start-group|--end-group) continue ;;
    -Wl,-rpath,*|-Wl,-rpath|-rpath) continue ;;
    -Wl,--enable-new-dtags|-Wl,-soname,*) continue ;;
    -Wl,-Bsymbolic-functions|-Wl,-Bsymbolic|-Bsymbolic-functions|-Bsymbolic) continue ;;
    -Wl,--export-dynamic|--export-dynamic|-Wl,--version-script,*) continue ;;
    -Wl,--as-needed|-Wl,--no-as-needed|--as-needed|--no-as-needed) continue ;;
    -ldl) continue ;;
    -pthread|-lpthread)
      [ "${SECURE_EXEC_WASM_THREADS:-0}" = "1" ] && args+=("$a"); continue ;;
    *'$ORIGIN'*) continue ;;
  esac
  args+=("$a")
done
exec "$REAL" "${args[@]}"
