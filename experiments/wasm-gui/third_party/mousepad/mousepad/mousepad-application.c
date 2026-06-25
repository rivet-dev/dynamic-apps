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
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-prefs-dialog.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-plugin-provider.h>
#include <mousepad/mousepad-history.h>



#define DEFAULT_FONT "Monospace 10"



/* GObject virtual functions */
static void        mousepad_application_set_property              (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   const GValue             *value,
                                                                   GParamSpec               *pspec);
static void        mousepad_application_get_property              (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   GValue                   *value,
                                                                   GParamSpec               *pspec);

/* GApplication virtual functions, in the order in which they are called on launch */
static gint        mousepad_application_handle_local_options      (GApplication             *gapplication,
                                                                   GVariantDict             *options);
static void        mousepad_application_startup                   (GApplication             *gapplication);
static gint        mousepad_application_command_line              (GApplication             *gapplication,
                                                                   GApplicationCommandLine  *command_line);
static void        mousepad_application_activate                  (GApplication             *gapplication);
static void        mousepad_application_open                      (GApplication             *gapplication,
                                                                   GFile                   **files,
                                                                   gint                      n_files,
                                                                   const gchar              *hint);
static void        mousepad_application_shutdown                  (GApplication             *gapplication);

/* MousepadApplication own functions */
static gboolean    mousepad_application_parse_encoding            (const gchar              *option_name,
                                                                   const gchar              *value,
                                                                   gpointer                  data,
                                                                   GError                  **error);
static GtkWidget  *mousepad_application_create_window             (MousepadApplication      *application);
static void        mousepad_application_new_window_with_document  (MousepadWindow           *existing,
                                                                   MousepadDocument         *document,
                                                                   gint                      x,
                                                                   gint                      y,
                                                                   MousepadApplication      *application);
static void        mousepad_application_new_window                (MousepadWindow           *existing,
                                                                   MousepadApplication      *application);
static void        mousepad_application_active_window_changed     (MousepadApplication      *application);
static void        mousepad_application_set_shared_menu_parts     (MousepadApplication      *application,
                                                                   GMenuModel               *model);
static void        mousepad_application_create_languages_menu     (MousepadApplication      *application);
static void        mousepad_application_create_style_schemes_menu (MousepadApplication      *application);
static void        mousepad_application_action_preferences        (GSimpleAction            *action,
                                                                   GVariant                 *value,
                                                                   gpointer                  data);
static void        mousepad_application_action_quit               (GSimpleAction            *action,
                                                                   GVariant                 *value,
                                                                   gpointer                  data);
static void        mousepad_application_toggle_activate           (GSimpleAction            *action,
                                                                   GVariant                 *parameter,
                                                                   gpointer                  data);
static void        mousepad_application_radio_activate            (GSimpleAction            *action,
                                                                   GVariant                 *parameter,
                                                                   gpointer                  data);
static void        mousepad_application_plugin_activate           (GSimpleAction            *action,
                                                                   GVariant                 *parameter,
                                                                   gpointer                  data);
static void        mousepad_application_action_update             (MousepadApplication      *application,
                                                                   gchar                    *key,
                                                                   GSettings                *settings);
static void        mousepad_application_plugin_update             (MousepadApplication      *application);
static void        mousepad_application_action_whitespace         (GSimpleAction            *action,
                                                                   GVariant                 *state,
                                                                   gpointer                  data);



struct _MousepadApplication
{
  GtkApplication      __parent__;

  /* preferences dialog */
  GtkWidget *prefs_dialog;
  gboolean   prefs_dialog_standalone;

  /* command line options */
  gint             opening_mode, line, column;
  MousepadEncoding encoding;

  /* properties */
  gchar                       *default_font;
  GtkSourceSpaceLocationFlags  space_location_flags;

  /* plugins */
  GList *providers;
};

/* MousepadApplication properties */
enum
{
  PROP_DEFAULT_FONT = 1,
  PROP_SPACE_LOCATION,
  N_PROPERTIES
};



/* command line options */
static const GOptionEntry option_entries[] =
{
  {
    "encoding", 'e', G_OPTION_FLAG_OPTIONAL_ARG,
    G_OPTION_ARG_CALLBACK, mousepad_application_parse_encoding,
    N_("Encoding to be used to open files (leave empty to open files in the encoding dialog)"),
    N_("ENCODING")
  },
  {
    "list-encodings", '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Display a list of possible encodings to open files"), NULL
  },
  {
    "line", 'l', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_INT, NULL,
    N_("Line number the cursor to position to (LINE > 0 from top, LINE < 0 from bottom)"),
    N_("LINE")
  },
  {
    "column", 'c', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_INT, NULL,
    N_("Column number the cursor to position to (COLUMN >= 0 from start, COLUMN < 0 from end)"),
    N_("COLUMN")
  },
  {
    "preferences", '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Open the preferences dialog"), NULL
  },
  {
    "disable-server", '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Do not register with the D-BUS session message bus"), NULL
  },
  {
    "quit", 'q', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Quit a running Mousepad primary instance"), NULL
  },
  {
    "version", 'v', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Print version information and exit"), NULL
  },
  {
    G_OPTION_REMAINING, '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_FILENAME_ARRAY, NULL,
    NULL, N_("[FILES...]")
  },
  { NULL }
};

/* opening mode provided on the command line */
enum
{
  TAB = 0,
  WINDOW,
  MIXED
};



/* application actions */

/* stateless actions */
static const GActionEntry stateless_actions[] =
{
  /* command line options */
  { "preferences", mousepad_application_action_preferences, NULL, NULL, NULL },
  { "quit", mousepad_application_action_quit, NULL, NULL, NULL }
};
#define N_STATELESS G_N_ELEMENTS (stateless_actions)

/* preferences dialog */
static const GActionEntry dialog_actions[] =
{
  /* "View" tab */
  { MOUSEPAD_SETTING_SHOW_LINE_NUMBERS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SHOW_WHITESPACE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SHOW_LINE_ENDINGS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SHOW_RIGHT_MARGIN, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_HIGHLIGHT_CURRENT_LINE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_MATCH_BRACES, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_WORD_WRAP, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_USE_DEFAULT_FONT, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_COLOR_SCHEME, mousepad_application_radio_activate, "s", "'none'", NULL },

  /* "Editor" tab */
  { MOUSEPAD_SETTING_INSERT_SPACES, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_AUTO_INDENT, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_INDENT_ON_TAB, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SMART_BACKSPACE, mousepad_application_toggle_activate, NULL, "false", NULL },

  /* "Window" tab */
  { MOUSEPAD_SETTING_CLIENT_SIDE_DECORATIONS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_PATH_IN_TITLE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_REMEMBER_SIZE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_REMEMBER_POSITION, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_REMEMBER_STATE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_TOOLBAR_VISIBLE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_ALWAYS_SHOW_TABS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_EXPAND_TABS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_CYCLE_TABS, mousepad_application_toggle_activate, NULL, "false", NULL },

  /* "File" tab */
  { MOUSEPAD_SETTING_ADD_LAST_EOL, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_MAKE_BACKUP, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_MONITOR_CHANGES, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_AUTO_RELOAD, mousepad_application_toggle_activate, NULL, "false", NULL },
};
#define N_DIALOG G_N_ELEMENTS (dialog_actions)

