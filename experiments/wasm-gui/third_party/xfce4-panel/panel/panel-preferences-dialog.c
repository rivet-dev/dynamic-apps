/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <common/panel-private.h>
#include <common/panel-utils.h>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include <panel/panel-window.h>
#include <panel/panel-application.h>
#include <panel/panel-dialogs.h>
#include <panel/panel-module.h>
#include <panel/panel-itembar.h>
#include <panel/panel-item-dialog.h>
#include <panel/panel-preferences-dialog.h>
#include <panel/panel-preferences-dialog-ui.h>
#include <panel/panel-plugin-external.h>

#define PREFERENCES_HELP_URL "http://www.xfce.org"



static void                     panel_preferences_dialog_finalize               (GObject                *object);
static void                     panel_preferences_dialog_response               (GtkWidget              *window,
                                                                                 gint                    response_id,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_bindings_unbind        (PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_bindings_add           (PanelPreferencesDialog *dialog,
                                                                                 const gchar            *property1,
                                                                                 const gchar            *property2,
                                                                                 GBindingFlags           flags);
static void                     panel_preferences_dialog_bindings_update        (PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_output_changed         (GtkComboBox            *combobox,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_autohide_changed       (GtkComboBox            *combobox,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_bg_style_changed       (PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_bg_image_file_set      (GtkFileChooserButton   *button,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_bg_image_notified      (PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_length_max_notified    (PanelPreferencesDialog *dialog);
static gboolean                 panel_preferences_dialog_length_transform_to    (GBinding               *binding,
                                                                                 const GValue           *from_value,
                                                                                 GValue                 *to_value,
                                                                                 gpointer                user_data);
static gboolean                 panel_preferences_dialog_length_transform_from  (GBinding               *binding,
                                                                                 const GValue           *from_value,
                                                                                 GValue                 *to_value,
                                                                                 gpointer                user_data);
static void                     panel_preferences_dialog_panel_combobox_changed (GtkComboBox            *combobox,
                                                                                 PanelPreferencesDialog *dialog);
static gboolean                 panel_preferences_dialog_panel_combobox_rebuild (PanelPreferencesDialog *dialog,
                                                                                 gint                    panel_id);
static void                     panel_preferences_dialog_panel_add              (GtkWidget              *widget,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_panel_remove           (GtkWidget              *widget,
                                                                                 PanelPreferencesDialog *dialog);
static gboolean                 panel_preferences_dialog_icon_size_state_set    (GtkSwitch              *widget,
                                                                                 gboolean                state,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_icon_size_changed      (GtkSpinButton          *button,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_compositing_clicked    (GtkButton              *button,
                                                                                 gpointer                user_data);
static void                     panel_preferences_dialog_panel_switch           (GtkWidget              *widget,
                                                                                 PanelPreferencesDialog *dialog);
static XfcePanelPluginProvider *panel_preferences_dialog_item_get_selected      (PanelPreferencesDialog *dialog,
                                                                                 GtkTreeIter            *return_iter);
static void                     panel_preferences_dialog_item_store_rebuild     (GtkWidget              *itembar,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_move              (GtkWidget              *button,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_remove            (GtkWidget              *button,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_add               (GtkWidget              *button,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_properties        (GtkWidget              *button,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_about             (GtkWidget              *button,
                                                                                 PanelPreferencesDialog *dialog);
static gboolean                 panel_preferences_dialog_treeview_clicked       (GtkTreeView            *treeview,
                                                                                 GdkEventButton         *event,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_row_changed       (GtkTreeModel           *model,
                                                                                 GtkTreePath            *path,
                                                                                 GtkTreeIter            *iter,
                                                                                 PanelPreferencesDialog *dialog);
static void                     panel_preferences_dialog_item_selection_changed (GtkTreeSelection       *selection,
                                                                                 PanelPreferencesDialog *dialog);



enum
{
  ITEM_COLUMN_ICON_NAME,
  ITEM_COLUMN_DISPLAY_NAME,
  ITEM_COLUMN_TOOLTIP,
  ITEM_COLUMN_PROVIDER,
  N_ITEM_COLUMNS
};

enum
{
  OUTPUT_NAME,
  OUTPUT_TITLE
};

struct _PanelPreferencesDialogClass
{
  GtkBuilderClass __parent__;
};

struct _PanelPreferencesDialog
{
  GtkBuilder  __parent__;

  PanelApplication *application;

  /* currently selected window in the selector */
  PanelWindow      *active;

  /* GBinding's between dialog <-> window */
  GSList           *bindings;

  /* store for the items list */
  GtkListStore     *store;

  /* changed signal for the active panel's itembar */
  gulong            items_changed_handler_id;

  /* background image watch */
  gulong            bg_image_notify_handler_id;

  /* changed signal for the output selector */
  gulong            output_changed_handler_id;

  /* plug in which the dialog is embedded */
  GtkWidget        *socket_plug;
};



G_DEFINE_TYPE (PanelPreferencesDialog, panel_preferences_dialog, GTK_TYPE_BUILDER)



static PanelPreferencesDialog *dialog_singleton = NULL;



static void
panel_preferences_dialog_class_init (PanelPreferencesDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = panel_preferences_dialog_finalize;
}



static void
panel_preferences_dialog_init (PanelPreferencesDialog *dialog)
{
  GObject           *window;
  GObject           *object;
  GObject           *info;
  GObject           *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;
  gchar             *path_old;
  gchar             *path_new;
  gchar             *xfwm4_tweaks;

  dialog->bindings = NULL;
  dialog->application = panel_application_get ();

  /* block autohide */
  panel_application_windows_blocked (dialog->application, TRUE);

  /* load the builder data into the object */
  gtk_builder_add_from_string (GTK_BUILDER (dialog), panel_preferences_dialog_ui,
                               panel_preferences_dialog_ui_length, NULL);

  /* get the dialog */
  window = gtk_builder_get_object (GTK_BUILDER (dialog), "dialog");
  panel_return_if_fail (GTK_IS_WIDGET (window));
  g_signal_connect (G_OBJECT (window), "response",
      G_CALLBACK (panel_preferences_dialog_response), dialog);

#define connect_signal(name,detail_signal,c_handler) \
  object = gtk_builder_get_object (GTK_BUILDER (dialog), name); \
  panel_return_if_fail (G_IS_OBJECT (object)); \
  g_signal_connect (G_OBJECT (object), detail_signal, G_CALLBACK (c_handler), dialog);

  /* panel selector buttons and combobox */
  connect_signal ("panel-add", "clicked", panel_preferences_dialog_panel_add);
  connect_signal ("panel-remove", "clicked", panel_preferences_dialog_panel_remove);
  connect_signal ("panel-combobox", "changed", panel_preferences_dialog_panel_combobox_changed);

  /* check if xfce4-panel-profiles or panel-switch are installed and if either is show the button */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-profiles");
  path_old = g_find_program_in_path ("xfpanel-switch");
  path_new = g_find_program_in_path ("xfce4-panel-profiles");
  if (path_new == NULL && path_old == NULL)
    gtk_widget_hide (GTK_WIDGET (object));
  g_free (path_old);
  g_free (path_new);
  connect_signal ("panel-switch", "clicked", panel_preferences_dialog_panel_switch);

  /* appearance tab */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-style");
  panel_return_if_fail (G_IS_OBJECT (object));
  g_signal_connect_swapped (G_OBJECT (object), "changed",
      G_CALLBACK (panel_preferences_dialog_bg_style_changed), dialog);

  /* icon size switch, handling enabling/disabling of custom icon sizes */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-spinbutton");
  panel_return_if_fail (G_IS_OBJECT (object));
  g_signal_connect (G_OBJECT (object), "value-changed",
                    G_CALLBACK (panel_preferences_dialog_icon_size_changed), dialog);
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-switch");
  panel_return_if_fail (G_IS_OBJECT (object));
  g_signal_connect (G_OBJECT (object), "state-set",
                    G_CALLBACK (panel_preferences_dialog_icon_size_state_set), dialog);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "composited");
  panel_return_if_fail (G_IS_OBJECT (object));
  g_signal_connect_swapped (G_OBJECT (object), "notify::visible",
      G_CALLBACK (panel_preferences_dialog_bg_style_changed), dialog);

  info = gtk_builder_get_object (GTK_BUILDER (dialog), "composited-info");
  panel_return_if_fail (G_IS_OBJECT (info));
  g_object_bind_property (G_OBJECT (object), "sensitive",
                          G_OBJECT (info), "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "xfwm4-settings");
  panel_return_if_fail (G_IS_OBJECT (object));
  xfwm4_tweaks = g_find_program_in_path ("xfwm4-tweaks-settings");
  gtk_widget_set_visible (GTK_WIDGET (object), xfwm4_tweaks != NULL);
  g_free (xfwm4_tweaks);
  g_signal_connect (G_OBJECT (object), "clicked",
                    G_CALLBACK (panel_preferences_dialog_compositing_clicked), NULL);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-image");
  panel_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (object));
  g_signal_connect (G_OBJECT (object), "file-set",
    G_CALLBACK (panel_preferences_dialog_bg_image_file_set), dialog);

  /* items treeview and buttons */
  connect_signal ("item-up", "clicked", panel_preferences_dialog_item_move);
  connect_signal ("item-down", "clicked", panel_preferences_dialog_item_move);
  connect_signal ("item-remove", "clicked", panel_preferences_dialog_item_remove);
  connect_signal ("item-add", "clicked", panel_preferences_dialog_item_add);
  connect_signal ("item-properties", "clicked", panel_preferences_dialog_item_properties);
  connect_signal ("item-about", "clicked", panel_preferences_dialog_item_about);

  /* create store for panel items */
  dialog->store = gtk_list_store_new (N_ITEM_COLUMNS,
                                      G_TYPE_STRING, /* ITEM_COLUMN_ICON_NAME */
                                      G_TYPE_STRING, /* ITEM_COLUMN_DISPLAY_NAME */
                                      G_TYPE_STRING, /* ITEM_COLUMN_TOOLTIP */
                                      G_TYPE_OBJECT); /* ITEM_COLUMN_PROVIDER */

  /* build tree for panel items */
  treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
  panel_return_if_fail (GTK_IS_WIDGET (treeview));
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (dialog->store));
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), ITEM_COLUMN_TOOLTIP);
  g_signal_connect (G_OBJECT (treeview), "button-press-event",
      G_CALLBACK (panel_preferences_dialog_treeview_clicked), dialog);

  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), TRUE);
  g_signal_connect (G_OBJECT (dialog->store), "row-changed",
      G_CALLBACK (panel_preferences_dialog_item_row_changed), dialog);

  /* setup tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed",
      G_CALLBACK (panel_preferences_dialog_item_selection_changed), dialog);

  /* icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer, "icon-name",
                                                     ITEM_COLUMN_ICON_NAME, NULL);
  g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* text renderer */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "markup",
                                       ITEM_COLUMN_DISPLAY_NAME, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* connect the output changed signal */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "output-name");
  panel_return_if_fail (GTK_IS_COMBO_BOX (object));
  dialog->output_changed_handler_id =
      g_signal_connect (G_OBJECT (object), "changed",
                        G_CALLBACK (panel_preferences_dialog_output_changed),
                        dialog);

  /* connect the autohide behavior changed signal */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "autohide-behavior");
  panel_return_if_fail (GTK_IS_COMBO_BOX (object));
  g_signal_connect (G_OBJECT (object), "changed",
      G_CALLBACK (panel_preferences_dialog_autohide_changed), dialog);
}



static void
panel_preferences_dialog_finalize (GObject *object)
{
  PanelPreferencesDialog *dialog = PANEL_PREFERENCES_DIALOG (object);
  GtkWidget              *itembar;

  /* unblock autohide */
  panel_application_windows_blocked (dialog->application, FALSE);

  /* free bindings list */
  g_slist_free (dialog->bindings);

  /* destroy possible pluggable dialog */
  if (dialog->socket_plug != NULL)
    gtk_widget_destroy (dialog->socket_plug);

  if (dialog->active != NULL)
    {
      if (dialog->items_changed_handler_id != 0)
        {
          /* disconnect changed signal */
          itembar = gtk_bin_get_child (GTK_BIN (dialog->active));
          g_signal_handler_disconnect (G_OBJECT (itembar),
              dialog->items_changed_handler_id);
        }

      if (dialog->bg_image_notify_handler_id != 0)
        {
          g_signal_handler_disconnect (G_OBJECT (dialog->active),
              dialog->bg_image_notify_handler_id);
        }
    }

  /* deselect all windows */
  if (!panel_item_dialog_visible ())
    panel_application_window_select (dialog->application, NULL);

  g_object_unref (G_OBJECT (dialog->application));
  g_object_unref (G_OBJECT (dialog->store));

  (*G_OBJECT_CLASS (panel_preferences_dialog_parent_class)->finalize) (object);
}



static void
panel_preferences_dialog_response (GtkWidget              *window,
                                   gint                    response_id,
                                   PanelPreferencesDialog *dialog)
{
  panel_return_if_fail (GTK_IS_DIALOG (window));
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  if (G_UNLIKELY (response_id == 1))
    {
      panel_utils_show_help (GTK_WINDOW (window), "preferences", NULL);
    }
  else
    {
      gtk_widget_destroy (window);
      g_object_unref (G_OBJECT (dialog));
    }
}



static void
panel_preferences_dialog_bindings_unbind (PanelPreferencesDialog *dialog)
{
  GSList *li;

  if (dialog->bindings != NULL)
    {
      /* remove all bindings */
      for (li = dialog->bindings; li != NULL; li = li->next)
        g_object_unref (G_OBJECT (li->data));

      g_slist_free (dialog->bindings);
      dialog->bindings = NULL;
    }

  /* disconnect image watch */
  if (dialog->bg_image_notify_handler_id != 0)
    {
      if (dialog->active != NULL)
        {
          g_signal_handler_disconnect (G_OBJECT (dialog->active),
              dialog->bg_image_notify_handler_id);
        }

      dialog->bg_image_notify_handler_id = 0;
    }

  if (dialog->active != NULL)
    g_signal_handlers_disconnect_by_func (dialog->active,
                                          panel_preferences_dialog_length_max_notified, dialog);
}



static void
panel_preferences_dialog_bindings_add (PanelPreferencesDialog *dialog,
                                       const gchar            *property1,
                                       const gchar            *property2,
                                       GBindingFlags           flags)
{
  GBinding *binding;
  GObject  *object;

  /* get the object from the builder */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), property1);
  panel_return_if_fail (G_IS_OBJECT (object));

  /* create the binding and prepend to the list */
  binding = g_object_bind_property (G_OBJECT (dialog->active), property1, object, property2,
                                    flags ? flags : G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  dialog->bindings = g_slist_prepend (dialog->bindings, binding);
}



static void
panel_preferences_dialog_bindings_update (PanelPreferencesDialog *dialog)
{
  GdkDisplay  *display;
  GdkMonitor  *monitor;
  gint         n_monitors = 1;
  GObject     *object;
  GObject     *store;
  GBinding    *binding;
  gchar       *output_name = NULL;
  gboolean     selector_visible = TRUE;
  GtkTreeIter  iter;
  gboolean     output_selected = FALSE;
  gint         n = 0, i;
  gchar       *name, *title;
  gboolean     span_monitors_sensitive = FALSE;
  gboolean     icon_size_set;

  /* leave when there is no active panel */
  panel_return_if_fail (G_IS_OBJECT (dialog->active));
  if (dialog->active == NULL)
    return;

  /* hook up the bindings */
  panel_preferences_dialog_bindings_add (dialog, "mode", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "span-monitors", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "position-locked", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "autohide-behavior", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "enable-struts", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "size", "value", 0);
  panel_preferences_dialog_bindings_add (dialog, "nrows", "value", 0);
  panel_preferences_dialog_bindings_add (dialog, "length-adjust", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "enter-opacity", "value", 0);
  panel_preferences_dialog_bindings_add (dialog, "leave-opacity", "value", 0);
  panel_preferences_dialog_bindings_add (dialog, "composited", "sensitive", G_BINDING_SYNC_CREATE);
  panel_preferences_dialog_bindings_add (dialog, "background-style", "active", 0);
  panel_preferences_dialog_bindings_add (dialog, "background-rgba", "rgba", 0);
  panel_preferences_dialog_bindings_add (dialog, "icon-size", "value", 0);
  panel_preferences_dialog_bindings_add (dialog, "dark-mode", "active", 0);

  /* watch image changes from the panel */
  dialog->bg_image_notify_handler_id = g_signal_connect_swapped (G_OBJECT (dialog->active),
      "notify::background-image", G_CALLBACK (panel_preferences_dialog_bg_image_notified), dialog);
  panel_preferences_dialog_bg_image_notified (dialog);

  /* manage panel length */
  panel_preferences_dialog_length_max_notified (dialog);
  g_signal_connect_object (dialog->active, "notify::length-max",
                           G_CALLBACK (panel_preferences_dialog_length_max_notified),
                           dialog, G_CONNECT_SWAPPED);
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "length");
  binding = g_object_bind_property_full (dialog->active, "length", object, "value",
                                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                                         panel_preferences_dialog_length_transform_to,
                                         panel_preferences_dialog_length_transform_from,
                                         dialog, NULL);
  dialog->bindings = g_slist_prepend (dialog->bindings, binding);

  /* get run mode of the driver (multiple monitors or randr) */
  display = gtk_widget_get_display (GTK_WIDGET (dialog->active));
  n_monitors = gdk_display_get_n_monitors (display);

  /* update the output selector */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "output-name");
  panel_return_if_fail (GTK_IS_COMBO_BOX (object));

  g_signal_handler_block (G_OBJECT (object), dialog->output_changed_handler_id);

  store = gtk_builder_get_object (GTK_BUILDER (dialog), "output-store");
  panel_return_if_fail (GTK_IS_LIST_STORE (store));
  gtk_list_store_clear (GTK_LIST_STORE (store));

  g_object_get (G_OBJECT (dialog->active), "output-name", &output_name, NULL);

  if (n_monitors > 1 || !panel_str_is_empty (output_name))
    {
      gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, n++,
                                         OUTPUT_NAME, "Automatic",
                                         OUTPUT_TITLE, _("Automatic"), -1);
      if (panel_str_is_empty (output_name) ||
          g_strcmp0 (output_name, "Automatic") == 0)
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (object), &iter);
          output_selected = TRUE;
          span_monitors_sensitive = TRUE;
        }
      gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, n++,
                                         OUTPUT_NAME, "Primary",
                                         OUTPUT_TITLE, _("Primary"), -1);
      if (g_strcmp0 (output_name, "Primary") == 0)
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (object), &iter);
          output_selected = TRUE;
          span_monitors_sensitive = FALSE;
        }

      if (n_monitors >= 1)
        {
          for (i = 0; i < n_monitors; i++)
            {
              monitor = gdk_display_get_monitor(display, i);
              name = g_strdup(gdk_monitor_get_model (monitor));
              if (panel_str_is_empty (name))
                {
                  g_free (name);

                  /* I18N: monitor name in the output selector */
                  title = g_strdup_printf (_("Monitor %d"), i + 1);
                  name = g_strdup_printf ("monitor-%d", i);
                }
              else
                {
                  /* use the randr name for the title */
                  title = g_strdup (name);
                }

              gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, n++,
                                                 OUTPUT_NAME, name,
                                                 OUTPUT_TITLE, title, -1);
              if (!output_selected
                  && g_strcmp0 (name, output_name) == 0)
                {
                  gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (object), &iter);
                  output_selected = TRUE;
                }

              g_free (name);
              g_free (title);
            }
        }

      /* add the output from the config if still nothing has been selected */
      if (!output_selected && !panel_str_is_empty (output_name))
        {
          gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, n++,
                                             OUTPUT_NAME, output_name,
                                             OUTPUT_TITLE, output_name, -1);
          gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (object), &iter);
        }
    }
  else
    {
      /* hide the selector */
      selector_visible = FALSE;
      span_monitors_sensitive = TRUE;
    }

  g_signal_handler_unblock (G_OBJECT (object), dialog->output_changed_handler_id);

  /* update visibility of the output selector */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "output-label");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_set (G_OBJECT (object), "visible", selector_visible, NULL);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "output-name");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_set (G_OBJECT (object), "visible", selector_visible, NULL);

  /* monitor spanning is only active when no output is selected */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "span-monitors");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  gtk_widget_set_sensitive (GTK_WIDGET (object), span_monitors_sensitive);
  g_object_set (G_OBJECT (object), "visible", n_monitors > 1, NULL);

  g_free (output_name);

  /* update sensitivity of "don't reserve space on borders" option */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "autohide-behavior");
  panel_return_if_fail (GTK_IS_COMBO_BOX (object));
  panel_preferences_dialog_autohide_changed (GTK_COMBO_BOX (object), dialog);

  /* update visibility of the "icon-size" widgets */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-spinbutton");
  panel_return_if_fail (G_IS_OBJECT (object));

  if (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (object)) == 0)
    icon_size_set = FALSE;
  else
    icon_size_set = TRUE;

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-revealer");
  panel_return_if_fail (G_IS_OBJECT (object));
  gtk_revealer_set_reveal_child (GTK_REVEALER (object), icon_size_set);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-switch");
  panel_return_if_fail (G_IS_OBJECT (object));
  gtk_switch_set_state (GTK_SWITCH (object), !icon_size_set);
}



