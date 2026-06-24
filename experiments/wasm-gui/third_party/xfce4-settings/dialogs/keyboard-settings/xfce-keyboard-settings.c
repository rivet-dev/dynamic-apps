/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 *               2008 Olivier Fourdan <olivier@xfce.org>
 * Portions based on xfkc
 * Copyright (c) 2007 Gauvain Pocentek <gauvainpocentek@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <exo/exo.h>
#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4kbd-private/xfce-shortcuts-provider.h>
#include <libxfce4kbd-private/xfce-shortcut-dialog.h>

#include "keyboard-dialog_ui.h"
#include "xfce-keyboard-settings.h"
#include "command-dialog.h"

#ifdef HAVE_LIBXKLAVIER
#include <libxklavier/xklavier.h>
#endif /* HAVE_LIBXKLAVIER */

#define CUSTOM_BASE_PROPERTY         "/commands/custom"



enum
{
  COMMAND_COLUMN,
  SHORTCUT_COLUMN,
  SNOTIFY_COLUMN,
  SHORTCUT_LABEL_COLUMN,
  N_COLUMNS
};

enum
{
    XKB_LAYOUTS_COMBO_DESCRIPTION = 0,
    XKB_LAYOUTS_COMBO_VALUE,
    XKB_LAYOUTS_COMBO_NUM_COLUMNS
};

enum
{
    XKB_TREE_LAYOUTS = 0,
    XKB_TREE_LAYOUTS_NAMES,
    XKB_TREE_VARIANTS,
    XKB_TREE_VARIANTS_NAMES,
    XKB_TREE_NUM_COLUMNS
};

enum
{
    XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION = 0,
    XKB_AVAIL_LAYOUTS_TREE_ID,
    XKB_AVAIL_LAYOUTS_TREE_NUM_COLUMNS
};

typedef enum
{
    MOVE_LAYOUT_UP,
    MOVE_LAYOUT_DOWN
} MOVE_LAYOUT_DIRECTION;


typedef struct _XfceKeyboardShortcutInfo    XfceKeyboardShortcutInfo;

typedef
void (*XfceKeyboardLayoutsComboInitFunc) (XfceKeyboardSettings *settings);

typedef
void (*XfceKeyboardLayoutsComboChangedFunc) (GtkComboBox          *combo,
                                             XfceKeyboardSettings *settings);


