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

/* st spawns its shell child via host_net.pty_spawn (ttynew). Replicate that to test whether spawning a
 * PTY descendant is what breaks device-event delivery on the spawner's X connection. */
extern unsigned pty_spawn(const char *cmd, unsigned cmd_len, const char *argv_json,
                          unsigned argv_json_len, unsigned *ret_master_fd)
    __attribute__((import_module("host_net"), import_name("pty_spawn")));

int main(void) {
    /* st's main() does this before opening the display; test if locale/IM modifiers break key input. */
    setlocale(LC_CTYPE, "");
    Display *dpy = XOpenDisplay(NULL);
    XSetLocaleModifiers("");
    if (!dpy) { mark("XF:no-display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    unsigned long blue = 0x3060C0;
    /* Replicate st's EXACT window creation: XCreateWindow at (0,0), large, with explicit attrs incl.
     * CWColormap + event_mask set in the create call (not XSelectInput), and GC created on the ROOT. */
    XSetWindowAttributes attrs; memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel = blue;
    attrs.border_pixel = blue;
    attrs.bit_gravity = NorthWestGravity;
    attrs.event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask | ExposureMask
        | VisibilityChangeMask | StructureNotifyMask | ButtonMotionMask
        | ButtonPressMask | ButtonReleaseMask;
    attrs.colormap = DefaultColormap(dpy, screen);
    Window win = XCreateWindow(dpy, root, 0, 0, 644, 408, 0, DefaultDepth(dpy, screen),
                               InputOutput, DefaultVisual(dpy, screen),
                               CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
                               &attrs);
    XStoreName(dpy, win, "secure-exec xft poll target");
    XSizeHints hints; memset(&hints, 0, sizeof(hints));
    hints.flags = PPosition; hints.x = 0; hints.y = 0;
    XSetWMNormalHints(dpy, win, &hints);
    XMapWindow(dpy, win);
    XFlush(dpy);
    GC gc = XCreateGC(dpy, root, 0, NULL);   /* st creates its GC on the ROOT window */
    unsigned long cur = blue;

    /* st's font init: open an Xft font by fontconfig name (triggers FcInit + RENDER queries). */
    XftFont *f = XftFontOpenName(dpy, screen, "DejaVu Sans Mono:pixelsize=14:antialias=true");
    if (f) mark("XF:font-opened\n"); else mark("XF:font-fail\n");

    /* st's render machinery: an offscreen pixmap + XftDraw on it + RENDER glyph draw, then XCopyArea
     * to the window (st never draws the window directly, only via this buffer). Tests whether the
     * RENDER/pixmap rendering path is what breaks device-event surfacing on st's connection. */
    Visual *vis = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    Pixmap buf = XCreatePixmap(dpy, win, 644, 408, DefaultDepth(dpy, screen));
    XftDraw *xd = XftDrawCreate(dpy, buf, vis, cmap);
    XftColor xc; XRenderColor rc = { 0xffff, 0xffff, 0xffff, 0xffff };
    XftColorAllocValue(dpy, vis, cmap, &rc, &xc);
    if (f && xd) {
        XftDrawStringUtf8(xd, &xc, f, 10, 30, (const FcChar8 *) "wsh ready", 9);
        XCopyArea(dpy, buf, win, gc, 0, 0, 644, 408, 0, 0);
        XFlush(dpy);
        mark("XF:rendered\n");
    }

    /* Replicate st's ttynew: spawn a PTY child. This is the major st behavior the baseline lacked. */
    {
        unsigned master = 0;
        const char *cmd = "/pty-shell.wasm";
        if (pty_spawn(cmd, (unsigned) strlen(cmd), "[]", 2, &master) == 0)
            mark("XF:spawned\n");
        else
            mark("XF:spawn-fail\n");
    }

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
            XFillRectangle(dpy, win, gc, 0, 0, 644, 408);
            XFlush(dpy);
        }
        struct timespec ts = { 0, 8 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    return 0;
}