static void
panel_preferences_dialog_output_changed (GtkComboBox            *combobox,
                                         PanelPreferencesDialog *dialog)
{
  GtkTreeIter   iter;
  GtkTreeModel *model;
  gchar        *output_name = NULL;
  GObject      *object;

  panel_return_if_fail (GTK_IS_COMBO_BOX (combobox));
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (PANEL_WINDOW (dialog->active));

  if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
      model = gtk_combo_box_get_model (combobox);
      gtk_tree_model_get (model, &iter, OUTPUT_NAME, &output_name, -1);
      g_object_set (G_OBJECT (dialog->active), "output-name", output_name, NULL);

      /* monitor spanning does not work when an output is selected */
      object = gtk_builder_get_object (GTK_BUILDER (dialog), "span-monitors");
      panel_return_if_fail (GTK_IS_WIDGET (object));
      if (output_name == NULL ||
          g_strcmp0 ("Automatic", output_name) == 0)
        gtk_widget_set_sensitive (GTK_WIDGET (object), TRUE);
      else
        gtk_widget_set_sensitive (GTK_WIDGET (object), FALSE);

      g_free (output_name);
    }
}



static void
panel_preferences_dialog_autohide_changed (GtkComboBox            *combobox,
                                           PanelPreferencesDialog *dialog)
{
  GObject *object;

  panel_return_if_fail (GTK_IS_COMBO_BOX (combobox));
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (PANEL_WINDOW (dialog->active));

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "enable-struts");
  panel_return_if_fail (GTK_IS_WIDGET (object));

  /* make "don't reserve space on borders" sensitive only when autohide is disabled */
  if (gtk_combo_box_get_active (combobox) == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (object), TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (object), FALSE);
}



