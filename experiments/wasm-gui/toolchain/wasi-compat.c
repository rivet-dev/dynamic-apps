/* No-op definitions for POSIX functions the patched wasi sysroot lacks (single-threaded, no process
   groups/signals). Linked into X-stack executables via the meson cross file. */
#include <stdio.h>
#include <sys/types.h>
/* Under the wasi-threads ABI (SECURE_EXEC_WASM_THREADS, Phase 0) these come from the real
   libc/libpthread and must NOT be redefined here, or the link hits duplicate symbols. */
#ifndef SECURE_EXEC_WASM_THREADS
void flockfile(FILE *f) { (void)f; }
void funlockfile(FILE *f) { (void)f; }
int ftrylockfile(FILE *f) { (void)f; return 0; }
#endif
int getpgrp(void) { return 1; }
int setpgid(int p, int g) { (void)p; (void)g; return 0; }
int setsid(void) { return 1; }              /* no process sessions on wasi (JWM/WMs call it once) */
void tzset(void) {}                          /* no timezone db on wasi; localtime stays UTC */
unsigned umask(unsigned m) { (void)m; return 0; }
/* pthread_sigmask: wasi has no signals, so a no-op is correct on both profiles. The threaded sysroot
 * neither defines nor (in libc) provides it, and GLib references it, so define it unconditionally.
 * (wasi-compat.c doesn't include <pthread.h>, so the void* signature can't clash with its prototype.) */
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
#ifndef SECURE_EXEC_WASM_THREADS
int pthread_create(void *t, const void *a, void *(*f)(void *), void *arg) { (void)t; (void)a; (void)f; (void)arg; return 1; }
int pthread_join(unsigned long t, void **r) { (void)t; (void)r; return 0; }
int pthread_attr_setscope(void *a, int s) { (void)a; (void)s; return 0; }
int pthread_setname_np(unsigned long t, const char *n) { (void)t; (void)n; return 0; }
#endif
#ifndef SECURE_EXEC_WASM_THREADS
void __SIG_IGN(int s) { (void)s; }  /* provided by libwasi-emulated-signal in the threaded profile */
#endif
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
/* weak: libwasi-emulated-signal provides a real strsignal; ours is only a fallback when that lib
 * isn't linked. Strong here caused a duplicate-symbol link error in guests (libfm) that pull both. */
__attribute__((weak)) char *strsignal(int s) { (void)s; return (char *)"signal"; }
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

/* DNS resolver (res_query family): wasi has no <resolv.h>, but GLib's GIO GResolver build requires
 * res_query() to link. Guest DNS in secure-exec goes through the kernel socket/DNS path, not libresolv,
 * so these are link-time stubs (GResolver's libresolv backend is unused at runtime). */
int res_query(const char *dname, int cls, int type, unsigned char *answer, int anslen) {
    (void)dname; (void)cls; (void)type; (void)answer; (void)anslen; return -1;
}
int res_search(const char *dname, int cls, int type, unsigned char *answer, int anslen) {
    (void)dname; (void)cls; (void)type; (void)answer; (void)anslen; return -1;
}
int res_init(void) { return 0; }
int dn_expand(const unsigned char *msg, const unsigned char *eom, const unsigned char *src,
              char *dst, int dstsiz) {
    (void)msg; (void)eom; (void)src; (void)dst; (void)dstsiz; return -1;
}
int dn_comp(const char *src, unsigned char *dst, int dstsiz, unsigned char **dnptrs,
            unsigned char **lastdnptr) {
    (void)src; (void)dst; (void)dstsiz; (void)dnptrs; (void)lastdnptr; return -1;
}

/* BSD socket-creation API: wasi-libc has no socket()/socketpair() (guests do network I/O through
 * secure-exec's host_net path, not libc sockets), but GLib's GIO GSocket build requires these symbols
 * to link. WEAK link-time stubs (so they never clash if a future wasi-libc provides real ones); GSocket
 * is unused at runtime in the sandbox. errno EAFNOSUPPORT-ish via -1. */
