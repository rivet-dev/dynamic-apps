/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-permissions-chooser.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>



/* Use native strlcpy() if available */
#if defined(HAVE_STRLCPY)
#define g_strlcpy(dst, src, size) (strlcpy ((dst), (src), (size)))
#endif



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILES,
  PROP_MUTABLE,
};

/* Column identifiers for the group combo box */
enum
{
  THUNAR_PERMISSIONS_STORE_COLUMN_NAME,
  THUNAR_PERMISSIONS_STORE_COLUMN_GID,
  THUNAR_PERMISSIONS_STORE_N_COLUMNS,
};



static void                 thunar_permissions_chooser_finalize         (GObject                        *object);
static void                 thunar_permissions_chooser_get_property     (GObject                        *object,
                                                                         guint                           prop_id,
                                                                         GValue                         *value,
                                                                         GParamSpec                     *pspec);
static void                 thunar_permissions_chooser_set_property     (GObject                        *object,
                                                                         guint                           prop_id,
                                                                         const GValue                   *value,
                                                                         GParamSpec                     *pspec);
static gint                 thunar_permissions_chooser_ask_recursive    (ThunarPermissionsChooser       *chooser);
static void                 thunar_permissions_chooser_change_group     (ThunarPermissionsChooser       *chooser,
                                                                         guint32                         gid);
static gboolean             thunar_permissions_chooser_change_mode      (ThunarPermissionsChooser       *chooser,
                                                                         ThunarFileMode                  dir_mask,
                                                                         ThunarFileMode                  dir_mode,
                                                                         ThunarFileMode                  file_mask,
                                                                         ThunarFileMode                  file_mode);
static void                 thunar_permissions_chooser_access_changed   (ThunarPermissionsChooser       *chooser,
                                                                         GtkWidget                      *combo);
static void                 thunar_permissions_chooser_file_changed     (ThunarPermissionsChooser       *chooser);
static void                 thunar_permissions_chooser_group_changed    (ThunarPermissionsChooser       *chooser,
                                                                         GtkWidget                      *combo);
static void                 thunar_permissions_chooser_program_toggled  (ThunarPermissionsChooser       *chooser,
                                                                         GtkWidget                      *button);
static void                 thunar_permissions_chooser_fixperm_clicked  (ThunarPermissionsChooser       *chooser,
                                                                         GtkWidget                      *button);
static ThunarJobResponse    thunar_permissions_chooser_job_ask          (ThunarPermissionsChooser       *chooser,
                                                                         const gchar                    *message,
                                                                         ThunarJobResponse               choices,
                                                                         ThunarJob                      *job);
static void                 thunar_permissions_chooser_job_cancel       (ThunarPermissionsChooser       *chooser);
static void                 thunar_permissions_chooser_job_error        (ThunarPermissionsChooser       *chooser,
                                                                         GError                         *error,
                                                                         ThunarJob                      *job);
static void                 thunar_permissions_chooser_job_finished     (ThunarPermissionsChooser       *chooser,
                                                                         ThunarJob                      *job);
static void                 thunar_permissions_chooser_job_percent      (ThunarPermissionsChooser       *chooser,
                                                                         gdouble                         percent,
                                                                         ThunarJob                      *job);
static void                 thunar_permissions_chooser_job_start        (ThunarPermissionsChooser       *chooser,
                                                                         ThunarJob                      *job,
                                                                         gboolean                        recursive);
static gboolean             thunar_permissions_chooser_row_separator    (GtkTreeModel                   *model,
                                                                         GtkTreeIter                    *iter,
                                                                         gpointer                        data);
static GList               *thunar_permissions_chooser_get_files        (ThunarPermissionsChooser       *chooser);
static void                 thunar_permissions_chooser_set_files        (ThunarPermissionsChooser       *chooser,
                                                                         GList                          *files);


struct _ThunarPermissionsChooserClass
{
  GtkVBoxClass __parent__;
};

struct _ThunarPermissionsChooser
{
  GtkVBox     __parent__;

  GList      *files;

  /* the main grid widget, which contains everything but the job control stuff */
  GtkWidget  *grid;

  GtkWidget  *user_label;
  GtkWidget  *group_combo;
  GtkWidget  *access_combos[3];
  GtkWidget  *program_button;
  GtkWidget  *fixperm_label;
  GtkWidget  *fixperm_button;

  /* job control stuff */
  ThunarJob  *job;
  GtkWidget  *job_progress;
};



G_DEFINE_TYPE (ThunarPermissionsChooser, thunar_permissions_chooser, GTK_TYPE_BOX)



