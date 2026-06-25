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

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <garcon/garcon.h>
#include <garcon-gtk/garcon-gtk.h>
#include <xfconf/xfconf.h>

#include <libxfce4panel/libxfce4panel.h>
#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>

#include "launcher.h"
#include "launcher-dialog.h"

#define ARROW_BUTTON_SIZE              (12)
#define MENU_POPUP_DELAY               (225)
#define NO_ARROW_INSIDE_BUTTON(plugin) ((plugin)->arrow_position != LAUNCHER_ARROW_INTERNAL \
                                        || LIST_HAS_ONE_OR_NO_ENTRIES ((plugin)->items))
#define ARROW_INSIDE_BUTTON(plugin)    (!NO_ARROW_INSIDE_BUTTON (plugin))
#define RELATIVE_CONFIG_PATH           PANEL_PLUGIN_RELATIVE_PATH G_DIR_SEPARATOR_S "%s-%d"



static void               launcher_plugin_get_property                  (GObject              *object,
                                                                         guint                 prop_id,
                                                                         GValue               *value,
                                                                         GParamSpec           *pspec);
static void               launcher_plugin_set_property                  (GObject              *object,
                                                                         guint                 prop_id,
                                                                         const GValue         *value,
                                                                         GParamSpec           *pspec);
static void               launcher_plugin_construct                     (XfcePanelPlugin      *panel_plugin);
static void               launcher_plugin_free_data                     (XfcePanelPlugin      *panel_plugin);
static void               launcher_plugin_removed                       (XfcePanelPlugin      *panel_plugin);
static gboolean           launcher_plugin_remote_event                  (XfcePanelPlugin      *panel_plugin,
                                                                         const gchar          *name,
                                                                         const GValue         *value);
static gboolean           launcher_plugin_save_delayed_timeout          (gpointer              user_data);
static void               launcher_plugin_save_delayed                  (LauncherPlugin       *plugin);
static void               launcher_plugin_mode_changed                  (XfcePanelPlugin      *panel_plugin,
                                                                         XfcePanelPluginMode   mode);
static gboolean           launcher_plugin_size_changed                  (XfcePanelPlugin      *panel_plugin,
                                                                         gint                  size);
static void               launcher_plugin_configure_plugin              (XfcePanelPlugin      *panel_plugin);
static void               launcher_plugin_screen_position_changed       (XfcePanelPlugin      *panel_plugin,
                                                                         XfceScreenPosition    position);
static LauncherArrowType  launcher_plugin_default_arrow_type            (LauncherPlugin       *plugin);
static void               launcher_plugin_pack_widgets                  (LauncherPlugin       *plugin);
static void               launcher_plugin_tooltip_icon_invalidate       (GObject              *object);
static void               launcher_plugin_icon_invalidate               (LauncherPlugin       *plugin);
static void               launcher_plugin_menu_deactivate               (GtkWidget            *menu,
                                                                         LauncherPlugin       *plugin);
static void               launcher_plugin_menu_item_activate            (GtkMenuItem          *widget,
                                                                         GarconMenuItem       *item);
static void               launcher_plugin_menu_item_drag_data_received  (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         GtkSelectionData     *data,
                                                                         guint                 info,
                                                                         guint                 drag_time,
                                                                         GarconMenuItem       *item);
static void               launcher_plugin_menu_construct                (LauncherPlugin       *plugin);
static void               launcher_plugin_menu_popup_destroyed          (gpointer              user_data);
static gboolean           launcher_plugin_menu_popup                    (gpointer              user_data);
static void               launcher_plugin_menu_destroy                  (LauncherPlugin       *plugin);
static void               launcher_plugin_button_update                 (LauncherPlugin       *plugin);
static void               launcher_plugin_button_update_action_menu     (LauncherPlugin       *plugin);
static void               launcher_plugin_button_state_changed          (GtkWidget            *button_a,
                                                                         GtkStateFlags         state,
                                                                         GtkWidget            *button_b);
