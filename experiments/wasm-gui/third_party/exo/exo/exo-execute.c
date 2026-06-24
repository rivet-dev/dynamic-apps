/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include <exo/exo-execute.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-execute
 * @title: Executing Applications
 * @short_description: Execute preferred applications
 * @include: exo/exo.h
 * @see_also: gtk_show_uri()
 *
 * This module provides functions to execute certain kinds of applications,
 * for which users can select their preferred ones. For example, whenever
 * you need to run a command in a terminal emulator from within your
 * application you should use exo_execute_terminal_shell() or
 * exo_execute_terminal_shell_on_screen() to make sure you run the user's
 * preferred terminal emulator.
 * On the other hand if you need to display an URL (i.e. you want to point
 * the user to the website of your application), you should use gtk_show_uri()
 * instead, as it will try to automatically determine the appropriate
 * viewer for a given URI.
 **/



/**
 * exo_execute_preferred_application:
 * @category          : the category of the preferred application to launch.
 * @parameter         : additional parameter to pass to the preferred application
 *                      (i.e. an URL to pass to the preferred browser) or %NULL
 *                      to pass no parameter.
 * @working_directory : path to the directory in which to execute the
 *                      preferred application for @category.
 * @envp              : child's environment, or %NULL to inherit parent's.
 * @error             : return location for errors or %NULL.
 *
 * Convenience wrapper to exo_execute_preferred_application_on_screen(), which
 * runs the preferred application for @category on the default #GdkScreen.
 *
 * Note that even if this method returns %TRUE there's no warranty that
 * the preferred application for @category was run successfully, because
 * of the way the helper framework is implemented. But you can be sure
 * that if the execution fails at a later stage, the library will popup
 * an error dialog to inform the user that the execution failed.
 *
 * Returns: %TRUE on success, else %FALSE.
 *
 * Since: 0.3.1.3
 **/
