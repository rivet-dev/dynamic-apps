#ifndef SE_COMPAT_SYS_WAIT_H
#define SE_COMPAT_SYS_WAIT_H
#include <sys/types.h>
#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s) (((s)&0x7f)==0)
#define WEXITSTATUS(s) (((s)&0xff00)>>8)
#define WIFSIGNALED(s) (((signed char)(((s)&0x7f)+1)>>1)>0)
#define WTERMSIG(s) ((s)&0x7f)
#define WIFSTOPPED(s) (((s)&0xff)==0x7f)
#define WSTOPSIG(s) WEXITSTATUS(s)
#define W_EXITCODE(ret,sig) ((ret)<<8 | (sig))
#define W_STOPCODE(sig) ((sig)<<8 | 0x7f)
#ifdef __cplusplus
extern "C" {
#endif
pid_t waitpid(pid_t, int *, int);
pid_t wait(int *);
#ifdef __cplusplus
}
#endif
#endif
