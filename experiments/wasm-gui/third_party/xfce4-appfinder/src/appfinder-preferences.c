/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include <src/appfinder-preferences.h>
#include <src/appfinder-preferences-ui.h>
#include <src/appfinder-model.h>
#include <src/appfinder-private.h>
#include <src/appfinder-actions.h>



static void xfce_appfinder_preferences_response                (GtkWidget                *window,
                                                                gint                      response_id,
                                                                XfceAppfinderPreferences *preferences);
static void xfce_appfinder_preferences_beside_sensitive        (GtkWidget                *show_icons,
                                                                GtkWidget                *text_beside_icon);
static void xfce_appfinder_preferences_single_window_sensitive (GtkWidget                *keep_running,
                                                                GtkWidget                *single_window);
static void xfce_appfinder_preferences_hide_category_sensitive (GtkWidget                *hide_category,
                                                                GtkWidget                *category_icon_size);
static void xfce_appfinder_preferences_clear_history           (XfceAppfinderPreferences *preferences);
static void xfce_appfinder_preferences_action_add              (XfceAppfinderPreferences *preferences);
static void xfce_appfinder_preferences_action_remove           (GtkWidget                *button,
                                                                XfceAppfinderPreferences *preferences);
static void xfce_appfinder_preferences_action_changed          (XfconfChannel            *channel,
                                                                const gchar              *prop_name,
                                                                const GValue             *value,
                                                                XfceAppfinderPreferences *preferences);
static void xfce_appfinder_preferences_action_populate         (XfceAppfinderPreferences *preferences);
static void xfce_appfinder_preferences_selection_changed       (GtkTreeSelection         *selection,
                                                                XfceAppfinderPreferences *preferences);



struct _XfceAppfinderPreferencesClass
{
  GtkBuilderClass __parent__;
};

struct _XfceAppfinderPreferences
{
  GtkBuilder __parent__;

  GObject          *dialog;

  XfconfChannel    *channel;

  GtkTreeSelection *selection;

  gulong            bindings[4];
  gulong            property_watch_id;
};

enum
{
  COLUMN_PATTERN,
  COLUMN_UNIQUE_ID
};



G_DEFINE_TYPE (XfceAppfinderPreferences, xfce_appfinder_preferences, GTK_TYPE_BUILDER)



static void
xfce_appfinder_preferences_class_init (XfceAppfinderPreferencesClass *klass)
{
}



static void
xfce_appfinder_preferences_init (XfceAppfinderPreferences *preferences)
{
  GObject     *object, *previous;
  GtkTreePath *path;

  preferences->channel = xfconf_channel_get ("xfce4-appfinder");

  /* load the builder data into the object */
  gtk_builder_add_from_string (GTK_BUILDER (preferences), appfinder_preferences_ui,
                               appfinder_preferences_ui_length, NULL);

  preferences->dialog = gtk_builder_get_object (GTK_BUILDER (preferences), "dialog");
  appfinder_return_if_fail (XFCE_IS_TITLED_DIALOG (preferences->dialog));
  g_signal_connect (G_OBJECT (preferences->dialog), "response",
      G_CALLBACK (xfce_appfinder_preferences_response), preferences);

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "remember-category");
  xfconf_g_property_bind (preferences->channel, "/remember-category", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "always-center");
  xfconf_g_property_bind (preferences->channel, "/always-center", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  previous = gtk_builder_get_object (GTK_BUILDER (preferences), "enable-service");
  xfconf_g_property_bind (preferences->channel, "/enable-service", G_TYPE_BOOLEAN,
                          G_OBJECT (previous), "active");

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "single-window");
  xfconf_g_property_bind (preferences->channel, "/single-window", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  g_signal_connect (G_OBJECT (previous), "toggled",
      G_CALLBACK (xfce_appfinder_preferences_single_window_sensitive), object);
  xfce_appfinder_preferences_single_window_sensitive (GTK_WIDGET (previous), GTK_WIDGET (object));

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "sort-by-frecency");
  xfconf_g_property_bind (preferences->channel, "/sort-by-frecency", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  previous = gtk_builder_get_object (GTK_BUILDER (preferences), "icon-view");
  xfconf_g_property_bind (preferences->channel, "/icon-view", G_TYPE_BOOLEAN,
                          G_OBJECT (previous), "active");

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "text-beside-icons");
  xfconf_g_property_bind (preferences->channel, "/text-beside-icons", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  g_signal_connect (G_OBJECT (previous), "toggled",
      G_CALLBACK (xfce_appfinder_preferences_beside_sensitive), object);
  xfce_appfinder_preferences_beside_sensitive (GTK_WIDGET (previous), GTK_WIDGET (object));

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "item-icon-size");
  gtk_combo_box_set_active (GTK_COMBO_BOX (object), XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM);
  xfconf_g_property_bind (preferences->channel, "/item-icon-size", G_TYPE_UINT,
                          G_OBJECT (object), "active");

  previous = gtk_builder_get_object (GTK_BUILDER (preferences), "category-icon-size");
  gtk_combo_box_set_active (GTK_COMBO_BOX (previous), XFCE_APPFINDER_ICON_SIZE_DEFAULT_CATEGORY);
  xfconf_g_property_bind (preferences->channel, "/category-icon-size", G_TYPE_UINT,
                          G_OBJECT (previous), "active");

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "hide-category-pane");
  xfconf_g_property_bind (preferences->channel, "/hide-category-pane", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  g_signal_connect (G_OBJECT (object), "toggled",
      G_CALLBACK (xfce_appfinder_preferences_hide_category_sensitive), previous);
  xfce_appfinder_preferences_hide_category_sensitive (GTK_WIDGET (object), GTK_WIDGET (previous));

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "hide-window-decorations");
  xfconf_g_property_bind (preferences->channel, "/hide-window-decorations", G_TYPE_BOOLEAN,
                          G_OBJECT (object), "active");

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "button-clear");
  g_signal_connect_swapped (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_appfinder_preferences_clear_history), preferences);

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "button-add");
  g_signal_connect_swapped (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_appfinder_preferences_action_add), preferences);

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "button-remove");
  g_signal_connect (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_appfinder_preferences_action_remove), preferences);

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "actions-treeview");
  preferences->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));

  xfce_appfinder_preferences_action_populate (preferences);

  gtk_tree_selection_set_mode (preferences->selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (preferences->selection), "changed",
      G_CALLBACK (xfce_appfinder_preferences_selection_changed), preferences);

  path = gtk_tree_path_new_first ();
  gtk_tree_selection_select_path (preferences->selection, path);
  gtk_tree_path_free (path);

  preferences->property_watch_id =
    g_signal_connect (G_OBJECT (preferences->channel), "property-changed",
        G_CALLBACK (xfce_appfinder_preferences_action_changed), preferences);
}



