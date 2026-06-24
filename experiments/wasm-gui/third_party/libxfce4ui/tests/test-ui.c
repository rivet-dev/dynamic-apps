/*-
 * vi:set et ai sts=2 sw=2 cindent:
 *
 * Copyright (c) 2016 Eric Koegel <eric@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <libxfce4ui/libxfce4ui.h>


static const gchar *APP_NAME = "libxfce4ui: Test";

static void
main_window_destroy (GtkWidget *window)
{
  gtk_main_quit ();
}

static void
show_xfce_message_dialog (GtkButton *button,
                          gpointer unused)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size ("../icons/48x48/xfce4-logo.png", 24, 24, NULL);

  xfce_message_dialog (NULL, "Question - Gtk3 version with no deprecated symbols",
                       "dialog-question",
                       "There are unsaved modifications",
                       "The menu has been modified, do you want to save it before quitting?",
                       XFCE_BUTTON_TYPE_MIXED, "document-save", "Save", GTK_RESPONSE_YES,
                       XFCE_BUTTON_TYPE_MIXED, "edit-delete", "Forget modifications", GTK_RESPONSE_APPLY,
                       XFCE_BUTTON_TYPE_PIXBUF, pixbuf, "Quit", GTK_RESPONSE_NO,
                       XFCE_BUTTON_TYPE_MIXED, "process-stop", "Cancel", GTK_RESPONSE_CANCEL,
                       NULL);

  if (pixbuf != NULL)
    {
      g_object_unref (G_OBJECT (pixbuf));
    }
}

static void
show_xfce_dialog_show_help (GtkButton *button,
                            gpointer unused)
{
  xfce_dialog_show_help (NULL, "xfce4-session", "preferences", NULL);
}

static void
show_xfce_dialog_show_help_with_version (GtkButton *button,
                                         gpointer unused)
{
  xfce_dialog_show_help_with_version (NULL, "xfce4-power-manager", "start", NULL, "4.14");
}

static void
remove_auto_online (GtkButton *button,
                    gpointer unused)
{
  XfceRc   *rc;
  gboolean  response;

  response = xfce_dialog_confirm (NULL, "system-run", "Execute",
                                  "Removing this will enable the show help "
                                  "dialogs to appear. Do you want to do this?",
                                  "Remove auto-online in rc file? (Gtk3)");

  if (response == FALSE)
    return;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/help.rc", FALSE);
  if (rc != NULL)
    {
      xfce_rc_write_bool_entry (rc, "auto-online", FALSE);
      xfce_rc_close (rc);
    }
}

static void
show_xfce_dialog_show_info (GtkButton *button,
                            gpointer unused)
{
  xfce_dialog_show_info(NULL, NULL, "%s", "xfburn_transcoder_get_description (priv->trans)");
}

static void
show_xfce_dialog_show_warning (GtkButton *button,
                               gpointer unused)
{
  xfce_dialog_show_warning(NULL, NULL, _("A file named \"%s\" already exists in this directory, the file hasn't been added."), "name");
}

static void
show_xfce_dialog_show_error (GtkButton *button,
                             gpointer unused)
{
  GError *error = NULL;

  g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "This isn't a real error");

  xfce_dialog_show_error (NULL, error, _("Failed to migrate the old panel configuration"));

  g_clear_error (&error);
}

static void
show_xfce_dialog_confirm (GtkButton *button,
                          gpointer unused)
{
  xfce_dialog_confirm (NULL, "system-run", "Execute",
                       "Do you want to start the panel? If you do, make sure "
                       "you save the session on logout, so the panel is "
                       "automatically started the next time you login.",
                       "No running instance of %s was found", G_LOG_DOMAIN);
}

static void
show_xfce_dialog_confirm_close_tabs (GtkButton *button,
                                     gpointer unused)
{
  gboolean checked = TRUE;
  xfce_dialog_confirm_close_tabs (NULL,
                                  3,
                                  TRUE,
                                  &checked);
}

static void
close_window (GtkDialog *dialog,
              gint       response_id,
              gpointer   user_data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_xfce_titled_dialog_new_with_buttons (GtkButton *button,
                                          gpointer unused)
{
  GtkWidget *dialog_gtk3;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  dialog_gtk3 = xfce_titled_dialog_new_with_buttons ("Settings Editor (Gtk3)", NULL,
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                "help-browser", GTK_RESPONSE_HELP,
                                                "window-close-symbolic", GTK_RESPONSE_OK,
                                                NULL);
G_GNUC_END_IGNORE_DEPRECATIONS

  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog_gtk3),
                                   _("Customize settings stored by Xfconf"));

  gtk_window_set_icon_name (GTK_WINDOW (dialog_gtk3), "preferences-system");
  gtk_window_set_type_hint (GTK_WINDOW (dialog_gtk3), GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (dialog_gtk3, "response", G_CALLBACK (close_window), NULL);

  gtk_widget_show_all (dialog_gtk3);
}

static void
show_xfce_titled_dialog_new_with_mixed_buttons (GtkButton *button,
                                                gpointer unused)
{
  GtkWidget *dialog_gtk3;

  dialog_gtk3 = xfce_titled_dialog_new_with_mixed_buttons ("Settings Editor (Gtk3)", NULL,
                                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                                           "help-browser", "Help", GTK_RESPONSE_HELP,
                                                           "weather-snow", "Snow!", GTK_RESPONSE_APPLY,
                                                           "", "no icon", GTK_RESPONSE_CANCEL,
                                                           "window-close-symbolic", "Close", GTK_RESPONSE_OK,
                                                           NULL);

  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog_gtk3),
                                   _("Customize settings stored by Xfconf"));

  gtk_window_set_icon_name (GTK_WINDOW (dialog_gtk3), "preferences-system");
  gtk_window_set_type_hint (GTK_WINDOW (dialog_gtk3), GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (dialog_gtk3, "response", G_CALLBACK (close_window), NULL);

  gtk_widget_show_all (dialog_gtk3);

}

static void
show_xfce_gdk_screen_get_active (GtkButton *button,
                                 gpointer unused)
{
  gint       monitor_num;
  GdkScreen *screen;

  screen = xfce_gdk_screen_get_active (&monitor_num);

  xfce_dialog_show_info (NULL, NULL, "monitor num: %d of %d",
                         monitor_num, gdk_display_get_n_monitors (gdk_screen_get_display (screen)));
}

static void
show_xfce_filename_input_dialog (GtkButton *button,
                                 gpointer   unused)
{
  GtkWidget         *dialog;
  XfceFilenameInput *filename_input;

  dialog = gtk_dialog_new_with_buttons ("Enter file name",
                                        NULL,
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "Cancel", GTK_RESPONSE_CANCEL,
                                        "Submit", GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 250, -1);

  filename_input = g_object_new (XFCE_TYPE_FILENAME_INPUT,
                                 "original-filename", "example.txt", "max-text-length", 30, NULL);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                     GTK_WIDGET (filename_input));
  gtk_widget_show_all (GTK_WIDGET (filename_input));

  g_signal_connect_swapped (filename_input, "text-invalid", G_CALLBACK (xfce_filename_input_desensitise_widget),
                            gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK));
  g_signal_connect_swapped (filename_input, "text-valid", G_CALLBACK (xfce_filename_input_sensitise_widget),
                            gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK));
  xfce_filename_input_check (filename_input); /* this call ensures that the "Submit" button has its
                                                sensitivity set correctly */
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}



