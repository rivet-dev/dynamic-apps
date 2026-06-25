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

#include <glib/gstdio.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <common/panel-debug.h>
#include <common/panel-utils.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include <panel/panel-dbus-service.h>
#include <panel/panel-base-window.h>
#include <panel/panel-window.h>
#include <panel/panel-application.h>
#include <panel/panel-itembar.h>
#include <panel/panel-module-factory.h>
#include <panel/panel-preferences-dialog.h>
#include <panel/panel-item-dialog.h>
#include <panel/panel-dialogs.h>
#include <panel/panel-plugin-external.h>

#define MIGRATE_BIN HELPERDIR G_DIR_SEPARATOR_S "migrate"



static void      panel_application_dispose            (GObject                *object);
static void      panel_application_finalize           (GObject                *object);
static void      panel_application_plugin_move        (GtkWidget              *item,
                                                       PanelApplication       *application);
static gboolean  panel_application_plugin_insert      (PanelApplication       *application,
                                                       PanelWindow            *window,
                                                       const gchar            *name,
                                                       gint                    unique_id,
                                                       gchar                 **arguments,
                                                       gint                    position);
static void      panel_application_dialog_destroyed   (GtkWindow              *dialog,
                                                       PanelApplication       *application);
static void      panel_application_drag_data_received (PanelWindow            *window,
                                                       GdkDragContext         *context,
                                                       gint                    x,
                                                       gint                    y,
                                                       GtkSelectionData       *selection_data,
                                                       guint                   info,
                                                       guint                   drag_time,
                                                       GtkWidget              *itembar);
static gboolean  panel_application_drag_motion        (GtkWidget              *window,
                                                       GdkDragContext         *context,
                                                       gint                    x,
                                                       gint                    y,
                                                       guint                   drag_time,
                                                       PanelApplication       *application);
static gboolean  panel_application_drag_drop          (GtkWidget              *window,
                                                       GdkDragContext         *context,
                                                       gint                    x,
                                                       gint                    y,
                                                       guint                   drag_time,
                                                       PanelApplication       *application);
static void      panel_application_drag_leave         (GtkWidget              *window,
                                                       GdkDragContext         *context,
                                                       guint                   drag_time,
                                                       PanelApplication       *application);



enum
{
  PROP_0,
  PROP_ITEMS_CHANGED
};

struct _PanelApplicationClass
{
  GObjectClass __parent__;
};

struct _PanelApplication
{
  GObject  __parent__;

  /* the plugin factory */
  PanelModuleFactory *factory;

  /* xfconf channel */
  XfconfChannel      *xfconf;

  /* internal list of all the panel windows */
  GSList             *windows;

  /* internal list of opened dialogs */
  GSList             *dialogs;

  /* autohide count at application level */
  gint                autohide_block;

#ifdef GDK_WINDOWING_X11
  guint               wait_for_wm_timeout_id;
#endif

  /* drag and drop data */
  guint               drop_data_ready : 1;
  guint               drop_occurred : 1;
  guint               drop_desktop_files : 1;
  guint               drop_index;
};

#ifdef GDK_WINDOWING_X11
typedef struct
{
  PanelApplication *application;

  Display          *dpy;
  Atom             *atoms;
  guint             atom_count;
  guint             have_wm : 1;
  guint             counter;
}
WaitForWM;
#endif

enum
{
  TARGET_PLUGIN_NAME,
  TARGET_PLUGIN_WIDGET,
  TARGET_TEXT_URI_LIST
};

static const GtkTargetEntry drag_targets[] =
{
  { "xfce-panel/plugin-widget",
    GTK_TARGET_SAME_APP, TARGET_PLUGIN_WIDGET }
};

static const GtkTargetEntry drop_targets[] =
{
  { "xfce-panel/plugin-name",
    GTK_TARGET_SAME_APP, TARGET_PLUGIN_NAME },
  { "xfce-panel/plugin-widget",
    GTK_TARGET_SAME_APP, TARGET_PLUGIN_WIDGET },
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST }
};



G_DEFINE_TYPE (PanelApplication, panel_application, G_TYPE_OBJECT)



static void
panel_application_class_init (PanelApplicationClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = panel_application_dispose;
  gobject_class->finalize = panel_application_finalize;
}



static void
panel_application_init (PanelApplication *application)
{
  GError *error = NULL;
  gint    configver;

  application->windows = NULL;
  application->dialogs = NULL;
  application->drop_desktop_files = FALSE;
  application->drop_data_ready = FALSE;
  application->drop_occurred = FALSE;
  application->autohide_block = 0;

  /* get the xfconf channel (singleton) */
  application->xfconf = panel_properties_get_channel (G_OBJECT (application));

  /* check if we need to migrate configuration */
  configver = xfconf_channel_get_int (application->xfconf, "/configver", -1);
  if (G_UNLIKELY (configver < XFCE4_PANEL_CONFIG_VERSION))
    {
      if (!g_spawn_command_line_sync (MIGRATE_BIN, NULL, NULL, NULL, &error))
        {
          xfce_dialog_show_error (NULL, error, _("Failed to launch the migration application"));
          g_error_free (error);
        }
    }

  /* check if we need to force all plugins to run external */
  if (xfconf_channel_get_bool (application->xfconf, "/force-all-external", FALSE))
    panel_module_factory_force_all_external ();

  /* get a factory reference so it never unloads */
  application->factory = panel_module_factory_get ();
}



static void
panel_application_dispose (GObject *object)
{
  PanelApplication *application = PANEL_APPLICATION (object);

  /* save plugins: xfconf_shutdown() is called via a weak ref i.e. on dispose(),
   * so this should be done here to avoid any use-after-free */
  panel_application_save (application, SAVE_PLUGIN_PROVIDERS);

  (*G_OBJECT_CLASS (panel_application_parent_class)->dispose) (object);
}



static void
panel_application_finalize (GObject *object)
{
  PanelApplication *application = PANEL_APPLICATION (object);

  panel_return_if_fail (application->dialogs == NULL);

#ifdef GDK_WINDOWING_X11
  /* stop autostart timeout */
  if (application->wait_for_wm_timeout_id != 0)
    g_source_remove (application->wait_for_wm_timeout_id);
#endif

  /* destroy all panels */
  g_slist_foreach (application->windows, (GFunc) (void (*)(void)) gtk_widget_destroy, NULL);
  g_slist_free (application->windows);

  g_object_unref (G_OBJECT (application->factory));

  /* this is a good reference if all the objects are released */
  panel_debug (PANEL_DEBUG_APPLICATION, "finalized");

  (*G_OBJECT_CLASS (panel_application_parent_class)->finalize) (object);
}



