/*
 * Copyright (c) 2009 Brian Tarricone <brian@terricone.org>
 * Copyright (C) 1999 Olivier Fourdan <fourdan@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

/**
 * SECTION:xfce-spawn
 * @title: Spawn
 * @short_description: spawn a command with startup notification
 * @stability: Stable
 * @include: libxfce4ui/libxfce4ui.h
 *
 * Functions to spawn a command with startup notification support.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h> /* for _NSGetEnviron */
#endif

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#else
/* no x11, no sn */
#undef HAVE_LIBSTARTUP_NOTIFICATION
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

#include <glib/gstdio.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/xfce-spawn.h>
#include <libxfce4ui/xfce-gdk-extensions.h>
#include <libxfce4ui/libxfce4ui-private.h>
#include <libxfce4ui/libxfce4ui-alias.h>

#ifdef HAVE__NSGETENVIRON
/* for support under apple/darwin */
#define environ (*_NSGetEnviron())
#elif !HAVE_DECL_ENVIRON
/* try extern if environ is not defined in unistd.h */
extern gchar **environ;
#endif

/* the maximum time (seconds) for an application to startup */
#define XFCE_SPAWN_STARTUP_TIMEOUT (30)



typedef struct
{
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  /* startup notification data */
  SnLauncherContext *sn_launcher;
  guint              timeout_id;
#endif

  /* child watch data */
  guint              watch_id;
  GPid               pid;
  GClosure          *closure;
} XfceSpawnData;



#ifdef HAVE_LIBSTARTUP_NOTIFICATION
static gboolean
xfce_spawn_startup_timeout (gpointer user_data)
{
  /* timeout reached, lets stop the startup notification */
  return FALSE;
}



static void
xfce_spawn_startup_timeout_destroy (gpointer user_data)
{
  XfceSpawnData *spawn_data = user_data;
  GPid           pid;

  spawn_data->timeout_id = 0;

  if (G_LIKELY (spawn_data->sn_launcher != NULL))
   {
     /* abort the startup notification */
     sn_launcher_context_complete (spawn_data->sn_launcher);
     sn_launcher_context_unref (spawn_data->sn_launcher);
     spawn_data->sn_launcher = NULL;
   }

  /* if there is no closure to watch the child, also stop
   * the child watch */
  if (G_LIKELY (spawn_data->closure == NULL
                && spawn_data->watch_id != 0))
    {
      pid = spawn_data->pid;
      g_source_remove (spawn_data->watch_id);
      g_child_watch_add (pid,
                         (GChildWatchFunc) (void (*)(void)) g_spawn_close_pid,
                         NULL);
    }
}
#endif



static void
xfce_spawn_startup_watch (GPid     pid,
                          gint     status,
                          gpointer user_data)
{
  XfceSpawnData *spawn_data = user_data;
  GValue         instance_and_params[2] = { { 0, }, { 0, } };

  g_return_if_fail (spawn_data->pid == pid);

  if (G_UNLIKELY (spawn_data->closure != NULL))
    {
      /* xfce spawn has no instance */
      g_value_init (&instance_and_params[0], G_TYPE_POINTER);
      g_value_set_pointer (&instance_and_params[0], NULL);

      g_value_init (&instance_and_params[1], G_TYPE_INT);
      g_value_set_int (&instance_and_params[1], status);

      g_closure_set_marshal (spawn_data->closure, g_cclosure_marshal_VOID__INT);

      g_closure_invoke (spawn_data->closure, NULL,
                        2, instance_and_params, NULL);
    }

  /* don't leave zombies */
  g_spawn_close_pid (pid);
}



static void
xfce_spawn_startup_watch_destroy (gpointer user_data)
{
  XfceSpawnData *spawn_data = user_data;

  spawn_data->watch_id = 0;

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  if (spawn_data->timeout_id != 0)
    g_source_remove (spawn_data->timeout_id);
#endif

  if (G_UNLIKELY (spawn_data->closure != NULL))
    {
      g_closure_invalidate (spawn_data->closure);
      g_closure_unref (spawn_data->closure);
    }

  g_slice_free (XfceSpawnData, spawn_data);
}



