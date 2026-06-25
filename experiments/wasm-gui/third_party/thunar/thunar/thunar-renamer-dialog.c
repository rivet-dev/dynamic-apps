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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-action-manager.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-menu.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-renamer-model.h>
#include <thunar/thunar-renamer-progress.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_STANDALONE,
};

/* Identifiers for DnD target types */
enum
{
  TARGET_GTK_TREE_MODEL_ROW,
  TARGET_TEXT_URI_LIST,
};

typedef enum
{
  THUNAR_RENAMER_ACTION_ADD_FILES,
  THUNAR_RENAMER_ACTION_REMOVE_FILES,
  THUNAR_RENAMER_ACTION_CLEAR,
  THUNAR_RENAMER_ACTION_ABOUT,

} ThunarRenamerAction;



static void        thunar_renamer_dialog_dispose               (GObject                  *object);
static void        thunar_renamer_dialog_finalize              (GObject                  *object);
static void        thunar_renamer_dialog_get_property          (GObject                  *object,
                                                                guint                     prop_id,
                                                                GValue                   *value,
                                                                GParamSpec               *pspec);
static void        thunar_renamer_dialog_set_property          (GObject                  *object,
                                                                guint                     prop_id,
                                                                const GValue             *value,
                                                                GParamSpec               *pspec);
static void        thunar_renamer_dialog_realize               (GtkWidget                *widget);
static void        thunar_renamer_dialog_unrealize             (GtkWidget                *widget);
static void        thunar_renamer_dialog_response              (GtkDialog                *dialog,
                                                                gint                      response);
