/*
 * Copyright (C) 2006-2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009 Jürgen Hötzel <juergen@archlinux.org>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2010 Lajos Kamocsay <lajos@panka.com>
 *               2012 Piotr Sipika <Piotr.Sipika@gmail.com>
 *               2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2012 Jack Chen <speed.up08311990@gmail.com>
 *               2012 Rafał Mużyło <galtgendo@gmail.com>
 *               2012 Michael Rawson <michaelrawson76@gmail.com>
 *               2012 Julien Lavergne <julien.lavergne@gmail.com>
 *               2013 Rouslan <rouslan-k@users.sourceforge.net>
 *               2013 peadaredwards <peadaredwards@users.sourceforge.net>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2015 Rafał Mużyło <galtgendo@gmail.com>
 *               2015 Hanno Zulla <hhz@users.sf.net>
 *               2018 Mamoru TASAKA <mtasaka@fedoraproject.org>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <libfm/fm-gtk.h>
#include <cairo-xlib.h>

#define __LXPANEL_INTERNALS__

#include "private.h"
#include "misc.h"
#include "space.h"

#include "lxpanelctl.h"
#include "dbg.h"
#include "gtk-compat.h"

gchar *cprofile = "default";

GSList* all_panels = NULL;  /* a single-linked list storing all panels */

gboolean is_in_lxde = FALSE;

static GtkWindowGroup* win_grp = NULL; /* window group used to limit the scope of model dialog. */

static gulong monitors_handler = 0;

static void panel_start_gui(LXPanel *p, config_setting_t *list);
static void ah_start(LXPanel *p);
static void ah_stop(LXPanel *p);
static void _panel_update_background(LXPanel * p, gboolean enforce);

