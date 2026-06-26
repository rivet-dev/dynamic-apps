/*  $Id$
 *
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "libscreenshooter.h"
#include <glib.h>
#include <stdlib.h>



/* Set default values for cli args */
gboolean version = FALSE;
gboolean window = FALSE;
gboolean region = FALSE;
gboolean fullscreen = FALSE;
gboolean mouse = FALSE;
gboolean no_border = FALSE;
gboolean supported_formats = FALSE;
gboolean clipboard = FALSE;
gboolean show_in_folder = FALSE;
gchar *screenshot_dir = NULL;
gchar *application = NULL;
gint delay = 0;



/* Set cli options. */
static GOptionEntry entries[] =
{
  {
    "clipboard", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &clipboard,
    N_("Copy the screenshot to the clipboard"),
    NULL
  },
  {
    "delay", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &delay,
    N_("Delay in seconds before taking the screenshot"),
    NULL
  },
  {
    "fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &fullscreen,
    N_("Take a screenshot of the entire screen"),
    NULL
  },
  {
    "mouse", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &mouse,
    N_("Display the mouse on the screenshot"),
    NULL
  },
  {
    "no-border", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &no_border,
    N_("Removes the window border from the screenshot"),
    NULL
  },
  {
    "open", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &application,
    N_("Application to open the screenshot"),
    NULL
  },
  {
    "region", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &region,
    N_("Select a region to be captured by clicking a point of the screen "
       "without releasing the mouse button, dragging your mouse to the "
       "other corner of the region, and releasing the mouse button."),
    NULL
  },
  {
    "save", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &screenshot_dir,
    N_("File path or directory where the screenshot will be saved, accepts png, jpg and bmp extensions. webp, jxl and avif are only supported if their respective pixbuf loaders are installed."),
    NULL
  },
  {
    "show in folder", 'S', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &show_in_folder,
    N_("Show the saved file in the folder."),
    NULL
  },
  {
    "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
    N_("Version information"),
    NULL
  },
  {
    "window", 'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &window,
    N_("Take a screenshot of the active window"),
    NULL
  },
  {
    "supported-formats", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &supported_formats,
    N_("Lists supported image formats, results can vary depending on installed pixbuf loaders"),
    NULL
  },
  {
    NULL, ' ', 0, 0, NULL,
    NULL,
    NULL
  }
};



/* Called when the screenshooter flow is finalized, successful or not
action_executed: whether the action was executed successfully.
data: what was defined in sd->finalize_callback_data, in this case unused.
*/
static void
cb_finalize (gboolean action_executed, gpointer data)
{
  TRACE ("Execute finalize callback");
  gtk_main_quit ();
}



/* Main */



