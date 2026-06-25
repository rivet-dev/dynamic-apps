/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h>

#include <gio/gio.h>

#include <common/panel-private.h>
#include <common/panel-dbus.h>
#include <common/panel-debug.h>
#include <common/panel-utils.h>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include <panel/panel-module.h>
#include <panel/panel-plugin-external.h>
#include <panel/panel-window.h>
#include <panel/panel-dialogs.h>



static void         panel_plugin_external_provider_init           (XfcePanelPluginProviderInterface *iface);
static void         panel_plugin_external_finalize                (GObject                          *object);
static void         panel_plugin_external_get_property            (GObject                          *object,
                                                                   guint                             prop_id,
                                                                   GValue                           *value,
                                                                   GParamSpec                       *pspec);
static void         panel_plugin_external_set_property            (GObject                          *object,
                                                                   guint                             prop_id,
                                                                   const GValue                     *value,
                                                                   GParamSpec                       *pspec);
static void         panel_plugin_external_size_allocate           (GtkWidget                        *widget,
                                                                   GtkAllocation                    *allocation);
static void         panel_plugin_external_realize                 (GtkWidget                        *widget);
static void         panel_plugin_external_unrealize               (GtkWidget                        *widget);
static void         panel_plugin_external_plug_added              (GtkSocket                        *socket);
static gboolean     panel_plugin_external_plug_removed            (GtkSocket                        *socket);
static gboolean     panel_plugin_external_child_ask_restart       (PanelPluginExternal              *external);
static void         panel_plugin_external_child_spawn             (PanelPluginExternal              *external);
static void         panel_plugin_external_child_respawn_schedule  (PanelPluginExternal              *external);
static void         panel_plugin_external_child_watch             (GPid                              pid,
                                                                   gint                              status,
                                                                   gpointer                          user_data);
static void         panel_plugin_external_child_watch_destroyed   (gpointer                          user_data);
static void         panel_plugin_external_queue_free              (PanelPluginExternal              *external);
static void         panel_plugin_external_queue_send_to_child     (PanelPluginExternal              *external);
static void         panel_plugin_external_queue_add               (PanelPluginExternal              *external,
                                                                   XfcePanelPluginProviderPropType   type,
                                                                   const GValue                     *value);
static void         panel_plugin_external_queue_add_action        (PanelPluginExternal              *external,
                                                                   XfcePanelPluginProviderPropType   type);
static const gchar *panel_plugin_external_get_name                (XfcePanelPluginProvider          *provider);
static gint         panel_plugin_external_get_unique_id           (XfcePanelPluginProvider          *provider);
static void         panel_plugin_external_set_size                (XfcePanelPluginProvider          *provider,
                                                                   gint                              size);
static void         panel_plugin_external_set_icon_size           (XfcePanelPluginProvider          *provider,
                                                                   gint                              icon_size);
static void         panel_plugin_external_set_dark_mode           (XfcePanelPluginProvider          *provider,
                                                                   gboolean                          dark_mode);
static void         panel_plugin_external_set_mode                (XfcePanelPluginProvider          *provider,
                                                                   XfcePanelPluginMode               mode);
static void         panel_plugin_external_set_nrows               (XfcePanelPluginProvider          *provider,
                                                                   guint                             rows);
static void         panel_plugin_external_set_screen_position     (XfcePanelPluginProvider          *provider,
                                                                   XfceScreenPosition                screen_position);
static void         panel_plugin_external_save                    (XfcePanelPluginProvider          *provider);
static gboolean     panel_plugin_external_get_show_configure      (XfcePanelPluginProvider          *provider);
static void         panel_plugin_external_show_configure          (XfcePanelPluginProvider          *provider);
static gboolean     panel_plugin_external_get_show_about          (XfcePanelPluginProvider          *provider);
static void         panel_plugin_external_show_about              (XfcePanelPluginProvider          *provider);
static void         panel_plugin_external_removed                 (XfcePanelPluginProvider          *provider);
static gboolean     panel_plugin_external_remote_event            (XfcePanelPluginProvider          *provider,
                                                                   const gchar                      *name,
                                                                   const GValue                     *value,
                                                                   guint                            *handler_id);
static void         panel_plugin_external_set_locked              (XfcePanelPluginProvider          *provider,
                                                                   gboolean                          locked);