static void
panel_application_xfconf_window_bindings (PanelApplication *application,
                                          PanelWindow      *window,
                                          gboolean          save_properties)
{
  gchar               *property_base;
  const PanelProperty  properties[] =
  {
    { "position-locked", G_TYPE_BOOLEAN },
    { "autohide-behavior", G_TYPE_UINT },
    { "popdown-speed", G_TYPE_UINT },
    { "span-monitors", G_TYPE_BOOLEAN },
    { "mode", G_TYPE_UINT },
    { "size", G_TYPE_UINT },
    { "nrows", G_TYPE_UINT },
    { "length", G_TYPE_DOUBLE },
    { "length-adjust", G_TYPE_BOOLEAN },
    { "enter-opacity", G_TYPE_UINT },
    { "leave-opacity", G_TYPE_UINT },
    { "background-style", G_TYPE_UINT },
    { "background-rgba", GDK_TYPE_RGBA },
    { "background-image", G_TYPE_STRING },
    { "icon-size", G_TYPE_UINT },
    { "output-name", G_TYPE_STRING },
    { "position", G_TYPE_STRING },
    { "enable-struts", G_TYPE_BOOLEAN },
    { NULL }
  };
  const PanelProperty  global_properties[] =
  {
    { "dark-mode", G_TYPE_BOOLEAN },
    { NULL }
  };
  const PanelProperty  old_properties[] =
  {
    { "autohide", G_TYPE_BOOLEAN },
    { "disable-struts", G_TYPE_BOOLEAN },
    { NULL }
  };
  const PanelProperty  new_properties[] =
  {
    { "autohide-behavior", G_TYPE_UINT },
    { "enable-struts", G_TYPE_BOOLEAN },
    { NULL }
  };

  panel_return_if_fail (XFCONF_IS_CHANNEL (application->xfconf));

  /* create the property base */
  property_base = g_strdup_printf (PANELS_PROPERTY_BASE, panel_window_get_id (window));

  /* migrate old properties */
  panel_window_migrate_old_properties (window, application->xfconf, property_base, old_properties, new_properties);

  /* bind all the properties */
  panel_properties_bind (application->xfconf, G_OBJECT (window),
                         property_base, properties, save_properties);
  panel_properties_bind (application->xfconf, G_OBJECT (window),
                         PANELS_PROPERTY_PREFIX, global_properties, save_properties);

  /* set locking for this panel */
  panel_window_set_locked (window,
      xfconf_channel_is_property_locked (application->xfconf, property_base));

  g_free (property_base);
}



static void
panel_application_load_real (PanelApplication *application)
{
  PanelWindow  *window;
  guint         i, j, n_panels;
  gchar         buf[50];
  gchar        *name;
  gint          unique_id;
  GdkScreen    *screen;
  GPtrArray    *array;
  const GValue *value;
  gchar        *output_name;
  gint          screen_num;
  GdkDisplay   *display;
  GValue        val = { 0, };
  GPtrArray    *panels;
  gint          panel_id;
  gboolean      save_changed_ids = FALSE;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (XFCONF_IS_CHANNEL (application->xfconf));

  display = gdk_display_get_default ();

  if (xfconf_channel_get_property (application->xfconf, PANELS_PROPERTY_PREFIX, &val)
      && (G_VALUE_HOLDS_UINT (&val)
          || G_VALUE_HOLDS (&val, G_TYPE_PTR_ARRAY)))
    {
      if (G_VALUE_HOLDS_UINT (&val))
        {
          n_panels = g_value_get_uint (&val);
          panels = NULL;
        }
      else
        {
          panels = g_value_get_boxed (&val);
          n_panels = panels->len;
        }

      /* walk all the panel in the configuration */
      for (i = 0; i < n_panels; i++)
        {
          screen = NULL;

          /* get the panel id */
          if (panels != NULL)
            {
              /* get the id from the array */
              value = g_ptr_array_index (panels, i);
              panel_assert (value != NULL);
              panel_id = g_value_get_int (value);
            }
          else
            {
              /* use the list position if /panels is an uint */
              panel_id = i;
            }

          /* start the panel directly on the correct screen */
          g_snprintf (buf, sizeof (buf), PANELS_PROPERTY_BASE "/output-name", panel_id);
          output_name = xfconf_channel_get_string (application->xfconf, buf, NULL);
          if (output_name != NULL
              && strncmp (output_name, "screen-", 7) == 0
              && sscanf (output_name, "screen-%d", &screen_num) == 1)
            {
              if (screen_num < 1)
                screen = gdk_display_get_default_screen (display);
            }
          g_free (output_name);

          /* create a new window */
          window = panel_application_new_window (application, screen, panel_id, FALSE);

          /* walk all the plugins on the panel */
          g_snprintf (buf, sizeof (buf), PLUGIN_IDS_PROPERTY_BASE, panel_id);
          array = xfconf_channel_get_arrayv (application->xfconf, buf);
          if (array == NULL)
            continue;

          for (j = 0; j < array->len; j++)
            {
              /* get the plugin id */
              value = g_ptr_array_index (array, j);
              panel_assert (value != NULL);
              unique_id = g_value_get_int (value);

              /* get the plugin name */
              g_snprintf (buf, sizeof (buf), PLUGINS_PROPERTY_BASE, unique_id);
              name = xfconf_channel_get_string (application->xfconf, buf, NULL);

              /* append the plugin to the panel */
              if (unique_id < 1 || name == NULL
                  || !panel_application_plugin_insert (application, window,
                                                       name, unique_id, NULL, -1))
                {
                  /* plugin could not be loaded, remove it from the channel */
                  if (xfconf_channel_has_property (application->xfconf, buf))
                    xfconf_channel_reset_property (application->xfconf, buf, TRUE);

                  /* show warnings */
                  g_message ("Plugin \"%s-%d\" was not found and has been "
                             "removed from the configuration", name, unique_id);

                  /* save configuration change after loading */
                  save_changed_ids = TRUE;
                }

              g_free (name);
            }

          xfconf_array_free (array);
        }

      /* free xfconf array or uint */
      g_value_unset (&val);
    }

  /* create empty window if everything else failed */
  if (G_UNLIKELY (application->windows == NULL))
    panel_application_new_window (application, NULL, -1, TRUE);

  if (save_changed_ids)
    panel_application_save (application, SAVE_PLUGIN_IDS);
}



