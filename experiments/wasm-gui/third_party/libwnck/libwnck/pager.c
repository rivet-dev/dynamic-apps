/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* vim: set sw=2 et: */
/* pager object */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2003, 2005-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <math.h>
#include <glib/gi18n-lib.h>

#include "pager.h"
#include "workspace.h"
#include "window.h"
#include "xutils.h"
#include "pager-accessible-factory.h"
#include "workspace-accessible-factory.h"
#include "private.h"

/**
 * SECTION:pager
 * @short_description: a pager widget, showing the content of workspaces.
 * @see_also: #WnckScreen
 * @stability: Unstable
 *
 * A #WnckPager shows a miniature view of the workspaces, representing managed
 * windows by small rectangles, and allows the user to initiate various window
 * manager actions by manipulating these representations. The #WnckPager offers
 * ways to move windows between workspaces and to change the current workspace.
 *
 * Alternatively, a #WnckPager can be configured to only show the names of the
 * workspace instead of their contents.
 *
 * The #WnckPager is also responsible for setting the layout of the workspaces.
 * Since only one application can be responsible for setting the layout on a
 * screen, the #WnckPager automatically tries to obtain the manager selection
 * for the screen and only sets the layout if it owns the manager selection.
 * See wnck_pager_set_orientation() and wnck_pager_set_n_rows() to change the
 * layout.
 */

#define N_SCREEN_CONNECTIONS 11

struct _WnckPagerPrivate
{
  WnckScreen *screen;

  int n_rows; /* really columns for vertical orientation */
  WnckPagerDisplayMode display_mode;
  gboolean show_all_workspaces;
  GtkShadowType shadow_type;

  GtkOrientation orientation;
  int workspace_size;
  guint screen_connections[N_SCREEN_CONNECTIONS];
  int prelight; /* workspace mouse is hovering over */
  gboolean prelight_dnd; /* is dnd happening? */

  guint dragging :1;
  int drag_start_x;
  int drag_start_y;
  WnckWindow *drag_window;

  GdkPixbuf *bg_cache;

  int layout_manager_token;

  guint dnd_activate; /* GSource that triggers switching to this workspace during dnd */
  guint dnd_time; /* time of last event during dnd (for delayed workspace activation) */
};

G_DEFINE_TYPE (WnckPager, wnck_pager, GTK_TYPE_WIDGET);
#define WNCK_PAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_PAGER, WnckPagerPrivate))

enum
{
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};


#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

static void wnck_pager_init        (WnckPager      *pager);
static void wnck_pager_class_init  (WnckPagerClass *klass);
static void wnck_pager_finalize    (GObject        *object);

static void     wnck_pager_realize       (GtkWidget        *widget);
static void     wnck_pager_unrealize     (GtkWidget        *widget);
static GtkSizeRequestMode wnck_pager_get_request_mode (GtkWidget *widget);
static void     wnck_pager_get_preferred_width (GtkWidget *widget,
                                                int       *minimum_width,
                                                int       *natural_width);
static void     wnck_pager_get_preferred_width_for_height (GtkWidget *widget,
                                                           int        height,
                                                           int       *minimum_width,
                                                           int       *natural_width);
static void     wnck_pager_get_preferred_height (GtkWidget *widget,
                                                 int       *minimum_height,
                                                 int       *natural_height);
static void     wnck_pager_get_preferred_height_for_width (GtkWidget *widget,
                                                           int        width,
                                                           int       *minimum_height,
                                                           int       *natural_height);
static void     wnck_pager_size_allocate (GtkWidget        *widget,
                                          GtkAllocation    *allocation);
static gboolean wnck_pager_draw          (GtkWidget        *widget,
                                          cairo_t          *cr);
static gboolean wnck_pager_button_press  (GtkWidget        *widget,
                                          GdkEventButton   *event);
static gboolean wnck_pager_drag_motion   (GtkWidget        *widget,
                                          GdkDragContext   *context,
                                          gint              x,
                                          gint              y,
                                          guint             time);
static void wnck_pager_drag_motion_leave (GtkWidget        *widget,
                                          GdkDragContext   *context,
                                          guint             time);
static gboolean wnck_pager_drag_drop	 (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  guint             time);
static void wnck_pager_drag_data_received (GtkWidget          *widget,
			  	           GdkDragContext     *context,
				           gint                x,
				           gint                y,
				           GtkSelectionData   *selection_data,
				           guint               info,
				           guint               time_);
static void wnck_pager_drag_data_get      (GtkWidget        *widget,
		                           GdkDragContext   *context,
		                           GtkSelectionData *selection_data,
		                           guint             info,
		                           guint             time);
static void wnck_pager_drag_end		  (GtkWidget        *widget,
					   GdkDragContext   *context);
static gboolean wnck_pager_motion        (GtkWidget        *widget,
                                          GdkEventMotion   *event);
static gboolean wnck_pager_leave_notify	 (GtkWidget          *widget,
					  GdkEventCrossing   *event);
static gboolean wnck_pager_button_release (GtkWidget        *widget,
                                           GdkEventButton   *event);
static gboolean wnck_pager_scroll_event  (GtkWidget        *widget,
                                          GdkEventScroll   *event);
static gboolean wnck_pager_query_tooltip (GtkWidget  *widget,
                                          gint        x,
                                          gint        y,
                                          gboolean    keyboard_tip,
                                          GtkTooltip *tooltip);
static void workspace_name_changed_callback (WnckWorkspace *workspace,
                                             gpointer       data);

static gboolean wnck_pager_window_state_is_relevant (int state);
static gint wnck_pager_window_get_workspace   (WnckWindow  *window,
                                               gboolean     is_state_relevant);
static void wnck_pager_queue_draw_workspace   (WnckPager   *pager,
					       gint	    i);
static void wnck_pager_queue_draw_window (WnckPager	   *pager,
					  WnckWindow	   *window);

static void wnck_pager_connect_screen    (WnckPager  *pager);
static void wnck_pager_connect_window    (WnckPager  *pager,
                                          WnckWindow *window);
static void wnck_pager_disconnect_screen (WnckPager  *pager);
static void wnck_pager_disconnect_window (WnckPager  *pager,
                                          WnckWindow *window);

static gboolean wnck_pager_set_layout_hint (WnckPager  *pager);

static void wnck_pager_clear_drag (WnckPager *pager);
static void wnck_pager_check_prelight (WnckPager *pager,
				       gint	  x,
				       gint	  y,
				       gboolean	  dnd);

static GdkPixbuf* wnck_pager_get_background (WnckPager *pager,
                                             int        width,
                                             int        height);

static AtkObject* wnck_pager_get_accessible (GtkWidget *widget);


static void
wnck_pager_init (WnckPager *pager)
{
  int i;
  static const GtkTargetEntry targets[] = {
    { "application/x-wnck-window-id", 0, 0}
  };

  pager->priv = WNCK_PAGER_GET_PRIVATE (pager);

  pager->priv->n_rows = 1;
  pager->priv->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
  pager->priv->show_all_workspaces = TRUE;
  pager->priv->shadow_type = GTK_SHADOW_NONE;

  pager->priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  pager->priv->workspace_size = 48;

  for (i = 0; i < N_SCREEN_CONNECTIONS; i++)
    pager->priv->screen_connections[i] = 0;

  pager->priv->prelight = -1;

  pager->priv->layout_manager_token = WNCK_NO_MANAGER_TOKEN;

  g_object_set (pager, "has-tooltip", TRUE, NULL);

  gtk_drag_dest_set (GTK_WIDGET (pager), 0, targets, G_N_ELEMENTS (targets), GDK_ACTION_MOVE);
  gtk_widget_set_can_focus (GTK_WIDGET (pager), TRUE);
}

static void
wnck_pager_class_init (WnckPagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WnckPagerPrivate));

  object_class->finalize = wnck_pager_finalize;

  widget_class->realize = wnck_pager_realize;
  widget_class->unrealize = wnck_pager_unrealize;
  widget_class->get_request_mode = wnck_pager_get_request_mode;
  widget_class->get_preferred_width = wnck_pager_get_preferred_width;
  widget_class->get_preferred_width_for_height = wnck_pager_get_preferred_width_for_height;
  widget_class->get_preferred_height = wnck_pager_get_preferred_height;
  widget_class->get_preferred_height_for_width = wnck_pager_get_preferred_height_for_width;
  widget_class->size_allocate = wnck_pager_size_allocate;
  widget_class->draw = wnck_pager_draw;
  widget_class->button_press_event = wnck_pager_button_press;
  widget_class->button_release_event = wnck_pager_button_release;
  widget_class->scroll_event = wnck_pager_scroll_event;
  widget_class->motion_notify_event = wnck_pager_motion;
  widget_class->leave_notify_event = wnck_pager_leave_notify;
  widget_class->get_accessible = wnck_pager_get_accessible;
  widget_class->drag_leave = wnck_pager_drag_motion_leave;
  widget_class->drag_motion = wnck_pager_drag_motion;
  widget_class->drag_drop = wnck_pager_drag_drop;
  widget_class->drag_data_received = wnck_pager_drag_data_received;
  widget_class->drag_data_get = wnck_pager_drag_data_get;
  widget_class->drag_end = wnck_pager_drag_end;
  widget_class->query_tooltip = wnck_pager_query_tooltip;
}

static void
wnck_pager_finalize (GObject *object)
{
  WnckPager *pager;

  pager = WNCK_PAGER (object);

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  if (pager->priv->dnd_activate != 0)
    {
      g_source_remove (pager->priv->dnd_activate);
      pager->priv->dnd_activate = 0;
    }

  G_OBJECT_CLASS (wnck_pager_parent_class)->finalize (object);
}

