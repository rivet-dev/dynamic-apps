/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
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

#include <exo/exo.h>
#include <gio/gio.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>
#include <common/panel-private.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#include "directorymenu.h"
#include "directorymenu-dialog_ui.h"

#define DEFAULT_ICON_NAME "folder"
#define DIALOG_ICON_SIZE 48

#define DIALOG_RESPONSE_CREATE 0
#define DIALOG_RESPONSE_OPEN 1

struct _DirectoryMenuPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _DirectoryMenuPlugin
{
  XfcePanelPlugin __parent__;

  GtkWidget       *button;
  GtkWidget       *icon;

  GFile           *base_directory;
  gchar           *icon_name;
  guint            open_folder;
  guint            open_in_terminal;
  guint            new_folder;
  guint            new_document;
  gchar           *file_pattern;
  guint            hidden_files : 1;

  GSList          *patterns;
};

enum
{
  PROP_0,
  PROP_BASE_DIRECTORY,
  PROP_ICON_NAME,
  PROP_FILE_PATTERN,
  PROP_HIDDEN_FILES,
  PROP_OPEN_FOLDER,
  PROP_OPEN_IN_TERMINAL,
  PROP_NEW_FOLDER,
  PROP_NEW_DOCUMENT
};



static void      directory_menu_plugin_get_property         (GObject             *object,
                                                             guint                prop_id,
                                                             GValue              *value,
                                                             GParamSpec          *pspec);
static void      directory_menu_plugin_set_property         (GObject             *object,
                                                             guint                prop_id,
                                                             const GValue        *value,
                                                             GParamSpec          *pspec);
static void      directory_menu_plugin_construct            (XfcePanelPlugin     *panel_plugin);
static void      directory_menu_plugin_free_file_patterns   (DirectoryMenuPlugin *plugin);
static void      directory_menu_plugin_free_data            (XfcePanelPlugin     *panel_plugin);
static gboolean  directory_menu_plugin_size_changed         (XfcePanelPlugin     *panel_plugin,
                                                             gint                 size);
static void      directory_menu_plugin_configure_plugin     (XfcePanelPlugin     *panel_plugin);
static gboolean  directory_menu_plugin_remote_event         (XfcePanelPlugin     *panel_plugin,
                                                             const gchar         *name,
                                                             const GValue        *value);
static void      directory_menu_plugin_menu                 (GtkWidget           *button,
                                                             DirectoryMenuPlugin *plugin);



/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (DirectoryMenuPlugin, directory_menu_plugin)



static GQuark menu_file = 0;


