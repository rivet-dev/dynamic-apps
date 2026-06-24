/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
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
#include <libxfce4ui/libxfce4ui.h>

#include <exo/exo.h>
#include <exo-desktop-item-edit/exo-die-editor.h>
#include <exo-desktop-item-edit/exo-die-utils.h>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif

/* string with fallback spoort */
#define STR_FB(string, fallback) ((string != NULL && *string != '\0') ? string : fallback)



/* --- constants --- */
static const gchar *CREATE_TITLES[] = { N_ ("Create Launcher"), N_ ("Create Link"), N_("Create Directory") };
static const gchar *EDIT_TITLES[] = { N_ ("Edit Launcher"), N_ ("Edit Link"), N_("Edit Directory") };
static const gchar *ICON_NAMES[] = { "applications-other", "applications-internet", "folder" };



/* --- globals --- */
static gboolean opt_create_new = FALSE;
static gboolean opt_version = FALSE;
static gchar   *opt_type = NULL;
static gchar   *opt_name = NULL;
static gchar   *opt_comment = NULL;
static gchar   *opt_command = NULL;
static gchar   *opt_url = NULL;
static gchar   *opt_icon = NULL;
static gint64   opt_xid = 0;



static void exo_die_error (const gchar *format, ...) G_GNUC_PRINTF (1, 2);



/* --- command line options --- */
static GOptionEntry option_entries[] =
{
  { "create-new", 'c', 0, G_OPTION_ARG_NONE, &opt_create_new, N_ ("Create a new desktop file in the given directory"), NULL, },
  { "type", 't', 0, G_OPTION_ARG_STRING, &opt_type, N_ ("Type of desktop file to create (Application or Link)"), NULL, },
  { "name", 0, 0, G_OPTION_ARG_STRING, &opt_name, N_ ("Preset name when creating a desktop file"), NULL, },
  { "comment", 0, 0, G_OPTION_ARG_STRING, &opt_comment, N_ ("Preset comment when creating a desktop file"), NULL, },
  { "command", 0, 0, G_OPTION_ARG_STRING, &opt_command, N_ ("Preset command when creating a launcher"), NULL, },
  { "url", 0, 0, G_OPTION_ARG_STRING, &opt_url, N_ ("Preset URL when creating a link"), NULL, },
  { "icon", 0, 0, G_OPTION_ARG_STRING, &opt_icon, N_ ("Preset icon when creating a desktop file"), NULL, },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Print version information and exit"), NULL, },
  { "xid", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT64, &opt_xid, NULL, NULL, },
  { NULL, },
};



static void
exo_die_error (const gchar *format,
               ...)
{
  gchar   *buffer;
  va_list  args;

  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);

  g_printerr ("%s: %s\n", G_LOG_DOMAIN, buffer);

  g_free (buffer);
}



static void
exo_die_response_cb (GtkDialog *dialog,
                     gint       response_id,
                     gpointer   user_data)
{
  if (response_id == GTK_RESPONSE_HELP)
    xfce_dialog_show_help (GTK_WINDOW (dialog), "exo", "desktop-item-edit", NULL);
}