static void
create_main_window (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *button;
  GtkWidget *label;

  /* Create main window */
  window = xfce_titled_dialog_new_with_mixed_buttons (APP_NAME, NULL,
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "window-close-symbolic", "_Close", GTK_RESPONSE_CLOSE,
                                                      NULL);


  gtk_window_set_icon_name (GTK_WINDOW (window), "xfce4-logo");
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);

  /* Exit main loop when when the window is closed */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (main_window_destroy), NULL);
  g_signal_connect (G_OBJECT (window), "response", G_CALLBACK (main_window_destroy), NULL);

  /* Create the grid to hold all the stuff */
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (window))), grid);

  /* Create buttons */
  /* Xfce Dialogs */
  label = gtk_label_new ("Xfce Dialogs");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  /* xfce_message_dialog */
  button = gtk_button_new_with_label ("xfce_message_dialog");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_message_dialog), NULL);

  /* xfce_dialog_show_info */
  button = gtk_button_new_with_label ("xfce_dialog_show_info");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 2, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_show_info), NULL);

  /* xfce_dialog_show_warning */
  button = gtk_button_new_with_label ("xfce_dialog_show_warning");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 3, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_show_warning), NULL);

  /* xfce_dialog_show_error */
  button = gtk_button_new_with_label ("xfce_dialog_show_error");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 4, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_show_error), NULL);

  /* xfce_dialog_confirm */
  button = gtk_button_new_with_label ("xfce_dialog_confirm");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 5, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_confirm), NULL);

  /* xfce_dialog_confirm_close_tabs */
  button = gtk_button_new_with_label ("xfce_dialog_confirm_close_tabs");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 6, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_confirm_close_tabs), NULL);

  /* Online Help Dialogs */
  label = gtk_label_new ("Online Help Dialogs");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

  /* xfce_dialog_show_help */
  button = gtk_button_new_with_label ("xfce_dialog_show_help");
  gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_show_help), NULL);

  /* xfce_dialog_show_help_with_version */
  button = gtk_button_new_with_label ("xfce_dialog_show_help_with_version");
  gtk_grid_attach (GTK_GRID (grid), button, 1, 2, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_dialog_show_help_with_version), NULL);

  /* remove auto-online check from rc file */
  button = gtk_button_new_with_label ("remove auto-online check from rc file");
  gtk_grid_attach (GTK_GRID (grid), button, 1, 3, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (remove_auto_online), NULL);

  /* Xfce Titled Dialogs */
  label = gtk_label_new ("Xfce Titled Dialog");
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);

  /* xfce_titled_dialog_new_with_buttons */
  button = gtk_button_new_with_label ("xfce_titled_dialog_new_with_buttons");
  gtk_grid_attach (GTK_GRID (grid), button, 2, 1, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_titled_dialog_new_with_buttons), NULL);

  /* xfce_titled_dialog_new_with_mixed_buttons */
  button = gtk_button_new_with_label ("xfce_titled_dialog_new_with_mixed_buttons");
  gtk_grid_attach (GTK_GRID (grid), button, 2, 2, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_titled_dialog_new_with_mixed_buttons), NULL);

  /* Other Dialogs */
  label = gtk_label_new ("Other Dialogs and Widgets");
  gtk_grid_attach (GTK_GRID (grid), label, 3, 0, 1, 1);

  /* xfce_gdk_screen_get_active */
  button = gtk_button_new_with_label ("xfce_gdk_screen_get_active");
  gtk_grid_attach (GTK_GRID (grid), button, 3, 1, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_gdk_screen_get_active), NULL);

  /* xfce_filename_input */
  button = gtk_button_new_with_label ("XfceFilenameInput");
  gtk_grid_attach (GTK_GRID (grid), button, 3, 2, 1, 1);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_xfce_filename_input_dialog), NULL);

  gtk_widget_show_all (window);
}



gint
main (gint    argc,
      gchar **argv)
{
  gint    exit_code = EXIT_SUCCESS;

  /* Initialize GTK+ */
  gtk_init (&argc, &argv);

  /* create the main window */
  create_main_window ();

  /* Enter main loop */
  gtk_main ();

  return exit_code;
}
