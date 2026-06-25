/*
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-history.h>
#include <mousepad/mousepad-util.h>

#include <test-plugin/test-plugin.h>



/* GObject virtual functions */
static void test_plugin_finalize       (GObject        *object);

/* MousepadPlugin virtual functions */
static void test_plugin_enable         (MousepadPlugin *mplugin);
static void test_plugin_disable        (MousepadPlugin *mplugin);

/* TestPlugin own functions */
static void test_plugin_window_actions (GSimpleAction  *test_action,
                                        GVariant       *parameter,
                                        gpointer        data);

#define LOG_COMMAND(command) g_printerr ("Command: %s: %s\n", G_STRLOC, command);
#define LOG_WARNING(warning) g_printerr ("%s: %s\n", G_STRLOC, warning);



struct _TestPlugin
{
  MousepadPlugin __parent__;

  /* actions opening a blocking or non-blocking dialog */
  GHashTable *dialog_actions;
};

static GApplication* application;

#define PF(str) ("mousepad-test-plugin." str)

static const GActionEntry test_actions[] =
{
  { PF ("window-actions"), test_plugin_window_actions, "s", NULL, NULL },
};

#undef PF



G_DEFINE_DYNAMIC_TYPE (TestPlugin, test_plugin, MOUSEPAD_TYPE_PLUGIN);



void
test_plugin_register (MousepadPluginProvider *plugin)
{
  test_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
test_plugin_class_init (TestPluginClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->finalize = test_plugin_finalize;

  mplugin_class->enable = test_plugin_enable;
  mplugin_class->disable = test_plugin_disable;
}



static void
test_plugin_class_finalize (TestPluginClass *klass)
{
}



static void
test_plugin_init (TestPlugin *plugin)
{
  /* get the mousepad application */
  application = g_application_get_default ();

  /* add test actions to the application */
  g_action_map_add_action_entries (G_ACTION_MAP (application), test_actions,
                                   G_N_ELEMENTS (test_actions), plugin);

  /* setup the dialog actions hash table */
  plugin->dialog_actions = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (plugin->dialog_actions, "preferences", GINT_TO_POINTER (FALSE));
  g_hash_table_insert (plugin->dialog_actions, "file.open", GINT_TO_POINTER (TRUE));
  g_hash_table_insert (plugin->dialog_actions, "file.open-recent.clear-history",
                       GINT_TO_POINTER (TRUE));
  g_hash_table_insert (plugin->dialog_actions, "file.save-as", GINT_TO_POINTER (TRUE));
  g_hash_table_insert (plugin->dialog_actions, "file.print", GINT_TO_POINTER (FALSE));
  g_hash_table_insert (plugin->dialog_actions, "search.find-and-replace", GINT_TO_POINTER (FALSE));
  g_hash_table_insert (plugin->dialog_actions, "search.go-to", GINT_TO_POINTER (TRUE));
  g_hash_table_insert (plugin->dialog_actions, "view.select-font", GINT_TO_POINTER (TRUE));
  g_hash_table_insert (plugin->dialog_actions, "document.tab.tab-size", GINT_TO_POINTER (TRUE));
  g_hash_table_insert (plugin->dialog_actions, "help.about", GINT_TO_POINTER (TRUE));

  test_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
test_plugin_finalize (GObject *object)
{
  TestPlugin *plugin = TEST_PLUGIN (object);

  test_plugin_disable (MOUSEPAD_PLUGIN (plugin));

  g_hash_table_destroy (plugin->dialog_actions);

  G_OBJECT_CLASS (test_plugin_parent_class)->finalize (object);
}



static gboolean
test_plugin_dialog_shown (gpointer instance)
{
  gpointer  dialog;
  GList    *windows, *window;

  /* disconnect this handler */
  if (G_IS_ACTION (instance))
    mousepad_disconnect_by_func (instance, test_plugin_dialog_shown, NULL);

  /* search for a GtkDialog in the GdkWindow list */
  windows = gdk_screen_get_toplevel_windows (gdk_screen_get_default ());
  for (window = windows; window != NULL; window = window->next)
    {
      gdk_window_get_user_data (window->data, &dialog);
      if (GTK_IS_DIALOG (dialog) && gtk_widget_get_visible (dialog))
        {
          if (g_application_get_is_busy (application))
            g_application_unmark_busy (application);
          else
            gtk_dialog_response (dialog, MOUSEPAD_RESPONSE_CANCEL);

          break;
        }
    }

  g_list_free (windows);

  if (window == NULL)
    LOG_WARNING ("Dialog not found");

  return FALSE;
}



static void
test_plugin_dialog_shown_timeout (gpointer instance)
{
  g_timeout_add_seconds (2, test_plugin_dialog_shown, mousepad_util_source_autoremove (instance));
}



static void
test_plugin_disable_monitoring (TestPlugin *plugin)
{
  GAction *action;

  /* remove pending sources */
  while (g_source_remove_by_user_data (application));

  /* disconnect signal handlers */
  g_signal_handlers_disconnect_by_data (application, plugin);

  action = g_action_map_lookup_action (G_ACTION_MAP (application), "preferences");
  g_signal_handlers_disconnect_by_data (action, NULL);
}



static void
test_plugin_is_busy_changed (GApplication *gapplication,
                             GParamSpec   *pspec,
                             TestPlugin   *plugin)
{
  if (! g_application_get_is_busy (gapplication))
    test_plugin_disable_monitoring (plugin);
}



static void
test_plugin_action_added (GActionGroup *action_group,
                          const gchar  *action_name,
                          TestPlugin   *plugin)
{
  GAction  *action;
  gpointer  blocking;

  if (g_hash_table_lookup_extended (plugin->dialog_actions, action_name, NULL, &blocking))
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), action_name);
      if (blocking)
        test_plugin_dialog_shown_timeout (action);
      else
        g_signal_connect (action, "activate", G_CALLBACK (test_plugin_dialog_shown), NULL);
    }
}