static void        thunar_renamer_dialog_context_menu          (ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_help                  (ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_save                  (ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_action_add_files      (ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_action_remove_files   (ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_action_clear          (ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_action_about          (ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_name_column_clicked   (GtkTreeViewColumn        *column,
                                                                ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_button_press_event    (GtkWidget                *tree_view,
                                                                GdkEventButton           *event,
                                                                ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_drag_data_received    (GtkWidget                *tree_view,
                                                                GdkDragContext           *context,
                                                                gint                      x,
                                                                gint                      y,
                                                                GtkSelectionData         *selection_data,
                                                                guint                     info,
                                                                guint                     timestamp,
                                                                ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_drag_leave            (GtkWidget                *tree_view,
                                                                GdkDragContext           *context,
                                                                guint                     timestamp,
                                                                ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_drag_motion           (GtkWidget                *tree_view,
                                                                GdkDragContext           *context,
                                                                gint                      x,
                                                                gint                      y,
                                                                guint                     timestamp,
                                                                ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_drag_drop             (GtkWidget                *tree_view,
                                                                GdkDragContext           *context,
                                                                gint                      x,
                                                                gint                      y,
                                                                guint                     timestamp,
                                                                ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_notify_page           (GtkNotebook              *notebook,
                                                                GParamSpec               *pspec,
                                                                ThunarRenamerDialog      *renamer_dialog);
static gboolean    thunar_renamer_dialog_popup_menu            (GtkWidget                *tree_view,
                                                                ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_row_activated         (GtkTreeView              *tree_view,
                                                                GtkTreePath              *path,
                                                                GtkTreeViewColumn        *column,
                                                                ThunarRenamerDialog      *renamer_dialog);
static void        thunar_renamer_dialog_selection_changed     (GtkTreeSelection         *selection,
                                                                ThunarRenamerDialog      *renamer_dialog);
static ThunarFile *thunar_renamer_dialog_get_current_directory (ThunarRenamerDialog *renamer_dialog);
static void        thunar_renamer_dialog_set_current_directory (ThunarRenamerDialog *renamer_dialog,
                                                                ThunarFile          *current_directory);
static GList      *thunar_renamer_dialog_get_selected_files    (ThunarRenamerDialog *renamer_dialog);
static gboolean    thunar_renamer_dialog_get_standalone        (ThunarRenamerDialog *renamer_dialog);
static void        thunar_renamer_dialog_set_standalone        (ThunarRenamerDialog *renamer_dialog,
                                                                gboolean             fixed);
static GtkWidget  *thunar_renamer_dialog_append_menu_item      (ThunarRenamerDialog *renamer_dialog,
                                                                GtkMenuShell        *menu,
                                                                ThunarRenamerAction  action);



struct _ThunarRenamerDialogClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarRenamerDialog
{
  ThunarAbstractDialog __parent__;

  /* need to grab a reference on the icon factory for the standalone version */
  ThunarIconFactory   *icon_factory;

  ThunarRenamerModel  *model;

  ThunarActionManager *action_mgr;

  GtkWidget           *cancel_button;
  GtkWidget           *close_button;

  GtkWidget           *tree_view;
  GtkWidget           *progress;

  GtkWidget           *mode_combo;

  GtkTreeViewColumn   *name_column;

  /* the current directory used for the "Add Files" dialog */
  ThunarFile          *current_directory;

  /* #GList of currently selected #ThunarFile<!---->s */
  GList               *selected_files;

  /* whether the dialog should act like a standalone application */
  gboolean             standalone;

  /* TRUE while drop highlighting */
  gboolean             drag_highlighted;
};



static XfceGtkActionEntry thunar_renamer_action_entries[] =
{
    { THUNAR_RENAMER_ACTION_ADD_FILES,    "<Actions>/ThunarRenamerDialog/add-files",    "", XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Add Files..."), N_ ("Include additional files in the list of files to be renamed"), "list-add",    G_CALLBACK (thunar_renamer_dialog_action_add_files),   },
    { THUNAR_RENAMER_ACTION_REMOVE_FILES, "<Actions>/ThunarRenamerDialog/remove-files", "", XFCE_GTK_IMAGE_MENU_ITEM, NULL,                 NULL,                                                               "list-remove", G_CALLBACK (thunar_renamer_dialog_action_remove_files),},
    { THUNAR_RENAMER_ACTION_CLEAR,        "",                                           "", XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Clear"),         N_ ("Clear the file list below"),                                   "edit-clear",  G_CALLBACK (thunar_renamer_dialog_action_clear),       },
    { THUNAR_RENAMER_ACTION_ABOUT,        "",                                           "", XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_About"),        N_ ("Display information about Thunar Bulk Rename"),                "help-about",  G_CALLBACK (thunar_renamer_dialog_action_about),       },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_renamer_action_entries,G_N_ELEMENTS(thunar_renamer_action_entries),id)

/* Target types for dropping to the tree view */
static const GtkTargetEntry drop_targets[] =
{
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TARGET_GTK_TREE_MODEL_ROW },
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST }
};

/* Target types for dragging files in the tree view */
static const GtkTargetEntry drag_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TARGET_GTK_TREE_MODEL_ROW }
};



G_DEFINE_TYPE (ThunarRenamerDialog, thunar_renamer_dialog, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_renamer_dialog_class_init (ThunarRenamerDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_renamer_dialog_dispose;
  gobject_class->finalize = thunar_renamer_dialog_finalize;
  gobject_class->get_property = thunar_renamer_dialog_get_property;
  gobject_class->set_property = thunar_renamer_dialog_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_renamer_dialog_realize;
  gtkwidget_class->unrealize = thunar_renamer_dialog_unrealize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_renamer_dialog_response;

  /**
   * ThunarRenamerDialog:current-directory:
   *
   * The current directory of the #ThunarRenamerDialog,
   * which is the default directory for the "Add Files"
   * dialog. May also be %NULL in which case the default
   * current directory of the file manager process will
   * be used.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_DIRECTORY,
                                   g_param_spec_object ("current-directory",
                                                        "current-directory",
                                                        "current-directory",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarRenamerDialog:selected-files:
   *
   * The list of currently selected #ThunarFile<!---->s
   * in this #ThunarRenamerDialog.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTED_FILES,
                                   g_param_spec_boxed ("selected-files",
                                                       "selected-files",
                                                       "selected-files",
                                                       THUNARX_TYPE_FILE_INFO_LIST,
                                                       EXO_PARAM_READABLE));

  /**
   * ThunarRenamerDialog:standalone:
   *
   * %TRUE if the #ThunarRenamerDialog should display a
   * toolbar and add several actions which are only
   * useful in a standalone fashion.
   *
   * If the #ThunarRenamerDialog is opened from within
   * Thunar this property will always be %FALSE.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_STANDALONE,
                                   g_param_spec_boolean ("standalone",
                                                         "standalone",
                                                         "standalone",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static gint
trd_renamer_compare (gconstpointer a,
                     gconstpointer b)
{
  return g_utf8_collate (thunarx_renamer_get_name (THUNARX_RENAMER (a)),
                         thunarx_renamer_get_name (THUNARX_RENAMER (b)));
}



static void
thunar_renamer_dialog_init (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxProviderFactory *provider_factory;
  GtkTreeViewColumn      *column;
  GtkTreeSelection       *selection;
  GtkCellRenderer        *renderer;
  GtkSizeGroup           *size_group;
  const gchar            *active_str;
  GHashTable             *settings;
  GEnumClass             *klass;
  GtkWidget              *notebook;
  GtkWidget              *toolbar;
  GtkToolItem            *seperator;
  GtkWidget              *button;
  GtkWidget              *mcombo;
  GtkWidget              *rcombo;
  GtkWidget              *frame;
  GtkWidget              *image;
  GtkWidget              *label;
  GtkWidget              *mbox;
  GtkWidget              *rbox;
  GtkWidget              *vbox;
  GtkWidget              *swin;
  GtkWidget              *infobar;
  XfceRc                 *rc;
  gchar                 **entries;
  GList                  *providers;
  GList                  *renamers = NULL;
  GList                  *lp;
  gint                    active = 0;
  guint                   n;

  /* allocate a new bulk rename model */
  renamer_dialog->model = thunar_renamer_model_new ();

  xfce_gtk_translate_action_entries (thunar_renamer_action_entries, G_N_ELEMENTS (thunar_renamer_action_entries));

  /* determine the list of available renamers (using the renamer providers) */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_RENAMER_PROVIDER);
  for (lp = providers; lp != NULL; lp = lp->next)
    {
      /* determine the renamers for this provider */
      renamers = g_list_concat (renamers, thunarx_renamer_provider_get_renamers (lp->data));

      /* release this provider */
      g_object_unref (G_OBJECT (lp->data));
    }
  g_object_unref (G_OBJECT (provider_factory));
  g_list_free (providers);

  /* initialize the dialog */
  gtk_window_set_default_size (GTK_WINDOW (renamer_dialog), 510, 490);
  gtk_window_set_title (GTK_WINDOW (renamer_dialog), _("Rename Multiple Files"));

  /* add the Cancel/Close buttons */
  renamer_dialog->cancel_button = gtk_dialog_add_button (GTK_DIALOG (renamer_dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  renamer_dialog->close_button = gtk_dialog_add_button (GTK_DIALOG (renamer_dialog), _("_Close"), GTK_RESPONSE_DELETE_EVENT);
  gtk_widget_hide (renamer_dialog->close_button);

  /* add the "Done" button */
  button = gtk_dialog_add_button (GTK_DIALOG (renamer_dialog), _("_Done"), GTK_RESPONSE_ACCEPT);
  g_object_bind_property (G_OBJECT (renamer_dialog->model), "can-rename",
                          G_OBJECT (button),                "sensitive",
                          G_BINDING_SYNC_CREATE);
  gtk_dialog_set_default_response (GTK_DIALOG (renamer_dialog), GTK_RESPONSE_ACCEPT);
  gtk_widget_set_tooltip_text (button, _("Click here to actually rename the files listed above to their new names and close the window."));

  /* add the "Apply" button */
  button = gtk_dialog_add_button (GTK_DIALOG (renamer_dialog), _("_Apply"), GTK_RESPONSE_APPLY);
  g_object_bind_property (G_OBJECT (renamer_dialog->model), "can-rename",
                          G_OBJECT (button),                "sensitive",
                          G_BINDING_SYNC_CREATE);
  gtk_dialog_set_default_response (GTK_DIALOG (renamer_dialog), GTK_RESPONSE_APPLY);
  gtk_widget_set_tooltip_text (button, _("Click here to actually rename the files listed above to their new names and keep the window open."));

  /* setup the action manager support for this dialog */
  renamer_dialog->action_mgr = g_object_new (THUNAR_TYPE_ACTION_MANAGER, "widget", GTK_WIDGET (renamer_dialog), NULL);
  g_object_bind_property (G_OBJECT (renamer_dialog), "selected-files", G_OBJECT (renamer_dialog->action_mgr), "selected-files", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (renamer_dialog), "current-directory", G_OBJECT (renamer_dialog->action_mgr), "current-directory", G_BINDING_SYNC_CREATE);

  /* add the toolbar to the dialog */
  toolbar = gtk_toolbar_new ();
  xfce_gtk_tool_button_new_from_action_entry (get_action_entry (THUNAR_RENAMER_ACTION_ADD_FILES), G_OBJECT (renamer_dialog), GTK_TOOLBAR (toolbar));
  xfce_gtk_tool_button_new_from_action_entry (get_action_entry (THUNAR_RENAMER_ACTION_REMOVE_FILES), G_OBJECT (renamer_dialog), GTK_TOOLBAR (toolbar));
  seperator = gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (seperator));
  gtk_widget_show (GTK_WIDGET (seperator));
  xfce_gtk_tool_button_new_from_action_entry (get_action_entry (THUNAR_RENAMER_ACTION_CLEAR), G_OBJECT (renamer_dialog), GTK_TOOLBAR (toolbar));
  seperator = gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (seperator));
  gtk_widget_show (GTK_WIDGET (seperator));
  xfce_gtk_tool_button_new_from_action_entry (get_action_entry (THUNAR_RENAMER_ACTION_ABOUT), G_OBJECT (renamer_dialog), GTK_TOOLBAR (toolbar));
  g_object_bind_property (G_OBJECT (renamer_dialog), "standalone", G_OBJECT (toolbar), "visible", G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (renamer_dialog))), toolbar, FALSE, FALSE, 0);
  gtk_widget_show_all (GTK_WIDGET (toolbar));

  /* create the main vbox */
  mbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (mbox), 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (renamer_dialog))), mbox, TRUE, TRUE, 0);
  gtk_widget_show (mbox);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (mbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  renamer_dialog->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (renamer_dialog->model));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (renamer_dialog->tree_view), THUNAR_RENAMER_MODEL_COLUMN_OLDNAME);
  gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (renamer_dialog->tree_view), TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (renamer_dialog->tree_view), TRUE);
  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (renamer_dialog->tree_view), TRUE);
  g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "button-press-event", G_CALLBACK (thunar_renamer_dialog_button_press_event), renamer_dialog);
  g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "popup-menu", G_CALLBACK (thunar_renamer_dialog_popup_menu), renamer_dialog);
  g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "row-activated", G_CALLBACK (thunar_renamer_dialog_row_activated), renamer_dialog);
  gtk_container_add (GTK_CONTAINER (swin), renamer_dialog->tree_view);
  gtk_widget_show (renamer_dialog->tree_view);

  /* create the tree view column for the old file name */
  renamer_dialog->name_column = column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_spacing (column, 2);
  gtk_tree_view_column_set_min_width (column, 100);
  gtk_tree_view_column_set_title (column, _("Name"));
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_fixed_width (column, 250);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_clickable (column, TRUE);
  g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (thunar_renamer_dialog_name_column_clicked), renamer_dialog);
  renderer = g_object_new (THUNAR_TYPE_ICON_RENDERER, "size", 16, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer, "file", THUNAR_RENAMER_MODEL_COLUMN_FILE, NULL);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "text", THUNAR_RENAMER_MODEL_COLUMN_OLDNAME, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (renamer_dialog->tree_view), column);

  /* create the tree view column for the new file name */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_min_width (column, 75);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_title (column, _("New Name"));
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "ellipsize", PANGO_ELLIPSIZE_END,
                           "foreground", "Red",
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "foreground-set", THUNAR_RENAMER_MODEL_COLUMN_CONFLICT,
                                       "text", THUNAR_RENAMER_MODEL_COLUMN_NEWNAME,
                                       "weight", THUNAR_RENAMER_MODEL_COLUMN_CONFLICT_WEIGHT,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (renamer_dialog->tree_view), column);

  /* create the vbox for the renamer parameters */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (mbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  /* create the hbox for the renamer selection */
  rbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start (GTK_BOX (vbox), rbox, FALSE, FALSE, 0);
  gtk_widget_show (rbox);

  /* create the rename progress bar */
  renamer_dialog->progress = thunar_renamer_progress_new ();
  gtk_box_pack_start (GTK_BOX (vbox), renamer_dialog->progress, FALSE, FALSE, 0);
  g_object_bind_property (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (rbox), "visible", G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (swin), "sensitive", G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (toolbar), "sensitive", G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (renamer_dialog->model), "frozen", G_BINDING_SYNC_CREATE);

  /* synchronize the height of the progress bar and the rbox with the combos */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  gtk_size_group_add_widget (size_group, renamer_dialog->progress);
  gtk_size_group_add_widget (size_group, rbox);
  g_object_unref (G_OBJECT (size_group));

  /* create the frame for the renamer widgets */
  frame = g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_ETCHED_IN, NULL);
  g_object_bind_property (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (frame), "sensitive", G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  renamer_dialog->mode_combo = NULL;

  /* check if we have any renamers */
  if (G_LIKELY (renamers != NULL))
    {
      /* sort the renamers by name */
      renamers = g_list_sort (renamers, trd_renamer_compare);

      /* open the config file, xfce_rc_config_open() should never return NULL */
      rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/renamerrc", TRUE);
      xfce_rc_set_group (rc, "Configuration");

      /* create the renamer combo box for the renamer selection */
      rcombo = gtk_combo_box_text_new ();
      for (lp = renamers; lp != NULL; lp = lp->next)
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rcombo), thunarx_renamer_get_name (lp->data));
      gtk_box_pack_start (GTK_BOX (rbox), rcombo, FALSE, FALSE, 0);
      gtk_widget_show (rcombo);

      /* add a "Help" button */
      button = gtk_button_new ();
      gtk_widget_set_tooltip_text (button, _("Click here to view the documentation for the selected rename operation."));
      g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_renamer_dialog_help), renamer_dialog);
      gtk_box_pack_start (GTK_BOX (rbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      image = gtk_image_new_from_icon_name ("help-browser", GTK_ICON_SIZE_BUTTON);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_widget_show (image);

      /* create the name/extension/both combo box */
      mcombo = gtk_combo_box_text_new ();
      klass = g_type_class_ref (THUNAR_TYPE_RENAMER_MODE);
      active_str = xfce_rc_read_entry_untranslated (rc, "LastActiveMode", "");
      for (active = 0, n = 0; n < klass->n_values; ++n)
        {
          if (g_strcmp0 (active_str, klass->values[n].value_name) == 0)
            active = n;
          gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mcombo), _(klass->values[n].value_nick));
        }
      g_object_bind_property (G_OBJECT (renamer_dialog->model), "mode", G_OBJECT (mcombo), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_box_pack_end (GTK_BOX (rbox), mcombo, FALSE, FALSE, 0);
      gtk_combo_box_set_active (GTK_COMBO_BOX (mcombo), active);
      g_type_class_unref (klass);
      gtk_widget_show (mcombo);
      renamer_dialog->mode_combo = mcombo;

      /* allocate the notebook with the renamers */
      notebook = gtk_notebook_new ();
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
      gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
      g_signal_connect (G_OBJECT (notebook), "notify::page", G_CALLBACK (thunar_renamer_dialog_notify_page), renamer_dialog);
      gtk_container_add (GTK_CONTAINER (frame), notebook);
      gtk_widget_show (notebook);

      /* determine the last active renamer (as string) */
      active_str = xfce_rc_read_entry_untranslated (rc, "LastActiveRenamer", "");

      /* add the renamers to the notebook */
      for (active = 0, lp = renamers; lp != NULL; lp = lp->next)
        {
          /* append the renamer to the notebook... */
          gtk_notebook_append_page (GTK_NOTEBOOK (notebook), lp->data, NULL);

          /* check if this page should be active by default */
          if (g_strcmp0 (G_OBJECT_TYPE_NAME (lp->data), active_str) == 0)
            active = g_list_position (renamers, lp);

          /* try to load the settings for the renamer */
          entries = xfce_rc_get_entries (rc, G_OBJECT_TYPE_NAME (lp->data));
          if (G_LIKELY (entries != NULL))
            {
              /* add the settings to a hash table */
              xfce_rc_set_group (rc, G_OBJECT_TYPE_NAME (lp->data));
              settings = g_hash_table_new (g_str_hash, g_str_equal);
              for (n = 0; entries[n] != NULL; ++n)
                g_hash_table_insert (settings, entries[n], (gchar *) xfce_rc_read_entry_untranslated (rc, entries[n], NULL));

              /* let the renamer load the settings */
              thunarx_renamer_load (THUNARX_RENAMER (lp->data), settings);

              /* cleanup */
              g_hash_table_destroy (settings);
              g_strfreev (entries);
            }

          /* ...and make sure its visible */
          gtk_widget_show (lp->data);
        }

      /* default to the first renamer */
      gtk_combo_box_set_active (GTK_COMBO_BOX (rcombo), active);

      /* connect the combo box to the notebook */
      g_object_bind_property (G_OBJECT (rcombo), "active", G_OBJECT (notebook), "page", G_BINDING_SYNC_CREATE);

      /* close the config handle */
      xfce_rc_close (rc);

      /* setup drop support for the tree view */
      gtk_drag_dest_set (renamer_dialog->tree_view, 0, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE);
      gtk_drag_source_set (renamer_dialog->tree_view, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets), GDK_ACTION_MOVE);
      g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "drag-data-received", G_CALLBACK (thunar_renamer_dialog_drag_data_received), renamer_dialog);
      g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "drag-leave", G_CALLBACK (thunar_renamer_dialog_drag_leave), renamer_dialog);
      g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "drag-motion", G_CALLBACK (thunar_renamer_dialog_drag_motion), renamer_dialog);
      g_signal_connect (G_OBJECT (renamer_dialog->tree_view), "drag-drop", G_CALLBACK (thunar_renamer_dialog_drag_drop), renamer_dialog);

      /* setup the tree selection */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (renamer_dialog->tree_view));
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
      g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (thunar_renamer_dialog_selection_changed), renamer_dialog);
      thunar_renamer_dialog_selection_changed (selection, renamer_dialog);
    }
  else
    {
      /* make the other widgets insensitive */
      gtk_widget_set_sensitive (toolbar, FALSE);
      gtk_widget_set_sensitive (swin, FALSE);

      /* display an error to the user */
      infobar = gtk_info_bar_new ();
      gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_ERROR);
      gtk_container_add (GTK_CONTAINER (frame), infobar);
      gtk_widget_show (infobar);

      image = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_DIALOG);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar))), image);
      gtk_widget_show (image);

      /* TRANSLATORS: You can test this string by temporarily removing thunar-sbr.* from $libdir/thunarx-2/,
       *              and opening the multi rename dialog by selecting multiple files and pressing F2.
       */
      label = gtk_label_new (_("No renamer modules were found on your system. Please check your\n"
                               "installation or contact your system administrator. If you install Thunar\n"
                               "from source, be sure to enable the \"Simple Builtin Renamers\" plugin."));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_label_set_selectable (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar))), label);
      gtk_widget_show (label);
    }

  /* release the renamers list */
  g_list_free_full (renamers, g_object_unref);
}



