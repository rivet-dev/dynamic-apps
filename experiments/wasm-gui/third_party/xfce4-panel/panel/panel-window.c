/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#include <libwnck/libwnck.h>

#include <libxfce4ui/libxfce4ui.h>

#include <xfconf/xfconf.h>
#include <common/panel-private.h>
#include <common/panel-debug.h>
#include <common/panel-utils.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>
#include <panel/panel-base-window.h>
#include <panel/panel-window.h>
#include <panel/panel-item-dialog.h>
#include <panel/panel-preferences-dialog.h>
#include <panel/panel-dialogs.h>
#include <panel/panel-dbus-service.h>
#include <panel/panel-plugin-external.h>
#include <panel/panel-tic-tac-toe.h>



#define SNAP_DISTANCE         (10)
#define DEFAULT_POPUP_DELAY   (225)
#define DEFAULT_POPDOWN_DELAY (350)
#define DEFAULT_AUTOHIDE_SIZE (3)
#define DEFAULT_POPDOWN_SPEED (25)
#define HANDLE_SPACING        (4)
#define HANDLE_DOTS           (2)
#define HANDLE_PIXELS         (2)
#define HANDLE_PIXEL_SPACE    (1)
#define HANDLE_SIZE           (HANDLE_DOTS * (HANDLE_PIXELS + \
                               HANDLE_PIXEL_SPACE) - HANDLE_PIXEL_SPACE)
#define HANDLE_SIZE_TOTAL     (2 * HANDLE_SPACING + HANDLE_SIZE)
#define IS_HORIZONTAL(window) ((window)->mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)



typedef enum _StrutsEgde    StrutsEgde;
typedef enum _AutohideBehavior AutohideBehavior;
typedef enum _AutohideState AutohideState;
typedef enum _SnapPosition  SnapPosition;
typedef enum _PluginProp    PluginProp;



static void         panel_window_get_property               (GObject          *object,
                                                             guint             prop_id,
                                                             GValue           *value,
                                                             GParamSpec       *pspec);
static void         panel_window_set_property               (GObject          *object,
                                                             guint             prop_id,
                                                             const GValue     *value,
                                                             GParamSpec       *pspec);
static void         panel_window_finalize                   (GObject          *object);
static gboolean     panel_window_draw                       (GtkWidget        *widget,
                                                             cairo_t          *cr);
static gboolean     panel_window_delete_event               (GtkWidget        *widget,
                                                             GdkEventAny      *event);
static gboolean     panel_window_enter_notify_event         (GtkWidget        *widget,
                                                             GdkEventCrossing *event);
static gboolean     panel_window_leave_notify_event         (GtkWidget        *widget,
                                                             GdkEventCrossing *event);
static gboolean     panel_window_drag_motion                (GtkWidget        *widget,
                                                             GdkDragContext   *context,
                                                             gint              x,
                                                             gint              y,
                                                             guint             drag_time);
static void         panel_window_drag_leave                 (GtkWidget        *widget,
                                                             GdkDragContext   *context,
                                                             guint             drag_time);
static gboolean     panel_window_motion_notify_event        (GtkWidget        *widget,
                                                             GdkEventMotion   *event);
static gboolean     panel_window_button_press_event         (GtkWidget        *widget,
                                                             GdkEventButton   *event);
static gboolean     panel_window_button_release_event       (GtkWidget        *widget,
                                                             GdkEventButton   *event);
static void         panel_window_get_preferred_width        (GtkWidget        *widget,
                                                             gint             *minimum_width,
                                                             gint             *natural_width);
static void         panel_window_get_preferred_height       (GtkWidget        *widget,
                                                             gint             *minimum_height,
                                                             gint             *natural_height);
static void         panel_window_get_preferred_width_for_height       (GtkWidget        *widget,
                                                                       gint              height,
                                                                       gint             *minimum_width,
                                                                       gint             *natural_width);
static void         panel_window_get_preferred_height_for_width       (GtkWidget        *widget,
                                                                       gint              width,
                                                                       gint             *minimum_height,
                                                                       gint             *natural_height);
static void         panel_window_size_allocate                        (GtkWidget        *widget,
                                                                       GtkAllocation    *alloc);
static void         panel_window_size_allocate_set_xy                 (PanelWindow      *window,
                                                                       gint              window_width,
                                                                       gint              window_height,
                                                                       gint             *return_x,
                                                                       gint             *return_y);
static void         panel_window_screen_changed                       (GtkWidget        *widget,
                                                                       GdkScreen        *previous_screen);
static void         panel_window_style_updated                        (GtkWidget        *widget);
static void         panel_window_update_dark_mode                     (gboolean          dark_mode);
static void         panel_window_realize                              (GtkWidget        *widget);
static StrutsEgde   panel_window_screen_struts_edge                   (PanelWindow      *window);
static void         panel_window_screen_struts_set                    (PanelWindow      *window);
static void         panel_window_screen_update_borders                (PanelWindow      *window);
static SnapPosition panel_window_snap_position                        (PanelWindow      *window);
static void         panel_window_display_layout_debug                 (GtkWidget        *widget);
static void         panel_window_screen_layout_changed                (GdkScreen        *screen,
                                                                       PanelWindow      *window);
static void         panel_window_active_window_changed                (WnckScreen       *screen,
                                                                       WnckWindow       *previous_window,
                                                                       PanelWindow      *window);
static void         panel_window_active_window_geometry_changed       (WnckWindow       *active_window,
                                                                       PanelWindow      *window);
static void         panel_window_active_window_state_changed          (WnckWindow       *active_window,
                                                                       WnckWindowState   changed,
                                                                       WnckWindowState   new,
                                                                       PanelWindow      *window);
static void         panel_window_autohide_timeout_destroy             (gpointer          user_data);
static void         panel_window_autohide_ease_out_timeout_destroy    (gpointer          user_data);
static void         panel_window_autohide_queue                       (PanelWindow      *window,
                                                                       AutohideState     new_state);
static void         panel_window_opacity_enter_queue                  (PanelWindow      *window,
                                                                       gboolean          enter);
static gboolean     panel_window_autohide_ease_out                    (gpointer          data);
static void         panel_window_set_autohide_behavior                (PanelWindow      *window,
                                                                       AutohideBehavior  behavior);
static void         panel_window_update_autohide_window               (PanelWindow      *window,
                                                                       WnckScreen       *screen,
                                                                       WnckWindow       *active_window);
static void         panel_window_menu_popup                           (PanelWindow      *window,
                                                                       GdkEventButton   *event,
                                                                       gboolean          show_tic_tac_toe);
static void         panel_window_plugins_update                       (PanelWindow      *window,
                                                                       PluginProp        prop);
static void         panel_window_plugin_set_mode                      (GtkWidget        *widget,
                                                                       gpointer          user_data);
static void         panel_window_plugin_set_size                      (GtkWidget        *widget,
                                                                       gpointer          user_data);
static void         panel_window_plugin_set_icon_size                 (GtkWidget        *widget,
                                                                       gpointer          user_data);
static void         panel_window_plugin_set_dark_mode                 (GtkWidget        *widget,
                                                                       gpointer          user_data);
static void         panel_window_plugin_set_nrows                     (GtkWidget        *widget,
                                                                       gpointer          user_data);
static void         panel_window_plugin_set_screen_position           (GtkWidget        *widget,
                                                                       gpointer          user_data);



enum
{
  PROP_0,
  PROP_ID,
  PROP_MODE,
  PROP_SIZE,
  PROP_ICON_SIZE,
  PROP_NROWS,
  PROP_LENGTH,
  PROP_LENGTH_MAX,
  PROP_LENGTH_ADJUST,
  PROP_POSITION_LOCKED,
  PROP_AUTOHIDE_BEHAVIOR,
  PROP_POPDOWN_SPEED,
  PROP_SPAN_MONITORS,
  PROP_OUTPUT_NAME,
  PROP_POSITION,
  PROP_ENABLE_STRUTS,
  PROP_DARK_MODE
};

enum _PluginProp
{
  PLUGIN_PROP_MODE,
  PLUGIN_PROP_SCREEN_POSITION,
  PLUGIN_PROP_NROWS,
  PLUGIN_PROP_SIZE,
  PLUGIN_PROP_ICON_SIZE,
  PLUGIN_PROP_DARK_MODE
};

enum _AutohideBehavior
{
  AUTOHIDE_BEHAVIOR_NEVER = 0,
  AUTOHIDE_BEHAVIOR_INTELLIGENTLY,
  AUTOHIDE_BEHAVIOR_ALWAYS,
};

enum _AutohideState
{
  AUTOHIDE_VISIBLE,      /* visible */
  AUTOHIDE_POPDOWN,      /* visible, but hide timeout is running */
  AUTOHIDE_POPDOWN_SLOW, /* same as popdown, but timeout is 4x longer */
  AUTOHIDE_HIDDEN,       /* invisible */
  AUTOHIDE_POPUP,        /* invisible, but show timeout is running */
};

enum _SnapPosition
{
  /* no snapping */
  SNAP_POSITION_NONE, /* snapping */

  /* right edge */
  SNAP_POSITION_E,    /* right */
  SNAP_POSITION_NE,   /* top right */
  SNAP_POSITION_EC,   /* right center */
  SNAP_POSITION_SE,   /* bottom right */

  /* left edge */
  SNAP_POSITION_W,    /* left */
  SNAP_POSITION_NW,   /* top left */
  SNAP_POSITION_WC,   /* left center */
  SNAP_POSITION_SW,   /* bottom left */

  /* top and bottom */
  SNAP_POSITION_NC,   /* top center */
  SNAP_POSITION_SC,   /* bottom center */
  SNAP_POSITION_N,    /* top */
  SNAP_POSITION_S,    /* bottom */
};

enum
{
  EDGE_GRAVITY_NONE   = 0,
  EDGE_GRAVITY_START  = (SNAP_POSITION_NE - SNAP_POSITION_E),
  EDGE_GRAVITY_CENTER = (SNAP_POSITION_EC - SNAP_POSITION_E),
  EDGE_GRAVITY_END    = (SNAP_POSITION_SE - SNAP_POSITION_E)
};

enum _StrutsEgde
{
  STRUTS_EDGE_NONE = 0,
  STRUTS_EDGE_LEFT,
  STRUTS_EDGE_RIGHT,
  STRUTS_EDGE_TOP,
  STRUTS_EDGE_BOTTOM
};

enum
{
  STRUT_LEFT = 0,
  STRUT_RIGHT,
  STRUT_TOP,
  STRUT_BOTTOM,
  STRUT_LEFT_START_Y,
  STRUT_LEFT_END_Y,
  STRUT_RIGHT_START_Y,
  STRUT_RIGHT_END_Y,
  STRUT_TOP_START_X,
  STRUT_TOP_END_X,
  STRUT_BOTTOM_START_X,
  STRUT_BOTTOM_END_X,
  N_STRUTS
};

struct _PanelWindowClass
{
  PanelBaseWindowClass __parent__;
};

struct _PanelWindow
{
  PanelBaseWindow      __parent__;

  /* unique id of this panel */
  gint                 id;

  /* whether the user is allowed to make
   * changes to this window */
  guint                locked : 1;

  /* screen and working area of this panel */
  GdkScreen           *screen;
  GdkDisplay          *display;
  GdkRectangle         area;

  /* struts information */
  StrutsEgde           struts_edge;
  gulong               struts[N_STRUTS];
  guint                struts_enabled : 1;

  /* dark mode */
  gboolean             dark_mode;

  /* window positioning */
  guint                size;
  guint                icon_size;
  gdouble              length;
  guint                length_max;
  guint                length_adjust : 1;
  XfcePanelPluginMode  mode;
  guint                nrows;
  SnapPosition         snap_position;
  gboolean             floating;
  guint                span_monitors : 1;
  gchar               *output_name;

  /* allocated position of the panel */
  GdkRectangle         alloc;

  /* autohiding */
  WnckScreen          *wnck_screen;
  WnckWindow          *wnck_active_window;
  GtkWidget           *autohide_window;
  AutohideBehavior     autohide_behavior;
  AutohideState        autohide_state;
  guint                autohide_timeout_id;
  guint                autohide_ease_out_id;
  guint                opacity_timeout_id;
  gint                 autohide_block;
  gint                 autohide_size;
  guint                popdown_speed;
  gint                 popdown_progress;

  /* popup/down delay from gtk style */
  gint                 popup_delay;
  gint                 popdown_delay;

  /* whether the window position is locked */
  guint                position_locked : 1;

  /* window base point */
  gint                 base_x;
  gint                 base_y;

  /* window drag information */
  guint32              grab_time;
  gint                 grab_x;
  gint                 grab_y;
};

/* used for a full XfcePanelWindow name in the class, but not in the code */
typedef PanelWindow      XfcePanelWindow;
typedef PanelWindowClass XfcePanelWindowClass;



static GdkAtom cardinal_atom = 0;
static GdkAtom net_wm_strut_partial_atom = 0;



G_DEFINE_TYPE (XfcePanelWindow, panel_window, PANEL_TYPE_BASE_WINDOW)



