/*
 * Copyright (C) 2017 Ali Abdallah <ali@xfce.org>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>
#include <common/panel-private.h>
#include <common/panel-dbus.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include <panel/panel-dbus-service.h>
#include <panel/panel-application.h>
#include <panel/panel-preferences-dialog.h>
#include <panel/panel-item-dialog.h>
#include <panel/panel-module-factory.h>

#include <panel/panel-gdbus-exported-service.h>

static void      panel_dbus_service_finalize                   (GObject                 *object);
static void      panel_dbus_service_plugin_event_result        (XfcePanelPluginProvider *prev_provider,
                                                                guint                    handle,
                                                                gboolean                 result,
                                                                PanelDBusService        *service);

static gboolean  panel_dbus_service_display_preferences_dialog (XfcePanelExportedService *skeleton,
                                                                GDBusMethodInvocation    *invocation,
                                                                guint                     active,
                                                                guint                     socket_id,
                                                                PanelDBusService         *service);
static gboolean  panel_dbus_service_display_items_dialog       (XfcePanelExportedService *skeleton,
                                                                GDBusMethodInvocation    *invocation,
                                                                guint                     active,
                                                                PanelDBusService         *service);
static gboolean  panel_dbus_service_save                       (XfcePanelExportedService *skeleton,
                                                                GDBusMethodInvocation    *invocation,
                                                                PanelDBusService         *service);
static gboolean  panel_dbus_service_add_new_item               (XfcePanelExportedService *skeleton,
                                                                GDBusMethodInvocation    *invocation,
                                                                const gchar              *plugin_name,
                                                                gchar                   **arguments,
                                                                PanelDBusService         *service);
static void      panel_dbus_service_plugin_event_free          (gpointer                  data);
static gboolean  panel_dbus_service_plugin_event               (XfcePanelExportedService *skeleton,
                                                                GDBusMethodInvocation    *invocation,
                                                                const gchar              *plugin_name,
                                                                const gchar              *name,
                                                                GVariant                 *variant,
                                                                PanelDBusService         *service);
static gboolean  panel_dbus_service_terminate                  (XfcePanelExportedService *skeleton,
                                                                GDBusMethodInvocation    *invocation,
                                                                gboolean                  restart,
                                                                PanelDBusService         *service);




struct _PanelDBusServiceClass
{
  XfcePanelExportedServiceSkeletonClass __parent__;
};

struct _PanelDBusService
{
  XfcePanelExportedServiceSkeleton __parent__;

  /* the dbus connection */
  GDBusConnection *connection;

  /* queue for remote-events */
  GHashTable      *remote_events;
};

typedef struct
{
  guint   handle;
  gchar  *name;
  GValue  value;
  GSList *plugins;
}
PluginEvent;



/* shared boolean for restart or quit */
static gboolean dbus_exit_restart = FALSE;



G_DEFINE_TYPE (PanelDBusService, panel_dbus_service, XFCE_PANEL_TYPE_EXPORTED_SERVICE_SKELETON)



static void
panel_dbus_service_class_init (PanelDBusServiceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = panel_dbus_service_finalize;

}



static void
panel_dbus_service_init (PanelDBusService *service)
{
  GError *error = NULL;

  service->remote_events = NULL;

  service->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (G_LIKELY (service->connection != NULL))
    {
      if (G_LIKELY (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service),
                                                      service->connection,
                                                      "/org/xfce/Panel",
                                                      &error)))
        {
          g_signal_connect (service, "handle_add_new_item",
                            G_CALLBACK(panel_dbus_service_add_new_item), service);
          g_signal_connect (service, "handle_display_items_dialog",
                            G_CALLBACK(panel_dbus_service_display_items_dialog), service);
          g_signal_connect (service, "handle_display_preferences_dialog",
                            G_CALLBACK(panel_dbus_service_display_preferences_dialog), service);
          g_signal_connect (service, "handle_plugin_event",
                            G_CALLBACK(panel_dbus_service_plugin_event), service);
          g_signal_connect (service, "handle_save",
                            G_CALLBACK(panel_dbus_service_save), service);
          g_signal_connect (service, "handle_terminate",
                            G_CALLBACK(panel_dbus_service_terminate), service);
        }
      else
        {
          g_critical ("Failed to export panel D-BUS interface: %s", error->message);
          g_error_free (error);
        }
    }
  else
    {
      g_warning ("Failed to connect to the D-BUS session bus: %s", error->message);
      g_error_free (error);
    }
}



