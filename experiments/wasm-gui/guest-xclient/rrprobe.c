#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(void){
    m("RR:start\n");
    Display *dpy = XOpenDisplay(":0"); if(!dpy){m("RR:open_fail\n");return 1;}
    m("RR:opened\n");
    int eb, errb;
    int have = XRRQueryExtension(dpy, &eb, &errb);
    fprintf(stderr,"RR:QueryExtension have=%d eventbase=%d\n", have, eb); fflush(stderr);
    if(have){
        int major=0,minor=0; XRRQueryVersion(dpy,&major,&minor);
        fprintf(stderr,"RR:version %d.%d\n",major,minor); fflush(stderr);
        m("RR:before SelectInput\n");
        XRRSelectInput(dpy, RootWindow(dpy,DefaultScreen(dpy)),
            RRScreenChangeNotifyMask|RRCrtcChangeNotifyMask|RROutputPropertyNotifyMask);
        XSync(dpy, False);
        m("RR:after SelectInput\n");
        m("RR:before GetScreenResourcesCurrent\n");
        XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, RootWindow(dpy,DefaultScreen(dpy)));
        fprintf(stderr,"RR:resources=%p ncrtc=%d noutput=%d\n",(void*)res, res?res->ncrtc:-1, res?res->noutput:-1); fflush(stderr);
        m("RR:before GetMonitors\n");
        int nmon=0; XRRMonitorInfo *mons = XRRGetMonitors(dpy, RootWindow(dpy,DefaultScreen(dpy)), True, &nmon);
        fprintf(stderr,"RR:monitors=%p nmon=%d\n",(void*)mons,nmon); fflush(stderr);
    }
    m("RR:done\n");
    return 0;
}
