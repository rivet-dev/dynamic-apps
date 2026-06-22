/* Single-threaded XInitThreads client doing MANY round-trips (the gtk_init pattern) to reproduce the
 * heisenbug round-trip hang cheaply, without GTK. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#define MARK(s) do{ fprintf(stderr,"RT: " s "\n"); fflush(stderr);}while(0)
int main(void){
    XInitThreads();
    Display *d = XOpenDisplay(":0");
    if(!d){ MARK("XOpenDisplay FAILED"); return 1; }
    MARK("display open");
    Window root = DefaultRootWindow(d);
    char nm[32];
    for(int i=0;i<200;i++){
        snprintf(nm,sizeof nm,"_ATOM_%d",i);
        Atom a = XInternAtom(d, nm, False);            /* round-trip */
        if(i%50==0){ fprintf(stderr,"RT: XInternAtom[%d]=%lu\n",i,(unsigned long)a); fflush(stderr); }
    }
    MARK("200 XInternAtom done; XQueryPointer x50");
    for(int i=0;i<50;i++){
        Window r,c; int rx,ry,wx,wy; unsigned m;
        XQueryPointer(d, root, &r,&c,&rx,&ry,&wx,&wy,&m);  /* round-trip */
        XSync(d, False);                                    /* round-trip (error_trap_pop uses sync) */
    }
    MARK("XQueryPointer+XSync x50 done; XListExtensions");
    int n=0; char **ext = XListExtensions(d, &n);            /* round-trip */
    fprintf(stderr,"RT: %d extensions\n", n); fflush(stderr);
    if(ext) XFreeExtensionList(ext);
    int op,ev,er;
    for(int i=0;i<20;i++){
        XQueryExtension(d, "RANDR", &op,&ev,&er);            /* round-trip x20 */
        XQueryExtension(d, "XFIXES", &op,&ev,&er);
    }
    MARK("XQueryExtension x40 done");
    XSync(d, False);
    MARK("ALL ROUND-TRIPS OK -> no hang");
    XCloseDisplay(d);
    return 0;
}
