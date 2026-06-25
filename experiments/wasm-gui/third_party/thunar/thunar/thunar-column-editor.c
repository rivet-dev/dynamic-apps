/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
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
#include <thunar/thunar-column-editor.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>



static void thunar_column_editor_finalize           (GObject                  *object);
static void thunar_column_editor_help_clicked       (ThunarColumnEditor       *column_editor,
                                                     GtkWidget                *button);
static void thunar_column_editor_move_down          (ThunarColumnEditor       *column_editor,
                                                     GtkWidget                *button);
static void thunar_column_editor_move_up            (ThunarColumnEditor       *column_editor,
                                                     GtkWidget                *button);
static void thunar_column_editor_toggled            (ThunarColumnEditor       *column_editor,
                                                     const gchar              *path_string,
                                                     GtkCellRendererToggle    *cell_renderer);
static void thunar_column_editor_toggle_visibility  (ThunarColumnEditor       *column_editor,
                                                     GtkWidget                *button);
static void thunar_column_editor_update_buttons     (ThunarColumnEditor       *column_editor);
static void thunar_column_editor_use_defaults       (ThunarColumnEditor       *column_editor,
                                                     GtkWidget                *button);



struct _ThunarColumnEditorClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarColumnEditor
{
  ThunarAbstractDialog __parent__;

  ThunarPreferences *preferences;

  ThunarColumnModel *column_model;
  GtkTreeModelFilter *filter;

  GtkWidget         *tree_view;
  GtkWidget         *up_button;
  GtkWidget         *down_button;
  GtkWidget         *show_button;
  GtkWidget         *hide_button;
};



