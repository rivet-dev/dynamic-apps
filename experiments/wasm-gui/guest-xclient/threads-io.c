/* Worker-thread KERNEL I/O proof (WASM-THREADS-SPEC.md DoD §9.3): a worker thread makes a real host
 * call (write to stdout) — only possible if the worker session's host imports route to the kernel
 * (the sidecar-mediated worker path, sharing the parent's process fd table). The main thread waits on
 * a shared flag the worker sets after writing, then confirms. */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

static atomic_int wrote = 0;

static void *worker(void *arg) {
    (void)arg;
    const char *msg = "WORKER_STDOUT_OK\n";
    /* fd_write to stdout (fd 1) — a real kernel host call from the worker thread. */
    ssize_t n = write(1, msg, strlen(msg));
    if (n == (ssize_t)strlen(msg)) {
        atomic_store(&wrote, 1);
    }
    return NULL;
}

int main(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) {
        printf("M8-THREADS-IO: FAIL (spawn)\n");
        return 1;
    }
    pthread_join(t, NULL);
    printf("M8-THREADS-IO: %s\n", atomic_load(&wrote) ? "PASS" : "FAIL");
    return 0;
}