static gboolean           launcher_plugin_button_press_event            (GtkWidget            *button,
                                                                         GdkEventButton       *event,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_button_release_event          (GtkWidget            *button,
                                                                         GdkEventButton       *event,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_button_query_tooltip          (GtkWidget            *widget,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         gboolean              keyboard_mode,
                                                                         GtkTooltip           *tooltip,
                                                                         LauncherPlugin       *plugin);
static void               launcher_plugin_button_drag_data_received     (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         GtkSelectionData     *selection_data,
                                                                         guint                 info,
                                                                         guint                 drag_time,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_button_drag_motion            (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         guint                 drag_time,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_button_drag_drop              (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         guint                 drag_time,
                                                                         LauncherPlugin       *plugin);
static void               launcher_plugin_button_drag_leave             (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         guint                 drag_time,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_button_draw                   (GtkWidget            *widget,
                                                                         cairo_t              *cr,
                                                                         LauncherPlugin       *plugin);
static void               launcher_plugin_arrow_visibility              (LauncherPlugin       *plugin);
static gboolean           launcher_plugin_arrow_press_event             (GtkWidget            *button,
                                                                         GdkEventButton       *event,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_arrow_drag_motion             (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         guint                 drag_time,
                                                                         LauncherPlugin       *plugin);
static void               launcher_plugin_arrow_drag_leave              (GtkWidget            *widget,
                                                                         GdkDragContext       *context,
                                                                         guint                 drag_time,
                                                                         LauncherPlugin       *plugin);
static gboolean           launcher_plugin_item_query_tooltip            (GtkWidget            *widget,
                                                                         gint                  x,
                                                                         gint                  y,
                                                                         gboolean              keyboard_mode,
                                                                         GtkTooltip           *tooltip,
                                                                         GarconMenuItem       *item);
static gboolean           launcher_plugin_item_exec_on_screen           (GarconMenuItem       *item,
                                                                         guint32               event_time,
                                                                         GdkScreen            *screen,
                                                                         GSList               *uri_list);
static void               launcher_plugin_item_exec                     (GarconMenuItem       *item,
                                                                         guint32               event_time,
                                                                         GdkScreen            *screen,
                                                                         GSList               *uri_list);
static void               launcher_plugin_item_exec_from_clipboard      (GarconMenuItem       *item,
                                                                         guint32               event_time,
                                                                         GdkScreen            *screen);
static GSList            *launcher_plugin_uri_list_extract              (GtkSelectionData     *data);
static void               launcher_plugin_uri_list_free                 (GSList               *uri_list);



struct _LauncherPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _LauncherPlugin
{
  XfcePanelPlugin __parent__;

  GtkWidget         *box;
  GtkWidget         *button;
  GtkWidget         *arrow;
  GtkWidget         *child;
  GtkWidget         *menu;
  GtkWidget         *action_menu;

  GSList            *items;

  cairo_surface_t   *surface;
  gchar             *icon_name;

  gulong             theme_change_id;

  guint              menu_timeout_id;

  guint              disable_tooltips : 1;
  guint              move_first : 1;
  guint              show_label : 1;
  LauncherArrowType  arrow_position;

  GFile             *config_directory;
  GFileMonitor      *config_monitor;

  guint              save_timeout_id;
};

enum
{
  PROP_0,
  PROP_ITEMS,
  PROP_DISABLE_TOOLTIPS,
  PROP_MOVE_FIRST,
  PROP_SHOW_LABEL,
  PROP_ARROW_POSITION
};

enum
{
  ITEMS_CHANGED,
  LAST_SIGNAL
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (LauncherPlugin, launcher_plugin)



/* quark to attach the plugin to menu items */
static GQuark      launcher_plugin_quark = 0;
static guint       launcher_signals[LAST_SIGNAL];



/* target types for dropping in the launcher plugin */
static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, 0, },
  { "STRING", 0, 0 },
  { "UTF8_STRING", 0, 0 },
  { "text/plain", 0, 0 },
};



static void
launcher_plugin_class_init (LauncherPluginClass *klass)
{
  GObjectClass         *gobject_class;
  XfcePanelPluginClass *plugin_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = launcher_plugin_get_property;
  gobject_class->set_property = launcher_plugin_set_property;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = launcher_plugin_construct;
  plugin_class->free_data = launcher_plugin_free_data;
  plugin_class->mode_changed = launcher_plugin_mode_changed;
  plugin_class->size_changed = launcher_plugin_size_changed;
  plugin_class->configure_plugin = launcher_plugin_configure_plugin;
  plugin_class->screen_position_changed = launcher_plugin_screen_position_changed;
  plugin_class->removed = launcher_plugin_removed;
  plugin_class->remote_event = launcher_plugin_remote_event;

  g_object_class_install_property (gobject_class,
                                   PROP_ITEMS,
                                   g_param_spec_boxed ("items",
                                                       NULL, NULL,
                                                       G_TYPE_PTR_ARRAY,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DISABLE_TOOLTIPS,
                                   g_param_spec_boolean ("disable-tooltips",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_MOVE_FIRST,
                                   g_param_spec_boolean ("move-first",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_LABEL,
                                   g_param_spec_boolean ("show-label",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ARROW_POSITION,
                                   g_param_spec_uint ("arrow-position",
                                                      NULL, NULL,
                                                      LAUNCHER_ARROW_DEFAULT,
                                                      LAUNCHER_ARROW_INTERNAL,
                                                      LAUNCHER_ARROW_DEFAULT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  launcher_signals[ITEMS_CHANGED] =
    g_signal_new (g_intern_static_string ("items-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* initialize the quark */
  launcher_plugin_quark = g_quark_from_static_string ("xfce-launcher-plugin");
}



static void
launcher_plugin_init (LauncherPlugin *plugin)
{
  GtkCssProvider  *css_provider;
  GtkStyleContext *context;
  gchar           *css_string;

  plugin->disable_tooltips = FALSE;
  plugin->move_first = FALSE;
  plugin->show_label = FALSE;
  plugin->arrow_position = LAUNCHER_ARROW_DEFAULT;
  plugin->menu = NULL;
  plugin->action_menu = NULL;
  plugin->items = NULL;
  plugin->child = NULL;
  plugin->surface = NULL;
  plugin->icon_name = NULL;
  plugin->menu_timeout_id = 0;
  plugin->save_timeout_id = 0;

  /* create the panel widgets */
  plugin->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->box);

  plugin->button = xfce_panel_create_button ();
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->button, TRUE, TRUE, 0);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
  gtk_widget_set_has_tooltip (plugin->button, TRUE);
  gtk_widget_set_name (plugin->button, "launcher-button");
  g_signal_connect (G_OBJECT (plugin->button), "button-press-event",
      G_CALLBACK (launcher_plugin_button_press_event), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "button-release-event",
      G_CALLBACK (launcher_plugin_button_release_event), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "query-tooltip",
      G_CALLBACK (launcher_plugin_button_query_tooltip), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-data-received",
      G_CALLBACK (launcher_plugin_button_drag_data_received), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-motion",
      G_CALLBACK (launcher_plugin_button_drag_motion), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-drop",
      G_CALLBACK (launcher_plugin_button_drag_drop), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-leave",
      G_CALLBACK (launcher_plugin_button_drag_leave), plugin);
  g_signal_connect_after (G_OBJECT (plugin->button), "draw",
      G_CALLBACK (launcher_plugin_button_draw), plugin);

  /* invalidate tooltip icon when needed */
  plugin->theme_change_id = g_signal_connect_swapped (gtk_icon_theme_get_default (), "changed",
      G_CALLBACK (launcher_plugin_tooltip_icon_invalidate), plugin->button);
  g_signal_connect (plugin, "notify::scale-factor",
      G_CALLBACK (launcher_plugin_icon_invalidate), NULL);
  g_signal_connect (plugin, "notify::scale-factor",
      G_CALLBACK (launcher_plugin_menu_destroy), NULL);

  /* Make sure there aren't any constraints set on buttons by themes (Adwaita sets those minimum sizes) */
  context = gtk_widget_get_style_context (plugin->button);
  css_provider = gtk_css_provider_new ();
  css_string = g_strdup_printf ("#launcher-arrow { min-height: 0; min-width: 0; }");
  gtk_css_provider_load_from_data (css_provider, css_string, -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_free (css_string);
  g_object_unref (css_provider);

  plugin->child = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (plugin->button), plugin->child);

  plugin->arrow = xfce_arrow_button_new (GTK_ARROW_UP);
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->arrow, FALSE, FALSE, 0);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->arrow);
  gtk_button_set_relief (GTK_BUTTON (plugin->arrow), GTK_RELIEF_NONE);
  gtk_widget_set_name (plugin->button, "launcher-arrow");
  g_signal_connect (G_OBJECT (plugin->arrow), "button-press-event",
      G_CALLBACK (launcher_plugin_arrow_press_event), plugin);
  g_signal_connect (G_OBJECT (plugin->arrow), "drag-motion",
      G_CALLBACK (launcher_plugin_arrow_drag_motion), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-drop",
      G_CALLBACK (launcher_plugin_button_drag_drop), plugin);
  g_signal_connect (G_OBJECT (plugin->arrow), "drag-leave",
      G_CALLBACK (launcher_plugin_arrow_drag_leave), plugin);

  panel_utils_set_atk_info (plugin->arrow, _("Open launcher menu"), NULL);

  /* accept all sorts of drag data, but filter in drag-drop, so we can
   * send other sorts of drops to parent widgets */
  gtk_drag_dest_set (plugin->button, 0, NULL, 0, 0);
  gtk_drag_dest_set (plugin->arrow, 0, NULL, 0, 0);

  /* sync button states */
  g_signal_connect (G_OBJECT (plugin->button), "state-flags-changed",
      G_CALLBACK (launcher_plugin_button_state_changed), plugin->arrow);
  g_signal_connect (G_OBJECT (plugin->arrow), "state-flags-changed",
      G_CALLBACK (launcher_plugin_button_state_changed), plugin->button);
}



static void
launcher_free_array_element (gpointer data)
{
  GValue *value = (GValue *)data;

  g_value_unset (value);
  g_free (value);
}



static void
launcher_plugin_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (object);
  GPtrArray      *array;
  GValue         *tmp;
  GSList         *li;
  GFile          *item_file;

  switch (prop_id)
    {
    case PROP_ITEMS:
      array = g_ptr_array_new_full (1, (GDestroyNotify) launcher_free_array_element);
      for (li = plugin->items; li != NULL; li = li->next)
        {
          tmp = g_new0 (GValue, 1);
          g_value_init (tmp, G_TYPE_STRING);
          panel_return_if_fail (GARCON_IS_MENU_ITEM (li->data));
          item_file = garcon_menu_item_get_file (li->data);
          if (g_file_has_prefix (item_file, plugin->config_directory))
            g_value_take_string (tmp, g_file_get_basename (item_file));
          else
            g_value_take_string (tmp, g_file_get_uri (item_file));
          g_object_unref (G_OBJECT (item_file));
          g_ptr_array_add (array, tmp);
        }
      g_value_set_boxed (value, array);
      g_ptr_array_unref (array);
      break;

    case PROP_DISABLE_TOOLTIPS:
      g_value_set_boolean (value, plugin->disable_tooltips);
      break;

    case PROP_MOVE_FIRST:
      g_value_set_boolean (value, plugin->move_first);
      break;

    case PROP_SHOW_LABEL:
      g_value_set_boolean (value, plugin->show_label);
      break;

    case PROP_ARROW_POSITION:
      g_value_set_uint (value, plugin->arrow_position);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
launcher_plugin_item_changed (GarconMenuItem *item,
                              LauncherPlugin *plugin)
{
  GSList *li;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* find the item */
  li = g_slist_find (plugin->items, item);
  if (G_LIKELY (li != NULL))
    {
      /* update the button or destroy the menu */
      if (plugin->items == li)
        {
          launcher_plugin_button_update (plugin);
          launcher_plugin_button_update_action_menu (plugin);
        }
      else
        launcher_plugin_menu_destroy (plugin);
    }
  else
    {
      panel_assert_not_reached ();
    }
}



static gboolean
launcher_plugin_item_duplicate (GFile   *src_file,
                                GFile   *dst_file,
                                GError **error)
{
  GKeyFile *key_file;
  gchar    *contents = NULL;
  gsize     length;
  gboolean  result = FALSE;
  gchar    *uri;

  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!g_file_load_contents (src_file, NULL, &contents, &length, NULL, error))
    return FALSE;

  /* note that we don't load the key file with preserving the translations
   * and comments, this way we save a small desktop file in the user's language */
  key_file = g_key_file_new ();
  if (!g_key_file_load_from_data (key_file, contents, length, 0, error))
    goto err1;

  /* store the source uri in the desktop file for restore purposes */
  uri = g_file_get_uri (src_file);
  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "X-XFCE-Source", uri);
  g_free (uri);

  g_free (contents);
  contents = g_key_file_to_data (key_file, &length, NULL);
  result = g_file_replace_contents (dst_file, contents, length, NULL, FALSE,
                                    G_FILE_CREATE_REPLACE_DESTINATION,
                                    NULL, NULL, error);

err1:
  g_free (contents);
  g_key_file_free (key_file);

  return result;
}


static GarconMenuItem *
launcher_plugin_item_load (LauncherPlugin *plugin,
                           const gchar    *str,
                           gboolean       *desktop_id_return,
                           gboolean       *location_changed)
{
  GFile          *src_file, *dst_file;
  gchar          *src_path, *dst_path;
  GSList         *li, *lnext;
  GarconMenuItem *item = NULL;
  GError         *error = NULL;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), NULL);
  panel_return_val_if_fail (str != NULL, NULL);
  panel_return_val_if_fail (G_IS_FILE (plugin->config_directory), NULL);

  if (G_UNLIKELY (g_path_is_absolute (str) || g_uri_is_valid (str, G_URI_FLAGS_NONE, NULL)))
    {
      src_file = g_file_new_for_commandline_arg (str);
      if (g_file_has_prefix (src_file, plugin->config_directory))
        {
          /* nothing, we use the file below */
        }
      else if (g_file_query_exists (src_file, NULL))
        {
          /* create a unique file in the config directory */
          dst_path = launcher_plugin_unique_filename (plugin);
          dst_file = g_file_new_for_path (dst_path);

          /* create a duplicate in the config directory */
          if (launcher_plugin_item_duplicate (src_file, dst_file, &error))
            {
              /* use the new file */
              g_object_unref (G_OBJECT (src_file));
              src_file = dst_file;

              if (G_LIKELY (location_changed != NULL))
                *location_changed = TRUE;
            }
          else
            {
              src_path = g_file_get_parse_name (src_file);
              g_warning ("Failed to create duplicate of desktop file \"%s\" "
                          "to \"%s\": %s", src_path, dst_path, error->message);
              g_error_free (error);
              g_free (src_path);

              /* continue using the source file, the user won't be able to
               * edit the item, but atleast we have something that works in
               * the panel */
              g_object_unref (G_OBJECT (dst_file));
            }

          g_free (dst_path);
        }
      else
        {
          /* nothing we can do with this file */
          src_path = g_file_get_parse_name (src_file);
          g_warning ("Failed to load desktop file \"%s\". It will be removed "
                     "from the configuration", src_path);
          g_free (src_path);
          g_object_unref (G_OBJECT (src_file));

          return NULL;
        }
    }
  else
    {
      /* assume the file is a child in the config directory */
      src_file = g_file_get_child (plugin->config_directory, str);

      /* str might also be a global desktop id */
      if (G_LIKELY (desktop_id_return != NULL))
        *desktop_id_return = TRUE;
    }

  panel_assert (G_IS_FILE (src_file));

  /* maybe we have this file in the launcher configuration, then we don't
   * have to load it again from the harddisk */
  for (li = plugin->items; item == NULL && li != NULL; li = lnext)
    {
      lnext = li->next;
      dst_file = garcon_menu_item_get_file (GARCON_MENU_ITEM (li->data));
      if (g_file_equal (src_file, dst_file))
        {
          item = GARCON_MENU_ITEM (li->data);
          plugin->items = g_slist_delete_link (plugin->items, li);
        }
      g_object_unref (G_OBJECT (dst_file));
    }

  /* load the file from the disk */
  if (item == NULL)
    item = garcon_menu_item_new (src_file);

  g_object_unref (G_OBJECT (src_file));

  return item;
}



static void
launcher_plugin_items_delete_configs (LauncherPlugin *plugin)
{
  GSList   *li;
  GFile    *file;
  gboolean  succeed = TRUE;
  GError   *error = NULL;

  panel_return_if_fail (G_IS_FILE (plugin->config_directory));

  /* cleanup desktop files in the config dir */
  for (li = plugin->items; succeed && li != NULL; li = li->next)
    {
      file = garcon_menu_item_get_file (li->data);
      if (g_file_has_prefix (file, plugin->config_directory))
        succeed = g_file_delete (file, NULL, &error);
      g_object_unref (G_OBJECT (file));
    }

  if (!succeed)
    {
      g_message ("launcher-%d: Failed to cleanup the configuration: %s",
                 xfce_panel_plugin_get_unique_id (XFCE_PANEL_PLUGIN (plugin)),
                 error->message);
      g_error_free (error);
    }
}



static void
launcher_plugin_items_free (LauncherPlugin *plugin)
{
  if (G_LIKELY (plugin->items != NULL))
    {
      g_slist_foreach (plugin->items, (GFunc) (void (*)(void)) g_object_unref, NULL);
      g_slist_free (plugin->items);
      plugin->items = NULL;
    }
}



static void
launcher_plugin_items_load (LauncherPlugin *plugin,
                            GPtrArray      *array)
{
  guint           i;
  const GValue   *value;
  const gchar    *str;
  GarconMenuItem *item;
  GarconMenuItem *pool_item;
  GSList         *items = NULL;
  GHashTable     *pool = NULL;
  gboolean        desktop_id;
  gchar          *uri;
  gboolean        items_modified = FALSE;
  gboolean        location_changed;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (array != NULL);

  for (i = 0; i < array->len; i++)
    {
      value = g_ptr_array_index (array, i);
      panel_assert (G_VALUE_HOLDS_STRING (value));
      str = g_value_get_string (value);

      /* only accept desktop files */
      if (str == NULL || !g_str_has_suffix (str, ".desktop"))
        continue;

      /* try to load the item */
      desktop_id = FALSE;
      location_changed = FALSE;
      item = launcher_plugin_item_load (plugin, str, &desktop_id, &location_changed);
      if (G_LIKELY (item == NULL))
        {
          /* str did not look like a desktop-id, so no need to look
           * for it in the application pool */
          if (!desktop_id)
            continue;

          /* we are going to load an desktop_id from the item pool,
           * even if this failes, save the new item list, so we don't
           * try this again in the future */
          items_modified = TRUE;

          /* load the pool with desktop items */
          if (pool == NULL)
            pool = launcher_plugin_garcon_menu_pool ();

          /* lookup the item in the item pool */
          pool_item = g_hash_table_lookup (pool, str);
          if (pool_item != NULL)
            {
              /* we want an editable file, so try to make a copy */
              uri = garcon_menu_item_get_uri (pool_item);
              item = launcher_plugin_item_load (plugin, uri, NULL, NULL);
              g_free (uri);

              /* if something failed, use the pool item, but this one
               * won't be editable in the dialog */
              if (G_UNLIKELY (item == NULL))
                item = GARCON_MENU_ITEM (g_object_ref (G_OBJECT (pool_item)));
            }

          /* skip this item if still not found */
          if (item == NULL)
            continue;
        }
      else if (location_changed)
        {
          items_modified = TRUE;
        }

      /* add the item to the list */
      panel_assert (GARCON_IS_MENU_ITEM (item));
      items = g_slist_append (items, item);
      g_signal_connect (G_OBJECT (item), "changed",
          G_CALLBACK (launcher_plugin_item_changed), plugin);
    }

  if (G_UNLIKELY (pool != NULL))
    g_hash_table_destroy (pool);

  /* remove config files of items not in the new config */
  launcher_plugin_items_delete_configs (plugin);

  /* release the old menu items and set new one */
  launcher_plugin_items_free (plugin);
  plugin->items = items;

  /* store the new item list */
  if (items_modified)
    launcher_plugin_save_delayed (plugin);
}



static void
launcher_plugin_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (object);
  GPtrArray      *array;

  panel_return_if_fail (G_IS_FILE (plugin->config_directory));

  /* destroy the menu, all the setting changes need this */
  launcher_plugin_menu_destroy (plugin);

  switch (prop_id)
    {
    case PROP_ITEMS:
      /* load new items from the array */
      array = g_value_get_boxed (value);
      if (G_LIKELY (array != NULL))
        {
          launcher_plugin_items_load (plugin, array);
        }
      else
        {
          launcher_plugin_items_delete_configs (plugin);
          launcher_plugin_items_free (plugin);
        }

      /* emit signal */
      g_signal_emit (G_OBJECT (plugin), launcher_signals[ITEMS_CHANGED], 0);

      /* update the button */
      launcher_plugin_button_update (plugin);
      launcher_plugin_button_update_action_menu (plugin);

      /* update the widget packing */
      goto update_arrow;
      break;

    case PROP_DISABLE_TOOLTIPS:
      plugin->disable_tooltips = g_value_get_boolean (value);
      gtk_widget_set_has_tooltip (plugin->button, !plugin->disable_tooltips);
      break;

    case PROP_MOVE_FIRST:
      plugin->move_first = g_value_get_boolean (value);
      break;

    case PROP_SHOW_LABEL:
      plugin->show_label = g_value_get_boolean (value);

      /* destroy the old child */
      if (plugin->child != NULL)
        gtk_widget_destroy (plugin->child);

      /* create child */
      if (G_UNLIKELY (plugin->show_label))
        plugin->child = gtk_label_new (NULL);
      else
        plugin->child = gtk_image_new ();
      gtk_container_add (GTK_CONTAINER (plugin->button), plugin->child);
      gtk_widget_show (plugin->child);

      /* update size */
      launcher_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin),
          xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));

      /* update the button */
      launcher_plugin_button_update (plugin);
      break;

    case PROP_ARROW_POSITION:
      plugin->arrow_position = g_value_get_uint (value);

update_arrow:
      /* update the arrow button visibility */
      launcher_plugin_arrow_visibility (plugin);

      /* repack the widgets */
      launcher_plugin_pack_widgets (plugin);

      /* update the plugin size */
      launcher_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin),
          xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
launcher_plugin_file_changed (GFileMonitor      *monitor,
                              GFile             *changed_file,
                              GFile             *other_file,
                              GFileMonitorEvent  event_type,
                              LauncherPlugin    *plugin)
{
  GSList         *li, *lnext;
  GarconMenuItem *item;
  GFile          *item_file;
  gboolean        found;
  GError         *error = NULL;
  gchar          *base_name;
  gboolean        result;
  gboolean        exists;
  gboolean        update_plugin = FALSE;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->config_monitor == monitor);

  /* waited until all events are proccessed */
  if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT
      && event_type != G_FILE_MONITOR_EVENT_DELETED
      && event_type != G_FILE_MONITOR_EVENT_CREATED)
    return;

  /* we only act on desktop files */
  base_name = g_file_get_basename (changed_file);
  result = g_str_has_suffix (base_name, ".desktop");
  g_free (base_name);
  if (!result)
    return;

  exists = g_file_query_exists (changed_file, NULL);

  /* lookup the file in the menu items */
  for (li = plugin->items, found = FALSE; !found && li != NULL; li = lnext)
    {
      lnext = li->next;
      item = GARCON_MENU_ITEM (li->data);
      item_file = garcon_menu_item_get_file (item);
      found = g_file_equal (changed_file, item_file);
      if (found)
        {
          if (exists)
            {
              /* reload the file */
              if (!garcon_menu_item_reload (item, NULL, &error))
                {
                  g_critical ("Failed to reload menu item: %s", error->message);
                  g_error_free (error);
                }
            }
          else
            {
              /* remove from the list */
              plugin->items = g_slist_delete_link (plugin->items, li);
              g_object_unref (G_OBJECT (item));
              update_plugin = TRUE;
            }
        }
      g_object_unref (G_OBJECT (item_file));
    }

  if (!found && exists)
    {
      /* add the new file to the config */
      item = garcon_menu_item_new (changed_file);
      if (G_LIKELY (item != NULL))
        {
          plugin->items = g_slist_append (plugin->items, item);
          g_signal_connect (G_OBJECT (item), "changed",
              G_CALLBACK (launcher_plugin_item_changed), plugin);
          update_plugin = TRUE;
        }
    }

  if (update_plugin)
    {
      launcher_plugin_button_update (plugin);
      launcher_plugin_menu_destroy (plugin);
      launcher_plugin_button_update_action_menu (plugin);

      /* save the new config */
      launcher_plugin_save_delayed (plugin);

      /* update the dialog */
      g_signal_emit (G_OBJECT (plugin), launcher_signals[ITEMS_CHANGED], 0);
    }
}



static void
launcher_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  LauncherPlugin      *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  const gchar * const *uris;
  guint                i;
  GPtrArray           *array;
  GValue              *value;
  gchar               *file, *path;
  GError              *error = NULL;
  const PanelProperty  properties[] =
  {
    { "show-label", G_TYPE_BOOLEAN },
    { "items", G_TYPE_PTR_ARRAY },
    { "disable-tooltips", G_TYPE_BOOLEAN },
    { "move-first", G_TYPE_BOOLEAN },
    { "arrow-position", G_TYPE_UINT },
    { NULL }
  };

  /* show the configure menu item */
  xfce_panel_plugin_menu_show_configure (panel_plugin);

  xfce_panel_plugin_set_small (panel_plugin, TRUE);

  /* lookup the config directory where this launcher stores it's desktop files */
  file = g_strdup_printf (RELATIVE_CONFIG_PATH,
                          xfce_panel_plugin_get_name (XFCE_PANEL_PLUGIN (plugin)),
                          xfce_panel_plugin_get_unique_id (XFCE_PANEL_PLUGIN (plugin)));
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, file, FALSE);
  plugin->config_directory = g_file_new_for_path (path);
  g_free (file);
  g_free (path);

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  /* handle and empty plugin */
  if (G_UNLIKELY (plugin->items == NULL))
    {
      /* get the plugin arguments list */
      uris = xfce_panel_plugin_get_arguments (panel_plugin);
      if (G_LIKELY (uris != NULL && *uris != NULL))
        {
          /* create array with all the uris */
          array = g_ptr_array_new ();
          for (i = 0; uris[i] != NULL; i++)
            {
              value = g_new0 (GValue, 1);
              g_value_init (value, G_TYPE_STRING);
              g_value_set_static_string (value, uris[i]);
              g_ptr_array_add (array, value);
            }

          /* set new file list */
          if (G_LIKELY (array->len > 0))
            g_object_set (G_OBJECT (plugin), "items", array, NULL);
          xfconf_array_free (array);
        }
      else
        {
          /* update the icon */
          launcher_plugin_button_update (plugin);
        }
    }

  /* start file monitor in our config directory */
  plugin->config_monitor = g_file_monitor_directory (plugin->config_directory,
                                                     G_FILE_MONITOR_NONE, NULL, &error);
  if (G_LIKELY (plugin->config_monitor != NULL))
    {
      g_signal_connect (G_OBJECT (plugin->config_monitor), "changed",
                        G_CALLBACK (launcher_plugin_file_changed), plugin);
    }
  else
    {
      g_critical ("Failed to start file monitor: %s", error->message);
      g_error_free (error);
    }

  /* show the beast */
  gtk_widget_show (plugin->box);
  gtk_widget_show (plugin->button);
  gtk_widget_show (plugin->child);
}



