/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

#include <gdk/gdkkeysyms.h>

#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-chooser-button.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-emblem-chooser.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-image.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-permissions-chooser.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-size-label.h>
#include <thunar/thunar-thumbnailer.h>
#include <thunar/thunar-util.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILES,
  PROP_FILE_SIZE_BINARY,
  PROP_SHOW_FILE_HIGHLIGHT_TAB,
};

/* Signal identifiers */
enum
{
  RELOAD,
  LAST_SIGNAL,
};

/* Notebook Pages */
enum
{
  NOTEBOOK_PAGE_GENERAL,
  NOTEBOOK_PAGE_EMBLEMS,
  NOTEBOOK_PAGE_HIGHLIGHT,
  NOTEBOOK_PAGE_PERMISSIONS
};


static void     thunar_properties_dialog_constructed          (GObject                     *object);
static void     thunar_properties_dialog_dispose              (GObject                     *object);
static void     thunar_properties_dialog_finalize             (GObject                     *object);
static void     thunar_properties_dialog_get_property         (GObject                     *object,
                                                               guint                        prop_id,
                                                               GValue                      *value,
                                                               GParamSpec                  *pspec);
static void     thunar_properties_dialog_set_property         (GObject                     *object,
                                                               guint                        prop_id,
                                                               const GValue                *value,
                                                               GParamSpec                  *pspec);
static void     thunar_properties_dialog_response             (GtkDialog                   *dialog,
                                                               gint                         response);
