/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo-config.h>
#include <exo/exo-private.h>
#include <exo/exo-tree-view.h>
#include <exo/exo-utils.h>
#include <exo/exo-alias.h>
#include <libxfce4util/libxfce4util.h>

/**
 * SECTION: exo-tree-view
 * @title: ExoTreeView
 * @short_description: An improved version of #GtkTreeView
 * @include: exo/exo.h
 *
 * The #ExoTreeView class derives from #GtkTreeView and extends it with
 * the ability to activate rows using single button clicks instead of
 * the default double button clicks. It also works around a few shortcomings
 * of #GtkTreeView, i.e. #ExoTreeView allows the user to drag around multiple
 * selected rows.
 **/



/* resurrect dead gdk apis for Gtk3
 * This is easier than using #ifs everywhere
 */
#ifdef gdk_cursor_unref
#undef gdk_cursor_unref
#endif
#define gdk_cursor_unref(cursor) g_object_unref (cursor)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_SINGLE_CLICK,
  PROP_SINGLE_CLICK_TIMEOUT,
};



static void     exo_tree_view_finalize                      (GObject          *object);
static void     exo_tree_view_get_property                  (GObject          *object,
                                                             guint             prop_id,
                                                             GValue           *value,
                                                             GParamSpec       *pspec);
static void     exo_tree_view_set_property                  (GObject          *object,
                                                             guint             prop_id,
                                                             const GValue     *value,
                                                             GParamSpec       *pspec);
static gboolean exo_tree_view_button_press_event            (GtkWidget        *widget,
                                                             GdkEventButton   *event);
static gboolean exo_tree_view_button_release_event          (GtkWidget        *widget,
                                                             GdkEventButton   *event);
static gboolean exo_tree_view_motion_notify_event           (GtkWidget        *widget,
                                                             GdkEventMotion   *event);
static gboolean exo_tree_view_leave_notify_event            (GtkWidget        *widget,
                                                             GdkEventCrossing *event);
static void     exo_tree_view_drag_begin                    (GtkWidget        *widget,
                                                             GdkDragContext   *context);
static gboolean exo_tree_view_move_cursor                   (GtkTreeView      *view,
                                                             GtkMovementStep   step,
                                                             gint              count);
static gboolean exo_tree_view_single_click_timeout          (gpointer          user_data);
static void     exo_tree_view_single_click_timeout_destroy  (gpointer          user_data);
static gboolean select_true                                 (GtkTreeSelection *selection,
                                                             GtkTreeModel     *model,
                                                             GtkTreePath      *path,
                                                             gboolean          path_currently_selected,
                                                             gpointer          data);
static gboolean select_false                                (GtkTreeSelection *selection,
                                                             GtkTreeModel     *model,
                                                             GtkTreePath      *path,
                                                             gboolean          path_currently_selected,
                                                             gpointer          data);



struct _ExoTreeViewPrivate
{
  /* whether the next button-release-event should emit "row-activate" */
  guint        button_release_activates : 1;

  /* whether drag and drop must be re-enabled on button-release-event (rubberbanding active) */
  guint        button_release_unblocks_dnd : 1;

  /* whether rubberbanding must be re-enabled on button-release-event (drag and drop active) */
  guint        button_release_enables_rubber_banding : 1;

  /* single click mode */
  guint        single_click : 1;
  guint        single_click_timeout;
  gint         single_click_timeout_id;
  guint        single_click_timeout_state;

  /* the path below the pointer or NULL */
  GtkTreePath *hover_path;
};



G_DEFINE_TYPE_WITH_PRIVATE (ExoTreeView, exo_tree_view, GTK_TYPE_TREE_VIEW)