static void
xfce_appfinder_preferences_response (GtkWidget                *window,
                                     gint                      response_id,
                                     XfceAppfinderPreferences *preferences)
{
  appfinder_return_if_fail (GTK_IS_DIALOG (window));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_PREFERENCES (preferences));

  if (response_id == GTK_RESPONSE_HELP)
    {
      xfce_dialog_show_help (GTK_WINDOW (window), "xfce4-appfinder", "preferences", NULL);
    }
  else
    {
      g_signal_handler_disconnect (preferences->channel, preferences->property_watch_id);
      g_object_unref (G_OBJECT (preferences));
      gtk_widget_destroy (window);
    }
}



static void
xfce_appfinder_preferences_beside_sensitive (GtkWidget *show_icons,
                                             GtkWidget *text_beside_icon)
{
  gtk_widget_set_sensitive (text_beside_icon,
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (show_icons)));
}



static void
xfce_appfinder_preferences_single_window_sensitive (GtkWidget *keep_running,
                                                    GtkWidget *single_window)
{
  gtk_widget_set_sensitive (single_window,
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (keep_running)));
}



static void
xfce_appfinder_preferences_hide_category_sensitive (GtkWidget *hide_category,
                                                    GtkWidget *category_icon_size)
{
  gboolean hide = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_category));
  gtk_widget_set_sensitive (category_icon_size, !hide);
}



static void
xfce_appfinder_preferences_clear_history (XfceAppfinderPreferences *preferences)
{
  XfceAppfinderModel *model;
  gboolean            sort_by_frecency;
  gint                scale_factor;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_PREFERENCES (preferences));

  if (xfce_dialog_confirm (GTK_WINDOW (preferences->dialog), XFCE_APPFINDER_ICON_NAME_CLEAR, _("C_lear"),
                           _("This will permanently clear the custom command history."),
                           _("Are you sure you want to clear the command history?")))
    {
      sort_by_frecency = xfconf_channel_get_bool (preferences->channel, "/sort-by-frecency", FALSE);
      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (preferences->dialog));
      model = xfce_appfinder_model_get (sort_by_frecency, scale_factor);
      xfce_appfinder_model_history_clear (model);
      g_object_unref (G_OBJECT (model));
    }
}



static gboolean
xfce_appfinder_preferences_action_save_foreach (GtkTreeModel *model,
                                                GtkTreePath  *path,
                                                GtkTreeIter  *iter,
                                                gpointer      data)
{
  GPtrArray *array = data;
  gint       id;
  GValue    *value;

  gtk_tree_model_get (model, iter, COLUMN_UNIQUE_ID, &id, -1);

  value = g_new0 (GValue, 1);
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, id);
  g_ptr_array_add (array, value);

  return FALSE;
}