static void
thunar_permissions_chooser_class_init (ThunarPermissionsChooserClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_permissions_chooser_finalize;
  gobject_class->get_property = thunar_permissions_chooser_get_property;
  gobject_class->set_property = thunar_permissions_chooser_set_property;

  /**
   * ThunarPermissionsChooser:file:
   *
   * The #ThunarFile whose permissions will be edited/viewed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILES,
                                   g_param_spec_boxed ("files", "files", "files",
                                                       THUNARX_TYPE_FILE_INFO_LIST,
                                                       EXO_PARAM_READWRITE));

  /**
   * ThunarPermissionsChooser:mutable:
   *
   * Whether the current #ThunarFile<!---->s permissions are
   * mutable.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MUTABLE,
                                   g_param_spec_boolean ("mutable",
                                                         "mutable",
                                                         "mutable",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));
}



static void
thunar_permissions_chooser_init (ThunarPermissionsChooser *chooser)
{
  GtkCellRenderer *renderer_text;
  GtkWidget       *separator;
  GtkWidget       *button;
  GtkWidget       *label;
  GtkWidget       *image;
  GtkWidget       *hbox;
  gint             row = 0;

  /* setup the chooser */
  gtk_container_set_border_width (GTK_CONTAINER (chooser), 12);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (chooser), GTK_ORIENTATION_VERTICAL);

  /* allocate the shared renderer for the various combo boxes */
  renderer_text = gtk_cell_renderer_text_new ();

  chooser->grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (chooser->grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (chooser->grid), 6);
  gtk_box_pack_start (GTK_BOX (chooser), chooser->grid, TRUE, TRUE, 0);
  gtk_widget_show (chooser->grid);

  label = gtk_label_new (_("Owner:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_grid_attach (GTK_GRID (chooser->grid), hbox, 1, row, 1, 1);
  gtk_widget_show (hbox);

  chooser->user_label = gtk_label_new (_("Unknown"));
  gtk_label_set_xalign (GTK_LABEL (chooser->user_label), 0.0f);
  gtk_box_pack_start (GTK_BOX (hbox), chooser->user_label, TRUE, TRUE, 0);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), chooser->user_label);
  gtk_widget_show (chooser->user_label);

  row += 1;

  label = gtk_label_new_with_mnemonic (_("_Access:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  chooser->access_combos[2] = gtk_combo_box_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser->access_combos[2]);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser->access_combos[2]), renderer_text, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (chooser->access_combos[2]), renderer_text, "text", 0);
  g_object_bind_property (G_OBJECT (chooser),                   "mutable",
                          G_OBJECT (chooser->access_combos[2]), "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (chooser->access_combos[2]), "changed", G_CALLBACK (thunar_permissions_chooser_access_changed), chooser);
  gtk_widget_set_hexpand (chooser->access_combos[2], TRUE);
  gtk_grid_attach (GTK_GRID (chooser->grid), chooser->access_combos[2], 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), chooser->access_combos[2]);
  gtk_widget_show (chooser->access_combos[2]);

  row += 1;

  separator = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_set (separator, "margin", 4, NULL);
  gtk_grid_attach (GTK_GRID (chooser->grid), separator, 0, row, 2, 1);
  gtk_widget_show (separator);

  row += 1;

  label = gtk_label_new_with_mnemonic (_("Gro_up:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  chooser->group_combo = gtk_combo_box_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser->group_combo);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser->group_combo), renderer_text, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (chooser->group_combo), renderer_text, "text", THUNAR_PERMISSIONS_STORE_COLUMN_NAME);
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (chooser->group_combo), thunar_permissions_chooser_row_separator, NULL, NULL);
  g_object_bind_property (G_OBJECT (chooser),              "mutable",
                          G_OBJECT (chooser->group_combo), "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (chooser->group_combo), "changed", G_CALLBACK (thunar_permissions_chooser_group_changed), chooser);
  gtk_widget_set_hexpand (chooser->group_combo, TRUE);
  gtk_grid_attach (GTK_GRID (chooser->grid), chooser->group_combo, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), chooser->group_combo);
  gtk_widget_show (chooser->group_combo);

  row += 1;

  label = gtk_label_new_with_mnemonic (_("Acc_ess:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  chooser->access_combos[1] = gtk_combo_box_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser->access_combos[1]);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser->access_combos[1]), renderer_text, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (chooser->access_combos[1]), renderer_text, "text", 0);
  g_object_bind_property (G_OBJECT (chooser),                   "mutable",
                          G_OBJECT (chooser->access_combos[1]), "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (chooser->access_combos[1]), "changed", G_CALLBACK (thunar_permissions_chooser_access_changed), chooser);
  gtk_widget_set_hexpand (chooser->access_combos[1], TRUE);
  gtk_grid_attach (GTK_GRID (chooser->grid), chooser->access_combos[1], 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), chooser->access_combos[1]);
  gtk_widget_show (chooser->access_combos[1]);

  row += 1;

  separator = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_set (separator, "margin", 4, NULL);
  gtk_grid_attach (GTK_GRID (chooser->grid), separator, 0, row, 2, 1);
  gtk_widget_show (separator);

  row += 1;

  label = gtk_label_new (_("Others"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  row += 1;

  label = gtk_label_new_with_mnemonic (_("Acce_ss:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  chooser->access_combos[0] = gtk_combo_box_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser->access_combos[0]);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser->access_combos[0]), renderer_text, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (chooser->access_combos[0]), renderer_text, "text", 0);
  g_object_bind_property (G_OBJECT (chooser),                   "mutable",
                          G_OBJECT (chooser->access_combos[0]), "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (chooser->access_combos[0]), "changed", G_CALLBACK (thunar_permissions_chooser_access_changed), chooser);
  gtk_widget_set_hexpand (chooser->access_combos[0], TRUE);
  gtk_grid_attach (GTK_GRID (chooser->grid), chooser->access_combos[0], 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), chooser->access_combos[0]);
  gtk_widget_show (chooser->access_combos[0]);

  row += 1;

  separator = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_set (separator, "margin", 4, NULL);
  gtk_grid_attach (GTK_GRID (chooser->grid), separator, 0, row, 2, 1);
  gtk_widget_show (separator);

  row += 1;

  label = gtk_label_new (_("Program:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_grid_attach (GTK_GRID (chooser->grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  chooser->program_button = gtk_check_button_new_with_mnemonic (_("Allow this file to _run as a program"));
  g_object_bind_property (G_OBJECT (chooser->program_button), "visible",
                          G_OBJECT (label),                   "visible",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (chooser),                 "mutable",
                          G_OBJECT (chooser->program_button), "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (chooser->program_button), "toggled", G_CALLBACK (thunar_permissions_chooser_program_toggled), chooser);
  gtk_grid_attach (GTK_GRID (chooser->grid), chooser->program_button, 1, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), chooser->program_button);
  gtk_widget_show (chooser->program_button);

  row += 1;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_bind_property (G_OBJECT (chooser), "mutable",
                          G_OBJECT (hbox),    "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (chooser->program_button), "visible",
                          G_OBJECT (hbox),                    "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (chooser->grid), hbox, 1, row, 1, 1);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("Allowing untrusted programs to run presents a security risk to your system."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_bind_property (G_OBJECT (chooser), "mutable",
                          G_OBJECT (hbox),    "sensitive",
                          G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (chooser->grid), hbox, 1, row, 1, 1);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  chooser->fixperm_label = gtk_label_new (_("The folder permissions are inconsistent, you may not be able to work with files in this folder."));
  gtk_label_set_xalign (GTK_LABEL (chooser->fixperm_label), 0.0f);
  gtk_label_set_attributes (GTK_LABEL (chooser->fixperm_label), thunar_pango_attr_list_small_italic ());
  gtk_label_set_line_wrap (GTK_LABEL (chooser->fixperm_label), TRUE);
  g_object_bind_property (G_OBJECT (chooser->fixperm_label), "visible",
                          G_OBJECT (hbox),                   "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (hbox), chooser->fixperm_label, TRUE, TRUE, 0);
  gtk_widget_show (chooser->fixperm_label);

  row += 1;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (chooser->grid), hbox, 1, row, 1, 1);
  gtk_widget_show (hbox);

  chooser->fixperm_button = gtk_button_new_with_mnemonic (_("Correct _folder permissions..."));
  gtk_widget_set_tooltip_text (chooser->fixperm_button, _("Click here to automatically fix the folder permissions."));
  g_signal_connect_swapped (G_OBJECT (chooser->fixperm_button), "clicked", G_CALLBACK (thunar_permissions_chooser_fixperm_clicked), chooser);
  g_object_bind_property (G_OBJECT (chooser->fixperm_button), "visible",
                          G_OBJECT (hbox), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_pack_end (GTK_BOX (hbox), chooser->fixperm_button, FALSE, FALSE, 0);
  gtk_widget_show (chooser->fixperm_button);

  /* the job control stuff */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (chooser), hbox, FALSE, FALSE, 0);

  chooser->job_progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (chooser->job_progress), _("Please wait..."));
  g_object_bind_property (G_OBJECT (chooser->job_progress), "visible",
                          G_OBJECT (hbox), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (hbox), chooser->job_progress, TRUE, TRUE, 0);

  button = gtk_button_new ();
  gtk_widget_set_tooltip_text (button, _("Stop applying permissions recursively."));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_permissions_chooser_job_cancel), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
}



static void
thunar_permissions_chooser_finalize (GObject *object)
{
  ThunarPermissionsChooser *chooser = THUNAR_PERMISSIONS_CHOOSER (object);

  /* cancel any pending job */
  if (G_UNLIKELY (chooser->job != NULL))
    {
      /* cancel the job (if not already done) */
      exo_job_cancel (EXO_JOB (chooser->job));

      /* disconnect from the job */
      g_signal_handlers_disconnect_matched (chooser->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, chooser);
      g_object_unref (chooser->job);
      chooser->job = NULL;
    }

  /* drop the reference on the file (if any) */
  thunar_permissions_chooser_set_files (chooser, NULL);

  (*G_OBJECT_CLASS (thunar_permissions_chooser_parent_class)->finalize) (object);
}



static void
thunar_permissions_chooser_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ThunarPermissionsChooser *chooser = THUNAR_PERMISSIONS_CHOOSER (object);
  GList                    *lp;

  switch (prop_id)
    {
    case PROP_FILES:
      g_value_set_boxed (value, thunar_permissions_chooser_get_files (chooser));
      break;

    case PROP_MUTABLE:
      for (lp = chooser->files; lp != NULL; lp = lp->next)
        if (!thunar_file_is_chmodable (THUNAR_FILE (lp->data)))
          break;

      g_value_set_boolean (value, (chooser->files != NULL) && (lp == NULL));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_permissions_chooser_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ThunarPermissionsChooser *chooser = THUNAR_PERMISSIONS_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_FILES:
      thunar_permissions_chooser_set_files (chooser, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_permissions_chooser_ask_recursive (ThunarPermissionsChooser *chooser)
{
  ThunarRecursivePermissionsMode mode;
  ThunarPreferences             *preferences;
  GtkWidget                     *toplevel;
  GtkWidget                     *dialog;
  GtkWidget                     *button;
  GtkWidget                     *label;
  GtkWidget                     *image;
  GtkWidget                     *hbox;
  GtkWidget                     *vbox;
  gint                           response;

  /* grab a reference on the preferences */
  preferences = thunar_preferences_get ();

  /* determine the current recursive permissions mode */
  g_object_get (G_OBJECT (preferences), "misc-recursive-permissions", &mode, NULL);

  /* check if we should ask the user first */
  if (G_UNLIKELY (mode == THUNAR_RECURSIVE_PERMISSIONS_ASK))
    {
      /* determine the toplevel widget for the chooser */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (chooser));

      /* allocate the question dialog */
      dialog = gtk_dialog_new_with_buttons (_("Question"), GTK_WINDOW (toplevel),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                                            _("_No"), GTK_RESPONSE_NO,
                                            _("_Yes"), GTK_RESPONSE_YES,
                                            NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      image = gtk_image_new_from_icon_name ("dialog-question", GTK_ICON_SIZE_DIALOG);
      gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (image, GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_widget_show (image);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      label = gtk_label_new (_("Apply recursively?"));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big_bold ());
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      label = gtk_label_new (_("Do you want to apply your changes recursively to\nall files and subfolders below the selected folder?"));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_check_button_new_with_mnemonic (_("Do _not ask me again"));
      gtk_widget_set_tooltip_text (button, _("If you select this option your choice will be remembered and you won't be asked "
                                             "again. You can use the preferences dialog to alter your choice afterwards."));
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      /* run the dialog and save the selected option (if requested) */
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      switch (response)
        {
        case GTK_RESPONSE_YES:
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
            g_object_set (G_OBJECT (preferences), "misc-recursive-permissions", THUNAR_RECURSIVE_PERMISSIONS_ALWAYS, NULL);
          break;

        case GTK_RESPONSE_NO:
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
            g_object_set (G_OBJECT (preferences), "misc-recursive-permissions", THUNAR_RECURSIVE_PERMISSIONS_NEVER, NULL);
          break;

        default:
          break;
        }

      /* destroy the dialog resources */
      gtk_widget_destroy (dialog);
    }
  else if (mode == THUNAR_RECURSIVE_PERMISSIONS_ALWAYS)
    {
      response = GTK_RESPONSE_YES;
    }
  else
    {
      response = GTK_RESPONSE_NO;
    }

  /* release the reference on the preferences */
  g_object_unref (G_OBJECT (preferences));

  return response;
}



static gboolean
thunar_permissions_chooser_has_directory (ThunarPermissionsChooser *chooser)
{
  GList *lp;

  for (lp = chooser->files; lp != NULL; lp = lp->next)
    if (thunar_file_is_directory (THUNAR_FILE (lp->data)))
      return TRUE;

  return FALSE;
}



static gboolean
thunar_permissions_chooser_is_fixable_directory (ThunarFile *file)
{
  ThunarFileMode mode;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (!thunar_file_is_directory (file)
      || !thunar_file_is_chmodable (file))
    return FALSE;

  mode = thunar_file_get_mode (file);

  return ((mode & 0111) != ((mode >> 2) & 0111));
}



static gboolean
thunar_permissions_chooser_has_fixable_directory (ThunarPermissionsChooser *chooser)
{
  GList *lp;

  for (lp = chooser->files; lp != NULL; lp = lp->next)
    if (thunar_permissions_chooser_is_fixable_directory (THUNAR_FILE (lp->data)))
      return TRUE;

  return FALSE;
}



/* free returned list with g_list_free_full (file_list, g_object_unref); */
static GList *
thunar_permissions_chooser_get_file_list (ThunarPermissionsChooser *chooser)
{
  GList *file_list = NULL;
  GList *lp;
  GFile *gfile;

  for (lp = chooser->files; lp != NULL; lp = lp->next)
    {
      gfile = thunar_file_get_file (THUNAR_FILE (lp->data));
      _thunar_assert (G_IS_FILE (gfile));
      file_list = g_list_prepend (file_list, g_object_ref (G_OBJECT (gfile)));
    }

  return file_list;
}



static void
thunar_permissions_chooser_change_group (ThunarPermissionsChooser *chooser,
                                         guint32                   gid)
{
  ThunarJob *job;
  gboolean   recursive = FALSE;
  gint       response;
  GList     *file_list;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (chooser->files != NULL);

  /* check if we should operate recursively */
  if (thunar_permissions_chooser_has_directory (chooser))
    {
      response = thunar_permissions_chooser_ask_recursive (chooser);
      switch (response)
        {
        case GTK_RESPONSE_YES:
          recursive = TRUE;
          break;

        case GTK_RESPONSE_NO:
          recursive = FALSE;
          break;

        default:  /* cancelled by the user */
          thunar_permissions_chooser_file_changed (chooser);
          return;
        }
    }

  /* try to allocate the new job */
  file_list = thunar_permissions_chooser_get_file_list (chooser);
  job = thunar_io_jobs_change_group (file_list, gid, recursive);
  exo_job_launch (EXO_JOB (job));
  thunar_permissions_chooser_job_start (chooser, job, recursive);
  g_list_free_full (file_list, g_object_unref);
  g_object_unref (job);
}



static gboolean
thunar_permissions_chooser_change_mode (ThunarPermissionsChooser *chooser,
                                        ThunarFileMode            dir_mask,
                                        ThunarFileMode            dir_mode,
                                        ThunarFileMode            file_mask,
                                        ThunarFileMode            file_mode)
{
  ThunarJob *job;
  gboolean   recursive = FALSE;
  gint       response;
  GList     *file_list;

  _thunar_return_val_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser), FALSE);
  _thunar_return_val_if_fail (chooser->files != NULL, FALSE);

  /* check if we should operate recursively */
  if (thunar_permissions_chooser_has_directory (chooser))
    {
      response = thunar_permissions_chooser_ask_recursive (chooser);
      switch (response)
        {
        case GTK_RESPONSE_YES:
          recursive = TRUE;
          break;

        case GTK_RESPONSE_NO:
          recursive = FALSE;
          break;

        default:  /* cancelled by the user */
          thunar_permissions_chooser_file_changed (chooser);
          return FALSE;
        }
    }

  /* try to allocate the new job */
  file_list = thunar_permissions_chooser_get_file_list (chooser);
  job = thunar_io_jobs_change_mode (file_list, dir_mask, dir_mode, file_mask, file_mode, recursive);
  exo_job_launch (EXO_JOB (job));
  thunar_permissions_chooser_job_start (chooser, job, recursive);
  g_list_free_full (file_list, g_object_unref);
  g_object_unref (job);

  return TRUE;
}



static void
thunar_permissions_chooser_access_changed (ThunarPermissionsChooser *chooser,
                                           GtkWidget                *combo)
{
  ThunarFileMode  file_mask;
  ThunarFileMode  file_mode;
  ThunarFileMode  dir_mask;
  ThunarFileMode  dir_mode;
  guint           n;
  gint            active_mode;
  GtkTreeModel   *model;
  GtkTreeIter     iter;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (GTK_IS_COMBO_BOX (combo));

  /* leave if the active mode is varying */
  active_mode = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  if (active_mode > 3)
    return;

  /* determine the new mode from the combo box */
  for (n = 0; n < G_N_ELEMENTS (chooser->access_combos) && chooser->access_combos[n] != combo ; ++n);
  dir_mode = file_mode = (active_mode << 1) << (n * 3);
  dir_mask = file_mask = 0006 << (n * 3);

  /* keep exec bit in sync for folders */
  if (thunar_permissions_chooser_has_directory (chooser))
    {
      /* if either read or write is, set exec as well, else unset exec as well */
      if ((dir_mode & (0004 << (n * 3))) != 0)
        dir_mode |= (0001 << (n * 3));
      dir_mask = 0007 << (n * 3);
    }

  /* change the permissions */
  if (thunar_permissions_chooser_change_mode (chooser, dir_mask, dir_mode, file_mask, file_mode))
    {
      /* for better feedback remove the varying item */
      model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
      if (gtk_tree_model_get_iter_from_string (model, &iter, "4"))
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}



static gint
group_compare (gconstpointer group_a,
               gconstpointer group_b,
               gpointer      group_primary)
{
  guint32 group_primary_id;
  guint32 group_a_id = thunar_group_get_id (THUNAR_GROUP (group_a));
  guint32 group_b_id = thunar_group_get_id (THUNAR_GROUP (group_b));

  /* check if the groups are equal */
  if (group_a_id == group_b_id)
    return 0;

  /* the primary group is always sorted first */
  if (group_primary != NULL)
    {
      group_primary_id = thunar_group_get_id (THUNAR_GROUP (group_primary));
      if (group_a_id == group_primary_id)
        return -1;
      else if (group_b_id == group_primary_id)
        return 1;
    }

  /* system groups (< 100) are always sorted last */
  if (group_a_id < 100 && group_b_id >= 100)
    return 1;
  else if (group_b_id < 100 && group_a_id >= 100)
    return -1;

  /* otherwise just sort by name */
  return g_ascii_strcasecmp (thunar_group_get_name (THUNAR_GROUP (group_a)),
                             thunar_group_get_name (THUNAR_GROUP (group_b)));
}



static void
thunar_permissions_chooser_file_changed (ThunarPermissionsChooser *chooser)
{
  ThunarFile        *file;
  ThunarUserManager *user_manager;
  ThunarFileMode     mode = 0;
  ThunarGroup       *group = NULL;
  ThunarUser        *user = NULL;
  GtkListStore      *store;
  GtkTreeIter        iter;
  const gchar       *user_name;
  const gchar       *real_name;
  GList             *groups = NULL;
  GList             *lp;
  gchar              buffer[1024];
  guint              n;
  guint              n_files = 0;
  gint               modes[3] = { 0, };
  gint               file_modes[3];
  GtkListStore      *access_store;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));

  /* compare multiple files */
  for (lp = chooser->files; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);

      /* transform the file modes in r/w/r+w for each group */
      mode = thunar_file_get_mode (file);
      for (n = 0; n < 3; n++)
        file_modes[n] = ((mode >> (n * 3)) & 0007) >> 1;

      if (n_files == 0)
        {
          /* get information of the first file */
          user = thunar_file_get_user (file);
          group = thunar_file_get_group (file);

          for (n = 0; n < 3; n++)
            modes[n] = file_modes[n];
        }
      else
        {
          /* unset the file info if it is different from the other files */
          if (user != NULL && user != thunar_file_get_user (file))
            user = NULL;

          if (group != NULL && group != thunar_file_get_group (file))
            group = NULL;

          for (n = 0; n < 3; n++)
            if (file_modes[n] != modes[n])
              modes[n] = 4;
        }

      n_files++;
    }

  file = THUNAR_FILE (chooser->files->data);

  /* allocate a new store for the group combo box */
  g_signal_handlers_block_by_func (G_OBJECT (chooser->group_combo), thunar_permissions_chooser_group_changed, chooser);
  store = gtk_list_store_new (THUNAR_PERMISSIONS_STORE_N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT);
  gtk_combo_box_set_model (GTK_COMBO_BOX (chooser->group_combo), GTK_TREE_MODEL (store));

  /* determine the owner of the new file */
  if (G_LIKELY (user != NULL))
    {
      /* determine sane display name for the owner */
      user_name = thunar_user_get_name (user);
      real_name = thunar_user_get_real_name (user);
      if (G_LIKELY (real_name != NULL))
        g_snprintf (buffer, sizeof (buffer), "%s (%s)", real_name, user_name);
      else
        g_strlcpy (buffer, user_name, sizeof (buffer));
      gtk_label_set_text (GTK_LABEL (chooser->user_label), buffer);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (chooser->user_label),
                          n_files > 1 ? _("Mixed file owners") :_("Unknown file owner"));
    }

  /* check if we have superuser privileges */
  if (G_UNLIKELY (geteuid () == 0))
    {
      /* determine all groups in the system */
      user_manager = thunar_user_manager_get_default ();
      groups = thunar_user_manager_get_all_groups (user_manager);
      g_object_unref (G_OBJECT (user_manager));
    }
  else
    {
      if (G_UNLIKELY (user == NULL && n_files > 1))
        {
          /* get groups of the active user */
          user_manager = thunar_user_manager_get_default ();
          user = thunar_user_manager_get_user_by_id (user_manager, geteuid ());
          g_object_unref (G_OBJECT (user_manager));
        }

      /* determine the groups for the user and take a copy */
      if (G_LIKELY (user != NULL))
        {
          groups = g_list_copy (thunar_user_get_groups (user));
          g_list_foreach (groups, (GFunc) (void (*)(void)) g_object_ref, NULL);
        }
    }

  /* make sure that the group list includes the file group */
  if (G_UNLIKELY (group != NULL && g_list_find (groups, group) == NULL))
    groups = g_list_prepend (groups, g_object_ref (G_OBJECT (group)));

  /* sort the groups according to group_compare() */
  groups = g_list_sort_with_data (groups, group_compare, group);

  /* add the groups to the store */
  for (lp = groups, n = 0; lp != NULL; lp = lp->next)
    {
      /* append a separator after the primary group and after the user-groups (not system groups) */
      if (group != NULL
          && thunar_group_get_id (groups->data) == thunar_group_get_id (group)
          && lp != groups && n == 0)
        {
          gtk_list_store_append (store, &iter);
          n += 1;
        }
      else if (lp != groups && thunar_group_get_id (lp->data) < 100 && n == 1)
        {
          gtk_list_store_append (store, &iter);
          n += 1;
        }

      /* append a new item for the group */
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          THUNAR_PERMISSIONS_STORE_COLUMN_NAME, thunar_group_get_name (lp->data),
                          THUNAR_PERMISSIONS_STORE_COLUMN_GID, thunar_group_get_id (lp->data),
                          -1);

      /* set the active iter for the combo box if this group is the primary group */
      if (G_UNLIKELY (lp->data == group))
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser->group_combo), &iter);
    }

  /* cleanup */
  if (G_LIKELY (user != NULL))
    g_object_unref (G_OBJECT (user));
  if (G_LIKELY (group != NULL))
    g_object_unref (G_OBJECT (group));

  g_list_free_full (groups, g_object_unref);

  /* determine the file mode and update the combo boxes */
  for (n = 0; n < G_N_ELEMENTS (chooser->access_combos); ++n)
    {
      g_signal_handlers_block_by_func (G_OBJECT (chooser->access_combos[n]), thunar_permissions_chooser_access_changed, chooser);

      /* allocate the store for the permission combos */
      access_store = gtk_list_store_new (1, G_TYPE_STRING);
      gtk_list_store_insert_with_values (access_store, NULL, 0, 0, _("None"), -1);         /* 0000 */
      gtk_list_store_insert_with_values (access_store, NULL, 1, 0, _("Write only"), -1);   /* 0002 */
      gtk_list_store_insert_with_values (access_store, NULL, 2, 0, _("Read only"), -1);    /* 0004 */
      gtk_list_store_insert_with_values (access_store, NULL, 3, 0, _("Read & Write"), -1); /* 0006 */
      if (modes[n] == 4)
        gtk_list_store_insert_with_values (access_store, NULL, 4, 0, _("Varying (no change)"), -1);

      gtk_combo_box_set_model (GTK_COMBO_BOX (chooser->access_combos[n]), GTK_TREE_MODEL (access_store));
      gtk_combo_box_set_active (GTK_COMBO_BOX (chooser->access_combos[n]), modes[n]);

      g_signal_handlers_unblock_by_func (G_OBJECT (chooser->access_combos[n]), thunar_permissions_chooser_access_changed, chooser);

      g_object_unref (G_OBJECT (access_store));
    }

  /* update the program setting based on the mode (only visible for regular files) */
  g_signal_handlers_block_by_func (G_OBJECT (chooser->program_button), thunar_permissions_chooser_program_toggled, chooser);
  g_object_set (G_OBJECT (chooser->program_button), "visible", thunar_file_is_regular (file), NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->program_button), (mode & 0111) != 0);
  g_signal_handlers_unblock_by_func (G_OBJECT (chooser->program_button), thunar_permissions_chooser_program_toggled, chooser);

  /* update the "inconsistent folder permissions" warning and the "fix permissions" button based on the mode */
  if (thunar_permissions_chooser_has_fixable_directory (chooser))
    {
      /* always display the warning even if we cannot fix it */
      gtk_widget_show (chooser->fixperm_label);
      gtk_widget_show (chooser->fixperm_button);
    }
  else
    {
      /* hide both the warning text and the fix button */
      gtk_widget_hide (chooser->fixperm_button);
      gtk_widget_hide (chooser->fixperm_label);
    }

  /* release our reference on the new combo store and unblock the combo */
  g_signal_handlers_unblock_by_func (G_OBJECT (chooser->group_combo), thunar_permissions_chooser_group_changed, chooser);
  g_object_unref (G_OBJECT (store));

  /* emit notification on "mutable", so all widgets update their sensitivity */
  g_object_notify (G_OBJECT (chooser), "mutable");
}



static void
thunar_permissions_chooser_group_changed (ThunarPermissionsChooser *chooser,
                                          GtkWidget                *combo)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  guint32       gid;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (chooser->group_combo == combo);
  _thunar_return_if_fail (GTK_IS_COMBO_BOX (combo));

  /* verify that we have a valid file */
  if (G_UNLIKELY (chooser->files == NULL))
    return;

  /* determine the tree model from the combo box */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  /* determine the iterator for the selected item */
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    {
      /* determine the group id for the selected item... */
      gtk_tree_model_get (model, &iter, THUNAR_PERMISSIONS_STORE_COLUMN_GID, &gid, -1);

      /* ...and try to change the group to the new gid */
      thunar_permissions_chooser_change_group (chooser, gid);
    }
}



