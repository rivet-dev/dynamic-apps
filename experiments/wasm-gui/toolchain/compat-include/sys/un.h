/* Compat <sys/un.h> for the wasm32-wasip1-threads (Phase 0) profile: the threaded sysroot guards the
 * real `struct sockaddr_un { sa_family_t sun_family; char sun_path[108]; }` behind
 * __wasilibc_unmodified_upstream and otherwise uses a stub without sun_path. GIO gsocketaddress.c
 * (GUnixSocketAddress) needs sun_path to COMPILE (unix sockets are unused at runtime in the sandbox).
 * Flip the gate just for this include so the full struct is used. */
#ifndef WASM_COMPAT_SYS_UN_H
#define WASM_COMPAT_SYS_UN_H
#if defined(SECURE_EXEC_WASM_THREADS) && !defined(__wasilibc_unmodified_upstream)
#define __wasilibc_unmodified_upstream 1
#include_next <sys/un.h>
#undef __wasilibc_unmodified_upstream
#else
#include_next <sys/un.h>
#endif
#endif
