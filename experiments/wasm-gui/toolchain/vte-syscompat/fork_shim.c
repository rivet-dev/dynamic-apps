/* __wrap_fork()+exec -> posix_spawn DEFERRED transform for VTE (no real __wrap_fork in wasm).
 * __wrap_fork(): setjmp + return 0 (pretend child); the caller runs its child-setup (dup2/close), which we
 * RECORD into a posix_spawn_file_actions instead of applying; __wrap_execve(): posix_spawn(recorded) + longjmp
 * back to __wrap_fork() returning the child pid. Platform layer (constraint #5), VTE untouched. */
#include <setjmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
typedef struct { int __pad0[2]; void *__actions; int __pad[16]; } posix_spawn_file_actions_t;
typedef struct { int __dummy; } posix_spawnattr_t;
extern int posix_spawn(pid_t*, const char*, const posix_spawn_file_actions_t*, const posix_spawnattr_t*, char*const[], char*const[]);
extern int posix_spawn_file_actions_init(posix_spawn_file_actions_t*);
extern int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t*);
extern int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t*, int, int);
extern int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t*, int);
extern char **environ;

static jmp_buf __fk_jmp;
static volatile int __fk_in_child = 0;
static posix_spawn_file_actions_t __fk_acts;
static pid_t __fk_pid;

int __se_in_fork_child(void) { return __fk_in_child; }
void __se_fork_record_dup2(int o, int n) { posix_spawn_file_actions_adddup2(&__fk_acts, o, n); }
void __se_fork_record_close(int fd) { posix_spawn_file_actions_addclose(&__fk_acts, fd); }

pid_t __wrap_fork(void) {
  posix_spawn_file_actions_init(&__fk_acts);
  __fk_in_child = 1;
  if (setjmp(__fk_jmp) == 0)
    return 0;                 /* "child": caller does its setup + __wrap_execve */
  __fk_in_child = 0;          /* back from the longjmp: the parent */
  posix_spawn_file_actions_destroy(&__fk_acts);
  return __fk_pid;
}
int __wrap_execve(const char *path, char *const argv[], char *const envp[]) {
  if (__fk_in_child) {
    pid_t pid; int r = posix_spawn(&pid, path, &__fk_acts, 0, argv, envp);
    __fk_pid = (r == 0) ? pid : -1;
    if (r != 0) errno = r;
    __fk_in_child = 0;
    longjmp(__fk_jmp, 1);
  }
  errno = ENOSYS; return -1;
}
int __wrap_execv(const char *path, char *const argv[]) { return __wrap_execve(path, argv, environ); }
int __wrap_execvp(const char *file, char *const argv[]) { return __wrap_execve(file, argv, environ); }
int execvpe(const char *file, char *const argv[], char *const envp[]) { return __wrap_execve(file, argv, envp); }
/* dup2: record into file_actions if inside the deferred-__wrap_fork child; else best-effort */
int dup2(int oldfd, int newfd) {
  if (__fk_in_child) { posix_spawn_file_actions_adddup2(&__fk_acts, oldfd, newfd); return newfd; }
  return newfd;
}