enum
{
    ICON_SIZE_CHANGED,
    PANEL_FONT_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE(PanelToplevel, lxpanel, GTK_TYPE_WINDOW);

static void lxpanel_finalize(GObject *object)
{
    LXPanel *self = LXPANEL(object);
    Panel *p = self->priv;

    if( p->config_changed )
        lxpanel_config_save( self );
    config_destroy(p->config);

    //XFree(p->workarea);
    g_free( p->background_file );
    g_slist_free( p->system_menus );

    g_free( p->name );
    g_free(p);

    G_OBJECT_CLASS(lxpanel_parent_class)->finalize(object);
}

static void panel_stop_gui(LXPanel *self)
{
    Panel *p = self->priv;
    Display *xdisplay;

    g_debug("panel_stop_gui on '%s'", p->name);
    if (p->autohide)
        ah_stop(self);

    if (p->pref_dialog != NULL)
        gtk_widget_destroy(p->pref_dialog);
    p->pref_dialog = NULL;

    if (p->plugin_pref_dialog != NULL)
        /* just close the dialog, it will do all required cleanup */
        gtk_dialog_response(GTK_DIALOG(p->plugin_pref_dialog), GTK_RESPONSE_CLOSE);


    if (p->initialized)
    {
        gtk_window_group_remove_window(win_grp, GTK_WINDOW(self));
        xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
        gdk_flush();
        XFlush(xdisplay);
        XSync(xdisplay, True);
        p->initialized = FALSE;
    }
    if (p->surface != NULL)
    {
        cairo_surface_destroy(p->surface);
        p->surface = NULL;
    }

    if (p->background_update_queued)
    {
        g_source_remove(p->background_update_queued);
        p->background_update_queued = 0;
    }
    if (p->strut_update_queued)
    {
        g_source_remove(p->strut_update_queued);
        p->strut_update_queued = 0;
    }
    if (p->reconfigure_queued)
    {
        g_source_remove(p->reconfigure_queued);
        p->reconfigure_queued = 0;
    }

    if (gtk_bin_get_child(GTK_BIN(self)))
    {
        gtk_widget_destroy(p->box);
        p->box = NULL;
    }
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void lxpanel_destroy(GtkWidget *object)
#else
static void lxpanel_destroy(GtkObject *object)
#endif
{
    LXPanel *self = LXPANEL(object);

    panel_stop_gui(self);

#if GTK_CHECK_VERSION(3, 0, 0)
    GTK_WIDGET_CLASS(lxpanel_parent_class)->destroy(object);
#else
    GTK_OBJECT_CLASS(lxpanel_parent_class)->destroy(object);
#endif
}

static gboolean idle_update_background(gpointer p)
{
    LXPanel *panel = LXPANEL(p);

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    /* Panel could be destroyed while background update scheduled */
    if (gtk_widget_get_realized(p))
    {
        gdk_display_sync( gtk_widget_get_display(p) );
        _panel_update_background(panel, FALSE);
    }
    panel->priv->background_update_queued = 0;

    return FALSE;
}

void _panel_queue_update_background(LXPanel *panel)
{
    if (panel->priv->background_update_queued)
        return;
    panel->priv->background_update_queued = g_idle_add_full(G_PRIORITY_HIGH,
                                                            idle_update_background,
                                                            panel, NULL);
}

static gboolean idle_update_strut(gpointer p)
{
    LXPanel *panel = LXPANEL(p);

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    _panel_set_wm_strut(panel);
    panel->priv->strut_update_queued = 0;

    return FALSE;
}

static void lxpanel_realize(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(lxpanel_parent_class)->realize(widget);

    _panel_queue_update_background(LXPANEL(widget));
}

static void lxpanel_style_set(GtkWidget *widget, GtkStyle* prev)
{
    GTK_WIDGET_CLASS(lxpanel_parent_class)->style_set(widget, prev);

    /* FIXME: This dirty hack is used to fix the background of systray... */
    _panel_queue_update_background(LXPANEL(widget));
}

static void lxpanel_size_request(GtkWidget *widget, GtkRequisition *req)
{
    LXPanel *panel = LXPANEL(widget);
    Panel *p = panel->priv;
    GdkRectangle rect;

#if !GTK_CHECK_VERSION(3, 0, 0)
    GTK_WIDGET_CLASS(lxpanel_parent_class)->size_request(widget, req);
#else
    GTK_WIDGET_CLASS(lxpanel_parent_class)->get_preferred_width(widget, &req->width, &req->width);
    GTK_WIDGET_CLASS(lxpanel_parent_class)->get_preferred_height(widget, &req->height, &req->height);
#endif

    if (!p->visible)
        /* When the panel is in invisible state, the content box also got hidden, thus always
         * report 0 size.  Ask the content box instead for its size. */
        gtk_widget_size_request(p->box, req);

    rect.width = req->width;
    rect.height = req->height;
    _calculate_position(panel, &rect);
    req->width = rect.width;
    req->height = rect.height;
    /* update data ahead of configuration request */
    p->cw = rect.width;
    p->ch = rect.height;
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void
lxpanel_get_preferred_width (GtkWidget *widget,
                             gint      *minimal_width,
                             gint      *natural_width)
{
  GtkRequisition requisition;

  lxpanel_size_request (widget, &requisition);

  if (minimal_width)
      *minimal_width = requisition.width;
  if (natural_width)
      *natural_width = requisition.width;
}

static void
lxpanel_get_preferred_height (GtkWidget *widget,
                              gint      *minimal_height,
                              gint      *natural_height)
{
  GtkRequisition requisition;

  lxpanel_size_request (widget, &requisition);

  if (minimal_height)
      *minimal_height = requisition.height;
  if (natural_height)
      *natural_height = requisition.height;
}
#endif

static void lxpanel_size_allocate(GtkWidget *widget, GtkAllocation *a)
{
    LXPanel *panel = LXPANEL(widget);
    Panel *p = panel->priv;
    GdkRectangle rect;
    gint x, y;

    /* some WM like mwm are too generous giving us space more than requested
       so let correct it right now, as much as we can */
    rect.x = a->x;
    rect.y = a->y;
    rect.width = MAX(8, MIN(p->cw, a->width));
    rect.height = MAX(8, MIN(p->ch, a->height));
    _calculate_position(panel, &rect);

    GTK_WIDGET_CLASS(lxpanel_parent_class)->size_allocate(widget, &rect);

    if (p->widthtype == WIDTH_REQUEST)
        p->width = (p->orientation == GTK_ORIENTATION_HORIZONTAL) ? rect.width : rect.height;
    if (p->heighttype == HEIGHT_REQUEST)
        p->height = (p->orientation == GTK_ORIENTATION_HORIZONTAL) ? rect.height : rect.width;

    if (!gtk_widget_get_realized(widget))
        return;

    /* get real coords since a contains 0, 0 */
    gdk_window_get_origin(gtk_widget_get_window(widget), &x, &y);
    p->ax = rect.x;
    p->ay = rect.y;

    if (rect.width != p->aw || rect.height != p->ah || x != p->ax || y != p->ay)
    {
        p->aw = rect.width;
        p->ah = rect.height;
        gtk_window_move(GTK_WINDOW(widget), p->ax, p->ay);
        /* SF bug #708: strut update does not work while in size allocation */
        if (!panel->priv->strut_update_queued)
            panel->priv->strut_update_queued = g_idle_add_full(G_PRIORITY_HIGH,
                                                               idle_update_strut,
                                                               panel, NULL);
        _panel_queue_update_background(panel);
    }

    if (gtk_widget_get_mapped(widget))
        _panel_establish_autohide(panel);
}

static gboolean lxpanel_configure_event (GtkWidget *widget, GdkEventConfigure *e)
{
    Panel *p = LXPANEL(widget)->priv;

    p->cw = e->width;
    p->ch = e->height;
    p->cx = e->x;
    p->cy = e->y;

    return GTK_WIDGET_CLASS(lxpanel_parent_class)->configure_event(widget, e);
}

static gboolean lxpanel_map_event(GtkWidget *widget, GdkEventAny *event)
{
    Panel *p = PLUGIN_PANEL(widget)->priv;

    if (p->autohide)
        ah_start(LXPANEL(widget));
    return GTK_WIDGET_CLASS(lxpanel_parent_class)->map_event(widget, event);
}

/* Handler for "button_press_event" signal with Panel as parameter. */
static gboolean lxpanel_button_press(GtkWidget *widget, GdkEventButton *event)
{
    LXPanel *panel = PLUGIN_PANEL(widget);

    if ((event->state & gtk_accelerator_get_default_mod_mask()) != 0)
        /* ignore clicks with modifiers */
        return FALSE;

    if (event->button == 3) /* right button */
    {
        GtkMenu* popup = (GtkMenu*) lxpanel_get_plugin_menu(panel, NULL, FALSE);
        gtk_menu_popup(popup, NULL, NULL, NULL, NULL, event->button, event->time);
        return TRUE;
    }
    else if (event->button == 2) /* middle button */
    {
        Panel *p = panel->priv;
        if (p->move_state == PANEL_MOVE_STOP)
        {
            gdk_window_get_origin(event->window, &p->move_x, &p->move_y);
            p->move_x += event->x - p->ax;
            p->move_y += event->y - p->ay;
            p->move_state = PANEL_MOVE_DETECT;
            p->move_device = event->device;
            /* rest of work see in panel-plugin-move.c file */
            return TRUE;
        }
    }
    return FALSE;
}

static void lxpanel_class_init(PanelToplevelClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
#if !GTK_CHECK_VERSION(3, 0, 0)
    GtkObjectClass *gtk_object_class = (GtkObjectClass *)klass;
#endif
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    gobject_class->finalize = lxpanel_finalize;
#if GTK_CHECK_VERSION(3, 0, 0)
    widget_class->destroy = lxpanel_destroy;
#else
    gtk_object_class->destroy = lxpanel_destroy;
#endif
    widget_class->realize = lxpanel_realize;
#if GTK_CHECK_VERSION(3, 0, 0)
    widget_class->get_preferred_width = lxpanel_get_preferred_width;
    widget_class->get_preferred_height = lxpanel_get_preferred_height;
#else
    widget_class->size_request = lxpanel_size_request;
#endif
    widget_class->size_allocate = lxpanel_size_allocate;
    widget_class->configure_event = lxpanel_configure_event;
    widget_class->style_set = lxpanel_style_set;
    widget_class->map_event = lxpanel_map_event;
    widget_class->button_press_event = lxpanel_button_press;
    widget_class->button_release_event = _lxpanel_button_release;
    widget_class->motion_notify_event = _lxpanel_motion_notify;

    signals[ICON_SIZE_CHANGED] =
        g_signal_new("icon-size-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PanelToplevelClass, icon_size_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0, G_TYPE_NONE);

    signals[PANEL_FONT_CHANGED] =
        g_signal_new("panel-font-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PanelToplevelClass, panel_font_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void lxpanel_init(PanelToplevel *self)
{
    Panel *p = g_new0(Panel, 1);

    self->priv = p;
    p->topgwin = self;
    p->align = ALIGN_CENTER;
    p->edge = EDGE_NONE;
    p->widthtype = WIDTH_PERCENT;
    p->width = 100;
    p->heighttype = HEIGHT_PIXEL;
    p->height = PANEL_HEIGHT_DEFAULT;
    p->monitor = 0;
    p->setdocktype = 1;
    p->setstrut = 1;
    p->round_corners = 0;
    p->autohide = 0;
    p->visible = TRUE;
    p->height_when_hidden = 2;
    p->transparent = 0;
    p->alpha = 255;
    gdk_color_parse("white", &p->gtintcolor);
    p->tintcolor = gcolor2rgb24(&p->gtintcolor);
    p->usefontcolor = 0;
    p->fontcolor = 0x00000000;
    p->usefontsize = 0;
    p->fontsize = 10;
    p->spacing = 0;
    p->icon_size = PANEL_ICON_SIZE;
    p->icon_theme = gtk_icon_theme_get_default();
    p->config = config_new();
    p->defstyle = gtk_widget_get_default_style();
}

/* Allocate and initialize new Panel structure. */
static LXPanel* panel_allocate(GdkScreen *screen)
{
    return g_object_new(LX_TYPE_PANEL,
                        "border-width", 0,
                        "decorated", FALSE,
                        "name", "PanelToplevel",
                        "resizable", FALSE,
                        "title", "panel",
                        "type-hint", GDK_WINDOW_TYPE_HINT_DOCK,
                        "window-position", GTK_WIN_POS_NONE,
                        "screen", screen,
                        NULL);
}

void _panel_emit_icon_size_changed(LXPanel *p)
{
    g_signal_emit(p, signals[ICON_SIZE_CHANGED], 0);
}

void _panel_emit_font_changed(LXPanel *p)
{
    g_signal_emit(p, signals[PANEL_FONT_CHANGED], 0);
}

/* Normalize panel configuration after load from file or reconfiguration. */
static void panel_normalize_configuration(Panel* p)
{
    panel_set_panel_configuration_changed( p );
    if (p->width < 0)
        p->width = 100;
    if (p->widthtype == WIDTH_PERCENT && p->width > 100)
        p->width = 100;
    p->heighttype = HEIGHT_PIXEL;
    if (p->heighttype == HEIGHT_PIXEL) {
        if (p->height < PANEL_HEIGHT_MIN)
            p->height = PANEL_HEIGHT_MIN;
        else if (p->height > PANEL_HEIGHT_MAX)
            p->height = PANEL_HEIGHT_MAX;
    }
    if (p->monitor < 0)
        p->monitor = -1;
    if (p->background)
        p->transparent = 0;
}

gboolean _panel_edge_can_strut(LXPanel *panel, int edge, gint monitor, gulong *size)
{
    Panel *p;
    GdkScreen *screen;
    GdkRectangle rect;
    GdkRectangle rect2;
    gint n, i;
    gulong s;

    if (!gtk_widget_get_mapped(GTK_WIDGET(panel)))
        return FALSE;

    p = panel->priv;
    /* Handle autohide case.  EWMH recommends having the strut be the minimized size. */
    if (p->autohide)
        s = p->height_when_hidden;
    else switch (edge)
    {
    case EDGE_LEFT:
    case EDGE_RIGHT:
        s = p->aw;
        break;
    case EDGE_TOP:
    case EDGE_BOTTOM:
        s = p->ah;
        break;
    default: /* error! */
        return FALSE;
    }
    if (s == 0)
        return FALSE; /* nothing to strut here */

    if (monitor < 0) /* screen span */
    {
        if (G_LIKELY(size))
            *size = s;
        return TRUE;
    }

    screen = gtk_widget_get_screen(GTK_WIDGET(panel));
    n = gdk_screen_get_n_monitors(screen);
    if (monitor >= n) /* hidden now */
        return FALSE;
    gdk_screen_get_monitor_geometry(screen, monitor, &rect);
    switch (edge)
    {
        case EDGE_LEFT:
            rect.width = rect.x;
            rect.x = 0;
            s += rect.width;
            break;
        case EDGE_RIGHT:
            rect.x += rect.width;
            rect.width = gdk_screen_get_width(screen) - rect.x;
            s += rect.width;
            break;
        case EDGE_TOP:
            rect.height = rect.y;
            rect.y = 0;
            s += rect.height;
            break;
        case EDGE_BOTTOM:
            rect.y += rect.height;
            rect.height = gdk_screen_get_height(screen) - rect.y;
            s += rect.height;
            break;
        default: ;
    }
    if (rect.height == 0 || rect.width == 0) ; /* on a border of monitor */
    else
    {
        for (i = 0; i < n; i++)
        {
            if (i == monitor)
                continue;
            gdk_screen_get_monitor_geometry(screen, i, &rect2);
            if (gdk_rectangle_intersect(&rect, &rect2, NULL))
                /* that monitor lies over the edge */
                return FALSE;
        }
    }
    if (G_LIKELY(size))
        *size = s;
    return TRUE;
}

/****************************************************
 *         panel's handlers for WM events           *
 ****************************************************/

void panel_set_wm_strut(Panel *p)
{
    _panel_set_wm_strut(p->topgwin);
}

void _panel_set_wm_strut(LXPanel *panel)
{
    int index;
    Display *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    Panel *p = panel->priv;
    gulong strut_size;
    gulong strut_lower;
    gulong strut_upper;

    if (!gtk_widget_get_mapped(GTK_WIDGET(panel)))
        return;
    /* most wm's tend to ignore struts of unmapped windows, and that's how
     * lxpanel hides itself. so no reason to set it. */
    if (p->autohide && p->height_when_hidden <= 0)
        return;

    /* Dispatch on edge to set up strut parameters. */
    switch (p->edge)
    {
        case EDGE_LEFT:
            index = 0;
            strut_lower = p->ay;
            strut_upper = p->ay + p->ah;
            break;
        case EDGE_RIGHT:
            index = 1;
            strut_lower = p->ay;
            strut_upper = p->ay + p->ah;
            break;
        case EDGE_TOP:
            index = 2;
            strut_lower = p->ax;
            strut_upper = p->ax + p->aw;
            break;
        case EDGE_BOTTOM:
            index = 3;
            strut_lower = p->ax;
            strut_upper = p->ax + p->aw;
            break;
        default:
            return;
    }

    /* Set up strut value in property format. */
    gulong desired_strut[12];
    memset(desired_strut, 0, sizeof(desired_strut));
    if (p->setstrut &&
        _panel_edge_can_strut(panel, p->edge, p->monitor, &strut_size))
    {
        desired_strut[index] = strut_size;
        desired_strut[4 + index * 2] = strut_lower;
        desired_strut[5 + index * 2] = strut_upper - 1;
    }
    else
    {
        strut_size = 0;
        strut_lower = 0;
        strut_upper = 0;
    }

    /* If strut value changed, set the property value on the panel window.
     * This avoids property change traffic when the panel layout is recalculated but strut geometry hasn't changed. */
    if ((p->strut_size != strut_size) || (p->strut_lower != strut_lower) || (p->strut_upper != strut_upper) || (p->strut_edge != p->edge))
    {
        p->strut_size = strut_size;
        p->strut_lower = strut_lower;
        p->strut_upper = strut_upper;
        p->strut_edge = p->edge;

        /* If window manager supports STRUT_PARTIAL, it will ignore STRUT.
         * Set STRUT also for window managers that do not support STRUT_PARTIAL. */
        if (strut_size != 0)
        {
            XChangeProperty(xdisplay, p->topxwin, a_NET_WM_STRUT_PARTIAL,
                XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) desired_strut, 12);
            XChangeProperty(xdisplay, p->topxwin, a_NET_WM_STRUT,
                XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) desired_strut, 4);
        }
        else
        {
            XDeleteProperty(xdisplay, p->topxwin, a_NET_WM_STRUT);
            XDeleteProperty(xdisplay, p->topxwin, a_NET_WM_STRUT_PARTIAL);
        }
    }
}

/****************************************************
 *         panel's handlers for GTK events          *
 ****************************************************/

static void paint_root_pixmap(LXPanel *panel, cairo_t *cr)
{
    /*
     * this code was extracted from code for FbBg object
     *
     * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
     *                     2002 Sun Microsystems, Inc.
     */
    XGCValues gcv;
    uint mask;
    Window xroot;
    GC gc;
    Display *dpy;
    Pixmap *prop;
#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_surface_t *surface;
#else
    GdkPixmap *pixmap;
#endif
    Pixmap xpixmap;
    Panel *p = panel->priv;

    dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    xroot = DefaultRootWindow(dpy);
    gcv.ts_x_origin = 0;
    gcv.ts_y_origin = 0;
    gcv.fill_style = FillTiled;
    mask = GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle;
    prop = get_xaproperty(xroot, a_XROOTPMAP_ID, XA_PIXMAP, NULL);
    if (prop)
    {
        gcv.tile = *prop;
        mask |= GCTile;
        XFree(prop);
    }
    gc = XCreateGC(dpy, xroot, mask, &gcv);
#if GTK_CHECK_VERSION(3, 0, 0)
    xpixmap = XCreatePixmap(dpy, xroot, p->aw, p->ah,
                            DefaultDepth(dpy, DefaultScreen(dpy)));
    surface = cairo_xlib_surface_create(dpy, xpixmap,
                                        DefaultVisual(dpy, DefaultScreen(dpy)),
                                        p->aw, p->ah);
#else
    pixmap = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(panel)),
                            p->aw, p->ah, -1);
    xpixmap = gdk_x11_drawable_get_xid(pixmap);
#endif
    XSetTSOrigin(dpy, gc, -p->ax, -p->ay);
    XFillRectangle(dpy, xpixmap, gc, 0, 0, p->aw, p->ah);
    XFreeGC(dpy, gc);
#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_set_source_surface(cr, surface, 0, 0);
#else
    gdk_cairo_set_source_pixmap(cr, pixmap, 0, 0);
#endif
    cairo_paint(cr);
#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_surface_destroy(surface);
    XFreePixmap(dpy, xpixmap);
#else
    g_object_unref(pixmap);
#endif
}

static void _panel_determine_background_pixmap(LXPanel * panel)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_pattern_t *pattern;
#else
    GdkPixmap * pixmap = NULL;
#endif
    GtkWidget * widget = GTK_WIDGET(panel);
    GdkWindow * window = gtk_widget_get_window(widget);
    Panel * p = panel->priv;
    cairo_t *cr;
    gint x = 0, y = 0;

    if (!p->background && !p->transparent)
        goto not_paintable;
    else if (p->aw <= 1 || p->ah <= 1)
        goto not_paintable;
    else if (p->surface == NULL)
    {
        GdkPixbuf *pixbuf = NULL;

        p->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, p->aw, p->ah);
        cr = cairo_create(p->surface);
        if (p->background)
        {
            /* User specified background pixmap. */
            pixbuf = gdk_pixbuf_new_from_file(p->background_file, NULL);
        }
        if ((p->transparent && p->alpha != 255) || /* ignore it for opaque panel */
            (pixbuf != NULL && gdk_pixbuf_get_has_alpha(pixbuf)))
        {
            /* Transparent.  Determine the appropriate value from the root pixmap. */
            paint_root_pixmap(panel, cr);
        }
        if (pixbuf != NULL)
        {
            gint w = gdk_pixbuf_get_width(pixbuf);
            gint h = gdk_pixbuf_get_height(pixbuf);

            /* Tile the image */
            for (y = 0; y < p->ah; y += h)
                for (x = 0; x < p->aw; x += w)
                {
                    gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
                    cairo_paint(cr);
                }
            y = 0;
            g_object_unref(pixbuf);
        }
        else
        {
            /* Either color is set or image is invalid, fill the background */
            gdk_cairo_set_source_color(cr, &p->gtintcolor);
            cairo_paint_with_alpha(cr, p->transparent ? (double)p->alpha/255 : 1.0);
        }
        cairo_destroy(cr);
    }

