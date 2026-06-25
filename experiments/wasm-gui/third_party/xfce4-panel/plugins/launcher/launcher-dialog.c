/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <garcon/garcon.h>
#include <xfconf/xfconf.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

#include <common/panel-private.h>
#include <common/panel-utils.h>

#include "launcher.h"
#include "launcher-dialog.h"
#include "launcher-dialog_ui.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#define LAUNCHER_WIDGET_XID(widget) ((guint) GDK_WINDOW_XID (gdk_screen_get_root_window (gtk_widget_get_screen (GTK_WIDGET (widget)))))
#else
#define LAUNCHER_WIDGET_XID(widget) (0)
#endif



typedef struct
{
  LauncherPlugin *plugin;
  GtkBuilder     *builder;
  guint           idle_populate_id;
  GSList         *items;
}
LauncherPluginDialog;

typedef struct
{
  LauncherPluginDialog *dialog;
  GarconMenuItem       *item;
}
LauncherChangedHandler;

enum
{
  COL_ICON,
  COL_NAME,
  COL_ITEM,
  COL_TOOLTIP,
  COL_SEARCH /* only in add-store */
};



static void     launcher_dialog_items_set_item         (GtkTreeModel         *model,
                                                        GtkTreeIter          *iter,
                                                        GarconMenuItem       *item,
                                                        LauncherPluginDialog *dialog);
static void     launcher_dialog_tree_save              (LauncherPluginDialog *dialog);
static void     launcher_dialog_tree_selection_changed (GtkTreeSelection     *selection,
                                                        LauncherPluginDialog *dialog);
static gboolean launcher_dialog_press_event            (LauncherPluginDialog *dialog,
                                                        const gchar          *object_name);
static void     launcher_dialog_items_unload           (LauncherPluginDialog *dialog);
static void     launcher_dialog_items_load             (LauncherPluginDialog *dialog);
static void     launcher_dialog_item_desktop_item_edit (GtkWidget            *widget,
                                                        const gchar          *type,
                                                        const gchar          *uri,
                                                        LauncherPluginDialog *dialog);



/* dnd for items in and from the treeviews */
enum
{
  DROP_TARGET_URI,
  DROP_TARGET_ROW
};

static const GtkTargetEntry add_drag_targets[] =
{
  { "text/uri-list", 0, 0 }
};

static const GtkTargetEntry list_drop_targets[] =
{
  { "text/uri-list", 0, DROP_TARGET_URI },
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, DROP_TARGET_ROW }
};

static const GtkTargetEntry list_drag_targets[] =
{
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
};



static gboolean
launcher_dialog_add_visible_function (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      gpointer      user_data)
{
  gchar       *string, *escaped;
  gboolean     visible = FALSE;
  const gchar *text;
  gchar       *normalized;
  gchar       *text_casefolded;
  gchar       *name_casefolded;

  /* get the search string from the item */
  text = gtk_entry_get_text (GTK_ENTRY (user_data));
  if (G_UNLIKELY (panel_str_is_empty (text)))
    return TRUE;

  /* casefold the search text */
  normalized = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
  text_casefolded = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  /* try the pre-build search string first */
  gtk_tree_model_get (model, iter, COL_SEARCH, &string, -1);
  if (!panel_str_is_empty (string))
    {
      /* search */
      visible = (strstr (string, text_casefolded) != NULL);
    }
  else
    {
      /* get the name */
      gtk_tree_model_get (model, iter, COL_NAME, &string, -1);
      if (!panel_str_is_empty (string))
        {
          /* escape and casefold the name */
          escaped = g_markup_escape_text (string, -1);
          normalized = g_utf8_normalize (escaped, -1, G_NORMALIZE_ALL);
          name_casefolded = g_utf8_casefold (normalized, -1);
          g_free (normalized);
          g_free (escaped);

          /* search */
          visible = (strstr (name_casefolded, text_casefolded) != NULL);

          /* store the generated search string */
          gtk_list_store_set (GTK_LIST_STORE (model), iter, COL_SEARCH,
                              name_casefolded, -1);

          g_free (name_casefolded);
        }
    }

  g_free (text_casefolded);
  g_free (string);

  return visible;
}



static void
launcher_dialog_add_store_insert (gpointer key,
                                  gpointer value,
                                  gpointer user_data)
{
  GtkTreeIter     iter;
  GarconMenuItem *item = GARCON_MENU_ITEM (value);
  GtkTreeModel   *model = GTK_TREE_MODEL (user_data);

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (GTK_IS_LIST_STORE (model));

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  launcher_dialog_items_set_item (model, &iter, item, NULL);
}