#ifdef GDK_WINDOWING_X11
static gboolean
panel_application_wait_for_window_manager (gpointer data)
{
  WaitForWM *wfwm = data;
  guint      i;
  gboolean   have_wm = TRUE;

  for (i = 0; i < wfwm->atom_count; i++)
    {
      if (XGetSelectionOwner (wfwm->dpy, wfwm->atoms[i]) == None)
        {
          panel_debug (PANEL_DEBUG_APPLICATION, "window manager not ready on screen %d", i);

          have_wm = FALSE;
          break;
        }
    }

  wfwm->have_wm = have_wm;

  /* abort if a window manager is found or 5 seconds expired */
  return wfwm->counter++ < 20 * 5 && !wfwm->have_wm;
}



static void
panel_application_wait_for_window_manager_destroyed (gpointer data)
{
  WaitForWM        *wfwm = data;
  PanelApplication *application = wfwm->application;

  application->wait_for_wm_timeout_id = 0;

  if (!wfwm->have_wm)
    {
      g_printerr (G_LOG_DOMAIN ": No window manager registered on screen 0. "
                  "To start the panel without this check, run with --disable-wm-check.\n");
    }
  else
    {
      panel_debug (PANEL_DEBUG_APPLICATION, "found window manager after %d tries",
                   wfwm->counter);
    }

  g_free (wfwm->atoms);
  XCloseDisplay (wfwm->dpy);
  g_slice_free (WaitForWM, wfwm);

  /* start loading the panels, hopefully a window manager is found, but it
   * probably also works fine without... */
  panel_application_load_real (application);
}
#endif



static void
panel_application_plugin_move_drag_data_get (GtkWidget        *item,
                                             GdkDragContext   *drag_context,
                                             GtkSelectionData *selection_data,
                                             guint             info,
                                             guint             drag_time,
                                             PanelApplication *application)
{
  /* set some data, we never use this, but GTK_DEST_DEFAULT_ALL
   * used in the item dialog requires this */
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data), 8,
                          (const guchar *) "0", 1);
}



static void
panel_application_plugin_move_drag_end (GtkWidget        *item,
                                        GdkDragContext   *context,
                                        PanelApplication *application)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (item));
  panel_return_if_fail (PANEL_IS_APPLICATION (application));

  /* disconnect this signal */
  g_signal_handlers_disconnect_by_func (G_OBJECT (item),
      G_CALLBACK (panel_application_plugin_move_drag_end), application);
  g_signal_handlers_disconnect_by_func (G_OBJECT (item),
      G_CALLBACK (panel_application_plugin_move_drag_data_get), application);

  /* unblock autohide */
  panel_application_windows_blocked (application, FALSE);
}



static void
panel_application_plugin_move (GtkWidget        *item,
                               PanelApplication *application)
{
  GtkTargetList  *target_list;
  const gchar    *icon_name;
  GdkDragContext *context;
  PanelModule    *module;
  GtkIconTheme   *theme;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (item));
  panel_return_if_fail (PANEL_IS_APPLICATION (application));

  /* block autohide */
  panel_application_windows_blocked (application, TRUE);

  /* create drag context */
  target_list = gtk_target_list_new (drag_targets, G_N_ELEMENTS (drag_targets));
  context = gtk_drag_begin_with_coordinates (item, target_list,
                                             GDK_ACTION_MOVE, 1, NULL, -1, -1);
  gtk_target_list_unref (target_list);

  /* set the drag context icon name */
  module = panel_module_get_from_plugin_provider (XFCE_PANEL_PLUGIN_PROVIDER (item));
  icon_name = panel_module_get_icon_name (module);
  theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (item));
  if (!panel_str_is_empty (icon_name)
      && gtk_icon_theme_has_icon (theme, icon_name))
    gtk_drag_set_icon_name (context, icon_name, 0, 0);
  else
    gtk_drag_set_icon_default (context);

  /* signal to make the window sensitive again on a drag end */
  g_signal_connect (G_OBJECT (item), "drag-end",
      G_CALLBACK (panel_application_plugin_move_drag_end), application);
  g_signal_connect (G_OBJECT (item), "drag-data-get",
      G_CALLBACK (panel_application_plugin_move_drag_data_get), application);
}



static void
panel_application_plugin_delete_config (PanelApplication *application,
                                        const gchar      *name,
                                        gint              unique_id)
{
  gchar *property;
  gchar *filename, *path;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (!panel_str_is_empty (name));
  panel_return_if_fail (unique_id != -1);

  /* remove the xfconf property */
  property = g_strdup_printf (PLUGINS_PROPERTY_BASE, unique_id);
  if (xfconf_channel_has_property (application->xfconf, property))
    xfconf_channel_reset_property (application->xfconf, property, TRUE);
  g_free (property);

  /* lookup the rc file */
  filename = g_strdup_printf (PANEL_PLUGIN_RC_RELATIVE_PATH, name, unique_id);
  path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, filename);
  g_free (filename);

  /* unlink the rc file */
  if (G_LIKELY (path != NULL))
    g_unlink (path);
  g_free (path);
}



static void
panel_application_plugin_remove (GtkWidget *widget,
                                 gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));

  /* ask the plugin to cleanup when we destroy a panel window */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (widget),
                                          PROVIDER_SIGNAL_REMOVE_PLUGIN);
}



