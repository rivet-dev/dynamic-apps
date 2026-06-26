#ifndef WASM_COMPAT_SYS_IOCTL_H
#define WASM_COMPAT_SYS_IOCTL_H
/* Pull struct winsize (alltypes.h guards it behind __NEED_struct_winsize) + the TIOC* request consts.
 * The sysroot's <sys/ioctl.h> already declares ioctl() -- do NOT redeclare it. Platform shim. */
#define __NEED_struct_winsize
#include <bits/alltypes.h>
#if defined(__has_include_next)
#if __has_include_next(<sys/ioctl.h>)
#include_next <sys/ioctl.h>
#endif
#endif
#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#endif
#ifndef TIOCSWINSZ
#define TIOCSWINSZ 0x5414
#endif
#ifndef TIOCSCTTY
#define TIOCSCTTY 0x540E
#endif
#ifndef TIOCNOTTY
#define TIOCNOTTY 0x5422
#endif
#endif
