#!/usr/bin/env bash
# M8.3: cross-compile the LXDE data layer (libfm-extra -> menu-cache -> libfm-gtk3) to
# wasm32-wasip1-threads, from UNMODIFIED upstream. Dep order is circular-ish: menu-cache needs
# libfm-extra, full libfm needs menu-cache. All wasi gaps fixed in the platform/toolchain layer
# (constraint #5): see toolchain/openbox-compat.c (getpwuid/exec*/ns_get*), compat-include/grp.h,
# wasi-compat.c (weak strsignal), libhostcompat.a.
#
# Host build-tool shims (intltool is unavailable; only needed for translations, which we --disable-nls):
#   toolchain/host-bin/{intltool-update,intltoolize,intltool-merge,intltool-extract} + a stub
#   perl5/XML/Parser.pm. The real gettext tools need libxml2.so.2 on LD_LIBRARY_PATH.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
TP="$EXP/third_party"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
# the host gettext tools (xgettext/msgfmt) need libxml2.so.2; point at the nix copy if present
XML2=$(find /nix/store -maxdepth 2 -name "libxml2.so.2" 2>/dev/null | head -1)
[ -n "$XML2" ] && export LD_LIBRARY_PATH="$(dirname "$XML2"):${LD_LIBRARY_PATH:-}"
newest_config_sub() { echo "$TP/libX11-threads/config.sub"; }
LDADD_HOST="-lhostcompat -Wl,--allow-undefined"

# 0. libhostcompat.a must exist (build-openbox.sh makes it; rebuild here too in case)
"$CC" $CFLAGS -c "$EXP/toolchain/openbox-compat.c" -o "$EXP/toolchain/openbox-compat.o" 2>/dev/null
"$AR" rcs "$PREFIX/lib/libhostcompat.a" "$EXP"/toolchain/threads-libs/{host_socket,host_pipe_dup,override_fcntl}.o \
  "$EXP/toolchain/wasi-compat-threads.o" "$EXP/toolchain/openbox-compat.o"

cfg() { # cfg <dir> <extra configure args...>
  local d="$1"; shift
  cp "$(newest_config_sub)" "$(dirname "$(newest_config_sub)")/config.guess" "$TP/$d/" 2>/dev/null
  ( cd "$TP/$d" && export LDFLAGS="$LDFLAGS $LDADD_HOST" && make distclean >/dev/null 2>&1
    ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode --disable-nls --disable-gtk-doc "$@" \
      > "/tmp/conf-$d.log" 2>&1 && touch aclocal.m4 configure config.h.in Makefile.in */Makefile.in 2>/dev/null )
}

# 1. libfm-extra (the non-GTK utility lib; menu-cache requires it)
if [ ! -f "$PREFIX/lib/libfm-extra.a" ]; then
  cd "$TP"; [ -d libfm ] || { mkdir -p libfm && tar xf libfm.tar -C libfm --strip-components=1; }
  [ -d libfm-threads ] || cp -r libfm libfm-threads
  cfg libfm-threads --with-extra-only
  ( cd "$TP/libfm-threads" && export LDFLAGS="$LDFLAGS $LDADD_HOST" && make -j4 && make install ) >/tmp/make-libfmextra.log 2>&1 \
    && echo "  OK libfm-extra" || { echo "  FAIL libfm-extra"; exit 1; }
fi

# 2. menu-cache (library only; menu-cache-gen needs fork/exec at runtime, not built)
if [ ! -f "$PREFIX/lib/libmenu-cache.a" ]; then
  cd "$TP"; [ -d menu-cache ] || { mkdir -p menu-cache && tar xf menu-cache.tar -C menu-cache --strip-components=1; }
  [ -d menu-cache-threads ] || cp -r menu-cache menu-cache-threads
  cfg menu-cache-threads
  ( cd "$TP/menu-cache-threads" && export LDFLAGS="$LDFLAGS $LDADD_HOST" \
    && make -C libmenu-cache -j4 && make -C libmenu-cache install \
    && cp libmenu-cache.pc "$PREFIX/lib/pkgconfig/" 2>/dev/null ) >/tmp/make-menucache.log 2>&1 \
    && echo "  OK menu-cache (lib)" || { echo "  FAIL menu-cache"; exit 1; }
fi

# 3. full libfm (gtk3). --disable-old-actions skips the Vala 'custom actions' (no valac here).
cd "$TP/libfm-threads"; cfg libfm-threads --with-gtk=3 --disable-old-actions
( cd "$TP/libfm-threads" && export LDFLAGS="$LDFLAGS $LDADD_HOST" && make -j4 && make install ) >/tmp/make-libfm.log 2>&1 \
  && echo "OK: libfm built — $(ls $PREFIX/lib/libfm*.a | xargs -n1 basename | tr '\n' ' ')" \
  || { echo "FAIL libfm"; tail -12 /tmp/make-libfm.log; exit 1; }
