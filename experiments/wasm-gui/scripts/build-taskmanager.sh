#!/usr/bin/env bash
# XU6: cross-compile UNMODIFIED xfce4-taskmanager 1.5.7 (the Xubuntu system monitor) to
# wasm32-wasip1-threads, via the PROVEN app recipe (build-screenshooter.sh). Single GTK guest.
# Constraint #5: upstream untouched; all fixes in the toolchain/build layer.
# NOTE: the process listing reads /proc (task-manager-linux.c); in the sandbox /proc is sparse, so the
# process table may be empty/partial -- the WINDOW + UI still render (the bundled-app render bar).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfce4-taskmanager"
[ -d "$SRC" ] || { echo "FATAL: $SRC not extracted"; exit 1; }

cd "$SRC"
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"; CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
find . -name Makefile -type f -delete 2>/dev/null
rm -f config.status config.cache config.log src/xfce4-taskmanager 2>/dev/null
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' \) -delete 2>/dev/null
find . -name '.libs' -type d -exec rm -rf {} + 2>/dev/null
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfce4-taskmanager =="
./configure $CROSS_CONFIGURE_ARGS --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-taskmanager.log 2>&1
RC=$?; if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED:"; tail -30 /tmp/conf-taskmanager.log; exit 1; fi
echo "configure OK"

WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"
"$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

echo "== compiling taskmanager objects =="
rm -f "$SRC/src/xfce4-taskmanager"
make -k -j4 >> /tmp/make-taskmanager.log 2>&1 || true
OBJS="$(ls "$SRC"/src/*.o 2>/dev/null) $(ls "$SRC"/lib/*.o 2>/dev/null)"
echo "make done ($(echo $OBJS | wc -w) objects)"
WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfconf-0 xmu cairo 2>/dev/null)
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat \
  $OBJS \
  -o "$SRC/src/xfce4-taskmanager" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXmu -lXmuu -lXt -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-taskmanager.log
RC=$?
echo "taskmanager direct-link rc=$RC"
if [ -f "$SRC/src/xfce4-taskmanager" ]; then
  echo "binary $(stat -c%s "$SRC/src/xfce4-taskmanager") bytes; __wasi_init_tp: $("$WSDK/bin/llvm-nm" "$SRC/src/xfce4-taskmanager" 2>/dev/null | grep -acawE '__wasi_init_tp')"
else echo "no binary"; tail -20 /tmp/make-taskmanager.log; exit 1; fi

echo "== fpcast-emu + -Oz -> xfce4-taskmanager.wasm =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/xfce4-taskmanager.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$SRC/src/xfce4-taskmanager" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built xfce4-taskmanager.wasm ($(stat -c%s "$OUT") bytes) -- render with: scripts/render-app.sh xfce4-taskmanager.wasm"
