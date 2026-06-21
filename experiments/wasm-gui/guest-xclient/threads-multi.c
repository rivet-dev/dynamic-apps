/* Multi-thread conformance/stress for wasi-threads (WASM-THREADS-SPEC.md DoD §9.1/§9.4): spawn N
 * worker threads concurrently, each doing many ATOMIC increments on a shared counter in shared linear
 * memory, then join them all and verify the total. Exercises: concurrent worker-isolate spawns, unique
 * tids, cross-isolate wasm atomics on one shared memory, and that every join completes. */
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define N_THREADS 8
#define ITERS 2000

static atomic_int counter = 0;

static void *worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[N_THREADS];
    int spawned = 0;
    for (int i = 0; i < N_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL) != 0) {
            printf("pthread_create failed at %d\n", i);
            break;
        }
        spawned++;
    }
    for (int i = 0; i < spawned; i++) {
        pthread_join(threads[i], NULL);
    }
    int total = atomic_load(&counter);
    int expected = N_THREADS * ITERS;
    printf("spawned=%d counter=%d expected=%d\n", spawned, total, expected);
    printf("M8-THREADS-MULTI: %s\n",
           (spawned == N_THREADS && total == expected) ? "PASS" : "FAIL");
    return 0;
}
