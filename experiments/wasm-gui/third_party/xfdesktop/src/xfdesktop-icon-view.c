/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006-2009 Brian Tarricone, <bjt23@cornell.edu>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <glib-object.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <exo/exo.h>

#include "xfdesktop-icon-view.h"
#include "xfdesktop-file-icon-manager.h"
#include "xfdesktop-window-icon-manager.h"
#include "xfdesktop-marshal.h"
#include "xfce-desktop.h"
#include "xfdesktop-volume-icon.h"
#include "xfdesktop-common.h"

#include <libwnck/libwnck.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#define DEFAULT_FONT_SIZE     12
#define DEFAULT_TOOLTIP_SIZE 128
#define MAX_TOOLTIP_SIZE     512

#define GRAVITY_HORIZONTAL     1
#define GRAVITY_RIGHT          2
#define GRAVITY_BOTTOM         4

#define ICON_SIZE         (icon_view->priv->icon_size)
#define TEXT_WIDTH        ((icon_view->priv->cell_text_width_proportion) * ICON_SIZE)
#define ICON_WIDTH        (TEXT_WIDTH)
#define CELL_PADDING      (icon_view->priv->cell_padding)
#define CELL_SIZE         (TEXT_WIDTH + CELL_PADDING * 2)
#define SPACING           (icon_view->priv->cell_spacing)
#define LABEL_RADIUS      (icon_view->priv->label_radius)
#define SHADOW_EXTENTS    (icon_view->priv->shadow_extents)
#define SHADOW_X_OFFSET   MAX(icon_view->priv->shadow_x_offset, icon_view->priv->selected_shadow_x_offset)
#define SHADOW_Y_OFFSET   MAX(icon_view->priv->shadow_y_offset, icon_view->priv->selected_shadow_y_offset)
#define TEXT_HEIGHT       (CELL_SIZE - ICON_SIZE - SPACING - (CELL_PADDING * 2) - LABEL_RADIUS)
#define MIN_MARGIN        8

#define KEYBOARD_NAVIGATION_TIMEOUT  1500

#if defined(DEBUG) && DEBUG > 0
#define DUMP_GRID_LAYOUT(icon_view) \
{\
    gint my_i, my_maxi;\
    \
    DBG("grid layout dump:"); \
    my_maxi = icon_view->priv->nrows * icon_view->priv->ncols;\
    for(my_i = 0; my_i < my_maxi; my_i++)\
        g_printerr("%c ", icon_view->priv->grid_layout[my_i] ? '1' : '0');\
    g_printerr("\n\n");\
}
#else
#define DUMP_GRID_LAYOUT(icon_view)
#endif

enum
{
    SIG_ICON_SELECTION_CHANGED = 0,
    SIG_ICON_ACTIVATED,
    SIG_SELECT_ALL,
    SIG_UNSELECT_ALL,
    SIG_SELECT_CURSOR_ITEM,
    SIG_TOGGLE_CURSOR_ITEM,
    SIG_MOVE_CURSOR,
    SIG_ACTIVATE_SELECTED_ITEMS,
    SIG_RESIZE_EVENT,
    SIG_N_SIGNALS,
};

typedef struct
{
    XfdesktopIconView *icon_view;
    XfdesktopIcon *icon;
    GdkRectangle area;
    guint source_id;
} XfdesktopIdleRepaintData;

struct _XfdesktopIconViewPrivate
{
    XfdesktopIconViewManager *manager;

    GtkWidget *parent_window;

    gint scale_factor;
    guint icon_size;
    gboolean primary;
    gdouble font_size;
    gboolean center_text;

    WnckScreen *wnck_screen;
    PangoLayout *playout;

    GList *pending_icons;
    GList *icons;
    GList *selected_icons;

    gint xorigin;
    gint yorigin;
    gint width;
    gint height;

    gint xmargin;
    gint ymargin;
    gint xspacing;
    gint yspacing;

    gint16 nrows;
    gint16 ncols;
    XfdesktopIcon **grid_layout;

    guint grid_resize_timeout;

    GtkSelectionMode sel_mode;
    guint maybe_begin_drag:1,
          definitely_dragging:1,
          allow_rubber_banding:1,
          definitely_rubber_banding:1,
          control_click:1,
          double_click:1;
    gint press_start_x;
    gint press_start_y;
    GdkRectangle band_rect;

    XfconfChannel *channel;

    /* element-type gunichar */
    GArray *keyboard_navigation_state;
    guint keyboard_navigation_state_timeout;

    XfdesktopIcon *cursor;
    XfdesktopIcon *first_clicked_item;
    XfdesktopIcon *item_under_pointer;

    GtkTargetList *native_targets;
    GtkTargetList *source_targets;
    GtkTargetList *dest_targets;

    gboolean drag_source_set;
    GdkDragAction foreign_source_actions;
    GdkModifierType foreign_source_mask;

    gboolean drag_dest_set;
    GdkDragAction foreign_dest_actions;

    gboolean dropped;
    GdkDragAction proposed_drop_action;
    gint16 hover_row, hover_col;

    gint cell_padding;
    gint cell_spacing;
    gdouble label_radius;
    gdouble cell_text_width_proportion;
    gint shadow_extents;

    gboolean ellipsize_icon_labels;

    /* tooltip options - There's a style and an xfconf property.
     * The order is xfconf, style, with a fall back of DEFAULT_TOOLTIP_SIZE.
     * xfdesktop_icon_view_get_tooltip_size will return the correct size. */
    gboolean show_tooltips;
    gint   tooltip_size_from_style;
    double tooltip_size_from_xfconf;

    gboolean single_click;
    gint gravity;
};

static void xfce_icon_view_set_property(GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void xfce_icon_view_get_property(GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec);

static gboolean xfdesktop_icon_view_button_press(GtkWidget *widget,
                                                 GdkEventButton *evt,
                                                 gpointer user_data);
static gboolean xfdesktop_icon_view_button_release(GtkWidget *widget,
                                                   GdkEventButton *evt,
                                                   gpointer user_data);
static gboolean xfdesktop_icon_view_key_press(GtkWidget *widget,
                                              GdkEventKey *evt,
                                              gpointer user_data);
static gboolean xfdesktop_icon_view_focus_in(GtkWidget *widget,
                                             GdkEventFocus *evt,
                                             gpointer user_data);
static gboolean xfdesktop_icon_view_focus_out(GtkWidget *widget,
                                              GdkEventFocus *evt,
                                              gpointer user_data);
static gboolean xfdesktop_icon_view_motion_notify(GtkWidget *widget,
                                                  GdkEventMotion *evt,
                                                  gpointer user_data);
static gboolean xfdesktop_icon_view_leave_notify(GtkWidget *widget,
                                                 GdkEventCrossing *evt,
                                                 gpointer user_data);
static void xfdesktop_icon_view_style_updated(GtkWidget *widget);
static void xfdesktop_icon_view_realize(GtkWidget *widget);
static void xfdesktop_icon_view_unrealize(GtkWidget *widget);
static gboolean xfdesktop_icon_view_draw(GtkWidget *widget,
                                         cairo_t *cr);
static void xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                                           GdkDragContext *contest);
static gboolean xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                guint time_);
static void xfdesktop_icon_view_drag_leave(GtkWidget *widget,
                                           GdkDragContext *context,
                                           guint time_);
static gboolean xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                                              GdkDragContext *context,
                                              gint x,
                                              gint y,
                                              guint time_);
static void xfdesktop_icon_view_drag_data_get(GtkWidget *widget,
                                              GdkDragContext *context,
                                              GtkSelectionData *data,
                                              guint info,
                                              guint time_);
static void xfdesktop_icon_view_drag_data_received(GtkWidget *widget,
                                                   GdkDragContext *context,
                                                   gint x,
                                                   gint y,
                                                   GtkSelectionData *data,
                                                   guint info,
                                                   guint time_);

static void xfdesktop_icon_view_finalize(GObject *obj);

static void xfdesktop_icon_view_add_move_binding(GtkBindingSet *binding_set,
                                                 guint keyval,
                                                 guint modmask,
                                                 GtkMovementStep step,
                                                 gint count);

static gboolean xfdesktop_icon_view_update_icon_extents(XfdesktopIconView *icon_view,
                                                        XfdesktopIcon *icon,
                                                        GdkRectangle *pixbuf_extents,
                                                        GdkRectangle *text_extents,
                                                        GdkRectangle *box_extents,
                                                        GdkRectangle *total_extents);
static void xfdesktop_icon_view_invalidate_icon(XfdesktopIconView *icon_view,
                                                XfdesktopIcon *icon,
                                                gboolean recalc_extents);
static void xfdesktop_icon_view_icon_changed(XfdesktopIcon *icon,
                                             gpointer user_data);

static void xfdesktop_icon_view_invalidate_icon_pixbuf(XfdesktopIconView *icon_view,
                                                       XfdesktopIcon *icon);

static void xfdesktop_icon_view_paint_icon(XfdesktopIconView *icon_view,
                                           XfdesktopIcon *icon,
                                           GdkRectangle *area,
                                           cairo_t *cr);
static void xfdesktop_icon_view_repaint_icons(XfdesktopIconView *icon_view,
                                              GdkRectangle *area,
                                              cairo_t *cr);

static void xfdesktop_setup_grids(XfdesktopIconView *icon_view);
static gboolean xfdesktop_grid_get_next_free_position(XfdesktopIconView *icon_view,
                                                      gint16 *row,
                                                      gint16 *col);
static inline gboolean xfdesktop_grid_is_free_position(XfdesktopIconView *icon_view,
                                                       gint16 row,
                                                       gint16 col);
static inline void xfdesktop_grid_set_position_free(XfdesktopIconView *icon_view,
                                                    gint16 row,
                                                    gint16 col);
static inline gboolean xfdesktop_grid_unset_position_free_raw(XfdesktopIconView *icon_view,
                                                              gint16 row,
                                                              gint16 col,
                                                              gpointer data);
static inline gboolean xfdesktop_grid_unset_position_free(XfdesktopIconView *icon_view,
                                                          XfdesktopIcon *icon);
static inline XfdesktopIcon *xfdesktop_icon_view_icon_in_cell_raw(XfdesktopIconView *icon_view,
                                                                  gint idx);
static inline XfdesktopIcon *xfdesktop_icon_view_icon_in_cell(XfdesktopIconView *icon_view,
                                                              gint16 row,
                                                              gint16 col);
static gint xfdesktop_check_icon_clicked(gconstpointer data,
                                         gconstpointer user_data);
static void xfdesktop_list_foreach_invalidate(gpointer data,
                                              gpointer user_data);

static inline void xfdesktop_xy_to_rowcol(XfdesktopIconView *icon_view,
                                          gint x,
                                          gint y,
                                          gint16 *row,
                                          gint16 *col);
static gboolean xfdesktop_grid_resize_timeout(gpointer user_data);
static void xfdesktop_monitors_changed_cb(GdkScreen *gscreen,
                                          gpointer user_data);
static void xfdesktop_screen_size_changed_cb(GdkScreen *gscreen,
                                            gpointer user_data);
static GdkFilterReturn xfdesktop_rootwin_watch_workarea(GdkXEvent *gxevent,
                                                        GdkEvent *event,
                                                        gpointer user_data);
static void xfdesktop_move_all_icons_to_pending_icons_list(XfdesktopIconView *icon_view);
static void xfdesktop_move_all_pending_icons_to_desktop(XfdesktopIconView* icon_view);
static void xfdesktop_grid_do_resize(XfdesktopIconView *icon_view);
static inline gboolean xfdesktop_rectangle_contains_point(GdkRectangle *rect,
                                                          gint x,
                                                          gint y);
static void xfdesktop_icon_view_modify_font_size(XfdesktopIconView *icon_view,
                                                 gdouble size);
static void xfdesktop_icon_view_add_item_internal(XfdesktopIconView *icon_view,
                                                  XfdesktopIcon *icon);
static gboolean xfdesktop_icon_view_icon_find_position(XfdesktopIconView *icon_view,
                                                       XfdesktopIcon *icon);
static gboolean xfdesktop_icon_view_shift_area_to_cell(XfdesktopIconView *icon_view,
                                                       XfdesktopIcon *icon,
                                                       GdkRectangle *text_area);
static gint xfdesktop_icon_view_get_tooltip_size(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_show_tooltip(GtkWidget *widget,
                                                 gint x,
                                                 gint y,
                                                 gboolean keyboard_tooltip,
                                                 GtkTooltip *tooltip,
                                                 gpointer user_data);

static gboolean xfdesktop_icon_view_is_icon_selected(XfdesktopIconView *icon_view,
                                                     XfdesktopIcon *icon);
static void xfdesktop_icon_view_real_select_all(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_real_unselect_all(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_real_select_cursor_item(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_real_toggle_cursor_item(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_real_activate_selected_items(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_real_move_cursor(XfdesktopIconView *icon_view,
                                                     GtkMovementStep step,
                                                     gint count);

static void xfdesktop_icon_view_move_cursor_left_right(XfdesktopIconView *icon_view,
                                                       gint count,
                                                       GdkModifierType modmask);
static void xfdesktop_icon_view_select_between(XfdesktopIconView *icon_view,
                                               XfdesktopIcon *start_icon,
                                               XfdesktopIcon *end_icon);

enum
{
    TARGET_XFDESKTOP_ICON = 9999,
};

enum
{
    PROP_0 = 0,
    PROP_SINGLE_CLICK,
    PROP_SHOW_TOOLTIPS,
    PROP_TOOLTIP_SIZE,
    PROP_GRAVITY,
};


static const GtkTargetEntry icon_view_targets[] = {
    { "XFDESKTOP_ICON", GTK_TARGET_SAME_APP, TARGET_XFDESKTOP_ICON }
};
static const gint icon_view_n_targets = 1;

static guint __signals[SIG_N_SIGNALS] = { 0, };


G_DEFINE_TYPE_WITH_PRIVATE(XfdesktopIconView, xfdesktop_icon_view, GTK_TYPE_WIDGET)


static void
xfdesktop_icon_view_class_init(XfdesktopIconViewClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
    GtkBindingSet *binding_set;

    binding_set = gtk_binding_set_by_class(klass);

    gobject_class->finalize = xfdesktop_icon_view_finalize;
    gobject_class->set_property = xfce_icon_view_set_property;
    gobject_class->get_property = xfce_icon_view_get_property;

    widget_class->style_updated = xfdesktop_icon_view_style_updated;
    widget_class->realize = xfdesktop_icon_view_realize;
    widget_class->unrealize = xfdesktop_icon_view_unrealize;
    widget_class->draw = xfdesktop_icon_view_draw;
    widget_class->drag_begin = xfdesktop_icon_view_drag_begin;
    widget_class->drag_motion = xfdesktop_icon_view_drag_motion;
    widget_class->drag_leave = xfdesktop_icon_view_drag_leave;
    widget_class->drag_drop = xfdesktop_icon_view_drag_drop;
    widget_class->drag_data_get = xfdesktop_icon_view_drag_data_get;
    widget_class->drag_data_received = xfdesktop_icon_view_drag_data_received;

    klass->select_all = xfdesktop_icon_view_real_select_all;
    klass->unselect_all = xfdesktop_icon_view_real_unselect_all;
    klass->select_cursor_item = xfdesktop_icon_view_real_select_cursor_item;
    klass->toggle_cursor_item = xfdesktop_icon_view_real_toggle_cursor_item;
    klass->activate_selected_items = xfdesktop_icon_view_real_activate_selected_items;
    klass->move_cursor = xfdesktop_icon_view_real_move_cursor;

    __signals[SIG_ICON_SELECTION_CHANGED] = g_signal_new("icon-selection-changed",
                                                         XFDESKTOP_TYPE_ICON_VIEW,
                                                         G_SIGNAL_RUN_LAST,
                                                         G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                         icon_selection_changed),
                                                         NULL, NULL,
                                                         g_cclosure_marshal_VOID__VOID,
                                                         G_TYPE_NONE, 0);

    __signals[SIG_ICON_ACTIVATED] = g_signal_new("icon-activated",
                                                 XFDESKTOP_TYPE_ICON_VIEW,
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                 icon_activated),
                                                 NULL, NULL,
                                                 g_cclosure_marshal_VOID__VOID,
                                                 G_TYPE_NONE, 0);

    __signals[SIG_SELECT_ALL] = g_signal_new(I_("select-all"),
                                             XFDESKTOP_TYPE_ICON_VIEW,
                                             G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                             G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                             select_all),
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE, 0);

    __signals[SIG_UNSELECT_ALL] = g_signal_new(I_("unselect-all"),
                                               XFDESKTOP_TYPE_ICON_VIEW,
                                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                               G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                               unselect_all),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

    __signals[SIG_SELECT_CURSOR_ITEM] = g_signal_new(I_("select-cursor-item"),
                                                     XFDESKTOP_TYPE_ICON_VIEW,
                                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                     G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                     select_cursor_item),
                                                     NULL, NULL,
                                                     g_cclosure_marshal_VOID__VOID,
                                                     G_TYPE_NONE, 0);

    __signals[SIG_TOGGLE_CURSOR_ITEM] = g_signal_new(I_("toggle-cursor-item"),
                                                     XFDESKTOP_TYPE_ICON_VIEW,
                                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                     G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                     toggle_cursor_item),
                                                     NULL, NULL,
                                                     g_cclosure_marshal_VOID__VOID,
                                                     G_TYPE_NONE, 0);

    __signals[SIG_ACTIVATE_SELECTED_ITEMS] = g_signal_new(I_("activate-selected-items"),
                                                       XFDESKTOP_TYPE_ICON_VIEW,
                                                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                       G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                       activate_selected_items),
                                                       NULL, NULL,
                                                       xfdesktop_marshal_BOOLEAN__VOID,
                                                       G_TYPE_BOOLEAN, 0);

    __signals[SIG_MOVE_CURSOR] = g_signal_new(I_("move-cursor"),
                                              XFDESKTOP_TYPE_ICON_VIEW,
                                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                              G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                              move_cursor),
                                              NULL, NULL,
                                              xfdesktop_marshal_BOOLEAN__ENUM_INT,
                                              G_TYPE_BOOLEAN, 2,
                                              GTK_TYPE_MOVEMENT_STEP,
                                              G_TYPE_INT);

    __signals[SIG_RESIZE_EVENT] = g_signal_new(I_("resize-event"),
                                               XFDESKTOP_TYPE_ICON_VIEW,
                                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                               G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                               resize_event),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);


    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_int("cell-spacing",
                                                             "Cell spacing",
                                                             "Spacing between desktop icon cells",
                                                             0, 255, 2,
                                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_int("cell-padding",
                                                             "Cell padding",
                                                             "Padding in desktop icon cell",
                                                             0, 255, 6,
                                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_double("cell-text-width-proportion",
                                                                "Cell text width proportion",
                                                                "Width of text in desktop icon cell, "
                                                                "calculated as multiplier of the icon size",
                                                                1.0, 10.0, 1.9,
                                                                G_PARAM_READABLE));
    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_boolean("ellipsize-icon-labels",
                                                                 "Ellipsize Icon Labels",
                                                                 "Ellipzize labels of unselected icons on desktop",
                                                                 TRUE,
                                                                 G_PARAM_READABLE));
    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_double("label-radius",
                                                                "Label radius",
                                                                "The radius of the rounded corners of the text background",
                                                                0.0, 50.0, 4.0,
                                                                G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_int("tooltip-size",
                                                             "Tooltip Image Size",
                                                             "The size of the tooltip image preview",
                                                             -1, MAX_TOOLTIP_SIZE, -1,
                                                             G_PARAM_READABLE));

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_CONSTRUCT \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class, PROP_SINGLE_CLICK,
                                    g_param_spec_boolean("single-click",
                                                         "single-click",
                                                         "single-click",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_GRAVITY,
                                    g_param_spec_int("gravity",
                                                     "gravity",
                                                     "set gravity of icons placement",
                                                     0, 7, 0,
                                                     XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SHOW_TOOLTIPS,
                                    g_param_spec_boolean("show-tooltips",
                                                         "show tooltips",
                                                         "show tooltips for icons on the desktop",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_TOOLTIP_SIZE,
                                    g_param_spec_double("tooltip-size",
                                                        "tooltip size",
                                                        "size of the tooltip image preview",
                                                        -1, MAX_TOOLTIP_SIZE, -1,
                                                        XFDESKTOP_PARAM_FLAGS));

#undef XFDESKTOP_PARAM_FLAGS

    /* same binding entries as GtkIconView */
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                 "select-all", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_a,
                                 GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                 "unselect-all", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_space, GDK_CONTROL_MASK,
                                 "toggle-cursor-item", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                 "toggle-cursor-item", 0);

    gtk_binding_entry_add_signal(binding_set, GDK_KEY_space, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Space, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_Return, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_ISO_Enter, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Enter, 0,
                                 "activate-selected-items", 0);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Up, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, -1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Up, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Down, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Down, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, 1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_p, GDK_CONTROL_MASK,
                                         GTK_MOVEMENT_DISPLAY_LINES, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_n, GDK_CONTROL_MASK,
                                         GTK_MOVEMENT_DISPLAY_LINES, 1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Home, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, -1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Home, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_End, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_End, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, 1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Right, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Left, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Right, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Left, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, -1);

    gtk_widget_class_set_css_name (widget_class, "XfdesktopIconView");
}

