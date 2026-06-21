/* Worker-trap behavior (WASM-THREADS-SPEC.md DoD §9.6): a worker thread traps. The runtime must NOT
 * hang the host; a worker trap corrupts shared memory, so the VM should fault. */
#include <stdio.h>
#include <pthread.h>
static void *worker(void *a){ (void)a; __builtin_trap(); return NULL; }
int main(void){
  pthread_t t;
  printf("M8-THREADS-TRAP: spawning trapping worker\n");
  pthread_create(&t,NULL,worker,NULL);
  pthread_join(t,NULL);
  printf("M8-THREADS-TRAP: main survived (worker trap did not fault VM)\n");
  return 0;
}