/* settings only accessible from the menubar */
static const GActionEntry menubar_actions[] =
{
  { MOUSEPAD_SETTING_SEARCH_INCREMENTAL, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SEARCH_HIGHLIGHT_ALL, mousepad_application_toggle_activate, NULL, "false", NULL }
};
#define N_MENUBAR G_N_ELEMENTS (menubar_actions)

/* whitespace location settings, only accessible from GSettings */
static const GActionEntry whitespace_actions[] =
{
  { MOUSEPAD_SETTING_SHOW_WHITESPACE_LEADING, mousepad_application_toggle_activate, NULL,
    "false", mousepad_application_action_whitespace },
  { MOUSEPAD_SETTING_SHOW_WHITESPACE_INSIDE, mousepad_application_toggle_activate, NULL,
    "false", mousepad_application_action_whitespace },
  { MOUSEPAD_SETTING_SHOW_WHITESPACE_TRAILING, mousepad_application_toggle_activate, NULL,
    "false", mousepad_application_action_whitespace },
};
#define N_WHITESPACE G_N_ELEMENTS (whitespace_actions)

/* concatenate all setting actions */
static const GActionEntry* setting_actions[] =
{
  dialog_actions,
  menubar_actions,
  whitespace_actions
};
static const guint n_setting_actions[] =
{
  N_DIALOG,
  N_MENUBAR,
  N_WHITESPACE
};
#define N_SETTING G_N_ELEMENTS (n_setting_actions)



G_DEFINE_TYPE (MousepadApplication, mousepad_application, GTK_TYPE_APPLICATION)



