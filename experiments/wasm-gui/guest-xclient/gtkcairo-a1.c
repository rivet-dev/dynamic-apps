/* Constraint #4: observe cairo->A1 in a REAL cairo context (a GTK app, where cairo inits fine -- unlike
 * the minimal cairomask-test which crashed on cairo init). After gtk_init, draw into a depth-1 (A1) X
 * pixmap with cairo exactly like xfwm4's mask code (mypixmap.c), then XGetImage the pixmap and count set
 * bits. This is THE decisive test of whether cairo->A1 produces an empty mask. xfwm4 stays unmodified;
 * this is our own probe. */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>

static int count_bits(Display *dpy, Pixmap pm, int W, int H, int *left, int *right) {
    XImage *img = XGetImage(dpy, pm, 0, 0, W, H, 1, XYPixmap);
    if (!img) return -1;
    int total = 0; *left = 0; *right = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            unsigned long p = XGetPixel(img, x, y);
            if (p) { total++; if (x < W/2) (*left)++; else (*right)++; }
        }
    XDestroyImage(img);
    return total;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    g_printerr("A1PROBE: after gtk_init\n");
    Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int W = 64, H = 32;

    /* Pattern A: simple solid fill of the left half (cairo's basic A1 path). */
    {
        Pixmap pm = XCreatePixmap(dpy, root, W, H, 1);
        GC z = XCreateGC(dpy, pm, 0, NULL); XSetForeground(dpy, z, 0); XFillRectangle(dpy, pm, z, 0, 0, W, H); XFreeGC(dpy, z);
        cairo_surface_t *s = cairo_xlib_surface_create_for_bitmap(dpy, pm, ScreenOfDisplay(dpy, scr), W, H);
        cairo_status_t st = cairo_surface_status(s);
        cairo_t *cr = cairo_create(s);
        cairo_set_source_rgba(cr, 0, 0, 0, 1.0);          /* opaque -> A1 bit set */
        cairo_rectangle(cr, 0, 0, W/2, H);
        cairo_fill(cr);
        cairo_surface_flush(s);
        cairo_destroy(cr); cairo_surface_destroy(s);
        int l, r; int t = count_bits(dpy, pm, W, H, &l, &r);
        g_printerr("A1PROBE: patternA(solid-fill) surf_status=%d set=%d left=%d right=%d (expect left=%d right=0)\n",
                   (int)st, t, l, r, (W/2)*H);
        XFreePixmap(dpy, pm);
    }

    /* Pattern B: xfwm4's alpha-mask path -- CLEAR everything, then SOURCE white fill of the left half. */
    {
        Pixmap pm = XCreatePixmap(dpy, root, W, H, 1);
        cairo_surface_t *s = cairo_xlib_surface_create_for_bitmap(dpy, pm, ScreenOfDisplay(dpy, scr), W, H);
        cairo_t *cr = cairo_create(s);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_rectangle(cr, 0, 0, W, H); cairo_fill(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_rectangle(cr, 0, 0, W/2, H); cairo_fill(cr);
        cairo_surface_flush(s);
        cairo_destroy(cr); cairo_surface_destroy(s);
        int l, r; int t = count_bits(dpy, pm, W, H, &l, &r);
        g_printerr("A1PROBE: patternB(xfwm4-clear+fill) set=%d left=%d right=%d (expect left=%d right=0)\n",
                   t, l, r, (W/2)*H);
        XFreePixmap(dpy, pm);
    }

    /* Control: cairo to a COLOR (depth-24) pixmap, same fill -- proves cairo draws at all in this context. */
    {
        int depth = DefaultDepth(dpy, scr);
        Pixmap pm = XCreatePixmap(dpy, root, W, H, depth);
        cairo_surface_t *s = cairo_xlib_surface_create(dpy, pm, DefaultVisual(dpy, scr), W, H);
        cairo_t *cr = cairo_create(s);
        cairo_set_source_rgb(cr, 1, 0, 0); cairo_rectangle(cr, 0, 0, W/2, H); cairo_fill(cr);
        cairo_surface_flush(s);
        cairo_destroy(cr); cairo_surface_destroy(s);
        XImage *img = XGetImage(dpy, pm, 0, 0, W, H, ~0, ZPixmap);
        unsigned long p = img ? XGetPixel(img, 10, 10) : 0;
        g_printerr("A1PROBE: controlColor(depth%d) px(10,10)=0x%lx (expect non-zero red)\n", depth, p);
        if (img) XDestroyImage(img);
        XFreePixmap(dpy, pm);
    }

    /* ★ The real test: load an actual xfwm4 decoration PNG and run xfwm4's exact alpha-threshold mask
     * logic (mypixmap.c:862-901). If it finds 0 opaque pixels for an opaque image, the gdk_pixbuf alpha
     * read is the bug. */
    {
        /* A/B: an LA (gray+alpha) title gradient vs an RGBA button -- discriminate color-type vs general. */
        const char *paths[] = {
            "/usr/share/themes/Greybird/xfwm4/title-1-active.png",  /* 2x24 LA (gray+alpha) */
            "/usr/share/themes/Greybird/xfwm4/close-active.png",    /* 20x24 RGBA */
            "/usr/share/themes/Greybird/xfwm4/top-left-active.png", /* 8x24 RGBA */
            NULL };
        for (int i = 0; paths[i]; i++) {
            GError *e2 = NULL;
            GdkPixbuf *p2 = gdk_pixbuf_new_from_file(paths[i], &e2);
            g_printerr("A1PROBE: load %s -> %s%s\n", paths[i], p2 ? "OK" : "FAIL: ", p2 ? "" : (e2?e2->message:"?"));
            if (p2) g_object_unref(p2);
            if (e2) g_error_free(e2);
        }
        const char *path = "/usr/share/themes/Greybird/xfwm4/close-active.png"; /* use a known-RGBA for the threshold test */
        GError *err = NULL;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &err);
        if (!pb) {
            g_printerr("A1PROBE: decoPNG load FAILED %s: %s\n", path, err ? err->message : "?");
        } else {
            int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
            int rs = gdk_pixbuf_get_rowstride(pb), nc = gdk_pixbuf_get_n_channels(pb);
            gboolean ha = gdk_pixbuf_get_has_alpha(pb);
            guchar *px = gdk_pixbuf_get_pixels(pb);
            g_printerr("A1PROBE: decoPNG %dx%d has_alpha=%d n_channels=%d rowstride=%d (w*nc=%d)\n",
                       w, h, ha, nc, rs, w*nc);
            /* xfwm4's exact index: dpx = rowstride / width; alpha = pixels[(y*width+x+1)*dpx - 1] */
            int dpx = rs / w;
            int opaque = 0, sample_alpha[3] = {-1,-1,-1};
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    guchar a = px[(y * w + x + 1) * dpx - 1];   /* xfwm4's (buggy-on-padding?) index */
                    if (a == 0xff) opaque++;
                }
            /* correct index for comparison: pixels[y*rowstride + x*nc + (nc-1)] (alpha is last channel) */
            int opaque_correct = 0;
            if (ha) for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if (px[y*rs + x*nc + (nc-1)] == 0xff) opaque_correct++;
            sample_alpha[0] = ha ? px[(nc-1)] : 255;
            sample_alpha[1] = ha ? px[(h/2)*rs + (w/2)*nc + (nc-1)] : 255;
            g_printerr("A1PROBE: dpx=%d  xfwm4-index opaque=%d/%d  correct-index opaque=%d/%d  alpha[0]=%d alpha[mid]=%d\n",
                       dpx, opaque, w*h, opaque_correct, w*h, sample_alpha[0], sample_alpha[1]);
            g_object_unref(pb);
        }
    }

    g_printerr("A1PROBE: done\n");
    return 0;
}