static void
panel_application_plugin_provider_signal (XfcePanelPluginProvider       *provider,
                                          XfcePanelPluginProviderSignal  provider_signal,
                                          PanelApplication              *application)
{
  GtkWidget   *itembar;
  PanelWindow *window;
  gint         unique_id;
  gchar       *name;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  window = PANEL_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (provider)));
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  switch (provider_signal)
    {
    case PROVIDER_SIGNAL_MOVE_PLUGIN:
      /* check the window locking, not that of the provider, because
       * the users might have worked around that and both should be identical */
      if (!panel_window_get_locked (window))
        panel_application_plugin_move (GTK_WIDGET (provider), application);
      break;

    case PROVIDER_SIGNAL_EXPAND_PLUGIN:
    case PROVIDER_SIGNAL_COLLAPSE_PLUGIN:
      itembar = gtk_bin_get_child (GTK_BIN (window));
      gtk_container_child_set (GTK_CONTAINER (itembar),
                               GTK_WIDGET (provider),
                               "expand",
                               provider_signal == PROVIDER_SIGNAL_EXPAND_PLUGIN,
                               NULL);
      break;

    case PROVIDER_SIGNAL_SHRINK_PLUGIN:
    case PROVIDER_SIGNAL_UNSHRINK_PLUGIN:
      itembar = gtk_bin_get_child (GTK_BIN (window));
      gtk_container_child_set (GTK_CONTAINER (itembar),
                               GTK_WIDGET (provider),
                               "shrink",
                               provider_signal == PROVIDER_SIGNAL_SHRINK_PLUGIN,
                               NULL);
      break;

    case PROVIDER_SIGNAL_SMALL_PLUGIN:
    case PROVIDER_SIGNAL_UNSMALL_PLUGIN:
      itembar = gtk_bin_get_child (GTK_BIN (window));
      gtk_container_child_set (GTK_CONTAINER (itembar),
                               GTK_WIDGET (provider),
                               "small",
                               provider_signal == PROVIDER_SIGNAL_SMALL_PLUGIN,
                               NULL);
      break;

    case PROVIDER_SIGNAL_LOCK_PANEL:
      /* increase window's autohide counter */
      panel_window_freeze_autohide (window);
      break;

    case PROVIDER_SIGNAL_UNLOCK_PANEL:
      /* decrease window's autohide counter */
      panel_window_thaw_autohide (window);
      break;

    case PROVIDER_SIGNAL_REMOVE_PLUGIN:
      /* check the window locking, not that of the provider, because
       * the users might have worked around that and both should be identical */
      if (!panel_window_get_locked (window))
        {
          /* give plugin the opportunity to cleanup special configuration */
          xfce_panel_plugin_provider_removed (provider);

          /* store the provider's unique id and name (lost after destroy) */
          unique_id = xfce_panel_plugin_provider_get_unique_id (provider);
          name = g_strdup (xfce_panel_plugin_provider_get_name (provider));

          /* destroy the plugin */
          itembar = gtk_bin_get_child (GTK_BIN (window));
          gtk_container_remove (GTK_CONTAINER (itembar), GTK_WIDGET (provider));

          /* remove the plugin configuration */
          panel_application_plugin_delete_config (application, name, unique_id);
          g_free (name);

          /* save new ids */
          panel_application_save_window (application, window, SAVE_PLUGIN_IDS);
        }
      break;

    case PROVIDER_SIGNAL_ADD_NEW_ITEMS:
      /* open the items dialog, locking is handled in the object */
      panel_item_dialog_show (window);
      break;

    case PROVIDER_SIGNAL_PANEL_PREFERENCES:
      /* open the panel preferences, locking is handled in the object */
      panel_preferences_dialog_show (window);
      break;

    case PROVIDER_SIGNAL_PANEL_LOGOUT:
      /* logout */
      panel_application_logout ();
      break;

    case PROVIDER_SIGNAL_PANEL_ABOUT:
      /* show the about dialog */
      panel_dialogs_show_about ();
      break;

    case PROVIDER_SIGNAL_PANEL_HELP:
      /* try to launch help browser */
      panel_utils_show_help (NULL, NULL, NULL);
      break;

    case PROVIDER_SIGNAL_FOCUS_PLUGIN:
       /* focus the panel window (as part of focusing a widget within the plugin) */
       panel_window_focus (window);
       break;

    case PROVIDER_SIGNAL_SHOW_CONFIGURE:
    case PROVIDER_SIGNAL_SHOW_ABOUT:
      /* signals we can ignore, only for external plugins */
      break;

    default:
      g_critical ("Received unknown provider signal %d", provider_signal);
      break;
    }
}



static gboolean
panel_application_plugin_insert (PanelApplication  *application,
                                 PanelWindow       *window,
                                 const gchar       *name,
                                 gint               unique_id,
                                 gchar            **arguments,
                                 gint               position)
{
  GtkWidget *itembar, *provider;
  gint       new_unique_id;

  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), FALSE);
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);
  panel_return_val_if_fail (name != NULL, FALSE);

  /* create a new panel plugin */
  provider = panel_module_factory_new_plugin (application->factory, name,
                                              gtk_window_get_screen (GTK_WINDOW (window)),
                                              unique_id, arguments, &new_unique_id);
  if (G_UNLIKELY (provider == NULL))
    return FALSE;

  /* make sure there is no panel configuration with this unique id when a
   * new plugin is created */
  if (G_UNLIKELY (unique_id == -1))
    panel_application_plugin_delete_config (application, name, new_unique_id);

  /* add signal to monitor provider signals */
  g_signal_connect (G_OBJECT (provider), "provider-signal",
      G_CALLBACK (panel_application_plugin_provider_signal), application);

  /* add the item to the panel */
  itembar = gtk_bin_get_child (GTK_BIN (window));
  panel_itembar_insert (PANEL_ITEMBAR (itembar),
                        GTK_WIDGET (provider), position);

  /* send all the needed info about the panel to the plugin */
  panel_window_set_provider_info (window, provider, FALSE);

  /* show the plugin */
  gtk_widget_show (provider);

  return TRUE;
}



static void
panel_application_dialog_destroyed (GtkWindow        *dialog,
                                    PanelApplication *application)
{
  panel_return_if_fail (GTK_IS_WINDOW (dialog));
  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (g_slist_find (application->dialogs, dialog) != NULL);

  /* remove the window from the list */
  application->dialogs = g_slist_remove (application->dialogs, dialog);

  /* unblock autohide if there are no open windows anymore */
  if (application->dialogs == NULL)
    panel_application_windows_blocked (application, FALSE);
}