static void
thunar_permissions_chooser_program_toggled (ThunarPermissionsChooser *chooser,
                                            GtkWidget                *button)
{
  ThunarFileMode mode;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (chooser->program_button == button);
  _thunar_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  /* verify that we have a valid file */
  if (G_UNLIKELY (chooser->files == NULL))
    return;

  /* determine the new mode based on the toggle state */
  mode = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) ? 0111 : 0000;

  /* apply the new mode (only the executable bits for files) */
  thunar_permissions_chooser_change_mode (chooser, 0000, 0000, 0111, mode);
}



static void
thunar_permissions_chooser_fixperm_clicked (ThunarPermissionsChooser *chooser,
                                            GtkWidget                *button)
{
  ThunarFileMode mode;
  GtkWidget     *dialog;
  GtkWidget     *window;
  ThunarJob     *job;
  gint           response;
  GList         *lp;
  GList          file_list;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (chooser->fixperm_button == button);
  _thunar_return_if_fail (GTK_IS_BUTTON (button));
  _thunar_return_if_fail (thunar_permissions_chooser_has_fixable_directory (chooser));

  /* verify that we have a valid file */
  if (G_UNLIKELY (chooser->files == NULL))
    return;

  /* determine the toplevel widget */
  window = gtk_widget_get_toplevel (GTK_WIDGET (chooser));
  if (G_UNLIKELY (window == NULL))
    return;

  /* popup a confirm dialog */
  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT
                                   | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Correct folder permissions automatically?"));
  gtk_window_set_title (GTK_WINDOW (dialog), _("Folder permissions"));
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Correct _folder permissions"), GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("The folder permissions will be reset to a consistent state. Only users "
                                                                           "allowed to read the contents of this folder will be allowed to enter the "
                                                                           "folder afterwards."));
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* check if we should apply the changes */
  if (response == GTK_RESPONSE_OK)
    {
      for (lp = chooser->files; lp != NULL; lp = lp->next)
        {
          /* skip files that are fine */
          if (!thunar_permissions_chooser_is_fixable_directory (THUNAR_FILE (lp->data)))
            continue;

          /* determine the current mode */
          mode = thunar_file_get_mode (THUNAR_FILE (lp->data));

          /* determine the new mode (making sure the owner can read/enter the folder) */
          mode = (THUNAR_FILE_MODE_USR_READ | THUNAR_FILE_MODE_USR_EXEC)
               | (((mode & THUNAR_FILE_MODE_GRP_READ) != 0) ? THUNAR_FILE_MODE_GRP_EXEC : 0)
               | (((mode & THUNAR_FILE_MODE_OTH_READ) != 0) ? THUNAR_FILE_MODE_OTH_EXEC : 0);

          file_list.prev = NULL;
          file_list.data = thunar_file_get_file (THUNAR_FILE (lp->data));
          file_list.next = NULL;

          /* try to allocate the new job */
          job = thunar_io_jobs_change_mode (&file_list,
                                            0511, mode, 0000, 0000, FALSE);
          exo_job_launch (EXO_JOB (job));

          /* handle the job */
          thunar_permissions_chooser_job_start (chooser, job, FALSE);
          g_object_unref (job);
        }
    }
}



