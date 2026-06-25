/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <common/panel-private.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-macros.h>

#include <migrate/migrate-config.h>
#include <migrate/migrate-default.h>



#define DEFAULT_CONFIG_FILENAME "xfce4" G_DIR_SEPARATOR_S "panel" G_DIR_SEPARATOR_S "default.xml"
#define DEFAULT_CONFIG_PATH     XDGCONFIGDIR G_DIR_SEPARATOR_S DEFAULT_CONFIG_FILENAME


gint
main (gint argc, gchar **argv)
{
  GError        *error = NULL;
  gint           retval = EXIT_SUCCESS;
  XfconfChannel *channel;
  gint           configver;
  gchar         *filename_default;
  gboolean       migrate_vendor_default;

  /* set translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  gtk_init (&argc, &argv);

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialize Xfconf: %s", error->message);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  channel = xfconf_channel_get (XFCE_PANEL_CHANNEL_NAME);
  if (!xfconf_channel_has_property (channel, PANELS_PROPERTY_PREFIX))
    {
      /* lookup the default configuration */
      xfce_resource_push_path (XFCE_RESOURCE_CONFIG, XDGCONFIGDIR);
      filename_default = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, DEFAULT_CONFIG_FILENAME);
      xfce_resource_pop_path (XFCE_RESOURCE_CONFIG);

      if (filename_default == NULL)
        {
          g_warning ("No default configuration found");
          return EXIT_FAILURE;
        }

      /* if the default configuration does not match with the file found
       * by the resource lookup, migrate it without asking */
      migrate_vendor_default = (g_strcmp0 (DEFAULT_CONFIG_PATH, filename_default) != 0);

      /* check if we auto-migrate the default configuration */
      if (g_getenv ("XFCE_PANEL_MIGRATE_DEFAULT") != NULL
          || migrate_vendor_default)
        {
          if (filename_default == NULL)
            g_message ("Tried to auto-migrate, but no default configuration found");
          else
            goto migrate_default;
        }

      if (filename_default != NULL)
        {
          migrate_default:

          /* apply default config */
          if (!migrate_default (filename_default, &error))
            {
              xfce_dialog_show_error (NULL, error, _("Failed to load the default configuration"));
              g_error_free (error);
              retval = EXIT_FAILURE;
            }
        }

      g_free (filename_default);
    }

  configver = xfconf_channel_get_int (channel, "/configver", -1);
  if (configver < XFCE4_PANEL_CONFIG_VERSION)
    {
      g_message (_("Panel config needs migration..."));

      if (!migrate_config (channel, configver, &error))
        {
          xfce_dialog_show_error (NULL, error, _("Failed to migrate the existing configuration"));
          g_error_free (error);
          retval = EXIT_FAILURE;
        }
      else
        {
          g_message (_("Panel configuration has been updated."));
        }

      /* migration complete, set new version */
      xfconf_channel_set_int (channel, "/configver", XFCE4_PANEL_CONFIG_VERSION);
    }

  xfconf_shutdown ();

  return retval;
}