    if (p->surface != NULL)
    {
        gtk_widget_set_app_paintable(widget, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
        pattern = cairo_pattern_create_for_surface(p->surface);
        gdk_window_set_background_pattern(window, pattern);
        cairo_pattern_destroy(pattern);
#else
        pixmap = gdk_pixmap_new(window, p->aw, p->ah, -1);
        cr = gdk_cairo_create(pixmap);
        cairo_set_source_surface(cr, p->surface, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        gdk_window_set_back_pixmap(window, pixmap, FALSE);
        g_object_unref(pixmap);
#endif
    }
    else
    {
not_paintable:
        gtk_widget_set_app_paintable(widget, FALSE);
    }
}

void panel_determine_background_pixmap(Panel * panel, GtkWidget * widget, GdkWindow * window)
{
    if (GTK_WIDGET(panel->topgwin) != widget)
    {
        /* Backward compatibility:
           reset background for the child, using background of panel */
        gtk_widget_set_app_paintable(widget, (panel->background || panel->transparent));
#if GTK_CHECK_VERSION(3, 0, 0)
        gdk_window_set_background_pattern(window, NULL);
#else
        gdk_window_set_back_pixmap(window, NULL, TRUE);
#endif
    }
    else
        _panel_determine_background_pixmap(panel->topgwin);
}

/* Update the background of the entire panel.
 * This function should only be called after the panel has been realized. */
void panel_update_background(Panel * p)
{
    _panel_update_background(p->topgwin, TRUE);
}

static void _panel_update_background(LXPanel * p, gboolean enforce)
{
    GtkWidget *w = GTK_WIDGET(p);
    GList *plugins = NULL, *l;

    /* reset background image */
    if (p->priv->surface != NULL) /* FIXME: honor enforce on composited screen */
    {
        cairo_surface_destroy(p->priv->surface);
        p->priv->surface = NULL;
    }

    /* Redraw the top level widget. */
    _panel_determine_background_pixmap(p);
#if !GTK_CHECK_VERSION(3, 0, 0)
    gdk_window_clear(gtk_widget_get_window(w));
#endif
    gtk_widget_queue_draw(w);

    /* Loop over all plugins redrawing each plugin. */
    if (p->priv->box != NULL)
        plugins = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
    for (l = plugins; l != NULL; l = l->next)
        plugin_widget_set_background(l->data, p);
    g_list_free(plugins);
}

/****************************************************
 *         autohide : borrowed from fbpanel         *
 ****************************************************/

/* Autohide is behaviour when panel hides itself when mouse is "far enough"
 * and pops up again when mouse comes "close enough".
 * Formally, it's a state machine with 3 states that driven by mouse
 * coordinates and timer:
 * 1. VISIBLE - ensures that panel is visible. When/if mouse goes "far enough"
 *      switches to WAITING state
 * 2. WAITING - starts timer. If mouse comes "close enough", stops timer and
 *      switches to VISIBLE.  If timer expires, switches to HIDDEN
 * 3. HIDDEN - hides panel. When mouse comes "close enough" switches to VISIBLE
 *
 * Note 1
 * Mouse coordinates are queried every PERIOD milisec
 *
 * Note 2
 * If mouse is less then GAP pixels to panel it's considered to be close,
 * otherwise it's far
 */

#define GAP 2
#define PERIOD 300

typedef enum
{
    AH_STATE_VISIBLE,
    AH_STATE_WAITING,
    AH_STATE_HIDDEN
} PanelAHState;

static void ah_state_set(LXPanel *p, PanelAHState ah_state);

static gboolean
mouse_watch(LXPanel *panel)
{
    Panel *p = panel->priv;
    gint x, y;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    ENTER;
    gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);

/*  Reduce sensitivity area
    p->ah_far = ((x < p->cx - GAP) || (x > p->cx + p->cw + GAP)
        || (y < p->cy - GAP) || (y > p->cy + p->ch + GAP));
*/

    gint cx, cy, cw, ch, gap;

    cx = p->ax;
    cy = p->ay;
    cw = p->cw;
    ch = p->ch;

    if (p->move_state != PANEL_MOVE_STOP)
        /* prevent autohide when dragging is on */
        return TRUE;

    if (cw == 1) cw = 0;
    if (ch == 1) ch = 0;
    /* reduce area which will raise panel so it does not interfere with apps */
    if (p->ah_state == AH_STATE_HIDDEN) {
        gap = MAX(p->height_when_hidden, GAP);
        switch (p->edge) {
        case EDGE_LEFT:
            cw = gap;
            break;
        case EDGE_RIGHT:
            cx = cx + cw - gap;
            cw = gap;
            break;
        case EDGE_TOP:
            ch = gap;
            break;
        case EDGE_BOTTOM:
            cy = cy + ch - gap;
            ch = gap;
            break;
       }
    }
    p->ah_far = ((x < cx) || (x > cx + cw) || (y < cy) || (y > cy + ch));

    ah_state_set(panel, p->ah_state);
    RET(TRUE);
}

static gboolean ah_state_hide_timeout(gpointer p)
{
    if (!g_source_is_destroyed(g_main_current_source()))
    {
        ah_state_set(p, AH_STATE_HIDDEN);
        ((LXPanel *)p)->priv->hide_timeout = 0;
    }
    return FALSE;
}

static void ah_state_set(LXPanel *panel, PanelAHState ah_state)
{
    Panel *p = panel->priv;
    GdkRectangle rect;

    ENTER;
    if (p->ah_state != ah_state) {
        p->ah_state = ah_state;
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            p->visible = TRUE;
            _calculate_position(panel, &rect);
            gtk_window_move(GTK_WINDOW(panel), rect.x, rect.y);
            gtk_widget_show(GTK_WIDGET(panel));
            gtk_widget_show(p->box);
            gtk_widget_queue_resize(GTK_WIDGET(panel));
            gtk_window_stick(GTK_WINDOW(panel));
            break;
        case AH_STATE_WAITING:
            if (p->hide_timeout)
                g_source_remove(p->hide_timeout);
            p->hide_timeout = g_timeout_add(2 * PERIOD, ah_state_hide_timeout, panel);
            break;
        case AH_STATE_HIDDEN:
            if (p->height_when_hidden > 0)
                gtk_widget_hide(p->box);
            else
                gtk_widget_hide(GTK_WIDGET(panel));
            p->visible = FALSE;
        }
    } else if (p->autohide && p->ah_far) {
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            ah_state_set(panel, AH_STATE_WAITING);
            break;
        case AH_STATE_WAITING:
            break;
        case AH_STATE_HIDDEN:
            /* configurator might change height_when_hidden value */
            if (p->height_when_hidden > 0)
            {
                if (gtk_widget_get_visible(p->box))
                {
                    gtk_widget_hide(p->box);
                    gtk_widget_show(GTK_WIDGET(panel));
                }
            }
            else
                if (gtk_widget_get_visible(GTK_WIDGET(panel)))
                {
                    gtk_widget_hide(GTK_WIDGET(panel));
                    gtk_widget_show(p->box);
                }
        }
    } else {
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            break;
        case AH_STATE_WAITING:
            if (p->hide_timeout)
                g_source_remove(p->hide_timeout);
            p->hide_timeout = 0;
            /* continue with setting visible */
        case AH_STATE_HIDDEN:
            ah_state_set(panel, AH_STATE_VISIBLE);
        }
    }
    RET();
}

