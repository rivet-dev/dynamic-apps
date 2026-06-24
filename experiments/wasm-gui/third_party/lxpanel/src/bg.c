/*
 * fb-background-monitor.c:
 *
 * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
 *                     2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authors:
 *      Ian McKellar <yakk@yakk.net>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#if !GTK_CHECK_VERSION(3, 0, 0)
/* we don't provide these APIs for GTK+ 3.0 */

#include "bg.h"

//#define DEBUG
#include "dbg.h"

enum {
    CHANGED,
    LAST_SIGNAL
};


struct _FbBgClass {
    GObjectClass   parent_class;
    void         (*changed) (FbBg *monitor);
};

struct _FbBg {
    GObject    parent_instance;

    Window   xroot;
    Atom     id;
    GC       gc;
    Display *dpy;
    Pixmap   pixmap;
};

static void fb_bg_class_init (FbBgClass *klass);
static void fb_bg_init (FbBg *monitor);
static void fb_bg_finalize (GObject *object);
static Pixmap fb_bg_get_xrootpmap(FbBg *monitor);
static void fb_bg_changed(FbBg *monitor);


static guint signals [LAST_SIGNAL] = { 0 };

static FbBg *default_bg = NULL;

GType
fb_bg_get_type (void)
{
    static GType object_type = 0;

    if (!object_type) {
        static const GTypeInfo object_info = {
            sizeof (FbBgClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) fb_bg_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (FbBg),
            0,              /* n_preallocs */
            (GInstanceInitFunc) fb_bg_init,
        };

        object_type = g_type_register_static (
            G_TYPE_OBJECT, "FbBg", &object_info, 0);
    }

    return object_type;
}



static void
fb_bg_class_init (FbBgClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    ENTER;
    signals [CHANGED] =
        g_signal_new ("changed",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbBgClass, changed),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    klass->changed = fb_bg_changed;
    object_class->finalize = fb_bg_finalize;
    RET();
}

static void
fb_bg_init (FbBg *bg)
{
    XGCValues  gcv;
    uint mask;

    ENTER;
    bg->dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    bg->xroot = DefaultRootWindow(bg->dpy);
    bg->id = gdk_x11_get_xatom_by_name("_XROOTPMAP_ID");
    bg->pixmap = fb_bg_get_xrootpmap(bg);
    gcv.ts_x_origin = 0;
    gcv.ts_y_origin = 0;
    gcv.fill_style = FillTiled;
    mask = GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle;
    if (bg->pixmap != None) {
        gcv.tile = bg->pixmap;
        mask |= GCTile ;
    }
    bg->gc = XCreateGC (bg->dpy, bg->xroot, mask, &gcv) ;
    RET();
}

static void
fb_bg_finalize (GObject *object)
{
    FbBg *bg;

    ENTER;
    bg = FB_BG (object);
    XFreeGC(bg->dpy, bg->gc);
    RET();
}


static Pixmap
fb_bg_get_xrootpmap(FbBg *bg)
{
    Pixmap ret = None;

    ENTER;
    if (bg->id) {
        int  act_format, c = 2 ;
        u_long  nitems ;
        u_long  bytes_after ;
        u_char *prop = NULL;
        Atom ret_type;

        do {
            if (XGetWindowProperty(bg->dpy, bg->xroot, bg->id, 0, 1,
                      False, XA_PIXMAP, &ret_type, &act_format,
                      &nitems, &bytes_after, &prop) == Success) {
                if (ret_type == XA_PIXMAP) {
                    ret = *((Pixmap *)prop);
                    XFree(prop);
                    break;
                }
            }
        } while (--c > 0);
    }
    RET(ret);

}


GdkPixmap *
fb_bg_get_xroot_pix_for_win(FbBg *bg, GtkWidget *widget)
{
    Window win;
    Window dummy;
    Pixmap bgpix;
    GdkPixmap *gbgpix;
    guint  width, height, border, depth;
    int  x, y;

    ENTER;
    win = GDK_WINDOW_XWINDOW(gtk_widget_get_window(widget));
    if (!XGetGeometry(bg->dpy, win, &dummy, &x, &y, &width, &height, &border,
              &depth)) {
        g_warning("XGetGeometry failed\n");
        RET(NULL);
    }
    XTranslateCoordinates(bg->dpy, win, bg->xroot, 0, 0, &x, &y, &dummy);
    DBG("win=%x %dx%d%+d%+d\n", win, width, height, x, y);
    gbgpix = gdk_pixmap_new(NULL, width, height, depth);
    if (!gbgpix) {
        g_critical("gdk_pixmap_new failed");
        RET(NULL);
    }
    bgpix =  gdk_x11_drawable_get_xid(gbgpix);
    XSetTSOrigin(bg->dpy, bg->gc, -x, -y) ;
    XFillRectangle(bg->dpy, bgpix, bg->gc, 0, 0, width, height);
    RET(gbgpix);
}

void
fb_bg_composite(GdkDrawable *base, GdkColor *tintcolor, gint alpha)
{
    cairo_t *cr;
    FbBg *bg;

    ENTER;
    cr = gdk_cairo_create(base);
    gdk_cairo_set_source_color(cr, tintcolor);
    cairo_paint_with_alpha(cr, (double) alpha/255);
    check_cairo_status(cr);
    cairo_destroy(cr);
    bg = fb_bg_get_for_display();
    fb_bg_changed(bg);
    g_object_unref(bg);
    RET();
}


static void
fb_bg_changed(FbBg *bg)
{
    ENTER;
    bg->pixmap = fb_bg_get_xrootpmap(bg);
    if (bg->pixmap != None) {
        XGCValues  gcv;

        gcv.tile = bg->pixmap;
        XChangeGC(bg->dpy, bg->gc, GCTile, &gcv);
        DBG("changed\n");
    }
    RET();
}

inline void fb_bg_notify_changed_bg(FbBg *bg)
{
    ENTER;
    g_signal_emit (bg, signals [CHANGED], 0);
    RET();
}

FbBg *fb_bg_get_for_display(void)
{
    ENTER;
    if (!default_bg)
    {
        default_bg = fb_bg_new();
        g_object_add_weak_pointer( G_OBJECT(default_bg),
                (gpointer)&default_bg );
    }
    else
        g_object_ref(default_bg);
    RET(default_bg);
}

GdkPixmap *
fb_bg_get_pix_from_file(GtkWidget *widget, const char *filename)
{
    ENTER;
    GdkPixbuf *pixbuf;
    cairo_t *cr;
    GdkPixmap *pixmap;

    pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
    if (!pixbuf) {
        GtkStyle *style = gtk_widget_get_style(widget);
        if (style->bg_pixmap[0])
            g_object_ref(style->bg_pixmap[0]);
        RET(style->bg_pixmap[0]);
    }
    pixmap = gdk_pixmap_new(gtk_widget_get_window(widget), gdk_pixbuf_get_width(pixbuf),
                            gdk_pixbuf_get_height(pixbuf), -1);
    cr = gdk_cairo_create(pixmap);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);
    check_cairo_status(cr);
    cairo_destroy(cr);

    g_object_unref( pixbuf );
    RET(pixmap);
}
#endif /* GTK_CHECK_VERSION */
