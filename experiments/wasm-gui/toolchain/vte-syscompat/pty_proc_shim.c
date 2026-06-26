/* Guest-side pty + process shims for VTE, mapping onto the wasm-gui host imports.
 * posix_openpt -> host_process.pty_open (master+slave installed in the fd table).
 * Process/termios funcs the threaded wasi-libc omits. Platform layer (constraint #5), VTE untouched.
 * NOTE: the fork->posix_spawn deferred transform is in a separate shim; runtime fd-passing TBD. */
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#define WASM_IMPORT(mod,fn) __attribute__((__import_module__(mod), __import_name__(fn)))
WASM_IMPORT("host_process","pty_open") int __se_pty_open(unsigned *m, unsigned *s);

static int __pty_slave_for_master[1024];
int posix_openpt(int flags) {
  (void)flags; unsigned m = 0, s = 0;
  int e = __se_pty_open(&m, &s);
  if (e) { errno = e; return -1; }
  if (m < 1024) __pty_slave_for_master[m] = (int)s;
  return (int)m;
}
int grantpt(int fd) { (void)fd; return 0; }
int unlockpt(int fd) { (void)fd; return 0; }
char *ptsname(int fd) {
  static char buf[40];
  int s = (fd >= 0 && fd < 1024) ? __pty_slave_for_master[fd] : -1;
  snprintf(buf, sizeof buf, "/dev/pts/%d", s);
  return buf;
}
int ptsname_r(int fd, char *b, size_t n) { char *p = ptsname(fd); if (!p) return EINVAL; strncpy(b, p, n); return 0; }
/* process-model stubs (single-process kernel) */
pid_t setsid(void) { return getpid(); }
pid_t getpgid(pid_t p) { (void)p; return getpid(); }
int setpgid(pid_t a, pid_t b) { (void)a; (void)b; return 0; }
int pthread_sigmask(int how, const void *set, void *old) { (void)how; (void)set; (void)old; return 0; }
/* termios: minimal (the kernel pty discipline handles real modes via __pty_set_raw_mode) */
int tcgetattr(int fd, struct termios *t) { (void)fd; if (t) memset(t, 0, sizeof(*t)); return 0; }
int tcsetattr(int fd, int act, const struct termios *t) { (void)fd; (void)act; (void)t; return 0; }
int tcsendbreak(int fd, int d) { (void)fd; (void)d; return 0; }
int tcdrain(int fd) { (void)fd; return 0; }
int tcflush(int fd, int q) { (void)fd; (void)q; return 0; }
int tcflow(int fd, int a) { (void)fd; (void)a; return 0; }
void cfmakeraw(struct termios *t) { (void)t; }
speed_t cfgetispeed(const struct termios *t) { (void)t; return 0; }
speed_t cfgetospeed(const struct termios *t) { (void)t; return 0; }
int cfsetispeed(struct termios *t, speed_t s) { (void)t; (void)s; return 0; }
int cfsetospeed(struct termios *t, speed_t s) { (void)t; (void)s; return 0; }
int cfsetspeed(struct termios *t, speed_t s) { (void)t; (void)s; return 0; }
pid_t tcgetsid(int fd) { (void)fd; return getpid(); }
int kill(pid_t p, int s) { (void)p; (void)s; return 0; }
