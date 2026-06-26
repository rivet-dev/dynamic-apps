#!/usr/bin/env bash
# XU6: build the UNMODIFIED xfce4-notifyd-config (the Notifications settings dialog) to wasm. Clone of the
# dialog recipe + type-ensure shim + gtk_init_with_args wrap (glade root is XfceTitledDialog). No running-WM
# check, so it renders solo. Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfce4-notifyd"
[ -d "$SRC/xfce4-notifyd-config" ] || { echo "FATAL: xfce4-notifyd not configured"; exit 1; }
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"

cd "$SRC"
rm -f xfce4-notifyd-config/xfce4-notifyd-config xfce4-notifyd-config/*.o
echo "== compile xfce4-notifyd-config objects (libtool binary discarded) =="
make -k -j4 -C xfce4-notifyd-config LDFLAGS="$LINK" >> /tmp/make-notifyc.log 2>&1 || true
# The config's LDADD pulls libxfce-notifyd-common (common/*.c: migrate-settings, enum-types, the log-gbus proxy,
# log-types, log-util). The config SOURCES list only the common headers, so compile ALL common/*.c directly and
# link them, else each is an undefined env import -> LinkError.
CFL="$CFLAGS $(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --cflags gtk+-3.0 libxfconf-0 libxfce4util-1.0 gio-2.0 gio-unix-2.0 libnotify 2>/dev/null) -I$SRC"
COMMONOS=""
for c in "$SRC"/common/*.c; do o="${c%.c}.o"; "$EXP/toolchain/clang-wasi-wrap.sh" $CFL -c "$c" -o "$o" >> /tmp/make-notifyc.log 2>&1 || true; COMMONOS="$COMMONOS $o"; done
OBJS="$(ls "$SRC"/xfce4-notifyd-config/xfce4_notifyd_config-*.o 2>/dev/null) $COMMONOS"
echo "notifyd-config objects: $(echo $OBJS | wc -w)"
[ -n "$OBJS" ] || { echo "no objects built"; tail -25 /tmp/make-notifyc.log; exit 1; }

RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
TYPESHIM="$EXP/toolchain/xfce-type-ensure.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/xfce-type-ensure.c" -o "$TYPESHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"; "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,--wrap=gtk_init_with_args"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfce4util-1.0 libxfconf-0 libnotify gio-2.0 gio-unix-2.0 2>/dev/null)
BIN="$SRC/xfce4-notifyd-config/xfce4-notifyd-config"
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat $OBJS "$TYPESHIM" -o "$BIN" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS -lsqlite3 \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-notifyc.log
RC=$?; echo "notifyd-config link rc=$RC"
if [ ! -f "$BIN" ]; then echo "no binary"; grep -aiE "error|undefined symbol" /tmp/make-notifyc.log | head -12; exit 1; fi
echo "binary $(stat -c%s "$BIN") bytes"

echo "== fpcast-emu + -Oz -> xfce4-notifyd-config.wasm =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/xfce4-notifyd-config.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$BIN" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built xfce4-notifyd-config.wasm ($(stat -c%s "$OUT") bytes)"
