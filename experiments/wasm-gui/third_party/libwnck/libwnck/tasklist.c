/* tasklist object */
/* vim: set sw=2 et: */

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
#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include "tasklist.h"
#include "window.h"
#include "class-group.h"
#include "window-action-menu.h"
#include "workspace.h"
#include "xutils.h"
#include "private.h"

/**
 * SECTION:tasklist
 * @short_description: a tasklist widget, showing the list of windows as a list
 * of buttons.
 * @see_also: #WnckScreen, #WnckSelector
 * @stability: Unstable
 *
 * The #WnckTasklist represents client windows on a screen as a list of buttons
 * labelled with the window titles and icons. Pressing a button can activate or
 * minimize the represented window, and other typical actions are available
 * through a popup menu. Windows needing attention can also be distinguished
 * by a fade effect on the buttons representing them, to help attract the
 * user's attention.
 *
 * The behavior of the #WnckTasklist can be customized in various ways, like
 * grouping multiple windows of the same application in one button (see
 * wnck_tasklist_set_grouping() and wnck_tasklist_set_grouping_limit()), or
 * showing windows from all workspaces (see
 * wnck_tasklist_set_include_all_workspaces()). The fade effect for windows
 * needing attention can be controlled by various style properties like
 * #WnckTasklist:fade-max-loops and #WnckTasklist:fade-opacity.
 *
 * The tasklist also acts as iconification destination. If there are multiple
 * #WnckTasklist or other applications setting the iconification destination
 * for windows, the iconification destinations might not be consistent among
 * windows and it is not possible to determine which #WnckTasklist (or which
 * other application) owns this propriety.
 */

/* TODO:
 *
 *  Add total focused time to the grouping score function
 *  Fine tune the grouping scoring function
 *  Fix "changes" to icon for groups/applications
 *  Maybe fine tune size_allocate() some more...
 *  Better vertical layout handling
 *  prefs
 *  support for right-click menu merging w/ bonobo for the applet
 *
 */


#define WNCK_TYPE_TASK              (wnck_task_get_type ())
#define WNCK_TASK(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_TASK, WnckTask))
#define WNCK_TASK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_TASK, WnckTaskClass))
#define WNCK_IS_TASK(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_TASK))
#define WNCK_IS_TASK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_TASK))
#define WNCK_TASK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_TASK, WnckTaskClass))

typedef struct _WnckTask        WnckTask;
typedef struct _WnckTaskClass   WnckTaskClass;

#define DEFAULT_GROUPING_LIMIT 80

#define MINI_ICON_SIZE _wnck_get_default_mini_icon_size ()
#define TASKLIST_BUTTON_PADDING 4
#define TASKLIST_TEXT_MAX_WIDTH 25 /* maximum width in characters */

#define N_SCREEN_CONNECTIONS 5

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

typedef enum
{
  WNCK_TASK_CLASS_GROUP,
  WNCK_TASK_WINDOW,
  WNCK_TASK_STARTUP_SEQUENCE
} WnckTaskType;

struct _WnckTask
{
  GObject parent_instance;

  WnckTasklist *tasklist;

  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;

  WnckTaskType type;

  WnckClassGroup *class_group;
  WnckWindow *window;
#ifdef HAVE_STARTUP_NOTIFICATION
  SnStartupSequence *startup_sequence;
#endif

  gdouble grouping_score;

  GList *windows; /* List of the WnckTask for the window,
		     if this is a class group */
  guint state_changed_tag;
  guint icon_changed_tag;
  guint name_changed_tag;
  guint class_name_changed_tag;
  guint class_icon_changed_tag;

  /* task menu */
  GtkWidget *menu;
  /* ops menu */
  GtkWidget *action_menu;

  guint really_toggling : 1; /* Set when tasklist really wants
                              * to change the togglebutton state
                              */
  guint was_active : 1;      /* used to fixup activation behavior */

  guint button_activate;

  guint32 dnd_timestamp;

  time_t  start_needs_attention;
  gdouble glow_start_time;
  gdouble glow_factor;

  guint button_glow;

  guint row;
  guint col;
};

struct _WnckTaskClass
{
  GObjectClass parent_class;
};

typedef struct _skipped_window
{
  WnckWindow *window;
  gulong tag;
} skipped_window;

struct _WnckTasklistPrivate
{
  WnckScreen *screen;

  WnckTask *active_task; /* NULL if active window not in tasklist */
  WnckTask *active_class_group; /* NULL if active window not in tasklist */

  gboolean include_all_workspaces;

  /* Calculated by update_lists */
  GList *class_groups;
  GList *windows;
  GList *windows_without_class_group;

  /* Not handled by update_lists */
  GList *startup_sequences;

  /* windows with _NET_WM_STATE_SKIP_TASKBAR set; connected to
   * "state_changed" signal, but excluded from tasklist.
   */
  GList *skipped_windows;

  GHashTable *class_group_hash;
  GHashTable *win_hash;

  gint max_button_width;
  gint max_button_height;

  gboolean switch_workspace_on_unminimize;
  gboolean middle_click_close;

  WnckTasklistGroupingType grouping;
  gint grouping_limit;

  guint activate_timeout_id;
  guint screen_connections [N_SCREEN_CONNECTIONS];

  guint idle_callback_tag;

  int *size_hints;
  int size_hints_len;

  WnckLoadIconFunction icon_loader;
  void *icon_loader_data;
  GDestroyNotify free_icon_loader_data;

#ifdef HAVE_STARTUP_NOTIFICATION
  SnMonitorContext *sn_context;
  guint startup_sequence_timeout;
#endif

  gint monitor_num;
  GdkRectangle monitor_geometry;
  GtkReliefStyle relief;
  GtkOrientation orientation;

  guint drag_start_time;
};

static GType wnck_task_get_type (void);

G_DEFINE_TYPE (WnckTask, wnck_task, G_TYPE_OBJECT);
G_DEFINE_TYPE (WnckTasklist, wnck_tasklist, GTK_TYPE_CONTAINER);
#define WNCK_TASKLIST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_TASKLIST, WnckTasklistPrivate))

static void wnck_task_init        (WnckTask      *task);
static void wnck_task_class_init  (WnckTaskClass *klass);
static void wnck_task_finalize    (GObject       *object);

static void wnck_task_stop_glow   (WnckTask *task);

static WnckTask *wnck_task_new_from_window      (WnckTasklist    *tasklist,
						 WnckWindow      *window);
static WnckTask *wnck_task_new_from_class_group (WnckTasklist    *tasklist,
						 WnckClassGroup  *class_group);
#ifdef HAVE_STARTUP_NOTIFICATION
static WnckTask *wnck_task_new_from_startup_sequence (WnckTasklist      *tasklist,
                                                      SnStartupSequence *sequence);
#endif
static gboolean wnck_task_get_needs_attention (WnckTask *task);


static char      *wnck_task_get_text (WnckTask *task,
                                      gboolean  icon_text,
                                      gboolean  include_state);
static GdkPixbuf *wnck_task_get_icon (WnckTask *task);
static gint       wnck_task_compare_alphabetically (gconstpointer  a,
                                                    gconstpointer  b);
static gint       wnck_task_compare  (gconstpointer  a,
				      gconstpointer  b);
static void       wnck_task_update_visible_state (WnckTask *task);
static void       wnck_task_state_changed        (WnckWindow      *window,
                                                  WnckWindowState  changed_mask,
                                                  WnckWindowState  new_state,
                                                  gpointer         data);

static void       wnck_task_drag_begin    (GtkWidget          *widget,
                                           GdkDragContext     *context,
                                           WnckTask           *task);
static void       wnck_task_drag_end      (GtkWidget          *widget,
                                           GdkDragContext     *context,
                                           WnckTask           *task);
static void       wnck_task_drag_data_get (GtkWidget          *widget,
                                           GdkDragContext     *context,
                                           GtkSelectionData   *selection_data,
                                           guint               info,
                                           guint               time,
                                           WnckTask           *task);

static void     wnck_tasklist_init          (WnckTasklist      *tasklist);
static void     wnck_tasklist_class_init    (WnckTasklistClass *klass);
static void     wnck_tasklist_finalize      (GObject        *object);

static void     wnck_tasklist_get_preferred_width (GtkWidget *widget,
                                                   int       *minimum_width,
                                                   int       *natural_width);
static void     wnck_tasklist_get_preferred_height (GtkWidget *widget,
                                                    int       *minimum_height,
                                                    int       *natural_height);
static void     wnck_tasklist_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static void     wnck_tasklist_realize       (GtkWidget        *widget);
static void     wnck_tasklist_unrealize     (GtkWidget        *widget);
static gboolean wnck_tasklist_scroll_event  (GtkWidget        *widget,
                                             GdkEventScroll   *event);
static void     wnck_tasklist_forall        (GtkContainer     *container,
                                             gboolean	       include_internals,
                                             GtkCallback       callback,
                                             gpointer          callback_data);
static void     wnck_tasklist_remove	    (GtkContainer   *container,
					     GtkWidget	    *widget);
static void     wnck_tasklist_free_tasks    (WnckTasklist   *tasklist);
static void     wnck_tasklist_update_lists  (WnckTasklist   *tasklist);
static int      wnck_tasklist_layout        (GtkAllocation  *allocation,
					     int             max_width,
					     int             max_height,
					     int             n_buttons,
                                             GtkOrientation  orientation,
					     int            *n_cols_out,
					     int            *n_rows_out);

static void     wnck_tasklist_active_window_changed    (WnckScreen   *screen,
                                                        WnckWindow   *previous_window,
							WnckTasklist *tasklist);
static void     wnck_tasklist_active_workspace_changed (WnckScreen   *screen,
                                                        WnckWorkspace *previous_workspace,
							WnckTasklist *tasklist);
static void     wnck_tasklist_window_added             (WnckScreen   *screen,
							WnckWindow   *win,
							WnckTasklist *tasklist);
static void     wnck_tasklist_window_removed           (WnckScreen   *screen,
							WnckWindow   *win,
							WnckTasklist *tasklist);
static void     wnck_tasklist_viewports_changed        (WnckScreen   *screen,
							WnckTasklist *tasklist);
static void     wnck_tasklist_connect_window           (WnckTasklist *tasklist,
							WnckWindow   *window);
static void     wnck_tasklist_disconnect_window        (WnckTasklist *tasklist,
							WnckWindow   *window);

static void     wnck_tasklist_change_active_task       (WnckTasklist *tasklist,
							WnckTask *active_task);
static gboolean wnck_tasklist_change_active_timeout    (gpointer data);
static void     wnck_tasklist_activate_task_window     (WnckTask *task,
                                                        guint32   timestamp);

static void     wnck_tasklist_update_icon_geometries   (WnckTasklist *tasklist,
							GList        *visible_tasks);
static void     wnck_tasklist_connect_screen           (WnckTasklist *tasklist);
static void     wnck_tasklist_disconnect_screen        (WnckTasklist *tasklist);

#ifdef HAVE_STARTUP_NOTIFICATION
static void     wnck_tasklist_sn_event                 (SnMonitorEvent *event,
                                                        void           *user_data);
static void     wnck_tasklist_check_end_sequence       (WnckTasklist   *tasklist,
                                                        WnckWindow     *window);
#endif


/*
 * Keep track of all tasklist instances so we can decide
 * whether to show windows from all monitors in the
 * tasklist
 */
static GSList *tasklist_instances;

static GType
wnck_task_get_type (void) G_GNUC_CONST;

static void
wnck_task_init (WnckTask *task)
{
  task->type = WNCK_TASK_WINDOW;
}

static void
wnck_task_class_init (WnckTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = wnck_task_finalize;
}

static gboolean
wnck_task_button_glow (WnckTask *task)
{
  GTimeVal tv;
  gdouble now;
  gfloat fade_opacity, loop_time;
  gint fade_max_loops;
  gboolean stopped;

  g_get_current_time (&tv);
  now = (tv.tv_sec * (1.0 * G_USEC_PER_SEC) +
        tv.tv_usec) / G_USEC_PER_SEC;

  if (task->glow_start_time <= G_MINDOUBLE)
    task->glow_start_time = now;

  gtk_widget_style_get (GTK_WIDGET (task->tasklist), "fade-opacity", &fade_opacity,
                                                     "fade-loop-time", &loop_time,
                                                     "fade-max-loops", &fade_max_loops,
                                                     NULL);

  if (task->button_glow == 0)
    {
      /* we're in "has stopped glowing" mode */
      task->glow_factor = fade_opacity * 0.5;
      stopped = TRUE;
    }
  else
    {
      task->glow_factor = fade_opacity * (0.5 -
                                          0.5 * cos ((now - task->glow_start_time) *
                                                     M_PI * 2.0 / loop_time));

      if (now - task->start_needs_attention > loop_time * 1.0 * fade_max_loops)
        stopped = ABS (task->glow_factor - fade_opacity * 0.5) < 0.05;
      else
        stopped = FALSE;
    }

  gtk_widget_queue_draw (task->button);

  if (stopped)
    wnck_task_stop_glow (task);

  return !stopped;
}

static void
wnck_task_clear_glow_start_timeout_id (WnckTask *task)
{
  task->button_glow = 0;
}

static void
wnck_task_queue_glow (WnckTask *task)
{
  if (task->button_glow == 0)
    {
      task->glow_start_time = 0.0;

      /* The animation doesn't speed up or slow down based on the
       * timeout value, but instead will just appear smoother or
       * choppier.
       */
      task->button_glow =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                            50,
                            (GSourceFunc) wnck_task_button_glow, task,
                            (GDestroyNotify) wnck_task_clear_glow_start_timeout_id);
    }
}

static void
wnck_task_stop_glow (WnckTask *task)
{
  /* We stop glowing, but we might still have the task colored,
   * so we don't reset the glow factor */
  if (task->button_glow != 0)
    g_source_remove (task->button_glow);
}

static void
wnck_task_reset_glow (WnckTask *task)
{
  wnck_task_stop_glow (task);
  task->glow_factor = 0.0;
}

