/* Constraint #4: confirm the XU2 root cause -- does CAIRO render to a depth-1 (A1) X bitmap on the wasm
 * X server? xfwm4 builds its decoration SHAPE masks by drawing into a depth-1 pixmap via
 * cairo_xlib_surface_create_for_bitmap (mypixmap.c), then XShapeCombineMask's its decoration windows with
 * them. If cairo->A1 is a no-op, those masks are empty -> decorations shaped to nothing -> invisible.
 *
 * Two windows, each shaped to its LEFT HALF, but the mask is drawn two different ways:
 *   E @ (40,40)  : mask drawn with CAIRO (cairo_xlib_surface_create_for_bitmap + fill) -- the xfwm4 path.
 *   F @ (300,40) : mask drawn with CORE X (XFillRectangle on the bitmap)               -- the control.
 * Read back: F must show orange-left/black-right (core-X masks already proven to work). If E matches F ->
 * cairo->A1 works (the xfwm4 bug is its alpha-threshold logic, not cairo). If E is fully BLACK while F is
 * half-orange -> cairo->A1 rendering is broken = the confirmed xfwm4 root cause. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <unistd.h>
#include <string.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

int main(void) {
    mark("CM:start\n");
    Display *dpy = XOpenDisplay(":0");
    if (!dpy) { mark("CM:open_failed\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int W = 200, H = 150;

    if (!XShapeQueryExtension(dpy, &(int){0}, &(int){0})) { mark("CM:no_shape\n"); return 1; }

    /* E: mask drawn with CAIRO into a depth-1 bitmap (the xfwm4 mechanism). */
    Window e = XCreateSimpleWindow(dpy, root, 40, 40, W, H, 0, 0, 0xE08000); /* orange */
    Pixmap emask = XCreatePixmap(dpy, root, W, H, 1);
    {
        /* Mirror xfwm4's exact sequence: create+use a COLOR xlib surface FIRST (this is what triggers
         * cairo's one-time init in a full app), THEN the depth-1 bitmap surface. */
        Pixmap colorpm = XCreatePixmap(dpy, root, W, H, DefaultDepth(dpy, scr));
        cairo_surface_t *cs = cairo_xlib_surface_create(dpy, colorpm, DefaultVisual(dpy, scr), W, H);
        cairo_t *ccr = cairo_create(cs);
        cairo_set_source_rgb(ccr, 1, 1, 1); cairo_paint(ccr);
        cairo_destroy(ccr); cairo_surface_destroy(cs);
        mark("CM:e_color_surface_ok\n");

        GC z = XCreateGC(dpy, emask, 0, NULL);
        XSetForeground(dpy, z, 0); XFillRectangle(dpy, emask, z, 0, 0, W, H);
        XFreeGC(dpy, z);
        mark("CM:e_bitmap_zeroed\n");
        cairo_surface_t *s = cairo_xlib_surface_create_for_bitmap(dpy, emask, ScreenOfDisplay(dpy, scr), W, H);
        mark("CM:e_surface_created\n");
        cairo_t *cr = cairo_create(s);
        mark("CM:e_cr_created\n");
        /* Exactly xfwm4's non-alpha path: SOURCE op, rgba(0,0,0,1), rectangle, fill -> set bits opaque. */
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
        cairo_rectangle(cr, 0, 0, W/2, H);
        cairo_fill(cr);
        mark("CM:e_filled\n");
        cairo_surface_flush(s);
        cairo_destroy(cr);
        cairo_surface_destroy(s);
        mark("CM:e_cairo_done\n");
    }
    XShapeCombineMask(dpy, e, ShapeBounding, 0, 0, emask, ShapeSet);

    /* F: mask drawn with CORE X (control, already proven to work). */
    Window f = XCreateSimpleWindow(dpy, root, 300, 40, W, H, 0, 0, 0xE08000);
    Pixmap fmask = XCreatePixmap(dpy, root, W, H, 1);
    {
        GC mgc = XCreateGC(dpy, fmask, 0, NULL);
        XSetForeground(dpy, mgc, 0); XFillRectangle(dpy, fmask, mgc, 0, 0, W, H);
        XSetForeground(dpy, mgc, 1); XFillRectangle(dpy, fmask, mgc, 0, 0, W/2, H);
        XFreeGC(dpy, mgc);
    }
    XShapeCombineMask(dpy, f, ShapeBounding, 0, 0, fmask, ShapeSet);

    XMapWindow(dpy, e);
    XMapWindow(dpy, f);
    XFlush(dpy);
    mark("CM:mapped\n");
    for (;;) { XEvent ev; XNextEvent(dpy, &ev); }
    return 0;
}
