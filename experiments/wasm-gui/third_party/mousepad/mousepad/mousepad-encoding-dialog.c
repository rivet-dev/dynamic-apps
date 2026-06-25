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
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-encoding.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-history.h>



static void     mousepad_encoding_dialog_constructed            (GObject                     *object);
static void     mousepad_encoding_dialog_finalize               (GObject                     *object);
static void     mousepad_encoding_dialog_response               (GtkDialog                   *dialog,
                                                                 gint                         response_id);
static gboolean mousepad_encoding_dialog_test_encodings_idle    (gpointer                     user_data);
static void     mousepad_encoding_dialog_test_encodings_destroy (gpointer                     user_data);
static void     mousepad_encoding_dialog_test_encodings         (MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_cancel_encoding_test   (GtkWidget                   *button,
                                                                 MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_read_file              (MousepadEncodingDialog      *dialog,
                                                                 MousepadEncoding             encoding);
static void     mousepad_encoding_dialog_button_toggled         (GtkWidget                   *button,
                                                                 MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_combo_changed          (GtkComboBox                 *combo,
                                                                 MousepadEncodingDialog      *dialog);



enum
{
  COLUMN_LABEL,
  COLUMN_ID,
  N_COLUMNS
};

struct _MousepadEncodingDialog
{
  GtkDialog __parent__;

  /* the file */
  MousepadDocument *document;

  /* dialog title */
  gchar *title;

  /* encoding test idle id */
  guint timer_id;

  /* boolean to cancel the testing loop */
  gboolean cancel_testing;

  /* dialog widgets */
  GtkWidget *button_ok, *button_cancel, *error_box, *error_label, *progress_bar;

  /* radio buttons */
  GtkWidget *radio_default, *radio_system, *radio_history, *radio_other;

  /* "Other Encodings" combo box */
  GtkListStore *store, *fallback_store;
  GtkWidget    *combo;
};



G_DEFINE_TYPE (MousepadEncodingDialog, mousepad_encoding_dialog, GTK_TYPE_DIALOG)



static void
mousepad_encoding_dialog_class_init (MousepadEncodingDialogClass *klass)
{
  GObjectClass   *gobject_class;
  GtkDialogClass *gtkdialog_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = mousepad_encoding_dialog_constructed;
  gobject_class->finalize = mousepad_encoding_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = mousepad_encoding_dialog_response;
}



static void
mousepad_encoding_dialog_init (MousepadEncodingDialog *dialog)
{
  GtkWidget       *area, *vbox, *hbox, *icon;
  GtkCellRenderer *cell;

  /* set some dialog properties */
  gtk_window_set_default_size (GTK_WINDOW (dialog), 550, 350);

  /* add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), MOUSEPAD_LABEL_CANCEL, MOUSEPAD_RESPONSE_CANCEL);
  dialog->button_ok = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                             MOUSEPAD_LABEL_OK, MOUSEPAD_RESPONSE_OK);

  /* create an empty header */
  dialog->title = NULL;
  mousepad_util_dialog_create_header (GTK_DIALOG (dialog), dialog->title, NULL, NULL);

  /* dialog vbox */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (area), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* encoding radio buttons */

  /* default encoding */
  dialog->radio_default = gtk_radio_button_new_with_label (NULL, NULL);
  g_signal_connect (dialog->radio_default, "toggled",
                    G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_default, FALSE, FALSE, 0);

  /* system charset: added only if different from default */
  if (mousepad_encoding_get_default () != mousepad_encoding_get_system ())
    {
      dialog->radio_system = gtk_radio_button_new_with_label_from_widget (
                               GTK_RADIO_BUTTON (dialog->radio_default), NULL);
      g_signal_connect (dialog->radio_system, "toggled",
                        G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
      gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_system, FALSE, FALSE, 0);
    }
  else
    dialog->radio_system = NULL;

  /* history charset: shown only if different from default and system */
  dialog->radio_history = gtk_radio_button_new_with_label_from_widget (
                            GTK_RADIO_BUTTON (dialog->radio_default), NULL);
  g_signal_connect (dialog->radio_history, "toggled",
                    G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_history, FALSE, FALSE, 0);

  /* valid conversions to UTF-8 if there are any, else partially valid conversions, else hidden */
  dialog->radio_other = gtk_radio_button_new_with_label_from_widget (
                          GTK_RADIO_BUTTON (dialog->radio_default), _("Other:"));
  g_signal_connect (dialog->radio_other, "toggled",
                    G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_other, FALSE, FALSE, 0);

  /* create stores */
  dialog->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
  dialog->fallback_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

  /* combobox with other charsets */
  dialog->combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (dialog->store));
  gtk_box_pack_start (GTK_BOX (hbox), dialog->combo, FALSE, FALSE, 0);
  g_signal_connect (dialog->combo, "changed",
                    G_CALLBACK (mousepad_encoding_dialog_combo_changed), dialog);

  /* text renderer for 1st column */
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->combo),
                                  cell, "text", COLUMN_LABEL, NULL);

  /* progress bar */
  dialog->progress_bar = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (hbox), dialog->progress_bar, TRUE, TRUE, 0);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progress_bar),
                             _("Checking encodings..."));
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (dialog->progress_bar), TRUE);
  gtk_widget_show (dialog->progress_bar);

  /* cancel button */
  dialog->button_cancel = gtk_button_new_with_mnemonic (MOUSEPAD_LABEL_CANCEL);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->button_cancel, FALSE, FALSE, 0);
  g_signal_connect (dialog->button_cancel, "clicked",
                    G_CALLBACK (mousepad_encoding_dialog_cancel_encoding_test), dialog);
  gtk_widget_show (dialog->button_cancel);

  /* error box */
  dialog->error_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->error_box, FALSE, FALSE, 0);

  /* error icon */
  icon = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (dialog->error_box), icon, FALSE, FALSE, 0);
  gtk_widget_show (icon);

  /* error label */
  dialog->error_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (dialog->error_box), dialog->error_label, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (dialog->error_label), TRUE);
  gtk_widget_show (dialog->error_label);

  /* create text view */
  dialog->document = mousepad_document_new ();
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (dialog->document), TRUE, TRUE, 0);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (dialog->document->textview), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (dialog->document->textview), FALSE);
  g_settings_unbind (dialog->document->textview, "show-line-numbers");
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (dialog->document->textview), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (dialog->document->textview), GTK_WRAP_NONE);
  gtk_widget_show (GTK_WIDGET (dialog->document));
}