static void
panel_preferences_dialog_bg_style_changed (PanelPreferencesDialog *dialog)
{
  gint      active;
  GObject  *object;
  gboolean  composited;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (PANEL_WINDOW (dialog->active));

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-style");
  panel_return_if_fail (GTK_IS_COMBO_BOX (object));
  active = gtk_combo_box_get_active (GTK_COMBO_BOX (object));

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-rgba");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_get (G_OBJECT (dialog->active), "composited", &composited, NULL);
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (object), composited);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-rgba-label");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_set (G_OBJECT (object), "visible", active == 1, NULL);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-rgba");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_set (G_OBJECT (object), "visible", active == 1, NULL);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-image-label");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_set (G_OBJECT (object), "visible", active == 2, NULL);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "background-image");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  g_object_set (G_OBJECT (object), "visible", active == 2, NULL);
}



static void
panel_preferences_dialog_bg_image_file_set (GtkFileChooserButton   *button,
                                            PanelPreferencesDialog *dialog)
{
  gchar *uri;

  panel_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (PANEL_IS_WINDOW (dialog->active));

  g_signal_handler_block (G_OBJECT (dialog->active),
      dialog->bg_image_notify_handler_id);

  uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
  g_object_set (G_OBJECT (dialog->active), "background-image", uri, NULL);
  g_free (uri);

  g_signal_handler_unblock (G_OBJECT (dialog->active),
      dialog->bg_image_notify_handler_id);
}



