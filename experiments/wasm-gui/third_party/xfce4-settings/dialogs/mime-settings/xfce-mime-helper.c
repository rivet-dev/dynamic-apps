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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "xfce-mime-helper.h"
#include "xfce-mime-helper-utils.h"



static void       xfce_mime_helper_finalize   (GObject        *object);
static XfceMimeHelper *xfce_mime_helper_new        (const gchar    *id,
                                         XfceRc         *rc);
static void       clear_bad_entries     (XfceRc         *rc);



struct _XfceMimeHelperClass
{
  GObjectClass __parent__;
};

struct _XfceMimeHelper
{
  GObject __parent__;

  guint             startup_notify : 1;

  gchar            *id;
  gchar            *icon;
  gchar            *name;
  gchar           **commands;
  gchar           **commands_with_parameter;
  gchar           **commands_with_flag;
  XfceMimeHelperCategory category;
};



G_DEFINE_TYPE (XfceMimeHelper, xfce_mime_helper, G_TYPE_OBJECT)



static void
xfce_mime_helper_class_init (XfceMimeHelperClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_mime_helper_finalize;
}



static void
xfce_mime_helper_init (XfceMimeHelper *helpers)
{
}



static void
xfce_mime_helper_finalize (GObject *object)
{
  XfceMimeHelper *helper = XFCE_MIME_HELPER (object);

  g_strfreev (helper->commands_with_flag);
  g_strfreev (helper->commands_with_parameter);
  g_strfreev (helper->commands);
  g_free (helper->name);
  g_free (helper->icon);
  g_free (helper->id);

  (*G_OBJECT_CLASS (xfce_mime_helper_parent_class)->finalize) (object);
}



static gchar**
substitute_binary (const gchar *commands,
                   const gchar *binary)
{
  gchar **result;
  gchar **s, **t;
  gchar  *tmp;

  /* split the commands */
  result = g_strsplit (commands, ";", -1);

  /* (pre-)process the result */
  for (s = t = result; *s != NULL; ++s)
    {
      if (**s == '\0')
        {
          g_free (*s);
        }
      else if (binary != NULL)
        {
          tmp = xfce_str_replace (*s, "%B", binary);
          g_free (*s);
          *t++ = tmp;
        }
      else
        {
          *t++ = *s;
        }
    }
  *t = NULL;

  return result;
}



/**
 * Substitute env command usage.
 * For launchers that modify the env, such as snaps, this is required
 * to get a functional command from the commands_with_parameter. Otherwise
 * the launcher will only run `env`, quietly doing nothing.
 */
static gchar **
substitute_env(const gchar *commands,
               const gchar *commands_with_parameter,
               const gchar *binary)
{
  gchar **result;

  result = substitute_binary(commands, binary);

  if (G_UNLIKELY(*result != NULL && g_strcmp0 (*result, "env") == 0))
  {
    gchar **replaced;
    gchar *command = xfce_str_replace(commands_with_parameter, "%s", "");
    gchar *cleaned = xfce_str_replace(command, "\"\"", "");

    replaced = substitute_binary(cleaned, binary);
    if (*replaced != NULL && g_strcmp0 (*replaced, "env") != 0)
    {
      g_strfreev(result);
      result = replaced;
    }
    else
    {
      g_strfreev(replaced);
    }

    g_free(cleaned);
    g_free(command);
  }

  return result;
}



