#!/usr/bin/env bash
# XU3: cross-compile UNMODIFIED xfce4-whiskermenu-plugin 2.8.3 (the Xubuntu-DEFAULT app menu) to
# wasm32-wasip1-threads. whiskermenu is CMake/C++; rather than author a CMake cross-toolchain, we
# compile its .cpp set directly with clang++ (reusing the established cross flags) -- the same approach
# as build-gtk-app.sh, extended to C++. Produces libwhiskermenu.a (24 objects). The static-plugin
# integration (gmodule shim entry-rename + link into the panel) is done by build-xfce4-panel.sh.
# Constraint #5: upstream untouched; the config defines + include paths come from the build env, not
# source edits. whiskermenu loads garcon LAZILY (on click) like applicationsmenu, so it AVOIDS the
# binaryen --fpcast-emu file-view gate (no eager g_file_query_info at startup).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
SRC="$EXP/third_party/whiskermenu"
[ -d "$SRC/panel-plugin" ] || { echo "FATAL: whiskermenu source not at $SRC (fetch xfce4-whiskermenu-plugin-2.8.3.tar.bz2)"; exit 1; }

# Install the panel SDK + its .pc if missing (whiskermenu deps).
[ -f "$PREFIX/lib/libxfce4panel-2.0.a" ] || cp -f "$EXP/third_party/xfce4-panel/libxfce4panel/.libs/libxfce4panel-2.0.a" "$PREFIX/lib/" 2>/dev/null || true
[ -f "$PREFIX/lib/pkgconfig/libxfce4panel-2.0.pc" ] || cp -f "$EXP/third_party/xfce4-panel/libxfce4panel/libxfce4panel-2.0.pc" "$PREFIX/lib/pkgconfig/" 2>/dev/null || true

# config.h-equivalent defines (whiskermenu's CMake target_compile_definitions, paths mapped to the VM tree).
CFGH="$EXP/toolchain/whiskermenu-config.h"
cat > "$CFGH" <<'H'
#define GETTEXT_PACKAGE "xfce4-whiskermenu-plugin"
#define PACKAGE_LOCALE_DIR "/usr/share/locale"
#define PACKAGE_NAME "xfce4-whiskermenu-plugin"
#define PACKAGE_VERSION "2.8.3"
#define BINDIR "/usr/bin"
#define DATADIR "/usr/share"
#define SETTINGS_MENUFILE "/etc/xdg/menus/xfce-settings-manager.menu"
#define G_LOG_DOMAIN "whiskermenu"
#define GSEAL_ENABLE 1
#define GTK_MULTIHEAD_SAFE 1
#define G_DISABLE_ASSERT 1
#define G_DISABLE_CAST_CHECKS 1
H

# Include dirs: pkg-config for the deps + xfconf (under xfce4/) + the libxfce4panel headers (the panel
# build did not `make install` its headers, so point at the source tree).
CF="$(pkg-config --cflags gtk+-3.0 garcon-1 garcon-gtk3-1 libxfce4panel-2.0 libxfce4ui-2 exo-2 2>/dev/null)"
CF="$CF -I$PREFIX/include/xfce4/xfconf-0 -I$EXP/third_party/xfce4-panel"
DEFS="-D_WASI_EMULATED_SIGNAL -fno-exceptions -std=c++17 -include $CFGH"

OBJDIR="$SRC/wasm-obj"; mkdir -p "$OBJDIR"; rm -f "$OBJDIR"/*.o
n=0; fails=0
for f in "$SRC"/panel-plugin/*.cpp; do
  o="$OBJDIR/$(basename "$f" .cpp).o"
  if "$WSDK/bin/clang++" $CFLAGS $DEFS $CF -c "$f" -o "$o" 2>"/tmp/wm-$(basename "$f").err"; then
    n=$((n+1))
  else
    fails=$((fails+1)); echo "FAIL $(basename "$f"):"; grep -aiE "error|not found" "/tmp/wm-$(basename "$f").err" | head -2
  fi
done
echo "whiskermenu: compiled $n .cpp (fails=$fails)"
[ $fails -eq 0 ] || { echo "COMPILE FAILED"; exit 1; }

# Archive into libwhiskermenu.a (the panel build force-links the entry + the gmodule shim resolves it).
"$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwhiskermenu.a" "$OBJDIR"/*.o
echo "built libwhiskermenu.a ($(stat -c%s "$PREFIX/lib/libwhiskermenu.a") bytes, $n objects)"
echo "entry function (for the gmodule static-plugin shim):"
"$WSDK/bin/llvm-nm" "$OBJDIR/plugin.o" 2>/dev/null | grep -iE "xfce_panel_module|construct|register" | head