static void
mousepad_encoding_dialog_constructed (GObject *object)
{
  GtkWindow *dialog = GTK_WINDOW (object);

  G_OBJECT_CLASS (mousepad_encoding_dialog_parent_class)->constructed (object);

  /* setup CSD titlebar */
  mousepad_util_set_titlebar (dialog);
}



static void
mousepad_encoding_dialog_finalize (GObject *object)
{
  MousepadEncodingDialog *dialog = MOUSEPAD_ENCODING_DIALOG (object);

  /* stop running timeout */
  if (G_UNLIKELY (dialog->timer_id))
    g_source_remove (dialog->timer_id);

  /* clear and release stores */
  g_free (dialog->title);
  gtk_list_store_clear (dialog->store);
  gtk_list_store_clear (dialog->fallback_store);
  g_object_unref (dialog->store);
  g_object_unref (dialog->fallback_store);

  (*G_OBJECT_CLASS (mousepad_encoding_dialog_parent_class)->finalize) (object);
}



static void
mousepad_encoding_dialog_response (GtkDialog *dialog,
                                   gint       response_id)
{
  /* make sure we cancel encoding testing asap */
  MOUSEPAD_ENCODING_DIALOG (dialog)->cancel_testing = TRUE;
}



static gboolean
mousepad_encoding_dialog_set_radio (GtkWidget        *radio,
                                    const gchar      *name,
                                    MousepadEncoding  encoding,
                                    const gchar      *contents,
                                    gsize             length)
{
  gchar       *encoded, *label;
  const gchar *charset;
  gsize        written;
  gboolean     valid = FALSE;

  /* attach encoding to the button for later use */
  mousepad_object_set_data (radio, "encoding", GINT_TO_POINTER (encoding));

  /* try to convert the contents in the radio encoding */
  charset = mousepad_encoding_get_charset (encoding);
  encoded = g_convert (contents, length, "UTF-8", charset, NULL, &written, NULL);

  /* set the radio label accordingly */
  if (encoded == NULL)
    label = g_strdup_printf (_("%s (%s, failed)"), name, charset);
  else if (! g_utf8_validate (encoded, written, NULL))
    label = g_strdup_printf (_("%s (%s, partial)"), name, charset);
  else
    {
      valid = TRUE;
      label = g_strdup_printf ("%s (%s)", name, charset);
    }

  gtk_button_set_label (GTK_BUTTON (radio), label);

  /* cleanup */
  g_free (encoded);
  g_free (label);

  return valid;
}