static void
wnck_task_finalize (GObject *object)
{
  WnckTask *task;

  task = WNCK_TASK (object);

  if (task->tasklist->priv->active_task == task)
    wnck_tasklist_change_active_task (task->tasklist, NULL);

  if (task->button)
    {
      g_object_remove_weak_pointer (G_OBJECT (task->button),
                                    (void**) &task->button);
      gtk_widget_destroy (task->button);
      task->button = NULL;
      task->image = NULL;
      task->label = NULL;
    }

#ifdef HAVE_STARTUP_NOTIFICATION
  if (task->startup_sequence)
    {
      sn_startup_sequence_unref (task->startup_sequence);
      task->startup_sequence = NULL;
    }
#endif

  g_list_free (task->windows);
  task->windows = NULL;

  if (task->state_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->window,
				   task->state_changed_tag);
      task->state_changed_tag = 0;
    }

  if (task->icon_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->window,
				   task->icon_changed_tag);
      task->icon_changed_tag = 0;
    }

  if (task->name_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->window,
				   task->name_changed_tag);
      task->name_changed_tag = 0;
    }

  if (task->class_name_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->class_group,
				   task->class_name_changed_tag);
      task->class_name_changed_tag = 0;
    }

  if (task->class_icon_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->class_group,
				   task->class_icon_changed_tag);
      task->class_icon_changed_tag = 0;
    }

  if (task->class_group)
    {
      g_object_unref (task->class_group);
      task->class_group = NULL;
    }

  if (task->window)
    {
      g_object_unref (task->window);
      task->window = NULL;
    }

  if (task->menu)
    {
      gtk_widget_destroy (task->menu);
      task->menu = NULL;
    }

  if (task->action_menu)
    {
      g_object_remove_weak_pointer (G_OBJECT (task->action_menu),
                                    (void**) &task->action_menu);
      gtk_widget_destroy (task->action_menu);
      task->action_menu = NULL;
    }

  if (task->button_activate != 0)
    {
      g_source_remove (task->button_activate);
      task->button_activate = 0;
    }

  wnck_task_stop_glow (task);

  G_OBJECT_CLASS (wnck_task_parent_class)->finalize (object);
}

static void
wnck_tasklist_init (WnckTasklist *tasklist)
{
  GtkWidget *widget;
  AtkObject *atk_obj;

  widget = GTK_WIDGET (tasklist);

  gtk_widget_set_has_window (widget, FALSE);

  tasklist->priv = WNCK_TASKLIST_GET_PRIVATE (tasklist);

  tasklist->priv->class_group_hash = g_hash_table_new (NULL, NULL);
  tasklist->priv->win_hash = g_hash_table_new (NULL, NULL);

  tasklist->priv->grouping = WNCK_TASKLIST_AUTO_GROUP;
  tasklist->priv->grouping_limit = DEFAULT_GROUPING_LIMIT;

  tasklist->priv->monitor_num = -1;
  tasklist->priv->monitor_geometry.width = -1; /* invalid value */
  tasklist->priv->relief = GTK_RELIEF_NORMAL;
  tasklist->priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  atk_obj = gtk_widget_get_accessible (widget);
  atk_object_set_name (atk_obj, _("Window List"));
  atk_object_set_description (atk_obj, _("Tool to switch between visible windows"));

#if 0
  /* This doesn't work because, and I think this is because we have no window;
   * therefore, we use the scroll events on task buttons instead */
  gtk_widget_add_events (widget, GDK_SCROLL_MASK);
#endif
}

static void
wnck_tasklist_class_init (WnckTasklistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WnckTasklistPrivate));

  object_class->finalize = wnck_tasklist_finalize;

  widget_class->get_preferred_width = wnck_tasklist_get_preferred_width;
  widget_class->get_preferred_height = wnck_tasklist_get_preferred_height;
  widget_class->size_allocate = wnck_tasklist_size_allocate;
  widget_class->realize = wnck_tasklist_realize;
  widget_class->unrealize = wnck_tasklist_unrealize;
#if 0
  /* See comment above gtk_widget_add_events() in wnck_tasklist_init() */
  widget_class->scroll_event = wnck_tasklist_scroll_event;
#endif

  container_class->forall = wnck_tasklist_forall;
  container_class->remove = wnck_tasklist_remove;

  /**
   * WnckTasklist:fade-loop-time:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. This property controls the time one loop of this
   * fade effect takes, in seconds.
   *
   * Since: 2.16
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("fade-loop-time",
                                                              "Loop time",
                                                              "The time one loop takes when fading, in seconds. Default: 3.0",
                                                              0.2, 10.0, 3.0,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * WnckTasklist:fade-max-loops:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. This property controls the number of loops for
   * this fade effect. 0 means the button will only fade to the final color.
   *
   * Since: 2.20
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("fade-max-loops",
                                                              "Maximum number of loops",
                                                              "The number of fading loops. 0 means the button will only fade to the final color. Default: 5",
                                                              0, 50, 5,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * WnckTasklist:fade-overlay-rect:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. Set this property to %TRUE to enable a
   * compatibility mode for pixbuf engine themes that cannot react to color
   * changes. If enabled, a rectangle with the correct color will be drawn on
   * top of the button.
   *
   * Since: 2.16
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("fade-overlay-rect",
                                                                 "Overlay a rectangle, instead of modifying the background.",
                                                                 "Compatibility mode for pixbuf engine themes that cannot react to color changes. If enabled, a rectangle with the correct color will be drawn on top of the button. Default: TRUE",
                                                                 TRUE,
                                                                 G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * WnckTasklist:fade-opacity:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. This property controls the final opacity that
   * will be reached by the fade effect.
   *
   * Since: 2.16
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("fade-opacity",
                                                              "Final opacity",
                                                              "The final opacity that will be reached. Default: 0.8",
                                                              0.0, 1.0, 0.8,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
wnck_tasklist_free_skipped_windows (WnckTasklist  *tasklist)
{
  GList *l;

  l = tasklist->priv->skipped_windows;

  while (l != NULL)
    {
      skipped_window *skipped = (skipped_window*) l->data;
      g_signal_handler_disconnect (skipped->window, skipped->tag);
      g_object_unref (skipped->window);
      g_free (skipped);
      l = l->next;
    }

  g_list_free (tasklist->priv->skipped_windows);
}

static void
wnck_tasklist_finalize (GObject *object)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (object);

  /* Tasks should have gone away due to removing their
   * buttons in container destruction
   */
  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (tasklist->priv->windows == NULL);
  g_assert (tasklist->priv->windows_without_class_group == NULL);
  g_assert (tasklist->priv->startup_sequences == NULL);
  /* wnck_tasklist_free_tasks (tasklist); */

  if (tasklist->priv->skipped_windows)
    {
      wnck_tasklist_free_skipped_windows (tasklist);
      tasklist->priv->skipped_windows = NULL;
    }

  g_hash_table_destroy (tasklist->priv->class_group_hash);
  tasklist->priv->class_group_hash = NULL;

  g_hash_table_destroy (tasklist->priv->win_hash);
  tasklist->priv->win_hash = NULL;

  if (tasklist->priv->activate_timeout_id != 0)
    {
      g_source_remove (tasklist->priv->activate_timeout_id);
      tasklist->priv->activate_timeout_id = 0;
    }

  if (tasklist->priv->idle_callback_tag != 0)
    {
      g_source_remove (tasklist->priv->idle_callback_tag);
      tasklist->priv->idle_callback_tag = 0;
    }

  g_free (tasklist->priv->size_hints);
  tasklist->priv->size_hints = NULL;
  tasklist->priv->size_hints_len = 0;

  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);
  tasklist->priv->free_icon_loader_data = NULL;
  tasklist->priv->icon_loader_data = NULL;

  G_OBJECT_CLASS (wnck_tasklist_parent_class)->finalize (object);
}

/**
 * wnck_tasklist_set_grouping:
 * @tasklist: a #WnckTasklist.
 * @grouping: a grouping policy.
 *
 * Sets the grouping policy for @tasklist to @grouping.
 */
void
wnck_tasklist_set_grouping (WnckTasklist            *tasklist,
			    WnckTasklistGroupingType grouping)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->grouping == grouping)
    return;

  tasklist->priv->grouping = grouping;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_set_relief_callback (WnckWindow   *win,
				   WnckTask     *task,
				   WnckTasklist *tasklist)
{
  gtk_button_set_relief (GTK_BUTTON (task->button), tasklist->priv->relief);
}

/**
 * wnck_tasklist_set_button_relief:
 * @tasklist: a #WnckTasklist.
 * @relief: a relief type.
 *
 * Sets the relief type of the buttons in @tasklist to @relief. The main use of
 * this function is proper integration of #WnckTasklist in panels with
 * non-system backgrounds.
 *
 * Since: 2.12
 */
void
wnck_tasklist_set_button_relief (WnckTasklist *tasklist, GtkReliefStyle relief)
{
  GList *walk;

  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (relief == tasklist->priv->relief)
    return;

  tasklist->priv->relief = relief;

  g_hash_table_foreach (tasklist->priv->win_hash,
                        (GHFunc) wnck_tasklist_set_relief_callback,
                        tasklist);
  for (walk = tasklist->priv->class_groups; walk; walk = g_list_next (walk))
    gtk_button_set_relief (GTK_BUTTON (WNCK_TASK (walk->data)->button), relief);
}

/**
 * wnck_tasklist_set_middle_click_close:
 * @tasklist: a #WnckTasklist.
 * @middle_click_close: whether to close windows with middle click on
 * button.
 *
 * Sets @tasklist to close windows with mouse middle click on button,
 * according to @middle_click_close.
 *
 * Since: 3.4.6
 */
void
wnck_tasklist_set_middle_click_close (WnckTasklist  *tasklist,
				      gboolean       middle_click_close)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  tasklist->priv->middle_click_close = middle_click_close;
}

/**
 * wnck_tasklist_set_orientation:
 * @tasklist: a #WnckTasklist.
 * @orient: a GtkOrientation.
 *
 * Set the orientation of the @tasklist to match @orient.
 * This function can be used to integrate a #WnckTasklist in vertical panels.
 *
 * Since: 3.4.6
 */
void wnck_tasklist_set_orientation (WnckTasklist *tasklist,
				    GtkOrientation orient)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  tasklist->priv->orientation = orient;
}

/**
 * wnck_tasklist_set_switch_workspace_on_unminimize:
 * @tasklist: a #WnckTasklist.
 * @switch_workspace_on_unminimize: whether to activate the #WnckWorkspace a
 * #WnckWindow is on when unminimizing it.
 *
 * Sets @tasklist to activate or not the #WnckWorkspace a #WnckWindow is on
 * when unminimizing it, according to @switch_workspace_on_unminimize.
 *
 * FIXME: does it still work?
 */
void
wnck_tasklist_set_switch_workspace_on_unminimize (WnckTasklist  *tasklist,
						  gboolean       switch_workspace_on_unminimize)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  tasklist->priv->switch_workspace_on_unminimize = switch_workspace_on_unminimize;
}

/**
 * wnck_tasklist_set_include_all_workspaces:
 * @tasklist: a #WnckTasklist.
 * @include_all_workspaces: whether to display #WnckWindow from all
 * #WnckWorkspace in @tasklist.
 *
 * Sets @tasklist to display #WnckWindow from all #WnckWorkspace or not,
 * according to @include_all_workspaces.
 *
 * Note that if the active #WnckWorkspace has a viewport and if
 * @include_all_workspaces is %FALSE, then only the #WnckWindow visible in the
 * viewport are displayed in @tasklist. The rationale for this is that the
 * viewport is generally used to implement workspace-like behavior. A
 * side-effect of this is that, when using multiple #WnckWorkspace with
 * viewport, it is not possible to show all #WnckWindow from a #WnckWorkspace
 * (even those that are not visible in the viewport) in @tasklist without
 * showing all #WnckWindow from all #WnckWorkspace.
 */
void
wnck_tasklist_set_include_all_workspaces (WnckTasklist *tasklist,
					  gboolean      include_all_workspaces)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  include_all_workspaces = (include_all_workspaces != 0);

  if (tasklist->priv->include_all_workspaces == include_all_workspaces)
    return;

  tasklist->priv->include_all_workspaces = include_all_workspaces;
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

/**
 * wnck_tasklist_set_grouping_limit:
 * @tasklist: a #WnckTasklist.
 * @limit: a size in pixels.
 *
 * Sets the maximum size of buttons in @tasklist before @tasklist tries to
 * group #WnckWindow in the same #WnckApplication in only one button. This
 * limit is valid only when the grouping policy of @tasklist is
 * %WNCK_TASKLIST_AUTO_GROUP.
 */
void
wnck_tasklist_set_grouping_limit (WnckTasklist *tasklist,
				  gint          limit)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->grouping_limit == limit)
    return;

  tasklist->priv->grouping_limit = limit;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

/**
 * wnck_tasklist_set_icon_loader:
 * @tasklist: a #WnckTasklist
 * @load_icon_func: icon loader function
 * @data: data for icon loader function
 * @free_data_func: function to free the data
 *
 * Sets a function to be used for loading icons.
 *
 * Since: 2.2
 **/
void
wnck_tasklist_set_icon_loader (WnckTasklist         *tasklist,
                               WnckLoadIconFunction  load_icon_func,
                               void                 *data,
                               GDestroyNotify        free_data_func)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);

  tasklist->priv->icon_loader = load_icon_func;
  tasklist->priv->icon_loader_data = data;
  tasklist->priv->free_icon_loader_data = free_data_func;
}

/* returns the maximal possible button width (i.e. if you
 * don't want to stretch the buttons to fill the alloctions
 * the width can be smaller) */
static int
wnck_tasklist_layout (GtkAllocation *allocation,
		      int            max_width,
		      int            max_height,
		      int            n_buttons,
		      GtkOrientation orientation,
		      int           *n_cols_out,
		      int           *n_rows_out)
{
  int n_cols, n_rows;

  if (n_buttons == 0)
    {
      *n_cols_out = 0;
      *n_rows_out = 0;
      return 0;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* How many rows fit in the allocation */
      n_rows = allocation->height / max_height;

      /* Don't have more rows than buttons */
      n_rows = MIN (n_rows, n_buttons);

      /* At least one row */
      n_rows = MAX (n_rows, 1);

      /* We want to use as many cols as possible to limit the width */
      n_cols = (n_buttons + n_rows - 1) / n_rows;

      /* At least one column */
      n_cols = MAX (n_cols, 1);
    }
  else
    {
      /* How many cols fit in the allocation */
      n_cols = allocation->width / max_width;

      /* Don't have more cols than buttons */
      n_cols = MIN (n_cols, n_buttons);

      /* At least one col */
      n_cols = MAX (n_cols, 1);

      /* We want to use as many rows as possible to limit the height */
      n_rows = (n_buttons + n_cols - 1) / n_cols;

      /* At least one row */
      n_rows = MAX (n_rows, 1);
    }

  *n_cols_out = n_cols;
  *n_rows_out = n_rows;

  return allocation->width / n_cols;
}