static void
_wnck_pager_set_screen (WnckPager *pager)
{
  GdkScreen *gdkscreen;

  if (!gtk_widget_has_screen (GTK_WIDGET (pager)))
    return;

  gdkscreen = gtk_widget_get_screen (GTK_WIDGET (pager));
  pager->priv->screen = wnck_screen_get (gdk_screen_get_number (gdkscreen));

  if (!wnck_pager_set_layout_hint (pager))
    {
      _WnckLayoutOrientation orientation;

      /* we couldn't set the layout on the screen. This means someone else owns
       * it. Let's at least show the correct layout. */
      _wnck_screen_get_workspace_layout (pager->priv->screen,
                                         &orientation,
                                         &pager->priv->n_rows,
                                         NULL, NULL);

      /* test in this order to default to horizontal in case there was in issue
       * when fetching the layout */
      if (orientation == WNCK_LAYOUT_ORIENTATION_VERTICAL)
        pager->priv->orientation = GTK_ORIENTATION_VERTICAL;
      else
        pager->priv->orientation = GTK_ORIENTATION_HORIZONTAL;

      gtk_widget_queue_resize (GTK_WIDGET (pager));
    }

  wnck_pager_connect_screen (pager);
}

static void
wnck_pager_realize (GtkWidget *widget)
{

  GdkWindowAttr attributes;
  gint attributes_mask;
  WnckPager *pager;
  GtkAllocation allocation;
  GdkWindow *window;

  pager = WNCK_PAGER (widget);

  /* do not call the parent class realize since we're doing things a bit
   * differently here */
  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK |
	  		  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			  GDK_SCROLL_MASK |
			  GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK |
			  GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, widget);

  gtk_style_context_set_background (gtk_widget_get_style_context (widget), window);

  /* connect to the screen of this pager. In theory, this will already have
   * been done in wnck_pager_size_request() */
  if (pager->priv->screen == NULL)
    _wnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);
}

static void
wnck_pager_unrealize (GtkWidget *widget)
{
  WnckPager *pager;

  pager = WNCK_PAGER (widget);

  wnck_pager_clear_drag (pager);
  pager->priv->prelight = -1;
  pager->priv->prelight_dnd = FALSE;

  wnck_screen_release_workspace_layout (pager->priv->screen,
                                        pager->priv->layout_manager_token);
  pager->priv->layout_manager_token = WNCK_NO_MANAGER_TOKEN;

  wnck_pager_disconnect_screen (pager);
  pager->priv->screen = NULL;

  GTK_WIDGET_CLASS (wnck_pager_parent_class)->unrealize (widget);
}

static void
_wnck_pager_get_padding (WnckPager *pager,
                         GtkBorder *padding)
{
  if (pager->priv->shadow_type != GTK_SHADOW_NONE)
    {
      GtkWidget       *widget;
      GtkStyleContext *context;
      GtkStateFlags    state;

      widget = GTK_WIDGET (pager);
      state = gtk_widget_get_state_flags (widget);
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get_padding (context, state, padding);
    }
  else
    {
      GtkBorder empty_padding = { 0, 0, 0, 0 };
      *padding = empty_padding;
    }
}

static int
_wnck_pager_get_workspace_width_for_height (WnckPager *pager,
                                            int        workspace_height)
{
  int workspace_width = 0;

  if (pager->priv->display_mode == WNCK_PAGER_DISPLAY_CONTENT)
    {
      WnckWorkspace *space;
      double screen_aspect;

      space = wnck_screen_get_workspace (pager->priv->screen, 0);

      if (space) {
        screen_aspect =
              (double) wnck_workspace_get_width (space) /
              (double) wnck_workspace_get_height (space);
      } else {
        screen_aspect =
              (double) wnck_screen_get_width (pager->priv->screen) /
              (double) wnck_screen_get_height (pager->priv->screen);
      }

      workspace_width = screen_aspect * workspace_height;
    }
  else
    {
      PangoLayout *layout;
      WnckScreen *screen;
      int n_spaces;
      int i, w;

      layout = gtk_widget_create_pango_layout  (GTK_WIDGET (pager), NULL);
      screen = pager->priv->screen;
      n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
      workspace_width = 1;

      for (i = 0; i < n_spaces; i++)
	{
	  pango_layout_set_text (layout,
				 wnck_workspace_get_name (wnck_screen_get_workspace (screen, i)),
				 -1);
	  pango_layout_get_pixel_size (layout, &w, NULL);
	  workspace_width = MAX (workspace_width, w);
	}

      g_object_unref (layout);

      workspace_width += 2;
    }

  return workspace_width;
}

static int
_wnck_pager_get_workspace_height_for_width (WnckPager *pager,
                                            int        workspace_width)
{
  int workspace_height = 0;
  WnckWorkspace *space;
  double screen_aspect;

  /* TODO: Handle WNCK_PAGER_DISPLAY_NAME for this case */

  space = wnck_screen_get_workspace (pager->priv->screen, 0);

  if (space) {
    screen_aspect =
	  (double) wnck_workspace_get_height (space) /
	  (double) wnck_workspace_get_width (space);
  } else {
    screen_aspect =
	  (double) wnck_screen_get_height (pager->priv->screen) /
	  (double) wnck_screen_get_width (pager->priv->screen);
  }

  workspace_height = screen_aspect * workspace_width;

  return workspace_height;
}

static void
wnck_pager_size_request  (GtkWidget      *widget,
                          GtkRequisition *requisition)
{
  WnckPager *pager;
  int n_spaces;
  int spaces_per_row;
  int workspace_width, workspace_height;
  int n_rows;
  GtkBorder padding;
  int focus_width;

  pager = WNCK_PAGER (widget);

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _wnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);

  g_assert (pager->priv->n_rows > 0);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  if (pager->priv->show_all_workspaces)
    {
      n_rows = pager->priv->n_rows;
      spaces_per_row = (n_spaces + n_rows - 1) / n_rows;
    }
  else
    {
      n_rows = 1;
      spaces_per_row = 1;
    }

  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      workspace_width = pager->priv->workspace_size;
      workspace_height = _wnck_pager_get_workspace_height_for_width (pager,
                                                                     workspace_width);

      requisition->width = workspace_width * n_rows + (n_rows - 1);
      requisition->height = workspace_height * spaces_per_row  + (spaces_per_row - 1);
    }
  else
    {
      workspace_height = pager->priv->workspace_size;
      workspace_width = _wnck_pager_get_workspace_width_for_height (pager,
                                                                    workspace_height);

      requisition->width = workspace_width * spaces_per_row + (spaces_per_row - 1);
      requisition->height = workspace_height * n_rows + (n_rows - 1);
    }

  _wnck_pager_get_padding (pager, &padding);
  requisition->width += padding.left + padding.right;
  requisition->height += padding.top + padding.bottom;

  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			NULL);
  requisition->width  += 2 * focus_width;
  requisition->height += 2 * focus_width;
}

static GtkSizeRequestMode
wnck_pager_get_request_mode (GtkWidget *widget)
{
  WnckPager *pager;

  pager = WNCK_PAGER (widget);

  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
  else
    return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
wnck_pager_get_preferred_width (GtkWidget *widget,
                                int       *minimum_width,
                                int       *natural_width)
{
  GtkRequisition req;

  wnck_pager_size_request (widget, &req);

  *minimum_width = *natural_width = req.width;
}

static void
wnck_pager_get_preferred_width_for_height (GtkWidget *widget,
                                           int        height,
                                           int       *minimum_width,
                                           int       *natural_width)
{
  WnckPager *pager;
  int n_spaces;
  int n_rows;
  int spaces_per_row;
  int workspace_width, workspace_height;
  int focus_width;
  GtkBorder padding;
  int width = 0;

  pager = WNCK_PAGER (widget);

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _wnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);

  g_assert (pager->priv->n_rows > 0);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  if (pager->priv->show_all_workspaces)
    {
      n_rows = pager->priv->n_rows;
      spaces_per_row = (n_spaces + n_rows - 1) / n_rows;
    }
  else
    {
      n_rows = 1;
      spaces_per_row = 1;
    }

  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			NULL);
  height -= 2 * focus_width;
  width += 2 * focus_width;

  _wnck_pager_get_padding (pager, &padding);
  height -= padding.top + padding.bottom;
  width += padding.left + padding.right;

  height -= (n_rows - 1);
  workspace_height = height / n_rows;

  workspace_width = _wnck_pager_get_workspace_width_for_height (pager,
                                                                workspace_height);

  width += workspace_width * spaces_per_row + (spaces_per_row - 1);
  *natural_width = *minimum_width = width;
}

static void
wnck_pager_get_preferred_height (GtkWidget *widget,
                                 int       *minimum_height,
                                 int       *natural_height)
{
  GtkRequisition req;

  wnck_pager_size_request (widget, &req);

  *minimum_height = *natural_height = req.height;
}

static void
wnck_pager_get_preferred_height_for_width (GtkWidget *widget,
                                           int        width,
                                           int       *minimum_height,
                                           int       *natural_height)
{
  WnckPager *pager;
  int n_spaces;
  int n_rows;
  int spaces_per_row;
  int workspace_width, workspace_height;
  int focus_width;
  GtkBorder padding;
  int height = 0;

  pager = WNCK_PAGER (widget);

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _wnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);

  g_assert (pager->priv->n_rows > 0);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  if (pager->priv->show_all_workspaces)
    {
      n_rows = pager->priv->n_rows;
      spaces_per_row = (n_spaces + n_rows - 1) / n_rows;
    }
  else
    {
      n_rows = 1;
      spaces_per_row = 1;
    }

  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			NULL);
  width -= 2 * focus_width;
  height += 2 * focus_width;

  _wnck_pager_get_padding (pager, &padding);
  width -= padding.left + padding.right;
  height += padding.top + padding.bottom;

  width -= (n_rows - 1);
  workspace_width = width / n_rows;

  workspace_height = _wnck_pager_get_workspace_height_for_width (pager,
                                                                 workspace_width);

  height += workspace_height * spaces_per_row + (spaces_per_row - 1);
  *natural_height = *minimum_height = height;
}

static gboolean
_wnck_pager_queue_resize (gpointer data)
{
  gtk_widget_queue_resize (GTK_WIDGET (data));
  return FALSE;
}

