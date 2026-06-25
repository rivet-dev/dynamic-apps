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
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-history.h>



static void              mousepad_replace_dialog_finalize               (GObject               *object);
static void              mousepad_replace_dialog_response               (GtkWidget             *widget,
                                                                         gint                   response_id);
static void              mousepad_replace_dialog_search_completed       (MousepadReplaceDialog *dialog,
                                                                         gint                   cur_match,
                                                                         gint                   n_matches,
                                                                         const gchar           *search_string,
                                                                         MousepadSearchFlags    flags);
static void              mousepad_replace_dialog_entry_changed          (MousepadReplaceDialog *dialog);
static void              mousepad_replace_dialog_setting_changed        (MousepadReplaceDialog *dialog);
static void              mousepad_replace_dialog_entry_activate         (MousepadReplaceDialog *dialog);
static void              mousepad_replace_dialog_entry_reverse_activate (MousepadReplaceDialog *dialog);



struct _MousepadReplaceDialog
{
  GtkDialog __parent__;

  /* dialog widgets */
  GtkWidget *search_box;
  GtkWidget *replace_box;
  GtkWidget *search_entry;
  GtkWidget *replace_entry;
  GtkWidget *find_button;
  GtkWidget *replace_button;
  GtkWidget *search_location_combo;
  GtkWidget *hits_label;
  GtkWidget *spinner;
};

enum
{
  IN_SELECTION = 0,
  IN_DOCUMENT,
  IN_ALL_DOCUMENTS
};

enum
{
  DIRECTION_UP = 0,
  DIRECTION_DOWN
};

enum
{
  SEARCH,
  LAST_SIGNAL
};



static guint   dialog_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadReplaceDialog, mousepad_replace_dialog, GTK_TYPE_DIALOG)


static void
mousepad_replace_dialog_class_init (MousepadReplaceDialogClass *klass)
{
  GObjectClass   *gobject_class, *entry_class;
  GtkBindingSet  *binding_set;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_replace_dialog_finalize;

  dialog_signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__FLAGS_STRING_STRING,
                  G_TYPE_NONE, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS,
                  G_TYPE_STRING, G_TYPE_STRING);

  /* add a reverse-activate signal to GtkEntry */
  entry_class = g_type_class_ref (GTK_TYPE_ENTRY);
  binding_set = gtk_binding_set_by_class (entry_class);
  if (G_LIKELY (g_signal_lookup ("reverse-activate", GTK_TYPE_ENTRY) == 0))
    {
      g_signal_new ("reverse-activate", GTK_TYPE_ENTRY, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return,
                                    GDK_SHIFT_MASK, "reverse-activate", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter,
                                    GDK_SHIFT_MASK, "reverse-activate", 0);
    }

  /* add a select-all signal to GtkEntry */
  if (G_LIKELY (g_signal_lookup ("select-all", GTK_TYPE_ENTRY) == 0))
    {
      g_signal_new ("select-all", GTK_TYPE_ENTRY, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, "select-all", 0);
    }

  /* cleanup */
  g_type_class_unref (entry_class);
}



static void
mousepad_replace_dialog_entry_select_all (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}



static void
mousepad_replace_dialog_bind_setting (MousepadReplaceDialog *dialog,
                                      const gchar           *path,
                                      gpointer               object,
                                      const gchar           *property)
{
  mousepad_setting_bind (path, object, property, G_SETTINGS_BIND_DEFAULT);

  mousepad_setting_connect_object (path,
                                   G_CALLBACK (mousepad_replace_dialog_setting_changed),
                                   dialog,
                                   G_CONNECT_SWAPPED);
}



static void
mousepad_replace_dialog_update_label (MousepadReplaceDialog *dialog,
                                      GtkWidget             *check)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));

  gtk_button_set_label (GTK_BUTTON (dialog->replace_button),
                        active ? _("_Replace All") : _("_Replace"));
}



