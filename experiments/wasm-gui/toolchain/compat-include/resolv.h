/* Minimal <resolv.h> for wasm32-wasip1: wasi-libc omits it, but GLib's GIO build probes res_query().
 * Declares the res_* family (implemented as link-time stubs in toolchain/wasi-compat.c). Guest DNS in
 * secure-exec goes through the kernel socket/DNS path, not libresolv, so these are unused at runtime.
 * Parameter names are omitted so the header is valid in C++ (meson runs some checks as .cpp, and the
 * real resolv.h uses `int class` which is a C++ keyword). */
#ifndef WASM_COMPAT_RESOLV_H
#define WASM_COMPAT_RESOLV_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int res_init(void);
int res_query(const char *, int, int, unsigned char *, int);
int res_search(const char *, int, int, unsigned char *, int);
int dn_expand(const unsigned char *, const unsigned char *, const unsigned char *, char *, int);
int dn_comp(const char *, unsigned char *, int, unsigned char **, unsigned char **);

#ifdef __cplusplus
}
#endif

#endif /* WASM_COMPAT_RESOLV_H */
