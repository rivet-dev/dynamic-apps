/* secure-exec platform stubs: functions openbox references that the single-user/no-DNS sandbox
 * does not provide. Weak so a real impl (if ever linked) wins. Faithful semantics: no groups,
 * no alarm timer, no reverse-DNS in the sandbox. */
#include <stddef.h>
__attribute__((weak)) unsigned int alarm(unsigned int s) { (void)s; return 0; }
__attribute__((weak)) void *getgrent(void) { return NULL; }
__attribute__((weak)) void endgrent(void) {}
__attribute__((weak)) void setgrent(void) {}
__attribute__((weak)) void *gethostbyaddr(const void *a, unsigned int l, int t) { (void)a;(void)l;(void)t; return NULL; }
/* threaded wasi libc omits pthread_exit; openbox references it via libSM/libICE cleanup but is
 * single-threaded for the WM loop, so terminating the process is the correct main-thread semantics. */
extern void exit(int);
__attribute__((weak)) _Noreturn void pthread_exit(void *r) { (void)r; exit(0); }

/* The sandbox has no /etc/passwd, but every running uid on real Linux resolves to a passwd entry.
 * wasi-compat's weak getpwuid returns NULL, which makes unmodified apps that deref pw->pw_name
 * (e.g. openbox find_uid_gid, glib g_get_home_dir) crash. Provide a valid stub identity instead
 * (strong overrides the weak stub; faithful "this uid exists" semantics). */
#include <pwd.h>
#include <unistd.h>
#include <string.h>
static struct passwd se_pw;
static struct passwd *se_fill_pw(void) {
    se_pw.pw_name = (char *)"user";
    se_pw.pw_passwd = (char *)"x";
    se_pw.pw_uid = getuid();
    se_pw.pw_gid = getgid();
    se_pw.pw_gecos = (char *)"user";
    se_pw.pw_dir = (char *)"/";
    se_pw.pw_shell = (char *)"/bin/sh";
    return &se_pw;
}
struct passwd *getpwuid(uid_t uid) { (void)uid; return se_fill_pw(); }
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    (void)uid;
    if (buflen < 32) { *result = 0; return 34 /*ERANGE*/; }
    strcpy(buf, "user");
    pwd->pw_name = buf; pwd->pw_passwd = (char *)"x";
    pwd->pw_uid = getuid(); pwd->pw_gid = getgid();
    pwd->pw_gecos = buf; pwd->pw_dir = (char *)"/"; pwd->pw_shell = (char *)"/bin/sh";
    *result = pwd;
    return 0;
}