__attribute__((weak)) int socket(int domain, int type, int protocol) {
    (void)domain; (void)type; (void)protocol; return -1;
}
__attribute__((weak)) int socketpair(int domain, int type, int protocol, int sv[2]) {
    (void)domain; (void)type; (void)protocol; (void)sv; return -1;
}
__attribute__((weak)) unsigned if_nametoindex(const char *ifname) { (void)ifname; return 0; }
__attribute__((weak)) char *if_indextoname(unsigned ifindex, char *ifname) {
    (void)ifindex; (void)ifname; return 0;
}
__attribute__((weak)) int getnameinfo(const void *sa, unsigned salen, char *host, unsigned hostlen,
                                       char *serv, unsigned servlen, int flags) {
    (void)sa; (void)salen; (void)host; (void)hostlen; (void)serv; (void)servlen; (void)flags; return -1;
}

/* h_errno: GLib's GIO threaded resolver references this libresolv global; wasi-libc omits it. */
int h_errno = 0;

/* Process primitives wasi-libc omits: GLib builds helper executables (gio-launch-desktop, gtester)
 * that reference getpid()/kill()/getppid(). The sandbox is single-process; stub them. */
__attribute__((weak)) int getpid(void) { return 1; }
__attribute__((weak)) int getppid(void) { return 0; }
__attribute__((weak)) int kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
__attribute__((weak)) int killpg(int pgrp, int sig) { (void)pgrp; (void)sig; return -1; }
__attribute__((weak)) int waitpid(int pid, int *status, int options) {
    (void)pid; (void)status; (void)options; return -1;
}

/* POSIX signal API: wasi has no signals. GLib's gmain.c (child-watch / unix-signal sources) references
 * these; stub them (sets/masks are no-ops, sigaction/sigsuspend fail). Pointer args as void* — symbol
 * names are what the linker needs; arg counts match the real prototypes (all ptrs are i32 on wasm32). */
__attribute__((weak)) int sigemptyset(void *set) { (void)set; return 0; }
__attribute__((weak)) int sigfillset(void *set) { (void)set; return 0; }
__attribute__((weak)) int sigaction(int signo, const void *act, void *oact) {
    (void)signo; (void)act; (void)oact; return -1;
}

/* User/identity + fd-dup + name-resolution functions wasi-libc omits but GLib/GIO reference (and the
 * final GTK app link will need too). Single-user sandbox -> fixed ids; getpw lookups return empty. */
__attribute__((weak)) unsigned getuid(void) { return 0; }
__attribute__((weak)) unsigned geteuid(void) { return 0; }
__attribute__((weak)) unsigned getgid(void) { return 0; }
__attribute__((weak)) unsigned getegid(void) { return 0; }
__attribute__((weak)) int dup(int fd) { (void)fd; return -1; }
__attribute__((weak)) int dup2(int a, int b) { (void)a; (void)b; return -1; }
__attribute__((weak)) void *getpwuid(unsigned uid) { (void)uid; return 0; }
__attribute__((weak)) int getpwuid_r(unsigned uid, void *pwd, char *buf, unsigned long buflen, void **result) {
    (void)uid; (void)pwd; (void)buf; (void)buflen; if (result) *result = 0; return 0;
}
__attribute__((weak)) int getpwnam_r(const char *name, void *pwd, char *buf, unsigned long buflen, void **result) {
    (void)name; (void)pwd; (void)buf; (void)buflen; if (result) *result = 0; return 0;
}

/* Misc symbols GTK/GDK reference that wasi-libc + our libs lack (stubs; unused in the sandbox). */
__attribute__((weak)) int chown(const char *p, unsigned u, unsigned g) { (void)p;(void)u;(void)g; return 0; }
__attribute__((weak)) int pthread_attr_setinheritsched(void *attr, int inherit) { (void)attr;(void)inherit; return 0; }

