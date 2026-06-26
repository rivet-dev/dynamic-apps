#!/usr/bin/env bash
# XU6: cross-compile UNMODIFIED xfce4-screenshooter 1.11.1 (the Xubuntu screenshot tool) to
# wasm32-wasip1-threads, via the PROVEN app recipe (build-ristretto.sh): direct (non-libtool) final link
# (libc.a __init_tls.o pulled -> __wasi_init_tp), errno.o bundle, gio-vfs-local + empty-path shims, the
# libxfce4ui GResource force-link. Deps (gtk/libxfce4ui/util/panel/exo/xfconf) already built.
# Constraint #5: upstream untouched; all fixes in the toolchain/build layer.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfce4-screenshooter"
[ -d "$SRC" ] || { echo "FATAL: $SRC not extracted"; exit 1; }

cd "$SRC"
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"; CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
find . -name Makefile -type f -delete 2>/dev/null
rm -f config.status config.cache config.log src/xfce4-screenshooter 2>/dev/null
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' \) -delete 2>/dev/null
find . -name '.libs' -type d -exec rm -rf {} + 2>/dev/null
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfce4-screenshooter =="
./configure $CROSS_CONFIGURE_ARGS --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-screenshooter.log 2>&1
RC=$?; if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED:"; tail -30 /tmp/conf-screenshooter.log; exit 1; fi
echo "configure OK"

WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"
"$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

echo "== compiling screenshooter objects (libtool binary discarded) =="
rm -f "$SRC/src/xfce4-screenshooter"
# Non-recursive automake (top Makefile builds src/ + lib/ directly). -k keeps going so the main-app .o's
# build despite the OPTIONAL panel-plugin failing (it needs libxfce4panel headers, not staged; not needed
# for the standalone screenshot tool). We only consume src/*.o + lib/*.o below.
make -k -j4 >> /tmp/make-screenshooter.log 2>&1 || true
echo "make done ($(ls "$SRC"/src/*.o "$SRC"/lib/*.o 2>/dev/null | wc -l) main-app objects)"
WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfce4util-1.0 libxfconf-0 exo-2 2>/dev/null)
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat \
  "$SRC"/src/*.o "$SRC"/lib/*.o \
  -o "$SRC/src/xfce4-screenshooter" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-screenshooter.log
RC=$?
echo "screenshooter direct-link rc=$RC"
if [ -f "$SRC/src/xfce4-screenshooter" ]; then
  echo "binary $(stat -c%s "$SRC/src/xfce4-screenshooter") bytes; __wasi_init_tp defined: $("$WSDK/bin/llvm-nm" "$SRC/src/xfce4-screenshooter" 2>/dev/null | grep -acawE '__wasi_init_tp')"
else echo "no binary"; tail -20 /tmp/make-screenshooter.log; exit 1; fi

echo "== fpcast-emu + -Oz -> xfce4-screenshooter.wasm (the runnable guest; see build-gtk-app.sh for the rationale) =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/xfce4-screenshooter.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$SRC/src/xfce4-screenshooter" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built xfce4-screenshooter.wasm ($(stat -c%s "$OUT") bytes) -- render with: scripts/render-app.sh xfce4-screenshooter.wasm"
