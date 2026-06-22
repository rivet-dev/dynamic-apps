/* WASM-THREADS-SPEC §10 DoD: contended pthread_mutex must block-and-yield (park), not busy-spin.
 * Producer holds the lock across a real (host) sleep; consumers MUST park (state S) while waiting,
 * then all complete with the correct count (no starvation/livelock). */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static long counter = 0;
static void *consumer(void *a){
    for(int i=0;i<5;i++){
        pthread_mutex_lock(&m);     /* contends with producer holding it across usleep -> must PARK */
        counter++;
        pthread_mutex_unlock(&m);
        usleep(1000);
    }
    fprintf(stderr,"CONTEND: consumer %s done\n",(char*)a);fflush(stderr);
    return 0;
}
int main(void){
    fprintf(stderr,"CONTEND: start\n");fflush(stderr);
    pthread_mutex_lock(&m);          /* hold the lock across a long sleep so consumers must park */
    pthread_t c1,c2;
    pthread_create(&c1,0,consumer,(void*)"c1");
    pthread_create(&c2,0,consumer,(void*)"c2");
    fprintf(stderr,"CONTEND: leader holding lock 3s (consumers must PARK, state S, not spin R)\n");fflush(stderr);
    usleep(3000000);
    counter += 100;
    pthread_mutex_unlock(&m);        /* release -> consumers wake and proceed */
    pthread_join(c1,0); pthread_join(c2,0);
    fprintf(stderr,"CONTEND: done counter=%ld (expected 110)\n", counter);fflush(stderr);
    return counter==110?0:1;
}
