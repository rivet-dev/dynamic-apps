/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-tree-pane.h>
#include <thunar/thunar-tree-view.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
};



static void          thunar_tree_pane_component_init        (ThunarComponentIface *iface);
static void          thunar_tree_pane_navigator_init        (ThunarNavigatorIface *iface);
static void          thunar_tree_pane_side_pane_init        (ThunarSidePaneIface  *iface);
static void          thunar_tree_pane_dispose               (GObject              *object);
static void          thunar_tree_pane_get_property          (GObject              *object,
                                                             guint                 prop_id,
                                                             GValue               *value,
                                                             GParamSpec           *pspec);
static void          thunar_tree_pane_set_property          (GObject              *object,
                                                             guint                 prop_id,
                                                             const GValue         *value,
                                                             GParamSpec           *pspec);
static ThunarFile   *thunar_tree_pane_get_current_directory (ThunarNavigator      *navigator);
static void          thunar_tree_pane_set_current_directory (ThunarNavigator      *navigator,
                                                             ThunarFile           *current_directory);
static gboolean      thunar_tree_pane_get_show_hidden       (ThunarSidePane       *side_pane);
static void          thunar_tree_pane_set_show_hidden       (ThunarSidePane       *side_pane,
                                                             gboolean              show_hidden);



struct _ThunarTreePaneClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarTreePane
{
  GtkScrolledWindow __parent__;

  ThunarFile *current_directory;
  GtkWidget  *view;
  gboolean    show_hidden;
};



G_DEFINE_TYPE_WITH_CODE (ThunarTreePane, thunar_tree_pane, GTK_TYPE_SCROLLED_WINDOW,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_tree_pane_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_tree_pane_component_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_SIDE_PANE, thunar_tree_pane_side_pane_init))



static void
thunar_tree_pane_class_init (ThunarTreePaneClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_tree_pane_dispose;
  gobject_class->get_property = thunar_tree_pane_get_property;
  gobject_class->set_property = thunar_tree_pane_set_property;

  /* override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");

  /* override ThunarSidePane's properties */
  g_object_class_override_property (gobject_class, PROP_SHOW_HIDDEN, "show-hidden");
}



static void
thunar_tree_pane_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = NULL;
  iface->set_selected_files = NULL;
}



static void
thunar_tree_pane_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_tree_pane_get_current_directory;
  iface->set_current_directory = thunar_tree_pane_set_current_directory;
}



static void
thunar_tree_pane_side_pane_init (ThunarSidePaneIface *iface)
{
  iface->get_show_hidden = thunar_tree_pane_get_show_hidden;
  iface->set_show_hidden = thunar_tree_pane_set_show_hidden;
}



static void
thunar_tree_pane_init (ThunarTreePane *tree_pane)
{
  /* configure the GtkScrolledWindow */
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (tree_pane), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (tree_pane), NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (tree_pane), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_pane), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /* allocate the tree view */
  tree_pane->view = thunar_tree_view_new ();
  g_object_bind_property (G_OBJECT (tree_pane), "show-hidden", G_OBJECT (tree_pane->view), "show-hidden", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (tree_pane), "current-directory", G_OBJECT (tree_pane->view), "current-directory", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (tree_pane->view), "change-directory", G_CALLBACK (thunar_navigator_change_directory), tree_pane);
  g_signal_connect_swapped (G_OBJECT (tree_pane->view), "open-new-tab", G_CALLBACK (thunar_navigator_open_new_tab), tree_pane);
  gtk_container_add (GTK_CONTAINER (tree_pane), tree_pane->view);
  gtk_widget_show (tree_pane->view);
}



static void
thunar_tree_pane_dispose (GObject *object)
{
  ThunarTreePane *tree_pane = THUNAR_TREE_PANE (object);

  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (tree_pane), NULL);
  thunar_component_set_selected_files (THUNAR_COMPONENT (tree_pane), NULL);

  (*G_OBJECT_CLASS (thunar_tree_pane_parent_class)->dispose) (object);
}



static void
thunar_tree_pane_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_side_pane_get_show_hidden (THUNAR_SIDE_PANE (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_tree_pane_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (object), g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_tree_pane_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_TREE_PANE (navigator)->current_directory;
}



static void
thunar_tree_pane_set_current_directory (ThunarNavigator *navigator,
                                        ThunarFile      *current_directory)
{
  ThunarTreePane *tree_pane = THUNAR_TREE_PANE (navigator);

  /* disconnect from the previously set current directory */
  if (G_LIKELY (tree_pane->current_directory != NULL))
    g_object_unref (G_OBJECT (tree_pane->current_directory));

  /* activate the new directory */
  tree_pane->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    g_object_ref (G_OBJECT (current_directory));

  /* notify listeners */
  g_object_notify (G_OBJECT (tree_pane), "current-directory");
}



static gboolean
thunar_tree_pane_get_show_hidden (ThunarSidePane *side_pane)
{
  return THUNAR_TREE_PANE (side_pane)->show_hidden;
}



static void
thunar_tree_pane_set_show_hidden (ThunarSidePane *side_pane,
                                  gboolean        show_hidden)
{
  ThunarTreePane *tree_pane = THUNAR_TREE_PANE (side_pane);

  show_hidden = !!show_hidden;

  /* check if we have a new setting */
  if (G_UNLIKELY (tree_pane->show_hidden != show_hidden))
    {
      /* remember the new setting */
      tree_pane->show_hidden = show_hidden;

      /* notify listeners */
      g_object_notify (G_OBJECT (tree_pane), "show-hidden");
    }
}


