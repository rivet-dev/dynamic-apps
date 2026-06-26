#ifndef SE_COMPAT_TERMIOS_H
#define SE_COMPAT_TERMIOS_H
#include_next <termios.h>
#ifdef __cplusplus
extern "C" {
#endif
int tcgetattr(int, struct termios *);
int tcsetattr(int, int, const struct termios *);
int tcsendbreak(int, int);
int tcdrain(int);
int tcflush(int, int);
int tcflow(int, int);
void cfmakeraw(struct termios *);
speed_t cfgetispeed(const struct termios *);
speed_t cfgetospeed(const struct termios *);
int cfsetispeed(struct termios *, speed_t);
int cfsetospeed(struct termios *, speed_t);
int cfsetspeed(struct termios *, speed_t);
pid_t tcgetsid(int);
#ifdef __cplusplus
}
#endif
#endif