static gboolean
launcher_dialog_add_populate_model_idle (gpointer user_data)
{
  LauncherPluginDialog *dialog = user_data;
  GObject              *store;
  GHashTable           *pool;

  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);

  /* load the item pool */
  pool = launcher_plugin_garcon_menu_pool ();

  /* insert the items in the store */
  store = gtk_builder_get_object (dialog->builder, "add-store");
  g_hash_table_foreach (pool, launcher_dialog_add_store_insert, store);

  g_hash_table_destroy (pool);

  return FALSE;
}



static void
launcher_dialog_add_populate_model_idle_destroyed (gpointer user_data)
{
  ((LauncherPluginDialog *) user_data)->idle_populate_id = 0;
}



static void
launcher_dialog_add_populate_model (LauncherPluginDialog *dialog)
{
  GObject *store;

  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  /* get the store and make sure it's empty */
  store = gtk_builder_get_object (dialog->builder, "add-store");
  gtk_list_store_clear (GTK_LIST_STORE (store));

  /* fire an idle menu system load */
  if (G_LIKELY (dialog->idle_populate_id == 0))
    dialog->idle_populate_id = gdk_threads_add_idle_full (
        G_PRIORITY_DEFAULT_IDLE,
        launcher_dialog_add_populate_model_idle,
        dialog, launcher_dialog_add_populate_model_idle_destroyed);
}



static void
launcher_dialog_add_drag_data_get (GtkWidget            *treeview,
                                   GdkDragContext       *drag_context,
                                   GtkSelectionData     *data,
                                   guint                 info,
                                   guint                 timestamp,
                                   LauncherPluginDialog *dialog)
{
  GtkTreeSelection  *selection;
  GList             *rows, *li;
  GtkTreeModel      *model;
  gchar            **uris;
  GarconMenuItem    *item;
  guint              i;
  GtkTreeIter        iter;

  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));
  panel_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);
  if (G_UNLIKELY (rows == NULL))
    return;

  uris = g_new0 (gchar *, g_list_length (rows) + 1);
  for (li = rows, i = 0; li != NULL; li = li->next)
    {
      if (!gtk_tree_model_get_iter (model, &iter, li->data))
        continue;

      gtk_tree_model_get (model, &iter, COL_ITEM, &item, -1);
      if (G_UNLIKELY (item == NULL))
        continue;

      uris[i++] = garcon_menu_item_get_uri (item);
      g_object_unref (G_OBJECT (item));
    }

  gtk_selection_data_set_uris (data, uris);
  g_list_free (rows);
  g_strfreev (uris);
}



static void
launcher_dialog_add_selection_changed (GtkTreeSelection     *selection,
                                       LauncherPluginDialog *dialog)
{
  GObject  *object;
  gboolean  sensitive;

  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));
  panel_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  object = gtk_builder_get_object (dialog->builder, "button-add");
  sensitive = !!(gtk_tree_selection_count_selected_rows (selection) > 0);
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
}



static gboolean
launcher_dialog_add_button_press_event (GtkTreeView          *treeview,
                                        GdkEventButton       *event,
                                        LauncherPluginDialog *dialog)
{
  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);
  panel_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

  if (event->button == 1
      && event->type == GDK_2BUTTON_PRESS
      && event->window == gtk_tree_view_get_bin_window (treeview)
      && gtk_tree_view_get_path_at_pos (treeview, event->x, event->y,
                                        NULL, NULL, NULL, NULL))
    return launcher_dialog_press_event (dialog, "button-add");

  return FALSE;
}



static gboolean
launcher_dialog_add_key_press_event (GtkTreeView          *treeview,
                                     GdkEventKey          *event,
                                     LauncherPluginDialog *dialog)
{
  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);
  panel_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

  if (event->keyval == GDK_KEY_Return
      || event->keyval == GDK_KEY_KP_Enter)
    return launcher_dialog_press_event (dialog, "button-add");

  return FALSE;
}