static void
wnck_pager_size_allocate (GtkWidget      *widget,
                          GtkAllocation  *allocation)
{
  WnckPager *pager;
  int workspace_size;
  int focus_width;
  GtkBorder padding;
  int width;
  int height;

  pager = WNCK_PAGER (widget);

  gtk_widget_style_get (GTK_WIDGET (pager),
			"focus-line-width", &focus_width,
			NULL);
  width  = allocation->width  - 2 * focus_width;
  height = allocation->height - 2 * focus_width;

  _wnck_pager_get_padding (pager, &padding);
  width  -= padding.left + padding.right;
  height -= padding.top + padding.bottom;

  g_assert (pager->priv->n_rows > 0);

  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (pager->priv->show_all_workspaces)
	workspace_size = (width - (pager->priv->n_rows - 1))  / pager->priv->n_rows;
      else
	workspace_size = width;
    }
  else
    {
      if (pager->priv->show_all_workspaces)
	workspace_size = (height - (pager->priv->n_rows - 1))/ pager->priv->n_rows;
      else
	workspace_size = height;
    }

  workspace_size = MAX (workspace_size, 1);

  if (workspace_size != pager->priv->workspace_size)
    {
      pager->priv->workspace_size = workspace_size;
      g_idle_add (_wnck_pager_queue_resize, pager);
      return;
    }

  GTK_WIDGET_CLASS (wnck_pager_parent_class)->size_allocate (widget,
                                                             allocation);
}

static void
get_workspace_rect (WnckPager    *pager,
                    int           space,
                    GdkRectangle *rect)
{
  int hsize, vsize;
  int n_spaces;
  int spaces_per_row;
  GtkWidget *widget;
  int col, row;
  GtkAllocation allocation;
  GtkBorder padding;
  int focus_width;

  widget = GTK_WIDGET (pager);

  gtk_widget_get_allocation (widget, &allocation);

  _wnck_pager_get_padding (pager, &padding);
  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			NULL);

  if (!pager->priv->show_all_workspaces)
    {
      WnckWorkspace *active_space;

      active_space = wnck_screen_get_active_workspace (pager->priv->screen);

      if (active_space && space == wnck_workspace_get_number (active_space))
	{
	  rect->x = focus_width + padding.left;
	  rect->y = focus_width + padding.top;
	  rect->width = allocation.width - 2 * focus_width - padding.left - padding.right;
	  rect->height = allocation.height - 2 * focus_width - padding.top - padding.bottom;
	}
      else
	{
	  rect->x = 0;
	  rect->y = 0;
	  rect->width = 0;
	  rect->height = 0;
	}

      return;
    }

  hsize = allocation.width - 2 * focus_width;
  vsize = allocation.height - 2 * focus_width;

  if (pager->priv->shadow_type != GTK_SHADOW_NONE)
    {
      hsize -= padding.left + padding.right;
      vsize -= padding.top + padding.bottom;
    }

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  g_assert (pager->priv->n_rows > 0);
  spaces_per_row = (n_spaces + pager->priv->n_rows - 1) / pager->priv->n_rows;

  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      rect->width = (hsize - (pager->priv->n_rows - 1)) / pager->priv->n_rows;
      rect->height = (vsize - (spaces_per_row - 1)) / spaces_per_row;

      col = space / spaces_per_row;
      row = space % spaces_per_row;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        col = pager->priv->n_rows - col - 1;

      rect->x = (rect->width + 1) * col;
      rect->y = (rect->height + 1) * row;

      if (col == pager->priv->n_rows - 1)
	rect->width = hsize - rect->x;

      if (row  == spaces_per_row - 1)
	rect->height = vsize - rect->y;
    }
  else
    {
      rect->width = (hsize - (spaces_per_row - 1)) / spaces_per_row;
      rect->height = (vsize - (pager->priv->n_rows - 1)) / pager->priv->n_rows;

      col = space % spaces_per_row;
      row = space / spaces_per_row;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        col = spaces_per_row - col - 1;

      rect->x = (rect->width + 1) * col;
      rect->y = (rect->height + 1) * row;

      if (col == spaces_per_row - 1)
	rect->width = hsize - rect->x;

      if (row  == pager->priv->n_rows - 1)
	rect->height = vsize - rect->y;
    }

  rect->x += focus_width;
  rect->y += focus_width;

  if (pager->priv->shadow_type != GTK_SHADOW_NONE)
    {
      rect->x += padding.left;
      rect->y += padding.top;
    }
}

static gboolean
wnck_pager_window_state_is_relevant (int state)
{
  return (state & (WNCK_WINDOW_STATE_HIDDEN | WNCK_WINDOW_STATE_SKIP_PAGER)) ? FALSE : TRUE;
}

static gint
wnck_pager_window_get_workspace (WnckWindow *window,
                                 gboolean    is_state_relevant)
{
  gint state;
  WnckWorkspace *workspace;

  state = wnck_window_get_state (window);
  if (is_state_relevant && !wnck_pager_window_state_is_relevant (state))
    return -1;
  workspace = wnck_window_get_workspace (window);
  if (workspace == NULL && wnck_window_is_pinned (window))
    workspace = wnck_screen_get_active_workspace (wnck_window_get_screen (window));

  return workspace ? wnck_workspace_get_number (workspace) : -1;
}

static GList*
get_windows_for_workspace_in_bottom_to_top (WnckScreen    *screen,
                                            WnckWorkspace *workspace)
{
  GList *result;
  GList *windows;
  GList *tmp;
  int workspace_num;

  result = NULL;
  workspace_num = wnck_workspace_get_number (workspace);

  windows = wnck_screen_get_windows_stacked (screen);
  for (tmp = windows; tmp != NULL; tmp = tmp->next)
    {
      WnckWindow *win = WNCK_WINDOW (tmp->data);
      if (wnck_pager_window_get_workspace (win, TRUE) == workspace_num)
	result = g_list_prepend (result, win);
    }

  result = g_list_reverse (result);

  return result;
}

static void
get_window_rect (WnckWindow         *window,
                 const GdkRectangle *workspace_rect,
                 GdkRectangle       *rect)
{
  double width_ratio, height_ratio;
  int x, y, width, height;
  WnckWorkspace *workspace;
  GdkRectangle unclipped_win_rect;

  workspace = wnck_window_get_workspace (window);
  if (workspace == NULL)
    workspace = wnck_screen_get_active_workspace (wnck_window_get_screen (window));

  /* scale window down by same ratio we scaled workspace down */
  width_ratio = (double) workspace_rect->width / (double) wnck_workspace_get_width (workspace);
  height_ratio = (double) workspace_rect->height / (double) wnck_workspace_get_height (workspace);

  wnck_window_get_geometry (window, &x, &y, &width, &height);

  x += wnck_workspace_get_viewport_x (workspace);
  y += wnck_workspace_get_viewport_y (workspace);
  x = x * width_ratio + 0.5;
  y = y * height_ratio + 0.5;
  width = width * width_ratio + 0.5;
  height = height * height_ratio + 0.5;

  x += workspace_rect->x;
  y += workspace_rect->y;

  if (width < 3)
    width = 3;
  if (height < 3)
    height = 3;

  unclipped_win_rect.x = x;
  unclipped_win_rect.y = y;
  unclipped_win_rect.width = width;
  unclipped_win_rect.height = height;

  gdk_rectangle_intersect ((GdkRectangle *) workspace_rect, &unclipped_win_rect, rect);
}

static void
draw_window (cairo_t            *cr,
             GtkWidget          *widget,
             WnckWindow         *win,
             const GdkRectangle *winrect,
             GtkStateFlags       state,
             gboolean            translucent)
{
  GtkStyleContext *context;
  GdkPixbuf *icon;
  int icon_x, icon_y, icon_w, icon_h;
  gboolean is_active;
  GdkRGBA fg;
  gdouble translucency;

  context = gtk_widget_get_style_context (widget);

  is_active = wnck_window_is_active (win);
  translucency = translucent ? 0.4 : 1.0;

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  cairo_push_group (cr);

  gtk_render_background (context, cr, winrect->x + 1, winrect->y + 1,
                         MAX (0, winrect->width - 2), MAX (0, winrect->height - 2));

  if (is_active)
    {
      /* Sharpen the foreground color */
      cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.3f);
      cairo_rectangle (cr, winrect->x + 1, winrect->y + 1,
                       MAX (0, winrect->width - 2), MAX (0, winrect->height - 2));
      cairo_fill (cr);
    }

  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, translucency);

  icon = wnck_window_get_icon (win);

  icon_w = icon_h = 0;

  if (icon)
    {
      icon_w = gdk_pixbuf_get_width (icon);
      icon_h = gdk_pixbuf_get_height (icon);

      /* If the icon is too big, fall back to mini icon.
       * We don't arbitrarily scale the icon, because it's
       * just too slow on my Athlon 850.
       */
      if (icon_w > (winrect->width - 2) ||
          icon_h > (winrect->height - 2))
        {
          icon = wnck_window_get_mini_icon (win);
          if (icon)
            {
              icon_w = gdk_pixbuf_get_width (icon);
              icon_h = gdk_pixbuf_get_height (icon);

              /* Give up. */
              if (icon_w > (winrect->width - 2) ||
                  icon_h > (winrect->height - 2))
                icon = NULL;
            }
        }
    }

  if (icon)
    {
      icon_x = winrect->x + (winrect->width - icon_w) / 2;
      icon_y = winrect->y + (winrect->height - icon_h) / 2;

      cairo_push_group (cr);
      gtk_render_icon (context, cr, icon, icon_x, icon_y);
      cairo_pop_group_to_source (cr);
      cairo_paint_with_alpha (cr, translucency);
    }

  cairo_push_group (cr);
  gtk_render_frame (context, cr, winrect->x + 0.5, winrect->y + 0.5,
                    MAX (0, winrect->width - 1), MAX (0, winrect->height - 1));
  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, translucency);

  gtk_style_context_get_color (context, state, &fg);
  fg.alpha = translucency;
  gdk_cairo_set_source_rgba (cr, &fg);
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   winrect->x + 0.5, winrect->y + 0.5,
                   MAX (0, winrect->width - 1), MAX (0, winrect->height - 1));
  cairo_stroke (cr);

  gtk_style_context_restore (context);
}