static void
launcher_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  GtkIconTheme   *icon_theme;

  /* stop monitoring */
  if (plugin->config_monitor != NULL)
    {
      g_file_monitor_cancel (plugin->config_monitor);
      g_object_unref (G_OBJECT (plugin->config_monitor));
    }

  if (plugin->save_timeout_id != 0)
    {
      g_source_remove (plugin->save_timeout_id);
      launcher_plugin_save_delayed_timeout (plugin);
    }

  /* destroy the menu and timeout */
  launcher_plugin_menu_destroy (plugin);

  launcher_plugin_items_free (plugin);

  if (plugin->config_directory != NULL)
    g_object_unref (G_OBJECT (plugin->config_directory));

  /* stop watching the icon theme */
  if (plugin->theme_change_id != 0)
    {
      icon_theme = gtk_icon_theme_get_default ();
      g_signal_handler_disconnect (G_OBJECT (icon_theme), plugin->theme_change_id);
    }

  /* release the cached pixbuf */
  if (plugin->surface != NULL)
    cairo_surface_destroy (plugin->surface);
  if (plugin->icon_name != NULL)
    g_free (plugin->icon_name);
}



static void
launcher_plugin_removed (XfcePanelPlugin *panel_plugin)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  GError         *error = NULL;

  panel_return_if_fail (G_IS_FILE (plugin->config_directory));

  /* leave if there is not config */
  if (!g_file_query_exists (plugin->config_directory, NULL))
    return;

  /* stop monitoring */
  if (plugin->config_monitor != NULL)
    {
      g_file_monitor_cancel (plugin->config_monitor);
      g_object_unref (G_OBJECT (plugin->config_monitor));
      plugin->config_monitor = NULL;
    }

  /* cleanup desktop files in the config dir */
  launcher_plugin_items_delete_configs (plugin);

  if (!g_file_delete (plugin->config_directory, NULL, &error))
    {
      g_message ("launcher-%d: Failed to cleanup the configuration: %s",
                 xfce_panel_plugin_get_unique_id (panel_plugin),
                 error->message);
      g_error_free (error);
    }
}