static ThunarJobResponse
thunar_permissions_chooser_job_ask (ThunarPermissionsChooser *chooser,
                                    const gchar              *message,
                                    ThunarJobResponse         choices,
                                    ThunarJob                *job)
{
  GtkWidget *toplevel;

  _thunar_return_val_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (g_utf8_validate (message, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (chooser->job == job, THUNAR_JOB_RESPONSE_CANCEL);

  /* be sure to display the progress bar prior to opening the question dialog */
  gtk_widget_show_now (chooser->job_progress);

  /* determine the toplevel window for the chooser */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (chooser));
  if (G_UNLIKELY (toplevel == NULL))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* display the question dialog */
  return thunar_dialogs_show_job_ask (GTK_WINDOW (toplevel), message, choices);
}



static void
thunar_permissions_chooser_job_cancel (ThunarPermissionsChooser *chooser)
{
  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));

  /* verify that we have a job to cancel */
  if (G_UNLIKELY (chooser->job == NULL))
    return;

  /* cancel the job (if not already done) */
  exo_job_cancel (EXO_JOB (chooser->job));

  /* disconnect from the job */
  g_signal_handlers_disconnect_matched (chooser->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, chooser);
  g_object_unref (G_OBJECT (chooser->job));
  chooser->job = NULL;

  /* hide the progress bar */
  gtk_widget_hide (chooser->job_progress);

  /* make the remaining widgets sensitive again */
  gtk_widget_set_sensitive (chooser->grid, TRUE);
}