static gboolean thunar_properties_dialog_reload               (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_name_activate        (GtkWidget                   *entry,
                                                               ThunarPropertiesDialog      *dialog);
static gboolean thunar_properties_dialog_name_focus_out_event (GtkWidget                   *entry,
                                                               GdkEventFocus               *event,
                                                               ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_icon_button_clicked  (GtkWidget                   *button,
                                                               ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_update               (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_update_providers     (ThunarPropertiesDialog      *dialog);
static GList   *thunar_properties_dialog_get_files            (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_reset_highlight      (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_apply_highlight      (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_set_foreground       (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_set_background       (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_update_apply_button  (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_colorize_example_box (ThunarPropertiesDialog      *dialog,
                                                               const gchar                 *background,
                                                               const gchar                 *foreground);
static void     thunar_properties_dialog_color_editor_changed (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_color_editor_close   (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_notebook_page_changed(ThunarPropertiesDialog      *dialog);


struct _ThunarPropertiesDialogClass
{
  ThunarAbstractDialogClass __parent__;

  /* signals */
  gboolean (*reload) (ThunarPropertiesDialog *dialog);
};

struct _ThunarPropertiesDialog
{
  ThunarAbstractDialog    __parent__;

  ThunarxProviderFactory *provider_factory;
  GList                  *provider_pages;

  ThunarPreferences      *preferences;

  GList                  *files;
  gboolean                file_size_binary;
  gboolean                show_file_highlight_tab;
  
  ThunarThumbnailer      *thumbnailer;
  guint                   thumbnail_request;

  XfceFilenameInput      *name_entry;

  GtkWidget              *notebook;
  GtkWidget              *icon_button;
  GtkWidget              *icon_image;
  GtkWidget              *names_label;
  GtkWidget              *single_box;
  GtkWidget              *kind_ebox;
  GtkWidget              *kind_label;
  GtkWidget              *openwith_chooser;
  GtkWidget              *link_label;
  GtkWidget              *location_label;
  GtkWidget              *origin_label;
  GtkWidget              *created_label;
  GtkWidget              *deleted_label;
  GtkWidget              *modified_label;
  GtkWidget              *accessed_label;
  GtkWidget              *capacity_vbox;
  GtkWidget              *capacity_label;
  GtkWidget              *freespace_vbox;
  GtkWidget              *freespace_bar;
  GtkWidget              *freespace_label;
  GtkWidget              *volume_image;
  GtkWidget              *volume_label;
  GtkWidget              *permissions_chooser;
  GtkWidget              *content_label;
  GtkWidget              *content_value_label;
  GtkWidget              *color_chooser;
  GtkWidget              *example_box;
  GtkWidget              *highlight_buttons;
  GtkWidget              *highlight_apply_button;
  GtkWidget              *editor_button;

  gchar                  *foreground_color;
  gchar                  *background_color;
};



G_DEFINE_TYPE (ThunarPropertiesDialog, thunar_properties_dialog, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_properties_dialog_class_init (ThunarPropertiesDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkBindingSet  *binding_set;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_properties_dialog_dispose;
  gobject_class->finalize = thunar_properties_dialog_finalize;
  gobject_class->get_property = thunar_properties_dialog_get_property;
  gobject_class->set_property = thunar_properties_dialog_set_property;
  gobject_class->constructed = thunar_properties_dialog_constructed;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_properties_dialog_response;

  klass->reload = thunar_properties_dialog_reload;

  /**
   * ThunarPropertiesDialog:files:
   *
   * The list of currently selected files whose properties are displayed by
   * this #ThunarPropertiesDialog. This property may also be %NULL
   * in which case nothing is displayed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILES,
                                   g_param_spec_boxed ("files", "files", "files",
                                                        THUNARX_TYPE_FILE_INFO_LIST,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPropertiesDialog:file_size_binary:
   *
   * Whether the file size should be shown in binary or decimal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE_SIZE_BINARY,
                                   g_param_spec_boolean ("file-size-binary",
                                                         "FileSizeBinary",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPropertiesDialog:show_file_highlight_tab:
   *
   * Whether the highlight tab should be shown
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_FILE_HIGHLIGHT_TAB,
                                   g_param_spec_boolean ("show-file-hightlight-tab",
                                                         "ShowFileHighlightTab",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * ThunarPropertiesDialog::reload:
   * @dialog : a #ThunarPropertiesDialog.
   *
   * Emitted whenever the user requests reset the reload the
   * file properties. This is an internal signal used to bind
   * the action to keys.
   **/
  g_signal_new (I_("reload"),
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                G_STRUCT_OFFSET (ThunarPropertiesDialogClass, reload),
                g_signal_accumulator_true_handled, NULL,
                _thunar_marshal_BOOLEAN__VOID,
                G_TYPE_BOOLEAN, 0);

  /* setup the key bindings for the properties dialog */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F5, 0, "reload", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_r, GDK_CONTROL_MASK, "reload", 0);
}



static void
thunar_properties_dialog_init (ThunarPropertiesDialog *dialog)
{
  /* acquire a reference on the preferences and monitor the
     "misc-date-style" and "misc-file-size-binary" settings */
  dialog->preferences = thunar_preferences_get ();
  g_signal_connect_swapped (G_OBJECT (dialog->preferences), "notify::misc-date-style",
                            G_CALLBACK (thunar_properties_dialog_reload), dialog);
  g_object_bind_property (G_OBJECT (dialog->preferences), "misc-file-size-binary",
                          G_OBJECT (dialog), "file-size-binary",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (dialog->preferences), "notify::misc-file-size-binary",
                            G_CALLBACK (thunar_properties_dialog_reload), dialog);

  /* create a new thumbnailer */
  dialog->thumbnailer = thunar_thumbnailer_get ();
  dialog->thumbnail_request = 0;

  dialog->provider_factory = thunarx_provider_factory_get_default ();
}



static void
thunar_properties_dialog_constructed (GObject *object)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  GtkWidget *chooser;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *box;
  GtkWidget *spacer;
  guint      row = 0;
  GtkWidget *image;
  GtkWidget *button;
  GtkWidget *infobar;
  GtkWidget *frame;

  G_OBJECT_CLASS (thunar_properties_dialog_parent_class)->constructed (object);
  
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Help"), GTK_RESPONSE_HELP,
                          _("_Close"), GTK_RESPONSE_CLOSE,
                          NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 450);

  dialog->notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (dialog->notebook), 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), dialog->notebook, TRUE, TRUE, 0);
  gtk_widget_show (dialog->notebook);

  grid = gtk_grid_new ();
  label = gtk_label_new (_("General"));
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), grid, label);
  gtk_widget_show (label);
  gtk_widget_show (grid);


  /*
     First box (icon, name) for 1 file
   */
  dialog->single_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (grid), dialog->single_box, 0, row, 1, 1);

  dialog->icon_button = gtk_button_new ();
  g_signal_connect (G_OBJECT (dialog->icon_button), "clicked", G_CALLBACK (thunar_properties_dialog_icon_button_clicked), dialog);
  gtk_box_pack_start (GTK_BOX (dialog->single_box), dialog->icon_button, FALSE, TRUE, 0);
  gtk_widget_show (dialog->icon_button);

  dialog->icon_image = thunar_image_new ();
  gtk_box_pack_start (GTK_BOX (dialog->single_box), dialog->icon_image, FALSE, TRUE, 0);
  gtk_widget_set_valign (GTK_WIDGET (dialog->icon_image), GTK_ALIGN_START);
  gtk_widget_show (dialog->icon_image);

  label = gtk_label_new_with_mnemonic (_("_Name:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_box_pack_end (GTK_BOX (dialog->single_box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* set up the widget for entering the filename */
  dialog->name_entry = g_object_new (XFCE_TYPE_FILENAME_INPUT, NULL);
  gtk_widget_set_hexpand (GTK_WIDGET (dialog->name_entry), TRUE);
  gtk_widget_set_valign (GTK_WIDGET (dialog->name_entry), GTK_ALIGN_CENTER);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (xfce_filename_input_get_entry (dialog->name_entry)));
  gtk_widget_show_all (GTK_WIDGET (dialog->name_entry));

  g_signal_connect (G_OBJECT (xfce_filename_input_get_entry (dialog->name_entry)),
                    "activate", G_CALLBACK (thunar_properties_dialog_name_activate), dialog);
  g_signal_connect (G_OBJECT (xfce_filename_input_get_entry (dialog->name_entry)),
                    "focus-out-event", G_CALLBACK (thunar_properties_dialog_name_focus_out_event), dialog);

  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (dialog->name_entry), 1, row, 1, 1);
  g_object_bind_property (G_OBJECT (dialog->single_box), "visible",
                          G_OBJECT (dialog->name_entry), "visible",
                          G_BINDING_SYNC_CREATE);

  ++row;


  /*
     First box (icon, name) for multiple files
   */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (grid), box, 0, row, 1, 1);
  g_object_bind_property (G_OBJECT (dialog->single_box), "visible",
                          G_OBJECT (box),                "visible",
                          G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);

  image = gtk_image_new_from_icon_name ("text-x-generic", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("Names:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_box_pack_end (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  dialog->names_label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (dialog->names_label), 0.0f);
  gtk_widget_set_hexpand (dialog->names_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->names_label, 1, row, 1, 1);
  gtk_label_set_ellipsize (GTK_LABEL (dialog->names_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_selectable (GTK_LABEL (dialog->names_label), TRUE);
  g_object_bind_property (G_OBJECT (box),                 "visible",
                          G_OBJECT (dialog->names_label), "visible",
                          G_BINDING_SYNC_CREATE);


  ++row;


  /*
     Second box (kind, open with, link target)
   */
  label = gtk_label_new (_("Kind:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->kind_ebox = gtk_event_box_new ();
  gtk_event_box_set_above_child (GTK_EVENT_BOX (dialog->kind_ebox), TRUE);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (dialog->kind_ebox), FALSE);
  g_object_bind_property (G_OBJECT (dialog->kind_ebox), "visible",
                          G_OBJECT (label),             "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->kind_ebox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->kind_ebox, 1, row, 1, 1);
  gtk_widget_show (dialog->kind_ebox);

  dialog->kind_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->kind_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (dialog->kind_label), PANGO_ELLIPSIZE_END);
  gtk_container_add (GTK_CONTAINER (dialog->kind_ebox), dialog->kind_label);
  gtk_widget_show (dialog->kind_label);

  ++row;

  label = gtk_label_new_with_mnemonic (_("_Open With:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->openwith_chooser = thunar_chooser_button_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->openwith_chooser);
  g_object_bind_property (G_OBJECT (dialog->openwith_chooser), "visible",
                          G_OBJECT (label),                    "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->openwith_chooser, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->openwith_chooser, 1, row, 1, 1);
  gtk_widget_show (dialog->openwith_chooser);

  ++row;

  label = gtk_label_new (_("Link Target:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->link_label = g_object_new (GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->link_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->link_label), "visible",
                          G_OBJECT (label),              "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->link_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->link_label, 1, row, 1, 1);
  gtk_widget_show (dialog->link_label);

  ++row;

  /* TRANSLATORS: Try to come up with a short translation of "Original Path" (which is the path
   * where the trashed file/folder was located before it was moved to the trash), otherwise the
   * properties dialog width will be messed up.
   */
  label = gtk_label_new (_("Original Path:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->origin_label = g_object_new (GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->origin_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->origin_label), "visible",
                          G_OBJECT (label), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->origin_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->origin_label, 1, row, 1, 1);
  gtk_widget_show (dialog->origin_label);

  ++row;

  label = gtk_label_new (_("Location:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->location_label = g_object_new (GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->location_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->location_label), "visible",
                          G_OBJECT (label), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->location_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->location_label, 1, row, 1, 1);
  gtk_widget_show (dialog->location_label);

  ++row;


  spacer = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
  gtk_grid_attach (GTK_GRID (grid), spacer, 0, row, 2, 1);
  gtk_widget_show (spacer);

  ++row;


  /*
     Third box (deleted, modified, accessed)
   */
  label = gtk_label_new (_("Deleted:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->deleted_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->deleted_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->deleted_label), "visible",
                          G_OBJECT (label), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->deleted_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->deleted_label, 1, row, 1, 1);
  gtk_widget_show (dialog->deleted_label);

  ++row;

  label = gtk_label_new (_("Created:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->created_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->created_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->created_label), "visible",
                          G_OBJECT (label), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->created_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->created_label, 1, row, 1, 1);
  gtk_widget_show (dialog->created_label);

  ++row;

  label = gtk_label_new (_("Modified:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->modified_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->modified_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->modified_label), "visible",
                          G_OBJECT (label),                  "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->modified_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->modified_label, 1, row, 1, 1);
  gtk_widget_show (dialog->modified_label);

  ++row;

  label = gtk_label_new (_("Accessed:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->accessed_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->accessed_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->accessed_label), "visible",
                          G_OBJECT (label),                  "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (dialog->accessed_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->accessed_label, 1, row, 1, 1);
  gtk_widget_show (dialog->accessed_label);

  ++row;


  spacer = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
  gtk_grid_attach (GTK_GRID (grid), spacer, 0, row, 2, 1);
  g_object_bind_property (G_OBJECT (dialog->accessed_label), "visible",
                          G_OBJECT (spacer),                 "visible",
                          G_BINDING_SYNC_CREATE);

  ++row;


  /*
     Fourth box (size, volume, free space)
   */
  label = gtk_label_new (_("Size:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  label = thunar_size_label_new ();
  g_object_bind_property (G_OBJECT (dialog), "files",
                          G_OBJECT (label),  "files",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 1, row, 1, 1);
  gtk_widget_show (label);

  ++row;

  label = gtk_label_new (_("Content:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);
  dialog->content_label = label;

  label = thunar_content_label_new ();
  g_object_bind_property (G_OBJECT (dialog), "files",
                          G_OBJECT (label), "files",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 1, row, 1, 1);
  gtk_widget_show (label);
  dialog->content_value_label = label;

  ++row;

  label = gtk_label_new (_("Volume:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_bind_property (G_OBJECT (box),   "visible",
                          G_OBJECT (label), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_grid_attach (GTK_GRID (grid), box, 1, row, 1, 1);
  gtk_widget_show (box);

  dialog->volume_image = gtk_image_new ();
  g_object_bind_property (G_OBJECT (dialog->volume_image), "visible",
                          G_OBJECT (box),                  "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (box), dialog->volume_image, FALSE, TRUE, 0);
  gtk_widget_show (dialog->volume_image);

  dialog->volume_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->volume_label), TRUE);
  g_object_bind_property (G_OBJECT (dialog->volume_label), "visible",
                          G_OBJECT (dialog->volume_image), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (box), dialog->volume_label, TRUE, TRUE, 0);
  gtk_widget_show (dialog->volume_label);

  ++row;

  label = gtk_label_new (_("Capacity:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->capacity_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_hexpand (dialog->capacity_vbox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->capacity_vbox, 1, row, 1, 1);
  g_object_bind_property (G_OBJECT (dialog->capacity_vbox), "visible",
                          G_OBJECT (label),                 "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_show (dialog->capacity_vbox);

  dialog->capacity_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->capacity_label), TRUE);
  gtk_box_pack_start (GTK_BOX (dialog->capacity_vbox), dialog->capacity_label, TRUE, TRUE, 0);
  gtk_widget_show (dialog->capacity_label);

  ++row;

  label = gtk_label_new (_("Usage:"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  dialog->freespace_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_hexpand (dialog->freespace_vbox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), dialog->freespace_vbox, 1, row, 1, 1);
  g_object_bind_property (G_OBJECT (dialog->freespace_vbox), "visible",
                          G_OBJECT (label),                  "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_show (dialog->freespace_vbox);

  dialog->freespace_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_selectable (GTK_LABEL (dialog->freespace_label), TRUE);
  gtk_box_pack_start (GTK_BOX (dialog->freespace_vbox), dialog->freespace_label, TRUE, TRUE, 0);
  gtk_widget_show (dialog->freespace_label);

  dialog->freespace_bar = g_object_new (GTK_TYPE_PROGRESS_BAR, NULL);
  gtk_box_pack_start (GTK_BOX (dialog->freespace_vbox), dialog->freespace_bar, TRUE, TRUE, 0);
  gtk_widget_set_size_request (dialog->freespace_bar, -1, 10);
  gtk_widget_show (dialog->freespace_bar);

  ++row;

  spacer = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
  gtk_grid_attach (GTK_GRID (grid), spacer, 0, row, 2, 1);
  gtk_widget_show (spacer);

  ++row;


  /*
     Emblem chooser
   */
  label = gtk_label_new (_("Emblems"));
  chooser = thunar_emblem_chooser_new ();
  g_object_bind_property (G_OBJECT (dialog),  "files",
                          G_OBJECT (chooser), "files",
                          G_BINDING_SYNC_CREATE);
  gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), chooser, label);
  gtk_widget_show (chooser);
  gtk_widget_show (label);

  /*
     Highlight Color Chooser
   */
  if (dialog->show_file_highlight_tab)
    {
      grid = gtk_grid_new ();
      label = gtk_label_new (_("Highlight"));
      gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
      gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), grid, label);
      gtk_widget_show (label);
      gtk_widget_show (grid);

      row = 0;

      chooser = gtk_color_chooser_widget_new ();
      dialog->color_chooser = chooser;
      g_signal_connect_swapped (G_OBJECT (chooser), "notify::show-editor",
                                G_CALLBACK (thunar_properties_dialog_color_editor_changed), dialog);
      g_signal_connect_swapped (G_OBJECT (dialog->notebook), "switch-page",
                                G_CALLBACK (thunar_properties_dialog_notebook_page_changed), dialog);
      gtk_grid_attach (GTK_GRID (grid), chooser, 0, row, 1, 1);
      gtk_widget_set_vexpand (chooser, TRUE);
      gtk_widget_show (chooser);

      row++;

      /* check if gvfs metadata is supported */
      if (G_UNLIKELY (!thunar_g_vfs_metadata_is_supported ()))
        {
          frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
          gtk_grid_attach (GTK_GRID (grid), frame, 0, row, 1, 1);
          gtk_widget_set_sensitive (dialog->color_chooser, FALSE);
          gtk_widget_show (frame);

          label = gtk_label_new (_("Missing dependencies"));
          gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
          gtk_frame_set_label_widget (GTK_FRAME (frame), label);
          gtk_widget_show (label);

          infobar = gtk_info_bar_new ();
          gtk_container_set_border_width (GTK_CONTAINER (infobar), 12);
          label = gtk_label_new (NULL);
          gtk_label_set_markup (GTK_LABEL (label), _("It looks like <a href=\"https://wiki.gnome.org/Projects/gvfs\">gvfs</a> is not available.\n"
                                                    "This feature will not work. "
                                                    "<a href=\"https://docs.xfce.org/xfce/thunar/unix-filesystem#gnome_virtual_file_system\">[Read more]</a>"));
          box = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));
          gtk_container_add (GTK_CONTAINER (box), label);
          gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_WARNING);
          gtk_widget_show (label);
          gtk_widget_show (infobar);
          gtk_container_add (GTK_CONTAINER (frame), infobar);

          row++;
        }
      else
        {
          dialog->example_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
          gtk_widget_set_name (dialog->example_box, "example");
          gtk_widget_set_hexpand (dialog->example_box, TRUE);
          gtk_widget_set_margin_top (dialog->example_box, 10);
          gtk_widget_set_margin_bottom (dialog->example_box, 10);
          gtk_grid_attach (GTK_GRID (grid), dialog->example_box, 0, row, 1, 1);
          gtk_widget_show (dialog->example_box);

          image = gtk_image_new_from_icon_name ("text-x-generic", GTK_ICON_SIZE_DIALOG);
          gtk_box_pack_start (GTK_BOX (dialog->example_box), image, FALSE, FALSE, 5);
          gtk_widget_set_margin_top (image, 5);
          gtk_widget_set_margin_bottom (image, 5);
          gtk_widget_show (image);

          label = gtk_label_new_with_mnemonic (_("Example.txt"));
          gtk_box_pack_start (GTK_BOX (dialog->example_box), label, TRUE, TRUE, 0);
          gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
          gtk_widget_show (label);

          row++;
        }

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      dialog->highlight_buttons = box;
      gtk_widget_set_hexpand (box, TRUE);
      gtk_widget_set_vexpand (box, TRUE);
      gtk_grid_attach (GTK_GRID (grid), box, 0, row, 1, 1);
      if (G_UNLIKELY (!thunar_g_vfs_metadata_is_supported ()))
        gtk_widget_set_sensitive (box, FALSE);
      gtk_widget_show (box);

      button = gtk_button_new_with_mnemonic (_("_Apply"));
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
      g_signal_connect_swapped (G_OBJECT (button), "clicked",
                                G_CALLBACK (thunar_properties_dialog_apply_highlight), dialog);
      gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_set_vexpand (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_END);
      gtk_widget_show (button);

      dialog->highlight_apply_button = button;
      gtk_widget_set_sensitive (dialog->highlight_apply_button, FALSE);

      button = gtk_button_new_with_mnemonic (_("_Reset"));
      g_signal_connect_swapped (G_OBJECT (button), "clicked",
                                G_CALLBACK (thunar_properties_dialog_reset_highlight), dialog);
      gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_set_vexpand (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_END);
      gtk_widget_show (button);

      button = gtk_button_new_with_mnemonic (_("Set _Foreground"));
      g_signal_connect_swapped (G_OBJECT (button), "clicked",
                                G_CALLBACK (thunar_properties_dialog_set_foreground), dialog);
      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_set_vexpand (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_END);
      gtk_widget_show (button);

      button = gtk_button_new_with_mnemonic (_("Set _Background"));
      g_signal_connect_swapped (G_OBJECT (button), "clicked",
                                G_CALLBACK (thunar_properties_dialog_set_background), dialog);
      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_set_vexpand (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_END);
      gtk_widget_show (button);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      dialog->editor_button = box;
      gtk_grid_attach (GTK_GRID (grid), box, 0, row, 1, 1);

      button = gtk_button_new_from_icon_name ("go-previous", GTK_ICON_SIZE_BUTTON);
      g_signal_connect_swapped (G_OBJECT (button), "clicked",
                                G_CALLBACK (thunar_properties_dialog_color_editor_close), dialog);
      gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  /*
     Permissions chooser
   */
  label = gtk_label_new (_("Permissions"));
  dialog->permissions_chooser = thunar_permissions_chooser_new ();
  g_object_bind_property (G_OBJECT (dialog),                      "files",
                          G_OBJECT (dialog->permissions_chooser), "files",
                          G_BINDING_SYNC_CREATE);
  gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), dialog->permissions_chooser, label);
  gtk_widget_show (dialog->permissions_chooser);
  gtk_widget_show (label);
}



static void
thunar_properties_dialog_dispose (GObject *object)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  /* reset the file displayed by the dialog */
  thunar_properties_dialog_set_files (dialog, NULL);

  (*G_OBJECT_CLASS (thunar_properties_dialog_parent_class)->dispose) (object);
}



static void
thunar_properties_dialog_finalize (GObject *object)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  _thunar_return_if_fail (dialog->files == NULL);

  /* disconnect from the preferences */
  g_signal_handlers_disconnect_by_func (dialog->preferences, thunar_properties_dialog_reload, dialog);
  g_object_unref (dialog->preferences);

  /* cancel any pending thumbnailer requests */
  if (dialog->thumbnail_request > 0)
    {
      thunar_thumbnailer_dequeue (dialog->thumbnailer, dialog->thumbnail_request);
      dialog->thumbnail_request = 0;
    }

  /* release the thumbnailer */
  g_object_unref (dialog->thumbnailer);

  /* release the provider property pages */
  g_list_free_full (dialog->provider_pages, g_object_unref);

  /* drop the reference on the provider factory */
  g_object_unref (dialog->provider_factory);

  (*G_OBJECT_CLASS (thunar_properties_dialog_parent_class)->finalize) (object);
}



static void
thunar_properties_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILES:
      g_value_set_boxed (value, thunar_properties_dialog_get_files (dialog));
      break;

    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, dialog->file_size_binary);
      break;

    case PROP_SHOW_FILE_HIGHLIGHT_TAB:
      g_value_set_boolean (value, dialog->show_file_highlight_tab);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_properties_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILES:
      thunar_properties_dialog_set_files (dialog, g_value_get_boxed (value));
      break;

    case PROP_FILE_SIZE_BINARY:
      dialog->file_size_binary = g_value_get_boolean (value);
      break;

    case PROP_SHOW_FILE_HIGHLIGHT_TAB:
      dialog->show_file_highlight_tab = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_properties_dialog_response (GtkDialog *dialog,
                                   gint       response)
{
  if (response == GTK_RESPONSE_CLOSE)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
  else if (response == GTK_RESPONSE_HELP)
    {
      xfce_dialog_show_help (GTK_WINDOW (dialog), "thunar",
                             "working-with-files-and-folders",
                             "file_properties");
    }
  else if (GTK_DIALOG_CLASS (thunar_properties_dialog_parent_class)->response != NULL)
    {
      (*GTK_DIALOG_CLASS (thunar_properties_dialog_parent_class)->response) (dialog, response);
    }
}


static void
thunar_properties_file_reload_iter (gpointer data, gpointer user_data)
{
  ThunarFile *file = data;

  thunar_file_reload (file);
}

static gboolean
thunar_properties_dialog_reload (ThunarPropertiesDialog *dialog)
{
  /* reload the active files */
  g_list_foreach (dialog->files, thunar_properties_file_reload_iter, NULL);

  return dialog->files != NULL;
}



static void
thunar_properties_dialog_rename_error (ExoJob                 *job,
                                       GError                 *error,
                                       ThunarPropertiesDialog *dialog)
{
  _thunar_return_if_fail (EXO_IS_JOB (job));
  _thunar_return_if_fail (error != NULL);
  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (g_list_length (dialog->files) == 1);

  /* reset the entry display name to the original name, so the focus
     out event does not trigger the rename again by calling
     thunar_properties_dialog_name_activate */
  gtk_entry_set_text (GTK_ENTRY (xfce_filename_input_get_entry (dialog->name_entry)),
                      thunar_file_get_display_name (THUNAR_FILE (dialog->files->data)));

  /* display an error message */
  thunar_dialogs_show_error (GTK_WIDGET (dialog), error, _("Failed to rename \"%s\""),
                             thunar_file_get_display_name (THUNAR_FILE (dialog->files->data)));
}



static void
thunar_properties_dialog_rename_finished (ExoJob                 *job,
                                          ThunarPropertiesDialog *dialog)
{
  _thunar_return_if_fail (EXO_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (g_list_length (dialog->files) == 1);

  g_signal_handlers_disconnect_matched (job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, dialog);
  g_object_unref (job);
}



static void
thunar_properties_dialog_name_activate (GtkWidget              *entry,
                                        ThunarPropertiesDialog *dialog)
{
  const gchar *old_name;
  const gchar *new_name;
  ThunarJob   *job;
  ThunarFile  *file;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  /* check if we still have a valid file and if the user is allowed to rename */
  if (G_UNLIKELY (!gtk_widget_get_sensitive (GTK_WIDGET (xfce_filename_input_get_entry (dialog->name_entry)))
      || g_list_length (dialog->files) != 1))
    return;

  /* determine new and old name */
  file = THUNAR_FILE (dialog->files->data);
  new_name = xfce_filename_input_get_text (dialog->name_entry);
  old_name = thunar_file_get_display_name (file);
  if (g_utf8_collate (new_name, old_name) != 0)
    {
      job = thunar_io_jobs_rename_file (file, new_name, THUNAR_OPERATION_LOG_OPERATIONS);
      exo_job_launch (EXO_JOB (job));
      if (job != NULL)
        {
          g_signal_connect (job, "error", G_CALLBACK (thunar_properties_dialog_rename_error), dialog);
          g_signal_connect (job, "finished", G_CALLBACK (thunar_properties_dialog_rename_finished), dialog);
        }
    }
}



static gboolean
thunar_properties_dialog_name_focus_out_event (GtkWidget              *entry,
                                               GdkEventFocus          *event,
                                               ThunarPropertiesDialog *dialog)
{
  thunar_properties_dialog_name_activate (entry, dialog);
  return FALSE;
}



static void
thunar_properties_dialog_icon_button_clicked (GtkWidget              *button,
                                              ThunarPropertiesDialog *dialog)
{
  GtkWidget   *chooser;
  GError      *err = NULL;
  const gchar *custom_icon;
  gchar       *title;
  gchar       *icon;
  ThunarFile  *file;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));
  _thunar_return_if_fail (g_list_length (dialog->files) == 1);

  /* make sure we still have a file */
  if (G_UNLIKELY (dialog->files == NULL))
    return;

  file = THUNAR_FILE (dialog->files->data);

  /* allocate the icon chooser */
  title = g_strdup_printf (_("Select an Icon for \"%s\""), thunar_file_get_display_name (file));
  chooser = exo_icon_chooser_dialog_new (title, GTK_WINDOW (dialog),
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_OK"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  g_free (title);

  /* use the custom_icon of the file as default */
  custom_icon = thunar_file_get_custom_icon (file);
  if (G_LIKELY (custom_icon != NULL && *custom_icon != '\0'))
    exo_icon_chooser_dialog_set_icon (EXO_ICON_CHOOSER_DIALOG (chooser), custom_icon);

  /* run the icon chooser dialog and make sure the dialog still has a file */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT && file != NULL)
    {
      /* determine the selected icon and use it for the file */
      icon = exo_icon_chooser_dialog_get_icon (EXO_ICON_CHOOSER_DIALOG (chooser));
      if (!thunar_file_set_custom_icon (file, icon, &err))
        {
          /* hide the icon chooser dialog first */
          gtk_widget_hide (chooser);

          /* tell the user that we failed to change the icon of the .desktop file */
          thunar_dialogs_show_error (GTK_WIDGET (dialog), err,
                                     _("Failed to change icon of \"%s\""),
                                     thunar_file_get_display_name (file));
          g_error_free (err);
        }
      g_free (icon);
    }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}



static void
thunar_properties_dialog_update_providers (ThunarPropertiesDialog *dialog)
{
  GtkWidget *label_widget;
  GList     *providers;
  GList     *pages = NULL;
  GList     *tmp;
  GList     *lp;

  /* load the property page providers from the provider factory */
  providers = thunarx_provider_factory_list_providers (dialog->provider_factory, THUNARX_TYPE_PROPERTY_PAGE_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
      /* load the pages offered by the menu providers */
      for (lp = providers; lp != NULL; lp = lp->next)
        {
          tmp = thunarx_property_page_provider_get_pages (lp->data, dialog->files);
          pages = g_list_concat (pages, tmp);
          g_object_unref (G_OBJECT (lp->data));
        }
      g_list_free (providers);
    }

  /* destroy any previous set pages */
  for (lp = dialog->provider_pages; lp != NULL; lp = lp->next)
    {
      gtk_widget_destroy (GTK_WIDGET (lp->data));
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (dialog->provider_pages);

  /* apply the new set of pages */
  dialog->provider_pages = pages;
  for (lp = pages; lp != NULL; lp = lp->next)
    {
      label_widget = thunarx_property_page_get_label_widget (THUNARX_PROPERTY_PAGE (lp->data));
      gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), GTK_WIDGET (lp->data), label_widget);
      g_object_ref (G_OBJECT (lp->data));
      gtk_widget_show (lp->data);
    }
}



static void
thunar_properties_dialog_update_single (ThunarPropertiesDialog *dialog)
{
  ThunarIconFactory *icon_factory;
  ThunarDateStyle    date_style;
  GtkIconTheme      *icon_theme;
  const gchar       *content_type;
  const gchar       *name;
  const gchar       *path;
  GVolume           *volume;
  GIcon             *gicon;
  glong              offset;
  gchar             *date_custom_style;
  gchar             *date;
  gchar             *display_name;
  gchar             *fs_string;
  gchar             *str;
  gchar             *content_type_desc = NULL;
  gchar             *volume_name;
  gchar             *volume_id;
  gchar             *volume_label;
  guint64            capacity = 0;
  gchar             *capacity_str = NULL;
  ThunarFile        *file;
  ThunarFile        *parent_file;
  gboolean           show_chooser;
  guint64            fs_free;
  guint64            fs_size;
  gdouble            fs_fraction = 0.0;
  const gchar       *background;
  const gchar       *foreground;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (g_list_length (dialog->files) == 1);
  _thunar_return_if_fail (THUNAR_IS_FILE (dialog->files->data));

  /* whether the dialog shows a single file or a group of files */
  file = THUNAR_FILE (dialog->files->data);

  /* hide the permissions chooser for trashed files */
  gtk_widget_set_visible (dialog->permissions_chooser, !thunar_file_is_trashed (file));

  /* queue a new thumbnail request */
  thunar_thumbnailer_queue_file (dialog->thumbnailer, file,
                                 &dialog->thumbnail_request, THUNAR_THUMBNAIL_SIZE_DEFAULT);

  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (dialog)));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  /* determine the style used to format dates */
  g_object_get (G_OBJECT (dialog->preferences), "misc-date-style", &date_style, NULL);
  g_object_get (G_OBJECT (dialog->preferences), "misc-date-custom-style", &date_custom_style, NULL);

  /* update the properties dialog title */
  str = g_strdup_printf (_("%s - Properties"), thunar_file_get_display_name (file));
  gtk_window_set_title (GTK_WINDOW (dialog), str);
  g_free (str);

  /* update the preview image */
  thunar_image_set_file (THUNAR_IMAGE (dialog->icon_image), file);

  /* check if the icon may be changed (only for writable .desktop files) */
  g_object_ref (G_OBJECT (dialog->icon_image));
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (dialog->icon_image)), dialog->icon_image);
  if (thunar_file_is_writable (file) && thunar_file_is_desktop_file (file))
    {
      gtk_container_add (GTK_CONTAINER (dialog->icon_button), dialog->icon_image);
      gtk_widget_show (dialog->icon_button);
    }
  else
    {
      gtk_box_pack_start (GTK_BOX (gtk_widget_get_parent (dialog->icon_button)), dialog->icon_image, FALSE, TRUE, 0);
      gtk_widget_hide (dialog->icon_button);
    }
  g_object_unref (G_OBJECT (dialog->icon_image));

  /* update the name (if it differs) */
  gtk_editable_set_editable (GTK_EDITABLE (xfce_filename_input_get_entry (dialog->name_entry)),
                             thunar_file_is_renameable (file));
  name = thunar_file_get_display_name (file);
  if (G_LIKELY (strcmp (name, xfce_filename_input_get_text (dialog->name_entry)) != 0))
    {
      gtk_entry_set_text (xfce_filename_input_get_entry (dialog->name_entry), name);

      /* grab the input focus to the name entry */
      gtk_widget_grab_focus (GTK_WIDGET (xfce_filename_input_get_entry (dialog->name_entry)));

      /* select the pre-dot part of the name */
      str = thunar_util_str_get_extension (name);
      if (G_LIKELY (str != NULL))
        {
          /* calculate the offset */
          offset = g_utf8_pointer_to_offset (name, str);

          /* select the region */
          if (G_LIKELY (offset > 0))
            gtk_editable_select_region (GTK_EDITABLE (xfce_filename_input_get_entry (dialog->name_entry)), 0, offset);
        }
    }

  /* update the content type */
  content_type = thunar_file_get_content_type (file);
  if (content_type != NULL)
    {
      content_type_desc = thunar_file_get_content_type_desc (file);
      gtk_widget_set_tooltip_text (dialog->kind_ebox, content_type);
      gtk_label_set_text (GTK_LABEL (dialog->kind_label), content_type_desc);
      g_free (content_type_desc);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (dialog->kind_label), _("unknown"));
    }

  /* update the application chooser (shown only for non-executable regular files!) */
  show_chooser = thunar_file_is_regular (file) && !thunar_file_can_execute (file);
  gtk_widget_set_visible (dialog->openwith_chooser, show_chooser);
  if (show_chooser)
    thunar_chooser_button_set_file (THUNAR_CHOOSER_BUTTON (dialog->openwith_chooser), file);

  /* update the link target */
  path = thunar_file_is_symlink (file) ? thunar_file_get_symlink_target (file) : NULL;
  if (G_UNLIKELY (path != NULL))
    {
      display_name = g_filename_display_name (path);
      gtk_label_set_text (GTK_LABEL (dialog->link_label), display_name);
      gtk_widget_show (dialog->link_label);
      g_free (display_name);
    }
  else
    {
      gtk_widget_hide (dialog->link_label);
    }

  /* update the original path */
  path = thunar_file_get_original_path (file);
  if (G_UNLIKELY (path != NULL))
    {
      display_name = g_filename_display_name (path);
      gtk_label_set_text (GTK_LABEL (dialog->origin_label), display_name);
      gtk_widget_show (dialog->origin_label);
      g_free (display_name);
    }
  else
    {
      gtk_widget_hide (dialog->origin_label);
    }

  /* update the file or folder location (parent) */
  parent_file = thunar_file_get_parent (file, NULL);
  if (G_UNLIKELY (parent_file != NULL))
    {
      display_name = g_file_get_parse_name (thunar_file_get_file (parent_file));
      gtk_label_set_text (GTK_LABEL (dialog->location_label), display_name);
      gtk_widget_show (dialog->location_label);
      g_object_unref (G_OBJECT (parent_file));
      g_free (display_name);
    }
  else
    {
      gtk_widget_hide (dialog->location_label);
    }

  /* update the created time */
  date = thunar_file_get_date_string (file, THUNAR_FILE_DATE_CREATED, date_style, date_custom_style);
  if (G_LIKELY (date != NULL))
    {
      gtk_label_set_text (GTK_LABEL (dialog->created_label), date);
      gtk_widget_show (dialog->created_label);
      g_free (date);
    }
  else
    {
      gtk_widget_hide (dialog->created_label);
    }

  /* update the deleted time */
  date = thunar_file_get_deletion_date (file, date_style, date_custom_style);
  if (G_LIKELY (date != NULL))
    {
      gtk_label_set_text (GTK_LABEL (dialog->deleted_label), date);
      gtk_widget_show (dialog->deleted_label);
      g_free (date);
    }
  else
    {
      gtk_widget_hide (dialog->deleted_label);
    }

  /* update the modified time */
  date = thunar_file_get_date_string (file, THUNAR_FILE_DATE_MODIFIED, date_style, date_custom_style);
  if (G_LIKELY (date != NULL))
    {
      gtk_label_set_text (GTK_LABEL (dialog->modified_label), date);
      gtk_widget_show (dialog->modified_label);
      g_free (date);
    }
  else
    {
      gtk_widget_hide (dialog->modified_label);
    }

  /* update the accessed time */
  date = thunar_file_get_date_string (file, THUNAR_FILE_DATE_ACCESSED, date_style, date_custom_style);
  if (G_LIKELY (date != NULL))
    {
      gtk_label_set_text (GTK_LABEL (dialog->accessed_label), date);
      gtk_widget_show (dialog->accessed_label);
      g_free (date);
    }
  else
    {
      gtk_widget_hide (dialog->accessed_label);
    }

  /* update the free space (only for folders) */
  if (thunar_file_is_directory (file))
    {
      fs_string = thunar_g_file_get_free_space_string (thunar_file_get_file (file),
                                                       dialog->file_size_binary);
      if (thunar_g_file_get_free_space (thunar_file_get_file (file), &fs_free, &fs_size)
          && fs_size > 0)
        {
          /* free disk space fraction */
          fs_fraction = ((fs_size - fs_free) * 100 / fs_size);
        }
      if (fs_string != NULL)
        {
          gtk_label_set_text (GTK_LABEL (dialog->freespace_label), fs_string);
          gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->freespace_bar), fs_fraction / 100);
          gtk_widget_show (dialog->freespace_vbox);
          g_free (fs_string);
        }
      else
        {
          gtk_widget_hide (dialog->freespace_vbox);
        }
    }
  else
    {
      gtk_widget_hide (dialog->freespace_vbox);
    }

  /* update the capacity (space of containing volume) */
  if (thunar_g_file_get_free_space (thunar_file_get_file (file), NULL, &capacity))
    capacity_str = g_format_size_full (capacity, dialog->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
  gtk_label_set_text (GTK_LABEL (dialog->capacity_label), capacity_str);
  g_free (capacity_str);

  /* update the volume */
  volume = thunar_file_get_volume (file);
  if (G_LIKELY (volume != NULL))
    {
      gicon = g_volume_get_icon (volume);
      gtk_image_set_from_gicon (GTK_IMAGE (dialog->volume_image), gicon, GTK_ICON_SIZE_MENU);
      if (G_LIKELY (gicon != NULL))
        g_object_unref (gicon);

      volume_name = g_volume_get_name (volume);
      volume_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
      volume_label= g_strdup_printf ("%s (%s)", volume_name, volume_id);
      gtk_label_set_text (GTK_LABEL (dialog->volume_label), volume_label);
      gtk_widget_show (dialog->volume_label);
      g_free (volume_name);
      g_free (volume_id);
      g_free (volume_label);

      g_object_unref (G_OBJECT (volume));
    }
  else
    {
      gtk_widget_hide (dialog->volume_label);
    }

  if (dialog->show_file_highlight_tab && G_LIKELY (thunar_g_vfs_metadata_is_supported ()))
    {
      background = thunar_file_get_metadata_setting (file, "highlight-color-background");
      foreground = thunar_file_get_metadata_setting (file, "highlight-color-foreground");
      thunar_properties_dialog_colorize_example_box (dialog, background, foreground);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (icon_factory));
  g_free (date_custom_style);
}



static void
thunar_properties_dialog_update_multiple (ThunarPropertiesDialog *dialog)
{
  ThunarFile  *file;
  GString     *names_string;
  gboolean     first_file = TRUE;
  GList       *lp;
  const gchar *content_type = NULL;
  const gchar *tmp;
  gchar       *str;
  GVolume     *volume = NULL;
  GVolume     *tmp_volume;
  GIcon       *gicon;
  gchar       *volume_name;
  gchar       *volume_id;
  gchar       *volume_label;
  gchar       *display_name;
  ThunarFile  *parent_file = NULL;
  ThunarFile  *tmp_parent;
  gboolean     has_trashed_files = FALSE;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (g_list_length (dialog->files) > 1);

  /* update the properties dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Properties"));

  /* widgets not used with > 1 file selected */
  gtk_widget_hide (dialog->created_label);
  gtk_widget_hide (dialog->deleted_label);
  gtk_widget_hide (dialog->modified_label);
  gtk_widget_hide (dialog->accessed_label);
  gtk_widget_hide (dialog->freespace_vbox);
  gtk_widget_hide (dialog->origin_label);
  gtk_widget_hide (dialog->openwith_chooser);
  gtk_widget_hide (dialog->link_label);

  names_string = g_string_new (NULL);

  /* collect data of the selected files */
  for (lp = dialog->files; lp != NULL; lp = lp->next)
    {
      _thunar_assert (THUNAR_IS_FILE (lp->data));
      file = THUNAR_FILE (lp->data);

      /* append the name */
      if (!first_file)
        g_string_append (names_string, ", ");
      g_string_append (names_string, thunar_file_get_display_name (file));

      /* update the content type */
      if (first_file)
        {
          content_type = thunar_file_get_content_type (file);
        }
      else if (content_type != NULL)
        {
          /* check the types match */
          tmp = thunar_file_get_content_type (file);
          if (tmp == NULL || !g_content_type_equals (content_type, tmp))
            content_type = NULL;
        }

      /* check if all selected files are on the same volume */
      tmp_volume = thunar_file_get_volume (file);
      if (first_file)
        {
          volume = tmp_volume;
        }
      else if (tmp_volume != NULL)
        {
          /* we only display information if the files are on the same volume */
          if (tmp_volume != volume)
            {
              if (volume != NULL)
                g_object_unref (G_OBJECT (volume));
              volume = NULL;
            }

          g_object_unref (G_OBJECT (tmp_volume));
        }

      /* check if all files have the same parent */
      tmp_parent = thunar_file_get_parent (file, NULL);
      if (first_file)
        {
          parent_file = tmp_parent;
        }
      else if (tmp_parent != NULL)
        {
          /* we only display the location if they are all equal */
          if (!g_file_equal (thunar_file_get_file (parent_file), thunar_file_get_file (tmp_parent)))
            {
              if (parent_file != NULL)
                g_object_unref (G_OBJECT (parent_file));
              parent_file = NULL;
            }

          g_object_unref (G_OBJECT (tmp_parent));
        }

      if (thunar_file_is_trashed (file))
        has_trashed_files = TRUE;

      first_file = FALSE;
    }

  /* set the labels string */
  gtk_label_set_text (GTK_LABEL (dialog->names_label), names_string->str);
  gtk_widget_set_tooltip_text (dialog->names_label, names_string->str);
  g_string_free (names_string, TRUE);

  /* hide the permissions chooser for trashed files */
  gtk_widget_set_visible (dialog->permissions_chooser, !has_trashed_files);

  /* update the content type */
  if (content_type != NULL
      && !g_content_type_equals (content_type, "inode/symlink"))
    {
      str = g_content_type_get_description (content_type);
      gtk_widget_set_tooltip_text (dialog->kind_ebox, content_type);
      gtk_label_set_text (GTK_LABEL (dialog->kind_label), str);
      g_free (str);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (dialog->kind_label), _("mixed"));
    }

  /* update the file or folder location (parent) */
  if (G_UNLIKELY (parent_file != NULL))
    {
      display_name = g_file_get_parse_name (thunar_file_get_file (parent_file));
      gtk_label_set_text (GTK_LABEL (dialog->location_label), display_name);
      gtk_widget_show (dialog->location_label);
      g_object_unref (G_OBJECT (parent_file));
      g_free (display_name);
    }
  else
    {
      gtk_widget_hide (dialog->location_label);
    }

  /* update the volume */
  if (G_LIKELY (volume != NULL))
    {
      gicon = g_volume_get_icon (volume);
      gtk_image_set_from_gicon (GTK_IMAGE (dialog->volume_image), gicon, GTK_ICON_SIZE_MENU);
      if (G_LIKELY (gicon != NULL))
        g_object_unref (gicon);

      volume_name = g_volume_get_name (volume);
      volume_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
      volume_label = g_strdup_printf ("%s (%s)", volume_name, volume_id);
      gtk_label_set_text (GTK_LABEL (dialog->volume_label), volume_label);
      gtk_widget_show (dialog->volume_label);
      g_free (volume_name);
      g_free (volume_id);
      g_free (volume_label);

      g_object_unref (G_OBJECT (volume));
    }
  else
    {
      gtk_widget_hide (dialog->volume_label);
    }
}



static void
thunar_properties_dialog_update (ThunarPropertiesDialog *dialog)
{
  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (dialog->files != NULL);

  /* cancel any pending thumbnail requests */
  if (dialog->thumbnail_request > 0)
    {
      thunar_thumbnailer_dequeue (dialog->thumbnailer, dialog->thumbnail_request);
      dialog->thumbnail_request = 0;
    }

  if (dialog->files->next == NULL)
    {
      /* show single file name box */
      gtk_widget_show (dialog->single_box);

      /* update the properties for a dialog showing 1 file */
      thunar_properties_dialog_update_single (dialog);
    }
  else
    {
      /* show multiple files box */
      gtk_widget_hide (dialog->single_box);

      /* update the properties for a dialog showing multiple files */
      thunar_properties_dialog_update_multiple (dialog);
    }

  if (g_list_length (dialog->files) == 1 && thunar_file_is_directory (dialog->files->data) == FALSE)
    {
      gtk_widget_hide (dialog->content_label);
      gtk_widget_hide (dialog->content_value_label);
    }
  else
    {
      gtk_widget_show (dialog->content_label);
      gtk_widget_show (dialog->content_value_label);
    }
}



/**
 * thunar_properties_dialog_new:
 * @parent: transient window or NULL;
 * @flags: flags to consider
 *
 * Allocates a new #ThunarPropertiesDialog instance,
 * that is not associated with any #ThunarFile.
 *
 * Return value: the newly allocated #ThunarPropertiesDialog
 *               instance.
 **/
GtkWidget*
thunar_properties_dialog_new (GtkWindow                   *parent,
                              ThunarPropertiesDialogFlags  flags)
{
  _thunar_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  return g_object_new (THUNAR_TYPE_PROPERTIES_DIALOG,
                       "transient-for", parent,
                       "destroy-with-parent", parent != NULL,
                       "show-file-hightlight-tab", flags & THUNAR_PROPERTIES_DIALOG_SHOW_HIGHLIGHT,
                       NULL);
}



/**
 * thunar_properties_dialog_get_files:
 * @dialog : a #ThunarPropertiesDialog.
 *
 * Returns the #ThunarFile currently being displayed
 * by @dialog or %NULL if @dialog doesn't display
 * any file right now.
 *
 * Return value: list of #ThunarFile's displayed by @dialog
 *               or %NULL.
 **/
static GList*
thunar_properties_dialog_get_files (ThunarPropertiesDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog), NULL);
  return dialog->files;
}



/**
 * thunar_properties_dialog_set_files:
 * @dialog : a #ThunarPropertiesDialog.
 * @files  : a GList of #ThunarFile's or %NULL.
 *
 * Sets the #ThunarFile that is displayed by @dialog
 * to @files.
 **/
void
thunar_properties_dialog_set_files (ThunarPropertiesDialog *dialog,
                                    GList                  *files)
{
  GList      *lp;
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  /* check if the same lists are used (or null) */
  if (G_UNLIKELY (dialog->files == files))
    return;

  /* disconnect from any previously set files */
  for (lp = dialog->files; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);

      /* unregister our file watch */
      thunar_file_unwatch (file);

      /* unregister handlers */
      g_signal_handlers_disconnect_by_func (G_OBJECT (file), thunar_properties_dialog_update, dialog);
      g_signal_handlers_disconnect_by_func (G_OBJECT (file), gtk_widget_destroy, dialog);

      g_object_unref (G_OBJECT (file));
    }
  g_list_free (dialog->files);

  /* activate the new list */
  dialog->files = g_list_copy (files);

  /* connect to the new files */
  for (lp = dialog->files; lp != NULL; lp = lp->next)
    {
      _thunar_assert (THUNAR_IS_FILE (lp->data));
      file = THUNAR_FILE (g_object_ref (G_OBJECT (lp->data)));

      /* watch the file for changes */
      thunar_file_watch (file);

      /* install signal handlers */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_properties_dialog_update), dialog);
      g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (gtk_widget_destroy), dialog);
    }

  /* update the dialog contents */
  if (dialog->files != NULL)
    {
      /* update the UI for the new file */
      thunar_properties_dialog_update (dialog);

      /* update the provider property pages */
      thunar_properties_dialog_update_providers (dialog);
    }

  /* tell everybody that we have a new file here */
  g_object_notify (G_OBJECT (dialog), "files");
}



/**
 * thunar_properties_dialog_set_file:
 * @dialog : a #ThunarPropertiesDialog.
 * @file   : a #ThunarFile or %NULL.
 *
 * Sets the #ThunarFile that is displayed by @dialog
 * to @file.
 **/
void
thunar_properties_dialog_set_file (ThunarPropertiesDialog *dialog,
                                   ThunarFile             *file)
{
  GList foo;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  if (file == NULL)
    {
      thunar_properties_dialog_set_files (dialog, NULL);
    }
  else
    {
      /* create a fake list */
      foo.next = NULL;
      foo.prev = NULL;
      foo.data = file;

      thunar_properties_dialog_set_files (dialog, &foo);
    }
}



static void
thunar_properties_dialog_reset_highlight (ThunarPropertiesDialog *dialog)
{
  GList   *lp;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  for (lp = dialog->files; lp != NULL; lp = lp->next)
    {
      thunar_file_clear_metadata_setting (lp->data, "highlight-color-background");
      thunar_file_clear_metadata_setting (lp->data, "highlight-color-foreground");
    }

  /* clear previouly set colors */
  g_free (dialog->foreground_color);
  dialog->foreground_color = NULL;
  g_free (dialog->background_color);
  dialog->background_color = NULL;

  thunar_properties_dialog_colorize_example_box (dialog, NULL, NULL);

  thunar_properties_dialog_reload (dialog);

  thunar_properties_dialog_update_apply_button (dialog);
}



static void
thunar_properties_dialog_apply_highlight (ThunarPropertiesDialog *dialog)
{
  GList    *lp;
  gboolean  highlighting_enabled;

  /* if this feature is disabled, then enable the feature */
  if (dialog->foreground_color != NULL || dialog->background_color != NULL)
    {
      g_object_get (G_OBJECT (dialog->preferences), "misc-highlighting-enabled", &highlighting_enabled, NULL);
      if (!highlighting_enabled)
        g_object_set (G_OBJECT (dialog->preferences), "misc-highlighting-enabled", TRUE, NULL);
    }

  for (lp = dialog->files; lp != NULL; lp = lp->next)
    {
      if (dialog->foreground_color != NULL)
        thunar_file_set_metadata_setting (lp->data, "highlight-color-foreground", dialog->foreground_color, FALSE);
      if (dialog->background_color != NULL)
        thunar_file_set_metadata_setting (lp->data, "highlight-color-background", dialog->background_color, FALSE);
    }

  thunar_properties_dialog_reload (dialog);

  thunar_properties_dialog_update_apply_button (dialog);
}



static void
thunar_properties_dialog_set_foreground (ThunarPropertiesDialog *dialog)
{
  GdkRGBA   color;
  gchar    *color_str;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog->color_chooser), &color);
  color_str = gdk_rgba_to_string (&color);
  thunar_properties_dialog_colorize_example_box (dialog, NULL, color_str);
  g_free (dialog->foreground_color);
  dialog->foreground_color = NULL;
  dialog->foreground_color = color_str;
  thunar_properties_dialog_update_apply_button (dialog);
}



static void
thunar_properties_dialog_set_background (ThunarPropertiesDialog *dialog)
{
  GdkRGBA   color;
  gchar    *color_str;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog->color_chooser), &color);
  color_str = gdk_rgba_to_string (&color);
  thunar_properties_dialog_colorize_example_box (dialog, color_str, NULL);
  g_free (dialog->background_color);
  dialog->background_color = NULL;
  dialog->background_color = color_str;
  thunar_properties_dialog_update_apply_button (dialog);
}