static void
xfdesktop_icon_view_init(XfdesktopIconView *icon_view)
{
    GtkStyleContext *context;

    icon_view->priv = xfdesktop_icon_view_get_instance_private(icon_view);

    icon_view->priv->icon_size = DEFAULT_ICON_SIZE;
    icon_view->priv->font_size = DEFAULT_FONT_SIZE;

    icon_view->priv->allow_rubber_banding = TRUE;

    icon_view->priv->native_targets = gtk_target_list_new(icon_view_targets,
                                                          icon_view_n_targets);

    icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                          icon_view_n_targets);
    gtk_drag_source_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);

    icon_view->priv->dest_targets = gtk_target_list_new(icon_view_targets,
                                                        icon_view_n_targets);
    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);

    g_object_set(G_OBJECT(icon_view), "has-tooltip", TRUE, NULL);
    g_signal_connect(G_OBJECT(icon_view), "query-tooltip",
                     G_CALLBACK(xfdesktop_icon_view_show_tooltip), NULL);

    gtk_widget_set_has_window(GTK_WIDGET(icon_view), FALSE);
    gtk_widget_set_can_focus(GTK_WIDGET(icon_view), FALSE);

    context = gtk_widget_get_style_context(GTK_WIDGET(icon_view));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_VIEW);
}

static void
xfdesktop_icon_view_finalize(GObject *obj)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(obj);

    if (icon_view->priv->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->priv->keyboard_navigation_state_timeout);
    }
    if (icon_view->priv->keyboard_navigation_state) {
        g_array_free(icon_view->priv->keyboard_navigation_state, TRUE);
    }

    if(icon_view->priv->manager) {
        xfdesktop_icon_view_manager_fini(icon_view->priv->manager);
        g_object_unref(G_OBJECT(icon_view->priv->manager));
    }

    gtk_target_list_unref(icon_view->priv->native_targets);
    gtk_target_list_unref(icon_view->priv->source_targets);
    gtk_target_list_unref(icon_view->priv->dest_targets);

    g_list_free_full(icon_view->priv->pending_icons, g_object_unref);
    /* icon_view->priv->icons should be cleared in _unrealize() */

    if (icon_view->priv->channel)
        icon_view->priv->channel = NULL;

    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->finalize(obj);
}

static void
xfce_icon_view_set_property(GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(object);

    switch(property_id) {
        case PROP_SINGLE_CLICK:
            icon_view->priv->single_click = g_value_get_boolean (value);
            break;

        case PROP_SHOW_TOOLTIPS:
            icon_view->priv->show_tooltips = g_value_get_boolean(value);
            break;

        case PROP_TOOLTIP_SIZE:
            icon_view->priv->tooltip_size_from_xfconf = g_value_get_double(value);
            break;

        case PROP_GRAVITY:
            icon_view->priv->gravity = g_value_get_int(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_icon_view_get_property(GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(object);

    switch(property_id) {
        case PROP_SINGLE_CLICK:
            g_value_set_boolean(value, icon_view->priv->single_click);
            break;

        case PROP_GRAVITY:
            g_value_set_int(value, icon_view->priv->gravity);
            break;

        case PROP_SHOW_TOOLTIPS:
            g_value_set_boolean(value, icon_view->priv->show_tooltips);
            break;

        case PROP_TOOLTIP_SIZE:
            g_value_set_double(value, icon_view->priv->tooltip_size_from_xfconf);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_add_move_binding(GtkBindingSet *binding_set,
                                     guint keyval,
                                     guint modmask,
                                     GtkMovementStep step,
                                     gint count)
{
    gtk_binding_entry_add_signal(binding_set, keyval, modmask,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);

    gtk_binding_entry_add_signal(binding_set, keyval, GDK_SHIFT_MASK,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);

    if(modmask & GDK_CONTROL_MASK)
        return;

    gtk_binding_entry_add_signal(binding_set, keyval,
                                 GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);

    gtk_binding_entry_add_signal(binding_set, keyval, GDK_CONTROL_MASK,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);
}

static gboolean
xfdesktop_icon_view_get_single_click(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);

    return icon_view->priv->single_click;
}

static void
xfdesktop_icon_view_clear_drag_event(XfdesktopIconView *icon_view, GtkWidget *widget)
{
    DBG("unsetting stuff");
    icon_view->priv->control_click = FALSE;
    icon_view->priv->double_click = FALSE;
    icon_view->priv->maybe_begin_drag = FALSE;
    icon_view->priv->definitely_dragging = FALSE;
    if(icon_view->priv->definitely_rubber_banding) {
        /* Remove the rubber band selection box */
        icon_view->priv->definitely_rubber_banding = FALSE;
        gtk_widget_queue_draw_area(widget, icon_view->priv->band_rect.x,
                                   icon_view->priv->band_rect.y,
                                   icon_view->priv->band_rect.width,
                                   icon_view->priv->band_rect.height);
    }
}

static gboolean
xfdesktop_icon_view_button_press(GtkWidget *widget,
                                 GdkEventButton *evt,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    XfdesktopIcon *icon;

    DBG("entering");

    if(evt->type == GDK_BUTTON_PRESS) {
        GList *icon_l;

        /* Clear drag event if ongoing */
        if(evt->button == 2 || evt->button == 3)
            xfdesktop_icon_view_clear_drag_event(icon_view, widget);

        /* Let xfce-desktop handle button 2 */
        if(evt->button == 2) {
            /* If we had the grab release it so the desktop gets the event */
            if(gtk_widget_has_grab(widget))
                gtk_grab_remove(widget);

            return FALSE;
        }

        /* Grab the focus, release on button release */
        if(!gtk_widget_has_grab(widget))
            gtk_grab_add(widget);

        icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                    (GCompareFunc)xfdesktop_check_icon_clicked);
        if(icon_l && (icon = icon_l->data)) {
            if(xfdesktop_icon_view_is_icon_selected(icon_view, icon)) {
                /* clicked an already-selected icon */

                if(evt->state & GDK_CONTROL_MASK)
                    icon_view->priv->control_click = TRUE;

                icon_view->priv->cursor = icon;
            } else {
                /* clicked a non-selected icon */
                if(icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
                   || !(evt->state & GDK_CONTROL_MASK))
                {
                    /* unselect all of the other icons if we haven't held
                     * down the ctrl key.  we'll handle shift in the next block,
                     * but for shift we do need to unselect everything */
                    xfdesktop_icon_view_unselect_all(icon_view);

                    if(!(evt->state & GDK_SHIFT_MASK))
                        icon_view->priv->first_clicked_item = NULL;
                }

                icon_view->priv->cursor = icon;

                if(!icon_view->priv->first_clicked_item)
                    icon_view->priv->first_clicked_item = icon;

                if(icon_view->priv->sel_mode == GTK_SELECTION_MULTIPLE
                   && evt->state & GDK_SHIFT_MASK
                   && icon_view->priv->first_clicked_item
                   && icon_view->priv->first_clicked_item != icon)
                {
                    xfdesktop_icon_view_select_between(icon_view,
                                                       icon_view->priv->first_clicked_item,
                                                       icon);
                } else
                    xfdesktop_icon_view_select_item(icon_view, icon);
            }

            if(evt->button == 1 || evt->button == 3) {
                /* we might be the start of a drag */
                DBG("setting stuff");
                icon_view->priv->maybe_begin_drag = TRUE;
                icon_view->priv->definitely_dragging = FALSE;
                icon_view->priv->definitely_rubber_banding = FALSE;
                icon_view->priv->press_start_x = evt->x;
                icon_view->priv->press_start_y = evt->y;
            }

            return TRUE;
        } else {
            /* Button press wasn't over any icons */
            /* unselect previously selected icons if we didn't click one */
            if(icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
               || !(evt->state & GDK_CONTROL_MASK))
            {
                xfdesktop_icon_view_unselect_all(icon_view);
            }

            icon_view->priv->cursor = NULL;
            icon_view->priv->first_clicked_item = NULL;

            if(icon_view->priv->allow_rubber_banding && evt->button == 1
               && !(evt->state & GDK_SHIFT_MASK))
            {
                icon_view->priv->maybe_begin_drag = TRUE;
                icon_view->priv->definitely_dragging = FALSE;
                icon_view->priv->press_start_x = evt->x;
                icon_view->priv->press_start_y = evt->y;
            }

            /* Since we're not over any icons this won't be the start of a
             * drag so we can pop up menu */
            if(evt->button == 3 || (evt->button == 1 && (evt->state & GDK_SHIFT_MASK))) {
                xfce_desktop_popup_root_menu(XFCE_DESKTOP(widget), evt->button, evt->time);
                return TRUE;
            }
        }
    } else if(evt->type == GDK_2BUTTON_PRESS) {
        /* filter this event in single click mode */
        if(xfdesktop_icon_view_get_single_click(icon_view)) {
            icon_view->priv->double_click = TRUE;
            xfdesktop_icon_view_unselect_all(icon_view);
            return TRUE;
        }

        /* be sure to cancel any pending drags that might have snuck through.
         * this shouldn't happen, but it does sometimes (bug 3426).  */
        icon_view->priv->maybe_begin_drag = FALSE;
        icon_view->priv->definitely_dragging = FALSE;
        icon_view->priv->definitely_rubber_banding = FALSE;

        if(evt->button == 1) {
            GList *icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                               (GCompareFunc)xfdesktop_check_icon_clicked);
            if(icon_l && (icon = icon_l->data)) {
                icon_view->priv->cursor = icon;
                g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED],
                              0, NULL);
                xfdesktop_icon_activated(icon);
                xfdesktop_icon_view_unselect_all(icon_view);
            }
        }

        return TRUE;
    }

    return FALSE;
}

static gint
xfdesktop_icon_view_get_tooltip_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), DEFAULT_TOOLTIP_SIZE);

    /* Check if we're showing tooltips at all */
    if(!icon_view->priv->show_tooltips)
        return 0;

    /* If the xfconf size is set and sane, use it */
    if(icon_view->priv->tooltip_size_from_xfconf >= 0)
        return icon_view->priv->tooltip_size_from_xfconf;

    /* if the style size is set and sane, use it */
    if(icon_view->priv->tooltip_size_from_style >= 0)
        return icon_view->priv->tooltip_size_from_style;

    /* Fall back */
    return DEFAULT_TOOLTIP_SIZE;
}

static gboolean
xfdesktop_icon_view_button_release(GtkWidget *widget,
                                   GdkEventButton *evt,
                                   gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    XfdesktopIcon *icon;
    GList *icon_l = NULL;

    DBG("entering btn=%d", evt->button);

    /* single-click */
    if(xfdesktop_icon_view_get_single_click(icon_view)
       && evt->button == 1
       && !(evt->state & GDK_SHIFT_MASK)
       && !(evt->state & GDK_CONTROL_MASK)
       && !icon_view->priv->definitely_dragging
       && !icon_view->priv->definitely_rubber_banding
       && !icon_view->priv->double_click) {
        /* Find out if we clicked on an icon */
        icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                    (GCompareFunc)xfdesktop_check_icon_clicked);
        if(icon_l && (icon = icon_l->data)) {
            /* We did, activate it */
            icon_view->priv->cursor = icon;
            g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED],
                          0, NULL);
            xfdesktop_icon_activated(icon);
            xfdesktop_icon_view_unselect_all(icon_view);
        }
    }

    if((evt->button == 3 || (evt->button == 1 && (evt->state & GDK_SHIFT_MASK))) &&
       icon_view->priv->definitely_dragging == FALSE &&
       icon_view->priv->definitely_rubber_banding == FALSE &&
       icon_view->priv->maybe_begin_drag == TRUE)
    {
        /* If we're in single click mode we may already have the icon, don't
         * find it again. */
        if(icon_l == NULL) {
            icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                        (GCompareFunc)xfdesktop_check_icon_clicked);
        }

        /* If we clicked an icon then we didn't pop up the menu during the
         * button press in order to support right click DND, pop up the menu
         * now.
         * We pass 0 as the button because the docs say that you must use 0
         * for pop ups other than button press events. */
        if(icon_l && icon_l->data) {
            xfce_desktop_popup_root_menu(XFCE_DESKTOP(widget), 0, evt->time);
        }
    }

    if(evt->button == 1 && evt->state & GDK_CONTROL_MASK
       && icon_view->priv->control_click)
    {
        icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                    (GCompareFunc)xfdesktop_check_icon_clicked);
        if(icon_l && (icon = icon_l->data)) {
            if(xfdesktop_icon_view_is_icon_selected(icon_view, icon)) {
                /* clicked an already-selected icon */

                /* unselect */
                xfdesktop_icon_view_unselect_item(icon_view, icon);
            }
        }
    }

    if(evt->button == 1 || evt->button == 3 || evt->button == 0) {
        xfdesktop_icon_view_clear_drag_event(icon_view, widget);
    }

    gtk_grab_remove(widget);

    /* TRUE: stop other handlers from being invoked for the event. FALSE: propagate the event further. */
    /* On FALSE this method will be called twice in single-click-mode (possibly a gtk3 bug)            */
    return TRUE;
}

static gboolean
clear_keyboard_navigation_state(gpointer data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(data);

    if (icon_view->priv->keyboard_navigation_state != NULL) {
        g_array_free(icon_view->priv->keyboard_navigation_state, TRUE);
        icon_view->priv->keyboard_navigation_state = NULL;
    }

    icon_view->priv->keyboard_navigation_state_timeout = 0;

    return G_SOURCE_REMOVE;
}