static void
xfce_appfinder_preferences_action_save (XfceAppfinderPreferences *preferences,
                                        GtkTreeModel             *model)
{
  GPtrArray *array;

  appfinder_return_if_fail (GTK_IS_TREE_MODEL (model));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_PREFERENCES (preferences));

  g_signal_handler_block (preferences->channel, preferences->property_watch_id);

  array = g_ptr_array_new ();
  gtk_tree_model_foreach (model, xfce_appfinder_preferences_action_save_foreach, array);
  xfconf_channel_set_arrayv (preferences->channel, "/actions", array);
  xfconf_array_free (array);

  g_signal_handler_unblock (preferences->channel, preferences->property_watch_id);
}



static void
xfce_appfinder_preferences_action_add (XfceAppfinderPreferences *preferences)
{
  XfceAppfinderActions *actions;
  gint                  id;
  GObject              *store;
  GtkTreeIter           iter;
  gchar                 prop[32];

  appfinder_return_if_fail (XFCE_IS_APPFINDER_PREFERENCES (preferences));

  /* get an unused id */
  actions = xfce_appfinder_actions_get ();
  id = xfce_appfinder_actions_get_unique_id (actions);
  g_object_unref (G_OBJECT (actions));

  /* make sure property does not exist */
  g_snprintf (prop, sizeof (prop), "/actions/action-%d", id);
  xfconf_channel_reset_property (preferences->channel, prop, TRUE);

  /* add new item to store */
  store = gtk_builder_get_object (GTK_BUILDER (preferences), "actions-store");
  gtk_list_store_append (GTK_LIST_STORE (store), &iter);
  gtk_list_store_set (GTK_LIST_STORE (store), &iter, COLUMN_UNIQUE_ID, id, -1);

  /* select item */
  gtk_tree_selection_select_iter (preferences->selection, &iter);

  /* save new id */
  xfce_appfinder_preferences_action_save (preferences, GTK_TREE_MODEL (store));
}



static void
xfce_appfinder_preferences_action_remove (GtkWidget                *button,
                                          XfceAppfinderPreferences *preferences)
{
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *pattern;
  gint              id;
  gchar             prop[32];

  appfinder_return_if_fail (GTK_IS_WIDGET (button));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_PREFERENCES (preferences));

  if (!gtk_tree_selection_get_selected (preferences->selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      COLUMN_PATTERN, &pattern,
                      COLUMN_UNIQUE_ID, &id,
                      -1);

  if (xfce_dialog_confirm (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                           XFCE_APPFINDER_ICON_NAME_DELETE, _("_Delete"),
                           _("The custom action will be deleted permanently."),
                           _("Are you sure you want to delete pattern \"%s\"?"),
                           pattern))
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      /* remove data from channel */
      g_snprintf (prop, sizeof (prop), "/actions/action-%d", id);
      xfconf_channel_reset_property (preferences->channel, prop, TRUE);

      /* save */
      xfce_appfinder_preferences_action_save (preferences, model);
    }

  g_free (pattern);
}



typedef struct
{
  gint          unique_id;
  const GValue *value;
}
UpdateContext;



static gboolean
xfce_appfinder_preferences_action_changed_func (GtkTreeModel *model,
                                                GtkTreePath  *path,
                                                GtkTreeIter  *iter,
                                                gpointer      data)
{
  gint           unique_id;
  UpdateContext *context = data;

  gtk_tree_model_get (model, iter, COLUMN_UNIQUE_ID, &unique_id, -1);

  if (context->unique_id == unique_id)
    {
      gtk_list_store_set (GTK_LIST_STORE (model), iter, COLUMN_PATTERN,
                          g_value_get_string (context->value),
                          -1);

      return TRUE;
    }

  return FALSE;
}



static void
xfce_appfinder_preferences_action_changed (XfconfChannel            *channel,
                                           const gchar              *prop_name,
                                           const GValue             *value,
                                           XfceAppfinderPreferences *preferences)
{
  gint           unique_id;
  GObject       *store;
  UpdateContext  context;
  gint           offset = 0;

  if (prop_name == NULL)
    return;

  if (strcmp (prop_name, "/actions") == 0)
    {
      xfce_appfinder_preferences_action_populate (preferences);
    }
  else if (G_VALUE_HOLDS_STRING (value)
           && sscanf (prop_name, "/actions/action-%d%n", &unique_id, &offset) == 1
           && g_strcmp0 (prop_name + offset, "/pattern") == 0)
    {
      context.unique_id = unique_id;
      context.value = value;

      store = gtk_builder_get_object (GTK_BUILDER (preferences), "actions-store");
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
          xfce_appfinder_preferences_action_changed_func, &context);
    }
}