static void
launcher_dialog_add_response (GtkWidget            *widget,
                              gint                  response_id,
                              LauncherPluginDialog *dialog)
{
  GObject          *treeview, *store;
  GtkTreeSelection *selection;
  GtkTreeModel     *item_model, *add_model;
  GtkTreeIter       iter, sibling, tmp;
  GarconMenuItem   *item;
  GList            *list, *li;

  panel_return_if_fail (GTK_IS_DIALOG (widget));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (dialog->plugin));

  if (response_id == 1)
    {
      /* add all the selected rows in the add dialog */
      treeview = gtk_builder_get_object (dialog->builder, "add-treeview");
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      list = gtk_tree_selection_get_selected_rows (selection, &add_model);

      /* append after the selected item in the item dialog */
      treeview = gtk_builder_get_object (dialog->builder, "item-treeview");
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      item_model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
      if (gtk_tree_selection_get_selected (selection, NULL, &sibling))
        gtk_list_store_insert_after (GTK_LIST_STORE (item_model),
                                     &iter, &sibling);
      else
        gtk_list_store_append (GTK_LIST_STORE (item_model), &iter);

      for (li = list; li != NULL; li = g_list_next (li))
        {
          /* get the selected file in the add dialog */
          gtk_tree_model_get_iter (add_model, &tmp, li->data);
          gtk_tree_model_get (add_model, &tmp, COL_ITEM, &item, -1);

          /* insert the item in the item store */
          if (G_LIKELY (item != NULL))
            {
              launcher_dialog_items_set_item (item_model, &iter, item, dialog);
              g_object_unref (G_OBJECT (item));

              /* select the first item */
              if (li == list)
                gtk_tree_selection_select_iter (selection, &iter);
            }

          gtk_tree_path_free (li->data);

          if (g_list_next (li) != NULL)
            {
              /* insert a new iter after the new added item */
              sibling = iter;
              gtk_list_store_insert_after (GTK_LIST_STORE (item_model),
                                           &iter, &sibling);
            }
        }

      g_list_free (list);

      /* write the model to xfconf */
      launcher_dialog_tree_save (dialog);

      /* update the selection */
      launcher_dialog_tree_selection_changed (selection, dialog);
    }
  else
    {
      /* empty the store */
      store = gtk_builder_get_object (dialog->builder, "add-store");
      gtk_list_store_clear (GTK_LIST_STORE (store));

      /* hide the dialog, since it's owned by gtkbuilder */
      gtk_widget_hide (widget);
    }
}



static gboolean
launcher_dialog_tree_save_foreach (GtkTreeModel *model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
  GPtrArray      *array = user_data;
  GValue         *value;
  GarconMenuItem *item;

  /* get the desktop id of the item from the store */
  gtk_tree_model_get (model, iter, COL_ITEM, &item, -1);
  if (G_LIKELY (item != NULL))
    {
      /* create a value with the filename */
      value = g_new0 (GValue, 1);
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, garcon_menu_item_get_uri (item));

      /* put it in the array and release */
      g_ptr_array_add (array, value);
      g_object_unref (G_OBJECT (item));
    }

  return FALSE;
}



static void
launcher_dialog_tree_save (LauncherPluginDialog *dialog)
{
  GObject   *store;
  GPtrArray *array;

  store = gtk_builder_get_object (dialog->builder, "item-store");

  array = g_ptr_array_new ();
  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          launcher_dialog_tree_save_foreach, array);

  g_signal_handlers_block_by_func (G_OBJECT (dialog->plugin),
          G_CALLBACK (launcher_dialog_items_load), dialog);
  g_object_set (dialog->plugin, "items", array, NULL);
    g_signal_handlers_unblock_by_func (G_OBJECT (dialog->plugin),
          G_CALLBACK (launcher_dialog_items_load), dialog);

  xfconf_array_free (array);
}

static gboolean
launcher_dialog_tree_save_cb (gpointer user_data)
{
  LauncherPluginDialog *dialog = user_data;

  launcher_dialog_tree_save (dialog);
  return FALSE;
}

static void
launcher_dialog_tree_drag_data_received (GtkWidget            *treeview,
                                         GdkDragContext       *context,
                                         gint                  x,
                                         gint                  y,
                                         GtkSelectionData     *data,
                                         guint                 info,
                                         guint                 timestamp,
                                         LauncherPluginDialog *dialog)
{
  GtkTreePath              *path;
  GtkTreeViewDropPosition   drop_pos;
  gchar                   **uris;
  GtkTreeIter               iter;
  GtkTreeModel             *model;
  gint                      position;
  GarconMenuItem           *item;
  guint                     i;

  panel_return_if_fail (GTK_IS_TREE_VIEW (treeview));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  if (info == DROP_TARGET_URI)
    {
      uris = gtk_selection_data_get_uris (data);
      if (G_LIKELY (uris == NULL))
        {
          gtk_drag_finish (context, FALSE, FALSE, timestamp);
          return;
        }

      /* get the insert position */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
      if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (treeview),
                                             x, y, &path, &drop_pos))
        {
          position = gtk_tree_path_get_indices (path)[0];
          gtk_tree_path_free (path);
          if (drop_pos == GTK_TREE_VIEW_DROP_AFTER
              || drop_pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
            position++;
        }
      else
        {
          /* prepend at the end of the model */
          position = gtk_tree_model_iter_n_children (model, NULL);
        }

      /* insert the uris */
      for (i = 0; uris[i] != NULL; i++)
        {
          if (!g_str_has_suffix (uris[i], ".desktop"))
            continue;

          item = garcon_menu_item_new_for_uri (uris[i]);
          if (G_UNLIKELY (item == NULL))
            continue;

          gtk_list_store_insert (GTK_LIST_STORE (model), &iter, position);
          launcher_dialog_items_set_item (model, &iter, item, dialog);
          g_object_unref (G_OBJECT (item));
        }

      g_strfreev (uris);

      launcher_dialog_tree_save (dialog);

      gtk_drag_finish (context, TRUE, FALSE, timestamp);
    }
  else if (info == DROP_TARGET_ROW)
    {
      /* nothing to do here, just wait for an row-inserted signal */
    }
}



