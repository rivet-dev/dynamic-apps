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
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo-desktop-item-edit/exo-die-command-entry.h>
#include <exo-desktop-item-edit/exo-die-command-model.h>
#include <libxfce4util/libxfce4util.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_TEXT,
};



static void exo_die_command_entry_finalize        (GObject                  *object);
static void exo_die_command_entry_get_property    (GObject                  *object,
                                                   guint                     prop_id,
                                                   GValue                   *value,
                                                   GParamSpec               *pspec);
static void exo_die_command_entry_set_property    (GObject                  *object,
                                                   guint                     prop_id,
                                                   const GValue             *value,
                                                   GParamSpec               *pspec);
static void exo_die_command_entry_activate        (ExoDieCommandEntry       *command_entry);
static void exo_die_command_entry_button_clicked  (GtkWidget                *button,
                                                   ExoDieCommandEntry       *command_entry);
static void exo_die_command_entry_model_loaded    (ExoDieCommandModel       *command_model,
                                                   ExoDieCommandEntry       *command_entry);



struct _ExoDieCommandEntryClass
{
  GtkHBoxClass __parent__;

  /* signals */
  void (*activate) (ExoDieCommandEntry *command_entry);
};

struct _ExoDieCommandEntry
{
  GtkHBox             __parent__;
  ExoDieCommandModel *model;
  GtkWidget          *entry;
  gchar              *text;
};



G_DEFINE_TYPE (ExoDieCommandEntry, exo_die_command_entry, GTK_TYPE_BOX)



static void
exo_die_command_entry_class_init (ExoDieCommandEntryClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_die_command_entry_finalize;
  gobject_class->get_property = exo_die_command_entry_get_property;
  gobject_class->set_property = exo_die_command_entry_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);

  klass->activate = exo_die_command_entry_activate;

  /**
   * ExoDieCommandEntry:text:
   *
   * The text entered into the command entry box.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "text",
                                                        "text",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoDieCommandEntry::activate:
   * @command_entry : an #ExoDieCommandEntry.
   *
   * The "activate" signal on #ExoDieCommandEntry is an action
   * signal and emitting causes the entry to grab focus.
   **/
  gtkwidget_class->activate_signal =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ExoDieCommandEntryClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
exo_die_command_entry_init (ExoDieCommandEntry *command_entry)
{
  GtkWidget *button;
  GtkWidget *image;

  /* setup the box */
  gtk_orientable_set_orientation (GTK_ORIENTABLE (command_entry), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (command_entry), 6);

  /* allocate the command model */
  command_entry->model = exo_die_command_model_new ();
  g_signal_connect (G_OBJECT (command_entry->model), "loaded", G_CALLBACK (exo_die_command_entry_model_loaded), command_entry);

  /* TODO: switch to widget templates
  gtk_widget_push_composite_child ();*/

  command_entry->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (command_entry->entry), TRUE);
  g_object_bind_property (G_OBJECT (command_entry->entry), "text", G_OBJECT (command_entry), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (command_entry), command_entry->entry, TRUE, TRUE, 0);
  gtk_widget_show (command_entry->entry);

  button = gtk_button_new ();
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (exo_die_command_entry_button_clicked), command_entry);
  gtk_box_pack_start (GTK_BOX (command_entry), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  /*gtk_widget_pop_composite_child ();*/
}



static void
exo_die_command_entry_finalize (GObject *object)
{
  ExoDieCommandEntry *command_entry = EXO_DIE_COMMAND_ENTRY (object);

  /* release the model */
  g_signal_handlers_disconnect_by_func (G_OBJECT (command_entry->model), exo_die_command_entry_model_loaded, command_entry);
  g_object_unref (G_OBJECT (command_entry->model));

  /* release the text */
  g_free (command_entry->text);

  (*G_OBJECT_CLASS (exo_die_command_entry_parent_class)->finalize) (object);
}