static void
thunar_renamer_dialog_dispose (GObject *object)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (object);

  /* reset the "current-directory" property */
  thunar_renamer_dialog_set_current_directory (renamer_dialog, NULL);

  (*G_OBJECT_CLASS (thunar_renamer_dialog_parent_class)->dispose) (object);
}



static void
thunar_renamer_dialog_finalize (GObject *object)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (object);

  /* release the action manager support */
  g_object_unref (G_OBJECT (renamer_dialog->action_mgr));

  /* release our bulk rename model */
  g_object_unref (G_OBJECT (renamer_dialog->model));

  /* release the list of selected files */
  thunarx_file_info_list_free (renamer_dialog->selected_files);

  (*G_OBJECT_CLASS (thunar_renamer_dialog_parent_class)->finalize) (object);
}



static void
thunar_renamer_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_renamer_dialog_get_current_directory (renamer_dialog));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_renamer_dialog_get_selected_files (renamer_dialog));
      break;

    case PROP_STANDALONE:
      g_value_set_boolean (value, thunar_renamer_dialog_get_standalone (renamer_dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_renamer_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_renamer_dialog_set_current_directory (renamer_dialog, g_value_get_object (value));
      break;

    case PROP_STANDALONE:
      thunar_renamer_dialog_set_standalone (renamer_dialog, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_renamer_dialog_realize (GtkWidget *widget)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (widget);
  GtkIconTheme        *icon_theme;

  /* realize the widget */
  (*GTK_WIDGET_CLASS (thunar_renamer_dialog_parent_class)->realize) (widget);

  /* grab a reference on the icon factory for the screen */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  renamer_dialog->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
}



static void
thunar_renamer_dialog_unrealize (GtkWidget *widget)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (widget);

  /* release the reference on the icon factory */
  g_object_unref (G_OBJECT (renamer_dialog->icon_factory));
  renamer_dialog->icon_factory = NULL;

  /* unrealize the dialog */
  (*GTK_WIDGET_CLASS (thunar_renamer_dialog_parent_class)->unrealize) (widget);
}



static void
thunar_renamer_dialog_response (GtkDialog *dialog,
                                gint       response)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (dialog);
  GtkTreeIter          iter;
  ThunarFile          *file;
  GList               *pair_list = NULL;
  gchar               *name;

  /* grab an additional reference on the dialog */
  g_object_ref (G_OBJECT (dialog));

  /* check if we should rename */
  if (G_LIKELY (response == GTK_RESPONSE_ACCEPT  || response == GTK_RESPONSE_APPLY))
    {
      /* hide the close button and show the cancel button */
      gtk_widget_show (renamer_dialog->cancel_button);
      gtk_widget_hide (renamer_dialog->close_button);

      /* save the current settings */
      thunar_renamer_dialog_save (renamer_dialog);

      /* display the rename progress bar */
      gtk_widget_show (renamer_dialog->progress);

      /* determine the iterator for the first item */
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (renamer_dialog->model), &iter))
        {
          /* determine the tree row references for the rows that should be renamed */
          do
            {
              /* determine the file and newname for the row */
              gtk_tree_model_get (GTK_TREE_MODEL (renamer_dialog->model), &iter,
                                  THUNAR_RENAMER_MODEL_COLUMN_NEWNAME, &name,
                                  THUNAR_RENAMER_MODEL_COLUMN_FILE, &file,
                                  -1);

              /* check if this row should be renamed */
              if (G_LIKELY (name != NULL && *name != '\0'))
                pair_list = g_list_prepend (pair_list, thunar_renamer_pair_new (file, name));

              /* cleanup */
              g_object_unref (file);
              g_free (name);
            }
          while (gtk_tree_model_iter_next (GTK_TREE_MODEL (renamer_dialog->model), &iter));
        }

      /* perform the rename (returns when done) */
      thunar_renamer_progress_run (THUNAR_RENAMER_PROGRESS (renamer_dialog->progress), pair_list);

      /* hide the rename progress bar */
      gtk_widget_hide (renamer_dialog->progress);

      /* release the pairs */
      thunar_renamer_pair_list_free (pair_list);

      /* check if we're in standalone mode */
      if (thunar_renamer_dialog_get_standalone (renamer_dialog) || response == GTK_RESPONSE_APPLY)
        {
          /* hide the cancel button again and display the close button */
          gtk_widget_hide (renamer_dialog->cancel_button);
          gtk_widget_show (renamer_dialog->close_button);
        }
      else
        {
          /* hide and destroy the dialog window */
          gtk_widget_hide (GTK_WIDGET (dialog));
          gtk_widget_destroy (GTK_WIDGET (dialog));
        }
    }
  else if (thunar_renamer_progress_running (THUNAR_RENAMER_PROGRESS (renamer_dialog->progress)))
    {
      /* cancel the rename operation */
      thunar_renamer_progress_cancel (THUNAR_RENAMER_PROGRESS (renamer_dialog->progress));
    }
  else
    {
      /* hide and destroy the dialog window */
      gtk_widget_hide (GTK_WIDGET (dialog));
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }

  /* release the additional reference on the dialog */
  g_object_unref (G_OBJECT (dialog));
}