static gboolean
mousepad_encoding_dialog_test_encodings_idle (gpointer user_data)
{
  MousepadEncodingDialog *dialog = MOUSEPAD_ENCODING_DIALOG (user_data);
  MousepadEncoding        default_encoding, system_encoding,
                          history_encoding = MOUSEPAD_ENCODING_NONE;
  GError                 *error = NULL;
  const gchar            *charset, *subtitle;
  gchar                  *contents, *label, *encoded;
  gsize                   length, written;
  guint                   i, n;
  gint                    result = 0;
  gboolean                default_valid, system_valid = FALSE, history_valid = FALSE, show_history;

  /* exit with a popup dialog in case of problem */
  if (! g_file_load_contents (mousepad_file_get_location (dialog->document->file),
                              NULL, &contents, &length, NULL, &error))
    {
      /* show the warning */
      mousepad_dialogs_show_error (GTK_WINDOW (dialog), error, MOUSEPAD_MESSAGE_IO_ERROR_OPEN);

      /* cleanup */
      g_error_free (error);

      /* cancel encoding test */
      gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

      return FALSE;
    }

  /* see if default, system and history encodings are valid and set their button labels */
  default_valid = g_utf8_validate (contents, length, NULL);
  default_encoding = mousepad_encoding_get_default ();
  charset = mousepad_encoding_get_charset (default_encoding);
  if (default_valid)
    label = g_strdup_printf ("%s (%s)", MOUSEPAD_ENCODING_LABEL_DEFAULT, charset);
  else
    label = g_strdup_printf (_("%s (%s, partial)"), MOUSEPAD_ENCODING_LABEL_DEFAULT, charset);

  gtk_button_set_label (GTK_BUTTON (dialog->radio_default), label);
  mousepad_object_set_data (dialog->radio_default, "encoding", GINT_TO_POINTER (default_encoding));
  g_free (label);

  /* set the system radio button */
  system_encoding = mousepad_encoding_get_system ();
  if (dialog->radio_system != NULL)
    system_valid = mousepad_encoding_dialog_set_radio (dialog->radio_system,
                                                       MOUSEPAD_ENCODING_LABEL_SYSTEM,
                                                       system_encoding, contents, length);

  /* set the history radio button */
  mousepad_history_recent_get_encoding (mousepad_file_get_location (dialog->document->file),
                                        &history_encoding);
  show_history = (history_encoding != MOUSEPAD_ENCODING_NONE
                  && history_encoding != default_encoding
                  && history_encoding != system_encoding);
  if (show_history)
    history_valid = mousepad_encoding_dialog_set_radio (dialog->radio_history,
                                                        MOUSEPAD_ENCODING_LABEL_HISTORY,
                                                        history_encoding, contents, length);

  /* test all other encodings */
  for (i = 1, n = 0; i < MOUSEPAD_N_ENCODINGS && ! dialog->cancel_testing; i++)
    {
      /* skip default, system and history encodings */
      if (i == default_encoding || i == system_encoding || i == history_encoding)
        continue;

      /* set progress bar fraction */
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progress_bar),
                                     (i + 1.00) / MOUSEPAD_N_ENCODINGS);

      /* try to convert the content */
      charset = mousepad_encoding_get_charset (i);
      encoded = g_convert (contents, length, "UTF-8", charset, NULL, &written, NULL);

      if (encoded != NULL)
        {
          /* insert in the store */
          if (g_utf8_validate (encoded, written, NULL))
            gtk_list_store_insert_with_values (dialog->store, NULL, n++,
                                               COLUMN_LABEL, charset, COLUMN_ID, i, -1);
          else
            gtk_list_store_insert_with_values (dialog->fallback_store, NULL, n++,
                                               COLUMN_LABEL, charset, COLUMN_ID, i, -1);

          /* cleanup */
          g_free (encoded);
        }

      /* iterate the main loop to update the gui */
      while (gtk_events_pending ())
        gtk_main_iteration ();
    }

  /* cleanup */
  g_free (contents);

  /* hide progress bar and cancel button */
  gtk_widget_hide (dialog->progress_bar);
  gtk_widget_hide (dialog->button_cancel);

  /* check if we have something to propose to the user apart from the default encoding */
  if (! gtk_tree_model_iter_n_children (GTK_TREE_MODEL (dialog->store), NULL))
    {
      /* fall back to partially valid conversions if possible */
      if (G_LIKELY (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (dialog->fallback_store), NULL)))
        {
          result = 1 - (default_valid || system_valid || history_valid);
          gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->combo),
                                   GTK_TREE_MODEL (dialog->fallback_store));
          gtk_button_set_label (GTK_BUTTON (dialog->radio_other), _("Other (partial):"));
        }
      else
        result = 2 * (1 - (default_valid || system_valid || history_valid));
    }

  /* show and activate radio buttons as needed */
  if (result < 2)
    {
      /* update the dialog header */
      if (result == 0)
        subtitle = _("Other valid encodings were found, please choose below.");
      else
        subtitle = _("Other partially valid encodings were found, please choose below.");

      mousepad_util_dialog_update_header (GTK_DIALOG (dialog), dialog->title,
                                          subtitle, "text-x-generic");

      /* show the default, system and history radio buttons */
      gtk_widget_show (dialog->radio_default);
      if (dialog->radio_system != NULL)
        gtk_widget_show (dialog->radio_system);
      if (show_history)
        gtk_widget_show (dialog->radio_history);

      /* show the "Other" radio button and combo box */
      gtk_widget_show (dialog->radio_other);
      gtk_widget_show (dialog->combo);

      /* select the first item in the combo box*/
      gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combo), 0);

      /* spread the encoding list over several columns */
      gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (dialog->combo), n / 10 + (n % 10 != 0));

      /* activate history encoding if possible, or the first valid encoding */
      if (history_valid)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->radio_history), TRUE);
      else if (default_valid)
        gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON (dialog->radio_default));
      else if (system_valid)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->radio_system), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->radio_other), TRUE);
    }
  else
    {
      /* update the dialog header */
      mousepad_util_dialog_update_header (GTK_DIALOG (dialog), dialog->title,
                                          _("No other valid encoding was found."),
                                          "text-x-generic");

      /* the system and history charsets won't be recognized here, this is just to
       * inform the user that they are different from default */
      if (dialog->radio_system != NULL)
        {
          gtk_widget_show (dialog->radio_default);
          gtk_widget_show (dialog->radio_system);
        }

      if (show_history)
        {
          gtk_widget_show (dialog->radio_default);
          gtk_widget_show (dialog->radio_history);
        }

      /* activate the radio button (maybe hidden) */
      gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON (dialog->radio_default));
    }

  return FALSE;
}