static void
exo_tree_view_class_init (ExoTreeViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;
  GtkWidgetClass   *gtkwidget_class;
  GObjectClass     *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_tree_view_finalize;
  gobject_class->get_property = exo_tree_view_get_property;
  gobject_class->set_property = exo_tree_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->button_press_event = exo_tree_view_button_press_event;
  gtkwidget_class->button_release_event = exo_tree_view_button_release_event;
  gtkwidget_class->motion_notify_event = exo_tree_view_motion_notify_event;
  gtkwidget_class->leave_notify_event = exo_tree_view_leave_notify_event;
  gtkwidget_class->drag_begin = exo_tree_view_drag_begin;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->move_cursor = exo_tree_view_move_cursor;

  /* initialize the library's i18n support */
  _exo_i18n_init ();

  /**
   * ExoTreeView:single-click:
   *
   * %TRUE to activate items using a single click instead of a
   * double click.
   *
   * Since: 0.3.1.3
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SINGLE_CLICK,
                                   g_param_spec_boolean ("single-click",
                                                         _("Single Click"),
                                                         _("Whether the items in the view can be activated with single clicks"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ExoTreeView:single-click-timeout:
   *
   * The amount of time in milliseconds after which the hover row (the row
   * which is hovered by the mouse cursor) will be selected automatically
   * in single-click mode. A value of %0 disables the automatic selection.
   *
   * Since: 0.3.1.5
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SINGLE_CLICK_TIMEOUT,
                                   g_param_spec_uint ("single-click-timeout",
                                                      _("Single Click Timeout"),
                                                      _("The amount of time after which the item under the mouse cursor will be selected automatically in single click mode"),
                                                      0, G_MAXUINT, 0,
                                                      EXO_PARAM_READWRITE));
}



static void
exo_tree_view_init (ExoTreeView *tree_view)
{
  /* grab a pointer on the private data */
  tree_view->priv = exo_tree_view_get_instance_private (tree_view);
  tree_view->priv->single_click_timeout_id = -1;
}



static void
exo_tree_view_finalize (GObject *object)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (object);

  /* be sure to cancel any single-click timeout */
  if (G_UNLIKELY (tree_view->priv->single_click_timeout_id >= 0))
    g_source_remove (tree_view->priv->single_click_timeout_id);

  /* be sure to release the hover path */
  if (G_UNLIKELY (tree_view->priv->hover_path != NULL))
    gtk_tree_path_free (tree_view->priv->hover_path);

  (*G_OBJECT_CLASS (exo_tree_view_parent_class)->finalize) (object);
}



static void
exo_tree_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (object);

  switch (prop_id)
    {
    case PROP_SINGLE_CLICK:
      g_value_set_boolean (value, exo_tree_view_get_single_click (tree_view));
      break;

    case PROP_SINGLE_CLICK_TIMEOUT:
      g_value_set_uint (value, exo_tree_view_get_single_click_timeout (tree_view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_tree_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (object);

  switch (prop_id)
    {
    case PROP_SINGLE_CLICK:
      exo_tree_view_set_single_click (tree_view, g_value_get_boolean (value));
      break;

    case PROP_SINGLE_CLICK_TIMEOUT:
      exo_tree_view_set_single_click_timeout (tree_view, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
exo_tree_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  GtkTreeSelection *selection;
  ExoTreeView      *tree_view = EXO_TREE_VIEW (widget);
  GtkTreePath      *path = NULL;
  gboolean          result;
  gpointer          drag_data;

  /* by default we won't emit "row-activated" on button-release-events */
  tree_view->priv->button_release_activates = FALSE;

  /* grab the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  /* be sure to cancel any pending single-click timeout */
  if (G_UNLIKELY (tree_view->priv->single_click_timeout_id >= 0))
    g_source_remove (tree_view->priv->single_click_timeout_id);

  /* check if the button press was on the internal tree view window */
  if (G_LIKELY (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view))))
    {
      /* determine the path at the event coordinates */
      if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL))
        path = NULL;

      /* we unselect all selected items if the user clicks on an empty
       * area of the tree view and no modifier key is active.
       */
      if (path == NULL && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        gtk_tree_selection_unselect_all (selection);

      /* completely ignore double-clicks in single-click mode */
      if (tree_view->priv->single_click && event->type == GDK_2BUTTON_PRESS)
        {
          /* make sure we ignore the GDK_BUTTON_RELEASE
           * event for this GDK_2BUTTON_PRESS event.
           */
          gtk_tree_path_free (path);
          return TRUE;
        }

      /* check if the next button-release-event should activate the selected row (single click support) */
      tree_view->priv->button_release_activates = (tree_view->priv->single_click && event->type == GDK_BUTTON_PRESS && event->button == 1
                                                   && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0);
    }

  /* Rubberbanding in GtkTreeView 2.9.0 and above is rather buggy, unfortunately, and
   * doesn't interact properly with GTKs own DnD mechanism. So we need to block all
   * dragging here when pressing the mouse button on a not yet selected row if
   * rubberbanding is active, or disable rubberbanding when starting a drag.
   */
  if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE
      && gtk_tree_view_get_rubber_banding (GTK_TREE_VIEW (tree_view))
      && event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      /* check if clicked on empty area or on a not yet selected row */
      if (G_LIKELY (path == NULL || !gtk_tree_selection_path_is_selected (selection, path)))
        {
          /* need to disable drag and drop because we're rubberbanding now */
          drag_data = g_object_get_data (G_OBJECT (tree_view), I_("gtk-site-data"));
          if (G_LIKELY (drag_data != NULL))
            {
              g_signal_handlers_block_matched (G_OBJECT (tree_view),
                                               G_SIGNAL_MATCH_DATA,
                                               0, 0, NULL, NULL,
                                               drag_data);
            }

          /* remember to re-enable drag and drop later */
          tree_view->priv->button_release_unblocks_dnd = TRUE;
        }
      else
        {
          /* need to disable rubberbanding because we're dragging now */
          gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (tree_view), FALSE);

          /* remember to re-enable rubberbanding later */
          tree_view->priv->button_release_enables_rubber_banding = TRUE;
        }
    }

  /* unfortunately GtkTreeView will unselect rows except the clicked one,
   * which makes dragging from a GtkTreeView problematic.
   * So we temporary disable selection updates if the path is still selected
   */
  if (event->type == GDK_BUTTON_PRESS && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0
      && path != NULL && gtk_tree_selection_path_is_selected (selection, path))
    {
      gtk_tree_selection_set_select_function (selection, select_false, NULL, NULL);
    }

  /* call the parent's button press handler */
  result = (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->button_press_event) (widget, event);

  /* Note that since we have already "chained up" by calling the parent's button press handler,
   * we must re-grab the tree selection, since the old one might be corrupted
   */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  if (GTK_IS_TREE_SELECTION (selection))
    {
      /* Re-enable selection updates */
      if (G_LIKELY (gtk_tree_selection_get_select_function (selection) == select_false))
        {
          gtk_tree_selection_set_select_function (selection, select_true, NULL, NULL);
        }
    }

  /* release the path (if any) */
  if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return result;
}