static void
mousepad_application_class_init (MousepadApplicationClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  gobject_class->set_property = mousepad_application_set_property;
  gobject_class->get_property = mousepad_application_get_property;

  application_class->handle_local_options = mousepad_application_handle_local_options;
  application_class->startup = mousepad_application_startup;
  application_class->command_line = mousepad_application_command_line;
  application_class->activate = mousepad_application_activate;
  application_class->open = mousepad_application_open;
  application_class->shutdown = mousepad_application_shutdown;

  g_object_class_install_property (gobject_class, PROP_DEFAULT_FONT,
    g_param_spec_string ("default-font", "DefaultFont", "The default font to use in text views",
                         DEFAULT_FONT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SPACE_LOCATION,
    g_param_spec_flags ("space-location", "SpaceLocation", "The space location setting",
                        GTK_SOURCE_TYPE_SPACE_LOCATION_FLAGS, GTK_SOURCE_SPACE_LOCATION_ALL,
                        G_PARAM_READWRITE));
}



static void
mousepad_application_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_FONT:
      g_free (application->default_font);
      application->default_font = g_value_dup_string (value);
      break;
    case PROP_SPACE_LOCATION:
      application->space_location_flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_application_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_FONT:
      g_value_set_string (value, application->default_font);
      break;
    case PROP_SPACE_LOCATION:
      g_value_set_flags (value, application->space_location_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_application_init (MousepadApplication *application)
{
  gchar *option_desc;

  /* initialize mousepad settings */
  mousepad_settings_init ();

  /* initialize application attributes */
  application->prefs_dialog = NULL;
  application->prefs_dialog_standalone = FALSE;
  application->default_font = g_strdup (DEFAULT_FONT);
  application->space_location_flags = GTK_SOURCE_SPACE_LOCATION_ALL;
  application->opening_mode = TAB;
  application->encoding = mousepad_encoding_get_default ();
  application->line = 0;
  application->column = 0;
  application->providers = NULL;

  /* default application name */
  g_set_application_name (_("Mousepad"));

  /* use the Mousepad icon as default for new windows */
  gtk_window_set_default_icon_name (MOUSEPAD_ID);

  /* this option is added separately using g_strdup_printf() for the description, to be sure
   * that opening modes will be excluded from the translation (experience shows that using a
   * translation context is not enough to ensure it) */
  option_desc = g_strdup_printf (
    _("File opening mode: \"%s\", \"%s\" or \"%s\" (open tabs in a new window)"),
    "tab", "window", "mixed");
  g_application_add_main_option (G_APPLICATION (application), "opening-mode", 'o',
                                 G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, option_desc, _("MODE"));
  g_free (option_desc);

  /* add our option entries to the application */
  g_application_add_main_option_entries (G_APPLICATION (application), option_entries);
}



static gboolean
mousepad_application_parse_encoding (const gchar  *option_name,
                                     const gchar  *value,
                                     gpointer      data,
                                     GError      **error)
{
  MousepadApplication *application;
  gboolean             user_set_encoding = TRUE;

  application = MOUSEPAD_APPLICATION (g_application_get_default ());

  /* we will redirect the user to the encoding dialog */
  if (value == NULL)
    application->encoding = MOUSEPAD_ENCODING_NONE;
  else
    {
      /* try to find the encoding for the charset provided on the command line */
      application->encoding = mousepad_encoding_find (value);

      /* fallback to default if charset was invalid */
      if (application->encoding == MOUSEPAD_ENCODING_NONE)
        {
          g_printerr ("Invalid encoding '%s': ignored\n", value);
          application->encoding = mousepad_encoding_get_default ();
          user_set_encoding = FALSE;
        }
    }

  /* to be attached to the files to open later */
  mousepad_object_set_data (application, "user-set-encoding", GINT_TO_POINTER (user_set_encoding));

  return TRUE;
}



static gint
mousepad_application_handle_local_options (GApplication *gapplication,
                                           GVariantDict *options)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  MousepadEncoding     encoding;
  GApplicationFlags    flags;
  GError              *error = NULL;

  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s %s\n\n", PACKAGE_NAME, PACKAGE_VERSION);
      g_print ("%s\n", "Copyright (C) 2005-2023");
      g_print ("\t%s\n\n", _("The Mousepad developers. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (g_variant_dict_contains (options, "list-encodings"))
    {
      for (encoding = 1; encoding < MOUSEPAD_N_ENCODINGS; encoding++)
        g_print ("%s\n", mousepad_encoding_get_charset (encoding));

      return EXIT_SUCCESS;
    }

  if (g_variant_dict_contains (options, "quit"))
    {
      /* try to register the application */
      if (! g_application_register (gapplication, NULL, &error))
        {
          g_printerr ("%s\n", error->message);
          g_error_free (error);

          return EXIT_FAILURE;
        }

      /* try to find a running primary instance */
      if (! g_application_get_is_remote (gapplication))
        {
          g_printerr ("%s\n", "Failed to find a running Mousepad primary instance");

          return EXIT_FAILURE;
        }
      else
        g_action_group_activate_action (G_ACTION_GROUP (gapplication), "quit", NULL);

      return EXIT_SUCCESS;
    }

  if (g_variant_dict_contains (options, "disable-server"))
    {
      flags = g_application_get_flags (gapplication);
      g_application_set_flags (gapplication, flags | G_APPLICATION_NON_UNIQUE);
    }

  /* transfer encoding from remote to primary instance via the options dictionary */
  g_variant_dict_insert (options, "encoding", "u", application->encoding);
  g_variant_dict_insert (options, "user-set-encoding", "b",
    GPOINTER_TO_INT (mousepad_object_get_data (application, "user-set-encoding")));

  /* chain up to startup (primary instance) or command_line (remote instance) */
  return -1;
}



static void
mousepad_application_update_accels (GtkApplication  *application,
                                    const gchar     *accel_path,
                                    guint            accel_key,
                                    GdkModifierType  accel_mods,
                                    GtkAccelMap     *object)
{
  GVariant    *target;
  gchar       *action, *accel;
  const gchar *accels[] = { NULL, NULL };

  /* make sure to not crash because of corrupted data */
  if (g_str_has_prefix (accel_path, "<Actions>/")
      && g_action_parse_detailed_name (accel_path + 10, &action, &target, NULL)
      && g_action_name_is_valid (action))
    {
      accel = gtk_accelerator_name (accel_key, accel_mods);
      if (*accel != '\0')
        accels[0] = accel;

      gtk_application_set_accels_for_action (application, accel_path + 10, accels);

      /* cleanup */
      g_free (accel);
      g_free (action);
      if (target != NULL)
        g_variant_unref (target);
    }
}



static void
mousepad_application_complete_accel_map (GtkApplication *application)
{
  GtkWindow    *window;
  gchar        *accel_path, *filename;
  guint         n;
  gchar       **action_names;
  const gchar  *excluded_actions[] = { "win.insensitive", "win.file.new-from-template",
                                       "win.file.open-recent", "win.document" };

  /* disconnect this handler */
  mousepad_disconnect_by_func (application, mousepad_application_complete_accel_map, NULL);

  /* complete the accel map with window actions that do not have a default accel */
  window = gtk_application_get_active_window (application);
  action_names = g_action_group_list_actions (G_ACTION_GROUP (window));
  n = 0;
  while (action_names[n] != NULL)
    {
      /* add accel map entry to fill the accels file at shutdown */
      accel_path = g_strconcat ("<Actions>/win.", action_names[n], NULL);
      if (! gtk_accel_map_lookup_entry (accel_path, NULL))
        gtk_accel_map_add_entry (accel_path, 0, 0);

      g_free (accel_path);
      n++;
    }

  g_strfreev (action_names);

  /* prevent actions that should not have a (configurable) keybinding from being saved
   * in the accels file at shutdown */
  for (n = 0; n < G_N_ELEMENTS (excluded_actions); n++)
    {
      accel_path = g_strconcat ("<Actions>/", excluded_actions[n], NULL);
      gtk_accel_map_add_filter (accel_path);
      g_free (accel_path);
    }

  /* connect a handler to update accels when reading the accels file */
  g_signal_connect_swapped (gtk_accel_map_get (), "changed",
                            G_CALLBACK (mousepad_application_update_accels), application);

  /* read the accels file */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, FALSE);
  if (G_LIKELY (filename != NULL))
    {
      gtk_accel_map_load (filename);
      g_free (filename);
    }
}



static void
mousepad_application_set_accels (MousepadApplication *application)
{
  GtkAccelMap      *accel_map;
  GdkModifierType   accel_mods;
  GTypeModule      *provider;
  GList            *item;
  guint             n, accel_key;
  gchar            *accel_path;
  gchar           **action_names;
  const gchar      *accel;
  const gchar      *accels[] = { NULL, NULL };
  const gchar      *accel_maps[][2] =
  {
    /* increase/decrease font size from keyboard/mouse */
    { "win.increase-font-size", "<Control>plus" }, { "win.decrease-font-size", "<Control>minus" },
    { "win.reset-font-size", "<Control>0" },

    /* "File" menu */
    { "win.file.new", "<Control>N" }, { "win.file.new-window", "<Control><Shift>N" },
    { "win.file.open", "<Control>O" }, { "win.file.save", "<Control>S" },
    { "win.file.save-as", "<Control><Shift>S" }, { "win.file.print", "<Control>P" },
    { "win.file.detach-tab", "<Control>D" }, { "win.file.close-tab", "<Control>W" },
    { "win.file.close-window", "<Control><Shift>W" }, { "app.quit", "<Control>Q" },

    /* "Edit" menu */
    { "win.edit.undo", "<Control>Z" }, { "win.edit.redo", "<Control>Y" },
    { "win.edit.cut", "<Control>X" }, { "win.edit.copy", "<Control>C" },
    { "win.edit.paste", "<Control>V" }, { "win.edit.delete-selection", "Delete" },
    { "win.edit.delete-line", "<Control><Shift>Delete" }, { "win.edit.select-all", "<Control>A" },
    { "win.edit.convert.to-opposite-case", "<Alt><Control>U" },
    { "win.edit.convert.transpose", "<Control>T" },
    { "win.edit.move.line-up", "<Alt>Up" }, { "win.edit.move.line-down", "<Alt>Down" },
    { "win.edit.move.word-left", "<Alt>Left" }, { "win.edit.move.word-right", "<Alt>Right" },
    { "win.edit.increase-indent", "<Control>I" }, { "win.edit.decrease-indent", "<Control>U" },

    /* "Search" menu */
    { "win.search.find", "<Control>F" }, { "win.search.find-next", "<Control>G" },
    { "win.search.find-previous", "<Control><Shift>G" },
    { "win.search.find-and-replace", "<Control>R" },
    { "win.search.go-to", "<Control>L" },

    /* "View" menu */
    { "win.preferences.window.menubar-visible", "<Control>M" }, { "win.view.fullscreen", "F11" },

    /* "Document" menu */
    { "win.document.previous-tab", "<Control>Page_Up" },
    { "win.document.next-tab", "<Control>Page_Down" },
    { "win.document.go-to-tab(0)", "<Alt>1" }, { "win.document.go-to-tab(1)", "<Alt>2" },
    { "win.document.go-to-tab(2)", "<Alt>3" }, { "win.document.go-to-tab(3)", "<Alt>4" },
    { "win.document.go-to-tab(4)", "<Alt>5" }, { "win.document.go-to-tab(5)", "<Alt>6" },
    { "win.document.go-to-tab(6)", "<Alt>7" }, { "win.document.go-to-tab(7)", "<Alt>8" },
    { "win.document.go-to-tab(8)", "<Alt>9" },

    /* "Help" menu */
    { "win.help.contents", "F1" }
  };

  /* actions that have a default accel */
  accel_map = gtk_accel_map_get ();
  for (n = 0; n < G_N_ELEMENTS (accel_maps); n++)
    {
      /* add accel map entry to fill the accels file at shutdown */
      accel_path = g_strconcat ("<Actions>/", accel_maps[n][0], NULL);
      gtk_accelerator_parse (accel_maps[n][1], &accel_key, &accel_mods);
      gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);
      g_free (accel_path);

      /* add accel to the application */
      accels[0] = accel_maps[n][1];
      gtk_application_set_accels_for_action (GTK_APPLICATION (application),
                                             accel_maps[n][0], accels);

      /* store default accel for future reference */
      mousepad_object_set_data_full (accel_map, g_intern_string (accel_maps[n][0]),
                                     g_strdup (accel_maps[n][1]), g_free);
    }

  /* plugin enabling actions */
  for (item = application->providers; item != NULL; item = item->next)
    {
      if ((accel = mousepad_plugin_provider_get_accel (item->data)) == NULL)
        continue;

      /* add accel map entry to fill the accels file at shutdown */
      provider = item->data;
      accel_path = g_strconcat ("<Actions>/app.", provider->name, NULL);
      gtk_accelerator_parse (accel, &accel_key, &accel_mods);
      gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);

      /* add accel to the application */
      accels[0] = accel;
      gtk_application_set_accels_for_action (GTK_APPLICATION (application),
                                             accel_path + 10, accels);
      g_free (accel_path);
    }

  /* actions that do not have a default accel */
  action_names = g_action_group_list_actions (G_ACTION_GROUP (application));
  n = 0;
  while (action_names[n] != NULL)
    {
      /* add accel map entry to fill the accels file at shutdown */
      accel_path = g_strconcat ("<Actions>/app.", action_names[n], NULL);
      if (! gtk_accel_map_lookup_entry (accel_path, NULL))
        gtk_accel_map_add_entry (accel_path, 0, 0);

      g_free (accel_path);
      n++;
    }

  g_strfreev (action_names);

  /* we will complete the accel map with window actions when the first window is set */
  g_signal_connect (application, "notify::active-window",
                    G_CALLBACK (mousepad_application_complete_accel_map), NULL);
}



