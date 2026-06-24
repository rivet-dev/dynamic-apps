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

#include <exo-desktop-item-edit/exo-die-command-entry.h>
#include <exo-desktop-item-edit/exo-die-desktop-model.h>
#include <exo-desktop-item-edit/exo-die-editor.h>
#include <libxfce4util/libxfce4util.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_COMPLETE,
  PROP_MODE,
  PROP_NAME,
  PROP_COMMENT,
  PROP_COMMAND,
  PROP_URL,
  PROP_ICON,
  PROP_PATH,
  PROP_SNOTIFY,
  PROP_TERMINAL,
};



static void     exo_die_editor_finalize       (GObject            *object);
static void     exo_die_editor_get_property   (GObject            *object,
                                               guint               prop_id,
                                               GValue             *value,
                                               GParamSpec         *pspec);
static void     exo_die_editor_set_property   (GObject            *object,
                                               guint               prop_id,
                                               const GValue       *value,
                                               GParamSpec         *pspec);
static void     exo_die_editor_icon_clicked   (GtkWidget          *button,
                                               ExoDieEditor       *editor);
static void     exo_die_editor_path_clicked   (GtkWidget          *button,
                                               ExoDieEditor       *editor);
static gboolean exo_die_editor_match_selected (GtkEntryCompletion *completion,
                                               GtkTreeModel       *model,
                                               GtkTreeIter        *iter,
                                               gpointer            user_data);
static void     exo_die_editor_cell_data_func (GtkCellLayout      *cell_layout,
                                               GtkCellRenderer    *renderer,
                                               GtkTreeModel       *model,
                                               GtkTreeIter        *iter,
                                               gpointer            user_data);



struct _ExoDieEditorClass
{
  GtkGridClass __parent__;
};

struct _ExoDieEditor
{
  GtkGrid         __parent__;
  GtkWidget       *name_entry;
  GtkWidget       *icon_button;
  ExoDieEditorMode mode;
  gchar           *name;
  gchar           *comment;
  gchar           *command;
  gchar           *url;
  gchar           *icon;
  gchar           *path;
  guint            snotify : 1;
  guint            terminal : 1;
};



G_DEFINE_TYPE (ExoDieEditor, exo_die_editor, GTK_TYPE_GRID)