gboolean
exo_execute_preferred_application (const gchar *category,
                                   const gchar *parameter,
                                   const gchar *working_directory,
                                   gchar      **envp,
                                   GError     **error)
{
  g_return_val_if_fail (category != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return exo_execute_preferred_application_on_screen (category, parameter, working_directory, envp, gdk_screen_get_default (), error);
}

/* Set the DISPLAY variable, to be use by g_spawn_async. */
static void
set_environment (gchar *display)
{
  if (display != NULL)
    g_setenv ("DISPLAY", display, TRUE);
}

static gchar *
find_fallback_application_from_rc_file (const gchar *category)
{
  gchar    *cfg = NULL;
  gchar    *data = NULL;
  gchar    *contents = NULL;
  gchar    *app = NULL;
  gchar    *path = NULL;
  GKeyFile *keyfile;

  cfg = g_strconcat(g_get_user_config_dir(), "/xfce4/helpers.rc", NULL);

  if (g_file_get_contents (cfg, &contents, NULL, NULL))
  {
    data = g_strconcat("[Default]\n", contents, NULL);
    keyfile = g_key_file_new ();
    if (g_key_file_load_from_data (keyfile, data, -1, G_KEY_FILE_NONE, NULL))
    {
      app = g_key_file_get_string (keyfile, "Default", category, NULL);
      if (app != NULL)
      {
        path = g_find_program_in_path(app);
        g_free (app);
      }
    }
    g_key_file_free (keyfile);
    g_free (data);
    g_free (contents);
  }

  g_free (cfg);

  return path;
}

#ifdef HAVE_GIO_UNIX
static gchar *
find_fallback_application_from_xdg_mime (const gchar *category)
{
  const gchar *query = NULL;
  gchar *output = NULL;
  gchar *cmd = NULL;
  gchar *path = NULL;
  GDesktopAppInfo *info = NULL;

  if (g_strcmp0(category, "FileManager") == 0)
  {
    query = "xdg-mime query default inode/directory";
  }
  else if (g_strcmp0(category, "MailReader") == 0)
  {
    query = "xdg-mime query default x-scheme-handler/mailto";
  }
  else if (g_strcmp0(category, "WebBrowser") == 0)
  {
    query = "xdg-mime query default x-scheme-handler/http";
  }
  else if (g_strcmp0(category, "TerminalEmulator") == 0)
  {
    info = g_desktop_app_info_new("xfce4-terminal.desktop");
  }
  else
  {
    return NULL;
  }

  if (info == NULL && query != NULL)
  {
    if (g_spawn_command_line_sync(query, &output, NULL, NULL, NULL))
    {
      if (output != NULL)
      {
        cmd = g_utf8_substring(output, 0, g_utf8_strlen(output, -1) - 1);
        g_free(output);
        info = g_desktop_app_info_new(cmd);
      }
    }
  }

  if (info != NULL)
  {
    gchar *executable = g_strdup(g_app_info_get_executable(G_APP_INFO(info)));
    if (g_strcmp0(executable, "exo-open") != 0)
    {
      path = g_find_program_in_path(executable);
    }
    g_free(executable);
  }

  if (cmd != NULL)
  {
    g_free(cmd);
  }

  return path;
}
#endif

static gchar *
find_fallback_application (const gchar *category)
{
  gchar *path = NULL;

  path = find_fallback_application_from_rc_file (category);
  if (path != NULL) {
    return path;
  }

#ifdef HAVE_GIO_UNIX
  path = find_fallback_application_from_xdg_mime (category);
#endif

  return path;
}

/**
 * exo_execute_preferred_application_on_screen:
 * @category          : the category of the preferred application to launch.
 * @parameter         : additional parameter to pass to the preferred application
 *                      (i.e. an URL to pass to the preferred browser) or %NULL
 *                      to pass no parameter.
 * @working_directory : path to the directory in which to execute the
 *                      preferred application for @category.
 * @envp              : child's environment, or %NULL to inherit parent's.
 * @screen            : the #GdkScreen on which to run the preferred
 *                      application for @category.
 * @error             : return location for errors or %NULL.
 *
 * Launches the preferred application for the given @category with the
 * @parameter on @screen in the specified @working_directory.
 *
 * libexo currently supports the following categories: %"WebBrowser",
 * %"MailReader" and %"TerminalEmulator". If you specify an invalid
 * @category here, the execution will fail at a later stage and the
 * user will be presented with an error dialog.
 *
 * Note that even if this method returns %TRUE there's no warranty that
 * the preferred application for @category was run successfully, because
 * of the way the helper framework is implemented. But you can be sure
 * that if the execution fails at a later stage, the library will popup
 * an error dialog to inform the user that the execution failed.
 *
 * Returns: %TRUE on success, else %FALSE.
 *
 * Since: 0.3.1.3
 **/
gboolean
exo_execute_preferred_application_on_screen (const gchar *category,
                                             const gchar *parameter,
                                             const gchar *working_directory,
                                             gchar      **envp,
                                             GdkScreen   *screen,
                                             GError     **error)
{
  GdkDisplay *display = NULL;
  gchar      *argv[5];
  gchar      *display_name = NULL;
  gchar      *path = NULL;
  gint        argc = 0;
  gboolean    success;

  g_return_val_if_fail (category != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* generate the argument vector */

  path = g_find_program_in_path ("xfce4-mime-helper");
  if (G_LIKELY(path != NULL))
  {
    argv[argc++] = path;
    argv[argc++] = "--launch";
    argv[argc++] = (gchar *)category;
  }
  else
  {
    // Fallback mode
    path = find_fallback_application (category);
    if (path == NULL) {
      g_set_error (error, G_SPAWN_ERROR, 0, "Could not find fallback %s application", category);
      return FALSE;
    }
    argv[argc++] = path;
  }

  /* append parameter if given */
  if (G_LIKELY (parameter != NULL))
    argv[argc++] = (gchar *) parameter;

  /* null terminate the argument vector */
  argv[argc] = NULL;

  /* set the display environment variable */
#ifdef GDK_WINDOWING_X11
  display = gdk_screen_get_display (screen);
  if (display != NULL && GDK_IS_X11_DISPLAY (display))
    display_name = g_strdup (gdk_display_get_name (display));
#endif /* GDK_WINDOWING_X11 */

  /* launch the command */
  success = g_spawn_async (working_directory,
    argv,
    envp,
    0,
    (GSpawnChildSetupFunc) set_environment,
    display_name,
    NULL,
    error);

  if (path)
    g_free (path);

  g_free (display_name);
  return success;
}



/**
 * exo_execute_terminal_shell:
 * @command_line      : shell command line to execute.
 * @working_directory : path to the directory in which to execute @command_line
 *                      or %NULL to use the current working directory.
 * @envp              : child's environment, or %NULL to inherit parent's.
 * @error             : return location for errors or %NULL.
 *
 * Convenience wrapper to exo_execute_terminal_shell_on_screen(), which
 * executes the @command_line on the default #GdkScreen.
 *
 * Note that even if this method returns %TRUE there's no warranty that
 * the @command_line was run successfully, because of the way the helper
 * framework is implemented. But you can be sure that if the execution
 * fails at a later stage, the library will popup an error dialog to
 * inform the user that the execution failed.
 *
 * Returns: %TRUE on success, else %FALSE.
 *
 * Since: 0.3.1.3
 **/
gboolean
exo_execute_terminal_shell (const gchar *command_line,
                            const gchar *working_directory,
                            gchar      **envp,
                            GError     **error)
{
  g_return_val_if_fail (command_line != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return exo_execute_terminal_shell_on_screen (command_line, working_directory, envp, gdk_screen_get_default (), error);
}



/**
 * exo_execute_terminal_shell_on_screen:
 * @command_line      : shell command line to execute.
 * @working_directory : path to the directory in which to execute @command_line
 *                      or %NULL to use the current working directory.
 * @envp              : child's environment, or %NULL to inherit parent's.
 * @screen            : the #GdkScreen on which to run the @command_line.
 * @error             : return location for errors or %NULL.
 *
 * Executes @command_line in the default terminal emulator on the specified
 * @screen.
 *
 * If no preferred terminal emulator was chosen by the user so far and
 * no sane fallback could be located, the user will be presented with
 * the preferred application chooser dialog, which prompts to choose
 * a default terminal emulator, and the @command_line will be run
 * afterwards using the new default.
 *
 * Note that even if this method returns %TRUE there's no warranty that
 * the @command_line was run successfully, because of the way the helper
 * framework is implemented. But you can be sure that if the execution
 * fails at a later stage, the library will popup an error dialog to
 * inform the user that the execution failed.
 *
 * Returns: %TRUE on success, else %FALSE.
 *
 * Since: 0.3.1.3
 **/
gboolean
exo_execute_terminal_shell_on_screen (const gchar *command_line,
                                      const gchar *working_directory,
                                      gchar      **envp,
                                      GdkScreen   *screen,
                                      GError     **error)
{
  g_return_val_if_fail (command_line != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return exo_execute_preferred_application_on_screen ("TerminalEmulator", command_line, working_directory, envp, screen, error);
}



#define __EXO_EXECUTE_C__
#include <exo/exo-aliasdef.c>
