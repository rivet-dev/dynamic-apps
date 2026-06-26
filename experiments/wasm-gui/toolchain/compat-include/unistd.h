#ifndef SE_COMPAT_UNISTD_H
#define SE_COMPAT_UNISTD_H
#include_next <unistd.h>
#include <sys/types.h>
/* wasi-libc omits fork() (no process duplication). Declare it so configure checks that only test
 * compilability (e.g. VTE meson.build "Checking if fork compiles") pass; the real implementation is a
 * fork->posix_spawn emulation shim (posix_spawn IS provided by the wasi-spawn bridge). Platform layer. */
#ifdef __cplusplus
extern "C" {
#endif
pid_t fork(void);
#ifdef __cplusplus
}
#endif
#endif
#ifndef SE_FDWALK_DECLARED
#define SE_FDWALK_DECLARED
#ifdef __cplusplus
extern "C" {
#endif
int fdwalk(int (*)(void *, int), void *);
int close_range(unsigned int, unsigned int, int);
#ifdef __cplusplus
}
#endif
#endif
#ifndef SE_DUP2_DECLARED
#define SE_DUP2_DECLARED
#ifdef __cplusplus
extern "C" {
#endif
int dup2(int, int);
#ifdef __cplusplus
}
#endif
#endif
#ifndef SE_PROC_DECLARED
#define SE_PROC_DECLARED
#ifdef __cplusplus
extern "C" {
#endif
int execv(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execvp(const char *, char *const[]);
int execvpe(const char *, char *const[], char *const[]);
pid_t getpgid(pid_t);
pid_t getppid(void);
pid_t setsid(void);
int setpgid(pid_t, pid_t);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
#ifdef __cplusplus
}
#endif
#endif