static gboolean
launcher_plugin_remote_event (XfcePanelPlugin *panel_plugin,
                              const gchar     *name,
                              const GValue    *value)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);

  panel_return_val_if_fail (value == NULL || G_IS_VALUE (value), FALSE);

  if (g_strcmp0 (name, "popup") == 0
      && LIST_HAS_TWO_OR_MORE_ENTRIES (plugin->items)
      && (plugin->menu == NULL || !gtk_widget_get_visible (plugin->menu)))
    {
      launcher_plugin_menu_popup (plugin);

      return TRUE;
    }

  if (g_strcmp0 (name, "disable-tooltips") == 0
      && value != NULL
      && G_VALUE_HOLDS_BOOLEAN (value))
    {
      g_object_set_property (G_OBJECT (plugin), "disable-tooltips", value);

      return FALSE;
    }

  return FALSE;
}



static void
launcher_plugin_save_delayed_timeout_destroyed (gpointer user_data)
{
  XFCE_LAUNCHER_PLUGIN (user_data)->save_timeout_id = 0;
}



static gboolean
launcher_plugin_save_delayed_timeout (gpointer user_data)
{
  /* make sure the items are stored */
  g_object_notify (G_OBJECT (user_data), "items");

  return FALSE;
}



static void
launcher_plugin_save_delayed (LauncherPlugin *plugin)
{
  if (plugin->save_timeout_id != 0)
    g_source_remove (plugin->save_timeout_id);

  plugin->save_timeout_id = gdk_threads_add_timeout_seconds_full (G_PRIORITY_LOW, 1,
      launcher_plugin_save_delayed_timeout, plugin,
      launcher_plugin_save_delayed_timeout_destroyed);
}



static void
launcher_plugin_mode_changed (XfcePanelPlugin    *panel_plugin,
                              XfcePanelPluginMode mode)
{
  /* update label orientation */
  launcher_plugin_button_update (XFCE_LAUNCHER_PLUGIN (panel_plugin));

  /* update the widget order */
  launcher_plugin_pack_widgets (XFCE_LAUNCHER_PLUGIN (panel_plugin));

  /* update the arrow button */
  launcher_plugin_screen_position_changed (panel_plugin,
      xfce_panel_plugin_get_screen_position (panel_plugin));

  /* update the plugin size */
  launcher_plugin_size_changed (panel_plugin,
      xfce_panel_plugin_get_size (panel_plugin));
}



static gboolean
launcher_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                              gint             size)
{
  LauncherPlugin    *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  gint               p_width, p_height;
  gint               a_width, a_height;
  gboolean           horizontal;
  LauncherArrowType  arrow_position;

  /* initialize the plugin size */
  size /= xfce_panel_plugin_get_nrows (panel_plugin);
  p_width = p_height = size;
  a_width = a_height = -1;

  /* add the arrow size */
  if (gtk_widget_get_visible (plugin->arrow))
    {
      /* if the panel is horizontal */
      horizontal = !!(xfce_panel_plugin_get_orientation (panel_plugin) ==
          GTK_ORIENTATION_HORIZONTAL);

      /* translate default direction */
      arrow_position = launcher_plugin_default_arrow_type (plugin);

      switch (arrow_position)
        {
        case LAUNCHER_ARROW_NORTH:
        case LAUNCHER_ARROW_SOUTH:
          a_height = ARROW_BUTTON_SIZE;
          if (!horizontal)
            p_height += ARROW_BUTTON_SIZE;
          break;

        case LAUNCHER_ARROW_EAST:
        case LAUNCHER_ARROW_WEST:
          a_width = ARROW_BUTTON_SIZE;
          if (horizontal)
            p_width += ARROW_BUTTON_SIZE;
          break;

        default:
          /* the default position should never be returned */
          panel_assert_not_reached ();
          break;
        }

        /* set the arrow size */
        gtk_widget_set_size_request (plugin->arrow, a_width, a_height);

    }

  /* set the panel plugin size */
  if (plugin->show_label)
    gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), -1, -1);
  else
    {
      gint icon_size;

      gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), p_width, p_height);
      icon_size = xfce_panel_plugin_get_icon_size (panel_plugin);

      /* if the icon is a pixbuf we have to recreate and scale it */
      if (plugin->surface != NULL && plugin->icon_name != NULL)
        {
          GdkPixbuf *pixbuf;
          gint scale_factor;

          cairo_surface_destroy (plugin->surface);
          plugin->surface = NULL;
          scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (plugin));
          pixbuf = gdk_pixbuf_new_from_file_at_size (plugin->icon_name,
                                                     icon_size * scale_factor,
                                                     icon_size * scale_factor,
                                                     NULL);
          if (pixbuf != NULL)
            {
              plugin->surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
              g_object_unref (pixbuf);
            }

          gtk_image_set_from_surface (GTK_IMAGE (plugin->child), plugin->surface);
        }
      /* set the panel plugin icon size */
      else
        gtk_image_set_pixel_size (GTK_IMAGE (plugin->child), icon_size);
    }

  /* destroy the menu to update its size */
  launcher_plugin_menu_destroy (plugin);

  return TRUE;
}



static void
launcher_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  /* run the configure dialog */
  launcher_dialog_show (XFCE_LAUNCHER_PLUGIN (panel_plugin));
}



static void
launcher_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                         XfceScreenPosition  position)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);

  /* set the new arrow direction */
  xfce_arrow_button_set_arrow_type (XFCE_ARROW_BUTTON (plugin->arrow),
      xfce_panel_plugin_arrow_type (panel_plugin));

  /* destroy the menu to update sort order */
  launcher_plugin_menu_destroy (plugin);
}



static LauncherArrowType
launcher_plugin_default_arrow_type (LauncherPlugin *plugin)
{
  LauncherArrowType pos = plugin->arrow_position;
  gboolean          rtl;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), LAUNCHER_ARROW_NORTH);

  if (pos == LAUNCHER_ARROW_DEFAULT)
    {
      /* get the plugin direction */
      rtl = !!(gtk_widget_get_direction (GTK_WIDGET (plugin)) == GTK_TEXT_DIR_RTL);

      if (xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)) ==
              GTK_ORIENTATION_HORIZONTAL)
        pos = rtl ? LAUNCHER_ARROW_WEST : LAUNCHER_ARROW_EAST;
      else
        pos = rtl ? LAUNCHER_ARROW_NORTH : LAUNCHER_ARROW_SOUTH;
    }

  return pos;
}



