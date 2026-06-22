#include <stdio.h>
#include <pthread.h>
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(void){
    m("PP:start\n");
    pthread_mutex_t mtx; int r1 = pthread_mutex_init(&mtx, NULL);
    fprintf(stderr,"PP:mutex_init r=%d\n", r1); fflush(stderr);
    int r2 = pthread_mutex_lock(&mtx);
    fprintf(stderr,"PP:lock r=%d\n", r2); fflush(stderr);
    int r3 = pthread_mutex_unlock(&mtx);
    fprintf(stderr,"PP:unlock r=%d\n", r3); fflush(stderr);
    pthread_cond_t cnd; int r4 = pthread_cond_init(&cnd, NULL);
    fprintf(stderr,"PP:cond_init r=%d\n", r4); fflush(stderr);
    pthread_key_t key; int r5 = pthread_key_create(&key, NULL);
    fprintf(stderr,"PP:key_create r=%d\n", r5); fflush(stderr);
    m("PP:done\n");
    return 0;
}
