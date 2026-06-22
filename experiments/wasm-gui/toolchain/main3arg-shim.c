/* secure-exec: apps with the GNU 3-arg main(argc,argv,envp) (e.g. lxpanel) don't connect to the wasi
 * crt, which references `main` with the 2-arg wasm type (i32,i32)->i32; the 3-arg def's type
 * (i32,i32,i32)->i32 doesn't match, so `main` stays undefined-weak and calling it traps. Override the
 * crt's weak __main_void with one that fetches argv from WASI and calls the real 3-arg main directly
 * (the call type then matches the definition). Link this object for 3-arg-main guests. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wasi/api.h>
extern char **environ;
extern int main(int argc, char **argv, char **envp);
int __main_void(void) {
    size_t argc = 0, bufsz = 0;
    __wasi_args_sizes_get(&argc, &bufsz);
    char **argv = (char **)calloc(argc + 1, sizeof(char *));
    char *buf = (char *)malloc(bufsz ? bufsz : 1);
    if (argv && buf) __wasi_args_get((uint8_t **)argv, (uint8_t *)buf);
    return main((int)argc, argv, environ);
}
