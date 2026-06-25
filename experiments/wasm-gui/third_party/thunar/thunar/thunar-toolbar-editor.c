/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2021 Sergios - Anestis Kefalidis <sergioskefalidis@gmail.com>
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

#include <exo/exo.h>

#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-toolbar-editor.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-application.h>



static void     thunar_toolbar_editor_finalize                           (GObject                  *object);
static gboolean thunar_toolbar_editor_visible_func                       (GtkTreeModel             *model,
                                                                          GtkTreeIter              *iter,
                                                                          gpointer                  data);
static void     thunar_toolbar_editor_help_clicked                       (ThunarToolbarEditor      *toolbar_editor,
                                                                          GtkWidget                *button);
static void     thunar_toolbar_editor_swap_toolbar_items_for_all_windows (GtkListStore             *model,
                                                                          GtkTreeIter              *item1,
                                                                          GtkTreeIter              *item2);
static void     thunar_toolbar_editor_move_down                          (ThunarToolbarEditor      *toolbar_editor,
                                                                          GtkWidget                *button);
static void     thunar_toolbar_editor_move_up                            (ThunarToolbarEditor      *toolbar_editor,
                                                                          GtkWidget                *button);
static void     thunar_toolbar_editor_toggle_visibility                  (ThunarToolbarEditor      *toolbar_editor,
                                                                          const gchar              *path_string,
                                                                          GtkCellRendererToggle    *cell_renderer);
static void     thunar_toolbar_editor_update_buttons                     (ThunarToolbarEditor      *toolbar_editor);
static void     thunar_toolbar_editor_use_defaults                       (ThunarToolbarEditor      *toolbar_editor,
                                                                          GtkWidget                *button);
static void     thunar_toolbar_editor_save_model                         (ThunarToolbarEditor      *toolbar_editor);
static void     thunar_toolbar_editor_populate_model                     (ThunarToolbarEditor      *toolbar_editor);



struct _ThunarToolbarEditorClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarToolbarEditor
{
  ThunarAbstractDialog __parent__;

  ThunarPreferences *preferences;

  GtkListStore      *model;
  GtkTreeModel      *filter;

  /* Used to get information about the toolbar items, it is used as a reference. All the toolbars are changed, not just this one. */
  GtkWidget         *toolbar;

  GtkWidget         *tree_view;
  GtkWidget         *up_button;
  GtkWidget         *down_button;
};