static void
panel_application_drag_data_received (PanelWindow      *window,
                                      GdkDragContext   *context,
                                      gint              x,
                                      gint              y,
                                      GtkSelectionData *selection_data,
                                      guint             info,
                                      guint             drag_time,
                                      GtkWidget        *itembar)
{
  PanelApplication  *application;
  GtkWidget         *provider;
  gboolean           succeed = FALSE;
  gboolean           save_application = FALSE;
  const gchar       *name;
  guint              old_position;
  gchar            **uris;
  guint              i;
  gboolean           found;
  gint               n_items;
  gboolean           child_small;
  gboolean           child_expand;
  gboolean           child_shrink;
  GtkWidget         *parent_itembar;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  panel_return_if_fail (PANEL_IS_ITEMBAR (itembar));

  application = panel_application_get ();

  /* we don't allow any kind of drops here when the panel is locked */
  if (panel_application_get_locked (application)
      || panel_window_get_locked (window))
    goto invalid_drag;

  if (!application->drop_data_ready)
    {
      panel_assert (!application->drop_desktop_files);

      if (info == TARGET_TEXT_URI_LIST)
        {
          /* look if the selection data contains atleast 1 desktop file */
          uris = gtk_selection_data_get_uris (selection_data);
          if (G_LIKELY (uris != NULL))
            {
              for (i = 0, found = FALSE; !found && uris[i] != NULL; i++)
                found = g_str_has_suffix (uris[i], ".desktop");
              g_strfreev (uris);
              application->drop_desktop_files = found;
            }
        }

      /* reset the state */
      application->drop_data_ready = TRUE;
    }

  /* check if the data was droppped */
  if (application->drop_occurred)
    {
      /* reset the state */
      application->drop_occurred = FALSE;

      switch (info)
        {
        case TARGET_PLUGIN_NAME:
          if (G_LIKELY (gtk_selection_data_get_length (selection_data) > 0))
            {
              /* create a new item with a unique id */
              name = (const gchar *) gtk_selection_data_get_data (selection_data);
              succeed = panel_application_plugin_insert (application, window, name,
                                                         -1, NULL, application->drop_index);
            }
          break;

        case TARGET_PLUGIN_WIDGET:
          /* get the source widget */
          provider = gtk_drag_get_source_widget (context);

          /* debug check */
          panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

          /* check if we move to another itembar */
          parent_itembar = gtk_widget_get_parent (provider);
          if (parent_itembar == itembar)
            {
              /* get the current position on the itembar */
              old_position = panel_itembar_get_child_index (PANEL_ITEMBAR (itembar),
                                                            provider);

              /* decrease the counter if we drop after the current position */
              if (application->drop_index > old_position)
                application->drop_index--;

              /* reorder the child if needed */
              if (old_position != application->drop_index)
                panel_itembar_reorder_child (PANEL_ITEMBAR (itembar), provider, application->drop_index);
            }
          else
            {
              /* get properties from old itemsbar */
              gtk_container_child_get (GTK_CONTAINER (parent_itembar), provider,
                                       "expand", &child_expand,
                                       "shrink", &child_shrink,
                                       "small", &child_small, NULL);

              /* reparent the widget, this will also call remove and add for the itembar */
              gtk_widget_hide (provider);
              xfce_widget_reparent (provider, itembar);
              gtk_widget_show (provider);

              /* move the item to the correct position on the itembar */
              panel_itembar_reorder_child (PANEL_ITEMBAR (itembar), provider, application->drop_index);

              /* restore child properties */
              gtk_container_child_set (GTK_CONTAINER (itembar), provider,
                                       "expand", child_expand,
                                       "shrink", child_shrink,
                                       "small", child_small, NULL);

              /* send all the needed panel information to the plugin */
              panel_window_set_provider_info (window, provider, TRUE);

              /* moved between panels, save everything */
              save_application = TRUE;
            }

          /* everything went fine */
          succeed = TRUE;
          break;

        case TARGET_TEXT_URI_LIST:
          if (G_LIKELY (application->drop_desktop_files))
            {
              /* pass all the uris to the launcher, it will filter out
               * the desktop files on it's own */
              uris = gtk_selection_data_get_uris (selection_data);
              if (G_LIKELY (uris != NULL))
                {
                  n_items = g_strv_length (uris);
                  if (xfce_dialog_confirm (NULL, "list-add", _("Create _Launcher"),
                                           _("This will create a new launcher plugin on the panel and inserts "
                                             "the dropped files as menu items."),
                                           ngettext ("Create new launcher from %d desktop file",
                                                     "Create new launcher from %d desktop files",
                                                     n_items),
                                           n_items))
                    {
                      /* create a new item with a unique id */
                      succeed = panel_application_plugin_insert (application, window, LAUNCHER_PLUGIN_NAME,
                                                                 -1, uris, application->drop_index);
                    }

                  g_strfreev (uris);
                }

              application->drop_desktop_files = FALSE;
            }
          break;

        default:
          panel_assert_not_reached ();
          break;
        }

      /* save the new plugin ids */
      if (G_LIKELY (save_application))
        panel_application_save (application, SAVE_PLUGIN_IDS);
      else if (succeed)
        panel_application_save_window (application, window, SAVE_PLUGIN_IDS);

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, drag_time);
    }
  else
    {
      invalid_drag:
      gdk_drag_status (context, 0, drag_time);
    }

  g_object_unref (G_OBJECT (application));
}



static gboolean
panel_application_drag_motion (GtkWidget        *window,
                               GdkDragContext   *context,
                               gint              x,
                               gint              y,
                               guint             drag_time,
                               PanelApplication *application)
{
  GdkAtom        target;
  GtkWidget     *itembar;
  GdkDragAction  drag_action = 0;

  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);
  panel_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);
  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), FALSE);
  panel_return_val_if_fail (!application->drop_occurred, FALSE);

  /* don't allow anything when the window is locked */
  if (panel_window_get_locked (PANEL_WINDOW (window)))
    goto not_a_drop_zone;

  /* determine the drag target */
  target = gtk_drag_dest_find_target (window, context, NULL);

  if (target == gdk_atom_intern_static_string ("text/uri-list"))
    {
      /* request the drop data on-demand (if we don't have it already) */
      if (!application->drop_data_ready)
        {
          /* request the drop data from the source */
          gtk_drag_get_data (window, context, target, drag_time);

          /* we cannot drop here (yet!) */
          return TRUE;
        }
      else if (application->drop_desktop_files)
        {
          /* there are valid uris in the drop data */
          drag_action = GDK_ACTION_COPY;
        }
    }
  else if (target == gdk_atom_intern_static_string ("xfce-panel/plugin-name"))
    {
      /* insert a new plugin */
      drag_action = GDK_ACTION_COPY;
    }
  else if (target == gdk_atom_intern_static_string ("xfce-panel/plugin-widget"))
    {
      /* move an existing plugin */
      drag_action = GDK_ACTION_MOVE;
    }

  if (drag_action != 0)
    {
      /* highlight the drop zone */
      itembar = gtk_bin_get_child (GTK_BIN (window));
      application->drop_index = panel_itembar_get_drop_index (PANEL_ITEMBAR (itembar), x, y);
      panel_itembar_set_drop_highlight_item (PANEL_ITEMBAR (itembar), application->drop_index);
    }