int main (int argc, char **argv)
{
  ScreenshotData *sd;
  GError *cli_error = NULL;
  GFile *default_save_dir;
  const gchar *rc_file;
  const gchar *conflict_error =
    _("Conflicting options: --%s and --%s cannot be used at the same time.\n");
  const gchar *ignore_error =
    _("The --%s option is only used when --fullscreen, --window or"
      " --region is given. It will be ignored.\n");

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* Print a message to advise to use help when a non existing cli option is
  passed to the executable. */
  if (!gtk_init_with_args(&argc, &argv, NULL, entries, PACKAGE, &cli_error))
    {
      if (cli_error != NULL)
        {
          g_print (_("%s: %s\nTry %s --help to see a full list of"
                     " available command line options.\n"),
                   PACKAGE, cli_error->message, PACKAGE_NAME);

          g_error_free (cli_error);

          return EXIT_FAILURE;
        }
    }

  /* Exit if two region options were given */
  if (window && fullscreen)
    {
      g_printerr (conflict_error, "window", "fullscreen");

      return EXIT_FAILURE;
    }
  else if (window && region)
    {
      g_printerr (conflict_error, "window", "region");

      return EXIT_FAILURE;
    }
  else if (fullscreen && region)
    {
      g_printerr (conflict_error, "fullscreen", "region");

      return EXIT_FAILURE;
    }

  /* Exit if two actions options were given */
  if ((application != NULL) && (screenshot_dir != NULL))
    {
      g_printerr (conflict_error, "open", "save");

      return EXIT_FAILURE;
    }

  /* Warn that action options, mouse and delay will be ignored in
   * non-cli mode */
  if ((application != NULL) && !(fullscreen || window || region))
    g_printerr (ignore_error, "open");
  if ((screenshot_dir != NULL)  && !(fullscreen || window || region ))
    {
      g_printerr (ignore_error, "save");
      screenshot_dir = NULL;
    }
  if (clipboard && !(fullscreen || window || region))
    g_printerr (ignore_error, "clipboard");
  if (delay && !(fullscreen || window || region))
    g_printerr (ignore_error, "delay");
  if (mouse && !(fullscreen || window || region))
    g_printerr (ignore_error, "mouse");

  /* Warn when dependent option is not present */
  if (screenshot_dir == NULL && show_in_folder)
    {
      g_printerr ("The -S option is only used when --save is given."
        "It will be ignored.\n");
      show_in_folder = FALSE;
    }

  /* Just print the version if we are in version mode */
  if (version)
    {
      g_print ("%s\n", PACKAGE_STRING);

      return EXIT_SUCCESS;
    }

  /* Just print the supported image formats if we are in supported formats mode */
  if (supported_formats)
    {
      for (ImageFormat *format = screenshooter_get_image_formats (); format->type != NULL; format++)
        {
          gchar *extensions = g_strjoinv (", ", format->extensions);
          g_print ("[%c] %s (%s)\n", format->supported ? 'x' : ' ', format->name, extensions);
          g_free (extensions);
        }

      return EXIT_SUCCESS;
    }

  sd = g_new0 (ScreenshotData, 1);
  sd->path_is_dir = TRUE;
  sd->app_info = NULL;
  sd->action = 0;
  sd->custom_action_name = g_strdup ("none");
  sd->finalize_callback = cb_finalize;
  sd->finalize_callback_data = NULL;

  /* Read the preferences */
  rc_file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/xfce4-screenshooter", TRUE);
  screenshooter_read_rc_file (rc_file, sd);

  /* Default to no action specified */
  sd->action_specified = FALSE;

  /* If a region cli option is given, take the screenshot accordingly.*/
  if (fullscreen || window || region)
    {
      /* Set the region to be captured */
      sd->region_specified = TRUE;

      if (window)
        sd->region = ACTIVE_WINDOW;
      else if (fullscreen)
        sd->region = FULLSCREEN;
      else
        sd->region = SELECT;

      /* Whether to display the mouse pointer on the screenshot */
      mouse ? (sd->show_mouse = 1) : (sd->show_mouse = 0);

      /* Whether to display the window border on the screenshot */
      no_border ? (sd->show_border = 0) : (sd->show_border = 1);

      sd->delay = delay;

      if (application != NULL)
        {
          sd->app = application;
          sd->action = OPEN;
          sd->action_specified = TRUE;
        }
      else if (screenshot_dir != NULL)
        {
          sd->action = SAVE;
          sd->action_specified = TRUE;
          sd->show_in_folder = show_in_folder;
        }

      if (clipboard)
        {
          /* if no other action was specified, reset the value loaded from prefs */
          if (!sd->action_specified)
            sd->action = NONE;

          sd->action |= CLIPBOARD;
          sd->action_specified = TRUE;
        }

      if (!sd->app)
        sd->app = g_strdup ("none");

      /* If the user gave a directory name, check that it is valid */
      if (screenshot_dir != NULL)
        {
          default_save_dir = g_file_new_for_commandline_arg (screenshot_dir);

          sd->action_specified = TRUE;
          g_free (sd->screenshot_dir);
          sd->screenshot_dir = g_file_get_uri (default_save_dir);

          /* Check if given path is a directory */
          sd->path_is_dir = g_file_test (screenshot_dir, G_FILE_TEST_IS_DIR);

          g_object_unref (default_save_dir);
          g_free (screenshot_dir);
        }

      screenshooter_take_screenshot (sd, TRUE);
      gtk_main ();
    }
  /* Else we show a dialog which allows to set the screenshot options */
  else
    {
      screenshooter_region_dialog_show (sd, FALSE);
    }

  /* Save preferences */
  screenshooter_write_rc_file (rc_file, sd);

  g_free (sd->screenshot_dir);
  g_free (sd->title);
  g_free (sd->app);
  g_free (sd->custom_action_name);
  g_free (sd->custom_action_command);
  g_free (sd->last_user);
  g_free (sd->last_extension);
  g_free (sd);

  TRACE ("Ciao");

  return EXIT_SUCCESS;
}