static void
panel_preferences_dialog_bg_image_notified (PanelPreferencesDialog *dialog)
{
  gchar   *uri;
  GObject *button;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (PANEL_IS_WINDOW (dialog->active));

  button = gtk_builder_get_object (GTK_BUILDER (dialog), "background-image");
  panel_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  g_signal_handlers_block_by_func (G_OBJECT (button),
      G_CALLBACK (panel_preferences_dialog_bg_image_file_set), dialog);

  g_object_get (G_OBJECT (dialog->active), "background-image", &uri, NULL);
  gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (button), uri != NULL ? uri : "");
  g_free (uri);

  g_signal_handlers_unblock_by_func (G_OBJECT (button),
      G_CALLBACK (panel_preferences_dialog_bg_image_file_set), dialog);
}



static void
panel_preferences_dialog_length_max_notified (PanelPreferencesDialog *dialog)
{
  GObject *adjust;
  gdouble  length;
  guint    length_max;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (PANEL_IS_WINDOW (dialog->active));

  adjust = gtk_builder_get_object (GTK_BUILDER (dialog), "length");
  panel_return_if_fail (GTK_IS_ADJUSTMENT (adjust));

  g_object_get (dialog->active, "length-max", &length_max, NULL);
  gtk_adjustment_set_upper (GTK_ADJUSTMENT (adjust), length_max);
  g_object_get (dialog->active, "length", &length, NULL);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adjust), length_max * length / 100);
}



static gboolean
panel_preferences_dialog_length_transform_to (GBinding     *binding,
                                              const GValue *from_value,
                                              GValue       *to_value,
                                              gpointer      user_data)
{
  GObject *adjust;
  gdouble  length, upper;

  panel_return_val_if_fail (PANEL_IS_PREFERENCES_DIALOG (user_data), FALSE);

  adjust = gtk_builder_get_object (user_data, "length");
  panel_return_val_if_fail (GTK_IS_ADJUSTMENT (adjust), FALSE);

  length = g_value_get_double (from_value);
  upper = gtk_adjustment_get_upper (GTK_ADJUSTMENT (adjust));
  g_value_set_double (to_value, rint (upper * length / 100));

  return TRUE;
}