static XfceMimeHelper*
xfce_mime_helper_new (const gchar *id,
                XfceRc      *rc)
{
  const gchar *commands_with_flag;
  const gchar *commands_with_parameter;
  const gchar *commands;
  const gchar *str;
  XfceMimeHelper   *helper;
  gchar      **binaries;
  gchar       *binary = NULL;
  guint        n;

  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (rc != NULL, NULL);

  xfce_rc_set_group (rc, "Desktop Entry");

  /* allocate a new helper */
  helper = g_object_new (XFCE_MIME_TYPE_HELPER, NULL);
  helper->id = g_strdup (id);
  helper->startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE);

  /* verify the type of the desktop file */
  str = xfce_rc_read_entry_untranslated (rc, "Type", NULL);
  if (G_UNLIKELY (g_strcmp0 (str, "X-XFCE-Helper") != 0))
    goto failed;

  /* determine the category of the helper */
  str = xfce_rc_read_entry_untranslated (rc, "X-XFCE-Category", NULL);
  if (!xfce_mime_helper_category_from_string (str, &helper->category))
    goto failed;

  /* determine the name of the helper */
  str = xfce_rc_read_entry (rc, "Name", NULL);
  if (G_UNLIKELY (xfce_str_is_empty (str)))
    goto failed;
  helper->name = g_strdup (str);

  /* determine the icon of the helper */
  str = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
  if (G_LIKELY (!xfce_str_is_empty (str)))
    helper->icon = g_strdup (str);

  /* determine the commands */
  commands = xfce_rc_read_entry_untranslated (rc, "X-XFCE-Commands", NULL);
  if (G_UNLIKELY (commands == NULL))
    goto failed;

  commands_with_flag = xfce_str_replace (commands, ";", " %s;");

  /* determine the commands (with parameter) */
  commands_with_parameter = xfce_rc_read_entry_untranslated (rc, "X-XFCE-CommandsWithParameter", NULL);
  if (G_UNLIKELY (commands_with_parameter == NULL))
    goto failed;

  /* check if we need binaries for substitution */
  if (strstr (commands, "%B") != NULL || strstr (commands_with_parameter, "%B") != NULL)
    {
      /* determine the binaries */
      str = xfce_rc_read_entry_untranslated (rc, "X-XFCE-Binaries", NULL);
      if (G_UNLIKELY (str == NULL))
        goto failed;

      /* determine the first available binary */
      binaries = g_strsplit (str, ";", -1);
      for (binary = NULL, n = 0; binaries[n] != NULL && binary == NULL; ++n)
        if (G_LIKELY (binaries[n][0] != '\0'))
          binary = g_find_program_in_path (binaries[n]);
      g_strfreev (binaries);

      /* check if we found a binary */
      if (G_UNLIKELY (binary == NULL))
        goto failed;
    }

  /* substitute the binary (if any) */
  helper->commands = substitute_env (commands, commands_with_parameter, binary);
  helper->commands_with_flag = substitute_binary (commands_with_flag, binary);
  helper->commands_with_parameter = substitute_binary (commands_with_parameter, binary);
  g_free (binary);

  /* verify that we have atleast one command */
  if (G_UNLIKELY (*helper->commands == NULL || *helper->commands_with_parameter == NULL))
    goto failed;

  return helper;

failed:
  g_object_unref (G_OBJECT (helper));
  return NULL;
}



/**
 * xfce_mime_helper_get_category:
 * @helper : a #XfceMimeHelper.
 *
 * Returns the #XfceMimeHelperCategory of @helper.
 *
 * Return value: the #XfceMimeHelperCategory of @helper.
 **/
XfceMimeHelperCategory
xfce_mime_helper_get_category (const XfceMimeHelper *helper)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), XFCE_MIME_HELPER_WEBBROWSER);
  return helper->category;
}



/**
 * xfce_mime_helper_get_id:
 * @helper : a #XfceMimeHelper.
 *
 * Returns the unique id (the .desktop file basename) of @helper.
 *
 * Return value: the unique id of @helper.
 **/
const gchar*
xfce_mime_helper_get_id (const XfceMimeHelper *helper)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), NULL);
  return helper->id;
}



/**
 * xfce_mime_helper_get_name:
 * @helper : a #XfceMimeHelper.
 *
 * Returns the (translated) name of the @helper.
 *
 * Return value: the name of @helper.
 **/
const gchar*
xfce_mime_helper_get_name (const XfceMimeHelper *helper)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), NULL);
  return helper->name;
}



/**
 * xfce_mime_helper_get_icon:
 * @helper : a #XfceMimeHelper.
 *
 * Return the name of the themed icon for @helper or
 * the absolute path to an icon file, or %NULL if no
 * icon is available for @helper.
 *
 * Return value: the icon for @helper or %NULL.
 **/
const gchar*
xfce_mime_helper_get_icon (const XfceMimeHelper *helper)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), NULL);
  return helper->icon;
}



/**
 * xfce_mime_helper_get_command:
 * @helper : a #XfceMimeHelper.
 *
 * Returns a reasonable command for @helper.
 *
 * Return value: a command for @helper.
 **/
const gchar*
xfce_mime_helper_get_command (const XfceMimeHelper *helper)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), NULL);
  return *helper->commands_with_parameter;
}

/* Set the DISPLAY variable, to be use by g_spawn_async. */
static void
set_environment (gchar *display)
{
  if (display != NULL)
    g_setenv ("DISPLAY", display, TRUE);
}

