/* compat.h — forced-include shim filling full-OS libc gaps for porting
 * Linux C programs (vim) to the secure-exec wasm runtime. */
#ifndef _SECUREEXEC_WASM_COMPAT_H
#define _SECUREEXEC_WASM_COMPAT_H
#include <sys/types.h>
#include <poll.h>
#include <sys/time.h>

/* waitpid flags (no sys/wait.h with these on wasi) */
#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WUNTRACED
#define WUNTRACED 2
#endif
#ifndef WCONTINUED
#define WCONTINUED 8
#endif

/* interval timers — wasi-libc guards these out; we expose + stub them */
#ifndef ITIMER_REAL
#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF 2
struct itimerval { struct timeval it_interval; struct timeval it_value; };
int setitimer(int, const struct itimerval *, struct itimerval *);
int getitimer(int, struct itimerval *);
#endif

mode_t umask(mode_t);

#endif