static gboolean
panel_preferences_dialog_length_transform_from (GBinding     *binding,
                                                const GValue *from_value,
                                                GValue       *to_value,
                                                gpointer      user_data)
{
  GObject *adjust;
  gdouble  length, upper;

  panel_return_val_if_fail (PANEL_IS_PREFERENCES_DIALOG (user_data), FALSE);

  adjust = gtk_builder_get_object (user_data, "length");
  panel_return_val_if_fail (GTK_IS_ADJUSTMENT (adjust), FALSE);

  length = g_value_get_double (from_value);
  upper = gtk_adjustment_get_upper (GTK_ADJUSTMENT (adjust));
  g_value_set_double (to_value, 100 * length / upper);

  return TRUE;
}



static void
panel_preferences_dialog_panel_sensitive (PanelPreferencesDialog *dialog)
{

  GObject  *object;
  gboolean  locked = TRUE;
  GSList   *windows;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  if (G_LIKELY (dialog->active != NULL))
    locked = panel_window_get_locked (dialog->active);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-remove");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  windows = panel_application_get_windows (dialog->application);
  gtk_widget_set_sensitive (GTK_WIDGET (object),
      !locked && g_slist_length (windows) > 1);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-add");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  gtk_widget_set_sensitive (GTK_WIDGET (object),
      !panel_application_get_locked (dialog->application));

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "notebook");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  gtk_widget_set_sensitive (GTK_WIDGET (object), !locked);

  object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-add");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  gtk_widget_set_sensitive (GTK_WIDGET (object), !locked);
}



static void
panel_preferences_dialog_panel_combobox_changed (GtkComboBox            *combobox,
                                                 PanelPreferencesDialog *dialog)
{
  gint          panel_id;
  GtkWidget    *itembar;
  GtkTreeModel *model;
  GtkTreeIter   iter;

  panel_return_if_fail (GTK_IS_COMBO_BOX (combobox));
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* disconnect signal we used to monitor changes in the itembar */
  if (dialog->active != NULL && dialog->items_changed_handler_id != 0)
    {
      itembar = gtk_bin_get_child (GTK_BIN (dialog->active));
      g_signal_handler_disconnect (G_OBJECT (itembar), dialog->items_changed_handler_id);
    }

  /* remove all the active bindings */
  panel_preferences_dialog_bindings_unbind (dialog);

  /* set the selected window */
  if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
      model = gtk_combo_box_get_model (combobox);
      gtk_tree_model_get (model, &iter, 0, &panel_id, -1);

      dialog->active = panel_application_get_window (dialog->application, panel_id);
    }
  else
    {
      dialog->active = NULL;
    }

  panel_application_window_select (dialog->application, dialog->active);

  if (G_LIKELY (dialog->active != NULL))
    {
      itembar = gtk_bin_get_child (GTK_BIN (dialog->active));
      dialog->items_changed_handler_id =
          g_signal_connect (G_OBJECT (itembar), "changed",
                            G_CALLBACK (panel_preferences_dialog_item_store_rebuild),
                            dialog);

      /* rebind the dialog bindings */
      panel_preferences_dialog_bindings_update (dialog);

      /* update the items treeview */
      panel_preferences_dialog_item_store_rebuild (itembar, dialog);
    }

  panel_preferences_dialog_panel_sensitive (dialog);
}



static gboolean
panel_preferences_dialog_panel_combobox_rebuild (PanelPreferencesDialog *dialog,
                                                 gint                    panel_id)
{
  GObject     *store, *combo;
  gint         i;
  GSList      *windows, *li;
  gchar       *name;
  gint         id;
  GtkTreeIter  iter;
  gboolean     selected = FALSE;

  /* get the combo box and model */
  store = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-store");
  panel_return_val_if_fail (GTK_IS_LIST_STORE (store), FALSE);
  combo = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-combobox");
  panel_return_val_if_fail (GTK_IS_COMBO_BOX (combo), FALSE);

  /* block signal */
  g_signal_handlers_block_by_func (combo,
      panel_preferences_dialog_panel_combobox_changed, dialog);

  /* empty the combo box */
  gtk_list_store_clear (GTK_LIST_STORE (store));

  /* add new names */
  windows = panel_application_get_windows (dialog->application);
  for (li = windows, i = 0; li != NULL; li = li->next, i++)
    {
      /* I18N: panel combo box in the preferences dialog */
      id = panel_window_get_id (li->data);
      name = g_strdup_printf (_("Panel %d"), id);
      gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, i,
                                         0, id, 1, name, -1);
      g_free (name);

      if (id == panel_id)
        {
          /* select panel id */
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
          selected = TRUE;
        }
    }

  /* unblock signal */
  g_signal_handlers_unblock_by_func (combo,
      panel_preferences_dialog_panel_combobox_changed, dialog);

  if (selected)
    panel_preferences_dialog_panel_combobox_changed (GTK_COMBO_BOX (combo), dialog);

  panel_preferences_dialog_panel_sensitive (dialog);

  return selected;
}



static void
panel_preferences_dialog_panel_add (GtkWidget              *widget,
                                    PanelPreferencesDialog *dialog)
{
  PanelWindow *window;
  gint         panel_id;

  /* create new window */
  window = panel_application_new_window (dialog->application,
      gtk_widget_get_screen (widget), -1, TRUE);

  /* show window */
  gtk_widget_show (GTK_WIDGET (window));

  /* rebuild the selector */
  panel_id = panel_window_get_id (window);
  panel_preferences_dialog_panel_combobox_rebuild (dialog, panel_id);
}



static void
panel_preferences_dialog_panel_remove (GtkWidget              *widget,
                                       PanelPreferencesDialog *dialog)
{
  gint       id;
  GtkWidget *toplevel;
  GSList    *windows, *window;

  /* leave if the window is locked */
  if (panel_window_get_locked (dialog->active))
    return;

  toplevel = gtk_widget_get_toplevel (widget);
  if (xfce_dialog_confirm (GTK_WINDOW (toplevel), "list-remove", _("_Remove"),
          _("The panel and plugin configurations will be permanently removed"),
          _("Are you sure you want to remove panel %d?"),
          panel_window_get_id (dialog->active)))
    {
      /* release the bindings */
      panel_preferences_dialog_bindings_unbind (dialog);

      /* get new panel id to be selected */
      windows = panel_application_get_windows (dialog->application);
      window = g_slist_find (windows, dialog->active);
      if (window->next != NULL)
        id = panel_window_get_id (window->next->data);
      else
        id = panel_window_get_id (windows->data);

      /* remove the panel, plugins and configuration */
      panel_application_remove_window (dialog->application,
                                       dialog->active);
      dialog->active = NULL;

      /* rebuild the selector */
      panel_preferences_dialog_panel_combobox_rebuild (dialog, id);
    }
}