static void
directory_menu_plugin_class_init (DirectoryMenuPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = directory_menu_plugin_get_property;
  gobject_class->set_property = directory_menu_plugin_set_property;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = directory_menu_plugin_construct;
  plugin_class->free_data = directory_menu_plugin_free_data;
  plugin_class->size_changed = directory_menu_plugin_size_changed;
  plugin_class->configure_plugin = directory_menu_plugin_configure_plugin;
  plugin_class->remote_event = directory_menu_plugin_remote_event;

  g_object_class_install_property (gobject_class,
                                   PROP_BASE_DIRECTORY,
                                   g_param_spec_string ("base-directory",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_OPEN_FOLDER,
                                   g_param_spec_boolean ("open-folder",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_OPEN_IN_TERMINAL,
                                   g_param_spec_boolean ("open-in-terminal",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_NEW_FOLDER,
                                   g_param_spec_boolean ("new-folder",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_NEW_DOCUMENT,
                                   g_param_spec_boolean ("new-document",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_FILE_PATTERN,
                                   g_param_spec_string ("file-pattern",
                                                        NULL, NULL,
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN_FILES,
                                   g_param_spec_boolean ("hidden-files",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  menu_file = g_quark_from_static_string ("dir-menu-file");
}



static void
directory_menu_plugin_init (DirectoryMenuPlugin *plugin)
{
  plugin->button = xfce_panel_create_toggle_button ();
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
  gtk_widget_set_name (plugin->button, "directorymenu-button");
  gtk_button_set_relief (GTK_BUTTON (plugin->button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT (plugin->button), "toggled",
      G_CALLBACK (directory_menu_plugin_menu), plugin);

  plugin->icon_name = g_strdup (DEFAULT_ICON_NAME);
  plugin->icon = gtk_image_new_from_icon_name (plugin->icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (plugin->button), plugin->icon);
  gtk_widget_show (plugin->icon);

  plugin->open_folder = TRUE;
  plugin->open_in_terminal = TRUE;
  plugin->new_folder = TRUE;
  plugin->new_document = TRUE;
}



static void
directory_menu_plugin_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  DirectoryMenuPlugin *plugin = XFCE_DIRECTORY_MENU_PLUGIN (object);
  gchar               *str;

  switch (prop_id)
    {
    case PROP_BASE_DIRECTORY:
      if (g_file_is_native (plugin->base_directory))
        str = g_file_get_path (plugin->base_directory);
      else
        str = g_file_get_uri (plugin->base_directory);
      g_value_take_string (value, str);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, plugin->icon_name);
      break;

    case PROP_OPEN_FOLDER:
      g_value_set_boolean (value, plugin->open_folder);
      break;

    case PROP_OPEN_IN_TERMINAL:
      g_value_set_boolean (value, plugin->open_in_terminal);
      break;

    case PROP_NEW_FOLDER:
      g_value_set_boolean (value, plugin->new_folder);
      break;

    case PROP_NEW_DOCUMENT:
      g_value_set_boolean (value, plugin->new_document);
      break;

    case PROP_FILE_PATTERN:
      g_value_set_string (value, panel_str_is_empty (plugin->file_pattern) ?
          "" : plugin->file_pattern);
      break;

    case PROP_HIDDEN_FILES:
      g_value_set_boolean (value, plugin->hidden_files);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
directory_menu_plugin_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  DirectoryMenuPlugin  *plugin = XFCE_DIRECTORY_MENU_PLUGIN (object);
  gchar                *display_name;
  gchar               **array;
  guint                 i;
  gint                  size;
  const gchar          *path;

  switch (prop_id)
    {
    case PROP_BASE_DIRECTORY:
      path = g_value_get_string (value);
      if (panel_str_is_empty (path))
        path = g_get_home_dir ();

      if (plugin->base_directory != NULL)
        g_object_unref (G_OBJECT (plugin->base_directory));
      plugin->base_directory = g_file_new_for_commandline_arg (path);

      display_name = g_file_get_parse_name (plugin->base_directory);
      gtk_widget_set_tooltip_text (plugin->button, display_name);

      panel_utils_set_atk_info (plugin->button, _("Directory Menu"), display_name);

      g_free (display_name);
      break;

    case PROP_ICON_NAME:
      g_free (plugin->icon_name);
      plugin->icon_name =
        panel_str_is_empty (g_value_get_string (value)) ? g_strdup (DEFAULT_ICON_NAME)
                                                        : g_value_dup_string (value);
      size = xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin));
      directory_menu_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin), size);
      break;

    case PROP_OPEN_FOLDER:
      plugin->open_folder = g_value_get_boolean (value);
      break;

    case PROP_OPEN_IN_TERMINAL:
      plugin->open_in_terminal = g_value_get_boolean (value);
      break;

    case PROP_NEW_FOLDER:
      plugin->new_folder = g_value_get_boolean (value);
      break;

    case PROP_NEW_DOCUMENT:
      plugin->new_document = g_value_get_boolean (value);
      break;

    case PROP_FILE_PATTERN:
      g_free (plugin->file_pattern);
      plugin->file_pattern = g_value_dup_string (value);

      directory_menu_plugin_free_file_patterns (plugin);

      array = g_strsplit (plugin->file_pattern, ";", -1);
      if (G_LIKELY (array != NULL))
        {
          for (i = 0; array[i] != NULL; i++)
            if (!panel_str_is_empty (array[i]))
                plugin->patterns = g_slist_prepend (plugin->patterns,
                    g_pattern_spec_new (array[i]));

          g_strfreev (array);
        }
      break;

    case PROP_HIDDEN_FILES:
      plugin->hidden_files = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
directory_menu_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  DirectoryMenuPlugin *plugin = XFCE_DIRECTORY_MENU_PLUGIN (panel_plugin);
  const PanelProperty  properties[] =
  {
    { "base-directory", G_TYPE_STRING },
    { "icon-name", G_TYPE_STRING },
    { "open-folder", G_TYPE_BOOLEAN },
    { "open-in-terminal", G_TYPE_BOOLEAN },
    { "new-folder", G_TYPE_BOOLEAN },
    { "new-document", G_TYPE_BOOLEAN },
    { "file-pattern", G_TYPE_STRING },
    { "hidden-files", G_TYPE_BOOLEAN },
    { NULL }
  };

  xfce_panel_plugin_menu_show_configure (panel_plugin);

  xfce_panel_plugin_set_small (panel_plugin, TRUE);

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  if (plugin->base_directory == NULL)
    g_object_set (G_OBJECT (plugin), "base-directory", g_get_home_dir (), NULL);

  gtk_widget_show (plugin->button);
}



static void
directory_menu_plugin_free_file_patterns (DirectoryMenuPlugin *plugin)
{
  GSList *li;

  panel_return_if_fail (XFCE_IS_DIRECTORY_MENU_PLUGIN (plugin));

  for (li = plugin->patterns; li != NULL; li = li->next)
    g_pattern_spec_free (li->data);

  g_slist_free (plugin->patterns);
  plugin->patterns = NULL;
}



static void
directory_menu_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  DirectoryMenuPlugin *plugin = XFCE_DIRECTORY_MENU_PLUGIN (panel_plugin);

  if (plugin->base_directory != NULL)
    g_object_unref (G_OBJECT (plugin->base_directory));
  g_free (plugin->icon_name);
  g_free (plugin->file_pattern);

  directory_menu_plugin_free_file_patterns (plugin);
}



static gboolean
directory_menu_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                    gint             size)
{
  DirectoryMenuPlugin *plugin = XFCE_DIRECTORY_MENU_PLUGIN (panel_plugin);
  gint icon_size;

  /* force a square button */
  size /= xfce_panel_plugin_get_nrows (panel_plugin);
  gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), size, size);
  icon_size = xfce_panel_plugin_get_icon_size (panel_plugin);
  xfce_panel_set_image_from_source (GTK_IMAGE (plugin->icon), plugin->icon_name, NULL, icon_size,
                                    gtk_widget_get_scale_factor (GTK_WIDGET (plugin)));

  return TRUE;
}




static void
directory_menu_plugin_configure_plugin_file_set (GtkFileChooserButton *button,
                                                 DirectoryMenuPlugin  *plugin)
{
  gchar *uri;

  panel_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));
  panel_return_if_fail (XFCE_IS_DIRECTORY_MENU_PLUGIN (plugin));

  uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
  g_object_set (G_OBJECT (plugin), "base-directory", uri, NULL);
  g_free (uri);
}



