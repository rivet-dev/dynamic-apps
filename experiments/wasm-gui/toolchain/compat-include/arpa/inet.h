/* Compat <arpa/inet.h> for wasm32-wasip1[-threads]: pulls in the sysroot header (which provides
 * inet_ntop/inet_pton + htons/ntohs), then declares the legacy IPv4 helpers it omits. libX11's
 * Xtranssock.c uses inet_addr to parse a TCP X-server address; the TCP path is unused at runtime in
 * the sandbox (the X connection is a unix socket through host_net), but it must compile. Real (small)
 * implementations live in toolchain/wasi-compat.c so a TCP-loopback address would still parse. */
#ifndef WASM_COMPAT_ARPA_INET_H
#define WASM_COMPAT_ARPA_INET_H

#if defined(__has_include)
#if __has_include_next(<arpa/inet.h>)
#include_next <arpa/inet.h>
#endif
#endif

#include <inttypes.h>
#include <netinet/in.h>   /* struct in_addr (inet_ntoa takes it by value) */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffffU
#endif

/* The sysroot arpa/inet.h declares inet_aton/inet_pton/inet_ntop unconditionally but guards
 * inet_addr/inet_ntoa behind __wasilibc_unmodified_upstream (off). Declare only those two, with the
 * canonical signatures, so there is no conflict with the sysroot decls. (Symbols for all three live
 * in toolchain/wasi-compat.c.) */
in_addr_t inet_addr(const char *cp);
char *inet_ntoa(struct in_addr in);

#ifdef __cplusplus
}
#endif

#endif /* WASM_COMPAT_ARPA_INET_H */
