#!/usr/bin/env bash
# XU3: cross-compile UNMODIFIED xfce4-panel 4.18 to wasm32-wasip1-threads. Builds libxfce4panel (the
# plugin SDK lib), common/, and the panel/ binary. Plugins are handled separately (the sandbox has no
# dlopen, so external plugins must be linked statically -- see the plugin build step). Proven Xfce
# autotools + GTK recipe (clone of build-xfwm4.sh). Constraint #5: upstream untouched; wasi/platform gaps
# in the toolchain/runtime layer.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="4.18.6"
SRC="$TP/xfce4-panel"
for l in libxfce4util.a libxfce4ui-2.a libxfconf-0.a libwnck-3.a libgarcon-1.a libgarcon-gtk3-1.a libexo-2.a libhostcompat.a libdbuscreds.a; do
  [ -f "$PREFIX/lib/$l" ] || { echo "FATAL: $l missing"; exit 1; }
done

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/xfce4-panel.tar.bz2" \
    "https://archive.xfce.org/src/xfce/xfce4-panel/4.18/xfce4-panel-$PV.tar.bz2" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf xfce4-panel.tar.bz2 && mv "xfce4-panel-$PV" xfce4-panel )
fi

cd "$SRC"
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"
CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfce4-panel =="
# --datadir=/usr/share: the panel loads data files (panel-default.xml, plugin .desktop) from the
# compile-time DATADIR, not XDG (same as xfwm4). Disable optional deps we don't have.
./configure $CROSS_CONFIGURE_ARGS \
  --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  --disable-introspection --disable-vala --disable-dbusmenu-gtk \
  > /tmp/conf-xfce4-panel.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-xfce4-panel.log; exit 1; fi

WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
# ★ Force-link the libxfce4ui GResource: --undefined creates a ref to libxfce4ui_get_resource, then the
# following -lxfce4ui-2 satisfies it, pulling libxfce4ui-resources.o (whose ctor registers the bundled
# dialog UI). Must precede -lxfce4ui-2 in link order; automake ignores our LDFLAGS override but DOES use
# LIBS (appended last), so we put both here. (libxfce4ui untouched -- pure link ordering.)
GTKTRANS="-Wl,--undefined=libxfce4ui_get_resource -lxfce4ui-2 -lgarcon-gtk3-1 -lgarcon-1 -lexo-2 -lwnck-3 -lXinerama -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
# ★ Force-link the libxfce4ui GResource object. Its register constructor lives in
# libxfce4ui-resources.o inside libxfce4ui-2.a, but archive-pull only includes a .o that satisfies an
# undefined symbol; nothing references it, so it's dropped and the bundled UI (libxfce4ui-dialog-ui.ui)
# is never registered -> the first libxfce4ui dialog hits "resource does not exist" -> Gtk-ERROR abort.
# Referencing libxfce4ui_get_resource pulls the .o so its constructor runs. (libxfce4ui untouched.)
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--undefined=libxfce4ui_get_resource -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"
echo "== building libxfce4panel + common =="
make -j4 -C libxfce4panel CFLAGS="$CFLAGS" LDFLAGS="$LINK" > /tmp/make-xfce4-panel.log 2>&1
make -j4 -C common LDFLAGS="$LINK" >> /tmp/make-xfce4-panel.log 2>&1
echo "== building panel binary =="
make -j4 -C panel LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-xfce4-panel.log 2>&1
RC=$?
echo "panel make rc=$RC"
find . -name "xfce4-panel" -type f -not -path "*.deps*" 2>/dev/null | while read f; do echo "  $(stat -c%s "$f") $f"; done
if [ $RC -ne 0 ]; then echo "(panel link incomplete; tail:)"; tail -30 /tmp/make-xfce4-panel.log; fi
echo "== libxfce4panel.a =="
find . -name "libxfce4panel-2.0.a" -o -name "libxfce4panel*.a" 2>/dev/null | head
