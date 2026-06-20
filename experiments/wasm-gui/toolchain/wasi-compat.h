#ifndef SECURE_EXEC_WASI_COMPAT_H
#define SECURE_EXEC_WASI_COMPAT_H
/* sysroot fcntl.h lacks F_DUPFD; wasi has no real fcntl(F_DUPFD) but the value lets code compile. */
#ifndef F_DUPFD
#define F_DUPFD 0
#endif
/* The patched wasi-libc gates struct rlimit behind __wasilibc_unmodified_upstream (disabled), so it
 * is absent. Provide it + the resource limits the xserver references (no core dumps on wasi). */
#ifndef RLIMIT_CORE
typedef unsigned long long rlim_t;
struct rlimit { rlim_t rlim_cur; rlim_t rlim_max; };
#define RLIMIT_CORE   4
#define RLIMIT_NOFILE 7
#define RLIMIT_DATA   2
#define RLIMIT_STACK  3
#define RLIM_INFINITY (~0ULL)
int getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
#endif
#endif