static gboolean
xfdesktop_icon_view_keyboard_navigate(XfdesktopIconView *icon_view,
                                      gunichar lower_char)
{
    if (icon_view->priv->keyboard_navigation_state == NULL) {
        icon_view->priv->keyboard_navigation_state = g_array_sized_new(TRUE, TRUE, sizeof(gunichar), 16);
        xfdesktop_icon_view_unselect_all(icon_view);
    }

    if (icon_view->priv->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->priv->keyboard_navigation_state_timeout);
    }
    icon_view->priv->keyboard_navigation_state_timeout = g_timeout_add(KEYBOARD_NAVIGATION_TIMEOUT,
                                                                       clear_keyboard_navigation_state,
                                                                       icon_view);

    g_array_append_val(icon_view->priv->keyboard_navigation_state, lower_char);

    for (GList *l = icon_view->priv->icons; l != NULL; l = l->next) {
        XfdesktopIcon *icon = (XfdesktopIcon *)l->data;
        const gchar *label = xfdesktop_icon_peek_label(icon);

        if (label != NULL && g_utf8_validate(label, -1, NULL)) {
            gchar *p = (gchar *)label;
            gboolean matches = TRUE;

            for (guint i = 0; i < icon_view->priv->keyboard_navigation_state->len; ++i) {
                gunichar label_char;

                if (*p == '\0') {
                    matches = FALSE;
                    break;
                }

                label_char = g_unichar_tolower(g_utf8_get_char(p));
                if (label_char != g_array_index(icon_view->priv->keyboard_navigation_state, gunichar, i)) {
                    matches = FALSE;
                    break;
                }

                p = g_utf8_find_next_char(p, NULL);
            }

            if (matches) {
                xfdesktop_icon_view_unselect_all(icon_view);
                icon_view->priv->cursor = icon;
                xfdesktop_icon_view_select_item(icon_view, icon);
                return TRUE;
            }
        }
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_key_press(GtkWidget *widget,
                              GdkEventKey *evt,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gboolean ret = FALSE;
    gboolean could_keyboard_navigate = FALSE;

    DBG("entering");

    /* since we're NO_WINDOW, events don't get delivered to us normally,
     * so we have to activate the bindings manually */
    ret = gtk_bindings_activate_event(G_OBJECT(icon_view), evt);
    if(ret == FALSE) {
        GdkModifierType ignore_modifiers = gtk_accelerator_get_default_mod_mask();
        if((evt->state & ignore_modifiers) == 0) {
            /* Binding not found and key press is not part of a combo.
             * Now inspect the pressed character. Let's try to find an
             * icon starting with this character and make the icon selected. */
            guint32 unicode = gdk_keyval_to_unicode(evt->keyval);
            if (unicode != 0 && g_unichar_isgraph(unicode)) {
                could_keyboard_navigate = TRUE;
                xfdesktop_icon_view_keyboard_navigate(icon_view, g_unichar_tolower(unicode));
            }
        }
    }

    if (!could_keyboard_navigate) {
        if (icon_view->priv->keyboard_navigation_state != NULL) {
            g_array_free(icon_view->priv->keyboard_navigation_state, TRUE);
            icon_view->priv->keyboard_navigation_state = NULL;
        }
    }

    return ret;
}

static gboolean
xfdesktop_icon_view_focus_in(GtkWidget *widget,
                             GdkEventFocus *evt,
                             gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    GList *l;

    gtk_widget_grab_focus(GTK_WIDGET(icon_view));
    DBG("GOT FOCUS");

    for(l = icon_view->priv->selected_icons; l; l = l->next) {
        xfdesktop_icon_view_invalidate_icon(icon_view, l->data, FALSE);
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_focus_out(GtkWidget *widget,
                              GdkEventFocus *evt,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    GList *l;

    DBG("LOST FOCUS");

    for(l = icon_view->priv->selected_icons; l; l = l->next) {
        xfdesktop_icon_view_invalidate_icon(icon_view, l->data, FALSE);
    }

    if(G_UNLIKELY(icon_view->priv->single_click)) {
        if(G_LIKELY(gtk_widget_get_window(GTK_WIDGET(icon_view->priv->parent_window)) != NULL)) {
            gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(icon_view->priv->parent_window)), NULL);
        }
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_maybe_begin_drag(XfdesktopIconView *icon_view,
                                     GdkEventMotion *evt)
{
    GdkDragAction actions;

    /* sanity check */
    g_return_val_if_fail(icon_view->priv->cursor, FALSE);

    if(!gtk_drag_check_threshold(GTK_WIDGET(icon_view),
                                 icon_view->priv->press_start_x,
                                 icon_view->priv->press_start_y,
                                 evt->x, evt->y))
    {
        return FALSE;
    }

    actions = GDK_ACTION_MOVE | (icon_view->priv->drag_source_set ?
                                 icon_view->priv->foreign_source_actions : 0);

    if(!(evt->state & GDK_BUTTON3_MASK)) {
        gtk_drag_begin_with_coordinates(GTK_WIDGET(icon_view),
                                        icon_view->priv->source_targets,
                                        actions,
                                        1,
                                        (GdkEvent *)evt,
                                        -1,
                                        -1);
    } else {
        gtk_drag_begin_with_coordinates(GTK_WIDGET(icon_view),
                                        icon_view->priv->source_targets,
                                        actions | GDK_ACTION_ASK,
                                        3,
                                        (GdkEvent *)evt,
                                        -1,
                                        -1);
    }

    DBG("DRAG BEGIN!");

    return TRUE;
}

static gboolean
xfdesktop_icon_view_show_tooltip(GtkWidget *widget,
                                 gint x,
                                 gint y,
                                 gboolean keyboard_tooltip,
                                 GtkTooltip *tooltip,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    const gchar *tip_text;
    gchar *padded_tip_text = NULL;
    gint tooltip_size;
    GtkWidget *box, *label;

    if(!icon_view->priv->item_under_pointer
       || icon_view->priv->definitely_dragging)
    {
        return FALSE;
    }

    /* not showing tooltips */
    if(!icon_view->priv->show_tooltips)
        return FALSE;

    tip_text = xfdesktop_icon_peek_tooltip(icon_view->priv->item_under_pointer);
    if (!tip_text)
        return FALSE;

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    tooltip_size = xfdesktop_icon_view_get_tooltip_size(icon_view);
    if (tooltip_size > 0) {
        gint scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(icon_view));
        GdkPixbuf *tip_pix = xfdesktop_icon_peek_tooltip_pixbuf(icon_view->priv->item_under_pointer,
                                                                tooltip_size * scale_factor * 1.5f,
                                                                tooltip_size * scale_factor);
        cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(tip_pix, scale_factor, gtk_widget_get_window(GTK_WIDGET(icon_view)));
        GtkWidget *img = gtk_image_new_from_surface(surface);
        cairo_surface_destroy(surface);
        gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
    }

    padded_tip_text = g_strdup_printf("%s\t", tip_text);
    label = gtk_label_new(padded_tip_text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_yalign(GTK_LABEL(label), 0.5);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

    gtk_widget_show_all(box);
    gtk_tooltip_set_custom(tooltip, box);

    g_free(padded_tip_text);

    return TRUE;
}

static gboolean
xfdesktop_icon_view_motion_notify(GtkWidget *widget,
                                  GdkEventMotion *evt,
                                  gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gboolean ret = FALSE;

    if(icon_view->priv->maybe_begin_drag
       && icon_view->priv->item_under_pointer
       && !icon_view->priv->definitely_dragging)
    {
        /* we might have the start of an icon click + drag here */
        icon_view->priv->definitely_dragging = xfdesktop_icon_view_maybe_begin_drag(icon_view,
                                                                                    evt);
        if(icon_view->priv->definitely_dragging) {
            icon_view->priv->maybe_begin_drag = FALSE;
            ret = TRUE;
        }
    } else if(icon_view->priv->maybe_begin_drag
              && ((!icon_view->priv->item_under_pointer
                   && !icon_view->priv->definitely_rubber_banding)
                  || icon_view->priv->definitely_rubber_banding))
    {
        GdkRectangle old_rect, *new_rect, intersect;
        cairo_region_t *region;
        GList *l;

        /* we're dragging with no icon under the cursor -> rubber band start
         * OR, we're already doin' the band -> update it */

        new_rect = &icon_view->priv->band_rect;

        if(!icon_view->priv->definitely_rubber_banding) {
            icon_view->priv->definitely_rubber_banding = TRUE;
            old_rect.x = icon_view->priv->press_start_x;
            old_rect.y = icon_view->priv->press_start_y;
            old_rect.width = old_rect.height = 1;
        } else
            memcpy(&old_rect, new_rect, sizeof(old_rect));

        new_rect->x = MIN(icon_view->priv->press_start_x, evt->x);
        new_rect->y = MIN(icon_view->priv->press_start_y, evt->y);
        new_rect->width = ABS(evt->x - icon_view->priv->press_start_x) + 1;
        new_rect->height = ABS(evt->y - icon_view->priv->press_start_y) + 1;

        region = cairo_region_create_rectangle(&old_rect);
        cairo_region_union_rectangle(region, new_rect);

        if(gdk_rectangle_intersect(&old_rect, new_rect, &intersect)
           && intersect.width > 2 && intersect.height > 2)
        {
            cairo_region_t *region_intersect;

            /* invalidate border too */
            intersect.x += 1;
            intersect.width -= 2;
            intersect.y += 1;
            intersect.height -= 2;

            region_intersect = cairo_region_create_rectangle(&intersect);
            cairo_region_subtract(region, region_intersect);
            cairo_region_destroy(region_intersect);
        }

        gdk_window_invalidate_region(gtk_widget_get_window(widget), region, TRUE);
        cairo_region_destroy(region);

        /* update list of selected icons */

        /* first pass: if the rubber band area got smaller at least in
         * one dimension, we can try first to just remove icons that
         * aren't in the band anymore */
        if(old_rect.width > new_rect->width
           || old_rect.height > new_rect->height)
        {
            l = icon_view->priv->selected_icons;
            while(l) {
                GdkRectangle extents, dummy;
                XfdesktopIcon *icon = l->data;

                /* To be removed, it must intersect the old rectangle and
                 * not intersect the new one. This way CTRL + rubber band
                 * works properly (Bug 10275) */
                if(xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)
                   && gdk_rectangle_intersect(&extents, &old_rect, NULL)
                   && !gdk_rectangle_intersect(&extents, new_rect, &dummy))
                {
                    /* remove the icon from the selected list */
                    l = l->next;
                    xfdesktop_icon_view_unselect_item(icon_view, icon);
                } else
                    l = l->next;
            }
        }

        /* second pass: if at least one dimension got larger, unfortunately
         * we have to figure out what icons to add to the selected list */
        if(old_rect.width < new_rect->width
           || old_rect.height < new_rect->height)
        {
            for(l = icon_view->priv->icons; l; l = l->next) {
                GdkRectangle extents, dummy;
                XfdesktopIcon *icon = l->data;

                if(xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)
                   && gdk_rectangle_intersect(&extents, new_rect, &dummy)
                   && !xfdesktop_icon_view_is_icon_selected(icon_view, icon))
                {
                    /* since _select_item() prepends to the list, we
                     * should be ok just calling this */
                    xfdesktop_icon_view_select_item(icon_view, icon);
                }
            }
        }
    } else {
        XfdesktopIcon *icon;
        GdkRectangle extents;

        /* normal movement; highlight icons as they go under the pointer */

        if(icon_view->priv->item_under_pointer) {
            if(G_UNLIKELY(icon_view->priv->single_click)) {
                GdkCursor *cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_HAND2);
                gdk_window_set_cursor(evt->window, cursor);
                g_object_unref(cursor);
            }
            if(!xfdesktop_icon_get_extents(icon_view->priv->item_under_pointer,
                                           NULL, NULL, &extents)
               || !xfdesktop_rectangle_contains_point(&extents, evt->x, evt->y))
            {
                icon = icon_view->priv->item_under_pointer;
                icon_view->priv->item_under_pointer = NULL;

                xfdesktop_icon_view_invalidate_icon_pixbuf(icon_view, icon);
            }
        } else {
            if(G_UNLIKELY(icon_view->priv->single_click)) {
                gdk_window_set_cursor(evt->window, NULL);
            }
            icon = xfdesktop_icon_view_widget_coords_to_item(icon_view,
                                                             evt->x,
                                                             evt->y);
            if(icon && xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)
               && xfdesktop_rectangle_contains_point(&extents, evt->x, evt->y))
            {
                icon_view->priv->item_under_pointer = icon;

                xfdesktop_icon_view_invalidate_icon_pixbuf(icon_view, icon);
            }
        }
    }

    gdk_event_request_motions(evt);

    return ret;
}

static gboolean
xfdesktop_icon_view_leave_notify(GtkWidget *widget,
                                 GdkEventCrossing *evt,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);

    if(icon_view->priv->item_under_pointer) {
        XfdesktopIcon *icon = icon_view->priv->item_under_pointer;
        icon_view->priv->item_under_pointer = NULL;

        xfdesktop_icon_view_invalidate_icon(icon_view, icon, FALSE);
    }

    if(G_UNLIKELY(icon_view->priv->single_click)) {
        if(gtk_widget_get_realized(widget)) {
            gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
        }
    }

    return FALSE;
}

static void
xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                               GdkDragContext *context)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    XfdesktopIcon *icon;
    GdkRectangle extents;

    icon = icon_view->priv->cursor;
    g_return_if_fail(icon);

    if(xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)) {
        GdkPixbuf *pix;

        pix = xfdesktop_icon_peek_pixbuf(icon,
                                         ICON_WIDTH * icon_view->priv->scale_factor,
                                         ICON_SIZE * icon_view->priv->scale_factor);
        if(pix) {
            cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(pix,
                                                                            icon_view->priv->scale_factor,
                                                                            gtk_widget_get_window(GTK_WIDGET(icon_view)));
            gtk_drag_set_icon_surface(context, surface);
            cairo_surface_destroy(surface);
        }
    }
}

static inline void
xfdesktop_xy_to_rowcol(XfdesktopIconView *icon_view,
                       gint x,
                       gint y,
                       gint16 *row,
                       gint16 *col)
{
    g_return_if_fail(row && col);

    *row = (y - icon_view->priv->yorigin - icon_view->priv->ymargin) / (CELL_SIZE + icon_view->priv->yspacing);
    *col = (x - icon_view->priv->xorigin - icon_view->priv->xmargin) / (CELL_SIZE + icon_view->priv->xspacing);
}

static inline void
xfdesktop_icon_view_clear_drag_highlight(XfdesktopIconView *icon_view)
{
    XfdesktopIcon *icon;

    /* remove highlight from icon */
    if(icon_view->priv->item_under_pointer) {
        icon = icon_view->priv->item_under_pointer;
        icon_view->priv->item_under_pointer = NULL;

        xfdesktop_icon_view_invalidate_icon_pixbuf(icon_view, icon);
    }
}

static inline void
xfdesktop_icon_view_draw_drag_highlight(XfdesktopIconView *icon_view,
                                        guint16 row,
                                        guint16 col)
{
    XfdesktopIcon *icon;

    icon = xfdesktop_icon_view_icon_in_cell(icon_view, row, col);

    if(!icon && icon_view->priv->item_under_pointer) {
        /* clear previous highlight */
        xfdesktop_icon_view_clear_drag_highlight(icon_view);
    }

    if(icon && icon_view->priv->item_under_pointer != icon) {
        if(icon_view->priv->item_under_pointer) {
            /* clear previous highlight */
            xfdesktop_icon_view_clear_drag_highlight(icon_view);
        }

        /* highlight icon */
        icon_view->priv->item_under_pointer = icon;

        xfdesktop_icon_view_invalidate_icon_pixbuf(icon_view, icon);
    }
}

static gboolean
xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkAtom target;
    gint16 hover_row = 0, hover_col = 0;
    XfdesktopIcon *icon_on_dest = NULL;
    GdkDragAction our_action = 0;
    gboolean is_local_drag;

    target = gtk_drag_dest_find_target(widget, context,
                                       icon_view->priv->native_targets);
    if(target == GDK_NONE) {
        target = gtk_drag_dest_find_target(widget, context,
                                           icon_view->priv->dest_targets);
        if(target == GDK_NONE)
            return FALSE;
    }

    /* can we drop here? */
    xfdesktop_xy_to_rowcol(icon_view, x, y, &hover_row, &hover_col);
    if(hover_row >= icon_view->priv->nrows || hover_col >= icon_view->priv->ncols)
        return FALSE;
    icon_on_dest = xfdesktop_icon_view_icon_in_cell(icon_view, hover_row,
                                                    hover_col);
    if(icon_on_dest) {
        if(!xfdesktop_icon_get_allowed_drop_actions(icon_on_dest, NULL))
            return FALSE;
    } else if(!xfdesktop_grid_is_free_position(icon_view, hover_row, hover_col))
        return FALSE;

    is_local_drag = (target == gdk_atom_intern("XFDESKTOP_ICON", FALSE));

    /* at this point there are four cases to account for:
     * 1.  local drag, empty space -> MOVE
     * 2.  local drag, icon is there -> depends on icon_on_dest
     * 3.  foreign drag, empty space -> depends on the source
     * 4.  foreign drag, icon is there -> depends on source and icon_on_dest
     */

    if(!icon_on_dest) {
        if(is_local_drag)  /* # 1 */
            our_action = GDK_ACTION_MOVE;
        else  /* #3 */
            our_action = gdk_drag_context_get_suggested_action(context);
    } else {
        /* start with all available actions (may be filtered by modifier keys) */
        GdkDragAction allowed_actions = gdk_drag_context_get_actions(context);

        if(is_local_drag) {  /* #2 */
            GList *l;
            gint16 sel_row, sel_col;
            gboolean action_ask = FALSE;

            /* check to make sure we aren't just hovering over ourself */
            for(l = icon_view->priv->selected_icons; l; l = l->next) {
                XfdesktopIcon *sel_icon = l->data;
                if(xfdesktop_icon_get_position(sel_icon, &sel_row, &sel_col)
                   && sel_row == hover_row && sel_col == hover_col)
                {
                    xfdesktop_icon_view_clear_drag_highlight(icon_view);
                    return FALSE;
                }
            }

            if(allowed_actions & GDK_ACTION_ASK)
                action_ask = TRUE;

            allowed_actions &= xfdesktop_icon_get_allowed_drag_actions(icon_view->priv->cursor);

            /* for local drags, let the dest icon decide */
            allowed_actions &= xfdesktop_icon_get_allowed_drop_actions(icon_on_dest, &our_action);

            /* check if drag&drop menu should be triggered */
            if(action_ask) {
                if(allowed_actions == (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)) {
                    allowed_actions |= GDK_ACTION_ASK;
                    our_action = GDK_ACTION_ASK;
                }
            }
        } else {  /* #4 */
            allowed_actions &= xfdesktop_icon_get_allowed_drop_actions(icon_on_dest, NULL);

            /* for foreign drags, take the action suggested by the source */
            our_action = gdk_drag_context_get_suggested_action(context);
        }

        /* #2 or #4 */

        /* fallback actions if the suggested action is not allowed,
         * priority: move, copy, link */
        if(!(our_action & allowed_actions)) {
            if(allowed_actions & GDK_ACTION_MOVE)
                our_action = GDK_ACTION_MOVE;
            else if(allowed_actions & GDK_ACTION_COPY)
                our_action = GDK_ACTION_COPY;
            else if(allowed_actions & GDK_ACTION_LINK)
                our_action = GDK_ACTION_LINK;
            else
                our_action = 0;
        }
    }

    /* allow the drag dest to override the selected action based on the drag data */
    icon_view->priv->hover_row = hover_row;
    icon_view->priv->hover_col = hover_col;
    icon_view->priv->proposed_drop_action = our_action;
    icon_view->priv->dropped = FALSE;
    g_object_set_data(G_OBJECT(context), "--xfdesktop-icon-view-drop-icon",
                      icon_on_dest);
    gtk_drag_get_data(widget, context, target, time_);

    /* the actual call to gdk_drag_status() is deferred to
     * xfdesktop_icon_view_drag_data_received() */

    return TRUE;
}