static void
wnck_tasklist_score_groups (WnckTasklist *tasklist,
			    GList        *ungrouped_class_groups)
{
  WnckTask *class_group_task;
  WnckTask *win_task;
  GList *l, *w;
  const char *first_name = NULL;
  int n_windows;
  int n_same_title;
  double same_window_ratio;

  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = WNCK_TASK (l->data);

      n_windows = g_list_length (class_group_task->windows);

      n_same_title = 0;
      w = class_group_task->windows;
      while (w != NULL)
	{
	  win_task = WNCK_TASK (w->data);

	  if (first_name == NULL)
	    {
              if (wnck_window_has_icon_name (win_task->window))
                first_name = wnck_window_get_icon_name (win_task->window);
              else
                first_name = wnck_window_get_name (win_task->window);
	      n_same_title++;
	    }
	  else
	    {
              const char *name;

              if (wnck_window_has_icon_name (win_task->window))
                name = wnck_window_get_icon_name (win_task->window);
              else
                name = wnck_window_get_name (win_task->window);

	      if (strcmp (name, first_name) == 0)
		n_same_title++;
	    }

	  w = w->next;
	}
      same_window_ratio = (double)n_same_title/(double)n_windows;

      /* FIXME: This is fairly bogus and should be researched more.
       *        XP groups by least used, so we probably want to add
       *        total focused time to this expression.
       */
      class_group_task->grouping_score = -same_window_ratio * 5 + n_windows;

      l = l->next;
    }
}

static GList *
wnck_task_get_highest_scored (GList     *ungrouped_class_groups,
			      WnckTask **class_group_task_out)
{
  WnckTask *class_group_task;
  WnckTask *best_task = NULL;
  double max_score = -1000000000.0; /* Large negative score */
  GList *l;

  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = WNCK_TASK (l->data);

      if (class_group_task->grouping_score >= max_score)
	{
	  max_score = class_group_task->grouping_score;
	  best_task = class_group_task;
	}

      l = l->next;
    }

  *class_group_task_out = best_task;

  return g_list_remove (ungrouped_class_groups, best_task);
}

static int
wnck_tasklist_get_button_size (GtkWidget *widget)
{
  GtkStyleContext *style_context;
  GtkStateFlags state;
  PangoContext *context;
  PangoFontMetrics *metrics;
  PangoFontDescription *description;
  gint char_width;
  gint text_width;
  gint width;

  style_context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get (style_context, state, GTK_STYLE_PROPERTY_FONT, &description, NULL);

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context, description,
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);
  text_width = PANGO_PIXELS (TASKLIST_TEXT_MAX_WIDTH * char_width);

  width = text_width + 2 * TASKLIST_BUTTON_PADDING
	  + MINI_ICON_SIZE + 2 * TASKLIST_BUTTON_PADDING;

  return width;
}

static void
wnck_tasklist_size_request  (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  WnckTasklist *tasklist;
  GtkRequisition child_req;
  GtkAllocation  tasklist_allocation;
  GtkAllocation  fake_allocation;
  int max_height = 1;
  int max_width = 1;
  /* int u_width, u_height; */
  GList *l;
  GArray *array;
  GList *ungrouped_class_groups;
  int n_windows;
  int n_startup_sequences;
  int n_rows;
  int n_cols, last_n_cols;
  int n_grouped_buttons;
  gboolean score_set;
  int val;
  WnckTask *class_group_task;
  int lowest_range;
  int grouping_limit;

  tasklist = WNCK_TASKLIST (widget);

  /* Calculate max needed height and width of the buttons */
#define GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS(list)                 \
  l = list;                                                     \
  while (l != NULL)                                             \
    {                                                           \
      WnckTask *task = WNCK_TASK (l->data);                     \
                                                                \
      gtk_widget_get_preferred_size (task->button,              \
                                     &child_req, NULL);         \
                                                                \
      max_height = MAX (child_req.height,                       \
			max_height);                            \
      max_width = MAX (child_req.width,                         \
		       max_width);                              \
                                                                \
      l = l->next;                                              \
    }

  GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS (tasklist->priv->windows)
  GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS (tasklist->priv->class_groups)
  GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS (tasklist->priv->startup_sequences)

  /* Note that the fact that we nearly don't care about the width/height
   * requested by the buttons makes it possible to hide/show the label/image
   * in wnck_task_size_allocated(). If we really cared about those, this
   * wouldn't work since our call to gtk_widget_size_request() does not take
   * into account the hidden widgets.
   */
  tasklist->priv->max_button_width = wnck_tasklist_get_button_size (widget);
  tasklist->priv->max_button_height = max_height;

  gtk_widget_get_allocation (GTK_WIDGET (tasklist), &tasklist_allocation);

  fake_allocation.width = tasklist_allocation.width;
  fake_allocation.height = tasklist_allocation.height;

  array = g_array_new (FALSE, FALSE, sizeof (int));

  /* Calculate size_hints list */

  n_windows = g_list_length (tasklist->priv->windows);
  n_startup_sequences = g_list_length (tasklist->priv->startup_sequences);
  n_grouped_buttons = 0;
  ungrouped_class_groups = g_list_copy (tasklist->priv->class_groups);
  score_set = FALSE;

  grouping_limit = MIN (tasklist->priv->grouping_limit,
			tasklist->priv->max_button_width);

  /* Try ungrouped mode */
  wnck_tasklist_layout (&fake_allocation,
			tasklist->priv->max_button_width,
			tasklist->priv->max_button_height,
			n_windows + n_startup_sequences,
			tasklist->priv->orientation,
			&n_cols, &n_rows);

  last_n_cols = G_MAXINT;
  lowest_range = G_MAXINT;
  if (tasklist->priv->grouping != WNCK_TASKLIST_ALWAYS_GROUP)
    {
      if (tasklist->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          val = n_cols * tasklist->priv->max_button_width;
          g_array_insert_val (array, array->len, val);
          val = n_cols * grouping_limit;
          g_array_insert_val (array, array->len, val);

          last_n_cols = n_cols;
          lowest_range = val;
        }
      else
        {
          val = n_rows * tasklist->priv->max_button_height;
          g_array_insert_val (array, array->len, val);
          val = n_rows * grouping_limit;
          g_array_insert_val (array, array->len, val);

          last_n_cols = n_rows;
          lowest_range = val;
        }
    }

  while (ungrouped_class_groups != NULL &&
	 tasklist->priv->grouping != WNCK_TASKLIST_NEVER_GROUP)
    {
      if (!score_set)
        {
          wnck_tasklist_score_groups (tasklist, ungrouped_class_groups);
          score_set = TRUE;
        }

      ungrouped_class_groups = wnck_task_get_highest_scored (ungrouped_class_groups, &class_group_task);

      n_grouped_buttons += g_list_length (class_group_task->windows) - 1;

      wnck_tasklist_layout (&fake_allocation,
			    tasklist->priv->max_button_width,
			    tasklist->priv->max_button_height,
			    n_startup_sequences + n_windows - n_grouped_buttons,
			    tasklist->priv->orientation,
			    &n_cols, &n_rows);

      if (tasklist->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (n_cols != last_n_cols &&
              (tasklist->priv->grouping == WNCK_TASKLIST_AUTO_GROUP ||
               ungrouped_class_groups == NULL))
            {
              val = n_cols * tasklist->priv->max_button_width;
              if (val >= lowest_range)
                {
                  /* Overlaps old range */
                  g_assert (array->len > 0);
                  lowest_range = n_cols * grouping_limit;
                  g_array_index(array, int, array->len-1) = lowest_range;
                }
              else
                {
                  /* Full new range */
                  g_array_insert_val (array, array->len, val);
                  val = n_cols * grouping_limit;
                  g_array_insert_val (array, array->len, val);
                  lowest_range = val;
                }

              last_n_cols = n_cols;
            }
        }
      else
        {
          if (n_rows != last_n_cols &&
              (tasklist->priv->grouping == WNCK_TASKLIST_AUTO_GROUP ||
               ungrouped_class_groups == NULL))
            {
              val = n_rows * tasklist->priv->max_button_height;
              if (val >= lowest_range)
                {
                  /* Overlaps old range */
                  g_assert (array->len > 0);
                  lowest_range = n_rows * grouping_limit;
                  g_array_index (array, int, array->len-1) = lowest_range;
                }
              else
                {
                  /* Full new range */
                  g_array_insert_val (array, array->len, val);
                  val = n_rows * grouping_limit;
                  g_array_insert_val (array, array->len, val);
                  lowest_range = val;
                }

              last_n_cols = n_rows;
            }
        }
    }

  g_list_free (ungrouped_class_groups);

  /* Always let you go down to a zero size: */
  if (array->len > 0)
    {
      g_array_index(array, int, array->len-1) = 0;
    }
  else
    {
      val = 0;
      g_array_insert_val (array, 0, val);
      g_array_insert_val (array, 0, val);
    }

  if (tasklist->priv->size_hints)
    g_free (tasklist->priv->size_hints);

  tasklist->priv->size_hints_len = array->len;
  tasklist->priv->size_hints = (int *)g_array_free (array, FALSE);

  if (tasklist->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width = tasklist->priv->size_hints[0];
      requisition->height = fake_allocation.height;
    }
  else
    {
      requisition->width = fake_allocation.width;
      requisition->height = tasklist->priv->size_hints[0];
    }
}

static void
wnck_tasklist_get_preferred_width (GtkWidget *widget,
                                   int       *minimum_width,
                                   int       *natural_width)
{
  GtkRequisition req;

  wnck_tasklist_size_request (widget, &req);

  *minimum_width = *natural_width = req.width;
}

static void
wnck_tasklist_get_preferred_height (GtkWidget *widget,
                                   int       *minimum_height,
                                   int       *natural_height)
{
  GtkRequisition req;

  wnck_tasklist_size_request (widget, &req);

  *minimum_height = *natural_height = req.height;
}


/**
 * wnck_tasklist_get_size_hint_list:
 * @tasklist: a #WnckTasklist.
 * @n_elements: return location for the number of elements in the array
 * returned by this function. This number should always be pair.
 *
 * Since a #WnckTasklist does not have a fixed size (#WnckWindow can be grouped
 * when needed, for example), the standard size request mechanism in GTK+ is
 * not enough to announce what sizes can be used by @tasklist. The size hints
 * mechanism is a solution for this. See panel_applet_set_size_hints() for more
 * information.
 *
 * Return value: a list of size hints that can be used to allocate an
 * appropriate size for @tasklist.
 */
const int *
wnck_tasklist_get_size_hint_list (WnckTasklist  *tasklist,
				  int           *n_elements)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), NULL);
  g_return_val_if_fail (n_elements != NULL, NULL);

  *n_elements = tasklist->priv->size_hints_len;
  return tasklist->priv->size_hints;
}

static void
wnck_task_size_allocated (GtkWidget     *widget,
                          GtkAllocation *allocation,
                          gpointer       data)
{
  WnckTask        *task = WNCK_TASK (data);
  GtkStyleContext *context;
  GtkStateFlags    state;
  GtkBorder        padding;
  int              min_image_width;

  state = gtk_widget_get_state_flags (widget);
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (context, state, &padding);

  min_image_width = MINI_ICON_SIZE +
                    padding.left + padding.right +
                    2 * TASKLIST_BUTTON_PADDING;

  if ((allocation->width < min_image_width + 2 * TASKLIST_BUTTON_PADDING) &&
      (allocation->width >= min_image_width)) {
    gtk_widget_show (task->image);
    gtk_widget_hide (task->label);
  } else if (allocation->width < min_image_width) {
    gtk_widget_hide (task->image);
    gtk_widget_show (task->label);
  } else {
    gtk_widget_show (task->image);
    gtk_widget_show (task->label);
  }
}