static void
directory_menu_plugin_configure_plugin_icon_chooser (GtkWidget           *button,
                                                     DirectoryMenuPlugin *plugin)
{
  GtkWidget *chooser, *image;
  gchar     *icon;

  panel_return_if_fail (XFCE_IS_DIRECTORY_MENU_PLUGIN (plugin));

  chooser = exo_icon_chooser_dialog_new (_("Select An Icon"),
                                         GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_OK"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);

  exo_icon_chooser_dialog_set_icon (EXO_ICON_CHOOSER_DIALOG (chooser), plugin->icon_name);

  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      icon = exo_icon_chooser_dialog_get_icon (EXO_ICON_CHOOSER_DIALOG (chooser));
      g_object_set (G_OBJECT (plugin), "icon-name", icon, NULL);
      g_free (icon);

      image = gtk_image_new ();
      xfce_panel_set_image_from_source (GTK_IMAGE (image), plugin->icon_name,
                                        NULL, DIALOG_ICON_SIZE,
                                        gtk_widget_get_scale_factor (GTK_WIDGET (plugin)));
      gtk_container_remove (GTK_CONTAINER (button), gtk_bin_get_child (GTK_BIN (button)));
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_widget_show (image);
    }

  gtk_widget_destroy (chooser);
}



static void
directory_menu_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  DirectoryMenuPlugin *plugin = XFCE_DIRECTORY_MENU_PLUGIN (panel_plugin);
  GtkBuilder          *builder;
  GtkWidget           *image;
  GObject             *dialog, *object;

  /* setup the dialog */
  PANEL_UTILS_LINK_4UI
  builder = panel_utils_builder_new (panel_plugin, directorymenu_dialog_ui,
                                     directorymenu_dialog_ui_length, &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

  object = gtk_builder_get_object (builder, "base-directory");
  panel_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (object));
  if (!gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (object),
                                                 plugin->base_directory, NULL))
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (object), g_get_home_dir ());
  g_signal_connect (G_OBJECT (object), "selection-changed",
     G_CALLBACK (directory_menu_plugin_configure_plugin_file_set), plugin);

  object = gtk_builder_get_object (builder, "icon-button");
  panel_return_if_fail (GTK_IS_BUTTON (object));
  g_signal_connect (G_OBJECT (object), "clicked",
     G_CALLBACK (directory_menu_plugin_configure_plugin_icon_chooser), plugin);

  image = gtk_image_new ();
  xfce_panel_set_image_from_source (GTK_IMAGE (image), plugin->icon_name,
                                    NULL, DIALOG_ICON_SIZE,
                                    gtk_widget_get_scale_factor (GTK_WIDGET (plugin)));
  gtk_container_add (GTK_CONTAINER (object), image);
  gtk_widget_show (image);

  object = gtk_builder_get_object (builder, "open-folder");
  panel_return_if_fail (GTK_IS_CHECK_BUTTON (object));
  g_object_bind_property (G_OBJECT (plugin), "open-folder",
                          G_OBJECT (object), "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  object = gtk_builder_get_object (builder, "open-in-terminal");
  panel_return_if_fail (GTK_IS_CHECK_BUTTON (object));
  g_object_bind_property (G_OBJECT (plugin), "open-in-terminal",
                          G_OBJECT (object), "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  object = gtk_builder_get_object (builder, "new-folder");
  panel_return_if_fail (GTK_IS_CHECK_BUTTON (object));
  g_object_bind_property (G_OBJECT (plugin), "new-folder",
                          G_OBJECT (object), "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  object = gtk_builder_get_object (builder, "new-document");
  panel_return_if_fail (GTK_IS_CHECK_BUTTON (object));
  g_object_bind_property (G_OBJECT (plugin), "new-document",
                          G_OBJECT (object), "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  object = gtk_builder_get_object (builder, "file-pattern");
  panel_return_if_fail (GTK_IS_ENTRY (object));
  g_object_bind_property (G_OBJECT (plugin), "file-pattern",
                          G_OBJECT (object), "text",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  object = gtk_builder_get_object (builder, "hidden-files");
  panel_return_if_fail (GTK_IS_CHECK_BUTTON (object));
  g_object_bind_property (G_OBJECT (plugin), "hidden-files",
                          G_OBJECT (object), "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  gtk_widget_show (GTK_WIDGET (dialog));
}



static gboolean
directory_menu_plugin_remote_event (XfcePanelPlugin *panel_plugin,
                                    const gchar     *name,
                                    const GValue    *value)
{
  DirectoryMenuPlugin *plugin = XFCE_DIRECTORY_MENU_PLUGIN (panel_plugin);
  GtkWidget *invisible;

  panel_return_val_if_fail (value == NULL || G_IS_VALUE (value), FALSE);

  /* try next plugin or indicate that it failed */
  if (strcmp (name, "popup") != 0
      || ! gtk_widget_get_visible (GTK_WIDGET (panel_plugin)))
    return FALSE;

  invisible = gtk_invisible_new ();
  gtk_widget_show (invisible);

  /* a menu is already shown, don't popup another one */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->button))
      || ! panel_utils_device_grab (invisible))
    {
      gtk_widget_destroy (invisible);
      return TRUE;
    }

  /*
   * The menu will take over the grab when it is shown or it will be lost when destroying
   * invisible below. This way we are sure that other invocations of the command by
   * keyboard shortcut will not interfere.
   */
  if (value != NULL
      && G_VALUE_HOLDS_BOOLEAN (value)
      && g_value_get_boolean (value))
    {
      /* popup menu at pointer */
      directory_menu_plugin_menu (NULL, plugin);
    }
  else
    {
      /* popup menu at button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), TRUE);
    }

  gtk_widget_destroy (invisible);

  /* don't popup another menu */
  return TRUE;
}



static void
directory_menu_plugin_deactivate (GtkWidget           *menu,
                                  DirectoryMenuPlugin *plugin)
{
  panel_return_if_fail (plugin->button == NULL || GTK_IS_TOGGLE_BUTTON (plugin->button));
  panel_return_if_fail (GTK_IS_MENU (menu));

  if (plugin->button != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), FALSE);

  /* delay destruction so we can handle the activate event first */
  panel_utils_destroy_later (GTK_WIDGET (menu));
}



static gint
directory_menu_plugin_menu_sort (gconstpointer a,
                                 gconstpointer b)
{
  GFileType type_a = g_file_info_get_file_type (G_FILE_INFO (a));
  GFileType type_b = g_file_info_get_file_type (G_FILE_INFO (b));
  gboolean  hidden_a, hidden_b;
  const gchar *display_name_a, *display_name_b;
  gchar *sort_display_name_a, *sort_display_name_b;
  gint sort_value;

  if (type_a != type_b)
    {
      /* sort directories before files */
      if (type_a == G_FILE_TYPE_DIRECTORY)
        return -1;
      else if (type_b == G_FILE_TYPE_DIRECTORY)
        return 1;
    }

  hidden_a = g_file_info_get_is_hidden (G_FILE_INFO (a));
  hidden_b = g_file_info_get_is_hidden (G_FILE_INFO (b));

  /* sort hidden files above 'normal' files */
  if (hidden_a != hidden_b)
    return hidden_a ? -1 : 1;

  display_name_a = g_file_info_get_display_name (G_FILE_INFO (a));
  display_name_b = g_file_info_get_display_name (G_FILE_INFO (b));
  sort_display_name_a = g_utf8_collate_key_for_filename (display_name_a, -1);
  sort_display_name_b = g_utf8_collate_key_for_filename (display_name_b, -1);
  sort_value = strcmp (sort_display_name_a,
                       sort_display_name_b);
  g_free (sort_display_name_a);
  g_free (sort_display_name_b);
  return sort_value;
}



#ifdef HAVE_GIO_UNIX
static void
directory_menu_plugin_menu_launch_desktop_file (GtkWidget *mi,
                                                GAppInfo  *info)
{
  GdkAppLaunchContext *context;
  GIcon               *icon;
  GError              *error = NULL;
  GdkDisplay          *display;

  panel_return_if_fail (G_IS_APP_INFO (info));
  panel_return_if_fail (GTK_IS_WIDGET (mi));

  display = gtk_widget_get_display (mi);
  context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_screen (context, gtk_widget_get_screen (mi));
  gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());
  icon = g_app_info_get_icon (info);
  if (G_LIKELY (icon != NULL))
    gdk_app_launch_context_set_icon (context, icon);

  if (!g_app_info_launch (info, NULL, G_APP_LAUNCH_CONTEXT (context), &error))
    {
      xfce_dialog_show_error (NULL, error, _("Failed to launch application \"%s\""),
                              g_app_info_get_executable (info));
      g_error_free (error);
    }

  g_object_unref (G_OBJECT (context));
}
#endif



static void
directory_menu_plugin_menu_launch (GtkWidget *mi,
                                   GFile     *file)
{
  GAppInfo            *appinfo;
  GError              *error = NULL;
  gchar               *display_name;
  GList                fake_list = { NULL, };
  GdkAppLaunchContext *context;
  GFileInfo           *info;
  const gchar         *message;
  gboolean             result;
  GdkDisplay          *display;

  panel_return_if_fail (G_IS_FILE (file));
  panel_return_if_fail (GTK_IS_WIDGET (mi));

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                            G_FILE_QUERY_INFO_NONE, NULL, &error);
  if (G_UNLIKELY (info == NULL))
    {
      message = _("Failed to query content type for \"%s\"");
      goto err;
    }

  appinfo = g_app_info_get_default_for_type (g_file_info_get_content_type (info),
                                             !g_file_is_native (file));
  g_object_unref (G_OBJECT (info));
  if (G_LIKELY (appinfo == NULL))
    {
      message = _("No default application found for \"%s\"");
      goto err;
    }

  fake_list.data = file;

  display = gtk_widget_get_display (mi);
  context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_screen (context, gtk_widget_get_screen (mi));
  gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());

  result = g_app_info_launch (appinfo, &fake_list, G_APP_LAUNCH_CONTEXT (context), &error);
  g_object_unref (G_OBJECT (context));
  g_object_unref (G_OBJECT (appinfo));
  if (G_UNLIKELY (!result))
    {
      message = _("Failed to launch default application for \"%s\"");
      goto err;
    }

  return;

