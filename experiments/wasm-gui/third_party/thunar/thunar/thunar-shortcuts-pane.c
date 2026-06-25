/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-shortcuts-model.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-shortcuts-view.h>
#include <thunar/thunar-side-pane.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
};



static void          thunar_shortcuts_pane_component_init        (ThunarComponentIface     *iface);
static void          thunar_shortcuts_pane_navigator_init        (ThunarNavigatorIface     *iface);
static void          thunar_shortcuts_pane_side_pane_init        (ThunarSidePaneIface      *iface);
static void          thunar_shortcuts_pane_dispose               (GObject                  *object);
static void          thunar_shortcuts_pane_finalize              (GObject                  *object);
static void          thunar_shortcuts_pane_get_property          (GObject                  *object,
                                                                  guint                     prop_id,
                                                                  GValue                   *value,
                                                                  GParamSpec               *pspec);
static void          thunar_shortcuts_pane_set_property          (GObject                  *object,
                                                                  guint                     prop_id,
                                                                  const GValue             *value,
                                                                  GParamSpec               *pspec);
static ThunarFile   *thunar_shortcuts_pane_get_current_directory (ThunarNavigator          *navigator);
static void          thunar_shortcuts_pane_set_current_directory (ThunarNavigator          *navigator,
                                                                  ThunarFile               *current_directory);
static GList        *thunar_shortcuts_pane_get_selected_files    (ThunarComponent          *component);
static void          thunar_shortcuts_pane_set_selected_files    (ThunarComponent          *component,
                                                                  GList                    *selected_files);
static void          thunar_shortcuts_pane_show_shortcuts_view_padding (GtkWidget          *widget);
static void          thunar_shortcuts_pane_hide_shortcuts_view_padding (GtkWidget          *widget);



struct _ThunarShortcutsPaneClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarShortcutsPane
{
  GtkScrolledWindow __parent__;

  ThunarFile       *current_directory;

  /* #GList of currently selected #ThunarFile<!---->s */
  GList            *selected_files;

  GtkWidget        *view;

  guint             idle_select_directory;
};



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsPane, thunar_shortcuts_pane, GTK_TYPE_SCROLLED_WINDOW,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_shortcuts_pane_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_shortcuts_pane_component_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_SIDE_PANE, thunar_shortcuts_pane_side_pane_init))



static void
thunar_shortcuts_pane_class_init (ThunarShortcutsPaneClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_shortcuts_pane_dispose;
  gobject_class->finalize = thunar_shortcuts_pane_finalize;
  gobject_class->get_property = thunar_shortcuts_pane_get_property;
  gobject_class->set_property = thunar_shortcuts_pane_set_property;

  /* override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");

  /* override ThunarSidePane's properties */
  g_object_class_override_property (gobject_class, PROP_SHOW_HIDDEN, "show-hidden");
}



static void
thunar_shortcuts_pane_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_shortcuts_pane_get_selected_files;
  iface->set_selected_files = thunar_shortcuts_pane_set_selected_files;
}



static void
thunar_shortcuts_pane_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_shortcuts_pane_get_current_directory;
  iface->set_current_directory = thunar_shortcuts_pane_set_current_directory;
}



static void
thunar_shortcuts_pane_side_pane_init (ThunarSidePaneIface *iface)
{
  iface->get_show_hidden = NULL;
  iface->set_show_hidden = NULL;
}



static void
thunar_shortcuts_pane_init (ThunarShortcutsPane *shortcuts_pane)
{
  GtkWidget *vscrollbar;

  /* configure the GtkScrolledWindow */
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (shortcuts_pane), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (shortcuts_pane), NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (shortcuts_pane), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (shortcuts_pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  /* allocate the shortcuts view */
  shortcuts_pane->view = thunar_shortcuts_view_new ();
  gtk_container_add (GTK_CONTAINER (shortcuts_pane), shortcuts_pane->view);
  gtk_widget_show (shortcuts_pane->view);

  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (shortcuts_pane));
  g_signal_connect_swapped (G_OBJECT (vscrollbar), "map",
                            G_CALLBACK (thunar_shortcuts_pane_show_shortcuts_view_padding),
                            shortcuts_pane->view);
  g_signal_connect_swapped (G_OBJECT (vscrollbar), "unmap",
                            G_CALLBACK (thunar_shortcuts_pane_hide_shortcuts_view_padding),
                            shortcuts_pane->view);

  /* add widget to css class */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (shortcuts_pane)), "shortcuts-pane");

  g_object_bind_property (G_OBJECT (shortcuts_pane), "current-directory", G_OBJECT (shortcuts_pane->view), "current-directory", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (shortcuts_pane->view), "change-directory", G_CALLBACK (thunar_navigator_change_directory), shortcuts_pane);
  g_signal_connect_swapped (G_OBJECT (shortcuts_pane->view), "open_new_tab", G_CALLBACK (thunar_navigator_open_new_tab), shortcuts_pane);
}