static void
exo_die_editor_class_init (ExoDieEditorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_die_editor_finalize;
  gobject_class->get_property = exo_die_editor_get_property;
  gobject_class->set_property = exo_die_editor_set_property;

  /**
   * ExoDieEditor:complete:
   *
   * %TRUE if the values entered into the #ExoDieEditor are
   * complete for the given mode.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMPLETE,
                                   g_param_spec_boolean ("complete",
                                                         "complete",
                                                         "complete",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

  /**
   * ExoDieEditor:mode:
   *
   * The #ExoDieEditorMode for this editor.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode", "mode", "mode",
                                                      EXO_DIE_TYPE_EDITOR_MODE,
                                                      EXO_DIE_EDITOR_MODE_APPLICATION,
                                                      EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * ExoDieEditor:name:
   *
   * The name of the desktop item edited by this editor.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "name",
                                                        "name",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:comment:
   *
   * The commment of the desktop item edited by this editor.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMENT,
                                   g_param_spec_string ("comment",
                                                        "comment",
                                                        "comment",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:command:
   *
   * The command of the desktop item edited by this editor, only
   * valid if the mode is %EXO_DIE_EDITOR_MODE_APPLICATION.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "command",
                                                        "command",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:url:
   *
   * The URL of the desktop item edited by this editor, only valid
   * if the mode is %EXO_DIE_EDITOR_MODE_LINK.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_URL,
                                   g_param_spec_string ("url",
                                                        "url",
                                                        "url",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:icon:
   *
   * The icon of the desktop item edited by this editor.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        "icon",
                                                        "icon",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:path:
   *
   * The path of the desktop item edited by this editor.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        "path",
                                                        "path",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:snotify:
   *
   * Whether the desktop item edited by this editor should use startup
   * notification, only valid if mode is %EXO_DIE_EDITOR_MODE_APPLICATION.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SNOTIFY,
                                   g_param_spec_boolean ("snotify",
                                                         "snotify",
                                                         "snotify",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ExoDieEditor:terminal:
   *
   * Whether the desktop item edited by this editor should have its command
   * run in a terminal, only valid if mode is %EXO_DIE_EDITOR_MODE_APPLICATION.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TERMINAL,
                                   g_param_spec_boolean ("terminal",
                                                         "terminal",
                                                         "terminal",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static gboolean
exo_die_true_if_application (GBinding     *binding,
                             const GValue *src_value,
                             GValue       *dst_value,
                             gpointer      user_data)
{
  g_value_set_boolean (dst_value, (g_value_get_enum (src_value) == EXO_DIE_EDITOR_MODE_APPLICATION));
  return TRUE;
}



static gboolean
exo_die_true_if_link (GBinding     *binding,
                      const GValue *src_value,
                      GValue       *dst_value,
                      gpointer      user_data)
{
  g_value_set_boolean (dst_value, (g_value_get_enum (src_value) == EXO_DIE_EDITOR_MODE_LINK));
  return TRUE;
}



static void
exo_die_editor_init (ExoDieEditor *editor)
{
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *box;
  gint       row;

  /* start with sane defaults */
  editor->mode = EXO_DIE_EDITOR_MODE_LINK;
  editor->command = g_strdup ("");
  editor->comment = g_strdup ("");
  editor->icon = g_strdup ("");
  editor->name = g_strdup ("");
  editor->url = g_strdup ("");
  editor->path = g_strdup ("");

  /* configure the table */
  gtk_grid_set_column_spacing (GTK_GRID (editor), 12);
  gtk_grid_set_row_spacing (GTK_GRID (editor), 6);

  row = 0;

  /* TRANSLATORS: Label in "Create Launcher"/"Create Link" dialog, make sure to avoid mnemonic conflicts */
  label = gtk_label_new_with_mnemonic (_("_Name:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);
  gtk_widget_show (label);

  editor->name_entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (editor->name_entry), TRUE);
  g_object_bind_property (G_OBJECT (editor), "name", G_OBJECT (editor->name_entry), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (editor), editor->name_entry, 1, row, 1, 1);
  g_object_set (editor->name_entry, "hexpand", TRUE, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), editor->name_entry);
  gtk_widget_show (editor->name_entry);

  row += 1;

  /* TRANSLATORS: Label in "Create Launcher"/"Create Link" dialog, make sure to avoid mnemonic conflicts */
  label = gtk_label_new_with_mnemonic (_("C_omment:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_object_bind_property (G_OBJECT (editor), "comment", G_OBJECT (entry), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (editor), entry, 1, row, 1, 1);
  g_object_set (entry, "hexpand", TRUE, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_show (entry);

  row += 1;

  /* TRANSLATORS: Label in "Create Launcher" dialog, make sure to avoid mnemonic conflicts */
  label = gtk_label_new_with_mnemonic (_("Comm_and:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  g_object_bind_property_full (editor, "mode", label, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);

  entry = exo_die_command_entry_new ();
  g_object_bind_property (G_OBJECT (editor), "command", G_OBJECT (entry), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (editor, "mode", entry, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), entry, 1, row, 1, 1);
  g_object_set (entry, "hexpand", TRUE, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

  row += 1;

  /* TRANSLATORS: Label in "Create Link" dialog, make sure to avoid mnemonic conflicts */
  label = gtk_label_new_with_mnemonic (_("_URL:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  g_object_bind_property_full (editor, "mode", label, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_link, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_object_bind_property (G_OBJECT (editor), "url", G_OBJECT (entry), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (editor, "mode", entry, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_link, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), entry, 1, row, 1, 1);
  g_object_set (entry, "hexpand", TRUE, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

  row += 1;

  /* TRANSLATORS: Label in "Create Launcher" dialog, make sure to avoid mnemonic conflicts */
  label = gtk_label_new_with_mnemonic (_("Working _Directory:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  g_object_bind_property_full (editor, "mode", label, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (editor), box, 1, row, 1, 1);
  g_object_set (box, "hexpand", TRUE, NULL);
  g_object_bind_property_full (editor, "mode", box, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_object_bind_property (G_OBJECT (editor), "path", G_OBJECT (entry), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_show (entry);

  button = gtk_button_new ();
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (exo_die_editor_path_clicked), editor);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  row += 1;

  /* TRANSLATORS: Label in "Create Launcher"/"Create Link" dialog, make sure to avoid mnemonic conflicts */
  label = gtk_label_new_with_mnemonic (_("_Icon:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);
  gtk_widget_show (label);

  editor->icon_button = gtk_button_new ();
  g_signal_connect (G_OBJECT (editor->icon_button), "clicked", G_CALLBACK (exo_die_editor_icon_clicked), editor);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), editor->icon_button);
  gtk_widget_show (editor->icon_button);
  gtk_grid_attach (GTK_GRID (editor), editor->icon_button, 1, row, 1, 1);
  g_object_set (editor->icon_button, "expand", FALSE, "halign", GTK_ALIGN_START, "valign", GTK_ALIGN_CENTER, NULL);

  /* TRANSLATORS: Label for the icon button in "Create Launcher"/"Create Link" dialog if no icon selected */
  label = gtk_label_new (_("No icon"));
  gtk_container_add (GTK_CONTAINER (editor->icon_button), label);
  gtk_widget_show (label);

  row += 1;

  label = gtk_label_new (_("Options:"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_object_set (label, "xalign", 1.0f, "yalign", 0.5f, NULL);
  g_object_bind_property_full (editor, "mode", label, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), label, 0, row, 1, 1);

  /* TRANSLATORS: Check button label in "Create Launcher" dialog, make sure to avoid mnemonic conflicts
   *              and sync your translations with the translations in Thunar and xfce4-panel.
   */
  button = gtk_check_button_new_with_mnemonic (_("Use _startup notification"));
  gtk_widget_set_tooltip_text (button, _("Select this option to enable startup notification when the command "
                                         "is run from the file manager or the menu. Not every application supports "
                                         "startup notification."));
  g_object_bind_property (G_OBJECT (editor), "snotify", G_OBJECT (button), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (editor, "mode", button, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), button, 1, row, 1, 1);
  g_object_set (button, "hexpand", TRUE, NULL);

  row += 1;

  /* TRANSLATORS: Check button label in "Create Launcher" dialog, make sure to avoid mnemonic conflicts
   *              and sync your translations with the translations in Thunar and xfce4-panel.
   */
  button = gtk_check_button_new_with_mnemonic (_("Run in _terminal"));
  gtk_widget_set_tooltip_text (button, _("Select this option to run the command in a terminal window."));
  g_object_bind_property (G_OBJECT (editor), "terminal", G_OBJECT (button), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (editor, "mode", button, "visible",
                               G_BINDING_SYNC_CREATE,
                               exo_die_true_if_application, NULL,
                               NULL, NULL);
  gtk_grid_attach (GTK_GRID (editor), button, 1, row, 1, 1);
  g_object_set (button, "hexpand", TRUE, NULL);
}



static void
exo_die_editor_finalize (GObject *object)
{
  ExoDieEditor *editor = EXO_DIE_EDITOR (object);

  /* cleanup */
  g_free (editor->command);
  g_free (editor->comment);
  g_free (editor->icon);
  g_free (editor->name);
  g_free (editor->url);
  g_free (editor->path);

  (*G_OBJECT_CLASS (exo_die_editor_parent_class)->finalize) (object);
}



static void
exo_die_editor_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ExoDieEditor *editor = EXO_DIE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_COMPLETE:
      g_value_set_boolean (value, exo_die_editor_get_complete (editor));
      break;

    case PROP_MODE:
      g_value_set_enum (value, exo_die_editor_get_mode (editor));
      break;

    case PROP_NAME:
      g_value_set_string (value, exo_die_editor_get_name (editor));
      break;

    case PROP_COMMENT:
      g_value_set_string (value, exo_die_editor_get_comment (editor));
      break;

    case PROP_COMMAND:
      g_value_set_string (value, exo_die_editor_get_command (editor));
      break;

    case PROP_URL:
      g_value_set_string (value, exo_die_editor_get_url (editor));
      break;

    case PROP_PATH:
      g_value_set_string (value, exo_die_editor_get_path (editor));
      break;

    case PROP_ICON:
      g_value_set_string (value, exo_die_editor_get_icon (editor));
      break;

    case PROP_SNOTIFY:
      g_value_set_boolean (value, exo_die_editor_get_snotify (editor));
      break;

    case PROP_TERMINAL:
      g_value_set_boolean (value, exo_die_editor_get_terminal (editor));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_die_editor_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ExoDieEditor *editor = EXO_DIE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_MODE:
      exo_die_editor_set_mode (editor, g_value_get_enum (value));
      break;

    case PROP_NAME:
      exo_die_editor_set_name (editor, g_value_get_string (value));
      break;

    case PROP_COMMENT:
      exo_die_editor_set_comment (editor, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      exo_die_editor_set_command (editor, g_value_get_string (value));
      break;

    case PROP_URL:
      exo_die_editor_set_url (editor, g_value_get_string (value));
      break;

    case PROP_PATH:
      exo_die_editor_set_path (editor, g_value_get_string (value));
      break;

    case PROP_ICON:
      exo_die_editor_set_icon (editor, g_value_get_string (value));
      break;

    case PROP_SNOTIFY:
      exo_die_editor_set_snotify (editor, g_value_get_boolean (value));
      break;

    case PROP_TERMINAL:
      exo_die_editor_set_terminal (editor, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_die_editor_icon_clicked (GtkWidget    *button,
                             ExoDieEditor *editor)
{
  GtkWidget *toplevel;
  GtkWidget *chooser;
  gchar     *icon;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));

  /* determine the toplevel widget */
  toplevel = gtk_widget_get_toplevel (button);
  if (toplevel == NULL || !gtk_widget_is_toplevel (toplevel))
    return;

  /* allocate the icon chooser dialog */
  chooser = exo_icon_chooser_dialog_new (_("Select an icon"),
                                         GTK_WINDOW (toplevel),
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_OK"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);

  /* check if we have an icon to set for the chooser */
  if (G_LIKELY (!xfce_str_is_empty (editor->icon)))
    exo_icon_chooser_dialog_set_icon (EXO_ICON_CHOOSER_DIALOG (chooser), editor->icon);

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      /* remember the selected icon from the chooser */
      icon = exo_icon_chooser_dialog_get_icon (EXO_ICON_CHOOSER_DIALOG (chooser));
      exo_die_editor_set_icon (editor, icon);
      g_free (icon);
    }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}



static void
exo_die_editor_path_clicked (GtkWidget    *button,
                             ExoDieEditor *editor)
{
  GtkWidget *toplevel;
  GtkWidget *chooser;
  gchar     *path;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));

  /* determine the toplevel widget */
  toplevel = gtk_widget_get_toplevel (button);
  if (toplevel == NULL || !gtk_widget_is_toplevel (toplevel))
    return;

  /* allocate the file chooser dialog */
  chooser = gtk_file_chooser_dialog_new (_("Select a working directory"),
                                         GTK_WINDOW (toplevel),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_OK"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);

  /* check if we have a path to set for the chooser */
  if (G_LIKELY (!xfce_str_is_empty (editor->path)))
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), editor->path);

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      /* remember the selected path from the chooser */
      path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      exo_die_editor_set_path (editor, path);
      g_free (path);
    }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}



static gboolean
exo_die_editor_match_selected (GtkEntryCompletion *completion,
                               GtkTreeModel       *model,
                               GtkTreeIter        *iter,
                               gpointer            user_data)
{
  ExoDieEditor *editor = EXO_DIE_EDITOR (user_data);
  gboolean      terminal;
  gboolean      snotify;
  gchar        *comment;
  gchar        *command;
  gchar        *icon;
  gchar        *name;

  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

  /* determine the attributes for the selected row */
  gtk_tree_model_get (model, iter,
                      EXO_DIE_DESKTOP_MODEL_COLUMN_COMMENT, &comment,
                      EXO_DIE_DESKTOP_MODEL_COLUMN_COMMAND, &command,
                      EXO_DIE_DESKTOP_MODEL_COLUMN_ICON, &icon,
                      EXO_DIE_DESKTOP_MODEL_COLUMN_NAME, &name,
                      EXO_DIE_DESKTOP_MODEL_COLUMN_SNOTIFY, &snotify,
                      EXO_DIE_DESKTOP_MODEL_COLUMN_TERMINAL, &terminal,
                      -1);

  /* apply the settings to the editor */
  exo_die_editor_set_name (editor, name);
  exo_die_editor_set_comment (editor, (comment != NULL) ? comment : "");
  exo_die_editor_set_command (editor, command);
  exo_die_editor_set_icon (editor, (icon != NULL) ? icon : "");
  exo_die_editor_set_snotify (editor, snotify);
  exo_die_editor_set_terminal (editor, terminal);
  exo_die_editor_set_path (editor, "");

  /* cleanup */
  g_free (comment);
  g_free (command);
  g_free (icon);
  g_free (name);

  return TRUE;
}



static void
exo_die_editor_cell_data_func (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *renderer,
                               GtkTreeModel    *model,
                               GtkTreeIter     *iter,
                               gpointer         user_data)
{
  ExoDieEditor *editor = EXO_DIE_EDITOR (user_data);
  GtkIconTheme *icon_theme;
  GdkPixbuf    *pixbuf_scaled;
  GdkPixbuf    *pixbuf = NULL;
  gchar        *icon;
  gint          pixbuf_width;
  gint          pixbuf_height;

  /* determine the icon for the row */
  gtk_tree_model_get (model, iter, EXO_DIE_DESKTOP_MODEL_COLUMN_ICON, &icon, -1);

  /* check the icon depending on the type */
  if (icon != NULL && g_path_is_absolute (icon))
    {
      /* try to load the icon from the file */
      pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
    }
  else if (!xfce_str_is_empty (icon))
    {
      /* determine the appropriate icon theme */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (editor)));

      /* try to load the named icon */
      pixbuf = gtk_icon_theme_load_icon (icon_theme, icon, 16, 0, NULL);
    }

  /* setup the icon button */
  if (G_LIKELY (pixbuf != NULL))
    {
      /* scale down the icon if required */
      pixbuf_width = gdk_pixbuf_get_width (pixbuf);
      pixbuf_height = gdk_pixbuf_get_height (pixbuf);
      if (G_UNLIKELY (pixbuf_width > 16 || pixbuf_height > 16))
        {
          pixbuf_scaled = exo_gdk_pixbuf_scale_ratio (pixbuf, 16);
          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = pixbuf_scaled;
        }
    }

  /* setup the pixbuf for the renderer */
  g_object_set (G_OBJECT (renderer), "pixbuf", pixbuf, NULL);

  /* cleanup */
  if (G_LIKELY (pixbuf != NULL))
    g_object_unref (G_OBJECT (pixbuf));
  g_free (icon);
}



/**
 * exo_die_editor_new:
 *
 * Allocates a new #ExoDieEditor instance.
 *
 * Return value: the newly allocated #ExoDieEditor.
 **/
GtkWidget*
exo_die_editor_new (void)
{
  return g_object_new (EXO_DIE_TYPE_EDITOR, NULL);
}



/**
 * exo_die_dialog_get_complete:
 * @editor : an #ExoDieEditor.
 *
 * Returns %TRUE if the values entered into the
 * @editor are complete.
 *
 * Return value: if @editor<!---->s values are
 *               complete.
 **/
gboolean
exo_die_editor_get_complete (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), FALSE);

  /* the exact meaning of complete depends on the mode */
  switch (editor->mode)
    {
    case EXO_DIE_EDITOR_MODE_APPLICATION:
      return (!xfce_str_is_empty (editor->name)
              && !xfce_str_is_empty (editor->command));

    case EXO_DIE_EDITOR_MODE_LINK:
      return (!xfce_str_is_empty (editor->name)
              && !xfce_str_is_empty (editor->url));

    case EXO_DIE_EDITOR_MODE_DIRECTORY:
      return !xfce_str_is_empty (editor->name);

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}



/**
 * exo_die_editor_get_mode:
 * @editor : an #ExoDieEditor.
 *
 * Returns the #ExoDieEditorMode for @editor.
 *
 * Return value: the #ExoDieEditorMode for @editor.
 **/
ExoDieEditorMode
exo_die_editor_get_mode (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), EXO_DIE_EDITOR_MODE_APPLICATION);
  return editor->mode;
}



/**
 * exo_die_editor_set_mode:
 * @editor : an #ExoDieEditor.
 * @mode   : an #ExoDieEditorMode.
 *
 * Sets the mode for @editor to @mode.
 **/
void
exo_die_editor_set_mode (ExoDieEditor    *editor,
                         ExoDieEditorMode mode)
{
  ExoDieDesktopModel *desktop_model;
  GtkEntryCompletion *completion;
  GtkCellRenderer    *renderer;

  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));

  /* check if we have a new mode here */
  if (G_LIKELY (editor->mode != mode))
    {
      /* apply the new mode */
      editor->mode = mode;

      /* enable name completion based on the mode */
      if (mode == EXO_DIE_EDITOR_MODE_APPLICATION)
        {
          /* allocate a new completion for the name entry */
          completion = gtk_entry_completion_new ();
          gtk_entry_completion_set_inline_completion (completion, TRUE);
          gtk_entry_completion_set_minimum_key_length (completion, 3);
          gtk_entry_completion_set_popup_completion (completion, TRUE);
          g_signal_connect (G_OBJECT (completion), "match-selected", G_CALLBACK (exo_die_editor_match_selected), editor);
          gtk_entry_set_completion (GTK_ENTRY (editor->name_entry), completion);
          g_object_unref (G_OBJECT (completion));

          /* allocate the desktop application model */
          desktop_model = exo_die_desktop_model_new ();
          gtk_entry_completion_set_match_func (completion, exo_die_desktop_model_match_func, desktop_model, NULL);
          gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (desktop_model));
          g_object_unref (G_OBJECT (desktop_model));

          /* add the icon renderer */
          renderer = gtk_cell_renderer_pixbuf_new ();
          gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, FALSE);
          gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion), renderer, exo_die_editor_cell_data_func, editor, NULL);

          /* add the text renderer */
          renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
          gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, TRUE);
          gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (completion), renderer,
                                          "markup", EXO_DIE_DESKTOP_MODEL_COLUMN_ABSTRACT,
                                          NULL);
        }
      else
        {
          /* completion is disabled for links */
          gtk_entry_set_completion (GTK_ENTRY (editor->name_entry), NULL);
        }

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "complete");
      g_object_notify (G_OBJECT (editor), "mode");
    }
}