/* starts autohide behaviour */
static void ah_start(LXPanel *p)
{
    ENTER;
    if (!p->priv->mouse_timeout)
        p->priv->mouse_timeout = g_timeout_add(PERIOD, (GSourceFunc) mouse_watch, p);
    RET();
}

/* stops autohide */
static void ah_stop(LXPanel *p)
{
    ENTER;
    if (p->priv->mouse_timeout) {
        g_source_remove(p->priv->mouse_timeout);
        p->priv->mouse_timeout = 0;
    }
    if (p->priv->hide_timeout) {
        g_source_remove(p->priv->hide_timeout);
        p->priv->hide_timeout = 0;
    }
    RET();
}
/* end of the autohide code
 * ------------------------------------------------------------- */

static gint
panel_popupmenu_configure(GtkWidget *widget, gpointer user_data)
{
    panel_configure( (LXPanel*)user_data, 0 );
    return TRUE;
}

static void panel_popupmenu_config_plugin( GtkMenuItem* item, GtkWidget* plugin )
{
    Panel *panel = PLUGIN_PANEL(plugin)->priv;

    lxpanel_plugin_show_config_dialog(plugin);

    /* FIXME: this should be more elegant */
    panel->config_changed = TRUE;
}

static void panel_popupmenu_add_item( GtkMenuItem* item, LXPanel* panel )
{
    /* panel_add_plugin( panel, panel->topgwin ); */
    panel_configure( panel, 2 );
}

static void panel_popupmenu_remove_item( GtkMenuItem* item, GtkWidget* plugin )
{
    lxpanel_remove_plugin(PLUGIN_PANEL(plugin), plugin);
}