static WnckWindow *
window_at_point (WnckPager     *pager,
                 WnckWorkspace *space,
                 GdkRectangle  *space_rect,
                 int            x,
                 int            y)
{
  WnckWindow *window;
  GList *windows;
  GList *tmp;

  window = NULL;

  windows = get_windows_for_workspace_in_bottom_to_top (pager->priv->screen,
                                                        space);

  /* clicks on top windows first */
  windows = g_list_reverse (windows);

  for (tmp = windows; tmp != NULL; tmp = tmp->next)
    {
      WnckWindow *win = WNCK_WINDOW (tmp->data);
      GdkRectangle winrect;

      get_window_rect (win, space_rect, &winrect);

      if (POINT_IN_RECT (x, y, winrect))
        {
          /* wnck_window_activate (win); */
          window = win;
          break;
        }
    }

  g_list_free (windows);

  return window;
}

static int
workspace_at_point (WnckPager *pager,
                    int        x,
                    int        y,
                    int       *viewport_x,
                    int       *viewport_y)
{
  GtkWidget *widget;
  int i;
  int n_spaces;
  GtkAllocation allocation;
  int focus_width;
  GtkBorder padding;

  widget = GTK_WIDGET (pager);

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_style_get (GTK_WIDGET (pager),
			"focus-line-width", &focus_width,
			NULL);
  _wnck_pager_get_padding (pager, &padding);

  padding.left += focus_width;
  padding.right += focus_width;
  padding.top += focus_width;
  padding.bottom += focus_width;

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;

      get_workspace_rect (pager, i, &rect);

      /* If workspace is on the edge, pretend points on the frame belong to the
       * workspace.
       * Else, pretend the right/bottom line separating two workspaces belong
       * to the workspace.
       */

      if (rect.x == padding.left)
        {
          rect.x = 0;
          rect.width += padding.left;
        }
      if (rect.y == padding.top)
        {
          rect.y = 0;
          rect.height += padding.top;
        }
      if (rect.y + rect.height == allocation.height - padding.bottom)
        {
          rect.height += padding.bottom;
        }
      else
        {
          rect.height += 1;
        }
      if (rect.x + rect.width == allocation.width - padding.right)
        {
          rect.width += padding.right;
        }
      else
        {
          rect.width += 1;
        }

      if (POINT_IN_RECT (x, y, rect))
        {
	  double width_ratio, height_ratio;
	  WnckWorkspace *space;

	  space = wnck_screen_get_workspace (pager->priv->screen, i);
          g_assert (space != NULL);

          /* Scale x, y mouse coords to corresponding screenwide viewport coords */

          width_ratio = (double) wnck_workspace_get_width (space) / (double) rect.width;
          height_ratio = (double) wnck_workspace_get_height (space) / (double) rect.height;

          if (viewport_x)
            *viewport_x = width_ratio * (x - rect.x);
          if (viewport_y)
            *viewport_y = height_ratio * (y - rect.y);

	  return i;
	}

      ++i;
    }

  return -1;
}

static void
draw_dark_rectangle (GtkStyleContext *context,
                     cairo_t *cr,
                     GtkStateFlags state,
                     int rx, int ry, int rw, int rh)
{
  gtk_style_context_save (context);

  gtk_style_context_set_state (context, state);

  cairo_push_group (cr);

  gtk_render_background (context, cr, rx, ry, rw, rh);
  cairo_set_source_rgba (cr, 0.0f, 0.0f, 0.0f, 0.3f);
  cairo_rectangle (cr, rx, ry, rw, rh);
  cairo_fill (cr);

  cairo_pop_group_to_source (cr);
  cairo_paint (cr);

  gtk_style_context_restore (context);
}

static void
wnck_pager_draw_workspace (WnckPager    *pager,
                           cairo_t      *cr,
                           int           workspace,
                           GdkRectangle *rect,
                           GdkPixbuf    *bg_pixbuf)
{
  GList *windows;
  GList *tmp;
  gboolean is_current;
  WnckWorkspace *space;
  GtkWidget *widget;
  GtkStateFlags state;
  GtkStyleContext *context;

  space = wnck_screen_get_workspace (pager->priv->screen, workspace);
  if (!space)
    return;

  widget = GTK_WIDGET (pager);
  is_current = (space == wnck_screen_get_active_workspace (pager->priv->screen));

  state = GTK_STATE_FLAG_NORMAL;
  if (is_current)
    state |= GTK_STATE_FLAG_SELECTED;
  else if (workspace == pager->priv->prelight)
    state |= GTK_STATE_FLAG_PRELIGHT;

  context = gtk_widget_get_style_context (widget);

  /* FIXME in names mode, should probably draw things like a button.
   */

  if (bg_pixbuf)
    {
      gdk_cairo_set_source_pixbuf (cr, bg_pixbuf, rect->x, rect->y);
      cairo_paint (cr);
    }
  else
    {
      if (!wnck_workspace_is_virtual (space))
        {
          draw_dark_rectangle (context, cr, state,
                               rect->x, rect->y, rect->width, rect->height);
        }
      else
        {
          //FIXME prelight for dnd in the viewport?
          int workspace_width, workspace_height;
          int screen_width, screen_height;
          double width_ratio, height_ratio;
          double vx, vy, vw, vh; /* viewport */

          workspace_width = wnck_workspace_get_width (space);
          workspace_height = wnck_workspace_get_height (space);
          screen_width = wnck_screen_get_width (pager->priv->screen);
          screen_height = wnck_screen_get_height (pager->priv->screen);

          if ((workspace_width % screen_width == 0) &&
              (workspace_height % screen_height == 0))
            {
              int i, j;
              int active_i, active_j;
              int horiz_views;
              int verti_views;

              horiz_views = workspace_width / screen_width;
              verti_views = workspace_height / screen_height;

              /* do not forget thin lines to delimit "workspaces" */
              width_ratio = (rect->width - (horiz_views - 1)) /
                            (double) workspace_width;
              height_ratio = (rect->height - (verti_views - 1)) /
                             (double) workspace_height;

              if (is_current)
                {
                  active_i =
                    wnck_workspace_get_viewport_x (space) / screen_width;
                  active_j =
                    wnck_workspace_get_viewport_y (space) / screen_height;
                }
              else
                {
                  active_i = -1;
                  active_j = -1;
                }

              for (i = 0; i < horiz_views; i++)
                {
                  /* "+ i" is for the thin lines */
                  vx = rect->x + (width_ratio * screen_width) * i + i;

                  if (i == horiz_views - 1)
                    vw = rect->width + rect->x - vx;
                  else
                    vw = width_ratio * screen_width;

                  vh = height_ratio * screen_height;

                  for (j = 0; j < verti_views; j++)
                    {
                      /* "+ j" is for the thin lines */
                      vy = rect->y + (height_ratio * screen_height) * j + j;
                      GtkStateFlags rec_state = GTK_STATE_FLAG_NORMAL;

                      if (j == verti_views - 1)
                        vh = rect->height + rect->y - vy;

                      if (active_i == i && active_j == j)
                        rec_state = GTK_STATE_FLAG_SELECTED;

                      draw_dark_rectangle (context, cr, rec_state, vx, vy, vw, vh);
                    }
                }
            }
          else
            {
              width_ratio = rect->width / (double) workspace_width;
              height_ratio = rect->height / (double) workspace_height;

              /* first draw non-active part of the viewport */
              draw_dark_rectangle (context, cr, GTK_STATE_FLAG_NORMAL,
                                   rect->x, rect->y, rect->width, rect->height);

              if (is_current)
                {
                  /* draw the active part of the viewport */
                  vx = rect->x +
                    width_ratio * wnck_workspace_get_viewport_x (space);
                  vy = rect->y +
                    height_ratio * wnck_workspace_get_viewport_y (space);
                  vw = width_ratio * screen_width;
                  vh = height_ratio * screen_height;

                  draw_dark_rectangle (context, cr, GTK_STATE_FLAG_SELECTED,
                                       vx, vy, vw, vh);
                }
            }
        }
    }

  if (pager->priv->display_mode == WNCK_PAGER_DISPLAY_CONTENT)
    {
      windows = get_windows_for_workspace_in_bottom_to_top (pager->priv->screen,
							    wnck_screen_get_workspace (pager->priv->screen,
										       workspace));

  tmp = windows;
  while (tmp != NULL)
  	{
  	  WnckWindow *win = tmp->data;
  	  GdkRectangle winrect;

  	  get_window_rect (win, rect, &winrect);

      gboolean translucent = (win == pager->priv->drag_window) && pager->priv->dragging;
      draw_window (cr, widget, win, &winrect, state, translucent);

  	  tmp = tmp->next;
  	}

      g_list_free (windows);
    }
  else
    {
      /* Workspace name mode */
      GtkStateFlags layout_state;
      const char *workspace_name;
      PangoLayout *layout;
      WnckWorkspace *ws;
      int w, h;

      ws = wnck_screen_get_workspace (pager->priv->screen, workspace);
      workspace_name = wnck_workspace_get_name (ws);
      layout = gtk_widget_create_pango_layout (widget, workspace_name);

      pango_layout_get_pixel_size (layout, &w, &h);

      layout_state = (is_current) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_NORMAL;
      gtk_style_context_save (context);
      gtk_style_context_set_state (context, layout_state);
      gtk_render_layout (context, cr, rect->x + (rect->width - w) / 2,
                         rect->y + (rect->height - h) / 2, layout);

      gtk_style_context_restore (context);
      g_object_unref (layout);
    }

  if (workspace == pager->priv->prelight && pager->priv->prelight_dnd)
    {
      gtk_style_context_save (context);
      gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
      gtk_render_frame (context, cr, rect->x, rect->y, rect->width, rect->height);
      gtk_style_context_restore (context);

      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
      cairo_set_line_width (cr, 1.0);
      cairo_rectangle (cr, rect->x + 0.5, rect->y + 0.5,
                           MAX (0, rect->width - 1), MAX (0, rect->height - 1));
      cairo_stroke (cr);
    }
}

