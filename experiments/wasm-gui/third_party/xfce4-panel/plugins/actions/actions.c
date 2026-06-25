/*
 * Copyright (C) 2009-2011 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2009      Brian Tarricone <brian@tarricone.org>
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

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <gio/gio.h>

#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>

#include "actions.h"
#include "actions-dialog_ui.h"



#define DEFAULT_TITLE    _("Session Menu")
#define DEFAULT_ICON_SIZE (16)
#define DEFAULT_TIMEOUT   (30)



static void       actions_plugin_get_property        (GObject               *object,
                                                      guint                  prop_id,
                                                      GValue                *value,
                                                      GParamSpec            *pspec);
static void       actions_plugin_set_property        (GObject               *object,
                                                      guint                  prop_id,
                                                      const GValue          *value,
                                                      GParamSpec            *pspec);
static void       actions_plugin_construct           (XfcePanelPlugin       *panel_plugin);
static void       actions_plugin_free_data           (XfcePanelPlugin       *panel_plugin);
static gboolean   actions_plugin_size_changed        (XfcePanelPlugin       *panel_plugin,
                                                      gint                   size);
static void       actions_plugin_configure_plugin    (XfcePanelPlugin       *panel_plugin);
static void       actions_plugin_mode_changed        (XfcePanelPlugin       *panel_plugin,
                                                      XfcePanelPluginMode    mode);
static void       actions_plugin_nrows_changed       (XfcePanelPlugin       *panel_plugin,
                                                      guint                  rows);
static void       actions_plugin_pack                (ActionsPlugin         *plugin);
static GPtrArray *actions_plugin_default_array       (void);
static void       actions_plugin_menu                (GtkWidget             *button,
                                                      ActionsPlugin         *plugin);



typedef enum
{
  APPEARANCE_TYPE_BUTTONS,
  APPEARANCE_TYPE_MENU
}
AppearanceType;

typedef enum
{
  BUTTON_TITLE_TYPE_FULLNAME,
  BUTTON_TITLE_TYPE_USERNAME,
  BUTTON_TITLE_TYPE_USERID,
  BUTTON_TITLE_TYPE_CUSTOM
}
ButtonTitleType;

enum
{
  PROP_0,
  PROP_ITEMS,
  PROP_APPEARANCE,
  PROP_BUTTON_TITLE,
  PROP_CUSTOM_TITLE,
  PROP_ASK_CONFIRMATION
};

enum
{
  COLUMN_VISIBLE,
  COLUMN_DISPLAY_NAME,
  COLUMN_NAME,
  COLUMN_TYPE
};

struct _ActionsPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _ActionsPlugin
{
  XfcePanelPlugin __parent__;

  AppearanceType  type;
  ButtonTitleType button_title;
  gchar          *custom_title;
  GPtrArray      *items;
  GtkWidget      *menu;
  guint           ask_confirmation : 1;
  guint           pack_idle_id;
};

typedef enum
{
  ACTION_TYPE_SEPARATOR     = 1 << 1,
  ACTION_TYPE_LOGOUT        = 1 << 2,
  ACTION_TYPE_LOGOUT_DIALOG = 1 << 3,
  ACTION_TYPE_SWITCH_USER   = 1 << 4,
  ACTION_TYPE_LOCK_SCREEN   = 1 << 5,
  ACTION_TYPE_HIBERNATE     = 1 << 6,
  ACTION_TYPE_HYBRID_SLEEP  = 1 << 7,
  ACTION_TYPE_SUSPEND       = 1 << 8,
  ACTION_TYPE_RESTART       = 1 << 9,
  ACTION_TYPE_SHUTDOWN      = 1 << 10
}
ActionType;

typedef struct
{
  ActionType   type;
  const gchar *name;
  const gchar *display_name;
  const gchar *mnemonic;
  const gchar *question;
  const gchar *status;
  const gchar *icon_name;
  const gchar *fallback_icon_name;
}
ActionEntry;

typedef struct
{
  ActionEntry *entry;
  GtkWidget   *dialog;
  gint         time_left;
  guint        unattended : 1;
}
ActionTimeout;

static ActionEntry action_entries[] =
{
  { ACTION_TYPE_SEPARATOR,
    "separator",
    NULL, NULL, NULL, NULL, NULL, NULL /* not needed */
  },
  { ACTION_TYPE_LOGOUT,
    "logout-dialog",
    N_("Log Out"),
    N_("_Log Out"),
    N_("Are you sure you want to log out?"),
    N_("Logging out in %d seconds."),
    "xfsm-logout",
    "system-log-out"
  },
  { ACTION_TYPE_LOGOUT_DIALOG,
    "logout",
    N_("Log Out..."),
    N_("Log _Out..."),
    NULL, NULL, /* already shows a dialog */
    "xfsm-logout",
    "system-log-out"
  },
  { ACTION_TYPE_SWITCH_USER,
    "switch-user",
    N_("Switch User"),
    N_("_Switch User"),
    NULL, NULL, /* not needed */
    "xfsm-switch-user",
    "system-users"
  },
  { ACTION_TYPE_LOCK_SCREEN,
    "lock-screen",
    N_("Lock Screen"),
    N_("Loc_k Screen"),
    NULL, NULL, /* not needed */
    "xfsm-lock",
    "system-lock-screen"
  },
  { ACTION_TYPE_HIBERNATE,
    "hibernate",
    N_("Hibernate"),
    N_("_Hibernate"),
    N_("Do you want to suspend to disk?"),
    N_("Hibernating computer in %d seconds."),
    "xfsm-hibernate",
    "system-hibernate"
  },
  { ACTION_TYPE_HYBRID_SLEEP,
    "hybrid-sleep",
    N_("Hybrid Sleep"),
    N_("_Hybrid Sleep"),
    N_("Do you want to hibernate and suspend the system?"),
    N_("Hibernating and Suspending computer in %d seconds."),
    "xfsm-hibernate",
    "system-hibernate"
  },
  { ACTION_TYPE_SUSPEND,
    "suspend",
    N_("Suspend"),
    N_("Sus_pend"),
    N_("Do you want to suspend to RAM?"),
    N_("Suspending computer in %d seconds."),
    "xfsm-suspend",
    "system-suspend"
  },
  { ACTION_TYPE_RESTART,
    "restart",
    N_("Restart"),
    N_("_Restart"),
    N_("Are you sure you want to restart?"),
    N_("Restarting computer in %d seconds."),
    "xfsm-reboot",
    "system-reboot"
  },
  { ACTION_TYPE_SHUTDOWN,
    "shutdown",
    N_("Shut Down"),
    N_("Shut _Down"),
    N_("Are you sure you want to shut down?"),
    N_("Turning off computer in %d seconds."),
    "xfsm-shutdown",
    "system-shutdown"
  }
};