/**
 * xfce_mime_helper_execute:
 * @helper    : a #XfceMimeHelper.
 * @screen    : the #GdkScreen on which to execute @helper or %NULL to use default.
 * @parameter : the parameter to pass to @helper (e.g. URL for WebBrowser) or %NULL
 *              to just run @helper.
 * @error     : return location for errors or %NULL.
 *
 * Executes @helper on @screen with the given @parameter. Returns %TRUE if the
 * execution succeed, else %FALSE and @error will be set.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
xfce_mime_helper_execute (XfceMimeHelper   *helper,
                    GdkScreen   *screen,
                    const gchar *parameter,
                    GError     **error)
{
  gint64        previous;
  gint64        current;
  GdkDisplay   *display = NULL;
  gboolean      succeed = FALSE;
  GError       *err = NULL;
  gchar       **commands;
  gchar       **argv;
  gchar        *command;
  gchar        *display_name = NULL;
  guint         n;
  gint          status;
  gint          result;
  gint          pid;
  const gchar  *real_parameter = parameter;

  // FIXME: startup-notification

  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);

  /* fallback to default screen */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* strip the mailto part if needed */
  if (real_parameter != NULL && g_str_has_prefix (real_parameter, "mailto:"))
    real_parameter = parameter + 7;

  /* determine the command set to use */
  if (real_parameter != NULL && g_str_has_prefix (real_parameter, "-")) {
    commands = helper->commands_with_flag;
  } else if (xfce_str_is_empty (real_parameter)) {
    commands = helper->commands;
  } else {
    commands = helper->commands_with_parameter;
  }

  /* verify that we have atleast one command */
  if (G_UNLIKELY (*commands == NULL))
    {
      g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_INVAL, _("No command specified"));
      return FALSE;
    }

  /* try to run the helper using the various given commands */
  for (n = 0; commands[n] != NULL; ++n)
    {
      /* reset the error */
      g_clear_error (&err);

      /* prepare the command */
      if (xfce_str_is_empty (real_parameter))
        command = g_strdup (commands[n]);
      else
        {
          /* split command into "quoted"/unquoted parts */
          gchar **cmd_parts = g_regex_split_simple ("(\"[^\"]*\")", commands[n], 0, 0);

          /* walk the part array */
          for (gchar **cmd_part = cmd_parts; *cmd_part != NULL; cmd_part++)
            {
              /* quoted part: unquote it, replace %s and re-quote it properly */
              if (g_str_has_prefix (*cmd_part, "\"") && g_str_has_suffix (*cmd_part, "\""))
                {
                  gchar *unquoted = g_strndup (*cmd_part + 1, strlen (*cmd_part) - 2);
                  gchar *filled = xfce_str_replace (unquoted, "%s", real_parameter);
                  gchar *quoted = g_shell_quote (filled);
                  g_free (filled);
                  g_free (unquoted);
                  g_free (*cmd_part);
                  *cmd_part = quoted;
                }
              /* unquoted part: just replace %s */
              else
                {
                  gchar *filled = xfce_str_replace (*cmd_part, "%s", real_parameter);
                  g_free (*cmd_part);
                  *cmd_part = filled;
                }
            }

          /* join parts to reconstitute the command, filled and quoted */
          command = g_strjoinv (NULL, cmd_parts);
          g_strfreev (cmd_parts);
        }

      /* parse the command */
      succeed = g_shell_parse_argv (command, NULL, &argv, &err);
      g_free (command);

      /* check if the parsing failed */
      if (G_UNLIKELY (!succeed))
        continue;

      /* set the display variable */
#ifdef GDK_WINDOWING_X11
      display = gdk_screen_get_display (screen);
      if (display != NULL && GDK_IS_X11_DISPLAY (display))
        display_name = g_strdup (gdk_display_get_name (display));
#endif /* GDK_WINDOWING_X11 */

      /* try to run the command */
      succeed = g_spawn_async (NULL,
        argv,
        NULL,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
        (GSpawnChildSetupFunc) set_environment,
        display_name,
        &pid,
        &err);

      /* cleanup */
      g_strfreev (argv);
      g_free (display_name);

      /* check if the execution was successful */
      if (G_LIKELY (succeed))
        {
          /* determine the current time */
          previous = g_get_monotonic_time ();

          /* wait up to 5 seconds to see whether the command worked */
          for (;;)
            {
              /* check if the command exited with an error */
              result = waitpid (pid, &status, WNOHANG);
              if (result < 0)
                {
                  /* something weird happened */
                  err = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
                  succeed = FALSE;
                  break;
                }
              else if (result > 0 && status != 0)
                {
                  /* the command failed */
                  err = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (EIO), g_strerror (EIO));
                  succeed = FALSE;
                  break;
                }
              else if (result == pid)
                {
                  /* the command succeed */
                  succeed = TRUE;
                  break;
                }

              /* determine the current time */
              current = g_get_monotonic_time ();

              /* check if the command is still running after 5 seconds (which indicates that the command worked) */
              if ((current - previous) / G_USEC_PER_SEC > 5)
                break;

              /* wait some time */
              g_usleep (50 * 1000);
            }

          /* check if we should retry with the next command */
          if (G_LIKELY (succeed))
            break;
        }
    }

  /* propagate the error */
  if (G_UNLIKELY (!succeed))
    g_propagate_error (error, err);

  return succeed;
}