static gboolean
exo_tree_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkTreePath       *path;
  ExoTreeView       *tree_view = EXO_TREE_VIEW (widget);
  gpointer           drag_data;

  /* verify that the release event is for the internal tree view window */
  if (G_LIKELY (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view))))
    {
      /* check if we're in single-click mode and the button-release-event should emit a "row-activate" */
      if (G_UNLIKELY (tree_view->priv->single_click && tree_view->priv->button_release_activates))
        {
          /* reset the "release-activates" flag */
          tree_view->priv->button_release_activates = FALSE;

          /* determine the path to the row that should be activated */
          if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, &column, NULL, NULL))
            {
              /* emit row-activated for the determined row */
              gtk_tree_view_row_activated (GTK_TREE_VIEW (tree_view), path, column);

              /* cleanup */
              gtk_tree_path_free (path);
            }
        }
      else if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0 && !tree_view->priv->button_release_unblocks_dnd)
        {
          /* determine the path on which the button was released and select only this row, this way
           * the user is still able to alter the selection easily if all rows are selected.
           */
          if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, &column, NULL, NULL))
            {
              /* check if the path is selected */
              selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
              if (gtk_tree_selection_path_is_selected (selection, path))
                {
                  /* unselect all rows */
                  gtk_tree_selection_unselect_all (selection);

                  /* select the row and place the cursor on it */
                  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, column, FALSE);
                }

              /* cleanup */
              gtk_tree_path_free (path);
            }
        }
    }

  /* check if we need to re-enable drag and drop */
  if (G_LIKELY (tree_view->priv->button_release_unblocks_dnd))
    {
      drag_data = g_object_get_data (G_OBJECT (tree_view), I_("gtk-site-data"));
      if (G_LIKELY (drag_data != NULL))
        {
          g_signal_handlers_unblock_matched (G_OBJECT (tree_view),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             drag_data);
        }
      tree_view->priv->button_release_unblocks_dnd = FALSE;
    }

  /* check if we need to re-enable rubberbanding */
  if (G_UNLIKELY (tree_view->priv->button_release_enables_rubber_banding))
    {
      gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (tree_view), TRUE);
      tree_view->priv->button_release_enables_rubber_banding = FALSE;
    }

  /* call the parent's button release handler */
  return (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->button_release_event) (widget, event);
}