err:
  display_name = g_file_get_parse_name (file);
  xfce_dialog_show_error (NULL, error, message, display_name);
  g_free (display_name);
  g_error_free (error);
}



static void
directory_menu_plugin_menu_open (GtkWidget   *mi,
                                 GFile       *dir,
                                 const gchar *category,
                                 gboolean     path_as_arg)
{
  GError       *error = NULL;
  XfceRc       *rc, *helperrc;
  const gchar  *value, *working_dir;
  gchar        *filename;
  gchar       **binaries = NULL;
  guint         i;
  gboolean      result = FALSE;
  gchar        *argv[3];
  gboolean      startup_notify = FALSE;

  /* try to work around the exo code and get the direct command */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", TRUE);
  if (G_LIKELY (rc != NULL))
    {
      value = xfce_rc_read_entry_untranslated (rc, category, NULL);
      if (G_LIKELY (value != NULL))
        {
          filename = g_strconcat ("xfce4/helpers/", value, ".desktop", NULL);
          helperrc = xfce_rc_config_open (XFCE_RESOURCE_DATA, filename, TRUE);
          g_free (filename);

          if (G_LIKELY (helperrc != NULL))
            {
              startup_notify = xfce_rc_read_bool_entry (helperrc, "StartupNotify", FALSE);
              value = xfce_rc_read_entry_untranslated (helperrc, "X-XFCE-Binaries", NULL);
              if (value != NULL)
                binaries = g_strsplit (value, ";", -1);

              xfce_rc_close (helperrc);
            }
        }

      xfce_rc_close (rc);
    }

  working_dir = g_file_peek_path (dir);

  /* if there are binaries, there is a helper that
   * supports startup notification, try to spawn one */
  if (binaries != NULL)
    {
      for (i = 0; binaries[i] != NULL; i++)
        {
          filename = g_find_program_in_path (binaries[i]);
          if (filename == NULL)
            continue;

          argv[0] = filename;
          argv[1] = path_as_arg ? (gchar *) working_dir : NULL;
          argv[2] = NULL;

          /* try to spawn the program, if this fails we try exo for
           * a decent error message */
          result = xfce_spawn (gtk_widget_get_screen (mi),
                               working_dir, argv, NULL, 0,
                               startup_notify,
                               gtk_get_current_event_time (),
                               NULL, FALSE, NULL);
          g_free (filename);
          break;
        }

      g_strfreev (binaries);
    }

  if (!result
      && !exo_execute_preferred_application_on_screen (category,
                                                       path_as_arg ? working_dir : NULL,
                                                       working_dir,
                                                       NULL,
                                                       gtk_widget_get_screen (mi), &error)
     )
    {
      xfce_dialog_show_error (NULL, error,
          _("Failed to execute the preferred application for category \"%s\""), category);
      g_error_free (error);
    }
}