static void
launcher_dialog_tree_selection_changed (GtkTreeSelection     *selection,
                                        LauncherPluginDialog *dialog)
{
  GObject        *object;
  GtkTreeModel   *model;
  GtkTreeIter     iter;
  gint            n_children = -1;
  gint            position = 0;
  GtkTreePath    *path;
  gboolean        sensitive;
  GarconMenuItem *item = NULL;
  gboolean        editable = FALSE;

  panel_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* get the number of launchers in the tree */
      n_children = gtk_tree_model_iter_n_children (model, NULL);

      /* get the position of the selected item in the tree */
      path = gtk_tree_model_get_path (model, &iter);
      position = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);

      gtk_tree_model_get (model, &iter, COL_ITEM, &item, -1);
      if (G_LIKELY (item != NULL))
        {
          editable = launcher_plugin_item_is_editable (dialog->plugin, item, NULL);
          g_object_unref (G_OBJECT (item));
        }
    }

  /* update the sensitivity of the buttons */
  sensitive = n_children > 0;
  object = gtk_builder_get_object (dialog->builder, "item-delete");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
  object = gtk_builder_get_object (dialog->builder, "mi-delete");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  sensitive = !!(position > 0 && position <= n_children);
  object = gtk_builder_get_object (dialog->builder, "item-move-up");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
  object = gtk_builder_get_object (dialog->builder, "mi-move-up");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  sensitive = !!(position >= 0 && position < n_children - 1);
  object = gtk_builder_get_object (dialog->builder, "item-move-down");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
  object = gtk_builder_get_object (dialog->builder, "mi-move-down");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  object = gtk_builder_get_object (dialog->builder, "item-edit");
  gtk_widget_set_sensitive (GTK_WIDGET (object), editable);
  object = gtk_builder_get_object (dialog->builder, "mi-edit");
  gtk_widget_set_sensitive (GTK_WIDGET (object), editable);

  sensitive = n_children > 1;

  object = gtk_builder_get_object (dialog->builder, "arrow-position");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  object = gtk_builder_get_object (dialog->builder, "move-first");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);

  object = gtk_builder_get_object (dialog->builder, "arrow-position-label");
  gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
}



static gboolean
launcher_dialog_press_event (LauncherPluginDialog *dialog,
                             const gchar          *object_name)
{
  GObject *object;

  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);

  object = gtk_builder_get_object (dialog->builder, object_name);
  panel_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
  if (gtk_widget_get_sensitive (GTK_WIDGET (object)))
    {
      gtk_button_clicked (GTK_BUTTON (object));
      return TRUE;
    }

  return FALSE;
}



static gboolean
launcher_dialog_tree_popup_menu (GtkWidget            *treeview,
                                 LauncherPluginDialog *dialog)
{
  GObject *menu;

  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);
  panel_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

  /* show the menu */
  menu = gtk_builder_get_object (dialog->builder, "popup-menu");
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
  return TRUE;
}



static void
launcher_dialog_tree_popup_menu_activated (GtkWidget            *mi,
                                           LauncherPluginDialog *dialog)
{
  const gchar *name;

  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));
  panel_return_if_fail (GTK_IS_BUILDABLE (mi));

  /* name of the button */
  name = gtk_buildable_get_name (GTK_BUILDABLE (mi));
  if (G_UNLIKELY (name == NULL))
    return;

  /* click the button in the dialog to trigger the action */
  if (strcmp (name, "mi-move-up") == 0)
    launcher_dialog_press_event (dialog, "item-move-up");
  else if (strcmp (name, "mi-move-down") == 0)
    launcher_dialog_press_event (dialog, "item-move-down");
  else if (strcmp (name, "mi-edit") == 0)
    launcher_dialog_press_event (dialog, "item-edit");
  else if (strcmp (name, "mi-delete") == 0)
    launcher_dialog_press_event (dialog, "item-delete");
  else if (strcmp (name, "mi-add") == 0)
    launcher_dialog_press_event (dialog, "item-add");
  else if (strcmp (name, "mi-application") == 0)
    launcher_dialog_press_event (dialog, "item-new");
  else if (strcmp (name, "mi-link") == 0)
    launcher_dialog_item_desktop_item_edit (mi, "Link", NULL, dialog);
  else
    panel_assert_not_reached ();
}



