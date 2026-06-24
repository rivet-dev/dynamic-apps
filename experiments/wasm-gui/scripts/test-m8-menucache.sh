#!/usr/bin/env bash
# M8.3 acceptance: prove the menu-cache freedesktop *enumeration* runs in-sandbox. Upstream LXDE builds
# the application menu by fork/exec'ing the menu-cached daemon -> menu-cache-gen; the sandbox has no
# fork/exec, so we build menu-cache-gen (the one-shot generator; no daemon, no fork/exec) as a wasm
# guest and run it directly against a staged freedesktop fixture tree. PASS = it composes the full
# categorized application menu (Applications -> the category submenus) headless in the VM.
#
# Constraint #5: menu-cache builds from UNMODIFIED upstream. The only build-layer glue is a guarded
# unity TU (clang's -fno-common default + an LLVM -fcommon ICE on wasm rule out per-TU compilation of
# menu-tags.h's tentative-def globals) — the upstream .c files are #included verbatim.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
# Capture THIS workspace's root before sourcing cross-env.sh (which overwrites REPO to the toolchain
# home / main repo). The host + sidecar with the --guest-env/--vm-tree-in-exec support this test needs
# live in THIS workspace, so resolve them from WGREPO, not the cross-env REPO.
WGREPO="$(cd ../.. && pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
HOST="$WGREPO/target/debug/wasm-gui-host"; SIDECAR="$WGREPO/target/debug/secure-exec-sidecar"
[ -x "$HOST" ]    || { echo "MISSING $HOST — build wasm-gui-host in this workspace first"; exit 1; }
[ -x "$SIDECAR" ] || { echo "MISSING $SIDECAR — build secure-exec-sidecar in this workspace first"; exit 1; }
[ -f "$PREFIX/lib/libmenu-cache.a" ] || bash "$EXP/scripts/build-libfm.sh" || { echo "menu-cache build failed"; exit 1; }

MCG="$EXP/third_party/menu-cache-threads/menu-cache-gen"
WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"; LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
HOSTOBJS="$EXP/toolchain/threads-libs/host_socket.o $EXP/toolchain/threads-libs/host_pipe_dup.o $EXP/toolchain/threads-libs/override_fcntl.o"
INC="-I$MCG/../libmenu-cache -I$EXP/third_party/wasm-prefix-threads/include/menu-cache"
PCCF=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --cflags glib-2.0 gio-2.0 libfm-extra)
PCLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --libs glib-2.0 gio-2.0 libfm-extra)
VER='-DPACKAGE_VERSION="1.1.0" -DMENUCACHE_LIBEXECDIR="/usr/libexec" -DG_LOG_DOMAIN="menu-cache-gen"'

# --- build menu-cache-gen.wasm (guarded unity build; private compat object dodges the shared-tree cleaner) ---
BT=/tmp/mcg-build; rm -rf "$BT"; mkdir -p "$BT"
cp "$MCG/main.c" "$MCG/menu-compose.c" "$MCG/menu-merge.c" "$BT/"
printf '#ifndef MCG_GUARD\n#define MCG_GUARD\n#include "%s/menu-tags.h"\n#endif\n' "$MCG" > "$BT/menu-tags.h"
printf '#include "main.c"\n#include "menu-compose.c"\n#include "menu-merge.c"\n' > "$BT/unity.c"
CC_O=/tmp/mcg-compat-threads.o
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -D_WASI_EMULATED_MMAN \
  -DSECURE_EXEC_WASM_THREADS -I"$EXP/toolchain/compat-include" -c "$EXP/toolchain/wasi-compat.c" -o "$CC_O" || exit 1
"$CC" $CFLAGS $PCCF -I"$BT" $INC -I"$PREFIX/include" $VER -c "$BT/unity.c" -o /tmp/mcg-unity.o || { echo "unity compile FAIL"; exit 1; }
"$CC" $LDFLAGS -Wl,--allow-undefined -o "$EXP/menu-cache-gen.wasm" /tmp/mcg-unity.o \
  "$CC_O" $HOSTOBJS $PCLIBS -lhostcompat -lwasi-emulated-signal "$SETJMP" "$LIBC" || { echo "link FAIL"; exit 1; }
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
wasm-opt --fpcast-emu --enable-bulk-memory --enable-threads -O0 "$EXP/menu-cache-gen.wasm" -o /tmp/mcg.fp \
  && mv /tmp/mcg.fp "$EXP/menu-cache-gen.wasm"
echo "built menu-cache-gen.wasm ($(stat -c%s "$EXP/menu-cache-gen.wasm")b)"

# --- stage freedesktop fixtures + run the generator headless ---
FIX="${VMMENUCACHE:-/tmp/vmmenucache}"
bash "$EXP/scripts/prepare-menucache-fixtures.sh" "$FIX" >/dev/null
out="$(env -u DISPLAY "$HOST" --exec --guest "$EXP/menu-cache-gen.wasm" --vm-tree "$FIX" \
  --guest-env G_MESSAGES_DEBUG=all --timeout 35 --sidecar "$SIDECAR" \
  -- -i /menus/sandbox-applications.menu -o /data/menu-cache.out -l C -v 2>&1)"
echo "$out" | grep -aE "menu-cache-gen-DEBUG.*(entering|done|new AppDir)" | sed 's/.*menu-cache-gen-DEBUG: [0-9:.]* //'

# PASS if the generator composed the Applications root + at least 3 of the category submenus.
cats=$(echo "$out" | grep -acE "entering (Accessories|Internet|Office|System)")
if echo "$out" | grep -q "done Applications" && [ "$cats" -ge 3 ]; then
  echo "M8.3 MENU-CACHE ENUMERATION: PASS ($cats categories composed)"; exit 0
else
  echo "M8.3 MENU-CACHE ENUMERATION: FAIL"; exit 1
fi
