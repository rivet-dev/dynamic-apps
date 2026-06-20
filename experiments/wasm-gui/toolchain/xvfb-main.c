#include <unistd.h>
extern int dix_main(int argc, char **argv, char **envp);
extern char **environ;
__attribute__((visibility("default")))
int __main_argc_argv(int argc, char **argv) {
    write(2, "XMARK:entry\n", 12);
    int rc = dix_main(argc, argv, environ);
    write(2, "XMARK:dix_main_returned\n", 24);
    return rc;
}
