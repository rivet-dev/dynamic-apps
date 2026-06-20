/* BSD err()/warn() family + uid getters that wasi libc lacks. Linked (via LDFLAGS) ONLY into the
   demo/CLI/test executables some X libs build alongside their library (sxpm, libXfont2 test progs).
   The libraries themselves never call these; this just lets `make`/`make install` finish so the
   real product (the .a) installs. Not part of any guest .wasm. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
static void vp(const char *fmt, va_list ap, int useerrno) {
    if (fmt) vfprintf(stderr, fmt, ap);
    fputc('\n', stderr); (void)useerrno;
}
void verr(int e, const char *f, va_list ap){ vp(f,ap,1); exit(e); }
void verrx(int e, const char *f, va_list ap){ vp(f,ap,0); exit(e); }
void vwarn(const char *f, va_list ap){ vp(f,ap,1); }
void vwarnx(const char *f, va_list ap){ vp(f,ap,0); }
void err(int e, const char *f, ...){ va_list a; va_start(a,f); vp(f,a,1); va_end(a); exit(e); }
void errx(int e, const char *f, ...){ va_list a; va_start(a,f); vp(f,a,0); va_end(a); exit(e); }
void warn(const char *f, ...){ va_list a; va_start(a,f); vp(f,a,1); va_end(a); }
void warnx(const char *f, ...){ va_list a; va_start(a,f); vp(f,a,0); va_end(a); }
unsigned getuid(void){ return 0; }
unsigned geteuid(void){ return 0; }
unsigned getgid(void){ return 0; }
unsigned getegid(void){ return 0; }