/**
 * thunar_renamer_dialog_append_menu_item:
 * @renamer_dialog : Instance of a  #ThunarRenamerDialog
 * @menu           : #GtkMenuShell to which the item should be added
 * @action         : #ThunarRenamerAction to select which item should be added
 *
 * Adds the selected, widget specific #GtkMenuItem to the passed #GtkMenuShell
 *
 * Return value: (transfer none): The added #GtkMenuItem
 **/
static GtkWidget*
thunar_renamer_dialog_append_menu_item (ThunarRenamerDialog *renamer_dialog,
                                        GtkMenuShell        *menu,
                                        ThunarRenamerAction  action)
{
  gchar                      *label_text;
  gchar                      *tooltip_text;
  const XfceGtkActionEntry   *action_entry = get_action_entry (action);
  GtkWidget                  *item;

  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), NULL);
  _thunar_return_val_if_fail (action_entry != NULL, NULL);

  switch (action)
    {
      case THUNAR_RENAMER_ACTION_REMOVE_FILES:
        label_text = ngettext ("Remove File", "Remove Files", g_list_length (renamer_dialog->selected_files));
        tooltip_text = ngettext ("Remove the selected file from the list of files to be renamed",
                                 "Remove the selected files from the list of files to be renamed", g_list_length (renamer_dialog->selected_files));
        item = xfce_gtk_image_menu_item_new_from_icon_name (label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                                            G_OBJECT (renamer_dialog), action_entry->menu_item_icon_name, GTK_MENU_SHELL (menu) );
        gtk_widget_set_sensitive (item, renamer_dialog->selected_files != NULL);
        return item;
      default:
        return xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (renamer_dialog), menu);
    }
  return NULL;
}