static gboolean
exo_tree_view_motion_notify_event (GtkWidget      *widget,
                                   GdkEventMotion *event)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (widget);
  GtkTreePath *path;
  GdkCursor   *cursor;

  /* check if the event occurred on the tree view internal window and we are in single-click mode */
  if (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view)) && tree_view->priv->single_click)
    {
      /* check if we're doing a rubberband selection right now (which means DnD is blocked) */
      if (G_UNLIKELY (tree_view->priv->button_release_unblocks_dnd))
        {
          /* we're doing a rubberband selection, so don't activate anything */
          tree_view->priv->button_release_activates = FALSE;

          /* also be sure to reset the cursor */
          gdk_window_set_cursor (event->window, NULL);
        }
      else
        {
          /* determine the path at the event coordinates */
          if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL))
            path = NULL;

          /* check if we have a new path */
          if ((path == NULL && tree_view->priv->hover_path != NULL) || (path != NULL && tree_view->priv->hover_path == NULL)
              || (path != NULL && tree_view->priv->hover_path != NULL && gtk_tree_path_compare (path, tree_view->priv->hover_path) != 0))
            {
              /* release the previous hover path */
              if (tree_view->priv->hover_path != NULL)
                gtk_tree_path_free (tree_view->priv->hover_path);

              /* setup the new path */
              tree_view->priv->hover_path = path;

              /* check if we're over a row right now */
              if (G_LIKELY (path != NULL))
                {
                  /* setup the hand cursor to indicate that the row at the pointer can be activated with a single click */
                  cursor = gdk_cursor_new_for_display (gdk_window_get_display (event->window), GDK_HAND2);
                  gdk_window_set_cursor (event->window, cursor);
                  gdk_cursor_unref (cursor);
                }
              else
                {
                  /* reset the cursor to its default */
                  gdk_window_set_cursor (event->window, NULL);
                }

              /* check if autoselection is enabled and the pointer is over a row */
              if (G_LIKELY (tree_view->priv->single_click_timeout > 0 && tree_view->priv->hover_path != NULL))
                {
                  /* cancel any previous single-click timeout */
                  if (G_LIKELY (tree_view->priv->single_click_timeout_id >= 0))
                    g_source_remove (tree_view->priv->single_click_timeout_id);

                  /* remember the current event state */
                  tree_view->priv->single_click_timeout_state = event->state;

                  /* schedule a new single-click timeout */
                  tree_view->priv->single_click_timeout_id = gdk_threads_add_timeout_full (G_PRIORITY_LOW, tree_view->priv->single_click_timeout,
                                                                                           exo_tree_view_single_click_timeout, tree_view,
                                                                                           exo_tree_view_single_click_timeout_destroy);
                }
            }
          else
            {
              /* release the path resources */
              if (path != NULL)
                gtk_tree_path_free (path);
            }
        }
    }

  /* call the parent's motion notify handler */
  return (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->motion_notify_event) (widget, event);
}



static gboolean
exo_tree_view_leave_notify_event (GtkWidget        *widget,
                                  GdkEventCrossing *event)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (widget);

  /* be sure to cancel any pending single-click timeout */
  if (G_UNLIKELY (tree_view->priv->single_click_timeout_id >= 0))
    g_source_remove (tree_view->priv->single_click_timeout_id);

  /* release and reset the hover path (if any) */
  if (tree_view->priv->hover_path != NULL)
    {
      gtk_tree_path_free (tree_view->priv->hover_path);
      tree_view->priv->hover_path = NULL;
    }

  /* reset the cursor for the tree view internal window */
  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    gdk_window_set_cursor (gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view)), NULL);

  /* the next button-release-event should not activate */
  tree_view->priv->button_release_activates = FALSE;

  /* call the parent's leave notify handler */
  return (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->leave_notify_event) (widget, event);
}



static void
exo_tree_view_drag_begin (GtkWidget      *widget,
                          GdkDragContext *context)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (widget);

  /* the next button-release-event should not activate */
  tree_view->priv->button_release_activates = FALSE;

  /* call the parent's drag begin handler */
  (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->drag_begin) (widget, context);
}



