/* Minimal repro of the root bug (WASM-THREADS-SPEC §0a): two worker threads + the leader heavily
 * contend ONE pthread_mutex. On a correct runtime the loser parks in atomic.wait (S, 0% CPU) and this
 * finishes fast. If contended lock acquisition busy-spins, it livelocks (R, 100% CPU). */
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static volatile long counter = 0;
static const long ITERS = 300000;
static void *worker(void *a){
    const char *who = (const char*)a;
    for(long i=0;i<ITERS;i++){
        pthread_mutex_lock(&m);
        counter++;
        /* a little work under the lock to force real contention */
        for(volatile int k=0;k<20;k++);
        pthread_mutex_unlock(&m);
    }
    fprintf(stderr,"CONTEND: %s done\n", who); fflush(stderr);
    return 0;
}
int main(void){
    fprintf(stderr,"CONTEND: start (3 threads x %ld each)\n", ITERS); fflush(stderr);
    pthread_t t1,t2;
    pthread_create(&t1,0,worker,(void*)"w1");
    pthread_create(&t2,0,worker,(void*)"w2");
    worker((void*)"leader");
    pthread_join(t1,0);
    pthread_join(t2,0);
    fprintf(stderr,"CONTEND: ALL done counter=%ld (expected %ld)\n", counter, ITERS*3); fflush(stderr);
    return counter==ITERS*3 ? 0 : 1;
}
