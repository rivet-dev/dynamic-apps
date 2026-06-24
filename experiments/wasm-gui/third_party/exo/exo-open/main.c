/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#include <glib/gstdio.h>
#include <glib.h>
#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif
#include <exo/exo.h>



#define USERCHARS       "-[:alnum:]"
#define USERCHARS_CLASS "[" USERCHARS "]"
#define PASSCHARS_CLASS "[-[:alnum:]\\Q,?;.:/!%$^*&~\"#'\\E]"
#define HOSTCHARS_CLASS "[-[:alnum:]]"
#define HOST            HOSTCHARS_CLASS "+(\\." HOSTCHARS_CLASS "+)*"
#define PORT            "(?:\\:[[:digit:]]{1,5})?"
#define PATHCHARS_CLASS "[-[:alnum:]\\Q_$.+!*,;@&=?/:~#'%\\E]"
#define PATHTERM_CLASS  "[^\\Q]'.}>) \t\r\n,\"\\E]"
#define USERPASS        USERCHARS_CLASS "+(?:" PASSCHARS_CLASS "+)?"
#define URLPATH         "(?:(/"PATHCHARS_CLASS"+(?:[(]"PATHCHARS_CLASS"*[)])*"PATHCHARS_CLASS"*)*"PATHTERM_CLASS")?"

#define MATCH_PATTERN_HTTP      "(?:www|ftp)" HOSTCHARS_CLASS "*\\." HOST PORT URLPATH
#define MATCH_PATTERN_EMAIL     "(?:mailto:)?" USERCHARS_CLASS "[" USERCHARS ".]*\\@" HOSTCHARS_CLASS "+\\." HOST



/**
 * For testing this code, the following commands should work:
 *
 * exo-open --launch WebBrowser https://xfce.org (bug #5461).
 * exo-open --launch WebBrowser https://xfce.org gitlab.xfce.org 'http://www.google.com/search?q=what is a space' 'https://wiki.xfce.org'
 * exo-open https://xfce.org
 * exo-open --launch TerminalEmulator ./script.sh 'something with a space' 'nospace' (bug #5132).
 * exo-open --launch TerminalEmulator ssh -l username some.host.com
 *
 * xfterm4 -e ssh -l username some.host.com (bug #5301, this generates line below)
 *   exo-open --launch TerminalEmulator 'ssh -l username some.host.com'
 *
 * exo-open /some/path/to/a/file.desktop
 * exo-open somerelativefile
 **/



static gboolean  opt_help = FALSE;
static gboolean  opt_version = FALSE;
static gchar    *opt_launch = NULL;
static gchar    *opt_working_directory = NULL;

static gchar    *startup_id = NULL;

static GOptionEntry entries[] =
{
  { "help", '?', 0, G_OPTION_ARG_NONE, &opt_help, NULL, NULL, },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, NULL, NULL, },
  { "launch", 0, 0, G_OPTION_ARG_STRING, &opt_launch, NULL, NULL, },
  { "working-directory", 0, 0, G_OPTION_ARG_FILENAME, &opt_working_directory, NULL, NULL, },
  { NULL, },
};

typedef struct _KnownSchemes KnownSchemes;
struct _KnownSchemes
{
  const gchar *pattern;
  const gchar *category;
};

static KnownSchemes known_schemes[] =
{
  { "^(https?|gopher)$", "WebBrowser" },
  { "^mailto$",          "MailReader" },
};

struct launch_uri_data
{
  gboolean is_done;
  gboolean success;
  GError  *error;
};



/* Prototypes */
static void     usage                        (void);
static gboolean exo_open_launch_desktop_file (const gchar  *arg);
static gchar   *exo_open_get_path            (const gchar  *string);
static gchar   *exo_open_find_scheme         (const gchar  *string);
static gboolean exo_open_launch_category     (const gchar  *category,
                                              const gchar  *parameters);
static gboolean exo_open_uri_known_category  (const gchar  *uri,
                                              const gchar  *scheme,
                                              gboolean     *succeed);
static gboolean exo_open_uri                 (const gchar  *uri,
                                              GError      **error);
static gboolean exo_g_app_info_launch_uri    (GAppInfo          *appinfo,
                                              const gchar       *uri,
                                              GAppLaunchContext *context,
                                              GError           **error);
static void     launch_uri_callback          (GObject           *src,
                                              GAsyncResult      *res,
                                              gpointer           user_data);