static void
xfdesktop_icon_view_drag_leave(GtkWidget *widget,
                               GdkDragContext *context,
                               guint time_)
{
    xfdesktop_icon_view_clear_drag_highlight(XFDESKTOP_ICON_VIEW(widget));
}

static void
xfdesktop_next_slot(XfdesktopIconView *icon_view,
                    gint16 *col,
                    gint16 *row,
                    gint16 ncols,
                    gint16 nrows)
{
    gint scol = *col, srow = *row;

    if(icon_view->priv->gravity & GRAVITY_HORIZONTAL) {
        scol += (icon_view->priv->gravity & GRAVITY_RIGHT) ? -1 : 1;
        if(scol < 0) {
            scol = ncols - 1;
            srow += (icon_view->priv->gravity & GRAVITY_BOTTOM) ? -1 : 1;
        } else {
            if(scol >= ncols) {
                scol = 0;
                srow += (icon_view->priv->gravity & GRAVITY_BOTTOM) ? -1 : 1;
            }
        }
    } else {
        srow += (icon_view->priv->gravity & GRAVITY_BOTTOM) ? -1 : 1;
        if(srow < 0) {
            srow = nrows - 1;
            scol += (icon_view->priv->gravity & GRAVITY_RIGHT) ? -1 : 1;
        } else {
            if(srow >= nrows) {
                srow = 0;
                scol += (icon_view->priv->gravity & GRAVITY_RIGHT) ? -1 : 1;
            }
        }
    }

    *col = scol;
    *row = srow;
}

static gboolean
xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                              GdkDragContext *context,
                              gint x,
                              gint y,
                              guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkAtom target;
    XfdesktopIcon *icon;
    gint16 old_row, old_col, row, col, offset_col, offset_row;
    GList *l;
    XfdesktopIcon *icon_on_dest = NULL;

    DBG("entering: (%d,%d)", x, y);

    DBG("unsetting stuff");
    icon_view->priv->control_click = FALSE;
    icon_view->priv->double_click = FALSE;
    icon_view->priv->maybe_begin_drag = FALSE;
    icon_view->priv->definitely_dragging = FALSE;
    icon_view->priv->dropped = TRUE;

    target = gtk_drag_dest_find_target(widget, context,
                                       icon_view->priv->native_targets);
    if(target == GDK_NONE) {
        target = gtk_drag_dest_find_target(widget, context,
                                           icon_view->priv->dest_targets);
        if(target == GDK_NONE)
            return FALSE;
    }
    XF_DEBUG("target=%ld (%s)", (glong)target, gdk_atom_name(target));

    xfdesktop_xy_to_rowcol(icon_view, x, y, &row, &col);
    icon_on_dest = xfdesktop_icon_view_icon_in_cell(icon_view, row, col);

    if(target == gdk_atom_intern("XFDESKTOP_ICON", FALSE)) {
        if(icon_on_dest) {
            GdkDragAction action;
            gboolean ret = FALSE;

            action = gdk_drag_context_get_selected_action(context);

#ifdef ENABLE_FILE_ICONS
            if(action == GDK_ACTION_ASK) {
                xfdesktop_dnd_menu(icon_view->priv->manager, icon_on_dest,
                                   context, &action, row, col, time_);

                if(action == 0) {
                    gtk_drag_finish(context, FALSE, FALSE, time_);
                    return ret;
                }
            }
#endif

            ret = xfdesktop_icon_do_drop_dest(icon_on_dest, icon_view->priv->selected_icons, action);

            gtk_drag_finish(context, ret, FALSE, time_);

            return ret;
        }

        icon = icon_view->priv->cursor;
        g_return_val_if_fail(icon, FALSE);

        /* 1: Get amount of offset between the old slot and new slot
         *    of the icon that's being dragged.
         * 2: Remove all the icons that are going to be moved from
         *    the desktop. That's in case the icons being moved
         *    want to rearrange themselves there.
         * 3: We move all the icons using the offset. */
        if(xfdesktop_icon_get_position(icon, &old_row, &old_col)) {
            offset_col = old_col-col;
            offset_row = old_row-row;
        } else {
            offset_col = 0;
            offset_row = 0;
        }

        for(l = icon_view->priv->selected_icons; l; l = l->next) {
            /* clear out old position */
            xfdesktop_icon_view_invalidate_icon(icon_view, l->data, FALSE);
            if(xfdesktop_icon_get_position(l->data, &old_row, &old_col))
                xfdesktop_grid_set_position_free(icon_view, old_row, old_col);
        }

        for(l = icon_view->priv->selected_icons; l; l = l->next) {
            /* use offset to figure out where to put the icon*/
            if(xfdesktop_icon_get_position(l->data, &old_row, &old_col)) {
                col = (old_col-offset_col) % icon_view->priv->ncols;
                row = (old_row-offset_row) % icon_view->priv->nrows;
                /* wrap around the view */
                while(col < 0)
                    col += icon_view->priv->ncols;
                while(row < 0)
                    row += icon_view->priv->nrows;
            }

            /* Find the next available slot for an icon if offset slot is not available */
            while(!xfdesktop_grid_is_free_position(icon_view, row, col)) {
                xfdesktop_next_slot(icon_view, &col, &row, icon_view->priv->ncols, icon_view->priv->nrows);
            }

            /* set new position */
            xfdesktop_icon_set_position(l->data, row, col);
            xfdesktop_grid_unset_position_free(icon_view, l->data);

            /* clear out old extents, if any */
            xfdesktop_icon_view_invalidate_icon(icon_view, l->data, TRUE);
        }

        XF_DEBUG("drag succeeded");

        gtk_drag_finish(context, TRUE, FALSE, time_);
    } else {
        g_object_set_data(G_OBJECT(context), "--xfdesktop-icon-view-drop-icon",
                          icon_on_dest);
        return xfdesktop_icon_view_manager_drag_drop(icon_view->priv->manager,
                                                     icon_on_dest,
                                                     context,
                                                     row, col, time_);
    }

    return TRUE;
}

static void
xfdesktop_icon_view_drag_data_get(GtkWidget *widget,
                                  GdkDragContext *context,
                                  GtkSelectionData *data,
                                  guint info,
                                  guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    DBG("entering");

    if(XFDESKTOP_IS_WINDOW_ICON_MANAGER(icon_view->priv->manager))
        return;

    /* Sometimes during a ctrl+drag this is NULL
     * but works when the file(s) are dropped */
    if(icon_view->priv->selected_icons == NULL)
        return;

    xfdesktop_icon_view_manager_drag_data_get(icon_view->priv->manager,
                                              icon_view->priv->selected_icons,
                                              context, data, info, time_);

    xfdesktop_icon_view_clear_drag_event(icon_view, widget);
}

static void
xfdesktop_icon_view_drag_data_received(GtkWidget *widget,
                                       GdkDragContext *context,
                                       gint x,
                                       gint y,
                                       GtkSelectionData *data,
                                       guint info,
                                       guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    gint16 row, col;
    XfdesktopIcon *icon_on_dest;

    DBG("entering");

    icon_on_dest = g_object_get_data(G_OBJECT(context),
                                     "--xfdesktop-icon-view-drop-icon");

    if(icon_view->priv->dropped) {
        icon_view->priv->dropped = FALSE;

        xfdesktop_xy_to_rowcol(icon_view, x, y, &row, &col);
        if(row >= icon_view->priv->nrows || col >= icon_view->priv->ncols)
            return;

        xfdesktop_icon_view_manager_drag_data_received(icon_view->priv->manager,
                                                       icon_on_dest,
                                                       context, row, col, data,
                                                       info, time_);
    } else {
        /* FIXME: cannot use x and y here, for they don't seem to have any
         * meaningful value */

        GdkDragAction action = icon_view->priv->proposed_drop_action;

        if(!XFDESKTOP_IS_WINDOW_ICON_MANAGER(icon_view->priv->manager)) {
            action = xfdesktop_icon_view_manager_propose_drop_action(icon_view->priv->manager,
                                                                     icon_on_dest,
                                                                     action,
                                                                     context, data,
                                                                     info);
        }

        if(action == 0)
            xfdesktop_icon_view_clear_drag_highlight(icon_view);
        else
            xfdesktop_icon_view_draw_drag_highlight(icon_view,
                                                    icon_view->priv->hover_row,
                                                    icon_view->priv->hover_col);

        gdk_drag_status(context, action, time_);
    }
}

#ifdef ENABLE_FILE_ICONS
static gint
xfdesktop_icon_view_compare_icons(gconstpointer *a,
                                  gconstpointer *b)
{
    XfdesktopIcon *a_icon, *b_icon;
    const gchar *a_str, *b_str;

    a_icon = XFDESKTOP_ICON(a);
    b_icon = XFDESKTOP_ICON(b);

    a_str = xfdesktop_icon_peek_label(a_icon);
    b_str = xfdesktop_icon_peek_label(b_icon);

    if(a_str == NULL)
        a_str = "";
    if(b_str == NULL)
        b_str = "";

    return g_utf8_collate(a_str, b_str);
}
#endif /* ENABLE_FILE_ICONS */

#ifdef ENABLE_FILE_ICONS
static void
xfdesktop_icon_view_append_icons(XfdesktopIconView *icon_view,
                                 GList *icon_list,
                                 gint16 *row,
                                 gint16 *col)
{
    GList *l = NULL;
    for(l = icon_list; l != NULL; l = g_list_next(l)) {

        /* Find the next available slot for an icon */
        do {
            xfdesktop_next_slot(icon_view, col, row, icon_view->priv->ncols, icon_view->priv->nrows);
        } while(!xfdesktop_grid_is_free_position(icon_view, *row, *col));

        /* set new position */
        xfdesktop_icon_set_position(l->data, *row, *col);
        xfdesktop_grid_unset_position_free(icon_view, l->data);

        xfdesktop_icon_view_invalidate_icon(icon_view, l->data, TRUE);
    }
}
#endif /* ENABLE_FILE_ICONS */

void
xfdesktop_icon_view_sort_icons(XfdesktopIconView *icon_view)
{
#ifdef ENABLE_FILE_ICONS
    GList *l = NULL;
    guint i;
    GList *icons[4] = { NULL, NULL, NULL, NULL };
    gint16 row;
    gint16 col;

    for(l = icon_view->priv->icons; l; l = l->next) {
        /* clear out old position */
        xfdesktop_icon_view_invalidate_icon(icon_view, l->data, FALSE);

        /* Choose the correct list index */
        if(XFDESKTOP_IS_SPECIAL_FILE_ICON(l->data)) {
            i = 0;
        } else if(XFDESKTOP_IS_VOLUME_ICON(l->data)) {
            i = 1;
        } else if(XFDESKTOP_IS_FILE_ICON(l->data) &&
                  g_file_query_file_type(xfdesktop_file_icon_peek_file(l->data),
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL) == G_FILE_TYPE_DIRECTORY)
        {
            i = 2;
        } else {
            i = 3;
        }

        /* Add the icon to the correct list */
        icons[i] = g_list_prepend(icons[i], l->data);
    }

    /* free all positions in the layout */
    for(col = 0; col < icon_view->priv->ncols; col++)
        for(row = 0; row < icon_view->priv->nrows; row++)
            icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = NULL;

    /* start at appropriate position */
    if(icon_view->priv->gravity & GRAVITY_HORIZONTAL) {
        row = -1;
        col = icon_view->priv->ncols - 1;
    } else {
        row = icon_view->priv->nrows - 1;
        col = -1;
    }

    if(icon_view->priv->gravity & GRAVITY_RIGHT)
        col = icon_view->priv->ncols - 1 - col;
    if(icon_view->priv->gravity & GRAVITY_BOTTOM)
        row = icon_view->priv->nrows - 1 - row;

    /* Append the icons: special, folder, then regular */
    for(i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i) {
        l = g_list_sort(icons[i],
                        (GCompareFunc)xfdesktop_icon_view_compare_icons);
        xfdesktop_icon_view_append_icons(icon_view, l, &row, &col);
        g_list_free(l);
    }
#endif
}

static void
xfdesktop_icon_view_icon_theme_changed(GtkIconTheme *icon_theme,
                                       gpointer user_data)
{
    gtk_widget_queue_draw(GTK_WIDGET(user_data));
}

static void
xfdesktop_icon_view_style_updated(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    DBG("entering");

    gtk_widget_style_get(widget,
                         "cell-spacing", &icon_view->priv->cell_spacing,
                         "cell-padding", &icon_view->priv->cell_padding,
                         "cell-text-width-proportion", &icon_view->priv->cell_text_width_proportion,
                         "ellipsize-icon-labels", &icon_view->priv->ellipsize_icon_labels,
                         "tooltip-size", &icon_view->priv->tooltip_size_from_style,
                         "label-radius", &icon_view->priv->label_radius,
                         NULL);

    XF_DEBUG("cell spacing is %d", icon_view->priv->cell_spacing);
    XF_DEBUG("cell padding is %d", icon_view->priv->cell_padding);
    XF_DEBUG("cell text width proportion is %f", icon_view->priv->cell_text_width_proportion);
    XF_DEBUG("ellipsize icon label is %s", icon_view->priv->ellipsize_icon_labels?"true":"false");
    XF_DEBUG("tooltip size is %d", icon_view->priv->tooltip_size_from_style);
    XF_DEBUG("label radius is %f", icon_view->priv->label_radius);


    GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->style_updated(widget);
}

static void
scale_factor_changed_cb(XfdesktopIconView *icon_view,
                        gpointer user_data)
{
    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        gint new_scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(icon_view));
        if (new_scale_factor != icon_view->priv->scale_factor) {
            icon_view->priv->scale_factor = new_scale_factor;
            xfdesktop_grid_do_resize(icon_view);
        }
    }
}

static void
xfdesktop_icon_view_realize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkWindow *topwin;
    PangoContext *pctx;
    GdkScreen *gscreen;
    GdkWindow *groot;

    icon_view->priv->parent_window = gtk_widget_get_toplevel(widget);
    g_return_if_fail(icon_view->priv->parent_window);
    topwin = gtk_widget_get_window(icon_view->priv->parent_window);
    gtk_widget_set_window(widget, topwin);

    icon_view->priv->scale_factor = gdk_window_get_scale_factor(topwin);
    g_signal_connect(icon_view, "notify::scale-factor",
                     G_CALLBACK(scale_factor_changed_cb), NULL);

    /* we need this call here to initalize some members of icon_view->priv,
     * those depend on custom style properties */
    xfdesktop_icon_view_style_updated(widget);

    /* there's no reason to start up the manager before we're realized,
     * but we do NOT shut it down if we unrealize, since there may not be
     * a reason to do so.  shutdown occurs in finalize. */
    xfdesktop_icon_view_manager_init(icon_view->priv->manager, icon_view);

    gtk_widget_set_realized(widget, TRUE);

    gtk_window_set_accept_focus(GTK_WINDOW(icon_view->priv->parent_window),
                                TRUE);
    gtk_window_set_focus_on_map(GTK_WINDOW(icon_view->priv->parent_window),
                                FALSE);

    pctx = gtk_widget_get_pango_context(GTK_WIDGET(icon_view));
    icon_view->priv->playout = pango_layout_new(pctx);

    if(icon_view->priv->font_size > 0) {
        xfdesktop_icon_view_modify_font_size(icon_view,
                                             icon_view->priv->font_size);
    }

    xfdesktop_setup_grids(icon_view);

    /* unfortunately GTK_NO_WINDOW widgets don't receive events, with the
     * exception of draw events. */
    gtk_widget_add_events(icon_view->priv->parent_window,
                          GDK_POINTER_MOTION_HINT_MASK | GDK_KEY_PRESS_MASK
                          | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                          | GDK_FOCUS_CHANGE_MASK | GDK_EXPOSURE_MASK
                          | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "motion-notify-event",
                     G_CALLBACK(xfdesktop_icon_view_motion_notify), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "leave-notify-event",
                     G_CALLBACK(xfdesktop_icon_view_leave_notify), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "key-press-event",
                     G_CALLBACK(xfdesktop_icon_view_key_press), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "button-press-event",
                     G_CALLBACK(xfdesktop_icon_view_button_press), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "button-release-event",
                     G_CALLBACK(xfdesktop_icon_view_button_release), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "focus-in-event",
                     G_CALLBACK(xfdesktop_icon_view_focus_in), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "focus-out-event",
                     G_CALLBACK(xfdesktop_icon_view_focus_out), icon_view);

    /* watch for _NET_WORKAREA changes */
    gscreen = gtk_widget_get_screen(widget);
    groot = gdk_screen_get_root_window(gscreen);
    gdk_window_set_events(groot, gdk_window_get_events(groot)
                                 | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(groot, xfdesktop_rootwin_watch_workarea, icon_view);

    g_signal_connect(G_OBJECT(gscreen), "monitors-changed",
                     G_CALLBACK(xfdesktop_monitors_changed_cb), icon_view);
    g_signal_connect(G_OBJECT(gscreen), "size-changed",
                     G_CALLBACK(xfdesktop_screen_size_changed_cb), icon_view);

    g_signal_connect_after(G_OBJECT(gtk_icon_theme_get_for_screen(gscreen)),
                           "changed",
                           G_CALLBACK(xfdesktop_icon_view_icon_theme_changed),
                           icon_view);

    xfdesktop_move_all_pending_icons_to_desktop(icon_view);
}

static void
xfdesktop_icon_view_unrealize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkScreen *gscreen;
    GdkWindow *groot;

    gtk_window_set_accept_focus(GTK_WINDOW(icon_view->priv->parent_window), FALSE);

    gscreen = gtk_widget_get_screen(widget);
    groot = gdk_screen_get_root_window(gscreen);
    gdk_window_remove_filter(groot, xfdesktop_rootwin_watch_workarea, icon_view);

    g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_icon_theme_get_for_screen(gscreen)),
                     G_CALLBACK(xfdesktop_icon_view_icon_theme_changed),
                     icon_view);

    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_motion_notify), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_leave_notify), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_key_press), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_button_press), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_button_release), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_focus_in), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_focus_out), icon_view);

    if(icon_view->priv->grid_resize_timeout) {
        g_source_remove(icon_view->priv->grid_resize_timeout);
        icon_view->priv->grid_resize_timeout = 0;
    }

    g_signal_handlers_disconnect_by_func(G_OBJECT(gscreen),
                                         G_CALLBACK(xfdesktop_screen_size_changed_cb),
                                         icon_view);

    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view),
                                         G_CALLBACK(scale_factor_changed_cb),
                                         NULL);

    /* FIXME: really clear these? */
    g_list_free(icon_view->priv->selected_icons);
    icon_view->priv->selected_icons = NULL;

    xfdesktop_move_all_icons_to_pending_icons_list(icon_view);

    g_free(icon_view->priv->grid_layout);
    icon_view->priv->grid_layout = NULL;

    g_object_unref(G_OBJECT(icon_view->priv->playout));
    icon_view->priv->playout = NULL;

    gtk_widget_set_window(widget, NULL);
    gtk_widget_set_realized(widget, FALSE);
}