void lxpanel_remove_plugin(LXPanel *p, GtkWidget *plugin)
{
    Panel* panel = p->priv;

    /* If the configuration dialog is open, there will certainly be a crash if the
     * user manipulates the Configured Plugins list, after we remove this entry.
     * Close the configuration dialog if it is open. */
    if (panel->pref_dialog != NULL)
    {
        gtk_widget_destroy(panel->pref_dialog);
        panel->pref_dialog = NULL;
    }
    _lxpanel_remove_plugin(p, plugin);
}

void _lxpanel_remove_plugin(LXPanel *p, GtkWidget *plugin)
{
    Panel* panel = p->priv;
    GtkWidget *prev, *next;
    GList *children;
    GtkAllocation alloc;
    gint idx = -1, size1, size2;
    gboolean expand;

    config_setting_destroy(g_object_get_qdata(G_OBJECT(plugin), lxpanel_plugin_qconf));
    /* reset conf pointer because the widget still may be referenced by configurator */
    g_object_set_qdata(G_OBJECT(plugin), lxpanel_plugin_qconf, NULL);

    /* squash previous and next spaces if this plugin was between two spaces */
    children = gtk_container_get_children(GTK_CONTAINER(panel->box));
    idx = g_list_index(children, plugin);
    if (idx > 0)
    {

        prev = g_list_nth_data(children, idx - 1);
        next = g_list_nth_data(children, idx + 1);
        if (next && PANEL_IS_SPACE(next) && PANEL_IS_SPACE(prev))
        {
            expand = FALSE;
            gtk_container_child_get(GTK_CONTAINER(panel->box), prev, "expand", &expand, NULL);
            if (expand == TRUE)
            {
                /* prev is expandable, remove next */
                config_setting_destroy(g_object_get_qdata(G_OBJECT(next), lxpanel_plugin_qconf));
                g_object_set_qdata(G_OBJECT(next), lxpanel_plugin_qconf, NULL);
                gtk_widget_destroy(next);
            }
            else
            {
                gtk_container_child_get(GTK_CONTAINER(panel->box), next, "expand", &expand, NULL);
                if (expand == TRUE)
                {
                    /* next is expandable, remove prev */
                    config_setting_destroy(g_object_get_qdata(G_OBJECT(prev), lxpanel_plugin_qconf));
                    g_object_set_qdata(G_OBJECT(prev), lxpanel_plugin_qconf, NULL);
                    gtk_widget_destroy(prev);
                }
                else
                {
                    /* calculate size of prev -- add size of this and next to it */
                    size1 = _panel_space_get_size(prev);
                    size2 = _panel_space_get_size(next);
                    gtk_widget_get_allocation(plugin, &alloc);
                    if (panel->orientation == GTK_ORIENTATION_HORIZONTAL)
                        size1 += alloc.width + size2;
                    else
                        size1 += alloc.height + size2;
                    /* remove next */
                    config_setting_destroy(g_object_get_qdata(G_OBJECT(next), lxpanel_plugin_qconf));
                    g_object_set_qdata(G_OBJECT(next), lxpanel_plugin_qconf, NULL);
                    gtk_widget_destroy(next);
                    _panel_space_resize(prev, size1);
                }
            }
        }
    }
    g_list_free(children);

    lxpanel_config_save(p);
    gtk_widget_destroy(plugin);
}

/* FIXME: Potentially we can support multiple panels at the same edge,
 * but currently this cannot be done due to some positioning problems. */
static char* gen_panel_name( int edge, gint monitor )
{
    const char* edge_str = num2str( edge_pair, edge, "" );
    char* name = NULL;
    char* dir = _user_config_file_name("panels", NULL);
    int i;
    for( i = 0; i < G_MAXINT; ++i )
    {
        char* f;
        if(monitor != 0)
            name = g_strdup_printf( "%s-m%d-%d", edge_str, monitor, i );
        else if( G_LIKELY( i > 0 ) )
            name =  g_strdup_printf( "%s%d", edge_str, i );
        else
            name = g_strdup( edge_str );

        f = g_build_filename( dir, name, NULL );
        if( ! g_file_test( f, G_FILE_TEST_EXISTS ) )
        {
            g_free( f );
            break;
        }
        g_free( name );
        g_free( f );
    }
    g_free( dir );
    return name;
}

/* FIXME: Potentially we can support multiple panels at the same edge,
 * but currently this cannot be done due to some positioning problems. */
static void panel_popupmenu_create_panel( GtkMenuItem* item, LXPanel* panel )
{
    gint m, e, monitors;
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(panel));
    LXPanel *new_panel = panel_allocate(screen);
    Panel *p = new_panel->priv;
    config_setting_t *global;

    /* Allocate the edge. */
    g_assert(screen);
    monitors = gdk_screen_get_n_monitors(screen);
    /* try to allocate edge on current monitor first */
    m = panel->priv->monitor;
    if (m < 0)
    {
        /* panel is spanned over the screen, guess from pointer now */
        gint x, y;
        gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);
        m = gdk_screen_get_monitor_at_point(screen, x, y);
    }
    for (e = 1; e < 5; ++e)
    {
        if (panel_edge_available(p, e, m))
        {
            p->edge = e;
            p->monitor = m;
            goto found_edge;
        }
    }
    /* try all monitors */
    for(m=0; m<monitors; ++m)
    {
        /* try each of the four edges */
        for(e=1; e<5; ++e)
        {
            if(panel_edge_available(p,e,m)) {
                p->edge = e;
                p->monitor = m;
                goto found_edge;
            }
        }
    }

    gtk_widget_destroy(GTK_WIDGET(new_panel));
    g_warning("Error adding panel: There is no room for another panel. All the edges are taken.");
    fm_show_error(NULL, NULL, _("There is no room for another panel. All the edges are taken."));
    return;

found_edge:
    p->name = gen_panel_name(p->edge, p->monitor);

    /* create new config with first group "Global" */
    global = config_group_add_subgroup(config_root_setting(p->config), "Global");
    config_group_set_string(global, "edge", num2str(edge_pair, p->edge, "none"));
    config_group_set_int(global, "monitor", p->monitor);
    panel_configure(new_panel, 0);
    panel_normalize_configuration(p);
    panel_start_gui(new_panel, NULL);

    lxpanel_config_save(new_panel);
    all_panels = g_slist_prepend(all_panels, new_panel);
}

static void panel_popupmenu_delete_panel( GtkMenuItem* item, LXPanel* panel )
{
    GtkWidget* dlg;
    gboolean ok;
    dlg = gtk_message_dialog_new_with_markup( GTK_WINDOW(panel),
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_QUESTION,
                                                    GTK_BUTTONS_OK_CANCEL,
                                                    _("Really delete this panel?\n<b>Warning: This can not be recovered.</b>") );
    panel_apply_icon(GTK_WINDOW(dlg));
    gtk_window_set_title( (GtkWindow*)dlg, _("Confirm") );
    ok = ( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK );
    gtk_widget_destroy( dlg );
    if( ok )
    {
        gchar *fname;
        all_panels = g_slist_remove( all_panels, panel );

        /* delete the config file of this panel */
        fname = _user_config_file_name("panels", panel->priv->name);
        g_unlink( fname );
        g_free(fname);
        panel->priv->config_changed = 0;
        gtk_widget_destroy(GTK_WIDGET(panel));
    }
}

static void panel_popupmenu_about( GtkMenuItem* item, Panel* panel )
{
    GtkWidget *about;
    const gchar* authors[] = {
        "Hong Jen Yee (PCMan) <pcman.tw@gmail.com>",
        "Jim Huang <jserv.tw@gmail.com>",
        "Greg McNew <gmcnew@gmail.com> (battery plugin)",
        "Fred Chien <cfsghost@gmail.com>",
        "Daniel Kesler <kesler.daniel@gmail.com>",
        "Juergen Hoetzel <juergen@archlinux.org>",
        "Marty Jack <martyj19@comcast.net>",
        "Martin Bagge <brother@bsnet.se>",
        "Andriy Grytsenko <andrej@rep.kiev.ua>",
        "Giuseppe Penone <giuspen@gmail.com>",
        "Piotr Sipika <piotr.sipika@gmail.com>",
        NULL
    };
    /* TRANSLATORS: Replace this string with your names, one name per line. */
    gchar *translators = _( "translator-credits" );

    about = gtk_about_dialog_new();
    panel_apply_icon(GTK_WINDOW(about));
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), _("LXPanel"));

    if(gtk_icon_theme_has_icon(panel->icon_theme, "video-display"))
    {
         gtk_about_dialog_set_logo( GTK_ABOUT_DIALOG(about),
                                    gtk_icon_theme_load_icon(panel->icon_theme, "video-display", 48, 0, NULL));
    }
    else if (gtk_icon_theme_has_icon(panel->icon_theme, "start-here"))
    {
         gtk_about_dialog_set_logo( GTK_ABOUT_DIALOG(about),
                                    gtk_icon_theme_load_icon(panel->icon_theme, "start-here", 48, 0, NULL));
    }
    else
    {
        gtk_about_dialog_set_logo(  GTK_ABOUT_DIALOG(about),
                                    gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/images/my-computer.png", NULL));
    }

    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about), _("Copyright (C) 2008-2021"));
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), _( "Desktop panel for LXDE project"));
    gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about), "This program is free software; you can redistribute it and/or\nmodify it under the terms of the GNU General Public License\nas published by the Free Software Foundation; either version 2\nof the License, or (at your option) any later version.\n\nThis program is distributed in the hope that it will be useful,\nbut WITHOUT ANY WARRANTY; without even the implied warranty of\nMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\nGNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License\nalong with this program; if not, write to the Free Software\nFoundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), "http://lxde.org/");
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about), translators);
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_destroy(about);
}

