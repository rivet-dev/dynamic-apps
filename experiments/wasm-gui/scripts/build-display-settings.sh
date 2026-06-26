#!/usr/bin/env bash
# XU6/XU1+: build the UNMODIFIED xfce4-settings 4.18.4 APPEARANCE dialog (xfce4-display-settings) to wasm
# -- the core Xubuntu "look" settings UI (Style/Icons/Fonts/Settings tabs). The xfce4-settings source is
# already fetched + configured (build-xfce4-settings.sh, --disable-pluggable-dialogs => standalone dialog
# binaries). We compile the dialog's .o's and direct-link (libtool binary discarded), per the app recipe.
# Constraint #5: upstream untouched; platform fixes only.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfce4-settings"
[ -d "$SRC/dialogs/display-settings" ] || { echo "FATAL: xfce4-settings not configured (run build-xfce4-settings.sh)"; exit 1; }
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"

cd "$SRC"
rm -f dialogs/display-settings/xfce4-display-settings dialogs/display-settings/*.o
echo "== compile common/ + the display dialog objects (libtool binary discarded) =="
make -k -j4 -C common LDFLAGS="$LINK" >> /tmp/make-display.log 2>&1 || true
make -k -j4 -C dialogs/display-settings LDFLAGS="$LINK" >> /tmp/make-display.log 2>&1 || true
# display-settings links the dialog's objects PLUS the common/ lib objects -- xfce_randr_new() lives in
# common/xfce-randr.c (libxfce4_settings_la-*.o); without them the link leaves it as an unresolvable env
# import (LinkError at instantiate). The common lib is a no-main settings helper, safe to pull in.
OBJS="$(ls "$SRC"/dialogs/display-settings/xfce4_display_settings-*.o "$SRC"/common/libxfce4_settings_la-*.o 2>/dev/null)"
echo "dialog objects: $(echo $OBJS | wc -w)"
[ -n "$OBJS" ] || { echo "no objects built"; tail -25 /tmp/make-display.log; exit 1; }

RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"; "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfconf-0 exo-2 2>/dev/null)
BIN="$SRC/dialogs/display-settings/xfce4-display-settings"
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat $OBJS -o "$BIN" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-display.log
RC=$?; echo "display-settings link rc=$RC"
if [ ! -f "$BIN" ]; then echo "no binary"; grep -aiE "error|undefined symbol" /tmp/make-display.log | head -12; exit 1; fi
echo "binary $(stat -c%s "$BIN") bytes; __wasi_init_tp: $("$WSDK/bin/llvm-nm" "$BIN" 2>/dev/null | grep -acawE '__wasi_init_tp')"

echo "== fpcast-emu + -Oz -> xfce4-display-settings.wasm =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/xfce4-display-settings.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$BIN" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built xfce4-display-settings.wasm ($(stat -c%s "$OUT") bytes)"