G_DEFINE_TYPE (ThunarToolbarEditor, thunar_toolbar_editor, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_toolbar_editor_class_init (ThunarToolbarEditorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_toolbar_editor_finalize;
}



static void
thunar_toolbar_editor_init (ThunarToolbarEditor *toolbar_editor)
{
  GtkCellRenderer        *renderer;
  GtkWidget              *button;
  GtkWidget              *image;
  GtkWidget              *label;
  GtkWidget              *vbox;
  GtkWidget              *hbox;
  GtkWidget              *vbox_buttons;
  GtkWidget              *swin;

  /* grab a reference on the preferences */
  toolbar_editor->preferences = thunar_preferences_get ();

  /* grab a reference on the shared toolbar model */
  toolbar_editor->model = gtk_list_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

  /* setup the dialog */
  gtk_dialog_add_button (GTK_DIALOG (toolbar_editor), _("_Close"), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (toolbar_editor), GTK_RESPONSE_CLOSE);
  gtk_window_set_default_size (GTK_WINDOW (toolbar_editor), 510, 490);
  gtk_window_set_resizable (GTK_WINDOW (toolbar_editor), TRUE);
  gtk_window_set_title (GTK_WINDOW (toolbar_editor), _("Configure the Toolbar"));

  /* add the "Help" button */
  button = gtk_button_new_with_mnemonic (_("_Help"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_toolbar_editor_help_clicked), toolbar_editor);
  gtk_box_pack_start (GTK_BOX (exo_gtk_dialog_get_action_area (GTK_DIALOG (toolbar_editor))), button, FALSE, FALSE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (exo_gtk_dialog_get_action_area (GTK_DIALOG (toolbar_editor))), button, TRUE);
  gtk_widget_show (button);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (toolbar_editor))), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* create the top label for the toolbar editor dialog */
  label = gtk_label_new (_("Configure the order and visibility of toolbar items."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);
  gtk_widget_show (hbox);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (swin, TRUE);
  gtk_widget_set_vexpand (swin, TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create a filter from the shared column model */
  toolbar_editor->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (toolbar_editor->model), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (toolbar_editor->filter),
                                          (GtkTreeModelFilterVisibleFunc) thunar_toolbar_editor_visible_func,
                                          NULL, NULL);

  /* create the tree view */
  toolbar_editor->tree_view = gtk_tree_view_new_with_model (toolbar_editor->filter);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (toolbar_editor->tree_view), FALSE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (toolbar_editor->tree_view), 2);
  gtk_container_add (GTK_CONTAINER (swin), toolbar_editor->tree_view);
  gtk_widget_show (toolbar_editor->tree_view);

  /* append the toggle toolbar */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_swapped (G_OBJECT (renderer), "toggled", G_CALLBACK (thunar_toolbar_editor_toggle_visibility), toolbar_editor);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (toolbar_editor->tree_view),
                                               -1,
                                               "Active",
                                               renderer,
                                               "active", 0,
                                               NULL);

  /* append the icon toolbar */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (toolbar_editor->tree_view),
                                               -1,
                                               "Icon",
                                               renderer,
                                               "icon-name", 1,
                                               NULL);

  /* append the name toolbar */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (toolbar_editor->tree_view),
                                               -1,
                                               "Name",
                                               renderer,
                                               "text", 2,
                                               NULL);

  /* Create the buttons vbox container */
  vbox_buttons = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox_buttons, FALSE, FALSE, 0);
  gtk_widget_set_vexpand (vbox_buttons, FALSE);
  gtk_widget_show (vbox_buttons);

  /* create the "Move Up" button */
  toolbar_editor->up_button = gtk_button_new_with_mnemonic (_("Move _Up"));
  g_signal_connect_swapped (G_OBJECT (toolbar_editor->up_button), "clicked", G_CALLBACK (thunar_toolbar_editor_move_up), toolbar_editor);
  gtk_box_pack_start (GTK_BOX (vbox_buttons), toolbar_editor->up_button, FALSE, FALSE, 0);
  gtk_widget_show (toolbar_editor->up_button);

  image = gtk_image_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (toolbar_editor->up_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (toolbar_editor->up_button), image);
  gtk_widget_show (image);

  /* create the "Move Down" button */
  toolbar_editor->down_button = gtk_button_new_with_mnemonic (_("Move Dow_n"));
  g_signal_connect_swapped (G_OBJECT (toolbar_editor->down_button), "clicked", G_CALLBACK (thunar_toolbar_editor_move_down), toolbar_editor);
  gtk_box_pack_start (GTK_BOX (vbox_buttons), toolbar_editor->down_button, FALSE, FALSE, 0);
  gtk_widget_show (toolbar_editor->down_button);

  image = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (toolbar_editor->down_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (toolbar_editor->down_button), image);
  gtk_widget_show (image);

  /* create the "Use Default" button */
  button = gtk_button_new_with_mnemonic (_("De_fault Order"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_toolbar_editor_use_defaults), toolbar_editor);
  gtk_box_pack_start (GTK_BOX (vbox_buttons), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* grab focus to the tree view */
  gtk_widget_grab_focus (toolbar_editor->tree_view);
}



static void
thunar_toolbar_editor_finalize (GObject *object)
{
  ThunarToolbarEditor *toolbar_editor = THUNAR_TOOLBAR_EDITOR (object);

  thunar_toolbar_editor_save_model (toolbar_editor);

  /* release our reference on the shared toolbar model */
  g_signal_handlers_disconnect_matched (G_OBJECT (toolbar_editor->model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, toolbar_editor);
  g_object_unref (G_OBJECT (toolbar_editor->model));

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (toolbar_editor->preferences));

  (*G_OBJECT_CLASS (thunar_toolbar_editor_parent_class)->finalize) (object);
}



static gboolean
thunar_toolbar_editor_visible_func (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    gpointer     data)
{
  gboolean visible = TRUE;
  gint     order;

  gtk_tree_model_get (model, iter, 3, &order, -1);

   /* The item with order 0 is always THUNAR_WINDOW_ACTION_VIEW_MENUBAR which we want to hide */
  if (order == 0)
    visible = FALSE;

  return visible;
}