static void       xfce_mime_helper_database_finalize    (GObject                *object);
static XfceMimeHelper *xfce_mime_helper_database_lookup      (XfceMimeHelperDatabase      *database,
                                                   XfceMimeHelperCategory       category,
                                                   const gchar            *id);



struct _XfceMimeHelperDatabaseClass
{
  GObjectClass __parent__;
};

struct _XfceMimeHelperDatabase
{
  GObject     __parent__;
  GHashTable *helpers;
};



G_DEFINE_TYPE (XfceMimeHelperDatabase, xfce_mime_helper_database, G_TYPE_OBJECT)



static void
xfce_mime_helper_database_class_init (XfceMimeHelperDatabaseClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_mime_helper_database_finalize;
}



static void
xfce_mime_helper_database_init (XfceMimeHelperDatabase *database)
{
  database->helpers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}



static void
xfce_mime_helper_database_finalize (GObject *object)
{
  XfceMimeHelperDatabase *database = XFCE_MIME_HELPER_DATABASE (object);

  g_hash_table_destroy (database->helpers);

  (*G_OBJECT_CLASS (xfce_mime_helper_database_parent_class)->finalize) (object);
}



static XfceMimeHelper*
xfce_mime_helper_database_lookup (XfceMimeHelperDatabase *database,
                            XfceMimeHelperCategory  category,
                            const gchar       *id)
{
  XfceMimeHelper *helper;
  XfceRc    *rc;
  gchar     *file;
  gchar     *spec;

  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  /* generate the spec for the helper */
  spec = g_strconcat ("xfce4/helpers/", id, ".desktop", NULL);

  /* try to find a cached version */
  helper = g_hash_table_lookup (database->helpers, spec);

  /* load the helper from the file */
  if (G_LIKELY (helper == NULL))
    {
      xfce_resource_push_path (XFCE_RESOURCE_DATA, DATADIR);
      file = xfce_resource_lookup (XFCE_RESOURCE_DATA, spec);
      xfce_resource_pop_path (XFCE_RESOURCE_DATA);

      if (G_LIKELY (file != NULL))
        {
          rc = xfce_rc_simple_open (file, TRUE);
          if (G_LIKELY (rc != NULL))
            {
              helper = xfce_mime_helper_new (id, rc);
              xfce_rc_close (rc);
            }
          g_free (file);
        }

      /* add the loaded helper to the cache */
      if (G_LIKELY (helper != NULL))
        {
          g_hash_table_insert (database->helpers, spec, helper);
          spec = NULL;
        }
    }

  if (G_LIKELY (helper != NULL))
    {
      if (xfce_mime_helper_get_category (helper) == category)
        g_object_ref (G_OBJECT (helper));
      else
        helper = NULL;
    }

  g_free (spec);

  return helper;
}



/**
 * xfce_mime_helper_database_get:
 *
 * Returns a reference on the default #XfceMimeHelperDatabase
 * instance. The caller is responsible to free the
 * returned object using g_object_unref() when no longer
 * needed.
 *
 * Return value: a reference to the default #XfceMimeHelperDatabase.
 **/
XfceMimeHelperDatabase*
xfce_mime_helper_database_get (void)
{
  static XfceMimeHelperDatabase *database = NULL;

  if (G_LIKELY (database == NULL))
    {
      database = g_object_new (XFCE_MIME_TYPE_HELPER_DATABASE, NULL);
      g_object_add_weak_pointer (G_OBJECT (database), (gpointer) &database);
    }
  else
    {
      g_object_ref (G_OBJECT (database));
    }

  return database;
}