static void
mousepad_encoding_dialog_test_encodings_destroy (gpointer user_data)
{
  MOUSEPAD_ENCODING_DIALOG (user_data)->timer_id = 0;
}



static void
mousepad_encoding_dialog_test_encodings (MousepadEncodingDialog *dialog)
{
  if (G_LIKELY (dialog->timer_id == 0))
    {
      /* reset boolean */
      dialog->cancel_testing = FALSE;

      /* start a new idle function */
      dialog->timer_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_encoding_dialog_test_encodings_idle,
                                          dialog, mousepad_encoding_dialog_test_encodings_destroy);
    }
}



static void
mousepad_encoding_dialog_cancel_encoding_test (GtkWidget              *button,
                                               MousepadEncodingDialog *dialog)
{
  /* cancel the testing loop */
  dialog->cancel_testing = TRUE;
}



static void
mousepad_encoding_dialog_read_file (MousepadEncodingDialog *dialog,
                                    MousepadEncoding        encoding)
{
  GtkTextIter  start, end;
  GError      *error = NULL;
  gchar       *message;
  gint         result;

  /* clear buffer */
  gtk_text_buffer_get_bounds (dialog->document->buffer, &start, &end);
  gtk_text_buffer_delete (dialog->document->buffer, &start, &end);

  if (G_LIKELY (encoding))
    {
      /* set encoding */
      mousepad_file_set_encoding (dialog->document->file, encoding);

      /* try to open the file */
      result = mousepad_file_open (dialog->document->file, 0, 0, TRUE, TRUE, TRUE, &error);
    }
  /* unsupported system charset */
  else
    result = 1;

  /* set sensitivity of the ok button */
  gtk_widget_set_sensitive (dialog->button_ok, result == 0);

  /* no error, hide the box */
  if (result == 0)
    gtk_widget_hide (dialog->error_box);
  else
    {
      /* conversion error */
      if (error)
        {
          /* format message */
          message = g_strdup_printf ("<b>%s.</b>", error->message);

          /* clear the error */
          g_error_free (error);
        }
      /* conversion skipped */
      else
        message = g_strdup_printf ("<b>%s.</b>", MOUSEPAD_MESSAGE_UNSUPPORTED_ENCODING);

      /* set the error label */
      gtk_label_set_markup (GTK_LABEL (dialog->error_label), message);

      /* cleanup */
      g_free (message);

      /* show the error box */
      gtk_widget_show (dialog->error_box);
    }
}