static void
wnck_tasklist_size_allocate (GtkWidget      *widget,
                             GtkAllocation  *allocation)
{
  GtkAllocation child_allocation;
  WnckTasklist *tasklist;
  WnckTask *class_group_task;
  int n_windows;
  int n_startup_sequences;
  GList *l;
  int button_width;
  int total_width;
  int n_rows;
  int n_cols;
  int n_grouped_buttons;
  int i;
  gboolean score_set;
  GList *ungrouped_class_groups;
  WnckTask *win_task;
  GList *visible_tasks = NULL;
  GList *windows_sorted = NULL;
  int grouping_limit;

  tasklist = WNCK_TASKLIST (widget);

  n_windows = g_list_length (tasklist->priv->windows);
  n_startup_sequences = g_list_length (tasklist->priv->startup_sequences);
  n_grouped_buttons = 0;
  ungrouped_class_groups = g_list_copy (tasklist->priv->class_groups);
  score_set = FALSE;

  grouping_limit = MIN (tasklist->priv->grouping_limit,
			tasklist->priv->max_button_width);

  /* Try ungrouped mode */
  button_width = wnck_tasklist_layout (allocation,
				       tasklist->priv->max_button_width,
				       tasklist->priv->max_button_height,
				       n_startup_sequences + n_windows,
				       tasklist->priv->orientation,
				       &n_cols, &n_rows);
  while (ungrouped_class_groups != NULL &&
	 ((tasklist->priv->grouping == WNCK_TASKLIST_ALWAYS_GROUP) ||
	  ((tasklist->priv->grouping == WNCK_TASKLIST_AUTO_GROUP) &&
	   (button_width < grouping_limit))))
    {
      if (!score_set)
	{
	  wnck_tasklist_score_groups (tasklist, ungrouped_class_groups);
	  score_set = TRUE;
	}

      ungrouped_class_groups = wnck_task_get_highest_scored (ungrouped_class_groups, &class_group_task);

      n_grouped_buttons += g_list_length (class_group_task->windows) - 1;

      if (g_list_length (class_group_task->windows) > 1)
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task);

          /* Sort */
          class_group_task->windows = g_list_sort (class_group_task->windows,
                                                   wnck_task_compare_alphabetically);

	  /* Hide all this group's windows */
	  l = class_group_task->windows;
	  while (l != NULL)
	    {
	      win_task = WNCK_TASK (l->data);

	      gtk_widget_set_child_visible (GTK_WIDGET (win_task->button), FALSE);

	      l = l->next;
	    }
	}
      else
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task->windows->data);
	  gtk_widget_set_child_visible (GTK_WIDGET (class_group_task->button), FALSE);
        }

      button_width = wnck_tasklist_layout (allocation,
					   tasklist->priv->max_button_width,
					   tasklist->priv->max_button_height,
					   n_startup_sequences + n_windows - n_grouped_buttons,
					   tasklist->priv->orientation,
					   &n_cols, &n_rows);
    }

  /* Add all ungrouped windows to visible_tasks, and hide their class groups */
  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = WNCK_TASK (l->data);

      visible_tasks = g_list_concat (visible_tasks, g_list_copy (class_group_task->windows));
      gtk_widget_set_child_visible (GTK_WIDGET (class_group_task->button), FALSE);

      l = l->next;
    }

  /* Add all windows that are ungrouped because they don't belong to any class
   * group */
  l = tasklist->priv->windows_without_class_group;
  while (l != NULL)
    {
      WnckTask *task;

      task = WNCK_TASK (l->data);
      visible_tasks = g_list_append (visible_tasks, task);

      l = l->next;
    }

  /* Add all startup sequences */
  visible_tasks = g_list_concat (visible_tasks, g_list_copy (tasklist->priv->startup_sequences));

  /* Sort */
  visible_tasks = g_list_sort (visible_tasks, wnck_task_compare);

  /* Allocate children */
  l = visible_tasks;
  i = 0;
  total_width = tasklist->priv->max_button_width * n_cols;
  total_width = MIN (total_width, allocation->width);
  /* FIXME: this is obviously wrong, but if we don't this, some space that the
   * panel allocated to us won't have the panel popup menu, but the tasklist
   * popup menu */
  total_width = allocation->width;
  while (l != NULL)
    {
      WnckTask *task = WNCK_TASK (l->data);
      int row = i % n_rows;
      int col = i / n_rows;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        col = n_cols - col - 1;

      child_allocation.x = total_width*col / n_cols;
      child_allocation.y = allocation->height*row / n_rows;
      child_allocation.width = total_width*(col + 1) / n_cols - child_allocation.x;
      child_allocation.height = allocation->height*(row + 1) / n_rows - child_allocation.y;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (task->button, &child_allocation);
      gtk_widget_set_child_visible (GTK_WIDGET (task->button), TRUE);

      if (task->type != WNCK_TASK_STARTUP_SEQUENCE)
        {
          GList *ll;

          /* Build sorted windows list */
          if (g_list_length (task->windows) > 1)
            windows_sorted = g_list_concat (windows_sorted,
                                           g_list_copy (task->windows));
          else
            windows_sorted = g_list_append (windows_sorted, task);
          task->row = row;
          task->col = col;
          for (ll = task->windows; ll; ll = ll->next)
            {
              WNCK_TASK (ll->data)->row = row;
              WNCK_TASK (ll->data)->col = col;
            }
        }
      i++;
      l = l->next;
    }

  /* Update icon geometries. */
  wnck_tasklist_update_icon_geometries (tasklist, visible_tasks);

  g_list_free (visible_tasks);
  g_list_free (tasklist->priv->windows);
  g_list_free (ungrouped_class_groups);
  tasklist->priv->windows = windows_sorted;

  GTK_WIDGET_CLASS (wnck_tasklist_parent_class)->size_allocate (widget,
                                                                allocation);
}

static void
wnck_tasklist_realize (GtkWidget *widget)
{
  WnckTasklist *tasklist;
  GdkScreen *gdkscreen;

  tasklist = WNCK_TASKLIST (widget);

  gdkscreen = gtk_widget_get_screen (widget);
  tasklist->priv->screen = wnck_screen_get (gdk_screen_get_number (gdkscreen));
  g_assert (tasklist->priv->screen != NULL);

#ifdef HAVE_STARTUP_NOTIFICATION
  tasklist->priv->sn_context =
    sn_monitor_context_new (_wnck_screen_get_sn_display (tasklist->priv->screen),
                            wnck_screen_get_number (tasklist->priv->screen),
                            wnck_tasklist_sn_event,
                            tasklist,
                            NULL);
#endif

  (* GTK_WIDGET_CLASS (wnck_tasklist_parent_class)->realize) (widget);

  tasklist_instances = g_slist_append (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances,
		   (GFunc) wnck_tasklist_update_lists,
		   NULL);

  wnck_tasklist_update_lists (tasklist);

  wnck_tasklist_connect_screen (tasklist);
}

static void
wnck_tasklist_unrealize (GtkWidget *widget)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);

  wnck_tasklist_disconnect_screen (tasklist);
  tasklist->priv->screen = NULL;

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_monitor_context_unref (tasklist->priv->sn_context);
  tasklist->priv->sn_context = NULL;
#endif

  (* GTK_WIDGET_CLASS (wnck_tasklist_parent_class)->unrealize) (widget);

  tasklist_instances = g_slist_remove (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances,
		   (GFunc) wnck_tasklist_update_lists,
		   NULL);
}

static void
wnck_tasklist_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  WnckTasklist *tasklist;
  GList *tmp;

  tasklist = WNCK_TASKLIST (container);

  tmp = tasklist->priv->windows;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      (* callback) (task->button, callback_data);
    }

  tmp = tasklist->priv->class_groups;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      (* callback) (task->button, callback_data);
    }

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      (* callback) (task->button, callback_data);
    }
}

static void
wnck_tasklist_remove (GtkContainer   *container,
		      GtkWidget	     *widget)
{
  WnckTasklist *tasklist;
  GList *tmp;

  g_return_if_fail (WNCK_IS_TASKLIST (container));
  g_return_if_fail (widget != NULL);

  tasklist = WNCK_TASKLIST (container);

  /* it's safer to handle windows_without_class_group before windows */
  tmp = tasklist->priv->windows_without_class_group;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  tasklist->priv->windows_without_class_group =
	    g_list_remove (tasklist->priv->windows_without_class_group,
			   task);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->windows;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->win_hash,
			       task->window);
	  tasklist->priv->windows =
	    g_list_remove (tasklist->priv->windows,
			   task);

          gtk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->class_groups;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->class_group_hash,
			       task->class_group);
	  tasklist->priv->class_groups =
	    g_list_remove (tasklist->priv->class_groups,
			   task);

          gtk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  tasklist->priv->startup_sequences =
	    g_list_remove (tasklist->priv->startup_sequences,
			   task);

          gtk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
wnck_tasklist_connect_screen (WnckTasklist *tasklist)
{
  GList *windows;
  guint *c;
  int    i;
  WnckScreen *screen;

  g_return_if_fail (tasklist->priv->screen != NULL);

  screen = tasklist->priv->screen;

  i = 0;
  c = tasklist->priv->screen_connections;

  c [i++] = g_signal_connect_object (G_OBJECT (screen), "active_window_changed",
                                     G_CALLBACK (wnck_tasklist_active_window_changed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "active_workspace_changed",
                                     G_CALLBACK (wnck_tasklist_active_workspace_changed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "window_opened",
                                     G_CALLBACK (wnck_tasklist_window_added),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "window_closed",
                                     G_CALLBACK (wnck_tasklist_window_removed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "viewports_changed",
                                     G_CALLBACK (wnck_tasklist_viewports_changed),
                                     tasklist, 0);


  g_assert (i == N_SCREEN_CONNECTIONS);

  windows = wnck_screen_get_windows (screen);
  while (windows != NULL)
    {
      wnck_tasklist_connect_window (tasklist, windows->data);
      windows = windows->next;
    }
}

static void
wnck_tasklist_disconnect_screen (WnckTasklist *tasklist)
{
  GList *windows;
  int    i;

  windows = wnck_screen_get_windows (tasklist->priv->screen);
  while (windows != NULL)
    {
      wnck_tasklist_disconnect_window (tasklist, windows->data);
      windows = windows->next;
    }

  i = 0;
  while (i < N_SCREEN_CONNECTIONS)
    {
      if (tasklist->priv->screen_connections [i] != 0)
        g_signal_handler_disconnect (G_OBJECT (tasklist->priv->screen),
                                     tasklist->priv->screen_connections [i]);

      tasklist->priv->screen_connections [i] = 0;

      ++i;
    }

  g_assert (i == N_SCREEN_CONNECTIONS);

#ifdef HAVE_STARTUP_NOTIFICATION
  if (tasklist->priv->startup_sequence_timeout != 0)
    {
      g_source_remove (tasklist->priv->startup_sequence_timeout);
      tasklist->priv->startup_sequence_timeout = 0;
    }
#endif
}

static gboolean
wnck_tasklist_scroll_event (GtkWidget      *widget,
                            GdkEventScroll *event)
{
  /* use the fact that tasklist->priv->windows is sorted
   * see wnck_tasklist_size_allocate() */
  WnckTasklist *tasklist;
  GtkTextDirection ltr;
  GList *window;
  gint row = 0;
  gint col = 0;

  tasklist = WNCK_TASKLIST (widget);

  window = g_list_find (tasklist->priv->windows,
                        tasklist->priv->active_task);
  if (window)
    {
      row = WNCK_TASK (window->data)->row;
      col = WNCK_TASK (window->data)->col;
    }
  else
    if (tasklist->priv->activate_timeout_id)
      /* There is no active_task yet, but there will be one after the timeout.
       * It occurs if we change the active task too fast. */
      return TRUE;

  ltr = (gtk_widget_get_direction (GTK_WIDGET (tasklist)) != GTK_TEXT_DIR_RTL);

  switch (event->direction)
    {
      case GDK_SCROLL_UP:
        if (!window)
          window = g_list_last (tasklist->priv->windows);
        else
          window = window->prev;
      break;

      case GDK_SCROLL_DOWN:
        if (!window)
          window = tasklist->priv->windows;
        else
          window = window->next;
      break;

#define TASKLIST_GET_MOST_LEFT(ltr, window, tasklist)   \
  do                                                    \
    {                                                   \
      if (ltr)                                          \
        window = tasklist->priv->windows;               \
      else                                              \
        window = g_list_last (tasklist->priv->windows); \
    } while (0)

#define TASKLIST_GET_MOST_RIGHT(ltr, window, tasklist)  \
  do                                                    \
    {                                                   \
      if (ltr)                                          \
        window = g_list_last (tasklist->priv->windows); \
      else                                              \
        window = tasklist->priv->windows;               \
    } while (0)

      case GDK_SCROLL_LEFT:
        if (!window)
          TASKLIST_GET_MOST_RIGHT (ltr, window, tasklist);
        else
          {
            /* Search the first window on the previous colomn at same row */
            if (ltr)
              {
                while (window && (WNCK_TASK(window->data)->row != row ||
                                  WNCK_TASK(window->data)->col != col-1))
                  window = window->prev;
              }
            else
              {
                while (window && (WNCK_TASK(window->data)->row != row ||
                                  WNCK_TASK(window->data)->col != col-1))
                  window = window->next;
              }
            /* If no window found, select the top/bottom left one */
            if (!window)
              TASKLIST_GET_MOST_LEFT (ltr, window, tasklist);
          }
      break;

      case GDK_SCROLL_RIGHT:
        if (!window)
          TASKLIST_GET_MOST_LEFT (ltr, window, tasklist);
        else
          {
            /* Search the first window on the next colomn at same row */
            if (ltr)
              {
                while (window && (WNCK_TASK(window->data)->row != row ||
                                  WNCK_TASK(window->data)->col != col+1))
                  window = window->next;
              }
            else
              {
                while (window && (WNCK_TASK(window->data)->row != row ||
                                  WNCK_TASK(window->data)->col != col+1))
                  window = window->prev;
              }
            /* If no window found, select the top/bottom right one */
            if (!window)
              TASKLIST_GET_MOST_RIGHT (ltr, window, tasklist);
          }
      break;

      case GDK_SCROLL_SMOOTH:
        window = NULL;
      break;

#undef TASKLIST_GET_MOST_LEFT
#undef TASKLIST_GET_MOST_RIGHT

      default:
        g_assert_not_reached ();
    }

  if (window)
    wnck_tasklist_activate_task_window (window->data, event->time);

  return TRUE;
}

/**
 * wnck_tasklist_new:
 *
 * Creates a new #WnckTasklist. The #WnckTasklist will list #WnckWindow of the
 * #WnckScreen it is on.
 *
 * Return value: a newly created #WnckTasklist.
 */
GtkWidget*
wnck_tasklist_new (void)
{
  WnckTasklist *tasklist;

  tasklist = g_object_new (WNCK_TYPE_TASKLIST, NULL);

  return GTK_WIDGET (tasklist);
}

static void
wnck_tasklist_free_tasks (WnckTasklist *tasklist)
{
  GList *l;

  tasklist->priv->active_task = NULL;
  tasklist->priv->active_class_group = NULL;

  if (tasklist->priv->windows)
    {
      l = tasklist->priv->windows;
      while (l != NULL)
	{
	  WnckTask *task = WNCK_TASK (l->data);
	  l = l->next;
          /* if we just unref the task it means we lose our ref to the
           * task before we unparent the button, which breaks stuff.
           */
	  gtk_widget_destroy (task->button);
	}
    }
  g_assert (tasklist->priv->windows == NULL);
  g_assert (tasklist->priv->windows_without_class_group == NULL);
  g_assert (g_hash_table_size (tasklist->priv->win_hash) == 0);

  if (tasklist->priv->class_groups)
    {
      l = tasklist->priv->class_groups;
      while (l != NULL)
	{
	  WnckTask *task = WNCK_TASK (l->data);
	  l = l->next;
          /* if we just unref the task it means we lose our ref to the
           * task before we unparent the button, which breaks stuff.
           */
	  gtk_widget_destroy (task->button);
	}
    }

  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (g_hash_table_size (tasklist->priv->class_group_hash) == 0);

  if (tasklist->priv->skipped_windows)
    {
      wnck_tasklist_free_skipped_windows (tasklist);
      tasklist->priv->skipped_windows = NULL;
    }
}


/*
 * This function determines if a window should be included in the tasklist.
 */
static gboolean
tasklist_include_window_impl (WnckTasklist *tasklist,
                              WnckWindow   *win,
                              gboolean      check_for_skipped_list)
{
  WnckWorkspace *active_workspace;
  int x, y, w, h;

  if (!check_for_skipped_list &&
      wnck_window_get_state (win) & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    return FALSE;

  if (tasklist->priv->monitor_num != -1)
    {
      wnck_window_get_geometry (win, &x, &y, &w, &h);
      /* Don't include the window if its center point is not on the same monitor */
      if (gdk_screen_get_monitor_at_point (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
                                           x + w / 2, y + h / 2) != tasklist->priv->monitor_num)
        return FALSE;
    }

  /* Remainder of checks aren't relevant for checking if the window should
   * be in the skipped list.
   */
  if (check_for_skipped_list)
    return TRUE;

  if (tasklist->priv->include_all_workspaces)
    return TRUE;

  if (wnck_window_is_pinned (win))
    return TRUE;

  active_workspace = wnck_screen_get_active_workspace (tasklist->priv->screen);
  if (active_workspace == NULL)
    return TRUE;

  if (wnck_window_or_transient_needs_attention (win))
    return TRUE;

  if (active_workspace != wnck_window_get_workspace (win))
    return FALSE;

  if (!wnck_workspace_is_virtual (active_workspace))
    return TRUE;

  return wnck_window_is_in_viewport (win, active_workspace);
}

static gboolean
tasklist_include_in_skipped_list (WnckTasklist *tasklist, WnckWindow *win)
{
  return tasklist_include_window_impl (tasklist,
                                       win,
                                       TRUE /* check_for_skipped_list */);
}

static gboolean
wnck_tasklist_include_window (WnckTasklist *tasklist, WnckWindow *win)
{
  return tasklist_include_window_impl (tasklist,
                                       win,
                                       FALSE /* check_for_skipped_list */);
}

static void
wnck_tasklist_update_lists (WnckTasklist *tasklist)
{
  GdkWindow *tasklist_window;
  GList *windows;
  WnckWindow *win;
  WnckClassGroup *class_group;
  GList *l;
  WnckTask *win_task;
  WnckTask *class_group_task;

  wnck_tasklist_free_tasks (tasklist);

  /* wnck_tasklist_update_lists() will be called on realize */
  if (!gtk_widget_get_realized (GTK_WIDGET (tasklist)))
    return;

  tasklist_window = gtk_widget_get_window (GTK_WIDGET (tasklist));

  if (tasklist_window != NULL)
    {
      /*
       * only show windows from this monitor if there is more than one tasklist running
       */
      if (tasklist_instances == NULL || tasklist_instances->next == NULL)
	{
	  tasklist->priv->monitor_num = -1;
        }
      else
	{
	  int monitor_num;

	  monitor_num = gdk_screen_get_monitor_at_window (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
							  tasklist_window);

	  if (monitor_num != tasklist->priv->monitor_num)
	    {
	      tasklist->priv->monitor_num = monitor_num;
	      gdk_screen_get_monitor_geometry (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
					       tasklist->priv->monitor_num,
					       &tasklist->priv->monitor_geometry);
	    }
	}
    }

  l = windows = wnck_screen_get_windows (tasklist->priv->screen);
  while (l != NULL)
    {
      win = WNCK_WINDOW (l->data);

      if (wnck_tasklist_include_window (tasklist, win))
	{
	  win_task = wnck_task_new_from_window (tasklist, win);
	  tasklist->priv->windows = g_list_prepend (tasklist->priv->windows, win_task);
	  g_hash_table_insert (tasklist->priv->win_hash, win, win_task);

	  gtk_widget_set_parent (win_task->button, GTK_WIDGET (tasklist));
	  gtk_widget_show (win_task->button);

	  /* Class group */

	  class_group = wnck_window_get_class_group (win);
          /* don't group windows if they do not belong to any class */
          if (strcmp (wnck_class_group_get_id (class_group), "") != 0)
            {
              class_group_task =
                        g_hash_table_lookup (tasklist->priv->class_group_hash,
                                             class_group);

              if (class_group_task == NULL)
                {
                  class_group_task =
                                  wnck_task_new_from_class_group (tasklist,
                                                                  class_group);
                  gtk_widget_set_parent (class_group_task->button,
                                         GTK_WIDGET (tasklist));
                  gtk_widget_show (class_group_task->button);

                  tasklist->priv->class_groups =
                                  g_list_prepend (tasklist->priv->class_groups,
                                                  class_group_task);
                  g_hash_table_insert (tasklist->priv->class_group_hash,
                                       class_group, class_group_task);
                }

              class_group_task->windows =
                                    g_list_prepend (class_group_task->windows,
                                                    win_task);
            }
          else
            {
              g_object_ref (win_task);
              tasklist->priv->windows_without_class_group =
                              g_list_prepend (tasklist->priv->windows_without_class_group,
                                              win_task);
            }
	}
      else if (tasklist_include_in_skipped_list (tasklist, win))
        {
          skipped_window *skipped = g_new0 (skipped_window, 1);
          skipped->window = g_object_ref (win);
          skipped->tag = g_signal_connect (G_OBJECT (win),
                                           "state_changed",
                                           G_CALLBACK (wnck_task_state_changed),
                                           tasklist);
          tasklist->priv->skipped_windows =
            g_list_prepend (tasklist->priv->skipped_windows,
                            (gpointer) skipped);
        }

      l = l->next;
    }

  /* Sort the class group list */
  l = tasklist->priv->class_groups;
  while (l)
    {
      class_group_task = WNCK_TASK (l->data);

      class_group_task->windows = g_list_sort (class_group_task->windows, wnck_task_compare);

      /* so the number of windows in the task gets reset on the
       * task label
       */
      wnck_task_update_visible_state (class_group_task);

      l = l->next;
    }

  /* since we cleared active_window we need to reset it */
  wnck_tasklist_active_window_changed (tasklist->priv->screen, NULL, tasklist);

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_change_active_task (WnckTasklist *tasklist, WnckTask *active_task)
{
  if (active_task &&
      active_task == tasklist->priv->active_task)
    return;

  g_assert (active_task == NULL ||
            active_task->type != WNCK_TASK_STARTUP_SEQUENCE);

  if (tasklist->priv->active_task)
    {
      tasklist->priv->active_task->really_toggling = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_task->button),
				    FALSE);
      tasklist->priv->active_task->really_toggling = FALSE;
    }

  tasklist->priv->active_task = active_task;

  if (tasklist->priv->active_task)
    {
      tasklist->priv->active_task->really_toggling = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_task->button),
				    TRUE);
      tasklist->priv->active_task->really_toggling = FALSE;
    }

  if (active_task)
    {
      active_task = g_hash_table_lookup (tasklist->priv->class_group_hash,
					 active_task->class_group);

      if (active_task &&
	  active_task == tasklist->priv->active_class_group)
	return;

      if (tasklist->priv->active_class_group)
	{
	  tasklist->priv->active_class_group->really_toggling = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_class_group->button),
					FALSE);
	  tasklist->priv->active_class_group->really_toggling = FALSE;
	}

      tasklist->priv->active_class_group = active_task;

      if (tasklist->priv->active_class_group)
	{
	  tasklist->priv->active_class_group->really_toggling = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_class_group->button),
					TRUE);
	  tasklist->priv->active_class_group->really_toggling = FALSE;
	}
    }
}