static void
mousepad_application_opening_mode_changed (MousepadApplication *application)
{
  application->opening_mode = MOUSEPAD_SETTING_GET_ENUM (OPENING_MODE);
}



static gint
mousepad_application_sort_plugins (gconstpointer a,
                                   gconstpointer b)
{
  MousepadPluginProvider *p = (MousepadPluginProvider *) a,
                         *q = (MousepadPluginProvider *) b;
  gint comp_cat;

  comp_cat = g_utf8_collate (mousepad_plugin_provider_get_category (p),
                             mousepad_plugin_provider_get_category (q));

  /* alphabetical sorting of categories, and within categories of labels */
  if (comp_cat == 0)
    return g_utf8_collate (mousepad_plugin_provider_get_label (p),
                           mousepad_plugin_provider_get_label (q));
  else
    return comp_cat;
}



static void
mousepad_application_load_plugins (MousepadApplication *application)
{
  MousepadPluginProvider  *provider;
  GSimpleAction           *action;
  GError                  *error = NULL;
  GDir                    *dir;
  const gchar             *basename;
  gchar                   *provider_name, *schema_id;
  gchar                  **strs;
  gsize                    n_strs;

  if (! g_module_supported ())
    {
      g_warning ("Dynamic type loading is not supported on this system");
      return;
    }
  else if ((dir = g_dir_open (MOUSEPAD_PLUGIN_DIRECTORY, 0, &error)) == NULL)
    {
      /* the plugin directory may not exist (compilation without plugin) */
      if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_message ("Plugin directory '%s' not found", MOUSEPAD_PLUGIN_DIRECTORY);
      else
        g_warning ("Failed to open plugin directory '%s': %s",
                   MOUSEPAD_PLUGIN_DIRECTORY, error->message);

      return;
    }

  /* scan plugin directory */
  for (basename = g_dir_read_name (dir); basename != NULL; basename = g_dir_read_name (dir))
    {
      if (! g_str_has_prefix (basename, "lib")
          || ! g_str_has_suffix (basename, "." G_MODULE_SUFFIX))
        continue;

      /* remove prefix */
      provider_name = (gchar *) (basename + 3);

      /* remove suffix */
      strs = g_strsplit (provider_name, ".", -1);
      n_strs = g_strv_length (strs);
      g_free (strs[n_strs - 1]);
      strs[n_strs - 1] = NULL;
      provider_name = g_strjoinv (".", strs);
      g_strfreev (strs);

      /* get the list of enabled plugins */
      strs = MOUSEPAD_SETTING_GET_STRV (ENABLED_PLUGINS);

      /* try to load this module */
      provider = mousepad_plugin_provider_new (provider_name);
      if (g_type_module_use (G_TYPE_MODULE (provider)))
        {
          /* add this provider to the list */
          application->providers = g_list_prepend (application->providers, provider);

          /* create its action and add it to the application */
          action = g_simple_action_new_stateful (provider_name, NULL,
                                                 g_variant_new_boolean (FALSE));
          g_signal_connect (action, "activate",
                            G_CALLBACK (mousepad_application_plugin_activate), application);
          MOUSEPAD_SETTING_CONNECT_OBJECT (ENABLED_PLUGINS, mousepad_application_plugin_update,
                                           application, G_CONNECT_SWAPPED);
          g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));

          /* add its settings to the setting store */
          if (g_str_has_prefix (provider_name, "mousepad-plugin-"))
            schema_id = provider_name + 16;
          else
            schema_id = provider_name;

          schema_id = g_strconcat (MOUSEPAD_ID, ".plugins.", schema_id, NULL);
          mousepad_settings_add_root (schema_id);
          g_free (schema_id);

          /* instantiate this provider types and initialize its action state */
          if (g_strv_contains ((const gchar *const *) strs, provider_name))
            {
              mousepad_plugin_provider_new_plugin (provider);
              g_simple_action_set_state (action, g_variant_new_boolean (TRUE));
            }
          else
            g_type_module_unuse (G_TYPE_MODULE (provider));
        }
      else
        g_object_unref (provider);

      /* cleanup */
      g_strfreev (strs);
      g_free (provider_name);
    }

  /* ends scan */
  g_dir_close (dir);

  /* sort the list */
  application->providers = g_list_sort (application->providers,
                                        mousepad_application_sort_plugins);
}



