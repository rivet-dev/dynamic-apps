/* Compat <signal.h> for the wasm32-wasip1-threads (Phase 0) profile: the threaded sysroot's emulated
 * signal.h forward-declares struct sigaction and omits the SA_ and SIG_ constants that GLib gmain.c
 * (unix signal source) needs to COMPILE. The patched non-threaded sysroot provided these. Runtime is
 * a no-op (wasi has no signals; wasi-compat provides a weak sigaction stub). Threads-guarded so the
 * non-threaded build is untouched. */
#ifndef WASM_COMPAT_SIGNAL_H
#define WASM_COMPAT_SIGNAL_H
#include_next <signal.h>
#ifdef SECURE_EXEC_WASM_THREADS
#ifndef SA_NOCLDSTOP
#define SA_NOCLDSTOP 1
#endif
#ifndef SA_RESTART
#define SA_RESTART 0x10000000
#endif
#ifndef SIG_BLOCK
#define SIG_BLOCK 0
#endif
#ifndef SIG_UNBLOCK
#define SIG_UNBLOCK 1
#endif
#ifndef SIG_SETMASK
#define SIG_SETMASK 2
#endif
/* sigset_t is defined by the sysroot's __typedef_sigset_t.h but the emulated signal.h doesn't pull it
 * in; include it directly (its own include guard makes this idempotent) so we use the real type. */
#include <__typedef_sigset_t.h>
/* Complete the forward-declared struct sigaction (incomplete in the threaded signal.h). */
struct sigaction {
  void (*sa_handler)(int);
  sigset_t sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
};
int sigaction(int, const struct sigaction *, struct sigaction *);
int sigemptyset(sigset_t *);
int sigfillset(sigset_t *);
int sigaddset(sigset_t *, int);
/* kill()/killpg() are gated behind __wasilibc_unmodified_upstream in the threaded sysroot (wasi has no
 * signals). GTK's gtkmountoperation-x11.c calls kill() to stop a mount helper (dead code in the
 * sandbox); declare it so the call compiles. Weak no-op stub lives in wasi-compat.c.
 * Pull in pid_t directly (musl __NEED mechanism) so this header is self-contained: gbacktrace.c and
 * others include <signal.h> before <sys/types.h>, so pid_t would otherwise be undefined here. */
#define __NEED_pid_t
#include <bits/alltypes.h>
int kill(pid_t, int);
int killpg(pid_t, int);
#endif /* SECURE_EXEC_WASM_THREADS */
#endif /* WASM_COMPAT_SIGNAL_H */
#ifndef SE_PTHREAD_SIGMASK_DECLARED
#define SE_PTHREAD_SIGMASK_DECLARED
#ifdef __cplusplus
extern "C" {
#endif
int pthread_sigmask(int, const void *, void *);
#ifdef __cplusplus
}
#endif
#endif