static void
launch_uri_callback (GObject      *src,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  struct launch_uri_data *data = user_data;

  data->success = g_app_info_launch_uris_finish (G_APP_INFO (src),
                                                 res,
                                                 &data->error);
  data->is_done = TRUE;
  return;
}



/**
 * exo_g_app_info_launch_uri:
 * @appinfo: a #GAppInfo.
 * @uri: an utf-8 encoded uri to pass as an argument.
 * @context: a #GAppLaunchContext or NULL.
 * @error: a #GError.
 *
 * D-BUS friendly version of g_app_info_launch_uris.
 * D-BUS-activated applications don't have to be started
 * if the caller is terminated early. This function
 * properly waits until the application is started.
 *
 * See also: https://gitlab.gnome.org/GNOME/glib/-/commit/051c6ba4e7111b04ab417403730b82de02a1c0d8
 *
 * Returns: %TRUE on success, %FALSE on error.
 **/
static gboolean
exo_g_app_info_launch_uri  (GAppInfo          *appinfo,
                            const gchar       *uri,
                            GAppLaunchContext *context,
                            GError           **error)
{
  GList    fake_list;

  struct launch_uri_data data;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  fake_list.data = (gpointer) uri;
  fake_list.prev = fake_list.next = NULL;

  data.is_done = FALSE;
  data.success = FALSE;
  data.error   = NULL;

  g_app_info_launch_uris_async (appinfo,
                                &fake_list,
                                NULL,
                                NULL,
                                launch_uri_callback,
                                (gpointer) &data);

  while (!data.is_done)
    g_main_context_iteration (NULL, TRUE);

  if (error == NULL)
    g_clear_error (&data.error);
  else
    *error = data.error;

  return data.success;
}



static void
usage (void)
{
  g_print ("%s\n", _("Usage: exo-open [URLs...]"));
  g_print ("%s\n", _("       exo-open --launch TYPE [PARAMETERs...]"));
  g_print ("\n");
  g_print ("%s\n", _("  -?, --help                          Print this help message and exit"));
  g_print ("%s\n", _("  -V, --version                       Print version information and exit"));
  g_print ("\n");
  g_print ("%s\n", _("  --launch TYPE [PARAMETERs...]       Launch the preferred application of\n"
                     "                                      TYPE with the optional PARAMETERs, where\n"
                     "                                      TYPE is one of the following values."));
  g_print ("\n");
  g_print ("%s\n", _("  --working-directory DIRECTORY       Default working directory for applications\n"
                     "                                      when using the --launch option."));
  g_print ("\n");
  g_print ("%s\n", _("The following TYPEs are supported for the --launch command:"));
  g_print ("\n");

  /* Note to Translators: Do not translate the TYPEs (WebBrowser, MailReader, TerminalEmulator),
   * since the xfce4-mime-helper utility will not accept localized TYPEs.
   */
  g_print ("%s\n", _("  WebBrowser       - The preferred Web Browser.\n"
                     "  MailReader       - The preferred Mail Reader.\n"
                     "  FileManager      - The preferred File Manager.\n"
                     "  TerminalEmulator - The preferred Terminal Emulator."));
  g_print ("\n");
  g_print ("%s\n", _("If you don't specify the --launch option, exo-open will open all specified\n"
                     "URLs with their preferred URL handlers. Else, if you specify the --launch\n"
                     "option, you can select which preferred application you want to run, and\n"
                     "pass additional parameters to the application (i.e. for TerminalEmulator\n"
                     "you can pass the command line that should be run in the terminal)."));
  g_print ("\n");
}