#ifdef HAVE_LIBSTARTUP_NOTIFICATION
static gint
xfce_spawn_get_active_workspace_number (GdkScreen *screen)
{
  GdkWindow *root;
  gulong     bytes_after_ret = 0;
  gulong     nitems_ret = 0;
  guint     *prop_ret = NULL;
  Atom       _NET_CURRENT_DESKTOP;
  Atom       _WIN_WORKSPACE;
  Atom       type_ret = None;
  gint       format_ret;
  gint       ws_num = 0;

  GdkDisplay *display;
  display = gdk_screen_get_display (screen);
  gdk_x11_display_error_trap_push (display);

  root = gdk_screen_get_root_window (screen);

  /* determine the X atom values */
  _NET_CURRENT_DESKTOP = XInternAtom (GDK_WINDOW_XDISPLAY (root), "_NET_CURRENT_DESKTOP", False);
  _WIN_WORKSPACE = XInternAtom (GDK_WINDOW_XDISPLAY (root), "_WIN_WORKSPACE", False);

  if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root),
                          gdk_x11_get_default_root_xwindow(),
                          _NET_CURRENT_DESKTOP, 0, 32, False, XA_CARDINAL,
                          &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                          (gpointer) &prop_ret) != Success)
    {
      if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root),
                              gdk_x11_get_default_root_xwindow(),
                              _WIN_WORKSPACE, 0, 32, False, XA_CARDINAL,
                              &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                              (gpointer) &prop_ret) != Success)
        {
          if (G_UNLIKELY (prop_ret != NULL))
            {
              XFree (prop_ret);
              prop_ret = NULL;
            }
        }
    }

  if (G_LIKELY (prop_ret != NULL))
    {
      if (G_LIKELY (type_ret != None && format_ret != 0))
        ws_num = *prop_ret;
      XFree (prop_ret);
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  return ws_num;
}
#endif



/* Called by g_spawn_async before execv() is run in the new child process
 * Used if child_process is set to FALSE in xfce_spawn_process */
static void
background_process_call (gpointer _unused)
{
  /* 1 = leave current working directory unchanged (we already set this)
   * 0 = redirect /dev/{stdin, stdout, stderr} to /dev/null */
  if (G_UNLIKELY (daemon (1, 0) == -1))
    {
      perror ("xfce_spawn: daemon() failed");
    }
}



static gboolean
xfce_spawn_process (GdkScreen    *screen,
                    const gchar  *working_directory,
                    gchar       **argv,
                    gchar       **envp,
                    GSpawnFlags   flags,
                    gboolean      startup_notify,
                    guint32       startup_timestamp,
                    const gchar  *startup_icon_name,
                    GClosure     *child_watch_closure,
                    GError      **error,
                    gboolean      child_process)
{
  gboolean            succeed;
  gboolean            is_x11_display = FALSE;
  gchar             **cenvp;
  guint               n;
  guint               n_cenvp;
  gchar              *display_name;
  GPid                pid;
  XfceSpawnData      *spawn_data;
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  SnLauncherContext  *sn_launcher = NULL;
  SnDisplay          *sn_display = NULL;
  gint                sn_workspace;
  const gchar        *startup_id;
  const gchar        *prgname;
#endif

  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail ((flags & G_SPAWN_DO_NOT_REAP_CHILD) == 0, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      if (startup_notify == TRUE)
        {
          /* 'sn_display_new' crashes when used via wayland, so no startup notification support here */
          g_warning ("startup notification not supported for wayland sessions");
          startup_notify = FALSE;
        }
    }
#endif

  /* lookup the screen with the pointer */
  if (screen == NULL)
    screen = xfce_gdk_screen_get_active (NULL);

  /* setup the child environment (stripping $DESKTOP_STARTUP_ID and $DISPLAY) */
  if (G_LIKELY (envp == NULL))
    envp = (gchar **) environ;
  for (n = 0; envp[n] != NULL; ++n);
  cenvp = g_new0 (gchar *, n + 3);
#ifdef GDK_WINDOWING_X11
  is_x11_display = GDK_IS_X11_DISPLAY (gdk_screen_get_display (screen));
#endif
  for (n_cenvp = n = 0; envp[n] != NULL; ++n)
    {
      if (strncmp (envp[n], "DESKTOP_STARTUP_ID=", 19) != 0
          && (!is_x11_display || strncmp (envp[n], "DISPLAY=", 8) != 0))
        cenvp[n_cenvp++] = g_strdup (envp[n]);
    }

#ifdef GDK_WINDOWING_X11
  if (is_x11_display)
    {
      /* add the real display name for the screen */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* TODO: Move xfwm_make_display_name to libxfce4ui */
      display_name = gdk_screen_make_display_name (screen);
G_GNUC_END_IGNORE_DEPRECATIONS
      cenvp[n_cenvp++] = g_strconcat ("DISPLAY=", display_name, NULL);
      g_free (display_name);
    }
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  /* initialize the sn launcher context */
  if (G_LIKELY (startup_notify))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      sn_display = sn_display_new (GDK_SCREEN_XDISPLAY (screen),
                                   (SnDisplayErrorTrapPush) (void (*)(void)) gdk_error_trap_push,
                                   (SnDisplayErrorTrapPop) (void (*)(void)) gdk_error_trap_pop);
G_GNUC_END_IGNORE_DEPRECATIONS

      if (G_LIKELY (sn_display != NULL))
        {
          sn_launcher = sn_launcher_context_new (sn_display, GDK_SCREEN_XNUMBER (screen));
          if (G_LIKELY (sn_launcher != NULL))
            {
              /* initiate the sn launcher context */
              sn_workspace = xfce_spawn_get_active_workspace_number (screen);
              sn_launcher_context_set_workspace (sn_launcher, sn_workspace);
              sn_launcher_context_set_binary_name (sn_launcher, argv[0]);
              sn_launcher_context_set_icon_name (sn_launcher, startup_icon_name != NULL ?
                                                 startup_icon_name : "applications-other");

              if (G_LIKELY (!sn_launcher_context_get_initiated (sn_launcher)))
                {
                  prgname = g_get_prgname ();
                  sn_launcher_context_initiate (sn_launcher, prgname != NULL ?
                                                prgname : "unknown", argv[0],
                                                startup_timestamp);
                }

              /* add the real startup id to the child environment */
              startup_id = sn_launcher_context_get_startup_id (sn_launcher);
              if (G_LIKELY (startup_id != NULL))
                cenvp[n_cenvp++] = g_strconcat ("DESKTOP_STARTUP_ID=", startup_id, NULL);
            }
        }
    }
