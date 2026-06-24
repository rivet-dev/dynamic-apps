/*
 * GLib version-skew shim for the Xfce builds. The HOST gdbus-codegen comes from a newer GLib (>= 2.84)
 * and emits calls to g_variant_builder_init_static() in the generated D-Bus bindings, but the wasm
 * target links the vendored GLib 2.78 (which has only g_variant_builder_init()). Forward the new
 * symbol to the old one -- they are ABI-identical (init a builder for a long-lived/static type).
 *
 * This is a TOOLCHAIN shim for a host-tool/target-lib version mismatch, not a patch to any component
 * (constraint #5). Linked into the Xfce binaries alongside -lhostcompat. Opaque pointers avoid needing
 * the GLib headers here.
 */

extern void g_variant_builder_init(void *builder, const void *type);

void g_variant_builder_init_static(void *builder, const void *type) {
    g_variant_builder_init(builder, type);
}

/* BSD <err.h> family + daemon(): wasi-libc lacks these and xfconf (and other Xfce/D-Bus tools) use
 * them. fork()/sigaction() are already provided by wasi-compat.c (libhostcompat). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void glibcompat_vwarn(const char *fmt, va_list ap) {
    if (fmt)
        vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}
void warnx(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); glibcompat_vwarn(fmt, ap); va_end(ap);
}
void warn(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); glibcompat_vwarn(fmt, ap); va_end(ap);
}
void errx(int eval, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); glibcompat_vwarn(fmt, ap); va_end(ap); exit(eval);
}
void err(int eval, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); glibcompat_vwarn(fmt, ap); va_end(ap); exit(eval);
}
/* xfconfd daemonizes; with --no-daemon this is never called, but the symbol must resolve. There is no
 * fork() on wasi, so report "stayed in foreground" success (0). */
int daemon(int nochdir, int noclose) { (void) nochdir; (void) noclose; return 0; }