static void
panel_window_class_init (PanelWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = panel_window_get_property;
  gobject_class->set_property = panel_window_set_property;
  gobject_class->finalize = panel_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->draw = panel_window_draw;
  gtkwidget_class->delete_event = panel_window_delete_event;
  gtkwidget_class->enter_notify_event = panel_window_enter_notify_event;
  gtkwidget_class->leave_notify_event = panel_window_leave_notify_event;
  gtkwidget_class->drag_motion = panel_window_drag_motion;
  gtkwidget_class->drag_leave = panel_window_drag_leave;
  gtkwidget_class->motion_notify_event = panel_window_motion_notify_event;
  gtkwidget_class->button_press_event = panel_window_button_press_event;
  gtkwidget_class->button_release_event = panel_window_button_release_event;
  gtkwidget_class->get_preferred_width = panel_window_get_preferred_width;
  gtkwidget_class->get_preferred_height = panel_window_get_preferred_height;
  gtkwidget_class->get_preferred_width_for_height = panel_window_get_preferred_width_for_height;
  gtkwidget_class->get_preferred_height_for_width = panel_window_get_preferred_height_for_width;
  gtkwidget_class->size_allocate = panel_window_size_allocate;
  gtkwidget_class->screen_changed = panel_window_screen_changed;
  gtkwidget_class->style_updated = panel_window_style_updated;
  gtkwidget_class->realize = panel_window_realize;

  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   g_param_spec_int ("id", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                                                     | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode", NULL, NULL,
                                                      XFCE_TYPE_PANEL_PLUGIN_MODE,
                                                      XFCE_PANEL_PLUGIN_MODE_HORIZONTAL,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_uint ("size", NULL, NULL,
                                                      16, 128, 48,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SIZE,
                                   g_param_spec_uint ("icon-size", NULL, NULL,
                                                      0, 256, 0,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DARK_MODE,
                                   g_param_spec_boolean ("dark-mode", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_NROWS,
                                   g_param_spec_uint ("nrows", NULL, NULL,
                                                      1, 6, 1,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* keep this property as a percentage for backwards compatibility */
  g_object_class_install_property (gobject_class,
                                   PROP_LENGTH,
                                   g_param_spec_double ("length", NULL, NULL,
                                                        1, 100, 10,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_LENGTH_MAX,
                                   g_param_spec_uint ("length-max", NULL, NULL,
                                                      1, G_MAXUINT, 1,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_LENGTH_ADJUST,
                                   g_param_spec_boolean ("length-adjust", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_POSITION_LOCKED,
                                   g_param_spec_boolean ("position-locked", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_AUTOHIDE_BEHAVIOR,
                                   g_param_spec_uint ("autohide-behavior", NULL, NULL,
                                                      AUTOHIDE_BEHAVIOR_NEVER,
                                                      AUTOHIDE_BEHAVIOR_ALWAYS,
                                                      AUTOHIDE_BEHAVIOR_NEVER,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_POPDOWN_SPEED,
                                   g_param_spec_uint ("popdown-speed", NULL, NULL,
                                                      0, G_MAXINT, 25,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SPAN_MONITORS,
                                   g_param_spec_boolean ("span-monitors", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_OUTPUT_NAME,
                                   g_param_spec_string ("output-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_POSITION,
                                   g_param_spec_string ("position", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ENABLE_STRUTS,
                                   g_param_spec_boolean ("enable-struts", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("popup-delay",
                                                             NULL,
                                                             "Time before the panel will unhide on an enter event",
                                                             1, G_MAXINT,
                                                             DEFAULT_POPUP_DELAY,
                                                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("popdown-delay",
                                                             NULL,
                                                             "Time before the panel will hide on a leave event",
                                                             1, G_MAXINT,
                                                             DEFAULT_POPDOWN_DELAY,
                                                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("autohide-size",
                                                             NULL,
                                                             "Size of hidden panel",
                                                             1, G_MAXINT,
                                                             DEFAULT_AUTOHIDE_SIZE,
                                                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* initialize the atoms */
  cardinal_atom = gdk_atom_intern_static_string ("CARDINAL");
  net_wm_strut_partial_atom = gdk_atom_intern_static_string ("_NET_WM_STRUT_PARTIAL");
}



static void
panel_window_decrease_size (PanelWindow *window)
{
  g_signal_handlers_disconnect_by_func (window, panel_window_decrease_size, NULL);
  g_object_set (window, "size", window->size - 1, NULL);
}



static void
panel_window_force_redraw (PanelWindow *window)
{
  g_signal_connect_after (window, "draw", G_CALLBACK (panel_window_decrease_size), NULL);
  g_object_set (window, "size", window->size + 1, NULL);
}



static void
panel_window_is_active_changed (PanelWindow *window)
{
  if (gtk_window_is_active (GTK_WINDOW (window)))
    panel_window_freeze_autohide (window);
  else
    panel_window_thaw_autohide (window);
}



static void
panel_window_init (PanelWindow *window)
{
  window->id = -1;
  window->locked = TRUE;
  window->screen = NULL;
  window->display = NULL;
  window->wnck_screen = NULL;
  window->wnck_active_window = NULL;
  window->struts_edge = STRUTS_EDGE_NONE;
  window->struts_enabled = TRUE;
  window->mode = XFCE_PANEL_PLUGIN_MODE_HORIZONTAL;
  window->size = 48;
  window->icon_size = 0;
  window->dark_mode = FALSE;
  window->nrows = 1;
  window->length = 0.10;
  window->length_max = 1;
  window->length_adjust = TRUE;
  window->snap_position = SNAP_POSITION_NONE;
  window->floating = TRUE;
  window->span_monitors = FALSE;
  window->position_locked = FALSE;
  window->autohide_behavior = AUTOHIDE_BEHAVIOR_NEVER;
  window->autohide_state = AUTOHIDE_VISIBLE;
  window->autohide_timeout_id = 0;
  window->autohide_ease_out_id = 0;
  window->opacity_timeout_id = 0;
  window->autohide_block = 0;
  window->autohide_size = DEFAULT_AUTOHIDE_SIZE;
  window->popup_delay = DEFAULT_POPUP_DELAY;
  window->popdown_delay = DEFAULT_POPDOWN_DELAY;
  window->popdown_speed = DEFAULT_POPDOWN_SPEED;
  window->popdown_progress = 0;
  window->base_x = -1;
  window->base_y = -1;
  window->grab_time = 0;
  window->grab_x = 0;
  window->grab_y = 0;

  /* not resizable, so allocation will follow size request */
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  /* set additional events */
  gtk_widget_add_events (GTK_WIDGET (window), GDK_BUTTON_PRESS_MASK);

  /* create a 'fake' drop zone for autohide drag motion */
  gtk_drag_dest_set (GTK_WIDGET (window), 0, NULL, 0, 0);

  /* set the screen */
  panel_window_screen_changed (GTK_WIDGET (window), NULL);

  /* a workaround to force external plugins to fully re-render when scale factor changes */
  g_signal_connect (window, "notify::scale-factor", G_CALLBACK (panel_window_force_redraw), NULL);

  /* block autohide when the panel has input focus, e.g. via a GtkEntry in a plugin */
  g_signal_connect (window, "notify::is-active", G_CALLBACK (panel_window_is_active_changed), NULL);
}



static void
panel_window_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  PanelWindow *window = PANEL_WINDOW (object);
  gchar       *position;

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int (value, window->id);
      break;

    case PROP_MODE:
      g_value_set_enum (value, window->mode);
      break;

    case PROP_SIZE:
      g_value_set_uint (value, window->size);
      break;

    case PROP_ICON_SIZE:
      g_value_set_uint (value, window->icon_size);
      break;

    case PROP_DARK_MODE:
      g_value_set_boolean (value, window->dark_mode);
      break;

    case PROP_NROWS:
      g_value_set_uint (value, window->nrows);
      break;

    case PROP_LENGTH:
      g_value_set_double (value, window->length * 100);
      break;

    case PROP_LENGTH_MAX:
      g_value_set_uint (value, window->length_max);
      break;

    case PROP_LENGTH_ADJUST:
      g_value_set_boolean (value, window->length_adjust);
      break;

    case PROP_POSITION_LOCKED:
      g_value_set_boolean (value, window->position_locked);
      break;

    case PROP_AUTOHIDE_BEHAVIOR:
      g_value_set_uint (value, window->autohide_behavior);
      break;

    case PROP_POPDOWN_SPEED:
      g_value_set_uint (value, window->popdown_speed);
      break;

    case PROP_SPAN_MONITORS:
      g_value_set_boolean (value, window->span_monitors);
      break;

    case PROP_OUTPUT_NAME:
      g_value_set_static_string (value, window->output_name);
      break;

    case PROP_POSITION:
      position = g_strdup_printf ("p=%d;x=%d;y=%d",
                                  window->snap_position,
                                  window->base_x,
                                  window->base_y);
      g_value_take_string (value, position);
      break;

    case PROP_ENABLE_STRUTS:
      g_value_set_boolean (value, window->struts_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
panel_window_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  PanelWindow         *window = PANEL_WINDOW (object);
  gboolean             val_bool;
  guint                val_uint;
  gdouble              val_double;
  const gchar         *val_string;
  gboolean             update = FALSE;
  gint                 x, y, snap_position;
  XfcePanelPluginMode  val_mode;

  switch (prop_id)
    {
    case PROP_ID:
      window->id = g_value_get_int (value);
      break;

    case PROP_MODE:
      val_mode = g_value_get_enum (value);
      if (window->mode != val_mode)
        {
          window->mode = val_mode;
          panel_window_screen_layout_changed (window->screen, window);
        }
      panel_base_window_orientation_changed (PANEL_BASE_WINDOW (window), window->mode);
      /* send the new orientation and screen position to the panel plugins */
      panel_window_plugins_update (window, PLUGIN_PROP_MODE);
      panel_window_plugins_update (window, PLUGIN_PROP_SCREEN_POSITION);
      break;

    case PROP_SIZE:
      val_uint = g_value_get_uint (value);
      if (window->size != val_uint)
        {
          window->size = val_uint;
          gtk_widget_queue_resize (GTK_WIDGET (window));
        }

      /* send the new size to the panel plugins */
      panel_window_plugins_update (window, PLUGIN_PROP_SIZE);
      break;

    case PROP_ICON_SIZE:
      val_uint = g_value_get_uint (value);
      if (window->icon_size != val_uint)
        {
          window->icon_size = val_uint;
        }

      /* send the new icon size to the panel plugins */
      panel_window_plugins_update (window, PLUGIN_PROP_ICON_SIZE);
      break;

    case PROP_DARK_MODE:
      val_bool = g_value_get_boolean (value);
      if (window->dark_mode != val_bool)
        {
          window->dark_mode = val_bool;
        }

      /* set dark mode for the main application and plugins */
      panel_window_update_dark_mode (window->dark_mode);
      panel_window_plugins_update (window, PLUGIN_PROP_DARK_MODE);
      break;

    case PROP_NROWS:
      val_uint = g_value_get_uint (value);
      if (window->nrows != val_uint)
        {
          window->nrows = val_uint;
          gtk_widget_queue_resize (GTK_WIDGET (window));
        }

      /* send the new size to the panel plugins */
      panel_window_plugins_update (window, PLUGIN_PROP_NROWS);
      break;

    case PROP_LENGTH:
      val_double = g_value_get_double (value) / 100;
      if (window->length != val_double)
        {
          if (window->length == 1.00 || val_double == 1.00)
            update = TRUE;

          window->length = val_double;

          if (update)
            panel_window_screen_update_borders (window);

          gtk_widget_queue_resize (GTK_WIDGET (window));
        }
      break;

    case PROP_LENGTH_MAX:
      val_uint = g_value_get_uint (value);
      if (window->length_max != val_uint)
        {
          window->length_max = val_uint;
        }
      break;

    case PROP_LENGTH_ADJUST:
      val_bool = g_value_get_boolean (value);
      if (window->length_adjust != val_bool)
        {
          window->length_adjust = !!val_bool;
          gtk_widget_queue_resize (GTK_WIDGET (window));
        }
      break;

    case PROP_POSITION_LOCKED:
      val_bool = g_value_get_boolean (value);
      if (window->position_locked != val_bool)
        {
          window->position_locked = !!val_bool;
          gtk_widget_queue_resize (GTK_WIDGET (window));
        }
      break;

    case PROP_AUTOHIDE_BEHAVIOR:
      panel_window_set_autohide_behavior (window, MIN (g_value_get_uint (value),
                                                       AUTOHIDE_BEHAVIOR_ALWAYS));
      break;


    case PROP_POPDOWN_SPEED:
      val_uint = g_value_get_uint (value);
      if (window->popdown_speed != val_uint)
        {
          window->popdown_speed = val_uint;
        }
      break;

    case PROP_SPAN_MONITORS:
      val_bool = g_value_get_boolean (value);
      if (window->span_monitors != val_bool)
        {
          window->span_monitors = !!val_bool;
          panel_window_screen_layout_changed (window->screen, window);
        }
      break;

    case PROP_OUTPUT_NAME:
      g_free (window->output_name);

      val_string = g_value_get_string (value);
      if (panel_str_is_empty (val_string))
        window->output_name = NULL;
      else
        window->output_name = g_strdup (val_string);

      panel_window_screen_layout_changed (window->screen, window);
      break;

    case PROP_POSITION:
      val_string = g_value_get_string (value);
      if (!panel_str_is_empty (val_string)
          && sscanf (val_string, "p=%d;x=%d;y=%d", &snap_position, &x, &y) == 3)
        {
          window->snap_position = CLAMP (snap_position, SNAP_POSITION_NONE, SNAP_POSITION_S);
          window->base_x = MAX (x, 0);
          window->base_y = MAX (y, 0);

          panel_window_screen_layout_changed (window->screen, window);

          /* send the new screen position to the panel plugins */
          panel_window_plugins_update (window, PLUGIN_PROP_SCREEN_POSITION);
        }
      break;

    case PROP_ENABLE_STRUTS:
      val_bool = g_value_get_boolean (value);
      if (val_bool != window->struts_enabled)
        {
          window->struts_enabled = val_bool;
          panel_window_screen_layout_changed (window->screen, window);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
panel_window_finalize (GObject *object)
{
  PanelWindow *window = PANEL_WINDOW (object);

  /* disconnect from active screen and window */
  panel_window_update_autohide_window (window, NULL, NULL);

  /* stop running autohide timeout */
  if (G_UNLIKELY (window->autohide_timeout_id != 0))
    g_source_remove (window->autohide_timeout_id);

  if (G_UNLIKELY (window->autohide_ease_out_id != 0))
    g_source_remove (window->autohide_ease_out_id);

  if (G_UNLIKELY (window->opacity_timeout_id != 0))
    g_source_remove (window->opacity_timeout_id);

  /* destroy the autohide window */
  if (window->autohide_window != NULL)
    gtk_widget_destroy (window->autohide_window);

  g_free (window->output_name);

  (*G_OBJECT_CLASS (panel_window_parent_class)->finalize) (object);
}



static gboolean
panel_window_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  PanelWindow      *window = PANEL_WINDOW (widget);
  GdkRGBA           fg_rgba;
  GdkRGBA          *dark_rgba;
  guint             xx, yy, i;
  gint              xs, xe, ys, ye;
  gint              handle_w, handle_h;
  GtkStyleContext  *ctx;

  /* expose the background and borders handled in PanelBaseWindow */
  (*GTK_WIDGET_CLASS (panel_window_parent_class)->draw) (widget, cr);

  if (window->position_locked || !gtk_widget_is_drawable (widget))
    return FALSE;

  if (IS_HORIZONTAL (window))
    {
      handle_h = window->alloc.height / 2;
      handle_w = HANDLE_SIZE;

      xs = HANDLE_SPACING + 1;
      xe = window->alloc.width - HANDLE_SIZE - HANDLE_SIZE;
      ys = ye = (window->alloc.height - handle_h) / 2;
    }
  else
    {
      handle_h = HANDLE_SIZE;
      handle_w = window->alloc.width / 2;

      xs = xe = (window->alloc.width - handle_w) / 2;
      ys = HANDLE_SPACING + 1;
      ye = window->alloc.height - HANDLE_SIZE - HANDLE_SIZE;
    }

  /* create cairo context and set some default properties */
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

  ctx = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (ctx, gtk_widget_get_state_flags(widget), &fg_rgba);
  dark_rgba = gdk_rgba_copy (&fg_rgba);
  fg_rgba.alpha = 0.5;
  dark_rgba->alpha = 0.15;

  for (i = HANDLE_PIXELS; i >= HANDLE_PIXELS - 1; i--)
    {
      /* set the source color */
      if (i == HANDLE_PIXELS)
        gdk_cairo_set_source_rgba (cr, &fg_rgba);
      else
        gdk_cairo_set_source_rgba (cr, dark_rgba);

      /* draw the dots */
      for (xx = 0; xx < (guint) handle_w; xx += HANDLE_PIXELS + HANDLE_PIXEL_SPACE)
        for (yy = 0; yy < (guint) handle_h; yy += HANDLE_PIXELS + HANDLE_PIXEL_SPACE)
          {
            cairo_rectangle (cr, xs + xx, ys + yy, i, i);
            cairo_rectangle (cr, xe + xx, ye + yy, i, i);
          }

      /* fill the rectangles */
      cairo_fill (cr);
    }
  gdk_rgba_free (dark_rgba);

  return FALSE;
}



static gboolean
panel_window_delete_event (GtkWidget   *widget,
                           GdkEventAny *event)
{
  /* do not respond to alt-f4 or any other signals */
  return TRUE;
}



static gboolean
panel_window_enter_notify_event (GtkWidget        *widget,
                                 GdkEventCrossing *event)
{
  PanelWindow *window = PANEL_WINDOW (widget);

  /* update autohide status */
  if (event->detail != GDK_NOTIFY_INFERIOR
      && window->autohide_block == 0)
    {
      panel_window_opacity_enter_queue (window, TRUE);
      if (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER)
        panel_window_autohide_queue (window, AUTOHIDE_VISIBLE);
    }

  return FALSE;
}



static gboolean
panel_window_leave_notify_event (GtkWidget        *widget,
                                 GdkEventCrossing *event)
{
  if (event->detail != GDK_NOTIFY_INFERIOR)
    panel_window_drag_leave (widget, NULL, 0);

  return FALSE;
}



static gboolean
panel_window_drag_motion (GtkWidget      *widget,
                          GdkDragContext *context,
                          gint            x,
                          gint            y,
                          guint           drag_time)
{
  if ((*GTK_WIDGET_CLASS (panel_window_parent_class)->drag_motion) != NULL)
    (*GTK_WIDGET_CLASS (panel_window_parent_class)->drag_motion) (widget, context, x, y, drag_time);

  return TRUE;
}



static void
panel_window_drag_leave (GtkWidget      *widget,
                         GdkDragContext *context,
                         guint           drag_time)
{
  PanelWindow *window = PANEL_WINDOW (widget);

  if (window->autohide_block > 0)
    return;

  panel_window_opacity_enter_queue (window, FALSE);

  /* queue an autohide timeout if needed */
  if (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER)
    {
      /* simulate a geometry change to check for overlapping windows with intelligent hiding */
      if (window->autohide_behavior == AUTOHIDE_BEHAVIOR_INTELLIGENTLY)
        panel_window_active_window_geometry_changed (window->wnck_active_window, window);
      /* otherwise just hide the panel */
      else
        panel_window_autohide_queue (window, AUTOHIDE_POPDOWN_SLOW);
    }
}



static gboolean
panel_window_motion_notify_event (GtkWidget      *widget,
                                  GdkEventMotion *event)
{
  PanelWindow     *window = PANEL_WINDOW (widget);
  PanelBaseWindow *base_window = PANEL_BASE_WINDOW (widget);
  gint             pointer_x, pointer_y;
  gint             window_x, window_y;
  gint             high;
  GdkScreen       *screen = NULL;
  gboolean         retval = TRUE;

  /* sometimes the panel does not receive enter events so let's fill this gap here */
  if (base_window->is_composited && ! base_window->opacity_is_enter)
    panel_window_opacity_enter_queue (window, TRUE);

  /* leave when the pointer is not grabbed */
  if (G_LIKELY (window->grab_time == 0))
    return FALSE;

  /* get the pointer position from the event */
  pointer_x = event->x_root;
  pointer_y = event->y_root;

  /* the 0x0 coordinate is a sign the cursor is on another screen then
   * the panel that is currently dragged */
  if (event->x == 0 && event->y == 0)
    {
      gdk_device_get_position (event->device,
                               &screen, NULL, NULL);
      if (screen != gtk_window_get_screen (GTK_WINDOW (window)))
        {
          gtk_window_set_screen (GTK_WINDOW (window), screen);

          /* stop the drag, we somehow loose the motion event */
          window->grab_time = 0;
          panel_window_thaw_autohide (window);
          retval = FALSE;
        }
    }
  /* check if the pointer moved to another monitor */
  else if (!window->span_monitors
           && (pointer_x < window->area.x
               || pointer_y < window->area.y
               || pointer_x > window->area.x + window->area.width
               || pointer_y > window->area.y + window->area.height))
    {
      /* set base point to cursor position and update working area */
      window->base_x = pointer_x;
      window->base_y = pointer_y;
      panel_window_screen_layout_changed (window->screen, window);
    }

  /* calculate the new window position, but keep it inside the working geometry */
  window_x = pointer_x - window->grab_x;
  high = window->area.x + window->area.width - window->alloc.width;
  window_x = CLAMP (window_x, window->area.x, high);

  window_y = pointer_y - window->grab_y;
  high = window->area.y + window->area.height - window->alloc.height;
  window_y = CLAMP (window_y, window->area.y, high);

  /* update the grab coordinates */
  window->grab_x = pointer_x - window_x;
  window->grab_y = pointer_y - window_y;

  /* update the base coordinates */
  window->base_x = window_x + window->alloc.width / 2;
  window->base_y = window_y + window->alloc.height / 2;

  /* update the allocation */
  window->alloc.x = window_x;
  window->alloc.y = window_y;

  /* update the snapping position */
  window->snap_position = panel_window_snap_position (window);

  /* update the working area */
  panel_window_screen_layout_changed (window->screen, window);

  return retval;
}



static gboolean
panel_window_button_press_event (GtkWidget      *widget,
                                 GdkEventButton *event)
{
  PanelWindow   *window = PANEL_WINDOW (widget);
  GdkSeat       *seat;
  GdkCursor     *cursor;
  GdkGrabStatus  status;
  guint          modifiers;

  /* leave if the event is not for this window */
  if (event->window != gtk_widget_get_window (widget))
    goto end;

  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  if (event->button == 1
      && event->type == GDK_BUTTON_PRESS
      && !window->position_locked
      && modifiers == 0)
    {
      panel_return_val_if_fail (window->grab_time == 0, FALSE);

      /* create a cursor */
      cursor = gdk_cursor_new_for_display (window->display, GDK_FLEUR);

      /* grab the pointer for dragging the window */
      seat = gdk_device_get_seat (event->device);

      status = gdk_seat_grab (seat, event->window,
                              GDK_SEAT_CAPABILITY_ALL_POINTING,
                              FALSE, cursor, (GdkEvent*)event, NULL, NULL);

      if (cursor != NULL)
        g_object_unref (cursor);

      /* set the grab info if the grab was successfully made */
      if (G_LIKELY (status == GDK_GRAB_SUCCESS))
        {
          window->grab_time = event->time;
          window->grab_x = event->x;
          window->grab_y = event->y;
          panel_window_freeze_autohide (window);
        }

      return !!(status == GDK_GRAB_SUCCESS);
    }
  else if (event->button == 3
           || (event->button == 1 && modifiers == GDK_CONTROL_MASK))
    {
      panel_window_menu_popup (window, event, modifiers == GDK_SHIFT_MASK);

      return TRUE;
    }

end:
  if (GTK_WIDGET_CLASS (panel_window_parent_class)->button_release_event != NULL)
    return (*GTK_WIDGET_CLASS (panel_window_parent_class)->button_release_event) (widget, event);
  else
    return FALSE;
}



static gboolean
panel_window_button_release_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  PanelWindow *window = PANEL_WINDOW (widget);

  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);

  if (window->grab_time != 0)
    {
      /* ungrab the pointer */
      gdk_seat_ungrab (gdk_device_get_seat (event->device));
      window->grab_time = 0;

      /* store the new position */
      g_object_notify (G_OBJECT (widget), "position");

      /* send the new screen position to the panel plugins */
      panel_window_plugins_update (window, PLUGIN_PROP_SCREEN_POSITION);

      /* release autohide lock */
      panel_window_thaw_autohide (window);

      return TRUE;
    }

  if (GTK_WIDGET_CLASS (panel_window_parent_class)->button_release_event != NULL)
    return (*GTK_WIDGET_CLASS (panel_window_parent_class)->button_release_event) (widget, event);
  else
    return FALSE;
}



static void
panel_window_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum_width,
                                  gint      *natural_width)
{
  PanelWindow    *window = PANEL_WINDOW (widget);
  gint            m_width = 0;
  gint            n_width = 0;
  gint            length;
  gint            extra_width = 0;
  PanelBorders    borders;

  /* get the child requisition */
  if (gtk_bin_get_child (GTK_BIN (widget)) != NULL)
    gtk_widget_get_preferred_width (gtk_bin_get_child (GTK_BIN (widget)), &m_width, &n_width);

  /* handle size */
  if (!window->position_locked)
    {
      if (IS_HORIZONTAL (window))
        extra_width += 2 * HANDLE_SIZE_TOTAL;
    }

  /* get the active borders */
  borders = panel_base_window_get_borders (PANEL_BASE_WINDOW (window));
  if (PANEL_HAS_FLAG (borders, PANEL_BORDER_LEFT))
    extra_width++;
  if (PANEL_HAS_FLAG (borders, PANEL_BORDER_RIGHT))
    extra_width++;

  m_width += extra_width;
  n_width += extra_width;

  /* respect the length and monitor/screen size */
  if (IS_HORIZONTAL (window))
    {
      if (!window->length_adjust)
        {
          m_width = n_width = extra_width;
        }

      length = window->area.width * window->length;
      m_width = CLAMP (m_width, length, window->area.width);
      n_width = CLAMP (n_width, length, window->area.width);
    }

  if (minimum_width != NULL)
    *minimum_width = m_width;

  if (natural_width != NULL)
    *natural_width = n_width;
}



static void
panel_window_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum_height,
                                   gint      *natural_height)
{
  PanelWindow    *window = PANEL_WINDOW (widget);
  gint            m_height = 0;
  gint            n_height = 0;
  gint            length;
  gint            extra_height = 0;
  PanelBorders    borders;

  /* get the child requisition */
  if (gtk_bin_get_child (GTK_BIN (widget)) != NULL)
    gtk_widget_get_preferred_height (gtk_bin_get_child (GTK_BIN (widget)), &m_height, &n_height);

  /* handle size */
  if (!window->position_locked)
    {
      if (!IS_HORIZONTAL (window))
        extra_height += 2 * HANDLE_SIZE_TOTAL;
    }

  /* get the active borders */
  borders = panel_base_window_get_borders (PANEL_BASE_WINDOW (window));
  if (PANEL_HAS_FLAG (borders, PANEL_BORDER_TOP))
    extra_height++;
  if (PANEL_HAS_FLAG (borders, PANEL_BORDER_BOTTOM))
    extra_height++;

  m_height += extra_height;
  n_height += extra_height;

  /* respect the length and monitor/screen size */
  if (!IS_HORIZONTAL (window))
    {
      if (!window->length_adjust)
        {
          m_height = n_height = extra_height;
        }

      length = window->area.height * window->length;
      m_height = CLAMP (m_height, length, window->area.height);
      n_height = CLAMP (n_height, length, window->area.height);
    }

  if (minimum_height != NULL)
    *minimum_height = m_height;

  if (natural_height != NULL)
    *natural_height = n_height;
}



static void
panel_window_get_preferred_width_for_height (GtkWidget *widget,
                                             gint       height,
                                             gint      *minimum_width,
                                             gint      *natural_width)
{
  panel_window_get_preferred_width (widget, minimum_width, natural_width);
}



static void
panel_window_get_preferred_height_for_width (GtkWidget *widget,
                                             gint       width,
                                             gint      *minimum_height,
                                             gint      *natural_height)
{
  panel_window_get_preferred_height (widget, minimum_height, natural_height);
}



static void
panel_window_size_allocate (GtkWidget     *widget,
                            GtkAllocation *alloc)
{
  PanelWindow   *window = PANEL_WINDOW (widget);
  GtkAllocation  child_alloc;
  gint           w, h, x, y;
  PanelBorders   borders;
  GtkWidget     *child;

  gtk_widget_set_allocation (widget, alloc);
  window->alloc = *alloc;

  if (G_UNLIKELY (window->autohide_state == AUTOHIDE_HIDDEN
                  || window->autohide_state == AUTOHIDE_POPUP))
    {
      /* autohide timeout is already running, so let's wait with hiding the panel */
      if (window->autohide_timeout_id != 0)
        return;

      /* window is invisible */
      window->alloc.x = window->alloc.y = -9999;

      /* set hidden window size */
      w = h = window->autohide_size;

      switch (window->snap_position)
        {
        /* left or right of the screen */
        case SNAP_POSITION_E:
        case SNAP_POSITION_EC:
        case SNAP_POSITION_W:
        case SNAP_POSITION_WC:
          h = alloc->height;
          break;

        /* top or bottom of the screen */
        case SNAP_POSITION_NC:
        case SNAP_POSITION_SC:
        case SNAP_POSITION_N:
        case SNAP_POSITION_S:
          w = alloc->width;
          break;

        /* corner or floating panel */
        default:
          if (IS_HORIZONTAL (window))
            w = alloc->width;
          else
            h = alloc->height;
          break;
        }

      /* position the autohide window */
      panel_window_size_allocate_set_xy (window, w, h, &x, &y);
      panel_base_window_move_resize (PANEL_BASE_WINDOW (window->autohide_window),
                                     x, y, w, h);

      /* slide out the panel window with popdown_speed, but ignore panels that are floating, i.e. not
         attached to a GdkScreen border (i.e. including panels which are on a monitor border, but
         at are at the same time between two monitors) */
      if (IS_HORIZONTAL (window)
          && (((y + h) == panel_screen_get_height (window->screen))
               || (y == 0)))
        {
          window->popdown_progress = window->alloc.height;
          window->floating = FALSE;
        }
      else if (!IS_HORIZONTAL (window)
               && (((x + w) == panel_screen_get_width (window->screen))
                    || (x == 0)))
        {
          window->popdown_progress = window->alloc.width;
          window->floating = FALSE;
        }
      else
        window->floating = TRUE;

      /* make the panel invisible without animation */
      if (window->floating
          || window->popdown_speed == 0)
        {
          /* cancel any pending animations */
          if (window->autohide_ease_out_id != 0)
            g_source_remove (window->autohide_ease_out_id);

          gtk_window_move (GTK_WINDOW (window), window->alloc.x, window->alloc.y);
        }
    }
  else
    {
      /* stop a running autohide animation */
      if (window->autohide_ease_out_id != 0)
        g_source_remove (window->autohide_ease_out_id);

      /* update the allocation */
      panel_window_size_allocate_set_xy (window, alloc->width,
          alloc->height, &window->alloc.x, &window->alloc.y);

      /* update the struts if needed, leave when nothing changed */
      if (window->struts_edge != STRUTS_EDGE_NONE
          && window->autohide_behavior == AUTOHIDE_BEHAVIOR_NEVER)
        panel_window_screen_struts_set (window);

      /* move the autohide window offscreen */
      if (window->autohide_window != NULL)
        panel_base_window_move_resize (PANEL_BASE_WINDOW (window->autohide_window),
                                       -9999, -9999, -1, -1);

      gtk_window_move (GTK_WINDOW (window), window->alloc.x, window->alloc.y);
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (G_LIKELY (child != NULL))
    {
      child_alloc.x = 0;
      child_alloc.y = 0;
      child_alloc.width = alloc->width;
      child_alloc.height = alloc->height;

      /* set position against the borders */
      borders = panel_base_window_get_borders (PANEL_BASE_WINDOW (window));
      if (PANEL_HAS_FLAG (borders, PANEL_BORDER_LEFT))
        {
          child_alloc.x++;
          child_alloc.width--;
        }
      if (PANEL_HAS_FLAG (borders, PANEL_BORDER_TOP))
        {
          child_alloc.y++;
          child_alloc.height--;
        }
      if (PANEL_HAS_FLAG (borders, PANEL_BORDER_RIGHT))
        child_alloc.width--;
      if (PANEL_HAS_FLAG (borders, PANEL_BORDER_BOTTOM))
        child_alloc.height--;

      /* keep space for the panel handles if not locked */
      if (!window->position_locked)
        {
          if (IS_HORIZONTAL (window))
            {
              child_alloc.width -= 2 * HANDLE_SIZE_TOTAL;
              child_alloc.x += HANDLE_SIZE_TOTAL;
            }
          else
            {
              child_alloc.height -= 2 * HANDLE_SIZE_TOTAL;
              child_alloc.y += HANDLE_SIZE_TOTAL;
            }
        }

      /* allocate the itembar */
      gtk_widget_size_allocate (child, &child_alloc);
    }
}



static void
panel_window_size_allocate_set_xy (PanelWindow *window,
                                   gint         window_width,
                                   gint         window_height,
                                   gint        *return_x,
                                   gint        *return_y)
{
  gint value, hight;

  /* x-position */
  switch (window->snap_position)
    {
    case SNAP_POSITION_NONE:
    case SNAP_POSITION_N:
    case SNAP_POSITION_S:
      /* clamp base point on screen */
      value = window->base_x - (window_width / 2);
      hight = window->area.x + window->area.width - window_width;
      *return_x = CLAMP (value, window->area.x, hight);
      break;

    case SNAP_POSITION_W:
    case SNAP_POSITION_NW:
    case SNAP_POSITION_WC:
    case SNAP_POSITION_SW:
      /* left */
      *return_x = window->area.x;
      break;

    case SNAP_POSITION_E:
    case SNAP_POSITION_NE:
    case SNAP_POSITION_EC:
    case SNAP_POSITION_SE:
      /* right */
      *return_x = window->area.x + window->area.width - window_width;
      break;

    case SNAP_POSITION_NC:
    case SNAP_POSITION_SC:
      /* center */
      *return_x = window->area.x + (window->area.width - window_width) / 2;
      break;
    }

  /* y-position */
  switch (window->snap_position)
    {
    case SNAP_POSITION_NONE:
    case SNAP_POSITION_E:
    case SNAP_POSITION_W:
      /* clamp base point on screen */
      value = window->base_y - (window_height / 2);
      hight = window->area.y + window->area.height - window_height;
      *return_y = CLAMP (value, window->area.y, hight);
      break;

    case SNAP_POSITION_NE:
    case SNAP_POSITION_NW:
    case SNAP_POSITION_NC:
    case SNAP_POSITION_N:
      /* top */
      *return_y = window->area.y;
      break;

    case SNAP_POSITION_SE:
    case SNAP_POSITION_SW:
    case SNAP_POSITION_SC:
    case SNAP_POSITION_S:
      /* bottom */
      *return_y = window->area.y + window->area.height - window_height;
      break;

    case SNAP_POSITION_EC:
    case SNAP_POSITION_WC:
      /* center */
      *return_y = window->area.y + (window->area.height - window_height) / 2;
      break;
    }
}



static void
panel_window_screen_changed (GtkWidget *widget,
                             GdkScreen *previous_screen)
{
  PanelWindow *window = PANEL_WINDOW (widget);
  WnckWindow  *wnck_window;
  WnckScreen  *wnck_screen;
  GdkScreen   *screen;

  if (G_LIKELY (GTK_WIDGET_CLASS (panel_window_parent_class)->screen_changed != NULL))
    GTK_WIDGET_CLASS (panel_window_parent_class)->screen_changed (widget, previous_screen);

  /* get the new screen */
  screen = gtk_window_get_screen (GTK_WINDOW (widget));
  panel_return_if_fail (GDK_IS_SCREEN (screen));
  if (G_UNLIKELY (window->screen == screen))
    return;

  /* disconnect from previous screen */
  if (G_UNLIKELY (window->screen != NULL))
    g_signal_handlers_disconnect_by_func (G_OBJECT (window->screen),
        panel_window_screen_layout_changed, window);

  /* set the new screen */
  window->screen = screen;
  window->display = gdk_screen_get_display (screen);
  g_signal_connect_object (G_OBJECT (window->screen), "monitors-changed",
      G_CALLBACK (panel_window_screen_layout_changed), window, 0);
  g_signal_connect_object (G_OBJECT (window->screen), "size-changed",
      G_CALLBACK (panel_window_screen_layout_changed), window, 0);

  /* update the screen layout */
  panel_window_screen_layout_changed (screen, window);

  /* update wnck screen to be used for the autohide feature */
  wnck_screen = panel_wnck_screen_get_default ();
  wnck_window = wnck_screen_get_active_window (wnck_screen);
  panel_window_update_autohide_window (window, wnck_screen, wnck_window);
}



static void
panel_window_update_dark_mode (gboolean dark_mode)
{
  GtkSettings *gtk_settings;

  gtk_settings = gtk_settings_get_default ();

  if (!dark_mode)
    gtk_settings_reset_property (gtk_settings,
                                 "gtk-application-prefer-dark-theme");

  g_object_set (gtk_settings,
                "gtk-application-prefer-dark-theme",
                dark_mode,
                NULL);
}


static void
panel_window_style_updated (GtkWidget *widget)
{
  PanelWindow *window = PANEL_WINDOW (widget);
  PanelBaseWindow *base_window = PANEL_BASE_WINDOW (window);

  /* let GTK update the widget style */
  GTK_WIDGET_CLASS (panel_window_parent_class)->style_updated (widget);

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "popup-delay", &window->popup_delay,
                        "popdown-delay", &window->popdown_delay,
                        "autohide-size", &window->autohide_size,
                        NULL);
  /* Make sure the background and borders are redrawn on Gtk theme changes */
  if (base_window->background_style == PANEL_BG_STYLE_NONE)
    panel_base_window_reset_background_css (base_window);
}



static GdkFilterReturn
panel_window_filter (GdkXEvent *xev,
                     GdkEvent  *gev,
                     gpointer   data)
{
  PanelWindow    *window = data;
  GdkEventButton *event = (GdkEventButton *) gev;
  XEvent         *xevent = (XEvent *) xev;
  XButtonEvent   *xbutton_event = (XButtonEvent *) xev;
  GdkDevice      *device;
  gint            position, limit, scale_factor;

  panel_return_val_if_fail (PANEL_IS_WINDOW (window), GDK_FILTER_CONTINUE);

  /* we are only interested in button press on handles */
  if (window->position_locked || xevent->type != ButtonPress)
    return GDK_FILTER_CONTINUE;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));
  if (IS_HORIZONTAL (window))
    {
      position = xbutton_event->x;
      limit = window->alloc.width * scale_factor;
    }
  else
    {
      position = xbutton_event->y;
      limit = window->alloc.height * scale_factor;
    }

  /* leave when the pointer is not on the handles */
  if (position >= HANDLE_SIZE_TOTAL && position <= limit - HANDLE_SIZE_TOTAL)
    return GDK_FILTER_CONTINUE;

  /* leave when the pointer is grabbed, typically when the context menu is shown */
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (window->display));
  if (device == NULL || gdk_display_device_is_grabbed (window->display, device))
    return GDK_FILTER_CONTINUE;

  event->type = GDK_BUTTON_PRESS;
  event->send_event = TRUE;
  event->time = xbutton_event->time;
  event->x = xbutton_event->x;
  event->y = xbutton_event->y;
  event->axes = NULL;
  event->state = xbutton_event->state;
  event->button = xbutton_event->button;
  event->device = device;
  event->x_root = xbutton_event->x_root;
  event->y_root = xbutton_event->y_root;

  /* force the panel to process the event instead of its child widgets */
  return panel_window_button_press_event (GTK_WIDGET (window), event) ? GDK_FILTER_REMOVE
                                                                      : GDK_FILTER_CONTINUE;
}



static void
panel_window_realize (GtkWidget *widget)
{
  PanelWindow *window = PANEL_WINDOW (widget);
  GdkWindow   *gdkwindow;

  (*GTK_WIDGET_CLASS (panel_window_parent_class)->realize) (widget);

  /* clear opaque region so compositor properly applies transparency */
  gdkwindow = gtk_widget_get_window (widget);
  gdk_window_set_opaque_region (gdkwindow, NULL);

  /* set struts if we snap to an edge */
  if (window->struts_edge != STRUTS_EDGE_NONE)
    panel_window_screen_struts_set (window);

  /* redirect some corner cases (see issue #227) */
  gdk_window_add_filter (gdkwindow, panel_window_filter, window);
}



static StrutsEgde
panel_window_screen_struts_edge (PanelWindow *window)
{
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), STRUTS_EDGE_NONE);

  /* no struts when autohide is active or they are disabled by the user */
  if (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER
      || ! window->struts_enabled)
    return STRUTS_EDGE_NONE;

  /* return the screen edge on which the window is
   * visually snapped and where the struts are set */
  switch (window->snap_position)
    {
    case SNAP_POSITION_NONE:
      return STRUTS_EDGE_NONE;

    case SNAP_POSITION_E:
    case SNAP_POSITION_EC:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_NONE : STRUTS_EDGE_RIGHT;

    case SNAP_POSITION_NE:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_TOP : STRUTS_EDGE_RIGHT;

    case SNAP_POSITION_SE:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_BOTTOM : STRUTS_EDGE_RIGHT;

    case SNAP_POSITION_W:
    case SNAP_POSITION_WC:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_NONE : STRUTS_EDGE_LEFT;

    case SNAP_POSITION_NW:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_TOP : STRUTS_EDGE_LEFT;

    case SNAP_POSITION_SW:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_BOTTOM : STRUTS_EDGE_LEFT;

    case SNAP_POSITION_NC:
    case SNAP_POSITION_N:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_TOP : STRUTS_EDGE_NONE;

    case SNAP_POSITION_SC:
    case SNAP_POSITION_S:
      return IS_HORIZONTAL (window) ? STRUTS_EDGE_BOTTOM : STRUTS_EDGE_NONE;
    }

  return STRUTS_EDGE_NONE;
}



static void
panel_window_screen_struts_set (PanelWindow *window)
{
  gulong         struts[N_STRUTS] = { 0, };
  GdkRectangle  *alloc = &window->alloc;
  guint          i;
  gboolean       update_struts = FALSE;
  gint           n, scale_factor;
  const gchar   *strut_border[] = { "left", "right", "top", "bottom" };
  const gchar   *strut_xy[] = { "y", "y", "x", "x" };

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (cardinal_atom != 0 && net_wm_strut_partial_atom != 0);
  panel_return_if_fail (GDK_IS_SCREEN (window->screen));
  panel_return_if_fail (GDK_IS_DISPLAY (window->display));

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  /* set the struts */
  /* Note that struts are relative to the screen edge! (NOT the monitor)
     This means we have no choice but to use deprecated GtkScreen calls.
     The screen height/width can't be calculated from monitor geometry
     because it can extend beyond the lowest/rightmost monitor. */
  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));
  if (window->struts_edge == STRUTS_EDGE_TOP)
    {
      /* the window is snapped on the top screen edge */
      struts[STRUT_TOP] = (alloc->y + alloc->height) * scale_factor;
      struts[STRUT_TOP_START_X] = alloc->x * scale_factor;
      struts[STRUT_TOP_END_X] = (alloc->x + alloc->width - 1) * scale_factor;
    }
  else if (window->struts_edge == STRUTS_EDGE_BOTTOM)
    {
      /* the window is snapped on the bottom screen edge */
      struts[STRUT_BOTTOM] = (panel_screen_get_height (window->screen) - alloc->y) * scale_factor;
      struts[STRUT_BOTTOM_START_X] = alloc->x * scale_factor;
      struts[STRUT_BOTTOM_END_X] = (alloc->x + alloc->width - 1) * scale_factor;
    }
  else if (window->struts_edge == STRUTS_EDGE_LEFT)
    {
      /* the window is snapped on the left screen edge */
      struts[STRUT_LEFT] = (alloc->x + alloc->width) * scale_factor;
      struts[STRUT_LEFT_START_Y] = alloc->y * scale_factor;
      struts[STRUT_LEFT_END_Y] = (alloc->y + alloc->height - 1) * scale_factor;
    }
  else if (window->struts_edge == STRUTS_EDGE_RIGHT)
    {
      /* the window is snapped on the right screen edge */
      struts[STRUT_RIGHT] = (panel_screen_get_width (window->screen) - alloc->x) * scale_factor;
      struts[STRUT_RIGHT_START_Y] = alloc->y * scale_factor;
      struts[STRUT_RIGHT_END_Y] = (alloc->y + alloc->height - 1) * scale_factor;
    }

  /* store the new struts */
  for (i = 0; i < N_STRUTS; i++)
    {
      /* check if we need to update */
      if (G_LIKELY (struts[i] == window->struts[i]))
        continue;

      /* set new value */
      update_struts = TRUE;
      window->struts[i] = struts[i];
    }

  /* leave when there is nothing to update */
  if (!update_struts)
    return;

  /* don't crash on x errors */
  gdk_x11_display_error_trap_push (window->display);

  /* set the wm strut partial */
  panel_return_if_fail (GDK_IS_WINDOW (gtk_widget_get_window (GTK_WIDGET (window))));
  gdk_property_change (gtk_widget_get_window (GTK_WIDGET (window)),
                       net_wm_strut_partial_atom,
                       cardinal_atom, 32, GDK_PROP_MODE_REPLACE,
                       (guchar *) &struts, N_STRUTS);

  /* release the trap */
  if (gdk_x11_display_error_trap_pop (window->display) != 0)
    g_critical ("Failed to set the struts");

  if (panel_debug_has_domain (PANEL_DEBUG_YES))
    {
      n = -1;

      if (struts[STRUT_LEFT] != 0)
        n = STRUT_LEFT;
      else if (struts[STRUT_RIGHT] != 0)
        n = STRUT_RIGHT;
      else if (struts[STRUT_TOP] != 0)
        n = STRUT_TOP;
      else if (struts[STRUT_BOTTOM] != 0)
        n = STRUT_BOTTOM;
      else
        panel_debug (PANEL_DEBUG_STRUTS, "%p: unset", window);

      if (n != -1)
        {
          panel_debug (PANEL_DEBUG_STRUTS,
                       "%p: %s=%ld, start_%s=%ld, end_%s=%ld",
                       window, strut_border[n], struts[n],
                       strut_xy[n], struts[4 + n * 2],
                       strut_xy[n], struts[5 + n * 2]);
        }
    }
}



static void
panel_window_screen_update_borders (PanelWindow *window)
{
  PanelBorders borders = PANEL_BORDER_NONE;

  panel_return_if_fail (PANEL_IS_WINDOW (window));

  switch (window->snap_position)
    {
    case SNAP_POSITION_NONE:
      borders = PANEL_BORDER_TOP | PANEL_BORDER_BOTTOM
                | PANEL_BORDER_LEFT | PANEL_BORDER_RIGHT;
      break;

    case SNAP_POSITION_E:
    case SNAP_POSITION_EC:
      borders = PANEL_BORDER_TOP | PANEL_BORDER_BOTTOM
                | PANEL_BORDER_LEFT;
      break;

    case SNAP_POSITION_W:
    case SNAP_POSITION_WC:
      borders = PANEL_BORDER_TOP | PANEL_BORDER_BOTTOM
                | PANEL_BORDER_RIGHT;
      break;

    case SNAP_POSITION_N:
    case SNAP_POSITION_NC:
      borders = PANEL_BORDER_BOTTOM | PANEL_BORDER_LEFT
                | PANEL_BORDER_RIGHT;
      break;

    case SNAP_POSITION_S:
    case SNAP_POSITION_SC:
      borders = PANEL_BORDER_TOP | PANEL_BORDER_LEFT
                | PANEL_BORDER_RIGHT;
      break;

    case SNAP_POSITION_NE:

      borders = PANEL_BORDER_BOTTOM | PANEL_BORDER_LEFT;
      break;

    case SNAP_POSITION_SE:
      borders = PANEL_BORDER_LEFT | PANEL_BORDER_TOP;
      break;

    case SNAP_POSITION_NW:
      borders = PANEL_BORDER_RIGHT | PANEL_BORDER_BOTTOM;
      break;

    case SNAP_POSITION_SW:
      borders = PANEL_BORDER_RIGHT | PANEL_BORDER_TOP;
      break;
    }

  /* don't show the side borders if the length is 100% */
  if (window->length == 1.00)
    {
      if (IS_HORIZONTAL (window))
        PANEL_UNSET_FLAG (borders, PANEL_BORDER_LEFT | PANEL_BORDER_RIGHT);
      else
        PANEL_UNSET_FLAG (borders, PANEL_BORDER_TOP | PANEL_BORDER_BOTTOM);
    }

  /* set the borders */
  panel_base_window_set_borders (PANEL_BASE_WINDOW (window), borders);
}



static inline guint
panel_window_snap_edge_gravity (gint value,
                                gint start,
                                gint end)
{
  gint center;

  /* snap at the start */
  if (value >= start && value <= start + SNAP_DISTANCE)
    return EDGE_GRAVITY_START;

  /* snap at the end */
  if (value <= end && value >= end - SNAP_DISTANCE)
    return EDGE_GRAVITY_END;

  /* snap at the center */
  center = start + (end - start) / 2;
  if (value >= center - 10 && value <= center + SNAP_DISTANCE)
    return EDGE_GRAVITY_CENTER;

  return EDGE_GRAVITY_NONE;
}



static SnapPosition
panel_window_snap_position (PanelWindow *window)
{
  guint         snap_horz, snap_vert;
  GdkRectangle *alloc = &window->alloc;

  /* get the snap offsets */
  snap_horz = panel_window_snap_edge_gravity (alloc->x, window->area.x,
      window->area.x + window->area.width - alloc->width);
  snap_vert = panel_window_snap_edge_gravity (alloc->y, window->area.y,
      window->area.y + window->area.height - alloc->height);

  /* detect the snap mode */
  if (snap_horz == EDGE_GRAVITY_START)
    return SNAP_POSITION_W + snap_vert;
  else if (snap_horz == EDGE_GRAVITY_END)
    return SNAP_POSITION_E + snap_vert;
  else if (snap_horz == EDGE_GRAVITY_CENTER && snap_vert == EDGE_GRAVITY_START)
    return SNAP_POSITION_NC;
  else if (snap_horz == EDGE_GRAVITY_CENTER && snap_vert == EDGE_GRAVITY_END)
    return SNAP_POSITION_SC;
  else if (snap_horz == EDGE_GRAVITY_NONE && snap_vert == EDGE_GRAVITY_START)
    return SNAP_POSITION_N;
  else if (snap_horz == EDGE_GRAVITY_NONE && snap_vert == EDGE_GRAVITY_END)
    return SNAP_POSITION_S;

  return SNAP_POSITION_NONE;
}



static void
panel_window_display_layout_debug (GtkWidget *widget)
{
  GdkDisplay   *display;
  GdkScreen    *screen;
  GdkMonitor   *monitor;
  gint          m, n_monitors;
  gint          w, h;
  GdkRectangle  rect;
  GString      *str;
  const gchar  *name;
  gboolean      composite = FALSE;

  panel_return_if_fail (GTK_IS_WIDGET (widget));
  panel_return_if_fail (panel_debug_has_domain (PANEL_DEBUG_YES));

  str = g_string_new (NULL);

  display = gtk_widget_get_display (widget);
  screen = gtk_widget_get_screen(widget);

  w = panel_screen_get_width (screen);
  h = panel_screen_get_height (screen);

  g_string_append_printf (str, "screen-0[%p]=[%d,%d]", screen, w, h);

  if (panel_debug_has_domain (PANEL_DEBUG_DISPLAY_LAYOUT))
    {
      g_string_append_printf (str, "{comp=%s, sys=%p, rgba=%p}",
          PANEL_DEBUG_BOOL (gdk_screen_is_composited (screen)),
          gdk_screen_get_system_visual (screen),
          gdk_screen_get_rgba_visual (screen));
    }

  str = g_string_append (str, " (");

  n_monitors = gdk_display_get_n_monitors (display);
  for (m = 0; m < n_monitors; m++)
    {
      monitor = gdk_display_get_monitor (display, m);
      name = gdk_monitor_get_model (monitor);
      if (name == NULL)
        name = g_strdup_printf ("monitor-%d", m);

      gdk_monitor_get_geometry (monitor, &rect);
      g_string_append_printf (str, "%s=[%d,%d;%d,%d]", name,
          rect.x, rect.y, rect.width, rect.height);

      if (m < n_monitors - 1)
        g_string_append (str, ", ");
    }

  g_string_append (str, ")");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  composite = gdk_display_supports_composite (display);
G_GNUC_END_IGNORE_DEPRECATIONS

  panel_debug (PANEL_DEBUG_DISPLAY_LAYOUT,
               "%p: display=%s{comp=%s}, %s", widget,
               gdk_display_get_name (display),
               PANEL_DEBUG_BOOL (composite),
               str->str);

  g_string_free (str, TRUE);
}



static void
panel_window_screen_layout_changed (GdkScreen   *screen,
                                    PanelWindow *window)
{
  GdkRectangle  a = { 0 }, b;
  gint          n_monitors, n;
  const gchar  *name;
  GdkMonitor   *monitor;
  StrutsEgde    struts_edge;
  gboolean      force_struts_update = FALSE;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (GDK_IS_SCREEN (screen));
  panel_return_if_fail (window->screen == screen);

  /* leave when the screen position if not set */
  if (window->base_x == -1 && window->base_y == -1)
    return;

  /* print the display layout when debugging is enabled */
  if (G_UNLIKELY (panel_debug_has_domain (PANEL_DEBUG_YES)))
    panel_window_display_layout_debug (GTK_WIDGET (window));

  /* update the struts edge of this window and check if we need to force
   * a struts update (ie. remove struts that are currently set) */
  struts_edge = panel_window_screen_struts_edge (window);
  if (window->struts_edge != struts_edge && struts_edge == STRUTS_EDGE_NONE)
    force_struts_update = TRUE;
  window->struts_edge = struts_edge;

  /* get the number of monitors */
  n_monitors = gdk_display_get_n_monitors (window->display);
  panel_return_if_fail (n_monitors > 0);

  panel_debug (PANEL_DEBUG_POSITIONING,
               "%p: screen=%p, monitors=%d, output-name=%s, span-monitors=%s, base=%d,%d",
               window, screen,
               n_monitors, window->output_name,
               PANEL_DEBUG_BOOL (window->span_monitors),
               window->base_x, window->base_y);

  if ((window->output_name == NULL || g_strcmp0 (window->output_name, "Automatic") == 0)
      && (window->span_monitors || n_monitors == 1))
    {
      /* get the screen geometry we also use this if there is only
       * one monitor and no output is choosen, as a fast-path */
      monitor = gdk_display_get_monitor(window->display, 0);
      gdk_monitor_get_geometry (monitor, &a);

      a.width += a.x;
      a.height += a.y;

      for (n = 1; n < n_monitors; n++)
        {
          gdk_monitor_get_geometry (gdk_display_get_monitor(window->display, n), &b);

          a.x = MIN (a.x, b.x);
          a.y = MIN (a.y, b.y);
          a.width = MAX (a.width, b.x + b.width);
          a.height = MAX (a.height, b.y + b.height);
        }

      a.width -= a.x;
      a.height -= a.y;

      panel_return_if_fail (a.width > 0 && a.height > 0);
    }
  else
    {
      if (g_strcmp0 (window->output_name, "Automatic") == 0
          || window->output_name == NULL)
        {
          /* get the monitor geometry based on the panel position */
          monitor = gdk_display_get_monitor_at_point (window->display, window->base_x,
                                                      window->base_y);
          gdk_monitor_get_geometry (monitor, &a);
          panel_return_if_fail (a.width > 0 && a.height > 0);
        }
      else if (g_strcmp0 (window->output_name, "Primary") == 0)
        {
          /* get the primary monitor */
          monitor = gdk_display_get_primary_monitor (window->display);
          if (monitor == NULL)
            monitor = gdk_display_get_monitor (window->display, 0);

          gdk_monitor_get_geometry (monitor, &a);
          panel_return_if_fail (a.width > 0 && a.height > 0);
        }
      else
        {
          /* check if we've stored the monitor number in the config or
           * should lookup the number from the randr output name */
          if (strncmp (window->output_name, "monitor-", 8) == 0
              && sscanf (window->output_name, "monitor-%d", &n) == 1)
            {
              /* check if extracted monitor number is out of range */
              monitor = gdk_display_get_monitor(window->display, n);
              if (monitor != NULL)
                {
                  gdk_monitor_get_geometry (monitor, &a);
                  panel_return_if_fail (a.width > 0 && a.height > 0);
                }
            }
          else
            {
              /* detect the monitor number by output name */
              for (n = 0; n < n_monitors; n++)
                {
                  monitor = gdk_display_get_monitor(window->display, n);
                  name = gdk_monitor_get_model (monitor);

                  /* check if this is the monitor we're looking for */
                  if (g_strcmp0 (window->output_name, name) == 0)
                    {
                      gdk_monitor_get_geometry (monitor, &a);
                      panel_return_if_fail (a.width > 0 && a.height > 0);
                      break;
                    }
                }
            }

          if (G_UNLIKELY (a.height == 0 && a.width == 0))
            {
              panel_debug (PANEL_DEBUG_POSITIONING,
                           "%p: monitor %s not found, hiding window",
                           window, window->output_name);

              /* hide the panel if the monitor was not found */
              if (gtk_widget_get_visible (GTK_WIDGET (window)))
                gtk_widget_hide (GTK_WIDGET (window));
              return;
            }
        }
    }

  /* set the new working area of the panel */
  window->area = a;
  panel_debug (PANEL_DEBUG_POSITIONING,
               "%p: working-area: screen=%p, x=%d, y=%d, w=%d, h=%d",
               window, screen,
               a.x, a.y, a.width, a.height);

  /* update max length in pixels with notification */
  g_object_set (window, "length-max", IS_HORIZONTAL (window) ? a.width : a.height, NULL);

  panel_window_screen_update_borders (window);

  gtk_widget_queue_resize (GTK_WIDGET (window));

  /* update the struts if needed (ie. we need to reset the struts) */
  if (force_struts_update)
    panel_window_screen_struts_set (window);

  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_widget_show (GTK_WIDGET (window));
}



static void
panel_window_active_window_changed (WnckScreen  *screen,
                                    WnckWindow  *previous_window,
                                    PanelWindow *window)
{
  WnckWindow *active_window;

  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* obtain new active window from the screen */
  active_window = wnck_screen_get_active_window (screen);

  /* update the active window to be used for the autohide feature */
  panel_window_update_autohide_window (window, screen, active_window);
}



static void
panel_window_active_window_geometry_changed (WnckWindow  *active_window,
                                             PanelWindow *window)
{
  GdkRectangle panel_area;
  GdkRectangle window_area;
  gboolean     geometry_fixed = FALSE;
  gint         scale_factor;

  panel_return_if_fail (WNCK_IS_WINDOW (active_window));
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* ignore if for some reason the active window does not match the one we know */
  if (G_UNLIKELY (window->wnck_active_window != active_window))
    return;

  /* only react to active window geometry changes if we are doing
   * intelligent autohiding */
  if (window->autohide_behavior == AUTOHIDE_BEHAVIOR_INTELLIGENTLY
      && window->autohide_block == 0)
    {
      if (wnck_window_get_window_type (active_window) != WNCK_WINDOW_DESKTOP)
        {
          GdkWindow *gdkwindow;
          GtkBorder extents;

          /* obtain position and dimensions from the active window */
          wnck_window_get_geometry (active_window,
                                    &window_area.x, &window_area.y,
                                    &window_area.width, &window_area.height);

          /* if a window uses client-side decorations, check the _GTK_FRAME_EXTENTS
           * application window property to get its actual size without the shadows */
          gdkwindow = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (),
                                                              wnck_window_get_xid (active_window));
          if (gdkwindow != NULL)
            {
              if (xfce_has_gtk_frame_extents (gdkwindow, &extents))
                {
                  window_area.x += extents.left;
                  window_area.y += extents.top;
                  window_area.width -= extents.left + extents.right;
                  window_area.height -= extents.top + extents.bottom;
                  geometry_fixed = TRUE;
                }

              g_object_unref (gdkwindow);
            }

          /* if a window is shaded, check the height of the window's
           * decoration as exposed through the _NET_FRAME_EXTENTS application
           * window property */
          if (! geometry_fixed && wnck_window_is_shaded (active_window))
          {
            Display *display;
            Atom real_type;
            int real_format;
            unsigned long items_read, items_left;
            guint32 *data;

            display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
            if (XGetWindowProperty (display, wnck_window_get_xid (active_window),
                                    XInternAtom(display, "_NET_FRAME_EXTENTS", True),
                                    0, 4, FALSE, AnyPropertyType,
                                    &real_type, &real_format, &items_read, &items_left,
                                    (unsigned char **) &data) == Success
                                    && (items_read >= 4))
              window_area.height = data[2] + data[3];

            if (data)
            {
              XFree (data);
            }
          }

          /* apply scale factor */
          scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));
          window_area.x /= scale_factor;
          window_area.y /= scale_factor;
          window_area.width /= scale_factor;
          window_area.height /= scale_factor;

          /* obtain position and dimension from the panel */
          panel_window_size_allocate_set_xy (window,
                                             window->alloc.width,
                                             window->alloc.height,
                                             &panel_area.x,
                                             &panel_area.y);
          gtk_window_get_size (GTK_WINDOW (window),
                               &panel_area.width,
                               &panel_area.height);

          /* show/hide the panel, depending on whether the active window overlaps
           * with its coordinates */
          if (window->autohide_state != AUTOHIDE_HIDDEN)
            {
              if (gdk_rectangle_intersect (&panel_area,  &window_area, NULL))
                {
                  GdkDevice *device;
                  gint       pointer_x, pointer_y;

                  device = gdk_seat_get_pointer (gdk_display_get_default_seat (window->display));
                  if (device == NULL)
                    return;

                  /* check if the cursor is outside the panel area before proceeding */
                  gdk_device_get_position (device, NULL, &pointer_x, &pointer_y);
                  if (pointer_x <= panel_area.x
                      || pointer_y <= panel_area.y
                      || pointer_x >= panel_area.x + panel_area.width
                      || pointer_y >= panel_area.y + panel_area.height)
                    panel_window_autohide_queue (window, AUTOHIDE_POPDOWN);
                }
            }
          else
            {
              if (!gdk_rectangle_intersect (&panel_area, &window_area, NULL))
                panel_window_autohide_queue (window, AUTOHIDE_VISIBLE);
            }
        }
      else
        {
          /* make the panel visible if it isn't at the moment and the active
           * window is the desktop */
          if (window->autohide_state != AUTOHIDE_VISIBLE)
            panel_window_autohide_queue (window, AUTOHIDE_VISIBLE);
        }
    }
}



static void
panel_window_active_window_state_changed (WnckWindow  *active_window,
                                          WnckWindowState changed,
                                          WnckWindowState new,
                                          PanelWindow *window)
{
  panel_return_if_fail (WNCK_IS_WINDOW (active_window));

  if (changed & WNCK_WINDOW_STATE_SHADED)
    panel_window_active_window_geometry_changed (active_window, window);
}



static gboolean
panel_window_autohide_timeout (gpointer user_data)
{
  PanelWindow *window = PANEL_WINDOW (user_data);

  panel_return_val_if_fail (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER, FALSE);
  panel_return_val_if_fail (window->autohide_block == 0, FALSE);

  /* update the status */
  if (window->autohide_state == AUTOHIDE_POPDOWN
      || window->autohide_state == AUTOHIDE_POPDOWN_SLOW)
    window->autohide_state = AUTOHIDE_HIDDEN;
  else if (window->autohide_state == AUTOHIDE_POPUP)
    window->autohide_state = AUTOHIDE_VISIBLE;

  /* move the windows around */
  gtk_widget_queue_resize (GTK_WIDGET (window));

  /* check whether the panel should be animated on autohide */
  if (window->floating == FALSE
      || window->popdown_speed > 0)
    window->autohide_ease_out_id =
            g_timeout_add_full (G_PRIORITY_LOW, 25,
                                panel_window_autohide_ease_out, window,
                                panel_window_autohide_ease_out_timeout_destroy);

  return FALSE;
}



static void
panel_window_autohide_timeout_destroy (gpointer user_data)
{
  PANEL_WINDOW (user_data)->autohide_timeout_id = 0;
}



/* Cubic ease out function based on Robert Penner's Easing Functions,
   which are licensed under MIT and BSD license
   http://robertpenner.com/easing/ */
static guint
panel_window_cubic_ease_out (guint p)
{
  guint f = (p - 1);
  return f * f * f + 1;
}



static gboolean
panel_window_autohide_ease_out (gpointer data)
{
  PanelWindow  *window = PANEL_WINDOW (data);
  gint          x, y, w, h, progress;
  gboolean      ret = TRUE;

  if (window->autohide_ease_out_id == 0)
    return FALSE;

  gtk_window_get_position (GTK_WINDOW (window), &x, &y);
  w = panel_screen_get_width (window->screen);
  h = panel_screen_get_height (window->screen);

  if (IS_HORIZONTAL (window))
    {
      progress = panel_window_cubic_ease_out (window->alloc.height - window->popdown_progress) / window->popdown_speed;
      if (window->snap_position == SNAP_POSITION_N || window->snap_position == SNAP_POSITION_NC
          || window->snap_position == SNAP_POSITION_NW || window->snap_position == SNAP_POSITION_NE)
        {
          y -= progress;

          if (y < 0 - window->alloc.height)
            ret = FALSE;
        }
      else if (window->snap_position == SNAP_POSITION_S || window->snap_position == SNAP_POSITION_SC
               || window->snap_position == SNAP_POSITION_SW || window->snap_position == SNAP_POSITION_SE)
        {
          y += progress;

          if (y > h + window->alloc.height)
            ret = FALSE;
        }
    }
  else
    {
      progress = panel_window_cubic_ease_out (window->alloc.width - window->popdown_progress) / window->popdown_speed;
      if (window->snap_position == SNAP_POSITION_W || window->snap_position == SNAP_POSITION_WC
          || window->snap_position == SNAP_POSITION_NW || window->snap_position == SNAP_POSITION_SW)
        {
          x -= progress;

          if (x < 0 - window->alloc.width)
            ret = FALSE;
        }
      else if (window->snap_position == SNAP_POSITION_E || window->snap_position == SNAP_POSITION_EC
               || window->snap_position == SNAP_POSITION_NE || window->snap_position == SNAP_POSITION_SE)
        {
          x += progress;

          if (x > (w + window->alloc.width))
            ret = FALSE;
        }
    }

  window->popdown_progress--;
  gtk_window_move (GTK_WINDOW (window), x, y);

  return ret;
}



static void
panel_window_autohide_ease_out_timeout_destroy (gpointer user_data)
{
  PANEL_WINDOW (user_data)->autohide_ease_out_id = 0;
}



static void
panel_window_autohide_queue (PanelWindow   *window,
                             AutohideState  new_state)
{
  guint delay;

  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* stop pending timeout */
  if (window->autohide_timeout_id != 0)
    g_source_remove (window->autohide_timeout_id);

  if (window->autohide_ease_out_id != 0)
    g_source_remove (window->autohide_ease_out_id);

  /* set new autohide state */
  window->autohide_state = new_state;

  /* already hidden */
  if (new_state == AUTOHIDE_HIDDEN)
    return;

  /* force a layout update to disable struts */
  if (window->struts_edge != STRUTS_EDGE_NONE
      || window->snap_position != SNAP_POSITION_NONE)
    panel_window_screen_layout_changed (window->screen, window);

  if (new_state == AUTOHIDE_VISIBLE)
    {
      /* queue a resize to make sure the panel is visible */
      gtk_widget_queue_resize (GTK_WIDGET (window));
    }
  else
    {
      /* timeout delay */
      if (new_state == AUTOHIDE_POPDOWN)
        delay = window->popdown_delay;
      else if (new_state == AUTOHIDE_POPDOWN_SLOW)
        delay = window->popdown_delay * 4;
      else
        delay = window->popup_delay;

      /* start a new timeout to hide the panel */
      window->autohide_timeout_id =
        g_timeout_add_full (G_PRIORITY_LOW, delay,
                            panel_window_autohide_timeout, window,
                            panel_window_autohide_timeout_destroy);
    }
}



static gboolean
panel_window_opacity_enter_timeout (gpointer data)
{
  PanelWindow *window = data;

  panel_base_window_opacity_enter (PANEL_BASE_WINDOW (window), FALSE);
  window->opacity_timeout_id = 0;

  return FALSE;
}



static void
panel_window_opacity_enter_queue (PanelWindow *window,
                                  gboolean     enter)
{
  if (window->opacity_timeout_id != 0)
    {
      g_source_remove (window->opacity_timeout_id);
      window->opacity_timeout_id = 0;
    }

  /* avoid flickering by using the same timeout as for autohide when leaving */
  if (enter)
    panel_base_window_opacity_enter (PANEL_BASE_WINDOW (window), TRUE);
  else
    window->opacity_timeout_id =
      g_timeout_add (DEFAULT_POPDOWN_DELAY, panel_window_opacity_enter_timeout, window);
}



static gboolean
panel_window_autohide_drag_motion (GtkWidget        *widget,
                                   GdkDragContext   *context,
                                   gint              x,
                                   gint              y,
                                   guint             drag_time,
                                   PanelWindow      *window)
{
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), TRUE);
  panel_return_val_if_fail (window->autohide_window == widget, TRUE);

  /* queue a popup is state is hidden */
  if (window->autohide_state == AUTOHIDE_HIDDEN)
    panel_window_autohide_queue (window, AUTOHIDE_POPUP);

  return TRUE;
}



static void
panel_window_autohide_drag_leave (GtkWidget      *widget,
                                  GdkDragContext *drag_context,
                                  guint           drag_time,
                                  PanelWindow    *window)
{
  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (window->autohide_window == widget);

  /* we left the autohide window before the panel was shown, stop the queue */
  panel_window_autohide_queue (window, AUTOHIDE_HIDDEN);
}



static gboolean
panel_window_autohide_event (GtkWidget        *widget,
                             GdkEventCrossing *event,
                             PanelWindow      *window)
{
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);
  panel_return_val_if_fail (window->autohide_window == widget, FALSE);

  if (event->type == GDK_ENTER_NOTIFY)
    panel_window_autohide_queue (window, AUTOHIDE_POPUP);
  else
    panel_window_autohide_drag_leave (widget, NULL, 0, window);

  return FALSE;
}



static void
panel_window_set_autohide_behavior (PanelWindow *window,
                                    AutohideBehavior behavior)
{
  GtkWidget   *popup;
  guint        i;
  const gchar *properties[] = { "enter-opacity", "leave-opacity",
                                "borders", "background-style",
                                "background-rgba",
                                "role", "screen" };

  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* do nothing if the behavior hasn't changed at all */
  if (window->autohide_behavior == behavior)
    return;

  /* remember the new behavior */
  window->autohide_behavior = behavior;

  /* create an autohide window only if we are autohiding at all */
  if (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER)
    {
      /* create an authohide window; doing this only when it doesn't exist
       * yet allows us to transition between "always autohide" and "intelligently
       * autohide" without recreating the window */
      if (window->autohide_window == NULL)
        {
          /* create the window */
          panel_return_if_fail (window->autohide_window == NULL);
          popup = g_object_new (PANEL_TYPE_BASE_WINDOW,
                                "type", GTK_WINDOW_TOPLEVEL,
                                "decorated", FALSE,
                                "resizable", TRUE,
                                "type-hint", GDK_WINDOW_TYPE_HINT_DOCK,
                                "gravity", GDK_GRAVITY_STATIC,
                                "name", "XfcePanelWindowHidden",
                                NULL);

          /* move the window offscreen */
          panel_base_window_move_resize (PANEL_BASE_WINDOW (popup),
                                         -9999, -9999, 3, 3);

          /* bind some properties to sync the two windows */
          for (i = 0; i < G_N_ELEMENTS (properties); i++)
            {
              g_object_bind_property (G_OBJECT (window), properties[i],
                                      G_OBJECT (popup), properties[i],
                                      G_BINDING_DEFAULT);
            }

          /* respond to drag motion */
          gtk_drag_dest_set_track_motion (GTK_WIDGET (window), TRUE);

          /* signals for pointer enter/leave events */
          g_signal_connect (G_OBJECT (popup), "enter-notify-event",
              G_CALLBACK (panel_window_autohide_event), window);
          g_signal_connect (G_OBJECT (popup), "leave-notify-event",
              G_CALLBACK (panel_window_autohide_event), window);

          /* show/hide the panel on drag events */
          gtk_drag_dest_set (popup, 0, NULL, 0, 0);
          gtk_drag_dest_set_track_motion (popup, TRUE);
          g_signal_connect (G_OBJECT (popup), "drag-motion",
              G_CALLBACK (panel_window_autohide_drag_motion), window);
          g_signal_connect (G_OBJECT (popup), "drag-leave",
              G_CALLBACK (panel_window_autohide_drag_leave), window);

          /* show the window */
          window->autohide_window = popup;
          gtk_widget_show (popup);
        }

      if (window->autohide_behavior == AUTOHIDE_BEHAVIOR_ALWAYS)
        {
          /* start autohide by hiding the panel straight away */
          if (window->autohide_state != AUTOHIDE_HIDDEN)
          {
            panel_window_autohide_queue (window,
                window->autohide_block == 0 ? AUTOHIDE_POPDOWN_SLOW : AUTOHIDE_VISIBLE);
          }
        }
      else if (window->autohide_behavior == AUTOHIDE_BEHAVIOR_INTELLIGENTLY)
        {
          /* start intelligent autohide by making the panel visible initially */
          panel_window_autohide_queue (window, AUTOHIDE_VISIBLE);
        }
    }
  else if (window->autohide_window != NULL)
    {
      /* stop autohide */
      panel_window_autohide_queue (window, AUTOHIDE_VISIBLE);

      /* destroy the autohide window */
      panel_return_if_fail (GTK_IS_WINDOW (window->autohide_window));
      gtk_widget_destroy (window->autohide_window);
      window->autohide_window = NULL;
    }
}



static void
panel_window_update_autohide_window (PanelWindow *window,
                                     WnckScreen  *screen,
                                     WnckWindow  *active_window)
{
  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (screen == NULL || WNCK_IS_SCREEN (screen));
  panel_return_if_fail (active_window == NULL || WNCK_IS_WINDOW (active_window));

  /* update current screen */
  if (screen != window->wnck_screen)
    {
      /* disconnect from previous screen */
      if (G_LIKELY (window->wnck_screen != NULL))
        {
          g_signal_handlers_disconnect_by_func (window->wnck_screen,
              panel_window_active_window_changed, window);
        }

      /* remember new screen */
      window->wnck_screen = screen;

      /* connect to the new screen */
      if (screen != NULL)
        {
          g_signal_connect (G_OBJECT (screen), "active-window-changed",
              G_CALLBACK (panel_window_active_window_changed), window);
        }
    }

  /* update active window */
  if (G_LIKELY (active_window != window->wnck_active_window))
    {
      /* disconnect from previously active window */
      if (G_LIKELY (window->wnck_active_window != NULL))
        {
          g_signal_handlers_disconnect_by_func (window->wnck_active_window,
              panel_window_active_window_geometry_changed, window);
          g_signal_handlers_disconnect_by_func (window->wnck_active_window,
              panel_window_active_window_state_changed, window);
        }

      /* remember the new window */
      window->wnck_active_window = active_window;

      /* connect to the new window but only if it is not a desktop/root-type window */
      if (active_window != NULL)
        {
          g_signal_connect (G_OBJECT (active_window), "geometry-changed",
              G_CALLBACK (panel_window_active_window_geometry_changed), window);
          g_signal_connect (G_OBJECT (active_window), "state-changed",
              G_CALLBACK (panel_window_active_window_state_changed), window);

          /* simulate a geometry change for immediate hiding when the new active
           * window already overlaps the panel */
          panel_window_active_window_geometry_changed (active_window, window);
        }
    }
}



static void
panel_window_menu_toggle_locked (GtkCheckMenuItem *item,
                                 PanelWindow      *window)
{
  panel_return_if_fail (GTK_IS_CHECK_MENU_ITEM (item));
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  g_object_set (G_OBJECT (window), "position-locked",
      gtk_check_menu_item_get_active (item), NULL);
}



static void
panel_window_menu_help (void)
{
  panel_utils_show_help (NULL, NULL, NULL);
}



static void
panel_window_menu_deactivate (GtkMenu     *menu,
                              PanelWindow *window)
{
  panel_return_if_fail (GTK_IS_MENU (menu));
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* thaw autohide block */
  panel_window_thaw_autohide (window);

  g_object_unref (G_OBJECT (menu));
}



static void
panel_window_menu_popup (PanelWindow    *window,
                         GdkEventButton *event,
                         gboolean        show_tic_tac_toe)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *image;

  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* freeze autohide */
  panel_window_freeze_autohide (window);

  /* create menu */
  menu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (menu),
      gtk_window_get_screen (GTK_WINDOW (window)));
  g_object_ref_sink (G_OBJECT (menu));
  g_signal_connect (G_OBJECT (menu), "deactivate",
      G_CALLBACK (panel_window_menu_deactivate), window);

  item = gtk_menu_item_new_with_label (_("Panel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, FALSE);
  gtk_widget_show (item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  if (!panel_window_get_locked (window))
    {
      /* add new items */
      item = panel_image_menu_item_new_with_mnemonic (_("Add _New Items..."));
      g_signal_connect_swapped (G_OBJECT (item), "activate",
          G_CALLBACK (panel_item_dialog_show), window);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (item, image);
      gtk_widget_show (image);

      /* customize panel */
      item = panel_image_menu_item_new_with_mnemonic (_("Panel Pr_eferences..."));
      g_signal_connect_swapped (G_OBJECT (item), "activate",
          G_CALLBACK (panel_preferences_dialog_show), window);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      image = gtk_image_new_from_icon_name ("preferences-system", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (item, image);
      gtk_widget_show (image);

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      item = gtk_check_menu_item_new_with_mnemonic (_("_Lock Panel"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
          window->position_locked);
      g_signal_connect (G_OBJECT (item), "toggled",
          G_CALLBACK (panel_window_menu_toggle_locked), window);
      gtk_widget_show (item);

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  /* logout item */
  item = panel_image_menu_item_new_with_mnemonic (_("Log _Out"));
  g_signal_connect_swapped (G_OBJECT (item), "activate",
      G_CALLBACK (panel_application_logout), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  image = gtk_image_new_from_icon_name ("system-log-out", GTK_ICON_SIZE_MENU);
  panel_image_menu_item_set_image (item, image);
  gtk_widget_show (image);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* help item */
  image = gtk_image_new_from_icon_name ("help-browser", GTK_ICON_SIZE_MENU);
  item = panel_image_menu_item_new_with_mnemonic (_("_Help"));
  panel_image_menu_item_set_image (item, image);
  g_signal_connect (G_OBJECT (item), "activate",
      G_CALLBACK (panel_window_menu_help), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* about item */
  image = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_MENU);
  item = panel_image_menu_item_new_with_mnemonic (_("_About"));
  panel_image_menu_item_set_image (item, image);
  g_signal_connect (G_OBJECT (item), "activate",
      G_CALLBACK (panel_dialogs_show_about), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* tic tac toe item */
  if (show_tic_tac_toe)
    {
      item = gtk_menu_item_new_with_label ("Tic Tac Toe");
      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (panel_tic_tac_toe_show), NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
}



static void
panel_window_plugins_update (PanelWindow *window,
                             PluginProp   prop)
{
  GtkWidget   *itembar;
  GtkCallback  func;

  panel_return_if_fail (PANEL_IS_WINDOW (window));

  switch (prop)
    {
    case PLUGIN_PROP_MODE:
      func = panel_window_plugin_set_mode;
      break;

    case PLUGIN_PROP_SCREEN_POSITION:
      func = panel_window_plugin_set_screen_position;
      break;

    case PLUGIN_PROP_NROWS:
      func = panel_window_plugin_set_nrows;
      break;

    case PLUGIN_PROP_SIZE:
      func = panel_window_plugin_set_size;
      break;

    case PLUGIN_PROP_ICON_SIZE:
      func = panel_window_plugin_set_icon_size;
      break;

    case PLUGIN_PROP_DARK_MODE:
      func = panel_window_plugin_set_dark_mode;
      break;

    default:
      panel_assert_not_reached ();
      return;
    }

  itembar = gtk_bin_get_child (GTK_BIN (window));
  if (G_LIKELY (itembar != NULL))
    {
      panel_return_if_fail (GTK_IS_CONTAINER (itembar));
      gtk_container_foreach (GTK_CONTAINER (itembar), func, window);
    }
}



static void
panel_window_plugin_set_mode (GtkWidget *widget,
                              gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_WINDOW (user_data));

  xfce_panel_plugin_provider_set_mode (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                       PANEL_WINDOW (user_data)->mode);
}



static void
panel_window_plugin_set_size (GtkWidget *widget,
                              gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_WINDOW (user_data));

  xfce_panel_plugin_provider_set_size (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                       PANEL_WINDOW (user_data)->size);
}



static void
panel_window_plugin_set_icon_size (GtkWidget *widget,
                                   gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_WINDOW (user_data));

  xfce_panel_plugin_provider_set_icon_size (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                            PANEL_WINDOW (user_data)->icon_size);
}



static void
panel_window_plugin_set_dark_mode (GtkWidget *widget,
                                   gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_WINDOW (user_data));

  xfce_panel_plugin_provider_set_dark_mode (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                            PANEL_WINDOW (user_data)->dark_mode);
}



static void
panel_window_plugin_set_nrows (GtkWidget *widget,
                               gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_WINDOW (user_data));

  xfce_panel_plugin_provider_set_nrows (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                        PANEL_WINDOW (user_data)->nrows);
}



static void
panel_window_plugin_set_screen_position (GtkWidget *widget,
                                         gpointer   user_data)
{
  PanelWindow        *window = PANEL_WINDOW (user_data);
  XfceScreenPosition  position;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_WINDOW (user_data));

  switch (window->snap_position)
    {
    case SNAP_POSITION_NONE:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_FLOATING_H :
          XFCE_SCREEN_POSITION_FLOATING_V;
      break;

    case SNAP_POSITION_NW:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_NW_H :
          XFCE_SCREEN_POSITION_NW_V;
      break;

    case SNAP_POSITION_NE:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_NE_H :
          XFCE_SCREEN_POSITION_NE_V;
      break;

    case SNAP_POSITION_SW:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_SW_H :
          XFCE_SCREEN_POSITION_SW_V;
      break;

    case SNAP_POSITION_SE:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_SE_H :
          XFCE_SCREEN_POSITION_SE_V;
      break;

    case SNAP_POSITION_W:
    case SNAP_POSITION_WC:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_FLOATING_H :
          XFCE_SCREEN_POSITION_W;
      break;

    case SNAP_POSITION_E:
    case SNAP_POSITION_EC:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_FLOATING_H :
          XFCE_SCREEN_POSITION_E;
      break;

    case SNAP_POSITION_S:
    case SNAP_POSITION_SC:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_S :
          XFCE_SCREEN_POSITION_FLOATING_V;
      break;

    case SNAP_POSITION_N:
    case SNAP_POSITION_NC:
      position = IS_HORIZONTAL (window) ? XFCE_SCREEN_POSITION_N :
          XFCE_SCREEN_POSITION_FLOATING_V;
      break;

    default:
      panel_assert_not_reached ();
      break;
    }

  xfce_panel_plugin_provider_set_screen_position (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                                  position);
}



GtkWidget *
panel_window_new (GdkScreen *screen,
                  gint       id,
                  gint       autohide_block)
{
  PanelWindow *window;

  if (screen == NULL)
    screen = gdk_screen_get_default ();

  window = g_object_new (PANEL_TYPE_WINDOW,
                         "id", id,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "decorated", FALSE,
                         "resizable", FALSE,
                         "screen", screen,
                         "type-hint", GDK_WINDOW_TYPE_HINT_DOCK,
                         "gravity", GDK_GRAVITY_STATIC,
                         "role", "Panel",
                         "name", "XfcePanelWindow",
                         NULL);
  window->autohide_block = autohide_block;

  return GTK_WIDGET (window);
}



gint
panel_window_get_id (PanelWindow *window)
{
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), -1);
  panel_return_val_if_fail (window->id > -1, -1);
  return window->id;
}



gboolean
panel_window_has_position (PanelWindow *window)
{
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);
  return window->base_x != -1 && window->base_y != -1;
}



void
panel_window_set_provider_info (PanelWindow *window,
                                GtkWidget   *provider,
                                gboolean     moving_to_other_panel)
{
  PanelBaseWindow *base_window = PANEL_BASE_WINDOW (window);

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  xfce_panel_plugin_provider_set_locked (XFCE_PANEL_PLUGIN_PROVIDER (provider),
                                         panel_window_get_locked (window));

  if (PANEL_IS_PLUGIN_EXTERNAL (provider))
    {
      if (base_window->background_style == PANEL_BG_STYLE_COLOR)
        {
          panel_plugin_external_set_background_color (PANEL_PLUGIN_EXTERNAL (provider),
              base_window->background_rgba);
        }
      else if (base_window->background_style == PANEL_BG_STYLE_IMAGE)
        {
          panel_plugin_external_set_background_image (PANEL_PLUGIN_EXTERNAL (provider),
              base_window->background_image);
        }
      else if (moving_to_other_panel)
        {
          /* unset the background (PROVIDER_PROP_TYPE_ACTION_BACKGROUND_UNSET) */
          panel_plugin_external_set_background_color (PANEL_PLUGIN_EXTERNAL (provider), NULL);
        }

      if (base_window->is_composited)
        panel_plugin_external_set_opacity (PANEL_PLUGIN_EXTERNAL (provider),
                                           gtk_widget_get_opacity (GTK_WIDGET (window)));
    }

  panel_window_plugin_set_mode (provider, window);
  panel_window_plugin_set_screen_position (provider, window);
  panel_window_plugin_set_size (provider, window);
  panel_window_plugin_set_icon_size (provider, window);
  panel_window_plugin_set_dark_mode (provider, window);
  panel_window_plugin_set_nrows (provider, window);
}



void
panel_window_freeze_autohide (PanelWindow *window)
{
  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (window->autohide_block >= 0);

  /* increase autohide block counter */
  window->autohide_block++;

  if (window->autohide_block > 1)
    return;

  panel_window_opacity_enter_queue (window, TRUE);
  if (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER)
    panel_window_autohide_queue (window, AUTOHIDE_VISIBLE);
}



void
panel_window_thaw_autohide (PanelWindow *window)
{
  GdkDevice *device;
  GdkWindow *gdkwindow;
  gboolean   outside = FALSE;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (window->autohide_block > 0);

  /* decrease autohide block counter */
  window->autohide_block--;

  if (window->autohide_block > 0)
    return;

  /* check if pointer is outside the panel */
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (window->display));
  if (device != NULL)
    {
      gdkwindow = gdk_device_get_window_at_position (device, NULL, NULL);
      outside = (gdkwindow == NULL || gdk_window_get_effective_toplevel (gdkwindow)
                                      != gtk_widget_get_window (GTK_WIDGET (window)));
    }

  if (outside)
    panel_window_opacity_enter_queue (window, FALSE);

  if (window->autohide_behavior != AUTOHIDE_BEHAVIOR_NEVER)
    {
      /* simulate a geometry change to check for overlapping windows with intelligent hiding */
      if (window->autohide_behavior == AUTOHIDE_BEHAVIOR_INTELLIGENTLY)
        panel_window_active_window_geometry_changed (window->wnck_active_window, window);
      /* otherwise hide the panel if the pointer is outside */
      else if (outside)
        panel_window_autohide_queue (window, AUTOHIDE_POPDOWN);
    }
}



void
panel_window_set_locked (PanelWindow *window,
                         gboolean     locked)
{
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  window->locked = locked;
}



gboolean
panel_window_get_locked (PanelWindow *window)
{
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), TRUE);

  return window->locked;
}



