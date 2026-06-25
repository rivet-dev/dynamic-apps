/* Measure async X EVENT-delivery latency: time from XMapWindow to the MapNotify event. The XSync
 * round-trips are fast (~5ms); if THIS is ~seconds, GTK's show_all is slow because the X server's
 * pushed events reach the guest with high latency (the event-poll path), not the request/reply path. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>
static double now_ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0 + t.tv_nsec/1e6; }
int main(void){
  Display *d=XOpenDisplay(NULL); if(!d){fprintf(stderr,"MAP-BENCH: no display\n");return 1;}
  Window root=DefaultRootWindow(d);
  Window w=XCreateSimpleWindow(d,root,0,0,120,80,0,0,0xffffff);
  XSelectInput(d,w,StructureNotifyMask|ExposureMask);
  fprintf(stderr,"MAP-BENCH: XMapWindow\n");
  double t0=now_ms(); XMapWindow(d,w); XFlush(d);
  XEvent e; int got_map=0, got_expose=0;
  while(!(got_map&&got_expose)){
    XNextEvent(d,&e);
    if(e.type==MapNotify && !got_map){ got_map=1; fprintf(stderr,"MAP-BENCH: MapNotify after %.1fms\n", now_ms()-t0); }
    if(e.type==Expose && !got_expose){ got_expose=1; fprintf(stderr,"MAP-BENCH: Expose after %.1fms\n", now_ms()-t0); }
  }
  return 0;
}