static gboolean
xfdesktop_icon_view_draw(GtkWidget *widget,
                         cairo_t *cr)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    cairo_rectangle_list_t *rects;
    cairo_rectangle_int_t temp;
    GdkRectangle clipbox;
    GtkStyleContext *context;
    gint i;

    /*DBG("entering");*/

    rects = cairo_copy_clip_rectangle_list(cr);

    if(rects->status != CAIRO_STATUS_SUCCESS) {
        cairo_rectangle_list_destroy (rects);
        return FALSE;
    }

    gdk_cairo_get_clip_rectangle(cr, &clipbox);

    xfdesktop_icon_view_repaint_icons(icon_view, &clipbox, cr);

    if(icon_view->priv->definitely_rubber_banding) {
        GdkRectangle intersect;

        context = gtk_widget_get_style_context(widget);
        gtk_style_context_save(context);
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_RUBBERBAND);

        /* paint each rectangle in the expose region with the rubber
         * band color, semi-transparently */
        for(i = 0; i < rects->num_rectangles; ++i) {
            /* rects only contains cairo_rectangle_t's (with double values) which we
               cannot use in gdk_rectangle_intersect, so copy those values to temp.
               If there is a better way please update this part */
            temp.x = rects->rectangles[i].x;
            temp.y = rects->rectangles[i].y;
            temp.width = rects->rectangles[i].width;
            temp.height = rects->rectangles[i].height;

            if (!gdk_rectangle_intersect(&temp, &icon_view->priv->band_rect, &intersect))
            {
                continue;
            }

            cairo_save(cr);

            /* paint the rubber band area */
            gdk_cairo_rectangle(cr, &intersect);
            cairo_clip_preserve(cr);
            gtk_render_background(context, cr,
                                  icon_view->priv->band_rect.x,
                                  icon_view->priv->band_rect.y,
                                  icon_view->priv->band_rect.width,
                                  icon_view->priv->band_rect.height);
            gtk_render_frame(context, cr,
                             icon_view->priv->band_rect.x,
                             icon_view->priv->band_rect.y,
                             icon_view->priv->band_rect.width,
                             icon_view->priv->band_rect.height);

            cairo_restore(cr);
        }

        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_RUBBERBAND);
        gtk_style_context_restore(context);
    }

    cairo_rectangle_list_destroy(rects);

    return FALSE;
}

static void
xfdesktop_icon_view_real_select_all(XfdesktopIconView *icon_view)
{
    DBG("entering");

    xfdesktop_icon_view_select_all(icon_view);
}

static void
xfdesktop_icon_view_real_unselect_all(XfdesktopIconView *icon_view)
{
    DBG("entering");

    xfdesktop_icon_view_unselect_all(icon_view);
}

static void
xfdesktop_icon_view_real_select_cursor_item(XfdesktopIconView *icon_view)
{
    DBG("entering");

    if(icon_view->priv->cursor)
        xfdesktop_icon_view_select_item(icon_view, icon_view->priv->cursor);
}

static void
xfdesktop_icon_view_real_toggle_cursor_item(XfdesktopIconView *icon_view)
{
    DBG("entering");

    if(!icon_view->priv->cursor)
        return;

    if(g_list_find(icon_view->priv->selected_icons, icon_view->priv->cursor))
        xfdesktop_icon_view_unselect_item(icon_view, icon_view->priv->cursor);
    else
        xfdesktop_icon_view_select_item(icon_view, icon_view->priv->cursor);
}

static gboolean
xfdesktop_icon_view_real_activate_selected_items(XfdesktopIconView *icon_view)
{
    DBG("entering");

    if(!icon_view->priv->selected_icons)
        return FALSE;

    g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0, NULL);
    g_list_foreach(icon_view->priv->selected_icons, xfdesktop_icon_activated_g_func, NULL);

    return TRUE;
}

static void
xfdesktop_icon_view_select_between(XfdesktopIconView *icon_view,
                                   XfdesktopIcon *start_icon,
                                   XfdesktopIcon *end_icon)
{
    gint16 start_row, start_col, end_row, end_col;
    gint i, j;
    XfdesktopIcon *icon;

    if(xfdesktop_icon_get_position(start_icon, &start_row, &start_col)
       && xfdesktop_icon_get_position(end_icon, &end_row, &end_col))
    {
        if(start_row > end_row || (start_row == end_row && start_col > end_col)) {
            /* flip start and end */
            gint16 tmpr = start_row, tmpc = start_col;

            start_row = end_row;
            start_col = end_col;
            end_row = tmpr;
            end_col = tmpc;
        }

        for(i = start_row; i <= end_row; ++i) {
            for(j = (i == start_row ? start_col : 0);
                (i == end_row ? j <= end_col : j < icon_view->priv->ncols);
                ++j)
            {
                icon = xfdesktop_icon_view_icon_in_cell(icon_view, i, j);
                if(icon) {
                    xfdesktop_icon_view_select_item(icon_view, icon);
                }
            }
        }
    }
}

static XfdesktopIcon *
xfdesktop_icon_view_find_first_icon(XfdesktopIconView *icon_view)
{
    gint i, j;
    XfdesktopIcon *icon = NULL;

    if(!icon_view->priv->icons)
        return NULL;

    for(i = 0; i < icon_view->priv->nrows && !icon; ++i) {
        for(j = 0; j < icon_view->priv->ncols; ++j) {
            icon = xfdesktop_icon_view_icon_in_cell(icon_view, i, j);
            if(icon)
                break;
        }
    }

    return icon;
}

static XfdesktopIcon *
xfdesktop_icon_view_find_last_icon(XfdesktopIconView *icon_view)
{
    XfdesktopIcon *icon = NULL;
    gint i, j;

    if(!icon_view->priv->icons)
        return NULL;

    for(i = icon_view->priv->nrows - 1; i >= 0 && !icon; --i) {
        for(j = icon_view->priv->ncols - 1; j >= 0; --j) {
            icon = xfdesktop_icon_view_icon_in_cell(icon_view, i, j);
            if(icon)
                break;
        }
    }

    return icon;
}

static void
xfdesktop_icon_view_move_cursor_left_right(XfdesktopIconView *icon_view,
                                           gint count,
                                           GdkModifierType modmask)
{
    gint16 row, col;
    gint i, j;
    guint left = (count < 0 ? -count : count);
    gint step = (count < 0 ? -1 : 1);
    XfdesktopIcon *icon = NULL;

    if(!icon_view->priv->cursor) {
        /* choose first or last item depending on left or right */
        if(count < 0)
            icon = xfdesktop_icon_view_find_last_icon(icon_view);
        else
            icon = xfdesktop_icon_view_find_first_icon(icon_view);

        if(icon) {
            if(!(modmask & GDK_CONTROL_MASK))
                xfdesktop_icon_view_unselect_all(icon_view);
            icon_view->priv->cursor = icon;
            xfdesktop_icon_view_select_item(icon_view, icon);
        }
    } else {
        if(!xfdesktop_icon_get_position(icon_view->priv->cursor, &row, &col))
            return;

        if(!(modmask & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)))
            xfdesktop_icon_view_unselect_all(icon_view);

        for(i = row;
            (count < 0 ? i >= 0 : i < icon_view->priv->nrows) && left > 0;
            i += step)
        {
            for(j = (i == row ? col + step : (count < 0) ? icon_view->priv->ncols - 1 : 0);
                (count < 0 ? j >= 0 : j < icon_view->priv->ncols) && left > 0;
                j += step)
            {
                icon = xfdesktop_icon_view_icon_in_cell(icon_view, i, j);
                if(icon) {
                    icon_view->priv->cursor = icon;
                    if((modmask & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) || left == 1)
                        xfdesktop_icon_view_select_item(icon_view, icon);
                    left--;
                }
            }
        }

        if(!icon_view->priv->selected_icons) {
            if(count < 0)
                icon = xfdesktop_icon_view_find_first_icon(icon_view);
            else
                icon = xfdesktop_icon_view_find_last_icon(icon_view);

            if(icon) {
                xfdesktop_icon_view_select_item(icon_view, icon);
                icon_view->priv->cursor = icon;
            }
        }
    }
}

static void
xfdesktop_icon_view_move_cursor_up_down(XfdesktopIconView *icon_view,
                                        gint count,
                                        GdkModifierType modmask)
{
    gint16 row, col;
    gint i, j;
    guint left = (count < 0 ? -count : count);
    gint step = (count < 0 ? -1 : 1);
    XfdesktopIcon *icon = NULL;

    if(!icon_view->priv->cursor) {
        /* choose first or last item depending on up or down */
        if(count < 0)
            icon = xfdesktop_icon_view_find_last_icon(icon_view);
        else
            icon = xfdesktop_icon_view_find_first_icon(icon_view);

        if(icon) {
            if(!(modmask & GDK_CONTROL_MASK))
                xfdesktop_icon_view_unselect_all(icon_view);
            icon_view->priv->cursor = icon;
            xfdesktop_icon_view_select_item(icon_view, icon);
        }
    } else {
        if(!xfdesktop_icon_get_position(icon_view->priv->cursor, &row, &col))
            return;

        if(!(modmask & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)))
            xfdesktop_icon_view_unselect_all(icon_view);

        for(j = col;
            (count < 0 ? j >= 0 : j < icon_view->priv->ncols) && left > 0;
            j += step)
        {
        for(i = (j == col ? row + step : (count < 0) ? icon_view->priv->nrows - 1 : 0);
            (count < 0 ? i >= 0 : i < icon_view->priv->nrows) && left > 0;
            i += step)
        {
                icon = xfdesktop_icon_view_icon_in_cell(icon_view, i, j);
                if(icon) {
                    icon_view->priv->cursor = icon;
                    if((modmask & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) || left == 1)
                        xfdesktop_icon_view_select_item(icon_view, icon);
                    left--;
                }
            }
        }

        if(!icon_view->priv->selected_icons) {
            if(count < 0)
                icon = xfdesktop_icon_view_find_first_icon(icon_view);
            else
                icon = xfdesktop_icon_view_find_last_icon(icon_view);

            if(icon) {
                xfdesktop_icon_view_select_item(icon_view, icon);
                icon_view->priv->cursor = icon;
            }
        }
    }
}

static void
xfdesktop_icon_view_move_cursor_begin_end(XfdesktopIconView *icon_view,
                                          gint count,
                                          GdkModifierType modmask)
{
    XfdesktopIcon *icon = NULL, *old_cursor;

    if(count < 0)
        icon = xfdesktop_icon_view_find_first_icon(icon_view);
    else
        icon = xfdesktop_icon_view_find_last_icon(icon_view);

    if(!icon)
        return;

    old_cursor = icon_view->priv->cursor;
    icon_view->priv->cursor = icon;

    if(!old_cursor || !(modmask & (GDK_SHIFT_MASK|GDK_CONTROL_MASK))) {
        xfdesktop_icon_view_unselect_all(icon_view);
        xfdesktop_icon_view_select_item(icon_view, icon);
    } else if(old_cursor) {
        if(modmask & GDK_SHIFT_MASK) {
            /* select everything between the cursor and the old_cursor */
            xfdesktop_icon_view_select_between(icon_view, old_cursor, icon);
        } else if(modmask & GDK_CONTROL_MASK) {
            /* add the icon to the selection */
            xfdesktop_icon_view_select_item(icon_view, icon);
        }

    }
}

static gboolean
xfdesktop_icon_view_real_move_cursor(XfdesktopIconView *icon_view,
                                     GtkMovementStep step,
                                     gint count)
{
    GdkModifierType modmask = 0;

    g_return_val_if_fail(step == GTK_MOVEMENT_VISUAL_POSITIONS
                         || step == GTK_MOVEMENT_DISPLAY_LINES
                         || step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

    if(count == 0)
        return FALSE;

    gtk_widget_grab_focus(GTK_WIDGET(icon_view));
    gtk_get_current_event_state(&modmask);

    switch(step) {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
            xfdesktop_icon_view_move_cursor_left_right(icon_view, count, modmask);
            break;

        case GTK_MOVEMENT_DISPLAY_LINES:
            xfdesktop_icon_view_move_cursor_up_down(icon_view, count, modmask);
            break;

        case GTK_MOVEMENT_BUFFER_ENDS:
            xfdesktop_icon_view_move_cursor_begin_end(icon_view, count, modmask);
            break;

        default:
            g_assert_not_reached();
    }

    return TRUE;
}


static void
xfdesktop_monitors_changed_cb(GdkScreen *gscreen,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);

    /* Resize the grid to be sure we take into account monitor setup changes */
    xfdesktop_grid_do_resize(icon_view);
}


static void
xfdesktop_screen_size_changed_cb(GdkScreen *gscreen,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);

   /* this is kinda icky.  we want to use _NET_WORKAREA to reset the size of
     * the grid, but we can never be sure it'll actually change.  so let's
     * give it 7 seconds, and then fix it manually */
    if(icon_view->priv->grid_resize_timeout)
        g_source_remove(icon_view->priv->grid_resize_timeout);
    icon_view->priv->grid_resize_timeout = g_timeout_add(7000,
                                                         xfdesktop_grid_resize_timeout,
                                                         icon_view);
}

static void
xfdesktop_icon_view_repaint_icons(XfdesktopIconView *icon_view,
                                  GdkRectangle *area,
                                  cairo_t *cr)
{
    GdkRectangle extents, dummy;
    GList *l;
    XfdesktopIcon *icon;

    /* fist paint non-selected items, then paint selected items */
    for(l = icon_view->priv->icons; l; l = l->next) {
        icon = (XfdesktopIcon *)l->data;
        if (xfdesktop_icon_view_is_icon_selected(icon_view, icon))
            continue;

        if(!xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)
           || gdk_rectangle_intersect(area, &extents, &dummy))
        {
            xfdesktop_icon_view_paint_icon(icon_view, icon, area, cr);
        }
    }

    for(l = icon_view->priv->icons; l; l = l->next) {
        icon = (XfdesktopIcon *)l->data;
        if (!xfdesktop_icon_view_is_icon_selected(icon_view, icon))
            continue;

        if(!xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)
           || gdk_rectangle_intersect(area, &extents, &dummy))
        {
            xfdesktop_icon_view_paint_icon(icon_view, icon, area, cr);
        }
    }
}

static inline gboolean
xfdesktop_rectangle_equal(GdkRectangle *rect1, GdkRectangle *rect2)
{
    return (rect1->x == rect2->x && rect1->y == rect2->y
            && rect1->width == rect2->width && rect1->height == rect2->height);
}

static inline gboolean
xfdesktop_rectangle_is_bounded_by(GdkRectangle *rect,
                                  GdkRectangle *bounds)
{
    GdkRectangle intersection;

    if(gdk_rectangle_intersect(rect, bounds, &intersection)) {
        if(xfdesktop_rectangle_equal(rect, &intersection))
            return TRUE;
    }

    return FALSE;
}

/* FIXME: add a cache for this so we don't have to compute this EVERY time */
static void
xfdesktop_icon_view_setup_grids_xinerama(XfdesktopIconView *icon_view)
{
    GdkDisplay *display;
    GdkRectangle *monitor_geoms, cell_rect;
    gint nmonitors, i, row, col;

    DBG("entering");

    display = gtk_widget_get_display(GTK_WIDGET(icon_view));
    nmonitors = gdk_display_get_n_monitors(display);

    if(nmonitors == 1)  /* optimisation */
        return;

    monitor_geoms = g_new0(GdkRectangle, nmonitors);

    for(i = 0; i < nmonitors; ++i) {
        gdk_monitor_get_geometry(gdk_display_get_monitor(display, i), &monitor_geoms[i]);
    }

    /* cubic time; w00t! */
    cell_rect.width = cell_rect.height = CELL_SIZE;
    for(row = 0; row < icon_view->priv->nrows; ++row) {
        for(col = 0; col < icon_view->priv->ncols; ++col) {
            gboolean bounded = FALSE;

            cell_rect.x = icon_view->priv->xorigin + icon_view->priv->xmargin + col * CELL_SIZE + col * icon_view->priv->xspacing;
            cell_rect.y = icon_view->priv->yorigin + icon_view->priv->ymargin + row * CELL_SIZE + row * icon_view->priv->yspacing;

            for(i = 0; i < nmonitors; ++i) {
                if(xfdesktop_rectangle_is_bounded_by(&cell_rect,
                                                     &monitor_geoms[i]))
                {
                    bounded = TRUE;
                    break;
                }
            }

            if(!bounded) {
                xfdesktop_grid_unset_position_free_raw(icon_view, row, col,
                                                       (gpointer)0xdeadbeef);
            }
        }
    }

    g_free(monitor_geoms);

    DBG("exiting");
}