static void                      xfce_keyboard_settings_constructed           (GObject                   *object);
static void                      xfce_keyboard_settings_finalize              (GObject                   *object);
static void                      xfce_keyboard_settings_row_activated         (GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path,
                                                                               GtkTreeViewColumn         *column,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_initialize_shortcuts  (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_load_shortcuts        (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_edit_shortcut         (XfceKeyboardSettings      *settings,
                                                                               GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path);
static void                      xfce_keyboard_settings_edit_command          (XfceKeyboardSettings      *settings,
                                                                               GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path);
static gboolean                  xfce_keyboard_settings_validate_shortcut     (XfceShortcutDialog        *dialog,
                                                                               const gchar               *shortcut,
                                                                               XfceKeyboardSettings      *settings);
static XfceKeyboardShortcutInfo *xfce_keyboard_settings_get_shortcut_info     (XfceKeyboardSettings      *settings,
                                                                               const gchar               *shortcut);
static void                      xfce_keyboard_settings_free_shortcut_info    (XfceKeyboardShortcutInfo  *info);
static void                      xfce_keyboard_settings_shortcut_added        (XfceShortcutsProvider     *provider,
                                                                               const gchar               *shortcut,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_shortcut_removed      (XfceShortcutsProvider     *provider,
                                                                               const gchar               *shortcut,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_button_clicked    (XfceKeyboardSettings      *settings,
                                                                               GtkButton                 *button);
static void                      xfce_keyboard_settings_edit_button_clicked   (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_delete_button_clicked (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_reset_button_clicked  (XfceKeyboardSettings      *settings);

#ifdef HAVE_LIBXKLAVIER

static gboolean                  xfce_keyboard_settings_update_sensitive      (GtkSwitch                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_system_default_cb     (GtkSwitch                 *widget,
                                                                               gboolean                   state,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_set_layout            (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_init_layout           (XfceKeyboardSettings      *settings);

static void                      xfce_keyboard_settings_layouts_combo_populate(XfceKeyboardSettings     *settings,
                                                                               const gchar              *combo_name,
                                                                               const gchar              *option_group_name,
                                                                               XfceKeyboardLayoutsComboInitFunc init_func,
                                                                               XfceKeyboardLayoutsComboChangedFunc cb_func);
static void                      xfce_keyboard_settings_layouts_combo_init    (XfceKeyboardSettings      *settings,
                                                                               const gchar               *combo_name,
                                                                               const gchar               *xfconf_prop_name,
                                                                               const gchar               *default_value);
static void                      xfce_keyboard_settings_layouts_combo_add     (XklConfigRegistry         *config_registry,
                                                                               const XklConfigItem       *config_item,
                                                                               gpointer                   user_data);
static void                      xfce_keyboard_settings_layouts_combo_changed (GtkComboBox               *combo,
                                                                               XfceKeyboardSettings      *settings,
                                                                               const gchar               *xfconf_prop_name);

static void                      xfce_keyboard_settings_init_model            (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_model_changed_cb      (GtkComboBox               *combo,
                                                                               XfceKeyboardSettings      *settings);

static void                      xfce_keyboard_settings_init_grpkey           (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_grpkey_changed_cb     (GtkComboBox               *combo,
                                                                               XfceKeyboardSettings      *settings);

static void                      xfce_keyboard_settings_init_compkey          (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_compkey_changed_cb    (GtkComboBox               *combo,
                                                                               XfceKeyboardSettings      *settings);

static void                      xfce_keyboard_settings_update_layout_buttons (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_edit_layout_button_cb (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_layout_button_cb  (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_row_activated_cb      (GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path,
                                                                               GtkTreeViewColumn         *column,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_del_layout_button_cb  (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_layout_move           (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings,
                                                                               MOVE_LAYOUT_DIRECTION      direction);
static void                      xfce_keyboard_settings_up_layout_button_cb   (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_down_layout_button_cb (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_variant_to_list   (XklConfigRegistry         *config_registry,
                                                                               XklConfigItem             *config_item,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_layout_to_list    (XklConfigRegistry         *config_registry,
                                                                               XklConfigItem             *config_item,
                                                                               XfceKeyboardSettings      *settings);
static gchar**                   xfce_keyboard_settings_layout_selection      (XfceKeyboardSettings      *settings,
                                                                               const gchar               *layout,
                                                                               const gchar               *variant);

#endif /* HAVE_LIBXKLAVIER */



struct _XfceKeyboardSettingsPrivate
{
  XfceShortcutsProvider *provider;

#ifdef HAVE_LIBXKLAVIER
  XklEngine             *xkl_engine;
  XklConfigRegistry     *xkl_registry;
  XklConfigRec          *xkl_rec_config;
  GtkTreeIter            layout_selection_iter;
  GtkTreeStore          *layout_selection_treestore;
#endif

  XfconfChannel         *keyboards_channel;
  XfconfChannel         *keyboard_layout_channel;
  XfconfChannel         *xsettings_channel;
};

struct _XfceKeyboardShortcutInfo
{
  XfceShortcutsProvider *provider;
  XfceShortcut          *shortcut;
};



G_DEFINE_TYPE_WITH_PRIVATE (XfceKeyboardSettings, xfce_keyboard_settings, GTK_TYPE_BUILDER)



static void
xfce_keyboard_settings_class_init (XfceKeyboardSettingsClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = xfce_keyboard_settings_constructed;
  gobject_class->finalize = xfce_keyboard_settings_finalize;
}



static void
xfce_keyboard_settings_init (XfceKeyboardSettings *settings)
{
  GError *error = NULL;

  settings->priv = xfce_keyboard_settings_get_instance_private (settings);

  settings->priv->keyboards_channel = xfconf_channel_new ("keyboards");
  settings->priv->keyboard_layout_channel = xfconf_channel_new ("keyboard-layout");
  settings->priv->xsettings_channel = xfconf_channel_new ("xsettings");

  settings->priv->provider = xfce_shortcuts_provider_new ("commands");
  g_signal_connect (settings->priv->provider, "shortcut-added",
                    G_CALLBACK (xfce_keyboard_settings_shortcut_added), settings);
  g_signal_connect (settings->priv->provider, "shortcut-removed",
                    G_CALLBACK (xfce_keyboard_settings_shortcut_removed), settings);

  if (gtk_builder_add_from_string (GTK_BUILDER (settings), keyboard_dialog_ui,
                                   keyboard_dialog_ui_length, &error) == 0)
    {
      g_error ("Failed to load the UI file: %s.", error->message);
      g_error_free (error);
    }
}



/**
  This is a tad hacky - it will query xkb keyboard models
  if @option_group_name is NULL and will query the xkb option
  @option_group_name if it is not NULL
 */
#ifdef HAVE_LIBXKLAVIER
static void
xfce_keyboard_settings_layouts_combo_populate (XfceKeyboardSettings *settings,
                                               const gchar *combo_name,
                                               const gchar *option_group_name,
                                               XfceKeyboardLayoutsComboInitFunc combo_init_func,
                                               XfceKeyboardLayoutsComboChangedFunc combo_changed_func)
{
  GtkListStore    *list_store;
  GtkTreeIter      iter;
  GObject         *xkb_combo;
  GtkCellRenderer *renderer;

  list_store = gtk_list_store_new (XKB_LAYOUTS_COMBO_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), 0, GTK_SORT_ASCENDING);

  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter,
                      XKB_LAYOUTS_COMBO_DESCRIPTION, "-",
                      XKB_LAYOUTS_COMBO_VALUE, "", -1);

  if (option_group_name != NULL)
  {
    xkl_config_registry_foreach_option (settings->priv->xkl_registry,
                                        option_group_name,
                                        xfce_keyboard_settings_layouts_combo_add,
                                        list_store);
  }
  else
  {
    xkl_config_registry_foreach_model (settings->priv->xkl_registry,
                                       xfce_keyboard_settings_layouts_combo_add,
                                       list_store);
  }

  xkb_combo = gtk_builder_get_object (GTK_BUILDER (settings), combo_name);
  gtk_combo_box_set_model (GTK_COMBO_BOX (xkb_combo), GTK_TREE_MODEL (list_store));
  g_object_unref (G_OBJECT (list_store));

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (xkb_combo));
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (xkb_combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (xkb_combo), renderer, "text", 0);

  combo_init_func (settings);
  g_signal_connect (G_OBJECT (xkb_combo), "changed",
                    G_CALLBACK (combo_changed_func), settings);
}
#endif /* HAVE_LIBXKLAVIER */

static void
xfce_keyboard_settings_constructed (GObject *object)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);
  GtkTreeViewColumn    *column;
  GtkCellRenderer      *renderer;
  GObject              *xkb_key_repeat_rate;
  GObject              *xkb_key_repeat_delay;
  GObject              *net_cursor_blink_time;
  GtkListStore         *list_store;
  GObject              *xkb_key_repeat_check;
  GObject              *xkb_key_repeat_box;
  GObject              *net_cursor_blink_check;
  GObject              *net_cursor_blink_box;
  GObject              *kbd_shortcuts_view;
  GObject              *xkb_numlock;
  GObject              *button;
#ifdef HAVE_LIBXKLAVIER
  GObject              *xkb_use_system_default_switch;
  GObject              *xkb_tab_layout_vbox;
  GObject              *xkb_layout_view;
  GObject              *xkb_layout_add_button;
  GObject              *xkb_layout_edit_button;
  GObject              *xkb_layout_delete_button;
  GObject              *xkb_layout_up_button;
  GObject              *xkb_layout_down_button;
#endif /* HAVE_LIBXKLAVIER */

  /* XKB settings */
  xkb_key_repeat_check = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_check");
  xkb_key_repeat_box = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_box");

  g_object_bind_property(G_OBJECT (xkb_key_repeat_check), "active",
                         G_OBJECT (xkb_key_repeat_box), "sensitive",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat", G_TYPE_BOOLEAN, G_OBJECT (xkb_key_repeat_check), "active");

  xkb_key_repeat_rate = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_rate");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat/Rate", G_TYPE_INT, xkb_key_repeat_rate, "value");

  xkb_key_repeat_delay = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_delay");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat/Delay", G_TYPE_INT, xkb_key_repeat_delay, "value");

  xkb_numlock = gtk_builder_get_object (GTK_BUILDER (settings), "restore_numlock");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/RestoreNumlock", G_TYPE_BOOLEAN, xkb_numlock, "active");

  /* XSETTINGS */
  net_cursor_blink_check = gtk_builder_get_object (GTK_BUILDER (settings), "net_cursor_blink_check");
  net_cursor_blink_box = gtk_builder_get_object (GTK_BUILDER (settings), "net_cursor_blink_box");
  g_object_bind_property(G_OBJECT (net_cursor_blink_check), "active",
                         G_OBJECT (net_cursor_blink_box), "sensitive",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  xfconf_g_property_bind (settings->priv->xsettings_channel, "/Net/CursorBlink", G_TYPE_BOOLEAN, G_OBJECT (net_cursor_blink_check), "active");

  net_cursor_blink_time = gtk_builder_get_object (GTK_BUILDER (settings), "net_cursor_blink_time");
  xfconf_g_property_bind (settings->priv->xsettings_channel, "/Net/CursorBlinkTime", G_TYPE_INT, net_cursor_blink_time, "value");

  /* Configure shortcuts tree view */
  kbd_shortcuts_view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (kbd_shortcuts_view)), GTK_SELECTION_MULTIPLE);
  g_signal_connect (kbd_shortcuts_view, "row-activated", G_CALLBACK (xfce_keyboard_settings_row_activated), settings);

  /* Create list store for keyboard shortcuts */
  list_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COMMAND_COLUMN, GTK_SORT_ASCENDING);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (kbd_shortcuts_view), COMMAND_COLUMN);
  gtk_tree_view_set_model (GTK_TREE_VIEW (kbd_shortcuts_view), GTK_TREE_MODEL (list_store));

  /* Create command column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Command"), renderer, "text", COMMAND_COLUMN, NULL);
  g_object_set (column, "expand", TRUE, "resizable", TRUE, NULL);
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_set_sort_column_id (column, COMMAND_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Create shortcut column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, "text", SHORTCUT_LABEL_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (column, SHORTCUT_LABEL_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Connect to add button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "add_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_add_button_clicked), settings);

  /* Connect to edit button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "edit_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_edit_button_clicked), settings);

  /* Connect to remove button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "delete_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_delete_button_clicked), settings);

  /* Connect to reset button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "reset_shortcuts_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_reset_button_clicked), settings);

  xfce_keyboard_settings_initialize_shortcuts (settings);
  xfce_keyboard_settings_load_shortcuts (settings);

#ifdef HAVE_LIBXKLAVIER
  /* Init xklavier engine */
  settings->priv->xkl_engine = xkl_engine_get_instance (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
  xkl_engine_start_listen (settings->priv->xkl_engine, XKLL_TRACK_KEYBOARD_STATE);

  settings->priv->xkl_rec_config = xkl_config_rec_new ();
  xkl_config_rec_get_from_server (settings->priv->xkl_rec_config, settings->priv->xkl_engine);

  settings->priv->xkl_registry = xkl_config_registry_get_instance (settings->priv->xkl_engine);
#ifdef HAVE_LIBXKLAVIER4
  xkl_config_registry_load (settings->priv->xkl_registry, FALSE);
#else
  xkl_config_registry_load (settings->priv->xkl_registry);
#endif

  /* Tab */
  xkb_tab_layout_vbox = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_tab_layout_vbox");
  gtk_widget_show (GTK_WIDGET (xkb_tab_layout_vbox));

  /* Use system defaults, i.e., disable options */
  xkb_use_system_default_switch = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_use_system_default_switch");
  xfconf_g_property_bind (settings->priv->keyboard_layout_channel, "/Default/XkbDisable", G_TYPE_BOOLEAN,
                          (GObject *) xkb_use_system_default_switch, "active");
  g_signal_connect (G_OBJECT (xkb_use_system_default_switch),
                    "state-set",
                    G_CALLBACK (xfce_keyboard_settings_system_default_cb),
                    settings);
  xfce_keyboard_settings_update_sensitive (GTK_SWITCH (xkb_use_system_default_switch), settings);

  /* Keyboard model combo */
  xfce_keyboard_settings_layouts_combo_populate (settings,
                                                 "xkb_model_combo",
                                                 NULL,
                                                 xfce_keyboard_settings_init_model,
                                                 xfce_keyboard_settings_model_changed_cb);
  /* Group key combo */
  xfce_keyboard_settings_layouts_combo_populate (settings,
                                                 "xkb_grpkey_combo",
                                                 "grp",
                                                 xfce_keyboard_settings_init_grpkey,
                                                 xfce_keyboard_settings_grpkey_changed_cb);
  /* Compose key combo */
  xfce_keyboard_settings_layouts_combo_populate (settings,
                                                 "xkb_composekey_combo",
                                                 "Compose key",
                                                 xfce_keyboard_settings_init_compkey,
                                                 xfce_keyboard_settings_compkey_changed_cb);

  /* Keyboard layout/variant treeview */
  settings->priv->layout_selection_treestore = NULL;
  xkb_layout_view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (xkb_layout_view)), GTK_SELECTION_BROWSE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (xkb_layout_view), -1, _("Layout"), renderer, "text", XKB_TREE_LAYOUTS_NAMES, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (xkb_layout_view), -1, _("Variant"), renderer, "text", XKB_TREE_VARIANTS_NAMES, NULL);

  list_store = gtk_list_store_new (XKB_TREE_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (xkb_layout_view), GTK_TREE_MODEL (list_store));
  xfce_keyboard_settings_init_layout (settings);
  g_signal_connect (G_OBJECT (xkb_layout_view), "row-activated", G_CALLBACK (xfce_keyboard_settings_row_activated_cb), settings);

  /* Layout buttons */
  xkb_layout_add_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_add_button");
  xkb_layout_edit_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_edit_button");
  xkb_layout_delete_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_delete_button");
  xkb_layout_up_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_up_button");
  xkb_layout_down_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_down_button");

  g_signal_connect (G_OBJECT (xkb_layout_add_button),    "clicked", G_CALLBACK (xfce_keyboard_settings_add_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_edit_button),   "clicked", G_CALLBACK (xfce_keyboard_settings_edit_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_delete_button), "clicked", G_CALLBACK (xfce_keyboard_settings_del_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_up_button),     "clicked", G_CALLBACK (xfce_keyboard_settings_up_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_down_button),   "clicked", G_CALLBACK (xfce_keyboard_settings_down_layout_button_cb), settings);

  xfce_keyboard_settings_update_layout_buttons (settings);
#endif /* HAVE_LIBXKLAVIER */
}



static void
xfce_keyboard_settings_finalize (GObject *object)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);

#ifdef HAVE_LIBXKLAVIER
  /* Stop xklavier engine */
#ifdef HAVE_LIBXKLAVIER5
  xkl_engine_stop_listen (settings->priv->xkl_engine, XKLL_TRACK_KEYBOARD_STATE);
#else
  xkl_engine_stop_listen (settings->priv->xkl_engine);
#endif /* HAVE_LIBXKLAVIER5 */

  g_object_unref (settings->priv->xkl_rec_config);
  g_object_unref (settings->priv->xkl_registry);
  g_object_unref (settings->priv->xkl_engine);
#endif /* HAVE_LIBXKLAVIER */

  g_object_unref (G_OBJECT (settings->priv->provider));

  (*G_OBJECT_CLASS (xfce_keyboard_settings_parent_class)->finalize) (object);
}



XfceKeyboardSettings *
xfce_keyboard_settings_new (void)
{
  return g_object_new (XFCE_TYPE_KEYBOARD_SETTINGS, NULL);
}



GtkWidget *
xfce_keyboard_settings_create_dialog (XfceKeyboardSettings *settings)
{
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), NULL);
  return GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (settings), "keyboard-settings-dialog"));
}



GtkWidget *
xfce_keyboard_settings_create_plug (XfceKeyboardSettings *settings,
                                    gint                  socket_id)
{
  GtkWidget *plug;
  GObject   *child;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), NULL);

  plug = gtk_plug_new (socket_id);
  gtk_widget_show (plug);

  child = gtk_builder_get_object (GTK_BUILDER (settings), "plug-child");
  xfce_widget_reparent (GTK_WIDGET (child), plug);
  gtk_widget_show (GTK_WIDGET (child));

  return plug;
}



static void
xfce_keyboard_settings_row_activated (GtkTreeView          *tree_view,
                                      GtkTreePath          *path,
                                      GtkTreeViewColumn    *column,
                                      XfceKeyboardSettings *settings)
{
  if (column == gtk_tree_view_get_column (tree_view, SHORTCUT_COLUMN))
    xfce_keyboard_settings_edit_shortcut (settings, tree_view, path);
  else
    xfce_keyboard_settings_edit_command (settings, tree_view, path);
}



static void
xfce_keyboard_settings_initialize_shortcuts (XfceKeyboardSettings *settings)
{
  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
}



static void
_xfce_keyboard_settings_load_shortcut (XfceShortcut         *shortcut,
                                       XfceKeyboardSettings *settings)
{
  GdkModifierType  modifiers;
  GtkTreeModel    *tree_model;
  GtkTreeIter      iter;
  GObject         *tree_view;
  guint            keyval;
  gchar           *label;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (shortcut != NULL);

  DBG ("property = %s, shortcut = %s, command = %s, snotify = %s",
       shortcut->property_name, shortcut->shortcut,
       shortcut->command, shortcut->snotify ? "true" : "false");

  tree_view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  /* Get the shortcut label */
  gtk_accelerator_parse (shortcut->shortcut, &keyval, &modifiers);
  label = gtk_accelerator_get_label (keyval, modifiers);

  gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (tree_model), &iter,
                      COMMAND_COLUMN, shortcut->command,
                      SHORTCUT_COLUMN, shortcut->shortcut,
                      SNOTIFY_COLUMN, shortcut->snotify,
                      SHORTCUT_LABEL_COLUMN, label, -1);

  g_free (label);
}



static void
xfce_keyboard_settings_load_shortcuts (XfceKeyboardSettings *settings)
{
  GList *shortcuts;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (settings->priv->provider));

  /* Get all (custom) command shortcuts */
  shortcuts = xfce_shortcuts_provider_get_shortcuts (settings->priv->provider);

  /* Load shortcuts one by one */
  g_list_foreach (shortcuts, (GFunc) _xfce_keyboard_settings_load_shortcut, settings);

  xfce_shortcuts_free (shortcuts);
}



static void
xfce_keyboard_settings_edit_shortcut (XfceKeyboardSettings *settings,
                                      GtkTreeView          *tree_view,
                                      GtkTreePath          *path)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkWidget    *dialog;
  const gchar  *new_shortcut;
  gchar        *shortcut;
  gchar        *command;
  gint          response;
  gboolean      snotify;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read current shortcut from the activated row */
      gtk_tree_model_get (model, &iter,
                          COMMAND_COLUMN, &command,
                          SHORTCUT_COLUMN, &shortcut,
                          SNOTIFY_COLUMN, &snotify, -1);

      DBG ("shortcut = %s, command = %s, snotify = %s", shortcut, command, snotify ? "true" : "false");

      /* Request a new shortcut from the user */
      dialog = xfce_shortcut_dialog_new ("commands", command, command);
      g_signal_connect (dialog, "validate-shortcut", G_CALLBACK (xfce_keyboard_settings_validate_shortcut), settings);
      response = xfce_shortcut_dialog_run (XFCE_SHORTCUT_DIALOG (dialog), gtk_widget_get_toplevel (GTK_WIDGET (tree_view)));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Remove old shortcut from the settings */
          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
          xfce_shortcuts_provider_reset_shortcut (settings->priv->provider, shortcut);

          /* Get the shortcut entered by the user */
          new_shortcut = xfce_shortcut_dialog_get_shortcut (XFCE_SHORTCUT_DIALOG (dialog));

          /* Save new shortcut to the settings */
          xfce_shortcuts_provider_set_shortcut (settings->priv->provider, new_shortcut, command, snotify);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (command);
      g_free (shortcut);
    }
}



