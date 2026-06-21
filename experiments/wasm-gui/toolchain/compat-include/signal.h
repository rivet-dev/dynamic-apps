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
#endif /* SECURE_EXEC_WASM_THREADS */
#endif /* WASM_COMPAT_SIGNAL_H */
