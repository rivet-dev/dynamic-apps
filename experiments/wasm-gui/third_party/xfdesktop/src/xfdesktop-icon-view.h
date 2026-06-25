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

#ifndef __XFDESKTOP_ICON_VIEW_H__
#define __XFDESKTOP_ICON_VIEW_H__

#include <gtk/gtk.h>

#include "xfdesktop-icon.h"
#include "xfdesktop-icon-view-manager.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON_VIEW     (xfdesktop_icon_view_get_type())
#define XFDESKTOP_ICON_VIEW(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON_VIEW, XfdesktopIconView))
#define XFDESKTOP_IS_ICON_VIEW(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON_VIEW))

typedef struct _XfdesktopIconView         XfdesktopIconView;
typedef struct _XfdesktopIconViewClass    XfdesktopIconViewClass;
typedef struct _XfdesktopIconViewPrivate  XfdesktopIconViewPrivate;

typedef void (*XfdesktopIconViewIconInitFunc)(XfdesktopIconView *icon_view);
typedef void (*XfdesktopIconViewIconFiniFunc)(XfdesktopIconView *icon_view);

struct _XfdesktopIconView
{
    GtkWidget parent;

    /*< private >*/
    XfdesktopIconViewPrivate *priv;
};

struct _XfdesktopIconViewClass
{
    GtkWidgetClass parent;

    /*< signals >*/
    void (*icon_selection_changed)(XfdesktopIconView *icon_view);
    void (*icon_activated)(XfdesktopIconView *icon_view);

    void (*select_all)(XfdesktopIconView *icon_view);
    void (*unselect_all)(XfdesktopIconView *icon_view);

    void (*select_cursor_item)(XfdesktopIconView *icon_view);
    void (*toggle_cursor_item)(XfdesktopIconView *icon_view);

    gboolean (*activate_selected_items)(XfdesktopIconView *icon_view);

    gboolean (*move_cursor)(XfdesktopIconView *icon_view,
                            GtkMovementStep step,
                            gint count);

    void (*resize_event)(XfdesktopIconView *icon_view);
};

GType xfdesktop_icon_view_get_type(void) G_GNUC_CONST;

GtkWidget *xfdesktop_icon_view_new(XfdesktopIconViewManager *manager);

XfdesktopIconViewManager *xfdesktop_icon_view_get_manager(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_add_item(XfdesktopIconView *icon_view,
                                  XfdesktopIcon *icon);

void xfdesktop_icon_view_remove_item(XfdesktopIconView *icon_view,
                                     XfdesktopIcon *icon);
void xfdesktop_icon_view_remove_all(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_set_selection_mode(XfdesktopIconView *icon_view,
                                            GtkSelectionMode mode);
GtkSelectionMode xfdesktop_icon_view_get_selection_mode(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_enable_drag_source(XfdesktopIconView *icon_view,
                                            GdkModifierType start_button_mask,
                                            const GtkTargetEntry *targets,
                                            gint n_targets,
                                            GdkDragAction actions);
void xfdesktop_icon_view_enable_drag_dest(XfdesktopIconView *icon_view,
                                          const GtkTargetEntry *targets,
                                          gint n_targets,
                                          GdkDragAction actions);
void xfdesktop_icon_view_unset_drag_source(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_unset_drag_dest(XfdesktopIconView *icon_view);

XfdesktopIcon *xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                                         gint wx,
                                                         gint wy);

GList *xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_select_item(XfdesktopIconView *icon_view,
                                     XfdesktopIcon *icon);
void xfdesktop_icon_view_select_all(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_unselect_item(XfdesktopIconView *icon_view,
                                       XfdesktopIcon *icon);
void xfdesktop_icon_view_unselect_all(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_set_icon_size(XfdesktopIconView *icon_view,
                                       guint icon_size);
guint xfdesktop_icon_view_get_icon_size(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_set_primary(XfdesktopIconView *icon_view,
                                     gboolean primary);

void xfdesktop_icon_view_set_font_size(XfdesktopIconView *icon_view,
                                       gdouble font_size_points);
gdouble xfdesktop_icon_view_get_font_size(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_set_center_text (XfdesktopIconView *icon_view,
                                          gboolean center_text);

GtkWidget *xfdesktop_icon_view_get_window_widget(XfdesktopIconView *icon_view);

gboolean
xfdesktop_get_workarea_single(XfdesktopIconView *icon_view,
                              guint ws_num,
                              gint *xorigin,
                              gint *yorigin,
                              gint *width,
                              gint *height);

void xfdesktop_icon_view_sort_icons(XfdesktopIconView *icon_view);

#if defined(DEBUG) && DEBUG > 0
guint _xfdesktop_icon_view_n_items(XfdesktopIconView *icon_view);
#endif

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_H__ */