static gboolean
launcher_dialog_tree_button_press_event (GtkTreeView          *treeview,
                                         GdkEventButton       *event,
                                         LauncherPluginDialog *dialog)
{
  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);
  panel_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

  if (event->button == 1
      && event->type == GDK_2BUTTON_PRESS
      && event->window == gtk_tree_view_get_bin_window (treeview)
      && gtk_tree_view_get_path_at_pos (treeview, event->x, event->y,
                                        NULL, NULL, NULL, NULL))
    {
      return launcher_dialog_press_event (dialog, "item-edit");
    }
  else if (event->button == 3
           && event->type == GDK_BUTTON_PRESS)
    {
      launcher_dialog_tree_popup_menu (GTK_WIDGET (treeview), dialog);
    }

  return FALSE;
}



static gboolean
launcher_dialog_tree_key_press_event (GtkTreeView          *treeview,
                                      GdkEventKey          *event,
                                      LauncherPluginDialog *dialog)
{
  panel_return_val_if_fail (GTK_IS_BUILDER (dialog->builder), FALSE);
  panel_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

  if (event->keyval == GDK_KEY_Return
      || event->keyval == GDK_KEY_KP_Enter)
    return launcher_dialog_press_event (dialog, "item-edit");

  return FALSE;
}



static void
launcher_dialog_item_desktop_item_edit (GtkWidget            *widget,
                                        const gchar          *type,
                                        const gchar          *uri,
                                        LauncherPluginDialog *dialog)
{
  gchar     *filename;
  gchar     *command;
  GdkScreen *screen;
  GError    *error = NULL;
  GtkWidget *toplevel;

  panel_return_if_fail (GTK_IS_WIDGET (widget));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));
  panel_return_if_fail (type != NULL || uri != NULL);

  /* build command */
  if (uri != NULL)
    {
      command = g_strdup_printf ("exo-desktop-item-edit --xid=0x%x '%s'",
                                 LAUNCHER_WIDGET_XID (widget), uri);
    }
  else
    {
      filename = launcher_plugin_unique_filename (dialog->plugin);
      command = g_strdup_printf ("exo-desktop-item-edit -t %s -c --xid=0x%x '%s'",
                                 type, LAUNCHER_WIDGET_XID (widget),
                                 filename);
      g_free (filename);
    }

  /* spawn item editor */
  screen = gtk_widget_get_screen (widget);
  if (!xfce_spawn_command_line (screen, command, FALSE, FALSE, TRUE, &error))
    {
      toplevel = gtk_widget_get_toplevel (widget);
      xfce_dialog_show_error (GTK_WINDOW (toplevel), error,
          _("Failed to open desktop item editor"));
      g_error_free (error);
    }

  g_free (command);
}



static void
launcher_dialog_item_link_button_clicked (GtkWidget            *button,
                                          LauncherPluginDialog *dialog)
{
  launcher_dialog_item_desktop_item_edit (button, "Link", NULL, dialog);
}



static void
launcher_dialog_item_button_clicked (GtkWidget            *button,
                                     LauncherPluginDialog *dialog)
{
  const gchar      *name;
  const gchar      *display_name = NULL;
  GObject          *object;
  GObject          *treeview;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter_a, iter_b;
  GtkTreePath      *path;
  gchar            *uri;
  GarconMenuItem   *item;
  GtkWidget        *toplevel;
  gboolean          save_items = TRUE;

  panel_return_if_fail (GTK_IS_BUILDABLE (button));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  /* name of the button */
  name = gtk_buildable_get_name (GTK_BUILDABLE (button));
  if (G_UNLIKELY (name == NULL))
    return;

  if (strcmp (name, "item-add") == 0)
    {
      object = gtk_builder_get_object (dialog->builder, "dialog-add");
      launcher_dialog_add_populate_model (dialog);
      gtk_widget_show (GTK_WIDGET (object));
    }
  else
    {
      /* get the selected item in the tree, leave if none is found */
      treeview = gtk_builder_get_object (dialog->builder, "item-treeview");
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      if (!gtk_tree_selection_get_selected (selection, &model, &iter_a)
          && strcmp (name, "item-new") != 0)
        return;

      if (strcmp (name, "item-delete") == 0)
        {
          /* get item name */
          gtk_tree_model_get (model, &iter_a, COL_ITEM, &item, -1);
          if (G_LIKELY (item != NULL))
            display_name = garcon_menu_item_get_name (item);

          /* ask the user */
          toplevel = gtk_widget_get_toplevel (button);
          if (xfce_dialog_confirm (GTK_WINDOW (toplevel), "edit-delete", _("_Remove"),
                  _("If you delete an item, it will be permanently removed"),
                  _("Are you sure you want to remove \"%s\"?"),
                  panel_str_is_empty (display_name) ? _("Unnamed item") : display_name))
            {
              /* remove the item from the store */
              gtk_list_store_remove (GTK_LIST_STORE (model), &iter_a);

              /* the .desktop file will be automatically removed in the launcher code */
            }
          else
            {
              save_items = FALSE;
            }

          if (G_LIKELY (item != NULL))
            g_object_unref (G_OBJECT (item));
        }
      else if (strcmp (name, "item-new") == 0
               || strcmp (name, "item-edit") == 0)
        {
          if (strcmp (name, "item-edit") == 0)
            {
              gtk_tree_model_get (model, &iter_a, COL_ITEM, &item, -1);
              if (G_UNLIKELY (item == NULL))
                return;

              /* build command */
              uri = garcon_menu_item_get_uri (item);
              launcher_dialog_item_desktop_item_edit (button, NULL, uri, dialog);
              g_free (uri);
            }
          else
            {
              launcher_dialog_item_desktop_item_edit (button, "Application", NULL, dialog);
            }

          save_items = FALSE;
        }
      else if (strcmp (name, "item-move-up") == 0)
        {
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter_a);
          if (gtk_tree_path_prev (path)
              && gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter_b, path))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter_a, &iter_b);
          gtk_tree_path_free (path);
        }
      else if (strcmp (name, "item-move-down") == 0)
        {
          iter_b = iter_a;
          if (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter_b))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter_a, &iter_b);
        }
      else
        {
          panel_assert_not_reached ();
        }

      /* store the new settings */
      if (save_items)
        launcher_dialog_tree_save (dialog);

      /* emit a changed signal to update the button states */
      launcher_dialog_tree_selection_changed (selection, dialog);
    }
}