static void
xfdesktop_setup_grids(XfdesktopIconView *icon_view)
{
    gint xorigin = 0, yorigin = 0, xrest = 0, yrest = 0, width = 0, height = 0;
    gsize old_size, new_size;
    GdkScreen *screen;
    GdkDisplay *display;
    GdkMonitor *monitor;
    GdkRectangle rectangle;

    old_size = (guint)icon_view->priv->nrows * icon_view->priv->ncols
               * sizeof(XfdesktopIcon *);

    screen = gtk_widget_get_screen (GTK_WIDGET (icon_view));
    display = gdk_screen_get_display (screen);
    if (icon_view->priv->primary)
    {
        monitor = gdk_display_get_primary_monitor (display);
        gdk_monitor_get_workarea (monitor, &rectangle);
        width = rectangle.width;
        height = rectangle.height;
        xorigin = rectangle.x;
        yorigin = rectangle.y;
    }
    else if (!xfdesktop_get_workarea_single(icon_view, 0,
                                            &xorigin, &yorigin,
                                            &width, &height)) {
        xfdesktop_get_screen_dimensions (screen, &width, &height);
        xorigin = yorigin = 0;
    }

    icon_view->priv->xorigin = xorigin;
    icon_view->priv->yorigin = yorigin;
    icon_view->priv->width = width;
    icon_view->priv->height = height;

    icon_view->priv->nrows = MAX((icon_view->priv->height - MIN_MARGIN * 2) / CELL_SIZE, 0);
    icon_view->priv->ncols = MAX((icon_view->priv->width - MIN_MARGIN * 2) / CELL_SIZE, 0);

    xrest = icon_view->priv->width - icon_view->priv->ncols * CELL_SIZE;
    if (icon_view->priv->ncols > 1) {
        icon_view->priv->xspacing = (xrest - MIN_MARGIN * 2) / (icon_view->priv->ncols - 1);
    } else {
        /* Let's not try to divide by 0 */
        icon_view->priv->xspacing = 1;
    }

    icon_view->priv->xmargin = (xrest - (icon_view->priv->ncols - 1) * icon_view->priv->xspacing) / 2;

    yrest = icon_view->priv->height - icon_view->priv->nrows * CELL_SIZE;
    if (icon_view->priv->nrows > 1) {
        icon_view->priv->yspacing = (yrest - MIN_MARGIN * 2) / (icon_view->priv->nrows - 1);
    } else {
        /* Let's not try to divide by 0 */
        icon_view->priv->yspacing = 1;
    }
    icon_view->priv->ymargin = (yrest - (icon_view->priv->nrows - 1) * icon_view->priv->yspacing) / 2;

    new_size = (guint)icon_view->priv->nrows * icon_view->priv->ncols
               * sizeof(XfdesktopIcon *);

    if(old_size == new_size && icon_view->priv->grid_layout != NULL) {
        DBG("old_size == new_size exiting");
        return;
    }

    XF_DEBUG("CELL_SIZE=%0.3f, TEXT_WIDTH=%0.3f, ICON_SIZE=%u", CELL_SIZE, TEXT_WIDTH, ICON_SIZE);
    XF_DEBUG("grid size is %dx%d", icon_view->priv->nrows, icon_view->priv->ncols);

    if(icon_view->priv->grid_layout) {
        icon_view->priv->grid_layout = g_realloc(icon_view->priv->grid_layout,
                                                 new_size);

        if(new_size > old_size) {
            memset(((guint8 *)icon_view->priv->grid_layout) + old_size, 0,
                   new_size - old_size);
        }
    } else
        icon_view->priv->grid_layout = g_malloc0(new_size);

    XF_DEBUG("created grid_layout with %lu positions", (gulong)(new_size/sizeof(gpointer)));
    DUMP_GRID_LAYOUT(icon_view);

    xfdesktop_icon_view_setup_grids_xinerama(icon_view);
}

static GdkFilterReturn
xfdesktop_rootwin_watch_workarea(GdkXEvent *gxevent,
                                 GdkEvent *event,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = user_data;
    XPropertyEvent *xevt = (XPropertyEvent *)gxevent;

    if(xevt->type == PropertyNotify
       && XInternAtom(xevt->display, "_NET_WORKAREA", False) == xevt->atom)
    {
        XF_DEBUG("got _NET_WORKAREA change on rootwin!");
        if(icon_view->priv->grid_resize_timeout) {
            g_source_remove(icon_view->priv->grid_resize_timeout);
            icon_view->priv->grid_resize_timeout = 0;
        }
        xfdesktop_grid_do_resize(icon_view);
    }

    return GDK_FILTER_CONTINUE;
}

static void
xfdesktop_icon_view_invalidate_icon(XfdesktopIconView *icon_view,
                                    XfdesktopIcon *icon,
                                    gboolean recalc_extents)
{
    GdkRectangle invalidate_extents = { 0, };
    GdkRectangle cell_extents, icon_total_extents;

    g_return_if_fail(icon);

    /*DBG("entering (recalc=%s)", recalc_extents?"true":"false");*/

    // First find the normal cell extents.
    cell_extents.width = cell_extents.height = CELL_SIZE;
    if (xfdesktop_icon_view_shift_area_to_cell(icon_view, icon, &cell_extents)) {
        invalidate_extents = cell_extents;
    }

    // The normal cell extents may not encompass everything; if the icon is
    // selected, and the icon label is long, it might spill out past the
    // cell extents.
    if (xfdesktop_icon_get_extents(icon, NULL, NULL, &icon_total_extents)) {
        if (invalidate_extents.width > 0 && invalidate_extents.height > 0) {
            gdk_rectangle_union(&invalidate_extents, &icon_total_extents, &invalidate_extents);
        } else {
            invalidate_extents = icon_total_extents;
        }
    } else {
        recalc_extents = TRUE;
    }

    if(recalc_extents) {
        GdkRectangle pixbuf_extents, text_extents, box_extents, total_extents;

        if(!xfdesktop_icon_view_update_icon_extents(icon_view, icon,
                                                    &pixbuf_extents,
                                                    &text_extents,
                                                    &box_extents,
                                                    &total_extents))
        {
            g_warning("Trying to invalidate icon, but can't recalculate extents");
        } else {
            if (invalidate_extents.width > 0 && invalidate_extents.height > 0) {
                gdk_rectangle_union(&invalidate_extents, &total_extents, &invalidate_extents);
            } else {
                invalidate_extents = total_extents;
            }
        }
    }

    if (invalidate_extents.width > 0 && invalidate_extents.height > 0) {
        if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
            gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                                       invalidate_extents.x, invalidate_extents.y,
                                       invalidate_extents.width, invalidate_extents.height);
        }
    } else {
        DBG("Icon '%s' doesn't have extents: need to call paint some other way",
            xfdesktop_icon_peek_label(icon));
    }
}

static void
xfdesktop_icon_view_invalidate_icon_pixbuf(XfdesktopIconView *icon_view,
                                           XfdesktopIcon *icon)
{
    GdkPixbuf *pix;

    pix = xfdesktop_icon_peek_pixbuf(icon, ICON_WIDTH, ICON_SIZE);
    if(pix) {
        GdkRectangle rect = { 0, };

        rect.width = gdk_pixbuf_get_width(pix);
        rect.height = gdk_pixbuf_get_height(pix);

        if(!xfdesktop_icon_view_shift_area_to_cell(icon_view, icon, &rect))
            return;

        rect.x += CELL_PADDING + ((CELL_SIZE - 2 * CELL_PADDING) - rect.width) / 2;
        rect.y += CELL_PADDING + (ICON_SIZE - rect.height) / 2;;

        if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
            gtk_widget_queue_draw_area(GTK_WIDGET(icon_view), rect.x, rect.y,
                                       rect.width, rect.height);
        }
    }
}

static gboolean
xfdesktop_icon_view_calculate_icon_pixbuf_area(XfdesktopIconView *icon_view,
                                               XfdesktopIcon *icon,
                                               GdkRectangle *pixbuf_area)
{
    GdkPixbuf *pix;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                         && XFDESKTOP_IS_ICON(icon)
                         && pixbuf_area, FALSE);

    pixbuf_area->x = 0;
    pixbuf_area->y = 0;

    pix = xfdesktop_icon_peek_pixbuf(icon, ICON_WIDTH, ICON_SIZE);
    if(G_LIKELY(pix)) {
        pixbuf_area->width = gdk_pixbuf_get_width(pix);
        pixbuf_area->height = gdk_pixbuf_get_height(pix);
    } else {
        /* presumably this should never happen, but... */
        pixbuf_area->width = ICON_SIZE;
        pixbuf_area->height = ICON_SIZE;
    }

    return TRUE;
}

static void
xfdesktop_icon_view_setup_pango_layout(XfdesktopIconView *icon_view,
                                       XfdesktopIcon *icon,
                                       PangoLayout *playout)
{
#if PANGO_VERSION_CHECK (1, 44, 0)
    PangoAttrList *attr_list;
    PangoAttribute *attr;
#endif
    const gchar *label = xfdesktop_icon_peek_label(icon);

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));

    pango_layout_set_ellipsize(playout, PANGO_ELLIPSIZE_NONE);
    pango_layout_set_wrap(playout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_width(playout, TEXT_WIDTH * PANGO_SCALE);
    if (icon_view->priv->center_text)
        pango_layout_set_alignment(playout, PANGO_ALIGN_CENTER);
    else
        pango_layout_set_alignment(playout, PANGO_ALIGN_LEFT);
    pango_layout_set_text(playout, label, -1);

    if(!xfdesktop_icon_view_is_icon_selected(icon_view, icon)
       && icon_view->priv->ellipsize_icon_labels) {
        /* constrain the text area */
        pango_layout_set_height(playout, TEXT_HEIGHT * PANGO_SCALE);
        pango_layout_set_ellipsize(playout, PANGO_ELLIPSIZE_END);
    }

#if PANGO_VERSION_CHECK (1, 44, 0)
    /* Do not add hyphens on line breaks */
    attr_list = pango_attr_list_new ();
    attr = pango_attr_insert_hyphens_new (FALSE);
    attr->start_index = 0;
    attr->end_index = -1;
    pango_attr_list_insert (attr_list, attr);
    pango_layout_set_attributes (playout, attr_list);
    pango_attr_list_unref (attr_list);
#endif
}

static gboolean
xfdesktop_icon_view_calculate_icon_text_area(XfdesktopIconView *icon_view,
                                             XfdesktopIcon *icon,
                                             GdkRectangle *text_area)
{
    PangoLayout *playout;
    PangoRectangle prect;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                         && XFDESKTOP_IS_ICON(icon)
                         && text_area, FALSE);

    playout = icon_view->priv->playout;
    xfdesktop_icon_view_setup_pango_layout(icon_view, icon, playout);
    pango_layout_get_pixel_extents(playout, NULL, &prect);

    text_area->x = prect.x;
    text_area->y = prect.y;
    text_area->width = prect.width + 2;
    text_area->height = prect.height + 2;

    return TRUE;
}

static gboolean
xfdesktop_icon_view_shift_area_to_cell(XfdesktopIconView *icon_view,
                                       XfdesktopIcon *icon,
                                       GdkRectangle *area)
{
    gint16 row, col;

    if(!xfdesktop_icon_get_position(icon, &row, &col)) {
        g_warning("trying to calculate without a position for icon '%s'",
                  xfdesktop_icon_peek_label(icon));
        return FALSE;
    }

    area->x = icon_view->priv->xorigin + icon_view->priv->xmargin + col * CELL_SIZE + col * icon_view->priv->xspacing;
    area->y = icon_view->priv->yorigin + icon_view->priv->ymargin + row * CELL_SIZE + row * icon_view->priv->yspacing;

    return TRUE;
}

static gboolean
xfdesktop_icon_view_update_icon_extents(XfdesktopIconView *icon_view,
                                        XfdesktopIcon *icon,
                                        GdkRectangle *pixbuf_extents,
                                        GdkRectangle *text_extents,
                                        GdkRectangle *box_extents,
                                        GdkRectangle *total_extents)
{
    gint rtl_offset;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                         && XFDESKTOP_IS_ICON(icon)
                         && pixbuf_extents && text_extents
                         && box_extents && total_extents, FALSE);

    if(!xfdesktop_icon_view_calculate_icon_pixbuf_area(icon_view, icon,
                                                       pixbuf_extents)
       || !xfdesktop_icon_view_shift_area_to_cell(icon_view, icon,
                                                  pixbuf_extents))
    {
        return FALSE;
    }
    pixbuf_extents->x += CELL_PADDING + ((CELL_SIZE - CELL_PADDING * 2) - pixbuf_extents->width) / 2;
    pixbuf_extents->y += CELL_PADDING + (ICON_SIZE - pixbuf_extents->height) / 2;

    if(!xfdesktop_icon_view_calculate_icon_text_area(icon_view, icon, text_extents))
        return FALSE;

    /* text_extents->x right now includes the padding needed for rtl languages
     * to display properly if it's set */
    rtl_offset = text_extents->x;

    if(!xfdesktop_icon_view_shift_area_to_cell(icon_view, icon, text_extents))
        return FALSE;

    text_extents->x += (CELL_SIZE - text_extents->width) / 2 - rtl_offset;
    text_extents->y += ICON_SIZE + SPACING + LABEL_RADIUS + CELL_PADDING;

    *box_extents = *text_extents;
    box_extents->x -= LABEL_RADIUS - rtl_offset;
    box_extents->y -= LABEL_RADIUS;
    box_extents->width += LABEL_RADIUS * 2;
    box_extents->height += LABEL_RADIUS * 2;

    gdk_rectangle_union(pixbuf_extents, box_extents, total_extents);

    xfdesktop_icon_set_extents(icon, pixbuf_extents, text_extents, total_extents);

    return TRUE;
}

static void
xfdesktop_icon_view_draw_image(XfdesktopIconView *icon_view, cairo_t *cr, GdkPixbuf *pix, GdkRectangle *rect)
{
    cairo_surface_t *pix_surface;

    cairo_save(cr);

    pix_surface = gdk_cairo_surface_create_from_pixbuf(pix,
                                                       gtk_widget_get_scale_factor(GTK_WIDGET(icon_view)),
                                                       gtk_widget_get_window(GTK_WIDGET(icon_view)));
    cairo_set_source_surface(cr, pix_surface, rect->x, rect->y);
    cairo_surface_destroy(pix_surface);
    cairo_paint(cr);

    cairo_restore(cr);
}

static void
xfdesktop_icon_view_draw_text(GtkWidget *icon_view, cairo_t *cr,
                              PangoLayout *playout, GdkRectangle *text_area,
                              GdkRectangle *box_area, GtkStateFlags state)
{
    GtkStyleContext *context;

    cairo_save(cr);

    /*  Clip the cairo area */
    gdk_cairo_rectangle(cr, box_area);
    cairo_clip(cr);

    context = gtk_widget_get_style_context(icon_view);
    gtk_style_context_save(context);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_LABEL);
    gtk_style_context_set_state (context, state);

    gtk_render_background(context, cr, box_area->x, box_area->y, box_area->width, box_area->height);
    gtk_render_layout(context, cr, text_area->x, text_area->y, playout);

    gtk_style_context_remove_class(context, GTK_STYLE_CLASS_LABEL);
    gtk_style_context_restore(context);
    cairo_restore(cr);
}

static void
xfdesktop_icon_view_paint_icon(XfdesktopIconView *icon_view,
                               XfdesktopIcon *icon,
                               GdkRectangle *area,
                               cairo_t *cr)
{
    GtkWidget *widget = GTK_WIDGET(icon_view);
    PangoLayout *playout;
    GdkRectangle pixbuf_extents, text_extents, box_extents, total_extents;
    GdkRectangle intersection;
    GtkStateFlags state;
#ifdef G_ENABLE_DEBUG
    gint16 row, col;
#endif

    DBG("entering, (%s)(area=%dx%d+%d+%d)", xfdesktop_icon_peek_label(icon),
          area->width, area->height, area->x, area->y);

    playout = icon_view->priv->playout;

    cr = cairo_reference(cr);

    if(!xfdesktop_icon_get_extents(icon, &pixbuf_extents,
                                   &text_extents, &total_extents))
    {
        g_warning("Can't get extents for icon '%s'", xfdesktop_icon_peek_label(icon));
    }

    if(!xfdesktop_icon_view_update_icon_extents(icon_view, icon,
                                                &pixbuf_extents,
                                                &text_extents,
                                                &box_extents,
                                                &total_extents))
    {
        g_warning("Can't update extents for icon '%s'",
                  xfdesktop_icon_peek_label(icon));
    }

    if(xfdesktop_icon_view_is_icon_selected(icon_view, icon)) {
        if(gtk_widget_has_focus(widget))
            state = GTK_STATE_FLAG_SELECTED;
        else
            state = GTK_STATE_FLAG_ACTIVE;
    } else
        state = GTK_STATE_FLAG_NORMAL;

    if(gdk_rectangle_intersect(area, &pixbuf_extents, &intersection)) {
        GdkPixbuf *pix = xfdesktop_icon_peek_pixbuf(icon,
                                                    ICON_WIDTH * icon_view->priv->scale_factor,
                                                    ICON_SIZE * icon_view->priv->scale_factor);
        GdkPixbuf *pix_free = NULL;

        if(state != GTK_STATE_FLAG_NORMAL) {
            GtkStyleContext *context;
            GdkRGBA rgba;
            GdkColor color;

            context = gtk_widget_get_style_context(widget);
            gtk_style_context_get_color(context, state, &rgba);

            color.red   = rgba.red   * G_MAXUINT16;
            color.green = rgba.green * G_MAXUINT16;
            color.blue  = rgba.blue  * G_MAXUINT16;

            pix_free = exo_gdk_pixbuf_colorize(pix, &color);
            pix = pix_free;
        }

        if(icon_view->priv->item_under_pointer == icon) {
            GdkPixbuf *tmp = exo_gdk_pixbuf_spotlight(pix);
            if(pix_free)
                g_object_unref(G_OBJECT(pix_free));
            pix = tmp;
            pix_free = tmp;
        }

#ifdef G_ENABLE_DEBUG
        if(!xfdesktop_icon_get_position(icon, &row, &col)) {
            g_warning("trying to calculate without a position for icon '%s'",
                      xfdesktop_icon_peek_label(icon));
        } else {
            DBG("painting pixbuf at %dx%d+%d+%d (row %d ,col %d)",
                  pixbuf_extents.width, pixbuf_extents.height,
                  pixbuf_extents.x, pixbuf_extents.y,
                  row, col);
        }
#endif

        xfdesktop_icon_view_draw_image(icon_view, cr, pix, &pixbuf_extents);

        if(pix_free)
            g_object_unref(G_OBJECT(pix_free));
    }

    /* Only redraw the text if the text area requires it. */
    if(gdk_rectangle_intersect(area, &box_extents, &intersection)
       && icon_view->priv->font_size > 0)
    {
        DBG("painting text at %dx%d+%d+%d",
              text_extents.width, text_extents.height,
              text_extents.x, text_extents.y);

        xfdesktop_icon_view_draw_text(GTK_WIDGET(icon_view), cr, playout,
                                      &text_extents, &box_extents,
                                      state);
    }


#if 0 /*def DEBUG*/
    {
        GdkRectangle cell = { 0, };
        gint16 row, col;

        if(!xfdesktop_icon_get_position(icon, &row, &col))
            DBG("can't get icon position for '%s'", xfdesktop_icon_peek_label(icon));
        else
            DBG("for icon at (%hu,%hu) (%s)", row, col, xfdesktop_icon_peek_label(icon));

        cairo_set_line_width(cr, 1.0);

        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
        cairo_rectangle(cr, area->x, area->y, area->width, area->height);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
        cairo_rectangle(cr, text_extents.x, text_extents.y,
                        text_extents.width, text_extents.height);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0);
        cairo_rectangle(cr, total_extents.x, total_extents.y,
                        total_extents.width, total_extents.height);
        cairo_stroke(cr);

        /* this might not totally paint, but that's ok */
        cell.width = cell.height = CELL_SIZE;
        xfdesktop_icon_view_shift_area_to_cell(icon_view, icon, &cell);

        cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1.0);
        cairo_rectangle(cr, cell.x, cell.y, cell.width, cell.height);
        cairo_stroke(cr);


        DBG("cell extents:       %dx%d+%d+%d", cell.width, cell.height, cell.x, cell.y);
        DBG("new pixbuf extents: %dx%d+%d+%d", pixbuf_extents.width, pixbuf_extents.height, pixbuf_extents.x, pixbuf_extents.y);
        DBG("new text extents:   %dx%d+%d+%d", text_extents.width, text_extents.height, text_extents.x, text_extents.y);
        DBG("new total extents:  %dx%d+%d+%d", total_extents.width, total_extents.height, total_extents.x, total_extents.y);
    }
