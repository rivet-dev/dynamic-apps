#!/usr/bin/env bash
# XU1: cross-compile UNMODIFIED xfce4-settings 4.18 to wasm32-wasip1-threads. The binary we need is
# xfsettingsd (the settings daemon that reads xfconf and publishes XSETTINGS to the X server). Proven
# Xfce autotools recipe. Constraint #5: upstream untouched; wasi gaps in the platform layer.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="4.18.4"
SRC="$TP/xfce4-settings"
for l in libxfce4util.a libxfce4ui-2.a libxfconf-0.a libhostcompat.a libdbuscreds.a; do
  [ -f "$PREFIX/lib/$l" ] || { echo "FATAL: $l missing"; exit 1; }
done

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/xfce4-settings.tar.bz2" \
    "https://archive.xfce.org/src/xfce/xfce4-settings/4.18/xfce4-settings-$PV.tar.bz2" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf xfce4-settings.tar.bz2 && mv "xfce4-settings-$PV" xfce4-settings )
fi

cd "$SRC"
cp "$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)" \
   "$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)" . 2>/dev/null || true
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfce4-settings =="
# Disable everything optional (upower/colord/libnotify/canberra/xcursor-extras) so xfsettingsd builds
# with only the libs we have. Keep xrandr (displays) + libxklavier off if missing.
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  --disable-gobject-introspection --disable-vala \
  --disable-upower-glib --disable-colord --disable-libnotify --disable-libcanberra \
  --disable-xcursor --disable-libxklavier --disable-pluggable-dialogs \
  > /tmp/conf-xfce4-settings.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-xfce4-settings.log; exit 1; fi
echo "== building xfce4-settings (xfsettingsd) =="
# libsetjmp.a (the -wasm-enable-sjlj intrinsics __wasm_setjmp/__wasm_longjmp, pulled in via GTK's
# gdk-pixbuf/libpng/jpeg). It must come AFTER the objects so the undefined refs pull it (with
# --allow-undefined an earlier position would turn them into imports), so pass it through LIBS, which
# automake appends last.
WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
# Transitive GTK libs that non-static pkg-config misses (Requires.private), appended via LIBS (after
# the objects, so the refs resolve): the stub atk-bridge, epoxy, and the X extension libs. Duplicates
# of anything already in GTK_LIBS are harmless.
GTKTRANS="-latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lXcomposite -lX11 -lXau -lXdmcp"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt"
# Build the libs (common/) first, then ONLY xfsettingsd -- skip the GUI dialog binaries we do not need
# (and whose huge transitive link lines hit "argument list too long").
make -j4 -C common LDFLAGS="$LINK" > /tmp/make-xfce4-settings.log 2>&1
make -j4 -C xfsettingsd LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-xfce4-settings.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -45 /tmp/make-xfce4-settings.log; exit 1; fi
echo "OK: xfce4-settings built."
find . -name "xfsettingsd" -not -path "*.deps*" | while read f; do echo "  $(stat -c%s "$f") $f"; done