/**
 * exo_die_editor_get_name:
 * @editor : an #ExoDieEditor.
 *
 * Returns the name for the @editor.
 *
 * Return value: the name for the @editor.
 **/
const gchar*
exo_die_editor_get_name (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), NULL);
  return editor->name;
}



/**
 * exo_die_editor_set_name:
 * @editor : an #ExoDieEditor.
 * @name   : the new name for @editor.
 *
 * Sets the name for @editor to @name.
 **/
void
exo_die_editor_set_name (ExoDieEditor *editor,
                         const gchar  *name)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));
  g_return_if_fail (g_utf8_validate (name, -1, NULL));

  /* check if we have a new name */
  if (g_strcmp0 (editor->name, name) != 0)
    {
      /* apply the new name */
      g_free (editor->name);
      editor->name = g_strdup (name);

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "complete");
      g_object_notify (G_OBJECT (editor), "name");
    }
}



/**
 * exo_die_editor_get_comment:
 * @editor : an #ExoDieEditor.
 *
 * Returns the comment for @editor.
 *
 * Return value: the comment for @editor.
 **/
const gchar*
exo_die_editor_get_comment (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), NULL);
  return editor->comment;
}



/**
 * exo_die_editor_set_comment:
 * @editor  : an #ExoDieEditor.
 * @comment : the new comment for @editor.
 *
 * Sets the comment for @editor to @comment.
 **/