static void
panel_dbus_service_finalize (GObject *object)
{
  PanelDBusService *service = PANEL_DBUS_SERVICE (object);

  if (service->remote_events != NULL)
    {
      panel_return_if_fail (g_hash_table_size (service->remote_events) == 0);
      g_hash_table_destroy (service->remote_events);
    }

  (*G_OBJECT_CLASS (panel_dbus_service_parent_class)->finalize) (object);
}



static gboolean
panel_dbus_service_display_preferences_dialog (XfcePanelExportedService *skeleton,
                                               GDBusMethodInvocation    *invocation,
                                               guint                     active,
                                               guint                     socket_id,
                                               PanelDBusService         *service)
{
  panel_return_val_if_fail (PANEL_IS_DBUS_SERVICE (service), FALSE);

  /* show the preferences dialog */
  panel_preferences_dialog_show_from_id (active, socket_id);

  xfce_panel_exported_service_complete_display_preferences_dialog(skeleton, invocation);

  return TRUE;
}



static gboolean
panel_dbus_service_display_items_dialog (XfcePanelExportedService *skeleton,
                                         GDBusMethodInvocation    *invocation,
                                         guint                     active,
                                         PanelDBusService         *service)
{
  panel_return_val_if_fail (PANEL_IS_DBUS_SERVICE (service), FALSE);

  /* show the items dialog */
  panel_item_dialog_show_from_id (active);

  xfce_panel_exported_service_complete_display_items_dialog (skeleton, invocation);

  return TRUE;
}



static gboolean
panel_dbus_service_save (XfcePanelExportedService *skeleton,
                         GDBusMethodInvocation    *invocation,
                         PanelDBusService         *service)
{
  PanelApplication *application;

  panel_return_val_if_fail (PANEL_IS_DBUS_SERVICE (service), FALSE);

  /* save the configuration */
  application = panel_application_get ();
  panel_application_save (application, SAVE_EVERYTHING);
  g_object_unref (G_OBJECT (application));

  xfce_panel_exported_service_complete_save (skeleton,
                                             invocation);

  return TRUE;
}



static gboolean
panel_dbus_service_add_new_item (XfcePanelExportedService *skeleton,
                                 GDBusMethodInvocation    *invocation,
                                 const gchar              *plugin_name,
                                 gchar                   **arguments,
                                 PanelDBusService         *service)
{
  PanelApplication *application;

  panel_return_val_if_fail (PANEL_IS_DBUS_SERVICE (service), FALSE);
  panel_return_val_if_fail (plugin_name != NULL, FALSE);

  application = panel_application_get ();

  if (arguments != NULL && panel_str_is_empty (*arguments))
    arguments = NULL;

  /* add new plugin (with or without arguments) */
  panel_application_add_new_item (application, NULL, plugin_name, arguments);

  g_object_unref (G_OBJECT (application));

  xfce_panel_exported_service_complete_add_new_item (skeleton,
                                                     invocation);

  return TRUE;

}



static gboolean
panel_dbus_service_plugin_event (XfcePanelExportedService *skeleton,
                                 GDBusMethodInvocation    *invocation,
                                 const gchar              *plugin_name,
                                 const gchar              *name,
                                 GVariant                 *variant,
                                 PanelDBusService         *service)
{
  GSList             *plugins, *li, *lnext;
  PanelModuleFactory *factory;
  PluginEvent        *event;
  guint               handle;
  gboolean            result;
  GValue              value = { 0, };
  gboolean            plugin_replied = FALSE;

  panel_return_val_if_fail (PANEL_IS_DBUS_SERVICE (service), FALSE);
  panel_return_val_if_fail (plugin_name != NULL, FALSE);
  panel_return_val_if_fail (name != NULL, FALSE);

  /* send the event to all matching plugins, break if one of the
   * plugins returns TRUE in this remote-event handler */
  factory = panel_module_factory_get ();
  plugins = panel_module_factory_get_plugins (factory, plugin_name);
  variant = g_variant_get_variant (variant);

  g_dbus_gvariant_to_gvalue (variant, &value);

  for (li = plugins, handle = 0; li != NULL; li = lnext, handle = 0)
    {
      lnext = li->next;

      panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (li->data), FALSE);

      result = xfce_panel_plugin_provider_remote_event (li->data, name, &value, &handle);

      if (handle > 0 && lnext != NULL)
        {
          event = g_slice_new0 (PluginEvent);
          event->handle = handle;
          event->name = g_strdup (name);
          event->plugins = g_slist_copy (lnext);
          g_value_init (&event->value, G_VALUE_TYPE (&value));
          g_value_copy (&value, &event->value);

          /* create hash table if needed */
          if (service->remote_events == NULL)
            service->remote_events = g_hash_table_new_full (g_int_hash, g_int_equal, NULL,
                                                            panel_dbus_service_plugin_event_free);

          g_hash_table_insert (service->remote_events, &event->handle, event);
          g_signal_connect (G_OBJECT (li->data), "remote-event-result",
              G_CALLBACK (panel_dbus_service_plugin_event_result), service);

          /* not entirely sure the event is handled, but at least suitable
           * plugins were found */
          plugin_replied = TRUE;

          /* we're going to wait until the plugin replied */
          break;
        }
      else if (result)
        {
          /* we've handled the event */
          plugin_replied = TRUE;

          /* plugin returned %TRUE, so abort the event notification */
          break;
        }
    }

  g_slist_free (plugins);
  g_object_unref (G_OBJECT (factory));

  xfce_panel_exported_service_complete_plugin_event (skeleton, invocation, plugin_replied);

  return TRUE;

}