static void
mousepad_application_startup (GApplication *gapplication)
{
  static GSettings *settings;

  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GSettingsSchema     *schema;
  GVariant            *state;
  GAction             *action;
  GMenu               *menu;
  guint                m, n;

  /* chain up to parent */
  G_APPLICATION_CLASS (mousepad_application_parent_class)->startup (gapplication);

  /* load plugins */
  mousepad_application_load_plugins (application);

  /* bind the default font to GNOME settings if possible */
  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gnome.desktop.interface", TRUE);
  if (schema != NULL)
    {
      if (g_settings_schema_has_key (schema, "monospace-font-name"))
        {
          settings = g_settings_new ("org.gnome.desktop.interface");
          g_settings_bind (settings, "monospace-font-name",
                           application, "default-font", G_SETTINGS_BIND_GET);
        }

      g_settings_schema_unref (schema);
    }

  /* keep opening mode in sync for the open dialog */
  MOUSEPAD_SETTING_CONNECT_OBJECT (OPENING_MODE, mousepad_application_opening_mode_changed,
                                   application, G_CONNECT_SWAPPED);

  /* add application actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application), stateless_actions,
                                   N_STATELESS, application);
  for (m = 0; m < N_SETTING; m++)
    g_action_map_add_action_entries (G_ACTION_MAP (application), setting_actions[m],
                                     n_setting_actions[m], application);

  /* associate flags to whitespace actions: to do after mapping and before initialization */
  for (m = 0; m < N_WHITESPACE; m++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (application), whitespace_actions[m].name);
      mousepad_object_set_data (action, "flag", GUINT_TO_POINTER (1 << m));
    }

  for (m = 0; m < N_SETTING; m++)
    for (n = 0; n < n_setting_actions[m]; n++)
      {
        /* sync the action state to its setting */
        mousepad_setting_connect_object (setting_actions[m][n].name,
                                         G_CALLBACK (mousepad_application_action_update),
                                         application, G_CONNECT_SWAPPED);

        /* initialize the action state */
        state = mousepad_setting_get_variant (setting_actions[m][n].name);
        g_action_group_change_action_state (G_ACTION_GROUP (application),
                                            setting_actions[m][n].name, state);
        g_variant_unref (state);
      }

  /* set shared menu parts in the application menus */
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "shared-sections");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "tab-menu");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "textview-menu");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "toolbar");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "menubar");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  /* set accels for actions */
  mousepad_application_set_accels (application);

  /* add some static submenus to the application menubar */
  mousepad_application_create_languages_menu (application);
  mousepad_application_create_style_schemes_menu (application);

  /* do some actions when the active window changes */
  g_signal_connect (application, "notify::active-window",
                    G_CALLBACK (mousepad_application_active_window_changed), NULL);

  /* initialize history management */
  mousepad_history_init ();
}



static gint
mousepad_application_command_line (GApplication            *gapplication,
                                   GApplicationCommandLine *command_line)
{
  MousepadApplication  *application = MOUSEPAD_APPLICATION (gapplication);
  GVariantDict         *options;
  GFile               **files;
  GFile                *file;
  const gchar          *opening_mode;
  const gchar         **filenames = NULL;
  gint                  n, n_files;
  gboolean              user_set_encoding, user_set_cursor = FALSE, restored = FALSE;

  /* get the option dictionary */
  options = g_application_command_line_get_options_dict (command_line);

  /* see if the prefs dialog is to be opened */
  if (g_variant_dict_contains (options, "preferences"))
    {
      /* create and show the prefs dialog */
      g_action_group_activate_action (G_ACTION_GROUP (gapplication), "preferences", NULL);

      /* increase application use count to keep the prefs dialog alive */
      if (! application->prefs_dialog_standalone)
        {
          g_application_hold (gapplication);
          application->prefs_dialog_standalone = TRUE;
        }

      return EXIT_SUCCESS;
    }

  /* restore previous session */
  if (MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE) != MOUSEPAD_SESSION_RESTORE_NEVER
      && ! g_application_command_line_get_is_remote (command_line))
    {
      application->opening_mode = MIXED;
      application->encoding = mousepad_encoding_get_default ();
      restored = mousepad_history_session_restore (application);
    }

  /* retrieve encoding from the remote instance */
  g_variant_dict_lookup (options, "encoding", "u", &(application->encoding));
  g_variant_dict_lookup (options, "user-set-encoding", "b", &user_set_encoding);

  /* see if an opening mode was provided on the command line */
  if (g_variant_dict_lookup (options, "opening-mode", "&s", &opening_mode))
    {
      if (g_strcmp0 (opening_mode, "tab") == 0)
        application->opening_mode = TAB;
      else if (g_strcmp0 (opening_mode, "window") == 0)
        application->opening_mode = WINDOW;
      else if (g_strcmp0 (opening_mode, "mixed") == 0)
        application->opening_mode = MIXED;
      else
        {
          application->opening_mode = MOUSEPAD_SETTING_GET_ENUM (OPENING_MODE);
          g_application_command_line_printerr (command_line,
                                               "Invalid opening mode '%s': ignored\n",
                                               opening_mode);
        }
    }
  /* use the opening mode stored in the settings */
  else
    application->opening_mode = MOUSEPAD_SETTING_GET_ENUM (OPENING_MODE);

  /* see if line number was not provided on the command line */
  if (! g_variant_dict_lookup (options, "line", "i", &(application->line)))
    application->line = 0;
  else
    {
      user_set_cursor = TRUE;

      /* for user line starts from 1 but for gtk line starts from 0 */
      if (application->line > 0)
        --application->line;
    }

  /* see if column number was not provided on the command line */
  if (! g_variant_dict_lookup (options, "column", "i", &(application->column)))
    application->column = 0;
  else
    user_set_cursor = TRUE;

  /* extract filenames */
  g_variant_dict_lookup (options, G_OPTION_REMAINING, "^a&ay", &filenames);

  /* open files provided on the command line */
  if (filenames != NULL && (n_files = g_strv_length ((gchar **) filenames)) > 0)
    {
      /* prepare the GFile array */
      files = g_new (GFile *, n_files);
      for (n = 0; n < n_files; n++)
        {
          file = g_application_command_line_create_file_for_arg (command_line, filenames[n]);
          mousepad_object_set_data (file, "user-set-encoding",
                                    GINT_TO_POINTER (user_set_encoding));
          mousepad_object_set_data (file, "user-set-cursor",
                                    GINT_TO_POINTER (user_set_cursor));
          files[n] = file;
        }

      /* open the files */
      g_application_open (gapplication, files, n_files, NULL);

      /* cleanup */
      for (n = 0; n < n_files; n++)
        g_object_unref (files[n]);

      g_free (files);
    }
  /* open an empty document if previous session wasn't restored */
  else if (! restored)
    g_application_activate (gapplication);

  /* restore opening mode from the settings if it was overridden from the command line */
  application->opening_mode = MOUSEPAD_SETTING_GET_ENUM (OPENING_MODE);

  /* cleanup */
  g_free (filenames);

  return EXIT_SUCCESS;
}



static GtkWidget *
mousepad_application_get_window_for_files (MousepadApplication *application)
{
  GtkWidget *window = NULL;

  /* when opening mode is "tab", retrieve the last active MousepadWindow, if any */
  if (application->opening_mode == TAB)
    window = GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (application)));

  /* create a new window (signals added and already hooked up) if needed */
  if (window == NULL)
    window = mousepad_application_create_window (application);

  return window;
}



static void
mousepad_application_activate (GApplication *gapplication)
{
  GtkWidget *window;

  /* get the window to open a new document */
  window = mousepad_application_get_window_for_files (MOUSEPAD_APPLICATION (gapplication));

  /* create a new document and add it to the window */
  g_action_group_activate_action (G_ACTION_GROUP (window), "file.new", NULL);

  /* present the window, so that it requires attention if already open */
  gtk_window_present (GTK_WINDOW (window));
}