static void         panel_plugin_external_ask_remove              (XfcePanelPluginProvider          *provider);
static void         panel_plugin_external_set_sensitive           (PanelPluginExternal              *external);



struct _PanelPluginExternalPrivate
{
  /* startup arguments */
  gchar     **arguments;

  guint       embedded : 1;

  /* dbus message queue */
  GSList     *queue;

  /* auto restart timer */
  GTimer     *restart_timer;

  /* child watch data */
  GPid        pid;
  guint       watch_id;

  /* delayed spawning */
  guint       spawn_timeout_id;

  /* silence allocation warnings */
  guint       resize_timeout_id;
};

gulong global_resize_timeout_id = 0;

enum
{
  PROP_0,
  PROP_MODULE,
  PROP_UNIQUE_ID,
  PROP_ARGUMENTS
};



G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PanelPluginExternal, panel_plugin_external, GTK_TYPE_SOCKET,
                                  G_ADD_PRIVATE (PanelPluginExternal)
                                  G_IMPLEMENT_INTERFACE (XFCE_TYPE_PANEL_PLUGIN_PROVIDER,
                                                         panel_plugin_external_provider_init))



static void
panel_plugin_external_class_init (PanelPluginExternalClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;
  GtkSocketClass *gtksocket_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = panel_plugin_external_finalize;
  gobject_class->set_property = panel_plugin_external_set_property;
  gobject_class->get_property = panel_plugin_external_get_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_allocate = panel_plugin_external_size_allocate;
  gtkwidget_class->realize = panel_plugin_external_realize;
  gtkwidget_class->unrealize = panel_plugin_external_unrealize;

  gtksocket_class = GTK_SOCKET_CLASS (klass);
  gtksocket_class->plug_added = panel_plugin_external_plug_added;
  gtksocket_class->plug_removed = panel_plugin_external_plug_removed;

  g_object_class_install_property (gobject_class,
                                   PROP_UNIQUE_ID,
                                   g_param_spec_int ("unique-id",
                                                     NULL, NULL,
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                                                     | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_MODULE,
                                   g_param_spec_object ("module",
                                                        NULL, NULL,
                                                        PANEL_TYPE_MODULE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                                                        | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_ARGUMENTS,
                                   g_param_spec_boxed ("arguments",
                                                       NULL, NULL,
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                                                       | G_PARAM_CONSTRUCT_ONLY));
}



static void
panel_plugin_external_init (PanelPluginExternal *external)
{
  external->priv = panel_plugin_external_get_instance_private (external);

  external->module = NULL;
  external->show_configure = FALSE;
  external->show_about = FALSE;
  external->unique_id = -1;

  external->priv->arguments = NULL;
  external->priv->queue = NULL;
  external->priv->restart_timer = NULL;
  external->priv->embedded = FALSE;
  external->priv->pid = 0;
  external->priv->spawn_timeout_id = 0;
  external->priv->resize_timeout_id = 0;

  /* signal to pass gtk_widget_set_sensitive() changes to the remote window */
  g_signal_connect (G_OBJECT (external), "notify::sensitive",
      G_CALLBACK (panel_plugin_external_set_sensitive), NULL);
}



static void
panel_plugin_external_provider_init (XfcePanelPluginProviderInterface *iface)
{
  iface->get_name = panel_plugin_external_get_name;
  iface->get_unique_id = panel_plugin_external_get_unique_id;
  iface->set_size = panel_plugin_external_set_size;
  iface->set_icon_size = panel_plugin_external_set_icon_size;
  iface->set_dark_mode = panel_plugin_external_set_dark_mode;
  iface->set_mode = panel_plugin_external_set_mode;
  iface->set_nrows = panel_plugin_external_set_nrows;
  iface->set_screen_position = panel_plugin_external_set_screen_position;
  iface->save = panel_plugin_external_save;
  iface->get_show_configure = panel_plugin_external_get_show_configure;
  iface->show_configure = panel_plugin_external_show_configure;
  iface->get_show_about = panel_plugin_external_get_show_about;
  iface->show_about = panel_plugin_external_show_about;
  iface->removed = panel_plugin_external_removed;
  iface->remote_event = panel_plugin_external_remote_event;
  iface->set_locked = panel_plugin_external_set_locked;
  iface->ask_remove = panel_plugin_external_ask_remove;
}



static void
panel_plugin_external_finalize (GObject *object)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (object);

  panel_debug (PANEL_DEBUG_EXTERNAL,
               "%s-%d: plugin is being finalized",
               panel_module_get_name (external->module),
               external->unique_id);

  if (external->priv->spawn_timeout_id != 0)
    g_source_remove (external->priv->spawn_timeout_id);

  if (external->priv->resize_timeout_id != 0)
    {
      g_source_remove (external->priv->resize_timeout_id);
      global_resize_timeout_id -= external->priv->resize_timeout_id;
    }

  if (external->priv->watch_id != 0)
    {
      /* remove the child watch and don't leave zombies */
      g_source_remove (external->priv->watch_id);
      external->priv->watch_id = 0;
      if (external->priv->pid != 0)
        g_child_watch_add (external->priv->pid,
                           (GChildWatchFunc) (void (*)(void)) g_spawn_close_pid,
                           NULL);
    }

  panel_plugin_external_queue_free (external);

  g_strfreev (external->priv->arguments);

  if (external->priv->restart_timer != NULL)
    g_timer_destroy (external->priv->restart_timer);

  g_object_unref (G_OBJECT (external->module));

  (*G_OBJECT_CLASS (panel_plugin_external_parent_class)->finalize) (object);
}



static void
panel_plugin_external_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (object);

  switch (prop_id)
    {
    case PROP_UNIQUE_ID:
      g_value_set_int (value, external->unique_id);
      break;

    case PROP_ARGUMENTS:
      g_value_set_boxed (value, external->priv->arguments);
      break;

    case PROP_MODULE:
      g_value_set_object (value, external->module);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
panel_plugin_external_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (object);

  switch (prop_id)
    {
    case PROP_UNIQUE_ID:
      external->unique_id = g_value_get_int (value);
      break;

    case PROP_ARGUMENTS:
      external->priv->arguments = g_value_dup_boxed (value);
      break;

    case PROP_MODULE:
      external->module = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
panel_plugin_external_queue_resize (GtkWidget *widget,
                                    gpointer   data)
{
  if (PANEL_IS_PLUGIN_EXTERNAL (data))
    gtk_widget_queue_resize (data);
}



static gboolean
panel_plugin_external_queue_resize_timeout (gpointer data)
{
  PanelPluginExternal *external = data;

  if (! gdk_window_is_visible (gtk_socket_get_plug_window (data)))
    return TRUE;

  global_resize_timeout_id -= external->priv->resize_timeout_id;
  external->priv->resize_timeout_id = 0;
  if (global_resize_timeout_id == 0)
    gtk_container_foreach (GTK_CONTAINER (gtk_widget_get_parent (data)),
                           panel_plugin_external_queue_resize, data);

  return FALSE;
}



static void
panel_plugin_external_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (widget);

  if (global_resize_timeout_id != 0)
    {
      if (external->priv->resize_timeout_id != 0)
        {
          global_resize_timeout_id -= external->priv->resize_timeout_id;
          g_source_remove (external->priv->resize_timeout_id);
          external->priv->resize_timeout_id =
            g_timeout_add_seconds (1, panel_plugin_external_queue_resize_timeout, external);
          global_resize_timeout_id += external->priv->resize_timeout_id;
        }

      allocation->width = MAX (allocation->width, 16);
      allocation->height = MAX (allocation->height, 16);
    }

  GTK_WIDGET_CLASS (panel_plugin_external_parent_class)->size_allocate (widget, allocation);
}



static void
panel_plugin_external_realize (GtkWidget *widget)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (widget);

  /* realize the socket first */
  (*GTK_WIDGET_CLASS (panel_plugin_external_parent_class)->realize) (widget);

  if (external->priv->pid == 0)
    {
      if (external->priv->spawn_timeout_id != 0)
        g_source_remove (external->priv->spawn_timeout_id);

      panel_plugin_external_child_spawn (external);
    }
  else
    {
      /* the child was asked to quit during unrealize and there is
       * still an pid, so wait for the child to quit and then
       * spawn it again */
      panel_plugin_external_child_respawn_schedule (external);
    }
}



static void
panel_plugin_external_unrealize (GtkWidget *widget)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (widget);

  /* ask the child to quit */
  if (external->priv->pid != 0)
    {
      if (external->priv->embedded)
        panel_plugin_external_queue_add_action (external, PROVIDER_PROP_TYPE_ACTION_QUIT);
      else
        kill (external->priv->pid, SIGTERM);
    }

  panel_debug (PANEL_DEBUG_EXTERNAL,
               "%s-%d: plugin unrealized; quitting child",
               panel_module_get_name (external->module),
               external->unique_id);

  (*GTK_WIDGET_CLASS (panel_plugin_external_parent_class)->unrealize) (widget);
}



