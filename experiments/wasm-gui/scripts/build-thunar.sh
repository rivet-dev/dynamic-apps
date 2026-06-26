#!/usr/bin/env bash
# XU5: cross-compile UNMODIFIED thunar 4.18 to wasm32-wasip1-threads (wallpaper + desktop icons + root
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
SRC="$TP/thunar"
[ -d "$SRC" ] || { echo "FATAL: $SRC not fetched"; exit 1; }
for l in libxfce4util.a libxfce4ui-2.a libxfconf-0.a libwnck-3.a libexo-2.a libhostcompat.a libdbuscreds.a; do
  [ -f "$PREFIX/lib/$l" ] || { echo "FATAL: $l missing"; exit 1; }
done

cd "$SRC"
# Robustness: the shared tree's generated dist files (Makefile.in, config.h.in) are untracked and get
# wiped by concurrent tree churn, breaking ./configure ("cannot find input file: Makefile.in"). Restore
# them from the dist tarball if absent. Only the *.in templates are restored, so local source edits
# (e.g. diagnostic instrumentation in thunar/*.c) are preserved.
# Restore UNCONDITIONALLY (the churn removes an arbitrary SUBSET of the 24 Makefile.in; a top-level-only
# check misses subdir losses, and config.status fails if ANY is absent). Only *.in templates are restored,
# so local source edits (e.g. diagnostic instrumentation in thunar/*.c) are preserved.
if [ -f "$EXP/third_party/thunar.tar.bz2" ]; then
  echo "== restoring Makefile.in/config.h.in from thunar.tar.bz2 (robust against tree churn) =="
  TMP="$(mktemp -d)"; tar xjf "$EXP/third_party/thunar.tar.bz2" -C "$TMP"
  ( cd "$TMP"/thunar-* && find . \( -name 'Makefile.in' -o -name 'config.h.in' \) | tar -cf - -T - ) | tar -xf - -C "$SRC/"
  rm -rf "$TMP"
fi
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"
CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
# Direct clean instead of `make distclean`: the prior build's Makefile has AM_MAINTAINER_MODE on, so
# `make <anything>` first tries to regenerate Makefile.in from Makefile.am via the host automake (wrong
# version) and WIPES the Makefile.in -> configure then fails "cannot find input file: Makefile.in". Remove
# the generated files directly (no `make`), so the restored Makefile.in survive into configure.
find . -name Makefile -type f -delete 2>/dev/null
rm -f config.status config.cache config.log thunar/thunar 2>/dev/null
# Remove the FULL libtool object set (.o AND .lo AND .la AND .libs/). Removing only .o leaves the libtool
# .lo "up-to-date", so make skips recompiling and the .o (in .libs/) is missing at archive time.
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' \) -delete 2>/dev/null
find . -name '.libs' -type d -exec rm -rf {} + 2>/dev/null
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring thunar =="
# --datadir=/usr/share (compile-time data path). Disable gvfs (no remote/trash backend in the sandbox),
# notifications, and the optional bits we don't have.
./configure $CROSS_CONFIGURE_ARGS \
  --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared \
  --disable-gio-unix --disable-notifications \
  ${THUNAR_EXTRA_DISABLE:-} \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-thunar.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-thunar.log; exit 1; fi
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

# FILE-VIEW GATE fix: wrap g_vfs_get_default -> g_vfs_get_local (module-less sandbox; unblocks Thunar's
# per-entry g_file_query_info folder enumeration). Toolchain --wrap shim; see toolchain/gio-vfs-local-shim.c.
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread \
  -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
# EMPTY-PATH fix: reject an empty path -> ENOENT at the libc boundary (POSIX). wasi-libc resolves
# open("")/fopen("") to the cwd DIRECTORY; Thunar's gtk_image_new_from_file("") then has gdk_pixbuf parse a
# dir as an image and HANGS (the ThunarWindow constructor never returns). See toolchain/wasi-empty-path-shim.c.
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread \
  -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
# gmodule static-plugin shim: --wrap=g_module_open returns the MAIN module for thunarx/panel plugin paths so the
# statically-linked plugin init (thunar_extension_initialize) resolves without dlopen. See GMODULE-STATIC-PLUGINS.md.
GMODSHIM="$EXP/toolchain/gmodule-static-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread \
  -c "$EXP/toolchain/gmodule-static-shim.c" -o "$GMODSHIM"