static gboolean
wnck_pager_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  WnckPager *pager;
  int i;
  int n_spaces;
  WnckWorkspace *active_space;
  GdkPixbuf *bg_pixbuf;
  gboolean first;
  GtkStyleContext *context;
  GtkStateFlags state;
  int focus_width;

  pager = WNCK_PAGER (widget);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
  active_space = wnck_screen_get_active_workspace (pager->priv->screen);
  bg_pixbuf = NULL;
  first = TRUE;

  state = gtk_widget_get_state_flags (widget);
  context = gtk_widget_get_style_context (widget);
  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			NULL);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  if (gtk_widget_has_focus (widget))
  {
    cairo_save (cr);
    gtk_render_focus (context, cr,
		      0, 0,
		      gtk_widget_get_allocated_width (widget),
		      gtk_widget_get_allocated_height (widget));
    cairo_restore (cr);
  }

  if (pager->priv->shadow_type != GTK_SHADOW_NONE)
    {
      cairo_save (cr);
      gtk_render_frame (context, cr,
			focus_width,
			focus_width,
                        gtk_widget_get_allocated_width (widget) - 2 * focus_width,
                        gtk_widget_get_allocated_height (widget) - 2 * focus_width);
      cairo_restore (cr);
    }

  gtk_style_context_restore (context);

  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;

      if (pager->priv->show_all_workspaces ||
	  (active_space && i == wnck_workspace_get_number (active_space)))
	{
	  get_workspace_rect (pager, i, &rect);

          /* We only want to do this once, even if w/h change,
           * for efficiency. width/height will only change by
           * one pixel at most.
           */
          if (first &&
              pager->priv->display_mode == WNCK_PAGER_DISPLAY_CONTENT)
            {
              bg_pixbuf = wnck_pager_get_background (pager,
                                                     rect.width,
                                                     rect.height);
              first = FALSE;
            }

	  wnck_pager_draw_workspace (pager, cr, i, &rect, bg_pixbuf);
	}

      ++i;
    }

  return FALSE;
}

static gboolean
wnck_pager_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
  WnckPager *pager;
  int space_number;
  WnckWorkspace *space = NULL;
  GdkRectangle workspace_rect;

  if (event->button != 1)
    return FALSE;

  pager = WNCK_PAGER (widget);

  space_number = workspace_at_point (pager, event->x, event->y, NULL, NULL);

  if (space_number != -1)
  {
    get_workspace_rect (pager, space_number, &workspace_rect);
    space = wnck_screen_get_workspace (pager->priv->screen, space_number);
  }

  if (space)
    {
      /* always save the start coordinates so we can know if we need to change
       * workspace when the button is released (ie, start and end coordinates
       * should be in the same workspace) */
      pager->priv->drag_start_x = event->x;
      pager->priv->drag_start_y = event->y;
    }

  if (space && (pager->priv->display_mode != WNCK_PAGER_DISPLAY_NAME))
    {
      pager->priv->drag_window = window_at_point (pager, space,
                                                  &workspace_rect,
                                                  event->x, event->y);
    }

  return TRUE;
}

static gboolean
wnck_pager_drag_motion_timeout (gpointer data)
{
  WnckPager *pager = WNCK_PAGER (data);
  WnckWorkspace *active_workspace, *dnd_workspace;

  pager->priv->dnd_activate = 0;
  active_workspace = wnck_screen_get_active_workspace (pager->priv->screen);
  dnd_workspace    = wnck_screen_get_workspace (pager->priv->screen,
                                                pager->priv->prelight);

  if (dnd_workspace &&
      (pager->priv->prelight != wnck_workspace_get_number (active_workspace)))
    wnck_workspace_activate (dnd_workspace, pager->priv->dnd_time);

  return FALSE;
}

static void
wnck_pager_queue_draw_workspace (WnckPager *pager,
                                 gint       i)
{
  GdkRectangle rect;

  if (i < 0)
    return;

  get_workspace_rect (pager, i, &rect);
  gtk_widget_queue_draw_area (GTK_WIDGET (pager),
                              rect.x, rect.y,
			      rect.width, rect.height);
}

static void
wnck_pager_queue_draw_window (WnckPager  *pager,
                              WnckWindow *window)
{
  gint workspace;

  workspace = wnck_pager_window_get_workspace (window, TRUE);
  if (workspace == -1)
    return;

  wnck_pager_queue_draw_workspace (pager, workspace);
}

static void
wnck_pager_check_prelight (WnckPager *pager,
                           gint       x,
                           gint       y,
                           gboolean   prelight_dnd)
{
  gint id;

  if (x < 0 || y < 0)
    id = -1;
  else
    id = workspace_at_point (pager, x, y, NULL, NULL);

  if (id != pager->priv->prelight)
    {
      wnck_pager_queue_draw_workspace (pager, pager->priv->prelight);
      wnck_pager_queue_draw_workspace (pager, id);
      pager->priv->prelight = id;
      pager->priv->prelight_dnd = prelight_dnd;
    }
  else if (prelight_dnd != pager->priv->prelight_dnd)
    {
      wnck_pager_queue_draw_workspace (pager, pager->priv->prelight);
      pager->priv->prelight_dnd = prelight_dnd;
    }
}

static gboolean
wnck_pager_drag_motion (GtkWidget          *widget,
                        GdkDragContext     *context,
                        gint                x,
                        gint                y,
                        guint               time)
{
  WnckPager *pager;
  gint previous_workspace;

  pager = WNCK_PAGER (widget);

  previous_workspace = pager->priv->prelight;
  wnck_pager_check_prelight (pager, x, y, TRUE);

  if (gtk_drag_dest_find_target (widget, context, NULL))
    {
#if GTK_CHECK_VERSION(2,21,0)
      gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), time);
#else
      gdk_drag_status (context, context->suggested_action, time);
#endif
    }
  else
    {
      gdk_drag_status (context, 0, time);

      if (pager->priv->prelight != previous_workspace &&
          pager->priv->dnd_activate != 0)
        {
          /* remove timeout, the window we hover over changed */
          g_source_remove (pager->priv->dnd_activate);
          pager->priv->dnd_activate = 0;
          pager->priv->dnd_time = 0;
        }

      if (pager->priv->dnd_activate == 0 && pager->priv->prelight > -1)
        {
          pager->priv->dnd_activate = g_timeout_add_seconds (WNCK_ACTIVATE_TIMEOUT,
                                                             wnck_pager_drag_motion_timeout,
                                                             pager);
          pager->priv->dnd_time = time;
        }
    }

  return (pager->priv->prelight != -1);
}

static gboolean
wnck_pager_drag_drop  (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       guint             time)
{
  WnckPager *pager = WNCK_PAGER (widget);
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target != GDK_NONE)
    gtk_drag_get_data (widget, context, target, time);
  else
    gtk_drag_finish (context, FALSE, FALSE, time);

  wnck_pager_clear_drag (pager);
  wnck_pager_check_prelight (pager, x, y, FALSE);

  return TRUE;
}

static void
wnck_pager_drag_data_received (GtkWidget          *widget,
	  	               GdkDragContext     *context,
			       gint                x,
			       gint                y,
			       GtkSelectionData   *selection_data,
			       guint               info,
			       guint               time)
{
  WnckPager *pager = WNCK_PAGER (widget);
  WnckWorkspace *space;
  GList *tmp;
  gint i;
  gulong xid;

  if ((gtk_selection_data_get_length (selection_data) != sizeof (gulong)) ||
      (gtk_selection_data_get_format (selection_data) != 8))
    {
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  i = workspace_at_point (pager, x, y, NULL, NULL);
  space = wnck_screen_get_workspace (pager->priv->screen, i);
  if (!space)
    {
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  xid = *((gulong *) gtk_selection_data_get_data (selection_data));

  for (tmp = wnck_screen_get_windows_stacked (pager->priv->screen); tmp != NULL; tmp = tmp->next)
    {
      if (wnck_window_get_xid (tmp->data) == xid)
	{
	  WnckWindow *win = tmp->data;
	  wnck_window_move_to_workspace (win, space);
	  if (space == wnck_screen_get_active_workspace (pager->priv->screen))
	    wnck_window_activate (win, time);
	  gtk_drag_finish (context, TRUE, FALSE, time);
	  return;
	}
    }

  gtk_drag_finish (context, FALSE, FALSE, time);
}

static void
wnck_pager_drag_data_get (GtkWidget        *widget,
                          GdkDragContext   *context,
                          GtkSelectionData *selection_data,
                          guint             info,
                          guint             time)
{
  WnckPager *pager = WNCK_PAGER (widget);
  gulong xid;

  if (pager->priv->drag_window == NULL)
    return;

  xid = wnck_window_get_xid (pager->priv->drag_window);
  gtk_selection_data_set (selection_data,
			  gtk_selection_data_get_target (selection_data),
			  8, (guchar *)&xid, sizeof (gulong));
}

static void
wnck_pager_drag_end (GtkWidget        *widget,
                     GdkDragContext   *context)
{
  WnckPager *pager = WNCK_PAGER (widget);

  wnck_pager_clear_drag (pager);
}

static void
wnck_pager_drag_motion_leave (GtkWidget          *widget,
                              GdkDragContext     *context,
                              guint               time)
{
  WnckPager *pager = WNCK_PAGER (widget);

  if (pager->priv->dnd_activate != 0)
    {
      g_source_remove (pager->priv->dnd_activate);
      pager->priv->dnd_activate = 0;
    }
  pager->priv->dnd_time = 0;
  wnck_pager_check_prelight (pager, -1, -1, FALSE);
}

static void
wnck_drag_clean_up (WnckWindow     *window,
		    GdkDragContext *context,
		    gboolean	    clean_up_for_context_destroy,
		    gboolean	    clean_up_for_window_destroy);

static void
wnck_drag_context_destroyed (gpointer  windowp,
                             GObject  *context)
{
  wnck_drag_clean_up (windowp, (GdkDragContext *) context, TRUE, FALSE);
}

static void
wnck_update_drag_icon (WnckWindow     *window,
		       GdkDragContext *context)
{
  gint org_w, org_h, dnd_w, dnd_h;
  WnckWorkspace *workspace;
  GdkRectangle rect;
  cairo_surface_t *surface;
  GtkWidget *widget;
  cairo_t *cr;

  widget = g_object_get_data (G_OBJECT (context), "wnck-drag-source-widget");
  if (!widget)
    return;

  if (!gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (widget),
					  GTK_ICON_SIZE_DND, &dnd_w, &dnd_h))
    dnd_w = dnd_h = 32;
  /* windows are huge, so let's make this huge */
  dnd_w *= 3;

  workspace = wnck_window_get_workspace (window);
  if (workspace == NULL)
    workspace = wnck_screen_get_active_workspace (wnck_window_get_screen (window));
  if (workspace == NULL)
    return;

  wnck_window_get_geometry (window, NULL, NULL, &org_w, &org_h);

  rect.x = rect.y = 0;
  rect.width = 0.5 + ((double) (dnd_w * org_w) / (double) wnck_workspace_get_width (workspace));
  rect.width = MIN (org_w, rect.width);
  rect.height = 0.5 + ((double) (rect.width * org_h) / (double) org_w);

  /* we need at least three pixels to draw the smallest window */
  rect.width = MAX (rect.width, 3);
  rect.height = MAX (rect.height, 3);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               rect.width, rect.height);
  cr = cairo_create (surface);
  draw_window (cr, widget, window, &rect, GTK_STATE_FLAG_NORMAL, FALSE);
  cairo_destroy (cr);
  cairo_surface_set_device_offset (surface, 2, 2);

  gtk_drag_set_icon_surface (context, surface);

  cairo_surface_destroy (surface);
}

