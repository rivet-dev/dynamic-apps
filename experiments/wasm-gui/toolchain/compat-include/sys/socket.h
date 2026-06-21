/* Compat <sys/socket.h> for wasm32-wasip1: pulls in wasi-libc's real header, then fills the BSD socket
 * gaps GLib's GIO (gsocket.c) needs to COMPILE. wasi-libc has sockets only partially; the missing bits
 * are control-message (ancillary-data / fd-passing) + a couple of socket types/options that don't apply
 * to wasi. These are link/compile shims — GIO's GSocket networking is unused at runtime in the sandbox
 * (guest I/O goes through secure-exec's host_net path). All additions are #ifndef-guarded so they never
 * clash with a future wasi-libc that provides them. */
#ifndef WASM_COMPAT_SYS_SOCKET_H
#define WASM_COMPAT_SYS_SOCKET_H

#include_next <sys/socket.h>

#include <stddef.h>

/* The vanilla wasm32-wasip1-threads sysroot (Phase 0) does not declare socket()/socketpair() — wasi
 * cannot create sockets, so wasi-libc omits them (the patched non-threaded sysroot adds them). Declare
 * them here under the threads profile; wasi-compat provides weak definitions. */
#ifdef SECURE_EXEC_WASM_THREADS
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
#endif

/* wasi defines SOCK_STREAM/SOCK_DGRAM via the WASI filetype enum (DGRAM=5, STREAM=6). SOCK_SEQPACKET
 * and SOCK_RAW are unsupported on wasi; give them distinct unused values so switch() over socket type
 * has no duplicate cases (these socket types are never actually created in the sandbox). */
#ifndef SOCK_SEQPACKET
#define SOCK_SEQPACKET 137
#endif
#ifndef SOCK_RAW
#define SOCK_RAW 138
#endif
#ifndef SO_BROADCAST
#define SO_BROADCAST 6
#endif
#ifndef SCM_RIGHTS
#define SCM_RIGHTS 0x01
#endif
/* Protocol-family aliases (BSD): GLib uses PF_UNIX; wasi defines AF_* but not all PF_*. */
#ifndef PF_UNIX
#ifdef AF_UNIX
#define PF_UNIX AF_UNIX
#else
#define PF_UNIX 1
#endif
#endif
#ifndef PF_LOCAL
#define PF_LOCAL PF_UNIX
#endif

/* Control-message (ancillary data) machinery. wasi-libc forward-declares struct cmsghdr but never
 * completes it; complete it here plus the CMSG_* accessors (musl-compatible layout). */
#ifndef WASM_COMPAT_HAVE_CMSGHDR
#define WASM_COMPAT_HAVE_CMSGHDR 1
struct cmsghdr {
    socklen_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};
#define CMSG_DATA(cmsg) ((unsigned char *) (((struct cmsghdr *) (cmsg)) + 1))
#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & (size_t) ~(sizeof(size_t) - 1))
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_SPACE(len) (CMSG_ALIGN(len) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define __WASM_CMSG_LEN(cmsg) \
    (((cmsg)->cmsg_len + sizeof(long) - 1) & ~(long) (sizeof(long) - 1))
#define __WASM_CMSG_NEXT(cmsg) ((unsigned char *) (cmsg) + __WASM_CMSG_LEN(cmsg))
#define __WASM_MHDR_END(mhdr) \
    ((unsigned char *) (mhdr)->msg_control + (mhdr)->msg_controllen)
#define CMSG_FIRSTHDR(mhdr) \
    ((size_t) (mhdr)->msg_controllen >= sizeof(struct cmsghdr) \
         ? (struct cmsghdr *) (mhdr)->msg_control \
         : (struct cmsghdr *) 0)
#define CMSG_NXTHDR(mhdr, cmsg) \
    ((cmsg)->cmsg_len < sizeof(struct cmsghdr) || \
     __WASM_CMSG_LEN(cmsg) + sizeof(struct cmsghdr) >= \
         (size_t) (__WASM_MHDR_END(mhdr) - (unsigned char *) (cmsg)) \
         ? (struct cmsghdr *) 0 \
         : (struct cmsghdr *) __WASM_CMSG_NEXT(cmsg))
#endif /* WASM_COMPAT_HAVE_CMSGHDR */

#endif /* WASM_COMPAT_SYS_SOCKET_H */
