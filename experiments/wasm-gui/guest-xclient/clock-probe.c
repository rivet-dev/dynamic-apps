/* Does CLOCK_MONOTONIC advance in the threaded wasm runtime? If it returns a constant, GLib timeout
 * sources always read "expired" -> g_main_context busy-spins (the GLib worker 150% R livelock). */
#include <stdio.h>
#include <time.h>
#include <stdint.h>
static int64_t now_us(clockid_t c){ struct timespec ts; clock_gettime(c,&ts); return (int64_t)ts.tv_sec*1000000 + ts.tv_nsec/1000; }
int main(void){
    for(int i=0;i<5;i++){
        int64_t m = now_us(CLOCK_MONOTONIC);
        int64_t r = now_us(CLOCK_REALTIME);
        fprintf(stderr,"CLK[%d]: MONOTONIC=%lld REALTIME=%lld\n", i, (long long)m, (long long)r); fflush(stderr);
        for(volatile long k=0;k<50000000L;k++); /* busy work so wall-clock advances */
    }
    fprintf(stderr,"CLK: done\n"); fflush(stderr);
    return 0;
}
