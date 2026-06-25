/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __XFDESKTOP_ICON_H__
#define __XFDESKTOP_ICON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON            (xfdesktop_icon_get_type())
#define XFDESKTOP_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON, XfdesktopIcon))
#define XFDESKTOP_ICON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), XFDESKTOP_TYPE_ICON, XfdesktopIconClass))
#define XFDESKTOP_IS_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON))
#define XFDESKTOP_ICON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_ICON, XfdesktopIconClass))

typedef struct _XfdesktopIcon        XfdesktopIcon;
typedef struct _XfdesktopIconClass   XfdesktopIconClass;
typedef struct _XfdesktopIconPrivate XfdesktopIconPrivate;

struct _XfdesktopIcon
{
    GObject parent;

    /*< private >*/
    XfdesktopIconPrivate *priv;
};

struct _XfdesktopIconClass
{
    GObjectClass parent;

    /*< signals >*/
    void (*pixbuf_changed)(XfdesktopIcon *icon);
    void (*label_changed)(XfdesktopIcon *icon);

    void (*position_changed)(XfdesktopIcon *icon);

    void (*selected)(XfdesktopIcon *icon);
    /* XfdektopIcon::activated has weird semantics: you should NEVER connect to
     * this signal normally: always use g_signal_connect_after(), as the default
     * signal handler may do some special setup for the icon.  this is lame;
     * you should be able to use normal g_signal_connect(), but signal handlers
     * with return values are (for some unknown reason) not allowed to be
     * G_SIGNAL_RUN_FIRST.  go figure. */
    gboolean (*activated)(XfdesktopIcon *icon);

    /*< virtual functions >*/
    GdkPixbuf *(*peek_pixbuf)(XfdesktopIcon *icon, gint width, gint height);
    const gchar *(*peek_label)(XfdesktopIcon *icon);

    GdkDragAction (*get_allowed_drag_actions)(XfdesktopIcon *icon);

    GdkDragAction (*get_allowed_drop_actions)(XfdesktopIcon *icon, GdkDragAction *suggested_action);
    gboolean (*do_drop_dest)(XfdesktopIcon *icon, GList *src_icons, GdkDragAction action);

    GdkPixbuf *(*peek_tooltip_pixbuf)(XfdesktopIcon *icon, gint width, gint height);
    const gchar *(*peek_tooltip)(XfdesktopIcon *icon);

    gchar *(*get_identifier)(XfdesktopIcon *icon);

    void (*set_thumbnail_file)(XfdesktopIcon *icon, GFile *file);
    void (*delete_thumbnail_file)(XfdesktopIcon *icon);

    gboolean (*populate_context_menu)(XfdesktopIcon *icon,
                                      GtkWidget *menu);
};

GType xfdesktop_icon_get_type(void) G_GNUC_CONST;

/* xfdesktop virtual function accessors */

GdkPixbuf *xfdesktop_icon_peek_pixbuf(XfdesktopIcon *icon,
                                     gint width,
                                     gint height);
const gchar *xfdesktop_icon_peek_label(XfdesktopIcon *icon);
GdkPixbuf *xfdesktop_icon_peek_tooltip_pixbuf(XfdesktopIcon *icon,
                                              gint width,
                                              gint height);
const gchar *xfdesktop_icon_peek_tooltip(XfdesktopIcon *icon);

/* returns a unique identifier for the icon, free when done using it */
gchar *xfdesktop_icon_get_identifier(XfdesktopIcon *icon);

void xfdesktop_icon_set_position(XfdesktopIcon *icon,
                                 gint16 row,
                                 gint16 col);
gboolean xfdesktop_icon_get_position(XfdesktopIcon *icon,
                                     gint16 *row,
                                     gint16 *col);

GdkDragAction xfdesktop_icon_get_allowed_drag_actions(XfdesktopIcon *icon);

GdkDragAction xfdesktop_icon_get_allowed_drop_actions(XfdesktopIcon *icon,
                                                      GdkDragAction *suggested_action);
gboolean xfdesktop_icon_do_drop_dest(XfdesktopIcon *icon,
                                     GList *src_icons,
                                     GdkDragAction action);

gboolean xfdesktop_icon_populate_context_menu(XfdesktopIcon *icon,
                                              GtkWidget *menu);

GtkWidget *xfdesktop_icon_peek_icon_view(XfdesktopIcon *icon);

void xfdesktop_icon_set_thumbnail_file(XfdesktopIcon *icon, GFile *file);
void xfdesktop_icon_delete_thumbnail(XfdesktopIcon *icon);

void xfdesktop_icon_invalidate_regular_pixbuf(XfdesktopIcon *icon);
void xfdesktop_icon_invalidate_tooltip_pixbuf(XfdesktopIcon *icon);
void xfdesktop_icon_invalidate_pixbuf(XfdesktopIcon *icon);

/*< signal triggers >*/

void xfdesktop_icon_pixbuf_changed(XfdesktopIcon *icon);
void xfdesktop_icon_label_changed(XfdesktopIcon *icon);
void xfdesktop_icon_position_changed(XfdesktopIcon *icon);

void xfdesktop_icon_selected(XfdesktopIcon *icon);
gboolean xfdesktop_icon_activated(XfdesktopIcon *icon);
void xfdesktop_icon_activated_g_func(gpointer data,
                                     gpointer user_data);

/*< private-ish; only for use by XfdesktopIconView >*/
void xfdesktop_icon_set_extents(XfdesktopIcon *icon,
                                const GdkRectangle *pixbuf_extents,
                                const GdkRectangle *text_extents,
                                const GdkRectangle *total_extents);
gboolean xfdesktop_icon_get_extents(XfdesktopIcon *icon,
                                    GdkRectangle *pixbuf_extents,
                                    GdkRectangle *text_extents,
                                    GdkRectangle *total_extents);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_H__ */