static void
wnck_tasklist_update_icon_geometries (WnckTasklist *tasklist,
				      GList        *visible_tasks)
{
	gint x, y, width, height;
	GList *l1;

	for (l1 = visible_tasks; l1; l1 = l1->next) {
		WnckTask *task = WNCK_TASK (l1->data);
                GtkAllocation allocation;

		if (!gtk_widget_get_realized (task->button))
			continue;

                /* Let's cheat with some internal knowledge of GtkButton: in a
                 * GtkButton, the window is the same as the parent window. So
                 * to know the position of the widget, we should use the
                 * the position of the parent window and the allocation information. */

                gtk_widget_get_allocation (task->button, &allocation);

                gdk_window_get_origin (gtk_widget_get_parent_window (task->button),
                                       &x, &y);

                x += allocation.x;
                y += allocation.y;
                width = allocation.width;
                height = allocation.height;

		if (task->window)
			wnck_window_set_icon_geometry (task->window,
						       x, y, width, height);
		else {
			GList *l2;

			for (l2 = task->windows; l2; l2 = l2->next) {
				WnckTask *win_task = WNCK_TASK (l2->data);

				g_assert (win_task->window);

				wnck_window_set_icon_geometry (win_task->window,
							       x, y, width, height);
			}
		}
	}
}

static void
wnck_tasklist_active_window_changed (WnckScreen   *screen,
                                     WnckWindow   *previous_window,
				     WnckTasklist *tasklist)
{
  WnckWindow *active_window;
  WnckWindow *initial_window;
  WnckTask *active_task = NULL;

  /* FIXME: check for group modal window */
  initial_window = active_window = wnck_screen_get_active_window (screen);
  active_task = g_hash_table_lookup (tasklist->priv->win_hash, active_window);
  while (active_window && !active_task)
    {
      active_window = wnck_window_get_transient (active_window);
      active_task = g_hash_table_lookup (tasklist->priv->win_hash,
                                         active_window);
      /* Check for transient cycles */
      if (active_window == initial_window)
        break;
    }

  wnck_tasklist_change_active_task (tasklist, active_task);
}

static void
wnck_tasklist_active_workspace_changed (WnckScreen    *screen,
                                        WnckWorkspace *previous_workspace,
					WnckTasklist  *tasklist)
{
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_window_changed_workspace (WnckWindow   *window,
					WnckTasklist *tasklist)
{
  WnckWorkspace *active_ws;
  WnckWorkspace *window_ws;
  gboolean       need_update;
  GList         *l;

  active_ws = wnck_screen_get_active_workspace (tasklist->priv->screen);
  window_ws = wnck_window_get_workspace (window);

  if (!window_ws)
    return;

  need_update = FALSE;

  if (active_ws == window_ws)
    need_update = TRUE;

  l = tasklist->priv->windows;
  while (!need_update && l != NULL)
    {
      WnckTask *task = l->data;

      if (task->type == WNCK_TASK_WINDOW &&
          task->window == window)
        need_update = TRUE;

      l = l->next;
    }

  if (need_update)
    {
      wnck_tasklist_update_lists (tasklist);
      gtk_widget_queue_resize (GTK_WIDGET (tasklist));
    }
}

static gboolean
do_wnck_tasklist_update_lists (gpointer data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);

  tasklist->priv->idle_callback_tag = 0;

  wnck_tasklist_update_lists (tasklist);

  return FALSE;
}

static void
wnck_tasklist_window_changed_geometry (WnckWindow   *window,
				       WnckTasklist *tasklist)
{
  GdkWindow *tasklist_window;
  WnckTask *win_task;
  gboolean show;
  gboolean monitor_changed;
  int x, y, w, h;

  if (tasklist->priv->idle_callback_tag != 0)
    return;

  tasklist_window = gtk_widget_get_window (GTK_WIDGET (tasklist));

  /*
   * If the (parent of the) tasklist itself skips
   * the tasklist, we need an extra check whether
   * the tasklist itself possibly changed monitor.
   */
  monitor_changed = FALSE;
  if (tasklist->priv->monitor_num != -1 &&
      (wnck_window_get_state (window) & WNCK_WINDOW_STATE_SKIP_TASKLIST) &&
      tasklist_window != NULL)
    {
      /* Do the extra check only if there is a suspect of a monitor change (= this window is off monitor) */
      wnck_window_get_geometry (window, &x, &y, &w, &h);
      if (!POINT_IN_RECT (x + w / 2, y + h / 2, tasklist->priv->monitor_geometry))
        {
          monitor_changed = (gdk_screen_get_monitor_at_window (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
                                                               tasklist_window) != tasklist->priv->monitor_num);
        }
    }

  /*
   * We want to re-generate the task list if
   * the window is shown but shouldn't be or
   * the window isn't shown but should be or
   * the tasklist itself changed monitor.
   */
  win_task = g_hash_table_lookup (tasklist->priv->win_hash, window);
  show = wnck_tasklist_include_window (tasklist, window);
  if (((win_task == NULL && !show) || (win_task != NULL && show)) &&
      !monitor_changed)
    return;

  /* Don't keep any stale references */
  gtk_widget_queue_draw (GTK_WIDGET (tasklist));

  tasklist->priv->idle_callback_tag = g_idle_add (do_wnck_tasklist_update_lists, tasklist);
}

static void
wnck_tasklist_connect_window (WnckTasklist *tasklist,
			      WnckWindow   *window)
{
  g_signal_connect_object (window, "workspace_changed",
			   G_CALLBACK (wnck_tasklist_window_changed_workspace),
			   tasklist, 0);
  g_signal_connect_object (window, "geometry_changed",
			   G_CALLBACK (wnck_tasklist_window_changed_geometry),
			   tasklist, 0);
}

static void
wnck_tasklist_disconnect_window (WnckTasklist *tasklist,
			         WnckWindow   *window)
{
  g_signal_handlers_disconnect_by_func (window,
                                        wnck_tasklist_window_changed_workspace,
                                        tasklist);
  g_signal_handlers_disconnect_by_func (window,
                                        wnck_tasklist_window_changed_geometry,
                                        tasklist);
}

static void
wnck_tasklist_window_added (WnckScreen   *screen,
			    WnckWindow   *win,
			    WnckTasklist *tasklist)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  wnck_tasklist_check_end_sequence (tasklist, win);