static void
thunar_toolbar_editor_help_clicked (ThunarToolbarEditor *toolbar_editor,
                                    GtkWidget          *button)
{
  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* open the user manual */
  xfce_dialog_show_help (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                         "thunar",
                         "the-file-manager-window",
                         "toolbar_customization");
}



static void
thunar_toolbar_editor_swap_toolbar_items_for_all_windows (GtkListStore *model,
                                                          GtkTreeIter  *item1,
                                                          GtkTreeIter  *item2)
{
  ThunarApplication *application;
  GList             *windows;
  GtkTreePath       *path1;
  GtkTreePath       *path2;

  path1 = gtk_tree_model_get_path (GTK_TREE_MODEL (model), item1);
  path2 = gtk_tree_model_get_path (GTK_TREE_MODEL (model), item2);

  application = thunar_application_get ();
  windows = thunar_application_get_windows (application);
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    {
      ThunarWindow *window = lp->data;
      thunar_window_toolbar_swap_items (window, gtk_tree_path_get_indices (path1)[0], gtk_tree_path_get_indices (path2)[0]);
    }
  g_object_unref (application);
  g_list_free (windows);

  gtk_tree_path_free (path1);
  gtk_tree_path_free (path2);
}



static void
thunar_toolbar_editor_move_down (ThunarToolbarEditor *toolbar_editor,
                                 GtkWidget           *button)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeModel     *childModel;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;
  GtkTreeIter       childIter1;
  GtkTreeIter       childIter2;

  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  gtk_tree_selection_get_selected (selection, &model, &iter1);
  iter2 = iter1;
  if (gtk_tree_model_iter_next (model, &iter2) == FALSE)
    return;

  /* tree view's model is made from GTK_TREE_MODEL_FILTER, hence fetching child model and child iter's */
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter1, &iter1);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter2, &iter2);
  childModel = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER (model));

  gtk_list_store_swap (GTK_LIST_STORE (childModel), &childIter1, &childIter2);
  thunar_toolbar_editor_swap_toolbar_items_for_all_windows (GTK_LIST_STORE (childModel), &childIter1, &childIter2);
}



static void
thunar_toolbar_editor_move_up (ThunarToolbarEditor *toolbar_editor,
                               GtkWidget          *button)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeModel     *childModel;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;
  GtkTreeIter       childIter1;
  GtkTreeIter       childIter2;

  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  gtk_tree_selection_get_selected (selection, &model, &iter1);
  iter2 = iter1;
  if (gtk_tree_model_iter_previous (model, &iter2) == FALSE)
    return;

  /* tree view's model is made from GTK_TREE_MODEL_FILTER, hence fetching child model and child iter's */
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter1, &iter1);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter2, &iter2);
  childModel = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER (model));

  gtk_list_store_swap (GTK_LIST_STORE (childModel), &childIter1, &childIter2);
  thunar_toolbar_editor_swap_toolbar_items_for_all_windows (GTK_LIST_STORE (childModel), &childIter1, &childIter2);
}



static void
thunar_toolbar_editor_toggle_visibility (ThunarToolbarEditor    *toolbar_editor,
                                         const gchar            *path_string,
                                         GtkCellRendererToggle  *cell_renderer)
{
  ThunarApplication *application;
  GList             *windows;
  GtkTreePath       *path;
  GtkTreePath       *child_path;
  GtkTreeIter        iter;
  gboolean           visible;

  _thunar_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (cell_renderer));
  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (path_string != NULL);

  /* determine the tree path for the string */
  path = gtk_tree_path_new_from_string (path_string);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (toolbar_editor->filter), path);

  if (child_path == NULL)
    {
      gtk_tree_path_free (path);
      return;
    }

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (toolbar_editor->model), &iter, child_path))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (toolbar_editor->model), &iter, 0, &visible, -1);
      gtk_list_store_set (toolbar_editor->model, &iter, 0, !visible, -1);
    }

  application = thunar_application_get ();
  windows = thunar_application_get_windows (application);
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    {
      ThunarWindow *window = lp->data;
      thunar_window_toolbar_toggle_item_visibility (window, gtk_tree_path_get_indices (child_path)[0]);
    }
  g_object_unref (application);
  g_list_free (windows);

  gtk_tree_path_free (path);
  gtk_tree_path_free (child_path);
}



