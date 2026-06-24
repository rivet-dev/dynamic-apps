#!/usr/bin/env bash
# XU0: cross-compile UNMODIFIED dbus 1.14.x (dbus-daemon + dbus-send/dbus-monitor) to
# wasm32-wasip1-threads for the Xubuntu D-Bus session bus. Constraint #5: the daemon builds from
# upstream source; wasi gaps are fixed in the platform layer (libhostcompat host_net sockets/pipe +
# wasi-compat stubs), never by patching dbus.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
SRC="$EXP/third_party/dbus-threads"
[ -d "$SRC" ] || { echo "MISSING $SRC — fetch dbus first"; exit 1; }
[ -f "$PREFIX/lib/libexpat.a" ] || { echo "MISSING libexpat.a (build expat first)"; exit 1; }

# Ensure libhostcompat.a exists (host_net sockets/pipe + compat stubs) — built by build-libfm.sh.
[ -f "$PREFIX/lib/libhostcompat.a" ] || bash "$EXP/scripts/build-libfm.sh" >/dev/null 2>&1 || true

cd "$SRC"
cp "$(ls "$EXP"/third_party/libX11-threads/config.sub 2>/dev/null | head -1)" \
   "$(ls "$EXP"/third_party/libX11-threads/config.guess 2>/dev/null | head -1)" . 2>/dev/null || true
make distclean >/dev/null 2>&1

# Point at our expat + link the platform host shims. Disable everything that needs a real OS:
# systemd/selinux/apparmor/audit (no init/LSM), x11-autolaunch (daemon needs no X), launchd/kqueue
# (not Linux), epoll (wasi has none -> generic poll() fallback, served by host_net poll).
export CFLAGS="$CFLAGS -I$PREFIX/include"
# IMPORTANT: configure WITHOUT --allow-undefined so its link-based feature tests are ACCURATE
# (with --allow-undefined every test passes -> false positives like Solaris getpeerucred -> <ucred.h>).
# -lhostcompat lets the real host_net socket funcs resolve so socket features detect correctly; genuinely
# missing functions then fail the test as they should. --allow-undefined is added only at the final link.
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat -lexpat"
export EXPAT_CFLAGS="-I$PREFIX/include"
export EXPAT_LIBS="-L$PREFIX/lib -lexpat"
LINK_LDFLAGS="$LDFLAGS -Wl,--allow-undefined -Wl,--wrap=writev"

echo "== configuring dbus =="
# Force-detect functions that ARE provided by libhostcompat/wasi-compat but whose autotools link-test
# fails here: those objects reference host-import / --wrap symbols (net_*, __real_mmap) that are only
# resolved at the final --allow-undefined link, so a bare configure link-test sees them undefined and
# concludes the function is missing. Without these, dbus compiles out its rlimit path and fatals at
# startup with "cannot change fd limit". (constraint #5: platform truth asserted in the build, dbus
# source untouched.)
export ac_cv_func_getrlimit=yes ac_cv_func_setrlimit=yes ac_cv_func_socketpair=yes
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-systemd --disable-selinux --disable-apparmor --disable-libaudit \
  --disable-x11-autolaunch --without-x \
  --disable-tests --disable-modular-tests --disable-asserts \
  --disable-doxygen-docs --disable-xml-docs --disable-ducktype-docs --disable-qt-help \
  --disable-launchd --disable-kqueue \
  --with-xml=expat --with-system-socket=/tmp/dbus-session \
  --disable-Werror \
  > /tmp/conf-dbus.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -30 /tmp/conf-dbus.log; exit 1; fi
echo "  configured. epoll=$(grep -c 'DBUS_HAVE_LINUX_EPOLL 1' config.h 2>/dev/null) (want 0)  expat=$(grep -c 'DBUS_USE_EXPAT 1' config.h 2>/dev/null)"

echo "  HAVE_GETPEERUCRED=$(grep -c 'define HAVE_GETPEERUCRED 1' config.h) (want 0)  SO_PEERCRED-path uses sys/socket struct ucred"
echo "== building dbus =="
make LDFLAGS="$LINK_LDFLAGS" -j4 > /tmp/make-dbus.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -40 /tmp/make-dbus.log; exit 1; fi
echo "OK: dbus built. daemon=$(ls -la bus/dbus-daemon 2>/dev/null | awk '{print $5}') send=$(ls bus/.libs/dbus-daemon 2>/dev/null)"
ls -la bus/dbus-daemon tools/dbus-send tools/dbus-monitor 2>/dev/null
