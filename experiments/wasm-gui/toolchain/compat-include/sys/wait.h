#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H
#include <sys/types.h>
pid_t waitpid(pid_t, int *, int);
pid_t wait(int *);
#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#define WEXITSTATUS(s)  (((s) >> 8) & 0xff)
#define WTERMSIG(s)     ((s) & 0x7f)
#define WIFSIGNALED(s)  (((signed char)(((s) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s)     WEXITSTATUS(s)
#define WCOREDUMP(s)    ((s) & 0x80)
#define WIFCONTINUED(s) ((s) == 0xffff)
#endif