not_a_drop_zone:
  gdk_drag_status (context, drag_action, drag_time);

  return (drag_action == 0);
}



static gboolean
panel_application_drag_drop (GtkWidget        *window,
                             GdkDragContext   *context,
                             gint              x,
                             gint              y,
                             guint             drag_time,
                             PanelApplication *application)
{
  GdkAtom target;

  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);
  panel_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);
  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), FALSE);

  target = gtk_drag_dest_find_target (window, context, NULL);

  /* we cannot handle the drop */
  if (G_UNLIKELY (target == GDK_NONE))
    return FALSE;

  /* set state so the drag-data-received handler
   * knows that this is really a drop this time. */
  application->drop_occurred = TRUE;

  /* request the drag data from the source. */
  gtk_drag_get_data (window, context, target, drag_time);

  /* we call gtk_drag_finish() later */
  return TRUE;
}



static void
panel_application_drag_leave (GtkWidget        *window,
                              GdkDragContext   *context,
                              guint             drag_time,
                              PanelApplication *application)
{
  GtkWidget *itembar;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  panel_return_if_fail (PANEL_IS_APPLICATION (application));

  /* reset the state */
  application->drop_data_ready = FALSE;
  application->drop_desktop_files = FALSE;
  application->drop_occurred = FALSE;

  /* unset the highlight position */
  itembar = gtk_bin_get_child (GTK_BIN (window));
  panel_itembar_set_drop_highlight_item (PANEL_ITEMBAR (itembar), -1);
}



static gboolean
panel_application_window_id_exists (PanelApplication *application,
                                    gint              id)
{
  GSList *li;

  for (li = application->windows; li != NULL; li = li->next)
    if (panel_window_get_id (li->data) == id)
      return TRUE;

  return FALSE;
}



PanelApplication *
panel_application_get (void)
{
  static PanelApplication *application = NULL;

  if (G_LIKELY (application))
    {
      g_object_ref (G_OBJECT (application));
    }
  else
    {
      application = g_object_new (PANEL_TYPE_APPLICATION, NULL);
      g_object_add_weak_pointer (G_OBJECT (application), (gpointer) &application);
    }

  return application;
}



gboolean
panel_application_load (PanelApplication  *application,
                        gboolean           disable_wm_check)
{
#ifdef GDK_WINDOWING_X11
  Display    *display;
  WaitForWM  *wfwm;
  guint       i;
  gchar     **atom_names;

  if (!disable_wm_check)
    {
      display = XOpenDisplay (NULL);
      if (display == NULL)
        {
          g_message ("Unable to open display from environment variable DISPLAY='%s', exiting.",
                     g_getenv ("DISPLAY"));
          return FALSE;
        }

      /* setup data for wm checking */
      wfwm = g_slice_new0 (WaitForWM);
      wfwm->application = application;
      wfwm->dpy = display;
      wfwm->have_wm = FALSE;
      wfwm->counter = 0;

      /* preload wm atoms for all screens */
      wfwm->atom_count = XScreenCount (wfwm->dpy);
      wfwm->atoms = g_new (Atom, wfwm->atom_count);
      atom_names = g_new0 (gchar *, wfwm->atom_count + 1);

      for (i = 0; i < wfwm->atom_count; i++)
        atom_names[i] = g_strdup_printf ("WM_S%d", i);

      if (!XInternAtoms (wfwm->dpy, atom_names, wfwm->atom_count, False, wfwm->atoms))
        wfwm->atom_count = 0;

      g_strfreev (atom_names);

      /* setup timeout to check for a window manager */
      application->wait_for_wm_timeout_id =
          gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT_IDLE, 50, panel_application_wait_for_window_manager,
                                        wfwm, panel_application_wait_for_window_manager_destroyed);
    }
  else
    {
      /* directly launch */
      panel_application_load_real (application);
    }
#else
  /* directly launch */
  panel_application_load_real (application);
#endif

  return TRUE;
}



void
panel_application_save (PanelApplication *application,
                        PanelSaveTypes    save_types)
{
  GSList        *li;
  XfconfChannel *channel = application->xfconf;
  GValue        *value;
  GPtrArray     *panels = NULL;
  gint           panel_id;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (XFCONF_IS_CHANNEL (channel));

  /* leave if the whole application is locked */
  if (xfconf_channel_is_property_locked (channel, PANELS_PROPERTY_PREFIX))
    return;

  if (PANEL_HAS_FLAG (save_types, SAVE_PANEL_IDS))
    panels = g_ptr_array_new ();

  for (li = application->windows; li != NULL; li = li->next)
    {
      if (panels != NULL)
        {
          /* store the panel id */
          value = g_new0 (GValue, 1);
          panel_id = panel_window_get_id (li->data);
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, panel_id);
          g_ptr_array_add (panels, value);
        }

      /* save the panel settings */
      panel_application_save_window (application, li->data, save_types);
    }

  if (panels != NULL)
    {
      /* store the panel ids */
      if (!xfconf_channel_set_arrayv (channel, PANELS_PROPERTY_PREFIX, panels))
        g_warning ("Failed to store the number of panels");
      xfconf_array_free (panels);
    }
}