void panel_apply_icon( GtkWindow *w )
{
    GdkPixbuf* window_icon;

    if(gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "video-display"))
    {
        window_icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "video-display", 24, 0, NULL);
    }
    else if(gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "start-here"))
    {
        window_icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "start-here", 24, 0, NULL);
    }
    else
    {
        window_icon = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/images/my-computer.png", NULL);
    }
    gtk_window_set_icon(w, window_icon);
}

GtkMenu* lxpanel_get_plugin_menu( LXPanel* panel, GtkWidget* plugin, gboolean use_sub_menu )
{
    GtkWidget  *menu_item, *img;
    GtkMenu *ret,*menu;
    const LXPanelPluginInit *init;
    char* tmp;

    ret = menu = GTK_MENU(gtk_menu_new());

    if (plugin)
    {
        init = PLUGIN_CLASS(plugin);
        /* create single item - plugin instance settings */
        img = gtk_image_new_from_stock( GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU );
        tmp = g_strdup_printf(_("\"%s\" Settings"),
                              g_dgettext(init->gettext_package, init->name));
        menu_item = gtk_image_menu_item_new_with_label( tmp );
        g_free( tmp );
        gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
        gtk_menu_shell_prepend(GTK_MENU_SHELL(ret), menu_item);
        if( init->config )
            g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_config_plugin), plugin );
        else
            gtk_widget_set_sensitive( menu_item, FALSE );
        /* add custom items by plugin if requested */
        if (init->update_context_menu != NULL)
            use_sub_menu = init->update_context_menu(plugin, ret);
        /* append a separator */
        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(ret), menu_item);
    }
    if (use_sub_menu)
        menu = GTK_MENU(gtk_menu_new());

    img = gtk_image_new_from_stock( GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU );
    menu_item = gtk_image_menu_item_new_with_label(_("Add / Remove Panel Items"));
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_add_item), panel );

    if( plugin )
    {
        img = gtk_image_new_from_stock( GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU );
        tmp = g_strdup_printf( _("Remove \"%s\" From Panel"), _(init->name) );
        menu_item = gtk_image_menu_item_new_with_label( tmp );
        g_free( tmp );
        gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_remove_item), plugin );
    }

    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    img = gtk_image_new_from_stock( GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU );
    menu_item = gtk_image_menu_item_new_with_label(_("Panel Settings"));
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(panel_popupmenu_configure), panel );

    img = gtk_image_new_from_stock( GTK_STOCK_NEW, GTK_ICON_SIZE_MENU );
    menu_item = gtk_image_menu_item_new_with_label(_("Create New Panel"));
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_create_panel), panel );

    img = gtk_image_new_from_stock( GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU );
    menu_item = gtk_image_menu_item_new_with_label(_("Delete This Panel"));
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_delete_panel), panel );
    if( ! all_panels->next )    /* if this is the only panel */
        gtk_widget_set_sensitive( menu_item, FALSE );

    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    img = gtk_image_new_from_stock( GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU );
    menu_item = gtk_image_menu_item_new_with_label(_("About"));
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_about), panel->priv );

    if( use_sub_menu )
    {
        menu_item = gtk_image_menu_item_new_with_label(_("Panel"));
        gtk_menu_shell_append(GTK_MENU_SHELL(ret), menu_item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(menu) );
    }

    gtk_widget_show_all(GTK_WIDGET(ret));

    g_signal_connect( ret, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL );
    return ret;
}

/* for old plugins compatibility */
GtkMenu* lxpanel_get_panel_menu( Panel* panel, Plugin* plugin, gboolean use_sub_menu )
{
    return lxpanel_get_plugin_menu(panel->topgwin, plugin->pwid, use_sub_menu);
}

/****************************************************
 *         panel creation                           *
 ****************************************************/

static void
make_round_corners(Panel *p)
{
    /* FIXME: This should be re-written with shape extension of X11 */
    /* gdk_window_shape_combine_mask() can be used */
}

void panel_set_dock_type(Panel *p)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    if (p->setdocktype) {
        Atom state = a_NET_WM_WINDOW_TYPE_DOCK;
        XChangeProperty(xdisplay, p->topxwin,
                        a_NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *) &state, 1);
    }
    else {
        XDeleteProperty( xdisplay, p->topxwin, a_NET_WM_WINDOW_TYPE );
    }
}

void panel_establish_autohide(Panel *p)
{
    _panel_establish_autohide(p->topgwin);
}

void _panel_establish_autohide(LXPanel *p)
{
    if (p->priv->autohide)
        ah_start(p);
    else
    {
        ah_stop(p);
        ah_state_set(p, AH_STATE_VISIBLE);
    }
}

/* Set an image from a file with scaling to the panel icon size. */
void panel_image_set_from_file(Panel * p, GtkWidget * image, const char * file)
{
    GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file_at_scale(file, p->icon_size, p->icon_size, TRUE, NULL);
    if (pixbuf != NULL)
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
    }
}

void lxpanel_image_set_from_file(LXPanel * p, GtkWidget * image, const char * file)
{
    panel_image_set_from_file(p->priv, image, file);
}

/* Set an image from a icon theme with scaling to the panel icon size. */
gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon)
{
    if (gtk_icon_theme_has_icon(p->icon_theme, icon))
    {
        GdkPixbuf * pixbuf = gtk_icon_theme_load_icon(p->icon_theme, icon, p->icon_size, 0, NULL);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
        return TRUE;
    }
    return FALSE;
}

gboolean lxpanel_image_set_icon_theme(LXPanel * p, GtkWidget * image, const gchar * icon)
{
    return panel_image_set_icon_theme(p->priv, image, icon);
}

static int
panel_parse_plugin(LXPanel *p, config_setting_t *cfg)
{
    const char *type = NULL;

    ENTER;
    config_setting_lookup_string(cfg, "type", &type);
    DBG("plug %s\n", type);

    if (!type || lxpanel_add_plugin(p, type, cfg, -1) == NULL) {
        g_warning( "lxpanel: can't load %s plugin", type);
        goto error;
    }
    RET(1);

error:
    RET(0);
}

