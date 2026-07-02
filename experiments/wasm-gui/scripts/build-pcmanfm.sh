#!/usr/bin/env bash
# M8.5: cross-compile pcmanfm (the LXDE file manager) to wasm32-wasip1-threads from UNMODIFIED
# upstream. pcmanfm is a thin GTK3 shell over M8.3's libfm-gtk3 — no new dep libraries beyond the
# GTK/X/glib + libfm stack already built by build-libfm.sh/build-lxpanel.sh. All wasi gaps are in the
# platform/toolchain layer (constraint #5): the 3-arg main shim, tmpfile()/exec*/getpw* in
# openbox-compat.c, etc. Mirrors build-lxpanel.sh's final link stage.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
TP="$EXP/third_party"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
XML2=$(find /nix/store -maxdepth 3 -name "libxml2.so.2" 2>/dev/null | head -1)
[ -n "$XML2" ] && export LD_LIBRARY_PATH="$(dirname "$XML2"):${LD_LIBRARY_PATH:-}"
WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"; LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
# Bump the wasm main stack to 8MB: pcmanfm's GtkUIManager/menu build + the recursive gtk_widget_show_all
# over the deep main-window tree (toolbar/notebook/treeview/statusbar) overflow the small default stack,
# which surfaces as "memory access out of bounds" inside the show_all vfunc recursion.
CLEAN_LDFLAGS="$LDFLAGS -lhostcompat -Wl,--allow-undefined -Wl,-z,stack-size=8388608 -Wl,--wrap=writev"
newest_config_sub() {
  find "$TP" -name config.sub -type f | while read -r f; do
    "$f" wasm32-wasi >/dev/null 2>&1 && { echo "$f"; exit 0; }
  done
}
cs() {
  local sub
  sub="$(newest_config_sub)"
  [ -n "$sub" ] && cp "$sub" "$(dirname "$sub")/config.guess" "$TP/$1/" 2>/dev/null || true
}
stat_bytes() {
  stat -c%s "$1" 2>/dev/null || stat -f%z "$1"
}

# libfm-gtk3 + libhostcompat must already exist (build-libfm.sh).
[ -f "$PREFIX/lib/libfm-gtk3.a" ] || bash "$EXP/scripts/build-libfm.sh"
# NOTE: pcmanfm's main is the standard 2-arg main(argc,argv) (src/pcmanfm.c:199), so it connects to
# the wasi crt's __main_void natively — do NOT link the 3-arg main shim (lxpanel needs it; pcmanfm
# does not, and forcing it makes `main` undefined-weak -> the whole module DCEs to a stub).

# pcmanfm (matches libfm 1.3.2).
cd "$TP"; [ -d pcmanfm-threads ] || { curl -fsSL -o pcmanfm.tar "https://downloads.sourceforge.net/pcmanfm/pcmanfm-1.3.2.tar.xz"; mkdir -p pcmanfm-threads && tar xf pcmanfm.tar -C pcmanfm-threads --strip-components=1; }
cs pcmanfm-threads
cd "$TP/pcmanfm-threads"
( export LDFLAGS="$CLEAN_LDFLAGS" && make distclean >/dev/null 2>&1
  ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode --disable-nls --disable-gtk-doc --with-gtk=3 --disable-man --enable-compile-warnings=no >/tmp/conf-pcmanfm.log 2>&1
  touch aclocal.m4 configure config.h.in Makefile.in */Makefile.in 2>/dev/null
  # Same link recipe as lxpanel minus the 3-arg-main shim: gtk's X-ext/epoxy/atk private deps
  # (non-static pkg-config misses Requires.private), libfm-gtk3, and setjmp+libc appended last.
  # gdk-x11 reaches libxcb directly (xcb_connection_has_error, shm/render/etc.); non-static
  # pkg-config doesn't pull these, and --allow-undefined would otherwise leak them as host imports.
  XCBLIBS="-lX11-xcb -lxcb -lxcb-shm -lxcb-render -lxcb-randr -lxcb-shape -lxcb-xfixes -lxcb-present -lxcb-sync -lxcb-damage -lxcb-composite -lxcb-glx -lxcb-dri2"
  XLIBS="-lfm-gtk3 -lfm -lfm-extra -lmenu-cache -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -latk-bridge-2.0 -latk-1.0 -lepoxy -lXft -lXrender -lXext -lX11 $XCBLIBS -lXau -lXdmcp"
  make LDFLAGS="$CLEAN_LDFLAGS" LIBS="${PCMANFM_EXTRA_OBJ:-} $XLIBS $SETJMP $LIBC" -j4 ) >/tmp/make-pcmanfm.log 2>&1 \
  && echo "  OK pcmanfm/pcmanfm ($(stat_bytes src/pcmanfm) bytes)" || { echo "  FAIL pcmanfm"; tail -16 /tmp/make-pcmanfm.log; exit 1; }

# fpcast wide-signature fix + strip DWARF (the linked GTK/X libs carry tens of MB of DWARF that OOMs
# the V8 isolate during compile).
# SECURE_EXEC_KEEP_NAMES=1 keeps the wasm name section (V8 --prof / stack-dump can name guest funcs).
# fpcast-emu erases the C name section UNLESS --debuginfo is on the fpcast pass, so split into two
# passes (fpcast w/ --debuginfo, then -Oz w/ --debuginfo) exactly like build-gtk-app.sh. Bigger binary;
# diagnostics only, same behaviour as the release strip. DWARF is still dropped (only the name section
# is kept) so the isolate doesn't OOM on the tens-of-MB DWARF.
if [ -n "${SECURE_EXEC_KEEP_NAMES:-}" ]; then
  wasm-opt --fpcast-emu --pass-arg=max-func-params@128 --enable-bulk-memory --enable-threads --debuginfo -O0 \
    src/pcmanfm -o "$EXP/pcmanfm.wasm.1" 2>/dev/null
  wasm-opt --strip-dwarf --debuginfo --enable-bulk-memory --enable-threads -Oz \
    "$EXP/pcmanfm.wasm.1" -o "$EXP/pcmanfm.wasm" 2>/dev/null
  rm -f "$EXP/pcmanfm.wasm.1"
  echo "OK: pcmanfm.wasm ($(( $(stat_bytes "$EXP/pcmanfm.wasm")/1024/1024 ))MB, name section KEPT)"
else
  wasm-opt --fpcast-emu --pass-arg=max-func-params@128 --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads -Oz \
    src/pcmanfm -o "$EXP/pcmanfm.wasm" 2>/dev/null
  echo "OK: pcmanfm.wasm ($(( $(stat_bytes "$EXP/pcmanfm.wasm")/1024/1024 ))MB stripped)"
fi
