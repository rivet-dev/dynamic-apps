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
/* Socket levels/options + flags GIO gsocket.c references; the threaded sysroot omits them (wasi has no
 * BSD sockets). Linux/musl numeric values (compile-only; runtime goes through host_net). */
#ifndef SOL_SOCKET
#define SOL_SOCKET 1
#endif
#ifndef SO_REUSEADDR
#define SO_REUSEADDR 2
#endif
#ifndef SO_TYPE
#define SO_TYPE 3
#endif
#ifndef SO_ERROR
#define SO_ERROR 4
#endif
#ifndef SO_DONTROUTE
#define SO_DONTROUTE 5
#endif
#ifndef SO_SNDBUF
#define SO_SNDBUF 7
#endif
#ifndef SO_RCVBUF
#define SO_RCVBUF 8
#endif
#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 9
#endif
#ifndef SO_OOBINLINE
#define SO_OOBINLINE 10
#endif
#ifndef SO_LINGER
#define SO_LINGER 13
#endif
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif
#ifndef SO_RCVTIMEO
#define SO_RCVTIMEO 20
#endif
#ifndef SO_SNDTIMEO
#define SO_SNDTIMEO 21
#endif
#ifndef SO_ACCEPTCONN
#define SO_ACCEPTCONN 30
#endif
#ifndef SO_PROTOCOL
#define SO_PROTOCOL 38
#endif
#ifndef SOMAXCONN
#define SOMAXCONN 128
#endif
#ifndef MSG_OOB
#define MSG_OOB 0x01
#endif
#ifndef MSG_PEEK
#define MSG_PEEK 0x02
#endif
#ifndef MSG_DONTROUTE
#define MSG_DONTROUTE 0x04
#endif
#ifndef MSG_CTRUNC
#define MSG_CTRUNC 0x08
#endif
#ifndef MSG_TRUNC
#define MSG_TRUNC 0x20
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0x40
#endif
#ifndef MSG_EOR
#define MSG_EOR 0x80
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x4000
#endif
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0x40000000
#endif
#ifndef SHUT_RD
#define SHUT_RD 0
#endif
#ifndef SHUT_WR
#define SHUT_WR 1
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
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