static void
xfce_keyboard_settings_edit_command (XfceKeyboardSettings *settings,
                                     GtkTreeView          *tree_view,
                                     GtkTreePath          *path)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkWidget    *dialog;
  const gchar  *new_command;
  gchar        *shortcut;
  gchar        *command;
  gint          response;
  gboolean      snotify;
  gboolean      new_snotify;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      gchar *shortcut_label;

      /* Read shortcut and current command from the activated row */
      gtk_tree_model_get (model, &iter,
                          COMMAND_COLUMN, &command,
                          SHORTCUT_COLUMN, &shortcut,
                          SHORTCUT_LABEL_COLUMN, &shortcut_label,
                          SNOTIFY_COLUMN, &snotify, -1);

      /* Request a new command from the user */
      dialog = command_dialog_new (shortcut_label, command, snotify);
      response = command_dialog_run (COMMAND_DIALOG (dialog), GTK_WIDGET (tree_view));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get the command entered by the user */
          new_command = command_dialog_get_command (COMMAND_DIALOG (dialog));
          new_snotify = command_dialog_get_snotify (COMMAND_DIALOG (dialog));

          if (g_strcmp0 (command, new_command) != 0
              || snotify != new_snotify)
            {
              /* Remove the row because we add new one from the shortcut-added signal */
              gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

              /* Save settings */
              xfce_shortcuts_provider_set_shortcut (settings->priv->provider, shortcut,
                                                    new_command, new_snotify);
            }
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (shortcut_label);
      g_free (shortcut);
      g_free (command);
    }
}



