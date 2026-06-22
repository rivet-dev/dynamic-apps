/* Isolate WHICH post-XOpenDisplay op spins under XInitThreads (single-threaded). */
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <stdio.h>
#define MARK(s) do{ fprintf(stderr,"XIT: " s "\n"); fflush(stderr);}while(0)
int main(void){
    MARK("XInitThreads"); XInitThreads();
    MARK("XOpenDisplay"); Display *d = XOpenDisplay(":0");
    if(!d){ MARK("XOpenDisplay FAILED"); return 1; }
    MARK("display open");
    MARK("XInternAtom (round-trip)"); Atom a = XInternAtom(d, "WM_PROTOCOLS", False);
    fprintf(stderr,"XIT: XInternAtom=%lu\n",(unsigned long)a); fflush(stderr);
    MARK("XNoOp"); XNoOp(d);
    MARK("XAllocID (xcb_generate_id)"); XID id = XAllocID(d);
    fprintf(stderr,"XIT: XAllocID=%lu\n",(unsigned long)id); fflush(stderr);
    MARK("XCreateGC"); GC gc = XCreateGC(d, DefaultRootWindow(d), 0, 0);
    fprintf(stderr,"XIT: XCreateGC gc=%p\n",(void*)gc); fflush(stderr);
    MARK("XSync"); XSync(d, False);
    MARK("ALL OK -> no hang");
    XCloseDisplay(d);
    return 0;
}