static void
thunar_toolbar_editor_update_buttons (ThunarToolbarEditor *toolbar_editor)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  gint              idx;

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the tree path for the iter */
      path = gtk_tree_model_get_path (model, &iter);
      if (G_UNLIKELY (path == NULL))
        return;

      /* update the "Move Up"/"Move Down" buttons */
      idx = gtk_tree_path_get_indices (path)[0];
      gtk_widget_set_sensitive (toolbar_editor->up_button, (idx > 0));
      gtk_widget_set_sensitive (toolbar_editor->down_button, (idx + 1 < gtk_tree_model_iter_n_children (model, NULL)));

      /* release the path */
      gtk_tree_path_free (path);
    }
  else
    {
      /* just disable all buttons */
      gtk_widget_set_sensitive (toolbar_editor->down_button, FALSE);
      gtk_widget_set_sensitive (toolbar_editor->up_button, FALSE);
    }
}



static void
thunar_toolbar_editor_use_defaults (ThunarToolbarEditor *toolbar_editor,
                                    GtkWidget           *button)
{
  GList *toolbar_items;
  guint item_count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (toolbar_editor->model), NULL);
  guint target_order[item_count];
  guint current_order[item_count];
  guint index;

  toolbar_items = gtk_container_get_children (GTK_CONTAINER (toolbar_editor->toolbar));

  index = 0;
  for (GList *lp = toolbar_items; lp != NULL; lp = lp->next)
    {
      GtkWidget *item = lp->data;
      gint *order = NULL;

      order = g_object_get_data (G_OBJECT (item), "default-order");
      current_order[index] = *order;
      target_order[index] = index;
      index++;
    }

  /* now rearrange the toolbar items, the goal is to make the current_order like the target_order */
  for (guint i = 0; i < item_count; i++)
    {
      guint x = target_order[i];
      for (guint j = 0; j < item_count; j++)
        {
          guint y = current_order[j];
          if (x == y && i != j)
            {
              GtkTreeIter iter_i, iter_j;
              gchar *path_i, *path_j;

              path_i = g_strdup_printf ("%i", i);
              path_j = g_strdup_printf ("%i", j);

              /* swap the positions of the toolbar items */
              gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (toolbar_editor->model), &iter_i, path_i);
              gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (toolbar_editor->model), &iter_j, path_j);
              gtk_list_store_swap (GTK_LIST_STORE (toolbar_editor->model), &iter_i, &iter_j);
              thunar_toolbar_editor_swap_toolbar_items_for_all_windows (toolbar_editor->model, &iter_i, &iter_j);

              y = current_order[i];
              current_order[i] = target_order[i];
              current_order[j] = y;

              g_free (path_i);
              g_free (path_j);
              break;
            }
        }
    }

  g_list_free (toolbar_items);
}



static void
thunar_toolbar_editor_save_model (ThunarToolbarEditor *toolbar_editor)
{
  GString    *item_order;
  GString    *item_visibility;
  guint       item_count;

  item_order = g_string_sized_new (256);
  item_visibility = g_string_sized_new (256);

  item_count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (toolbar_editor->model), NULL);

  /* transform the internal visible column list */
  for (guint i = 0; i < item_count; i++)
    {
      GtkTreeIter iter;
      gchar      *path;
      gint        order;
      gchar      *order_str;
      gboolean    visible;
      gchar      *visible_str;

      /* append a comma if not empty */
      if (*item_order->str != '\0')
        g_string_append_c (item_order, ',');

      if (*item_visibility->str != '\0')
        g_string_append_c (item_visibility, ',');

      /* get iterator for the ith item */
      path = g_strdup_printf("%i", i);
      gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (toolbar_editor->model), &iter, path);
      g_free (path);

      /* get the order value of the entry and store it */
      gtk_tree_model_get (GTK_TREE_MODEL (toolbar_editor->model), &iter, 3, &order, -1);
      order_str = g_strdup_printf("%i", order);
      g_string_append (item_order, order_str);
      g_free (order_str);

      /* get the visibility value of the entry and store it */
      gtk_tree_model_get (GTK_TREE_MODEL (toolbar_editor->model), &iter, 0, &visible, -1);
      visible_str = g_strdup_printf("%i", visible);
      g_string_append (item_visibility, visible_str);
      g_free (visible_str);
    }

  /* save the order and visibility lists */
  g_object_set (G_OBJECT (toolbar_editor->preferences), "last-toolbar-item-order", item_order->str, NULL);
  g_object_set (G_OBJECT (toolbar_editor->preferences), "last-toolbar-visible-buttons", item_visibility->str, NULL);

  /* release the strings */
  g_string_free (item_order, TRUE);
  g_string_free (item_visibility, TRUE);
}