static void
thunar_renamer_dialog_context_menu (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxRenamer *renamer;
  ThunarMenu     *menu;
  GList          *items = NULL;
  GList          *lp = NULL;

  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* grab an additional reference on the dialog */
  g_object_ref (G_OBJECT (renamer_dialog));

  menu = g_object_new (THUNAR_TYPE_MENU, "menu-type", THUNAR_MENU_TYPE_CONTEXT_RENAMER,
                                         "action_mgr", renamer_dialog->action_mgr,
                                         "tab-support-disabled", TRUE,
                                         "change_directory-support-disabled", TRUE, NULL);
  thunar_menu_add_sections (menu, THUNAR_MENU_SECTION_OPEN | THUNAR_MENU_SECTION_SENDTO);

  /* determine the active renamer */
  renamer = thunar_renamer_model_get_renamer (renamer_dialog->model);
  if (G_LIKELY (renamer != NULL))
    {
      /* determine the menu items provided by the active renamer */
      items = thunarx_renamer_get_menu_items (renamer, GTK_WINDOW (renamer_dialog), renamer_dialog->selected_files);
      if (items != NULL)
        {
          for(lp = items; lp != NULL; lp = lp->next)
              thunar_gtk_menu_thunarx_menu_item_new (lp->data, GTK_MENU_SHELL (menu));
          g_list_free (items);
          xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
        }
    }
  thunar_renamer_dialog_append_menu_item (renamer_dialog, GTK_MENU_SHELL (menu), THUNAR_RENAMER_ACTION_ADD_FILES);
  thunar_renamer_dialog_append_menu_item (renamer_dialog, GTK_MENU_SHELL (menu), THUNAR_RENAMER_ACTION_REMOVE_FILES);
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  thunar_menu_add_sections (menu, THUNAR_MENU_SECTION_PROPERTIES);
  thunar_gtk_menu_hide_accel_labels (GTK_MENU (menu));
  gtk_widget_show_all (GTK_WIDGET (menu));

  thunar_gtk_menu_run (GTK_MENU (menu));

  /* release the additional reference on the dialog */
  g_object_unref (G_OBJECT (renamer_dialog));
}



static void
thunar_renamer_dialog_help (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxRenamer *renamer;
  const gchar    *help_url = NULL;
  GError         *error = NULL;
  gboolean        uri_launched = FALSE;


  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* check if the active renamer provides a help URL */
  renamer = thunar_renamer_model_get_renamer (renamer_dialog->model);
  if (G_LIKELY (renamer != NULL))
    help_url = thunarx_renamer_get_help_url (renamer);

  /* check if we have a specific URL */
  if (G_LIKELY (help_url == NULL))
    {
      /* open the general documentation if no specific URL */
      xfce_dialog_show_help (GTK_WINDOW (renamer_dialog), "thunar",
                             "bulk-renamer/start", NULL);
    }
  else
    {
      /* try to launch the specific URL */
      uri_launched = gtk_show_uri_on_window (GTK_WINDOW (renamer_dialog),
                                             help_url,
                                             gtk_get_current_event_time (),
                                             &error);

      if (!uri_launched)
        {
          /* tell the user that we failed */
          thunar_dialogs_show_error (GTK_WIDGET (renamer_dialog), error, _("Failed to open the documentation browser"));

          /* release the error */
          g_error_free (error);
        }
    }
}



static void
trd_save_foreach (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  xfce_rc_write_entry (user_data, key, value);
}



static void
thunar_renamer_dialog_save (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxRenamer *renamer;
  GHashTable     *settings;
  GEnumClass     *klass;
  GEnumValue     *value;
  XfceRc         *rc;

  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* try to open the config handle for writing */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/renamerrc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    return;

  /* determine the currently active renamer */
  renamer = thunar_renamer_model_get_renamer (renamer_dialog->model);
  if (G_LIKELY (renamer != NULL))
    {
      /* save general settings first */
      xfce_rc_set_group (rc, "Configuration");

      /* remember current mode as last active */
      klass = g_type_class_ref (THUNAR_TYPE_RENAMER_MODE);
      value = g_enum_get_value (klass, thunar_renamer_model_get_mode (renamer_dialog->model));
      if (G_LIKELY (value != NULL))
        xfce_rc_write_entry (rc, "LastActiveMode", value->value_name);
      g_type_class_unref (klass);

      /* remember renamer as last active */
      xfce_rc_write_entry (rc, "LastActiveRenamer", G_OBJECT_TYPE_NAME (renamer));

      /* save the renamer's settings */
      settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      xfce_rc_delete_group (rc, G_OBJECT_TYPE_NAME (renamer), TRUE);
      xfce_rc_set_group (rc, G_OBJECT_TYPE_NAME (renamer));
      thunarx_renamer_save (renamer, settings);
      g_hash_table_foreach (settings, trd_save_foreach, rc);
      g_hash_table_destroy (settings);
    }

  /* close the config handle */
  xfce_rc_close (rc);
}



static gboolean
trd_audio_filter_func (const GtkFileFilterInfo *info,
                       gpointer                 data)
{
  /* check if the MIME type is an audio MIME type */
  return (strncmp (info->mime_type, "audio/", 6) == 0);
}



static gboolean
trd_video_filter_func (const GtkFileFilterInfo *info,
                       gpointer                 data)
{
  /* check if the MIME type is a video MIME type */
  return (strncmp (info->mime_type, "video/", 6) == 0);
}



