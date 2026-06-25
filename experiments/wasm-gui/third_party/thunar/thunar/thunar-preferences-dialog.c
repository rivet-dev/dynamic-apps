/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4kbd-private/xfce-shortcuts-editor.h>
#include <libxfce4kbd-private/xfce-shortcuts-editor-dialog.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-compact-view.h>
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-shortcuts-view.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-statusbar.h>



static void thunar_preferences_dialog_finalize   (GObject                      *object);
static void thunar_preferences_dialog_response   (GtkDialog                    *dialog,
                                                  gint                          response);
static void thunar_preferences_dialog_configure  (ThunarPreferencesDialog     *dialog);



struct _ThunarPreferencesDialogClass
{
  XfceTitledDialogClass __parent__;
};

struct _ThunarPreferencesDialog
{
  XfceTitledDialog   __parent__;
  ThunarPreferences *preferences;
};



G_DEFINE_TYPE (ThunarPreferencesDialog, thunar_preferences_dialog, XFCE_TYPE_TITLED_DIALOG)



static gboolean
transform_icon_size_to_index (GBinding     *binding,
                              const GValue *src_value,
                              GValue       *dst_value,
                              gpointer      user_data)
{
  GEnumClass *klass;
  guint       n;

  klass = g_type_class_ref (THUNAR_TYPE_ICON_SIZE);
  for (n = 0; n < klass->n_values; ++n)
    {
      if (klass->values[n].value == g_value_get_enum (src_value))
        {
          g_value_set_int (dst_value, n);
          break;
        }
    }
  g_type_class_unref (klass);

  return TRUE;
}



static gboolean
transform_index_to_icon_size (GBinding     *binding,
                              const GValue *src_value,
                              GValue       *dst_value,
                              gpointer      user_data)
{
  GEnumClass *klass;

  klass = g_type_class_ref (THUNAR_TYPE_ICON_SIZE);
  g_value_set_enum (dst_value, klass->values[g_value_get_int (src_value)].value);
  g_type_class_unref (klass);

  return TRUE;
}



static gboolean
transform_view_string_to_index (GBinding     *binding,
                                const GValue *src_value,
                                GValue       *dst_value,
                                gpointer      user_data)
{
  GType type;

  type = g_type_from_name (g_value_get_string (src_value));
  if (type == THUNAR_TYPE_ICON_VIEW)
    g_value_set_int (dst_value, 0);
  else if (type == THUNAR_TYPE_DETAILS_VIEW)
    g_value_set_int (dst_value, 1);
  else if (type == THUNAR_TYPE_COMPACT_VIEW)
    g_value_set_int (dst_value, 2);
  else
    g_value_set_int (dst_value, 3);

  return TRUE;
}



static gboolean
transform_view_index_to_string (GBinding     *binding,
                                const GValue *src_value,
                                GValue       *dst_value,
                                gpointer      user_data)
{
  switch (g_value_get_int (src_value))
    {
    case 0:
      g_value_set_static_string (dst_value, g_type_name (THUNAR_TYPE_ICON_VIEW));
      break;

    case 1:
      g_value_set_static_string (dst_value, g_type_name (THUNAR_TYPE_DETAILS_VIEW));
      break;

    case 2:
      g_value_set_static_string (dst_value, g_type_name (THUNAR_TYPE_COMPACT_VIEW));
      break;

    default:
      g_value_set_static_string (dst_value, g_type_name (G_TYPE_NONE));
      break;
    }

  return TRUE;
}



static gboolean
transform_string_to_uint64 (GBinding     *binding,
                            const GValue *src_value,
                            GValue       *dst_value,
                            gpointer      user_data)
{
  const gchar *string = g_value_get_string (src_value);
  g_value_set_uint64 (dst_value, g_ascii_strtoull (string, NULL, 10));
  return TRUE;
}



static gboolean
transform_uint64_to_string (GBinding     *binding,
                            const GValue *src_value,
                            GValue       *dst_value,
                            gpointer      user_data)
{
  gchar *string = g_strdup_printf ("%ld", g_value_get_uint64 (src_value));
  g_value_set_string (dst_value, string);
  g_free (string);
  return TRUE;
}