static void
mousepad_application_open (GApplication  *gapplication,
                           GFile        **files,
                           gint           n_files,
                           const gchar   *hint)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GtkWidget           *window;
  gint                 n, opened = 0;

  /* open the files in tabs */
  if (application->opening_mode != WINDOW)
    {
      /* get the window to open the files */
      window = mousepad_application_get_window_for_files (application);

      /* open the files */
      opened = mousepad_window_open_files (MOUSEPAD_WINDOW (window), files, n_files,
                                           application->encoding, application->line,
                                           application->column, FALSE);

      /* if at least one file was finally opened, present the window, so that it requires
       * attention if already open */
      if (opened > 0)
        gtk_window_present (GTK_WINDOW (window));
      /* destroy the window if it was not already destroyed, e.g. by "app.quit" */
      else if (G_LIKELY (mousepad_is_application_window (window)) && opened < 0)
        gtk_widget_destroy (window);
    }
  /* open the files in windows */
  else
    {
      for (n = 0; n < n_files; n++)
        {
          /* create a new window (signals added and already hooked up) */
          window = mousepad_application_create_window (application);

          /* open the file */
          opened = mousepad_window_open_files (MOUSEPAD_WINDOW (window), files + n, 1,
                                               application->encoding, application->line,
                                               application->column, FALSE);

          /* if the file was finally opened, show the window */
          if (opened > 0)
            gtk_widget_show (window);
          /* destroy the window if it was not already destroyed, e.g. by "app.quit" */
          else if (G_LIKELY (mousepad_is_application_window (window)))
            gtk_widget_destroy (window);
        }
    }
}



static void
mousepad_application_shutdown (GApplication *gapplication)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GList               *windows, *window;
  gchar               *filename;

  /* finalize history management */
  mousepad_history_finalize ();

  /* destroy the preferences dialog */
  if (application->prefs_dialog != NULL)
    gtk_widget_destroy (application->prefs_dialog);

  /* destroy the windows if they are still opened */
  windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (application)));
  for (window = windows; window != NULL; window = window->next)
    gtk_widget_destroy (window->data);

  g_list_free (windows);

  /* unload plugins */
  g_list_free_full (application->providers, mousepad_plugin_provider_unuse);

  /* release property related variables */
  g_free (application->default_font);

  /* save the current accel map */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, TRUE);
  if (G_LIKELY (filename != NULL))
    {
      gtk_accel_map_save (filename);
      g_free (filename);
    }

  /* finalize mousepad settings */
  mousepad_settings_finalize ();

  /* chain up to parent */
  G_APPLICATION_CLASS (mousepad_application_parent_class)->shutdown (gapplication);
}



static GtkWidget *
mousepad_application_create_window (MousepadApplication *application)
{
  GtkWindowGroup *window_group;
  GtkWidget      *window, *notebook;

  /* create a new window */
  window = mousepad_window_new (application);

  /* add window to own window group so that grabs only affect parent window */
  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

  /* place the window on the right screen */
  gtk_window_set_screen (GTK_WINDOW (window), gdk_screen_get_default ());

  /* connect signals */
  g_signal_connect (window, "new-window-with-document",
                    G_CALLBACK (mousepad_application_new_window_with_document), application);
  g_signal_connect (window, "new-window",
                    G_CALLBACK (mousepad_application_new_window), application);
  notebook = mousepad_window_get_notebook (MOUSEPAD_WINDOW (window));
  g_signal_connect_after (notebook, "switch-page",
                          G_CALLBACK (mousepad_history_session_save), NULL);
  g_signal_connect_after (notebook, "page-reordered",
                          G_CALLBACK (mousepad_history_session_save), NULL);

  return window;
}



static void
mousepad_application_new_window_with_document (MousepadWindow      *existing,
                                               MousepadDocument    *document,
                                               gint                 x,
                                               gint                 y,
                                               MousepadApplication *application)
{
  GtkWidget *window;
  GdkScreen *screen;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (existing));
  g_return_if_fail (document == NULL || MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_APPLICATION (application));

  /* create a new window (signals added and already hooked up) */
  window = mousepad_application_create_window (application);

  /* place the new window on the same screen as the existing window */
  screen = gtk_window_get_screen (GTK_WINDOW (existing));
  if (G_LIKELY (screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (window), screen);

  /* move the window on valid cooridinates */
  if (x > -1 && y > -1)
    gtk_window_move (GTK_WINDOW (window), x, y);

  /* create an empty document if no document was send */
  if (document == NULL)
    document = mousepad_document_new ();

  /* add the document to the new window */
  mousepad_window_add (MOUSEPAD_WINDOW (window), document);

  /* show the window */
  gtk_widget_show (window);
}



static void
mousepad_application_new_window (MousepadWindow      *existing,
                                 MousepadApplication *application)
{
  /* trigger new document function */
  mousepad_application_new_window_with_document (existing, NULL, -1, -1, application);
}



static void
mousepad_application_active_window_changed (MousepadApplication *application)
{
  GList        *app_windows;
  static GList *windows = NULL;

  /* get the application windows list */
  app_windows = gtk_application_get_windows (GTK_APPLICATION (application));

  /* filter false change and window additions */
  if (windows != NULL && app_windows != NULL && windows->data != app_windows->data
      && g_list_find (windows, app_windows->data))
    {
      /* update document dependent menu items */
      mousepad_window_update_document_menu_items (app_windows->data);

      /* update window dependent menu items */
      mousepad_window_update_window_menu_items (app_windows->data);

      /* save new session state */
      mousepad_history_session_save ();
    }

  /* store a copy of the application windows list to compare at next call */
  g_list_free (windows);
  windows = g_list_copy (app_windows);
}



static void
mousepad_application_update_menu (GMenuModel *shared_menu,
                                  gint        position,
                                  gint        removed,
                                  gint        added,
                                  GMenuModel *model)
{
  GMenuItem *item;
  gint       n, index;

  /* update the target menu when shared menu items are added or removed: some of them may
   * be temporarily added/removed during an update process, of permanently added/removed */
  for (n = 0; n < removed; n++)
    {
      index = position + n;
      item = g_menu_item_new_from_model (shared_menu, index);
      g_menu_remove (G_MENU (model), index);
      g_object_unref (item);
    }

  for (n = 0; n < added; n++)
    {
      index = position + n;
      item = g_menu_item_new_from_model (shared_menu, index);
      g_menu_insert_item (G_MENU (model), index, item);
      g_object_unref (item);
    }
}