static void
launcher_plugin_pack_widgets (LauncherPlugin *plugin)
{
  LauncherArrowType pos;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* leave when the arrow button is not visible */
  if (!gtk_widget_get_visible (plugin->arrow)
      || plugin->arrow_position == LAUNCHER_ARROW_INTERNAL)
    return;

  pos = launcher_plugin_default_arrow_type (plugin);
  panel_assert (pos != LAUNCHER_ARROW_DEFAULT);

  /* set the position of the arrow button in the box */
  gtk_box_set_child_packing (GTK_BOX (plugin->box), plugin->arrow, TRUE, TRUE, 0,
                      (pos == LAUNCHER_ARROW_SOUTH || pos == LAUNCHER_ARROW_EAST) ? GTK_PACK_END : GTK_PACK_START);
  gtk_box_set_child_packing (GTK_BOX (plugin->box), plugin->button, FALSE, FALSE, 0,
                      (pos == LAUNCHER_ARROW_SOUTH || pos == LAUNCHER_ARROW_EAST) ? GTK_PACK_START : GTK_PACK_END);

  /* set the orientation */
  gtk_orientable_set_orientation (GTK_ORIENTABLE (plugin->box),
      !!(pos == LAUNCHER_ARROW_WEST || pos == LAUNCHER_ARROW_EAST) ?
          GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
}



GIcon *
launcher_plugin_tooltip_icon (const gchar *icon_name)
{
  GIcon *icon;

  if (panel_str_is_empty (icon_name))
    return NULL;

  if (G_UNLIKELY (g_path_is_absolute (icon_name)))
    {
      GFile *file = g_file_new_for_path (icon_name);
      icon = g_file_icon_new (file);
      g_object_unref (file);
    }
  else
    icon = g_themed_icon_new (icon_name);

  return icon;
}



static void
launcher_plugin_menu_deactivate (GtkWidget      *menu,
                                 LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->menu == menu);

  /* deactivate the arrow button */
  if (plugin->arrow_position != LAUNCHER_ARROW_INTERNAL)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
      gtk_widget_unset_state_flags (GTK_WIDGET (plugin->arrow), GTK_STATE_FLAG_PRELIGHT);
    }
  else
    {
      gtk_widget_set_state_flags (GTK_WIDGET (plugin->button), GTK_STATE_FLAG_NORMAL, TRUE);
    }
}



static void
launcher_plugin_menu_item_activate (GtkMenuItem      *widget,
                                    GarconMenuItem   *item)
{
  LauncherPlugin *plugin;
  GdkScreen      *screen;
  GdkEvent       *event;
  guint32         event_time;

  panel_return_if_fail (GTK_IS_MENU_ITEM (widget));
  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* get a copy of the event causing the menu item to activate */
  event = gtk_get_current_event ();
  event_time = gdk_event_get_time (event);

  /* get the widget screen */
  screen = gtk_widget_get_screen (GTK_WIDGET (widget));

  /* launch the command */
  if (event != NULL
      && event->type == GDK_BUTTON_RELEASE
      && event->button.button == 2)
    launcher_plugin_item_exec_from_clipboard (item, event_time, screen);
  else
    launcher_plugin_item_exec (item, event_time, screen, NULL);

  if (event != NULL)
    gdk_event_free (event);

  /* get the plugin */
  plugin = g_object_get_qdata (G_OBJECT (widget), launcher_plugin_quark);
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* move the item to the first position if enabled */
  if (G_UNLIKELY (plugin->move_first))
    {
      /* prepend the item in the list */
      plugin->items = g_slist_remove (plugin->items, item);
      plugin->items = g_slist_prepend (plugin->items, item);

      /* destroy the menu and update the icon */
      launcher_plugin_menu_destroy (plugin);
      launcher_plugin_button_update (plugin);
    }
}



static void
launcher_plugin_menu_item_drag_data_received (GtkWidget          *widget,
                                              GdkDragContext     *context,
                                              gint                x,
                                              gint                y,
                                              GtkSelectionData   *data,
                                              guint               info,
                                              guint               drag_time,
                                              GarconMenuItem     *item)
{
  LauncherPlugin *plugin;
  GSList         *uri_list;

  panel_return_if_fail (GTK_IS_MENU_ITEM (widget));
  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* get the plugin */
  plugin = g_object_get_qdata (G_OBJECT (widget), launcher_plugin_quark);
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* extract the uris from the selection data */
  uri_list = launcher_plugin_uri_list_extract (data);
  if (G_LIKELY (uri_list != NULL))
    {
      /* execute the menu item */
      launcher_plugin_item_exec (item, drag_time,
                                 gtk_widget_get_screen (widget),
                                 uri_list);

      launcher_plugin_uri_list_free (uri_list);
    }

  /* hide the menu */
  gtk_widget_hide (gtk_widget_get_toplevel (plugin->menu));
  gtk_widget_hide (plugin->menu);

  /* deactivate the toggle button */
  if (plugin->arrow_position != LAUNCHER_ARROW_INTERNAL)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
      gtk_widget_unset_state_flags (GTK_WIDGET (plugin->arrow), GTK_STATE_FLAG_PRELIGHT);
    }
  else
    {
      gtk_widget_set_state_flags (GTK_WIDGET (plugin->button), GTK_STATE_FLAG_NORMAL, TRUE);
    }

  /* finish the drag */
  gtk_drag_finish (context, TRUE, FALSE, drag_time);
}



static void
launcher_plugin_menu_construct (LauncherPlugin *plugin)
{
  GtkArrowType    arrow_type;
  guint           n;
  GarconMenuItem *item;
  GtkWidget      *mi, *box, *label, *image;
  GdkPixbuf      *pixbuf;
  const gchar    *name, *icon_name;
  GSList         *li;
  gint            icon_size;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->menu == NULL);

  icon_size = xfce_panel_plugin_get_icon_size (XFCE_PANEL_PLUGIN (plugin));
  /* create a new menu */
  plugin->menu = gtk_menu_new ();
  gtk_menu_set_reserve_toggle_size (GTK_MENU (plugin->menu), FALSE);
  gtk_menu_attach_to_widget (GTK_MENU (plugin->menu), GTK_WIDGET (plugin), NULL);
  g_signal_connect (G_OBJECT (plugin->menu), "deactivate",
                    G_CALLBACK (launcher_plugin_menu_deactivate), plugin);

  /* get the arrow type of the plugin */
  arrow_type = xfce_arrow_button_get_arrow_type (XFCE_ARROW_BUTTON (plugin->arrow));

  /* walk through the menu entries */
  for (li = plugin->items, n = 0; li != NULL; li = li->next, n++)
    {
      /* skip the first entry when the arrow is visible */
      if (n == 0 && plugin->arrow_position != LAUNCHER_ARROW_INTERNAL)
        continue;

      /* get the item data */
      item = GARCON_MENU_ITEM (li->data);

      /* create the menu item */
      name = garcon_menu_item_get_name (item);
      mi = gtk_menu_item_new ();
      label = gtk_label_new (panel_str_is_empty (name) ? _("Unnamed Item") : name);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_box_pack_end (GTK_BOX (box), label, TRUE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (mi), box);
      g_object_set_qdata (G_OBJECT (mi), launcher_plugin_quark, plugin);
      gtk_widget_show_all (mi);
      gtk_drag_dest_set (mi, GTK_DEST_DEFAULT_ALL, drop_targets,
                         G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (launcher_plugin_menu_item_activate), item);
      g_signal_connect (G_OBJECT (mi), "drag-data-received",
          G_CALLBACK (launcher_plugin_menu_item_drag_data_received), item);
      g_signal_connect (G_OBJECT (mi), "drag-leave",
          G_CALLBACK (launcher_plugin_arrow_drag_leave), plugin);

      /* only connect the tooltip signal if tips are enabled */
      if (!plugin->disable_tooltips)
        {
          gtk_widget_set_has_tooltip (mi, TRUE);
          g_signal_connect (G_OBJECT (mi), "query-tooltip",
              G_CALLBACK (launcher_plugin_item_query_tooltip), item);

          /* invalidate tooltip icon when needed */
          g_signal_connect_object (gtk_icon_theme_get_default (), "changed",
              G_CALLBACK (launcher_plugin_tooltip_icon_invalidate), mi, G_CONNECT_SWAPPED);
        }

      /* depending on the menu position we prepend or append */
      if (G_UNLIKELY (arrow_type == GTK_ARROW_UP))
        gtk_menu_shell_prepend (GTK_MENU_SHELL (plugin->menu), mi);
      else
        gtk_menu_shell_append (GTK_MENU_SHELL (plugin->menu), mi);

      /* set the icon if one is set */
      icon_name = garcon_menu_item_get_icon_name (item);

      if (panel_str_is_empty (icon_name))
        {
          /* use an empty placeholder icon */
          image = gtk_image_new_from_icon_name ("", GTK_ICON_SIZE_DND);
        }
      else if (g_path_is_absolute (icon_name))
        {
          gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (plugin));
          pixbuf = gdk_pixbuf_new_from_file_at_size (icon_name,
                                                     icon_size * scale_factor,
                                                     icon_size * scale_factor,
                                                     NULL);
          image = gtk_image_new_from_surface (NULL);
          if (pixbuf != NULL)
            {
              cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
              gtk_image_set_from_surface (GTK_IMAGE (image), surface);
              cairo_surface_destroy (surface);
              g_object_unref (pixbuf);
            }
        }
      else
        {
          image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DND);
        }
      gtk_image_set_pixel_size (GTK_IMAGE (image), icon_size);

      gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 3);
      gtk_widget_show (image);
    }
}



static void
launcher_plugin_menu_popup_destroyed (gpointer user_data)
{
   XFCE_LAUNCHER_PLUGIN (user_data)->menu_timeout_id = 0;
}



