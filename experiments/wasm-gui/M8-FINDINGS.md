# M8 (full GTK desktop stack to wasm32-wasip1) — spike findings

M8 asks for the GTK stack (GLib/GObject/Pango/Cairo/GdkPixbuf/harfbuzz/ATK/GTK) cross-compiled to
`wasm32-wasip1`, all running as wasm in secure-exec. This documents a real spike (not an estimate).

## Spike: cross-compile GLib (the stack's foundation)

GLib is the base of the entire stack (GObject → Pango/ATK/GdkPixbuf/GTK all depend on it). Using the
same toolchain that built the X stack (`toolchain/cross-env.sh`, wasi-sdk clang, meson cross file),
`scripts/spike-m8-glib.sh` runs:

    meson setup build-wasm --cross-file $CROSS_INI -Dtests=false -Dnls=disabled ... -Ddefault_library=static

Result: meson proceeds — it fetches the **libffi** subproject and runs the wasm32-wasi compiler checks
(all pass: sizeof types, visibility, headers) — then **fails**:

    subprojects/libffi/meson.build:259: ERROR: Unsupported pair: system "wasi", cpu family "wasm32"

## The foundational blocker: libffi has no wasm/wasi port

GObject (part of GLib) requires **libffi** for closure marshalling (`g_cclosure_marshal_generic`,
signal/closure invocation, GObject introspection). libffi implements `ffi_call`/closures with
**per-architecture assembly trampolines** — its meson build has branches for x86/x86_64/arm/aarch64/
riscv/etc., EACH supplying an arch-specific `sysv.S`. **There is no wasm branch**, because wasm has no
inline assembly and no way to synthesize a callable function pointer at runtime from a signature (the
core thing libffi/closures need). So `TARGET` stays empty and the build errors out.

This is not a "patch a few stubs" issue like the X stack's `wasi-compat` gaps. It is fundamental:

- **Every GObject-based library needs libffi** → all of GTK, Pango, ATK, GdkPixbuf, GLib's GObject.
- **wasm cannot do dynamic FFI trampolines** the way native arches do. The only working wasm libffi is
  Emscripten's, which uses a **JavaScript-side trampoline shim** (and dynamic `addFunction`/table
  growth) — it requires the Emscripten JS runtime and is NOT a pure-wasi solution, so it does not apply
  to secure-exec's `wasm32-wasip1` guests.

## Implication for M8

M8 is blocked at its foundation. Making the GTK stack run on wasi requires FIRST solving wasm FFI for
wasi-without-Emscripten, by one of:
1. Porting libffi to wasm32-wasi with a trampoline mechanism (e.g. a fixed dispatch table + the wasm
   `funcref`/`call_indirect` + a generated set of signature shims, or a host-side trampoline import).
   This is a substantial, novel piece of systems work.
2. Building GObject WITHOUT libffi (replace the generic closure marshaller). This is invasive surgery
   on GObject internals and breaks introspection/dynamic signals.
3. A different toolkit that does not need runtime FFI closures (none of the GTK-family does).

After that, the rest of the stack (Pango/Cairo/GdkPixbuf/harfbuzz/GTK) is still a multi-week
cross-compile with its own wasi blockers (threads/GThread, dlopen/GModule, dbus/GIO, fontconfig
already done for M6.2). So M8 = "solve wasm FFI for wasi" (research-level) + a multi-week port. It is
not completable in a session, and the FFI piece is a genuine prerequisite, not just effort.

Reproduce: `scripts/spike-m8-glib.sh` (downloads GLib 2.78.4, runs the meson wasi cross setup).

## BREAKTHROUGH (2026-06-21): the FFI keystone is solvable via a secure-exec-native path