static gboolean
panel_preferences_dialog_icon_size_state_set (GtkSwitch              *widget,
                                              gboolean                state,
                                              PanelPreferencesDialog *dialog)
{
  GObject   *spinbutton, *revealer;

  panel_return_val_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog), FALSE);
  spinbutton = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-spinbutton");
  panel_return_val_if_fail (G_IS_OBJECT (spinbutton), FALSE);
  revealer = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-revealer");
  panel_return_val_if_fail (G_IS_OBJECT (revealer), FALSE);

  /* we set the icon-size to 0 for auto-handling */
  if (state)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinbutton), 0.0);
    }
  else
    {
      /* if the setting is initially enabled we start at the low default of 16px */
      if (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton)) == 0)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinbutton), 16.0);
    }

  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), !state);
  gtk_switch_set_state (GTK_SWITCH (widget), state);

  return TRUE;
}



static void
panel_preferences_dialog_icon_size_changed (GtkSpinButton          *button,
                                            PanelPreferencesDialog *dialog)
{
  GObject   *object;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "icon-size-switch");
  panel_return_if_fail (G_IS_OBJECT (object));

  /* 0 means the setting is disabled (icon-size-switch's state == true) so the lower value
     of the spinbutton has to be 0. however, we want to set 16px as the reasonable lower limit for icons. */
  if (gtk_spin_button_get_value_as_int (button) < 16
      && gtk_switch_get_state (GTK_SWITCH (object)))
      gtk_spin_button_set_value (button, 16.0);
}



static void
panel_preferences_dialog_compositing_clicked (GtkButton *button, gpointer user_data)
{
  gchar    *command;
  GAppInfo *app_info = NULL;
  GError   *error = NULL;

  command = g_find_program_in_path ("xfwm4-tweaks-settings");
  if (command != NULL)
    app_info = g_app_info_create_from_commandline (command, "Window Manager Tweaks", G_APP_INFO_CREATE_NONE, &error);
  g_free (command);

  if (error != NULL)
    {
      g_warning ("Could not find application %s", error->message);
      g_error_free (error);
      return;
    }

  if (app_info != NULL && !g_app_info_launch (app_info, NULL, NULL, &error))
    {
      g_warning ("Could not launch the application %s", error->message);
      g_error_free (error);
    }
}



static void
panel_preferences_dialog_panel_switch (GtkWidget *widget, PanelPreferencesDialog *dialog)
{
  GtkWidget *toplevel;
  gchar     *path_old;
  gchar     *path_new;

  path_old = g_find_program_in_path ("xfpanel-switch");
  path_new = g_find_program_in_path ("xfce4-panel-profiles");
  if (path_old == NULL && path_new == NULL)
    return;

  /* close the preferences dialog */
  toplevel = gtk_widget_get_toplevel (widget);
  panel_preferences_dialog_response (toplevel, 0, dialog);

  /* first try the new name of the executable, then the old */
  if (path_new)
    g_spawn_command_line_async (path_new, NULL);
  else if (path_old)
    g_spawn_command_line_async (path_old, NULL);

  g_free (path_old);
  g_free (path_new);
}



static XfcePanelPluginProvider *
panel_preferences_dialog_item_get_selected (PanelPreferencesDialog *dialog,
                                            GtkTreeIter            *return_iter)
{
  GObject                 *treeview;
  XfcePanelPluginProvider *provider = NULL;
  GtkTreeModel            *model;
  GtkTreeIter              iter;
  GtkTreeSelection        *selection;

  panel_return_val_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog), NULL);

  /* get the treeview selection */
  treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
  panel_return_val_if_fail (GTK_IS_WIDGET (treeview), NULL);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  /* get the selection item */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* get the selected provider */
      gtk_tree_model_get (model, &iter, ITEM_COLUMN_PROVIDER, &provider, -1);
      panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider), NULL);

      if (return_iter)
        *return_iter = iter;
    }

  return provider;
}



static void
panel_preferences_dialog_item_store_rebuild (GtkWidget              *itembar,
                                             PanelPreferencesDialog *dialog)
{
  GList                   *items, *li;
  guint                    i;
  PanelModule             *module;
  gchar                   *tooltip, *display_name;
  XfcePanelPluginProvider *selected_provider;
  GtkTreeIter              iter;
  GtkTreePath             *path;
  GObject                 *treeview;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (GTK_IS_LIST_STORE (dialog->store));
  panel_return_if_fail (PANEL_IS_ITEMBAR (itembar));

  /* memorize selected item */
  selected_provider = panel_preferences_dialog_item_get_selected (dialog, NULL);

  gtk_list_store_clear (dialog->store);

  g_signal_handlers_block_by_func (G_OBJECT (dialog->store),
      G_CALLBACK (panel_preferences_dialog_item_row_changed), dialog);

  /* add items to the store */
  items = gtk_container_get_children (GTK_CONTAINER (itembar));
  for (li = items, i = 0; li != NULL; li = li->next, i++)
    {
      /* get the panel module from the plugin */
      module = panel_module_get_from_plugin_provider (li->data);

      if (PANEL_IS_PLUGIN_EXTERNAL (li->data))
        {
          /* I18N: append (external) in the preferences dialog if the plugin
           * runs external */
          display_name = g_strdup_printf (_("%s <span color=\"grey\" size=\"small\">(external)</span>"),
                                          panel_module_get_display_name (module));

          /* I18N: tooltip in preferences dialog when hovering an item in the list
           * for external plugins */
          tooltip = g_strdup_printf (_("Internal name: %s-%d\n"
                                       "PID: %d"),
                                     xfce_panel_plugin_provider_get_name (li->data),
                                     xfce_panel_plugin_provider_get_unique_id (li->data),
                                     panel_plugin_external_get_pid (PANEL_PLUGIN_EXTERNAL (li->data)));
        }
      else
        {
          display_name = g_strdup (panel_module_get_display_name (module));

          /* I18N: tooltip in preferences dialog when hovering an item in the list
           * for internal plugins */
          tooltip = g_strdup_printf (_("Internal name: %s-%d"),
                                     xfce_panel_plugin_provider_get_name (li->data),
                                     xfce_panel_plugin_provider_get_unique_id (li->data));
        }

      gtk_list_store_insert_with_values (dialog->store, &iter, i,
                                         ITEM_COLUMN_ICON_NAME,
                                         panel_module_get_icon_name (module),
                                         ITEM_COLUMN_DISPLAY_NAME,
                                         display_name,
                                         ITEM_COLUMN_TOOLTIP,
                                         tooltip,
                                         ITEM_COLUMN_PROVIDER, li->data, -1);

      /* reconstruct selection */
      if (selected_provider == li->data)
        {
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->store), &iter);
          treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
          if (GTK_IS_WIDGET (treeview))
            gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
          gtk_tree_path_free (path);
        }

      g_free (tooltip);
      g_free (display_name);
    }

  g_list_free (items);

  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->store),
      G_CALLBACK (panel_preferences_dialog_item_row_changed), dialog);
}