static gboolean
launcher_plugin_menu_popup (gpointer user_data)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (user_data);
  gint            x, y;
  GdkEvent       *event = NULL;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* construct the menu if needed */
  if (plugin->menu == NULL)
    launcher_plugin_menu_construct (plugin);

  /* toggle the arrow button */
  if (plugin->arrow_position != LAUNCHER_ARROW_INTERNAL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), TRUE);
  else
    gtk_widget_set_state_flags (GTK_WIDGET (plugin->button), GTK_STATE_FLAG_CHECKED, TRUE);

  /* avoid warning when there is no current event */
  event = gtk_get_current_event ();
  if (event == NULL)
    {
      GdkSeat *seat = gdk_display_get_default_seat (gdk_display_get_default ());
      event = gdk_event_new (GDK_BUTTON_PRESS);
      event->button.window = g_object_ref (gdk_get_default_root_window ());
      gdk_event_set_device (event, gdk_seat_get_pointer (seat));
    }

  /* popup the menu */
  xfce_panel_plugin_popup_menu (XFCE_PANEL_PLUGIN (plugin), GTK_MENU (plugin->menu),
                                plugin->button, event);

  gdk_event_free (event);

  /* fallback to manual positioning, this is used with drag motion over the arrow button
   * from e.g. Thunar, not from the app menu, where showing the menu does not work for
   * some other reason: see https://gitlab.xfce.org/xfce/xfce4-panel/-/issues/205 */
  if (!gtk_widget_get_visible (plugin->menu))
    {
      /* make sure the size is allocated */
      if (!gtk_widget_get_realized (plugin->menu))
        gtk_widget_realize (plugin->menu);

      /* use the widget position function to get the coordinates */
      xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (plugin),
                                         plugin->menu, NULL, &x, &y);

      /* bit ugly... but show the menu */
      gtk_widget_show (plugin->menu);
      gtk_window_move (GTK_WINDOW (gtk_widget_get_toplevel (plugin->menu)), x, y);
      gtk_widget_show (gtk_widget_get_toplevel (plugin->menu));
    }

  return FALSE;
}



static void
launcher_plugin_menu_destroy (LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* stop pending timeout */
  if (plugin->menu_timeout_id != 0)
    g_source_remove (plugin->menu_timeout_id);

  if (plugin->menu != NULL)
    {
      /* destroy the menu */
      gtk_widget_destroy (plugin->menu);
      plugin->menu = NULL;

      /* deactivate the toggle button */
      if (plugin->arrow_position != LAUNCHER_ARROW_INTERNAL)
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
          gtk_widget_unset_state_flags (GTK_WIDGET (plugin->arrow), GTK_STATE_FLAG_PRELIGHT);
        }
      else
        {
          gtk_widget_set_state_flags (GTK_WIDGET (plugin->button), GTK_STATE_FLAG_NORMAL, TRUE);
        }
    }
}



static void
launcher_plugin_button_update (LauncherPlugin *plugin)
{
  GarconMenuItem      *item = NULL;
  const gchar         *icon_name;
  XfcePanelPluginMode  mode;
  gint                 icon_size;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* invalidate cached icons */
  launcher_plugin_tooltip_icon_invalidate (G_OBJECT (plugin->button));
  launcher_plugin_icon_invalidate (plugin);

  /* get first item */
  if (G_LIKELY (plugin->items != NULL))
    item = GARCON_MENU_ITEM (plugin->items->data);

  mode = xfce_panel_plugin_get_mode (XFCE_PANEL_PLUGIN (plugin));
  icon_size = xfce_panel_plugin_get_icon_size (XFCE_PANEL_PLUGIN (plugin));

  /* disable the "small" property in the deskbar mode and the label visible */
  if (G_UNLIKELY (plugin->show_label && mode == XFCE_PANEL_PLUGIN_MODE_DESKBAR))
    xfce_panel_plugin_set_small (XFCE_PANEL_PLUGIN (plugin), FALSE);
  else
    xfce_panel_plugin_set_small (XFCE_PANEL_PLUGIN (plugin), TRUE);

  if (G_UNLIKELY (plugin->show_label))
    {
      panel_return_if_fail (GTK_IS_LABEL (plugin->child));

      gtk_label_set_angle (GTK_LABEL (plugin->child),
                           (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ? 270 : 0);
      gtk_label_set_text (GTK_LABEL (plugin->child),
          item != NULL ? garcon_menu_item_get_name (item) : _("No items"));
    }
  else if (G_LIKELY (item != NULL))
    {
      panel_return_if_fail (GTK_IS_WIDGET (plugin->child));

      icon_name = garcon_menu_item_get_icon_name (item);
      if (!panel_str_is_empty (icon_name))
        {
          if (g_path_is_absolute (icon_name))
            {
              GdkPixbuf *pixbuf;
              gint scale_factor;

              /* remember the icon name for recreating the pixbuf when panel size changes */
              g_free (plugin->icon_name);
              plugin->icon_name = g_strdup (icon_name);
              scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (plugin));
              pixbuf = gdk_pixbuf_new_from_file_at_size (icon_name,
                                                         icon_size * scale_factor,
                                                         icon_size * scale_factor,
                                                         NULL);
              if (pixbuf != NULL)
                {
                  plugin->surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
                  g_object_unref (pixbuf);
                }

              gtk_image_set_from_surface (GTK_IMAGE (plugin->child), plugin->surface);
            }
          else
            {
              gtk_image_set_from_icon_name (GTK_IMAGE (plugin->child), icon_name, icon_size);
              gtk_image_set_pixel_size (GTK_IMAGE (plugin->child), icon_size);
            }
        }

      panel_utils_set_atk_info (plugin->button,
          garcon_menu_item_get_name (item),
          garcon_menu_item_get_comment (item));
    }
  else
    {
      /* set fallback icon if there is no application icon (yet) */
      panel_return_if_fail (GTK_IS_WIDGET (plugin->child));
      gtk_image_set_from_icon_name (GTK_IMAGE (plugin->child),
                                    "org.xfce.panel.launcher", icon_size);
    }
}



static void
launcher_plugin_add_desktop_actions (GtkWidget *widget, gpointer user_data)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (user_data);

  panel_return_if_fail (GTK_IS_WIDGET (widget));
  panel_return_if_fail (GTK_IS_MENU (plugin->action_menu));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* Pack the action menu item into the plugin's context menu */
  g_object_ref (widget);
  gtk_container_remove (GTK_CONTAINER (plugin->action_menu), widget);
  xfce_panel_plugin_menu_insert_item (XFCE_PANEL_PLUGIN (plugin), GTK_MENU_ITEM (widget));
  g_object_unref (widget);
}



static void
launcher_plugin_button_update_action_menu (LauncherPlugin *plugin)
{
  GarconMenuItem *item = NULL;
  GList          *list;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->menu == NULL);

  if (G_LIKELY (plugin->items != NULL))
    item = GARCON_MENU_ITEM (plugin->items->data);

  xfce_panel_plugin_menu_destroy (XFCE_PANEL_PLUGIN (plugin));
  if (plugin->action_menu)
    {
      gtk_widget_destroy (GTK_WIDGET (plugin->action_menu));
      plugin->action_menu = NULL;
    }
  else if (item != NULL && (list = garcon_menu_item_get_actions (item)) != NULL)
    {
      g_list_free (list);
      plugin->action_menu = GTK_WIDGET (garcon_gtk_menu_get_desktop_actions_menu (item));
      if (plugin->action_menu)
        {
          gtk_menu_set_reserve_toggle_size (GTK_MENU (plugin->action_menu), FALSE);
          gtk_container_foreach (GTK_CONTAINER (plugin->action_menu),
                                 launcher_plugin_add_desktop_actions,
                                 plugin);
        }
    }
}



static void
launcher_plugin_button_state_changed (GtkWidget     *button_a,
                                      GtkStateFlags  state,
                                      GtkWidget     *button_b)
{
  if (gtk_widget_get_state_flags (button_a) != gtk_widget_get_state_flags (button_b)
      && (gtk_widget_get_state_flags (button_a) & GTK_STATE_FLAG_INSENSITIVE))
    gtk_widget_set_state_flags (button_b, gtk_widget_get_state_flags (button_a), TRUE);
}



static gboolean
launcher_plugin_button_press_event (GtkWidget      *button,
                                    GdkEventButton *event,
                                    LauncherPlugin *plugin)
{
  guint modifiers;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* do nothing on anything else then a single click */
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  /* get the default accelerator modifier mask */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  /* leave when button 1 is not pressed or shift is pressed */
  if (event->button != 1 || modifiers == GDK_CONTROL_MASK)
    return FALSE;

  if (ARROW_INSIDE_BUTTON (plugin))
    {
      /* directly popup the menu */
      launcher_plugin_menu_popup (plugin);
    }
  else if (plugin->menu_timeout_id == 0
           && LIST_HAS_TWO_OR_MORE_ENTRIES (plugin->items))
    {
      /* start the popup timeout */
      plugin->menu_timeout_id =
        gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT_IDLE,
                                      MENU_POPUP_DELAY,
                                      launcher_plugin_menu_popup, plugin,
                                      launcher_plugin_menu_popup_destroyed);
    }

  return FALSE;
}



static gboolean
launcher_plugin_button_release_event (GtkWidget      *button,
                                      GdkEventButton *event,
                                      LauncherPlugin *plugin)
{
  GarconMenuItem *item;
  GdkScreen      *screen;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* remove a delayed popup timeout */
  if (plugin->menu_timeout_id != 0)
    g_source_remove (plugin->menu_timeout_id);

  /* leave when there are no menu items or there is an internal arrow */
  if (plugin->items == NULL
      || ARROW_INSIDE_BUTTON (plugin))
    return FALSE;

  /* leave if button release happens outside of launcher */
  if (event->x < 0 || event->x > gdk_window_get_width (event->window) ||
      event->y < 0 || event->y > gdk_window_get_height (event->window))
    return FALSE;

  /* get the menu item and the screen */
  item = GARCON_MENU_ITEM (plugin->items->data);
  screen = gtk_widget_get_screen (button);

  /* launcher the entry */
  if (event->button == 1)
    launcher_plugin_item_exec (item, event->time, screen, NULL);
  else if (event->button == 2)
    launcher_plugin_item_exec_from_clipboard (item, event->time, screen);
  else
    return TRUE;

  return FALSE;
}