static gboolean
xfce_keyboard_settings_validate_shortcut (XfceShortcutDialog   *dialog,
                                          const gchar          *shortcut,
                                          XfceKeyboardSettings *settings)
{
  XfceKeyboardShortcutInfo *info;
  gboolean                  accepted = TRUE;
  gint                      response;

  g_return_val_if_fail (XFCE_IS_SHORTCUT_DIALOG (dialog), FALSE);
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);

  /* Ignore empty shortcuts */
  if (G_UNLIKELY (g_utf8_strlen (shortcut, -1) == 0))
    return FALSE;

  /* Ignore raw 'Return' and 'space' since that may have been used to activate the shortcut row */
  if (G_UNLIKELY (g_utf8_collate (shortcut, "Return") == 0 || g_utf8_collate (shortcut, "space") == 0))
    return FALSE;

  DBG ("Validating shortcut = %s", shortcut);

  info = xfce_keyboard_settings_get_shortcut_info (settings, shortcut);

  if (G_UNLIKELY (info != NULL))
    {
      response = xfce_shortcut_conflict_dialog (GTK_WINDOW (dialog),
                                                xfce_shortcuts_provider_get_name (settings->priv->provider),
                                                xfce_shortcuts_provider_get_name (info->provider),
                                                shortcut,
                                                xfce_shortcut_dialog_get_action_name (dialog),
                                                info->shortcut->command,
                                                FALSE);

      if (G_UNLIKELY (response == GTK_RESPONSE_ACCEPT))
        {
        /* We want to use the shortcut with the new owner */
        DBG ("We want to use %s with %s", shortcut,
             xfce_shortcut_dialog_get_action_name (dialog));
        xfce_shortcuts_provider_reset_shortcut (info->provider, shortcut);

        /*Remove the shortcut manually from the treeview */
        xfce_keyboard_settings_shortcut_removed (settings->priv->provider,
                                                 shortcut,
                                                 settings);
        }
      else
        {
          /* We want to keep the old owner */
          DBG ("We want to keep using %s with %s", shortcut, info->shortcut->command);
          accepted = FALSE;
        }

      xfce_keyboard_settings_free_shortcut_info (info);
    }

  return accepted;
}



static XfceKeyboardShortcutInfo *
xfce_keyboard_settings_get_shortcut_info (XfceKeyboardSettings *settings,
                                          const gchar          *shortcut)
{
  XfceKeyboardShortcutInfo *info = NULL;
  GList                    *iter;
  XfceShortcut             *sc;
  GList                    *providers;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);

  DBG ("Looking for shortcut info for %s", shortcut);

  providers = xfce_shortcuts_provider_get_providers ();

  if (G_UNLIKELY (providers == NULL))
    return NULL;

  for (iter = providers; iter != NULL && info == NULL; iter = g_list_next (iter))
    {
      if (G_UNLIKELY (xfce_shortcuts_provider_has_shortcut (iter->data, shortcut)))
        {
          sc = xfce_shortcuts_provider_get_shortcut (iter->data, shortcut);

          if (G_LIKELY (sc != NULL))
            {
              info = g_new0 (XfceKeyboardShortcutInfo, 1);
              info->provider = g_object_ref (iter->data);
              info->shortcut = sc;
            }
        }
    }

  xfce_shortcuts_provider_free_providers (providers);

  return info;
}



static void
xfce_keyboard_settings_free_shortcut_info (XfceKeyboardShortcutInfo *info)
{
  g_object_unref (info->provider);
  xfce_shortcut_free (info->shortcut);
  g_free (info);
}



static void
xfce_keyboard_settings_shortcut_added (XfceShortcutsProvider *provider,
                                       const gchar           *shortcut,
                                       XfceKeyboardSettings  *settings)
{
  XfceShortcut *sc;
  GtkTreeModel *model;
  GObject      *view;
  GtkTreeIter   iter;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  sc = xfce_shortcuts_provider_get_shortcut (settings->priv->provider, shortcut);

  if (G_LIKELY (sc != NULL))
    {
      GdkModifierType  modifiers;
      guint            keyval;
      gchar           *label;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);

      /* Get the shortcut label */
      gtk_accelerator_parse (sc->shortcut, &keyval, &modifiers);
      label = gtk_accelerator_get_label (keyval, modifiers);

      DBG ("Add shortcut %s for command %s", shortcut, sc->command);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          SHORTCUT_COLUMN, shortcut,
                          COMMAND_COLUMN, sc->command,
                          SNOTIFY_COLUMN, sc->snotify,
                          SHORTCUT_LABEL_COLUMN, label, -1);

      g_free (label);

      xfce_shortcut_free (sc);
    }
}




static gboolean
_xfce_keyboard_settings_remove_shortcut (GtkTreeModel *model,
                                         GtkTreePath  *path,
                                         GtkTreeIter  *iter,
                                         const gchar  *shortcut)
{
  gboolean finished = FALSE;
  gchar   *row_shortcut;

  gtk_tree_model_get (model, iter, SHORTCUT_COLUMN, &row_shortcut, -1);

  if (G_UNLIKELY (g_utf8_collate (row_shortcut, shortcut) == 0))
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), iter);
      finished = TRUE;
    }

  g_free (row_shortcut);

  return finished;
}