static void
xfce_appfinder_preferences_action_populate (XfceAppfinderPreferences *preferences)
{
  GPtrArray    *array;
  GtkTreeModel *store;
  const GValue *value;
  gint          unique_id;
  gchar         prop[32];
  gchar        *pattern;
  guint         i;
  gint          restore_id = -1;
  GtkTreeIter   iter;

  APPFINDER_DEBUG ("populate tree model");

  if (gtk_tree_selection_get_selected (preferences->selection, &store, &iter))
    gtk_tree_model_get (store, &iter, COLUMN_UNIQUE_ID, &restore_id, -1);

  gtk_list_store_clear (GTK_LIST_STORE (store));

  array = xfconf_channel_get_arrayv (preferences->channel, "/actions");
  if (G_LIKELY (array != NULL))
    {
      for (i = 0; i < array->len; i++)
        {
          value = g_ptr_array_index (array, i);
          appfinder_assert (value != NULL);
          unique_id = g_value_get_int (value);

          g_snprintf (prop, sizeof (prop), "/actions/action-%d/pattern", unique_id);
          pattern = xfconf_channel_get_string (preferences->channel, prop, NULL);

          gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, i,
                                             COLUMN_UNIQUE_ID, unique_id,
                                             COLUMN_PATTERN, pattern,
                                             -1);

          if (restore_id == unique_id)
            gtk_tree_selection_select_iter (preferences->selection, &iter);

          g_free (pattern);
        }

      xfconf_array_free (array);
    }
}



typedef struct
{
  const gchar *name;
  const gchar *prop_name;
  GType        prop_type;
}
dialog_object;



static void
xfce_appfinder_preferences_selection_changed (GtkTreeSelection         *selection,
                                              XfceAppfinderPreferences *preferences)
{
  GtkTreeModel  *store;
  GtkTreeIter    iter;
  gint           unique_id;
  GObject       *object;
  guint          i;
  gchar          prop[32];
  dialog_object  objects[] =
  {
     { "type", "active", G_TYPE_INT },
     { "pattern", "text", G_TYPE_STRING },
     { "command", "text", G_TYPE_STRING },
     { "save", "active", G_TYPE_BOOLEAN }
  };

  appfinder_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_PREFERENCES (preferences));
  appfinder_return_if_fail (G_N_ELEMENTS (preferences->bindings) == G_N_ELEMENTS (objects));

  /* drop old bindings */
  for (i = 0; i < G_N_ELEMENTS (preferences->bindings); i++)
    {
      if (preferences->bindings[i] != 0)
        {
          xfconf_g_property_unbind (preferences->bindings[i]);
          preferences->bindings[i] = 0;
        }
    }

  if (gtk_tree_selection_get_selected (selection, &store, &iter))
    gtk_tree_model_get (store, &iter, COLUMN_UNIQUE_ID, &unique_id, -1);
  else
    unique_id = -1;

  for (i = 0; i < G_N_ELEMENTS (objects); i++)
    {
      object = gtk_builder_get_object (GTK_BUILDER (preferences), objects[i].name);
      appfinder_return_if_fail (GTK_IS_WIDGET (object));
      gtk_widget_set_sensitive (GTK_WIDGET (object), unique_id != -1);

      /* clear contents */
      if (GTK_IS_ENTRY (object))
        gtk_entry_set_text (GTK_ENTRY (object), "");
      else if (GTK_IS_COMBO_BOX (object))
        gtk_combo_box_set_active (GTK_COMBO_BOX (object), 0);
      else if (GTK_IS_TOGGLE_BUTTON (object))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), FALSE);
      else
        appfinder_assert_not_reached ();

      if (unique_id > -1)
        {
          g_snprintf (prop, sizeof (prop), "/actions/action-%d/%s", unique_id, objects[i].name);
          preferences->bindings[i] = xfconf_g_property_bind (preferences->channel, prop,
                                                             objects[i].prop_type, object,
                                                             objects[i].prop_name);
        }
    }

  object = gtk_builder_get_object (GTK_BUILDER (preferences), "button-remove");
  gtk_widget_set_sensitive (GTK_WIDGET (object), unique_id != -1);
}



void
xfce_appfinder_preferences_show (GdkScreen *screen)
{
  static XfceAppfinderPreferences *preferences = NULL;

  appfinder_return_if_fail (GDK_IS_SCREEN (screen));

  if (preferences == NULL)
    {
      preferences = g_object_new (XFCE_TYPE_APPFINDER_PREFERENCES, NULL);
      g_object_add_weak_pointer (G_OBJECT (preferences), (gpointer *) &preferences);
      gtk_widget_show (GTK_WIDGET (preferences->dialog));
    }
  else
    {
      gtk_window_present (GTK_WINDOW (preferences->dialog));
    }

  gtk_window_set_screen (GTK_WINDOW (preferences->dialog), screen);
}