static void
thunar_preferences_dialog_class_init (ThunarPreferencesDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_preferences_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_preferences_dialog_response;
}



static void
on_date_format_changed (GtkWidget *combo,
                        GtkWidget *customFormat)
{
  GtkComboBox *combobox = GTK_COMBO_BOX (combo);

  _thunar_return_if_fail (GTK_IS_COMBO_BOX (combobox));
  _thunar_return_if_fail (GTK_IS_WIDGET (customFormat));

  if (gtk_combo_box_get_active (combobox) == THUNAR_DATE_STYLE_CUSTOM || gtk_combo_box_get_active (combobox) == THUNAR_DATE_STYLE_CUSTOM_SIMPLE)
    gtk_widget_set_visible (customFormat, TRUE);
  else
    gtk_widget_set_visible (customFormat, FALSE);
}



static void
thunar_preferences_dialog_init (ThunarPreferencesDialog *dialog)
{
  ThunarDateStyle date_style;
  GtkAdjustment  *adjustment;
  GtkWidget      *notebook;
  GtkWidget      *button;
  GtkWidget      *combo;
  GtkWidget      *entry;
  GtkWidget      *image;
  GtkWidget      *frame;
  GtkWidget      *label;
  GtkWidget      *range;
  GtkWidget      *grid;
  GtkWidget      *hbox;
  GtkWidget      *ibox;
  GtkWidget      *vbox;
  GtkWidget      *infobar;
  GEnumClass     *type;
  gchar          *date;
  gint            row = 0;

  /* grab a reference on the preferences */
  dialog->preferences = thunar_preferences_get ();

  /* configure the dialog properties */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "org.xfce.thunar");
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("File Manager Preferences"));

  xfce_titled_dialog_create_action_area (XFCE_TITLED_DIALOG (dialog));

  /* add the "Close" button */
  button = gtk_button_new_with_mnemonic (_("_Close"));
  image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_CLOSE);

  gtk_widget_show (button);

  /* add the "Help" button */
  button = gtk_button_new_with_mnemonic (_("_Help"));
  image = gtk_image_new_from_icon_name ("help-browser", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_HELP);
  gtk_widget_show (button);

  notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);


  /*
     Display
   */
  label = gtk_label_new (_("Display"));
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 12, "spacing", 18, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("View Settings"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("View _new folders using:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Icon View"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("List View"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Compact View"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Last Active View"));
  g_object_bind_property_full (G_OBJECT (dialog->preferences), "default-view",
                               G_OBJECT (combo), "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_view_string_to_index,
                               transform_view_index_to_string,
                               NULL, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("_Remember view settings for each folder"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-directory-specific-settings",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button,
                               _("Select this option to remember view type, zoom level, sort column, and sort order individually for each folder"));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);
  if (!thunar_g_vfs_metadata_is_supported ())
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_widget_set_sensitive (button, FALSE);
      gtk_widget_set_tooltip_text (button, _("gvfs metadata support is required"));
    }

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Sort _folders before files"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-folders-first",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to list folders before files when you sort a folder."));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Show file size in binary format"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-file-size-binary",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to show file size in binary format instead of decimal."));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Thumbnails"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("Show thumbnails:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Never"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Local Files Only"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Always"));
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "misc-thumbnail-mode",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_thumbnail_mode_get_type, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  label = gtk_label_new_with_mnemonic (_("Only show thumbnails for files smaller than:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "524288", _("512KiB"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "1048576", _("1MiB"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "10485760", _("10MiB"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "104857600", _("100MiB"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "1073741824", _("1GiB"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "10737418240", _("10GiB"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "0", _("Unlimited"));
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "misc-thumbnail-max-file-size",
                               G_OBJECT (combo),
                               "active-id",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_uint64_to_string,
                               transform_string_to_uint64,
                               NULL, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Draw frames around thumbnails"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-thumbnail-draw-frames",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to draw black frames around thumbnails."));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Icon View"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  button = gtk_check_button_new_with_mnemonic (_("_Text beside icons"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-text-beside-icons",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to place the icon captions for items "
                                         "beside the icon rather than below the icon."));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Window Icon"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  button = gtk_check_button_new_with_mnemonic (_("Use current folder icon"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-change-window-icon",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to use the current folder icon as window icon"));

  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Date"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("_Format:"));
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  for (date_style = THUNAR_DATE_STYLE_SIMPLE; date_style <= THUNAR_DATE_STYLE_DDMMYYYY; ++date_style)
    {
      date = thunar_util_humanize_file_time (time (NULL), date_style, NULL);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), date);
      g_free (date);
    }

  /* TRANSLATORS: custom date format */
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Custom"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Today / Custom"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-date-style",
                          G_OBJECT (combo),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  entry = gtk_entry_new ();
  /* TRANSLATORS: Please do not translate the first column (specifiers), 'strftime' and of course '\n' */
  gtk_widget_set_tooltip_text (entry, _("Custom date format to apply.\n\n"
                                        "The most common specifiers are:\n"
                                        "%d day of month\n"
                                        "%m month\n"
                                        "%Y year including century\n"
                                        "%H hour\n"
                                        "%M minute\n"
                                        "%S second\n\n"
                                        "For a complete list, search 'Glib.DateTime.format'."));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-date-custom-style",
                          G_OBJECT (entry),
                          "text",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (on_date_format_changed), entry);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, row, 1, 1);
  if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) == THUNAR_DATE_STYLE_CUSTOM || gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) == THUNAR_DATE_STYLE_CUSTOM_SIMPLE)
    gtk_widget_set_visible (entry, TRUE);

  /*
     Side Pane
   */
  label = gtk_label_new (_("Side Pane"));
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 12, "spacing", 18, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Shortcuts Pane"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("_Icon Size:"));
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("16px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("24px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("32px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("48px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("64px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("96px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("128px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("160px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("192px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("256px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("512px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("1024px"));
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "shortcuts-icon-size",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_icon_size_to_index,
                               transform_index_to_icon_size,
                               NULL, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Show Icon _Emblems"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "shortcuts-icon-emblems",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to display icon emblems in the shortcuts pane for all folders "
                                         "for which emblems have been defined in the folders properties dialog."));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 2, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Tree Pane"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("Icon _Size:"));
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("16px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("24px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("32px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("48px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("64px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("96px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("128px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("160px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("192px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("256px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("512px"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("1024px"));
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "tree-icon-size",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_icon_size_to_index,
                               transform_index_to_icon_size,
                               NULL, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Image Preview"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Show Icon E_mblems"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "tree-icon-emblems",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to display icon emblems in the tree pane for all folders "
                                         "for which emblems have been defined in the folders properties dialog."));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 2, 1);
  gtk_widget_show (button);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("Image Preview Mode:"));
  gtk_widget_set_tooltip_text (label, _(
                                        "- Standalone: Image Preview uses a separate pane\n"
                                        "- Embedded: Image Preview is embedded in the left sidepane\n"
                                      ));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  type = g_type_class_ref (THUNAR_TYPE_IMAGE_PREVIEW_MODE);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_IMAGE_PREVIEW_MODE_STANDALONE)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED)->value_nick));
  g_type_class_unref (type);
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "misc-image-preview-mode",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_verify_file_get_type, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);


  /*
     Behavior
   */
  label = gtk_label_new (_("Behavior"));
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 12, "spacing", 18, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Navigation"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  button = gtk_radio_button_new_with_mnemonic (NULL, _("_Single click to activate items"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-single-click",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (g_object_notify), "active");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  /* next row */
  row++;

  ibox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand (ibox, TRUE);
  gtk_widget_set_margin_start (ibox, 12);
  gtk_widget_set_margin_bottom (ibox, 6);
  g_object_bind_property (G_OBJECT (button), "active",
                          G_OBJECT (ibox),   "sensitive",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (ibox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), ibox, 0, row, 1, 1);
  gtk_widget_show (ibox);

  label = gtk_label_new_with_mnemonic (_("Specify the d_elay before an item gets selected\n"
                                         "when the mouse pointer is paused over it:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0f);
  gtk_box_pack_start (GTK_BOX (ibox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  range = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 2000.0, 100.0);
  gtk_scale_set_draw_value (GTK_SCALE (range), FALSE);
  gtk_widget_set_margin_top (range, 6);
  gtk_scale_add_mark (GTK_SCALE (range), 0.0, GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (range), 1000.0, GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (range), 2000.0, GTK_POS_BOTTOM, NULL);
  gtk_widget_set_tooltip_text (range, _("When single-click activation is enabled, pausing the mouse pointer over an item "
                                        "will automatically select that item after the chosen delay. You can disable this "
                                        "behavior by moving the slider to the left-most position. This behavior may be "
                                        "useful when single clicks activate items, and you want only to select the item "
                                        "without activating it."));
  gtk_box_pack_start (GTK_BOX (ibox), range, FALSE, FALSE, 0);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), range);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), range);
  gtk_widget_show (range);

  /* connect the range's adjustment to the preferences */
  adjustment = gtk_range_get_adjustment (GTK_RANGE (range));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-single-click-timeout",
                          G_OBJECT (adjustment),
                          "value",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (ibox), hbox, FALSE, FALSE, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Disabled"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Medium"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Long"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* next row */
  row++;

  button = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button), _("_Double click to activate items"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-single-click",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_INVERT_BOOLEAN  | G_BINDING_SYNC_CREATE);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (g_object_notify), "active");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Tabs and Windows"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  button = gtk_check_button_new_with_mnemonic (_("Open folders in new tabs on middle click"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-middle-click-in-tab",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to open a new tab on middle click instead of a new window"));
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Open new thunar instances as tabs"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-open-new-window-as-tab",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to open new thunar instances as tabs in an existing thunar window"));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Show full directory paths in tab titles"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-full-path-in-tab-title",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to show the full directory path in the tab title instead of only the directory name"));
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  /* next row */
  row++;

  button = gtk_check_button_new_with_mnemonic (_("Restore tabs on startup"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "last-restore-tabs",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Select this option to restore your tabs as they were last time you used Thunar"));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
      gtk_widget_show (frame);

      label = gtk_label_new (_("Context Menu"));
      gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
      gtk_frame_set_label_widget (GTK_FRAME (frame), label);
      gtk_widget_show (label);

      /* new grid */
      row = 0;

      grid = gtk_grid_new ();
      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
      gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
      gtk_container_add (GTK_CONTAINER (frame), grid);
      gtk_widget_show (grid);

      button = gtk_check_button_new_with_mnemonic (_("Show action to permanently delete files and folders"));
      g_object_bind_property (G_OBJECT (dialog->preferences),
                              "misc-show-delete-action",
                              G_OBJECT (button),
                              "active",
                              G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_widget_set_tooltip_text (button, _("Select this option to show the 'Delete' action in the context menu"));
      gtk_widget_set_hexpand (button, TRUE);
      gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
      gtk_widget_show (button);
    }

  /*
     Advanced
   */

  label = gtk_label_new (_("Advanced"));
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 12, "spacing", 18, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("File Transfer"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("Transfer files in parallel:"));
  gtk_widget_set_tooltip_text (label, _(
                                        "Indicates the behavior during multiple copies:\n"
                                        "- Always: all copies are done simultaneously\n"
                                        "- Local Files Only: simultaneous copies for local (not remote, not attached) files\n"
                                        "- Local Files On Same Devices Only: if all files are locals but on different devices (disks, mount points), copies will be sequential\n"
                                        "- Never: all copies are done sequentially"
                                      ));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Always"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Local Files Only"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Local Files On Same Devices Only"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Never"));
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "misc-parallel-copy-mode",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_parallel_copy_mode_get_type, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  label = gtk_label_new_with_mnemonic (_("Use intermediate file on copy:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);
  gtk_widget_set_tooltip_text (label, _("Use intermediate file '*.partial~' to copy files. "
                                        "This will prevent fragmented files. "
                                        "The new file will only be shown after the copy was successfully finished."));

  combo = gtk_combo_box_text_new ();
  type = g_type_class_ref (THUNAR_TYPE_USE_PARTIAL_MODE);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_USE_PARTIAL_MODE_DISABLED)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_USE_PARTIAL_MODE_REMOTE_ONLY)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_USE_PARTIAL_MODE_ALWAYS)->value_nick));
  g_type_class_unref (type);
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "misc-transfer-use-partial",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_use_partial_get_type, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* next row */
  row++;

  label = gtk_label_new_with_mnemonic (_("Verify file checksum on copy:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);
  gtk_widget_set_tooltip_text (label, _("Compare original file and copy with checksum. "
                                        "This will prevent corrupted files. "
                                        "Enabling this option will largely increase copy time."));

  combo = gtk_combo_box_text_new ();
  type = g_type_class_ref (THUNAR_TYPE_VERIFY_FILE_MODE);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_VERIFY_FILE_MODE_DISABLED)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_VERIFY_FILE_MODE_REMOTE_ONLY)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (type, THUNAR_VERIFY_FILE_MODE_ALWAYS)->value_nick));
  g_type_class_unref (type);
  g_object_bind_property_full (G_OBJECT (dialog->preferences),
                               "misc-transfer-verify-file",
                               G_OBJECT (combo),
                               "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_verify_file_get_type, NULL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Search"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("Include subfolders:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Local Files Only"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Always"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Never"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-recursive-search",
                          G_OBJECT (combo),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Folder Permissions"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("When changing the permissions of a folder, you\n"
                                         "can also apply the changes to the contents of the\n"
                                         "folder. Select the default behavior below:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* next row */
  row++;

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Ask every time"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Apply to Folder and Contents"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Apply to Folder Only"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-recursive-permissions",
                          G_OBJECT (combo),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 0, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Security"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (grid), hbox, 0, row, 1, 1);
  gtk_widget_show (hbox);

  button = gtk_check_button_new_with_mnemonic (_("Execute shell scripts"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-exec-shell-scripts-by-default",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_MENU);
  gtk_widget_set_tooltip_text (image, _("Allowing untrusted programs to run presents a security risk to your system."));
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Volume Management"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* add check button to enable/disable auto mounting */
  button = gtk_check_button_new_with_mnemonic (_("Enable _Volume Management"));
  g_object_bind_property (G_OBJECT (dialog->preferences),
                          "misc-volume-management",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  gtk_widget_show (button);

  /* next row */
  row++;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  g_object_bind_property (G_OBJECT (button), "active",
                          G_OBJECT (label),  "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (label), "activate-link", G_CALLBACK (thunar_preferences_dialog_configure), dialog);
  gtk_label_set_markup (GTK_LABEL (label), _("<a href=\"volman-config:\">Configure</a> the management of removable drives,\n"
                                             "devices and media."));
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* check the most important gvfs-backends, and inform if they are missing */
  if (!thunar_g_vfs_is_uri_scheme_supported ("trash")    ||
      !thunar_g_vfs_is_uri_scheme_supported ("computer") || /* support for removable media */
      !thunar_g_vfs_is_uri_scheme_supported ("sftp") )
    {
      frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
      gtk_widget_show (frame);

      label = gtk_label_new (_("Missing dependencies"));
      gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
      gtk_frame_set_label_widget (GTK_FRAME (frame), label);
      gtk_widget_show (label);

      infobar = gtk_info_bar_new ();
      gtk_container_set_border_width (GTK_CONTAINER (infobar), 12);
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), _("It looks like <a href=\"https://wiki.gnome.org/Projects/gvfs\">gvfs</a> is not available.\n"
                                                 "Important features including trash support,\n"
                                                 "removable media and remote location browsing\n"
                                                 "will not work. <a href=\"https://docs.xfce.org/xfce/thunar/unix-filesystem#gnome_virtual_file_system\">[Read more]</a>"));
      vbox = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));
      gtk_container_add (GTK_CONTAINER (vbox), label);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_WARNING);
      gtk_widget_show (label);
      gtk_widget_show (infobar);
      gtk_container_add (GTK_CONTAINER (frame), infobar);
    }

  /*
   Shortcuts
  */
  label = gtk_label_new (_("Shortcuts"));
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 12, "spacing", 18, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_IN, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  grid = xfce_shortcuts_editor_new (13,
                                    _("Window"), thunar_window_get_action_entries (), (size_t)THUNAR_WINDOW_N_ACTIONS,
                                    _("View"), thunar_standard_view_get_action_entries (), (size_t)THUNAR_STANDARD_VIEW_N_ACTIONS,
                                    _("Launcher"), thunar_action_manager_get_action_entries (), (size_t)THUNAR_ACTION_MANAGER_N_ACTIONS,
                                    _("Status Bar"), thunar_statusbar_get_action_entries (), (size_t)THUNAR_STATUS_BAR_N_ACTIONS
                                    );
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);
}