/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (ActionsPlugin, actions_plugin)



static GQuark      action_quark = 0;



static void
actions_plugin_class_init (ActionsPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = actions_plugin_set_property;
  gobject_class->get_property = actions_plugin_get_property;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = actions_plugin_construct;
  plugin_class->free_data = actions_plugin_free_data;
  plugin_class->size_changed = actions_plugin_size_changed;
  plugin_class->configure_plugin = actions_plugin_configure_plugin;
  plugin_class->mode_changed = actions_plugin_mode_changed;
  plugin_class->nrows_changed = actions_plugin_nrows_changed;

  g_object_class_install_property (gobject_class,
                                   PROP_ITEMS,
                                   g_param_spec_boxed ("items",
                                                       NULL, NULL,
                                                       G_TYPE_PTR_ARRAY,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_APPEARANCE,
                                   g_param_spec_uint ("appearance",
                                                      NULL, NULL,
                                                      APPEARANCE_TYPE_BUTTONS,
                                                      APPEARANCE_TYPE_MENU,
                                                      APPEARANCE_TYPE_MENU,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_BUTTON_TITLE,
                                   g_param_spec_uint ("button-title",
                                                      NULL, NULL,
                                                      BUTTON_TITLE_TYPE_FULLNAME,
                                                      BUTTON_TITLE_TYPE_CUSTOM,
                                                      BUTTON_TITLE_TYPE_FULLNAME,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_CUSTOM_TITLE,
                                   g_param_spec_string ("custom-title",
                                                        NULL, NULL,
                                                        DEFAULT_TITLE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ASK_CONFIRMATION,
                                   g_param_spec_boolean ("ask-confirmation",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  action_quark = g_quark_from_string ("panel-action-quark");
}



static void
actions_plugin_init (ActionsPlugin *plugin)
{
  plugin->type = APPEARANCE_TYPE_MENU;
  plugin->button_title = BUTTON_TITLE_TYPE_FULLNAME;
  plugin->ask_confirmation = TRUE;
}



static void
actions_plugin_free_array_element (gpointer data)
{
  GValue *value = (GValue *)data;

  g_value_unset (value);
  g_free (value);
}



static void
actions_plugin_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ActionsPlugin *plugin = XFCE_ACTIONS_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_ITEMS:
      g_value_set_boxed (value, plugin->items);
      break;

    case PROP_APPEARANCE:
      g_value_set_uint (value, plugin->type);
      break;

    case PROP_BUTTON_TITLE:
      g_value_set_uint (value, plugin->button_title);
      break;

    case PROP_CUSTOM_TITLE:
      g_value_set_string (value, plugin->custom_title == NULL ?
                          DEFAULT_TITLE : plugin->custom_title);
      break;

    case PROP_ASK_CONFIRMATION:
      g_value_set_boolean (value, plugin->ask_confirmation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
actions_plugin_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ActionsPlugin *plugin = XFCE_ACTIONS_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_ITEMS:
      if (plugin->items != NULL) {
        g_ptr_array_unref (plugin->items);
      }
      plugin->items = g_value_dup_boxed (value);
      actions_plugin_pack (plugin);
      break;

    case PROP_APPEARANCE:
      plugin->type = g_value_get_uint (value);
      actions_plugin_pack (plugin);
      break;

    case PROP_BUTTON_TITLE:
      plugin->button_title = g_value_get_uint (value);
      actions_plugin_pack (plugin);
      break;

    case PROP_CUSTOM_TITLE:
      g_free (plugin->custom_title);
      plugin->custom_title = g_value_dup_string (value);
      actions_plugin_pack (plugin);
      break;

    case PROP_ASK_CONFIRMATION:
      plugin->ask_confirmation = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
actions_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  ActionsPlugin       *plugin = XFCE_ACTIONS_PLUGIN (panel_plugin);
  const PanelProperty  properties[] =
  {
    { "items", G_TYPE_PTR_ARRAY },
    { "appearance", G_TYPE_UINT },
    { "button-title", G_TYPE_UINT },
    { "custom-title", G_TYPE_STRING },
    { "ask-confirmation", G_TYPE_BOOLEAN },
    { NULL }
  };

  /* show the properties dialog */
  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  actions_plugin_pack (plugin);

  /* set orientation and size */
  actions_plugin_mode_changed (panel_plugin,
      xfce_panel_plugin_get_mode (panel_plugin));
}



static void
actions_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  ActionsPlugin *plugin = XFCE_ACTIONS_PLUGIN (panel_plugin);

  if (plugin->pack_idle_id != 0)
    g_source_remove (plugin->pack_idle_id);

  if (plugin->items != NULL)
    g_ptr_array_unref (plugin->items);

  if (plugin->menu != NULL)
    gtk_widget_destroy (plugin->menu);
}



static gboolean
actions_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                             gint             size)
{
  ActionsPlugin *plugin = XFCE_ACTIONS_PLUGIN (panel_plugin);
  GtkWidget     *box;
  GList         *children, *li;
  gint           max_size;
  GtkImage      *icon;
  gint           icon_size;

  if (plugin->type == APPEARANCE_TYPE_BUTTONS)
    {
      max_size = size / xfce_panel_plugin_get_nrows (panel_plugin);
      box = gtk_bin_get_child (GTK_BIN (plugin));
      if (box != NULL)
        {
          children = gtk_container_get_children (GTK_CONTAINER (box));
          if (G_UNLIKELY (children == NULL))
            return TRUE;

          for (li = children; li != NULL; li = li->next)
            {
              if (!GTK_IS_SEPARATOR (li->data))
                {
                  gtk_widget_set_size_request (GTK_WIDGET (li->data),
                                               max_size, max_size);
                  icon = GTK_IMAGE (gtk_bin_get_child (GTK_BIN (li->data)));
                  icon_size = xfce_panel_plugin_get_icon_size (panel_plugin);
                  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);
                }
            }
        }
    }

  return TRUE;
}



static gboolean
actions_plugin_configure_store (gpointer data)
{
  ActionsPlugin *plugin = XFCE_ACTIONS_PLUGIN (data);
  GtkTreeModel  *model;
  GtkTreeIter    iter;
  GPtrArray     *array;
  gboolean       visible;
  gchar         *name;
  GValue        *val;
  gchar          save_name[32];

  model = g_object_get_data (G_OBJECT (plugin), "items-store");
  panel_return_val_if_fail (GTK_IS_LIST_STORE (model), FALSE);

  array = g_ptr_array_new_full (1, (GDestroyNotify) actions_plugin_free_array_element);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      for (;;)
        {
          gtk_tree_model_get (model, &iter,
                              COLUMN_VISIBLE, &visible,
                              COLUMN_NAME, &name, -1);

          val = g_new0 (GValue, 1);
          g_value_init (val, G_TYPE_STRING);
          g_snprintf (save_name, sizeof (save_name), "%s%s",
                      visible ? "+" : "-", name);
          g_value_set_string (val, save_name);
          g_ptr_array_add (array, val);
          g_free (name);

          if (!gtk_tree_model_iter_next (model, &iter))
            break;
        }
    }

  /* Store the new array */
  if (plugin->items != NULL)
    g_ptr_array_unref (plugin->items);
  plugin->items = array;
  g_object_notify (G_OBJECT (plugin), "items");

  return FALSE;
}



static void
actions_plugin_configure_store_idle (ActionsPlugin *plugin)
{
  g_idle_add (actions_plugin_configure_store, plugin);
}



static void
actions_plugin_configure_visible_toggled (GtkCellRendererToggle *renderer,
                                          const gchar           *path_string,
                                          ActionsPlugin         *plugin)
{
  GtkTreeIter   iter;
  gboolean      visible;
  GtkTreeModel *model;

  panel_return_if_fail (XFCE_IS_ACTIONS_PLUGIN (plugin));

  model = g_object_get_data (G_OBJECT (plugin), "items-store");
  panel_return_if_fail (GTK_IS_LIST_STORE (model));
  if (gtk_tree_model_get_iter_from_string (model, &iter, path_string))
    {
      gtk_tree_model_get (model, &iter, COLUMN_VISIBLE, &visible, -1);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COLUMN_VISIBLE, !visible, -1);

      actions_plugin_configure_store (plugin);
    }
}



static ActionEntry *
actions_plugin_lookup_entry (const gchar *name)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (action_entries); i++)
    if (g_strcmp0 (name, action_entries[i].name) == 0)
      return &action_entries[i];

  return NULL;
}