void
exo_die_editor_set_comment (ExoDieEditor *editor,
                            const gchar  *comment)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));
  g_return_if_fail (g_utf8_validate (comment, -1, NULL));

  /* check if we have a new comment here */
  if (g_strcmp0 (editor->comment, comment) != 0)
    {
      /* apply the new comment */
      g_free (editor->comment);
      editor->comment = g_strdup (comment);

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "comment");
    }
}



/**
 * exo_die_editor_get_command:
 * @editor : an #ExoDieEditor.
 *
 * Returns the command for the @editor, which is only valid
 * if the mode is %EXO_DIE_EDITOR_MODE_APPLICATION.
 *
 * Return value: the command for the @editor.
 **/
const gchar*
exo_die_editor_get_command (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), NULL);
  return editor->command;
}



/**
 * exo_die_editor_set_command:
 * @editor  : an #ExoDieEditor.
 * @command : the new command for @editor.
 *
 * Sets the command for @editor to @command.
 **/
void
exo_die_editor_set_command (ExoDieEditor *editor,
                            const gchar  *command)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));
  g_return_if_fail (g_utf8_validate (command, -1, NULL));

  /* check if we have a new command here */
  if (g_strcmp0 (editor->command, command) != 0)
    {
      /* apply the new command */
      g_free (editor->command);
      editor->command = g_strdup (command);

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "complete");
      g_object_notify (G_OBJECT (editor), "command");
    }
}