static void
thunar_preferences_dialog_finalize (GObject *object)
{
  ThunarPreferencesDialog *dialog = THUNAR_PREFERENCES_DIALOG (object);

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (dialog->preferences));

  (*G_OBJECT_CLASS (thunar_preferences_dialog_parent_class)->finalize) (object);
}



static void
thunar_preferences_dialog_response (GtkDialog *dialog,
                                    gint       response)
{
  if (G_UNLIKELY (response == GTK_RESPONSE_HELP))
    {
      /* open the preferences section of the user manual */
      xfce_dialog_show_help (GTK_WINDOW (dialog), "thunar",
                             "preferences", NULL);
    }
  else
    {
      /* close the preferences dialog */
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}



static void
thunar_preferences_dialog_configure (ThunarPreferencesDialog *dialog)
{
  GError    *err = NULL;
  gchar     *argv[3];
  GdkScreen *screen;
  char      *display = NULL;

  _thunar_return_if_fail (THUNAR_IS_PREFERENCES_DIALOG (dialog));

  /* prepare the argument vector */
  argv[0] = (gchar *) "thunar-volman";
  argv[1] = (gchar *) "--configure";
  argv[2] = NULL;

  screen = gtk_widget_get_screen (GTK_WIDGET (dialog));

  if (screen != NULL)
    display = g_strdup (gdk_display_get_name (gdk_screen_get_display (screen)));

  /* invoke the configuration interface of thunar-volman */
  if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, thunar_setup_display_cb, display, NULL, &err))
    {
      /* tell the user that we failed to come up with the thunar-volman configuration dialog */
      thunar_dialogs_show_error (dialog, err, _("Failed to display the volume management settings"));
      g_error_free (err);
    }

  g_free (display);
}