#endif

  /* watch the child process */
  flags |= G_SPAWN_DO_NOT_REAP_CHILD;

  /* test if the working directory exists */
  if (working_directory == NULL || *working_directory == '\0')
    {
      /* not worth a warning */
      working_directory = NULL;
    }
  else if (!g_file_test (working_directory, G_FILE_TEST_IS_DIR))
    {
      /* print warning for user */
      g_printerr (_("Working directory \"%s\" does not exist. It won't be used "
                    "when spawning \"%s\"."), working_directory, *argv);
      working_directory = NULL;
    }

  /* try to spawn the new process */
  succeed = g_spawn_async (working_directory, argv, cenvp, flags,
                           child_process ? NULL : background_process_call,
                           NULL, &pid, error);

  g_strfreev (cenvp);

  if (G_LIKELY (succeed))
    {
      /* setup data to watch the child */
      spawn_data = g_slice_new0 (XfceSpawnData);
      spawn_data->pid = pid;
      if (child_watch_closure != NULL)
        {
          spawn_data->closure = g_closure_ref (child_watch_closure);
          g_closure_sink (spawn_data->closure);
        }

      spawn_data->watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid,
                                                     xfce_spawn_startup_watch,
                                                     spawn_data,
                                                     xfce_spawn_startup_watch_destroy);

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
      if (G_LIKELY (sn_launcher != NULL))
        {
          /* start a timeout to stop the startup notification sequence after
           * about XFCE_SPAWN_STARTUP_TIMEOUT seconds, to handle applications that do not
           * properly implement startup notify */
          spawn_data->sn_launcher = sn_launcher;
          spawn_data->timeout_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                               XFCE_SPAWN_STARTUP_TIMEOUT,
                                                               xfce_spawn_startup_timeout,
                                                               spawn_data,
                                                               xfce_spawn_startup_timeout_destroy);
        }
#endif
    }
  else
    {
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
      if (G_LIKELY (sn_launcher != NULL))
        {
          /* abort the startup notification sequence */
          sn_launcher_context_complete (sn_launcher);
          sn_launcher_context_unref (sn_launcher);
        }
#endif
    }

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  /* release the sn display */
  if (G_LIKELY (sn_display != NULL))
    sn_display_unref (sn_display);
#endif

  return succeed;
}



static gboolean
_xfce_spawn_command_line (GdkScreen    *screen,
                          const gchar  *command_line,
                          gboolean      in_terminal,
                          gboolean      startup_notify,
                          GError      **error,
                          gboolean      child_process)
{

  gchar    **argv;
  gboolean   succeed;

  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (command_line != NULL, FALSE);

  if (in_terminal == FALSE)
    {
      /* parse the command, retrun false with error when this fails */
      if (G_UNLIKELY (!g_shell_parse_argv (command_line, NULL, &argv, error)))
        return FALSE;
    }
  else
    {
      /* create an argv to run the command in a terminal */
      argv = g_new0 (gchar *, 5);
      argv[0] = g_strdup ("exo-open");
      argv[1] = g_strdup ("--launch");
      argv[2] = g_strdup ("TerminalEmulator");
      argv[3] = g_strdup (command_line);
      argv[4] = NULL;

      /* FIXME: startup notification does not work when
       * launching with exo-open */
      startup_notify = FALSE;
    }

  succeed = xfce_spawn_process (screen, NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                startup_notify, gtk_get_current_event_time (),
                                NULL, NULL, error, child_process);

  g_strfreev (argv);

  return succeed;
}