static void
mousepad_application_update_menu_item (GMenuModel *shared_item,
                                       gint        position,
                                       gint        removed,
                                       gint        added,
                                       GMenuModel *model)
{
  GMenuItem *item;
  GVariant  *value;
  gint       n;

  /* update the target menu item only when the shared menu item is added, that is at the end
   * of its own update: an isolated shared menu item is not supposed to be permanently removed */
  if (added)
    {
      value = g_menu_model_get_item_attribute_value (shared_item, 0, "item-share-id",
                                                     G_VARIANT_TYPE_STRING);
      n = GPOINTER_TO_INT (mousepad_object_get_data (model, g_variant_get_string (value, NULL)));
      g_variant_unref (value);

      item = g_menu_item_new_from_model (shared_item, 0);
      g_menu_remove (G_MENU (model), n);
      g_menu_insert_item (G_MENU (model), n, item);
      g_object_unref (item);
    }
}



static void
mousepad_application_set_shared_menu_parts (MousepadApplication *application,
                                            GMenuModel          *model)
{
  GMenuModel  *section, *shared_menu, *shared_item, *submodel;
  GVariant    *value;
  const gchar *share_id;
  gint         n;

  for (n = 0; n < g_menu_model_get_n_items (model); n++)
    {
      /* section GMenuItem with a share id: insert the shared menu */
      if ((section = g_menu_model_get_item_link (model, n, G_MENU_LINK_SECTION))
          && ((value = g_menu_model_get_item_attribute_value (model, n, "section-share-id",
                                                              G_VARIANT_TYPE_STRING))))
        {
          share_id = g_variant_get_string (value, NULL);
          g_variant_unref (value);

          shared_menu = G_MENU_MODEL (gtk_application_get_menu_by_id (
                                        GTK_APPLICATION (application), share_id));
          mousepad_application_update_menu (shared_menu, 0, 0,
                                            g_menu_model_get_n_items (shared_menu), section);

          g_signal_connect_object (shared_menu, "items-changed",
                                   G_CALLBACK (mousepad_application_update_menu), section, 0);
        }
      /* section GMenuItem without a share id: go ahead recursively */
      else if (section != NULL)
        mousepad_application_set_shared_menu_parts (application, section);
      /* real GMenuItem */
      else
        {
          /* set the menu item when shared */
          if ((value = g_menu_model_get_item_attribute_value (model, n, "item-share-id",
                                                              G_VARIANT_TYPE_STRING)))
            {
              share_id = g_variant_get_string (value, NULL);
              g_variant_unref (value);

              shared_item = G_MENU_MODEL (gtk_application_get_menu_by_id (
                                            GTK_APPLICATION (application), share_id));
              mousepad_object_set_data (model, g_intern_string (share_id), GINT_TO_POINTER (n));
              mousepad_application_update_menu_item (shared_item, 0, 0, 1, model);

              g_signal_connect_object (shared_item, "items-changed",
                                       G_CALLBACK (mousepad_application_update_menu_item), model, 0);
            }

          /* submenu GMenuItem with a share id: insert the shared menu */
          if ((submodel = g_menu_model_get_item_link (model, n, G_MENU_LINK_SUBMENU))
              && ((value = g_menu_model_get_item_attribute_value (model, n, "submenu-share-id",
                                                                  G_VARIANT_TYPE_STRING))))
            {
              share_id = g_variant_get_string (value, NULL);
              g_variant_unref (value);

              shared_menu = G_MENU_MODEL (gtk_application_get_menu_by_id (
                                            GTK_APPLICATION (application), share_id));
              mousepad_application_update_menu (shared_menu, 0, 0,
                                                g_menu_model_get_n_items (shared_menu), submodel);

              g_signal_connect_object (shared_menu, "items-changed",
                                       G_CALLBACK (mousepad_application_update_menu), submodel, 0);
            }
          /* submenu GMenuItem without a share id: go ahead recursively */
          else if (submodel != NULL)
            mousepad_application_set_shared_menu_parts (application, submodel);
        }
    }
}



static void
mousepad_application_create_languages_menu (MousepadApplication *application)
{
  GMenu       *menu, *submenu;
  GMenuItem   *item;
  GSList      *sections, *languages, *iter_sect, *iter_lang;
  const gchar *label;
  gchar       *action_name, *tooltip;

  /* get the empty "Filetype" submenu and populate it */
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "document.filetype.list");

  sections = mousepad_util_get_sorted_language_sections ();

  for (iter_sect = sections; iter_sect != NULL; iter_sect = g_slist_next (iter_sect))
    {
      /* create submenu item */
      label = iter_sect->data;
      submenu = g_menu_new ();
      item = g_menu_item_new_submenu (label, G_MENU_MODEL (submenu));

      /* set tooltip and append menu item */
      tooltip = iter_sect->data;
      g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
      g_menu_append_item (menu, item);
      g_object_unref (item);

      languages = mousepad_util_get_sorted_languages_for_section (label);

      for (iter_lang = languages; iter_lang != NULL; iter_lang = g_slist_next (iter_lang))
        {
          /* create menu item */
          label = gtk_source_language_get_id (iter_lang->data);
          action_name = g_strconcat ("win.document.filetype('", label, "')", NULL);
          label = gtk_source_language_get_name (iter_lang->data);
          item = g_menu_item_new (label, action_name);

          /* set tooltip */
          tooltip = g_strdup_printf ("%s/%s", (gchar *) iter_sect->data, label);
          g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));

          /* append menu item */
          g_menu_append_item (submenu, item);

          /* cleanup */
          g_object_unref (item);
          g_free (action_name);
          g_free (tooltip);
        }

      g_slist_free (languages);
    }

  g_slist_free (sections);
}



static void
mousepad_application_create_style_schemes_menu (MousepadApplication *application)
{
  GMenu        *menu;
  GMenuItem    *item;
  GSList       *schemes, *iter;
  const gchar  *label;
  gchar       **authors;
  gchar        *action_name, *author, *tooltip;

  /* get the empty "Color Scheme" submenu and populate it */
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "view.color-scheme.list");

  schemes = mousepad_util_get_sorted_style_schemes ();

  for (iter = schemes; iter != NULL; iter = g_slist_next (iter))
    {
      /* create menu item */
      label = gtk_source_style_scheme_get_id (iter->data);
      action_name = g_strconcat ("app.preferences.view.color-scheme('", label, "')", NULL);
      label = gtk_source_style_scheme_get_name (iter->data);
      item = g_menu_item_new (label, action_name);

      /* set tooltip */
      authors = (gchar**) gtk_source_style_scheme_get_authors (iter->data);
      author = g_strjoinv (", ", authors);
      tooltip = g_strdup_printf (_("%s | Authors: %s | Filename: %s"),
                                 gtk_source_style_scheme_get_description (iter->data),
                                 author,
                                 gtk_source_style_scheme_get_filename (iter->data));
      g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));

      /* append menu item */
      g_menu_append_item (menu, item);

      /* cleanup */
      g_object_unref (item);
      g_free (action_name);
      g_free (author);
      g_free (tooltip);
    }

  g_slist_free (schemes);
}



static void
mousepad_application_prefs_dialog_standalone (MousepadApplication *application)
{
  if (application->prefs_dialog_standalone)
    {
      g_application_release (G_APPLICATION (application));
      application->prefs_dialog_standalone = FALSE;
    }
}