static void
actions_plugin_combo_title_changed_cb (GtkWidget *widget,
                                       gpointer   user_data)
{
  GtkBuilder *builder = GTK_BUILDER (user_data);

  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "entry-cust-title")),
                            gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) == 3);
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "label-cust-title")),
                            gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) == 3);
}



static void
actions_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  ActionsPlugin *plugin = XFCE_ACTIONS_PLUGIN (panel_plugin);
  GtkBuilder    *builder;
  GObject       *dialog;
  GObject       *object;
  GObject       *combo;
  ActionEntry   *entry;
  guint          i;
  const GValue  *val;
  const gchar   *name;
  guint          n;
  GObject       *store;
  gboolean       found;
  GtkTreeIter    iter;
  gchar         *sep_str;
  const gchar   *display_name;

  panel_return_if_fail (XFCE_IS_ACTIONS_PLUGIN (plugin));
  panel_return_if_fail (plugin->items != NULL);

  /* setup the dialog */
  PANEL_UTILS_LINK_4UI
  builder = panel_utils_builder_new (panel_plugin, actions_dialog_ui,
                                     actions_dialog_ui_length, &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

  combo = gtk_builder_get_object (builder, "combo-mode");
  g_object_bind_property (G_OBJECT (plugin), "appearance",
                          G_OBJECT (combo), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  object = gtk_builder_get_object (builder, "revealer-title");
  g_object_bind_property (G_OBJECT (plugin), "appearance",
                          object, "reveal-child",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  combo = gtk_builder_get_object (builder, "combo-title");
  g_object_bind_property (G_OBJECT (plugin), "button-title",
                          G_OBJECT (combo), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  object = gtk_builder_get_object (builder, "entry-cust-title");
  g_object_bind_property (G_OBJECT (plugin), "custom-title",
                          G_OBJECT (object), "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_signal_connect (combo, "changed", G_CALLBACK (actions_plugin_combo_title_changed_cb), builder);
  actions_plugin_combo_title_changed_cb (GTK_WIDGET (combo), builder);

  object = gtk_builder_get_object (builder, "confirmation-dialog");
  g_object_bind_property (G_OBJECT (plugin), "ask-confirmation",
                          G_OBJECT (object), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  store = gtk_builder_get_object (builder, "items-store");
  panel_return_if_fail (GTK_IS_LIST_STORE (store));
  g_object_set_data (G_OBJECT (plugin), "items-store", store);

  object = gtk_builder_get_object (builder, "visible-toggle");
  panel_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (object));
  g_signal_connect (G_OBJECT (object), "toggled",
      G_CALLBACK (actions_plugin_configure_visible_toggled), plugin);

  sep_str = g_markup_printf_escaped ("<span color='grey' style='italic'>%s</span>", _("Separator"));

  /* add items from the settings */
  for (i = 0; i < plugin->items->len; i++)
    {
      /* get the value and check if it is within range */
      val = g_ptr_array_index (plugin->items, i);
      name = g_value_get_string (val);
      if (panel_str_is_empty (name))
        continue;

      /* find the entry in the available actions */
      entry = actions_plugin_lookup_entry (name + 1);
      if (entry == NULL)
        continue;

      if (entry->type == ACTION_TYPE_SEPARATOR)
        display_name = sep_str;
      else
        display_name = _(entry->display_name);

      /* insert in the model */
      gtk_list_store_insert_with_values (GTK_LIST_STORE (store), NULL, i,
                                         COLUMN_VISIBLE, *name == '+',
                                         COLUMN_DISPLAY_NAME, display_name,
                                         COLUMN_NAME, entry->name,
                                         COLUMN_TYPE, entry->type,
                                         -1);
    }

  g_free (sep_str);

  /* check if there are known actions not in the settings */
  for (i = 0; i < G_N_ELEMENTS (action_entries); i++)
    {
      entry = &action_entries[i];
      found = FALSE;

      for (n = 0; n < plugin->items->len; n++)
        {
          val = g_ptr_array_index (plugin->items, n);
          name = g_value_get_string (val);
          if (g_strcmp0 (entry->name, name + 1) == 0)
            {
              found = TRUE;
              break;
            }
        }

      if (!found)
        {
          gtk_list_store_append (GTK_LIST_STORE (store), &iter);
          gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                              COLUMN_VISIBLE, FALSE,
                              COLUMN_DISPLAY_NAME, _(entry->display_name),
                              COLUMN_TYPE, entry->type,
                              COLUMN_NAME, entry->name,
                              -1);
        }
    }

  /* save on dnd changes */
  g_signal_connect_swapped (G_OBJECT (store), "row-inserted",
      G_CALLBACK (actions_plugin_configure_store_idle), plugin);

  gtk_widget_show (GTK_WIDGET (dialog));
}



static void
actions_plugin_mode_changed (XfcePanelPlugin     *panel_plugin,
                             XfcePanelPluginMode  mode)
{
  actions_plugin_pack (XFCE_ACTIONS_PLUGIN (panel_plugin));
}



static void
actions_plugin_nrows_changed (XfcePanelPlugin *panel_plugin,
                              guint            rows)
{
  actions_plugin_pack (XFCE_ACTIONS_PLUGIN (panel_plugin));
}



static gboolean
actions_plugin_action_confirmation_time (gpointer data)
{
  ActionTimeout *timeout = data;

  panel_return_val_if_fail (timeout->entry != NULL, FALSE);

  if (timeout->time_left == 0)
    {
      /* unattended shutdown, don't save the session to avoid blocking the logout */
      timeout->unattended = TRUE;

      gtk_dialog_response (GTK_DIALOG (timeout->dialog),
                           GTK_RESPONSE_ACCEPT);
    }
  else
    {
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (timeout->dialog),
                                                _(timeout->entry->status),
                                                timeout->time_left);
    }

  return --timeout->time_left >= 0;
}



static gboolean
actions_plugin_action_confirmation (ActionsPlugin *plugin,
                                    ActionEntry   *entry,
                                    gboolean      *unattended)
{
  GtkWidget     *dialog;
  GtkWidget     *button;
  gint           result;
  GtkWidget     *image;
  ActionTimeout *timeout;
  guint          timeout_id;

  panel_return_val_if_fail (entry->question != NULL, FALSE);
  panel_return_val_if_fail (entry->status != NULL, FALSE);

  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
                                   "%s", _(entry->question));
  gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
  gtk_window_stick (GTK_WINDOW (dialog));
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog), _(entry->display_name));

  button = gtk_dialog_add_button (GTK_DIALOG (dialog), _(entry->mnemonic), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), entry->icon_name))
    image = gtk_image_new_from_icon_name (entry->icon_name, GTK_ICON_SIZE_BUTTON);
  else
    image = gtk_image_new_from_icon_name (entry->fallback_icon_name, GTK_ICON_SIZE_BUTTON);

  gtk_button_set_image (GTK_BUTTON (button), image);

  timeout = g_slice_new0 (ActionTimeout);
  timeout->entry = entry;
  timeout->time_left = DEFAULT_TIMEOUT;
  timeout->dialog = dialog;
  timeout->unattended = FALSE;

  /* first second looks out of sync with a second timer */
  timeout_id = g_timeout_add (1000, actions_plugin_action_confirmation_time, timeout);
  actions_plugin_action_confirmation_time (timeout);

  result = gtk_dialog_run (GTK_DIALOG (dialog));

  if (unattended != NULL)
    *unattended = timeout->unattended;

  g_source_remove (timeout_id);
  gtk_widget_destroy (dialog);
  g_slice_free (ActionTimeout, timeout);

  return result == GTK_RESPONSE_ACCEPT;
}