static void
panel_plugin_external_plug_added (GtkSocket *socket)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (socket);

  external->priv->embedded = TRUE;
  external->priv->resize_timeout_id =
    g_timeout_add_seconds (1, panel_plugin_external_queue_resize_timeout, external);
  global_resize_timeout_id += external->priv->resize_timeout_id;

  panel_debug (PANEL_DEBUG_EXTERNAL,
               "%s-%d: child is embedded; %d properties in queue",
               panel_module_get_name (external->module),
               external->unique_id,
               g_slist_length (external->priv->queue));

  /* send queue to wrapper */
  panel_plugin_external_queue_send_to_child (external);
}



static gboolean
panel_plugin_external_plug_removed (GtkSocket *socket)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (socket);

  external->priv->embedded = FALSE;

  panel_debug (PANEL_DEBUG_EXTERNAL,
               "%s-%d: child is unembedded",
               panel_module_get_name (external->module),
               external->unique_id);

  return TRUE;
}



static gboolean
panel_plugin_external_child_ask_restart_dialog (GtkWindow   *parent,
                                                const gchar *plugin_name)
{
  GtkWidget *dialog;
  gint       response;

  panel_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);
  panel_return_val_if_fail (plugin_name != NULL, FALSE);

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                   _("Plugin \"%s\" unexpectedly left the panel, do you want to restart it?"),
                                   plugin_name);
  gtk_window_set_title (GTK_WINDOW (dialog),
                        _("Plugin Restart"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("The plugin restarted more than once in "
                                            "the last %d seconds. If you press Execute the panel will try to restart "
                                            "the plugin otherwise it will be permanently removed from the panel."),
                                            PANEL_PLUGIN_AUTO_RESTART);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Execute"), GTK_RESPONSE_OK,
                          _("_Remove"), GTK_RESPONSE_CLOSE, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return (response == GTK_RESPONSE_OK);
}