G_DEFINE_TYPE (ThunarColumnEditor, thunar_column_editor, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_column_editor_class_init (ThunarColumnEditorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_column_editor_finalize;
}



static gboolean
thunar_column_visible_func (GtkTreeModel *model,
                            GtkTreeIter  *iter,
                            gpointer     data)
{
  gchar       *col_name;
  const gchar *del_date_col_name;
  gboolean     visible = TRUE;

  /* fetching Date Deleted column name */
  del_date_col_name = thunar_column_model_get_column_name (THUNAR_COLUMN_MODEL (model),
                                                           THUNAR_COLUMN_DATE_DELETED);

  /* fetching iter column name */
  gtk_tree_model_get (model, iter, THUNAR_COLUMN_MODEL_COLUMN_NAME, &col_name, -1);

  /* matching iter column name & Date Deleted column name */
  if (g_strcmp0 (col_name, del_date_col_name) == 0)
    visible = FALSE;

  g_free (col_name);

  return visible;
}



static void
thunar_column_editor_init (ThunarColumnEditor *column_editor)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkTreeIter        childIter;
  GtkTreeIter        iter;
  GtkWidget         *separator;
  GtkWidget         *button;
  GtkWidget         *combo;
  GtkWidget         *frame;
  GtkWidget         *image;
  GtkWidget         *label;
  GtkWidget         *grid;
  GtkWidget         *vbox;
  GtkWidget         *swin;
  GEnumClass        *enum_class;
  gint               row = 0;

  /* grab a reference on the preferences */
  column_editor->preferences = thunar_preferences_get ();

  /* grab a reference on the shared column model */
  column_editor->column_model = thunar_column_model_get_default ();
  g_signal_connect_data (G_OBJECT (column_editor->column_model), "row-changed", G_CALLBACK (thunar_column_editor_update_buttons),
                         column_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_data (G_OBJECT (column_editor->column_model), "rows-reordered", G_CALLBACK (thunar_column_editor_update_buttons),
                         column_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  /* create a filter from the shared column model */
  column_editor->filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (column_editor->column_model), NULL));
  gtk_tree_model_filter_set_visible_func (column_editor->filter,
                                          (GtkTreeModelFilterVisibleFunc) thunar_column_visible_func,
                                          NULL, NULL);

  /* setup the dialog */
  gtk_dialog_add_button (GTK_DIALOG (column_editor), _("_Close"), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (column_editor), GTK_RESPONSE_CLOSE);
  gtk_window_set_resizable (GTK_WINDOW (column_editor), FALSE);
  gtk_window_set_title (GTK_WINDOW (column_editor), _("Configure Columns in the Detailed List View"));

  /* add the "Help" button */
  button = gtk_button_new_with_mnemonic (_("_Help"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_column_editor_help_clicked), column_editor);
  gtk_box_pack_start (GTK_BOX (exo_gtk_dialog_get_action_area (GTK_DIALOG (column_editor))), button, FALSE, FALSE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (exo_gtk_dialog_get_action_area (GTK_DIALOG (column_editor))), button, TRUE);
  gtk_widget_show (button);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (column_editor))), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Visible Columns"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* create the top label for the column editor dialog */
  label = gtk_label_new (_("Choose the order of information to appear in the\ndetailed list view."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 2, 1);
  gtk_widget_show (label);

  /* next row */
  row++;

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (swin, TRUE);
  gtk_widget_set_vexpand (swin, TRUE);
  gtk_grid_attach (GTK_GRID (grid), swin, 0, row, 1, 6);
  gtk_widget_show (swin);

  /* create the tree view */
  column_editor->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (column_editor->filter));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (column_editor->tree_view), FALSE);
  gtk_container_add (GTK_CONTAINER (swin), column_editor->tree_view);
  gtk_widget_show (column_editor->tree_view);

  /* append the toggle column */
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_swapped (G_OBJECT (renderer), "toggled", G_CALLBACK (thunar_column_editor_toggled), column_editor);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "active", THUNAR_COLUMN_MODEL_COLUMN_VISIBLE,
                                       "activatable", THUNAR_COLUMN_MODEL_COLUMN_MUTABLE,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (column_editor->tree_view), column);

  /* append the name column */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_COLUMN_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (column_editor->tree_view), column);

  /* create the "Move Up" button */
  column_editor->up_button = gtk_button_new_with_mnemonic (_("Move _Up"));
  g_signal_connect_swapped (G_OBJECT (column_editor->up_button), "clicked", G_CALLBACK (thunar_column_editor_move_up), column_editor);
  gtk_grid_attach (GTK_GRID (grid), column_editor->up_button, 1, row, 1, 1);
  gtk_widget_show (column_editor->up_button);

  image = gtk_image_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (column_editor->up_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (column_editor->up_button), image);
  gtk_widget_show (image);

  /* next row */
  row++;

  /* create the "Move Down" button */
  column_editor->down_button = gtk_button_new_with_mnemonic (_("Move Dow_n"));
  g_signal_connect_swapped (G_OBJECT (column_editor->down_button), "clicked", G_CALLBACK (thunar_column_editor_move_down), column_editor);
  gtk_grid_attach (GTK_GRID (grid), column_editor->down_button, 1, row, 1, 1);
  gtk_widget_show (column_editor->down_button);

  image = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (column_editor->down_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (column_editor->down_button), image);
  gtk_widget_show (image);

  /* next row */
  row++;

  /* create the "Show" button */
  column_editor->show_button = gtk_button_new_with_mnemonic (_("_Show"));
  g_signal_connect_swapped (G_OBJECT (column_editor->show_button), "clicked", G_CALLBACK (thunar_column_editor_toggle_visibility), column_editor);
  gtk_grid_attach (GTK_GRID (grid), column_editor->show_button, 1, row, 1, 1);
  gtk_widget_show (column_editor->show_button);

  /* next row */
  row++;

  /* create the "Hide" button */
  column_editor->hide_button = gtk_button_new_with_mnemonic (_("Hi_de"));
  g_signal_connect_swapped (G_OBJECT (column_editor->hide_button), "clicked", G_CALLBACK (thunar_column_editor_toggle_visibility), column_editor);
  gtk_grid_attach (GTK_GRID (grid), column_editor->hide_button, 1, row, 1, 1);
  gtk_widget_show (column_editor->hide_button);

  /* next row */
  row++;

  /* create the horiz separator */
  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 1, row, 1, 1);
  gtk_widget_show (separator);

  /* next row */
  row++;

  /* create the "Use Default" button */
  button = gtk_button_new_with_mnemonic (_("Use De_fault"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_column_editor_use_defaults), column_editor);
  gtk_grid_attach (GTK_GRID (grid), button, 1, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Column Sizing"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* create the label that explains the column sizing option */
  label = gtk_label_new (_("By default columns will be automatically expanded if\n"
                           "needed to ensure the text is fully visible. If you dis-\n"
                           "able this behavior below the file manager will always\n"
                           "use the user defined column widths."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* next row */
  row++;

  /* create the "Automatically expand columns as needed" button */
  button = gtk_check_button_new_with_mnemonic (_("Automatically _expand columns as needed"));
  g_object_bind_property (G_OBJECT (column_editor->preferences),
                          "last-details-view-fixed-columns",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), button);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Size Column of Folders"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* explain what it does */
  label = gtk_label_new (_("Show number of containing items"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  enum_class = g_type_class_ref (THUNAR_TYPE_FOLDER_ITEM_COUNT);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (enum_class, THUNAR_FOLDER_ITEM_COUNT_NEVER)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (enum_class, THUNAR_FOLDER_ITEM_COUNT_ONLY_LOCAL)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (enum_class, THUNAR_FOLDER_ITEM_COUNT_ALWAYS)->value_nick));
  g_type_class_unref (enum_class);

  g_object_bind_property_full (G_OBJECT (column_editor->preferences), "misc-folder-item-count",
                               G_OBJECT (combo), "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_folder_item_count_get_type, NULL);

  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row - 1, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* setup the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  g_signal_connect_swapped (G_OBJECT (selection), "changed", G_CALLBACK (thunar_column_editor_update_buttons), column_editor);

  /* select the first item */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (column_editor->column_model), &childIter))
    {
      /* tree_view is created from filter & column_model is the child of filter
       * hence, child Iter needs to be converted to parent Iter */
      gtk_tree_model_filter_convert_child_iter_to_iter (column_editor->filter, &iter, &childIter);
      gtk_tree_selection_select_iter (selection, &iter);
    }

  /* grab focus to the tree view */
  gtk_widget_grab_focus (column_editor->tree_view);
}



