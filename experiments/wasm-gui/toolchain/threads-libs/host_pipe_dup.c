// Pipe and FD duplication via wasmVM host_process imports.
//
// Replaces wasi-libc's ENOSYS stubs with calls to our custom WASM imports:
//   host_process.fd_pipe  -> pipe()
//   host_process.fd_dup   -> dup()
//   host_process.fd_dup2  -> dup2()
//
// Import signatures match wasmvm/crates/wasi-ext/src/lib.rs exactly.

#include <__errno.h>
#include <stdint.h>
#include <unistd.h>

#define WASM_IMPORT(mod, fn) \
    __attribute__((__import_module__(mod), __import_name__(fn)))

WASM_IMPORT("host_process", "fd_pipe")
uint32_t __host_fd_pipe(uint32_t *ret_read_fd, uint32_t *ret_write_fd);

WASM_IMPORT("host_process", "fd_dup")
uint32_t __host_fd_dup(uint32_t fd, uint32_t *ret_new_fd);

WASM_IMPORT("host_process", "fd_dup2")
uint32_t __host_fd_dup2(uint32_t old_fd, uint32_t new_fd);

int pipe(int fd[2]) {
    uint32_t r, w;
    uint32_t err = __host_fd_pipe(&r, &w);
    if (err != 0) {
        errno = (int)err;
        return -1;
    }
    fd[0] = (int)r;
    fd[1] = (int)w;
    return 0;
}

int dup(int fd) {
    uint32_t new_fd;
    uint32_t err = __host_fd_dup((uint32_t)fd, &new_fd);
    if (err != 0) {
        errno = (int)err;
        return -1;
    }
    return (int)new_fd;
}

int dup2(int old, int new) {
    uint32_t err = __host_fd_dup2((uint32_t)old, (uint32_t)new);
    if (err != 0) {
        errno = (int)err;
        return -1;
    }
    return new;
}
