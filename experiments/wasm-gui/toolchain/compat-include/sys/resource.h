#ifndef SE_COMPAT_SYS_RESOURCE_H
#define SE_COMPAT_SYS_RESOURCE_H
#include <sys/types.h>
#include <sys/time.h>
typedef unsigned long long rlim_t;
#define RLIM_INFINITY (~0ULL)
#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIMIT_CPU 0
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_RSS 5
#define RLIMIT_NPROC 6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS 9
#define RLIM_NLIMITS 16
struct rlimit { rlim_t rlim_cur, rlim_max; };
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)
struct rusage { struct timeval ru_utime, ru_stime; long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss; };
#ifdef __cplusplus
extern "C" {
#endif
int getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
int getrusage(int, struct rusage *);
#ifdef __cplusplus
}
#endif
#endif