/**
 * xfce_spawn_on_screen_with_child_watch
 * @screen              : (nullable): a #GdkScreen or %NULL to use the active screen,
 *                        see xfce_gdk_screen_get_active().
 * @working_directory   : (nullable): child's current working directory or %NULL to
 *                        inherit parent's.
 * @argv                : child's argument vector.
 * @envp                : (nullable): child's environment vector or %NULL to inherit
 *                        parent's.
 * @flags               : flags from #GSpawnFlags. #G_SPAWN_DO_NOT_REAP_CHILD
 *                        is not allowed, you should use the
 *                        @child_watch_closure for this.
 * @startup_notify      : whether to use startup notification.
 * @startup_timestamp   : the timestamp to pass to startup notification, use
 *                        the event time here if possible to make focus
 *                        stealing prevention work property. If you don't
 *                        have direct access to the event time you could use
 *                        gtk_get_current_event_time() or if nothing is
 *                        available 0 is valid too.
 * @startup_icon_name   : (nullable): application icon or %NULL.
 * @child_watch_closure : (nullable): closure that is triggered when the child exists
 *                        or %NULL.
 * @error               : (out) (nullable) (transfer full): return location for errors or %NULL.
 *
 * Like xfce_spawn_on_screen(), but allows to attach a closure to watch the
 * child's exit status. This because only one g_child_watch_add() is allowed on
 * Unix (per PID) and this is already internally needed for a proper
 * startup notification implementation.
 *
 * <example>
 * <title>Spawning with a child watch</title>
 * <programlisting>
 * static void
 * child_watch_callback (GObject *object,
 *                       gint     status)
 * {
 *   g_message ("Child exit status is %d", status);
 * }
 *
 * static void
 * spawn_something (void)
 * {
 *   GClosure *child_watch;
 *
 *   child_watch = g_cclosure_new_swap (G_CALLBACK (child_watch_callback),
 *                                      object, NULL);
 *   xfce_spawn_on_screen_with_child_watch (...,
 *                                          child_watch,
 *                                          ...);
 * }
 * </programlisting>
 * </example>
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
xfce_spawn_on_screen_with_child_watch (GdkScreen    *screen,
                                       const gchar  *working_directory,
                                       gchar       **argv,
                                       gchar       **envp,
                                       GSpawnFlags   flags,
                                       gboolean      startup_notify,
                                       guint32       startup_timestamp,
                                       const gchar  *startup_icon_name,
                                       GClosure     *child_watch_closure,
                                       GError      **error)
{
  return xfce_spawn_process (screen, working_directory, argv,
                             envp, flags, startup_notify,
                             startup_timestamp, startup_icon_name,
                             child_watch_closure, error, TRUE);
}



/**
 * xfce_spawn_on_screen:
 * @screen            : (nullable): a #GdkScreen or %NULL to use the active screen,
 *                      see xfce_gdk_screen_get_active().
 * @working_directory : (nullable): child's current working directory or %NULL to
 *                      inherit parent's.
 * @argv              : child's argument vector.
 * @envp              : (nullable): child's environment vector or %NULL to inherit
 *                      parent's.
 * @flags             : flags from #GSpawnFlags. #G_SPAWN_DO_NOT_REAP_CHILD
 *                      is not allowed, use xfce_spawn_on_screen_with_child_watch()
 *                      if you want a child watch.
 * @startup_notify    : whether to use startup notification.
 * @startup_timestamp : the timestamp to pass to startup notification, use
 *                      the event time here if possible to make focus
 *                      stealing prevention work property. If you don't
 *                      have direct access to the event time you could use
 *                      gtk_get_current_event_time() or if nothing is
 *                      available 0 is valid too.
 * @startup_icon_name : (nullable): application icon or %NULL.
 * @error             : (out) (nullable) (transfer full): return location for errors or %NULL.
 *
 * Like gdk_spawn_on_screen() (GDK 2), but also supports startup notification
 * (if Libxfce4ui was built with startup notification support).
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 * 
 * Deprecated: 4.16: Use #xfce_spawn instead.
 **/