static void
launcher_dialog_response (GtkWidget            *widget,
                          gint                  response_id,
                          LauncherPluginDialog *dialog)
{
  GObject *add_dialog;

  panel_return_if_fail (GTK_IS_DIALOG (widget));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (dialog->plugin));
  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  if (G_UNLIKELY (response_id != 1))
    {
      /* stop idle if still running */
      if (G_UNLIKELY (dialog->idle_populate_id != 0))
        g_source_remove (dialog->idle_populate_id);

      /* disconnect from items-changed signal */
      g_signal_handlers_disconnect_by_func (G_OBJECT (dialog->plugin),
          G_CALLBACK (launcher_dialog_items_load), dialog);

      /* disconnect from the menu items */
      launcher_dialog_items_unload (dialog);

      /* also destroy the add dialog */
      add_dialog = gtk_builder_get_object (dialog->builder, "dialog-add");
      gtk_widget_destroy (GTK_WIDGET (add_dialog));

      /* destroy the dialog */
      gtk_widget_destroy (widget);

      g_slice_free (LauncherPluginDialog, dialog);
    }
}



static gboolean
launcher_dialog_item_changed_foreach (GtkTreeModel *model,
                                      GtkTreePath  *path,
                                      GtkTreeIter  *iter,
                                      gpointer      user_data)
{
  GarconMenuItem         *item;
  gboolean                found;
  LauncherChangedHandler *handler = user_data;

  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (handler->item), TRUE);

  /* check if this is the item in the model */
  gtk_tree_model_get (model, iter, COL_ITEM, &item, -1);
  found = !!(item == handler->item);

  if (G_UNLIKELY (found))
    launcher_dialog_items_set_item (model, iter, item, handler->dialog);

  g_object_unref (G_OBJECT (item));

  return found;
}



static void
launcher_dialog_item_changed (GarconMenuItem       *item,
                              LauncherPluginDialog *dialog)
{
  GObject                *treeview;
  GtkTreeModel           *model;
  LauncherChangedHandler *handler;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));

  treeview = gtk_builder_get_object (dialog->builder, "item-treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  /* find the item in the model and update it */
  handler = g_slice_new0 (LauncherChangedHandler);
  handler->dialog = dialog;
  handler->item = item;
  gtk_tree_model_foreach (model, launcher_dialog_item_changed_foreach, handler);
  g_slice_free (LauncherChangedHandler, handler);
}



static void
launcher_dialog_tree_row_changed (GtkTreeModel         *model,
                                  GtkTreePath          *path,
                                  GtkTreeIter          *iter,
                                  LauncherPluginDialog *dialog)
{
  GtkTreeSelection *selection;
  GObject          *treeview;

  panel_return_if_fail (GTK_IS_BUILDER (dialog->builder));

  /* item moved with dnd, save the tree to update the plugin */
  gdk_threads_add_idle (launcher_dialog_tree_save_cb, dialog);

  /* select the moved item to there is no selection change on reload */
  treeview = gtk_builder_get_object (dialog->builder, "item-treeview");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_select_iter (selection, iter);
}



