/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <xfce-mime-helper-launcher-dialog.h>
#include <xfce-mime-helper-utils.h>
#include <gtk/gtkx.h>



static const gchar *CATEGORY_EXEC_ERRORS[] =
{
  N_("Failed to execute default Web Browser"),
  N_("Failed to execute default Mail Reader"),
  N_("Failed to execute default File Manager"),
  N_("Failed to execute default Terminal Emulator"),
};



static void
error_dialog_dismiss_toggled (GtkToggleButton *button,
                              gboolean        *return_value)
{
  *return_value = gtk_toggle_button_get_active (button);
}



static GtkWidget *
get_helper_error_dialog (XfceMimeHelperCategory  category,
                         GError                 *error,
                         gboolean               *return_value)
{
  GtkWidget *dialog;
  GtkWidget *message_area;
  GtkWidget *check_button;
  GList     *children;

  dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                   "%s.", _(CATEGORY_EXEC_ERRORS[category]));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* Clear excess padding */
  children = gtk_container_get_children (GTK_CONTAINER (dialog));
  gtk_box_set_spacing (GTK_BOX (g_list_nth_data (children, 0)), 0);
  g_list_free (children);

  message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));

  /* Align labels left */
  children = gtk_container_get_children (GTK_CONTAINER (message_area));
  gtk_widget_set_halign (GTK_WIDGET (g_list_nth_data (children, 0)), GTK_ALIGN_START);
  gtk_widget_set_halign (GTK_WIDGET (g_list_nth_data (children, 1)), GTK_ALIGN_START);
  g_list_free (children);

  /* Enable permanently dismissing this error. */
  check_button = gtk_check_button_new_with_mnemonic (_("Do _not show this message again"));
  gtk_box_pack_end (GTK_BOX (message_area), check_button, FALSE, FALSE, 0);
  gtk_widget_set_margin_top (check_button, 12);
  gtk_widget_show (check_button);

  g_signal_connect (G_OBJECT (check_button), "toggled", G_CALLBACK (error_dialog_dismiss_toggled), return_value);

  return dialog;
}



