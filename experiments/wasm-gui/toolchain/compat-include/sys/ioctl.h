#ifndef WASM_COMPAT_SYS_IOCTL_H
#define WASM_COMPAT_SYS_IOCTL_H
/* wasi lacks the terminal ioctl request constants; VTE + terminal emulators need TIOCGWINSZ/TIOCSWINSZ
 * to COMPILE (struct winsize is already in the sysroot's bits/alltypes.h). Linux request values. The
 * kernel-PTY seam handles the actual runtime ioctl on the PTY fd. Constraint #5: platform shim, upstream
 * VTE untouched. */
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
#endif