void
panel_window_focus (PanelWindow *window)
{
#ifdef GDK_WINDOWING_X11
  XClientMessageEvent event;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (window)));

  /* we need a slightly custom version of the call through Gtk+ to
   * properly focus the panel when a plugin calls
   * xfce_panel_plugin_focus_widget() */
  event.type = ClientMessage;
  event.window = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (window)));
  event.message_type = gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW");
  event.format = 32;
  event.data.l[0] = 0;

  gdk_x11_display_error_trap_push (window->display);

  XSendEvent (gdk_x11_get_default_xdisplay (), GDK_ROOT_WINDOW (), False,
              StructureNotifyMask, (XEvent *) &event);

  gdk_display_flush (window->display);

  if (gdk_x11_display_error_trap_pop (window->display) != 0)
    g_critical ("Failed to focus panel window");
#else
  /* our best guess on non-x11 clients */
  gtk_window_present (GTK_WINDOW (window));
#endif
}



void
panel_window_migrate_old_properties (PanelWindow *window,
                                     XfconfChannel *xfconf,
                                     const gchar *property_base,
                                     const PanelProperty *old_properties,
                                     const PanelProperty *new_properties)
{
  gboolean old_bool, new_bool, migrated;
  guint new_uint;
  gchar *old_property, *new_property;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (XFCONF_IS_CHANNEL (xfconf));
  panel_return_if_fail (property_base != NULL && *property_base != '\0');
  panel_return_if_fail (old_properties != NULL);
  panel_return_if_fail (new_properties != NULL);

  for (gint i = 0; old_properties[i].property != NULL && new_properties[i].property != NULL; i++)
    {
      /* no old property: nothing to do */
      old_property = g_strdup_printf ("%s/%s", property_base, old_properties[i].property);
      if (! xfconf_channel_has_property (xfconf, old_property))
        {
          g_free (old_property);
          continue;
        }

      /* new property already set: simply remove old property */
      new_property = g_strdup_printf ("%s/%s", property_base, new_properties[i].property);
      if (xfconf_channel_has_property (xfconf, new_property))
        {
          xfconf_channel_reset_property (xfconf, old_property, FALSE);
          g_free (old_property);
          g_free (new_property);
          continue;
        }

      /* try to get old value depending on type */
      switch (old_properties[i].type)
        {
        case G_TYPE_BOOLEAN:
          old_bool = xfconf_channel_get_bool (xfconf, old_property, FALSE);
          break;

        default:
          g_warning ("Unsupported property type '%s' for property '%s'",
                     g_type_name (old_properties[i].type), old_properties[i].property);
          g_free (old_property);
          g_free (new_property);
          continue;
        }

      /* try to migrate from old property to new property */
      migrated = FALSE;
      switch (new_properties[i].type)
        {
        case G_TYPE_BOOLEAN:
          if (g_strcmp0 (new_properties[i].property, "enable-struts") == 0)
            new_bool = ! old_bool;
          else
            {
              g_warning ("Unrecognized new property '%s'", new_properties[i].property);
              g_free (old_property);
              g_free (new_property);
              continue;
            }

          migrated = xfconf_channel_set_bool (xfconf, new_property, new_bool);
          break;

        case G_TYPE_UINT:
          if (g_strcmp0 (new_properties[i].property, "autohide-behavior") == 0)
            new_uint = old_bool ? AUTOHIDE_BEHAVIOR_ALWAYS : AUTOHIDE_BEHAVIOR_NEVER;
          else
            {
              g_warning ("Unrecognized new property '%s'", new_properties[i].property);
              g_free (old_property);
              g_free (new_property);
              continue;
            }

          migrated = xfconf_channel_set_uint (xfconf, new_property, new_uint);
          break;

        default:
          g_warning ("Unsupported property type '%s' for property '%s'",
                     g_type_name (new_properties[i].type), new_properties[i].property);
          break;
        }

      /* remove old property */
      if (migrated)
        xfconf_channel_reset_property (xfconf, old_property, FALSE);

      g_free (old_property);
      g_free (new_property);
    }
}