static GDBusProxy *
actions_plugin_action_dbus_proxy_session (GDBusConnection *conn)
{
  return g_dbus_proxy_new_sync (conn,
                                G_DBUS_PROXY_FLAGS_NONE,
                                NULL,
                                "org.xfce.SessionManager",
                                "/org/xfce/SessionManager",
                                "org.xfce.Session.Manager",
                                NULL,
                                NULL);
}



static gboolean
actions_plugin_action_dbus_xfsm (const gchar  *method,
                                 gboolean      show_dialog,
                                 gboolean      allow_save,
                                 GError      **error)
{
  GDBusConnection *conn;
  GDBusProxy      *proxy;
  GVariant        *retval;

  conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
  if (conn == NULL)
    return FALSE;

  proxy = actions_plugin_action_dbus_proxy_session (conn);
  if (G_LIKELY (proxy != NULL))
    {
      if (g_strcmp0 (method, "Logout") == 0)
        {
          retval = g_dbus_proxy_call_sync (proxy, method,
                                           g_variant_new ("(bb)",
                                                          show_dialog,
                                                          allow_save),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           error);
        }
      else if (g_strcmp0 (method, "Suspend") == 0
               || g_strcmp0 (method, "Hibernate") == 0
               || g_strcmp0 (method, "HybridSleep") == 0)
        {
          retval = g_dbus_proxy_call_sync (proxy, method,
                                           NULL,
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           error);
        }
      else
        {
          retval = g_dbus_proxy_call_sync (proxy, method,
                                           g_variant_new ("(b)",
                                                          show_dialog),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           error);
        }

      g_object_unref (G_OBJECT (proxy));

      if (retval)
        {
          g_variant_unref (retval);
          return TRUE;
        }
    }

  return FALSE;
}