static gboolean
launcher_plugin_button_query_tooltip (GtkWidget      *widget,
                                      gint            x,
                                      gint            y,
                                      gboolean        keyboard_mode,
                                      GtkTooltip     *tooltip,
                                      LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (!plugin->disable_tooltips, FALSE);

  /* check if we show tooltips */
  if (plugin->arrow_position == LAUNCHER_ARROW_INTERNAL
      || plugin->items == NULL
      || plugin->items->data == NULL)
    return FALSE;

  return launcher_plugin_item_query_tooltip (widget, x, y, keyboard_mode, tooltip, plugin->items->data);
}



static void
launcher_plugin_button_drag_data_received (GtkWidget        *widget,
                                           GdkDragContext   *context,
                                           gint              x,
                                           gint              y,
                                           GtkSelectionData *selection_data,
                                           guint             info,
                                           guint             drag_time,
                                           LauncherPlugin   *plugin)
{
  GSList *uri_list;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* leave when there are not items or the arrow is internal */
  if (ARROW_INSIDE_BUTTON (plugin) || plugin->items == NULL)
    return;

  /* get the list of uris from the selection data */
  uri_list = launcher_plugin_uri_list_extract (selection_data);
  if (G_LIKELY (uri_list != NULL))
    {
      /* execute */
      launcher_plugin_item_exec (GARCON_MENU_ITEM (plugin->items->data),
                                 gtk_get_current_event_time (),
                                 gtk_widget_get_screen (widget),
                                 uri_list);

      launcher_plugin_uri_list_free (uri_list);
    }

  /* finish the drag */
  gtk_drag_finish (context, TRUE, FALSE, drag_time);
}



static GdkAtom
launcher_plugin_supported_drop (GdkDragContext *context,
                                GtkWidget      *widget)
{
  GList           *li;
  GdkAtom          target;
  guint            i;
  GdkModifierType  modifiers = 0;

  /* do not handle drops if control is pressed */
  gdk_window_get_device_position (gtk_widget_get_window (widget),
                                  gdk_drag_context_get_device(context),
                                  NULL, NULL, &modifiers);
  if (PANEL_HAS_FLAG (modifiers, GDK_CONTROL_MASK))
    return GDK_NONE;

  /* check if we support the target */
  for (li = gdk_drag_context_list_targets (context); li; li = li->next)
    {
      target = GDK_POINTER_TO_ATOM (li->data);
      for (i = 0; i < G_N_ELEMENTS (drop_targets); i++)
        if (target == gdk_atom_intern_static_string (drop_targets[i].target))
          return target;
    }

  return GDK_NONE;
}



static gboolean
launcher_plugin_button_drag_motion (GtkWidget      *widget,
                                    GdkDragContext *context,
                                    gint            x,
                                    gint            y,
                                    guint           drag_time,
                                    LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  if (launcher_plugin_supported_drop (context, widget) == GDK_NONE)
    return FALSE;

  /* do nothing if the plugin is empty */
  if (plugin->items == NULL)
    {
      /* not a drop zone */
      gdk_drag_status (context, 0, drag_time);
      return FALSE;
    }

  /* highlight the button if this is a launcher button */
  if (NO_ARROW_INSIDE_BUTTON (plugin))
    {
      gdk_drag_status (context, GDK_ACTION_COPY, drag_time);
      gtk_drag_highlight (widget);
      return TRUE;
    }

  /* handle the popup menu */
  return launcher_plugin_arrow_drag_motion (widget, context, x, y,
                                            drag_time, plugin);
}



static gboolean
launcher_plugin_button_drag_drop (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  gint            x,
                                  gint            y,
                                  guint           drag_time,
                                  LauncherPlugin *plugin)
{
  GdkAtom target;

  target = launcher_plugin_supported_drop (context, widget);
  if (target == GDK_NONE)
    return FALSE;

  gtk_drag_get_data (widget, context, target, drag_time);

  return TRUE;
}



static void
launcher_plugin_button_drag_leave (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   guint           drag_time,
                                   LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* unhighlight the widget or make sure the menu is deactivated */
  if (NO_ARROW_INSIDE_BUTTON (plugin))
    gtk_drag_unhighlight (widget);
  else
    launcher_plugin_arrow_drag_leave (widget, context, drag_time, plugin);
}



static gboolean
launcher_plugin_button_draw (GtkWidget      *widget,
                             cairo_t        *cr,
                             LauncherPlugin *plugin)
{
  GtkArrowType      arrow_type;
  gdouble           angle;
  gint              size, x, y, offset;
  GtkAllocation     allocation;
  GtkStyleContext  *ctx;
  GtkBorder         padding;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* leave when the arrow is not shown inside the button */
  if (NO_ARROW_INSIDE_BUTTON (plugin))
    return FALSE;

  /* get the arrow type */
  arrow_type = xfce_arrow_button_get_arrow_type (XFCE_ARROW_BUTTON (plugin->arrow));

  /* style thickness */
  ctx = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (ctx, gtk_widget_get_state_flags (widget), &padding);

  /* size of the arrow and the start coordinates */
  gtk_widget_get_allocation (widget, &allocation);

  size = allocation.width / 3;
  x = padding.left;
  y = padding.top;
  offset = size + padding.left + padding.right;
  angle = 1.5 * G_PI;

  /* calculate the position based on the arrow type */
  switch (arrow_type)
    {
    case GTK_ARROW_UP:
      /* north east */
      x += allocation.width - offset;
      angle = 0.0 * G_PI;
      break;

    case GTK_ARROW_DOWN:
      /* south west */
      y += allocation.height - offset;
      angle = 1.0 * G_PI;
      break;

    case GTK_ARROW_RIGHT:
      /* south east */
      x += allocation.width - offset;
      y += allocation.height - offset;
      angle = 0.5 * G_PI;
      break;

    default:
      /* north west */
      break;
    }

  /* paint the arrow */
  gtk_render_arrow (ctx, cr, angle, (gdouble) x, (gdouble) y, (gdouble) size);

  return FALSE;
}



static void
launcher_plugin_arrow_visibility (LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  if (plugin->arrow_position != LAUNCHER_ARROW_INTERNAL
       && LIST_HAS_TWO_OR_MORE_ENTRIES (plugin->items))
    gtk_widget_show (plugin->arrow);
  else
    gtk_widget_hide (plugin->arrow);
}



static gboolean
launcher_plugin_arrow_press_event (GtkWidget      *button,
                                   GdkEventButton *event,
                                   LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* only popup when button 1 is pressed */
  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      launcher_plugin_menu_popup (plugin);
      return FALSE;
    }

  return TRUE;
}




static gboolean
launcher_plugin_arrow_drag_motion (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   gint            x,
                                   gint            y,
                                   guint           drag_time,
                                   LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  if (launcher_plugin_supported_drop (context, widget) == GDK_NONE)
    return FALSE;

  /* the arrow is not a drop zone */
  gdk_drag_status (context, 0, drag_time);

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->arrow)))
    {
      /* make the toggle button active */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), TRUE);

      /* start the popup timeout */
      plugin->menu_timeout_id =
        gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT_IDLE, MENU_POPUP_DELAY,
                                      launcher_plugin_menu_popup, plugin,
                                      launcher_plugin_menu_popup_destroyed);
    }

  return TRUE;
}



static gboolean
launcher_plugin_arrow_drag_leave_timeout (gpointer user_data)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (user_data);
  gint            pointer_x, pointer_y;
  GtkWidget      *menu = plugin->menu;
  gint            menu_x, menu_y, menu_w, menu_h;
  GdkDevice      *device;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (menu == NULL || gtk_widget_get_has_window (menu), FALSE);

  /* leave when the menu is destroyed */
  if (G_UNLIKELY (plugin->menu == NULL))
    return FALSE;

  /* leave if the menu is already hidden */
  if (! gtk_widget_get_visible (plugin->menu))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
      return FALSE;
    }

  device = gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (menu)));
  if (device == NULL)
    return FALSE;

  /* get the pointer position */
  gdk_device_get_position (device, NULL, &pointer_x, &pointer_y);

  /* get the menu position */
  gdk_window_get_root_origin (gtk_widget_get_window (menu), &menu_x, &menu_y);
  menu_w = gdk_window_get_width (gtk_widget_get_window (menu));
  menu_h = gdk_window_get_height (gtk_widget_get_window (menu));

  /* check if we should hide the menu */
  if (pointer_x < menu_x || pointer_x > menu_x + menu_w
      || pointer_y < menu_y || pointer_y > menu_y + menu_h)
    {
      /* hide the menu */
      gtk_widget_hide (gtk_widget_get_toplevel (menu));
      gtk_widget_hide (menu);

      /* inactive the toggle button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
    }

  return FALSE;
}



static void
launcher_plugin_arrow_drag_leave (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  guint           drag_time,
                                  LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  if (plugin->menu_timeout_id != 0)
    {
      /* stop the popup timeout */
      g_source_remove (plugin->menu_timeout_id);

      /* inactive the toggle button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
    }
  else
    {
      /* start a timeout to give the user some time to drag to the menu */
      gdk_threads_add_timeout (MENU_POPUP_DELAY, launcher_plugin_arrow_drag_leave_timeout, plugin);
    }
}



static void
launcher_plugin_tooltip_icon_invalidate (GObject *object)
{
  g_object_set_data (object, "tooltip-icon", NULL);
}



static void
launcher_plugin_icon_invalidate (LauncherPlugin *plugin)
{
  if (plugin->surface != NULL)
    {
      cairo_surface_destroy (plugin->surface);
      plugin->surface = NULL;
    }
}



static gboolean
launcher_plugin_item_query_tooltip (GtkWidget      *widget,
                                    gint            x,
                                    gint            y,
                                    gboolean        keyboard_mode,
                                    GtkTooltip     *tooltip,
                                    GarconMenuItem *item)
{
  gchar *markup;
  const gchar *name, *comment;
  GIcon *icon;

  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* require atleast an item name */
  name = garcon_menu_item_get_name (item);
  if (panel_str_is_empty (name))
    return FALSE;

  comment = garcon_menu_item_get_comment (item);
  if (!panel_str_is_empty (comment))
    {
      markup = g_markup_printf_escaped ("<b>%s</b>\n%s", name, comment);
      gtk_tooltip_set_markup (tooltip, markup);
      g_free (markup);
    }
  else
    {
      gtk_tooltip_set_text (tooltip, name);
    }

  icon = g_object_get_data (G_OBJECT (widget), "tooltip-icon");
  if (G_UNLIKELY (icon == NULL))
    {
      icon = launcher_plugin_tooltip_icon (garcon_menu_item_get_icon_name (item));
      if (G_LIKELY (icon != NULL))
        {
          g_object_set_data_full (G_OBJECT (widget), "tooltip-icon", icon, g_object_unref);
        }
    }

  gtk_tooltip_set_icon_from_gicon (tooltip, icon, GTK_ICON_SIZE_DND);

  return TRUE;
}