static void
thunar_column_editor_finalize (GObject *object)
{
  ThunarColumnEditor *column_editor = THUNAR_COLUMN_EDITOR (object);

  /* release our reference on the shared column model */
  g_signal_handlers_disconnect_matched (G_OBJECT (column_editor->column_model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, column_editor);
  g_object_unref (G_OBJECT (column_editor->column_model));

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (column_editor->preferences));

  (*G_OBJECT_CLASS (thunar_column_editor_parent_class)->finalize) (object);
}



static void
thunar_column_editor_help_clicked (ThunarColumnEditor *column_editor,
                                   GtkWidget          *button)
{
  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* open the user manual */
  xfce_dialog_show_help (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                         "thunar",
                         "the-file-manager-window",
                         "customizing_the_appearance");
}



static void
thunar_column_editor_move_down (ThunarColumnEditor *column_editor,
                                GtkWidget          *button)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeModel     *childModel;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;
  GtkTreeIter       childIter1;
  GtkTreeIter       childIter2;

  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter1))
    {
      /* copy the iterator */
      iter2 = iter1;

      /* determine the next iterator and exchange the rows */
      if (gtk_tree_model_iter_next (model, &iter2))
        {
          /* tree view's model is made from GTK_TREE_MODEL_FILTER, hence fetching child model and child iter's */
          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter1, &iter1);
          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter2, &iter2);
          childModel = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER (model));

          thunar_column_model_exchange (THUNAR_COLUMN_MODEL (childModel), &childIter1, &childIter2);
        }
    }
}



static void
thunar_column_editor_move_up (ThunarColumnEditor *column_editor,
                              GtkWidget          *button)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeModel     *childModel;
  GtkTreePath      *path;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;
  GtkTreeIter       childIter1;
  GtkTreeIter       childIter2;

  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter1))
    {
      /* determine the path for the iterator */
      path = gtk_tree_model_get_path (model, &iter1);
      if (G_LIKELY (path != NULL))
        {
          /* advance to the prev path */
          if (gtk_tree_path_prev (path) && gtk_tree_model_get_iter (model, &iter2, path))
            {
              /* tree view's model is made from GTK_TREE_MODEL_FILTER, hence fetching child model and child iter's */
              gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter1, &iter1);
              gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter2, &iter2);
              childModel = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER (model));

              /* exchange the rows */
              thunar_column_model_exchange (THUNAR_COLUMN_MODEL (childModel), &childIter1, &childIter2);
            }

          /* release the path */
          gtk_tree_path_free (path);
        }
    }
}



static void
thunar_column_editor_toggled (ThunarColumnEditor    *column_editor,
                              const gchar           *path_string,
                              GtkCellRendererToggle *cell_renderer)
{
  ThunarColumn column;
  GtkTreePath *path;
  GtkTreeIter  iter;
  GtkTreeIter  child_iter;
  gboolean     visible;

  _thunar_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (cell_renderer));
  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (path_string != NULL);

  /* determine the tree path for the string */
  path = gtk_tree_path_new_from_string (path_string);
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (column_editor->filter), &iter, path))
    {
      gtk_tree_model_filter_convert_iter_to_child_iter (column_editor->filter, &child_iter, &iter);

      /* determine the column for the iterator... */
      column = thunar_column_model_get_column_for_iter (column_editor->column_model, &child_iter);

      /* ...determine the existing visbility setting... */
      visible = thunar_column_model_get_column_visible (column_editor->column_model, column);

      /* ...and change the visibility of the column */
      thunar_column_model_set_column_visible (column_editor->column_model, column, !visible);
    }
  gtk_tree_path_free (path);
}