static void
thunar_permissions_chooser_job_error (ThunarPermissionsChooser *chooser,
                                      GError                   *error,
                                      ThunarJob                *job)
{
  GtkWidget *toplevel;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (error != NULL && error->message != NULL);
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (chooser->job == job);

  /* be sure to display the progress bar prior to opening the error dialog */
  gtk_widget_show_now (chooser->job_progress);

  /* determine the toplevel widget for the chooser */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (chooser));
  if (G_UNLIKELY (toplevel == NULL))
    return;

  /* popup the error message dialog */
  thunar_dialogs_show_job_error (GTK_WINDOW (toplevel), error);
}



static void
thunar_permissions_chooser_job_finished (ThunarPermissionsChooser *chooser,
                                         ThunarJob                *job)
{
  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (chooser->job == job);

  /* we can just use job_cancel(), since the job is already done */
  thunar_permissions_chooser_job_cancel (chooser);
}



static void
thunar_permissions_chooser_job_percent (ThunarPermissionsChooser *chooser,
                                        gdouble                   percent,
                                        ThunarJob                *job)
{
  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (chooser->job == job);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (chooser->job_progress), percent / 100.0);
  gtk_widget_show (chooser->job_progress);
}



static void
thunar_permissions_chooser_job_start (ThunarPermissionsChooser *chooser,
                                      ThunarJob                *job,
                                      gboolean                  recursive)
{
  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (chooser->job == NULL);

  /* take a reference to the job and connect signals */
  chooser->job = g_object_ref (job);
  g_signal_connect_swapped (job, "ask", G_CALLBACK (thunar_permissions_chooser_job_ask), chooser);
  g_signal_connect_swapped (job, "error", G_CALLBACK (thunar_permissions_chooser_job_error), chooser);
  g_signal_connect_swapped (job, "finished", G_CALLBACK (thunar_permissions_chooser_job_finished), chooser);

  /* don't connect percent for single file operations */
  if (G_UNLIKELY (recursive))
    g_signal_connect_swapped (job, "percent", G_CALLBACK (thunar_permissions_chooser_job_percent), chooser);

  /* setup the progress bar */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (chooser->job_progress), 0.0);

  /* make the majority of widgets insensitive if doing recursively */
  gtk_widget_set_sensitive (chooser->grid, !recursive);
}