void
panel_application_save_window (PanelApplication *application,
                               PanelWindow      *window,
                               PanelSaveTypes    save_types)
{
  GList                   *children, *lp;
  GtkWidget               *itembar;
  XfcePanelPluginProvider *provider;
  gchar                    buf[50];
  XfconfChannel           *channel = application->xfconf;
  GPtrArray               *array = NULL;
  GValue                  *value;
  gint                     plugin_id;
  gint                     panel_id;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (PANEL_IS_WINDOW (window));

  /* skip this window if it is locked */
  if (panel_window_get_locked (window)
      || !PANEL_HAS_FLAG (save_types, SAVE_PLUGIN_IDS | SAVE_PLUGIN_PROVIDERS))
    return;

  panel_id = panel_window_get_id (window);
  panel_debug (PANEL_DEBUG_APPLICATION,
               "saving " PANELS_PROPERTY_BASE ": ids=%s, providers=%s",
               panel_id,
               PANEL_DEBUG_BOOL (PANEL_HAS_FLAG (save_types, SAVE_PLUGIN_IDS)),
               PANEL_DEBUG_BOOL (PANEL_HAS_FLAG (save_types, SAVE_PLUGIN_PROVIDERS)));

  /* get the itembar children */
  itembar = gtk_bin_get_child (GTK_BIN (window));
  children = gtk_container_get_children (GTK_CONTAINER (itembar));

  /* only cleanup and continue if there are no children */
  if (PANEL_HAS_FLAG (save_types, SAVE_PLUGIN_IDS))
    {
      if (G_UNLIKELY (children == NULL))
        {
          g_snprintf (buf, sizeof (buf), PLUGIN_IDS_PROPERTY_BASE, panel_id);
          if (xfconf_channel_has_property (channel, buf))
            xfconf_channel_reset_property (channel, buf, FALSE);
          return;
        }

      array = g_ptr_array_new ();
    }

  /* walk all the plugin children */
  for (lp = children; lp != NULL; lp = lp->next)
    {
      provider = XFCE_PANEL_PLUGIN_PROVIDER (lp->data);

      if (array != NULL)
        {
          plugin_id = xfce_panel_plugin_provider_get_unique_id (provider);

          /* add plugin id to the array */
          value = g_new0 (GValue, 1);
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, plugin_id);
          g_ptr_array_add (array, value);

          /* make sure the plugin type-name is store in the plugin item */
          g_snprintf (buf, sizeof (buf), PLUGINS_PROPERTY_BASE, plugin_id);
          xfconf_channel_set_string (channel, buf, xfce_panel_plugin_provider_get_name (provider));
        }

      /* ask the plugin to save */
      if (PANEL_HAS_FLAG (save_types, SAVE_PLUGIN_PROVIDERS))
        xfce_panel_plugin_provider_save (provider);
    }

  if (array != NULL)
    {
      /* store the plugin ids for this panel */
      g_snprintf (buf, sizeof (buf), PLUGIN_IDS_PROPERTY_BASE, panel_id);
      xfconf_channel_set_arrayv (channel, buf, array);
      xfconf_array_free (array);
    }

  g_list_free (children);
}



void
panel_application_take_dialog (PanelApplication *application,
                               GtkWindow        *dialog)
{
  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (GTK_IS_WINDOW (dialog));

  /* block autohide if this will be the first dialog */
  if (application->dialogs == NULL)
    panel_application_windows_blocked (application, TRUE);

  /* monitor window destruction */
  g_signal_connect (G_OBJECT (dialog), "destroy",
      G_CALLBACK (panel_application_dialog_destroyed), application);
  application->dialogs = g_slist_prepend (application->dialogs, dialog);
}



void
panel_application_destroy_dialogs (PanelApplication *application)
{
  GSList *li, *lnext;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));

  /* destroy all dialogs */
  for (li = application->dialogs; li != NULL; li = lnext)
    {
      lnext = li->next;
      gtk_widget_destroy (GTK_WIDGET (li->data));
    }

  panel_return_if_fail (application->dialogs == NULL);
}



void
panel_application_add_new_item (PanelApplication  *application,
                                PanelWindow       *window,
                                const gchar       *plugin_name,
                                gchar            **arguments)
{
  gint panel_id;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (plugin_name != NULL);
  panel_return_if_fail (application->windows != NULL);
  panel_return_if_fail (window == NULL || PANEL_IS_WINDOW (window));

  /* leave if the config is locked */
  if (panel_application_get_locked (application))
    return;

  if (panel_module_factory_has_module (application->factory, plugin_name))
    {
      if (window == NULL)
        {
          /* find a suitable panel if there are 2 or more panel */
          if (LIST_HAS_TWO_OR_MORE_ENTRIES (application->windows))
            {
              /* ask the user to select a panel */
              panel_id = panel_dialogs_choose_panel (application);
              if (panel_id == -1)
                {
                  /* cancel was clicked */
                  return;
                }
              else
                {
                  /* get panel from the id */
                  window = panel_application_get_window (application, panel_id);
                }
            }
          else
            {
              /* get the first (and only) window */
              window = g_slist_nth_data (application->windows, 0);
            }
        }

      if (window != NULL && !panel_window_get_locked (window))
        {
          /* insert plugin at the end of the panel */
          if (panel_application_plugin_insert (application, window,
                                               plugin_name, -1,
                                               arguments, -1))
            {
              /* save the new plugin ids */
              panel_application_save_window (application, window, SAVE_PLUGIN_IDS);
            }
        }
    }
  else
    {
      g_warning ("The plugin \"%s\" you want to add is not "
                 "known by the panel", plugin_name);
    }
}



PanelWindow *
panel_application_new_window (PanelApplication *application,
                              GdkScreen        *screen,
                              gint              panel_id,
                              gboolean          new_window)
{
  GtkWidget          *window;
  GtkWidget          *itembar;
  gchar              *property;
  gint                idx;
  static const gchar *props[] = { "mode", "size", "nrows", "icon-size", "dark-mode" };
  guint               i;
  gchar              *position;
  static gint         unqiue_id_counter = 1;
  GtkWindowGroup     *window_group;

  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), NULL);
  panel_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), NULL);
  panel_return_val_if_fail (XFCONF_IS_CHANNEL (application->xfconf), NULL);
  panel_return_val_if_fail (new_window || !panel_application_window_id_exists (application, panel_id), NULL);

  if (new_window)
    {
      /* get a new unique id */
      panel_id = unqiue_id_counter;
      while (panel_application_window_id_exists (application, panel_id))
        panel_id = ++unqiue_id_counter;
    }

  /* create panel window */
  window = panel_window_new (screen, panel_id, application->autohide_block);

  /* put the window in its own group */
  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_weak_ref (G_OBJECT (window), _panel_utils_weak_notify, window_group);

  /* add the window to internal list */
  application->windows = g_slist_append (application->windows, window);

  if (new_window)
    {
      /* remove the old xfconf properties to be sure */
      property = g_strdup_printf (PANELS_PROPERTY_BASE, panel_id);
      xfconf_channel_reset_property (application->xfconf, property, TRUE);
      g_free (property);
    }

  /* add the itembar */
  itembar = panel_itembar_new ();
  for (i = 0; i < G_N_ELEMENTS (props); i++)
    g_object_bind_property (G_OBJECT (window), props[i], G_OBJECT (itembar), props[i], G_BINDING_DEFAULT);
  gtk_container_add (GTK_CONTAINER (window), itembar);
  gtk_widget_show (itembar);

  /* set the itembar drag destination targets */
  gtk_drag_dest_set (GTK_WIDGET (window), 0,
                     drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  /* signals for drag and drop */
  g_signal_connect (G_OBJECT (window), "drag-data-received",
                    G_CALLBACK (panel_application_drag_data_received), itembar);
  g_signal_connect (G_OBJECT (window), "drag-motion",
                    G_CALLBACK (panel_application_drag_motion), application);
  g_signal_connect (G_OBJECT (window), "drag-drop",
                    G_CALLBACK (panel_application_drag_drop), application);
  g_signal_connect (G_OBJECT (window), "drag-leave",
                    G_CALLBACK (panel_application_drag_leave), application);

  /* add the xfconf bindings */
  panel_application_xfconf_window_bindings (application, PANEL_WINDOW (window), FALSE);

  /* make sure the panel has a valid position, else it is not visible */
  if (!panel_window_has_position (PANEL_WINDOW (window)))
    {
      if (!new_window)
        g_message ("No panel position set, restoring default");

      /* create a position so not all panels overlap */
      idx = g_slist_index (application->windows, window);
      position = g_strdup_printf ("p=0;x=100;y=%d", 30 + (idx * (48 + 10)));
      g_object_set (G_OBJECT (window), "position", position, NULL);
      g_object_set (G_OBJECT (window), "size", 48, NULL);
      g_free (position);
    }

  /* save the new panel layout */
  if (new_window)
    panel_application_save (application, SAVE_PANEL_IDS);

  return PANEL_WINDOW (window);
}



