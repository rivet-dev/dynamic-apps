/* Decisive diagnostic: xpoll-target (works: green on KeyPress via the non-blocking poll loop) PLUS
 * st's Xft font init (XftFontOpenName + FcInit-equivalent via Xft). If THIS loses KeyPress while
 * xpoll-target keeps it, st's Xft/fontconfig libxcb init is what breaks device-event surfacing for
 * st's connection. Built with Xft linkage (see build command in the test). */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void mark(const char *s) { (void) write(2, s, strlen(s)); }

int main(void) {
    /* st's main() does this before opening the display; test if locale/IM modifiers break key input. */
    setlocale(LC_CTYPE, "");
    Display *dpy = XOpenDisplay(NULL);
    XSetLocaleModifiers("");
    if (!dpy) { mark("XF:no-display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    unsigned long blue = 0x3060C0;
    Window win = XCreateSimpleWindow(dpy, root, 40, 40, 250, 160, 2,
                                     BlackPixel(dpy, screen), blue);
    XStoreName(dpy, win, "secure-exec xft poll target");
    XSizeHints hints; memset(&hints, 0, sizeof(hints));
    hints.flags = PPosition; hints.x = 40; hints.y = 40;
    XSetWMNormalHints(dpy, win, &hints);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    unsigned long cur = blue;

    /* st's font init: open an Xft font by fontconfig name (triggers FcInit + RENDER queries). */
    XftFont *f = XftFontOpenName(dpy, screen, "DejaVu Sans Mono:pixelsize=14:antialias=true");
    if (f) mark("XF:font-opened\n"); else mark("XF:font-fail\n");

    mark("XF:mapped\n");
    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case Expose:    break;
                case KeyPress:  cur = 0x20A020; mark("XF:key\n"); break;
                case ButtonPress: cur = 0xE08020; mark("XF:button\n"); break;
                default: break;
            }
            XSetForeground(dpy, gc, cur);
            XFillRectangle(dpy, win, gc, 0, 0, 250, 160);
            XFlush(dpy);
        }
        struct timespec ts = { 0, 8 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    return 0;
}
