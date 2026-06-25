/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <garcon/garcon.h>
#include <xfconf/xfconf.h>

#include <src/appfinder-window.h>
#include <src/appfinder-private.h>
#include <src/appfinder-model.h>
#include <src/appfinder-gdbus.h>



static gboolean            opt_collapsed = FALSE;
static gboolean            opt_version = FALSE;
static gboolean            opt_replace = FALSE;
static gboolean            opt_quit = FALSE;
static gboolean            opt_disable_server = FALSE;
static GSList             *windows = NULL;
static gboolean            service_owner = FALSE;
static XfceAppfinderModel *model_cache = NULL;

#ifdef DEBUG
static GHashTable         *objects_table = NULL;
static guint               objects_table_count = 0;
#endif



static GOptionEntry option_entries[] =
{
  { "collapsed", 'c', 0, G_OPTION_ARG_NONE, &opt_collapsed, N_("Start in collapsed mode"), NULL },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Print version information and exit"), NULL },
  { "replace", 'r', 0, G_OPTION_ARG_NONE, &opt_replace, N_("Replace the existing service"), NULL },
  { "quit", 'q', 0, G_OPTION_ARG_NONE, &opt_quit, N_("Quit all instances"), NULL },
  { "disable-server", 0, 0, G_OPTION_ARG_NONE, &opt_disable_server, N_("Do not try to use or become a D-Bus service"), NULL },
  { NULL }
};



#ifdef DEBUG
static void
appfinder_refcount_debug_weak_notify (gpointer  data,
                                      GObject  *where_the_object_was)
{
  /* remove the unreffed object pixbuf from the table */
  if (!g_hash_table_remove (objects_table, where_the_object_was))
    appfinder_assert_not_reached ();
}



static void
appfinder_refcount_debug_print (gpointer object,
                                gpointer desc,
                                gpointer data)
{
  APPFINDER_DEBUG ("object %p (type = %s, desc = %s) not released",
                   object, G_OBJECT_TYPE_NAME (object), (gchar *) desc);
}



void
appfinder_refcount_debug_add (GObject     *object,
                              const gchar *description)
{
  /* silently ignore objects that are already registered */
  if (object != NULL
      && g_hash_table_lookup (objects_table, object) == NULL)
    {
      objects_table_count++;
      g_object_weak_ref (G_OBJECT (object), appfinder_refcount_debug_weak_notify, NULL);
      g_hash_table_insert (objects_table, object, g_strdup (description));
    }
}



static void
appfinder_refcount_debug_init (void)
{
  objects_table = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}



static void
appfinder_refcount_debug_finalize (void)
{
  /* leakup refcount hashtable */
  APPFINDER_DEBUG ("%d objects leaked, %d registered",
                   g_hash_table_size (objects_table),
                   objects_table_count);

  g_hash_table_foreach (objects_table, appfinder_refcount_debug_print, NULL);
  g_hash_table_destroy (objects_table);
}
#endif



static void
appfinder_window_destroyed (GtkWidget *window)
{
  XfconfChannel *channel;
  gboolean       sort_by_frecency;
  gint           scale_factor;

  if (windows == NULL)
    return;

  /* take a reference on the model */
  if (model_cache == NULL)
    {
      APPFINDER_DEBUG ("main took reference on the main model");
      channel = xfconf_channel_get ("xfce4-appfinder");
      sort_by_frecency = xfconf_channel_get_bool (channel, "/sort-by-frecency", FALSE);
      scale_factor = gtk_widget_get_scale_factor (window);
      model_cache = xfce_appfinder_model_get (sort_by_frecency, scale_factor);
    }

  /* remove from internal list */
  windows = g_slist_remove (windows, window);

  /* check if we're going to the background
   * if the last window is closed */
  if (windows == NULL)
    {
      if (!service_owner)
        {
          /* leave if we're not the daemon or started
           * with --disable-server */
          gtk_main_quit ();
        }
      else
        {
          /* leave if the user disable the service in the prefereces */
          channel = xfconf_channel_get ("xfce4-appfinder");
          if (!xfconf_channel_get_bool (channel, "/enable-service", TRUE))
            gtk_main_quit ();
        }
    }
}



