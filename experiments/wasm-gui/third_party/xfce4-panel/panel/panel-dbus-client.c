/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2017      Ali Abdallah  <ali@xfce.org>
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

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <common/panel-private.h>
#include <common/panel-dbus.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include <panel/panel-dbus-client.h>
#include <panel/panel-dbus-service.h>

#include <panel/panel-gdbus-exported-service.h>


enum
{
  PLUGIN_NAME,
  NAME,
  TYPE,
  VALUE,
  N_TOKENS
};



static XfcePanelExportedService *
panel_dbus_client_get_proxy (GError **error)
{
  return xfce_panel_exported_service_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                             G_DBUS_PROXY_FLAGS_NONE,
                                                             PANEL_DBUS_NAME,
                                                             PANEL_DBUS_PATH,
                                                             NULL,
                                                             error);
}



gboolean
panel_dbus_client_display_preferences_dialog (guint         active,
                                              guint         socket_id,
                                              GError      **error)
{
  XfcePanelExportedService *dbus_proxy;
  gboolean                  result;

  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_proxy = panel_dbus_client_get_proxy (error);
  if (G_UNLIKELY (dbus_proxy == NULL))
    return FALSE;

  result = xfce_panel_exported_service_call_display_preferences_dialog_sync (dbus_proxy,
                                                                             active,
                                                                             socket_id,
                                                                             NULL,
                                                                             error);

  g_object_unref (G_OBJECT (dbus_proxy));

  return result;
}



gboolean
panel_dbus_client_display_items_dialog (guint    active,
                                        GError **error)
{
  XfcePanelExportedService *dbus_proxy;
  gboolean                  result;


  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_proxy = panel_dbus_client_get_proxy (error);
  if (G_UNLIKELY (dbus_proxy == NULL))
    return FALSE;

  result = xfce_panel_exported_service_call_display_items_dialog_sync (dbus_proxy,
                                                                       active,
                                                                       NULL,
                                                                       error);

  g_object_unref (G_OBJECT (dbus_proxy));

  return result;
}



gboolean
panel_dbus_client_save (GError **error)
{
  XfcePanelExportedService *dbus_proxy;
  gboolean                  result;

  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_proxy = panel_dbus_client_get_proxy (error);
  if (G_UNLIKELY (dbus_proxy == NULL))
    return FALSE;

  result = xfce_panel_exported_service_call_save_sync (dbus_proxy, NULL, error);

  g_object_unref (G_OBJECT (dbus_proxy));

  return result;
}



gboolean
panel_dbus_client_add_new_item (const gchar  *plugin_name,
                                gchar       **arguments,
                                GError      **error)
{

  XfcePanelExportedService *dbus_proxy;
  gboolean                  result;
  const gchar              *empty_arguments[] = {"", NULL};

  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_proxy = panel_dbus_client_get_proxy (error);
  if (G_UNLIKELY (dbus_proxy == NULL))
    return FALSE;

  if (arguments != NULL && *arguments != NULL)
    result = xfce_panel_exported_service_call_add_new_item_sync (dbus_proxy, plugin_name,
                                                                 (const gchar **) arguments,
                                                                 NULL,
                                                                 error);
  else
    result = xfce_panel_exported_service_call_add_new_item_sync (dbus_proxy, plugin_name,
                                                                 empty_arguments,
                                                                 NULL,
                                                                 error);

  g_object_unref (G_OBJECT (dbus_proxy));

  return result;
}



static GType
panel_dbus_client_gtype_from_string (const gchar *str)
{
  if (strcmp (str, "bool") == 0)
    return G_TYPE_BOOLEAN;
  else if (strcmp (str, "double") == 0)
    return G_TYPE_DOUBLE;
  else if (strcmp (str, "int") == 0)
    return G_TYPE_INT;
  else if (strcmp (str, "string") == 0)
    return G_TYPE_STRING;
  else if (strcmp (str, "uint") == 0)
    return G_TYPE_UINT;
  else
    return G_TYPE_NONE;
}



gboolean
panel_dbus_client_plugin_event (const gchar  *plugin_event,
                                gboolean     *return_succeed,
                                GError      **error)
{
  XfcePanelExportedService  *dbus_proxy;
  gchar                    **tokens;
  GType                      type;
  guint                      n_tokens;
  gboolean                   result = FALSE;
  GVariant                  *variant = NULL;

  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_proxy = panel_dbus_client_get_proxy (error);
  if (G_UNLIKELY (dbus_proxy == NULL))
    return FALSE;

  tokens = g_strsplit (plugin_event, ":", -1);
  n_tokens = g_strv_length (tokens);

  if (!(n_tokens == 2 || n_tokens == N_TOKENS)
      || panel_str_is_empty (tokens[PLUGIN_NAME])
      || panel_str_is_empty (tokens[NAME]))
    {
      g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                           _("Invalid plugin event syntax specified. "
                             "Use PLUGIN-NAME:NAME[:TYPE:VALUE]."));
      goto out;
    }
  else if (n_tokens == 2)
    {
      /* set noop value, recognized by the dbus service as %NULL value */
      variant = g_variant_new_byte ('\0');
    }
  else if (n_tokens == N_TOKENS)
    {
      type = panel_dbus_client_gtype_from_string (tokens[TYPE]);
      if (G_LIKELY (type != G_TYPE_NONE))
        {
          if (type == G_TYPE_BOOLEAN)
            variant = g_variant_new_boolean (strcmp (tokens[VALUE], "true") == 0);
          else if (type == G_TYPE_DOUBLE)
            variant = g_variant_new_double (g_ascii_strtod (tokens[VALUE], NULL));
          else if (type == G_TYPE_INT)
            variant = g_variant_new_int64 (g_ascii_strtoll (tokens[VALUE], NULL, 0));
          else if (type == G_TYPE_STRING)
            variant = g_variant_new_string (tokens[VALUE]);
          else if (type == G_TYPE_UINT)
            variant = g_variant_new_uint64 (g_ascii_strtoll (tokens[VALUE], NULL, 0));
          else
            panel_assert_not_reached ();
        }
      else
        {
          g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                       _("Invalid hint type \"%s\". Valid types "
                         "are bool, double, int, string and uint."),
                       tokens[TYPE]);
          goto out;
        }
    }
  else
    {
      panel_assert_not_reached ();
      goto out;
    }

  /* send value over dbus */
  panel_return_val_if_fail (variant != NULL, FALSE);
  result = xfce_panel_exported_service_call_plugin_event_sync (dbus_proxy,
                                                               tokens[PLUGIN_NAME],
                                                               tokens[NAME],
                                                               g_variant_new_variant(variant),
                                                               return_succeed,
                                                               NULL,
                                                               error);
out:
  g_strfreev (tokens);
  g_object_unref (G_OBJECT (dbus_proxy));

  return result;
}



gboolean
panel_dbus_client_terminate (gboolean   restart,
                             GError   **error)
{
  XfcePanelExportedService *dbus_proxy;
  gboolean                  result;

  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_proxy = panel_dbus_client_get_proxy (error);
  if (G_UNLIKELY (dbus_proxy == NULL))
    return FALSE;

  result = xfce_panel_exported_service_call_terminate_sync (dbus_proxy, restart, NULL, error);

  g_object_unref (G_OBJECT (dbus_proxy));

  return result;
}