static void
xfce_keyboard_settings_shortcut_removed (XfceShortcutsProvider *provider,
                                         const gchar           *shortcut,
                                         XfceKeyboardSettings  *settings)
{
  GtkTreeModel *model;
  GObject      *view;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  DBG ("Remove shortcut %s from treeview", shortcut);

  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) _xfce_keyboard_settings_remove_shortcut,
                          (gpointer) shortcut);
}



static void
xfce_keyboard_settings_add_button_clicked (XfceKeyboardSettings *settings,
                                           GtkButton            *button)
{
  GtkWidget   *command_dialog;
  GtkWidget   *shortcut_dialog;
  GObject      *parent;
  const gchar *shortcut;
  const gchar *command;
  gint         response;
  gboolean     snotify;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  /* Request a command from the user */
  command_dialog = command_dialog_new (NULL, NULL, FALSE);
  response = command_dialog_run (COMMAND_DIALOG (command_dialog), GTK_WIDGET (button));

  /* Abort if the dialog was cancelled */
  if (G_UNLIKELY (response == GTK_RESPONSE_OK))
    {
      /* Get the command */
      command = command_dialog_get_command (COMMAND_DIALOG (command_dialog));
      snotify = command_dialog_get_snotify (COMMAND_DIALOG (command_dialog));

      /* Hide the command dialog */
      gtk_widget_hide (command_dialog);

      /* Create shortcut dialog */
      shortcut_dialog = xfce_shortcut_dialog_new ("commands", command, command);
      g_signal_connect (shortcut_dialog, "validate-shortcut", G_CALLBACK (xfce_keyboard_settings_validate_shortcut), settings);

      /* Run shortcut dialog until a valid shortcut is entered or the dialog is cancelled */
      parent = gtk_builder_get_object (GTK_BUILDER (settings), "keyboard-shortcuts-dialog");
      response = xfce_shortcut_dialog_run (XFCE_SHORTCUT_DIALOG (shortcut_dialog), GTK_WIDGET (parent));

      /* Only continue if the shortcut dialog succeeded */
      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get shortcut */
          shortcut = xfce_shortcut_dialog_get_shortcut (XFCE_SHORTCUT_DIALOG (shortcut_dialog));

          /* Save the new shortcut to xfconf */
          DBG ("Save shortcut %s with command %s to Xfconf", shortcut, command);
          xfce_shortcuts_provider_set_shortcut (settings->priv->provider, shortcut, command, snotify);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (shortcut_dialog);
    }

  /* Destroy the shortcut dialog */
  gtk_widget_destroy (command_dialog);
}



static void
xfce_keyboard_settings_edit_button_clicked (XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GObject          *view;
  GList            *rows;
  GList            *row_iter;
  GList            *row_references = NULL;

  DBG ("edit!");

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (row_iter = g_list_first (rows); row_iter != NULL; row_iter = g_list_next (row_iter))
    row_references = g_list_append (row_references, gtk_tree_row_reference_new (model, row_iter->data));

  for (row_iter = g_list_first (row_references); row_iter != NULL; row_iter = g_list_next (row_iter))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (row_iter->data);

      /* Conver tree path to tree iter */
      if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
        {
          GtkWidget *command_dialog;
          gboolean  snotify;
          gchar    *shortcut_label;
          gchar    *shortcut;
          gchar    *command;
          gint      response;

          /* Read row values */
          gtk_tree_model_get (model, &iter,
                              SHORTCUT_LABEL_COLUMN, &shortcut_label,
                              SHORTCUT_COLUMN, &shortcut,
                              COMMAND_COLUMN, &command,
                              SNOTIFY_COLUMN, &snotify,
                              -1);

          DBG ("Edit shortcut %s / command %s", shortcut, command);

          /* Request a new command from the user */
          command_dialog = command_dialog_new (shortcut_label, command, snotify);
          response = command_dialog_run (COMMAND_DIALOG (command_dialog), GTK_WIDGET (view));

          /* Abort if the dialog was cancelled */
          if (G_UNLIKELY (response == GTK_RESPONSE_OK))
            {
              const gchar *new_command;
              GtkWidget   *shortcut_dialog;
              gboolean     new_snotify;
              GObject     *parent;

              /* Get the command */
              new_command = command_dialog_get_command (COMMAND_DIALOG (command_dialog));
              new_snotify = command_dialog_get_snotify (COMMAND_DIALOG (command_dialog));

              /* Hide the command dialog */
              gtk_widget_hide (command_dialog);

              /* Create shortcut dialog */
              shortcut_dialog =
                xfce_shortcut_dialog_new ("commands",
                                          new_command,
                                          new_command);

              g_signal_connect (shortcut_dialog, "validate-shortcut",
                                G_CALLBACK (xfce_keyboard_settings_validate_shortcut),
                                settings);

              /* Try to keep the window above as it grabs the keyboard, we don't
               * want users to wonder why the keyboard does not work in another
               * window */
              gtk_window_set_keep_above (GTK_WINDOW (shortcut_dialog), TRUE);

              /* Run shortcut dialog until a valid shortcut is entered or the dialog is cancelled */
              parent =
                gtk_builder_get_object (GTK_BUILDER (settings),
                                        "keyboard-shortcuts-dialog");
              response =
                xfce_shortcut_dialog_run (XFCE_SHORTCUT_DIALOG (shortcut_dialog),
                                          GTK_WIDGET (parent));

              /* Only continue if the shortcut dialog succeeded */
              if (G_LIKELY (response == GTK_RESPONSE_OK))
                {
                  const gchar *new_shortcut;
                  gboolean     test_new_shortcut;

                  /* Get shortcut */
                  new_shortcut =
                    xfce_shortcut_dialog_get_shortcut (XFCE_SHORTCUT_DIALOG (shortcut_dialog));
                  test_new_shortcut = (g_strcmp0 (shortcut, new_shortcut) != 0);
                  if (g_strcmp0 (command, new_command) != 0 || (test_new_shortcut) || snotify != new_snotify)
                    {
                      if (test_new_shortcut)
                        {
                          /* Remove the row because we add new one from the
                           * shortcut-added signal */
                          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

                          /* Remove old keyboard shortcut via xfconf */
                          xfce_shortcuts_provider_reset_shortcut (settings->priv->provider,
                                                                  shortcut);
                        }

                      /* Save settings */
                      xfce_shortcuts_provider_set_shortcut (settings->priv->provider,
                                                            new_shortcut,
                                                            new_command,
                                                            new_snotify);
                    }

                }

              /* Destroy the shortcut dialog */
              gtk_widget_destroy (shortcut_dialog);
            }

          g_free (shortcut_label);
          g_free (shortcut);
          g_free (command);
          gtk_widget_destroy (command_dialog);
        }

      gtk_tree_path_free (path);
    }

  /* Free row reference list */
  g_list_foreach (row_references, (GFunc) (void (*)(void)) gtk_tree_row_reference_free, NULL);
  g_list_free (row_references);

  /* Free row list */
  g_list_foreach (rows, (GFunc) (void (*)(void)) gtk_tree_path_free, NULL);
  g_list_free (rows);
}



static void
xfce_keyboard_settings_delete_button_clicked (XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  GObject          *view;
  GList            *rows;
  GList            *row_iter;
  GList            *row_references = NULL;
  gchar            *shortcut;

  DBG ("remove!");

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (row_iter = g_list_first (rows); row_iter != NULL; row_iter = g_list_next (row_iter))
    row_references = g_list_append (row_references, gtk_tree_row_reference_new (model, row_iter->data));

  for (row_iter = g_list_first (row_references); row_iter != NULL; row_iter = g_list_next (row_iter))
    {
      path = gtk_tree_row_reference_get_path (row_iter->data);

      /* Conver tree path to tree iter */
      if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
        {
          /* Read row values */
          gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &shortcut, -1);

          DBG ("remove shortcut %s", shortcut);

          /* Remove keyboard shortcut via xfconf */
          xfce_shortcuts_provider_reset_shortcut (settings->priv->provider, shortcut);

          /* Free strings */
          g_free (shortcut);
        }

      gtk_tree_path_free (path);
    }

  /* Free row reference list */
  g_list_foreach (row_references, (GFunc) (void (*)(void)) gtk_tree_row_reference_free, NULL);
  g_list_free (row_references);

  /* Free row list */
  g_list_foreach (rows, (GFunc) (void (*)(void)) gtk_tree_path_free, NULL);
  g_list_free (rows);
}