static void
panel_start_gui(LXPanel *panel, config_setting_t *list)
{
    Atom state[3];
    XWMHints wmhints;
    gulong val;
    Screen *xscreen = GDK_SCREEN_XSCREEN(gtk_widget_get_screen(GTK_WIDGET(panel)));
    Display *xdisplay = DisplayOfScreen(xscreen);
    Panel *p = panel->priv;
    GtkWidget *w = GTK_WIDGET(panel);
    config_setting_t *s;
    GdkRectangle rect;
    int i;

    ENTER;

    g_debug("panel_start_gui on '%s'", p->name);
    p->curdesk = get_net_current_desktop();
    p->desknum = get_net_number_of_desktops();
    //p->workarea = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_WORKAREA, XA_CARDINAL, &p->wa_len);
    p->ax = p->ay = p->aw = p->ah = 0;

    p->display = gdk_display_get_default();
    gtk_window_set_wmclass(GTK_WINDOW(panel), "panel", "lxpanel");

    if (G_UNLIKELY(win_grp == NULL))
    {
        win_grp = gtk_window_group_new();
        g_object_add_weak_pointer(G_OBJECT(win_grp), (gpointer *)&win_grp);
        gtk_window_group_add_window(win_grp, (GtkWindow*)panel);
        g_object_unref(win_grp);
    }
    else
        gtk_window_group_add_window(win_grp, (GtkWindow*)panel);

    gtk_widget_add_events( w, GDK_BUTTON_PRESS_MASK );

    gtk_widget_realize(w);
    //gdk_window_set_decorations(gtk_widget_get_window(p->topgwin), 0);

    // main layout manager as a single child of panel
    p->box = panel_box_new(panel, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(p->box), 0);
    gtk_container_add(GTK_CONTAINER(panel), p->box);
    gtk_widget_show(p->box);
    if (p->round_corners)
        make_round_corners(p);

    p->topxwin = GDK_WINDOW_XID(gtk_widget_get_window(w));
    DBG("topxwin = %x\n", p->topxwin);

    /* the settings that should be done before window is mapped */
    wmhints.flags = InputHint;
    wmhints.input = 0;
    XSetWMHints (xdisplay, p->topxwin, &wmhints);
#define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* "alt-tab" skips this win */
    val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(xdisplay, p->topxwin,
          XInternAtom(xdisplay, "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    panel_set_dock_type(p);

    /* window mapping point */
    p->visible = TRUE;
    _calculate_position(panel, &rect);
    gtk_window_move(GTK_WINDOW(panel), rect.x, rect.y);
    gtk_window_present(GTK_WINDOW(panel));

    /* the settings that should be done after window is mapped */

    /* send it to running wm */
    Xclimsgx(xscreen, p->topxwin, a_NET_WM_DESKTOP, G_MAXULONG, 0, 0, 0, 0);
    /* and assign it ourself just for case when wm is not running */
    val = G_MAXULONG;
    XChangeProperty(xdisplay, p->topxwin, a_NET_WM_DESKTOP, XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    state[0] = a_NET_WM_STATE_SKIP_PAGER;
    state[1] = a_NET_WM_STATE_SKIP_TASKBAR;
    state[2] = a_NET_WM_STATE_STICKY;
    XChangeProperty(xdisplay, p->topxwin, a_NET_WM_STATE, XA_ATOM,
          32, PropModeReplace, (unsigned char *) state, 3);

    p->initialized = TRUE;

    if (list) for (i = 1; (s = config_setting_get_elem(list, i)) != NULL; )
    {
        if (strcmp(config_setting_get_name(s), "Plugin") == 0 &&
            panel_parse_plugin(panel, s)) /* success on plugin start */
            i++;
        else /* remove invalid data from config */
            config_setting_remove_elem(list, i);
    }

    RET();
}

/* Exchange the "width" and "height" terminology for vertical and horizontal panels. */
void panel_adjust_geometry_terminology(Panel * p)
{
    if ((p->height_label != NULL) && (p->width_label != NULL)
    && (p->alignment_left_label != NULL) && (p->alignment_right_label != NULL))
    {
        if ((p->edge == EDGE_TOP) || (p->edge == EDGE_BOTTOM))
        {
            gtk_label_set_text(GTK_LABEL(p->height_label), _("Height:"));
            gtk_label_set_text(GTK_LABEL(p->width_label), _("Width:"));
            gtk_button_set_label(GTK_BUTTON(p->alignment_left_label), _("Left"));
            gtk_button_set_label(GTK_BUTTON(p->alignment_right_label), _("Right"));
        }
        else
        {
            gtk_label_set_text(GTK_LABEL(p->height_label), _("Width:"));
            gtk_label_set_text(GTK_LABEL(p->width_label), _("Height:"));
            gtk_button_set_label(GTK_BUTTON(p->alignment_left_label), _("Top"));
            gtk_button_set_label(GTK_BUTTON(p->alignment_right_label), _("Bottom"));
        }
    }
}

/* Draw text into a label, with the user preference color and optionally bold. */
static
void panel_draw_label_text_with_color(Panel * p, GtkWidget * label, const char * text,
                           gboolean bold, float custom_size_factor,
                           gboolean custom_color, GdkColor *gdkcolor)
{
    if (text == NULL)
    {
        /* Null string. */
        gtk_label_set_text(GTK_LABEL(label), NULL);
        return;
    }

    /* Compute an appropriate size so the font will scale with the panel's icon size. */
    int font_desc;
    if (p->usefontsize)
        font_desc = p->fontsize;
    else
    {
        GtkStyle *style = gtk_widget_get_style(label);
        font_desc = pango_font_description_get_size(style->font_desc) / PANGO_SCALE;
    }
    font_desc *= custom_size_factor;

    /* Check the string for characters that need to be escaped.
     * If any are found, create the properly escaped string and use it instead. */
    const char * valid_markup = text;
    char * escaped_text = NULL;
    const char * q;
    for (q = text; *q != '\0'; q += 1)
    {
        if ((*q == '<') || (*q == '>') || (*q == '&'))
        {
            escaped_text = g_markup_escape_text(text, -1);
            valid_markup = escaped_text;
            break;
        }
    }

    gchar * formatted_text;
    if (gdkcolor || ((custom_color) && (p->usefontcolor)))
    {
        /* Color, optionally bold. */
        guint32 rgb24 = gdkcolor ? gcolor2rgb24(gdkcolor) : gcolor2rgb24(&p->gfontcolor);
        formatted_text = g_strdup_printf("<span font_desc=\"%d\" color=\"#%06x\">%s%s%s</span>",
                font_desc,
                rgb24,
                ((bold) ? "<b>" : ""),
                valid_markup,
                ((bold) ? "</b>" : ""));
    }
    else
    {
        /* No color, optionally bold. */
        formatted_text = g_strdup_printf("<span font_desc=\"%d\">%s%s%s</span>",
                font_desc,
                ((bold) ? "<b>" : ""),
                valid_markup,
                ((bold) ? "</b>" : ""));
    }

    gtk_label_set_markup(GTK_LABEL(label), formatted_text);
    g_free(formatted_text);
    g_free(escaped_text);
}

void panel_draw_label_text(Panel * p, GtkWidget * label, const char * text,
                           gboolean bold, float custom_size_factor,
                           gboolean custom_color)
{
    panel_draw_label_text_with_color(p, label, text, bold, custom_size_factor, custom_color, NULL);
}

void lxpanel_draw_label_text(LXPanel * p, GtkWidget * label, const char * text,
                           gboolean bold, float custom_size_factor,
                           gboolean custom_color)
{
    panel_draw_label_text(p->priv, label, text, bold, custom_size_factor, custom_color);
}

void lxpanel_draw_label_text_with_color(LXPanel * p, GtkWidget * label, const char * text,
                                    gboolean bold, float custom_size_factor,
                                    GdkColor *color)
{
    panel_draw_label_text_with_color(p->priv, label, text, bold, custom_size_factor, FALSE, color);
}

void panel_set_panel_configuration_changed(Panel *p)
{
    _panel_set_panel_configuration_changed(p->topgwin);
}

static inline void _update_orientation(Panel *p)
{
    p->orientation = (p->edge == EDGE_TOP || p->edge == EDGE_BOTTOM)
                        ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
}

static gboolean _panel_idle_reconfigure(gpointer widget)
{
    LXPanel *panel;
    Panel *p;
    GList *plugins, *l;
    GtkOrientation previous_orientation;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    panel = LXPANEL(widget);
    p = panel->priv;
    previous_orientation = p->orientation;
    _update_orientation(p);

    /* either first run or orientation was changed */
    if (previous_orientation != p->orientation)
    {
        panel_adjust_geometry_terminology(p);
        p->height = ((p->orientation == GTK_ORIENTATION_HORIZONTAL) ? PANEL_HEIGHT_DEFAULT : PANEL_WIDTH_DEFAULT);
        if (p->height_control != NULL)
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->height_control), p->height);
        if ((p->widthtype == WIDTH_PIXEL) && (p->width_control != NULL))
        {
            int value = ((p->orientation == GTK_ORIENTATION_HORIZONTAL) ? gdk_screen_width() : gdk_screen_height());
            gtk_spin_button_set_range(GTK_SPIN_BUTTON(p->width_control), 0, value);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->width_control), value);
        }
    }

    /* FIXME: it's deprecated, kept for binary compatibility */
    if (p->orientation == GTK_ORIENTATION_HORIZONTAL) {
        p->my_box_new = gtk_hbox_new;
        p->my_separator_new = gtk_vseparator_new;
    } else {
        p->my_box_new = gtk_vbox_new;
        p->my_separator_new = gtk_hseparator_new;
    }

    /* recreate the main layout box */
    if (p->box != NULL)
    {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(p->box), p->orientation);
    }

    /* NOTE: This loop won't be executed when panel started since
       plugins are not loaded at that time.
       This is used when the orientation of the panel is changed
       from the config dialog, and plugins should be re-layout.
    */
    plugins = p->box ? gtk_container_get_children(GTK_CONTAINER(p->box)) : NULL;
    for( l = plugins; l; l = l->next ) {
        GtkWidget *w = (GtkWidget*)l->data;
        const LXPanelPluginInit *init = PLUGIN_CLASS(w);
        if (init->reconfigure)
            init->reconfigure(panel, w);
    }
    g_list_free(plugins);
    /* panel geometry changed? update panel background then */
    _panel_queue_update_background(panel);

    p->reconfigure_queued = 0;

    return FALSE;
}