static gboolean
exo_open_launch_desktop_file (const gchar *arg)
{
#ifdef HAVE_GIO_UNIX
  GFile           *gfile;
  GFile           *parent;
  gchar           *type;
  gchar           *link;
  gchar           *abs_path;
  gchar           *file_dir;
  gchar           *contents;
  gsize            length;
  gboolean         result;
  GKeyFile        *key_file;
  GDesktopAppInfo *appinfo;

  /* try to open a file from the arguments */
  gfile = g_file_new_for_commandline_arg (arg);
  if (G_UNLIKELY (gfile == NULL))
    return FALSE;

  /* Only execute local .desktop files to prevent execution of malicious launchers from  foreign locations */
  if (g_file_has_uri_scheme (gfile, "file") == FALSE)
    {
      char *uri = g_file_get_uri (gfile);
      g_warning ("Execution of remote .desktop file '%s' was skipped due to security concerns.", uri);
      g_object_unref (gfile);
      g_free (uri);
      return FALSE;
    }

  /* load the contents of the file */
  result = g_file_load_contents (gfile, NULL, &contents, &length, NULL, NULL);
  if (G_UNLIKELY (!result || length == 0))
    return FALSE;

  /* create the key file */
  key_file = g_key_file_new ();
  result = g_key_file_load_from_data (key_file, contents, length, G_KEY_FILE_NONE, NULL);
  g_free (contents);
  if (G_UNLIKELY (!result))
    {
      g_key_file_free (key_file);
      return FALSE;
    }

  /* try to launch "Link" type .desktop file */
  type = g_key_file_get_value (key_file,
                               G_KEY_FILE_DESKTOP_GROUP,
                               G_KEY_FILE_DESKTOP_KEY_TYPE,
                               NULL);
  if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_LINK) == 0)
    {
      link = g_key_file_get_value (key_file,
                                   G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_URL,
                                   NULL);
      if (!g_uri_is_valid (link, G_URI_FLAGS_NONE, NULL))
        {
          parent = g_file_get_parent (gfile);
          file_dir = g_file_get_path (parent);
          g_object_unref (parent);
          abs_path = g_build_filename (file_dir, link, NULL);
          g_free (file_dir);
          g_free (link);
          link = exo_open_find_scheme (abs_path);
          g_free (abs_path);
        }
      result = exo_open_uri (link, NULL);
      g_free (link);
    }
  else
    result = FALSE;
  g_free (type);
  g_object_unref (G_OBJECT (gfile));

  if (result)
    {
      g_key_file_free (key_file);
      return TRUE;
    }

  /* create the appinfo */
  appinfo = g_desktop_app_info_new_from_keyfile (key_file);
  g_key_file_free (key_file);
  if (G_UNLIKELY (appinfo == NULL))
    return FALSE;

  /* try to launch a (non-hidden) desktop file */
  if (G_LIKELY (!g_desktop_app_info_get_is_hidden (appinfo)))
    result = g_app_info_launch (G_APP_INFO (appinfo), NULL, NULL, NULL);
  else
    result = FALSE;

  g_object_unref (G_OBJECT (appinfo));

#ifndef NDEBUG
  g_debug ("launching desktop file %s", result ? "succeeded" : "failed");
#endif

  return result;
#else /* !HAVE_GIO_UNIX */
  g_critical (_("Launching desktop files is not supported when %s is compiled "
                "without GIO-Unix features."), g_get_prgname ());

  return FALSE;
#endif
}

static gchar *
exo_open_get_path (const gchar *string)
{
  gchar *escaped;
  gchar *uri;
  escaped = g_uri_escape_string (string, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
  uri = g_strconcat ("file://", escaped, NULL);
  g_free (escaped);
  return uri;
}

static gchar *
exo_open_find_scheme (const gchar *string)
{
  gchar *current_dir;
  gchar *uri;
  gchar *path;

  /* is an absolute path, return file uri */
  if (g_path_is_absolute (string))
    return exo_open_get_path (string);

  /* treat it like a relative path */
  current_dir = g_get_current_dir ();
  path = g_build_filename (current_dir, string, NULL);
  g_free (current_dir);

  /* verify that a file of the given name exists */
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
       uri = exo_open_get_path (path);
       g_free (path);
       return uri;
    }
  g_free (path);

  /* regular expression to check if it looks like an email address */
  if (g_regex_match_simple (MATCH_PATTERN_EMAIL, string, G_REGEX_CASELESS, 0))
    return g_strconcat ("mailto:", string, NULL);

  /* regular expression to check if it looks like an url, we don't need to check
   * for a complete url (http://) because this is already matched by the
   * g_uri_is_valid () test */
  if (g_regex_match_simple (MATCH_PATTERN_HTTP, string, G_REGEX_CASELESS, 0))
    return g_strconcat ("http://", string, NULL);

  return NULL;
}



static gboolean
exo_open_launch_category (const gchar *category,
                          const gchar *parameters)
{
  GtkWidget *dialog;
  GError    *error = NULL;

#ifndef NDEBUG
  g_debug ("category='%s', wd='%s', parameters='%s'", category, opt_working_directory, parameters);
#endif

  /* run the preferred application */
  if (!exo_execute_preferred_application (category, parameters, opt_working_directory, NULL, &error))
    {
      /* display an error dialog */
      dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                       _("Failed to launch preferred application for category \"%s\"."), category);
      if (startup_id != NULL)
        gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);

      return FALSE;
    }

  return TRUE;
}