static void
mousepad_encoding_dialog_button_toggled (GtkWidget              *button,
                                         MousepadEncodingDialog *dialog)
{
  MousepadEncoding encoding;

  /* ignore inactive buttons */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      if (button == dialog->radio_other)
        {
          gtk_widget_set_sensitive (dialog->combo, TRUE);
          mousepad_encoding_dialog_combo_changed (GTK_COMBO_BOX (dialog->combo), dialog);
        }
      else
        {
          gtk_widget_set_sensitive (dialog->combo, FALSE);
          encoding = GPOINTER_TO_INT (mousepad_object_get_data (button, "encoding"));
          mousepad_encoding_dialog_read_file (dialog, encoding);
        }
    }
}



static void
mousepad_encoding_dialog_combo_changed (GtkComboBox            *combo,
                                        MousepadEncodingDialog *dialog)
{
  GtkTreeIter iter;
  gint        id;

  /* get the selected item */
  if (gtk_widget_get_sensitive (GTK_WIDGET (combo))
      && gtk_combo_box_get_active_iter (combo, &iter))
    {
      /* get the id */
      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter, COLUMN_ID, &id, -1);

      /* open the file with other encoding */
      mousepad_encoding_dialog_read_file (dialog, id);
    }
}



gint
mousepad_encoding_dialog (GtkWindow        *parent,
                          MousepadFile     *file,
                          gboolean          valid,
                          MousepadEncoding *encoding)
{
  MousepadEncodingDialog *dialog;
  GError                 *error = NULL;
  gint                    result, response;

  g_return_val_if_fail (GTK_IS_WINDOW (parent), 0);
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), 0);

  /* create the dialog */
  dialog = g_object_new (MOUSEPAD_TYPE_ENCODING_DIALOG, "transient-for", parent,
                         "modal", TRUE, NULL);
  mousepad_dialogs_destroy_with_parent (GTK_WIDGET (dialog), parent);

  /* try first to read the file with the history or the default encoding if needed */
  if (mousepad_file_get_encoding (file) == MOUSEPAD_ENCODING_NONE)
    {
      *encoding = MOUSEPAD_ENCODING_NONE;
      mousepad_history_recent_get_encoding (mousepad_file_get_location (file), encoding);
      if (*encoding == MOUSEPAD_ENCODING_NONE)
        *encoding = mousepad_encoding_get_default ();

      mousepad_file_set_encoding (file, *encoding);
      result = mousepad_file_open (file, 0, 0, TRUE, TRUE, FALSE, &error);

      /* handle error */
      if (result == ERROR_READING_FAILED || result == ERROR_FILE_STATUS_FAILED)
        {
          mousepad_dialogs_show_error (GTK_WINDOW (dialog), error, MOUSEPAD_MESSAGE_IO_ERROR_OPEN);
          g_error_free (error);

          return MOUSEPAD_RESPONSE_CANCEL;
        }
      else
        valid = (result == 0);
    }

  /* dialog title */
  dialog->title = g_strdup_printf (valid ? _("The document is %s valid.")
                                         : _("The document is not %s valid."),
                    mousepad_encoding_get_charset (mousepad_file_get_encoding (file)));

  /* update the dialog header */
  mousepad_util_dialog_update_header (GTK_DIALOG (dialog), dialog->title, NULL, "text-x-generic");

  /* set the file location */
  mousepad_file_set_location (dialog->document->file, mousepad_file_get_location (file),
                              MOUSEPAD_LOCATION_REAL);

  /* queue idle function */
  mousepad_encoding_dialog_test_encodings (dialog);

  /* run the dialog, get the new encoding */
  if ((response = gtk_dialog_run (GTK_DIALOG (dialog))) == MOUSEPAD_RESPONSE_OK)
    *encoding = mousepad_file_get_encoding (dialog->document->file);

  /* destroy the dialog */
  gtk_widget_destroy (GTK_WIDGET (dialog));

  return response;
}