static void
wnck_drag_window_destroyed (gpointer  contextp,
                            GObject  *window)
{
  wnck_drag_clean_up ((WnckWindow *) window, GDK_DRAG_CONTEXT (contextp),
                      FALSE, TRUE);
}

static void
wnck_drag_source_destroyed (gpointer  contextp,
                            GObject  *drag_source)
{
  g_object_steal_data (G_OBJECT (contextp), "wnck-drag-source-widget");
}

/* CAREFUL: This function is a bit brittle, because the pointers given may be
 * finalized already */
static void
wnck_drag_clean_up (WnckWindow     *window,
		    GdkDragContext *context,
		    gboolean	    clean_up_for_context_destroy,
		    gboolean	    clean_up_for_window_destroy)
{
  if (clean_up_for_context_destroy)
    {
      GtkWidget *drag_source;

      drag_source = g_object_get_data (G_OBJECT (context),
                                       "wnck-drag-source-widget");
      if (drag_source)
        g_object_weak_unref (G_OBJECT (drag_source),
                             wnck_drag_source_destroyed, context);

      g_object_weak_unref (G_OBJECT (window),
                           wnck_drag_window_destroyed, context);
      if (g_signal_handlers_disconnect_by_func (window,
                                                wnck_update_drag_icon, context) != 2)
	g_assert_not_reached ();
    }

  if (clean_up_for_window_destroy)
    {
      g_object_steal_data (G_OBJECT (context), "wnck-drag-source-widget");
      g_object_weak_unref (G_OBJECT (context),
                           wnck_drag_context_destroyed, window);
    }
}

/**
 * wnck_window_set_as_drag_icon:
 * @window: #WnckWindow to use as drag icon
 * @context: #GdkDragContext to set the icon on
 * @drag_source: #GtkWidget that started the drag, or one of its parent. This
 * widget needs to stay alive as long as possible during the drag.
 *
 * Sets the given @window as the drag icon for @context.
 **/
void
_wnck_window_set_as_drag_icon (WnckWindow     *window,
                               GdkDragContext *context,
                               GtkWidget      *drag_source)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_weak_ref (G_OBJECT (window), wnck_drag_window_destroyed, context);
  g_signal_connect (window, "geometry_changed",
                    G_CALLBACK (wnck_update_drag_icon), context);
  g_signal_connect (window, "icon_changed",
                    G_CALLBACK (wnck_update_drag_icon), context);

  g_object_set_data (G_OBJECT (context), "wnck-drag-source-widget", drag_source);
  g_object_weak_ref (G_OBJECT (drag_source), wnck_drag_source_destroyed, context);

  g_object_weak_ref (G_OBJECT (context), wnck_drag_context_destroyed, window);

  wnck_update_drag_icon (window, context);
}

static gboolean
wnck_pager_motion (GtkWidget        *widget,
                   GdkEventMotion   *event)
{
  WnckPager *pager;
  GdkWindow *window;
  GdkDeviceManager *dev_manager;
  GdkDevice *pointer;
  int x, y;

  pager = WNCK_PAGER (widget);

  dev_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
  window = gtk_widget_get_window (widget);
  pointer = gdk_device_manager_get_client_pointer (dev_manager);
  gdk_window_get_device_position (window, pointer, &x, &y, NULL);

  if (!pager->priv->dragging &&
      pager->priv->drag_window != NULL &&
      gtk_drag_check_threshold (widget,
                                pager->priv->drag_start_x,
                                pager->priv->drag_start_y,
                                x, y))
    {
      GdkDragContext *context;
      context = gtk_drag_begin (widget,
				gtk_drag_dest_get_target_list (widget),
				GDK_ACTION_MOVE,
				1, (GdkEvent *)event);
      pager->priv->dragging = TRUE;
      pager->priv->prelight_dnd = TRUE;
      _wnck_window_set_as_drag_icon (pager->priv->drag_window,
				     context,
				     GTK_WIDGET (pager));
    }

  wnck_pager_check_prelight (pager, x, y, pager->priv->prelight_dnd);

  return TRUE;
}

static gboolean
wnck_pager_leave_notify	 (GtkWidget          *widget,
	      		  GdkEventCrossing   *event)
{
  WnckPager *pager;

  pager = WNCK_PAGER (widget);

  wnck_pager_check_prelight (pager, -1, -1, FALSE);

  return FALSE;
}

static gboolean
wnck_pager_button_release (GtkWidget        *widget,
                           GdkEventButton   *event)
{
  WnckWorkspace *space;
  WnckPager *pager;
  int i;
  int j;
  int viewport_x;
  int viewport_y;

  if (event->button != 1)
    return FALSE;

  pager = WNCK_PAGER (widget);

  if (!pager->priv->dragging)
    {
      i = workspace_at_point (pager,
                              event->x, event->y,
                              &viewport_x, &viewport_y);
      j = workspace_at_point (pager,
                              pager->priv->drag_start_x,
                              pager->priv->drag_start_y,
                              NULL, NULL);

      if (i == j && i >= 0 &&
          (space = wnck_screen_get_workspace (pager->priv->screen, i)))
        {
          int screen_width, screen_height;

          /* Don't switch the desktop if we're already there */
          if (space != wnck_screen_get_active_workspace (pager->priv->screen))
            wnck_workspace_activate (space, event->time);

          /* EWMH only lets us move the viewport for the active workspace,
           * but we just go ahead and hackily assume that the activate
           * just above takes effect prior to moving the viewport
           */

          /* Transform the pointer location to viewport origin, assuming
           * that we want the nearest "regular" viewport containing the
           * pointer.
           */
          screen_width  = wnck_screen_get_width  (pager->priv->screen);
          screen_height = wnck_screen_get_height (pager->priv->screen);
          viewport_x = (viewport_x / screen_width)  * screen_width;
          viewport_y = (viewport_y / screen_height) * screen_height;

          if (wnck_workspace_get_viewport_x (space) != viewport_x ||
              wnck_workspace_get_viewport_y (space) != viewport_y)
            wnck_screen_move_viewport (pager->priv->screen, viewport_x, viewport_y);
        }

      wnck_pager_clear_drag (pager);
    }

  return FALSE;
}

