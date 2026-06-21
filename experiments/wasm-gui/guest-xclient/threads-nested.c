/* Nested thread-spawn conformance (WASM-THREADS-SPEC.md DoD §9.1): a worker thread itself spawns
 * further worker threads (the shape GLib thread pools create). Proves thread-spawn works recursively
 * from a worker isolate, not just from the main isolate. */
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define FANOUT 4

static atomic_int leaves = 0;

static void *leaf(void *arg) {
    (void)arg;
    atomic_fetch_add_explicit(&leaves, 1, memory_order_relaxed);
    return NULL;
}

static void *branch(void *arg) {
    (void)arg;
    pthread_t t[FANOUT];
    int n = 0;
    for (int i = 0; i < FANOUT; i++) {
        if (pthread_create(&t[i], NULL, leaf, NULL) == 0) n++;
    }
    for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
    return (void *)(long)n;
}

int main(void) {
    pthread_t b;
    void *spawned = 0;
    if (pthread_create(&b, NULL, branch, NULL) != 0) {
        printf("M8-THREADS-NESTED: FAIL (branch spawn)\n");
        return 1;
    }
    pthread_join(b, &spawned);
    int n = (int)(long)spawned;
    int got = atomic_load(&leaves);
    printf("branch_spawned=%d leaves_ran=%d expected=%d\n", n, got, FANOUT);
    printf("M8-THREADS-NESTED: %s\n", (n == FANOUT && got == FANOUT) ? "PASS" : "FAIL");
    return 0;
}