static gboolean
thunar_renamer_dialog_action_add_files (ThunarRenamerDialog *renamer_dialog)
{
  GtkFileFilter *filter;
  ThunarFile    *file;
  GtkWidget     *chooser;
  GSList        *uris;
  GSList        *lp;
  gchar         *uri;

  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);

  /* allocate the file chooser */
  chooser = gtk_file_chooser_dialog_new (_("Select files to rename"),
                                         GTK_WINDOW (renamer_dialog),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  /* add file chooser filers */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Audio Files"));
  gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_MIME_TYPE, trd_audio_filter_func, NULL, NULL);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Image Files"));
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Video Files"));
  gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_MIME_TYPE, trd_video_filter_func, NULL, NULL);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* check if "current-directory" is set */
  if (G_LIKELY (renamer_dialog->current_directory != NULL))
    {
      /* use the "current-directory" as default for the chooser */
      uri = thunarx_file_info_get_uri (THUNARX_FILE_INFO (renamer_dialog->current_directory));
      gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (chooser), uri);
      g_free (uri);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      /* determine the URIs of the selected files */
      uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
      for (lp = uris; lp != NULL; lp = lp->next)
        {
          /* determine the file for the URI */
          file = thunar_file_get_for_uri (lp->data, NULL);
          if (G_LIKELY (file != NULL))
            {
              /* append the file to the renamer model */
              thunar_renamer_model_append (renamer_dialog->model, file);

              /* unset sort order */
              gtk_tree_view_column_set_sort_indicator (renamer_dialog->name_column, FALSE);

              /* release the file */
              g_object_unref (G_OBJECT (file));
            }

          /* release the URI */
          g_free (lp->data);
        }
      g_slist_free (uris);

      /* determine the current folder of the chooser */
      uri = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (chooser));
      if (G_LIKELY (uri != NULL))
        {
          /* determine the file for that URI */
          file = thunar_file_get_for_uri (uri, NULL);
          if (G_LIKELY (file != NULL))
            {
              /* remember the current folder of the chooser as default */
              thunar_renamer_dialog_set_current_directory (renamer_dialog, file);

              /* release the file */
              g_object_unref (G_OBJECT (file));
            }

          /* release the URI */
          g_free (uri);
        }
    }

  /* cleanup */
  gtk_widget_destroy (chooser);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_renamer_dialog_action_remove_files (ThunarRenamerDialog *renamer_dialog)
{
  GtkTreeRowReference *row;
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreePath         *path;
  GList               *rows;
  GList               *lp;

  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);

  /* determine the selected rows in the view */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (renamer_dialog->tree_view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);
  for (lp = rows; lp != NULL; lp = lp->next)
    {
      /* turn the path into a tree row reference */
      row = gtk_tree_row_reference_new (model, lp->data);
      gtk_tree_path_free (lp->data);
      lp->data = row;
    }

  /* actually remove the rows */
  for (lp = rows; lp != NULL; lp = lp->next)
    {
      /* determine the path for the row */
      path = gtk_tree_row_reference_get_path (lp->data);
      if (G_LIKELY (path != NULL))
        {
          /* remove the row from the model */
          thunar_renamer_model_remove (THUNAR_RENAMER_MODEL (model), path);
          gtk_tree_path_free (path);
        }

      /* release the row reference */
      gtk_tree_row_reference_free (lp->data);
    }

  /* cleanup */
  g_list_free (rows);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_renamer_dialog_action_clear (ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);

  /* just clear the list of files in the model */
  thunar_renamer_model_clear (renamer_dialog->model);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_renamer_dialog_action_about (ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);

  /* just popup the about dialog */
  thunar_dialogs_show_about (GTK_WINDOW (renamer_dialog), _("Bulk Rename"),
                             _("Thunar Bulk Rename is a powerful and extensible\n"
                               "tool to rename multiple files at once."));

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_renamer_dialog_name_column_clicked (GtkTreeViewColumn   *column,
                                           ThunarRenamerDialog *renamer_dialog)
{
  GtkSortType sort_order = GTK_SORT_ASCENDING;

  _thunar_return_if_fail (renamer_dialog->name_column == column);

  if (!gtk_tree_view_column_get_sort_indicator (column))
    {
      /* show sort order */
      gtk_tree_view_column_set_sort_indicator (column, TRUE);
    }
  else
    {
      /* invert new sort order */
      if (gtk_tree_view_column_get_sort_order (column) == GTK_SORT_ASCENDING)
        sort_order = GTK_SORT_DESCENDING;
      else
        sort_order = GTK_SORT_ASCENDING;
    }

  /* set new sort direction */
  gtk_tree_view_column_set_sort_order (column, sort_order);

  /* sort the model */
  thunar_renamer_model_sort (renamer_dialog->model, sort_order);
}



static gboolean
thunar_renamer_dialog_button_press_event (GtkWidget           *tree_view,
                                          GdkEventButton      *event,
                                          ThunarRenamerDialog *renamer_dialog)
{
  GtkTreeSelection *selection;
  GtkTreePath      *path;

  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* determine the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  /* we unselect all selected items if the user clicks on an empty
   * area of the treeview and no modifier key is active.
   */
  if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0
      && !gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, NULL, NULL, NULL, NULL))
    {
      /* just unselect all rows */
      gtk_tree_selection_unselect_all (selection);
    }

  /* check if we should popup the context menu */
  if (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view)) && event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      /* determine the path at the event coordinates */
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL))
        {
          /* select the path on which the user clicked if not already selected */
          if (!gtk_tree_selection_path_is_selected (selection, path))
            {
              /* we don't unselect all other items if Control is active */
              if ((event->state & GDK_CONTROL_MASK) == 0)
                gtk_tree_selection_unselect_all (selection);
              gtk_tree_selection_select_path (selection, path);
            }

          /* release the tree path */
          gtk_tree_path_free (path);
        }

      /* popup the context menu */
      thunar_renamer_dialog_context_menu (renamer_dialog);

      /* we handled the event */
      return TRUE;
    }

  return FALSE;
}



static void
thunar_renamer_dialog_drag_data_received (GtkWidget           *tree_view,
                                          GdkDragContext      *context,
                                          gint                 x,
                                          gint                 y,
                                          GtkSelectionData    *selection_data,
                                          guint                info,
                                          guint                timestamp,
                                          ThunarRenamerDialog *renamer_dialog)
{
  ThunarFile              *file;
  GList                   *file_list;
  GList                   *lp;
  GtkTreeModel            *model;
  GtkTreePath             *path;
  GtkTreeViewDropPosition  drop_pos;
  gint                     position = -1;

  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));
  _thunar_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* we only accept text/uri-list drops with format 8 and atleast one byte of data */
  if (info == TARGET_TEXT_URI_LIST && gtk_selection_data_get_format (selection_data) == 8 && gtk_selection_data_get_length (selection_data) > 0)
    {
      /* determine the renamer model */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

      /* get the drop position, if no path is foud we append the files */
      if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (tree_view), x, y, &path, &drop_pos))
        {
          /* get the position of the path */
          position = gtk_tree_path_get_indices (path)[0];
          gtk_tree_path_free (path);

          /* advance the offset if we drop after the item */
          if (drop_pos == GTK_TREE_VIEW_DROP_AFTER || drop_pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
            position++;

          /* append if we've reached the end of the model */
          if (position >= gtk_tree_model_iter_n_children (model, NULL))
            position = -1;
        }

      /* determine the file list from the selection_data */
      file_list = thunar_g_file_list_new_from_string ((const gchar *) gtk_selection_data_get_data (selection_data));

      /* add all paths to the model */
      for (lp = file_list; lp != NULL; lp = lp->next)
        {
          /* determine the file for the path */
          file = thunar_file_get (lp->data, NULL);
          if (G_LIKELY (file != NULL))
            {
              /* insert the file in the model */
              thunar_renamer_model_insert (renamer_dialog->model, file, position);

              /* unset sort order */
              gtk_tree_view_column_set_sort_indicator (renamer_dialog->name_column, FALSE);

              /* advance the offset if we do not append, so the drop order is consistent */
              if (position != -1)
                position++;

              /* release the file */
              g_object_unref (file);
            }

          /* release the GFile */
          g_object_unref (lp->data);
        }

      /* finish the drag */
      gtk_drag_finish (context, (file_list != NULL), FALSE, timestamp);

      /* release the list */
      g_list_free (file_list);
    }

  /* stop the emission of the "drag-data-received" signal */
  g_signal_stop_emission_by_name (G_OBJECT (tree_view), "drag-data-received");
}



