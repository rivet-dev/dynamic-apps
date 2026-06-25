/*
 * Copyright (C) 2013 Nick Schermer <nick@xfce.org>
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

#include <libxfce4ui/libxfce4ui.h>

#include <src/appfinder-gdbus.h>
#include <src/appfinder-private.h>



#define APPFINDER_DBUS_SERVICE     "org.xfce.Appfinder"
#define APPFINDER_DBUS_INTERFACE   APPFINDER_DBUS_SERVICE
#define APPFINDER_DBUS_PATH        "/org/xfce/Appfinder"
#define APPFINDER_DBUS_METHOD_OPEN "OpenWindow"
#define APPFINDER_DBUS_METHOD_QUIT "Quit"



static const gchar appfinder_gdbus_introspection_xml[] =
  "<node>"
    "<interface name='" APPFINDER_DBUS_INTERFACE "'>"
      "<method name='" APPFINDER_DBUS_METHOD_OPEN "'>"
        "<arg type='b' name='expanded' direction='in'/>"
        "<arg type='s' name='startup-id' direction='in'/>"
      "</method>"
      "<method name='" APPFINDER_DBUS_METHOD_QUIT "'/>"
    "</interface>"
  "</node>";



static void
appfinder_gdbus_method_call (GDBusConnection       *connection,
                             const gchar           *sender,
                             const gchar           *object_path,
                             const gchar           *interface_name,
                             const gchar           *method_name,
                             GVariant              *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer               user_data)
{
  gboolean  expanded;
  gchar    *startup_id = NULL;

  g_return_if_fail (!g_strcmp0 (object_path, APPFINDER_DBUS_PATH));
  g_return_if_fail (!g_strcmp0 (interface_name, APPFINDER_DBUS_INTERFACE));

  APPFINDER_DEBUG ("received dbus method %s", method_name);

  if (g_strcmp0 (method_name, APPFINDER_DBUS_METHOD_OPEN) == 0)
    {
      /* get paramenters */
      g_variant_get (parameters, "(bs)", &expanded, &startup_id);

      appfinder_window_new (startup_id, expanded);

      /* everything went fine */
      g_dbus_method_invocation_return_value (invocation, NULL);

      g_free (startup_id);
    }
  else if (g_strcmp0 (method_name, APPFINDER_DBUS_METHOD_QUIT) == 0)
    {
      /* close all windows and quit */
      g_printerr ("%s: %s.\n", PACKAGE_NAME, _("Forced to quit"));

      gtk_main_quit ();
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation,
          G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
          "Unknown method for DBus service " APPFINDER_DBUS_SERVICE);
    }
}



static const GDBusInterfaceVTable appfinder_gdbus_vtable =
{
  appfinder_gdbus_method_call,
  NULL, /* get property */
  NULL  /* set property */
};



static void
appfinder_gdbus_bus_acquired (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer         user_data)
{
  guint          register_id;
  GDBusNodeInfo *info;
  GError        *error = NULL;

  info = g_dbus_node_info_new_for_xml (appfinder_gdbus_introspection_xml, NULL);
  g_assert (info != NULL);
  g_assert (*info->interfaces != NULL);

  register_id = g_dbus_connection_register_object (connection,
                                                   APPFINDER_DBUS_PATH,
                                                   *info->interfaces, /* first iface */
                                                   &appfinder_gdbus_vtable,
                                                   user_data,
                                                   NULL,
                                                   &error);

  APPFINDER_DEBUG ("registered interface with id %d", register_id);

  if (register_id == 0)
    {
      g_message ("Failed to register object: %s", error->message);
      g_error_free (error);
    }

  g_dbus_node_info_unref (info);
}



gboolean
appfinder_gdbus_service (GError **error)
{
  guint owner_id;

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             APPFINDER_DBUS_SERVICE,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             appfinder_gdbus_bus_acquired,
                             NULL,
                             NULL,
                             NULL,
                             NULL);

  return (owner_id != 0);
}



gboolean
appfinder_gdbus_quit (GError **error)
{
  GVariant        *reply;
  GDBusConnection *connection;
  GError          *err = NULL;
  gboolean         result;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
  if (G_UNLIKELY (connection == NULL))
    return FALSE;

  reply = g_dbus_connection_call_sync (connection,
                                       APPFINDER_DBUS_SERVICE,
                                       APPFINDER_DBUS_PATH,
                                       APPFINDER_DBUS_INTERFACE,
                                       APPFINDER_DBUS_METHOD_QUIT,
                                       NULL,
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                       2000,
                                       NULL,
                                       &err);

  g_object_unref (connection);

  result = (reply != NULL);
  if (G_LIKELY (result))
    g_variant_unref (reply);
  else
    g_propagate_error (error, err);

  return result;
}



gboolean
appfinder_gdbus_open_window (gboolean      expanded,
                             const gchar  *startup_id,
                             GError      **error)
{
  GVariant        *reply;
  GDBusConnection *connection;
  GError          *err = NULL;
  gboolean         result;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
  if (G_UNLIKELY (connection == NULL))
    return FALSE;

  if (startup_id == NULL)
    startup_id = "";

  reply = g_dbus_connection_call_sync (connection,
                                       APPFINDER_DBUS_SERVICE,
                                       APPFINDER_DBUS_PATH,
                                       APPFINDER_DBUS_INTERFACE,
                                       APPFINDER_DBUS_METHOD_OPEN,
                                       g_variant_new ("(bs)",
                                                      expanded,
                                                      startup_id),
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                       2000,
                                       NULL,
                                       &err);

  g_object_unref (connection);

  result = (reply != NULL);
  if (G_LIKELY (result))
    g_variant_unref (reply);
  else
    g_propagate_error (error, err);

  return result;
}