static void
launcher_dialog_items_set_item (GtkTreeModel         *model,
                                GtkTreeIter          *iter,
                                GarconMenuItem       *item,
                                LauncherPluginDialog *dialog)
{
  const gchar *name, *comment;
  gchar       *markup;
  GIcon       *icon;
  gchar       *tooltip;
  GFile       *gfile;

  panel_return_if_fail (GTK_IS_LIST_STORE (model));
  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));

  name = garcon_menu_item_get_name (item);
  comment = garcon_menu_item_get_comment (item);

  if (!panel_str_is_empty (comment))
    markup = g_markup_printf_escaped ("<b>%s</b>\n%s", name, comment);
  else
    markup = g_markup_printf_escaped ("<b>%s</b>", name);

  icon = launcher_plugin_tooltip_icon (garcon_menu_item_get_icon_name (item));

  if (dialog != NULL)
    g_signal_handlers_block_by_func (G_OBJECT (model),
        G_CALLBACK (launcher_dialog_tree_row_changed), dialog);

  gfile = garcon_menu_item_get_file (item);
  tooltip = g_file_get_parse_name (gfile);
  g_object_unref (G_OBJECT (gfile));

  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      COL_ICON, icon,
                      COL_NAME, markup,
                      COL_ITEM, item,
                      COL_TOOLTIP, tooltip,
                      -1);

  if (dialog != NULL)
    g_signal_handlers_unblock_by_func (G_OBJECT (model),
        G_CALLBACK (launcher_dialog_tree_row_changed), dialog);

  if (G_LIKELY (icon != NULL))
    g_object_unref (G_OBJECT (icon));
  g_free (markup);
  g_free (tooltip);
}



static void
launcher_dialog_items_unload (LauncherPluginDialog *dialog)
{
  GSList *li;

  for (li = dialog->items; li != NULL; li = li->next)
    {
      panel_return_if_fail (GARCON_IS_MENU_ITEM (li->data));
      g_signal_handlers_disconnect_by_func (G_OBJECT (li->data),
          G_CALLBACK (launcher_dialog_item_changed), dialog);
      g_object_unref (G_OBJECT (li->data));
    }

  g_slist_free (dialog->items);
  dialog->items = NULL;
}



static void
launcher_dialog_items_load (LauncherPluginDialog *dialog)
{
  GObject          *treeview;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GSList           *li;
  GtkTreeIter       iter;
  GtkTreePath      *path;

  /* get the treeview */
  treeview = gtk_builder_get_object (dialog->builder, "item-treeview");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  /* we try to preserve the current selection */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    path = gtk_tree_model_get_path (model, &iter);
  else
    path = NULL;

  /* clear the old items */
  launcher_dialog_items_unload (dialog);
  gtk_list_store_clear (GTK_LIST_STORE (model));

  /* insert the launcher items */
  dialog->items = launcher_plugin_get_items (dialog->plugin);
  for (li = dialog->items; li != NULL; li = li->next)
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      launcher_dialog_items_set_item (model, &iter, GARCON_MENU_ITEM (li->data), dialog);
      g_signal_connect (G_OBJECT (li->data), "changed",
          G_CALLBACK (launcher_dialog_item_changed), dialog);
    }

  if (G_LIKELY (path != NULL))
    {
      /* restore the old selection */
      gtk_tree_selection_select_path (selection, path);
      gtk_tree_path_free (path);
    }
  else if (gtk_tree_model_get_iter_first (model, &iter))
    {
      /* select the first item */
      gtk_tree_selection_select_iter (selection, &iter);
    }
}