static void
thunar_column_editor_toggle_visibility (ThunarColumnEditor *column_editor,
                                        GtkWidget          *button)
{
  GtkTreeSelection *selection;
  ThunarColumn      column;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkTreeIter       childIter;
  gboolean          visible;

  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* tree view's model is made from GTK_TREE_MODEL_FILTER, hence fetching child iter */
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &childIter, &iter);

      /* determine the column for the iterator... */
      column = thunar_column_model_get_column_for_iter (column_editor->column_model, &childIter);

      /* ...determine the existing visbility setting... */
      visible = thunar_column_model_get_column_visible (column_editor->column_model, column);

      /* ...and change the visibility of the column */
      thunar_column_model_set_column_visible (column_editor->column_model, column, !visible);
    }
}



static void
thunar_column_editor_update_buttons (ThunarColumnEditor *column_editor)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  gboolean          mutable;
  gboolean          visible;
  gint              idx;

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the tree path for the iter */
      path = gtk_tree_model_get_path (model, &iter);

      if (G_UNLIKELY (path == NULL))
        return;

      /* update the "Move Up"/"Move Down" buttons */
      idx = gtk_tree_path_get_indices (path)[0];
      gtk_widget_set_sensitive (column_editor->up_button, (idx > 0));
      gtk_widget_set_sensitive (column_editor->down_button, (idx + 1 < gtk_tree_model_iter_n_children (model, NULL)));

      /* update the "Show"/"Hide" buttons */
      gtk_tree_model_get (model, &iter, THUNAR_COLUMN_MODEL_COLUMN_MUTABLE, &mutable, THUNAR_COLUMN_MODEL_COLUMN_VISIBLE, &visible, -1);
      gtk_widget_set_sensitive (column_editor->show_button, mutable && !visible);
      gtk_widget_set_sensitive (column_editor->hide_button, mutable && visible);

      /* release the path */
      gtk_tree_path_free (path);
    }
  else
    {
      /* just disable all buttons */
      gtk_widget_set_sensitive (column_editor->hide_button, FALSE);
      gtk_widget_set_sensitive (column_editor->show_button, FALSE);
      gtk_widget_set_sensitive (column_editor->down_button, FALSE);
      gtk_widget_set_sensitive (column_editor->up_button, FALSE);
    }
}



static void
thunar_column_editor_use_defaults (ThunarColumnEditor *column_editor,
                                   GtkWidget          *button)
{
  static const gchar *PROPERTY_NAMES[] =
  {
    "last-details-view-column-order",
    "last-details-view-visible-columns",
  };

  GtkTreeSelection *selection;
  GParamSpec       *pspec;
  GValue            value = { 0, };
  guint             n;

  /* reset the given properties to its default values */
  for (n = 0; n < G_N_ELEMENTS (PROPERTY_NAMES); ++n)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (column_editor->preferences), PROPERTY_NAMES[n]);
      g_value_init (&value, pspec->value_type);
      g_param_value_set_default (pspec, &value);
      g_object_set_property (G_OBJECT (column_editor->preferences), PROPERTY_NAMES[n], &value);
      g_value_unset (&value);
    }

  /* reset the tree view selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  gtk_tree_selection_unselect_all (selection);
}



/**
 * thunar_show_column_editor:
 * @parent : the #GtkWidget the #GdkScreen on which to open the
 *           column editor dialog. May also be %NULL in which case
 *           the default #GdkScreen will be used.
 *
 * Convenience function to display a #ThunarColumnEditor.
 *
 * If @parent is a #GtkWidget the editor dialog will be opened as
 * modal dialog above the @parent. Else if @parent is a screen (if
 * @parent is %NULL the default screen is used), the dialog won't
 * be modal and it will simply popup on the specified screen.
 **/
gboolean
thunar_show_column_editor (gpointer parent)
{
  GtkWidget *window = NULL;
  GtkWidget *dialog;
  GdkScreen *screen = NULL;

  _thunar_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent), FALSE);

  /* determine the screen for the dialog */
  if (G_UNLIKELY (parent == NULL))
    {
      /* just use the default screen, no toplevel window */
      screen = gdk_screen_get_default ();
    }
  else if (GTK_IS_WIDGET (parent))
    {
      /* use the screen for the widget and the toplevel window */
      screen = gtk_widget_get_screen (parent);
      window = gtk_widget_get_toplevel (parent);
    }
  else
    {
      /* parent is a screen, no toplevel window */
      screen = GDK_SCREEN (parent);
    }

  /* display the column editor */
  dialog = g_object_new (THUNAR_TYPE_COLUMN_EDITOR, NULL);

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
  gtk_widget_destroy (dialog);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}