static gboolean
exo_open_uri_known_category (const gchar  *uri,
                             const gchar  *scheme,
                             gboolean     *succeed)
{
  guint        i;
  const gchar *category = NULL;

  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (scheme != NULL, FALSE);

  /* check if the scheme matches a known preferred application type */
  for (i = 0; category == NULL && i < G_N_ELEMENTS (known_schemes); i++)
    {
      if (g_regex_match_simple (known_schemes[i].pattern, scheme, G_REGEX_CASELESS, 0))
        {
          /* launch the preferred application */
          *succeed = exo_open_launch_category (known_schemes[i].category, uri);

          /* we always return, because we found a matching scheme */
          return TRUE;
        }
    }

  return FALSE;
}



static gboolean
exo_open_uri (const gchar  *uri,
              GError      **error)
{
  GFile               *file;
  gchar               *scheme;
  GFileInfo           *file_info;
  gboolean             succeed = FALSE;
  gboolean             retval = FALSE;
  GFileType            file_type;
  const gchar         *content_type;
  GAppInfo            *app_info;
  gchar               *path;
  const gchar         *executable;
  const gchar * const *schemes;
  GError              *err = NULL;
  guint                i;

  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#ifndef NDEBUG
  schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
  scheme = g_strjoinv (", ", (gchar **) schemes);
  g_debug ("vfs supported schemes: %s", scheme);
  g_free (scheme);
#endif

  file = g_file_new_for_uri (uri);
  scheme = g_file_get_uri_scheme (file);

  /* try to launch common schemes for know preferred applications */
  if (scheme != NULL && exo_open_uri_known_category (uri, scheme, &retval))
    {
      g_free (scheme);
      return retval;
    }

  /* handle the uri as a file, maybe we succeed... */
  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE, NULL, &err);
  if (file_info != NULL)
    {
      file_type = g_file_info_get_file_type (file_info);
      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
#ifndef NDEBUG
          g_debug ("file is directory, use filemanager");
#endif
          /* directories should go fine with a file manager */
          retval = exo_open_launch_category ("FileManager", uri);
          succeed = TRUE;
        }
      else
        {
          content_type = g_file_info_get_content_type (file_info);
#ifndef NDEBUG
          g_debug ("content type=%s", content_type);
#endif
          if (G_LIKELY (content_type))
            {
              /* try to find a suitable application for this content type */
              path = g_file_get_path (file);
              app_info = g_app_info_get_default_for_type (content_type, path == NULL);
              g_free (path);

              if (app_info != NULL)
                {
                  /* make sure we don't loop somehow */
                  executable = g_app_info_get_executable (app_info);
#ifndef NDEBUG
                  g_debug ("default executable=%s", executable);
#endif
                  if (executable == NULL
                      || strcmp (executable, "exo-open") != 0)
                    {
                      /* launch it */
                      retval = exo_g_app_info_launch_uri (app_info, uri, NULL, &err);
                      succeed = TRUE;
                    }

                  g_object_unref (G_OBJECT (app_info));
                }
            }
        }

      g_object_unref (G_OBJECT (file_info));
    }
  else if (err != NULL
           && scheme != NULL
           && err->code == G_IO_ERROR_NOT_MOUNTED)
    {
      /* check if the scheme is supported by gio */
      schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
      if (G_LIKELY (schemes != NULL))
        {
          for (i = 0; schemes[i] != NULL; i++)
            {
              /* found scheme, open in file manager */
              if (strcmp (scheme, schemes[i]) == 0)
                {
                  retval = succeed = exo_open_launch_category ("FileManager", uri);
                  break;
                }
            }
        }
    }

  g_object_unref (G_OBJECT (file));

  /* our last try... */
  if (!succeed)
    {
#ifndef NDEBUG
          g_debug ("nothing worked, try ftp(s) or gtk_show_uri()");
#endif

      /* try ftp uris if the file manager/gio failed to recognize it */
      if (scheme != NULL
          && (strcmp (scheme, "ftp") == 0 || strcmp (scheme, "ftps") == 0))
        retval = exo_open_launch_category ("WebBrowser", uri);
      else
        retval = gtk_show_uri_on_window (NULL, uri, 0, error);
    }

  g_free (scheme);

  if (!retval && error != NULL)
    *error = err;
  else if (err != NULL)
    g_error_free (err);

  return retval;
}