/* NOTE: GLib's worker thread (g_system_thread_new -> pthread_create) is the fundamental M8 runtime
 * blocker on wasm32-wasip1 (no threads). A fake "success without running" pthread_create override was
 * explored and rejected: it fights <pthread.h>'s declarations here, and GLib's architecture assumes a
 * real preemptive worker thread (downstream code waits on it), so a no-op thread would deadlock rather
 * than render. The real fix is the wasm32-wasip1-threads target + wasi_thread_spawn in the runtime
 * (see M8-FINDINGS.md). pthread_create is left to libwasi-emulated-pthread (which fails, as expected). */
__attribute__((weak)) int raise(int sig) { (void)sig; return -1; }
__attribute__((weak)) long recvmsg(int fd, void *msg, int flags) { (void)fd;(void)msg;(void)flags; return -1; }
__attribute__((weak)) long sendmsg(int fd, const void *msg, int flags) { (void)fd;(void)msg;(void)flags; return -1; }
__attribute__((weak)) int getservbyname_r(const char *n, const char *p, void *r, char *b, unsigned long bl, void **res) {
    (void)n;(void)p;(void)r;(void)b;(void)bl; if (res) *res = 0; return -1;
}
/* NOTE: gethostbyname + inet_addr/inet_aton/inet_ntoa are already defined earlier in this file
 * (the X-server / libXfont2 transports referenced them). Their compat header decls live in
 * compat-include/{netdb.h,arpa/inet.h}; do not redefine them here. */
/* libepoxy GLX entrypoints: our libepoxy is built without GLX (no GL in the sandbox). GTK's GL paths
 * query these and fall back to cairo software rendering when GLX is absent. */
__attribute__((weak)) int epoxy_glx_version(void *dpy, int screen) { (void)dpy;(void)screen; return 0; }
__attribute__((weak)) int epoxy_has_glx(void *dpy) { (void)dpy; return 0; }
__attribute__((weak)) int epoxy_has_glx_extension(void *dpy, int screen, const char *ext) { (void)dpy;(void)screen;(void)ext; return 0; }

/* SysV shared memory: wasi has none. Cairo's Xlib XShm backend references these; all fail so cairo
 * falls back to plain XPutImage (no shared memory between guests in the sandbox anyway). */
__attribute__((weak)) int shmget(int key, unsigned long size, int flag) { (void)key; (void)size; (void)flag; return -1; }
__attribute__((weak)) void *shmat(int id, const void *addr, int flag) { (void)id; (void)addr; (void)flag; return (void *) -1; }
__attribute__((weak)) int shmdt(const void *addr) { (void)addr; return -1; }
__attribute__((weak)) int shmctl(int id, int cmd, void *buf) { (void)id; (void)cmd; (void)buf; return -1; }

/* pipe()/pipe2(): wasi-libc omits them (no host pipe syscall). Back them with the runtime's
 * host_process.fd_pipe import, which creates a REAL kernel pipe shared across this VM's threads (its
 * write notifies the kernel poll, so a cross-thread write wakes a blocked poll). The runner
 * range-encodes the returned fds. This is what makes glib's stock GWakeup work, so g_main_context
 * cross-thread dispatch (libfm/pcmanfm async jobs) wakes the GTK main loop. The O_NONBLOCK/O_CLOEXEC
 * pipe2 flags are no-ops here: the kernel-pipe read is already non-blocking and there is no exec. */
__attribute__((import_module("host_process"), import_name("fd_pipe")))
extern int __secure_exec_host_fd_pipe(int *read_fd, int *write_fd);

__attribute__((weak)) int pipe(int fds[2]) {
    int r = -1, w = -1;
    if (__secure_exec_host_fd_pipe(&r, &w) != 0 || r < 0 || w < 0) {
        return -1;
    }
    fds[0] = r;
    fds[1] = w;
    return 0;
}
__attribute__((weak)) int pipe2(int fds[2], int flags) { (void)flags; return pipe(fds); }