The "no pure-wasi solution" conclusion above is too pessimistic **for secure-exec specifically**. Our
guests do not run on a bare wasm engine — they run inside the **V8 sidecar**, and V8's WebAssembly
reflection CAN call a guest function by its `__indirect_function_table` index with dynamically-typed
args. So `ffi_call` (the half of libffi GObject's `g_cclosure_marshal_generic` uses to invoke handlers)
is implementable as a host import, with NO Emscripten and NO JS runtime leaking into the guest.

**Proven (`scripts/test-m8-ffi-spike.sh`, PASS):** a `host_net.ffi_call(fn_index, ret_kind, nargs,
arg_kinds, arg_vals, ret)` import (crates/execution/src/node_import_cache.rs) does
`instance.exports.__indirect_function_table.get(fn_index)(...marshalled args)` and writes the result
back to guest memory. `guest-xclient/ffi-spike.c` calls three functions PURELY BY POINTER with a
runtime-built arg list (no static call site for the callee signature): `add(7,5)->12` (i32),
`dmul(3,4)->12.0` (f64), and `slen("hello")->5` (pointer arg). All pass. On wasm32 a C function
pointer IS its table index, so `(uintptr_t)&fn` is the index the host calls. Build with
`-Wl,--export-table` and WITHOUT `--fpcast-emu` (fpcast rewrites indirect-call indices). This is safe
per the trust model: `ffi_call` only invokes the guest's OWN functions in its OWN table (a capability
guest code already has via `call_indirect`); it grants no host/cross-VM access.

**ffi_closure ALSO proven (`scripts/test-m8-closure-spike.sh`, PASS).** Closures (runtime callbacks
GObject uses for signal handlers / vfuncs) are demonstrated on PURE wasm via the TRAMPOLINE-POOL
technique — no host import, no V8 engine flag. `guest-xclient/closure-spike.c` pre-generates a pool of
per-slot trampolines (each a distinct wasm function forwarding to a generic dispatcher with its slot's
captured data); `closure_alloc(id)` hands out a free slot's trampoline as the closure's function
pointer. Two distinct closures (ids 100, 1000) each dispatch correctly (107, 1007). This is exactly how
a wasm libffi shim implements `ffi_prep_closure_loc` without runtime code generation, bounded to the
signatures whose trampolines are pre-generated (a full shim generates a pool per signature class).
NOTE: the alternative "one generic closure of ANY signature" needs V8's `WebAssembly.Function` type
reflection (probed: `WebAssembly.Function` is NOT enabled in the runner's V8 — it needs
`--experimental-wasm-type-reflection`, a core v8-runtime engine flag deliberately left untouched to keep
this experiment isolated from the production engine). The trampoline pool needs none of that.

**So BOTH libffi primitives are demonstrated** on wasm32-wasip1 for secure-exec: `ffi_call` (generic,
via the V8-reflection host import) and `ffi_closure` (trampoline pool, pure wasm). The foundational FFI
dead end is gone.

## A real libffi-wasm SHIM exists and works through the libffi ABI (`scripts/test-m8-ffi-abi.sh`, PASS)

`libffi-wasm/` is a libffi-ABI-compatible shim (`include/ffi.h`, `include/ffitarget.h`, `src/ffi.c`)
backed by the two proven primitives: `ffi_call` -> the `host_net.ffi_call` import; closures -> the
trampoline pool. `libffi-wasm/test/ffi-abi-test.c` drives the REAL libffi interface the way GObject's
marshaller does and passes: `ffi_prep_cif` + `ffi_call` for int(7,5)->12, double(3,4)->12.0,
slen("hello")->5, AND `ffi_closure_alloc` + `ffi_prep_closure_loc` + invoking a runtime callback that
captures base=1000 -> callback(20,3)=1023. So the libffi public ABI now functions on wasm32-wasip1.

