/* Constraint #4: localize the cairo->A1 gap. cairo draws xfwm4's solid decoration mask by compositing a
 * solid source onto a depth-1 (A1) destination Picture via XRender (PictOpSrc). XRenderFillRectangle on an
 * A1 picture is a faithful proxy. Two windows, each shaped to LEFT HALF, mask drawn two ways:
 *   E @ (40,40)  : A1 mask filled via XRENDER (XRenderFillRectangle on an A1 Picture)   -- the cairo path.
 *   F @ (300,40) : A1 mask filled via CORE X (XFillRectangle on the bitmap)             -- the control.
 * If E shows orange-left/black-right like F -> XRender->A1 works (cairo's gap is its image upload path).
 * If E is fully shaped away (invisible) while F is half-orange -> XRender-fill-to-A1 is broken in the wasm
 * Xvfb = the platform fix point. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrender.h>
#include <unistd.h>
#include <string.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

int main(void) {
    mark("XR:start\n");
    Display *dpy = XOpenDisplay(":0");
    if (!dpy) { mark("XR:open_failed\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int W = 200, H = 150;
    if (!XShapeQueryExtension(dpy, &(int){0}, &(int){0})) { mark("XR:no_shape\n"); return 1; }
    if (!XRenderQueryExtension(dpy, &(int){0}, &(int){0})) { mark("XR:no_render\n"); return 1; }
    XRenderPictFormat *a1 = XRenderFindStandardFormat(dpy, PictStandardA1);
    if (!a1) { mark("XR:no_a1_format\n"); return 1; }

    /* E: A1 mask filled via XRender (the cairo mechanism). */
    Window e = XCreateSimpleWindow(dpy, root, 40, 40, W, H, 0, 0, 0xE08000);
    Pixmap em = XCreatePixmap(dpy, root, W, H, 1);
    { GC z = XCreateGC(dpy, em, 0, NULL); XSetForeground(dpy, z, 0); XFillRectangle(dpy, em, z, 0, 0, W, H); XFreeGC(dpy, z); }
    Picture epic = XRenderCreatePicture(dpy, em, a1, 0, NULL);
    XRenderColor opaque = {0, 0, 0, 0xffff};            /* alpha=1 -> A1 bit set */
    XRenderFillRectangle(dpy, PictOpSrc, epic, &opaque, 0, 0, W/2, H);
    mark("XR:e_rendered\n");
    XShapeCombineMask(dpy, e, ShapeBounding, 0, 0, em, ShapeSet);

    /* F: A1 mask filled via core X (control). */
    Window f = XCreateSimpleWindow(dpy, root, 300, 40, W, H, 0, 0, 0xE08000);
    Pixmap fm = XCreatePixmap(dpy, root, W, H, 1);
    { GC g = XCreateGC(dpy, fm, 0, NULL); XSetForeground(dpy, g, 0); XFillRectangle(dpy, fm, g, 0, 0, W, H);
      XSetForeground(dpy, g, 1); XFillRectangle(dpy, fm, g, 0, 0, W/2, H); XFreeGC(dpy, g); }
    XShapeCombineMask(dpy, f, ShapeBounding, 0, 0, fm, ShapeSet);

    /* G: A1 mask via XPutImage (the path cairo uses to upload a client-side pixman A1 image). Build a
     * depth-1 bitmap with core X (left half set), XGetImage it, XPutImage into a fresh bitmap, shape with
     * that. If G != half-orange, depth-1 XGetImage/XPutImage is broken = the likely cairo upload gap. */
    Window g = XCreateSimpleWindow(dpy, root, 560, 40, W, H, 0, 0, 0xE08000);
    Pixmap src = XCreatePixmap(dpy, root, W, H, 1);
    { GC gc = XCreateGC(dpy, src, 0, NULL); XSetForeground(dpy, gc, 0); XFillRectangle(dpy, src, gc, 0, 0, W, H);
      XSetForeground(dpy, gc, 1); XFillRectangle(dpy, src, gc, 0, 0, W/2, H); XFreeGC(dpy, gc); }
    XImage *img = XGetImage(dpy, src, 0, 0, W, H, 1, XYPixmap);
    Pixmap gm = XCreatePixmap(dpy, root, W, H, 1);
    if (img) {
        GC gc2 = XCreateGC(dpy, gm, 0, NULL);
        XPutImage(dpy, gm, gc2, img, 0, 0, 0, 0, W, H);
        XFreeGC(dpy, gc2);
        mark("XR:g_putimage_done\n");
    } else mark("XR:g_getimage_NULL\n");
    XShapeCombineMask(dpy, g, ShapeBounding, 0, 0, gm, ShapeSet);

    XMapWindow(dpy, e); XMapWindow(dpy, f); XMapWindow(dpy, g); XFlush(dpy);
    mark("XR:mapped\n");
    for (;;) { XEvent ev; XNextEvent(dpy, &ev); }
    return 0;
}