#endif

  wnck_tasklist_connect_window (tasklist, win);

  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_window_removed (WnckScreen   *screen,
			      WnckWindow   *win,
			      WnckTasklist *tasklist)
{
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_viewports_changed (WnckScreen   *screen,
                                 WnckTasklist *tasklist)
{
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_task_position_menu (GtkMenu   *menu,
			 gint      *x,
			 gint      *y,
			 gboolean  *push_in,
			 gpointer   user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  GdkWindow *window;
  GdkDeviceManager *dev_manager;
  GdkDevice *pointer;
  GtkAllocation allocation;
  GtkRequisition requisition;
  gint menu_xpos;
  gint menu_ypos;
  gint pointer_x;
  gint pointer_y;

  gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);

  window = gtk_widget_get_window (widget);
  gtk_widget_get_allocation (widget, &allocation);

  gdk_window_get_origin (window, &menu_xpos, &menu_ypos);

  menu_xpos += allocation.x;
  menu_ypos += allocation.y;

  if (menu_ypos >  gdk_screen_height () / 2)
    menu_ypos -= requisition.height;
  else
    menu_ypos += allocation.height;

  dev_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
  pointer = gdk_device_manager_get_client_pointer (dev_manager);
  gdk_window_get_device_position (window, pointer, &pointer_x, &pointer_y, NULL);

  if (requisition.width < pointer_x)
    menu_xpos += MIN (pointer_x, allocation.width - requisition.width);

  *x = menu_xpos;
  *y = menu_ypos;
  *push_in = FALSE;
}

static gboolean
wnck_tasklist_change_active_timeout (gpointer data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);

  tasklist->priv->activate_timeout_id = 0;

  wnck_tasklist_active_window_changed (tasklist->priv->screen, NULL, tasklist);

  return FALSE;
}

static void
wnck_task_menu_activated (GtkMenuItem *menu_item,
			  gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);

  /* This is an "activate" callback function so gtk_get_current_event_time()
   * will suffice.
   */
  wnck_tasklist_activate_task_window (task, gtk_get_current_event_time ());
}

static void
wnck_tasklist_activate_next_in_class_group (WnckTask *task,
                                            guint32   timestamp)
{
  WnckTask *activate_task;
  gboolean  activate_next;
  GList    *l;

  activate_task = NULL;
  activate_next = FALSE;

  l = task->windows;
  while (l)
    {
      WnckTask *task;

      task = WNCK_TASK (l->data);

      if (wnck_window_is_most_recently_activated (task->window))
        activate_next = TRUE;
      else if (activate_next)
        {
          activate_task = task;
          break;
        }

      l = l->next;
    }

  /* no task in this group is active, or only the last one => activate
   * the first task */
  if (!activate_task && task->windows)
    activate_task = WNCK_TASK (task->windows->data);

  if (activate_task)
    {
      task->was_active = FALSE;
      wnck_tasklist_activate_task_window (activate_task, timestamp);
    }
}

static void
wnck_tasklist_activate_task_window (WnckTask *task,
                                    guint32   timestamp)
{
  WnckTasklist *tasklist;
  WnckWindowState state;
  WnckWorkspace *active_ws;
  WnckWorkspace *window_ws;

  tasklist = task->tasklist;

  if (task->window == NULL)
    return;

  state = wnck_window_get_state (task->window);

  active_ws = wnck_screen_get_active_workspace (tasklist->priv->screen);
  window_ws = wnck_window_get_workspace (task->window);

  if (state & WNCK_WINDOW_STATE_MINIMIZED)
    {
      if (window_ws &&
          active_ws != window_ws &&
          !tasklist->priv->switch_workspace_on_unminimize)
        wnck_workspace_activate (window_ws, timestamp);

      wnck_window_activate_transient (task->window, timestamp);
    }
  else
    {
      if ((task->was_active ||
           wnck_window_transient_is_most_recently_activated (task->window)) &&
          (!window_ws || active_ws == window_ws))
	{
	  task->was_active = FALSE;
	  wnck_window_minimize (task->window);
	  return;
	}
      else
	{
          /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
           * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
           * There should only be *one* activate call.
           */
          if (window_ws)
            wnck_workspace_activate (window_ws, timestamp);

          wnck_window_activate_transient (task->window, timestamp);
        }
    }


  if (tasklist->priv->activate_timeout_id)
    g_source_remove (tasklist->priv->activate_timeout_id);

  tasklist->priv->activate_timeout_id =
    g_timeout_add (500, &wnck_tasklist_change_active_timeout, tasklist);

  wnck_tasklist_change_active_task (tasklist, task);
}

static void
wnck_task_close_all (GtkMenuItem *menu_item,
 		     gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      wnck_window_close (child->window, gtk_get_current_event_time ());
      l = l->next;
    }
}

static void
wnck_task_unminimize_all (GtkMenuItem *menu_item,
		          gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      /* This is inside an activate callback, so gtk_get_current_event_time()
       * will work.
       */
      wnck_window_unminimize (child->window, gtk_get_current_event_time ());
      l = l->next;
    }
}

static void
wnck_task_minimize_all (GtkMenuItem *menu_item,
  		        gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      wnck_window_minimize (child->window);
      l = l->next;
    }
}

static void
wnck_task_unmaximize_all (GtkMenuItem *menu_item,
  		        gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      wnck_window_unmaximize (child->window);
      l = l->next;
    }
}

static void
wnck_task_maximize_all (GtkMenuItem *menu_item,
  		        gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      wnck_window_maximize (child->window);
      l = l->next;
    }
}

static void
wnck_task_popup_menu (WnckTask *task,
                      gboolean  action_submenu)
{
  GtkWidget *menu;
  WnckTask *win_task;
  char *text;
  GdkPixbuf *pixbuf;
  GtkWidget *menu_item;
  GtkWidget *image;
  GList *l, *list;

  g_return_if_fail (task->type == WNCK_TASK_CLASS_GROUP);

  if (task->class_group == NULL)
    return;

  if (task->menu == NULL)
    {
      task->menu = gtk_menu_new ();
      g_object_ref_sink (task->menu);
    }

  menu = task->menu;

  /* Remove old menu content */
  list = gtk_container_get_children (GTK_CONTAINER (menu));
  l = list;
  while (l)
    {
      GtkWidget *child = GTK_WIDGET (l->data);
      gtk_container_remove (GTK_CONTAINER (menu), child);
      l = l->next;
    }
  g_list_free (list);

  l = task->windows;
  while (l)
    {
      win_task = WNCK_TASK (l->data);

      text = wnck_task_get_text (win_task, TRUE, TRUE);
      menu_item = gtk_image_menu_item_new_with_label (text);
      g_free (text);

      gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item),
                                                 TRUE);

      if (wnck_task_get_needs_attention (win_task))
        _make_gtk_label_bold (GTK_LABEL (gtk_bin_get_child (GTK_BIN (menu_item))));

      text = wnck_task_get_text (win_task, FALSE, FALSE);
      gtk_widget_set_tooltip_text (menu_item, text);
      g_free (text);

      pixbuf = wnck_task_get_icon (win_task);
      if (pixbuf)
	{
	  image = gtk_image_new_from_pixbuf (pixbuf);
	  gtk_widget_show (image);
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
					 image);
	  g_object_unref (pixbuf);
	}

      gtk_widget_show (menu_item);

      if (action_submenu)
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                                   wnck_action_menu_new (win_task->window));
      else
        {
          static const GtkTargetEntry targets[] = {
            { "application/x-wnck-window-id", 0, 0 }
          };

          g_signal_connect_object (G_OBJECT (menu_item), "activate",
                                   G_CALLBACK (wnck_task_menu_activated),
                                   G_OBJECT (win_task),
                                   0);


          gtk_drag_source_set (menu_item, GDK_BUTTON1_MASK,
                               targets, 1, GDK_ACTION_MOVE);
          g_signal_connect_object (G_OBJECT(menu_item), "drag_begin",
                                   G_CALLBACK (wnck_task_drag_begin),
                                   G_OBJECT (win_task),
                                   0);
          g_signal_connect_object (G_OBJECT(menu_item), "drag_end",
                                   G_CALLBACK (wnck_task_drag_end),
                                   G_OBJECT (win_task),
                                   0);
          g_signal_connect_object (G_OBJECT(menu_item), "drag_data_get",
                                   G_CALLBACK (wnck_task_drag_data_get),
                                   G_OBJECT (win_task),
                                   0);
        }

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      l = l->next;
    }

  /* In case of Right click, show Minimize All, Unminimize All, Close All*/
  if (action_submenu)
    {
      GtkWidget *separator;
      GtkWidget *image;

      separator = gtk_separator_menu_item_new ();
      gtk_widget_show (separator);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);

      menu_item = gtk_image_menu_item_new_with_mnemonic (_("Mi_nimize All"));
      image = gtk_image_new_from_stock (WNCK_STOCK_MINIMIZE, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
	    		       G_CALLBACK (wnck_task_minimize_all),
			       G_OBJECT (task),
			       0);

      menu_item =  gtk_image_menu_item_new_with_mnemonic (_("Un_minimize All"));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (wnck_task_unminimize_all),
			       G_OBJECT (task),
			       0);

      menu_item = gtk_image_menu_item_new_with_mnemonic (_("Ma_ximize All"));
      image = gtk_image_new_from_stock (WNCK_STOCK_MAXIMIZE, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (wnck_task_maximize_all),
			       G_OBJECT (task),
			       0);

      menu_item =  gtk_image_menu_item_new_with_mnemonic (_("_Unmaximize All"));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (wnck_task_unmaximize_all),
			       G_OBJECT (task),
			       0);

      separator = gtk_separator_menu_item_new ();
      gtk_widget_show (separator);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);

      menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Close All"));
      image = gtk_image_new_from_stock (WNCK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
			       G_CALLBACK (wnck_task_close_all),
			       G_OBJECT (task),
			       0);
    }

  gtk_menu_set_screen (GTK_MENU (menu),
		       _wnck_screen_get_gdk_screen (task->tasklist->priv->screen));

  gtk_widget_show (menu);
  gtk_menu_popup (GTK_MENU (menu),
		  NULL, NULL,
		  wnck_task_position_menu, task->button,
		  1, gtk_get_current_event_time ());
}

static void
wnck_task_button_toggled (GtkButton *button,
			  WnckTask  *task)
{
  /* Did we really want to change the state of the togglebutton? */
  if (task->really_toggling)
    return;

  /* Undo the toggle */
  task->really_toggling = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
  task->really_toggling = FALSE;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      wnck_task_popup_menu (task, FALSE);
      break;
    case WNCK_TASK_WINDOW:
      if (task->window == NULL)
	return;

      /* This should only be called by clicking on the task button, so
       * gtk_get_current_event_time() should be fine here...
       */
      wnck_tasklist_activate_task_window (task, gtk_get_current_event_time ());
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }
}

static char *
wnck_task_get_text (WnckTask *task,
                    gboolean  icon_text,
                    gboolean  include_state)
{
  const char *name;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      name = wnck_class_group_get_name (task->class_group);
      if (name[0] != 0)
	return g_strdup_printf ("%s (%d)",
				name,
				g_list_length (task->windows));
      else
	return g_strdup_printf ("(%d)",
				g_list_length (task->windows));

    case WNCK_TASK_WINDOW:
      return _wnck_window_get_name_for_display (task->window,
                                                icon_text, include_state);
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
#ifdef HAVE_STARTUP_NOTIFICATION
      name = sn_startup_sequence_get_description (task->startup_sequence);
      if (name == NULL)
        name = sn_startup_sequence_get_name (task->startup_sequence);
      if (name == NULL)
        name = sn_startup_sequence_get_binary_name (task->startup_sequence);

      return g_strdup (name);
#else
      return NULL;
#endif
      break;
    }

  return NULL;
}

static void
wnck_dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;

  g_assert (pixbuf != NULL);

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (pixbuf);

  for (y = 0; y < h; y++)
    {
      pixels = row;

      for (x = 0; x < w; x++)
	{
	  pixels[3] /= 2;

	  pixels += pixel_stride;
	}

      row += row_stride;
    }
}

static GdkPixbuf *
wnck_task_scale_icon (GdkPixbuf *orig, gboolean minimized)
{
  int w, h;
  GdkPixbuf *pixbuf;

  if (!orig)
    return NULL;

  w = gdk_pixbuf_get_width (orig);
  h = gdk_pixbuf_get_height (orig);

  if (h != MINI_ICON_SIZE ||
      !gdk_pixbuf_get_has_alpha (orig))
    {
      double scale;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       TRUE,
			       8,
			       MINI_ICON_SIZE * w / (double) h,
			       MINI_ICON_SIZE);

      scale = MINI_ICON_SIZE / (double) gdk_pixbuf_get_height (orig);

      gdk_pixbuf_scale (orig,
			pixbuf,
			0, 0,
			gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			0, 0,
			scale, scale,
			GDK_INTERP_HYPER);
    }
  else
    pixbuf = orig;

  if (minimized)
    {
      if (orig == pixbuf)
	pixbuf = gdk_pixbuf_copy (orig);

      wnck_dimm_icon (pixbuf);
    }

  if (orig == pixbuf)
    g_object_ref (pixbuf);

  return pixbuf;
}


static GdkPixbuf *
wnck_task_get_icon (WnckTask *task)
{
  WnckWindowState state;
  GdkPixbuf *pixbuf;

  pixbuf = NULL;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      pixbuf = wnck_task_scale_icon (wnck_class_group_get_mini_icon (task->class_group),
				     FALSE);
      break;

    case WNCK_TASK_WINDOW:
      state = wnck_window_get_state (task->window);

      pixbuf =  wnck_task_scale_icon (wnck_window_get_mini_icon (task->window),
				      state & WNCK_WINDOW_STATE_MINIMIZED);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
#ifdef HAVE_STARTUP_NOTIFICATION
      if (task->tasklist->priv->icon_loader != NULL)
        {
          const char *icon;

          icon = sn_startup_sequence_get_icon_name (task->startup_sequence);
          if (icon != NULL)
            {
              GdkPixbuf *loaded;

              loaded =  (* task->tasklist->priv->icon_loader) (icon,
                                                               MINI_ICON_SIZE,
                                                               0,
                                                               task->tasklist->priv->icon_loader_data);

              if (loaded != NULL)
                {
                  pixbuf = wnck_task_scale_icon (loaded, FALSE);
                  g_object_unref (G_OBJECT (loaded));
                }
            }
        }

      if (pixbuf == NULL)
        {
          _wnck_get_fallback_icons (NULL, 0, 0,
                                    &pixbuf, MINI_ICON_SIZE, MINI_ICON_SIZE);
        }
#endif
      break;
    }

  return pixbuf;
}