static void
mousepad_replace_dialog_post_init (MousepadReplaceDialog *dialog)
{
  GtkApplication   *application;
  GtkWindow        *window;
  GtkBindingSet    *binding_set;
  GdkModifierType   accel_mods;
  guint             n, accel_key;
  gchar           **accels;
  const gchar      *actions[] = { "win.edit.cut", "win.edit.copy", "win.edit.paste",
                                  "win.edit.select-all" };
  const gchar      *signals[] = { "cut-clipboard", "copy-clipboard", "paste-clipboard",
                                  "select-all" };

  /* disconnect this handler */
  mousepad_disconnect_by_func (dialog, mousepad_replace_dialog_post_init, NULL);

  /* setup CSD titlebar */
  mousepad_util_set_titlebar (GTK_WINDOW (dialog));

  /* get the transient parent window and the application */
  window = gtk_window_get_transient_for (GTK_WINDOW (dialog));
  application = gtk_window_get_application (window);

  /* connect to the "search-completed" parent window signal */
  g_signal_connect_object (window, "search-completed",
                           G_CALLBACK (mousepad_replace_dialog_search_completed),
                           dialog, G_CONNECT_SWAPPED);

  /* make text entries keybindings consistent with those of the text view */
  binding_set = gtk_binding_set_by_class (g_type_class_peek (GTK_TYPE_ENTRY));

  accels = gtk_application_get_accels_for_action (application, "win.edit.delete-selection");
  if (accels[0] != NULL)
    {
      gtk_accelerator_parse (accels[0], &accel_key, &accel_mods);
      gtk_binding_entry_remove (binding_set, accel_key, accel_mods);
      gtk_binding_entry_add_signal (binding_set, accel_key, accel_mods, "delete-from-cursor",
                                    2, GTK_TYPE_DELETE_TYPE, GTK_DELETE_CHARS, G_TYPE_INT, 1);
    }

  g_strfreev (accels);

  for (n = 0; n < G_N_ELEMENTS (actions); n++)
    {
      accels = gtk_application_get_accels_for_action (application, actions[n]);
      if (accels[0] != NULL)
        {
          gtk_accelerator_parse (accels[0], &accel_key, &accel_mods);
          gtk_binding_entry_remove (binding_set, accel_key, accel_mods);
          gtk_binding_entry_add_signal (binding_set, accel_key, accel_mods, signals[n], 0);
        }

      g_strfreev (accels);
    }

  /* give the dialog its definite size by setting a fake occurrences label */
  gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->search_entry));
  gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), "fake-text");
  mousepad_replace_dialog_search_completed (dialog, 99999, 99999, "fake-text",
                                            MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION
                                            | MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS);

  /* show all widgets */
  gtk_widget_show_all (GTK_WIDGET (dialog));

  /* reset search entry and occurrences label */
  gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), "");
  gtk_label_set_text (GTK_LABEL (dialog->hits_label), NULL);
}