static gboolean
panel_dbus_service_terminate (XfcePanelExportedService *skeleton,
                              GDBusMethodInvocation    *invocation,
                              gboolean                  restart,
                              PanelDBusService         *service)
{
  panel_return_val_if_fail (PANEL_IS_DBUS_SERVICE (service), FALSE);

  panel_dbus_service_exit_panel (restart);

  xfce_panel_exported_service_complete_terminate (skeleton,
                                                  invocation);

  return TRUE;

}



static void
panel_dbus_service_plugin_event_free (gpointer data)
{
  PluginEvent *event = data;

  g_value_unset (&event->value);
  g_free (event->name);
  g_slist_free (event->plugins);
  g_slice_free (PluginEvent, event);
}



static void
panel_dbus_service_plugin_event_result (XfcePanelPluginProvider *prev_provider,
                                        guint                    handle,
                                        gboolean                 result,
                                        PanelDBusService        *service)
{
  PluginEvent             *event;
  GSList                  *li, *lnext;
  XfcePanelPluginProvider *provider;
  guint                    new_handle;
  gboolean                 new_result;

  g_signal_handlers_disconnect_by_func (G_OBJECT (prev_provider),
      G_CALLBACK (panel_dbus_service_plugin_event_result), service);

  event = g_hash_table_lookup (service->remote_events, &handle);
  if (G_LIKELY (event != NULL))
    {
      panel_assert (event->handle == handle);
      if (!result)
        {
          for (li = event->plugins, new_handle = 0; li != NULL; li = lnext, new_handle = 0)
            {
              lnext = li->next;
              provider = li->data;
              event->plugins = g_slist_delete_link (event->plugins, li);
              new_handle = 0;

              /* maybe the plugin has been destroyed */
              if (!XFCE_PANEL_PLUGIN_PROVIDER (provider))
                continue;

              new_result = xfce_panel_plugin_provider_remote_event (provider, event->name,
                                                                    &event->value, &new_handle);

              if (new_handle > 0 && lnext != NULL)
                {
                  /* steal the old value */
                  g_hash_table_steal (service->remote_events, &handle);

                  /* update handle and insert again */
                  event->handle = new_handle;
                  g_hash_table_insert (service->remote_events, &event->handle, event);
                  g_signal_connect (G_OBJECT (provider), "remote-event-result",
                      G_CALLBACK (panel_dbus_service_plugin_event_result), service);

                  /* leave and wait for reply */
                  return;
                }
              else if (new_result)
                {
                  /* we're done, remove from hash table below */
                  break;
                }
            }
        }

      /* handle can be removed */
      g_hash_table_remove (service->remote_events, &handle);
    }
}



PanelDBusService *
panel_dbus_service_get (void)
{
  static PanelDBusService *service = NULL;

  if (G_LIKELY (service != NULL))
    {
      g_object_ref (G_OBJECT (service));
    }
  else
    {
      service = g_object_new (PANEL_TYPE_DBUS_SERVICE, NULL);
      g_object_add_weak_pointer (G_OBJECT (service), (gpointer) &service);
    }

  return service;
}



void
panel_dbus_service_exit_panel (gboolean restart)
{
  XfceSMClient *sm_client;

  sm_client = xfce_sm_client_get ();
  xfce_sm_client_set_restart_style (sm_client, XFCE_SM_CLIENT_RESTART_NORMAL);

  dbus_exit_restart = !!restart;

  gtk_main_quit ();
}



gboolean
panel_dbus_service_get_restart (void)
{
  return dbus_exit_restart;
}
