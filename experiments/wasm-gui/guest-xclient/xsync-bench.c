/* Measure the X round-trip latency directly: each XSync(d,False) is one guest->X-server->guest
 * round-trip. If each is ~ms it is fine; if ~tens-of-ms, X round-trips are the show_all bottleneck. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>
static double now_ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0 + t.tv_nsec/1e6; }
int main(void){
  Display *d = XOpenDisplay(NULL);
  if(!d){ fprintf(stderr,"XSYNC-BENCH: no display\n"); return 1; }
  fprintf(stderr,"XSYNC-BENCH: connected\n");
  for(int i=0;i<3;i++) XSync(d,False); /* warm up */
  int N=100; double t0=now_ms();
  for(int i=0;i<N;i++) XSync(d,False);
  double dt=now_ms()-t0;
  fprintf(stderr,"XSYNC-BENCH: %d round-trips = %.1fms total, %.3fms each\n", N, dt, dt/N);
  return 0;
}