static void
thunar_toolbar_editor_populate_model (ThunarToolbarEditor *toolbar_editor)
{
  GtkTreeSelection *selection;
  GtkTreeIter       iter;
  GList            *toolbar_items;

  toolbar_items = gtk_container_get_children (GTK_CONTAINER (toolbar_editor->toolbar));

  for (GList *lp = toolbar_items; lp != NULL; lp = lp->next)
    {
      GtkWidget *item = lp->data;
      GtkWidget *widget = NULL; /* used to remove mnemonics */
      gchar     *label_with_mnemonic = NULL;
      gchar     *icon = NULL;
      gint      *order = NULL;

      label_with_mnemonic = g_object_get_data (G_OBJECT (item), "label");
      icon = g_object_get_data (G_OBJECT (item), "icon");
      order = g_object_get_data (G_OBJECT (item), "default-order");

      widget = gtk_label_new_with_mnemonic (label_with_mnemonic);
      g_object_ref_sink (widget);

      gtk_list_store_append (toolbar_editor->model, &iter);
      gtk_list_store_set (toolbar_editor->model, &iter,
                          0, gtk_widget_is_visible (item),
                          1, icon,
                          2, gtk_label_get_text (GTK_LABEL (widget)),
                          3, *order,
                          -1);

      g_object_unref (widget);
    }

  /* connect after model has been fully initialized */
  g_signal_connect_data (G_OBJECT (toolbar_editor->model), "row-changed", G_CALLBACK (thunar_toolbar_editor_update_buttons),
                         toolbar_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_data (G_OBJECT (toolbar_editor->model), "rows-reordered", G_CALLBACK (thunar_toolbar_editor_update_buttons),
                         toolbar_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  /* set up the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  g_signal_connect_swapped (G_OBJECT (selection), "changed", G_CALLBACK (thunar_toolbar_editor_update_buttons), toolbar_editor);

  /* select the first item */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (toolbar_editor->filter), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  g_list_free (toolbar_items);
}



/**
 * thunar_show_toolbar_editor:
 * @parent : the #GtkWidget the #GdkScreen on which to open the
 *           toolbar editor dialog. May also be %NULL in which case
 *           the default #GdkScreen will be used.
 *
 * Convenience function to display a #ThunarToolbarEditor.
 **/
void
thunar_show_toolbar_editor (GtkWidget *window,
                            GtkWidget *window_toolbar)
{
  GtkWidget *dialog;
  GdkScreen *screen = NULL;

  _thunar_return_if_fail (GTK_IS_WIDGET (window));

  screen = gtk_widget_get_screen (window);
  window = gtk_widget_get_toplevel (window);

  /* display the toolbar editor */
  dialog = g_object_new (THUNAR_TYPE_TOOLBAR_EDITOR, NULL);

  /* set the toolbar used for gathering information */
  g_object_ref (window_toolbar);
  THUNAR_TOOLBAR_EDITOR (dialog)->toolbar = window_toolbar;

  /* now that the toolbar is set, populate the tree view */
  thunar_toolbar_editor_populate_model (THUNAR_TOOLBAR_EDITOR (dialog));

  /* check if we have a toplevel window */
  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* dialog is transient for toplevel window and modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    }

  /* set the screen for the window */
  if (screen != NULL && GDK_IS_SCREEN (screen))
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  /* run the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  g_object_unref (window_toolbar);
  gtk_widget_destroy (dialog);
}