static gboolean
panel_plugin_external_remove (gpointer data)
{
  /* cleanup the plugin configuration (in PanelApplication) and destroy 'external' */
  xfce_panel_plugin_provider_emit_signal (data, PROVIDER_SIGNAL_REMOVE_PLUGIN);
  return FALSE;
}



static gboolean
panel_plugin_external_child_ask_restart (PanelPluginExternal *external)
{
  GtkWidget *toplevel;

  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external), FALSE);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (external));
  panel_return_val_if_fail (PANEL_IS_WINDOW (toplevel), FALSE);

  if (external->priv->restart_timer == NULL
      || g_timer_elapsed (external->priv->restart_timer, NULL) > PANEL_PLUGIN_AUTO_RESTART)
    {
      g_message ("Plugin %s-%d has been automatically restarted after crash.",
                 panel_module_get_name (external->module),
                 external->unique_id);
    }
  else if (!panel_plugin_external_child_ask_restart_dialog (GTK_WINDOW (toplevel),
               panel_module_get_display_name (external->module)))
    {
      if (external->priv->watch_id != 0)
        {
          /* remove the child watch and don't leave zombies */
          g_source_remove (external->priv->watch_id);
          external->priv->watch_id = 0;
          if (external->priv->pid != 0)
            g_child_watch_add (external->priv->pid,
                               (GChildWatchFunc) (void (*)(void)) g_spawn_close_pid,
                               NULL);
        }

      /* delay this until we get out of any other idle func, as this triggers the
       * finalization of 'external' */
      g_idle_add_full (G_PRIORITY_HIGH, panel_plugin_external_remove, external, NULL);

      return FALSE;
    }

  /* create or reset the restart timer */
  if (external->priv->restart_timer == NULL)
    external->priv->restart_timer = g_timer_new ();
  else
    g_timer_reset (external->priv->restart_timer);

  return TRUE;
}