static void
directory_menu_plugin_menu_open_terminal (GtkWidget *mi,
                                          GFile     *dir)
{
  panel_return_if_fail (GTK_IS_WIDGET (mi));
  panel_return_if_fail (G_IS_FILE (dir));

  directory_menu_plugin_menu_open (mi, dir, "TerminalEmulator", FALSE);
}



static void
directory_menu_plugin_menu_open_folder (GtkWidget *mi,
                                        GFile     *dir)
{
  panel_return_if_fail (GTK_IS_WIDGET (mi));
  panel_return_if_fail (G_IS_FILE (dir));

  directory_menu_plugin_menu_open (mi, dir, "FileManager", TRUE);
}



static void
directory_menu_plugin_menu_name_entry_changed (GtkEditable *editable,
                                               gpointer dialog)
{
  const gchar *text;
  GtkWidget   *create_button;
  GtkWidget   *open_button;

  create_button = gtk_dialog_get_widget_for_response (GTK_DIALOG(dialog),
                                                      DIALOG_RESPONSE_CREATE);
  open_button = gtk_dialog_get_widget_for_response (GTK_DIALOG(dialog),
                                                    DIALOG_RESPONSE_OPEN);

  text = gtk_entry_get_text (GTK_ENTRY(editable));
  if (strlen(text) > 0)
    {
      gtk_widget_set_sensitive (create_button, TRUE);
      gtk_widget_set_sensitive (open_button, TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (create_button, FALSE);
      gtk_widget_set_sensitive (open_button, FALSE);
    }
}



static void
directory_menu_plugin_create_new (GtkWidget *mi,
                                  GFile     *parent_dir,
                                  gboolean  is_folder)
{
  GtkWidget   *dialog;
  const gchar *dialog_title;
  gint         dialog_retval;
  GtkWidget   *content;
  GtkWidget   *grid;
  GtkWidget   *icon;
  GtkWidget   *label;
  GtkWidget   *entry;
  GDateTime   *date;
  gchar       *formatted_date;
  const gchar *filename;
  gchar       *filepath;
  GFile       *new_file = NULL;
  GError      *error = NULL;

  if (is_folder)
    {
      dialog_title = _("Create New Folder");
      icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_DIALOG);
    }
  else
    {
      dialog_title = _("Create New Text Document");
      icon = gtk_image_new_from_icon_name ("text-x-generic", GTK_ICON_SIZE_DIALOG);
    }

  dialog = gtk_dialog_new_with_buttons (dialog_title, NULL,
                                        GTK_DIALOG_MODAL,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("C_reate"), DIALOG_RESPONSE_CREATE,
                                        _("Create & _Open"), DIALOG_RESPONSE_OPEN,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), 1);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 9);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_widget_set_margin_start (grid, 6);
  gtk_widget_set_margin_end (grid, 6);
  gtk_widget_set_margin_top (grid, 6);
  gtk_widget_set_margin_bottom (grid, 6);

  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 2);

  label = gtk_label_new (_("Enter the new name:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

  date = g_date_time_new_now_local ();
  formatted_date = g_date_time_format (date, "%F %T");
  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_entry_set_text (GTK_ENTRY(entry), formatted_date);
  g_free (formatted_date);
  g_date_time_unref (date);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
  g_signal_connect (entry, "changed",
                    G_CALLBACK (directory_menu_plugin_menu_name_entry_changed),
                    dialog);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (content), grid);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_widget_show_all (dialog);

  dialog_retval = gtk_dialog_run (GTK_DIALOG (dialog));
  if (dialog_retval >= 0)
    {
      filename = gtk_entry_get_text (GTK_ENTRY(entry));
      if (strlen(filename) > 0)
        {
          new_file = g_file_get_child (parent_dir, filename);
          if (is_folder)
            g_file_make_directory (new_file, NULL, &error);
          else
            g_file_create (new_file, G_FILE_CREATE_NONE, NULL, &error);

          if (error != NULL)
            {
              filepath = g_file_get_parse_name (new_file);
              xfce_dialog_show_error (NULL, error, _("Failed to create folder: %s"), filepath);
              g_free (filepath);
              g_error_free (error);
            }
          else if (dialog_retval == DIALOG_RESPONSE_OPEN)
            {
              if (is_folder)
                directory_menu_plugin_menu_open (mi, new_file, "FileManager", TRUE);
              else
                directory_menu_plugin_menu_launch (mi, new_file);
            }
          g_object_unref (new_file);
        }
    }

  gtk_widget_destroy (dialog);
}