# Keep-shim: a constructor that references thunar_extension_initialize so wasm-ld --gc-sections doesn't strip
# the runtime-only-looked-up plugin entry (the fix --whole-archive/-u/direct-objects couldn't achieve).
KEEPSHIM="$EXP/toolchain/thunar-sbr-keep.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread \
  -c "$EXP/toolchain/thunar-sbr-keep.c" -o "$KEEPSHIM"
SYMWRAP="$EXP/toolchain/thunar-sbr-symwrap.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/thunar-sbr-symwrap.c" -o "$SYMWRAP"
# The thunar-sbr renamer plugin, statically linked (whole-archive keeps all renamer GTypes, not just the init).
SBR_A="$SRC/plugins/thunar-sbr/.libs/libthunar-sbr.a"
GTKTRANS="$RESO $WNCKRESO $VFSSHIM $EMPTYSHIM $GMODSHIM $KEEPSHIM $SYMWRAP -lxfce4ui-2 -lexo-2 -lwnck-3 -lexif -lXinerama -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,--wrap=g_module_open -Wl,--wrap=g_module_symbol -Wl,--export=thunar_extension_initialize -Wl,--export=thunar_extension_list_types -Wl,--export=thunar_extension_shutdown -Wl,-z,stack-size=8388608"
echo "== building thunarx (extension lib) =="
make -j4 -C thunarx LDFLAGS="$LINK" >> /tmp/make-thunar.log 2>&1
# Build the thunar-sbr renamer plugin here (the shared workspace churns away a prior standalone build), so
# SBR_A is always fresh for the static-plugin link below.
echo "== building thunar-sbr (renamer plugin, statically linked via the gmodule shim) =="
make -j4 -C plugins/thunar-sbr LDFLAGS="$LINK" >> /tmp/make-thunar.log 2>&1
[ -f "$SBR_A" ] || echo "WARN: SBR_A still missing after make: $SBR_A"
# Static-plugin link via DIRECT objects (not the .a): directly-listed objects are linked unconditionally where
# archive members are only pulled on reference, and libtool preserves a list of .o better than --whole-archive.
SBR_OBJS="$(ls "$SRC"/plugins/thunar-sbr/thunar_sbr_la-*.o 2>/dev/null | tr '\n' ' ')"
echo "SBR direct objects: $(echo $SBR_OBJS | wc -w)"
echo "== building thunar binary =="
rm -f "$SRC/thunar/thunar"
# Thunar links thunarx + libxfce4kbd-private-3 (keyboard shortcuts) on top of the GTK stack.
make -j4 -C thunar LDFLAGS="$LINK" LIBS="-lthunarx-3 -lxfce4kbd-private-3 -Wl,--whole-archive $SBR_A -Wl,--no-whole-archive $GTKTRANS $SETJMP" >> /tmp/make-thunar.log 2>&1
RC=$?
echo "thunar make rc=$RC"
find . -path "*/thunar/thunar" -type f -not -path "*.deps*" 2>/dev/null | while read f; do echo "  $(stat -c%s "$f") $f"; done
if [ $RC -ne 0 ]; then echo "(link incomplete; tail:)"; tail -30 /tmp/make-thunar.log; fi
# Verify the SBR statically linked (-u thunar_extension_initialize should keep it past GC), then fpcast -> thunar.wasm.
if [ $RC -eq 0 ] && [ -f "$SRC/thunar/thunar" ]; then
  echo "SBR thunar_extension_initialize in binary: $("$WSDK/bin/llvm-nm" "$SRC/thunar/thunar" 2>/dev/null | grep -acwE 'thunar_extension_initialize') (want >=1)"
  echo "== fpcast-emu + -Oz -> thunar.wasm =="
  export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
  wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$SRC/thunar/thunar" -o "$EXP/thunar.wasm.1"
  wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$EXP/thunar.wasm.1" -o "$EXP/thunar.wasm"; rm -f "$EXP/thunar.wasm.1"
  echo "built thunar.wasm ($(stat -c%s "$EXP/thunar.wasm") bytes)"
fi
