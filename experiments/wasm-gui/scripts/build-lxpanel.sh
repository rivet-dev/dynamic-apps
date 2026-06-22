#!/usr/bin/env bash
# M8.4: cross-compile lxpanel (the LXDE panel: app menu + taskbar + clock) to wasm32-wasip1-threads
# from UNMODIFIED upstream. Builds the dep chain it needs beyond M8.3's libfm: libXres -> libwnck-3.0
# (taskbar/window-list) and keybinder-3.0 (global hotkeys). All wasi gaps in the platform layer
# (constraint #5): tmpfile() via the VFS in openbox-compat.c, plus everything build-openbox.sh/build-libfm.sh set up.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
TP="$EXP/third_party"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
XML2=$(find /nix/store -maxdepth 2 -name "libxml2.so.2" 2>/dev/null | head -1)
[ -n "$XML2" ] && export LD_LIBRARY_PATH="$(dirname "$XML2"):${LD_LIBRARY_PATH:-}"
WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"; LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
CLEAN_LDFLAGS="$LDFLAGS -lhostcompat -Wl,--allow-undefined"
cs() { cp "$TP/libX11-threads/config.sub" "$TP/libX11-threads/config.guess" "$TP/$1/" 2>/dev/null; }

# libfm + libhostcompat must already exist (build-libfm.sh)
[ -f "$PREFIX/lib/libfm-gtk3.a" ] || bash "$EXP/scripts/build-libfm.sh"

# libXres (libwnck dep)
if [ ! -f "$PREFIX/lib/libXRes.a" ]; then
  cd "$TP"; [ -d libXres-threads ] || { curl -fsSL -o libXres.tar "https://www.x.org/releases/individual/lib/libXres-1.2.2.tar.xz"; mkdir -p libXres-threads && tar xf libXres.tar -C libXres-threads --strip-components=1; }
  cs libXres-threads; ( cd "$TP/libXres-threads" && export LDFLAGS="$CLEAN_LDFLAGS" && ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode >/tmp/c.log 2>&1 && touch aclocal.m4 configure config.h.in Makefile.in && make -j4 && make install ) >/tmp/make-xres.log 2>&1 && echo "  OK libXres"
fi
# libwnck-3.0 (taskbar)
if [ ! -f "$PREFIX/lib/libwnck-3.a" ]; then
  cd "$TP"; [ -d libwnck-threads ] || { curl -fsSL -o libwnck.tar "https://download.gnome.org/sources/libwnck/3.4/libwnck-3.4.9.tar.xz"; mkdir -p libwnck-threads && tar xf libwnck.tar -C libwnck-threads --strip-components=1; }
  cs libwnck-threads; ( cd "$TP/libwnck-threads" && export LDFLAGS="$CLEAN_LDFLAGS" && ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode --disable-nls --disable-gtk-doc --disable-startup-notification --disable-introspection --enable-compile-warnings=no >/tmp/c.log 2>&1 && touch aclocal.m4 configure config.h.in Makefile.in */Makefile.in && make -j4 && make install ) >/tmp/make-wnck.log 2>&1 && echo "  OK libwnck" || { echo "  FAIL libwnck"; exit 1; }
fi
# keybinder-3.0 (hotkeys)
if [ ! -f "$PREFIX/lib/libkeybinder-3.0.a" ]; then
  cd "$TP"; [ -d keybinder-threads ] || { curl -fsSL -o keybinder.tar "https://github.com/kupferlauncher/keybinder/releases/download/keybinder-3.0-v0.3.2/keybinder-3.0-0.3.2.tar.gz"; mkdir -p keybinder-threads && tar xf keybinder.tar -C keybinder-threads --strip-components=1; }
  cs keybinder-threads; ( cd "$TP/keybinder-threads" && export LDFLAGS="$CLEAN_LDFLAGS" && ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode --disable-gtk-doc --disable-introspection >/tmp/c.log 2>&1 && touch aclocal.m4 configure config.h.in Makefile.in */Makefile.in && make -j4 && make install ) >/tmp/make-kb.log 2>&1 && echo "  OK keybinder"
fi

# lxpanel (internal plugins only: menu/launchtaskbar/dclock/pager — dynamic plugins need dlopen)
cd "$TP"; [ -d lxpanel-threads ] || { curl -fsSL -o lxpanel.tar "https://downloads.sourceforge.net/lxde/lxpanel-0.10.1.tar.xz"; mkdir -p lxpanel-threads && tar xf lxpanel.tar -C lxpanel-threads --strip-components=1; }
cs lxpanel-threads
cd "$TP/lxpanel-threads"
( export LDFLAGS="$CLEAN_LDFLAGS" && make distclean >/dev/null 2>&1
  ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode --disable-nls --disable-gtk-doc --enable-gtk3 --disable-man --enable-compile-warnings=no --with-plugins=none >/tmp/conf-lxpanel.log 2>&1
  touch aclocal.m4 configure config.h.in Makefile.in */Makefile.in 2>/dev/null
  # override LDFLAGS at make (configure appends GNU-ld-only hardening flags wasm-ld rejects).
  # main3arg-shim.o overrides the wasi crt's __main_void to call lxpanel's GNU 3-arg
  # main(argc,argv,envp) — without it the 3-arg main's wasm type doesn't match the crt's `main`
  # reference, so main is undefined-weak and calling it traps. LIBS appends setjmp+libc at the end.
  "$CC" $CFLAGS -c "$EXP/toolchain/main3arg-shim.c" -o "$EXP/toolchain/main3arg-shim.o" 2>/dev/null
  make LDFLAGS="$CLEAN_LDFLAGS $EXP/toolchain/main3arg-shim.o" LIBS="$SETJMP $LIBC" -j4 ) >/tmp/make-lxpanel.log 2>&1 \
  && echo "  OK lxpanel/lxpanel ($(stat -c%s src/lxpanel) bytes)" || { echo "  FAIL lxpanel"; tail -10 /tmp/make-lxpanel.log; exit 1; }

# fpcast + STRIP DEBUG (the linked libs carry ~38MB of DWARF: 44MB -> 5MB; the bloat OOMs the V8 isolate).
wasm-opt --fpcast-emu --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads -Oz \
  src/lxpanel -o "$EXP/lxpanel.wasm" 2>/dev/null
echo "OK: lxpanel.wasm ($(( $(stat -c%s "$EXP/lxpanel.wasm")/1024/1024 ))MB stripped)"