static gboolean
wnck_pager_scroll_event (GtkWidget      *widget,
                         GdkEventScroll *event)
{
  WnckPager          *pager;
  WnckWorkspace      *space;
  GdkScrollDirection  absolute_direction;
  int                 index;
  int                 n_workspaces;
  int                 n_columns;
  int                 in_last_row;
  gdouble             smooth_x;
  gdouble             smooth_y;

  pager = WNCK_PAGER (widget);

  if (event->type != GDK_SCROLL)
    return FALSE;
  if (event->direction == GDK_SCROLL_SMOOTH)
    return FALSE;

  absolute_direction = event->direction;

  space = wnck_screen_get_active_workspace (pager->priv->screen);
  index = wnck_workspace_get_number (space);

  n_workspaces = wnck_screen_get_workspace_count (pager->priv->screen);
  n_columns = n_workspaces / pager->priv->n_rows;
  if (n_workspaces % pager->priv->n_rows != 0)
    n_columns++;
  in_last_row = n_workspaces % n_columns;

  if (gtk_widget_get_direction (GTK_WIDGET (pager)) == GTK_TEXT_DIR_RTL)
    {
      switch (event->direction)
        {
          case GDK_SCROLL_DOWN:
          case GDK_SCROLL_UP:
            break;
          case GDK_SCROLL_RIGHT:
            absolute_direction = GDK_SCROLL_LEFT;
            break;
          case GDK_SCROLL_LEFT:
            absolute_direction = GDK_SCROLL_RIGHT;
            break;
          case GDK_SCROLL_SMOOTH:
            gdk_event_get_scroll_deltas ((GdkEvent*)event, &smooth_x, &smooth_y);
            if (smooth_x > 5)
              absolute_direction = GDK_SCROLL_RIGHT;
            else if (smooth_x < -5)
              absolute_direction = GDK_SCROLL_LEFT;
            break;
        }
    }

  switch (absolute_direction)
    {
      case GDK_SCROLL_DOWN:
        if (index + n_columns < n_workspaces)
          index += n_columns;
        else if ((index < n_workspaces - 1 &&
                  index + in_last_row != n_workspaces - 1) ||
                 (index == n_workspaces - 1 &&
                  in_last_row != 0))
          index = (index % n_columns) + 1;
        break;

      case GDK_SCROLL_RIGHT:
        if (index < n_workspaces - 1)
          index++;
        break;

      case GDK_SCROLL_UP:
        if (index - n_columns >= 0)
          index -= n_columns;
        else if (index > 0)
          index = ((pager->priv->n_rows - 1) * n_columns) + (index % n_columns) - 1;
        if (index >= n_workspaces)
          index -= n_columns;
        break;

      case GDK_SCROLL_LEFT:
        if (index > 0)
          index--;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  space = wnck_screen_get_workspace (pager->priv->screen, index);
  wnck_workspace_activate (space, event->time);

  return TRUE;
}

static gboolean
wnck_pager_query_tooltip (GtkWidget  *widget,
                          gint        x,
                          gint        y,
                          gboolean    keyboard_tip,
                          GtkTooltip *tooltip)
{
  int i;
  WnckPager *pager;
  WnckScreen *screen;
  WnckWorkspace *space;
  char *name;

  pager = WNCK_PAGER (widget);
  screen = pager->priv->screen;

  i = workspace_at_point (pager, x, y, NULL, NULL);
  space = wnck_screen_get_workspace (screen, i);
  if (!space)
    return GTK_WIDGET_CLASS (wnck_pager_parent_class)->query_tooltip (widget,
                                                                      x, y,
                                                                      keyboard_tip,
                                                                      tooltip);

  if (wnck_screen_get_active_workspace (screen) == space)
    {
      WnckWindow *window;
      GdkRectangle workspace_rect;

      get_workspace_rect (pager, i, &workspace_rect);

      window = window_at_point (pager, space, &workspace_rect, x, y);

      if (window)
        name = g_strdup_printf (_("Click to start dragging \"%s\""),
                                wnck_window_get_name (window));
      else
        name = g_strdup_printf (_("Current workspace: \"%s\""),
                                wnck_workspace_get_name (space));
    }
  else
    {
      name = g_strdup_printf (_("Click to switch to \"%s\""),
                              wnck_workspace_get_name (space));
    }

  gtk_tooltip_set_text (tooltip, name);

  g_free (name);

  return TRUE;
}

/**
 * wnck_pager_new:
 *
 * Creates a new #WnckPager. The #WnckPager will show the #WnckWorkspace of the
 * #WnckScreen it is on.
 *
 * Return value: a newly created #WnckPager.
 */
GtkWidget*
wnck_pager_new (void)
{
  WnckPager *pager;

  pager = g_object_new (WNCK_TYPE_PAGER, NULL);

  return GTK_WIDGET (pager);
}

static gboolean
wnck_pager_set_layout_hint (WnckPager *pager)
{
  int layout_rows;
  int layout_cols;

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _wnck_pager_set_screen (pager);
  /* can still happen if the pager was not added to a widget hierarchy */
  if (pager->priv->screen == NULL)
    return FALSE;

  /* The visual representation of the pager doesn't
   * correspond to the layout of the workspaces
   * here. i.e. the user will not pay any attention
   * to the n_rows setting on this pager.
   */
  if (!pager->priv->show_all_workspaces)
    return FALSE;

  if (pager->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      layout_rows = pager->priv->n_rows;
      layout_cols = 0;
    }
  else
    {
      layout_rows = 0;
      layout_cols = pager->priv->n_rows;
    }

  pager->priv->layout_manager_token =
    wnck_screen_try_set_workspace_layout (pager->priv->screen,
                                          pager->priv->layout_manager_token,
                                          layout_rows,
                                          layout_cols);

  return (pager->priv->layout_manager_token != WNCK_NO_MANAGER_TOKEN);
}

/**
 * wnck_pager_set_orientation:
 * @pager: a #WnckPager.
 * @orientation: orientation to use for the layout of #WnckWorkspace on the
 * #WnckScreen @pager is watching.
 *
 * Tries to change the orientation of the layout of #WnckWorkspace on the
 * #WnckScreen @pager is watching. Since no more than one application should
 * set this property of a #WnckScreen at a time, setting the layout is not
 * guaranteed to work.
 *
 * If @orientation is %GTK_ORIENTATION_HORIZONTAL, the #WnckWorkspace will be
 * laid out in rows, with the first #WnckWorkspace in the top left corner.
 *
 * If @orientation is %GTK_ORIENTATION_VERTICAL, the #WnckWorkspace will be
 * laid out in columns, with the first #WnckWorkspace in the top left corner.
 *
 * For example, if the layout contains one row, but the orientation of the
 * layout is vertical, the #WnckPager will display a column of #WnckWorkspace.
 *
 * Note that setting the orientation will have an effect on the geometry
 * management: if @orientation is %GTK_ORIENTATION_HORIZONTAL,
 * %GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT will be used as request mode; if
 * @orientation is %GTK_ORIENTATION_VERTICAL, GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH
 * will be used instead.
 *
 * If @pager has not been added to a widget hierarchy, the call will fail
 * because @pager can't know the screen on which to modify the orientation.
 *
 * Return value: %TRUE if the layout of #WnckWorkspace has been successfully
 * changed or did not need to be changed, %FALSE otherwise.
 */
gboolean
wnck_pager_set_orientation (WnckPager     *pager,
                            GtkOrientation orientation)
{
  GtkOrientation old_orientation;
  gboolean       old_orientation_is_valid;

  g_return_val_if_fail (WNCK_IS_PAGER (pager), FALSE);

  if (pager->priv->orientation == orientation)
    return TRUE;

  old_orientation = pager->priv->orientation;
  old_orientation_is_valid = pager->priv->screen != NULL;

  pager->priv->orientation = orientation;

  if (wnck_pager_set_layout_hint (pager))
    {
      gtk_widget_queue_resize (GTK_WIDGET (pager));
      return TRUE;
    }
  else
    {
      if (old_orientation_is_valid)
        pager->priv->orientation = old_orientation;
      return FALSE;
    }
}

/**
 * wnck_pager_set_n_rows:
 * @pager: a #WnckPager.
 * @n_rows: the number of rows to use for the layout of #WnckWorkspace on the
 * #WnckScreen @pager is watching.
 *
 * Tries to change the number of rows in the layout of #WnckWorkspace on the
 * #WnckScreen @pager is watching. Since no more than one application should
 * set this property of a #WnckScreen at a time, setting the layout is not
 * guaranteed to work.
 *
 * If @pager has not been added to a widget hierarchy, the call will fail
 * because @pager can't know the screen on which to modify the layout.
 *
 * Return value: %TRUE if the layout of #WnckWorkspace has been successfully
 * changed or did not need to be changed, %FALSE otherwise.
 */
gboolean
wnck_pager_set_n_rows (WnckPager *pager,
		       int        n_rows)
{
  int      old_n_rows;
  gboolean old_n_rows_is_valid;

  g_return_val_if_fail (WNCK_IS_PAGER (pager), FALSE);
  g_return_val_if_fail (n_rows > 0, FALSE);

  if (pager->priv->n_rows == n_rows)
    return TRUE;

  old_n_rows = pager->priv->n_rows;
  old_n_rows_is_valid = pager->priv->screen != NULL;

  pager->priv->n_rows = n_rows;

  if (wnck_pager_set_layout_hint (pager))
    {
      gtk_widget_queue_resize (GTK_WIDGET (pager));
      return TRUE;
    }
  else
    {
      if (old_n_rows_is_valid)
        pager->priv->n_rows = old_n_rows;
      return FALSE;
    }
}

/**
 * wnck_pager_set_display_mode:
 * @pager: a #WnckPager.
 * @mode: a display mode.
 *
 * Sets the display mode for @pager to @mode.
 */
void
wnck_pager_set_display_mode (WnckPager            *pager,
			     WnckPagerDisplayMode  mode)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  if (pager->priv->display_mode == mode)
    return;

  g_object_set (pager, "has-tooltip", mode != WNCK_PAGER_DISPLAY_NAME, NULL);

  pager->priv->display_mode = mode;
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

/**
 * wnck_pager_set_show_all:
 * @pager: a #WnckPager.
 * @show_all_workspaces: whether to display all #WnckWorkspace in @pager.
 *
 * Sets @pager to display all #WnckWorkspace or not, according to
 * @show_all_workspaces.
 */
void
wnck_pager_set_show_all (WnckPager *pager,
			 gboolean   show_all_workspaces)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  show_all_workspaces = (show_all_workspaces != 0);

  if (pager->priv->show_all_workspaces == show_all_workspaces)
    return;

  pager->priv->show_all_workspaces = show_all_workspaces;
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

/**
 * wnck_pager_set_shadow_type:
 * @pager: a #WnckPager.
 * @shadow_type: a shadow type.
 *
 * Sets the shadow type for @pager to @shadow_type. The main use of this
 * function is proper integration of #WnckPager in panels with non-system
 * backgrounds.
 *
 * Since: 2.2
 */
void
wnck_pager_set_shadow_type (WnckPager *   pager,
			    GtkShadowType shadow_type)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  if (pager->priv->shadow_type == shadow_type)
    return;

  pager->priv->shadow_type = shadow_type;
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

static void
active_window_changed_callback    (WnckScreen      *screen,
                                   WnckWindow      *previous_window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
active_workspace_changed_callback (WnckScreen      *screen,
                                   WnckWorkspace   *previous_workspace,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_stacking_changed_callback  (WnckScreen      *screen,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_opened_callback            (WnckScreen      *screen,
                                   WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);

  wnck_pager_connect_window (pager, window);
  wnck_pager_queue_draw_window (pager, window);
}

static void
window_closed_callback            (WnckScreen      *screen,
                                   WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);

  if (pager->priv->drag_window == window)
    wnck_pager_clear_drag (pager);

  wnck_pager_queue_draw_window (pager, window);
}

static void
workspace_created_callback        (WnckScreen      *screen,
                                   WnckWorkspace   *space,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  g_signal_connect (space, "name_changed",
                    G_CALLBACK (workspace_name_changed_callback), pager);
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

static void
workspace_destroyed_callback      (WnckScreen      *screen,
                                   WnckWorkspace   *space,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  g_signal_handlers_disconnect_by_func (space, G_CALLBACK (workspace_name_changed_callback), pager);
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

static void
application_opened_callback       (WnckScreen      *screen,
                                   WnckApplication *app,
                                   gpointer         data)
{
  /*   WnckPager *pager = WNCK_PAGER (data); */
}

static void
application_closed_callback       (WnckScreen      *screen,
                                   WnckApplication *app,
                                   gpointer         data)
{
  /*   WnckPager *pager = WNCK_PAGER (data); */
}

static void
window_name_changed_callback      (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  wnck_pager_queue_draw_window (pager, window);
}

static void
window_state_changed_callback     (WnckWindow      *window,
                                   WnckWindowState  changed,
                                   WnckWindowState  new,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);

  /* if the changed state changes the visibility in the pager, we need to
   * redraw the whole workspace. wnck_pager_queue_draw_window() might not be
   * enough */
  if (!wnck_pager_window_state_is_relevant (changed))
    wnck_pager_queue_draw_workspace (pager,
                                     wnck_pager_window_get_workspace (window,
                                                                      FALSE));
  else
    wnck_pager_queue_draw_window (pager, window);
}

static void
window_workspace_changed_callback (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_icon_changed_callback      (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  wnck_pager_queue_draw_window (pager, window);
}

static void
window_geometry_changed_callback  (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);

  wnck_pager_queue_draw_window (pager, window);
}

static void
background_changed_callback (WnckWindow *window,
                             gpointer    data)
{
  WnckPager *pager = WNCK_PAGER (data);

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
workspace_name_changed_callback (WnckWorkspace *space,
                                 gpointer       data)
{
  gtk_widget_queue_resize (GTK_WIDGET (data));
}

static void
viewports_changed_callback (WnckWorkspace *space,
                            gpointer       data)
{
  gtk_widget_queue_resize (GTK_WIDGET (data));
}

static void
wnck_pager_connect_screen (WnckPager *pager)
{
  int i;
  guint *c;
  GList *tmp;
  WnckScreen *screen;

  g_return_if_fail (pager->priv->screen != NULL);

  screen = pager->priv->screen;

  for (tmp = wnck_screen_get_windows (screen); tmp; tmp = tmp->next)
    {
      wnck_pager_connect_window (pager, WNCK_WINDOW (tmp->data));
    }

  i = 0;
  c = pager->priv->screen_connections;

  c[i] = g_signal_connect (G_OBJECT (screen), "active_window_changed",
                           G_CALLBACK (active_window_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "active_workspace_changed",
                           G_CALLBACK (active_workspace_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_stacking_changed",
                           G_CALLBACK (window_stacking_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_opened",
                           G_CALLBACK (window_opened_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_closed",
                           G_CALLBACK (window_closed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "workspace_created",
                           G_CALLBACK (workspace_created_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "workspace_destroyed",
                           G_CALLBACK (workspace_destroyed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "application_opened",
                           G_CALLBACK (application_opened_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "application_closed",
                           G_CALLBACK (application_closed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "background_changed",
                           G_CALLBACK (background_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "viewports_changed",
                           G_CALLBACK (viewports_changed_callback),
                           pager);
  ++i;

  g_assert (i == N_SCREEN_CONNECTIONS);

  /* connect to name_changed on each workspace */
  for (i = 0; i < wnck_screen_get_workspace_count (pager->priv->screen); i++)
    {
      WnckWorkspace *space;
      space = wnck_screen_get_workspace (pager->priv->screen, i);
      g_signal_connect (space, "name_changed",
                        G_CALLBACK (workspace_name_changed_callback), pager);
    }
}

static void
wnck_pager_connect_window (WnckPager  *pager,
                           WnckWindow *window)
{
  g_signal_connect (G_OBJECT (window), "name_changed",
                    G_CALLBACK (window_name_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "state_changed",
                    G_CALLBACK (window_state_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "workspace_changed",
                    G_CALLBACK (window_workspace_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "icon_changed",
                    G_CALLBACK (window_icon_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "geometry_changed",
                    G_CALLBACK (window_geometry_changed_callback),
                    pager);
}

static void
wnck_pager_disconnect_screen (WnckPager  *pager)
{
  int i;
  GList *tmp;

  if (pager->priv->screen == NULL)
    return;

  i = 0;
  while (i < N_SCREEN_CONNECTIONS)
    {
      if (pager->priv->screen_connections[i] != 0)
        g_signal_handler_disconnect (G_OBJECT (pager->priv->screen),
                                     pager->priv->screen_connections[i]);

      pager->priv->screen_connections[i] = 0;

      ++i;
    }

  for (i = 0; i < wnck_screen_get_workspace_count (pager->priv->screen); i++)
    {
      WnckWorkspace *space;
      space = wnck_screen_get_workspace (pager->priv->screen, i);
      g_signal_handlers_disconnect_by_func (space, G_CALLBACK (workspace_name_changed_callback), pager);
    }

  for (tmp = wnck_screen_get_windows (pager->priv->screen); tmp; tmp = tmp->next)
    {
      wnck_pager_disconnect_window (pager, WNCK_WINDOW (tmp->data));
    }
}

static void
wnck_pager_disconnect_window (WnckPager  *pager,
                              WnckWindow *window)
{
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_name_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_state_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_workspace_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_icon_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_geometry_changed_callback),
                                        pager);
}

static void
wnck_pager_clear_drag (WnckPager *pager)
{
  if (pager->priv->dragging)
    wnck_pager_queue_draw_window (pager, pager->priv->drag_window);

  pager->priv->dragging = FALSE;
  pager->priv->drag_window = NULL;
  pager->priv->drag_start_x = -1;
  pager->priv->drag_start_y = -1;
}

static GdkPixbuf*
wnck_pager_get_background (WnckPager *pager,
                           int        width,
                           int        height)
{
  Pixmap p;
  GdkPixbuf *pix = NULL;

  /* We have to be careful not to keep alternating between
   * width/height values, otherwise this would get really slow.
   */
  if (pager->priv->bg_cache &&
      gdk_pixbuf_get_width (pager->priv->bg_cache) == width &&
      gdk_pixbuf_get_height (pager->priv->bg_cache) == height)
    return pager->priv->bg_cache;

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  if (pager->priv->screen == NULL)
    return NULL;

  /* FIXME this just globally disables the thumbnailing feature */
  return NULL;

#define MIN_BG_SIZE 10

  if (width < MIN_BG_SIZE || height < MIN_BG_SIZE)
    return NULL;

  p = wnck_screen_get_background_pixmap (pager->priv->screen);

  if (p != None)
    {
      Screen *xscreen;

      xscreen = WNCK_SCREEN_XSCREEN (pager->priv->screen);
      pix = _wnck_gdk_pixbuf_get_from_pixmap (xscreen, p);
    }

  if (pix)
    {
      pager->priv->bg_cache = gdk_pixbuf_scale_simple (pix,
                                                       width,
                                                       height,
                                                       GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (pix));
    }

  return pager->priv->bg_cache;
}

/*
 *This will return aobj_pager whose parent is wnck's atk object -Gail Container
 */
static AtkObject *
wnck_pager_get_accessible (GtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (first_time)
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type;
      GType derived_atk_type;

      /*
       * Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for
       * the parent type WnckPager.
       */
      derived_type = g_type_parent (WNCK_TYPE_PAGER);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry,
                                          derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);

      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
        {
          /*
           * Specify what factory to use to create accessible
           * objects
           */
          atk_registry_set_factory_type (registry,
                                         WNCK_TYPE_PAGER,
                                         WNCK_TYPE_PAGER_ACCESSIBLE_FACTORY);

          atk_registry_set_factory_type (registry,
                                         WNCK_TYPE_WORKSPACE,
                                         WNCK_TYPE_WORKSPACE_ACCESSIBLE_FACTORY);
        }
      first_time = FALSE;
    }
  return GTK_WIDGET_CLASS (wnck_pager_parent_class)->get_accessible (widget);
}

int
_wnck_pager_get_n_workspaces (WnckPager *pager)
{
  return wnck_screen_get_workspace_count (pager->priv->screen);
}

const char*
_wnck_pager_get_workspace_name (WnckPager *pager,
                                int        i)
{
  WnckWorkspace *space;

  space = wnck_screen_get_workspace (pager->priv->screen, i);
  if (space)
    return wnck_workspace_get_name (space);
  else
    return NULL;
}

WnckWorkspace*
_wnck_pager_get_active_workspace (WnckPager *pager)
{
  return wnck_screen_get_active_workspace (pager->priv->screen);
}

WnckWorkspace*
_wnck_pager_get_workspace (WnckPager *pager,
                           int        i)
{
  return wnck_screen_get_workspace (pager->priv->screen, i);
}

void
_wnck_pager_activate_workspace (WnckWorkspace *wspace,
                                guint32        timestamp)
{
  wnck_workspace_activate (wspace, timestamp);
}

void
_wnck_pager_get_workspace_rect (WnckPager    *pager,
                                int           i,
                                GdkRectangle *rect)
{
  get_workspace_rect (pager, i, rect);
}