static void
xfce_keyboard_settings_reset_button_clicked (XfceKeyboardSettings *settings)
{
  gint          response;
  GObject      *view;
  GtkListStore *store;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  response = xfce_message_dialog (NULL, _("Reset to Defaults"), "dialog-question",
                                  _("Reset to Defaults"),
                                  _("This will reset all shortcuts to their default "
                                    "values. Do you really want to do this?"),
                                  _("No"), GTK_RESPONSE_NO,
                                  _("Yes"), GTK_RESPONSE_YES,
                                  NULL);

  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");

      /* Clear out all the previous entries */
      store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
      gtk_list_store_clear (store);

      xfce_shortcuts_provider_reset_to_defaults (settings->priv->provider);
    }
}



#ifdef HAVE_LIBXKLAVIER

static gchar *
xfce_keyboard_settings_xkb_description (XklConfigItem *config_item)
{
  gchar *ci_description;
  gchar *description;

  ci_description = g_strstrip (config_item->description);

  if (ci_description[0] == 0)
    description = g_strdup (config_item->name);
  else
    description = g_strdup (ci_description);

  return description;
}



static gboolean
xfce_keyboard_settings_update_sensitive (GtkSwitch *widget, XfceKeyboardSettings *settings)
{
  GObject  *xkb_model_frame;
  GObject  *xkb_layout_frame;
  GObject  *xkb_grpkey_frame;
  GObject  *xkb_compkey_frame;
  gboolean  active;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);

  active = gtk_switch_get_active (widget);
  xkb_model_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_model_frame");
  xkb_layout_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_frame");
  xkb_grpkey_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_grpkey_frame");
  xkb_compkey_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_composekey_frame");

  gtk_widget_set_sensitive (GTK_WIDGET (xkb_model_frame), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (xkb_layout_frame), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (xkb_grpkey_frame), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (xkb_compkey_frame), !active);

  return active;
}



static void
xfce_keyboard_settings_system_default_cb (GtkSwitch *widget, gboolean state, XfceKeyboardSettings *settings)
{
  GtkWidget *warning_dialog;
  gboolean   use_system_defaults;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  use_system_defaults = xfce_keyboard_settings_update_sensitive (widget, settings);
  if (use_system_defaults)
    {
      warning_dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                               _("The system defaults will be "
                                                 "restored next time you log in."));
      gtk_window_set_title (GTK_WINDOW (warning_dialog), _("Warning"));
      gtk_dialog_run (GTK_DIALOG (warning_dialog));
      gtk_widget_destroy (warning_dialog);
    }
}



static void
xfce_keyboard_settings_set_layout (XfceKeyboardSettings *settings)
{
  GObject          *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *val_layout;
  gchar            *val_variant;
  gchar            *variants;
  gchar            *layouts;
  gchar            *tmp;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;
  gtk_tree_model_get (model, &iter,
                      XKB_TREE_LAYOUTS, &val_layout,
                      XKB_TREE_VARIANTS, &val_variant, -1);
  if (val_variant == NULL)
      val_variant = g_strdup ("");

  if (val_layout)
    {
      layouts = g_strdup (val_layout);
      g_free (val_layout);

      variants = g_strdup (val_variant);
      g_free (val_variant);
    }
  else
    {
      layouts = g_strdup ("");
      /* If the layout was NULL, we ignore the variant */
      variants = g_strdup ("");
      g_free (val_variant);
    }

  while (gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          XKB_TREE_LAYOUTS, &val_layout,
                          XKB_TREE_VARIANTS, &val_variant, -1);
      if (val_variant == NULL)
          val_variant = g_strdup ("");

      if (val_layout)
        {
          tmp = g_strconcat (layouts, ",", val_layout, NULL);
          g_free (val_layout);
          g_free (layouts);
          layouts = tmp;

          tmp = g_strconcat (variants, ",", val_variant, NULL);
          g_free (val_variant);
          g_free (variants);
          variants = tmp;
        }
      else
        {
          g_free (val_layout);
          g_free (val_variant);
        }
    }

  xfconf_channel_set_string (settings->priv->keyboard_layout_channel,
                             "/Default/XkbLayout", layouts);
  xfconf_channel_set_string (settings->priv->keyboard_layout_channel,
                             "/Default/XkbVariant", variants);

  g_free (layouts);
  g_free (variants);
}



static void
xfce_keyboard_settings_init_layout (XfceKeyboardSettings *settings)
{

  XklState         *xkl_state = NULL;
  GObject          *view;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *val_layout;
  gchar            *val_variant;
  gchar            *default_layouts;
  gchar            *default_variants;
  gchar           **layouts;
  gchar           **layout;
  gchar           **variants;
  gchar           **variant;
  gint              current_group = -1;
  gint              group_id;

  default_layouts  = g_strjoinv (",", settings->priv->xkl_rec_config->layouts);
  default_variants = g_strjoinv (",", settings->priv->xkl_rec_config->variants);

  val_layout  = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbLayout",  default_layouts);
  val_variant = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbVariant",  default_variants);

  layouts = g_strsplit (val_layout, ",", 0);
  variants = g_strsplit (val_variant, ",", 0);

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  xkl_state = xkl_engine_get_current_state (settings->priv->xkl_engine);
  if (xkl_state != NULL)
    current_group = xkl_state->group;

  for (layout = layouts, variant = variants, group_id = 0; *layout != NULL; ++layout, ++group_id)
    {
      XklConfigItem *litem;
      XklConfigItem *vitem;
      gchar         *layout_desc;
      gchar         *variant_desc;

      litem = xkl_config_item_new ();
      vitem = xkl_config_item_new ();

      g_snprintf (litem->name, sizeof litem->name, "%s", *layout);
      g_snprintf (vitem->name, sizeof vitem->name, "%s", *variant);

      if (xkl_config_registry_find_layout (settings->priv->xkl_registry, litem))
        layout_desc = litem->description;
      else
        layout_desc = *layout;

      if (xkl_config_registry_find_variant (settings->priv->xkl_registry, *layout, vitem))
        variant_desc = vitem->description;
      else
        variant_desc = *variant;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, XKB_TREE_LAYOUTS, *layout,
                                                         XKB_TREE_LAYOUTS_NAMES, layout_desc,
                                                         XKB_TREE_VARIANTS, *variant,
                                                         XKB_TREE_VARIANTS_NAMES, variant_desc,
                                                         -1);
      if (current_group == group_id)
        gtk_tree_selection_select_iter (selection, &iter);

      if (*variant)
        variant++;

      g_object_unref (litem);
      g_object_unref (vitem);
    }

  g_strfreev (layouts);
  g_strfreev (variants);
  g_free (default_layouts);
  g_free (default_variants);
  g_free (val_layout);
  g_free (val_variant);
}



static void
xfce_keyboard_settings_layouts_combo_add (XklConfigRegistry    *config_registry,
                                          const XklConfigItem  *config_item,
                                          gpointer              user_data)
{
  GtkListStore *store = GTK_LIST_STORE (user_data);
  GtkTreeIter   iter;
  gchar        *description;

  description = xfce_keyboard_settings_xkb_description ((XklConfigItem *) config_item);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      XKB_LAYOUTS_COMBO_DESCRIPTION, description,
                      XKB_LAYOUTS_COMBO_VALUE, config_item->name, -1);
  g_free (description);
}