static void
panel_plugin_external_child_spawn_child_setup (gpointer data)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (data);
  GdkDisplay          *display;
  const gchar         *name;

  /* this is what gdk_spawn_on_screen does */
  display = gtk_widget_get_display (GTK_WIDGET (external));
  name = gdk_display_get_name (display);
  g_setenv ("DISPLAY", name, TRUE);
}



static void
panel_plugin_external_child_spawn (PanelPluginExternal *external)
{
  gchar        **argv, **dbg_argv, **tmp_argv;
  GError        *error = NULL;
  gboolean       succeed;
  GPid           pid;
  gchar         *program, *cmd_line;
  guint          i;
  gint           tmp_argc;
  gint64         timestamp;

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));
  panel_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (external)));

  /* set plugin specific arguments */
  argv = (*PANEL_PLUGIN_EXTERNAL_GET_CLASS (external)->get_argv) (external, external->priv->arguments);
  panel_return_if_fail (argv != NULL);

  /* check debugging state */
  if (panel_debug_has_domain (PANEL_DEBUG_GDB)
      || panel_debug_has_domain (PANEL_DEBUG_VALGRIND))
    {
      timestamp = g_get_real_time ();
      cmd_line = NULL;
      program = NULL;

      /* note that if the program was not found in PATH, we already warned
       * for it in panel_debug_notify_proxy, so no need to do that again */
      if (panel_debug_has_domain (PANEL_DEBUG_GDB))
        {
          program = g_find_program_in_path ("gdb");
          if (G_LIKELY (program != NULL))
            {
              cmd_line = g_strdup_printf ("%s -batch "
                                          "-ex 'set logging file %s" G_DIR_SEPARATOR_S "%li_gdb_%s_%s.log' "
                                          "-ex 'set logging on' "
                                          "-ex 'set pagination off' "
                                          "-ex 'set logging redirect on' "
                                          "-ex 'run' "
                                          "-ex 'backtrace full' "
                                          "-ex 'info registers' "
                                          "-args",
                                          program, g_get_tmp_dir (), timestamp / G_USEC_PER_SEC,
                                          panel_module_get_name (external->module),
                                          argv[PLUGIN_ARGV_UNIQUE_ID]);
            }
        }
      else if (panel_debug_has_domain (PANEL_DEBUG_VALGRIND))
        {
          program = g_find_program_in_path ("valgrind");
          if (G_LIKELY (program != NULL))
            {
              cmd_line = g_strdup_printf ("%s "
                                          "--log-file='%s" G_DIR_SEPARATOR_S "%li_valgrind_%s_%s.log' "
                                          "--leak-check=full --show-reachable=yes -v ",
                                          program, g_get_tmp_dir (), timestamp / G_USEC_PER_SEC,
                                          panel_module_get_name (external->module),
                                          argv[PLUGIN_ARGV_UNIQUE_ID]);
            }
        }

      if (cmd_line != NULL
          && g_shell_parse_argv (cmd_line, &tmp_argc, &tmp_argv, &error))
        {
          dbg_argv = g_new0 (gchar *, tmp_argc + g_strv_length (argv) + 1);

          for (i = 0; tmp_argv[i] != NULL; i++)
            dbg_argv[i] = tmp_argv[i];
          g_free (tmp_argv);

          for (i = 0; argv[i] != NULL; i++)
            dbg_argv[i + tmp_argc] = argv[i];
          g_free (argv);

          argv = dbg_argv;
        }
      else
        {
          panel_debug (PANEL_DEBUG_EXTERNAL,
                       "%s-%d: Failed to run the plugin in %s: %s",
                       panel_module_get_name (external->module),
                       external->unique_id, program,
                       cmd_line != NULL ? error->message : "debugger not found");
          g_error_free (error);

          return;
        }

      g_free (program);
      g_free (cmd_line);
    }

  /* spawn the proccess */
  succeed = g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                           panel_plugin_external_child_spawn_child_setup,
                           external, &pid, &error);

  panel_debug (PANEL_DEBUG_EXTERNAL,
               "%s-%d: child spawned; pid=%d, argc=%d",
               panel_module_get_name (external->module),
               external->unique_id, pid, g_strv_length (argv));

  if (G_LIKELY (succeed))
    {
      /* watch the child */
      external->priv->pid = pid;
      external->priv->watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid,
                                                         panel_plugin_external_child_watch, external,
                                                         panel_plugin_external_child_watch_destroyed);
    }
  else
    {
      g_critical ("Failed to spawn the xfce4-panel-wrapper: %s", error->message);
      g_error_free (error);
    }

  g_strfreev (argv);
}