static void
thunar_renamer_dialog_drag_leave (GtkWidget           *tree_view,
                                  GdkDragContext      *context,
                                  guint                timestamp,
                                  ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));
  _thunar_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* disable the highlighting */
  if (renamer_dialog->drag_highlighted)
    {
      /* we use the tree view parent (the scrolled window),
       * as the drag_highlight doesn't work for tree views.
       */
      gtk_drag_unhighlight (gtk_widget_get_parent (tree_view));
      renamer_dialog->drag_highlighted = FALSE;
    }
}



static gboolean
thunar_renamer_dialog_drag_motion (GtkWidget           *tree_view,
                                   GdkDragContext      *context,
                                   gint                 x,
                                   gint                 y,
                                   guint                timestamp,
                                   ThunarRenamerDialog *renamer_dialog)
{
  GdkAtom                  target;
  GtkTreeViewDropPosition  position;
  GtkTreePath             *path;
  GdkDragAction            action;
  GtkTreeModel            *model;
  gint                     n_rows;

  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* determine the drop target */
  target = gtk_drag_dest_find_target (tree_view, context, NULL);
  if (target == gdk_atom_intern_static_string ("text/uri-list"))
    {
      /* set the drag action for uris */
      action = GDK_ACTION_COPY;
    }
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      /* set the drag action for tree rows */
      action = GDK_ACTION_MOVE;
    }
  else
    {
      if (renamer_dialog->drag_highlighted)
        {
          /* we use the tree view parent (the scrolled window),
           * as the drag_highlight doesn't work for tree views.
           */
          gtk_drag_unhighlight (gtk_widget_get_parent (tree_view));
          renamer_dialog->drag_highlighted = FALSE;
        }

      /* we cannot handle the drop */
      return FALSE;
    }

  /* compute the drop position */
  if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (tree_view), x, y, &path, &position))
    {
      /* we can only drop before or after an item */
      if (position == GTK_TREE_VIEW_DROP_BEFORE || position == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)
        position = GTK_TREE_VIEW_DROP_BEFORE;
      else
        position = GTK_TREE_VIEW_DROP_AFTER;
    }
  else
    {
      /* append the item if there are rows in the model */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
      n_rows = gtk_tree_model_iter_n_children (model, NULL);

      if (G_LIKELY (n_rows > 0))
        {
          /* get the last tree path in the model */
          path = gtk_tree_path_new_from_indices (n_rows - 1, -1);
          position = GTK_TREE_VIEW_DROP_AFTER;
        }
    }

  if (G_LIKELY (path != NULL))
    {
      /* highlight the appropriate row */
      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (tree_view), path, position);
      gtk_tree_path_free (path);
    }
  else if (!renamer_dialog->drag_highlighted)
    {
      /* highlight the parent */
      gtk_drag_highlight (gtk_widget_get_parent (tree_view));
      renamer_dialog->drag_highlighted = TRUE;
    }

  /* we can handle the drop */
  gdk_drag_status (context, action, timestamp);
  return TRUE;
}



static gboolean
thunar_renamer_dialog_drag_drop (GtkWidget           *tree_view,
                                 GdkDragContext      *context,
                                 gint                 x,
                                 gint                 y,
                                 guint                timestamp,
                                 ThunarRenamerDialog *renamer_dialog)
{
  GdkAtom                  target;
  GtkTreeSelection        *selection;
  GtkTreePath             *dst_path;
  GtkTreeModel            *model;
  GtkTreeViewDropPosition  drop_pos;
  gint                     position = -1;
  GList                   *rows;

  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* determine the drop target */
  target = gtk_drag_dest_find_target (tree_view, context, NULL);
  if (G_LIKELY (target == gdk_atom_intern_static_string ("text/uri-list")))
    {
      /* request the data, we call gtk_drag_finish() later */
      gtk_drag_get_data (tree_view, context, target, timestamp);
    }
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
      rows = gtk_tree_selection_get_selected_rows (selection, &model);
      if (G_LIKELY (rows != NULL))
        {
          /* get the drop position, if no path is found we append the file */
          if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (tree_view), x, y, &dst_path, &drop_pos))
            {
              /* get the position of the path */
              position = gtk_tree_path_get_indices (dst_path)[0];
              gtk_tree_path_free (dst_path);

              /* advance the offset if we drop after the item */
              if (drop_pos == GTK_TREE_VIEW_DROP_AFTER || drop_pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
                position++;
            }

          /* perform the move */
          if (G_LIKELY (rows != NULL))
            {
              thunar_renamer_model_reorder (renamer_dialog->model, rows, position);
              g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);
            }

          /* unset sort column */
          gtk_tree_view_column_set_sort_indicator (renamer_dialog->name_column, FALSE);
        }

      /* finish the dnd operation */
      gtk_drag_finish (context, TRUE, FALSE, timestamp);
    }
  else
    {
      /* we cannot handle the drop */
      return FALSE;
    }

  return TRUE;
}



static void
thunar_renamer_dialog_notify_page (GtkNotebook         *notebook,
                                   GParamSpec          *pspec,
                                   ThunarRenamerDialog *renamer_dialog)
{
  GtkWidget *renamer;
  gint       page;

  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* determine the currently active renamer page */
  page = gtk_notebook_get_current_page (notebook);
  if (G_LIKELY (page >= 0))
    {
      /* determine the renamer for that page... */
      renamer = gtk_notebook_get_nth_page (notebook, page);

      /* ...and set that renamer for the model */
      thunar_renamer_model_set_renamer (renamer_dialog->model, THUNARX_RENAMER (renamer));
    }
}



static gboolean
thunar_renamer_dialog_popup_menu (GtkWidget           *tree_view,
                                  ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);

  /* popup the context menu */
  thunar_renamer_dialog_context_menu (renamer_dialog);

  /* we handled the event */
  return TRUE;
}



static void
thunar_renamer_dialog_row_activated (GtkTreeView         *tree_view,
                                     GtkTreePath         *path,
                                     GtkTreeViewColumn   *column,
                                     ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  thunar_action_manager_activate_selected_files (renamer_dialog->action_mgr, THUNAR_ACTION_MANAGER_OPEN_AS_NEW_WINDOW, NULL);
}



static void
thunar_renamer_dialog_selection_changed (GtkTreeSelection    *selection,
                                         ThunarRenamerDialog *renamer_dialog)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  ThunarFile   *file;
  GList        *rows;
  GList        *lp;
  gint          n_selected_files = 0;

  _thunar_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* release the list of previously selected files */
  thunarx_file_info_list_free (renamer_dialog->selected_files);
  renamer_dialog->selected_files = NULL;

  /* determine the new list of selected files */
  rows = gtk_tree_selection_get_selected_rows (selection, &model);
  for (lp = g_list_last (rows); lp != NULL; lp = lp->prev, ++n_selected_files)
    {
      /* determine the iter for this path */
      if (gtk_tree_model_get_iter (model, &iter, lp->data))
        {
          /* determine the file for this row and prepend it to our list */
          gtk_tree_model_get (model, &iter, THUNAR_RENAMER_MODEL_COLUMN_FILE, &file, -1);
          if (G_LIKELY (file != NULL))
            renamer_dialog->selected_files = g_list_prepend (renamer_dialog->selected_files, file);
        }

      /* release this path */
      gtk_tree_path_free (lp->data);
    }
  g_list_free (rows);

  /* notify listeners */
  g_object_notify (G_OBJECT (renamer_dialog), "selected-files");
}



