#!/usr/bin/env bash
# M7.5.0 threads spike build (WASM-THREADS-SPEC.md §0). Builds guest-xclient/threads-test.c to a
# CORRECT wasi-threads binary: IMPORTED, SHARED, GROWABLE memory so the host can supply ONE shared
# WebAssembly.Memory to every isolate-thread. The stock `wasm32-wasip1-threads` build EXPORTS a fixed
# shared memory, which cannot be shared across isolates; --import-memory flips it to host-supplied.
#
# Distinct from the non-threaded profile in cross-env.sh: this targets wasm32-wasip1-threads against
# the VANILLA wasi-sdk threaded sysroot (NOT the patched non-threaded $SYSROOT), with -pthread.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="${SECURE_EXEC_TOOLCHAIN_HOME:-/home/nathan/secure-exec}"
WSDK="$REPO/registry/native/c/vendor/wasi-sdk"
TSYS="$WSDK/share/wasi-sysroot"
CC="$WSDK/bin/clang"

OUT="$EXP/guest-xclient/threads-test.wasm"
SRC="$EXP/guest-xclient/threads-test.c"

# Shared memory MUST declare a maximum and be page-aligned (64 KiB). Growable: initial < max.
INITIAL=$((16*1024*1024))   # 16 MiB
MAXMEM=$((64*1024*1024))    # 64 MiB (spike; GTK profile raises this)

"$CC" \
  --target=wasm32-wasip1-threads --sysroot="$TSYS" -O2 -g \
  -pthread \
  -Wl,--import-memory -Wl,--shared-memory \
  -Wl,--initial-memory=$INITIAL -Wl,--max-memory=$MAXMEM \
  -Wl,--export=wasi_thread_start \
  -o "$OUT" "$SRC" 2>&1 | grep -iE "error|undefined|warning: unsupported" | head -20

if [ ! -f "$OUT" ]; then echo "BUILD FAILED"; exit 1; fi
echo "built guest-xclient/threads-test.wasm ($(stat -c%s "$OUT") bytes)"

# Verify the ABI is what the host design needs: imported shared memory + wasi.thread-spawn import +
# wasi_thread_start export. Fail loudly if not, so a toolchain change can't silently regress it.
python3 - "$OUT" <<'PY'
import sys
b=open(sys.argv[1],'rb').read(); p=8
def u(o):
    r=s=0
    while True:
        x=b[o];o+=1;r|=(x&0x7f)<<s
        if not x&0x80: return r,o
        s+=7
imports=[]; exports=[]; mem_imported=False; mem_shared=None; mem_max=None
while p<len(b):
    sid=b[p];p+=1; sz,p=u(p); end=p+sz
    if sid==2:
        n,p=u(p)
        for _ in range(n):
            ml,p=u(p); mod=b[p:p+ml].decode();p+=ml
            nl,p=u(p); nm=b[p:p+nl].decode();p+=nl
            k=b[p];p+=1
            if k==0: _,p=u(p)
            elif k==1:
                p+=1; fl=b[p];p+=1; _,p=u(p)
                if fl&1: _,p=u(p)
            elif k==2:
                fl=b[p];p+=1; _,p=u(p)
                if fl&1: mem_max,p=u(p)
                if mod=='env' and nm=='memory':
                    mem_imported=True; mem_shared=bool(fl&2)
            elif k==3: p+=2
            imports.append(f"{mod}.{nm}")
    elif sid==7:
        n,p=u(p)
        for _ in range(n):
            nl,p=u(p); nm=b[p:p+nl].decode();p+=nl
            k=b[p];p+=1; _,p=u(p); exports.append(nm)
    p=end
ok=True
def check(c,msg):
    global ok
    print(("  OK  " if c else " FAIL ")+msg); ok=ok and c
check(mem_imported, f"memory is IMPORTED (env.memory)  [imported={mem_imported}]")
check(mem_shared is True, f"imported memory is SHARED  [shared={mem_shared}]")
check(mem_max is not None, f"imported memory declares a maximum  [max_pages={mem_max}]")
check('wasi.thread-spawn' in imports, "imports wasi.thread-spawn")
check('wasi_thread_start' in exports, "exports wasi_thread_start")
check('_start' in exports, "exports _start")
print("ABI:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
PY
