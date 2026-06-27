#ifndef SECURE_EXEC_WASI_COMPAT_H
#define SECURE_EXEC_WASI_COMPAT_H
/* NOTE: flockfile/funlockfile/ftrylockfile are declared by wasi-libc's <stdio.h> (and the symbols exist
 * in libc), so we do NOT redeclare them here — doing so conflicts with stdio.h's FILE* prototypes.
 * Code that hit implicit-declaration warnings for them compiles under -Wno-error=implicit-function-
 * declaration (already in the cross args). */
/* sysroot fcntl.h lacks F_DUPFD; wasi has no real fcntl(F_DUPFD) but the value lets code compile. */
#ifndef F_DUPFD
#define F_DUPFD 0
#endif
/* libX11's Xos_r.h references MAXHOSTNAMELEN but only pulls <sys/param.h> under platform guards that
 * don't match wasi; force-define it (matches the sysroot's sys/param.h value). */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif
/* The patched non-threaded libc declares + defines getuid/geteuid/getgid/getegid; the vanilla
 * threaded sysroot omits both. wasi-compat.c provides weak stub symbols (return 0); declare them
 * here so callers (e.g. libX11 GetDflt.c) compile. Threaded-only to avoid clashing with the patched
 * unistd.h decls on the non-threaded profile. */
#ifdef SECURE_EXEC_WASM_THREADS
unsigned getuid(void);
unsigned geteuid(void);
unsigned getgid(void);
unsigned getegid(void);
/* libXt NextEvent.c references POLLPRI, which the sysroot's <sys/poll.h> (__header_poll.h) omits
 * (only <poll.h> defines it). wasi's poll has no urgent-data band, so this is compile-only; value
 * matches the full <poll.h> (0x002) so a later include is a same-value (harmless) redefinition. */
#ifndef POLLPRI
#define POLLPRI 0x002
#endif
#endif
/* wasi has no POSIX record locking (F_SETLK/struct flock). Define the constants + struct so code that
 * does best-effort cache-file locking (e.g. fontconfig fccache.c) compiles; the wasi fcntl() ignores
 * these commands, which is fine in the single-process sandbox where nothing else contends. */
#ifndef F_SETLK
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#endif
/* The patched wasi-libc gates struct rlimit behind __wasilibc_unmodified_upstream (disabled), so it
 * is absent. Provide it + the resource limits the xserver references (no core dumps on wasi). */
#ifndef RLIMIT_CORE
typedef unsigned long long rlim_t;
struct rlimit { rlim_t rlim_cur; rlim_t rlim_max; };
#define SE_HAVE_RLIMIT 1
#define RLIMIT_CORE   4
#define RLIMIT_NOFILE 7
#define RLIMIT_DATA   2
#define RLIMIT_STACK  3
#define RLIM_INFINITY (~0ULL)
int getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
#endif
#endif