static void
directory_menu_plugin_menu_new_folder (GtkWidget *mi,
                                       GFile     *dir)
{
  panel_return_if_fail (GTK_IS_WIDGET (mi));
  panel_return_if_fail (G_IS_FILE (dir));

  directory_menu_plugin_create_new (mi, dir, TRUE);
}



static void
directory_menu_plugin_menu_new_document (GtkWidget *mi,
                                         GFile     *dir)
{
  panel_return_if_fail (GTK_IS_WIDGET (mi));
  panel_return_if_fail (G_IS_FILE (dir));

  directory_menu_plugin_create_new (mi, dir, FALSE);
}



static void
directory_menu_plugin_menu_unload (GtkWidget *menu)
{
  /* delay destruction so we can handle the activate event first */
  gtk_container_foreach (GTK_CONTAINER (menu),
     (GtkCallback) (void (*)(void)) panel_utils_destroy_later, NULL);
}



static void
directory_menu_plugin_menu_load (GtkWidget           *menu,
                                 DirectoryMenuPlugin *plugin)
{
  GFileEnumerator *iter;
  GFileInfo       *info;
  GtkWidget       *mi;
  const gchar     *display_name;
  GSList          *li, *infos = NULL;
  GIcon           *icon;
  GtkWidget       *image;
  GtkWidget       *submenu;
  GFile           *file;
  GFile           *dir;
  gboolean         visible;
  GFileType        file_type;
#ifdef HAVE_GIO_UNIX
  GDesktopAppInfo *desktopinfo;
  const gchar     *description;
#endif

  panel_return_if_fail (XFCE_IS_DIRECTORY_MENU_PLUGIN (plugin));
  panel_return_if_fail (GTK_IS_MENU (menu));

  dir = g_object_get_qdata (G_OBJECT (menu), menu_file);
  panel_return_if_fail (G_IS_FILE (dir));
  if (G_UNLIKELY (dir == NULL))
    return;

  if (plugin->open_folder) {
    mi = panel_image_menu_item_new_with_label (_("Open Folder"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    g_signal_connect_data (G_OBJECT (mi), "activate",
        G_CALLBACK (directory_menu_plugin_menu_open_folder),
        g_object_ref (dir), (GClosureNotify) (void (*)(void)) g_object_unref, 0);
    gtk_widget_show (mi);

    image = gtk_image_new_from_icon_name ("folder-open", GTK_ICON_SIZE_MENU);
    panel_image_menu_item_set_image (mi, image);
    gtk_widget_show (image);
  }

  if (plugin->open_in_terminal) {
    mi = panel_image_menu_item_new_with_label (_("Open in Terminal"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    g_signal_connect_data (G_OBJECT (mi), "activate",
        G_CALLBACK (directory_menu_plugin_menu_open_terminal),
        g_object_ref (dir), (GClosureNotify) (void (*)(void)) g_object_unref, 0);
    gtk_widget_show (mi);

    image = gtk_image_new_from_icon_name ("utilities-terminal", GTK_ICON_SIZE_MENU);
    panel_image_menu_item_set_image (mi, image);
    gtk_widget_show (image);
  }

  if (plugin->new_folder) {
    mi = panel_image_menu_item_new_with_label (_("Create Folder..."));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    g_signal_connect_data (G_OBJECT (mi), "activate",
        G_CALLBACK (directory_menu_plugin_menu_new_folder),
        g_object_ref (dir), (GClosureNotify) (void (*)(void)) g_object_unref, 0);
    gtk_widget_show (mi);

    image = gtk_image_new_from_icon_name ("folder-new", GTK_ICON_SIZE_MENU);
    panel_image_menu_item_set_image (mi, image);
    gtk_widget_show (image);
  }

  if (plugin->new_document) {
    mi = panel_image_menu_item_new_with_label (_("Create Text Document..."));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    g_signal_connect_data (G_OBJECT (mi), "activate",
        G_CALLBACK (directory_menu_plugin_menu_new_document),
        g_object_ref (dir), (GClosureNotify) (void (*)(void)) g_object_unref, 0);
    gtk_widget_show (mi);

    image = gtk_image_new_from_icon_name ("document-new", GTK_ICON_SIZE_MENU);
    panel_image_menu_item_set_image (mi, image);
    gtk_widget_show (image);
  }

  iter = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME
                                    "," G_FILE_ATTRIBUTE_STANDARD_NAME
                                    "," G_FILE_ATTRIBUTE_STANDARD_TYPE
                                    "," G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN
                                    "," G_FILE_ATTRIBUTE_STANDARD_ICON,
                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (G_LIKELY (iter != NULL))
    {
      for (;;)
        {
          info = g_file_enumerator_next_file (iter, NULL, NULL);
          if (G_UNLIKELY (info == NULL))
            break;

          /* skip hidden files if disabled by the user */
          if (!plugin->hidden_files
              && g_file_info_get_is_hidden (info))
            {
              g_object_unref (G_OBJECT (info));
              continue;
            }

          /* if the file is not a directory, check the file patterns */
          if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
            {
              if (plugin->patterns == NULL)
                {
                  g_object_unref (G_OBJECT (info));
                  continue;
                }

              visible = FALSE;
              display_name = g_file_info_get_display_name (info);
              if (G_LIKELY (display_name != NULL))
                for (li = plugin->patterns; !visible && li != NULL; li = li->next)
                   if (g_pattern_match_string (li->data, display_name))
                     visible = TRUE;

              if (!visible)
                {
                  g_object_unref (G_OBJECT (info));
                  continue;
                }
            }

          infos = g_slist_insert_sorted (infos, info, directory_menu_plugin_menu_sort);
        }

      g_object_unref (G_OBJECT (iter));

      if (G_LIKELY (infos != NULL
            && (plugin->open_folder || plugin->open_in_terminal)))
        {
          mi = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);
        }

      for (li = infos; li != NULL; li = li->next)
        {
          info = G_FILE_INFO (li->data);
          file_type = g_file_info_get_file_type (info);

          display_name = g_file_info_get_display_name (info);
          if (G_UNLIKELY (display_name == NULL))
            {
              g_object_unref (G_OBJECT (info));
              continue;
            }

          file = g_file_get_child (dir, g_file_info_get_name (info));
          icon = NULL;

#ifdef HAVE_GIO_UNIX
          /* for native desktop files we make an exception and try
           * to load them like a normal menu */
          desktopinfo = NULL;
          if (G_UNLIKELY (file_type != G_FILE_TYPE_DIRECTORY
              && g_file_is_native (file)
              && g_str_has_suffix (display_name, ".desktop")))
            {
              desktopinfo = g_desktop_app_info_new_from_filename (g_file_peek_path (file));
              if (G_LIKELY (desktopinfo != NULL))
                {
                  display_name = g_app_info_get_name (G_APP_INFO (desktopinfo));
                  icon = g_app_info_get_icon (G_APP_INFO (desktopinfo));

                  /* ignore invalid or hidden files */
                  if (panel_str_is_empty (display_name)
                      || g_desktop_app_info_get_is_hidden (desktopinfo))
                    {
                      g_object_unref (G_OBJECT (desktopinfo));
                      g_object_unref (G_OBJECT (info));
                      g_object_unref (G_OBJECT (file));
                      continue;
                    }
                }
            }
#endif

          mi = panel_image_menu_item_new_with_label (display_name);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);

          if (G_LIKELY (icon == NULL))
            icon = g_file_info_get_icon (info);
          if (G_LIKELY (icon != NULL))
            {
              image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
              panel_image_menu_item_set_image (mi, image);
              gtk_widget_show (image);
            }

          /* set a submenu for directories */
          if (G_LIKELY (file_type == G_FILE_TYPE_DIRECTORY))
            {
              submenu = gtk_menu_new ();
              gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), submenu);
              g_object_set_qdata_full (G_OBJECT (submenu), menu_file, file, g_object_unref);

              g_signal_connect (G_OBJECT (submenu), "show",
                  G_CALLBACK (directory_menu_plugin_menu_load), plugin);
              g_signal_connect_after (G_OBJECT (submenu), "hide",
                  G_CALLBACK (directory_menu_plugin_menu_unload), NULL);
            }
#ifdef HAVE_GIO_UNIX
          else if (G_UNLIKELY (desktopinfo != NULL))
            {
              description = g_app_info_get_description (G_APP_INFO (desktopinfo));
              if (!panel_str_is_empty (description))
                gtk_widget_set_tooltip_text (mi, description);

              g_signal_connect_data (G_OBJECT (mi), "activate",
                  G_CALLBACK (directory_menu_plugin_menu_launch_desktop_file),
                  desktopinfo, (GClosureNotify) (void (*)(void)) g_object_unref, 0);

              g_object_unref (G_OBJECT (file));
            }
#endif
          else
            {
              g_signal_connect_data (G_OBJECT (mi), "activate",
                  G_CALLBACK (directory_menu_plugin_menu_launch), file,
                  (GClosureNotify) (void (*)(void)) g_object_unref, 0);
            }

          g_object_unref (G_OBJECT (info));
        }

      g_slist_free (infos);
    }
}