#endif

    cairo_destroy(cr);
}

static void
xfdesktop_move_all_icons_to_pending_icons_list(XfdesktopIconView *icon_view)
{
    GList *l = NULL;

    /* move all icons into the pending_icons list and remove from the desktop */
    for(l = icon_view->priv->icons; l; l = l->next) {
        gint16 old_row, old_col;

        if(xfdesktop_icon_get_position(XFDESKTOP_ICON(l->data), &old_row, &old_col))
            xfdesktop_grid_set_position_free(icon_view, old_row, old_col);

        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(xfdesktop_icon_view_icon_changed),
                                             icon_view);
    }
    icon_view->priv->pending_icons = g_list_concat(icon_view->priv->icons,
                                                   icon_view->priv->pending_icons);
    icon_view->priv->icons = NULL;

    memset(icon_view->priv->grid_layout, 0,
           (guint)icon_view->priv->nrows * icon_view->priv->ncols
           * sizeof(XfdesktopIcon *));

    xfdesktop_setup_grids(icon_view);
}

/* When changing resolutions this moves all the icons that are in the rc file
 * for the new resolution to the desktop and removes them from the pending
 * icons list */
static void
xfdesktop_move_all_cached_icons_to_desktop(XfdesktopIconView *icon_view)
{
#ifdef ENABLE_FILE_ICONS
    GList *l, *leftovers = NULL;
    XfdesktopFileIconManager *fmanager = NULL;

    DBG("entering");

    if(XFDESKTOP_IS_FILE_ICON_MANAGER(icon_view->priv->manager))
        fmanager = XFDESKTOP_FILE_ICON_MANAGER(icon_view->priv->manager);

    if(fmanager == NULL)
        return;

    xfdesktop_file_icon_save(fmanager);

    /* add all cached icons back */
    for(l = icon_view->priv->pending_icons; l; l = l->next) {
        gint16 row, col;
        XfdesktopIcon *icon = XFDESKTOP_ICON(l->data);
        gchar *identifier = xfdesktop_icon_get_identifier(icon);

        if(!XFDESKTOP_IS_FILE_ICON(icon))
            continue;

        /* Try to get the cached position for the new resolution */
        if(xfdesktop_file_icon_manager_get_cached_icon_position(
                                                            fmanager,
                                                            xfdesktop_icon_peek_label(icon),
                                                            identifier,
                                                            &row,
                                                            &col))
        {
            XF_DEBUG("icon %s setting position row%dxcol%d",
                     xfdesktop_icon_peek_label(icon), row, col);

            /* Make sure the slot is available */
            if(xfdesktop_grid_is_free_position(icon_view, row, col)) {
                xfdesktop_icon_set_position(icon, row, col);
                xfdesktop_icon_view_add_item_internal(icon_view, icon);
            } else {
                leftovers = g_list_prepend(leftovers, icon);
            }
        } else {
            leftovers = g_list_prepend(leftovers, icon);
        }

        if(identifier)
            g_free(identifier);
    }

    g_list_free(icon_view->priv->pending_icons);
    icon_view->priv->pending_icons = g_list_reverse(leftovers);
#endif
}

/* Takes any icons in the pending icons list that has their original slot open.
 * This way icons stay somewhat stable during minor resolution changes */
static void
xfdesktop_move_all_previous_icons_to_desktop(XfdesktopIconView *icon_view)
{
    GList *l, *leftovers = NULL;

    DBG("entering");

    /* add all pending icons back if their space is still available */
    for(l = icon_view->priv->pending_icons; l; l = l->next) {
        gint16 row, col;
        XfdesktopIcon *icon = XFDESKTOP_ICON(l->data);

        if(!xfdesktop_icon_get_position(icon, &row, &col)) {
            g_warning("Trying to set previous position of an icon with no position");
        }

        if(xfdesktop_grid_is_free_position(icon_view, row, col)) {
            XF_DEBUG("adding icon %s position row %d x col %d",
                     xfdesktop_icon_peek_label(icon), row, col);
            xfdesktop_icon_view_add_item_internal(icon_view, icon);
        } else {
            leftovers = g_list_prepend(leftovers, icon);
        }
    }

    g_list_free(icon_view->priv->pending_icons);
    icon_view->priv->pending_icons = g_list_reverse(leftovers);
}

/* Takes any icons in the pending icons list and adds them where there is space */
static void
xfdesktop_append_all_pending_icons(XfdesktopIconView *icon_view)
{
    GList *l, *leftovers = NULL;

    DBG("entering");

    /* add all pending icons back if space is available */
    for(l = icon_view->priv->pending_icons; l; l = l->next) {
        gint16 row, col;
        XfdesktopIcon *icon = XFDESKTOP_ICON(l->data);

        if(xfdesktop_grid_get_next_free_position(icon_view, &row, &col)) {
            xfdesktop_icon_set_position(icon, row, col);
            xfdesktop_icon_view_add_item_internal(icon_view, icon);
        } else {
            leftovers = g_list_prepend(leftovers, icon);
        }
    }

    g_list_free(icon_view->priv->pending_icons);
    icon_view->priv->pending_icons = g_list_reverse(leftovers);
}

static void
xfdesktop_move_all_pending_icons_to_desktop(XfdesktopIconView *icon_view)
{
    if(!XFDESKTOP_IS_ICON_VIEW(icon_view))
        return;

    if(icon_view->priv->grid_layout == NULL)
        return;

    xfdesktop_move_all_cached_icons_to_desktop(icon_view);
    xfdesktop_move_all_previous_icons_to_desktop(icon_view);
    xfdesktop_append_all_pending_icons(icon_view);
}

static void
xfdesktop_grid_do_resize(XfdesktopIconView *icon_view)
{
    gint xorigin = 0, yorigin = 0, width = 0, height = 0;
    gint16 new_rows, new_cols;
    gsize old_size, new_size;
    GdkScreen *screen;

    /* First check to see if the grid actually did change. This way
     * we don't remove all the icons just to put them back again */
    old_size = (guint)icon_view->priv->nrows * icon_view->priv->ncols
               * sizeof(XfdesktopIcon *);

    screen = gtk_widget_get_screen (GTK_WIDGET (icon_view));

    if (icon_view->priv->primary)
    {
        GdkDisplay *display;
        GdkMonitor *monitor;
        GdkRectangle rectangle;

        display = gdk_screen_get_display (screen);
        monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_parent_window(GTK_WIDGET(icon_view)));
        gdk_monitor_get_workarea (monitor, &rectangle);
        width = rectangle.width;
        height = rectangle.height;
    }
    else if(!xfdesktop_get_workarea_single(icon_view, 0,
                                           &xorigin, &yorigin,
                                           &width, &height))
    {
        xfdesktop_get_screen_dimensions (screen, &width, &height);
    }

    new_rows = (width - MIN_MARGIN * 2) / CELL_SIZE;
    new_cols = (height - MIN_MARGIN * 2) / CELL_SIZE;

    new_size = (guint)new_rows * new_cols * sizeof(XfdesktopIcon *);

    if(old_size != new_size) {
        DBG("old_size != new_size use cache icon list");
        #if 0 /*def DEBUG*/
            DUMP_GRID_LAYOUT(icon_view);
        #endif

        /* Grid size did change */
        xfdesktop_move_all_icons_to_pending_icons_list(icon_view);
        xfdesktop_move_all_pending_icons_to_desktop(icon_view);

        #if 0 /*def DEBUG*/
            DUMP_GRID_LAYOUT(icon_view);
        #endif

        /* Fire off an event to notify others of the change */
        g_signal_emit(G_OBJECT(icon_view), __signals[SIG_RESIZE_EVENT], 0, NULL);
    }
    else {
        DBG("old_size == new_size updating grid");
        xfdesktop_setup_grids (icon_view);
    }

    gtk_widget_queue_draw(GTK_WIDGET(icon_view));
}

static gboolean
xfdesktop_grid_resize_timeout(gpointer user_data)
{
    XfdesktopIconView *icon_view = user_data;

    xfdesktop_grid_do_resize(icon_view);

    icon_view->priv->grid_resize_timeout = 0;
    return FALSE;
}

gboolean
xfdesktop_get_workarea_single(XfdesktopIconView *icon_view,
                              guint ws_num,
                              gint *xorigin,
                              gint *yorigin,
                              gint *width,
                              gint *height)
{
    gboolean ret = FALSE;
    GdkScreen *gscreen;
    GdkDisplay *gdisplay;
    Display *dpy;
    Window root;
    Atom property, actual_type = None;
    gint actual_format = 0, first_id;
    gulong nitems = 0, bytes_after = 0, offset = 0, tmp_size = 0;
    unsigned char *data_p = NULL;

    g_return_val_if_fail(xorigin && yorigin
                         && width && height, FALSE);

    gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
    gdisplay = gdk_screen_get_display(gscreen);
    dpy = GDK_DISPLAY_XDISPLAY(gdisplay);
    root = GDK_WINDOW_XID(gdk_screen_get_root_window(gscreen));
    property = XInternAtom(dpy, "_NET_WORKAREA", False);

    first_id = ws_num * 4;

    gdk_x11_display_error_trap_push(gdisplay);

    do {
        if(Success == XGetWindowProperty(dpy, root, property, offset,
                                         G_MAXULONG, False, XA_CARDINAL,
                                         &actual_type, &actual_format, &nitems,
                                         &bytes_after, &data_p))
        {
            gint i;
            gulong *data;

            if(actual_format != 32 || actual_type != XA_CARDINAL) {
                XFree(data_p);
                break;
            }

            tmp_size = (actual_format / 8) * nitems;
            if(actual_format == 32) {
                tmp_size *= sizeof(long)/4;
            }

            data = g_malloc(tmp_size);
            memcpy(data, data_p, tmp_size);

            i = offset / 32;  /* first element id in this batch */

            /* there's probably a better way to do this. */
            if(i + (glong)nitems >= first_id && first_id - (glong)offset >= 0)
                *xorigin = data[first_id - offset] / icon_view->priv->scale_factor + MIN_MARGIN;
            if(i + (glong)nitems >= first_id + 1 && first_id - (glong)offset + 1 >= 0)
                *yorigin = data[first_id - offset + 1] / icon_view->priv->scale_factor + MIN_MARGIN;
            if(i + (glong)nitems >= first_id + 2 && first_id - (glong)offset + 2 >= 0)
                *width = data[first_id - offset + 2] / icon_view->priv->scale_factor - 2 * MIN_MARGIN;
            if(i + (glong)nitems >= first_id + 3 && first_id - (glong)offset + 3 >= 0) {
                *height = data[first_id - offset + 3] / icon_view->priv->scale_factor - 2 * MIN_MARGIN;
                ret = TRUE;
                XFree(data_p);
                g_free(data);
                break;
            }

            offset += actual_format * nitems;
            XFree(data_p);
            g_free(data);
        } else
            break;
    } while(bytes_after > 0);

    gdk_x11_display_error_trap_pop_ignored(gdisplay);

    return ret;
}

static inline gboolean
xfdesktop_grid_is_free_position(XfdesktopIconView *icon_view,
                                gint16 row,
                                gint16 col)
{
    if(icon_view->priv->grid_layout == NULL) {
        return FALSE;
    }

    if(row >= icon_view->priv->nrows || col >= icon_view->priv->ncols || row < 0 || col < 0)
    {
        return FALSE;
    }

    return !icon_view->priv->grid_layout[col * icon_view->priv->nrows + row];
}


static gboolean
xfdesktop_grid_get_next_free_position(XfdesktopIconView *icon_view,
                                      gint16 *row,
                                      gint16 *col)
{
    gint16 i, j, c, r, idx;

    g_return_val_if_fail(row && col, FALSE);

    if(icon_view->priv->gravity & GRAVITY_HORIZONTAL) {
        for(j = 0; j < icon_view->priv->nrows; ++j) {
            r = (icon_view->priv->gravity & GRAVITY_BOTTOM) ?
                 icon_view->priv->nrows - 1 - j : j;

            for(i = 0; i < icon_view->priv->ncols; ++i) {
                c = (icon_view->priv->gravity & GRAVITY_RIGHT) ?
                     icon_view->priv->ncols - 1 - i : i;

                idx = c * icon_view->priv->nrows + r;

                if(!icon_view->priv->grid_layout[idx]) {
                    *col = c;
                    *row = r;
                    return TRUE;
                }
            }
        }
    } else {
        for(i = 0; i < icon_view->priv->ncols; ++i) {
            c = (icon_view->priv->gravity & GRAVITY_RIGHT) ?
                 icon_view->priv->ncols - 1 - i : i;

            for(j = 0; j < icon_view->priv->nrows; ++j) {
                r = (icon_view->priv->gravity & GRAVITY_BOTTOM) ?
                    icon_view->priv->nrows - 1 - j : j;

                idx = c * icon_view->priv->nrows + r;

                if(!icon_view->priv->grid_layout[idx]) {
                    *col = c;
                    *row = r;
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}


static inline void
xfdesktop_grid_set_position_free(XfdesktopIconView *icon_view,
                                 gint16 row,
                                 gint16 col)
{
    g_return_if_fail(row < icon_view->priv->nrows
                     && col < icon_view->priv->ncols
                     && row >= 0 && col >= 0);

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif

    icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = NULL;

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif
}

static inline gboolean
xfdesktop_grid_unset_position_free_raw(XfdesktopIconView *icon_view,
                                       gint16 row,
                                       gint16 col,
                                       gpointer data)
{
    gint idx;

    g_return_val_if_fail(row < icon_view->priv->nrows
                         && col < icon_view->priv->ncols
                         && row >= 0 && col >= 0, FALSE);

    idx = col * icon_view->priv->nrows + row;
    if(icon_view->priv->grid_layout[idx])
        return FALSE;

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif

    icon_view->priv->grid_layout[idx] = data;

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif

    return TRUE;
}

static inline gboolean
xfdesktop_grid_unset_position_free(XfdesktopIconView *icon_view,
                                   XfdesktopIcon *icon)
{
    gint16 row, col;

    if(!xfdesktop_icon_get_position(icon, &row, &col)) {
        g_warning("Trying to set free position of an icon with no position");
        return FALSE;
    }

    return xfdesktop_grid_unset_position_free_raw(icon_view, row, col, icon);
}

static inline XfdesktopIcon *
xfdesktop_icon_view_icon_in_cell_raw(XfdesktopIconView *icon_view,
                                     gint idx)
{
    XfdesktopIcon *icon = icon_view->priv->grid_layout[idx];

    if((gpointer)0xdeadbeef == icon)
        return NULL;

    return icon;
}

static inline XfdesktopIcon *
xfdesktop_icon_view_icon_in_cell(XfdesktopIconView *icon_view,
                                 gint16 row,
                                 gint16 col)
{
    gint idx;

    g_return_val_if_fail(row < icon_view->priv->nrows
                         && col < icon_view->priv->ncols, NULL);

    idx = col * icon_view->priv->nrows + row;

    /* FIXME: that's why we can't drag icons to monitors on the left or above,
     * the array maps positions on the grid starting from the primary monitor. */
    if (idx < 0)
        return NULL;

    return xfdesktop_icon_view_icon_in_cell_raw(icon_view, idx);
}

static inline gboolean
xfdesktop_rectangle_contains_point(GdkRectangle *rect, gint x, gint y)
{
    if(x > rect->x + rect->width
            || x < rect->x
            || y > rect->y + rect->height
            || y < rect->y)
    {
        return FALSE;
    }

    return TRUE;
}

static gint
xfdesktop_check_icon_clicked(gconstpointer data,
                             gconstpointer user_data)
{
    XfdesktopIcon *icon = XFDESKTOP_ICON(data);
    GdkEventButton *evt = (GdkEventButton *)user_data;
    GdkRectangle extents;

    if(xfdesktop_icon_get_extents(icon, NULL, NULL, &extents)
       && xfdesktop_rectangle_contains_point(&extents, evt->x, evt->y))
    {
        return 0;
    } else
        return 1;
}

static void
xfdesktop_list_foreach_invalidate(gpointer data,
                                  gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    XfdesktopIcon *icon = XFDESKTOP_ICON(data);
    xfdesktop_icon_view_invalidate_icon(icon_view, icon, TRUE);
}

static void
xfdesktop_icon_view_modify_font_size(XfdesktopIconView *icon_view,
                                     gdouble size)
{
    const PangoFontDescription *pfd;
    PangoFontDescription *pfd_new;

    pfd = pango_layout_get_font_description(icon_view->priv->playout);
    if(pfd)
        pfd_new = pango_font_description_copy(pfd);
    else
        pfd_new = pango_font_description_new();

    pango_font_description_set_size(pfd_new, (gint)(size * PANGO_SCALE));

    pango_layout_set_font_description(icon_view->priv->playout, pfd_new);

    pango_font_description_free(pfd_new);
}

static void
xfdesktop_icon_view_icon_changed(XfdesktopIcon *icon,
                                 gpointer user_data)
{
    /* maybe can pass FALSE here */
    xfdesktop_icon_view_invalidate_icon(XFDESKTOP_ICON_VIEW(user_data),
                                        icon, TRUE);
}

static gboolean
xfdesktop_icon_view_is_icon_selected(XfdesktopIconView *icon_view,
                                     XfdesktopIcon *icon)
{
    return (g_list_find(icon_view->priv->selected_icons, icon)) == NULL ? FALSE : TRUE;
}


/* public api */


GtkWidget *
xfdesktop_icon_view_new(XfdesktopIconViewManager *manager)
{
    XfdesktopIconView *icon_view;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), NULL);

    icon_view = g_object_new(XFDESKTOP_TYPE_ICON_VIEW, NULL);
    icon_view->priv->manager = manager;

    icon_view->priv->channel = xfconf_channel_get(XFDESKTOP_CHANNEL);

    xfconf_g_property_bind(icon_view->priv->channel,
                           "/desktop-icons/single-click",
                           G_TYPE_BOOLEAN,
                           G_OBJECT(icon_view),
                           "single_click");

    xfconf_g_property_bind(icon_view->priv->channel,
                           "/desktop-icons/gravity",
                           G_TYPE_INT,
                           G_OBJECT(icon_view),
                           "gravity");

    xfconf_g_property_bind(icon_view->priv->channel,
                           "/desktop-icons/show-tooltips",
                           G_TYPE_BOOLEAN,
                           G_OBJECT(icon_view),
                           "show_tooltips");

    xfconf_g_property_bind(icon_view->priv->channel,
                           "/desktop-icons/tooltip-size",
                           G_TYPE_DOUBLE,
                           G_OBJECT(icon_view),
                           "tooltip_size");

    return GTK_WIDGET(icon_view);
}

XfdesktopIconViewManager *
xfdesktop_icon_view_get_manager(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);
    return icon_view->priv->manager;
}

static void
xfdesktop_icon_view_add_item_internal(XfdesktopIconView *icon_view,
                                      XfdesktopIcon *icon)
{
    gint16 row, col;
    GdkRectangle pixbuf_extents, text_extents, box_extents, total_extents;

    /* sanity check: at this point this should be taken care of */
    if(!xfdesktop_icon_get_position(icon, &row, &col)) {
        g_warning("Attempting to add item without a position");
        return;
    }

    xfdesktop_grid_unset_position_free(icon_view, icon);

    icon_view->priv->icons = g_list_prepend(icon_view->priv->icons, icon);

    g_signal_connect(G_OBJECT(icon), "pixbuf-changed",
                     G_CALLBACK(xfdesktop_icon_view_icon_changed),
                     icon_view);
    g_signal_connect(G_OBJECT(icon), "label-changed",
                     G_CALLBACK(xfdesktop_icon_view_icon_changed),
                     icon_view);

    xfdesktop_icon_view_update_icon_extents(icon_view, icon,
                                            &pixbuf_extents, &text_extents, &box_extents, &total_extents);
    gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                               total_extents.x, total_extents.y,
                               total_extents.width, total_extents.height);
}