/**
 * exo_die_editor_get_url:
 * @editor : an #ExoDieEditor.
 *
 * Returns the URL for @editor, which is only valid
 * if the mode is %EXO_DIE_EDITOR_MODE_LINK.
 *
 * Return value: the URL for @editor.
 **/
const gchar*
exo_die_editor_get_url (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), NULL);
  return editor->url;
}



/**
 * exo_die_editor_set_url:
 * @editor : an #ExoDieEditor.
 * @url    : the new URL for @editor.
 *
 * Sets the URL for @editor to @url.
 **/
void
exo_die_editor_set_url (ExoDieEditor *editor,
                        const gchar  *url)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));
  g_return_if_fail (g_utf8_validate (url, -1, NULL));

  /* check if we have a new URL here */
  if (g_strcmp0 (editor->url, url) != 0)
    {
      /* apply the new URL */
      g_free (editor->url);
      editor->url = g_strdup (url);

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "complete");
      g_object_notify (G_OBJECT (editor), "url");
    }
}



/**
 * exo_die_editor_get_path:
 * @editor : an #ExoDieEditor.
 *
 * Returns the path for @editor, which is only valid
 * if the mode is %EXO_DIE_EDITOR_MODE_APPLICATION.
 *
 * Return value: the working directory for @editor.
 **/
