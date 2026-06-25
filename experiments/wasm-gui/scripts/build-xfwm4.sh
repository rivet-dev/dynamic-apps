#!/usr/bin/env bash
# XU2: cross-compile UNMODIFIED xfwm4 4.18 (the real Xfce window manager) to wasm32-wasip1-threads. The
# binary we need is src/xfwm4 (the WM that decorates/manages windows with the Greybird xfwm4 theme).
# Proven Xfce autotools + GTK recipe (clone of build-xfce4-settings.sh). Constraint #5: upstream
# untouched; wasi/platform gaps live in the toolchain/runtime layer.
#   Compositor OFF (software-rendered, no GPU) -> --disable-compositor; the missing optional X libs
#   (libstartup-notification, libXpresent) are disabled too. render + randr stay on (libs present).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="4.18.0"
SRC="$TP/xfwm4"
for l in libxfce4util.a libxfce4ui-2.a libxfconf-0.a libwnck-3.a libXinerama.a libhostcompat.a libdbuscreds.a; do
  [ -f "$PREFIX/lib/$l" ] || { echo "FATAL: $l missing"; exit 1; }
done

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/xfwm4.tar.bz2" \
    "https://archive.xfce.org/src/xfce/xfwm4/4.18/xfwm4-$PV.tar.bz2" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf xfwm4.tar.bz2 && mv "xfwm4-$PV" xfwm4 )
fi

cd "$SRC"
cp "$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)" \
   "$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)" . 2>/dev/null || true
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfwm4 =="
# ★ --datadir=/usr/share (+ sysconfdir=/etc): xfwm4 loads its 'defaults' settings file (and themes)
# from the COMPILE-TIME PACKAGE_DATADIR (parseRc("defaults", PACKAGE_DATADIR) in settings.c), NOT via
# XDG. With the default --prefix=<wasm-prefix>/share that path doesn't exist in the VM ("Missing
# defaults file" -> exit 1). Point DATADIR at the in-VM /usr/share where prepare-xfwm4.sh stages it
# (this is how distros build it). Libs/headers still come from the wasm prefix via CFLAGS/pkg-config.
./configure $CROSS_CONFIGURE_ARGS \
  --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  --disable-compositor --disable-startup-notification --disable-xpresent --disable-epoxy \
  > /tmp/conf-xfwm4.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-xfwm4.log; exit 1; fi
echo "== building xfwm4 (src/) =="
WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
# WM extras beyond the GTK transitive set: libwnck (workspace/window list) + Xinerama (multi-monitor,
# configure-required) + the render/randr X exts used for decoration drawing.
GTKTRANS="-lwnck-3 -lXinerama -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
# Two MANDATORY GTK link flags (--wrap=writev for libxcb's X-setup writev to host_net + an 8MB stack for
# GTK's deep init) and the GDBus creds shims (xfwm4 uses xfconf over GDBus, like xfsettingsd).
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"
# common/ first (builds libxfwm-common.la that src/xfwm4 links), then the WM binary in src/.
make -j4 -C common LDFLAGS="$LINK" > /tmp/make-xfwm4.log 2>&1
make -j4 -C src LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-xfwm4.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -45 /tmp/make-xfwm4.log; exit 1; fi
echo "OK: xfwm4 built."
find . -name "xfwm4" -type f -not -path "*.deps*" | while read f; do echo "  $(stat -c%s "$f") $f"; done