static gboolean
thunar_permissions_chooser_row_separator (GtkTreeModel *model,
                                          GtkTreeIter  *iter,
                                          gpointer      data)
{
  gchar *name;

  /* determine the value of the "name" column */
  gtk_tree_model_get (model, iter, THUNAR_PERMISSIONS_STORE_COLUMN_NAME, &name, -1);
  if (G_LIKELY (name != NULL))
    {
      g_free (name);
      return FALSE;
    }

  return TRUE;
}



/**
 * thunar_permissions_chooser_get_files:
 * @chooser : a #ThunarPermissionsChooser.
 *
 * Returns the #ThunarFile's associated with the specified @chooser.
 *
 * Return value: the file associated with @chooser.
 **/
GList*
thunar_permissions_chooser_get_files (ThunarPermissionsChooser *chooser)
{
  _thunar_return_val_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser), NULL);
  return chooser->files;
}



/**
 * thunar_permissions_chooser_set_files:
 * @chooser : a #ThunarPermissionsChooser.
 * @files   : a list of #ThunarFiles or %NULL.
 *
 * Associates @chooser with the specified @file.
 **/
void
thunar_permissions_chooser_set_files (ThunarPermissionsChooser *chooser,
                                      GList                    *files)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_PERMISSIONS_CHOOSER (chooser));

  /* check if we already use that file */
  if (G_UNLIKELY (chooser->files == files))
    return;

  /* disconnect from the previous files */
  for (lp = chooser->files; lp != NULL; lp = lp->next)
    {
      _thunar_assert (THUNAR_IS_FILE (lp->data));
      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), thunar_permissions_chooser_file_changed, chooser);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (chooser->files);

  /* activate the new file */
  chooser->files = g_list_copy (files);

  /* connect to the new files */
  for (lp = chooser->files; lp != NULL; lp = lp->next)
    {
      /* take a reference on the file */
      _thunar_assert (THUNAR_IS_FILE (lp->data));
      g_object_ref (G_OBJECT (lp->data));

      /* stay informed about changes */
      g_signal_connect_swapped (G_OBJECT (lp->data), "changed", G_CALLBACK (thunar_permissions_chooser_file_changed), chooser);
    }

  /* update our state */
  if (chooser->files != NULL)
    thunar_permissions_chooser_file_changed (chooser);

  /* notify listeners */
  g_object_notify (G_OBJECT (chooser), "files");
}



/**
 * thunar_permissions_chooser_new:
 *
 * Allocates a new #ThunarPermissionsChooser instance.
 *
 * Return value: the newly allocated #ThunarPermissionsChooser.
 **/
GtkWidget*
thunar_permissions_chooser_new (void)
{
  return g_object_new (THUNAR_TYPE_PERMISSIONS_CHOOSER, NULL);
}
