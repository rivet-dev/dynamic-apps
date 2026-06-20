CC = /home/nathan/secure-exec/registry/native/c/vendor/wasi-sdk/bin/clang
CFLAGS = --target=wasm32-wasip1 --sysroot=/home/nathan/secure-exec/registry/native/c/sysroot -O2 -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -D_GNU_SOURCE -mllvm -wasm-enable-sjlj -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-int-conversion -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/compat-include -include /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/wasi-compat.h -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/include/freetype2 -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/include -I/home/nathan/secure-exec/experiments/wasm-gui/third_party/wasm-prefix/include/freetype2   -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/include  -DLOCALEDIR=\"/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/share/locale\"
CPPFLAGS = --target=wasm32-wasip1 --sysroot=/home/nathan/secure-exec/registry/native/c/sysroot -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/include -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/compat-include -include /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/wasi-compat.h -I/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/include/freetype2
LDFLAGS = --target=wasm32-wasip1 --sysroot=/home/nathan/secure-exec/registry/native/c/sysroot -L/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/lib -L/home/nathan/secure-exec/registry/native/c/vendor/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasip1 -lwasi-emulated-mman -lwasi-emulated-process-clocks -lX11 -L/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/lib -lXft   -L/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/lib -lXrender -lX11  -lXmu  
BINDIR = $(DESTDIR)/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/bin

VPATH=.:os

OBJECTS = action.o background.o binding.o border.o button.o client.o \
   clientlist.o clock.o color.o command.o confirm.o cursor.o debug.o \
   default.o desktop.o dock.o event.o error.o font.o grab.o gradient.o \
   group.o help.o hint.o icon.o image.o lex.o main.o match.o menu.o misc.o \
   move.o outline.o pager.o parse.o place.o popup.o render.o resize.o \
   root.o screen.o settings.o spacer.o status.o swallow.o taskbar.o \
   timing.o tray.o traybutton.o winmenu.o

EXE = jwm

.SUFFIXES: .o .h .c

all: $(EXE)

install: all
	install -d $(BINDIR)
	install $(EXE) $(BINDIR)/$(EXE)

install-strip: install
	strip $(BINDIR)/$(EXE)

uninstall:
	rm -f $(BINDIR)/$(EXE)

$(EXE): $(OBJECTS)
	$(CC) -o $(EXE) $(OBJECTS) $(LDFLAGS)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $<

$(OBJECTS): *.h ../config.h

clean:
	rm -f $(OBJECTS) $(EXE) core

