/* Stress + lifecycle (WASM-THREADS-SPEC.md DoD §9.4): many spawn/join cycles in ONE guest run. Also
 * validates slot reclamation — ROUNDS*PER (>cap) spawns only succeed if each worker's slot is freed on
 * exit. Each worker does an atomic increment; the final count must equal ROUNDS*PER exactly. */
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define ROUNDS 50
#define PER 4

static atomic_int counter = 0;

static void *worker(void *arg) {
    (void)arg;
    atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
    return NULL;
}

int main(void) {
    for (int r = 0; r < ROUNDS; r++) {
        pthread_t t[PER];
        int n = 0;
        for (int i = 0; i < PER; i++) {
            if (pthread_create(&t[i], NULL, worker, NULL) != 0) {
                printf("M8-THREADS-STRESS: FAIL (spawn round=%d)\n", r);
                return 1;
            }
            n++;
        }
        for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
    }
    int c = atomic_load(&counter);
    int expected = ROUNDS * PER;
    printf("counter=%d expected=%d\n", c, expected);
    printf("M8-THREADS-STRESS: %s\n", c == expected ? "PASS" : "FAIL");
    return 0;
}