static void
mousepad_replace_dialog_init (MousepadReplaceDialog *dialog)
{
  GtkWidget    *button, *area, *vbox, *hbox, *combo, *label, *check;
  GtkSizeGroup *size_group;

  /* we will complete initialization when the parent window is set */
  g_signal_connect (dialog, "notify::transient-for",
                    G_CALLBACK (mousepad_replace_dialog_post_init), NULL);

  /* set dialog properties */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Find and Replace"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (mousepad_replace_dialog_response), NULL);

  /* dialog buttons */
  dialog->find_button = mousepad_util_image_button ("edit-find", _("_Find"));
  gtk_widget_set_can_default (dialog->find_button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                dialog->find_button, MOUSEPAD_RESPONSE_FIND);

  dialog->replace_button = mousepad_util_image_button ("edit-find-replace", _("_Replace"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                dialog->replace_button, MOUSEPAD_RESPONSE_REPLACE);

  button = mousepad_util_image_button ("window-close", _("_Close"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_FIND);

  /* create main vertical box */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (area), vbox, TRUE, TRUE, 6);

  /* horizontal box for search string */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  /* create a size group */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  label = gtk_label_new_with_mnemonic (_("_Search for:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);

  combo = dialog->search_box = gtk_combo_box_text_new_with_entry ();
  mousepad_history_search_fill_search_box (GTK_COMBO_BOX_TEXT (combo));
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  /* store as an entry widget */
  dialog->search_entry = gtk_bin_get_child (GTK_BIN (combo));
  g_signal_connect_swapped (dialog->search_entry, "changed",
                            G_CALLBACK (mousepad_replace_dialog_entry_changed), dialog);
  g_signal_connect_swapped (dialog->search_entry, "activate",
                            G_CALLBACK (mousepad_replace_dialog_entry_activate), dialog);
  g_signal_connect_swapped (dialog->search_entry, "reverse-activate",
                            G_CALLBACK (mousepad_replace_dialog_entry_reverse_activate), dialog);
  g_signal_connect (dialog->search_entry, "select-all",
                    G_CALLBACK (mousepad_replace_dialog_entry_select_all), NULL);

  /* bind the sensitivity of the find and replace buttons to the search text length */
  g_object_bind_property (dialog->search_entry, "text-length",
                          dialog->find_button, "sensitive", G_BINDING_SYNC_CREATE);
  g_object_bind_property (dialog->search_entry, "text-length",
                          dialog->replace_button, "sensitive", G_BINDING_SYNC_CREATE);

  /* horizontal box for replace string */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("Replace _with:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);

  combo = dialog->replace_box = gtk_combo_box_text_new_with_entry ();
  mousepad_history_search_fill_replace_box (GTK_COMBO_BOX_TEXT (combo));
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  /* store as an entry widget */
  dialog->replace_entry = gtk_bin_get_child (GTK_BIN (combo));
  g_signal_connect (dialog->replace_entry, "select-all",
                    G_CALLBACK (mousepad_replace_dialog_entry_select_all), NULL);

  /* search direction */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("Search _direction:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);

  combo = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Up"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Down"));

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_DIRECTION,
                                        combo, "active");

  /* release size group */
  g_object_unref (size_group);

  /* wrap around */
  check = gtk_check_button_new_with_mnemonic (_("_Wrap around"));
  gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_WRAP_AROUND,
                                        check, "active");

  /* case sensitive */
  check = gtk_check_button_new_with_mnemonic (_("Match _case"));
  gtk_widget_set_margin_start (check, 6);
  gtk_widget_set_margin_end (check, 6);
  gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_MATCH_CASE,
                                        check, "active");

  /* match whole word */
  check = gtk_check_button_new_with_mnemonic (_("_Match whole word"));
  gtk_widget_set_margin_start (check, 6);
  gtk_widget_set_margin_end (check, 6);
  gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_MATCH_WHOLE_WORD,
                                        check, "active");

  /* enable regex search */
  check = gtk_check_button_new_with_mnemonic (_("Regular e_xpression"));
  gtk_widget_set_margin_start (check, 6);
  gtk_widget_set_margin_end (check, 6);
  gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_ENABLE_REGEX,
                                        check, "active");

  /* horizontal box for the replace all options */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  check = gtk_check_button_new_with_mnemonic (_("Replace _all in:"));
  gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);

  combo = dialog->search_location_combo = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Selection"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Document"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("All Documents"));

  g_signal_connect_swapped (check, "toggled",
                            G_CALLBACK (mousepad_replace_dialog_update_label), dialog);
  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_REPLACE_ALL,
                                        check, "active");
  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_REPLACE_ALL_LOCATION,
                                        combo, "active");
  g_object_bind_property (check, "active", combo, "sensitive", G_BINDING_SYNC_CREATE);

  /* the occurrences label */
  label = dialog->hits_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  /* the spinner */
  dialog->spinner = gtk_spinner_new ();
  gtk_box_pack_start (GTK_BOX (hbox), dialog->spinner, FALSE, FALSE, 0);
}



static void
mousepad_replace_dialog_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_replace_dialog_parent_class)->finalize) (object);
}



