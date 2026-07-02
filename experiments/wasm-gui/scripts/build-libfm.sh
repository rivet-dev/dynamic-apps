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
newest_config_sub() {
  find "$TP" -name config.sub -type f | while read -r f; do
    "$f" wasm32-wasi >/dev/null 2>&1 && { echo "$f"; exit 0; }
  done
}
LDADD_HOST="-lhostcompat -Wl,--allow-undefined"

# 0. libhostcompat.a must exist (build-openbox.sh makes it; rebuild here too in case). Regenerate the
# gitignored compat/override objects from source (the shared-workspace cleaner deletes them; a missing
# member makes `ar` abort and leave a stale archive). override_fcntl/ioctl are the platform libc shims
# that let libxcb/libX11 use STOCK upstream fcntl/ioctl on host_net sockets (constraint #5).
"$CC" $CFLAGS -c "$EXP/toolchain/openbox-compat.c" -o "$EXP/toolchain/openbox-compat.o" 2>/dev/null
_COMPAT_CC=("$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 \
  -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -DSECURE_EXEC_WASM_THREADS -pthread)
mkdir -p "$EXP/toolchain/threads-libs" "$PREFIX/lib"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$EXP/toolchain/wasi-compat.c" -o "$EXP/toolchain/wasi-compat-threads.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$EXP/toolchain/threads-libs/host_socket.c" -o "$EXP/toolchain/threads-libs/host_socket.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$EXP/toolchain/threads-libs/host_pipe_dup.c" -o "$EXP/toolchain/threads-libs/host_pipe_dup.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$REPO/registry/native/patches/wasi-libc-overrides/fcntl.c" -o "$EXP/toolchain/threads-libs/override_fcntl.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$REPO/registry/native/patches/wasi-libc-overrides/ioctl.c" -o "$EXP/toolchain/threads-libs/override_ioctl.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$REPO/registry/native/patches/wasi-libc-overrides/writev_hostnet.c" -o "$EXP/toolchain/threads-libs/override_writev.o"
"$AR" rcs "$PREFIX/lib/libhostcompat.a" "$EXP"/toolchain/threads-libs/{host_socket,host_pipe_dup,override_fcntl,override_ioctl,override_writev}.o \
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
  # fm-utils.c: fm_run_in_default_main_context() on the GLIB>=2.32 path unconditionally does
  # g_main_context_invoke(NULL,...) + g_cond_wait(), which DEADLOCKS when called from the main thread
  # before any GMainContext is iterated (e.g. during pcmanfm widget construction, before gtk_main).
  # Restore the inline-run guard the older-GLib path already has: if the caller owns/can acquire the
  # default context, run the function inline instead of posting+waiting.
  python3 - "$TP/libfm-threads/src/base/fm-utils.c" <<'PY'
import sys
f = sys.argv[1]; s = open(f).read()
old = ("    g_main_context_invoke(NULL, _fm_run_in_default_main_context_real, &md);\n"
       "    g_mutex_lock(&main_loop_run_mutex);\n"
       "    while(!md.done)\n"
       "        g_cond_wait(&main_loop_run_cond, &main_loop_run_mutex);\n"
       "    g_mutex_unlock(&main_loop_run_mutex);")
new = ("    {\n"
       "        gboolean is_owner = g_main_context_is_owner(g_main_context_default());\n"
       "        gboolean acquired = !is_owner && g_main_context_acquire(g_main_context_default());\n"
       "        if(is_owner || acquired)\n"
       "        {\n"
       "            md.result = func(data);\n"
       "            if(acquired) g_main_context_release(g_main_context_default());\n"
       "            return md.result;\n"
       "        }\n"
       "    }\n"
       "    g_main_context_invoke(NULL, _fm_run_in_default_main_context_real, &md);\n"
       "    g_mutex_lock(&main_loop_run_mutex);\n"
       "    while(!md.done)\n"
       "        g_cond_wait(&main_loop_run_cond, &main_loop_run_mutex);\n"
       "    g_mutex_unlock(&main_loop_run_mutex);")
if old in s and "g_main_context_is_owner" not in s:
    open(f, "w").write(s.replace(old, new, 1)); print("patched fm-utils.c (inline-run guard)")
PY
  # fm-job.c: post the job-cleanup idle at G_PRIORITY_DEFAULT (not the default G_PRIORITY_DEFAULT_IDLE).
  # The GTK/GDK X11 event source stays perpetually ready in the headless wasm X setup (stray core focus
  # events it can't fully drain), which would STARVE the lower-priority idle so async job completion
  # (a folder's finish-loading -> the file view populating) never dispatches.
  python3 - "$TP/libfm-threads/src/job/fm-job.c" <<'PY'
import sys
f = sys.argv[1]; s = open(f).read()
old = "        idle_handler = g_idle_add(on_idle_cleanup, NULL);"
new = "        idle_handler = g_idle_add_full(G_PRIORITY_DEFAULT, on_idle_cleanup, NULL, NULL);"
if old in s:
    open(f, "w").write(s.replace(old, new, 1)); print("patched fm-job.c (idle cleanup priority)")
PY
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