int
main (int argc, char **argv)
{
  XfceMimeHelperCategory  category;
  XfceMimeHelperDatabase *database;
  XfceMimeHelper         *helper;
  GtkWidget              *dialog;
  GError                 *error = NULL;
  gint                    result = EXIT_SUCCESS;
  gchar                  *startup_id;

  gboolean                opt_version = FALSE;
  gchar                  *opt_launch_type = NULL;
  gchar                  *opt_query_type = NULL;

  GOptionContext         *opt_ctx;
  GOptionGroup           *gtk_option_group;
  GOptionEntry            option_entries[] = {
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Print version information and exit"), NULL, },
    { "launch", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_launch_type, N_("Launch the default helper of TYPE with the optional PARAMETER, where TYPE is one of the following values."), N_("TYPE [PARAMETER]"), },
    { "query", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_query_type, N_("Query the default helper of TYPE, where TYPE is one of the following values."), N_("TYPE [PARAMETER]"), },
    { NULL, },
  };

  /* sanity check helper categories */
  g_assert (XFCE_MIME_HELPER_N_CATEGORIES == G_N_ELEMENTS (CATEGORY_EXEC_ERRORS));

#ifdef GETTEXT_PACKAGE
  /* setup i18n support */
  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
#endif

  /* steal the startup id, before gtk tries to grab it */
  startup_id = g_strdup (g_getenv ("DESKTOP_STARTUP_ID"));
  if (startup_id != NULL)
    g_unsetenv ("DESKTOP_STARTUP_ID");

  /* set up options */
  opt_ctx = g_option_context_new (NULL);

  gtk_option_group = gtk_get_option_group (FALSE);
  g_option_context_add_group (opt_ctx, gtk_option_group);

  g_option_context_add_main_entries (opt_ctx, option_entries, NULL);
  g_option_context_set_ignore_unknown_options (opt_ctx, TRUE);
  /* Note to Translators: Do not translate the TYPEs (WebBrowser, MailReader,
   * FileManager and TerminalEmulator), since the xfce-mime-helper utility will
   * not accept localized TYPEs.
   */
  g_option_context_set_description (opt_ctx,
                                    _("The following TYPEs are supported for the --launch and --query commands:\n\n"
                                      "  WebBrowser       - The preferred Web Browser.\n"
                                      "  MailReader       - The preferred Mail Reader.\n"
                                      "  FileManager      - The preferred File Manager.\n"
                                      "  TerminalEmulator - The preferred Terminal Emulator."));

  if (!g_option_context_parse (opt_ctx, &argc, &argv, &error))
    {
      if (G_LIKELY (error)) {
            g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr ("\n");
            g_error_free (error);
        } else
            g_error ("Unable to open display.");

        return EXIT_FAILURE;
    }

  /* initialize Gtk+ */
  gtk_init (&argc, &argv);

  /* restore the startup-id for the child environment */
  if (startup_id)
    g_setenv ("DESKTOP_STARTUP_ID", startup_id, TRUE);

  /* setup default window icon */
  gtk_window_set_default_icon_name ("org.xfce.settings.preferred-applications");

  /* check for the action to perform */
  if (opt_launch_type != NULL)
    {
      /* try to parse the type */
      if (!xfce_mime_helper_category_from_string (opt_launch_type, &category))
        {
          g_warning (_("Invalid helper type \"%s\""), opt_launch_type);
          return EXIT_FAILURE;
        }

      /* determine the default helper for the category */
      database = xfce_mime_helper_database_get ();
      helper = xfce_mime_helper_database_get_default (database, category);

      /* check if we have a valid helper */
      if (G_UNLIKELY (helper == NULL))
        {
          /* ask the user to choose a default helper for category */
          dialog = xfce_mime_helper_launcher_dialog_new (category);
          if (startup_id != NULL)
            gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
          if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
            helper = xfce_mime_helper_database_get_default (database, category);
          else
            xfce_mime_helper_database_clear_default (database, category, NULL);
          gtk_widget_destroy (dialog);

          /* iterate the mainloop until the dialog is fully destroyed */
          while (gtk_events_pending ())
            gtk_main_iteration ();
        }

      /* check if we have a valid helper now */
      if (G_LIKELY (helper != NULL))
        {
          /* try to execute the helper with the given parameter */
          if (!xfce_mime_helper_execute (helper, NULL, (argc > 1) ? argv[1] : NULL, &error))
            {
              if (!xfce_mime_helper_database_get_dismissed (database, category))
                {
                  gboolean dismissed = FALSE;
                  dialog = get_helper_error_dialog (category, error, &dismissed);
                  if (startup_id != NULL)
                    gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
                  gtk_dialog_run (GTK_DIALOG (dialog));
                  gtk_widget_destroy (dialog);

                  if (dismissed)
                    {
                      xfce_mime_helper_database_set_dismissed (database, category, dismissed);
                    }
                }
              g_error_free (error);
              result = EXIT_FAILURE;
            }
          g_object_unref (G_OBJECT (helper));
        }

      /* release our reference on the database */
      g_object_unref(G_OBJECT(database));
    }
  else if (opt_version == TRUE)
    {
      g_print (_("%s (Xfce %s)\n\n"
                 "Copyright (c) 2003-2006\n"
                 "        os-cillation e.K. All rights reserved.\n\n"
                 "Written by Benedikt Meurer <benny@xfce.org>.\n\n"
                 "Built with Gtk+-%d.%d.%d, running Gtk+-%d.%d.%d.\n\n"
                 "Please report bugs to <%s>.\n"),
                 PACKAGE_STRING, xfce_version_string (),
                 GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                 gtk_major_version, gtk_minor_version, gtk_micro_version,
                 PACKAGE_BUGREPORT);
    }
  else if (opt_query_type != NULL)
    {
      /* try to parse the type */
      if (!xfce_mime_helper_category_from_string (opt_query_type, &category))
        {
          g_warning (_("Invalid helper type \"%s\""), opt_query_type);
          return EXIT_FAILURE;
        }

      /* determine the default helper for the category */
      database = xfce_mime_helper_database_get ();
      helper = xfce_mime_helper_database_get_default (database, category);

      if (G_UNLIKELY (helper == NULL))
        {
          g_printerr (_("No helper defined for \"%s\"."), opt_launch_type);
          return EXIT_FAILURE;
        }

      g_print ("%s\n", xfce_mime_helper_get_id (helper));
    }
  else
    {
      result = EXIT_FAILURE;

      g_printerr ("%s", g_option_context_get_help (opt_ctx, FALSE, NULL));
    }

  g_option_context_free (opt_ctx);

  return result;
}