static void
mousepad_replace_dialog_reset_display (MousepadReplaceDialog *dialog)
{
  const gchar *string;

  /* reset occurrences label */
  gtk_label_set_text (GTK_LABEL (dialog->hits_label), NULL);

  /* start the spinner or reset entry color */
  string = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  if (string != NULL && *string != '\0')
    gtk_spinner_start (GTK_SPINNER (dialog->spinner));
  else
    mousepad_util_entry_error (dialog->search_entry, FALSE);
}



static void
mousepad_replace_dialog_response (GtkWidget *widget,
                                  gint       response_id)
{
  MousepadReplaceDialog *dialog = MOUSEPAD_REPLACE_DIALOG (widget);
  MousepadSearchFlags    flags;
  GtkComboBoxText       *box;
  const gchar           *search_str, *replace_str;
  gint                   search_direction, replace_all_location;
  guint                  idx;

  /* close dialog */
  if (response_id == MOUSEPAD_RESPONSE_CLOSE || response_id < 0)
    {
      gtk_widget_destroy (widget);
      return;
    }

  /* get strings */
  search_str = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  replace_str = gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry));

  /* search direction */
  search_direction = MOUSEPAD_SETTING_GET_UINT (SEARCH_DIRECTION);
  if ((search_direction == DIRECTION_UP && response_id != MOUSEPAD_RESPONSE_REVERSE_FIND)
      || (search_direction != DIRECTION_UP && response_id == MOUSEPAD_RESPONSE_REVERSE_FIND))
    flags = MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;
  else
    flags = MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* search area */
  if (MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_REPLACE_ALL))
    {
      replace_all_location = MOUSEPAD_SETTING_GET_UINT (SEARCH_REPLACE_ALL_LOCATION);
      flags |= MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA;
      if (replace_all_location == IN_ALL_DOCUMENTS)
        flags |= MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS;
      else if (replace_all_location == IN_SELECTION)
        flags |= MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION;
    }

  /* start position */
  if (response_id == MOUSEPAD_RESPONSE_FIND || response_id == MOUSEPAD_RESPONSE_REVERSE_FIND)
    {
      /* update search history */
      box = GTK_COMBO_BOX_TEXT (dialog->search_box);
      idx = mousepad_history_search_insert_search_text (search_str);
      if (idx > 0)
        {
          gtk_combo_box_text_prepend_text (box, search_str);
          gtk_combo_box_text_remove (box, idx);

          /* always be in box: avoid `idx == -1` and `idx == history_size` */
          gtk_combo_box_set_active (GTK_COMBO_BOX (box), 0);
        }

      /* select the first match */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

      /* start at the 'end' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
    }
  else if (response_id == MOUSEPAD_RESPONSE_ENTRY_CHANGED)
    {
      /* select the first match if incremental search is enabled */
      if (MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_INCREMENTAL))
        flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_NONE;

      /* start at the 'beginning' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
    }
  else if (response_id == MOUSEPAD_RESPONSE_REPLACE)
    {
      /* update search and replace histories: even an implicit search must be added
       * to the history in this case */
      box = GTK_COMBO_BOX_TEXT (dialog->search_box);
      idx = mousepad_history_search_insert_search_text (search_str);
      if (idx > 0)
        {
          gtk_combo_box_text_prepend_text (box, search_str);
          gtk_combo_box_text_remove (box, idx);
          gtk_combo_box_set_active (GTK_COMBO_BOX (box), 0);
        }

      box = GTK_COMBO_BOX_TEXT (dialog->replace_box);
      idx = mousepad_history_search_insert_replace_text (replace_str);
      if (idx > 0)
        {
          gtk_combo_box_text_prepend_text (box, replace_str);
          gtk_combo_box_text_remove (box, idx);
          gtk_combo_box_set_active (GTK_COMBO_BOX (box), 0);
        }

      /* replace matches */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE;

      /* start at the 'beginning' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
    }

  /* reset display widgets */
  mousepad_replace_dialog_reset_display (dialog);

  /* emit the signal */
  g_signal_emit (dialog, dialog_signals[SEARCH], 0, flags, search_str, replace_str);
}