static void
mousepad_application_prefs_dialog_response (MousepadApplication *application,
                                            gint                 response_id,
                                            MousepadPrefsDialog *dialog)
{
  gtk_widget_destroy (application->prefs_dialog);
  application->prefs_dialog = NULL;

  /* decrease application use count, if needed */
  mousepad_application_prefs_dialog_standalone (application);
}



static void
mousepad_application_action_preferences (GSimpleAction *action,
                                         GVariant      *value,
                                         gpointer       data)
{
  MousepadApplication *application = data;

  /* if the dialog isn't already shown, create one */
  if (application->prefs_dialog == NULL)
    {
      application->prefs_dialog = mousepad_prefs_dialog_new ();

      /* destroy the dialog when it's close button is pressed */
      g_signal_connect_swapped (application->prefs_dialog, "response",
                                G_CALLBACK (mousepad_application_prefs_dialog_response),
                                application);
    }

  /* associate it with the active window, if any */
  gtk_window_set_transient_for (GTK_WINDOW (application->prefs_dialog),
                                gtk_application_get_active_window (data));

  /* show it to the user */
  gtk_window_present (GTK_WINDOW (application->prefs_dialog));
}



static void
mousepad_application_action_quit (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  GList   *windows, *window;
  GAction *close;

  /* block session handler */
  mousepad_history_session_set_quitting (TRUE);

  /* try to close all windows, abort at the first failure */
  windows = g_list_copy (gtk_application_get_windows (data));
  for (window = windows; window != NULL; window = window->next)
    {
      close = g_action_map_lookup_action (G_ACTION_MAP (window->data), "file.close-window");
      g_action_activate (close, NULL);
      if (! mousepad_action_get_state_int32_boolean (close))
        {
          /* unblock session handler and save session */
          mousepad_history_session_set_quitting (FALSE);
          mousepad_history_session_save ();

          break;
        }
    }

  g_list_free (windows);

  /* decrease application use count, if needed */
  mousepad_application_prefs_dialog_standalone (data);
}



static void
mousepad_application_toggle_activate (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       data)
{
  gboolean state;

  /* save the setting */
  state = ! mousepad_action_get_state_boolean (G_ACTION (action));
  mousepad_setting_set_boolean (g_action_get_name (G_ACTION (action)), state);
}



static void
mousepad_application_radio_activate (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       data)
{
  /* save the setting */
  mousepad_setting_set_variant (g_action_get_name (G_ACTION (action)), parameter);
}



static void
mousepad_application_plugin_activate (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       data)
{
  gchar       **plugins;
  const gchar  *action_name;
  gboolean      enabled, contained, need_update = FALSE;
  guint         length = 0;

  /* get the new action state */
  enabled = ! mousepad_action_get_state_boolean (G_ACTION (action));

  /* get the list of enabled plugins */
  plugins = MOUSEPAD_SETTING_GET_STRV (ENABLED_PLUGINS);

  /* check if this plugin is in the list */
  action_name = g_action_get_name (G_ACTION (action));
  contained = g_strv_contains ((const gchar *const *) plugins, action_name);

  /* update the list if needed */
  if (enabled && ! contained)
    {
      need_update = TRUE;
      length = g_strv_length (plugins);
      plugins = g_renew (gchar *, plugins, length + 2);
      plugins[length] = g_strdup (action_name);
      plugins[length + 1] = NULL;
    }
  else if (! enabled && contained)
    {
      need_update = TRUE;
      while (g_strcmp0 (plugins[length++], action_name) != 0);
      g_free (plugins[--length]);
      while (plugins[++length] != NULL)
        plugins[length - 1] = plugins[length];

      plugins[length - 1] = NULL;
    }

  /* save the setting */
  if (need_update)
    MOUSEPAD_SETTING_SET_STRV (ENABLED_PLUGINS, (const gchar *const *) plugins);

  /* cleanup */
  g_strfreev (plugins);
}



static void
mousepad_application_action_update (MousepadApplication *application,
                                    gchar               *key,
                                    GSettings           *settings)
{
  GVariant *state;
  gchar    *schema_id, *action_name;

  /* retrieve the action name from the setting schema id */
  g_object_get (settings, "schema_id", &schema_id, NULL);
  action_name = g_strdup_printf ("%s.%s", schema_id + MOUSEPAD_ID_LEN + 1, key);

  /* request for the action state to be changed according to the setting */
  state = mousepad_setting_get_variant (action_name);
  g_action_group_change_action_state (G_ACTION_GROUP (application), action_name, state);

  /* cleanup */
  g_free (schema_id);
  g_free (action_name);
  g_variant_unref (state);
}



static void
mousepad_application_plugin_update (MousepadApplication *application)
{
  MousepadPluginProvider  *mprovider;
  GTypeModule             *provider;
  GAction                 *action;
  GList                   *item;
  gchar                  **plugins;
  gboolean                 enabled, contained, destroyable;

  /* get the list of enabled plugins */
  plugins = MOUSEPAD_SETTING_GET_STRV (ENABLED_PLUGINS);

  /* check for each plugin if its action state should be changed, and if so,
   * if we should do something on the plugin itself from here */
  for (item = application->providers; item != NULL; item = item->next)
    {
      provider = item->data;
      contained = g_strv_contains ((const gchar *const *) plugins, provider->name);
      action = g_action_map_lookup_action (G_ACTION_MAP (application), provider->name);
      enabled = mousepad_action_get_state_boolean (action);
      if ((enabled && ! contained) || (! enabled && contained))
        {
          g_action_change_state (action, g_variant_new_boolean (! enabled));
          mprovider = item->data;
          destroyable = mousepad_plugin_provider_is_destroyable (mprovider);

          /* plugin should be loaded from here */
          if (! enabled && (! mousepad_plugin_provider_is_instantiated (mprovider) || destroyable)
              && g_type_module_use (provider))
            mousepad_plugin_provider_new_plugin (mprovider);
          /* plugin should be unloaded from here */
          else if (enabled && destroyable)
            g_type_module_unuse (provider);
        }
    }

  /* cleanup */
  g_strfreev (plugins);
}



/*
 * This job is done only once here, when whitespace location settings are updated,
 * then all the views update their own location flags from those of the application.
 */
static void
mousepad_application_action_whitespace (GSimpleAction *action,
                                        GVariant      *state,
                                        gpointer       data)
{
  GtkSourceSpaceLocationFlags flags, flag;

  /* change action state */
  g_simple_action_set_state (action, state);

  /* update the space location flags */
  flags = MOUSEPAD_APPLICATION (data)->space_location_flags;
  flag = GPOINTER_TO_UINT (mousepad_object_get_data (action, "flag"));
  g_variant_get_boolean (state) ? (flags |= flag) : (flags &= ~flag);

  /* set the application property */
  g_object_set (data, "space-location", flags, NULL);
}



GList *
mousepad_application_get_providers (MousepadApplication *application)
{
  return application->providers;
}



GtkWidget *
mousepad_application_get_prefs_dialog (MousepadApplication *application)
{
  return application->prefs_dialog;
}
