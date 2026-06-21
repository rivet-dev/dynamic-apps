/* Minimal wasm32-wasip1-threads test: spawn a thread, have it set a flag, join it. Proves the runtime
 * supports wasi_thread_spawn (multi-threaded wasm guests) — the M8 prerequisite (GLib's worker thread). */
#include <stdio.h>
#include <pthread.h>
static int g_ran = 0;
static void *worker(void *arg) { (void)arg; g_ran = 42; return (void*)123; }
int main(void) {
    pthread_t t; void *ret = 0;
    int rc = pthread_create(&t, NULL, worker, NULL);
    printf("pthread_create rc=%d\n", rc);
    if (rc == 0) { pthread_join(t, &ret); printf("joined; g_ran=%d ret=%ld\n", g_ran, (long)ret); }
    printf("M8-THREADS: %s\n", (rc==0 && g_ran==42) ? "PASS" : "FAIL");
    return 0;
}