const gchar*
exo_die_editor_get_path (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), NULL);
  return editor->path;
}



/**
 * exo_die_editor_set_path:
 * @editor : an #ExoDieEditor.
 * @path   : the new working directory for @editor.
 *
 * Sets the working directory for @editor to @url.
 **/
void
exo_die_editor_set_path (ExoDieEditor *editor,
                         const gchar  *path)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));
  g_return_if_fail (g_utf8_validate (path, -1, NULL));

  /* check if we have a new URL here */
  if (g_strcmp0 (editor->path, path) != 0)
    {
      /* apply the new URL */
      g_free (editor->path);
      editor->path = g_strdup (path);

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "path");
    }
}



/**
 * exo_die_editor_get_icon:
 * @editor : an #ExoDieEditor.
 *
 * Returns the icon for the @editor.
 *
 * Return value: the icon for the @editor.
 **/
const gchar*
exo_die_editor_get_icon (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), NULL);
  return editor->icon;
}



/**
 * exo_die_editor_set_icon:
 * @editor : an #ExoDieEditor.
 * @icon   : the new icon for the @editor.
 *
 * Sets the icon for the @editor to @icon.
 **/
void
exo_die_editor_set_icon (ExoDieEditor *editor,
                         const gchar  *icon)
{
  GtkIconTheme    *icon_theme;
  GdkPixbuf       *pixbuf_scaled;
  GdkPixbuf       *pixbuf = NULL;
  cairo_surface_t *surface;
  GtkWidget       *image;
  GtkWidget       *label;
  gint             scale_factor;
  gint             icon_size;
  gint             pixbuf_width;
  gint             pixbuf_height;

  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));
  g_return_if_fail (g_utf8_validate (icon, -1, NULL));

  /* check if we have a new icon here */
  if (g_strcmp0 (editor->icon, icon) != 0)
    {
      /* apply the new icon */
      g_free (editor->icon);
      editor->icon = g_strdup (icon);

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "icon");

      /* drop the previous icon button child */
      if (gtk_bin_get_child (GTK_BIN (editor->icon_button)) != NULL)
        gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (editor->icon_button)));

      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (editor));
      icon_size = 48 * scale_factor;

      /* check the icon depending on the type */
      if (icon != NULL && g_path_is_absolute (icon))
        {
          /* try to load the icon from the file */
          pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
        }
      else if (!xfce_str_is_empty (icon))
        {
          /* determine the appropriate icon theme */
          icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (editor)));

          /* try to load the named icon */
          pixbuf = gtk_icon_theme_load_icon (icon_theme, icon, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        }

      /* setup the icon button */
      if (G_LIKELY (pixbuf != NULL))
        {
          /* scale down the icon if required */
          pixbuf_width = gdk_pixbuf_get_width (pixbuf);
          pixbuf_height = gdk_pixbuf_get_height (pixbuf);
          if (G_UNLIKELY (pixbuf_width > icon_size || pixbuf_height > icon_size))
            {
              pixbuf_scaled = exo_gdk_pixbuf_scale_ratio (pixbuf, icon_size);
              g_object_unref (G_OBJECT (pixbuf));
              pixbuf = pixbuf_scaled;
            }
          surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, gtk_widget_get_window (GTK_WIDGET (editor)));

          /* setup an image for the icon */
          image = gtk_image_new_from_surface (surface);
          gtk_container_add (GTK_CONTAINER (editor->icon_button), image);
          gtk_widget_show (image);

          /* release the pixbuf */
          g_object_unref (G_OBJECT (pixbuf));
          cairo_surface_destroy (surface);
        }
      else
        {
          /* setup a label to tell that no icon was selected */
          label = gtk_label_new (_("No icon"));
          gtk_container_add (GTK_CONTAINER (editor->icon_button), label);
          gtk_widget_show (label);
        }
    }
}



