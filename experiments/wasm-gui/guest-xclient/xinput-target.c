/* A libX11 client that PROVES input delivery end to end. It starts blue, and repaints its window a
 * different colour for each input event class it receives: green on KeyPress, orange on ButtonPress.
 * Combined with the XTEST agent (xtest-agent.c) injecting synthetic input, the captured framebuffer
 * turning green/orange proves host-driven input reaches a real toolkit client through the wasm X
 * server. Event-driven: blocks in XNextEvent, repaints on Expose and on each input event. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void mark(const char *s) { write(2, s, strlen(s)); }

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { mark("XI:no-display\n"); return 1; }
    mark("XI:opened\n");

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    unsigned long blue = 0x3060C0;
    Window win = XCreateSimpleWindow(dpy, root, 40, 40, 250, 160, 2,
                                     BlackPixel(dpy, screen), blue);
    XStoreName(dpy, win, "secure-exec input target");

    /* Ask the WM to honour our position, and listen for exposure + input. */
    XSizeHints hints; memset(&hints, 0, sizeof(hints));
    hints.flags = PPosition; hints.x = 40; hints.y = 40;
    XSetWMNormalHints(dpy, win, &hints);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(dpy, win);
    XFlush(dpy);

    GC gc = XCreateGC(dpy, win, 0, NULL);
    unsigned long cur = blue;
    mark("XI:mapped\n");

    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case Expose:
                XSetForeground(dpy, gc, cur);
                XFillRectangle(dpy, win, gc, 0, 0, 250, 160);
                XFlush(dpy);
                break;
            case KeyPress:
                cur = 0x20A020;   /* green */
                XSetForeground(dpy, gc, cur);
                XFillRectangle(dpy, win, gc, 0, 0, 250, 160);
                XFlush(dpy);
                mark("XI:key\n");
                break;
            case ButtonPress:
                cur = 0xE08020;   /* orange */
                XSetForeground(dpy, gc, cur);
                XFillRectangle(dpy, win, gc, 0, 0, 250, 160);
                XFlush(dpy);
                mark("XI:button\n");
                break;
            default:
                break;
        }
    }
    return 0;
}
