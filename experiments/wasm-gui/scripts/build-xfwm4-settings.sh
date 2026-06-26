#!/usr/bin/env bash
# XU6: build the UNMODIFIED xfwm4 Window-Manager settings dialog (xfwm4-settings) to wasm -- a real Settings
# Manager entry, shipped by xfwm4 (already configured for XU2). Clone of the dialog recipe + type-ensure shim
# + gtk_init_with_args wrap (its glade root is XfceTitledDialog). Arg $1 selects the program: settings (default),
# tweaks, or workspace. Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfwm4"
[ -d "$SRC/settings-dialogs" ] || { echo "FATAL: xfwm4 not configured"; exit 1; }
WHICH="${1:-settings}"   # settings | tweaks | workspace
case "$WHICH" in
  settings)  PROG=xfwm4_settings;            BINNAME=xfwm4-settings ;;
  tweaks)    PROG=xfwm4_tweaks_settings;     BINNAME=xfwm4-tweaks-settings ;;
  workspace) PROG=xfwm4_workspace_settings;  BINNAME=xfwm4-workspace-settings ;;
  *) echo "unknown program $WHICH"; exit 1 ;;
esac
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"

cd "$SRC"
rm -f "settings-dialogs/$BINNAME" settings-dialogs/${PROG}-*.o
echo "== compile common/ + xfwm4 settings-dialogs ($WHICH) =="
make -k -j4 -C common LDFLAGS="$LINK" >> /tmp/make-xfwm4set.log 2>&1 || true
make -k -j4 -C settings-dialogs LDFLAGS="$LINK" >> /tmp/make-xfwm4set.log 2>&1 || true
OBJS="$(ls "$SRC"/settings-dialogs/${PROG}-*.o "$SRC"/common/libxfwm4util_la-*.o "$SRC"/common/*xfwm*common*-*.o 2>/dev/null)"
echo "$WHICH objects: $(echo $OBJS | wc -w)"
[ -n "$OBJS" ] || { echo "no objects built"; tail -25 /tmp/make-xfwm4set.log; exit 1; }

RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
TYPESHIM="$EXP/toolchain/xfce-type-ensure.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/xfce-type-ensure.c" -o "$TYPESHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"; "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,--wrap=gtk_init_with_args"
# workspace needs libwnck; all need gtk/libxfce4ui/xfconf/util. dbus-glib may be needed; --allow-undefined covers absence.
EXTRA=""; [ "$WHICH" = "workspace" ] && EXTRA="libwnck-3.0"
# libxfce4kbd-private-3 provides xfce_shortcuts_provider_* (the WM dialog's keybindings tab); without it the
# link leaves an undefined env import -> LinkError at instantiate.
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfconf-0 libxfce4util-1.0 libxfce4kbd-private-3 $EXTRA 2>/dev/null)
BIN="$SRC/settings-dialogs/$BINNAME"
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat $OBJS "$TYPESHIM" -o "$BIN" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-xfwm4set.log
RC=$?; echo "$WHICH link rc=$RC"
if [ ! -f "$BIN" ]; then echo "no binary"; grep -aiE "error|undefined symbol" /tmp/make-xfwm4set.log | head -12; exit 1; fi
echo "binary $(stat -c%s "$BIN") bytes"

echo "== fpcast-emu + -Oz -> $BINNAME.wasm =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/$BINNAME.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$BIN" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built $BINNAME.wasm ($(stat -c%s "$OUT") bytes)"