static void
thunar_properties_dialog_update_apply_button (ThunarPropertiesDialog *dialog)
{
  for (GList *lp = dialog->files; lp != NULL; lp = lp->next)
    {
      if (dialog->foreground_color != NULL && g_strcmp0 (dialog->foreground_color, thunar_file_get_metadata_setting (lp->data, "highlight-color-foreground")) != 0)
        {
          gtk_widget_set_sensitive (dialog->highlight_apply_button, TRUE);
          return;
        }

      if (dialog->background_color != NULL && g_strcmp0 (dialog->background_color, thunar_file_get_metadata_setting (lp->data, "highlight-color-background")) != 0)
        {
          gtk_widget_set_sensitive (dialog->highlight_apply_button, TRUE);
          return;
        }
    }

  gtk_widget_set_sensitive (dialog->highlight_apply_button, FALSE);
}



static void
thunar_properties_dialog_colorize_example_box (ThunarPropertiesDialog *dialog,
                                               const gchar            *background,
                                               const gchar            *foreground)
{
  GtkCssProvider *provider = gtk_css_provider_new ();
  gchar          *css_data = NULL;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  gtk_widget_reset_style (dialog->example_box);

  if (background != NULL && foreground != NULL)
    css_data = g_strdup_printf ("#example { background-color: %s; color: %s; }", background, foreground);
  else if (background != NULL)
    css_data = g_strdup_printf ("#example { background-color: %s; }", background);
  else if (foreground != NULL)
    css_data = g_strdup_printf ("#example { color: %s; }", foreground);
  else
    css_data = g_strdup_printf ("#example { color: inherit; background-color: inherit; }");

  if (css_data == NULL)
    return;

  gtk_css_provider_load_from_data (provider, css_data, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (dialog->example_box), GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_widget_show (dialog->example_box);
  g_free (css_data);
}



static void
thunar_properties_dialog_color_editor_changed (ThunarPropertiesDialog *dialog)
{
  gboolean show_editor;

  _thunar_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));

  g_object_get (dialog->color_chooser, "show-editor", &show_editor, NULL);
  
  if (show_editor)
    {
      gtk_widget_show (dialog->editor_button);
      gtk_widget_hide (dialog->example_box);
      gtk_widget_hide (dialog->highlight_buttons);
    }
  else
    {
      gtk_widget_hide (dialog->editor_button);
      gtk_widget_show (dialog->example_box);
      gtk_widget_show (dialog->highlight_buttons);
    }
}



static void
thunar_properties_dialog_color_editor_close (ThunarPropertiesDialog *dialog)
{
  GdkRGBA color;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog->color_chooser), &color);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog->color_chooser), &color);

  g_object_set (G_OBJECT (dialog->color_chooser), "show-editor", FALSE, NULL);
}



static void
thunar_properties_dialog_notebook_page_changed (ThunarPropertiesDialog *dialog)
{
  /* if the highlight tab is not active then the color editor should be switched off */
  if (gtk_notebook_get_current_page (GTK_NOTEBOOK (dialog->notebook)) != NOTEBOOK_PAGE_HIGHLIGHT)
    g_object_set (G_OBJECT (dialog->color_chooser), "show-editor", FALSE, NULL);
}