static gboolean
panel_plugin_external_child_respawn (gpointer user_data)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (user_data);
  GtkWidget           *window;

  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external), FALSE);

  /* abort startup if the plugin is not realized */
  if (!gtk_widget_get_realized (GTK_WIDGET (external)))
    return FALSE;

  /* delay startup if the old child is still embedded */
  if (external->priv->embedded
      || external->priv->pid != 0)
    {
      panel_debug (PANEL_DEBUG_EXTERNAL,
                   "%s-%d: still a child embedded, respawn delayed",
                   panel_module_get_name (external->module), external->unique_id);

      return TRUE;
    }

  panel_plugin_external_queue_free (external);

  window = gtk_widget_get_toplevel (GTK_WIDGET (external));
  panel_return_val_if_fail (PANEL_IS_WINDOW (window), FALSE);
  panel_window_set_provider_info (PANEL_WINDOW (window), GTK_WIDGET (external), FALSE);

  panel_plugin_external_child_spawn (external);

  /* stop timeout */
  return FALSE;
}



static void
panel_plugin_external_child_respawn_destroyed (gpointer user_data)
{
  PANEL_PLUGIN_EXTERNAL (user_data)->priv->spawn_timeout_id = 0;
}



static void
panel_plugin_external_child_respawn_schedule (PanelPluginExternal *external)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  if (external->priv->spawn_timeout_id == 0)
    {
      panel_debug (PANEL_DEBUG_EXTERNAL,
                   "%s-%d: scheduled a respawn of the child",
                   panel_module_get_name (external->module), external->unique_id);

      /* schedule a restart timeout */
      external->priv->spawn_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 100, panel_plugin_external_child_respawn,
                                                             external, panel_plugin_external_child_respawn_destroyed);
    }
}



static void
panel_plugin_external_child_watch (GPid     pid,
                                   gint     status,
                                   gpointer user_data)
{
  PanelPluginExternal *external = PANEL_PLUGIN_EXTERNAL (user_data);
  gboolean             auto_restart = FALSE;

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));
  panel_return_if_fail (external->priv->pid == pid);

  /* reset the pid, it can't be embedded as well */
  external->priv->pid = 0;
  external->priv->embedded = FALSE;

  panel_debug (PANEL_DEBUG_EXTERNAL,
               "%s-%d: child exited with status %d",
               panel_module_get_name (external->module),
               external->unique_id, status);

  if (WIFEXITED (status))
    {
      /* extract our return value from the status */
      switch (WEXITSTATUS (status))
        {
        case PLUGIN_EXIT_SUCCESS:
          /* normal exit, do not try to restart */
          goto close_pid;

        case PLUGIN_EXIT_SUCCESS_AND_RESTART:
          /* the panel asked for a restart, so do not bother the user */
          auto_restart = TRUE;
          break;

        case PLUGIN_EXIT_FAILURE:
          /* do nothing, maybe we try to restart */
          break;

        case PLUGIN_EXIT_ARGUMENTS_FAILED:
        case PLUGIN_EXIT_PREINIT_FAILED:
        case PLUGIN_EXIT_CHECK_FAILED:
        case PLUGIN_EXIT_NO_PROVIDER:
          g_warning ("Plugin %s-%d exited with status %d, removing from panel configuration",
                     panel_module_get_name (external->module),
                     external->unique_id, WEXITSTATUS (status));

          /* delay this until we get out of any other idle func, as this triggers the
           * finalization of 'external' */
          g_idle_add_full (G_PRIORITY_HIGH, panel_plugin_external_remove, external, NULL);

          goto close_pid;
        }
    }
  else if (WIFSIGNALED (status))
    {
      switch (WTERMSIG (status))
        {
        case SIGUSR1:
          /* the panel asked for a restart, so do not bother the user */
          auto_restart = TRUE;
          break;
        }
    }

  if (gtk_widget_get_realized (GTK_WIDGET (external))
      && (auto_restart || panel_plugin_external_child_ask_restart (external)))
    {
      panel_plugin_external_child_respawn_schedule (external);
    }