Scope/limits (honest): FOCUSED subset — scalar + pointer types (GObject's generic marshaller shape);
no struct-by-value, no variadic promotion; closures use the canonical (word,word)->word trampoline
(common signal-handler shape), a fuller shim generates a pool per signature class. Enough to link
GObject's closure marshalling; the type coverage is straightforward to extend.

## GLib + GObject CROSS-COMPILE to wasm32-wasip1 (`scripts/build-glib-stack.sh`)

The libffi-wasm shim was wired into a real GLib build and GLib's foundation now cross-compiles:

* **GLib 2.78.4 configures** for wasm32-wasip1 (meson, `--wrap-mode=nofallback`) — it resolves
  `dependency('libffi')` against the shim and proceeds. Dependencies solved along the way: PCRE2
  (cross-compiled), an `intl` stub (NLS off), `resolv.h` + `res_query` stubs (GIO probe), BSD
  `socket()`/`socketpair()` weak stubs (wasi-libc lacks them), and emulated pthreads
  (`-D_WASI_EMULATED_PTHREAD` + `-lwasi-emulated-pthread`, the THREADS answer — wasi-sdk ships stub
  pthreads, so no `-threads` target switch was needed for the build).
* **`libglib-2.0.a` builds** (~4.5 MB; all 108 TUs).
* **`libgobject-2.0.a` builds** — and it compiles `gclosure.c`/`gmarshal.c`, which `#include <ffi.h>`
  and call `ffi_prep_cif`/`ffi_call`/`ffi_prep_closure_loc`, AGAINST THE libffi-wasm SHIM. This is the
  decisive proof the FFI dead end is gone: the real GObject build links the shim.
* **`libgthread-2.0.a` and `libgmodule-2.0.a` build.**

Toolchain changes that enabled this (in `toolchain/cross-env.sh` -> the generated cross ini): add the
wasm-prefix include/lib to `c_args`/`c_link_args` (so meson's builtin dependency probes see our deps),
`-D_WASI_EMULATED_PTHREAD` + `-lwasi-emulated-pthread`, and `-Wno-error=format-security`. Resolver +
socket stubs are in `toolchain/wasi-compat.c`; `toolchain/compat-include/resolv.h` is new.

## GIO builds — the FULL GLib stack now cross-compiles to wasm32-wasip1

`libgio-2.0.a` (10.7 MB) builds, so **GLib + GObject + GThread + GModule + GIO all cross-compile** for
wasm32-wasip1 (`scripts/build-glib-stack.sh`, reproducible). GIO needed a batch of bounded wasi shims,
all toolchain-level except two small GIO source patches (applied idempotently by the build script):
- compat `<sys/socket.h>` (`#include_next` + `SOCK_SEQPACKET`/`SOCK_RAW`/`SO_BROADCAST`/`SCM_RIGHTS`/
  `PF_UNIX` + a completed `struct cmsghdr` + `CMSG_*` macros),
- compat `<netdb.h>` (`h_errno`, defined in wasi-compat.c), compat `<grp.h>` (no group DB on wasi —
  inline `getgrgid`/`getgrnam` stubs),
- GIO source: `ginetsocketaddress.c` (`sin_zero` guard) and `gunixmounts.c` (`__wasi__` stub branches —
  no mount table / fstab).
GSocket/GResolver/GUnixMount networking is unused at runtime in the sandbox; these are compile/link
shims so the platform links. The whole GLib platform that GTK sits on is now available for wasm.

## GTK rendering stack (in flight) — C++ cross-compilation now works

`scripts/build-gtk-deps.sh` builds the first rendering-stack deps on top of the GLib stack:
**libpng**, **fribidi**, and **harfbuzz** (the C++ text shaper, 47 MB) all cross-compile. harfbuzz
required two toolchain advances now in the cross ini: the wasi-sdk **libc++** headers/libs
(`-isystem .../wasm32-wasip1/c++/v1` + `-lc++ -lc++abi`) so C++ compiles, and **`-mllvm
-wasm-enable-sjlj`** (cpp_args/c_args) for freetype's setjmp. GLib's devel artifacts + host code-gen
tools (glib-mkenums/glib-genmarshal) are installed to the prefix so downstream libs resolve. Also fixed
`freetype2.pc` (it had a stale sibling-workspace prefix). All of `wasm-prefix` (lib/include/share/bin)
is now treated as build output (gitignored, rebuilt by the scripts).

**Cairo and Pango build too** (`scripts/build-gtk-deps.sh`): **libcairo.a** (8 MB, xlib backend) and
**libpango-1.0.a / libpangocairo-1.0.a / libpangoft2-1.0.a** cross-compile. Cairo needed compat
`<sys/ipc.h>`+`<sys/shm.h>` + shm stubs (its Xlib XShm path; falls back to XPutImage at runtime). Pango
needed `flockfile`/`funlockfile` declared (wasi-compat.h) and two pkg-config fixes baked into the script:
`fontconfig.pc` must pull `-lexpat` (fcxml.o needs `XML_*`), and the header-only X protocol packages
(xproto/kbproto/xextproto/renderproto/inputproto/fixesproto/fontsproto/recordproto) needed synthesized
`.pc` stubs (x11.pc requires them but they were never installed).

## GTK 3.24 ITSELF cross-compiles to wasm32-wasip1 (`scripts/build-gtk3.sh`)

`libgtk-3.a` (41 MB, 506/507 TUs) builds — the complete GTK toolkit + its entire dependency tree are
now cross-compiled for wasm. Beyond the rendering core, this needed: **gdk-pixbuf** (png) + **atk** +
a **stub at-spi2 atk-bridge** (no AT-SPI/D-Bus in the sandbox); the X libs GTK uses that the X stack
lacked (**Xrandr/Xcursor/Xcomposite/Xdamage** + randr/composite/damage proto .pc stubs); and HOST
code-gen tools — glib-compile-resources/schemas from the host (native), and crucially a **gdbus-codegen
wrapper bound to OUR GLib 2.78.4 codegen module** (the host's newer gdbus-codegen emits
`g_variant_builder_init_static`, a GLib 2.84 API our target lacks). GTK pulls libepoxy as a meson
subproject; wayland/introspection/demos/tests/colord disabled. KEY proto-stub gotcha: pkg-config
requires a `Description:` field or the .pc is silently "not found".

## A GTK 3 app LINKS and RUNS into the GDK X11 backend on the wasm X server

`guest-xclient/gtk-hello.c` (a GtkWindow + box + label + button) links against the full GTK stack into a
single wasm guest (`scripts/build-gtk-app.sh`): 52 MB linked, size-optimized to ~15 MB (to fit the
runtime's 64 MiB transfer frame), 0 unresolved env imports. Running it via `host --xdemo` on the wasm X
server, GTK **instantiates and executes**: GLib + GObject init, then the **GDK X11 backend connects to
the wasm X server and sets up the display, screens, devices and seats** — i.e. the GTK runtime works on
our wasm X server. Getting here required:
- Runner: a no-op fallback for `path_filestat_set_times`/`fd_filestat_set_times` (node:wasi omits them;
  GTK/GLib import them) in node_import_cache.rs (ASSET bumped).
- wasi-compat stubs for the last undefined GTK symbols: chown, raise, recvmsg/sendmsg, getservbyname_r,
  pthread_attr_setinheritsched, and epoxy GLX entrypoints (our epoxy is GLX-less); link the emulated
  mman/process-clocks/pthread libs so mmap/clock/pthread_* resolve.
- Link/opt: `--allow-undefined --no-check-features --max-memory=128MiB`; `wasm-opt --fpcast-emu
  -pa max-func-params@128` then `-Oz` (NOT `--enable-threads`, which makes memory shared+fixed -> the
  heap can't grow -> instant OOM). The runtime caps wasm memory at 128 MiB.

## The M8 runtime frontier: GTK needs THREADS (the fundamental blocker)

After fixing GWakeup (build-glib-stack.sh now patches gwakeup.c to use inert -1 fds on wasi, so the
single-threaded main loop no longer aborts on the missing eventfd/pipe), the GTK app runs further into
GLib init and hits the real wall: **GLib spawns its worker thread** (`g_system_thread_new` ->
`pthread_create`), which fails on wasm32-wasip1 ("error 'Argument list too long' during 'pthread_create'"
— the emulated-pthread stubs can't actually create a thread), and GLib `g_error`s -> abort. GLib's
runtime fundamentally assumes a working thread (the GLib worker context used by GIO/GDBus/file-monitor),
and GTK triggers it during startup.

This is the genuine M8 runtime frontier and it is NOT a quick patch: the GTK *build* is done, but the
GTK *runtime* needs real threads. That means (a) rebuilding the GTK stack for the **wasm32-wasip1-threads**
target (shared memory + atomics), and (b) the secure-exec runtime supporting **`wasi_thread_spawn`** +
shared-memory guests so `pthread_create` actually spawns a wasm thread. Both are substantial core efforts
(the SPEC explicitly defers threads: "added only where a milestone needs them" — M8 needs them).

## Threads runtime: exact gap reproduced + implementation plan (work authorized, multi-week)

`guest-xclient/threads-test.c` (a minimal `pthread_create`+`pthread_join`) built for the
**wasm32-wasip1-threads** target (`(memory shared)`) reproduces the precise gap: it imports
`(import "wasi" "thread-spawn" (func (param i32) (result i32)))`, which the runtime does not provide ->
`WebAssembly.Instance(): Import #6 "wasi": module is not an object or function`. (wasi-libc's
pthread_create calls this import; the host must run the module's exported `wasi_thread_start(tid,
start_arg)` on a new thread sharing the linear memory.)

Implementing it is a core `crates/v8-runtime` change (the JS runner can't — the embedding has no Web
Workers; threads must come from Rust spawning isolates):
1. Create the guest `WebAssembly.Memory` as **shared** (the module declares it; expose its
   SharedArrayBuffer backing store to Rust so it can be shared across isolates).
2. Provide a `"wasi"` import object with `"thread-spawn"`: on call, spawn an OS thread, create/enter a
   V8 isolate, instantiate the SAME module with the SAME shared memory + the wasi/host_net imports, and
   invoke the `wasi_thread_start` export. Manage tids, lifecycle, teardown, and the shared
   `__stack_pointer`/TLS per thread (wasi-libc thread bootstrap).
3. Rebuild the whole GTK stack (GLib...GTK + the X libs GTK links) for `-threads`.
4. SECURITY REVIEW: the executor threat model now spans concurrent shared-memory threads (atomics,
   data races, the sidecar<->executor boundary under concurrency). The sidecar is the TCB; this needs
   deliberate design review, not an autonomous change.

This is the active, user-authorized direction. The gap is reproduced and scoped; the implementation is a
substantial multi-week core-runtime project (and a notable change to the sandbox execution model).

## Remaining for M8 (still multi-week, NOT complete)

1. **Threads runtime** (above): the v8-runtime `wasi/thread-spawn` + shared-memory implementation +
   `-threads` stack rebuild, so GLib's worker thread (and GTK) run.
2. Then finish GDK input-device/seat setup (minimal XInput2 enumeration -> GDK_IS_DEVICE criticals) and
   tune the V8 CPU-time budget for the long-running GTK main loop, to get a rendered interactive window.
3. Then **LXDE** (openbox/lxpanel/pcmanfm), then the DE shell running live + interactive with an
   automated test + manual example.
2. Then **Pango / Cairo / GdkPixbuf / harfbuzz / GTK** cross-compile (each with its own wasi gaps),
   then **LXDE** (openbox/lxpanel/pcmanfm), then the DE shell running live + interactive on the wasm X
   server with an automated test. Multi-week.

So M8's foundational FFI blocker is fully resolved (GObject builds against the libffi-wasm shim), and
GLib's core four libraries cross-compile to wasm32-wasip1. M8 as a whole — GIO finished, the GTK stack,
the DE, the live interactive shell — is still a multi-week port and is NOT complete.
