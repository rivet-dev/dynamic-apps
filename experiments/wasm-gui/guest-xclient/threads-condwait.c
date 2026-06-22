/* Does pthread_cond_wait PARK (atomic.wait, state S) or busy-spin (state R) on the LEADER thread?
 * The leader cond_waits ~4s for a worker that signals late. Check /proc/<tid> state while it waits. */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  c = PTHREAD_COND_INITIALIZER;
static int ready = 0;
static void *signaller(void *a){
    (void)a;
    fprintf(stderr,"COND: worker sleeping 4s before signal\n");fflush(stderr);
    for(volatile long i=0;i<400000000L;i++);   /* ~busy delay, no host calls */
    pthread_mutex_lock(&m); ready=1; pthread_cond_signal(&c); pthread_mutex_unlock(&m);
    fprintf(stderr,"COND: worker signalled\n");fflush(stderr);
    return 0;
}
int main(void){
    fprintf(stderr,"COND: leader start\n");fflush(stderr);
    pthread_t t; pthread_create(&t,0,signaller,0);
    pthread_mutex_lock(&m);
    fprintf(stderr,"COND: leader entering cond_wait\n");fflush(stderr);
    while(!ready) pthread_cond_wait(&c,&m);   /* if this SPINS on the leader -> R state */
    pthread_mutex_unlock(&m);
    fprintf(stderr,"COND: leader woke (ready=%d)\n", ready);fflush(stderr);
    pthread_join(t,0);
    fprintf(stderr,"COND: done\n");fflush(stderr);
    return 0;
}