close_pid:
  g_spawn_close_pid (pid);
}



static void
panel_plugin_external_child_watch_destroyed (gpointer user_data)
{
  PANEL_PLUGIN_EXTERNAL (user_data)->priv->watch_id = 0;
}



static void
panel_plugin_external_queue_free (PanelPluginExternal *external)
{
  PluginProperty *property;
  GSList         *li;

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  for (li = external->priv->queue; li != NULL; li = li->next)
    {
      property = li->data;
      g_value_unset (&property->value);
      g_slice_free (PluginProperty, property);
    }

  g_slist_free (external->priv->queue);
  external->priv->queue = NULL;
}



static void
panel_plugin_external_queue_send_to_child (PanelPluginExternal *external)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  if (external->priv->queue != NULL)
    {
      external->priv->queue = g_slist_reverse (external->priv->queue);

      (*PANEL_PLUGIN_EXTERNAL_GET_CLASS (external)->set_properties) (external, external->priv->queue);

      panel_plugin_external_queue_free (external);
    }
}



static void
panel_plugin_external_queue_add (PanelPluginExternal             *external,
                                 XfcePanelPluginProviderPropType  type,
                                 const GValue                    *value)
{
  PluginProperty *prop;

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));
  panel_return_if_fail (G_TYPE_CHECK_VALUE (value));

  prop = g_slice_new0 (PluginProperty);
  prop->type = type;
  g_value_init (&prop->value, G_VALUE_TYPE (value));
  g_value_copy (value, &prop->value);

  external->priv->queue = g_slist_prepend (external->priv->queue, prop);

  if (external->priv->embedded)
    panel_plugin_external_queue_send_to_child (external);
}



static void
panel_plugin_external_queue_add_action (PanelPluginExternal             *external,
                                        XfcePanelPluginProviderPropType  type)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  /* add to queue with noop boolean */
  g_value_init (&value, G_TYPE_BOOLEAN);
  panel_plugin_external_queue_add (external, type, &value);
  g_value_unset (&value);
}



static const gchar *
panel_plugin_external_get_name (XfcePanelPluginProvider *provider)
{
  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider), NULL);
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider), NULL);

  return panel_module_get_name (PANEL_PLUGIN_EXTERNAL (provider)->module);
}



static gint
panel_plugin_external_get_unique_id (XfcePanelPluginProvider *provider)
{
  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider), -1);
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider), -1);

  return PANEL_PLUGIN_EXTERNAL (provider)->unique_id;
}



static void
panel_plugin_external_set_size (XfcePanelPluginProvider *provider,
                                gint                     size)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, size);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_SIZE, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_set_icon_size (XfcePanelPluginProvider *provider,
                                     gint                     icon_size)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, icon_size);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_ICON_SIZE, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_set_dark_mode (XfcePanelPluginProvider *provider,
                                     gboolean                 dark_mode)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, dark_mode);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_DARK_MODE, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_set_mode (XfcePanelPluginProvider *provider,
                                XfcePanelPluginMode      mode)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, mode);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_MODE, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_set_nrows (XfcePanelPluginProvider *provider,
                                 guint                    rows)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, rows);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_NROWS, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_set_screen_position (XfcePanelPluginProvider *provider,
                                           XfceScreenPosition       screen_position)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, screen_position);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_SCREEN_POSITION, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_save (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  panel_plugin_external_queue_add_action (PANEL_PLUGIN_EXTERNAL (provider),
                                          PROVIDER_PROP_TYPE_ACTION_SAVE);
}



static gboolean
panel_plugin_external_get_show_configure (XfcePanelPluginProvider *provider)
{
  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider), FALSE);
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider), FALSE);

  return PANEL_PLUGIN_EXTERNAL (provider)->show_configure;
}



static void
panel_plugin_external_show_configure (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  panel_plugin_external_queue_add_action (PANEL_PLUGIN_EXTERNAL (provider),
                                          PROVIDER_PROP_TYPE_ACTION_SHOW_CONFIGURE);
}



static gboolean
panel_plugin_external_get_show_about (XfcePanelPluginProvider *provider)
{
  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider), FALSE);
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider), FALSE);

  return PANEL_PLUGIN_EXTERNAL (provider)->show_about;
}