static void
mousepad_replace_dialog_search_completed (MousepadReplaceDialog *dialog,
                                          gint                   cur_match,
                                          gint                   n_matches,
                                          const gchar           *search_string,
                                          MousepadSearchFlags    flags)
{
  gchar       *message;
  const gchar *string;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));

  /* leave the dialog unchanged if the search was launched from the search bar
   * for a different string... */
  if (g_strcmp0 (string, search_string) != 0)
    {
      /* stop the spinner */
      gtk_spinner_stop (GTK_SPINNER (dialog->spinner));
      return;
    }
  /* ... or if irrelevant settings for it are in use here, without stopping the spinner
   * in this case (we are in multi-document mode and this is a partial result) */
  else if (MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_REPLACE_ALL)
           && MOUSEPAD_SETTING_GET_UINT (SEARCH_REPLACE_ALL_LOCATION) != IN_DOCUMENT
           && ! (flags & (MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION
                          | MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)))
    return;

  /* stop the spinner */
  gtk_spinner_stop (GTK_SPINNER (dialog->spinner));

  if (string != NULL && *string != '\0')
    {
      /* update entry color */
      mousepad_util_entry_error (dialog->search_entry, n_matches == 0);

      /* update counter */
      if (cur_match != 0)
        message = g_strdup_printf (ngettext ("%d of %d match", "%d of %d matches", n_matches),
                                   cur_match, n_matches);
      else
        message = g_strdup_printf (ngettext ("%d match", "%d matches", n_matches),
                                   n_matches);
      gtk_label_set_markup (GTK_LABEL (dialog->hits_label), message);
      g_free (message);
    }
}



static void
mousepad_replace_dialog_entry_changed (MousepadReplaceDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_ENTRY_CHANGED);
}



static gboolean
mousepad_replace_dialog_setting_changed_idle (gpointer dialog)
{
  mousepad_replace_dialog_entry_changed (dialog);

  return FALSE;
}



static void
mousepad_replace_dialog_setting_changed (MousepadReplaceDialog *dialog)
{
  /* allow time for the search context settings to synchronize with those of Mousepad */
  g_idle_add (mousepad_replace_dialog_setting_changed_idle,
              mousepad_util_source_autoremove (dialog));
}



static void
mousepad_replace_dialog_entry_activate (MousepadReplaceDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_FIND);
}



static void
mousepad_replace_dialog_entry_reverse_activate (MousepadReplaceDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_REVERSE_FIND);
}



GtkWidget *
mousepad_replace_dialog_new (MousepadWindow *window)
{
  return g_object_new (MOUSEPAD_TYPE_REPLACE_DIALOG, "transient-for", window,
                       "destroy-with-parent", TRUE, NULL);
}



void
mousepad_replace_dialog_page_switched (MousepadReplaceDialog *dialog,
                                       GtkTextBuffer         *old_buffer,
                                       GtkTextBuffer         *new_buffer)
{
  /* disconnect from old buffer signals */
  if (old_buffer != NULL)
    mousepad_disconnect_by_func (old_buffer, mousepad_replace_dialog_reset_display, dialog);

  /* connect to new buffer signals to update display widgets on change */
  g_signal_connect_object (new_buffer, "insert-text",
                           G_CALLBACK (mousepad_replace_dialog_reset_display),
                           dialog, G_CONNECT_SWAPPED);
  g_signal_connect_object (new_buffer, "delete-range",
                           G_CALLBACK (mousepad_replace_dialog_reset_display),
                           dialog, G_CONNECT_SWAPPED);

  /* run a search */
  mousepad_replace_dialog_entry_changed (dialog);
}



void
mousepad_replace_dialog_set_text (MousepadReplaceDialog *dialog,
                                  const gchar           *text)
{
  gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), text);
  gtk_editable_select_region (GTK_EDITABLE (dialog->search_entry), 0, -1);
}