/**
 * xfce_mime_helper_database_get_default:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 *
 * Returns a reference on the default #XfceMimeHelper for
 * the @category in @database or %NULL if no default
 * #XfceMimeHelper is registered for @category.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer needed.
 *
 * Return value: the default #XfceMimeHelper for @category
 *               or %NULL.
 **/
XfceMimeHelper*
xfce_mime_helper_database_get_default (XfceMimeHelperDatabase *database,
                                 XfceMimeHelperCategory  category)
{
  const gchar *id;
  XfceMimeHelper   *helper = NULL;
  XfceRc      *rc;
  gchar       *key;

  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), NULL);
  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, NULL);

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", TRUE);
  if (G_LIKELY (rc != NULL))
    {
      key = xfce_mime_helper_category_to_string (category);
      id = xfce_rc_read_entry_untranslated (rc, key, NULL);
      if (G_LIKELY (id != NULL))
        helper = xfce_mime_helper_database_lookup (database, category, id);

      /* handle migrating from Xfce 4.14 or older */
      if (helper == NULL && g_strcmp0 (id, "Thunar") == 0)
        helper = xfce_mime_helper_database_lookup (database, category, "thunar");

      xfce_rc_close (rc);
      g_free (key);
    }

  return helper;
}



static XfceRc*
mimeapps_open (gboolean readonly)
{
  XfceRc *rc;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "mimeapps.list", readonly);
  if (G_UNLIKELY (rc == NULL))
    {
      /* deprecated location (glib < 2.41) */
      rc = xfce_rc_config_open (XFCE_RESOURCE_DATA, "applications/mimeapps.list", readonly);
    }

  return rc;
}



/**
 * xfce_mime_helper_database_set_default:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 * @helper   : a #XfceMimeHelper.
 * @error    : return location for errors or %NULL.
 *
 * Sets the default #XfceMimeHelper for @category in @database to
 * @helper. Returns %TRUE on success, %FALSE if @error is set.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
xfce_mime_helper_database_set_default (XfceMimeHelperDatabase *database,
                                 XfceMimeHelperCategory  category,
                                 XfceMimeHelper         *helper,
                                 GError           **error)
{
  XfceRc       *rc, *desktop_file;
  gchar        *key;
  const gchar  *filename;
  gchar       **mimetypes;
  guint         i;
  gchar        *path;
  gchar        *entry;

  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, FALSE);
  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (XFCE_MIME_IS_HELPER (helper), FALSE);

  /* open the helpers.rc for writing */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to open %s for writing"), "helpers.rc");
      return FALSE;
    }

  /* save the new setting */
  key = xfce_mime_helper_category_to_string (category);
  xfce_rc_write_entry (rc, key, xfce_mime_helper_get_id (helper));
  g_free (key);

  /* clear the dismissed preference */
  key = g_strconcat (xfce_mime_helper_category_to_string (category), "Dismissed", NULL);
  xfce_rc_delete_entry (rc, key, FALSE);
  xfce_rc_close (rc);
  g_free (key);

  /* get the desktop filename */
  switch (category)
    {
      case XFCE_MIME_HELPER_WEBBROWSER:
        filename = "xfce4-web-browser.desktop";
        break;

      case XFCE_MIME_HELPER_MAILREADER:
        filename = "xfce4-mail-reader.desktop";
        break;

      case XFCE_MIME_HELPER_FILEMANAGER:
        filename = "xfce4-file-manager.desktop";
        break;

      default:
        /* no mimetype support for terminals */
        return TRUE;
    }

  /* open the mimeapp.list file to set the default handler of the mime type */
  rc = mimeapps_open (FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to open %s for writing"), "mimeapps.list");
      return FALSE;
    }

  /* open the exo desktop file to read the mimetypes the file supports */
  path = g_build_filename ("applications", filename, NULL);
  desktop_file = xfce_rc_config_open (XFCE_RESOURCE_DATA, path, TRUE);
  g_free (path);

  if (G_UNLIKELY (desktop_file != NULL))
    {
      xfce_rc_set_group (desktop_file, "Desktop Entry");
      mimetypes = xfce_rc_read_list_entry (desktop_file, "X-XFCE-MimeType", ";");
      if (mimetypes != NULL)
        {
#ifdef HAVE_GIO_UNIX
          GDesktopAppInfo *info = g_desktop_app_info_new (filename);
#endif

          xfce_rc_set_group (rc, "Default Applications");

          for (i = 0; mimetypes[i] != NULL; i++)
            if (!xfce_str_is_empty (mimetypes[i]))
              xfce_rc_write_entry (rc, mimetypes[i], filename);

          xfce_rc_set_group (rc, "Added Associations");

          for (i = 0; mimetypes[i] != NULL; i++)
            if (!xfce_str_is_empty (mimetypes[i]))
              {
                entry = g_strconcat (filename, ";", NULL);
                xfce_rc_write_entry (rc, mimetypes[i], entry);
                g_free (entry);

#ifdef HAVE_GIO_UNIX
                if (info != NULL)
                  {
                    g_app_info_set_as_default_for_type (G_APP_INFO (info),
                                                        mimetypes[i],
                                                        NULL);
                  }
#endif
              }
          g_strfreev (mimetypes);
#ifdef HAVE_GIO_UNIX
          if (info != NULL)
            {
              g_object_unref (info);
            }
#endif
        }

      xfce_rc_close (desktop_file);
    }

  clear_bad_entries (rc);

  xfce_rc_close (rc);

  return TRUE;
}



