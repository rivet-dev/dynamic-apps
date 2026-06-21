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

/* The threaded sysroot has no <netdb.h>, so struct addrinfo + AI_/EAI_ + getaddrinfo are absent (the
 * patched non-threaded sysroot provided them). GIO's ginetsocketaddress.c / gresolver need them to
 * COMPILE; guest name resolution actually goes through secure-exec's host_net path at runtime.
 * Guarded on AI_NUMERICHOST so this never clashes with a sysroot that already provides netdb. */
#ifndef AI_NUMERICHOST
#include <sys/socket.h>
#include <stddef.h>
struct addrinfo {
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  unsigned ai_addrlen;
  struct sockaddr *ai_addr;
  char *ai_canonname;
  struct addrinfo *ai_next;
};
#define AI_PASSIVE     0x0001
#define AI_CANONNAME   0x0002
#define AI_NUMERICHOST 0x0004
#define AI_NUMERICSERV 0x0008
#define AI_V4MAPPED    0x0800
#define AI_ALL         0x0100
#define AI_ADDRCONFIG  0x0400
#define NI_NUMERICHOST 0x0001
#define NI_NUMERICSERV 0x0002
#define NI_NOFQDN 0x0004
#define NI_NAMEREQD 0x0008
#define NI_DGRAM 0x0010
#define NI_MAXHOST     1025
#define NI_MAXSERV     32
#define EAI_BADFLAGS   -1
#define EAI_NONAME     -2
#define EAI_AGAIN      -3
#define EAI_FAIL       -4
#define EAI_FAMILY     -6
#define EAI_MEMORY     -10
#define EAI_SYSTEM     -11
int getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
void freeaddrinfo(struct addrinfo *);
const char *gai_strerror(int);
/* getservbyname/port + struct servent (GIO gnetworking.c). */
struct servent {
  char *s_name;
  char **s_aliases;
  int s_port;
  char *s_proto;
};
struct servent *getservbyname(const char *, const char *);
struct servent *getservbyport(int, const char *);
/* Legacy resolver API: libxcb's _xcb_open_tcp (xcb_util.c) connects to a TCP X server by hostname.
 * Unused at runtime in the sandbox (the X connection is a unix socket through host_net), but must
 * compile. gethostbyname is a weak NULL stub in wasi-compat.c. */
struct hostent {
  char *h_name;
  char **h_aliases;
  int h_addrtype;
  int h_length;
  char **h_addr_list;
};
#define h_addr h_addr_list[0]
struct hostent *gethostbyname(const char *);
struct hostent *gethostbyaddr(const void *, unsigned, int);
#endif /* AI_NUMERICHOST */

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