/**
 * thunar_preferences_dialog_new:
 * @parent : a #GtkWindow or %NULL.
 *
 * Allocates a new #ThunarPreferencesDialog widget.
 *
 * Return value: the newly allocated #ThunarPreferencesDialog.
 **/
GtkWidget*
thunar_preferences_dialog_new (GtkWindow *parent)
{
  GtkWidget    *dialog;
  gint          root_x, root_y;
  gint          window_width, window_height;
  gint          dialog_width, dialog_height;
  GdkMonitor *  monitor;
  GdkRectangle  geometry;
  GdkScreen    *screen;

  dialog = g_object_new (THUNAR_TYPE_PREFERENCES_DIALOG, NULL);

  /* fake gtk_window_set_transient_for() for centering the window on
   * the parent window. See bug #3586. */
  if (G_LIKELY (parent != NULL))
    {
      screen = gtk_window_get_screen (parent);
      gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      gtk_widget_realize (dialog);

      /* get the size and position on the windows */
      gtk_window_get_position (parent, &root_x, &root_y);
      gtk_window_get_size (parent, &window_width, &window_height);
      gtk_window_get_size (GTK_WINDOW (dialog), &dialog_width, &dialog_height);

      /* get the monitor geometry of the monitor with the parent window */
      monitor = gdk_display_get_monitor_at_point (gdk_display_get_default (), root_x, root_y);
      gdk_monitor_get_geometry (monitor, &geometry);

      /* center the dialog on the window and clamp on the monitor */
      root_x += (window_width - dialog_width) / 2;
      root_x = CLAMP (root_x, geometry.x, geometry.x + geometry.width - dialog_width);
      root_y += (window_height - dialog_height) / 2;
      root_y = CLAMP (root_y, geometry.y, geometry.y + geometry.height - dialog_height);

      /* move the dialog */
      gtk_window_move (GTK_WINDOW (dialog), root_x, root_y);
    }

  return dialog;
}