static gboolean
launcher_plugin_item_exec_on_screen (GarconMenuItem *item,
                                     guint32         event_time,
                                     GdkScreen      *screen,
                                     GSList         *uri_list)
{
  GError      *error = NULL;
  gchar      **argv;
  gboolean     succeed = FALSE;
  gchar       *command, *uri;
  const gchar *icon;

  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  panel_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  /* get the command */
  command = (gchar*) garcon_menu_item_get_command (item);
  panel_return_val_if_fail (!panel_str_is_empty (command), FALSE);

  /* expand the field codes */
  icon = garcon_menu_item_get_icon_name (item);
  uri = garcon_menu_item_get_uri (item);
  command = xfce_expand_desktop_entry_field_codes (command, uri_list, icon,
                                                   garcon_menu_item_get_name (item),
                                                   uri,
                                                   garcon_menu_item_requires_terminal (item));
  g_free (uri);

  /* parse the execute command */
  if (g_shell_parse_argv (command, NULL, &argv, &error))
    {
      /* launch the command on the screen */
      succeed = xfce_spawn (screen,
                            garcon_menu_item_get_path (item),
                            argv, NULL, G_SPAWN_SEARCH_PATH,
                            garcon_menu_item_supports_startup_notification (item),
                            event_time, icon, TRUE, &error);

      g_strfreev (argv);
    }

  if (G_UNLIKELY (!succeed))
    {
      /* show an error dialog */
      xfce_dialog_show_error (NULL, error, _("Failed to execute command \"%s\"."), command);
      g_error_free (error);
    }

  g_free (command);

  return succeed;
}



static void
launcher_plugin_item_exec (GarconMenuItem *item,
                           guint32         event_time,
                           GdkScreen      *screen,
                           GSList         *uri_list)
{
  GSList      *li, fake;
  gboolean     proceed = TRUE;
  const gchar *command;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (GDK_IS_SCREEN (screen));

  /* leave when there is nothing to execute */
  command = garcon_menu_item_get_command (item);
  if (panel_str_is_empty (command))
    return;

  if (G_UNLIKELY (uri_list != NULL
      && strstr (command, "%F") == NULL
      && strstr (command, "%U") == NULL))
    {
      fake.next = NULL;

      /* run an instance for each file, break on the first error */
      for (li = uri_list; li != NULL && proceed; li = li->next)
        {
          fake.data = li->data;
          proceed = launcher_plugin_item_exec_on_screen (item, event_time, screen, &fake);
        }
    }
  else
    {
      launcher_plugin_item_exec_on_screen (item, event_time, screen, uri_list);
    }
}



static void
launcher_plugin_item_exec_from_clipboard (GarconMenuItem *item,
                                          guint32         event_time,
                                          GdkScreen      *screen)
{
  GtkClipboard     *clipboard;
  gchar            *text = NULL;
  //GSList           *uri_list;
  //GtkSelectionData  data;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (GDK_IS_SCREEN (screen));

  /* get the primary clipboard text */
  clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
  if (G_LIKELY (clipboard))
    text = gtk_clipboard_wait_for_text (clipboard);

  /* try the secondary keayboard if the text is empty */
  if (panel_str_is_empty (text))
    {
      /* get the secondary clipboard text */
      clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
      if (G_LIKELY (clipboard))
        text = gtk_clipboard_wait_for_text (clipboard);
    }

  if (!panel_str_is_empty (text))
    {
      /* create fake selection data */
      //data.data = (guchar *) text;      //HOWTO?
      //data.length = strlen (text);
      //data.target = GDK_NONE;

      /* extract the uris from the selection data */
      //uri_list = launcher_plugin_uri_list_extract (&data);

      /* launch with the uri list */
      //launcher_plugin_item_exec (item, event_time,
      //                           screen, uri_list);

      //launcher_plugin_uri_list_free (uri_list);
    }

  g_free (text);
}



static GSList *
launcher_plugin_uri_list_extract (GtkSelectionData *data)
{
  GSList  *list = NULL;
  gchar  **array;
  guint    i;
  gchar   *uri;

  /* leave if there is no data */
  if (gtk_selection_data_get_length (data) <= 0)
    return NULL;

  /* extract the files */
  if (gtk_selection_data_get_target (data) == gdk_atom_intern_static_string ("text/uri-list"))
    {
      /* extract the list of uris */
      array = g_uri_list_extract_uris ((gchar *) gtk_selection_data_get_data (data));
      if (G_UNLIKELY (array == NULL))
        return NULL;

      /* create the list of uris */
      for (i = 0; array[i] != NULL; i++)
        {
          if (!panel_str_is_empty (array[i]))
            list = g_slist_prepend (list, array[i]);
          else
            g_free (array[i]);
        }

      g_free (array);
    }
  else
    {
      /* split the data on new lines */
      array = g_strsplit_set ((const gchar *) gtk_selection_data_get_data (data), "\n\r", -1);
      if (G_UNLIKELY (array == NULL))
        return NULL;

      /* create the list of uris */
      for (i = 0; array[i] != NULL; i++)
        {
          /* skip empty strings */
          if (!!panel_str_is_empty (array[i]))
            continue;

          uri = NULL;

          if (g_path_is_absolute (array[i]))
            uri = g_filename_to_uri (array[i], NULL, NULL);
          else if (g_uri_is_valid (array[i], G_URI_FLAGS_NONE, NULL))
            uri = g_strdup (array[i]);

          /* append the uri if we extracted one */
          if (G_LIKELY (uri != NULL))
            list = g_slist_prepend (list, uri);
        }

      g_strfreev (array);
    }

  return g_slist_reverse (list);
}



static void
launcher_plugin_uri_list_free (GSList *uri_list)
{
  if (uri_list != NULL)
    {
      g_slist_foreach (uri_list, (GFunc) (void (*)(void)) g_free, NULL);
      g_slist_free (uri_list);
    }
}



GSList *
launcher_plugin_get_items (LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), NULL);

  /* set extra reference and return a copy of the list */
  g_slist_foreach (plugin->items, (GFunc) (void (*)(void)) g_object_ref, NULL);
  return g_slist_copy (plugin->items);
}



gchar *
launcher_plugin_unique_filename (LauncherPlugin *plugin)
{
  gchar        *filename, *path;
  static guint  counter = 0;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), NULL);

  filename = g_strdup_printf (RELATIVE_CONFIG_PATH G_DIR_SEPARATOR_S "%" G_GINT64_FORMAT "%d.desktop",
                              xfce_panel_plugin_get_name (XFCE_PANEL_PLUGIN (plugin)),
                              xfce_panel_plugin_get_unique_id (XFCE_PANEL_PLUGIN (plugin)),
                              g_get_real_time () / G_USEC_PER_SEC,
                              ++counter);
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, filename, TRUE);
  g_free (filename);

  return path;

}



static void
launcher_plugin_garcon_menu_pool_add (GarconMenu *menu,
                                      GHashTable *pool)
{
  GList          *li, *items;
  GList          *menus;
  GarconMenuItem *item;
  const gchar    *desktop_id;

  panel_return_if_fail (GARCON_IS_MENU (menu));

  items = garcon_menu_get_items (menu);
  for (li = items; li != NULL; li = li->next)
    {
      item = GARCON_MENU_ITEM (li->data);
      panel_assert (GARCON_IS_MENU_ITEM (item));

      /* skip invisible items */
      if (!garcon_menu_element_get_visible (GARCON_MENU_ELEMENT (item)))
        continue;

      /* skip duplicates */
      desktop_id = garcon_menu_item_get_desktop_id (item);
      if (g_hash_table_lookup (pool, desktop_id) != NULL)
        continue;

      /* insert the item */
      g_hash_table_insert (pool, g_strdup (desktop_id),
                           g_object_ref (G_OBJECT (item)));
    }
  g_list_free (items);

  menus = garcon_menu_get_menus (menu);
  for (li = menus; li != NULL; li = li->next)
    launcher_plugin_garcon_menu_pool_add (li->data, pool);
  g_list_free (menus);
}



GHashTable *
launcher_plugin_garcon_menu_pool (void)
{
  GHashTable *pool;
  GarconMenu *menu;
  GError     *error = NULL;

  /* always return a hash table, even if it's empty */
  pool = g_hash_table_new_full (g_str_hash, g_str_equal,
                                (GDestroyNotify) g_free,
                                (GDestroyNotify) g_object_unref);

  menu = garcon_menu_new_applications ();
  if (G_LIKELY (menu != NULL))
    {
      if (garcon_menu_load (menu, NULL, &error))
        {
          launcher_plugin_garcon_menu_pool_add (menu, pool);
        }
      else
        {
          g_warning ("Failed to load the applications menu: %s.", error->message);
          g_error_free (error);
        }

      g_object_unref (G_OBJECT (menu));
    }
  else
    {
      g_warning ("Failed to create the applications menu");
    }

  return pool;
}



gboolean
launcher_plugin_item_is_editable (LauncherPlugin *plugin,
                                  GarconMenuItem *item,
                                  gboolean       *can_delete)
{
  GFile     *item_file;
  gboolean   editable = FALSE;
  GFileInfo *file_info;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  item_file = garcon_menu_item_get_file (item);
  if (!g_file_has_prefix (item_file, plugin->config_directory))
    goto out;

  file_info = g_file_query_info (item_file,
                                 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE ","
                                 G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
                                 G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (G_LIKELY (file_info != NULL))
    {
      editable = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

      if (editable && can_delete != NULL)
        *can_delete = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);

      g_object_unref (G_OBJECT (file_info));
    }

out:
  g_object_unref (G_OBJECT (item_file));

  return editable;
}