static void
xfce_keyboard_settings_layouts_combo_init (XfceKeyboardSettings *settings,
                                           const gchar *combo_name,
                                           const gchar *xfconf_prop_name,
                                           const gchar *default_value)
{
  GObject      *view;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *id;
  gchar        *xfconf_prop_value;
  gboolean      item;
  gboolean      found = FALSE;

  view = gtk_builder_get_object (GTK_BUILDER (settings), combo_name);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));

  xfconf_prop_value = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, xfconf_prop_name, default_value);
  item = gtk_tree_model_get_iter_first (model, &iter);

  if (xfconf_prop_value == NULL || *xfconf_prop_value == 0)
  {
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (view), &iter);
    return;
  }

  while (item && !found)
    {
      gtk_tree_model_get (model, &iter, XKB_LAYOUTS_COMBO_VALUE, &id, -1);
      found = !strcmp (id, xfconf_prop_value);
      g_free (id);

      if (found)
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (view), &iter);
          break;
        }
      item = gtk_tree_model_iter_next (model, &iter);
    }
  g_free (xfconf_prop_value);
}

static void
xfce_keyboard_settings_init_model (XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_init (settings,
                                             "xkb_model_combo",
                                             "/Default/XkbModel",
                                             settings->priv->xkl_rec_config->model);
}



static void
xfce_keyboard_settings_init_grpkey (XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_init (settings,
                                             "xkb_grpkey_combo",
                                             "/Default/XkbOptions/Group",
                                             NULL);
}



static void
xfce_keyboard_settings_init_compkey (XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_init (settings,
                                             "xkb_composekey_combo",
                                             "/Default/XkbOptions/Compose",
                                             NULL);
}



static void
xfce_keyboard_settings_row_activated_cb (GtkTreeView          *tree_view,
                                         GtkTreePath          *path,
                                         GtkTreeViewColumn    *column,
                                         XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_edit_layout_button_cb (NULL, settings);
}



static void
xfce_keyboard_settings_layouts_combo_changed (GtkComboBox          *combo,
                                              XfceKeyboardSettings *settings,
                                              const gchar *xfconf_prop_name)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *xfconf_prop_value;

  if (G_LIKELY(gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)))
  {
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
    gtk_tree_model_get (model, &iter, XKB_LAYOUTS_COMBO_VALUE, &xfconf_prop_value, -1);
    xfconf_channel_set_string (settings->priv->keyboard_layout_channel,
                               xfconf_prop_name, xfconf_prop_value);
    g_free (xfconf_prop_value);
  }
}


static void
xfce_keyboard_settings_model_changed_cb (GtkComboBox          *combo,
                                         XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_changed (combo, settings,
                                                "/Default/XkbModel");
}



static void
xfce_keyboard_settings_grpkey_changed_cb (GtkComboBox          *combo,
                                          XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_changed (combo, settings,
                                                "/Default/XkbOptions/Group");
}



static void
xfce_keyboard_settings_compkey_changed_cb (GtkComboBox          *combo,
                                           XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_changed (combo, settings,
                                                "/Default/XkbOptions/Compose");
}



static void
xfce_keyboard_settings_update_layout_buttons (XfceKeyboardSettings *settings)
{
  GObject      *view;
  GtkTreeModel *model;
  GObject      *object;
  gint          n_layouts;
  gint          max_layouts;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  n_layouts = gtk_tree_model_iter_n_children (model, NULL);
  max_layouts = xkl_engine_get_max_num_groups (settings->priv->xkl_engine);

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_add_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts < max_layouts));

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_delete_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts > 1));

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_up_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts > 1));

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_down_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts > 1));
}



static void
xfce_keyboard_settings_edit_layout_button_cb (GtkWidget            *widget,
                                              XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GObject          *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *current_layout;
  gchar            *current_variant;
  gchar           **layout_selection;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (G_LIKELY (gtk_tree_selection_get_selected (selection, &model, &iter)))
  {
    gtk_tree_model_get (model, &iter,
                        XKB_TREE_LAYOUTS, &current_layout,
                        XKB_TREE_VARIANTS, &current_variant,
                        -1);

     layout_selection =
        xfce_keyboard_settings_layout_selection (settings, current_layout, current_variant);
    if (layout_selection)
      {
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            XKB_TREE_LAYOUTS, layout_selection[0],
                            XKB_TREE_LAYOUTS_NAMES, layout_selection[1],
                            XKB_TREE_VARIANTS, layout_selection[2],
                            XKB_TREE_VARIANTS_NAMES, layout_selection[3],
                            -1);
        xfce_keyboard_settings_set_layout (settings);
        g_strfreev (layout_selection);
      }
    g_free (current_layout);
    g_free (current_variant);
  }
}



static void
xfce_keyboard_settings_add_layout_button_cb (GtkWidget            *widget,
                                             XfceKeyboardSettings *settings)
{
  GObject          *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar           **layout_selection;

  layout_selection = xfce_keyboard_settings_layout_selection (settings, NULL, NULL);
  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  if (layout_selection)
    {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          XKB_TREE_LAYOUTS, layout_selection[0],
                          XKB_TREE_LAYOUTS_NAMES, layout_selection[1],
                          XKB_TREE_VARIANTS, layout_selection[2],
                          XKB_TREE_VARIANTS_NAMES, layout_selection[3],
                          -1);
      xfce_keyboard_settings_update_layout_buttons (settings);
      xfce_keyboard_settings_set_layout (settings);
      g_strfreev (layout_selection);
    }
}



static void
xfce_keyboard_settings_del_layout_button_cb (GtkWidget            *widget,
                                             XfceKeyboardSettings *settings)
{
  GObject          *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      GtkTreeIter iter2;

      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      if (gtk_tree_model_get_iter_first (model, &iter2))
        gtk_tree_selection_select_iter (selection, &iter2);

      xfce_keyboard_settings_update_layout_buttons (settings);
      xfce_keyboard_settings_set_layout (settings);
    }
}



static void
xfce_keyboard_settings_up_layout_button_cb (GtkWidget *widget,
                                            XfceKeyboardSettings *settings)
{
    xfce_keyboard_settings_layout_move (widget, settings, MOVE_LAYOUT_UP);
}



static void
xfce_keyboard_settings_down_layout_button_cb (GtkWidget *widget,
                                              XfceKeyboardSettings *settings)
{
    xfce_keyboard_settings_layout_move (widget, settings, MOVE_LAYOUT_DOWN);
}



static void
xfce_keyboard_settings_layout_move (GtkWidget *widget,
                                    XfceKeyboardSettings *settings,
                                    MOVE_LAYOUT_DIRECTION direction)
{
  GObject          *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter_a, iter_b;
  GtkTreeSelection *selection;
  GtkTreePath      *path;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  g_return_if_fail (GTK_IS_WIDGET (view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter_a));

  switch (direction)
  {
  case MOVE_LAYOUT_UP:
      path = gtk_tree_model_get_path (model, &iter_a);
      if (!gtk_tree_path_prev (path))
      {
          gtk_tree_path_free (path);
          return;
      }
      gtk_tree_model_get_iter (model, &iter_b, path);
      gtk_tree_path_free (path);
      break;

  case MOVE_LAYOUT_DOWN:
      iter_b = iter_a;
      if (!gtk_tree_model_iter_next (model, &iter_b))
          return;
      break;

  default:
      return;
  }

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter_a, &iter_b);
  xfce_keyboard_settings_set_layout (settings);
}



static void
xfce_keyboard_settings_add_variant_to_list (XklConfigRegistry    *config_registry,
                                            XklConfigItem        *config_item,
                                            XfceKeyboardSettings *settings)
{
  GtkTreeStore *treestore;
  GtkTreeIter   iter;
  GObject      *treeview;
  gchar        *variant;

  variant = xfce_keyboard_settings_xkb_description (config_item);
  treeview = gtk_builder_get_object (GTK_BUILDER (settings), "layout_selection_view");
  treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
  gtk_tree_store_append (treestore, &iter, &settings->priv->layout_selection_iter);
  gtk_tree_store_set (treestore, &iter,
                      XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, variant,
                      XKB_AVAIL_LAYOUTS_TREE_ID, config_item->name, -1);
  g_free (variant);
}



