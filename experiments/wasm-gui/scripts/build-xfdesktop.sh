#!/usr/bin/env bash
# XU4: cross-compile UNMODIFIED xfdesktop 4.18 to wasm32-wasip1-threads (wallpaper + desktop icons + root
# menu). The root menu (garcon) is DISABLED (--disable-desktop-menu) because the populated garcon menu
# hits the GIO worker-context deadlock; the wallpaper + icons don't need it. Same Xfce autotools + GTK
# recipe as build-xfce4-panel.sh, incl. the libxfce4ui + libwnck GResource force-links. Constraint #5:
# upstream untouched; platform gaps in the toolchain/runtime layer.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
SRC="$TP/xfdesktop"
[ -d "$SRC" ] || { echo "FATAL: $SRC not fetched"; exit 1; }
for l in libxfce4util.a libxfce4ui-2.a libxfconf-0.a libwnck-3.a libexo-2.a libhostcompat.a libdbuscreds.a; do
  [ -f "$PREFIX/lib/$l" ] || { echo "FATAL: $l missing"; exit 1; }
done

cd "$SRC"
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"
CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfdesktop =="
# --datadir=/usr/share (compile-time data path, like the panel/xfwm4). Disable the garcon root menu
# (deadlocks), thunarx (no Thunar yet), and the optional bits we don't have.
./configure $CROSS_CONFIGURE_ARGS \
  --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared \
  --disable-desktop-menu --disable-thunarx \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-xfdesktop.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-xfdesktop.log; exit 1; fi
echo "configure OK"

WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
# GResource force-links (same as the panel): libxfce4ui dialog UI + libwnck default icon register-ctors
# get dropped by archive-pull; extract + link the resource .o as a plain object so the ctors run.
RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null \
  && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
WNCKRESO="$EXP/toolchain/libwnck-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libwnck-3.a" libwnck_3_la-wnck-resources.o 2>/dev/null \
  && mv -f libwnck_3_la-wnck-resources.o "$WNCKRESO" )

GTKTRANS="$RESO $WNCKRESO -lxfce4ui-2 -lexo-2 -lwnck-3 -lXinerama -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"
echo "== building common (libxfdesktop) =="
make -j4 -C common LDFLAGS="$LINK" >> /tmp/make-xfdesktop.log 2>&1
echo "== building xfdesktop =="
rm -f "$SRC/src/xfdesktop"
make -j4 -C src LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-xfdesktop.log 2>&1
RC=$?
echo "xfdesktop make rc=$RC"
find . -name "xfdesktop" -type f -not -path "*.deps*" 2>/dev/null | while read f; do echo "  $(stat -c%s "$f") $f"; done
if [ $RC -ne 0 ]; then echo "(link incomplete; tail:)"; tail -30 /tmp/make-xfdesktop.log; fi