/**
 * xfce_mime_helper_database_clear_default:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 * @error    : return location for errors or %NULL.
 *
 * Clears the default #XfceMimeHelper for @category in @database.
 * Returns %TRUE on success, %FALSE if @error is set.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 *
 * Since: 0.11.3
 **/
gboolean
xfce_mime_helper_database_clear_default (XfceMimeHelperDatabase *database,
                                   XfceMimeHelperCategory  category,
                                   GError           **error)
{
  XfceRc       *rc, *desktop_file;
  gchar        *key;
  const gchar  *filename;
  gchar       **mimetypes;
  guint         i;
  gchar        *path;

  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, FALSE);
  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* open the helpers.rc for writing */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to open %s for writing"), "helpers.rc");
      return FALSE;
    }

  /* save the new setting */
  key = xfce_mime_helper_category_to_string (category);
  xfce_rc_delete_entry (rc, key, FALSE);
  g_free (key);

  /* clear the dismissed preference */
  key = g_strconcat (xfce_mime_helper_category_to_string (category), "Dismissed", NULL);
  xfce_rc_delete_entry (rc, key, FALSE);
  xfce_rc_close (rc);
  g_free (key);

  /* get the desktop filename */
  switch (category)
    {
      case XFCE_MIME_HELPER_WEBBROWSER:
        filename = "xfce4-web-browser.desktop";
        break;

      case XFCE_MIME_HELPER_MAILREADER:
        filename = "xfce4-mail-reader.desktop";
        break;

      case XFCE_MIME_HELPER_FILEMANAGER:
        filename = "xfce4-file-manager.desktop";
        break;

      default:
        /* no mimetype support for terminals */
        return TRUE;
    }

  /* open the mimeapp.list file to set the default handler of the mime type */
  rc = mimeapps_open (FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to open %s for writing"), "mimeapps.list");
      return FALSE;
    }

  /* open the exo desktop file to read the mimetypes the file supports */
  path = g_build_filename ("applications", filename, NULL);
  desktop_file = xfce_rc_config_open (XFCE_RESOURCE_DATA, path, TRUE);
  g_free (path);

  if (G_UNLIKELY (desktop_file != NULL))
    {
      xfce_rc_set_group (desktop_file, "Desktop Entry");
      mimetypes = xfce_rc_read_list_entry (desktop_file, "X-XFCE-MimeType", ";");
      if (mimetypes != NULL)
        {
          xfce_rc_set_group (rc, "Default Applications");

          for (i = 0; mimetypes[i] != NULL; i++)
            if (!xfce_str_is_empty (mimetypes[i]))
              xfce_rc_delete_entry (rc, mimetypes[i], FALSE);

          xfce_rc_set_group (rc, "Added Associations");

          for (i = 0; mimetypes[i] != NULL; i++)
            if (!xfce_str_is_empty (mimetypes[i]))
              xfce_rc_delete_entry (rc, mimetypes[i], FALSE);

          g_strfreev (mimetypes);
        }

      xfce_rc_close (desktop_file);
    }

  clear_bad_entries (rc);

  xfce_rc_close (rc);

  return TRUE;
}