static gboolean
xfdesktop_icon_view_icon_find_position(XfdesktopIconView *icon_view,
                                       XfdesktopIcon *icon)
{
    gint16 row, col;

    if (!xfdesktop_icon_get_position(icon, &row, &col)
        || !xfdesktop_grid_is_free_position(icon_view, row, col)
        || (((icon_view->priv->gravity & GRAVITY_BOTTOM) ||
             (icon_view->priv->gravity & GRAVITY_RIGHT)) && row == 0 && col == 0))
    {
        if (xfdesktop_grid_get_next_free_position(icon_view, &row, &col)) {
            XF_DEBUG("old position didn't exist or isn't free, got (%d,%d) instead",
                     row, col);
            xfdesktop_icon_set_position(icon, row, col);
        } else {
            XF_DEBUG("can't fit icon on screen");
            return FALSE;
        }
    }

    return TRUE;
}

void
xfdesktop_icon_view_add_item(XfdesktopIconView *icon_view,
                             XfdesktopIcon *icon)
{
    gint16 row, col;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));

    /* ensure the icon isn't already in an icon view */
    g_return_if_fail(!g_object_get_data(G_OBJECT(icon),
                                        "--xfdesktop-icon-view"));

    g_object_set_data(G_OBJECT(icon), "--xfdesktop-icon-view", icon_view);
    g_object_ref(G_OBJECT(icon));

    if(!gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        /* if we aren't realized, we don't know what our grid looks like, so
         * just hang onto the icon for later */
        if(xfdesktop_icon_get_position(icon, &row, &col)) {
            icon_view->priv->pending_icons = g_list_prepend(icon_view->priv->pending_icons,
                                                            icon);
        } else {
            icon_view->priv->pending_icons = g_list_append(icon_view->priv->pending_icons,
                                                           icon);
        }
    } else {
        if(xfdesktop_icon_view_icon_find_position(icon_view, icon))
            xfdesktop_icon_view_add_item_internal(icon_view, icon);
        else {
            icon_view->priv->pending_icons = g_list_append(icon_view->priv->pending_icons,
                                                           icon);
        }
    }
}

void
xfdesktop_icon_view_remove_item(XfdesktopIconView *icon_view,
                                XfdesktopIcon *icon)
{
    gint16 row, col;
    GList *l;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));

    l = g_list_find(icon_view->priv->icons, icon);
    if(l) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(icon),
                                             G_CALLBACK(xfdesktop_icon_view_icon_changed),
                                             icon_view);

        if(xfdesktop_icon_get_position(icon, &row, &col)) {
            xfdesktop_icon_view_invalidate_icon(icon_view, icon, FALSE);
            xfdesktop_grid_set_position_free(icon_view, row, col);
        }
        icon_view->priv->icons = g_list_delete_link(icon_view->priv->icons, l);
        icon_view->priv->selected_icons = g_list_remove(icon_view->priv->selected_icons,
                                                        icon);
        if(icon_view->priv->cursor == icon) {
            icon_view->priv->cursor = NULL;
            if(icon_view->priv->selected_icons)
                icon_view->priv->cursor = icon_view->priv->selected_icons->data;
        }
        if(icon_view->priv->first_clicked_item == icon)
            icon_view->priv->first_clicked_item = NULL;
        if(icon_view->priv->item_under_pointer == icon)
            icon_view->priv->item_under_pointer = NULL;
    } else if((l = g_list_find(icon_view->priv->pending_icons, icon))) {
        icon_view->priv->pending_icons = g_list_delete_link(icon_view->priv->pending_icons,
                                                            l);
    } else {
        g_warning("Attempt to remove icon %p from XfdesktopIconView %p, but it's not in there.",
                  icon, icon_view);
        return;
    }

    g_object_set_data(G_OBJECT(icon), "--xfdesktop-icon-view", NULL);
    g_object_unref(G_OBJECT(icon));

    if(icon_view->priv->pending_icons != NULL) {
        /* Move in any pending icons to the space available */
        xfdesktop_move_all_pending_icons_to_desktop(icon_view);
    }
}

void
xfdesktop_icon_view_remove_all(XfdesktopIconView *icon_view)
{
    GList *l;
    gint16 row, col;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_view->priv->pending_icons) {
        g_list_free_full(icon_view->priv->pending_icons, g_object_unref);
        icon_view->priv->pending_icons = NULL;
    }

    for(l = icon_view->priv->icons; l; l = l->next) {
        XfdesktopIcon *icon = XFDESKTOP_ICON(l->data);
        if(xfdesktop_icon_get_position(icon, &row, &col)) {
            xfdesktop_icon_view_invalidate_icon(icon_view, icon, FALSE);
            xfdesktop_grid_set_position_free(icon_view, row, col);
        }

        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(xfdesktop_icon_view_icon_changed),
                                             icon_view);
        g_object_set_data(G_OBJECT(l->data), "--xfdesktop-icon-view", NULL);
        g_object_unref(G_OBJECT(l->data));
    }

    if(G_LIKELY(icon_view->priv->icons)) {
        g_list_free(icon_view->priv->icons);
        icon_view->priv->icons = NULL;
    }

    if(icon_view->priv->selected_icons) {
        g_list_free(icon_view->priv->selected_icons);
        icon_view->priv->selected_icons = NULL;
    }

    icon_view->priv->item_under_pointer = NULL;
    icon_view->priv->cursor = NULL;
    icon_view->priv->first_clicked_item = NULL;
}

void
xfdesktop_icon_view_set_selection_mode(XfdesktopIconView *icon_view,
                                       GtkSelectionMode mode)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(mode <= GTK_SELECTION_MULTIPLE);

    if(mode == icon_view->priv->sel_mode)
        return;

    icon_view->priv->sel_mode = mode;

    switch(mode) {
        case GTK_SELECTION_NONE:
            g_warning("GTK_SELECTION_NONE is not implemented for " \
                      "XfdesktopIconView.  Falling back to " \
                      "GTK_SELECTION_SINGLE.");
            icon_view->priv->sel_mode = GTK_SELECTION_SINGLE;
            /* fall through */
        case GTK_SELECTION_SINGLE:
            if(g_list_length(icon_view->priv->selected_icons) > 1) {
                GList *l;
                /* TODO: enable later and make sure it works */
                /*gdk_window_freeze_updates(GTK_WIDGET(icon_view)->window);*/
                for(l = icon_view->priv->selected_icons->next; l; l = l->next) {
                    xfdesktop_icon_view_unselect_item(icon_view,
                                                      XFDESKTOP_ICON(l->data));
                }
                /*gdk_window_thaw_updates(GTK_WIDGET(icon_view)->window);*/
            }
            icon_view->priv->allow_rubber_banding = FALSE;
            break;

        case GTK_SELECTION_BROWSE:
            g_warning("GTK_SELECTION_BROWSE is not implemented for " \
                  "XfdesktopIconView.  Falling back to " \
                  "GTK_SELECTION_MULTIPLE.");
            icon_view->priv->sel_mode = GTK_SELECTION_MULTIPLE;
            /* fall through */
        default:
            icon_view->priv->allow_rubber_banding = TRUE;
            break;
    }
}

GtkSelectionMode
xfdesktop_icon_view_get_selection_mode(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view),
                         GTK_SELECTION_NONE);

    return icon_view->priv->sel_mode;
}

void
xfdesktop_icon_view_enable_drag_source(XfdesktopIconView *icon_view,
                                       GdkModifierType start_button_mask,
                                       const GtkTargetEntry *targets,
                                       gint n_targets,
                                       GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_view->priv->drag_source_set) {
        gtk_target_list_unref(icon_view->priv->source_targets);
        icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                              icon_view_n_targets);
    }

    icon_view->priv->foreign_source_actions = actions;
    icon_view->priv->foreign_source_mask = start_button_mask;

    gtk_target_list_add_table(icon_view->priv->source_targets, targets,
                              n_targets);

    gtk_drag_source_set(GTK_WIDGET(icon_view), start_button_mask, NULL, 0,
                        GDK_ACTION_MOVE | actions);
    gtk_drag_source_set_target_list(GTK_WIDGET(icon_view),
                                    icon_view->priv->source_targets);

    icon_view->priv->drag_source_set = TRUE;
}

void
xfdesktop_icon_view_enable_drag_dest(XfdesktopIconView *icon_view,
                                     const GtkTargetEntry *targets,
                                     gint n_targets,
                                     GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_view->priv->drag_dest_set) {
        gtk_target_list_unref(icon_view->priv->dest_targets);
        icon_view->priv->dest_targets = gtk_target_list_new(icon_view_targets,
                                                            icon_view_n_targets);
    }

    icon_view->priv->foreign_dest_actions = actions;

    gtk_target_list_add_table(icon_view->priv->dest_targets, targets,
                              n_targets);

    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0,
                      GDK_ACTION_MOVE | actions);
    gtk_drag_dest_set_target_list(GTK_WIDGET(icon_view),
                                  icon_view->priv->dest_targets);

    icon_view->priv->drag_dest_set = TRUE;
}

void
xfdesktop_icon_view_unset_drag_source(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(!icon_view->priv->drag_source_set)
        return;

    if(icon_view->priv->source_targets)
        gtk_target_list_unref(icon_view->priv->source_targets);

    icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                              icon_view_n_targets);

    gtk_drag_source_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);
    gtk_drag_source_set_target_list(GTK_WIDGET(icon_view),
                                    icon_view->priv->source_targets);

    icon_view->priv->drag_source_set = FALSE;
}

void
xfdesktop_icon_view_unset_drag_dest(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(!icon_view->priv->drag_dest_set)
        return;

    if(icon_view->priv->dest_targets)
        gtk_target_list_unref(icon_view->priv->dest_targets);

    icon_view->priv->dest_targets = gtk_target_list_new(icon_view_targets,
                                                        icon_view_n_targets);

    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);
    gtk_drag_dest_set_target_list(GTK_WIDGET(icon_view),
                                  icon_view->priv->dest_targets);

    icon_view->priv->drag_dest_set = FALSE;
}

XfdesktopIcon *
xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                          gint wx,
                                          gint wy)
{
    gint16 row, col;

    xfdesktop_xy_to_rowcol(icon_view, wx, wy, &row, &col);
    if(row >= icon_view->priv->nrows || col >= icon_view->priv->ncols || row < 0 || col < 0)
    {
        return NULL;
    }

    return xfdesktop_icon_view_icon_in_cell(icon_view, row, col);
}

GList *
xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);

    return g_list_copy(icon_view->priv->selected_icons);
}

void
xfdesktop_icon_view_select_item(XfdesktopIconView *icon_view,
                                XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(xfdesktop_icon_view_is_icon_selected(icon_view, icon))
        return;

    if(icon_view->priv->sel_mode == GTK_SELECTION_SINGLE)
        xfdesktop_icon_view_unselect_all(icon_view);

    icon_view->priv->selected_icons = g_list_prepend(icon_view->priv->selected_icons,
                                                     icon);
    xfdesktop_icon_view_invalidate_icon(icon_view, icon, TRUE);

    g_signal_emit(G_OBJECT(icon_view),
                  __signals[SIG_ICON_SELECTION_CHANGED],
                  0, NULL);
    xfdesktop_icon_selected(icon);
}

void
xfdesktop_icon_view_select_all(XfdesktopIconView *icon_view)
{
    GList *l;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(!icon_view->priv->icons)
        return;

    if(icon_view->priv->selected_icons
       && g_list_length(icon_view->priv->icons)
          == g_list_length(icon_view->priv->selected_icons))
    {
        return;
    }

    /* simplify: just free the entire list and repopulate it */
    if(icon_view->priv->selected_icons) {
        g_list_free(icon_view->priv->selected_icons);
        icon_view->priv->selected_icons = NULL;
    }

    for(l = icon_view->priv->icons; l; l = l->next) {
        icon_view->priv->selected_icons = g_list_prepend(icon_view->priv->selected_icons, l->data);
        xfdesktop_icon_view_invalidate_icon(icon_view, l->data, TRUE);
        xfdesktop_icon_selected(l->data);
    }

    g_signal_emit(G_OBJECT(icon_view),
                  __signals[SIG_ICON_SELECTION_CHANGED],
                  0, NULL);
}

void
xfdesktop_icon_view_unselect_item(XfdesktopIconView *icon_view,
                                  XfdesktopIcon *icon)
{
    GList *l;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));

    l = g_list_find(icon_view->priv->selected_icons, icon);
    if(l) {
        icon_view->priv->selected_icons = g_list_delete_link(icon_view->priv->selected_icons,
                                                             l);
        xfdesktop_icon_view_invalidate_icon(icon_view, icon, TRUE);
        g_signal_emit(G_OBJECT(icon_view),
                      __signals[SIG_ICON_SELECTION_CHANGED],
                      0, NULL);
    }
}

void
xfdesktop_icon_view_unselect_all(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_view->priv->selected_icons) {
        GList *repaint_icons = icon_view->priv->selected_icons;
        icon_view->priv->selected_icons = NULL;
        g_list_foreach(repaint_icons, xfdesktop_list_foreach_invalidate,
                       icon_view);
        g_list_free(repaint_icons);
        g_signal_emit(G_OBJECT(icon_view),
                      __signals[SIG_ICON_SELECTION_CHANGED],
                      0, NULL);
    }
}

void
xfdesktop_icon_view_set_icon_size(XfdesktopIconView *icon_view,
                                  guint icon_size)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    /* Choose the correct icon based on it size */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    wnck_set_default_icon_size(icon_size * gtk_widget_get_scale_factor(GTK_WIDGET(icon_view)));
G_GNUC_END_IGNORE_DEPRECATIONS

    if(icon_size == icon_view->priv->icon_size)
        return;

    icon_view->priv->icon_size = icon_size;

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_grid_do_resize(icon_view);
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }
}

guint
xfdesktop_icon_view_get_icon_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), 0);
    return icon_view->priv->icon_size;
}

void
xfdesktop_icon_view_set_primary(XfdesktopIconView *icon_view,
                                gboolean primary)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(primary == icon_view->priv->primary)
        return;

    icon_view->priv->primary = primary;

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_grid_do_resize(icon_view);
    }
}

void
xfdesktop_icon_view_set_font_size(XfdesktopIconView *icon_view,
                                  gdouble font_size_points)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(font_size_points == icon_view->priv->font_size)
        return;

    icon_view->priv->font_size = font_size_points;

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_modify_font_size(icon_view, font_size_points);
        xfdesktop_grid_do_resize(icon_view);
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }
}

gdouble
xfdesktop_icon_view_get_font_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), 0.0);
    return icon_view->priv->font_size;
}

void
xfdesktop_icon_view_set_center_text (XfdesktopIconView *icon_view,
                                     gboolean center_text)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (center_text == icon_view->priv->center_text)
        return;

    icon_view->priv->center_text = center_text;

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }
}

GtkWidget *
xfdesktop_icon_view_get_window_widget(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);

    return icon_view->priv->parent_window;
}

#if defined(DEBUG) && DEBUG > 0
guint
_xfdesktop_icon_view_n_items(XfdesktopIconView *icon_view)
{
    return g_list_length(icon_view->priv->pending_icons) + g_list_length(icon_view->priv->icons);
}
#endif
