/* Decisive: does memory.atomic.wait PARK (state S) or SPIN (state R) on the LEADER (main) isolate?
 * Leader waits on an address that is never notified, 6s timeout. A worker exists (parks immediately). */
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
static _Atomic int futex = 0;
static _Atomic int worker_waiting = 0;
static void *worker(void *a){(void)a;
    atomic_store(&worker_waiting,1);
    /* worker also parks on its own never-notified address */
    static _Atomic int wf=0;
    __builtin_wasm_memory_atomic_wait32(&wf, 0, (int64_t)5e9);
    return 0;
}
int main(void){
    fprintf(stderr,"AW: leader start\n");fflush(stderr);
    pthread_t t; pthread_create(&t,0,worker,0);
    while(!atomic_load(&worker_waiting));
    fprintf(stderr,"AW: leader entering atomic.wait (6s, never notified)\n");fflush(stderr);
    long r = __builtin_wasm_memory_atomic_wait32((int*)&futex, 0, (int64_t)6e9);
    fprintf(stderr,"AW: leader atomic.wait returned %ld (2=timeout)\n", r);fflush(stderr);
    return 0;
}