/**
 * thunar_renamer_dialog_get_current_directory:
 * @renamer_dialog : a #ThunarRenamerDialog.
 *
 * Returns the current directory of the @renamer_dialog, which
 * is used as default directory for the "Add Files" dialog.
 * May also be %NULL in which case the current directory of the
 * file manager process will be used.
 *
 * Return value: the current directory for @renamer_dialog.
 **/
static ThunarFile*
thunar_renamer_dialog_get_current_directory (ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), NULL);
  return renamer_dialog->current_directory;
}



/**
 * thunar_renamer_dialog_set_current_directory:
 * @renamer_dialog    : a #ThunarRenamerDialog.
 * @current_directory : a #ThunarFile or %NULL.
 *
 * The @renamer_dialog will use @current_directory as default
 * directory for the "Add Files" dialog. If @current_directory
 * is %NULL, the current directory of the file manager process
 * will be used.
 **/
static void
thunar_renamer_dialog_set_current_directory (ThunarRenamerDialog *renamer_dialog,
                                             ThunarFile          *current_directory)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));
  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* check if we already use that directory */
  if (G_UNLIKELY (renamer_dialog->current_directory == current_directory))
    return;

  /* disconnect from the previous directory */
  if (renamer_dialog->current_directory != NULL)
    g_object_unref (G_OBJECT (renamer_dialog->current_directory));

  /* activate the new directory */
  renamer_dialog->current_directory = current_directory;

  /* connect to the new directory */
  if (current_directory != NULL)
    g_object_ref (G_OBJECT (current_directory));

  /* notify listeners */
  g_object_notify (G_OBJECT (renamer_dialog), "current-directory");
}



/**
 * thunar_renamer_dialog_get_selected_files:
 * @renamer_dialog : a #ThunarRenamerDialog.
 *
 * Returns the #GList of the currently selected #ThunarFile<!---->s
 * in @renamer_dialog.
 *
 * Return value: the list of currently selected files.
 **/
static GList*
thunar_renamer_dialog_get_selected_files (ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), NULL);
  return renamer_dialog->selected_files;
}



/**
 * thunar_renamer_dialog_get_standalone:
 * @renamer_dialog : a #ThunarRenamerDialog.
 *
 * Returns %TRUE if the @renamer_dialog is standalone, i.e.
 * if it was not opened from within the file manager with
 * a preset file list.
 *
 * Return value: %TRUE if @renamer_dialog is standalone.
 **/
static gboolean
thunar_renamer_dialog_get_standalone (ThunarRenamerDialog *renamer_dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog), FALSE);
  return renamer_dialog->standalone;
}



/**
 * thunar_renamer_dialog_set_standalone:
 * @renamer_dialog : a #ThunarRenamerDialog.
 * @standalone     : the new standalone state for @renamer_dialog.
 *
 * If @standalone is %TRUE the @renamer_dialog will display a
 * toolbar and several other actions that do not make sense
 * when opened from within the file manager.
 **/
static void
thunar_renamer_dialog_set_standalone (ThunarRenamerDialog *renamer_dialog,
                                      gboolean             standalone)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* normalize the value */
  standalone = !!standalone;

  /* check if we have a new setting */
  if (G_LIKELY (renamer_dialog->standalone != standalone))
    {
      /* apply the new setting */
      renamer_dialog->standalone = standalone;

      /* change title to reflect the standalone status */
      gtk_window_set_title (GTK_WINDOW (renamer_dialog), standalone ? _("Bulk Rename - Rename Multiple Files") : _("Rename Multiple Files"));

      /* display the appropriate Cancel/Close button */
      if (G_UNLIKELY (standalone))
        {
          /* standalone has Close button by default */
          gtk_widget_hide (renamer_dialog->cancel_button);
          gtk_widget_show (renamer_dialog->close_button);
        }
      else
        {
          /* else, Cancel button is default */
          gtk_widget_show (renamer_dialog->cancel_button);
          gtk_widget_hide (renamer_dialog->close_button);
        }

      /* notify listeners */
      g_object_notify (G_OBJECT (renamer_dialog), "standalone");
    }
}



/**
 * thunar_show_renamer_dialog:
 * @parent            : the #GtkWidget or the #GdkScreen on which to open the
 *                      dialog. May also be %NULL in which case the default
 *                      #GdkScreen will be used.
 * @current_directory : the current directory for the #ThunarRenamerDialog,
 *                      which is used as default folder for the "Add Files"
 *                      dialog. May also be %NULL in which case the current
 *                      directory of the file manager process will be used.
 * @files             : the list of #ThunarFile<!---->s to rename.
 * @standalone        : whether the dialog should appear like a standalone
 *                      application instead of an integrated renamer dialog.
 * @startup_id        : startup id to set on the window or %NULL.
 *
 * Convenience function to display a #ThunarRenamerDialog with
 * the given parameters.
 **/
void
thunar_show_renamer_dialog (gpointer     parent,
                            ThunarFile  *current_directory,
                            GList       *files,
                            gboolean     standalone,
                            const gchar *startup_id)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GtkWidget         *dialog;
  GtkWidget         *window = NULL;
  GtkComboBox       *mode_combo;
  GList             *lp;
  gboolean           directories_only;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* determine the screen for the dialog */
  if (G_UNLIKELY (parent == NULL))
    {
      /* just use the default screen, no toplevel window */
      screen = gdk_screen_get_default ();
    }
  else if (GTK_IS_WIDGET (parent))
    {
      /* use the screen for the widget and the toplevel window */
      screen = gtk_widget_get_screen (parent);
      window = gtk_widget_get_toplevel (parent);
    }
  else
    {
      /* parent is a screen, no toplevel window */
      screen = GDK_SCREEN (parent);
    }

  /* display the bulk rename dialog */
  dialog = g_object_new (THUNAR_TYPE_RENAMER_DIALOG,
                         "current-directory", current_directory,
                         "screen", screen,
                         "standalone", standalone,
                         NULL);

  /* set the dialogs startup id if available */
  if (startup_id != NULL && *startup_id != '\0')
    gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);

  /* check if we have a toplevel window */
  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* dialog is transient for toplevel window, but not modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    }

  /* append all specified files to the dialog's model */
  for (lp = files; lp != NULL; lp = lp->next)
    thunar_renamer_model_append (THUNAR_RENAMER_DIALOG (dialog)->model, lp->data);

  /* if there are only directories selected change the mode to both */
  directories_only = g_list_length (files) == 0 ? FALSE : TRUE;

  for (lp = files; lp != NULL; lp = lp->next)
    if (thunar_file_is_directory (THUNAR_FILE (lp->data)) == FALSE)
      directories_only = FALSE;

  mode_combo = GTK_COMBO_BOX (THUNAR_RENAMER_DIALOG (dialog)->mode_combo);
  if (directories_only && mode_combo != NULL)
    gtk_combo_box_set_active (mode_combo, THUNAR_RENAMER_MODE_BOTH);

  /* let the application handle the dialog */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* display the dialog */
  gtk_widget_show (dialog);
}


