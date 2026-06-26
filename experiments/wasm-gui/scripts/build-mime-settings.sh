#!/usr/bin/env bash
# XU6: build the UNMODIFIED xfce4-mime-settings (the MIME-type / Default-Applications editor) to wasm. Clone of
# the dialog recipe: compile the program objects, link only the xfce4_mime_settings-*.o (NOT the separate
# xfce4_mime_helper program), with the type-ensure shim + gtk_init_with_args wrap (it uses XfceTitledDialog).
# Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfce4-settings"
[ -d "$SRC/dialogs/mime-settings" ] || { echo "FATAL: xfce4-settings not configured (run build-xfce4-settings.sh)"; exit 1; }
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,-z,stack-size=8388608"

cd "$SRC"
rm -f dialogs/mime-settings/xfce4-mime-settings dialogs/mime-settings/*.o
echo "== compile common/ + the mime-settings objects (libtool binary discarded) =="
make -k -j4 -C common LDFLAGS="$LINK" >> /tmp/make-mime.log 2>&1 || true
make -k -j4 -C dialogs/mime-settings LDFLAGS="$LINK" >> /tmp/make-mime.log 2>&1 || true
# link ONLY the settings program's objects (xfce4_mime_settings-*), not the separate xfce4_mime_helper program.
OBJS="$(ls "$SRC"/dialogs/mime-settings/xfce4_mime_settings-*.o "$SRC"/common/libxfce4_settings_la-*.o 2>/dev/null)"
echo "mime-settings objects: $(echo $OBJS | wc -w)"
[ -n "$OBJS" ] || { echo "no objects built"; tail -25 /tmp/make-mime.log; exit 1; }

RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
TYPESHIM="$EXP/toolchain/xfce-type-ensure.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/xfce-type-ensure.c" -o "$TYPESHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"; "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,--wrap=gtk_init_with_args"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfconf-0 exo-2 2>/dev/null)
BIN="$SRC/dialogs/mime-settings/xfce4-mime-settings"
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat $OBJS "$TYPESHIM" -o "$BIN" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-mime.log
RC=$?; echo "mime-settings link rc=$RC"
if [ ! -f "$BIN" ]; then echo "no binary"; grep -aiE "error|undefined symbol" /tmp/make-mime.log | head -12; exit 1; fi
echo "binary $(stat -c%s "$BIN") bytes"

echo "== fpcast-emu + -Oz -> xfce4-mime-settings.wasm =="
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
OUT="$EXP/xfce4-mime-settings.wasm"
wasm-opt --fpcast-emu -pa max-func-params@${SECURE_EXEC_FPCAST_MAXP:-64} --enable-bulk-memory --enable-threads -O0 "$BIN" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"; rm -f "$OUT.1"
echo "built xfce4-mime-settings.wasm ($(stat -c%s "$OUT") bytes)"