void
launcher_dialog_show (LauncherPlugin *plugin)
{
  LauncherPluginDialog *dialog;
  GtkBuilder           *builder;
  GObject              *window, *object, *item;
  guint                 i;
  GtkTreeSelection     *selection;
  const gchar          *button_names[] = { "item-add", "item-delete",
                                           "item-move-up", "item-move-down",
                                           "item-edit", "item-new" };
  const gchar          *mi_names[] = { "mi-edit", "mi-delete",
                                       "mi-application", "mi-link", "mi-add",
                                       "mi-move-up", "mi-move-down" };
  const gchar          *binding_names[] = { "disable-tooltips", "show-label",
                                            "move-first", "arrow-position" };

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* setup the dialog */
  PANEL_UTILS_LINK_4UI
  builder = panel_utils_builder_new (XFCE_PANEL_PLUGIN (plugin), launcher_dialog_ui,
                                     launcher_dialog_ui_length, &window);
  if (G_UNLIKELY (builder == NULL))
    return;

  /* create structure */
  dialog = g_slice_new0 (LauncherPluginDialog);
  dialog->builder = builder;
  dialog->plugin = plugin;
  dialog->items = NULL;

  g_signal_connect (G_OBJECT (window), "response",
      G_CALLBACK (launcher_dialog_response), dialog);

  /* connect item buttons */
  for (i = 0; i < G_N_ELEMENTS (button_names); i++)
    {
      object = gtk_builder_get_object (builder, button_names[i]);
      panel_return_if_fail (GTK_IS_WIDGET (object));
      g_signal_connect (G_OBJECT (object), "clicked",
          G_CALLBACK (launcher_dialog_item_button_clicked), dialog);
    }
  object = gtk_builder_get_object (builder, "item-link");
  g_signal_connect (G_OBJECT (object), "clicked",
                    G_CALLBACK (launcher_dialog_item_link_button_clicked), dialog);

  /* connect menu items */
  for (i = 0; i < G_N_ELEMENTS (mi_names); i++)
    {
      object = gtk_builder_get_object (builder, mi_names[i]);
      panel_return_if_fail (GTK_IS_WIDGET (object));
      g_signal_connect (G_OBJECT (object), "activate",
          G_CALLBACK (launcher_dialog_tree_popup_menu_activated), dialog);
    }

  object = gtk_builder_get_object (dialog->builder, "item-store");
  g_signal_connect (G_OBJECT (object), "row-changed",
      G_CALLBACK (launcher_dialog_tree_row_changed), dialog);

  /* setup treeview selection */
  object = gtk_builder_get_object (builder, "item-treeview");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (object),
      list_drop_targets, G_N_ELEMENTS (list_drop_targets), GDK_ACTION_COPY);
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (object), GDK_BUTTON1_MASK,
      list_drag_targets, G_N_ELEMENTS (list_drag_targets), GDK_ACTION_MOVE);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (object), "drag-data-received",
      G_CALLBACK (launcher_dialog_tree_drag_data_received), dialog);
  g_signal_connect (G_OBJECT (selection), "changed",
      G_CALLBACK (launcher_dialog_tree_selection_changed), dialog);
  launcher_dialog_tree_selection_changed (selection, dialog);
  g_signal_connect (G_OBJECT (object), "button-press-event",
      G_CALLBACK (launcher_dialog_tree_button_press_event), dialog);
  g_signal_connect (G_OBJECT (object), "key-press-event",
      G_CALLBACK (launcher_dialog_tree_key_press_event), dialog);
  g_signal_connect (G_OBJECT (object), "popup-menu",
      G_CALLBACK (launcher_dialog_tree_popup_menu), dialog);

  /* connect bindings to the advanced properties */
  for (i = 0; i < G_N_ELEMENTS (binding_names); i++)
    {
      object = gtk_builder_get_object (builder, binding_names[i]);
      panel_return_if_fail (GTK_IS_WIDGET (object));
      g_object_bind_property (G_OBJECT (plugin), binding_names[i],
                              G_OBJECT (object), "active",
                              G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    }

  /* setup responses for the add dialog */
  object = gtk_builder_get_object (builder, "dialog-add");
  gtk_window_set_screen (GTK_WINDOW (object),
      gtk_window_get_screen (GTK_WINDOW (window)));
  g_signal_connect (G_OBJECT (object), "response",
      G_CALLBACK (launcher_dialog_add_response), dialog);
  g_signal_connect (G_OBJECT (object), "delete-event",
      G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  /* setup sorting in the add dialog */
  object = gtk_builder_get_object (builder, "add-store");
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (object),
                                        COL_NAME, GTK_SORT_ASCENDING);

  /* allow selecting multiple items in the add dialog */
  object = gtk_builder_get_object (builder, "add-treeview");
  gtk_drag_source_set (GTK_WIDGET (object), GDK_BUTTON1_MASK,
      add_drag_targets, G_N_ELEMENTS (add_drag_targets), GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (object), "drag-data-get",
      G_CALLBACK (launcher_dialog_add_drag_data_get), dialog);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (G_OBJECT (selection), "changed",
      G_CALLBACK (launcher_dialog_add_selection_changed), dialog);
  g_signal_connect (G_OBJECT (object), "button-press-event",
      G_CALLBACK (launcher_dialog_add_button_press_event), dialog);
  g_signal_connect (G_OBJECT (object), "key-press-event",
      G_CALLBACK (launcher_dialog_add_key_press_event), dialog);

  /* setup search filter in the add dialog */
  object = gtk_builder_get_object (builder, "add-store-filter");
  item = gtk_builder_get_object (builder, "add-search");
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (object),
      launcher_dialog_add_visible_function, item, NULL);
  g_signal_connect_swapped (G_OBJECT (item), "changed",
      G_CALLBACK (gtk_tree_model_filter_refilter), object);

  /* load the plugin items */
  launcher_dialog_items_load (dialog);
  g_signal_connect_swapped (G_OBJECT (plugin), "items-changed",
      G_CALLBACK (launcher_dialog_items_load), dialog);

  /* show the dialog */
  gtk_widget_show (GTK_WIDGET (window));
}