static void
clear_bad_entry (XfceRc *rc,
                 gchar  *key,
                 gchar  *filename)
{
  gchar **values;

  if (xfce_rc_has_entry (rc, key))
    {
      values = xfce_rc_read_list_entry (rc, key, ";");
      if (values != NULL)
        {
          GSList *list = NULL, *item = NULL;
          gint i;

          for (i = 0; values[i] != NULL; i++)
            {
              if (!xfce_str_is_empty(values[i]) && g_strcmp0(values[i], filename) != 0)
                {
                  list = g_slist_append (list, g_strdup(values[i]));
                }
            }
          g_strfreev(values);

          if (list == NULL)
            {
              xfce_rc_delete_entry (rc, key, FALSE);
            }
          else
            {
              gchar   *value;
              GString *string = g_string_new (NULL);
              for (item = list; item != NULL; item = g_slist_next (item))
                {
                  g_string_append_printf (string, "%s;", (gchar *)item->data);
                }
              value = g_string_free (string, FALSE);
              xfce_rc_write_entry (rc, key, value);
              g_slist_free_full (list, g_free);
              g_free (value);
            }
        }
    }
}



static void
clear_bad_entries (XfceRc *rc)
{
  xfce_rc_set_group (rc, "Added Associations");
  clear_bad_entry (rc, "x-scheme-handler/file", "xfce4-file-manager.desktop"); // Xfce #7257
}



static gint
helper_compare (gconstpointer a,
                gconstpointer b)
{
  return g_utf8_collate (xfce_mime_helper_get_name (a), xfce_mime_helper_get_name (b));
}



/**
 * xfce_mime_helper_database_get_all:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 *
 * Looks up all available helpers for @category
 * in @database, sorted in alphabetic order.
 *
 * The returned list keeps references on the included
 * helpers, so be sure to run
 * <informalexample><programlisting>
 *  g_list_foreach (list, (GFunc) g_object_unref, NULL);
 *  g_list_free (list);
 * </programlisting></informalexample>
 * when you are done.
 *
 * Return value: The list of all helpers available
 *               in @category.
 **/
GList*
xfce_mime_helper_database_get_all (XfceMimeHelperDatabase *database,
                             XfceMimeHelperCategory  category)
{
  XfceMimeHelper *helper;
  GList     *helpers = NULL;
  gchar    **specs;
  gchar     *id;
  gchar     *s;
  guint      n;

  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), NULL);
  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, NULL);

  xfce_resource_push_path (XFCE_RESOURCE_DATA, DATADIR);
  specs = xfce_resource_match (XFCE_RESOURCE_DATA, "xfce4/helpers/*.desktop", TRUE);
  xfce_resource_pop_path (XFCE_RESOURCE_DATA);

  for (n = 0; specs[n] != NULL; ++n)
    {
      s = strrchr (specs[n], '.');
      if (G_LIKELY (s != NULL))
        *s = '\0';

      id = strrchr (specs[n], '/');
      id = (id != NULL) ? id + 1 : specs[n];

      helper = xfce_mime_helper_database_lookup (database, category, id);
      if (G_LIKELY (helper != NULL))
        helpers = g_list_insert_sorted (helpers, helper, helper_compare);

      g_free (specs[n]);
    }
  g_free (specs);

  return helpers;
}



/**
 * xfce_mime_helper_database_get_custom:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 *
 * Returns the custom #XfceMimeHelper set for @database
 * or %NULL if no custom #XfceMimeHelper is set for
 * @category.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer
 * needed.
 *
 * Return value: the custom #XfceMimeHelper for @category
 *               in @database or %NULL.
 **/
XfceMimeHelper*
xfce_mime_helper_database_get_custom (XfceMimeHelperDatabase *database,
                                XfceMimeHelperCategory  category)
{
  gchar *string;
  gchar  id[256];

  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), NULL);
  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, NULL);

  /* determine the id for the custom helper */
  string = xfce_mime_helper_category_to_string (category);
  g_snprintf (id, sizeof (id), "custom-%s", string);
  g_free (string);

  return xfce_mime_helper_database_lookup (database, category, id);
}



/**
 * xfce_mime_helper_database_set_custom:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 * @command  : the custom command.
 *
 * Sets the custom helper for @category in @database
 * to @command.
 **/