static gboolean
wnck_task_get_needs_attention (WnckTask *task)
{
  GList *l;
  WnckTask *win_task;
  gboolean needs_attention;

  needs_attention = FALSE;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      task->start_needs_attention = 0;
      l = task->windows;
      while (l)
	{
	  win_task = WNCK_TASK (l->data);

	  if (wnck_window_or_transient_needs_attention (win_task->window))
	    {
	      needs_attention = TRUE;
              task->start_needs_attention = MAX (task->start_needs_attention, _wnck_window_or_transient_get_needs_attention_time (win_task->window));
	      break;
	    }

	  l = l->next;
	}
      break;

    case WNCK_TASK_WINDOW:
      needs_attention =
	wnck_window_or_transient_needs_attention (task->window);
      task->start_needs_attention = _wnck_window_or_transient_get_needs_attention_time (task->window);
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  return needs_attention != FALSE;
}

static void
wnck_task_update_visible_state (WnckTask *task)
{
  GdkPixbuf *pixbuf;
  char *text;

  pixbuf = wnck_task_get_icon (task);
  gtk_image_set_from_pixbuf (GTK_IMAGE (task->image),
			     pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);

  text = wnck_task_get_text (task, TRUE, TRUE);
  if (text != NULL)
    {
      gtk_label_set_text (GTK_LABEL (task->label), text);
      if (wnck_task_get_needs_attention (task))
        {
          _make_gtk_label_bold ((GTK_LABEL (task->label)));
          wnck_task_queue_glow (task);
        }
      else
        {
          _make_gtk_label_normal ((GTK_LABEL (task->label)));
          wnck_task_reset_glow (task);
        }
      g_free (text);
    }

  text = wnck_task_get_text (task, FALSE, FALSE);
  /* if text is NULL, this unsets the tooltip, which is probably what we'd want
   * to do */
  gtk_widget_set_tooltip_text (task->button, text);
  g_free (text);

  gtk_widget_queue_resize (GTK_WIDGET (task->tasklist));
}

static void
wnck_task_state_changed (WnckWindow     *window,
			 WnckWindowState changed_mask,
			 WnckWindowState new_state,
			 gpointer        data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);

  if (changed_mask & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    {
      wnck_tasklist_update_lists  (tasklist);
      gtk_widget_queue_resize (GTK_WIDGET (tasklist));
      return;
    }

  if ((changed_mask & WNCK_WINDOW_STATE_DEMANDS_ATTENTION) ||
      (changed_mask & WNCK_WINDOW_STATE_URGENT))
    {
      WnckWorkspace *active_workspace =
        wnck_screen_get_active_workspace (tasklist->priv->screen);

      if (active_workspace                              &&
          (active_workspace != wnck_window_get_workspace (window) ||
	   (wnck_workspace_is_virtual (active_workspace) &&
	    !wnck_window_is_in_viewport (window, active_workspace))))
        {
          wnck_tasklist_update_lists (tasklist);
          gtk_widget_queue_resize (GTK_WIDGET (tasklist));
        }
    }

  if ((changed_mask & WNCK_WINDOW_STATE_MINIMIZED)         ||
      (changed_mask & WNCK_WINDOW_STATE_DEMANDS_ATTENTION) ||
      (changed_mask & WNCK_WINDOW_STATE_URGENT))
    {
      WnckTask *win_task = NULL;

      /* FIXME: Handle group modal dialogs */
      for (; window && !win_task; window = wnck_window_get_transient (window))
        win_task = g_hash_table_lookup (tasklist->priv->win_hash, window);

      if (win_task)
	{
	  WnckTask *class_group_task;

	  wnck_task_update_visible_state (win_task);

	  class_group_task =
            g_hash_table_lookup (tasklist->priv->class_group_hash,
                                 win_task->class_group);

	  if (class_group_task)
	    wnck_task_update_visible_state (class_group_task);
	}
    }

}

static void
wnck_task_icon_changed (WnckWindow *window,
			gpointer    data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static void
wnck_task_name_changed (WnckWindow *window,
			gpointer    data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static void
wnck_task_class_name_changed (WnckClassGroup *class_group,
			      gpointer        data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static void
wnck_task_class_icon_changed (WnckClassGroup *class_group,
			      gpointer        data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static gboolean
wnck_task_motion_timeout (gpointer data)
{
  WnckWorkspace *ws;
  WnckTask *task = WNCK_TASK (data);

  task->button_activate = 0;

  /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
   * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
   * There should only be *one* activate call.
   */
  ws = wnck_window_get_workspace (task->window);
  if (ws && ws != wnck_screen_get_active_workspace (wnck_screen_get_default ()))
  {
    wnck_workspace_activate (ws, task->dnd_timestamp);
  }
  wnck_window_activate_transient (task->window, task->dnd_timestamp);

  task->dnd_timestamp = 0;

  return FALSE;
}

static void
wnck_task_drag_leave (GtkWidget          *widget,
		      GdkDragContext     *context,
		      guint               time,
		      WnckTask           *task)
{
  if (task->button_activate != 0)
    {
      g_source_remove (task->button_activate);
      task->button_activate = 0;
    }

  gtk_drag_unhighlight (widget);
}

static gboolean
wnck_task_drag_motion (GtkWidget          *widget,
		       GdkDragContext     *context,
		       gint                x,
		       gint                y,
		       guint               time,
		       WnckTask            *task)
{
  if (gtk_drag_dest_find_target (widget, context, NULL))
    {
      gtk_drag_highlight (widget);
#if GTK_CHECK_VERSION(2,21,0)
      gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), time);
#else
      gdk_drag_status (context, context->suggested_action, time);
#endif
    }
  else
    {
      task->dnd_timestamp = time;

      if (task->button_activate == 0 && task->type == WNCK_TASK_WINDOW)
        {
           task->button_activate = g_timeout_add_seconds (WNCK_ACTIVATE_TIMEOUT,
                                                          wnck_task_motion_timeout,
                                                          task);
        }

      gdk_drag_status (context, 0, time);
    }
  return TRUE;
}

static void
wnck_task_drag_begin (GtkWidget          *widget,
		      GdkDragContext     *context,
		      WnckTask           *task)
{
  _wnck_window_set_as_drag_icon (task->window, context,
                                 GTK_WIDGET (task->tasklist));

  task->tasklist->priv->drag_start_time = gtk_get_current_event_time ();
}

static void
wnck_task_drag_end (GtkWidget      *widget,
		    GdkDragContext *context,
		    WnckTask       *task)
{
  task->tasklist->priv->drag_start_time = 0;
}

static void
wnck_task_drag_data_get (GtkWidget          *widget,
		         GdkDragContext     *context,
		         GtkSelectionData   *selection_data,
		         guint               info,
		 	 guint               time,
		         WnckTask           *task)
{
  gulong xid;

  xid = wnck_window_get_xid (task->window);
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data),
			  8, (guchar *)&xid, sizeof (gulong));
}

static void
wnck_task_drag_data_received (GtkWidget          *widget,
                              GdkDragContext     *context,
                              gint                x,
                              gint                y,
                              GtkSelectionData   *data,
                              guint               info,
                              guint               time,
                              WnckTask           *target_task)
{
  WnckTasklist *tasklist;
  GList        *l, *windows;
  WnckWindow   *window;
  gulong       *xid;
  guint         new_order, old_order, order;
  WnckWindow   *found_window;

  if ((gtk_selection_data_get_length (data) != sizeof (gulong)) ||
      (gtk_selection_data_get_format (data) != 8))
    {
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  tasklist = target_task->tasklist;
  xid = (gulong *) gtk_selection_data_get_data (data);
  found_window = NULL;
  new_order = 0;
  windows = wnck_screen_get_windows (tasklist->priv->screen);

  for (l = windows; l; l = l->next)
    {
       window = WNCK_WINDOW (l->data);
       if (wnck_window_get_xid (window) == *xid)
         {
            old_order = wnck_window_get_sort_order (window);
            new_order = wnck_window_get_sort_order (target_task->window);
            if (old_order < new_order)
              new_order++;
            found_window = window;
            break;
         }
    }

  if (target_task->window == found_window)
    {
      GtkSettings  *settings;
      gint          double_click_time;

      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (tasklist)));
      double_click_time = 0;
      g_object_get (G_OBJECT (settings),
                    "gtk-double-click-time", &double_click_time,
                    NULL);

      if ((time - tasklist->priv->drag_start_time) < double_click_time)
        {
          wnck_tasklist_activate_task_window (target_task, time);
          gtk_drag_finish (context, TRUE, FALSE, time);
          return;
        }
    }

  if (found_window)
    {
       for (l = windows; l; l = l->next)
         {
            window = WNCK_WINDOW (l->data);
            order = wnck_window_get_sort_order (window);
            if (order >= new_order)
              wnck_window_set_sort_order (window, order + 1);
         }
       wnck_window_set_sort_order (found_window, new_order);

       if (!tasklist->priv->include_all_workspaces &&
           !wnck_window_is_pinned (found_window))
         {
           WnckWorkspace *active_space;
           active_space = wnck_screen_get_active_workspace (tasklist->priv->screen);
           wnck_window_move_to_workspace (found_window, active_space);
         }

       gtk_widget_queue_resize (GTK_WIDGET (tasklist));
    }

    gtk_drag_finish (context, TRUE, FALSE, time);
}

static gboolean
wnck_task_button_press_event (GtkWidget	      *widget,
			      GdkEventButton  *event,
			      gpointer         data)
{
  WnckTask *task = WNCK_TASK (data);

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (event->button == 2)
        wnck_tasklist_activate_next_in_class_group (task, event->time);
      else
        wnck_task_popup_menu (task,
                              event->button == 3);
      return TRUE;

    case WNCK_TASK_WINDOW:
      if (event->button == 1)
        {
          /* is_most_recently_activated == is_active for click &
	   * sloppy focus methods.  We use the former here because
	   * 'mouse' focus provides a special case.  In that case, no
	   * window will be active, but if a window was the most
	   * recently active one (i.e. user moves mouse straight from
	   * window to tasklist), then we should still minimize it.
           */
          if (wnck_window_is_most_recently_activated (task->window))
            task->was_active = TRUE;
          else
            task->was_active = FALSE;

          return FALSE;
        }
      else if (event->button == 2)
        {
          /* middle-click close window */
          if (task->tasklist->priv->middle_click_close == TRUE)
            {
              wnck_window_close (task->window, gtk_get_current_event_time ());
              return TRUE;
            }
        }
      else if (event->button == 3)
        {
          if (task->action_menu)
            gtk_widget_destroy (task->action_menu);

          g_assert (task->action_menu == NULL);

          task->action_menu = wnck_action_menu_new (task->window);

          g_object_add_weak_pointer (G_OBJECT (task->action_menu),
                                     (void**) &task->action_menu);

          gtk_menu_set_screen (GTK_MENU (task->action_menu),
                               _wnck_screen_get_gdk_screen (task->tasklist->priv->screen));

          gtk_widget_show (task->action_menu);
          gtk_menu_popup (GTK_MENU (task->action_menu),
                          NULL, NULL,
                          wnck_task_position_menu, task->button,
                          event->button,
                          gtk_get_current_event_time ());

          g_signal_connect (task->action_menu, "selection-done",
                            G_CALLBACK (gtk_widget_destroy), NULL);

          return TRUE;
        }
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  return FALSE;
}

static gboolean
wnck_task_scroll_event (GtkWidget *widget,
			GdkEvent  *event,
			gpointer   data)
{
  WnckTask *task = WNCK_TASK (data);

  return wnck_tasklist_scroll_event (GTK_WIDGET (task->tasklist), (GdkEventScroll *) event);
}

static gboolean
wnck_task_draw (GtkWidget *widget,
                cairo_t   *cr,
                gpointer   data);

