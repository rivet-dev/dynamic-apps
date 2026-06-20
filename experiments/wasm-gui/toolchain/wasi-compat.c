/* No-op definitions for POSIX functions the patched wasi sysroot lacks (single-threaded, no process
   groups/signals). Linked into X-stack executables via the meson cross file. */
#include <stdio.h>
#include <sys/types.h>
void flockfile(FILE *f) { (void)f; }
void funlockfile(FILE *f) { (void)f; }
int ftrylockfile(FILE *f) { (void)f; return 0; }
int getpgrp(void) { return 1; }
int setpgid(int p, int g) { (void)p; (void)g; return 0; }
int setsid(void) { return 1; }              /* no process sessions on wasi (JWM/WMs call it once) */
void tzset(void) {}                          /* no timezone db on wasi; localtime stays UTC */
unsigned umask(unsigned m) { (void)m; return 0; }
int pthread_sigmask(int how, const void *set, void *old) { (void)how; (void)set; (void)old; return 0; }
#include <string.h>
struct utsname;
int uname(struct utsname *u) {
    char *p = (char *)u;
    if (p) { memset(p, 0, 65*6);
        memcpy(p+0,    "secure-exec", 11);
        memcpy(p+65,   "wasm", 4);
        memcpy(p+65*2, "1.0", 3);
        memcpy(p+65*3, "1.0", 3);
        memcpy(p+65*4, "wasm32", 6); }
    return 0;
}
int getrlimit(int r, void *l) { (void)r; (void)l; return 0; }
int setrlimit(int r, const void *l) { (void)r; (void)l; return 0; }
/* genuinely-missing-in-wasi functions the X server references (stubs: server runs single-threaded,
   no real signals/hostname lookup). Name-linkage; arg types are placeholders. */
void *gethostbyname(const char *n) { (void)n; return 0; }
int msync(void *a, unsigned long l, int f) { (void)a; (void)l; (void)f; return 0; }
int sigprocmask(int h, const void *s, void *o) { (void)h; (void)s; (void)o; return 0; }
int pthread_create(void *t, const void *a, void *(*f)(void *), void *arg) { (void)t; (void)a; (void)f; (void)arg; return 1; }
int pthread_join(unsigned long t, void **r) { (void)t; (void)r; return 0; }
int pthread_attr_setscope(void *a, int s) { (void)a; (void)s; return 0; }
int pthread_setname_np(unsigned long t, const char *n) { (void)t; (void)n; return 0; }
void __SIG_IGN(int s) { (void)s; }
/* process/net stubs genuinely absent from wasi (X server runs single-process, local-only). */
int fork(void) { return -1; }
int execl(const char *p, ...) { (void)p; return -1; }
int execvp(const char *f, char *const a[]) { (void)f; (void)a; return -1; }
void *getservbyname(const char *n, const char *p) { (void)n; (void)p; return 0; }
/* inet_addr/inet_aton/inet_ntoa: referenced by libXfont2's font-server transport (fstrans.o),
   which is dead code here (Xvfb uses a local font path, never an xfs TCP font server). Provide a
   small functional dotted-quad parser so the symbols resolve; no real network name lookup occurs. */
unsigned inet_addr(const char *cp) {
    unsigned parts[4] = {0,0,0,0}; int i = 0; const char *p = cp;
    if (!p) return 0xFFFFFFFFu;
    for (; i < 4; i++) {
        unsigned v = 0; int digits = 0;
        while (*p >= '0' && *p <= '9') { v = v*10 + (unsigned)(*p - '0'); p++; digits++; }
        if (!digits || v > 255) return 0xFFFFFFFFu;
        parts[i] = v;
        if (i < 3) { if (*p != '.') return 0xFFFFFFFFu; p++; }
    }
    if (*p != '\0') return 0xFFFFFFFFu;
    /* network byte order (big-endian) */
    return (parts[0]) | (parts[1]<<8) | (parts[2]<<16) | (parts[3]<<24);
}
int inet_aton(const char *cp, void *inp) {
    unsigned a = inet_addr(cp);
    if (a == 0xFFFFFFFFu && !(cp && cp[0]=='2'&&cp[1]=='5'&&cp[2]=='5')) { return 0; }
    if (inp) *(unsigned *)inp = a;
    return 1;
}
char *inet_ntoa(unsigned in) {
    static char b[16]; unsigned char *o = (unsigned char *)&in;
    int n = 0; unsigned q[4] = {o[0],o[1],o[2],o[3]};
    for (int i=0;i<4;i++){ unsigned v=q[i]; char t[4]; int k=0;
        if(!v)t[k++]='0'; while(v){t[k++]=(char)('0'+v%10);v/=10;}
        while(k)b[n++]=t[--k]; if(i<3)b[n++]='.'; }
    b[n]='\0'; return b;
}
int seteuid(unsigned u) { (void)u; return 0; }
int setuid(unsigned u) { (void)u; return 0; }
int setgid(unsigned g) { (void)g; return 0; }
char *strsignal(int s) { (void)s; return (char *)"signal"; }
/* twm (WM) references these; stubbed: the WM runs in a single sandboxed process with no
   subprocess exec, no real uid, and uses tempnam only for a session id. */
int execlp(const char *f, const char *a, ...) { (void)f; (void)a; return -1; }
int system(const char *c) { (void)c; return -1; }
/* getuid/geteuid/getgid are provided by the patched libc — do not redefine (duplicate symbol). */
char *tempnam(const char *dir, const char *pfx) { (void)dir; (void)pfx; static char b[] = "/tmp/twmXXXXXX"; return b; }
/* dlopen/dlsym are absent on wasi (no shared objects). libX11 references them for loadable i18n,
   but it was built --disable-loadable-i18n, so these are never actually called. getpwnam has no
   passwd db in the sandbox. */
void *dlopen(const char *f, int flag) { (void)f; (void)flag; return 0; }
void *dlsym(void *h, const char *s) { (void)h; (void)s; return 0; }
int dlclose(void *h) { (void)h; return 0; }
char *dlerror(void) { return 0; }
void *getpwnam(const char *n) { (void)n; return 0; }
/* XkbStdBell (audio bell) isn't compiled into our libX11 XKB; no audio in the sandbox. Stub it. */
int XkbStdBell(void *dpy, unsigned long w, int percent, unsigned long name) { (void)dpy;(void)w;(void)percent;(void)name; return 1; }