static gboolean
actions_plugin_action_dbus_can (GDBusProxy  *proxy,
                                const gchar *method)
{
  GVariant *retval;
  gboolean allowed = FALSE;
  GError *error = NULL;

  retval = g_dbus_proxy_call_sync (proxy, method,
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);

  if (G_LIKELY (retval))
    {
      g_variant_get (retval, "(b)", &allowed);
      g_variant_unref (retval);
    }
  else if (error)
    {
      g_warning ("Calling %s failed %s", method, error->message);
      g_error_free (error);
    }

  return allowed;
}



static ActionType
actions_plugin_actions_allowed (void)
{
  GDBusConnection *conn;
  ActionType       allow_mask = ACTION_TYPE_SEPARATOR;
  gchar           *path;
  GDBusProxy      *proxy;
  GError          *error = NULL;

  /* check for commands we use */
  path = g_find_program_in_path ("dm-tool");
  if (path != NULL)
    PANEL_SET_FLAG (allow_mask, ACTION_TYPE_SWITCH_USER);
  else
  {
    /* check for gdmflexiserver if dm-tool is not present */
    g_free (path);
    path = g_find_program_in_path ("gdmflexiserver");
    if (path != NULL)
      PANEL_SET_FLAG (allow_mask, ACTION_TYPE_SWITCH_USER);
  }
  g_free (path);

  path = g_find_program_in_path ("xflock4");
  if (path != NULL)
    PANEL_SET_FLAG (allow_mask, ACTION_TYPE_LOCK_SCREEN);
  g_free (path);

  /* session bus for querying the managers */
  conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (conn != NULL)
    {
      /* xfce4-session */
      proxy = actions_plugin_action_dbus_proxy_session (conn);
      if (G_LIKELY (proxy != NULL))
        {
          /* when xfce4-session is connected, we can logout */
          PANEL_SET_FLAG (allow_mask, ACTION_TYPE_LOGOUT | ACTION_TYPE_LOGOUT_DIALOG);

          if (actions_plugin_action_dbus_can (proxy, "CanShutdown"))
            PANEL_SET_FLAG (allow_mask, ACTION_TYPE_SHUTDOWN);

          if (actions_plugin_action_dbus_can (proxy, "CanRestart"))
            PANEL_SET_FLAG (allow_mask, ACTION_TYPE_RESTART);

          if (actions_plugin_action_dbus_can (proxy, "CanSuspend"))
            PANEL_SET_FLAG (allow_mask, ACTION_TYPE_SUSPEND);

          if (actions_plugin_action_dbus_can (proxy, "CanHibernate"))
            PANEL_SET_FLAG (allow_mask, ACTION_TYPE_HIBERNATE);

          if (actions_plugin_action_dbus_can (proxy, "CanHybridSleep"))
            PANEL_SET_FLAG (allow_mask, ACTION_TYPE_HYBRID_SLEEP);

          g_object_unref (G_OBJECT (proxy));
        }
    }
  else
    {
      g_critical ("Unable to open DBus session bus: %s", error->message);
      g_error_free (error);
    }

  return allow_mask;
}