static void
panel_plugin_external_show_about (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  panel_plugin_external_queue_add_action (PANEL_PLUGIN_EXTERNAL (provider),
                                          PROVIDER_PROP_TYPE_ACTION_SHOW_ABOUT);
}



static void
panel_plugin_external_removed (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  panel_plugin_external_queue_add_action (PANEL_PLUGIN_EXTERNAL (provider),
                                          PROVIDER_PROP_TYPE_ACTION_REMOVED);
}



static gboolean
panel_plugin_external_remote_event (XfcePanelPluginProvider *provider,
                                    const gchar             *name,
                                    const GValue            *value,
                                    guint                   *handle)
{
  return (*PANEL_PLUGIN_EXTERNAL_GET_CLASS (provider)->remote_event) (PANEL_PLUGIN_EXTERNAL (provider),
                                                                      name, value, handle);
}



static void
panel_plugin_external_set_locked (XfcePanelPluginProvider *provider,
                                  gboolean                 locked)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, locked);

  panel_plugin_external_queue_add (PANEL_PLUGIN_EXTERNAL (provider),
                                   PROVIDER_PROP_TYPE_SET_LOCKED, &value);

  g_value_unset (&value);
}



static void
panel_plugin_external_ask_remove (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (provider));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider));

  panel_plugin_external_queue_add_action (PANEL_PLUGIN_EXTERNAL (provider),
                                          PROVIDER_PROP_TYPE_ACTION_ASK_REMOVE);
}



static void
panel_plugin_external_set_sensitive (PanelPluginExternal *external)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, gtk_widget_is_sensitive (GTK_WIDGET (external)));

  panel_plugin_external_queue_add (external, PROVIDER_PROP_TYPE_SET_SENSITIVE,
                                   &value);

  g_value_unset (&value);
}



void
panel_plugin_external_restart (PanelPluginExternal *external)
{
  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  if (external->priv->pid != 0)
    {
      panel_debug (PANEL_DEBUG_EXTERNAL,
                   "%s-%d: child asked to restart; pid=%d",
                   panel_module_get_name (external->module),
                   external->unique_id, external->priv->pid);

      panel_plugin_external_queue_free (external);

      if (external->priv->embedded)
        panel_plugin_external_queue_add_action (external, PROVIDER_PROP_TYPE_ACTION_QUIT_FOR_RESTART);
      else
        kill (external->priv->pid, SIGUSR1);
    }
}



void
panel_plugin_external_set_opacity (PanelPluginExternal *external,
                                   gdouble              opacity)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, opacity);

  panel_plugin_external_queue_add (external,
                                   PROVIDER_PROP_TYPE_SET_OPACITY,
                                   &value);

  g_value_unset (&value);
}



void
panel_plugin_external_set_background_color (PanelPluginExternal *external,
                                            const GdkRGBA       *color)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  if (G_LIKELY (color != NULL))
    {
      g_value_init (&value, G_TYPE_STRING);
      g_value_take_string (&value, gdk_rgba_to_string (color));

      panel_plugin_external_queue_add (external,
                                       PROVIDER_PROP_TYPE_SET_BACKGROUND_COLOR,
                                       &value);

      g_value_unset (&value);
    }
  else
    {
      panel_plugin_external_queue_add_action (external,
                                              PROVIDER_PROP_TYPE_ACTION_BACKGROUND_UNSET);
    }
}



void
panel_plugin_external_set_background_image (PanelPluginExternal *external,
                                            const gchar         *image)
{
  GValue value = { 0, };

  panel_return_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external));

  if (G_UNLIKELY (image != NULL))
    {
      g_value_init (&value, G_TYPE_STRING);
      g_value_set_string (&value, image);

      panel_plugin_external_queue_add (external,
                                       PROVIDER_PROP_TYPE_SET_BACKGROUND_IMAGE,
                                       &value);

      g_value_unset (&value);
    }
  else
    {
      panel_plugin_external_queue_add_action (external,
                                              PROVIDER_PROP_TYPE_ACTION_BACKGROUND_UNSET);
    }
}



GPid
panel_plugin_external_get_pid (PanelPluginExternal *external)
{
  panel_return_val_if_fail (PANEL_IS_PLUGIN_EXTERNAL (external), 0);
  return external->priv->pid;
}