/**
 * exo_die_editor_get_snotify:
 * @editor : an #ExoDieEditor.
 *
 * Returns %TRUE if @editor has enabled startup notification, which
 * is only valid if mode is %EXO_DIE_EDITOR_MODE_APPLICATION.
 *
 * Return value: %TRUE if startup notification is enabled.
 **/
gboolean
exo_die_editor_get_snotify (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), FALSE);
  return editor->snotify;
}



/**
 * exo_die_editor_set_snotify:
 * @editor  : an #ExoDieEditor.
 * @snotify : %TRUE to enable startup notification.
 *
 * Set startup notification state for @editor to @snotify.
 **/
void
exo_die_editor_set_snotify (ExoDieEditor *editor,
                            gboolean      snotify)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));

  /* normalize the value */
  snotify = !!snotify;

  /* check if we have a new value */
  if (editor->snotify != snotify)
    {
      /* apply the new value */
      editor->snotify = snotify;

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "snotify");
    }
}



/**
 * exo_die_editor_get_terminal:
 * @editor : an #ExoDieEditor.
 *
 * Returns %TRUE if the command should be run in a terminal, only valid
 * if mode for @editor is %EXO_DIE_EDITOR_MODE_APPLICATION.
 *
 * Return value: %TRUE if command should be run in terminal.
 **/
gboolean
exo_die_editor_get_terminal (ExoDieEditor *editor)
{
  g_return_val_if_fail (EXO_DIE_IS_EDITOR (editor), FALSE);
  return editor->terminal;
}



/**
 * exo_die_editor_set_terminal:
 * @editor   : an #ExoDieEditor.
 * @terminal : %TRUE to run command in terminal.
 *
 * Sets the run in terminal setting of @editor to @terminal.
 **/
void
exo_die_editor_set_terminal (ExoDieEditor *editor,
                             gboolean      terminal)
{
  g_return_if_fail (EXO_DIE_IS_EDITOR (editor));

  /* normalize the value */
  terminal = !!terminal;

  /* check if we have a new value */
  if (editor->terminal != terminal)
    {
      /* apply the new value */
      editor->terminal = terminal;

      /* notify listeners */
      g_object_notify (G_OBJECT (editor), "terminal");
    }
}
