/* Guest-side pty + process shims for VTE, mapping onto the wasm-gui host imports.
 * posix_openpt -> host_process.pty_open (master+slave installed in the fd table).
 * Process/termios funcs the threaded wasi-libc omits. Platform layer (constraint #5), VTE untouched.
 * NOTE: the __wrap_fork->posix_spawn deferred transform is in a separate shim; runtime fd-passing TBD. */
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define WASM_IMPORT(mod,fn) __attribute__((__import_module__(mod), __import_name__(fn)))
WASM_IMPORT("host_process","pty_open") int __se_pty_open(unsigned *m, unsigned *s);

/* master->slave fd map. The kernel pty fds are range-encoded (huge), so use a small linear map keyed by
 * the master fd, NOT an array indexed by fd. VTE opens one pty per terminal; 16 is ample. */
static int __pty_master_tbl[16], __pty_slave_tbl[16];
static int __pty_count = 0;
static int __pty_slave_for(int master) {
  for (int i = 0; i < __pty_count; i++) if (__pty_master_tbl[i] == master) return __pty_slave_tbl[i];
  return -1;
}
int posix_openpt(int flags) {
  (void)flags; unsigned m = 0, s = 0;
  int e = __se_pty_open(&m, &s);
  fprintf(stderr, "PTYDIAG openpt rc=%d master=%u slave=%u\n", e, m, s);
  if (e) { errno = e; return -1; }
  if (__pty_count < 16) { __pty_master_tbl[__pty_count] = (int)m; __pty_slave_tbl[__pty_count] = (int)s; __pty_count++; }
  return (int)m;
}
int grantpt(int fd) { (void)fd; return 0; }
int unlockpt(int fd) { (void)fd; return 0; }
char *ptsname(int fd) {
  static char buf[40];
  snprintf(buf, sizeof buf, "/dev/pts/%d", __pty_slave_for(fd));
  fprintf(stderr, "PTYDIAG ptsname(%d) -> %s\n", fd, buf);
  return buf;
}
int ptsname_r(int fd, char *b, size_t n) { char *p = ptsname(fd); if (!p) return EINVAL; strncpy(b, p, n); return 0; }
/* TIOCGPTPEER: VTE (pty.cc:110) calls ioctl(master, TIOCGPTPEER, flags) to get the slave fd directly,
 * BEFORE the ptsname+open fallback. Return the stashed slave fd so VTE never hits the broken fallback;
 * delegate every other ioctl to the real one. */
#ifndef TIOCGPTPEER
#define TIOCGPTPEER 0x5441
#endif
#ifndef TIOCPKT
#define TIOCPKT 0x5420
#endif
struct __se_ws { unsigned short r, c, x, y; };
static int __pty_is_known(int fd) {
  for (int i = 0; i < __pty_count; i++)
    if (__pty_master_tbl[i] == fd || __pty_slave_tbl[i] == fd) return 1;
  return 0;
}
extern int __real_ioctl(int fd, int request, ...);
int __wrap_ioctl(int fd, int request, ...) {
  va_list ap; va_start(ap, request); void *arg = va_arg(ap, void *); va_end(ap);
  if (request == TIOCGPTPEER) {
    int s = __pty_slave_for(fd);
    if (s >= 0) return s;
    errno = EINVAL; return -1;
  }
  /* The kernel pty fds are range-encoded and have NO real ioctl in the WASM runner (an unhandled ioctl
   * there traps the guest). Handle terminal ioctls on those fds locally: TIOCGWINSZ fills a default size,
   * TIOCPKT/TIOCSWINSZ/etc. are success no-ops (the kernel pty line discipline owns real modes). */
  if (__pty_is_known(fd)) {
    if (request == TIOCGWINSZ && arg) {
      struct __se_ws *w = (struct __se_ws *)arg; w->r = 24; w->c = 80; w->x = 0; w->y = 0;
    }
    return 0;
  }
  return __real_ioctl(fd, request, arg);
}
/* process-model stubs (single-process kernel) */
pid_t __wrap_setsid(void) { return getpid(); }
pid_t __wrap_getpgid(pid_t p) { (void)p; return getpid(); }
int __wrap_setpgid(pid_t a, pid_t b) { (void)a; (void)b; return 0; }
int __wrap_pthread_sigmask(int how, const void *set, void *old) { (void)how; (void)set; (void)old; return 0; }
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