void
xfce_mime_helper_database_set_custom (XfceMimeHelperDatabase *database,
                                XfceMimeHelperCategory  category,
                                const gchar       *command)
{
  XfceRc *rc;
  gchar **argv;
  gchar  *category_string;
  gchar  *name;
  gchar  *cmdline;
  gchar  *file;
  gchar   spec[256];

  g_return_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database));
  g_return_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES);
  g_return_if_fail (!xfce_str_is_empty (command));

  /* determine the spec for the custom helper */
  category_string = xfce_mime_helper_category_to_string (category);
  g_snprintf (spec, sizeof (spec), "xfce4/helpers/custom-%s.desktop", category_string);

  /* lookup the resource save location */
  file = xfce_resource_save_location (XFCE_RESOURCE_DATA, spec, TRUE);

  /* write the custom helper file */
  rc = xfce_rc_simple_open (file, FALSE);
  if (G_LIKELY (rc != NULL))
    {
      xfce_rc_set_group (rc, "Desktop Entry");
      xfce_rc_write_bool_entry (rc, "NoDisplay", TRUE);
      xfce_rc_write_entry (rc, "Version", "1.0");
      xfce_rc_write_entry (rc, "Encoding", "UTF-8");
      xfce_rc_write_entry (rc, "Type", "X-XFCE-Helper");
      xfce_rc_write_entry (rc, "X-XFCE-Category", category_string);

      /* check if the command includes a parameter */
      cmdline = (strstr (command, "%s") != NULL) ? g_strdup (command) : g_strconcat (command, " \"%s\"", NULL);

      /* use the command line for the CommandsWithParameter */
      xfce_rc_write_entry (rc, "X-XFCE-CommandsWithParameter", cmdline);

      /* try to parse the command line */
      if (g_shell_parse_argv (cmdline, NULL, &argv, NULL))
        {
          /* use the basename for Name and Icon */
          name = g_path_get_basename (*argv);
          xfce_rc_write_entry (rc, "Icon", name);
          xfce_rc_write_entry (rc, "Name", name);
          g_free (name);

          if (strstr (command, "%s") == NULL) {
            /* trust the user, they defined the command without a parameter (bug #4093) */
            xfce_rc_write_entry (rc, "X-XFCE-Commands", command);
          } else {
            /* use only the binary for the Commands */
            xfce_rc_write_entry (rc, "X-XFCE-Commands", *argv);
          }

          /* cleanup */
          g_strfreev (argv);
        }
      else
        {
          xfce_rc_write_entry (rc, "Name", command);
          xfce_rc_write_entry (rc, "X-XFCE-Commands", command);
        }

      /* save the helper file */
      xfce_rc_close (rc);

      /* cleanup */
      g_free (cmdline);
    }

  /* ditch any cached object */
  g_hash_table_remove (database->helpers, spec);

  /* cleanup */
  g_free (category_string);
  g_free (file);
}

/**
 * xfce_mime_helper_database_get_dismissed:
 * @database : a #XfceMimeHelperDatabase.
 * @category : a #XfceMimeHelperCategory.
 *
 * Returns %TRUE if errors should no longer be displayed
 * on the default #XfceMimeHelper for the @category in @database.
 *
 * Return value: %TRUE if dismissed, %FALSE otherwise.
 **/
gboolean xfce_mime_helper_database_get_dismissed (XfceMimeHelperDatabase *database,
                                            XfceMimeHelperCategory  category)
{
  XfceRc      *rc;
  gchar       *key;
  gboolean     dismissed = FALSE;

  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), FALSE);
  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, FALSE);

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", TRUE);
  if (G_LIKELY (rc != NULL))
    {
      key = g_strconcat (xfce_mime_helper_category_to_string (category), "Dismissed", NULL);
      dismissed = xfce_rc_read_bool_entry (rc, key, FALSE);
      xfce_rc_close (rc);
      g_free (key);
    }

  return dismissed;
}

/**
 * xfce_mime_helper_database_set_dismissed:
 * @database  : a #XfceMimeHelperDatabase.
 * @category  : a #XfceMimeHelperCategory.
 * @dismissed : TRUE if the errr should no longer be displayed.
 * @error     : return location for errors or %NULL.
 *
 * Dismisses future errors related to the selected helper category.
 * This setting is cleared any time a new default is configured.
 * Returns %TRUE on success, %FALSE if @error is set.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
xfce_mime_helper_database_set_dismissed (XfceMimeHelperDatabase *database,
                                   XfceMimeHelperCategory  category,
                                   gboolean           dismissed)
{
  XfceRc       *rc;
  gchar        *key;

  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, FALSE);
  g_return_val_if_fail (XFCE_MIME_IS_HELPER_DATABASE (database), FALSE);

  /* open the helpers.rc for writing */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/helpers.rc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      return FALSE;
    }

  /* save the new setting */
  key = g_strconcat (xfce_mime_helper_category_to_string (category), "Dismissed", NULL);
  xfce_rc_write_bool_entry (rc, key, dismissed);
  xfce_rc_close (rc);
  g_free (key);

  return TRUE;
}