static void
xfce_keyboard_settings_add_layout_to_list (XklConfigRegistry    *config_registry,
                                           XklConfigItem        *config_item,
                                           XfceKeyboardSettings *settings)
{
  GtkTreeStore *treestore;
  GObject      *treeview;
  gchar        *layout;

  layout = xfce_keyboard_settings_xkb_description (config_item);
  treeview = gtk_builder_get_object (GTK_BUILDER (settings), "layout_selection_view");
  treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
  gtk_tree_store_append (treestore, &settings->priv->layout_selection_iter, NULL);
  gtk_tree_store_set (treestore, &settings->priv->layout_selection_iter,
                      XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, layout,
                      XKB_AVAIL_LAYOUTS_TREE_ID, config_item->name, -1);
  g_free (layout);

  xkl_config_registry_foreach_layout_variant (config_registry, config_item->name,
      (ConfigItemProcessFunc)xfce_keyboard_settings_add_variant_to_list, settings);
}



static void
xfce_keyboard_settings_layout_activate_cb (GtkTreeView       *tree_view,
                                           GtkTreePath       *path,
                                           GtkTreeViewColumn *column,
                                           GtkDialog         *dialog)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}



static gchar**
xfce_keyboard_settings_layout_selection (XfceKeyboardSettings *settings,
                                         const gchar          *edit_layout,
                                         const gchar          *edit_variant)
{
  GObject           *keyboard_layout_selection_dialog;
  GObject           *layout_selection_view;
  GtkTreePath       *path;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *model;
  GtkTreeIter        iter;
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  gchar            **val_layout;
  gchar             *layout_desc;
  gchar             *variant_desc;
  gchar             *layout;
  gchar             *variant;
  gint               result;

  keyboard_layout_selection_dialog = gtk_builder_get_object (GTK_BUILDER (settings), "keyboard-layout-selection-dialog");
  layout_selection_view = gtk_builder_get_object (GTK_BUILDER (settings), "layout_selection_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (layout_selection_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  if (!settings->priv->layout_selection_treestore)
    {
      settings->priv->layout_selection_treestore = gtk_tree_store_new (XKB_AVAIL_LAYOUTS_TREE_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
      renderer = gtk_cell_renderer_text_new ();
      column   = gtk_tree_view_column_new_with_attributes (NULL, renderer, "text",
                                        XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (layout_selection_view), GTK_TREE_MODEL (settings->priv->layout_selection_treestore));
      gtk_tree_view_append_column (GTK_TREE_VIEW (layout_selection_view), column);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (settings->priv->layout_selection_treestore), 0, GTK_SORT_ASCENDING);
      xkl_config_registry_foreach_layout (settings->priv->xkl_registry,
          (ConfigItemProcessFunc) xfce_keyboard_settings_add_layout_to_list, settings);
      g_signal_connect (GTK_TREE_VIEW (layout_selection_view), "row-activated", G_CALLBACK (xfce_keyboard_settings_layout_activate_cb), keyboard_layout_selection_dialog);
      gtk_dialog_set_default_response (GTK_DIALOG (keyboard_layout_selection_dialog), GTK_RESPONSE_OK);
    }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (layout_selection_view));
  gtk_tree_view_collapse_all (GTK_TREE_VIEW (layout_selection_view));

  /* Selected and expand the layout/variant to be edited */
  if (edit_layout && g_strcmp0 (edit_layout, ""))
    {
      gboolean found;

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          gchar *tmp_layout;
          found = FALSE;

          do
            {
              gtk_tree_model_get (model, &iter, XKB_AVAIL_LAYOUTS_TREE_ID, &tmp_layout, -1);
              path = gtk_tree_model_get_path (model, &iter);

              if (found)
                break;

              if (g_strcmp0 (tmp_layout, edit_layout) == 0 )
                {
                  if (edit_variant && g_strcmp0 (edit_variant, "") && gtk_tree_model_iter_has_child (model, &iter))
                    {
                      GtkTreeIter iter2;
                      gint n, i;

                      n = gtk_tree_model_iter_n_children (model, &iter);

                      for (i = 0; i < n; i ++)
                        {
                          if (gtk_tree_model_iter_nth_child (model, &iter2, &iter, i))
                            {
                              gchar *tmp_variant;

                              gtk_tree_model_get (model, &iter2, XKB_AVAIL_LAYOUTS_TREE_ID, &tmp_variant, -1);

                              if (g_strcmp0 (tmp_variant, edit_variant) == 0)
                                {
                                  GtkTreePath *path2;

                                  path2 = gtk_tree_model_get_path (model, &iter2);

                                  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                                                               path2, NULL,
                                                                               TRUE, 0.5, 0);
                                  gtk_tree_view_expand_row (GTK_TREE_VIEW (layout_selection_view),
                                                            path,
                                                            TRUE);
                                  gtk_tree_selection_select_iter (selection, &iter2);

                                  found = TRUE;
                                  g_free (tmp_variant);
                                  gtk_tree_path_free (path2);
                                  break;
                                }

                              g_free (tmp_variant);
                            }
                        }
                    }
                else
                  {
                    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                                                 path, NULL,
                                                                 TRUE, 0.5, 0);
                    gtk_tree_selection_select_iter (selection, &iter);
                    found = TRUE;
                    break;
                  }
              }

              gtk_tree_path_free (path);
              path = NULL;
              g_free (tmp_layout);
              tmp_layout = NULL;
            }
          while (gtk_tree_model_iter_next (model, &iter));
          g_free (tmp_layout);
          if (path)
            gtk_tree_path_free (path);

          if (!found)
            {
              /* We did not find the iter to be edited, fallback to the first one */
              if (gtk_tree_model_get_iter_first (model, &iter))
                {
                  path = gtk_tree_model_get_path (model, &iter);
                  gtk_tree_selection_select_iter (selection, &iter);

                  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                                path, NULL,
                                                TRUE, 0.5, 0);
                  gtk_tree_path_free (path);
                }
            }
        }
    }
  else
    {
      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          path = gtk_tree_model_get_path (model, &iter);
          gtk_tree_selection_select_iter (selection, &iter);

          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                        path, NULL,
                                        TRUE, 0.5, 0);
          gtk_tree_path_free (path);
        }
    }

  val_layout = NULL;
  gtk_widget_show (GTK_WIDGET (keyboard_layout_selection_dialog));
  result = gtk_dialog_run (GTK_DIALOG (keyboard_layout_selection_dialog));
  if (result == GTK_RESPONSE_OK)
    {
      if (G_LIKELY (gtk_tree_selection_get_selected (selection, &model, &iter)))
      {
        gtk_tree_model_get (model, &iter, XKB_AVAIL_LAYOUTS_TREE_ID, &layout,
                                          XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, &layout_desc, -1);

        path = gtk_tree_model_get_path (model, &iter);
        if (gtk_tree_path_get_depth (path) == 1)
          {
            variant = g_strdup ("");
            variant_desc = g_strdup ("");
          }
        else
          {
            variant = layout;
            variant_desc = layout_desc;
            gtk_tree_path_up (path);
            if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
              {
                gtk_tree_model_get (model, &iter, XKB_AVAIL_LAYOUTS_TREE_ID, &layout,
                                                  XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, &layout_desc, -1);
              }
          }

        val_layout = g_new0 (typeof (gchar*), 5);
        val_layout[0] = layout;
        val_layout[1] = layout_desc;
        val_layout[2] = variant;
        val_layout[3] = variant_desc;
        val_layout[4] = NULL;

        gtk_tree_path_free (path);
      }
    }

  gtk_widget_hide (GTK_WIDGET (keyboard_layout_selection_dialog));
  return val_layout;
}

#endif /* HAVE_LIBXKLAVIER */