static gboolean
test_plugin_window_shown_idle (gpointer data)
{
  g_application_unmark_busy (application);

  /* `mousepad --disable-server` case: the test script can't run `mousepad --quit`
   * to terminate the current instance, so let's do it internally */
  if (g_application_get_flags (application) & G_APPLICATION_NON_UNIQUE)
    {
      LOG_COMMAND ("app.quit");
      g_action_group_activate_action (G_ACTION_GROUP (application), "quit", NULL);
    }

  return FALSE;
}



static void
test_plugin_window_shown (TestPlugin *plugin)
{
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (application));
  if (window != NULL && gtk_widget_get_visible (GTK_WIDGET (window)))
    {
      /* allow time to restore the entire session if needed */
      if (MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE) != MOUSEPAD_SESSION_RESTORE_NEVER)
        {
          test_plugin_disable_monitoring (plugin);
          g_idle_add (test_plugin_window_shown_idle, mousepad_util_source_autoremove (plugin));
        }
      else
        test_plugin_window_shown_idle (plugin);
    }
}



static void
test_plugin_enable (MousepadPlugin *mplugin)
{
  /* mark application as busy by default */
  g_application_mark_busy (application);

  /* connect to all required signals to monitor application state */
  g_signal_connect_object (application, "activate", G_CALLBACK (test_plugin_window_shown), mplugin,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_object (application, "open", G_CALLBACK (test_plugin_window_shown), mplugin,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect (application, "action-added", G_CALLBACK (test_plugin_action_added), mplugin);

  /* `mousepad --encoding -- file`: a special case where the dialog is not opened by an action */
  g_signal_connect (application, "open", G_CALLBACK (test_plugin_dialog_shown_timeout), mplugin);
  g_signal_connect (application, "notify::is-busy",
                    G_CALLBACK (test_plugin_is_busy_changed), mplugin);
}



static void
test_plugin_disable (MousepadPlugin *mplugin)
{
  test_plugin_disable_monitoring (TEST_PLUGIN (mplugin));
}



static void
test_plugin_activate_action (GActionGroup *group,
                             const gchar  *detailed_name)
{
  GAction  *action;
  GVariant *parameter;
  GError   *error = NULL;
  gchar    *name, *prefixed_name;

  /* retrieve action name and parameter */
  if (! g_action_parse_detailed_name (detailed_name, &name, &parameter, &error))
    {
      LOG_WARNING (error->message);
      return;
    }

  /* log command */
  prefixed_name = g_strconcat ("win.", name, NULL);
  LOG_COMMAND (prefixed_name);

  /* ensure action is enabled (e.g. if disabled when the view is not focused) */
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

  /* activate action */
  g_action_activate (action, parameter);

  /* cleanup */
  g_free (name);
  g_free (prefixed_name);
  if (parameter != NULL)
    g_variant_unref (parameter);
}



typedef struct _ActionHook
{
  gchar **prereqs;
  gchar **postprocs;
} ActionHook;

/*
 * Allocate a new ActionHook from 'pr' prerequisite and 'pp' post-process action names,
 * to be run before and after a given action:
 *   test_plugin_action_hook_new (pr, pp, "a_1", ..., "a_pr", "b_1", ..., "b_pp")
 *
 * The returned hook is to be inserted in a hash table with the name of this action as key.
 * If this action or one of the prerequisite or post-process actions takes a parameter, it
 * must be specified in the action name following the general syntax for detailed action names:
 * "action-name(parameter)" (see g_action_parse_detailed_name() documentation).
 *
 * As a special case, to only specify a parameter for an action via the hash table key, use
 * an empty hook:
 *   test_plugin_action_hook_new (0, 0)
 *
 * The returned hook should be freed with test_plugin_action_hook_free() when no longer needed.
 */
static ActionHook *
test_plugin_action_hook_new (guint pr,
                             guint pp,
                             ...)
{
  ActionHook *hook;
  guint       n;
  va_list     ap;

  hook = g_new (ActionHook, 1);
  hook->prereqs = g_new (gchar *, pr + 1);
  hook->postprocs = g_new (gchar *, pp + 1);

  va_start (ap, pp);

  hook->prereqs[pr] = NULL;
  for (n = 0; n < pr; n++)
    hook->prereqs[n] = g_strdup (va_arg (ap, gchar *));

  hook->postprocs[pp] = NULL;
  for (n = 0; n < pp; n++)
    hook->postprocs[n] = g_strdup (va_arg (ap, gchar *));

  va_end(ap);

  return hook;
}

static void
test_plugin_action_hook_free (gpointer data)
{
  ActionHook *hook = data;

  g_strfreev (hook->prereqs);
  g_strfreev (hook->postprocs);
  g_free (hook);
}

static gboolean
test_plugin_action_hook_equal_func (gconstpointer a,
                                    gconstpointer b)
{
  gchar    *p, *s = (gchar *) a, *t = (gchar *) b;
  gboolean  r;

  if ((p = g_strstr_len (s, -1, "(")) != NULL)
    s = g_strndup (s, p - s);
  else
    return g_str_equal (s, t);

  r = g_str_equal (s, t);
  g_free (s);

  return r;
}

static guint
test_plugin_action_hook_hash_func (gconstpointer k)
{
  gchar *p, *s = (gchar *) k;
  guint  r;

  if ((p = g_strstr_len (s, -1, "(")) != NULL)
    s = g_strndup (s, p - s);
  else
    return g_str_hash (s);

  r = g_str_hash (s);
  g_free (s);

  return r;
}



static void
test_plugin_window_actions (GSimpleAction *test_action,
                            GVariant      *parameter,
                            gpointer       data)
{
  ActionHook    *hook;
  GActionGroup  *group;
  GRegex        *included, *excluded;
  GHashTable    *hooks;
  gpointer       key;
  gchar        **actions, **pname, **qname;
  const gchar   *type;
  gboolean       save = FALSE;
  gint           restore;

  /* get the window actions list */
  group = G_ACTION_GROUP (gtk_application_get_active_window (GTK_APPLICATION (application)));
  actions = g_action_group_list_actions (group);

  /* create the hooks hash table */
  hooks = g_hash_table_new_full (test_plugin_action_hook_hash_func,
                                 test_plugin_action_hook_equal_func,
                                 NULL, test_plugin_action_hook_free);

  /* setup regexes and action hooks */
  type = g_variant_get_string (parameter, NULL);
  if (g_strcmp0 (type, "off-menu") == 0)
    {
      included = g_regex_new ("font-size", 0, 0, NULL);
      excluded = g_regex_new ("^[^.]+\\.", 0, 0, NULL);
    }
  else if (g_strcmp0 (type, "file") == 0)
    {
      included = g_regex_new ("^file\\.", 0, 0, NULL);
      excluded = g_regex_new ("\\.(new-from-template\\.new|open-recent\\.new|close-window)$",
                              0, 0, NULL);

      hook = test_plugin_action_hook_new (1, 0, "file.new");
      g_hash_table_insert (hooks, "file.close-tab", hook);

      hook = test_plugin_action_hook_new (1, 0, "file.new");
      g_hash_table_insert (hooks, "file.detach-tab", hook);

      hook = test_plugin_action_hook_new (0, 1, "file.close-tab");
      g_hash_table_insert (hooks, "file.new", hook);
    }
  else if (g_strcmp0 (type, "edit") == 0)
    {
      included = g_regex_new ("^edit\\.", 0, 0, NULL);
      excluded = g_regex_new ("\\.(paste-from-history|select-all|undo)$", 0, 0, NULL);

      hook = test_plugin_action_hook_new (1, 0, "edit.select-all");
      g_hash_table_insert (hooks, "edit.copy", hook);

      hook = test_plugin_action_hook_new (1, 1, "edit.select-all", "edit.undo");
      g_hash_table_insert (hooks, "edit.cut", hook);

      hook = test_plugin_action_hook_new (2, 0, "edit.select-all", "edit.copy");
      g_hash_table_insert (hooks, "edit.paste", hook);

      hook = test_plugin_action_hook_new (2, 0, "edit.select-all", "edit.copy");
      g_hash_table_insert (hooks, "edit.paste-special.paste-as-column", hook);

      hook = test_plugin_action_hook_new (2, 0, "edit.delete-selection", "edit.undo");
      g_hash_table_insert (hooks, "edit.redo", hook);

      hook = test_plugin_action_hook_new (0, 1, "edit.undo");
      g_hash_table_insert (hooks, "edit.convert.spaces-to-tabs", hook);

      hook = test_plugin_action_hook_new (0, 1, "edit.undo");
      g_hash_table_insert (hooks, "edit.convert.tabs-to-spaces", hook);

      hook = test_plugin_action_hook_new (0, 1, "edit.undo");
      g_hash_table_insert (hooks, "edit.delete-selection", hook);

      save = TRUE;
    }
  else if (g_strcmp0 (type, "search") == 0)
    {
      included = g_regex_new ("^search\\.", 0, 0, NULL);
      excluded = g_regex_new ("$^", 0, 0, NULL);
    }
  else if (g_strcmp0 (type, "view") == 0)
    {
      included = g_regex_new ("^view\\.|bar-visible$", 0, 0, NULL);
      excluded = g_regex_new ("$^", 0, 0, NULL);

      hook = test_plugin_action_hook_new (0, 1, "preferences.window.menubar-visible");
      g_hash_table_insert (hooks, "preferences.window.menubar-visible", hook);

      hook = test_plugin_action_hook_new (0, 1, "preferences.window.statusbar-visible");
      g_hash_table_insert (hooks, "preferences.window.statusbar-visible", hook);

      hook = test_plugin_action_hook_new (0, 1, "preferences.window.toolbar-visible");
      g_hash_table_insert (hooks, "preferences.window.toolbar-visible", hook);

      hook = test_plugin_action_hook_new (0, 1, "view.fullscreen");
      g_hash_table_insert (hooks, "view.fullscreen", hook);
    }
  else if (g_strcmp0 (type, "document") == 0)
    {
      included = g_regex_new ("^document", 0, 0, NULL);
      excluded = g_regex_new ("$^", 0, 0, NULL);

      hook = test_plugin_action_hook_new (1, 0, "file.new");
      g_hash_table_insert (hooks, "document", hook);

      hook = test_plugin_action_hook_new (0, 1, "document.go-to-tab(0)");
      g_hash_table_insert (hooks, "document.go-to-tab(1)", hook);

      hook = test_plugin_action_hook_new (0, 1, "document.viewer-mode");
      g_hash_table_insert (hooks, "document.viewer-mode", hook);

      hook = test_plugin_action_hook_new (0, 1, "document.write-unicode-bom");
      g_hash_table_insert (hooks, "document.write-unicode-bom", hook);

      hook = test_plugin_action_hook_new (0, 0);
      g_hash_table_insert (hooks, "document.filetype('sh')", hook);

      hook = test_plugin_action_hook_new (0, 0);
      g_hash_table_insert (hooks, "document.line-ending(1)", hook);

      hook = test_plugin_action_hook_new (0, 0);
      g_hash_table_insert (hooks, "document.tab.tab-size(0)", hook);

      save = TRUE;
    }
  else if (g_strcmp0 (type, "help") == 0)
    {
      included = g_regex_new ("^help\\.", 0, 0, NULL);
      excluded = g_regex_new ("\\.contents$", 0, 0, NULL);
    }
  else
    {
      included = g_regex_new ("$^", 0, 0, NULL);
      excluded = g_regex_new ("", 0, 0, NULL);
    }

  /* filter action list */
  for (pname = actions; *pname != NULL; pname++)
    if (g_regex_match (included, *pname, 0, NULL) && ! g_regex_match (excluded, *pname, 0, NULL))
      {
        /* activate prerequisites */
        if (g_hash_table_lookup_extended (hooks, *pname, &key, (gpointer *) &hook))
          for (qname = hook->prereqs; *qname != NULL; qname++)
            test_plugin_activate_action (group, *qname);

        /* handle dialog if the current action open one */
        test_plugin_action_added (group, *pname, data);

        /* activate current action */
        test_plugin_activate_action (group, key != NULL ? key : *pname);

        /* activate post-processes */
        if (hook != NULL)
          for (qname = hook->postprocs; *qname != NULL; qname++)
            test_plugin_activate_action (group, *qname);
      }

  /* final save to avoid triggering a dialog */
  restore = MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE);
  if (save && restore != MOUSEPAD_SESSION_RESTORE_UNSAVED
      && restore != MOUSEPAD_SESSION_RESTORE_ALWAYS)
    test_plugin_activate_action (group, "file.save");

  /* cleanup */
  g_strfreev (actions);
  g_hash_table_destroy (hooks);
  g_regex_unref (included);
  g_regex_unref (excluded);
}