static void
actions_plugin_action_activate (GtkWidget      *widget,
                                ActionsPlugin  *plugin)
{
  ActionEntry   *entry;
  gboolean       unattended = FALSE;
  GError        *error = NULL;
  gboolean       succeed = FALSE;
  XfconfChannel *channel;
  gboolean       allow_save;
  gchar         *path;

  entry = g_object_get_qdata (G_OBJECT (widget), action_quark);
  panel_return_if_fail (entry != NULL);

  if (plugin->ask_confirmation
      && entry->question != NULL
      && entry->status != NULL
      && !actions_plugin_action_confirmation (plugin, entry, &unattended))
    return;

  channel = xfconf_channel_get ("xfce4-session");
  allow_save = xfconf_channel_get_bool (channel, "/general/SaveOnExit", FALSE);
  /* unattended shutdown, don't save the session to avoid blocking the logout */
  allow_save = allow_save && !unattended;

  switch (entry->type)
    {
    case ACTION_TYPE_LOGOUT:
      succeed = actions_plugin_action_dbus_xfsm ("Logout", FALSE,
                                                 allow_save, &error);
      break;

    case ACTION_TYPE_LOGOUT_DIALOG:
      succeed = actions_plugin_action_dbus_xfsm ("Logout", TRUE,
                                                 allow_save, &error);
      break;

    case ACTION_TYPE_RESTART:
      succeed = actions_plugin_action_dbus_xfsm ("Restart", FALSE,
                                                 allow_save, &error);
      break;

    case ACTION_TYPE_SHUTDOWN:
      succeed = actions_plugin_action_dbus_xfsm ("Shutdown", FALSE,
                                                 allow_save, &error);
      break;

    case ACTION_TYPE_HIBERNATE:
      succeed = actions_plugin_action_dbus_xfsm ("Hibernate", FALSE,
                                                 FALSE, &error);
      break;

    case ACTION_TYPE_HYBRID_SLEEP:
      succeed = actions_plugin_action_dbus_xfsm ("HybridSleep", FALSE,
                                                 FALSE, &error);
      break;

    case ACTION_TYPE_SUSPEND:
      succeed = actions_plugin_action_dbus_xfsm ("Suspend", FALSE,
                                                 FALSE, &error);
      break;

    case ACTION_TYPE_SWITCH_USER:
      path = g_find_program_in_path ("dm-tool");
      if (path != NULL)
        succeed = g_spawn_command_line_async ("dm-tool switch-to-greeter", &error);
      else
        succeed = g_spawn_command_line_async ("gdmflexiserver", &error);
      g_free (path);
      break;

    case ACTION_TYPE_LOCK_SCREEN:
      succeed = g_spawn_command_line_async ("xflock4", &error);
      break;

    default:
      panel_assert_not_reached ();
      return;
    }

  if (!succeed)
    {
      xfce_dialog_show_error (NULL, error,
                              _("Failed to run action \"%s\""),
                              _(entry->display_name));
    }
}