void
appfinder_window_new (const gchar *startup_id,
                      gboolean     expanded)
{
  GtkWidget *window;
  XfconfChannel *channel;

  if (g_slist_length (windows) > 0)
    {
      channel = xfconf_channel_get ("xfce4-appfinder");
      if (xfconf_channel_get_bool (channel, "/enable-service", TRUE) &&
          xfconf_channel_get_bool (channel, "/single-window", TRUE))
        {
          gtk_window_present (GTK_WINDOW (g_slist_nth_data (windows, 0)));
          return;
        }
    }

  window = g_object_new (XFCE_TYPE_APPFINDER_WINDOW,
                         "startup-id", IS_STRING (startup_id) ? startup_id : NULL,
                         NULL);
  appfinder_refcount_debug_add (G_OBJECT (window), startup_id);
  xfce_appfinder_window_set_expanded (XFCE_APPFINDER_WINDOW (window), expanded);
  gtk_widget_show (window);

  windows = g_slist_prepend (windows, window);
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (appfinder_window_destroyed), NULL);
}



gint
main (gint argc, gchar **argv)
{
  GError      *error = NULL;
  const gchar *startup_id;
  GSList      *windows_destroy;

  /* set translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

#ifdef G_ENABLE_DEBUG
  /* do NOT remove this line for now, If something doesn't work,
   * fix your code instead! */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* get the startup notification id */
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");

  if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
      g_printerr ("%s: %s.\n", PACKAGE_NAME, error->message);
      g_printerr (_("Type \"%s --help\" for usage."), PACKAGE_NAME);
      g_printerr ("\n");
      g_error_free (error);

      return EXIT_FAILURE;
    }

  if (opt_version)
    {
      g_print ("%s %s (Xfce %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2004-2022");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (opt_quit)
    {
      return appfinder_gdbus_quit (NULL) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

  /* if started with the xfrun4 executable, start in collapsed mode */
  if (!opt_collapsed && strcmp (*argv, "xfrun4") == 0)
    opt_collapsed = TRUE;

  /* become the service owner or ask the current
   * owner to spawn an instance */
  if (G_LIKELY (!opt_disable_server))
    {
      /* try to open a new window */
      if (appfinder_gdbus_open_window (!opt_collapsed, startup_id, &error))
        {
          /* looks ok */
          return EXIT_SUCCESS;
        }
      else if (!g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER))
        {
          g_warning ("Unknown DBus error: %s", error->message);
        }

      g_clear_error (&error);

      /* become service owner */
      if (appfinder_gdbus_service (NULL))
        {
          /* successfully registered the service */
          service_owner = TRUE;

          APPFINDER_DEBUG ("requested dbus service");
        }
      else
        {
          g_warning ("Failed to register DBus serice");
        }
    }

  /* init garcon environment */
  garcon_set_environment_xdg (GARCON_ENVIRONMENT_XFCE);

  if (!xfconf_init (&error))
    {
       g_critical ("Failed to initialized xfconf: %s", error->message);
       g_error_free (error);

       return EXIT_FAILURE;
    }

#ifdef DEBUG
  appfinder_refcount_debug_init ();
#endif

  /* create initial window */
  appfinder_window_new (NULL, !opt_collapsed);

  APPFINDER_DEBUG ("enter mainloop");

  gtk_main ();

  /* release the model cache */
  if (model_cache != NULL)
    g_object_unref (G_OBJECT (model_cache));

  if (windows != NULL)
    {
      /* avoid calling appfinder_window_destroyed */
      windows_destroy = windows;
      windows = NULL;

      /* destroy all windows */
      g_slist_foreach (windows_destroy, (GFunc) (void (*)(void)) gtk_widget_destroy, NULL);
      g_slist_free (windows_destroy);
    }

  xfconf_shutdown ();

#ifdef DEBUG
  appfinder_refcount_debug_finalize ();
#endif

  return EXIT_SUCCESS;
}
