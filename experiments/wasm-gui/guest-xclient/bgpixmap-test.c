/* Constraint #4 observability: isolate whether the wasm X server paints a window's BACKGROUND PIXMAP
 * on map/ClearWindow -- the exact mechanism xfwm4 uses for ALL decorations (XSetWindowBackgroundPixmap
 * + XClearWindow), unlike openbox/twm which draw directly and DO render. Three side-by-side windows on
 * the bare X server (no WM):
 *   A @ (40,40)  : SOLID green background color (control: solid bg works?).
 *   B @ (300,40) : background PIXMAP filled magenta (the xfwm4 mechanism under test).
 *   C @ (560,40) : direct XFillRectangle cyan on Expose (control: direct draw works, like openbox).
 * Read the framebuffer: if B is magenta -> bg pixmaps work (xfwm4's blank is elsewhere); if B is black
 * while A is green and C is cyan -> the server does not paint background pixmaps = the xfwm4 root cause. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <string.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

int main(void) {
    mark("BGP:start\n");
    Display *dpy = XOpenDisplay(":0");
    if (!dpy) { mark("BGP:open_failed\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int depth = DefaultDepth(dpy, scr);

    /* A: solid green background color */
    Window a = XCreateSimpleWindow(dpy, root, 40, 40, 200, 150, 0, 0, 0x00A000);

    /* B: SMALL background PIXMAP (16x16) on a LARGE window -> the server must TILE it. This is exactly
     * the xfwm4 titlebar path (thin gradient-slice pixmaps tiled across the title window). */
    Pixmap pm = XCreatePixmap(dpy, root, 16, 16, depth);
    GC pgc = XCreateGC(dpy, pm, 0, NULL);
    XSetForeground(dpy, pgc, 0xC000C0);                 /* magenta */
    XFillRectangle(dpy, pm, pgc, 0, 0, 16, 16);
    XSetForeground(dpy, pgc, 0xFFFF00);                 /* yellow quadrant so tiling is visible */
    XFillRectangle(dpy, pm, pgc, 0, 0, 8, 8);
    Window b = XCreateSimpleWindow(dpy, root, 300, 40, 200, 150, 0, 0, 0);
    XSetWindowBackgroundPixmap(dpy, b, pm);             /* <-- tiled bg pixmap (the xfwm4 titlebar path) */

    /* C: direct-draw cyan on Expose (the openbox/twm path, known to work) */
    Window c = XCreateSimpleWindow(dpy, root, 560, 40, 200, 150, 0, 0, 0);
    XSelectInput(dpy, c, ExposureMask);

    /* D @ (40,250): SHAPE test -- xfwm4 shapes its title/side/corner decoration windows with an alpha
     * mask via XShapeCombineMask. Make a solid-orange window, then shape it to its LEFT HALF with a
     * depth-1 mask. If D shows as orange LEFT-half-only -> SHAPE works (so xfwm4's blank = empty mask
     * from cairo-to-1bit). If D vanishes entirely or ignores the mask -> the wasm Xvfb SHAPE ext is the
     * xfwm4 root cause. */
    int shape_ev=0, shape_err=0;
    int have_shape = XShapeQueryExtension(dpy, &shape_ev, &shape_err);
    mark(have_shape ? "BGP:shape_ext=yes\n" : "BGP:shape_ext=NO\n");
    Window d = XCreateSimpleWindow(dpy, root, 40, 250, 200, 150, 0, 0, 0xE08000); /* orange */
    Pixmap mask = XCreatePixmap(dpy, root, 200, 150, 1);   /* depth-1 bitmap */
    GC mgc = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, mgc, 0); XFillRectangle(dpy, mask, mgc, 0, 0, 200, 150);   /* all 0 */
    XSetForeground(dpy, mgc, 1); XFillRectangle(dpy, mask, mgc, 0, 0, 100, 150);   /* left half = 1 */
    if (have_shape)
        XShapeCombineMask(dpy, d, ShapeBounding, 0, 0, mask, ShapeSet);

    XMapWindow(dpy, a);
    XMapWindow(dpy, b);
    XMapWindow(dpy, c);
    XMapWindow(dpy, d);
    XClearWindow(dpy, b);                               /* force the bg-pixmap paint */
    XFlush(dpy);
    mark("BGP:mapped\n");

    GC cgc = XCreateGC(dpy, c, 0, NULL);
    int n = 0;
    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) {
            if (ev.xexpose.window == c) {
                XSetForeground(dpy, cgc, 0x00C0C0);     /* cyan direct draw */
                XFillRectangle(dpy, c, cgc, 0, 0, 200, 150);
            } else if (ev.xexpose.window == b) {
                XClearWindow(dpy, b);                   /* repaint bg pixmap on expose */
            }
            XFlush(dpy);
            if (n++ == 0) mark("BGP:drawn\n");
        }
    }
    return 0;
}