static GtkWidget *
actions_plugin_action_button (ActionsPlugin  *plugin,
                              const gchar    *name,
                              GtkOrientation  orientation,
                              ActionType     *type)
{
  GtkWidget   *widget;
  GtkWidget   *image;
  ActionEntry *entry;

  /* lookup the action entry */
  entry = actions_plugin_lookup_entry (name);
  if (entry == NULL)
    return NULL;

  if (type)
    *type = entry->type;

  if (entry->type == ACTION_TYPE_SEPARATOR)
    {
      widget = gtk_separator_new (orientation);
    }
  else
    {
      widget = xfce_panel_create_button ();
      gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
      g_object_set_qdata (G_OBJECT (widget), action_quark, entry);
      gtk_widget_set_tooltip_text (widget, _(entry->display_name));
      g_signal_connect (G_OBJECT (widget), "clicked",
          G_CALLBACK (actions_plugin_action_activate), plugin);

      if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), entry->icon_name))
        image = gtk_image_new_from_icon_name (entry->icon_name, GTK_ICON_SIZE_BUTTON);
      else
        image = gtk_image_new_from_icon_name (entry->fallback_icon_name, GTK_ICON_SIZE_BUTTON);

      gtk_container_add (GTK_CONTAINER (widget), image);
      gtk_widget_show (image);
    }

  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), widget);

  return widget;
}



static GtkWidget *
actions_plugin_action_menu_item (ActionsPlugin *plugin,
                                 const gchar   *name,
                                 ActionType    *type)
{
  GtkWidget   *mi;
  GtkWidget   *image;
  ActionEntry *entry;

  /* lookup the action entry */
  entry = actions_plugin_lookup_entry (name);
  if (entry == NULL)
    return NULL;

  if (type)
    *type = entry->type;

  if (entry->type == ACTION_TYPE_SEPARATOR)
    return gtk_separator_menu_item_new ();

  mi = panel_image_menu_item_new_with_mnemonic (_(entry->mnemonic));
  g_object_set_qdata (G_OBJECT (mi), action_quark, entry);
  g_signal_connect (G_OBJECT (mi), "activate",
      G_CALLBACK (actions_plugin_action_activate), plugin);

  if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), entry->icon_name))
    image = gtk_image_new_from_icon_name (entry->icon_name, GTK_ICON_SIZE_MENU);
  else
    image = gtk_image_new_from_icon_name (entry->fallback_icon_name, GTK_ICON_SIZE_MENU);

  panel_image_menu_item_set_image (mi, image);
  gtk_widget_show (image);

  return mi;
}



static gboolean
actions_plugin_pack_idle (gpointer data)
{
  ActionsPlugin       *plugin = XFCE_ACTIONS_PLUGIN (data);
  GtkWidget           *label;
  GtkWidget           *button;
  GtkWidget           *widget;
  const gchar         *button_title;
  GtkWidget           *child;
  GtkWidget           *box;
  guint                i;
  const GValue        *val;
  const gchar         *name;
  GtkOrientation       orientation;
  ActionType           allowed_types;
  ActionType           type;
  XfcePanelPluginMode  mode;
  guint                panel_rows;
  guint                left;
  guint                top;

  child = gtk_bin_get_child (GTK_BIN (plugin));
  if (child != NULL)
    gtk_widget_destroy (child);

  if (plugin->menu != NULL)
    gtk_widget_destroy (plugin->menu);

  if (plugin->items == NULL)
    plugin->items = actions_plugin_default_array ();

  allowed_types = actions_plugin_actions_allowed ();

  if (plugin->type == APPEARANCE_TYPE_BUTTONS)
    {
      panel_rows = left = top = 0;

      switch (xfce_panel_plugin_get_mode (XFCE_PANEL_PLUGIN (plugin)))
        {
        case XFCE_PANEL_PLUGIN_MODE_VERTICAL:
          orientation = GTK_ORIENTATION_VERTICAL;
          box = gtk_box_new (orientation, 0);
          break;

        case XFCE_PANEL_PLUGIN_MODE_DESKBAR:
          panel_rows = xfce_panel_plugin_get_nrows (XFCE_PANEL_PLUGIN (plugin));
          orientation = GTK_ORIENTATION_VERTICAL;
          box = gtk_grid_new ();
          break;

        default:
          orientation = GTK_ORIENTATION_HORIZONTAL;
          box = gtk_box_new (orientation, 0);
          break;
        };

      gtk_container_add (GTK_CONTAINER (plugin), box);
      gtk_widget_show (box);

      for (i = 0; i < plugin->items->len; i++)
        {
          val = g_ptr_array_index (plugin->items, i);
          name = g_value_get_string (val);
          if (name == NULL || *name != '+')
            continue;

          widget = actions_plugin_action_button (plugin, name + 1, orientation, &type);
          if (widget != NULL)
            {
              if (!panel_rows)
                {
                  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
                }
              else if (type == ACTION_TYPE_SEPARATOR)
                {
                  if (left > 0)
                    top++;
                  gtk_grid_attach (GTK_GRID (box), widget, 0, top, panel_rows, 1);
                  left = 0;
                  top++;
                }
              else
                {
                  gtk_grid_attach (GTK_GRID (box), widget, left, top, 1, 1);
                  left++;
                  if (left >= panel_rows)
                    {
                      left = 0;
                      top++;
                    }
                }

              gtk_widget_set_sensitive (widget, PANEL_HAS_FLAG (allowed_types, type));
              gtk_widget_show (widget);
            }
        }

      actions_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin),
          xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));
    }
  else
    {
      switch (plugin->button_title)
        {
          case BUTTON_TITLE_TYPE_FULLNAME:
            /* get a decent username, not the glib defaults */
            button_title = g_get_real_name ();
            if (panel_str_is_empty (button_title)
                || strcmp (button_title, "Unknown") == 0)
              {
                button_title = g_get_user_name ();
                if (panel_str_is_empty (button_title)
                    || strcmp (button_title, "username") == 0)
                  button_title = _("Little Mouse");
              }
            break;

          case BUTTON_TITLE_TYPE_USERNAME:
            button_title = g_get_user_name ();
            if (panel_str_is_empty (button_title))
              button_title = "username";
            break;

          case BUTTON_TITLE_TYPE_USERID:
            {
              char buf[16];
              snprintf(buf, sizeof(buf), "%u", (unsigned)getuid());
              button_title = buf;
            }
            break;

          default:
          case BUTTON_TITLE_TYPE_CUSTOM:
            button_title = (plugin->custom_title == NULL?
                            DEFAULT_TITLE : plugin->custom_title);
            break;
        }

      button = xfce_arrow_button_new (GTK_ARROW_NONE);
      gtk_widget_set_name (button, "actions-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), button);
      gtk_container_add (GTK_CONTAINER (plugin), button);
      g_signal_connect (G_OBJECT (button), "toggled",
          G_CALLBACK (actions_plugin_menu), plugin);
      gtk_widget_show (button);

      label = gtk_label_new (button_title);
      gtk_container_add (GTK_CONTAINER (button), label);
      mode = xfce_panel_plugin_get_mode (XFCE_PANEL_PLUGIN (plugin));
      gtk_label_set_angle (GTK_LABEL (label),
          (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ? 270 : 0);
      gtk_label_set_ellipsize (GTK_LABEL (label),
                               (mode == XFCE_PANEL_PLUGIN_MODE_DESKBAR) ?
                               PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE);
      gtk_widget_show (label);
    }

  return FALSE;
}



static void
actions_plugin_pack_idle_destoyed (gpointer data)
{
  XFCE_ACTIONS_PLUGIN (data)->pack_idle_id = 0;
}



static void
actions_plugin_pack (ActionsPlugin *plugin)
{
  if (plugin->pack_idle_id == 0)
    {
      plugin->pack_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, actions_plugin_pack_idle,
                                              plugin, actions_plugin_pack_idle_destoyed);
    }
}