void _panel_set_panel_configuration_changed(LXPanel *panel)
{
    if (panel->priv->reconfigure_queued)
        return;
    panel->priv->reconfigure_queued = g_idle_add(_panel_idle_reconfigure, panel);
}

static int
panel_parse_global(Panel *p, config_setting_t *cfg)
{
    const char *str;
    gint i;

    /* check Global config */
    if (!cfg || strcmp(config_setting_get_name(cfg), "Global") != 0)
    {
        g_warning( "lxpanel: Global section not found");
        RET(0);
    }
    if (config_setting_lookup_string(cfg, "edge", &str))
        p->edge = str2num(edge_pair, str, EDGE_NONE);
    if (config_setting_lookup_string(cfg, "align", &str) ||
        /* NOTE: supporting "allign" for backward compatibility */
        config_setting_lookup_string(cfg, "allign", &str))
        p->align = str2num(allign_pair, str, ALIGN_NONE);
    config_setting_lookup_int(cfg, "monitor", &p->monitor);
    config_setting_lookup_int(cfg, "margin", &p->margin);
    if (config_setting_lookup_string(cfg, "widthtype", &str))
        p->widthtype = str2num(width_pair, str, WIDTH_NONE);
    config_setting_lookup_int(cfg, "width", &p->width);
    if (config_setting_lookup_string(cfg, "heighttype", &str))
        p->heighttype = str2num(height_pair, str, HEIGHT_NONE);
    config_setting_lookup_int(cfg, "height", &p->height);
    if (config_setting_lookup_int(cfg, "spacing", &i) && i > 0)
        p->spacing = i;
    if (config_setting_lookup_int(cfg, "setdocktype", &i))
        p->setdocktype = i != 0;
    if (config_setting_lookup_int(cfg, "setpartialstrut", &i))
        p->setstrut = i != 0;
    if (config_setting_lookup_int(cfg, "RoundCorners", &i))
        p->round_corners = i != 0;
    if (config_setting_lookup_int(cfg, "transparent", &i))
        p->transparent = i != 0;
    if (config_setting_lookup_int(cfg, "alpha", &p->alpha))
    {
        if (p->alpha > 255)
            p->alpha = 255;
    }
    if (config_setting_lookup_int(cfg, "autohide", &i))
        p->autohide = i != 0;
    if (config_setting_lookup_int(cfg, "heightwhenhidden", &i))
        p->height_when_hidden = MAX(0, i);
    if (config_setting_lookup_string(cfg, "tintcolor", &str))
    {
        if (!gdk_color_parse (str, &p->gtintcolor))
            gdk_color_parse ("white", &p->gtintcolor);
        p->tintcolor = gcolor2rgb24(&p->gtintcolor);
            DBG("tintcolor=%x\n", p->tintcolor);
    }
    if (config_setting_lookup_int(cfg, "usefontcolor", &i))
        p->usefontcolor = i != 0;
    if (config_setting_lookup_string(cfg, "fontcolor", &str))
    {
        if (!gdk_color_parse (str, &p->gfontcolor))
            gdk_color_parse ("black", &p->gfontcolor);
        p->fontcolor = gcolor2rgb24(&p->gfontcolor);
            DBG("fontcolor=%x\n", p->fontcolor);
    }
    if (config_setting_lookup_int(cfg, "usefontsize", &i))
        p->usefontsize = i != 0;
    if (config_setting_lookup_int(cfg, "fontsize", &i) && i > 0)
        p->fontsize = i;
    if (config_setting_lookup_int(cfg, "background", &i))
        p->background = i != 0;
    if (config_setting_lookup_string(cfg, "backgroundfile", &str))
        p->background_file = g_strdup(str);
    config_setting_lookup_int(cfg, "iconsize", &p->icon_size);

    _update_orientation(p);
    panel_normalize_configuration(p);

    return 1;
}

static void on_monitors_changed(GdkScreen* screen, gpointer unused)
{
    GSList *pl;
    int monitors = gdk_screen_get_n_monitors(screen);

    for (pl = all_panels; pl; pl = pl->next)
    {
        LXPanel *p = pl->data;

        /* handle connecting and disconnecting monitors now */
        if (p->priv->monitor < monitors && !p->priv->initialized)
            panel_start_gui(p, config_setting_get_member(config_root_setting(p->priv->config), ""));
        else if (p->priv->monitor >= monitors && p->priv->initialized)
            panel_stop_gui(p);
        /* resize panel if appropriate monitor changed its size or position */
        else
        {
            /* SF bug #666: after screen resize panel cannot establish
               right size since cannot be moved while is hidden */
            ah_state_set(p, AH_STATE_VISIBLE);
            gtk_widget_queue_resize(GTK_WIDGET(p));
        }
    }
}

static int panel_start(LXPanel *p)
{
    config_setting_t *list;
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(p));

    /* parse global section */
    ENTER;

    list = config_setting_get_member(config_root_setting(p->priv->config), "");
    if (!list || !panel_parse_global(p->priv, config_setting_get_elem(list, 0)))
        RET(0);

    if (p->priv->monitor < gdk_screen_get_n_monitors(screen))
        panel_start_gui(p, list);
    if (monitors_handler == 0)
        monitors_handler = g_signal_connect(screen, "monitors-changed",
                                            G_CALLBACK(on_monitors_changed), NULL);
    return 1;
}

void panel_destroy(Panel *p)
{
    gtk_widget_destroy(GTK_WIDGET(p->topgwin));
}

LXPanel* panel_new( const char* config_file, const char* config_name )
{
    LXPanel* panel = NULL;

    if (G_LIKELY(config_file))
    {
        panel = panel_allocate(gdk_screen_get_default());
        panel->priv->name = g_strdup(config_name);
        g_debug("starting panel from file %s",config_file);
        if (!config_read_file(panel->priv->config, config_file) ||
            !panel_start(panel))
        {
            g_warning( "lxpanel: can't start panel");
            gtk_widget_destroy(GTK_WIDGET(panel));
            panel = NULL;
        }
    }
    return panel;
}


GtkOrientation panel_get_orientation(LXPanel *panel)
{
    return panel->priv->orientation;
}

gint panel_get_icon_size(LXPanel *panel)
{
    return panel->priv->icon_size;
}

gint panel_get_height(LXPanel *panel)
{
    return panel->priv->height;
}

Window panel_get_xwindow(LXPanel *panel)
{
    return panel->priv->topxwin;
}

gint panel_get_monitor(LXPanel *panel)
{
    return panel->priv->monitor;
}

GtkStyle *panel_get_defstyle(LXPanel *panel)
{
    return panel->priv->defstyle;
}

GtkIconTheme *panel_get_icon_theme(LXPanel *panel)
{
    return panel->priv->icon_theme;
}

gboolean panel_is_at_bottom(LXPanel *panel)
{
    return panel->priv->edge == EDGE_BOTTOM;
}

gboolean panel_is_dynamic(LXPanel *panel)
{
    return panel->priv->widthtype == WIDTH_REQUEST;
}

GtkWidget *panel_box_new(LXPanel *panel, gboolean homogeneous, gint spacing)
{
    if (panel->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        return gtk_hbox_new(homogeneous, spacing);
    return gtk_vbox_new(homogeneous, spacing);
}

GtkWidget *panel_separator_new(LXPanel *panel)
{
    if (panel->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        return gtk_vseparator_new();
    return gtk_hseparator_new();
}

gboolean _class_is_present(const LXPanelPluginInit *init)
{
    GSList *sl;

    for (sl = all_panels; sl; sl = sl->next )
    {
        LXPanel *panel = (LXPanel*)sl->data;
        GList *plugins, *p;

        if (panel->priv->box == NULL)
            continue;
        plugins = gtk_container_get_children(GTK_CONTAINER(panel->priv->box));
        for (p = plugins; p; p = p->next)
            if (PLUGIN_CLASS(p->data) == init)
            {
                g_list_free(plugins);
                return TRUE;
            }
        g_list_free(plugins);
    }
    return FALSE;
}
