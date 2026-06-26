/* Only the identity/signal funcs NOT provided by libhostcompat or libwasi-emulated-signal.
 * Single-process identity (uid/gid 1000), no-op signals. Platform layer (constraint #5). */
#include <sys/types.h>
#include <stddef.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>
uid_t getuid(void){return 1000;} uid_t geteuid(void){return 1000;}
gid_t getgid(void){return 1000;} gid_t getegid(void){return 1000;}
pid_t getppid(void){return 1;}
int sethostname(const char*n,size_t l){(void)n;(void)l;return 0;}
char*getlogin(void){static char u[]="user";return u;}
int getlogin_r(char*b,size_t l){const char*u="user";if(b&&l){strncpy(b,u,l);b[l-1]=0;}return 0;}
static struct passwd g_pw={(char*)"user",(char*)"x",1000,1000,(char*)"User",(char*)"/root",(char*)"/bin/sh"};
int getpwnam_r(const char*n,struct passwd*p,char*b,size_t bl,struct passwd**r){(void)n;(void)b;(void)bl;if(p)*p=g_pw;if(r)*r=p;return 0;}
int initgroups(const char*u,gid_t g){(void)u;(void)g;return 0;}
int sigaction(int s,const struct sigaction*a,struct sigaction*o){(void)s;(void)a;(void)o;return 0;}
int sigemptyset(sigset_t*s){(void)s;return 0;}
int sigfillset(sigset_t*s){(void)s;return 0;}
int sigaddset(sigset_t*s,int n){(void)s;(void)n;return 0;}
int sigdelset(sigset_t*s,int n){(void)s;(void)n;return 0;}
int sigismember(const sigset_t*s,int n){(void)s;(void)n;return 0;}
int sigsuspend(const sigset_t*s){(void)s;return -1;}
int sigpending(sigset_t*s){(void)s;return 0;}
int killpg(int p,int s){(void)p;(void)s;return 0;}
int siginterrupt(int s,int f){(void)s;(void)f;return 0;}