static gboolean
exo_tree_view_move_cursor (GtkTreeView    *view,
                           GtkMovementStep step,
                           gint            count)
{
  ExoTreeView *tree_view = EXO_TREE_VIEW (view);

  /* be sure to cancel any pending single-click timeout */
  if (G_UNLIKELY (tree_view->priv->single_click_timeout_id >= 0))
    g_source_remove (tree_view->priv->single_click_timeout_id);

  /* release and reset the hover path (if any) */
  if (tree_view->priv->hover_path != NULL)
    {
      gtk_tree_path_free (tree_view->priv->hover_path);
      tree_view->priv->hover_path = NULL;
    }

  /* reset the cursor for the tree view internal window */
  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    gdk_window_set_cursor (gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view)), NULL);

  /* call the parent's handler */
  return (*GTK_TREE_VIEW_CLASS (exo_tree_view_parent_class)->move_cursor) (view, step, count);
}



static gboolean
exo_tree_view_single_click_timeout (gpointer user_data)
{
  GtkTreeViewColumn *cursor_column;
  GtkTreeSelection  *selection;
  GtkTreeModel      *model;
  GtkTreePath       *cursor_path;
  GtkTreeIter        iter;
  ExoTreeView       *tree_view = EXO_TREE_VIEW (user_data);
  gboolean           hover_path_selected;
  GtkWidget         *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tree_view));

  /* verify that we are in single-click mode on an active window and have a hover path */
  if (GTK_IS_WINDOW (toplevel) && gtk_window_is_active (GTK_WINDOW (toplevel)) && tree_view->priv->single_click && tree_view->priv->hover_path != NULL)
    {
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      /* transform the hover_path to a tree iterator */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
      if (model != NULL && gtk_tree_model_get_iter (model, &iter, tree_view->priv->hover_path))
        {
          /* determine the current cursor path/column */
          gtk_tree_view_get_cursor (GTK_TREE_VIEW (tree_view), &cursor_path, &cursor_column);

          /* be sure the row is fully visible */
          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (tree_view), tree_view->priv->hover_path, cursor_column, FALSE, 0.0f, 0.0f);

          /* determine the selection and change it appropriately */
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
          if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
            {
              /* just place the cursor on the row */
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), tree_view->priv->hover_path, cursor_column, FALSE);
            }
          else if ((tree_view->priv->single_click_timeout_state & GDK_SHIFT_MASK) != 0
                && gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE)
            {
              /* check if the item is not already selected (otherwise do nothing) */
              if (!gtk_tree_selection_path_is_selected (selection, tree_view->priv->hover_path))
                {
                  /* unselect all previously selected items */
                  gtk_tree_selection_unselect_all (selection);

                  /* since we cannot access the anchor of a GtkTreeView, we
                   * use the cursor instead which is usually the same row.
                   */
                  if (G_UNLIKELY (cursor_path == NULL))
                    {
                      /* place the cursor on the new row */
                      gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), tree_view->priv->hover_path, cursor_column, FALSE);
                    }
                  else
                    {
                      /* select all between the cursor and the current row */
                      gtk_tree_selection_select_range (selection, tree_view->priv->hover_path, cursor_path);
                    }
                }
            }
          else
            {
              /* check if the hover path is selected (as it will be selected after the set_cursor() call) */
              hover_path_selected = gtk_tree_selection_path_is_selected (selection, tree_view->priv->hover_path);
              /* disable selection updates if the path is still selected */
              gtk_tree_selection_set_select_function (selection, select_false, NULL, NULL);

              /* place the cursor on the hover row */
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), tree_view->priv->hover_path, cursor_column, FALSE);

              /* re-enable selection updates */
              gtk_tree_selection_set_select_function (selection, select_true, NULL, NULL);

              /* check what to do */
              if ((gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE ||
                  (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE && hover_path_selected))
                  && (tree_view->priv->single_click_timeout_state & GDK_CONTROL_MASK) != 0)
                {
                  /* toggle the selection state of the row */
                  if (G_UNLIKELY (hover_path_selected))
                    gtk_tree_selection_unselect_path (selection, tree_view->priv->hover_path);
                  else
                    gtk_tree_selection_select_path (selection, tree_view->priv->hover_path);
                }
              else if (G_UNLIKELY (!hover_path_selected))
                {
                  /* unselect all other rows */
                  gtk_tree_selection_unselect_all (selection);

                  /* select only the hover row */
                  gtk_tree_selection_select_path (selection, tree_view->priv->hover_path);
                }
            }

          /* cleanup */
          if (G_LIKELY (cursor_path != NULL))
            gtk_tree_path_free (cursor_path);
        }
    }

  return FALSE;
}



