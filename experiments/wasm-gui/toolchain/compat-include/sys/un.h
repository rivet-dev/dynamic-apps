/* Compat <sys/un.h> for wasm32-wasip1-threads (Phase 0): the threaded sysroot's struct sockaddr_un
 * (from __struct_sockaddr_un.h) is a stub with only sun_family; GIO gsocketaddress.c needs sun_path.
 * Pre-set the wasi header's include guard and provide the FULL struct, so sys/un.h's
 * #include <__struct_sockaddr_un.h> is a no-op and uses ours. (Avoids the __wasilibc_unmodified_upstream
 * hack, which re-triggers bits/alltypes.h and redefines timeval/timespec/iovec.) Unix sockets are
 * unused at runtime in the sandbox. */
#ifndef WASM_COMPAT_SYS_UN_H
#define WASM_COMPAT_SYS_UN_H
#ifdef SECURE_EXEC_WASM_THREADS
#ifndef __wasilibc___struct_sockaddr_un_h
#define __wasilibc___struct_sockaddr_un_h
#include <__typedef_sa_family_t.h>
struct sockaddr_un {
  sa_family_t sun_family;
  char sun_path[108];
};
#endif
#endif
#include_next <sys/un.h>
#endif