static void
panel_preferences_dialog_item_move (GtkWidget              *button,
                                    PanelPreferencesDialog *dialog)
{
  GObject                 *treeview, *object;
  GtkTreeSelection        *selection;
  GtkTreeIter              iter_a, iter_b;
  XfcePanelPluginProvider *provider;
  GtkWidget               *itembar;
  gint                     position;
  gint                     direction;
  GtkTreePath             *path;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* direction */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-up");
  panel_return_if_fail (GTK_IS_WIDGET (object));
  direction = G_OBJECT (button) == object ? -1 : 1;

  provider = panel_preferences_dialog_item_get_selected (dialog, &iter_a);
  if (G_LIKELY (provider != NULL))
    {
      /* get the provider position on the panel */
      itembar = gtk_bin_get_child (GTK_BIN (dialog->active));
      position = panel_itembar_get_child_index (PANEL_ITEMBAR (itembar),
                                                GTK_WIDGET (provider));
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->store), &iter_a);

      if (G_LIKELY (position != -1))
        {
          /* block the changed signal */
          g_signal_handler_block (G_OBJECT (itembar), dialog->items_changed_handler_id);

          /* move the item on the panel */
          panel_itembar_reorder_child (PANEL_ITEMBAR (itembar),
                                       GTK_WIDGET (provider),
                                       position + direction);

          /* save the new ids */
          panel_application_save_window (dialog->application,
                                         dialog->active,
                                         SAVE_PLUGIN_IDS);

          /* unblock the changed signal */
          g_signal_handler_unblock (G_OBJECT (itembar), dialog->items_changed_handler_id);

          /* move the item up or down in the list */
          if (direction == 1)
            {
              /* swap the items in the list */
              iter_b = iter_a;
              if (gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->store), &iter_b))
                {
                  gtk_list_store_swap (dialog->store, &iter_a, &iter_b);
                  gtk_tree_path_next (path);
                }
            }
          else
            {
              /* get the previous item in the list */
              if (gtk_tree_path_prev (path))
                {
                  /* swap the items in the list */
                  gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->store), &iter_b, path);
                  gtk_list_store_swap (dialog->store, &iter_a, &iter_b);
                }
            }

          /* fake update the selection */
          treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
          panel_return_if_fail (GTK_IS_WIDGET (treeview));
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
          panel_preferences_dialog_item_selection_changed (selection, dialog);

          /* make the new selected position visible if moved out of area */
          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path, NULL, FALSE, 0, 0);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);

        }
      gtk_tree_path_free (path);
    }
}



static void
panel_preferences_dialog_item_remove (GtkWidget              *button,
                                      PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;
  GtkWidget               *widget, *toplevel;
  PanelModule             *module;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);
  if (G_LIKELY (provider != NULL))
    {
      module = panel_module_get_from_plugin_provider (provider);

      /* create question dialog (same code is also in xfce-panel-plugin.c) */
      toplevel = gtk_widget_get_toplevel (button);
      widget = gtk_message_dialog_new (GTK_WINDOW (toplevel), GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                       _("Are you sure that you want to remove \"%s\"?"),
                                       panel_module_get_display_name (module));
      gtk_window_set_title (GTK_WINDOW (widget),
                            _("Remove Item"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (widget),
                                                _("If you remove the item from the panel, "
                                                  "it is permanently lost."));
      gtk_dialog_add_buttons (GTK_DIALOG (widget), _("_Cancel"), GTK_RESPONSE_NO,
                              _("_Remove"), GTK_RESPONSE_YES, NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (widget), GTK_RESPONSE_NO);

      /* run the dialog */
      if (gtk_dialog_run (GTK_DIALOG (widget)) == GTK_RESPONSE_YES)
        {
          gtk_widget_hide (widget);
          xfce_panel_plugin_provider_emit_signal (provider, PROVIDER_SIGNAL_REMOVE_PLUGIN);
        }

      gtk_widget_destroy (widget);
    }
}



static void
panel_preferences_dialog_item_add (GtkWidget              *button,
                                   PanelPreferencesDialog *dialog)
{
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  panel_item_dialog_show (dialog->active);
}



static void
panel_preferences_dialog_item_properties (GtkWidget              *button,
                                          PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);
  if (G_LIKELY (provider != NULL))
    xfce_panel_plugin_provider_show_configure (provider);
}



static void
panel_preferences_dialog_item_about (GtkWidget              *button,
                                     PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);
  if (G_LIKELY (provider != NULL))
    xfce_panel_plugin_provider_show_about (provider);
}



static gboolean
panel_preferences_dialog_treeview_clicked (GtkTreeView            *treeview,
                                           GdkEventButton         *event,
                                           PanelPreferencesDialog *dialog)
{
  gint x, y;

  panel_return_val_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog), FALSE);
  panel_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

  gtk_tree_view_convert_widget_to_bin_window_coords (treeview,
                                                     event->x, event->y,
                                                     &x, &y);

  /* open preferences on double-click on a row */
  if (event->type == GDK_2BUTTON_PRESS
      && event->button == 1
      && gtk_tree_view_get_path_at_pos (treeview, x, y, NULL, NULL, NULL, NULL))
    {
      panel_preferences_dialog_item_properties (NULL, dialog);
      return TRUE;
    }

  return FALSE;
}



static void
panel_preferences_dialog_item_row_changed (GtkTreeModel           *model,
                                           GtkTreePath            *path,
                                           GtkTreeIter            *iter,
                                           PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider = NULL;
  gint                     position;
  GtkWidget               *itembar;
  gint                     store_position;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (GTK_TREE_MODEL (dialog->store) == model);
  panel_return_if_fail (PANEL_IS_WINDOW (dialog->active));

  /* get the changed row */
  gtk_tree_model_get (model, iter, ITEM_COLUMN_PROVIDER, &provider, -1);
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));
  store_position = gtk_tree_path_get_indices (path)[0];

  /* actual position on the panel */
  itembar = gtk_bin_get_child (GTK_BIN (dialog->active));
  position = panel_itembar_get_child_index (PANEL_ITEMBAR (itembar),
                                            GTK_WIDGET (provider));

  /* correct position in the list */
  if (position < store_position)
    store_position--;

  /* move the item on the panel */
  if (position != store_position)
    {
      panel_itembar_reorder_child (PANEL_ITEMBAR (itembar),
                                   GTK_WIDGET (provider),
                                   store_position);

      panel_application_save_window (dialog->application,
                                     dialog->active,
                                     SAVE_PLUGIN_IDS);
    }
}