void
panel_application_remove_window (PanelApplication *application,
                                 PanelWindow      *window)
{
  gchar     *property;
  GtkWidget *itembar;
  gint       panel_id;

  panel_return_if_fail (PANEL_IS_WINDOW (window));
  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (g_slist_find (application->windows, window) != NULL);

  /* leave if the application or window is locked */
  if (panel_application_get_locked (application)
      || panel_window_get_locked (PANEL_WINDOW (window)))
    return;

  panel_id = panel_window_get_id (PANEL_WINDOW (window));
  panel_debug (PANEL_DEBUG_APPLICATION,
               "removing configuration and plugins of panel %d",
               panel_id);

  /* remove from the internal list */
  application->windows = g_slist_remove (application->windows, window);

  /* disconnect bindings from this panel */
  panel_properties_unbind (G_OBJECT (window));

  /* set all the plugins on this panel the remove signal */
  itembar = gtk_bin_get_child (GTK_BIN (window));
  gtk_container_foreach (GTK_CONTAINER (itembar),
      panel_application_plugin_remove, NULL);

  /* destroy */
  gtk_widget_destroy (GTK_WIDGET (window));

  /* remove the panel settings */
  property = g_strdup_printf (PANELS_PROPERTY_BASE, panel_id);
  xfconf_channel_reset_property (application->xfconf, property, TRUE);
  g_free (property);

  /* save changed panel ids */
  panel_application_save (application, SAVE_PANEL_IDS);

  /* quit if there are no windows */
  /* TODO, allow removing all windows and ask user what to do */
  if (application->windows == NULL)
    gtk_main_quit ();
}



GSList *
panel_application_get_windows (PanelApplication *application)
{
  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), NULL);
  return application->windows;
}



PanelWindow *
panel_application_get_window (PanelApplication  *application,
                              gint               panel_id)
{
  GSList *li;

  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), NULL);

  for (li = application->windows; li != NULL; li = li->next)
    if (panel_window_get_id (li->data) == panel_id)
      return li->data;

  return NULL;
}



void
panel_application_window_select (PanelApplication *application,
                                 PanelWindow      *window)
{
  GSList *li;

  panel_return_if_fail (PANEL_IS_APPLICATION (application));

  /* update state for all windows */
  for (li = application->windows; li != NULL; li = li->next)
    g_object_set (G_OBJECT (li->data), "active", window == li->data, NULL);
}



void
panel_application_windows_blocked (PanelApplication *application,
                                   gboolean          blocked)
{
  void (*autohide_func) (PanelWindow *);

  panel_return_if_fail (PANEL_IS_APPLICATION (application));
  panel_return_if_fail (application->autohide_block >= 0);

  if (blocked)
    {
      application->autohide_block++;
      autohide_func = panel_window_freeze_autohide;
    }
  else
    {
      application->autohide_block--;
      autohide_func = panel_window_thaw_autohide;
    }

  /* walk the windows */
  for (GSList *li = application->windows; li != NULL; li = li->next)
    autohide_func (li->data);
}



gboolean
panel_application_get_locked (PanelApplication *application)
{
  GSList *li;

  panel_return_val_if_fail (PANEL_IS_APPLICATION (application), TRUE);
  panel_return_val_if_fail (XFCONF_IS_CHANNEL (application->xfconf), TRUE);

  /* don't even look for the individual window if the
   * entire channel is locked */
  if (xfconf_channel_is_property_locked (application->xfconf, "/"))
    return TRUE;

  /* if one of the windows is not locked, the user can still modify
   * some settings, so then we return %FALSE */
  for (li = application->windows; li != NULL; li = li->next)
    if (!panel_window_get_locked (li->data))
      return FALSE;

  /* TODO we could extend this to a plugin basis (ie. panels are
   * locked but maybe not all the plugins) */
  return TRUE;
}



void
panel_application_logout (void)
{
  XfceSMClient *sm_client;
  GError       *error = NULL;
  const gchar  *command = "xfce4-session-logout";

  /* first try to session client to logout else fallback and spawn xfce4-session-logout */
  sm_client = xfce_sm_client_get ();
  if (xfce_sm_client_is_connected (sm_client))
    {
      xfce_sm_client_request_shutdown (sm_client, XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK);

      return;
    }
  else if (g_getenv ("SESSION_MANAGER") == NULL)
    {
      if (xfce_dialog_confirm (NULL, "application-exit", _("Quit"),
          _("You have started X without session manager. Clicking Quit will close the X server."),
          _("Are you sure you want to quit the panel?")))
        command = "xfce4-panel --quit";
      else
        return;
    }

  if (!g_spawn_command_line_async (command, &error))
    {
      xfce_dialog_show_error (NULL, error, _("Failed to execute command \"%s\""),
                              command);
      g_error_free (error);
    }
}