static void
exo_tree_view_single_click_timeout_destroy (gpointer user_data)
{
  EXO_TREE_VIEW (user_data)->priv->single_click_timeout_id = -1;
}



/**
 * exo_tree_view_new:
 *
 * Allocates a new #ExoTreeView instance.
 *
 * Returns: the newly allocated #ExoTreeView.
 *
 * Since: 0.3.1.3
 **/
GtkWidget*
exo_tree_view_new (void)
{
  return g_object_new (EXO_TYPE_TREE_VIEW, NULL);
}



/**
 * exo_tree_view_get_single_click:
 * @tree_view : an #ExoTreeView.
 *
 * Returns %TRUE if @tree_view is in single-click mode, else %FALSE.
 *
 * Returns: whether @tree_view is in single-click mode.
 *
 * Since: 0.3.1.3
 **/
gboolean
exo_tree_view_get_single_click (const ExoTreeView *tree_view)
{
  g_return_val_if_fail (EXO_IS_TREE_VIEW (tree_view), FALSE);
  return tree_view->priv->single_click;
}



/**
 * exo_tree_view_set_single_click:
 * @tree_view    : an #ExoTreeView.
 * @single_click : %TRUE to use single-click for @tree_view, %FALSE otherwise.
 *
 * If @single_click is %TRUE, @tree_view will use single-click mode, else
 * the default double-click mode will be used.
 *
 * Since: 0.3.1.3
 **/
void
exo_tree_view_set_single_click (ExoTreeView *tree_view,
                                gboolean     single_click)
{
  g_return_if_fail (EXO_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->single_click != !!single_click)
    {
      tree_view->priv->single_click = !!single_click;
      g_object_notify (G_OBJECT (tree_view), "single-click");
    }
}



/**
 * exo_tree_view_get_single_click_timeout:
 * @tree_view : a #ExoTreeView.
 *
 * Returns the amount of time in milliseconds after which the
 * item under the mouse cursor will be selected automatically
 * in single click mode. A value of %0 means that the behavior
 * is disabled and the user must alter the selection manually.
 *
 * Returns: the single click autoselect timeout or %0 if
 *               the behavior is disabled.
 *
 * Since: 0.3.1.5
 **/
guint
exo_tree_view_get_single_click_timeout (const ExoTreeView *tree_view)
{
  g_return_val_if_fail (EXO_IS_TREE_VIEW (tree_view), 0u);
  return tree_view->priv->single_click_timeout;
}



/**
 * exo_tree_view_set_single_click_timeout:
 * @tree_view            : a #ExoTreeView.
 * @single_click_timeout : the new timeout or %0 to disable.
 *
 * If @single_click_timeout is a value greater than zero, it specifies
 * the amount of time in milliseconds after which the item under the
 * mouse cursor will be selected automatically in single click mode.
 * A value of %0 for @single_click_timeout disables the autoselection
 * for @tree_view.
 *
 * This setting does not have any effect unless the @tree_view is in
 * single-click mode, see exo_tree_view_set_single_click().
 *
 * Since: 0.3.1.5
 **/
void
exo_tree_view_set_single_click_timeout (ExoTreeView *tree_view,
                                        guint        single_click_timeout)
{
  g_return_if_fail (EXO_IS_TREE_VIEW (tree_view));

  /* check if we have a new setting */
  if (tree_view->priv->single_click_timeout != single_click_timeout)
    {
      /* apply the new setting */
      tree_view->priv->single_click_timeout = single_click_timeout;

      /* be sure to cancel any pending single click timeout */
      if (G_UNLIKELY (tree_view->priv->single_click_timeout_id >= 0))
        g_source_remove (tree_view->priv->single_click_timeout_id);

      /* notify listeners */
      g_object_notify (G_OBJECT (tree_view), "single-click-timeout");
    }
}



static gboolean
select_true (GtkTreeSelection *selection,
             GtkTreeModel     *model,
             GtkTreePath      *path,
             gboolean          path_currently_selected,
             gpointer          data)
{
  return TRUE;
}



static gboolean
select_false (GtkTreeSelection *selection,
              GtkTreeModel     *model,
              GtkTreePath      *path,
              gboolean          path_currently_selected,
              gpointer          data)
{
  return FALSE;
}



#define __EXO_TREE_VIEW_C__
#include <exo/exo-aliasdef.c>