static void
exo_die_command_entry_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ExoDieCommandEntry *command_entry = EXO_DIE_COMMAND_ENTRY (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, exo_die_command_entry_get_text (command_entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_die_command_entry_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ExoDieCommandEntry *command_entry = EXO_DIE_COMMAND_ENTRY (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      exo_die_command_entry_set_text (command_entry, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_die_command_entry_activate (ExoDieCommandEntry *command_entry)
{
  /* grab keyboard focus to the real entry widget */
  gtk_widget_mnemonic_activate (command_entry->entry, TRUE);
}



static void
exo_die_command_entry_button_clicked (GtkWidget          *button,
                                      ExoDieCommandEntry *command_entry)
{
  GtkFileFilter *filter;
  GtkWidget     *toplevel;
  GtkWidget     *chooser;
  gchar         *filename;
  gchar         *s;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (EXO_DIE_IS_COMMAND_ENTRY (command_entry));

  /* determine the toplevel window */
  toplevel = gtk_widget_get_toplevel (button);
  if (toplevel == NULL || !gtk_widget_is_toplevel (toplevel))
    return;

  /* allocate the file chooser */
  chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
                                         GTK_WINDOW (toplevel),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* use the bindir as default folder */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* setup the currently selected file */
  filename = g_strdup (command_entry->text);
  if (G_LIKELY (filename != NULL))
    {
      /* use only the first argument */
      s = strchr (filename, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* check if we have a filename */
      if (G_LIKELY (*filename != '\0'))
        {
          /* check if the filename is not an absolute path */
          if (G_LIKELY (!g_path_is_absolute (filename)))
            {
              /* try to lookup the filename in $PATH */
              s = g_find_program_in_path (filename);
              if (G_LIKELY (s != NULL))
                {
                  /* use the absolute path instead */
                  g_free (filename);
                  filename = s;
                }
            }

          /* check if we have an absolute path now */
          if (G_LIKELY (g_path_is_absolute (filename)))
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
        }

      /* release the filename */
      g_free (filename);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      /* determine the selected file name */
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));

      /* verify that the file name is UTF-8 encoded */
      if (!g_utf8_validate (filename, -1, NULL))
        {
          /* recode the file name */
          s = g_filename_display_name (filename);
          g_free (filename);
          filename = s;
        }

      /* quote the filename if it contains a space */
      if (filename != NULL
          && strchr (filename, ' ') != NULL)
        {
          s = g_shell_quote (filename);
          g_free (filename);
          filename = s;
        }

      /* apply the new command */
      exo_die_command_entry_set_text (command_entry, filename);
      g_free (filename);
    }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}



static void
exo_die_command_entry_model_loaded (ExoDieCommandModel *command_model,
                                    ExoDieCommandEntry *command_entry)
{
  GtkEntryCompletion *completion;

  /* allocate a new completion for the entry */
  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_minimum_key_length (completion, 1);
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_completion_set_popup_completion (completion, TRUE);
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (command_model));
  gtk_entry_completion_set_text_column (completion, EXO_DIE_COMMAND_MODEL_COLUMN_NAME);
  gtk_entry_set_completion (GTK_ENTRY (command_entry->entry), completion);
  g_object_unref (G_OBJECT (completion));
}



/**
 * exo_die_command_entry_new:
 *
 * Allocates a new #ExoDieCommandEntry instance.
 *
 * Return value: the newly allocated #ExoDieCommandEntry.
 **/
GtkWidget*
exo_die_command_entry_new (void)
{
  return g_object_new (EXO_DIE_TYPE_COMMAND_ENTRY, NULL);
}



/**
 * exo_die_command_entry_get_text:
 * @command_entry : an #ExoDieCommandEntry.
 *
 * Returns the text for the @command_entry.
 *
 * Return value: the text for @command_entry.
 **/
const gchar*
exo_die_command_entry_get_text (ExoDieCommandEntry *command_entry)
{
  g_return_val_if_fail (EXO_DIE_IS_COMMAND_ENTRY (command_entry), NULL);
  return command_entry->text;
}



/**
 * exo_die_command_entry_set_text:
 * @command_entry : an #ExoDieCommandEntry.
 * @text          : the new text for @command_entry.
 *
 * Sets the text of @command_entry to the specified
 * @text.
 **/
void
exo_die_command_entry_set_text (ExoDieCommandEntry *command_entry,
                                const gchar        *text)
{
  g_return_if_fail (EXO_DIE_IS_COMMAND_ENTRY (command_entry));
  g_return_if_fail (g_utf8_validate (text, -1, NULL));

  /* release the previous text */
  g_free (command_entry->text);

  /* apply the new text */
  command_entry->text = g_strdup (text);

  /* notify listeners */
  g_object_notify (G_OBJECT (command_entry), "text");
}


