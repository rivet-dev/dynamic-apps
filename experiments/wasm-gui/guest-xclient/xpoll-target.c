/* Diagnostic: identical to xinput-target.c (green on KeyPress, orange on ButtonPress) BUT uses a
 * non-blocking poll loop (XPending + nanosleep) instead of blocking XNextEvent -- exactly st's event
 * loop structure. If this stays blue (no key) while xinput-target turns green, the non-blocking poll
 * loop itself loses KeyPress events (the root cause of st live-typing); if it turns green, st's failure
 * is its heavy Xft/PTY load, not the loop structure. Minimal client = no Xft, no PTY load. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void mark(const char *s) { (void) write(2, s, strlen(s)); }

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { mark("XP:no-display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    unsigned long blue = 0x3060C0;
    Window win = XCreateSimpleWindow(dpy, root, 40, 40, 250, 160, 2,
                                     BlackPixel(dpy, screen), blue);
    XStoreName(dpy, win, "secure-exec poll target");
    XSizeHints hints; memset(&hints, 0, sizeof(hints));
    hints.flags = PPosition; hints.x = 40; hints.y = 40;
    XSetWMNormalHints(dpy, win, &hints);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    unsigned long cur = blue;
    mark("XP:mapped\n");

    /* st-style non-blocking poll loop: drain XPending, repaint, sleep, repeat. */
    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case Expose:    break;
                case KeyPress:  cur = 0x20A020; mark("XP:key\n"); break;
                case ButtonPress: cur = 0xE08020; mark("XP:button\n"); break;
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
