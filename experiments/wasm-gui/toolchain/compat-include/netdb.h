/* Compat <netdb.h> for wasm32-wasip1: pulls in wasi-libc's real header (if any), then declares h_errno,
 * which GLib's GIO threaded resolver references but wasi-libc omits. h_errno is defined as a stub global
 * in toolchain/wasi-compat.c (GIO's libresolv resolver is unused at runtime in the sandbox). */
#ifndef WASM_COMPAT_NETDB_H
#define WASM_COMPAT_NETDB_H

#if defined(__has_include)
#if __has_include_next(<netdb.h>)
#include_next <netdb.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef h_errno
extern int h_errno;
#endif

#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#define TRY_AGAIN 2
#define NO_RECOVERY 3
#define NO_DATA 4
#endif

#ifdef __cplusplus
}
#endif

#endif /* WASM_COMPAT_NETDB_H */