static void
wnck_task_create_widgets (WnckTask *task, GtkReliefStyle relief)
{
  GtkWidget *hbox;
  GdkPixbuf *pixbuf;
  char *text;
  GtkCssProvider *provider;
  static const GtkTargetEntry targets[] = {
    { "application/x-wnck-window-id", 0, 0 }
  };

  if (task->type == WNCK_TASK_STARTUP_SEQUENCE)
    task->button = gtk_button_new ();
  else
    task->button = gtk_toggle_button_new ();

  gtk_button_set_relief (GTK_BUTTON (task->button), relief);

  task->button_activate = 0;
  g_object_add_weak_pointer (G_OBJECT (task->button),
                             (void**) &task->button);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   "#tasklist-button {\n"
                                   " -GtkWidget-focus-line-width: 0px;\n"
                                   " -GtkWidget-focus-padding: 0px;\n"
                                   "}",
                                   -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (task->button),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  gtk_widget_set_name (task->button,
		       "tasklist-button");

  if (task->type == WNCK_TASK_WINDOW)
    {
      gtk_drag_source_set (GTK_WIDGET (task->button),
                           GDK_BUTTON1_MASK,
                           targets, 1,
                           GDK_ACTION_MOVE);
      gtk_drag_dest_set (GTK_WIDGET (task->button), GTK_DEST_DEFAULT_DROP,
                         targets, 1, GDK_ACTION_MOVE);
    }
  else
    gtk_drag_dest_set (GTK_WIDGET (task->button), 0,
                       NULL, 0, GDK_ACTION_DEFAULT);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  pixbuf = wnck_task_get_icon (task);
  if (pixbuf)
    {
      task->image = gtk_image_new_from_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }
  else
    task->image = gtk_image_new ();

  gtk_widget_show (task->image);

  text = wnck_task_get_text (task, TRUE, TRUE);
  task->label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (task->label), 0.0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (task->label),
                          PANGO_ELLIPSIZE_END);

  if (wnck_task_get_needs_attention (task))
    {
      _make_gtk_label_bold ((GTK_LABEL (task->label)));
      wnck_task_queue_glow (task);
    }

  gtk_widget_show (task->label);

  gtk_box_pack_start (GTK_BOX (hbox), task->image, FALSE, FALSE,
		      TASKLIST_BUTTON_PADDING);
  gtk_box_pack_start (GTK_BOX (hbox), task->label, TRUE, TRUE,
		      TASKLIST_BUTTON_PADDING);

  gtk_container_add (GTK_CONTAINER (task->button), hbox);
  gtk_widget_show (hbox);
  g_free (text);

  text = wnck_task_get_text (task, FALSE, FALSE);
  gtk_widget_set_tooltip_text (task->button, text);
  g_free (text);

  /* Set up signals */
  if (GTK_IS_TOGGLE_BUTTON (task->button))
    g_signal_connect_object (G_OBJECT (task->button), "toggled",
                             G_CALLBACK (wnck_task_button_toggled),
                             G_OBJECT (task),
                             0);

  g_signal_connect_object (G_OBJECT (task->button), "size_allocate",
                           G_CALLBACK (wnck_task_size_allocated),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT (task->button), "button_press_event",
                           G_CALLBACK (wnck_task_button_press_event),
                           G_OBJECT (task),
                           0);

  gtk_widget_add_events (task->button, GDK_SCROLL_MASK);
  g_signal_connect_object (G_OBJECT (task->button), "scroll_event",
                           G_CALLBACK (wnck_task_scroll_event),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT(task->button), "drag_motion",
                           G_CALLBACK (wnck_task_drag_motion),
                           G_OBJECT (task),
                           0);

  if (task->type == WNCK_TASK_WINDOW)
    {
      g_signal_connect_object (G_OBJECT (task->button), "drag_data_received",
                               G_CALLBACK (wnck_task_drag_data_received),
                               G_OBJECT (task),
                               0);

    }

  g_signal_connect_object (G_OBJECT(task->button), "drag_leave",
                           G_CALLBACK (wnck_task_drag_leave),
                           G_OBJECT (task),
                           0);

  if (task->type == WNCK_TASK_WINDOW) {
      g_signal_connect_object (G_OBJECT(task->button), "drag_data_get",
                               G_CALLBACK (wnck_task_drag_data_get),
                               G_OBJECT (task),
                               0);

      g_signal_connect_object (G_OBJECT(task->button), "drag_begin",
                               G_CALLBACK (wnck_task_drag_begin),
                               G_OBJECT (task),
                               0);

      g_signal_connect_object (G_OBJECT(task->button), "drag_end",
                               G_CALLBACK (wnck_task_drag_end),
                               G_OBJECT (task),
                               0);
  }

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      task->class_name_changed_tag = g_signal_connect (G_OBJECT (task->class_group), "name_changed",
						       G_CALLBACK (wnck_task_class_name_changed), task);
      task->class_icon_changed_tag = g_signal_connect (G_OBJECT (task->class_group), "icon_changed",
						       G_CALLBACK (wnck_task_class_icon_changed), task);
      break;

    case WNCK_TASK_WINDOW:
      task->state_changed_tag = g_signal_connect (G_OBJECT (task->window), "state_changed",
                                                  G_CALLBACK (wnck_task_state_changed), task->tasklist);
      task->icon_changed_tag = g_signal_connect (G_OBJECT (task->window), "icon_changed",
                                                 G_CALLBACK (wnck_task_icon_changed), task);
      task->name_changed_tag = g_signal_connect (G_OBJECT (task->window), "name_changed",
						 G_CALLBACK (wnck_task_name_changed), task);
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
      break;

    default:
      g_assert_not_reached ();
    }

  g_signal_connect_object (task->button, "draw",
                           G_CALLBACK (wnck_task_draw),
                           G_OBJECT (task),
                           G_CONNECT_AFTER);
}

#define ARROW_SPACE 4
#define ARROW_SIZE 12
#define INDICATOR_SIZE 7

static gboolean
wnck_task_draw (GtkWidget *widget,
                cairo_t   *cr,
                gpointer   data)
{
  int x, y;
  WnckTask *task;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkWidget    *tasklist_widget;
  gint width, height;
  gboolean overlay_rect;
  gint arrow_width;
  gint arrow_height;
  GdkRGBA color;

  task = WNCK_TASK (data);

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      context = gtk_widget_get_style_context (widget);

      gtk_style_context_get_padding (context, gtk_widget_get_state_flags (widget), &padding);
      state = (task->tasklist->priv->active_class_group == task) ?
              GTK_STATE_FLAG_ACTIVE : GTK_STATE_FLAG_NORMAL;
      gtk_style_context_get_color (context, state, &color);

      x = gtk_widget_get_allocated_width (widget) -
          (gtk_container_get_border_width (GTK_CONTAINER (widget)) + padding.right + ARROW_SIZE);
      y = gtk_widget_get_allocated_height (widget) / 2;

      arrow_width = INDICATOR_SIZE + ((INDICATOR_SIZE % 2) - 1);
      arrow_height = arrow_width / 2 + 1;
      x += (ARROW_SIZE - arrow_width) / 2;
      y -= (2 * arrow_height + ARROW_SPACE) / 2;

      cairo_save (cr);
      gdk_cairo_set_source_rgba (cr, &color);

      /* Up arrow */
      cairo_move_to (cr, x, y + arrow_height);
      cairo_line_to (cr, x + arrow_width / 2., y);
      cairo_line_to (cr, x + arrow_width, y + arrow_height);
      cairo_close_path (cr);
      cairo_fill (cr);

      /* Down arrow */
      y += arrow_height + ARROW_SPACE;
      cairo_move_to (cr, x, y);
      cairo_line_to (cr, x + arrow_width, y);
      cairo_line_to (cr, x + arrow_width / 2., y + arrow_height);
      cairo_close_path (cr);
      cairo_fill (cr);

      cairo_restore (cr);

      break;

    case WNCK_TASK_WINDOW:
    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  if (task->glow_factor == 0.0)
    return FALSE;

  /* push a translucent overlay to paint to, so we can blend later */
  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);

  width = gtk_widget_get_allocated_width (task->button);
  height = gtk_widget_get_allocated_height (task->button);

  tasklist_widget = GTK_WIDGET (task->tasklist);

  context = gtk_widget_get_style_context (task->button);

  /* first draw the button */
  gtk_widget_style_get (tasklist_widget, "fade-overlay-rect", &overlay_rect, NULL);
  if (overlay_rect)
    {
      GdkRGBA bg_color;

      /* Draw a rectangle with selected background color */
      gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED, &bg_color);
      gdk_cairo_set_source_rgba (cr, &bg_color);
      cairo_paint (cr);
    }
  else
    {
      gtk_style_context_save (context);
      gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

      cairo_save (cr);
      gtk_render_background (context, cr, 0, 0, width, height);
      gtk_render_frame (context, cr, 0, 0, width, height);
      cairo_restore (cr);

      gtk_style_context_restore (context);
    }

  /* then the contents */
  gtk_container_propagate_draw (GTK_CONTAINER (task->button),
                                gtk_bin_get_child (GTK_BIN (task->button)),
                                cr);
  /* finally blend it */
  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, task->glow_factor);

  return FALSE;
}

static gint
wnck_task_compare_alphabetically (gconstpointer a,
                                  gconstpointer b)
{
  char *text1;
  char *text2;
  gint  result;

  text1 = wnck_task_get_text (WNCK_TASK (a), TRUE, FALSE);
  text2 = wnck_task_get_text (WNCK_TASK (b), TRUE, FALSE);

  result= g_utf8_collate (text1, text2);

  g_free (text1);
  g_free (text2);

  return result;
}

static gint
compare_class_group_tasks (WnckTask *task1, WnckTask *task2)
{
  const char *name1, *name2;

  name1 = wnck_class_group_get_name (task1->class_group);
  name2 = wnck_class_group_get_name (task2->class_group);

  return g_utf8_collate (name1, name2);
}

static gint
wnck_task_compare (gconstpointer  a,
		   gconstpointer  b)
{
  WnckTask *task1 = WNCK_TASK (a);
  WnckTask *task2 = WNCK_TASK (b);
  gint pos1, pos2;

  pos1 = pos2 = 0;  /* silence the compiler */

  switch (task1->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (task2->type == WNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return -1; /* Sort groups before everything else */

    case WNCK_TASK_WINDOW:
      pos1 = wnck_window_get_sort_order (task1->window);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      pos1 = G_MAXINT; /* startup sequences are sorted at the end. */
      break;           /* Changing this will break scrolling.      */
    }

  switch (task2->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (task1->type == WNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return 1; /* Sort groups before everything else */

    case WNCK_TASK_WINDOW:
      pos2 = wnck_window_get_sort_order (task2->window);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      pos2 = G_MAXINT;
      break;
    }

  if (pos1 < pos2)
    return -1;
  else if (pos1 > pos2)
    return 1;
  else
    return 0; /* should only happen if there's multiple processes being
               * started, and then who cares about sort order... */
}

static void
remove_startup_sequences_for_window (WnckTasklist *tasklist,
                                     WnckWindow   *window)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  const char *win_id;
  GList *tmp;

  win_id = _wnck_window_get_startup_id (window);
  if (win_id == NULL)
    return;

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = tmp->data;
      GList *next = tmp->next;
      const char *task_id;

      g_assert (task->type == WNCK_TASK_STARTUP_SEQUENCE);

      task_id = sn_startup_sequence_get_id (task->startup_sequence);

      if (task_id && strcmp (task_id, win_id) == 0)
        gtk_widget_destroy (task->button);

      tmp = next;
    }
#else
  ; /* nothing */
#endif
}

static WnckTask *
wnck_task_new_from_window (WnckTasklist *tasklist,
			   WnckWindow   *window)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);
  task->type = WNCK_TASK_WINDOW;
  task->window = g_object_ref (window);
  task->class_group = g_object_ref (wnck_window_get_class_group (window));
  task->tasklist = tasklist;

  wnck_task_create_widgets (task, tasklist->priv->relief);

  remove_startup_sequences_for_window (tasklist, window);

  return task;
}

static WnckTask *
wnck_task_new_from_class_group (WnckTasklist   *tasklist,
				WnckClassGroup *class_group)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->type = WNCK_TASK_CLASS_GROUP;
  task->window = NULL;
  task->class_group = g_object_ref (class_group);
  task->tasklist = tasklist;

  wnck_task_create_widgets (task, tasklist->priv->relief);

  return task;
}

#ifdef HAVE_STARTUP_NOTIFICATION
static WnckTask*
wnck_task_new_from_startup_sequence (WnckTasklist      *tasklist,
                                     SnStartupSequence *sequence)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->type = WNCK_TASK_STARTUP_SEQUENCE;
  task->window = NULL;
  task->class_group = NULL;
  task->startup_sequence = sequence;
  sn_startup_sequence_ref (task->startup_sequence);
  task->tasklist = tasklist;

  wnck_task_create_widgets (task, tasklist->priv->relief);

  return task;
}

/* This should be fairly long, as it should never be required unless
 * apps or .desktop files are buggy, and it's confusing if
 * OpenOffice or whatever seems to stop launching - people
 * might decide they need to launch it again.
 */
#define STARTUP_TIMEOUT 15000

static gboolean
sequence_timeout_callback (void *user_data)
{
  WnckTasklist *tasklist = user_data;
  GList *tmp;
  GTimeVal now;
  long tv_sec, tv_usec;
  double elapsed;

  g_get_current_time (&now);

 restart:
  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);

      sn_startup_sequence_get_last_active_time (task->startup_sequence,
                                                &tv_sec, &tv_usec);

      elapsed =
        ((((double)now.tv_sec - tv_sec) * G_USEC_PER_SEC +
          (now.tv_usec - tv_usec))) / 1000.0;

      if (elapsed > STARTUP_TIMEOUT)
        {
          g_assert (task->button != NULL);
          /* removes task from list as a side effect */
          gtk_widget_destroy (task->button);

          goto restart; /* don't iterate over changed list, just restart;
                         * not efficient but who cares here.
                         */
        }

      tmp = tmp->next;
    }

  if (tasklist->priv->startup_sequences == NULL)
    {
      tasklist->priv->startup_sequence_timeout = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static void
wnck_tasklist_sn_event (SnMonitorEvent *event,
                        void           *user_data)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (user_data);

  switch (sn_monitor_event_get_type (event))
    {
    case SN_MONITOR_EVENT_INITIATED:
      {
        WnckTask *task;

        task = wnck_task_new_from_startup_sequence (tasklist,
                                                    sn_monitor_event_get_startup_sequence (event));

        gtk_widget_set_parent (task->button, GTK_WIDGET (tasklist));
        gtk_widget_show (task->button);

        tasklist->priv->startup_sequences =
          g_list_prepend (tasklist->priv->startup_sequences,
                          task);

        if (tasklist->priv->startup_sequence_timeout == 0)
          {
            tasklist->priv->startup_sequence_timeout =
              g_timeout_add_seconds (1, sequence_timeout_callback,
                                     tasklist);
          }

        gtk_widget_queue_resize (GTK_WIDGET (tasklist));
      }
      break;

    case SN_MONITOR_EVENT_COMPLETED:
      {
        GList *tmp;
        tmp = tasklist->priv->startup_sequences;
        while (tmp != NULL)
          {
            WnckTask *task = WNCK_TASK (tmp->data);

            if (task->startup_sequence ==
                sn_monitor_event_get_startup_sequence (event))
              {
                g_assert (task->button != NULL);
                /* removes task from list as a side effect */
                gtk_widget_destroy (task->button);
                break;
              }

            tmp = tmp->next;
          }
      }
      break;

    case SN_MONITOR_EVENT_CHANGED:
      break;

    case SN_MONITOR_EVENT_CANCELED:
      break;
    }

  if (tasklist->priv->startup_sequences == NULL &&
      tasklist->priv->startup_sequence_timeout != 0)
    {
      g_source_remove (tasklist->priv->startup_sequence_timeout);
      tasklist->priv->startup_sequence_timeout = 0;
    }
}

static void
wnck_tasklist_check_end_sequence (WnckTasklist   *tasklist,
                                  WnckWindow     *window)
{
  const char *res_class;
  const char *res_name;
  GList *tmp;

  if (tasklist->priv->startup_sequences == NULL)
    return;

  res_class = wnck_window_get_class_group_name (window);
  res_name = wnck_window_get_class_instance_name (window);

  if (res_class == NULL && res_name == NULL)
    return;

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      const char *wmclass;

      wmclass = sn_startup_sequence_get_wmclass (task->startup_sequence);

      if (wmclass != NULL &&
          ((res_class && strcmp (res_class, wmclass) == 0) ||
           (res_name && strcmp (res_name, wmclass) == 0)))
        {
          sn_startup_sequence_complete (task->startup_sequence);

          g_assert (task->button != NULL);
          /* removes task from list as a side effect */
          gtk_widget_destroy (task->button);

          /* only match one */
          return;
        }

      tmp = tmp->next;
    }
}

#endif /* HAVE_STARTUP_NOTIFICATION */
