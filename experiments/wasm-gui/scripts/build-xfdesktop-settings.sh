#!/usr/bin/env bash
# XU6: build the UNMODIFIED xfdesktop-settings (the "Desktop" settings dialog -- wallpaper / menus / icons, a
# Settings Manager entry) to wasm. Clone of the dialog recipe + type-ensure shim + gtk_init_with_args wrap
# (glade root is XfceTitledDialog). No running-WM check (WM=0). Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfdesktop"
[ -d "$SRC/settings" ] || { echo "FATAL: xfdesktop not configured"; exit 1; }
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"

cd "$SRC"
rm -f settings/xfdesktop-settings settings/*.o
echo "== compile xfdesktop settings objects (libtool binary discarded) =="
make -k -j4 -C settings LDFLAGS="$LINK" >> /tmp/make-xfdsettings.log 2>&1 || true
# settings LDADD pulls libxfdesktop (common/*.c: xfdesktop-common (defines xfdesktop_debug_set), marshal, tumbler,
# thumbnailer). Build it via its OWN make -- a direct re-compile fails to find xfce-backdrop.h etc. (the include
# paths live in the common Makefile) -- and link the libtool objects, else undefined env imports -> LinkError.
make -k -j4 -C common LDFLAGS="$LINK" >> /tmp/make-xfdsettings.log 2>&1 || true
COMMONOS="$(ls "$SRC"/common/libxfdesktop_la-*.o 2>/dev/null)"
OBJS="$(ls "$SRC"/settings/xfdesktop_settings-*.o 2>/dev/null) $COMMONOS"
echo "xfdesktop-settings objects: $(echo $OBJS | wc -w) (incl common: $(echo $COMMONOS | wc -w))"
[ -n "$OBJS" ] || { echo "no objects built"; tail -25 /tmp/make-xfdsettings.log; exit 1; }

RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
TYPESHIM="$EXP/toolchain/xfce-type-ensure.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/xfce-type-ensure.c" -o "$TYPESHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"; "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,--wrap=gtk_init_with_args"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfce4util-1.0 libxfconf-0 exo-2 libwnck-3.0 gthread-2.0 2>/dev/null)
BIN="$SRC/settings/xfdesktop-settings"
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat $OBJS "$TYPESHIM" -o "$BIN" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-xfdsettings.log
RC=$?; echo "xfdesktop-settings link rc=$RC"
if [ ! -f "$BIN" ]; then echo "no binary"; grep -aiE "error|undefined symbol" /tmp/make-xfdsettings.log | head -12; exit 1; fi
echo "binary $(stat -c%s "$BIN") bytes"

echo "== fpcast-emu + -Oz -> xfdesktop-settings.wasm =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/xfdesktop-settings.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$BIN" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built xfdesktop-settings.wasm ($(stat -c%s "$OUT") bytes)"
