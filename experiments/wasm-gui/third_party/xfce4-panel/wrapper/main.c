/*
 * Copyright (C) 2017 Ali Abdallah <ali@xfce.org>
 * Copyright (C) 2008-2009 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>

#include <gtk/gtk.h>
#include <common/panel-private.h>
#include <common/panel-dbus.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include <wrapper/wrapper-plug.h>
#include <wrapper/wrapper-module.h>



static GQuark   plug_quark = 0;
static gint     retval = PLUGIN_EXIT_FAILURE;



static void
wrapper_gproxy_name_owner_changed (GDBusProxy *proxy,
                                   GParamSpec *pspec,
                                   gpointer data)
{
   gchar *name_owner;

   name_owner = g_dbus_proxy_get_name_owner (proxy);

   /* we lost communication with the panel, silently close the wrapper */
   if (name_owner == NULL)
     gtk_main_quit ();

   g_free (name_owner);
}



static void
wrapper_gproxy_set (XfcePanelPluginProvider *provider,
                    GVariant                *parameters,
                    GDBusProxy              *proxy)
{
  WrapperPlug                    *plug;
  GVariantIter                    iter;
  GVariant                       *variant;
  XfcePanelPluginProviderPropType type;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));
  panel_return_if_fail (g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE));

  g_variant_iter_init (&iter, parameters);

  while (g_variant_iter_next (&iter, "(uv)", &type, &variant))
    {
      switch (type)
        {
        case PROVIDER_PROP_TYPE_SET_SIZE:
          xfce_panel_plugin_provider_set_size (provider, g_variant_get_int32 (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_ICON_SIZE:
          xfce_panel_plugin_provider_set_icon_size (provider, g_variant_get_int32 (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_DARK_MODE:
          xfce_panel_plugin_provider_set_dark_mode (provider, g_variant_get_boolean (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_MODE:
          xfce_panel_plugin_provider_set_mode (provider, g_variant_get_int32 (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_SCREEN_POSITION:
          xfce_panel_plugin_provider_set_screen_position (provider, g_variant_get_int32 (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_NROWS:
          xfce_panel_plugin_provider_set_nrows (provider, g_variant_get_int32 (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_LOCKED:
          xfce_panel_plugin_provider_set_locked (provider, g_variant_get_boolean (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_SENSITIVE:
          gtk_widget_set_sensitive (GTK_WIDGET (provider), g_variant_get_boolean (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_OPACITY:
          plug = g_object_get_qdata (G_OBJECT (provider), plug_quark);
          wrapper_plug_set_opacity (plug, g_variant_get_double (variant));
          break;

        case PROVIDER_PROP_TYPE_SET_BACKGROUND_COLOR:
        case PROVIDER_PROP_TYPE_SET_BACKGROUND_IMAGE:
        case PROVIDER_PROP_TYPE_ACTION_BACKGROUND_UNSET:
          plug = g_object_get_qdata (G_OBJECT (provider), plug_quark);

          if (type == PROVIDER_PROP_TYPE_SET_BACKGROUND_COLOR)
            wrapper_plug_set_background_color (plug, g_variant_get_string (variant, NULL));
          else if (type == PROVIDER_PROP_TYPE_SET_BACKGROUND_IMAGE)
            wrapper_plug_set_background_image (plug, g_variant_get_string (variant, NULL));
          else /* PROVIDER_PROP_TYPE_ACTION_BACKGROUND_UNSET */
            wrapper_plug_set_background_color (plug, NULL);
          break;

        case PROVIDER_PROP_TYPE_ACTION_REMOVED:
          xfce_panel_plugin_provider_removed (provider);
          break;

        case PROVIDER_PROP_TYPE_ACTION_SAVE:
          xfce_panel_plugin_provider_save (provider);
          break;

        case PROVIDER_PROP_TYPE_ACTION_QUIT_FOR_RESTART:
          retval = PLUGIN_EXIT_SUCCESS_AND_RESTART;
          /* fall through */
        case PROVIDER_PROP_TYPE_ACTION_QUIT:
          /* do not call gtk_main_quit() twice */
          g_signal_handlers_disconnect_by_func (proxy, wrapper_gproxy_name_owner_changed, NULL);
          gtk_main_quit ();
          break;

        case PROVIDER_PROP_TYPE_ACTION_SHOW_CONFIGURE:
          xfce_panel_plugin_provider_show_configure (provider);
          break;

        case PROVIDER_PROP_TYPE_ACTION_SHOW_ABOUT:
          xfce_panel_plugin_provider_show_about (provider);
          break;

        case PROVIDER_PROP_TYPE_ACTION_ASK_REMOVE:
          xfce_panel_plugin_provider_ask_remove (provider);
          break;

        default:
          g_critical ("Received unknown plugin property %u for %s-%d",
                      type, xfce_panel_plugin_provider_get_name (provider),
                      xfce_panel_plugin_provider_get_unique_id (provider));
          break;
        }

      g_variant_unref (variant);
    }
}


static void
wrapper_dbus_return_remote_event_result (GDBusProxy *proxy,
                                         guint handle,
                                         gboolean wrapper_result)
{
  GVariant *variant;
  GError   *error = NULL;

  variant = g_dbus_proxy_call_sync (proxy,
                                    "RemoteEventResult",
                                    g_variant_new ("(ub)",
                                                   handle,
                                                   wrapper_result),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);

  if (G_UNLIKELY (error != NULL ))
    {
      g_warning ("RemoteEventResult call failed: %s", error->message);
      g_error_free (error);
    }

  if (variant)
    g_variant_unref (variant);

}


static void
wrapper_gproxy_remote_event (XfcePanelPluginProvider *provider,
                             GDBusProxy *proxy,
                             GVariant   *parameters)
{
  GVariant     *variant;
  guint         handle;
  const gchar  *name;
  gboolean      result;
  GValue        real_value = { 0, };

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  if (G_LIKELY (g_variant_is_of_type (parameters, G_VARIANT_TYPE("(svu)"))))
    {
      g_variant_get (parameters, "(&svu)", &name, &variant, &handle);
      if ( g_variant_is_of_type (variant, G_VARIANT_TYPE_BYTE) &&
           g_variant_get_byte (variant) == '\0')
        {
          result = xfce_panel_plugin_provider_remote_event (provider, name, NULL, NULL);
        }
      else
        {
          g_dbus_gvariant_to_gvalue(variant, &real_value);
          result = xfce_panel_plugin_provider_remote_event (provider, name, &real_value, NULL);
          g_value_unset (&real_value);
        }

      wrapper_dbus_return_remote_event_result (proxy, handle, result);

      g_variant_unref (variant);
    }
  else
    {
      g_warning ("property changed handler expects (svu) type, but %s received",
                 g_variant_get_type_string(parameters));
    }

}



static void
wrapper_gproxy_g_signal (GDBusProxy *proxy,
                         gchar      *sender_name,
                         gchar      *signal_name,
                         GVariant   *parameters,
                         XfcePanelPluginProvider *provider)
{
  if (g_strcmp0(signal_name, "RemoteEvent") == 0)
    wrapper_gproxy_remote_event (provider, proxy, parameters);
  else if (g_strcmp0(signal_name, "Set") == 0)
    wrapper_gproxy_set (provider, parameters, proxy);
  else
    g_warning ("Unhandled signal name :%s", signal_name);
}



static void
wrapper_gproxy_provider_signal (XfcePanelPluginProvider       *provider,
                                XfcePanelPluginProviderSignal  provider_signal,
                                GDBusProxy                    *proxy)
{
  GVariant *variant;
  GError   *error = NULL;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  variant = g_dbus_proxy_call_sync (proxy,
                                    "ProviderSignal",
                                    g_variant_new ("(u)",
                                                   provider_signal),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);

  if (G_UNLIKELY (error != NULL ))
    {
      g_warning ("ProviderSignal call failed: %s", error->message);
      g_error_free (error);
    }

  if (variant)
    g_variant_unref (variant);
}



gint
main (gint argc, gchar **argv)
{
#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_NAME)
  gchar                    process_name[16];
#endif
  GModule                 *library = NULL;
  XfcePanelPluginPreInit   preinit_func;
  GDBusConnection         *dbus_gconnection;
  GDBusProxy              *dbus_gproxy = NULL;
  WrapperModule           *module = NULL;
  WrapperPlug             *plug;
  GtkWidget               *provider = NULL;
  gchar                   *path;
  GError                  *error = NULL;
  const gchar             *filename;
  gint                     unique_id;
  Window                   socket_id;
  const gchar             *name;
  const gchar             *display_name;
  const gchar             *comment;
  gchar                  **arguments;

  /* set translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* check if we have all the reuiqred arguments */
  if (G_UNLIKELY (argc < PLUGIN_ARGV_ARGUMENTS))
    {
      g_critical ("Not enough arguments are passed to the wrapper");
      return PLUGIN_EXIT_ARGUMENTS_FAILED;
    }

  /* put all arguments in understandable strings */
  filename = argv[PLUGIN_ARGV_FILENAME];
  unique_id = strtol (argv[PLUGIN_ARGV_UNIQUE_ID], NULL, 0);
  socket_id = strtol (argv[PLUGIN_ARGV_SOCKET_ID], NULL, 0);
  name = argv[PLUGIN_ARGV_NAME];
  display_name = argv[PLUGIN_ARGV_DISPLAY_NAME];
  comment = argv[PLUGIN_ARGV_COMMENT];
  arguments = argv + PLUGIN_ARGV_ARGUMENTS;

#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_NAME)
  /* change the process name to something that makes sence */
  g_snprintf (process_name, sizeof (process_name), "panel-%d-%s",
              unique_id, name);
  if (prctl (PR_SET_NAME, (gulong) process_name, 0, 0, 0) == -1)
    g_warning ("Failed to change the process name to \"%s\".", process_name);
#endif

  /* open the plugin module */
  library = g_module_open (filename, G_MODULE_BIND_LOCAL);
  if (G_UNLIKELY (library == NULL))
    {
      g_set_error (&error, 0, 0, "Failed to open plugin module \"%s\": %s",
                   filename, g_module_error ());
      goto leave;
    }

  /* check for a plugin preinit function */
  if (g_module_symbol (library, "xfce_panel_module_preinit", (gpointer) &preinit_func)
      && preinit_func != NULL
      && (*preinit_func) (argc, argv) == FALSE)
    {
      retval = PLUGIN_EXIT_PREINIT_FAILED;
      goto leave;
    }

  gtk_init (&argc, &argv);

  /* connect the dbus proxy */
  dbus_gconnection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (G_UNLIKELY (dbus_gconnection == NULL))
    goto leave;

  path = g_strdup_printf (PANEL_DBUS_WRAPPER_PATH, unique_id);
  dbus_gproxy = g_dbus_proxy_new_sync (dbus_gconnection,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       PANEL_DBUS_NAME,
                                       path,
                                       PANEL_DBUS_WRAPPER_INTERFACE,
                                       NULL,
                                       &error);
  g_free (path);
  if (G_UNLIKELY (dbus_gproxy == NULL))
    goto leave;

  /* quit when the proxy is destroyed (panel segfault for example) */
  g_signal_connect (G_OBJECT (dbus_gproxy), "notify::g-name-owner",
      G_CALLBACK (wrapper_gproxy_name_owner_changed), NULL);

  /* create the type module */
  module = wrapper_module_new (library);

  /* create the plugin provider */
  provider = wrapper_module_new_provider (module,
                                          gdk_screen_get_default (),
                                          name, unique_id,
                                          display_name, comment,
                                          arguments);

  if (G_LIKELY (provider != NULL))
    {
      /* create the wrapper plug */
      plug = wrapper_plug_new (socket_id);
      gtk_container_add (GTK_CONTAINER (plug), GTK_WIDGET (provider));
      g_object_add_weak_pointer (G_OBJECT (plug), (gpointer *) &plug);
      gtk_widget_show (GTK_WIDGET (plug));

      /* set plug data to provider */
      plug_quark = g_quark_from_static_string ("plug-quark");
      g_object_set_qdata (G_OBJECT (provider), plug_quark, plug);

      /* monitor provider signals */
      g_signal_connect (G_OBJECT (provider), "provider-signal",
          G_CALLBACK (wrapper_gproxy_provider_signal), dbus_gproxy);

      /* connect to service signals */
      g_signal_connect_object (dbus_gproxy, "g-signal",
                               G_CALLBACK (wrapper_gproxy_g_signal),
                               provider, 0);

      /* show the plugin */
      gtk_widget_show (GTK_WIDGET (provider));

      gtk_main ();

      /* destroy the plug and provider */
      if (plug != NULL)
        gtk_widget_destroy (GTK_WIDGET (plug));

      if (retval != PLUGIN_EXIT_SUCCESS_AND_RESTART)
        retval = PLUGIN_EXIT_SUCCESS;
    }
  else
    {
      retval = PLUGIN_EXIT_NO_PROVIDER;
    }

leave:
  if (G_LIKELY (dbus_gproxy != NULL))
    g_object_unref (G_OBJECT (dbus_gproxy));

  if (G_LIKELY (module != NULL))
    g_object_unref (G_OBJECT (module));

  if (G_LIKELY (library != NULL))
    g_module_close (library);

  if (G_UNLIKELY (error != NULL))
    {
      g_critical ("Wrapper %s-%d: %s.", name,
                  unique_id, error->message);
      g_error_free (error);
    }

  return retval;
}