static void
directory_menu_plugin_menu (GtkWidget           *button,
                            DirectoryMenuPlugin *plugin)
{
  GtkWidget *menu;
  GdkEvent  *event = NULL;

  panel_return_if_fail (XFCE_IS_DIRECTORY_MENU_PLUGIN (plugin));
  panel_return_if_fail (button == NULL || plugin->button == button);

  if (button != NULL
      && !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;

  /* Panel plugin remote events don't send actual GdkEvents, so construct a minimal one so that
   * gtk_menu_popup_at_pointer/rect can extract a location correctly from a GdkWindow */
  event = gtk_get_current_event ();
  if (event == NULL)
    {
      GdkSeat *seat = gdk_display_get_default_seat (gdk_display_get_default ());
      event = gdk_event_new (GDK_BUTTON_PRESS);
      event->button.window = g_object_ref (gdk_get_default_root_window ());
      gdk_event_set_device (event, gdk_seat_get_pointer (seat));
    }

  menu = gtk_menu_new ();
  g_signal_connect (G_OBJECT (menu), "deactivate",
      G_CALLBACK (directory_menu_plugin_deactivate), plugin);

  g_object_set_qdata_full (G_OBJECT (menu), menu_file,
                           g_object_ref (plugin->base_directory),
                           g_object_unref);
  directory_menu_plugin_menu_load (menu, plugin);

  /* do not block panel autohide if popup-command at pointer */
  if (button == NULL)
    gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
  else
    xfce_panel_plugin_popup_menu (XFCE_PANEL_PLUGIN (plugin), GTK_MENU (menu), button, event);

  gdk_event_free (event);
}
