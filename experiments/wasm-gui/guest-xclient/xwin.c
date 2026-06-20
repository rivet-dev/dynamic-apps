/* A real libX11 client that creates a top-level window, names it (so a window manager draws a
 * title bar), maps it, and draws content. Run alongside twm to prove the WM decorates a managed
 * client window. Stays alive in an event loop so the framebuffer can be captured. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <string.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

int main(int argc, char **argv) {
    mark("XW:start\n");
    /* The host launches clients in sequence (WM first), so we just connect normally. */
    Display *dpy = XOpenDisplay(":0");
    if (!dpy) { mark("XW:open_failed\n"); return 1; }
    mark("XW:opened\n");

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    unsigned long bg = 0x3060C0;   /* blue-ish window body */
    unsigned long fg = 0xF0F0F0;

    Window win = XCreateSimpleWindow(dpy, root, 40, 60, 250, 160, 1,
                                     BlackPixel(dpy, scr), bg);
    XStoreName(dpy, win, "secure-exec wasm window");
    /* Tell the WM to honor our position (twm UsePPosition) so windows don't overlap. */
    {
        XSizeHints hints;
        hints.flags = PPosition | PSize;
        hints.x = 40; hints.y = 60; hints.width = 250; hints.height = 160;
        XSetWMNormalHints(dpy, win, &hints);
    }
    XSelectInput(dpy, win, ExposureMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    mark("XW:mapped\n");

    GC gc = XCreateGC(dpy, win, 0, NULL);

    /* Proper event-driven X loop: block in XNextEvent and (re)draw only on Expose. A real X client
     * is quiet when idle — busy-redrawing would flood the shared sync-RPC bridge and starve the
     * window manager's request handling. We never close the display, so the window persists. */
    int drawn = 0;
    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);   /* blocks until an event (e.g. Expose from the WM mapping us) */
        if (ev.type == Expose) {
            XSetForeground(dpy, gc, fg);
            XFillRectangle(dpy, win, gc, 15, 15, 90, 45);
            XSetForeground(dpy, gc, 0x20A020);
            XFillRectangle(dpy, win, gc, 120, 80, 90, 50);
            XFlush(dpy);
            if (!drawn) { mark("XW:drawn\n"); drawn = 1; }
        }
    }
    return 0;
}
