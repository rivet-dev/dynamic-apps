#ifndef SE_COMPAT_STDLIB_H
#define SE_COMPAT_STDLIB_H
#include_next <stdlib.h>
/* wasi-libc omits the pty allocation functions. Declare them so VTE's meson compile-checks pass; the
 * real impls back onto the wasi-pty seam (the Browser-PTY kernel-pty bridge). Platform layer, not VTE. */
#ifdef __cplusplus
extern "C" {
#endif
int posix_openpt(int __flags);
int grantpt(int __fd);
int unlockpt(int __fd);
char *ptsname(int __fd);
int ptsname_r(int __fd, char *__buf, size_t __buflen);
#ifdef __cplusplus
}
#endif
#endif