#include <gdk/gdkx.h>

gint
main (gint argc, gchar **argv)
{
  GOptionContext  *context;
  GtkWidget       *dialog;
  GtkWidget       *message_area;
  GtkWidget       *label;
  GError          *err = NULL;
  gchar           *parameter;
  gint             result = EXIT_SUCCESS;
  GString         *join;
  guint            i;
  gchar           *uri;

#ifdef GETTEXT_PACKAGE
  /* setup i18n support */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

  /* steal the startup id, before gtk tries to grab it */
  startup_id = g_strdup (g_getenv ("DESKTOP_STARTUP_ID"));
  if (startup_id != NULL)
    g_unsetenv ("DESKTOP_STARTUP_ID");

  /* try to parse the command line parameters */
  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  g_option_context_set_ignore_unknown_options (context, TRUE);
  if (!g_option_context_parse (context, &argc, &argv, &err))
    {
      g_fprintf (stderr, "exo-open: %s.\n", err->message);
      g_error_free (err);
      return EXIT_FAILURE;
    }

  /* restore the startup-id for the child environment */
  if (startup_id != NULL)
    g_setenv ("DESKTOP_STARTUP_ID", startup_id, TRUE);

  /* setup default icon for windows */
  gtk_window_set_default_icon_name ("preferences-desktop-default-applications");

  /* check what to do */
  if (G_LIKELY (opt_launch != NULL))
    {
      if (argc > 1)
        {
          /* NOTE: see the comment at the top of this document! */

          /* combine all specified parameters to one parameter string */
          join = g_string_new (NULL);
          for (i = 1; argv[i] != NULL; i++)
            {
              /* separate the arguments */
              if (i > 1)
                join = g_string_append_c (join, ' ');

              /* only quote arguments with spaces if there are multiple
               * arguments to be merged, this is a bit of magic to make
               * common cares work property, see sample above with xfrun4 */
              if (argc > 2 && strchr (argv[i], ' ') != NULL)
                xfce_g_string_append_quoted (join, argv[i]);
              else
                g_string_append (join, argv[i]);
            }
          parameter = g_string_free (join, FALSE);
        }
      else
        {
          parameter = NULL;
        }

      /* run the preferred application */
      if (!exo_open_launch_category (opt_launch, parameter))
        result = EXIT_FAILURE;

      g_free (parameter);
    }
  else if (argc > 1)
    {
      /* open all specified urls */
      for (argv += 1; result == EXIT_SUCCESS && *argv != NULL; ++argv)
        {
          if (g_str_has_suffix (*argv, ".desktop")
              && exo_open_launch_desktop_file (*argv))
            {
              /* successfully launched a desktop file */
              continue;
            }
          else if (g_uri_is_valid (*argv, G_URI_FLAGS_NONE, NULL))
            {
              /* use the argument directly */
              uri = g_strdup (*argv);
            }
          else
            {
              /* try to build a valid uri */
              uri = exo_open_find_scheme (*argv);
            }

#ifndef NDEBUG
          g_debug ("opening the following uri: %s", uri);
#endif

          if (uri == NULL)
            {
              /* display an error dialog */
              dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                               _("Unable to detect the URI-scheme of \"%s\"."), *argv);
              if (startup_id != NULL)
                gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
              gtk_dialog_run (GTK_DIALOG (dialog));
              gtk_widget_destroy (dialog);

              result = EXIT_FAILURE;
            }
          else if (!exo_open_uri (uri, &err))
            {
              if (err != NULL)
                {
                  /* display an error dialog */
                  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                   _("Failed to open URI."));
                  if (startup_id != NULL)
                    gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
                  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", err->message);
                  g_error_free (err);

                  /* add full URI to message area */
                  message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
                  label = gtk_label_new (uri);
                  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                  gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_CHAR);
                  gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
                  gtk_container_add (GTK_CONTAINER (message_area), label);
                  gtk_widget_show (label);

                  gtk_dialog_run (GTK_DIALOG (dialog));
                  gtk_widget_destroy (dialog);
                }

              result = EXIT_FAILURE;
            }

          g_free (uri);
        }
    }
  else if (G_UNLIKELY (opt_help))
    {
      usage ();
    }
  else if (G_UNLIKELY (opt_version))
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
    }
  else
    {
      result = EXIT_FAILURE;
      usage ();
    }

  return result;
}