gboolean
xfce_spawn_on_screen (GdkScreen    *screen,
                      const gchar  *working_directory,
                      gchar       **argv,
                      gchar       **envp,
                      GSpawnFlags   flags,
                      gboolean      startup_notify,
                      guint32       startup_timestamp,
                      const gchar  *startup_icon_name,
                      GError      **error)
{
  return xfce_spawn_process (screen, working_directory, argv,
                             envp, flags, startup_notify,
                             startup_timestamp, startup_icon_name,
                             NULL, error, TRUE);
}

/**
 * xfce_spawn
 * @screen            : (nullable): a #GdkScreen or %NULL to use the active screen,
 *                      see xfce_gdk_screen_get_active().
 * @working_directory : (nullable): child's current working directory or %NULL to
 *                      inherit parent's.
 * @argv              : child's argument vector.
 * @envp              : (nullable): child's environment vector or %NULL to inherit
 *                      parent's.
 * @flags             : flags from #GSpawnFlags. #G_SPAWN_DO_NOT_REAP_CHILD
 *                      is not allowed, use xfce_spawn_on_screen_with_child_watch()
 *                      if you want a child watch.
 * @startup_notify    : whether to use startup notification.
 * @startup_timestamp : the timestamp to pass to startup notification, use
 *                      the event time here if possible to make focus
 *                      stealing prevention work property. If you don't
 *                      have direct access to the event time you could use
 *                      gtk_get_current_event_time() or if nothing is
 *                      available 0 is valid too.
 * @startup_icon_name : (nullable): application icon or %NULL.
 * @child_process     : %TRUE if the process should be a child process,
 *                      %FALSE if it should be reparented to init.
 * @error             : (out) (nullable) (transfer full): return location for errors or %NULL.
 *
 * Like gdk_spawn_on_screen() (GDK 2), but also supports startup notification
 * (if Libxfce4ui was built with startup notification support).
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 *
 * Since: 4.16
 **/
gboolean
xfce_spawn (GdkScreen    *screen,
            const gchar  *working_directory,
            gchar       **argv,
            gchar       **envp,
            GSpawnFlags   flags,
            gboolean      startup_notify,
            guint32       startup_timestamp,
            const gchar  *startup_icon_name,
            gboolean      child_process,
            GError      **error)
{
  return xfce_spawn_process (screen, working_directory, argv,
                             envp, flags, startup_notify,
                             startup_timestamp, startup_icon_name,
                             NULL, error, child_process);
}



/**
 * xfce_spawn_command_line_on_screen:
 * @screen            : (nullable): a #GdkScreen or %NULL to use the active screen, see xfce_gdk_screen_get_active().
 * @command_line      : command line to run.
 * @in_terminal       : whether to run @command_line in a terminal.
 * @startup_notify    : whether to use startup notification.
 * @error             : (out) (nullable) (transfer full): location for a #GError or %NULL.
 *
 * Executes the given @command_line and returns %TRUE if the
 * command terminated successfully. Else, the @error is set
 * to the standard error output.
 *
 * Returns: %TRUE if the @command_line was executed
 *          successfully, %FALSE if @error is set.
 * 
 * Deprecated: 4.16: Use #xfce_spawn_command_line instead.
 */
gboolean
xfce_spawn_command_line_on_screen (GdkScreen    *screen,
                                   const gchar  *command_line,
                                   gboolean      in_terminal,
                                   gboolean      startup_notify,
                                   GError      **error)
{
  return _xfce_spawn_command_line (screen, command_line, in_terminal,
                                   startup_notify, error, TRUE);
}



/**
 * xfce_spawn_command_line:
 * @screen            : (nullable): a #GdkScreen or %NULL to use the active screen, see xfce_gdk_screen_get_active().
 * @command_line      : command line to run.
 * @in_terminal       : whether to run @command_line in a terminal.
 * @startup_notify    : whether to use startup notification.
 * @child_process     : %TRUE if the process should be a child process, %FALSE if it should be reparented to init.
 * @error             : (out) (nullable) (transfer full): location for a #GError or %NULL.
 *
 * Executes the given @command_line and returns %TRUE if the
 * command terminated successfully. Else, the @error is set
 * to the standard error output.
 *
 * Returns: %TRUE if the @command_line was executed
 *          successfully, %FALSE if @error is set.
 *
 * Since: 4.16
 */
gboolean
xfce_spawn_command_line (GdkScreen    *screen,
                         const gchar  *command_line,
                         gboolean      in_terminal,
                         gboolean      startup_notify,
                         gboolean      child_process,
                         GError      **error)
{
  return _xfce_spawn_command_line (screen, command_line, in_terminal,
                                   startup_notify, error, child_process);
}



#define __XFCE_SPAWN_C__
#include <libxfce4ui/libxfce4ui-aliasdef.c>