static GPtrArray *
actions_plugin_default_array (void)
{
  GPtrArray   *array;
  GValue      *val;
  guint        i;
  const gchar *defaults[] =
    {
      "+lock-screen",
      "+switch-user",
      "+separator",
      "+suspend",
      "-hibernate",
      "-hybrid-sleep",
      "-separator",
      "+shutdown",
      "-restart",
      "+separator",
      "+logout"
    };

  array = g_ptr_array_new_full (G_N_ELEMENTS (defaults), actions_plugin_free_array_element);
  for (i = 0; i < G_N_ELEMENTS (defaults); i++)
    {
      val = g_new0 (GValue, 1);
      g_value_init (val, G_TYPE_STRING);
      g_value_set_static_string (val, defaults[i]);
      g_ptr_array_add (array, val);
    }

  return array;
}



static void
actions_plugin_menu_deactivate (GtkWidget     *menu,
                                ActionsPlugin *plugin)
{
  GtkWidget *button;

  panel_return_if_fail (XFCE_IS_ACTIONS_PLUGIN (plugin));
  panel_return_if_fail (GTK_IS_MENU (menu));

  /* button is NULL when we popup the menu under the cursor position */
  button = gtk_bin_get_child (GTK_BIN (plugin));
  if (button != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}



static void
actions_plugin_menu (GtkWidget     *button,
                     ActionsPlugin *plugin)
{
  guint         i;
  const GValue *val;
  const gchar  *name;
  GtkWidget    *mi;
  ActionType    type;
  ActionType    allowed_types;

  panel_return_if_fail (XFCE_IS_ACTIONS_PLUGIN (plugin));
  panel_return_if_fail (button != NULL);

  /* do not popup the menu if the button is being toggled off */
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;

  if (plugin->menu == NULL)
    {
      plugin->menu = gtk_menu_new ();
      g_signal_connect (G_OBJECT (plugin->menu), "deactivate",
          G_CALLBACK (actions_plugin_menu_deactivate), plugin);
      g_object_add_weak_pointer (G_OBJECT (plugin->menu), (gpointer) &plugin->menu);

      allowed_types = actions_plugin_actions_allowed ();

      for (i = 0; i < plugin->items->len; i++)
        {
          val = g_ptr_array_index (plugin->items, i);
          name = g_value_get_string (val);
          if (name == NULL || *name != '+')
            continue;

          mi = actions_plugin_action_menu_item (plugin, name + 1, &type);
          if (mi != NULL)
            {
              gtk_menu_shell_append (GTK_MENU_SHELL (plugin->menu), mi);
              gtk_widget_set_sensitive (mi, PANEL_HAS_FLAG (allowed_types, type));
              gtk_widget_show (mi);
            }
        }
    }

  xfce_panel_plugin_popup_menu (XFCE_PANEL_PLUGIN (plugin), GTK_MENU (plugin->menu), button, NULL);
}
