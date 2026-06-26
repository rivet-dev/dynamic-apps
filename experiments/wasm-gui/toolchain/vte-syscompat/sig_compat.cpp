/* VTE (C++) references kill/sigemptyset without an extern "C" decl (wasi lacks <signal.h> process bits),
 * so they were C++-name-mangled. Provide the mangled defs + getpid (absent from this wasi profile). Stubs. */
int kill(int, int) { return 0; }
int sigemptyset(unsigned char *) { return 0; }
int sigaddset(unsigned char *, int) { return 0; }
int sigprocmask(int, const void *, void *) { return 0; }
extern "C" { int getpid(void) { return 1; } }