static void
thunar_shortcuts_pane_dispose (GObject *object)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (object);

  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (shortcuts_pane), NULL);
  thunar_component_set_selected_files (THUNAR_COMPONENT (shortcuts_pane), NULL);

  (*G_OBJECT_CLASS (thunar_shortcuts_pane_parent_class)->dispose) (object);
}



static void
thunar_shortcuts_pane_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (thunar_shortcuts_pane_parent_class)->finalize) (object);
}



static void
thunar_shortcuts_pane_get_property (GObject    *object,
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
thunar_shortcuts_pane_set_property (GObject      *object,
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
thunar_shortcuts_pane_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_SHORTCUTS_PANE (navigator)->current_directory;
}



static gboolean
thunar_shortcuts_pane_set_current_directory_idle (gpointer data)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (data);

  if (shortcuts_pane->current_directory != NULL)
    {
      thunar_shortcuts_view_select_by_file (THUNAR_SHORTCUTS_VIEW (shortcuts_pane->view),
                                            shortcuts_pane->current_directory);
    }

  /* unset id */
  shortcuts_pane->idle_select_directory = 0;

  return FALSE;
}



static void
thunar_shortcuts_pane_set_current_directory (ThunarNavigator *navigator,
                                             ThunarFile      *current_directory)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (navigator);

  /* disconnect from the previously set current directory */
  if (G_LIKELY (shortcuts_pane->current_directory != NULL))
    g_object_unref (G_OBJECT (shortcuts_pane->current_directory));

  /* remove pending timeout */
  if (shortcuts_pane->idle_select_directory != 0)
    {
      g_source_remove (shortcuts_pane->idle_select_directory);
      shortcuts_pane->idle_select_directory = 0;
    }

  /* activate the new directory */
  shortcuts_pane->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the new directory */
      g_object_ref (G_OBJECT (current_directory));

      /* start idle to select item in sidepane (this to also make
       * the selection work when the bookmarks are loaded idle) */
      shortcuts_pane->idle_select_directory =
        g_idle_add_full (G_PRIORITY_LOW, thunar_shortcuts_pane_set_current_directory_idle,
                         shortcuts_pane, NULL);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "current-directory");
}



static GList*
thunar_shortcuts_pane_get_selected_files (ThunarComponent *component)
{
  return THUNAR_SHORTCUTS_PANE (component)->selected_files;
}



static void
thunar_shortcuts_pane_set_selected_files (ThunarComponent *component,
                                          GList           *selected_files)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (component);

  /* disconnect from the previously selected thunar files... */
  thunar_g_list_free_full (shortcuts_pane->selected_files);

  /* ...and take a copy of the newly selected thunar files */
  shortcuts_pane->selected_files = thunar_g_list_copy_deep (selected_files);

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "selected-files");
}



/**
 * thunar_shortcuts_pane_add_shortcut:
 * @shortcuts_pane : Instance of a #ThunarShortcutsPane
 * @file           : #ThunarFile for which a shortcut should be added
 *
 * Adds a shortcut for the passed #ThunarFile to the shortcuts_pane.
 * Only folders will be considered.
 **/
void
thunar_shortcuts_pane_add_shortcut (ThunarShortcutsPane *shortcuts_pane,
                                    ThunarFile          *file)
{
  GtkTreeModel *model;
  GtkTreeModel *child_model;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_PANE (shortcuts_pane));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the shortcuts model for the view */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (shortcuts_pane->view));
  if (G_LIKELY (model != NULL))
    {
      child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
      if (G_LIKELY (thunar_file_is_directory (file)))
        {
          /* append the folder to the shortcuts model */
          thunar_shortcuts_model_add (THUNAR_SHORTCUTS_MODEL (child_model), NULL, file);
        }
    }
}



static void
thunar_shortcuts_pane_show_shortcuts_view_padding (GtkWidget *widget)
{
  thunar_shortcuts_view_toggle_padding (THUNAR_SHORTCUTS_VIEW (widget), TRUE);
}



static void
thunar_shortcuts_pane_hide_shortcuts_view_padding (GtkWidget *widget)
{
  thunar_shortcuts_view_toggle_padding (THUNAR_SHORTCUTS_VIEW (widget), FALSE);
}