static void
panel_preferences_dialog_item_selection_changed (GtkTreeSelection       *selection,
                                                 PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;
  GtkWidget               *itembar;
  gint                     position;
  gint                     items;
  gboolean                 active;
  GObject                 *object;
  guint                    i;
  const gchar             *button_names[] = { "item-remove", "item-up",
                                              "item-down", "item-about",
                                              "item-properties" };

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);
  if (G_LIKELY (provider != NULL))
    {
      /* leave if the selection change is irrelevant */
      itembar = gtk_widget_get_parent (GTK_WIDGET (provider));
      if (itembar != gtk_bin_get_child (GTK_BIN (dialog->active)))
        return;

      /* get the current position and the items on the bar */
      position = panel_itembar_get_child_index (PANEL_ITEMBAR (itembar), GTK_WIDGET (provider));
      items = panel_itembar_get_n_children (PANEL_ITEMBAR (itembar)) - 1;

      /* update sensitivity of buttons */
      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-up");
      panel_return_if_fail (GTK_IS_WIDGET (object));
      gtk_widget_set_sensitive (GTK_WIDGET (object), !!(position > 0 && position <= items));

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-down");
      panel_return_if_fail (GTK_IS_WIDGET (object));
      gtk_widget_set_sensitive (GTK_WIDGET (object), !!(position >= 0 && position < items));

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-remove");
      panel_return_if_fail (GTK_IS_WIDGET (object));
      gtk_widget_set_sensitive (GTK_WIDGET (object), TRUE);

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-properties");
      panel_return_if_fail (GTK_IS_WIDGET (object));
      active = xfce_panel_plugin_provider_get_show_configure (provider);
      gtk_widget_set_sensitive (GTK_WIDGET (object), active);

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-about");
      panel_return_if_fail (GTK_IS_WIDGET (object));
      active = xfce_panel_plugin_provider_get_show_about (provider);
      gtk_widget_set_sensitive (GTK_WIDGET (object), active);
    }
  else
    {
      /* make all items insensitive, except for the add button */
      for (i = 0; i < G_N_ELEMENTS (button_names); i++)
        {
          object = gtk_builder_get_object (GTK_BUILDER (dialog), button_names[i]);
          panel_return_if_fail (GTK_IS_WIDGET (object));
          gtk_widget_set_sensitive (GTK_WIDGET (object), FALSE);
        }
    }
}



static void
panel_preferences_dialog_plug_deleted (GtkWidget *plug)
{
  g_signal_handlers_disconnect_by_func (G_OBJECT (plug),
      G_CALLBACK (panel_preferences_dialog_plug_deleted), NULL);

  g_object_unref (G_OBJECT (dialog_singleton));
}



static void
panel_preferences_dialog_show_internal (PanelWindow *active,
                                        Window       socket_window)
{
  gint         panel_id = 0;
  GObject     *window, *combo;
  GdkScreen   *screen;
  GSList      *windows;
  GtkWidget   *plug;
  GObject     *plug_child;
  GtkWidget   *content_area;

  panel_return_if_fail (active == NULL || PANEL_IS_WINDOW (active));

  /* check if not the entire application is locked */
  if (panel_dialogs_kiosk_warning ())
    return;

  if (dialog_singleton == NULL)
    {
      /* create new dialog singleton */
      dialog_singleton = g_object_new (PANEL_TYPE_PREFERENCES_DIALOG, NULL);
      g_object_add_weak_pointer (G_OBJECT (dialog_singleton), (gpointer) &dialog_singleton);
    }

  if (active == NULL)
    {
      /* select first window */
      windows = panel_application_get_windows (dialog_singleton->application);
      if (windows != NULL)
        active = g_slist_nth_data (windows, 0);
    }

  /* select the active window in the dialog */
  combo = gtk_builder_get_object (GTK_BUILDER (dialog_singleton), "panel-combobox");
  panel_return_if_fail (GTK_IS_WIDGET (combo));
  panel_id = panel_window_get_id (active);
  if (!panel_preferences_dialog_panel_combobox_rebuild (dialog_singleton, panel_id))
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  window = gtk_builder_get_object (GTK_BUILDER (dialog_singleton), "dialog");
  panel_return_if_fail (GTK_IS_WIDGET (window));
  plug_child = gtk_builder_get_object (GTK_BUILDER (dialog_singleton), "plug-child");
  panel_return_if_fail (GTK_IS_WIDGET (plug_child));

  /* check if we need to remove the window from the plug */
  if (dialog_singleton->socket_plug != NULL)
    {
      panel_return_if_fail (GTK_IS_PLUG (dialog_singleton->socket_plug));

      /* move the vbox to the dialog */
      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      xfce_widget_reparent (GTK_WIDGET (plug_child), content_area);
      gtk_widget_show (GTK_WIDGET (plug_child));

      /* destroy the plug */
      plug = dialog_singleton->socket_plug;
      dialog_singleton->socket_plug = NULL;

      g_signal_handlers_disconnect_by_func (G_OBJECT (plug),
          G_CALLBACK (panel_preferences_dialog_plug_deleted), NULL);
      gtk_widget_destroy (plug);
    }

  if (socket_window == 0)
    {
      /* show the dialog on the same screen as the panel */
      if (G_LIKELY (active != NULL))
        screen = gtk_widget_get_screen (GTK_WIDGET (active));
      else
        screen = gdk_screen_get_default ();
      gtk_window_set_screen (GTK_WINDOW (window), screen);

      gtk_window_present (GTK_WINDOW (window));
      panel_application_take_dialog (dialog_singleton->application, GTK_WINDOW (window));
    }
  else
    {
      /* hide window */
      gtk_widget_hide (GTK_WIDGET (window));

      /* create a new plug */
      plug = gtk_plug_new (socket_window);
      g_signal_connect (G_OBJECT (plug), "delete-event",
          G_CALLBACK (panel_preferences_dialog_plug_deleted), NULL);
      dialog_singleton->socket_plug = plug;
      gtk_widget_show (plug);

      /* move the vbox in the plug */
      xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
      gtk_widget_show (GTK_WIDGET (plug_child));
    }
}



void
panel_preferences_dialog_show (PanelWindow *active)
{
  panel_return_if_fail (active == NULL || PANEL_IS_WINDOW (active));
  panel_preferences_dialog_show_internal (active, 0);
}



void
panel_preferences_dialog_show_from_id (gint         panel_id,
                                       Window       socket_window)
{
  PanelApplication *application;
  PanelWindow      *window;

  application = panel_application_get ();
  window = panel_application_get_window (application, panel_id);
  panel_preferences_dialog_show_internal (window, socket_window);
  g_object_unref (G_OBJECT (application));
}



gboolean
panel_preferences_dialog_visible (void)
{
  return !!(dialog_singleton != NULL);
}