int
main (int argc, char **argv)
{
  ExoDieEditorMode mode;
  GEnumClass      *enum_klass;
  GEnumValue      *enum_value;
#if defined(GDK_WINDOWING_X11)
  GdkWindow       *xwindow;
#endif
  GtkWidget       *chooser;
  GtkWidget       *message;
  GtkWidget       *button;
  GtkWidget       *image;
  GtkWidget       *dialog;
  GtkWidget       *editor;
  GKeyFile        *key_file;
  GError          *error = NULL;
  gchar           *base_name;
  gchar           *value;
  gchar           *s;
  gint             response;
  gint             result = EXIT_SUCCESS;
  GFile           *gfile, *gfile_parent;
  gchar           *contents;
  gsize            length = 0;
  gboolean         res;
  GFileType        file_type;
  GFile           *gfile_local;
  gchar           *relpath;
  gchar           *path;
  gchar          **dirs;
  guint            i;
  const gchar     *mode_dir;
  gint             ox, oy, ow, oh;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

#ifdef G_ENABLE_DEBUG
  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* initialize Gtk+ */
  if (!gtk_init_with_args (&argc, &argv, _("[FILE|FOLDER]"), option_entries, GETTEXT_PACKAGE, &error))
    {
      /* determine the error message */
      if (G_UNLIKELY (error != NULL))
        {
          /* use the supplied error message */
          exo_die_error ("%s", error->message);
          g_error_free (error);
        }
      else
        {
          /* no error message, the GUI initialization failed */
          exo_die_error ("%s %s", _("Failed to open display"),
                      STR_FB (gdk_get_display_arg_name (), ""));
        }

      /* and fail */
      return EXIT_FAILURE;
    }

  /* check if we should print version information */
  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s\n\n", g_get_prgname (), PACKAGE_VERSION);
      g_print (_("Copyright (c) %s\n"
                 "        os-cillation e.K. All rights reserved.\n\n"
                 "Written by Benedikt Meurer <benny@xfce.org>.\n\n"),
               "2005-2007");
      g_print (_("%s comes with ABSOLUTELY NO WARRANTY,\n"
                 "You may redistribute copies of %s under the terms of\n"
                 "the GNU Lesser General Public License which can be found in the\n"
                 "%s source package.\n\n"), g_get_prgname (), g_get_prgname (), PACKAGE_TARNAME);
      g_print (_("Please report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
      return EXIT_SUCCESS;
    }

  /* verify that a file/folder is specified */
  if (G_UNLIKELY (argc != 2))
    {
      exo_die_error (_("No file/folder specified"));
      return EXIT_FAILURE;
    }

  /* create a file from the arguments */
  gfile = g_file_new_for_commandline_arg (argv[1]);
  if (!opt_create_new && opt_type == NULL
      && !g_file_query_exists (gfile, NULL))
    {
      /* to help alacarte and some users a bit, we try to be smart here */
      if (g_str_has_suffix (argv[1], ".desktop"))
        opt_type = "Application";
      else if (g_str_has_suffix (argv[1], ".directory"))
        opt_type = "Directory";

      /* go to create mode if we found a valid suffix */
      opt_create_new = (opt_type != NULL);
    }

  /* allocate a key file */
  key_file = g_key_file_new ();

  /* create new key file if --create-new was specified */
  if (opt_create_new)
    {
      /* generic stuff */
      g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                            G_KEY_FILE_DESKTOP_KEY_VERSION, "1.0");
      g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                            G_KEY_FILE_DESKTOP_KEY_TYPE, STR_FB (opt_type, "Application"));
      g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                            G_KEY_FILE_DESKTOP_KEY_NAME, STR_FB (opt_name, ""));
      g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                            G_KEY_FILE_DESKTOP_KEY_COMMENT, STR_FB (opt_comment, ""));

      /* type specific stuff */
      if (g_strcmp0 (opt_type, G_KEY_FILE_DESKTOP_TYPE_LINK) == 0)
        {
          g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_ICON, STR_FB (opt_icon, "user-bookmarks"));
          g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_URL, STR_FB (opt_url, ""));
        }
      else if (g_strcmp0 (opt_type, G_KEY_FILE_DESKTOP_TYPE_DIRECTORY) == 0)
        {
          g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_ICON, STR_FB (opt_icon, ""));
        }
      else
        {
          g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_EXEC, STR_FB (opt_command, ""));
          g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_ICON, STR_FB (opt_icon, ""));
        }
    }
  else
    {
      /* try to load the entire file into memory */
      res = g_file_load_contents (gfile, NULL, &contents, &length, NULL, &error);
      if (G_UNLIKELY (!res || length == 0))
        {
          /* we cannot open the file */
          if (G_LIKELY (error != NULL))
            {
              exo_die_error (_("Failed to load contents from \"%s\": %s"), argv[1], error->message);
              g_error_free (error);
            }
          else
            {
              exo_die_error (_("The file \"%s\" contains no data"), argv[1]);
            }

          return EXIT_FAILURE;
        }

      /* load the data into the key file */
      res = g_key_file_load_from_data (key_file, contents, length, G_KEY_FILE_KEEP_COMMENTS
                                       | G_KEY_FILE_KEEP_TRANSLATIONS, &error);
      g_free (contents);
      if (G_UNLIKELY (!res))
        {
          /* failed to parse the file */
          exo_die_error (_("Failed to parse contents of \"%s\": %s"), argv[1], error->message);
          g_error_free (error);
          return EXIT_FAILURE;
        }
    }

  /* determine the type of the desktop file */
  value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_TYPE, NULL);
  if (G_UNLIKELY (value == NULL))
    {
      /* we cannot continue without a type */
      exo_die_error (_("File \"%s\" has no type key"), argv[1]);
      return EXIT_FAILURE;
    }

  /* verify that we support the type */
  enum_klass = g_type_class_ref (EXO_DIE_TYPE_EDITOR_MODE);
  enum_value = g_enum_get_value_by_nick (enum_klass, value);
  if (G_UNLIKELY (enum_value == NULL))
    {
      /* tell the user that we don't support the type */
      exo_die_error (_("Unsupported desktop file type \"%s\""), value);
      return EXIT_FAILURE;
    }
  g_free (value);
  mode = enum_value->value;
  g_type_class_unref (enum_klass);

  /* allocate the dialog */
  dialog = xfce_titled_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), opt_create_new ? _(CREATE_TITLES[mode]) : _(EDIT_TITLES[mode]));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 375);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), ICON_NAMES[mode]);
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (exo_die_response_cb), dialog);

  xfce_titled_dialog_create_action_area (XFCE_TITLED_DIALOG (dialog));

  /* add the "Help" button */
  button = gtk_button_new_with_mnemonic (_("_Help"));
  image = gtk_image_new_from_icon_name ("help-browser", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_HELP);

  /* add the "Cancel" button */
  button = gtk_button_new_with_mnemonic (_("_Cancel"));
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);

  /* add the "Create"/"Save" button (as default) */
  button = gtk_button_new_with_mnemonic (opt_create_new ? _("C_reate") : _("_Save"));
  image = gtk_image_new_from_icon_name (opt_create_new ? ("document-new") : ("document-save"), GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);
  xfce_titled_dialog_set_default_response (XFCE_TITLED_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  /* allocate the editor widget */
  editor = exo_die_editor_new ();
  exo_die_editor_set_mode (EXO_DIE_EDITOR (editor), mode);
  gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
  g_object_bind_property (G_OBJECT (editor), "complete", G_OBJECT (button), "sensitive", G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), editor, TRUE, TRUE, 0);
  gtk_widget_show (editor);

  /* setup the name */
  value = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
  exo_die_editor_set_name (EXO_DIE_EDITOR (editor), (value != NULL) ? value : "");
  g_free (value);

  /* setup the comment */
  value = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL, NULL);
  exo_die_editor_set_comment (EXO_DIE_EDITOR (editor), (value != NULL) ? value : "");
  g_free (value);

  /* setup the icon (automatically fixing broken icons) */
  value = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_ICON, NULL, NULL);
  if (value != NULL && !g_path_is_absolute (value)
      && !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), value))
    {
      /* check if this is an invalid icon declaration */
      s = strrchr (value, '.');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';
    }
  exo_die_editor_set_icon (EXO_DIE_EDITOR (editor), (value != NULL) ? value : "");
  g_free (value);

  /* mode specific stuff */
  switch (mode)
    {
    case EXO_DIE_EDITOR_MODE_APPLICATION:
      /* setup the command but ignore escape sequences */
      value = g_key_file_get_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
      exo_die_editor_set_command (EXO_DIE_EDITOR (editor), (value != NULL) ? value : "");
      g_free (value);

      /* setup the working directory */
      value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_PATH, NULL);
      exo_die_editor_set_path (EXO_DIE_EDITOR (editor), (value != NULL) ? value : "");
      g_free (value);


      /* setup launcher options */
      exo_die_editor_set_snotify (EXO_DIE_EDITOR (editor),
          g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                  G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, NULL));
      exo_die_editor_set_terminal (EXO_DIE_EDITOR (editor),
          g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                  G_KEY_FILE_DESKTOP_KEY_TERMINAL, NULL));
      break;

    case EXO_DIE_EDITOR_MODE_LINK:
      /* setup the URL */
      value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_URL, NULL);
      exo_die_editor_set_url (EXO_DIE_EDITOR (editor), (value != NULL) ? value : "");
      g_free (value);
      break;

    case EXO_DIE_EDITOR_MODE_DIRECTORY:
      /* nothing special */
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  /* check if a parent window id was specified */
#if defined(GDK_WINDOWING_X11)
  if (G_UNLIKELY (opt_xid != 0))
    {
      /* try to determine the window for the id */
      xwindow = gdk_x11_window_foreign_new_for_display (gdk_display_get_default(), opt_xid);
      if (G_LIKELY (xwindow != NULL))
        {
          GtkAllocation allocation;

          /* realize the dialog first... */
          gtk_widget_realize (dialog);

          /* ...and set the "transient for" relation */
          gdk_window_set_transient_for (gtk_widget_get_window (dialog), xwindow);
          gtk_window_set_screen (GTK_WINDOW (dialog),
              gdk_window_get_screen (GDK_WINDOW (xwindow)));

          /* center on parent */
          gdk_window_get_root_origin (xwindow, &ox, &oy);
          gdk_window_get_geometry (xwindow, NULL, NULL, &ow, &oh);

          gtk_widget_get_allocation (dialog, &allocation);
          ox += (ow - allocation.width) / 2;
          oy += (oh - allocation.height) / 2;

          gtk_window_move (GTK_WINDOW (dialog), MAX (ox, 0), MAX (oy, 0));
        }
    }
#endif

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    {
      gboolean trusted_launcher = opt_create_new; // User-created launchers are launchable by default

      /* save common values (localized if possible) */
      exo_die_g_key_file_set_locale_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                           G_KEY_FILE_DESKTOP_KEY_NAME,
                                           exo_die_editor_get_name (EXO_DIE_EDITOR (editor)));
      exo_die_g_key_file_set_locale_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                           G_KEY_FILE_DESKTOP_KEY_ICON,
                                           exo_die_editor_get_icon (EXO_DIE_EDITOR (editor)));
      exo_die_g_key_file_set_locale_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                           G_KEY_FILE_DESKTOP_KEY_COMMENT,
                                           exo_die_editor_get_comment (EXO_DIE_EDITOR (editor)));

      /* save mode specific stuff */
      switch (mode)
        {
        case EXO_DIE_EDITOR_MODE_APPLICATION:
          /* save the new command but do not escape special characters */
          g_key_file_set_value (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_EXEC,
                                 exo_die_editor_get_command (EXO_DIE_EDITOR (editor)));

          /* save the new path */
          g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_PATH,
                                 exo_die_editor_get_path (EXO_DIE_EDITOR (editor)));

          /* save the new launcher options */
          g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                 exo_die_editor_get_terminal (EXO_DIE_EDITOR (editor)));
          g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY,
                                 exo_die_editor_get_snotify (EXO_DIE_EDITOR (editor)));
          break;

        case EXO_DIE_EDITOR_MODE_LINK:
          /* save the new URL */
          g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_URL,
                                 exo_die_editor_get_url (EXO_DIE_EDITOR (editor)));
          break;

        case EXO_DIE_EDITOR_MODE_DIRECTORY:
          /* nothing special */
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      /* try to save the file */
      if (!exo_die_g_key_file_save (key_file, opt_create_new, trusted_launcher, gfile, mode, &error))
        {
          if (opt_create_new)
            {
              /* reset the error */
              g_clear_error (&error);

              /* create failed, ask the user to specify a file name */
              chooser = gtk_file_chooser_dialog_new (_("Choose filename"),
                                                     GTK_WINDOW (dialog),
                                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                                     _("_Cancel"), GTK_RESPONSE_CANCEL,
                                                     _("_Save"), GTK_RESPONSE_ACCEPT,
                                                     NULL);
              gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
              gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);

              file_type = g_file_query_file_type (gfile, G_FILE_QUERY_INFO_NONE, NULL);

              /* if base is a folder, enter the folder */
              if (file_type == G_FILE_TYPE_DIRECTORY)
                {
                  gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (chooser), gfile, NULL);
                  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), "new-file.desktop");
                }
              else if (file_type == G_FILE_TYPE_REGULAR)
                {
                  gfile_parent = g_file_get_parent (gfile);
                  if (G_LIKELY (gfile_parent != NULL))
                    {
                      gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (chooser), gfile_parent, NULL);
                      g_object_unref (G_OBJECT (gfile_parent));
                    }

                  base_name = g_file_get_basename (gfile);
                  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), base_name);
                  g_free (base_name);
                }

              /* run the chooser */
              response = gtk_dialog_run (GTK_DIALOG (chooser));
              if (G_LIKELY (response == GTK_RESPONSE_ACCEPT))
                {
                  /* release the previous file name */
                  if (G_LIKELY (gfile != NULL))
                    g_object_unref (G_OBJECT (gfile));

                  /* try again to save to the new file */
                  gfile = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));
                  exo_die_g_key_file_save (key_file, FALSE, trusted_launcher, gfile, mode, &error);
                }

              /* destroy the chooser */
              gtk_widget_destroy (chooser);
            }
          else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)
                   || g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_ACCES))
            {
              if (mode == EXO_DIE_EDITOR_MODE_DIRECTORY)
                mode_dir = "desktop-directories/";
              else
                mode_dir = "applications/";

              /* check if the file is in one of the applications directories
               * and get the relative path */
              dirs = xfce_resource_lookup_all (XFCE_RESOURCE_DATA, mode_dir);
              base_name = NULL;
              for (base_name = NULL, i = 0; !base_name && dirs[i] != NULL; i++)
                {
                  gfile_parent = g_file_new_for_path (dirs[i]);
                  base_name = g_file_get_relative_path (gfile_parent, gfile);
                  g_object_unref (G_OBJECT (gfile_parent));
                }
              g_strfreev (dirs);

              /* file was found in an applications directory, write a new file in
               * the users' local applications directory */
              if (base_name != NULL)
                {
                  /* get the new file location */
                  relpath = g_build_filename (mode_dir, base_name, NULL);
                  path = xfce_resource_save_location (XFCE_RESOURCE_DATA, relpath, TRUE);
                  g_free (relpath);

                  if (G_LIKELY (path != NULL))
                    {
                      /* silently notify the user we're going to write to a new location */
                      exo_die_error ("\"%s\" is not writeable, saving to \"%s\" instead.",
                                     argv[1], path);

                      /* reset the error */
                      g_clear_error (&error);

                      /* try another save */
                      gfile_local = g_file_new_for_path (path);
                      exo_die_g_key_file_save (key_file, FALSE, trusted_launcher, gfile_local, mode, &error);
                      g_object_unref (G_OBJECT (gfile_local));
                    }

                  g_free (path);
                  g_free (base_name);
                }
            }
        }

      /* check if we failed to save/create */
      if (G_UNLIKELY (response == GTK_RESPONSE_ACCEPT && error != NULL))
        {
          /* display an error message to the user */
          s = g_file_get_uri (gfile);
          message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            opt_create_new ? _("Failed to create \"%s\".") : _("Failed to save \"%s\"."),
                                            s);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_free (s);

          /* jep, we failed */
          result = EXIT_FAILURE;
        }

      /* reset the error */
      g_clear_error (&error);
    }

  /* has to be called manually because we are not using GtkApplication */
  gtk_clipboard_store (gtk_widget_get_clipboard (dialog, GDK_SELECTION_CLIPBOARD));

  /* destroy the editor dialog */
  gtk_widget_destroy (dialog);

  /* cleanup */
  g_key_file_free (key_file);
  g_object_unref (G_OBJECT (gfile));

  return result;
}
