/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-util.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CASE_SENSITIVE,
  PROP_DATE_STYLE,
  PROP_DATE_CUSTOM_STYLE,
  PROP_FOLDER,
  PROP_FOLDERS_FIRST,
  PROP_NUM_FILES,
  PROP_SHOW_HIDDEN,
  PROP_FOLDER_ITEM_COUNT,
  PROP_FILE_SIZE_BINARY,
  N_PROPERTIES
};

/* Signal identifiers */
enum
{
  ERROR,
  SEARCH_DONE,
  LAST_SIGNAL,
};



typedef gint (*ThunarSortFunc) (const ThunarFile *a,
                                const ThunarFile *b,
                                gboolean          case_sensitive);

static void               thunar_list_model_tree_model_init             (GtkTreeModelIface            *iface);
static void               thunar_list_model_drag_dest_init              (GtkTreeDragDestIface         *iface);
static void               thunar_list_model_sortable_init               (GtkTreeSortableIface         *iface);
static void               thunar_list_model_dispose                     (GObject                      *object);
static void               thunar_list_model_finalize                    (GObject                      *object);
static void               thunar_list_model_get_property                (GObject                      *object,
                                                                         guint                         prop_id,
                                                                         GValue                       *value,
                                                                         GParamSpec                   *pspec);
static void               thunar_list_model_set_property                (GObject                      *object,
                                                                         guint                         prop_id,
                                                                         const GValue                 *value,
                                                                         GParamSpec                   *pspec);
static GtkTreeModelFlags  thunar_list_model_get_flags                   (GtkTreeModel                 *model);
static gint               thunar_list_model_get_n_columns               (GtkTreeModel                 *model);
static GType              thunar_list_model_get_column_type             (GtkTreeModel                 *model,
                                                                         gint                          idx);
static gboolean           thunar_list_model_get_iter                    (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter,
                                                                         GtkTreePath                  *path);
static GtkTreePath       *thunar_list_model_get_path                    (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter);
static void               thunar_list_model_get_value                   (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter,
                                                                         gint                          column,
                                                                         GValue                       *value);
static gboolean           thunar_list_model_iter_next                   (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter);
static gboolean           thunar_list_model_iter_children               (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter,
                                                                         GtkTreeIter                  *parent);
static gboolean           thunar_list_model_iter_has_child              (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter);
static gint               thunar_list_model_iter_n_children             (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter);
static gboolean           thunar_list_model_iter_nth_child              (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter,
                                                                         GtkTreeIter                  *parent,
                                                                         gint                          n);
static gboolean           thunar_list_model_iter_parent                 (GtkTreeModel                 *model,
                                                                         GtkTreeIter                  *iter,
                                                                         GtkTreeIter                  *child);
static gboolean           thunar_list_model_drag_data_received          (GtkTreeDragDest              *dest,
                                                                         GtkTreePath                  *path,
                                                                         GtkSelectionData             *data);
static gboolean           thunar_list_model_row_drop_possible           (GtkTreeDragDest              *dest,
                                                                         GtkTreePath                  *path,
                                                                         GtkSelectionData             *data);
static gboolean           thunar_list_model_get_sort_column_id          (GtkTreeSortable              *sortable,
                                                                         gint                         *sort_column_id,
                                                                         GtkSortType                  *order);
static void               thunar_list_model_set_sort_column_id          (GtkTreeSortable              *sortable,
                                                                         gint                          sort_column_id,
                                                                         GtkSortType                   order);
static void               thunar_list_model_set_default_sort_func       (GtkTreeSortable              *sortable,
                                                                         GtkTreeIterCompareFunc        func,
                                                                         gpointer                      data,
                                                                         GDestroyNotify                destroy);
static void               thunar_list_model_set_sort_func               (GtkTreeSortable              *sortable,
                                                                         gint                          sort_column_id,
                                                                         GtkTreeIterCompareFunc        func,
                                                                         gpointer                      data,
                                                                         GDestroyNotify                destroy);
static gboolean           thunar_list_model_has_default_sort_func       (GtkTreeSortable              *sortable);
static gint               thunar_list_model_cmp_func                    (gconstpointer                 a,
                                                                         gconstpointer                 b,
                                                                         gpointer                      user_data);
static void               thunar_list_model_sort                        (ThunarListModel              *store);
static void               thunar_list_model_file_changed                (ThunarFileMonitor            *file_monitor,
                                                                         ThunarFile                   *file,
                                                                         ThunarListModel              *store);
static void               thunar_list_model_folder_destroy              (ThunarFolder                 *folder,
                                                                         ThunarListModel              *store);
static void               thunar_list_model_folder_error                (ThunarFolder                 *folder,
                                                                         const GError                 *error,
                                                                         ThunarListModel              *store);
static void               thunar_list_model_files_added                 (ThunarFolder                 *folder,
                                                                         GList                        *files,
                                                                         ThunarListModel              *store);
static void               thunar_list_model_files_removed               (ThunarFolder                 *folder,
                                                                         GList                        *files,
                                                                         ThunarListModel              *store);
static void               thunar_list_model_insert_files                (ThunarListModel              *store,
                                                                         GList                        *files);
static gint               sort_by_date                                  (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive,
                                                                         gint                          type);
static gint               sort_by_date_created                          (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_date_accessed                         (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_date_modified                         (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_date_deleted                          (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_recency                               (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_location                              (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_group                                 (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_mime_type                             (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_owner                                 (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_permissions                           (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_size                                  (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_size_in_bytes                         (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_size_and_items_count                  (ThunarFile                   *a,
                                                                         ThunarFile                   *b,
                                                                         gboolean                      case_sensitive);
static gint               sort_by_type                                  (const ThunarFile             *a,
                                                                         const ThunarFile             *b,
                                                                         gboolean                      case_sensitive);

static gboolean           thunar_list_model_get_case_sensitive          (ThunarListModel              *store);
static void               thunar_list_model_set_case_sensitive          (ThunarListModel              *store,
                                                                         gboolean                      case_sensitive);
static ThunarDateStyle    thunar_list_model_get_date_style              (ThunarListModel              *store);
static void               thunar_list_model_set_date_style              (ThunarListModel              *store,
                                                                         ThunarDateStyle               date_style);
static const char*        thunar_list_model_get_date_custom_style       (ThunarListModel              *store);
static void               thunar_list_model_set_date_custom_style       (ThunarListModel              *store,
                                                                         const char                   *date_custom_style);
static gint               thunar_list_model_get_num_files               (ThunarListModel              *store);
static gboolean           thunar_list_model_get_folders_first           (ThunarListModel              *store);
static ThunarJob*         thunar_list_model_job_search_directory        (ThunarListModel              *model,
                                                                         const gchar                  *search_query_c,
                                                                         ThunarFile                   *directory);
static void               thunar_list_model_search_folder               (ThunarListModel              *model,
                                                                         ThunarJob                    *job,
                                                                         gchar                        *uri,
                                                                         gchar                       **search_query_c_terms,
                                                                         enum ThunarListModelSearch    search_type,
                                                                         gboolean                      show_hidden);
static void               thunar_list_model_cancel_search_job           (ThunarListModel              *model);
static gchar**            thunar_list_model_split_search_query          (const gchar                  *search_query,
                                                                         GError                      **error);
static gboolean           thunar_list_model_search_terms_match          (gchar                       **terms,
                                                                         gchar                        *str);

static void               thunar_list_model_search_error                (ThunarJob                    *job);
static void               thunar_list_model_search_finished             (ThunarJob                    *job,
                                                                         ThunarListModel              *store);
static gboolean           thunar_list_model_add_search_files            (gpointer user_data);

static gint               thunar_list_model_get_folder_item_count       (ThunarListModel              *store);
static void               thunar_list_model_set_folder_item_count       (ThunarListModel              *store,
                                                                         ThunarFolderItemCount         count_as_dir_size);

static void               thunar_list_model_file_count_callback         (ExoJob                       *job,
                                                                         gpointer                      model);

struct _ThunarListModelClass
{
  GObjectClass __parent__;

  /* signals */
  void (*error) (ThunarListModel *store,
                 const GError    *error);
  void (*search_done) (void);
};

struct _ThunarListModel
{
  GObject __parent__;

  /* the model stamp is only used when debugging is
   * enabled, to make sure we don't accept iterators
   * generated by another model.
   */
#ifndef NDEBUG
  gint           stamp;
#endif

  GSequence               *rows;
  GSList                  *hidden;
  ThunarFolder            *folder;
  gboolean                 show_hidden : 1;
  ThunarFolderItemCount    folder_item_count;
  gboolean                 file_size_binary : 1;
  ThunarDateStyle          date_style;
  char                    *date_custom_style;

  /* Normalized current search terms.
   * NULL if not presenting a search's results.
   * Search job may have finished even if this is non-NULL.
   */
  gchar **search_terms;

  /* Use the shared ThunarFileMonitor instance, so we
   * do not need to connect "changed" handler to every
   * file in the model.
   */
  ThunarFileMonitor *file_monitor;

  /* ids for the "row-inserted" and "row-deleted" signals
   * of GtkTreeModel to speed up folder changing.
   */
  guint          row_inserted_id;
  guint          row_deleted_id;

  gboolean       sort_case_sensitive : 1;
  gboolean       sort_folders_first : 1;
  gint           sort_sign;   /* 1 = ascending, -1 descending */
  ThunarSortFunc sort_func;

  /* searching runs in a separate thread which incrementally inserts results (files)
   * in the files_to_add list.
   * Periodically the main thread takes all the files in the files_to_add list
   * and adds them in the model. The list is then emptied.
   */
  ThunarJob     *recursive_search_job;
  GList         *files_to_add;
  GMutex         mutex_files_to_add;

  /* used to stop the periodic call to thunar_list_model_add_search_files when the search is finished/canceled */
  guint          update_search_results_timeout_id;
};



static guint       list_model_signals[LAST_SIGNAL];
static GParamSpec *list_model_props[N_PROPERTIES] = { NULL, };



G_DEFINE_TYPE_WITH_CODE (ThunarListModel, thunar_list_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_list_model_tree_model_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, thunar_list_model_drag_dest_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE, thunar_list_model_sortable_init))



static void
thunar_list_model_class_init (ThunarListModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class               = G_OBJECT_CLASS (klass);
  gobject_class->dispose      = thunar_list_model_dispose;
  gobject_class->finalize     = thunar_list_model_finalize;
  gobject_class->get_property = thunar_list_model_get_property;
  gobject_class->set_property = thunar_list_model_set_property;

  /**
   * ThunarListModel:case-sensitive:
   *
   * Tells whether the sorting should be case sensitive.
   **/
  list_model_props[PROP_CASE_SENSITIVE] =
      g_param_spec_boolean ("case-sensitive",
                            "case-sensitive",
                            "case-sensitive",
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:date-style:
   *
   * The style used to format dates.
   **/
  list_model_props[PROP_DATE_STYLE] =
      g_param_spec_enum ("date-style",
                         "date-style",
                         "date-style",
                         THUNAR_TYPE_DATE_STYLE,
                         THUNAR_DATE_STYLE_SIMPLE,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:date-custom-style:
   *
   * The style used for custom format of dates.
   **/
  list_model_props[PROP_DATE_CUSTOM_STYLE] =
      g_param_spec_string ("date-custom-style",
                           "DateCustomStyle",
                           NULL,
                           "%Y-%m-%d %H:%M:%S",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:folder:
   *
   * The folder presented by this #ThunarListModel.
   **/
  list_model_props[PROP_FOLDER] =
      g_param_spec_object ("folder",
                           "folder",
                           "folder",
                           THUNAR_TYPE_FOLDER,
                           EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::folders-first:
   *
   * Tells whether to always sort folders before other files.
   **/
  list_model_props[PROP_FOLDERS_FIRST] =
      g_param_spec_boolean ("folders-first",
                            "folders-first",
                            "folders-first",
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::num-files:
   *
   * The number of files in the folder presented by this #ThunarListModel.
   **/
  list_model_props[PROP_NUM_FILES] =
      g_param_spec_uint ("num-files",
                         "num-files",
                         "num-files",
                         0, G_MAXUINT, 0,
                         EXO_PARAM_READABLE);

  /**
   * ThunarListModel::show-hidden:
   *
   * Tells whether to include hidden (and backup) files.
   **/
  list_model_props[PROP_SHOW_HIDDEN] =
      g_param_spec_boolean ("show-hidden",
                            "show-hidden",
                            "show-hidden",
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::misc-file-size-binary:
   *
   * Tells whether to format file size in binary.
   **/
  list_model_props[PROP_FILE_SIZE_BINARY] =
      g_param_spec_boolean ("file-size-binary",
                            "file-size-binary",
                            "file-size-binary",
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:folder-item-count:
   *
   * Tells when the size column of folders should show the number of containing files
   **/
  list_model_props[PROP_FOLDER_ITEM_COUNT] =
      g_param_spec_enum ("folder-item-count",
                         "folder-item-count",
                         "folder-item-count",
                         THUNAR_TYPE_FOLDER_ITEM_COUNT,
                         TRUE,
                         EXO_PARAM_READWRITE);

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, list_model_props);

  /**
   * ThunarListModel::error:
   * @store : a #ThunarListModel.
   * @error : a #GError that describes the problem.
   *
   * Emitted when an error occurs while loading the
   * @store content.
   **/
  list_model_signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarListModelClass, error),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarListModel::search-done:
   * @store : a #ThunarListModel.
   * @error : a #GError that describes the problem.
   *
   * Emitted when a recursive search finishes.
   **/
  list_model_signals[SEARCH_DONE] =
      g_signal_new (I_("search-done"),
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarListModelClass, search_done),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);
}



static void
thunar_list_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags        = thunar_list_model_get_flags;
  iface->get_n_columns    = thunar_list_model_get_n_columns;
  iface->get_column_type  = thunar_list_model_get_column_type;
  iface->get_iter         = thunar_list_model_get_iter;
  iface->get_path         = thunar_list_model_get_path;
  iface->get_value        = thunar_list_model_get_value;
  iface->iter_next        = thunar_list_model_iter_next;
  iface->iter_children    = thunar_list_model_iter_children;
  iface->iter_has_child   = thunar_list_model_iter_has_child;
  iface->iter_n_children  = thunar_list_model_iter_n_children;
  iface->iter_nth_child   = thunar_list_model_iter_nth_child;
  iface->iter_parent      = thunar_list_model_iter_parent;
}



static void
thunar_list_model_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = thunar_list_model_drag_data_received;
  iface->row_drop_possible = thunar_list_model_row_drop_possible;
}



static void
thunar_list_model_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id     = thunar_list_model_get_sort_column_id;
  iface->set_sort_column_id     = thunar_list_model_set_sort_column_id;
  iface->set_sort_func          = thunar_list_model_set_sort_func;
  iface->set_default_sort_func  = thunar_list_model_set_default_sort_func;
  iface->has_default_sort_func  = thunar_list_model_has_default_sort_func;
}



static void
thunar_list_model_init (ThunarListModel *store)
{
  /* generate a unique stamp if we're in debug mode */
#ifndef NDEBUG
  store->stamp = g_random_int ();
#endif

  store->row_inserted_id = g_signal_lookup ("row-inserted", GTK_TYPE_TREE_MODEL);
  store->row_deleted_id = g_signal_lookup ("row-deleted", GTK_TYPE_TREE_MODEL);

  store->search_terms = NULL;

  store->sort_case_sensitive = TRUE;
  store->sort_folders_first = TRUE;
  store->sort_sign = 1;
  store->sort_func = thunar_file_compare_by_name;
  store->rows = g_sequence_new (g_object_unref);
  g_mutex_init (&store->mutex_files_to_add);

  /* connect to the shared ThunarFileMonitor, so we don't need to
   * connect "changed" to every single ThunarFile we own.
   */
  store->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (store->file_monitor), "file-changed",
                    G_CALLBACK (thunar_list_model_file_changed), store);
}



static void
thunar_list_model_dispose (GObject *object)
{
  /* unlink from the folder (if any) */
  thunar_list_model_set_folder (THUNAR_LIST_MODEL (object), NULL, NULL);

  (*G_OBJECT_CLASS (thunar_list_model_parent_class)->dispose) (object);
}



static void
thunar_list_model_finalize (GObject *object)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  thunar_list_model_cancel_search_job (store);

  if (store->update_search_results_timeout_id > 0)
    {
      g_source_remove (store->update_search_results_timeout_id);
      store->update_search_results_timeout_id = 0;
    }
  thunar_g_list_free_full (store->files_to_add);
  store->files_to_add = NULL;

  g_sequence_free (store->rows);
  g_mutex_clear (&store->mutex_files_to_add);

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (G_OBJECT (store->file_monitor), thunar_list_model_file_changed, store);
  g_object_unref (G_OBJECT (store->file_monitor));

  g_free (store->date_custom_style);

  g_strfreev (store->search_terms);

  (*G_OBJECT_CLASS (thunar_list_model_parent_class)->finalize) (object);
}



static void
thunar_list_model_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, thunar_list_model_get_case_sensitive (store));
      break;

    case PROP_DATE_STYLE:
      g_value_set_enum (value, thunar_list_model_get_date_style (store));
      break;

    case PROP_DATE_CUSTOM_STYLE:
      g_value_set_string (value, thunar_list_model_get_date_custom_style (store));
      break;

    case PROP_FOLDER:
      g_value_set_object (value, thunar_list_model_get_folder (store));
      break;

    case PROP_FOLDERS_FIRST:
      g_value_set_boolean (value, thunar_list_model_get_folders_first (store));
      break;

    case PROP_NUM_FILES:
      g_value_set_uint (value, thunar_list_model_get_num_files (store));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_list_model_get_show_hidden (store));
      break;

    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, thunar_list_model_get_file_size_binary (store));
      break;

    case PROP_FOLDER_ITEM_COUNT:
      g_value_set_enum (value, thunar_list_model_get_folder_item_count (store));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_list_model_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      thunar_list_model_set_case_sensitive (store, g_value_get_boolean (value));
      break;

    case PROP_DATE_STYLE:
      thunar_list_model_set_date_style (store, g_value_get_enum (value));
      break;

    case PROP_DATE_CUSTOM_STYLE:
      thunar_list_model_set_date_custom_style (store, g_value_get_string (value));
      break;

    case PROP_FOLDER:
      thunar_list_model_set_folder (store, g_value_get_object (value), NULL);
      break;

    case PROP_FOLDERS_FIRST:
      thunar_list_model_set_folders_first (store, g_value_get_boolean (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_list_model_set_show_hidden (store, g_value_get_boolean (value));
      break;

    case PROP_FILE_SIZE_BINARY:
      thunar_list_model_set_file_size_binary (store, g_value_get_boolean (value));
      break;

    case PROP_FOLDER_ITEM_COUNT:
      thunar_list_model_set_folder_item_count (store, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_list_model_get_flags (GtkTreeModel *model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_list_model_get_n_columns (GtkTreeModel *model)
{
  return THUNAR_N_COLUMNS;
}



static GType
thunar_list_model_get_column_type (GtkTreeModel *model,
                                   gint          idx)
{
  switch (idx)
    {
    case THUNAR_COLUMN_DATE_CREATED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_ACCESSED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_MODIFIED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_DELETED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_RECENCY:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_LOCATION:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_GROUP:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_MIME_TYPE:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_OWNER:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_PERMISSIONS:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_SIZE:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_TYPE:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_COLUMN_FILE_NAME:
      return G_TYPE_STRING;
    }

  _thunar_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_list_model_get_iter (GtkTreeModel *model,
                            GtkTreeIter  *iter,
                            GtkTreePath  *path)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GSequenceIter   *row;
  gint             offset;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the row for the path */
  offset = gtk_tree_path_get_indices (path)[0];
  row = g_sequence_get_iter_at_pos (store->rows, offset);

  if (!g_sequence_iter_is_end (row))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, row);
      return TRUE;
    }

  return FALSE;
}



static GtkTreePath*
thunar_list_model_get_path (GtkTreeModel *model,
                            GtkTreeIter  *iter)
{
  gint idx;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_LIST_MODEL (model)->stamp, NULL);

  idx = g_sequence_iter_get_position (iter->user_data);
  if (G_LIKELY (idx >= 0))
    return gtk_tree_path_new_from_indices (idx, -1);

  return NULL;
}



static void
thunar_list_model_get_value (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gint          column,
                             GValue       *value)
{
  ThunarGroup  *group;
  const gchar  *device_type;
  const gchar  *name;
  const gchar  *real_name;
  ThunarUser   *user;
  ThunarFile   *file;
  ThunarFolder *folder;
  gchar        *str;
  guint32       item_count;
  GFile        *g_file;
  GFile        *g_file_parent;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp);

  file = g_sequence_get (iter->user_data);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  switch (column)
    {
    case THUNAR_COLUMN_DATE_CREATED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_CREATED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_DATE_ACCESSED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_ACCESSED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_MODIFIED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_DATE_DELETED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_DELETED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_RECENCY:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_RECENCY, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_LOCATION:
      g_value_init (value, G_TYPE_STRING);
      g_file_parent = g_file_get_parent (thunar_file_get_file (file));
      str = NULL;

      /* g_file_parent will be NULL only if a search returned the root
       * directory somehow, or "file:///" is in recent:/// somehow.
       * These should be quite rare circumstances. */
      if (G_UNLIKELY (g_file_parent == NULL))
        {
          g_value_take_string (value, NULL);
          break;
        }

      /* Try and show a relative path beginning with the current folder's name to the parent folder.
       * Fall thru with str==NULL if that is not possible. */
      folder = THUNAR_LIST_MODEL (model)->folder;
      if (G_LIKELY (folder != NULL))
        {
          const gchar *folder_basename = thunar_file_get_basename( thunar_folder_get_corresponding_file (folder));
          GFile *g_folder = thunar_file_get_file (thunar_folder_get_corresponding_file (folder));
          if (g_file_equal (g_folder, g_file_parent))
            {
              /* commonest non-prefix case: item location is directly inside the search folder */
              str = g_strdup (folder_basename);
            }
          else
            {
              str = g_file_get_relative_path (g_folder, g_file_parent);
              /* str can still be NULL if g_folder is not a prefix of g_file_parent */
              if (str != NULL)
                {
                  gchar *tmp = g_build_path (G_DIR_SEPARATOR_S, folder_basename, str, NULL);
                  g_free (str);
                  str = tmp;
                }
            }
        }

      /* catchall for when model->folder is not an ancestor of the parent (e.g. when searching recent:///).
       * In this case, show a prettified absolute URI or local path. */
      if (str == NULL)
        str = g_file_get_parse_name (g_file_parent);

      g_object_unref (g_file_parent);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_GROUP:
      g_value_init (value, G_TYPE_STRING);
      group = thunar_file_get_group (file);
      if (G_LIKELY (group != NULL))
        {
          g_value_set_string (value, thunar_group_get_name (group));
          g_object_unref (G_OBJECT (group));
        }
      else
        {
          g_value_set_static_string (value, _("Unknown"));
        }
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_content_type (file));
      break;

    case THUNAR_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    case THUNAR_COLUMN_OWNER:
      g_value_init (value, G_TYPE_STRING);
      user = thunar_file_get_user (file);
      if (G_LIKELY (user != NULL))
        {
          /* determine sane display name for the owner */
          name = thunar_user_get_name (user);
          real_name = thunar_user_get_real_name (user);
          if(G_LIKELY (real_name != NULL))
            {
              if(strcmp (name, real_name) == 0)
                str = g_strdup (name);
              else
                str = g_strdup_printf ("%s (%s)", real_name, name);
            }
          else
            str = g_strdup (name);
          g_value_take_string (value, str);
          g_object_unref (G_OBJECT (user));
        }
      else
        {
          g_value_set_static_string (value, _("Unknown"));
        }
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, thunar_file_get_mode_string (file));
      break;

    case THUNAR_COLUMN_SIZE:
      g_value_init (value, G_TYPE_STRING);

      if (thunar_file_is_mountable (file))
        {
          g_file = thunar_file_get_target_location (file);
          if (g_file == NULL)
            break;
          g_value_take_string (value, thunar_g_file_get_free_space_string (g_file, THUNAR_LIST_MODEL (model)->file_size_binary));
          g_object_unref (g_file);
          break;
        }
      else if (thunar_file_is_directory (file))
        {
          /* If the option is set to never show folder sizes as item counts, then just give the folder's binary size */
          if (THUNAR_LIST_MODEL (model)->folder_item_count == THUNAR_FOLDER_ITEM_COUNT_NEVER)
            g_value_take_string (value, thunar_file_get_size_string_formatted (file, THUNAR_LIST_MODEL (model)->file_size_binary));

          /* If the option is set to always show folder sizes as item counts, then give the folder's item count */
          else if (THUNAR_LIST_MODEL (model)->folder_item_count == THUNAR_FOLDER_ITEM_COUNT_ALWAYS)
            {
              item_count = thunar_file_get_file_count (file, G_CALLBACK (thunar_list_model_file_count_callback), model);
              g_value_take_string (value, g_strdup_printf (ngettext ("%u item", "%u items", item_count), item_count));
            }

          /* If the option is set to always show folder sizes as item counts only for local files,
           * check if the files is local or not, and act accordingly */
          else if (THUNAR_LIST_MODEL (model)->folder_item_count == THUNAR_FOLDER_ITEM_COUNT_ONLY_LOCAL)
            {
              if (thunar_file_is_local (file))
                {
                  item_count = thunar_file_get_file_count (file, G_CALLBACK (thunar_list_model_file_count_callback), model);
                  g_value_take_string (value, g_strdup_printf (ngettext ("%u item", "%u items", item_count), item_count));
                }
              else
                g_value_take_string (value, thunar_file_get_size_string_formatted (file, THUNAR_LIST_MODEL (model)->file_size_binary));
            }
          else
              g_warning ("Error, unknown enum value for folder_item_count in the list model");
        }
      else
        {
          g_value_take_string (value, thunar_file_get_size_string_formatted (file, THUNAR_LIST_MODEL (model)->file_size_binary));
        }
      break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, thunar_file_get_size_in_bytes_string (file));
      break;

    case THUNAR_COLUMN_TYPE:
      g_value_init (value, G_TYPE_STRING);
      device_type = thunar_file_get_device_type (file);
      if (device_type != NULL)
        {
          g_value_take_string (value, g_strdup (device_type));
          break;
        }
      g_value_take_string (value, thunar_file_get_content_type_desc (file));
      break;

    case THUNAR_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, file);
      break;

    case THUNAR_COLUMN_FILE_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    default:
      _thunar_assert_not_reached ();
      break;
    }
}



static gboolean
thunar_list_model_iter_next (GtkTreeModel *model,
                             GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp, FALSE);

  iter->user_data = g_sequence_iter_next (iter->user_data);
  return !g_sequence_iter_is_end (iter->user_data);
}



static gboolean
thunar_list_model_iter_children (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_LIKELY (parent == NULL
      && g_sequence_get_length (store->rows) > 0))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, g_sequence_get_begin_iter (store->rows));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_iter_has_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_list_model_iter_n_children (GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);

  return (iter == NULL) ? g_sequence_get_length (store->rows) : 0;
}



static gboolean
thunar_list_model_iter_nth_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GSequenceIter   *row;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_LIKELY (parent == NULL))
    {
      row = g_sequence_get_iter_at_pos (store->rows, n);
      if (g_sequence_iter_is_end (row))
        return FALSE;

      GTK_TREE_ITER_INIT (*iter, store->stamp, row);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_iter_parent (GtkTreeModel *model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
thunar_list_model_drag_data_received (GtkTreeDragDest  *dest,
                                      GtkTreePath      *path,
                                      GtkSelectionData *data)
{
  return FALSE;
}



static gboolean
thunar_list_model_row_drop_possible (GtkTreeDragDest  *dest,
                                     GtkTreePath      *path,
                                     GtkSelectionData *data)
{
  return FALSE;
}



static gboolean
thunar_list_model_get_sort_column_id (GtkTreeSortable *sortable,
                                      gint            *sort_column_id,
                                      GtkSortType     *order)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (sortable);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (store->sort_func == sort_by_mime_type)
    *sort_column_id = THUNAR_COLUMN_MIME_TYPE;
  else if (store->sort_func == thunar_file_compare_by_name)
    *sort_column_id = THUNAR_COLUMN_NAME;
  else if (store->sort_func == sort_by_permissions)
    *sort_column_id = THUNAR_COLUMN_PERMISSIONS;
  else if (store->sort_func == sort_by_size || store->sort_func == (ThunarSortFunc) sort_by_size_and_items_count)
    *sort_column_id = THUNAR_COLUMN_SIZE;
  else if (store->sort_func == sort_by_size_in_bytes)
    *sort_column_id = THUNAR_COLUMN_SIZE_IN_BYTES;
  else if (store->sort_func == sort_by_date_created)
    *sort_column_id = THUNAR_COLUMN_DATE_CREATED;
  else if (store->sort_func == sort_by_date_accessed)
    *sort_column_id = THUNAR_COLUMN_DATE_ACCESSED;
  else if (store->sort_func == sort_by_date_modified)
    *sort_column_id = THUNAR_COLUMN_DATE_MODIFIED;
  else if (store->sort_func == sort_by_date_deleted)
    *sort_column_id = THUNAR_COLUMN_DATE_DELETED;
  else if (store->sort_func == sort_by_recency)
    *sort_column_id = THUNAR_COLUMN_RECENCY;
  else if (store->sort_func == sort_by_location)
    *sort_column_id = THUNAR_COLUMN_LOCATION;
  else if (store->sort_func == sort_by_type)
    *sort_column_id = THUNAR_COLUMN_TYPE;
  else if (store->sort_func == sort_by_owner)
    *sort_column_id = THUNAR_COLUMN_OWNER;
  else if (store->sort_func == sort_by_group)
    *sort_column_id = THUNAR_COLUMN_GROUP;
  else
    _thunar_assert_not_reached ();

  if (order != NULL)
    {
      if (store->sort_sign > 0)
        *order = GTK_SORT_ASCENDING;
      else
        *order = GTK_SORT_DESCENDING;
    }

  return TRUE;
}



static void
thunar_list_model_set_sort_column_id (GtkTreeSortable *sortable,
                                      gint             sort_column_id,
                                      GtkSortType      order)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (sortable);

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  switch (sort_column_id)
    {
    case THUNAR_COLUMN_DATE_CREATED:
      store->sort_func = sort_by_date_created;
      break;

    case THUNAR_COLUMN_DATE_ACCESSED:
      store->sort_func = sort_by_date_accessed;
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      store->sort_func = sort_by_date_modified;
      break;

    case THUNAR_COLUMN_DATE_DELETED:
      store->sort_func = sort_by_date_deleted;
      break;

    case THUNAR_COLUMN_RECENCY:
      store->sort_func = sort_by_recency;
      break;

    case THUNAR_COLUMN_LOCATION:
      store->sort_func = sort_by_location;
      break;

    case THUNAR_COLUMN_GROUP:
      store->sort_func = sort_by_group;
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      store->sort_func = sort_by_mime_type;
      break;

    case THUNAR_COLUMN_FILE_NAME:
    case THUNAR_COLUMN_NAME:
      store->sort_func = thunar_file_compare_by_name;
      break;

    case THUNAR_COLUMN_OWNER:
      store->sort_func = sort_by_owner;
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      store->sort_func = sort_by_permissions;
      break;

    case THUNAR_COLUMN_SIZE:
      store->sort_func = (store->folder_item_count != THUNAR_FOLDER_ITEM_COUNT_NEVER) ? (ThunarSortFunc) sort_by_size_and_items_count : sort_by_size;
      break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      store->sort_func = sort_by_size_in_bytes;
      break;

    case THUNAR_COLUMN_TYPE:
      store->sort_func = sort_by_type;
      break;

    default:
      _thunar_assert_not_reached ();
    }

  /* new sort sign */
  store->sort_sign = (order == GTK_SORT_ASCENDING) ? 1 : -1;

  /* re-sort the store */
  thunar_list_model_sort (store);

  /* notify listining parties */
  gtk_tree_sortable_sort_column_changed (sortable);
}



static void
thunar_list_model_set_default_sort_func (GtkTreeSortable       *sortable,
                                         GtkTreeIterCompareFunc func,
                                         gpointer               data,
                                         GDestroyNotify         destroy)
{
  g_critical ("ThunarListModel has sorting facilities built-in!");
}



static void
thunar_list_model_set_sort_func (GtkTreeSortable       *sortable,
                                 gint                   sort_column_id,
                                 GtkTreeIterCompareFunc func,
                                 gpointer               data,
                                 GDestroyNotify         destroy)
{
  g_critical ("ThunarListModel has sorting facilities built-in!");
}



static gboolean
thunar_list_model_has_default_sort_func (GtkTreeSortable *sortable)
{
  return FALSE;
}



static gint
thunar_list_model_cmp_func (gconstpointer a,
                            gconstpointer b,
                            gpointer      user_data)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (user_data);
  gboolean         isdir_a;
  gboolean         isdir_b;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (a), 0);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (b), 0);

  if (G_LIKELY (store->sort_folders_first))
    {
      isdir_a = thunar_file_is_directory (a);
      isdir_b = thunar_file_is_directory (b);
      if (isdir_a != isdir_b)
        return isdir_a ? -1 : 1;
    }

  return (*store->sort_func) (a, b, store->sort_case_sensitive) * store->sort_sign;
}



static void
thunar_list_model_sort (ThunarListModel *store)
{
  GtkTreePath    *path;
  GSequenceIter **old_order;
  gint           *new_order;
  gint            n;
  gint            length;
  GSequenceIter  *row;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  length = g_sequence_get_length (store->rows);
  if (G_UNLIKELY (length <= 1))
    return;

  /* be sure to not overuse the stack */
  if (G_LIKELY (length < 2000))
    {
      old_order = g_newa (GSequenceIter *, length);
      new_order = g_newa (gint, length);
    }
  else
    {
      old_order = g_new (GSequenceIter *, length);
      new_order = g_new (gint, length);
    }

  /* store old order */
  row = g_sequence_get_begin_iter (store->rows);
  for (n = 0; n < length; ++n)
    {
      old_order[n] = row;
      row = g_sequence_iter_next (row);
    }

  /* sort */
  g_sequence_sort (store->rows, thunar_list_model_cmp_func, store);

  /* new_order[newpos] = oldpos */
  for (n = 0; n < length; ++n)
    new_order[g_sequence_iter_get_position (old_order[n])] = n;

  /* tell the view about the new item order */
  path = gtk_tree_path_new_first ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, NULL, new_order);
  gtk_tree_path_free (path);

  /* clean up if we used the heap */
  if (G_UNLIKELY (length >= 2000))
    {
      g_free (old_order);
      g_free (new_order);
    }
}



static void
thunar_list_model_file_changed (ThunarFileMonitor *file_monitor,
                                ThunarFile        *file,
                                ThunarListModel   *store)
{
  GSequenceIter *row;
  GSequenceIter *end;
  gint           pos_after;
  gint           pos_before = 0;
  gint          *new_order;
  gint           length;
  gint           i, j;
  GtkTreePath   *path;
  GtkTreeIter    iter;

  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor) || file_monitor == NULL);
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  row = g_sequence_get_begin_iter (store->rows);
  end = g_sequence_get_end_iter (store->rows);

  while (row != end)
    {
      if (G_UNLIKELY (g_sequence_get (row) == file))
        {
          /* generate the iterator for this row */
          GTK_TREE_ITER_INIT (iter, store->stamp, row);

          _thunar_assert (pos_before == g_sequence_iter_get_position (row));

          /* check if the sorting changed */
          g_sequence_sort_changed (row, thunar_list_model_cmp_func, store);
          pos_after = g_sequence_iter_get_position (row);
          if (pos_after != pos_before)
            {
              /* do swap sorting here since its much faster than a complete sort */
              length = g_sequence_get_length (store->rows);
              if (G_LIKELY (length < 2000))
                new_order = g_newa (gint, length);
              else
                new_order = g_new (gint, length);

              /* new_order[newpos] = oldpos */
              for (i = 0, j = 0; i < length; ++i)
                {
                  if (G_UNLIKELY (i == pos_after))
                    {
                      new_order[i] = pos_before;
                    }
                  else
                    {
                      if (G_UNLIKELY (j == pos_before))
                        j++;
                      new_order[i] = j++;
                    }
                }

              /* tell the view about the new item order */
              path = gtk_tree_path_new_first ();
              gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, NULL, new_order);
              gtk_tree_path_free (path);

              /* clean up if we used the heap */
              if (G_UNLIKELY (length >= 2000))
                g_free (new_order);
            }

          /* notify the view that it has to redraw the file */
          path = gtk_tree_path_new_from_indices (pos_before, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);
          break;
        }

      row = g_sequence_iter_next (row);
      pos_before++;
    }
}



static void
thunar_list_model_folder_destroy (ThunarFolder    *folder,
                                  ThunarListModel *store)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  thunar_list_model_set_folder (store, NULL, NULL);

  /* TODO: What to do when the folder is deleted? */
}



static void
thunar_list_model_folder_error (ThunarFolder    *folder,
                                const GError    *error,
                                ThunarListModel *store)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (error != NULL);

  /* reset the current folder */
  thunar_list_model_set_folder (store, NULL, NULL);

  /* forward the error signal */
  g_signal_emit (G_OBJECT (store), list_model_signals[ERROR], 0, error);
}



static void
thunar_list_model_files_added (ThunarFolder    *folder,
                               GList           *files,
                               ThunarListModel *store)
{
  GList       *filtered;
  GList       *lp;
  ThunarFile  *file;
  gboolean     matched;
  gchar       *name_n;

  /* pass the list directly if not currently showing search results */
  if (store->search_terms == NULL)
    {
      thunar_list_model_insert_files (store, files);
      return;
    }

  /* otherwise, filter out files that don't match the current search terms */
  filtered = NULL;
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* take a reference on that file */
      file = THUNAR_FILE (g_object_ref (G_OBJECT (lp->data)));
      _thunar_return_if_fail (THUNAR_IS_FILE (file));

      name_n = (gchar *)thunar_file_get_display_name (file);
      name_n = thunar_g_utf8_normalize_for_search (name_n, TRUE, TRUE);
      matched = thunar_list_model_search_terms_match (store->search_terms, name_n);
      g_free (name_n);

      if (! matched)
        g_object_unref (file);
      else
        filtered = g_list_append (filtered, file);
    }
  thunar_list_model_insert_files (store, filtered);
  thunar_g_list_free_full (filtered);
}


static void
thunar_list_model_insert_files (ThunarListModel *store,
                                GList           *files)
{
  GtkTreePath   *path;
  GtkTreeIter    iter;
  ThunarFile    *file;
  gint          *indices;
  GSequenceIter *row;
  GList         *lp;
  gboolean       has_handler;
  gboolean       search_mode;

  /* we use a simple trick here to avoid allocating
   * GtkTreePath's again and again, by simply accessing
   * the indices directly and only modifying the first
   * item in the integer array... looks a hack, eh?
   */
  path = gtk_tree_path_new_first ();
  indices = gtk_tree_path_get_indices (path);

  /* check if we have any handlers connected for "row-inserted" */
  has_handler = g_signal_has_handler_pending (G_OBJECT (store), store->row_inserted_id, 0, FALSE);

  /* process all added files */
  search_mode = (store->search_terms != NULL);
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* take a reference on that file */
      file = THUNAR_FILE (g_object_ref (G_OBJECT (lp->data)));
      _thunar_return_if_fail (THUNAR_IS_FILE (file));

      /* check if the file should be stashed in the hidden list */
      /* The ->hidden list is an optimization used by the model when
       * it is not being used to store search results. In the search
       * case, we simply restart the search, */
      if (!store->show_hidden && thunar_file_is_hidden (file))
        {
          if (search_mode == FALSE)
            store->hidden = g_slist_prepend (store->hidden, file);
          else
            g_object_unref (file);
        }
      else
        {
          /* insert the file */
          row = g_sequence_insert_sorted (store->rows, file,
                                          thunar_list_model_cmp_func, store);

          if (has_handler)
            {
              /* generate an iterator for the new item */
              GTK_TREE_ITER_INIT (iter, store->stamp, row);

              indices[0] = g_sequence_iter_get_position (row);
              gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
            }
        }
    }

  /* release the path */
  gtk_tree_path_free (path);

  /* number of visible files may have changed */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
}



static void
thunar_list_model_files_removed (ThunarFolder    *folder,
                                 GList           *files,
                                 ThunarListModel *store)
{
  GList         *lp;
  GSequenceIter *row;
  GSequenceIter *end;
  GSequenceIter *next;
  GtkTreePath   *path;
  gboolean       found;
  gboolean       search_mode;

  /* drop all the referenced files from the model */
  search_mode = (store->search_terms != NULL);
  for (lp = files; lp != NULL; lp = lp->next)
    {
      row = g_sequence_get_begin_iter (store->rows);
      end = g_sequence_get_end_iter (store->rows);

      found = FALSE;

      while (row != end)
        {
          next = g_sequence_iter_next (row);

          if (g_sequence_get (row) == lp->data)
            {
              /* setup path for "row-deleted" */
              path = gtk_tree_path_new_from_indices (g_sequence_iter_get_position (row), -1);

              /* remove file from the model */
              g_sequence_remove (row);

              /* notify the view(s) */
              gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
              gtk_tree_path_free (path);

              /* no need to look in the hidden files */
              found = TRUE;

              break;
            }

          row = next;
        }

      /* check if the file was found */
      if (!found)
        {
          if (search_mode == FALSE)
            {
              /* file is hidden */
              /* this only makes sense when not storing search results */
              _thunar_assert (g_slist_find (store->hidden, lp->data) != NULL);
              store->hidden = g_slist_remove (store->hidden, lp->data);
              g_object_unref (G_OBJECT (lp->data));
            }
        }
    }

  /* this probably changed */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
}



static gint
sort_by_date         (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive,
                      gint              type)
{
  guint64 date_a;
  guint64 date_b;

  date_a = thunar_file_get_date (a, type);
  date_b = thunar_file_get_date (b, type);

  if (date_a < date_b)
    return -1;
  else if (date_a > date_b)
    return 1;

  return thunar_file_compare_by_name (a, b, case_sensitive);
}



static gint
sort_by_date_created (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_CREATED);
}



static gint
sort_by_date_accessed (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_ACCESSED);
}



static gint
sort_by_date_modified (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_MODIFIED);
}



static gint
sort_by_date_deleted (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_DELETED);
}



static gint
sort_by_recency      (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_RECENCY);
}



static gint
sort_by_location (const ThunarFile *a,
                  const ThunarFile *b,
                  gboolean          case_sensitive)
{
  gchar *uri_a;
  gchar *uri_b;
  gchar *location_a;
  gchar *location_b;
  gint   result;

  uri_a = thunar_file_dup_uri (a);
  uri_b = thunar_file_dup_uri (b);

  location_a = g_path_get_dirname (uri_a);
  location_b = g_path_get_dirname (uri_b);

  result = strcasecmp (location_a, location_b);

  g_free (uri_a);
  g_free (uri_b);
  g_free (location_a);
  g_free (location_b);

  return result;
}



static gint
sort_by_group (const ThunarFile *a,
               const ThunarFile *b,
               gboolean          case_sensitive)
{
  ThunarGroup *group_a;
  ThunarGroup *group_b;
  guint32      gid_a;
  guint32      gid_b;
  gint         result;
  const gchar *name_a;
  const gchar *name_b;

  if (thunar_file_get_info (a) == NULL || thunar_file_get_info (b) == NULL)
    return thunar_file_compare_by_name (a, b, case_sensitive);

  group_a = thunar_file_get_group (a);
  group_b = thunar_file_get_group (b);

  if (group_a != NULL && group_b != NULL)
    {
      name_a = thunar_group_get_name (group_a);
      name_b = thunar_group_get_name (group_b);

      if (!case_sensitive)
        result = strcasecmp (name_a, name_b);
      else
        result = strcmp (name_a, name_b);
    }
  else
    {
      gid_a = g_file_info_get_attribute_uint32 (thunar_file_get_info (a),
                                                G_FILE_ATTRIBUTE_UNIX_GID);
      gid_b = g_file_info_get_attribute_uint32 (thunar_file_get_info (b),
                                                G_FILE_ATTRIBUTE_UNIX_GID);

      result = CLAMP ((gint) gid_a - (gint) gid_b, -1, 1);
    }

  if (group_a != NULL)
    g_object_unref (group_a);

  if (group_b != NULL)
    g_object_unref (group_b);

  if (result == 0)
    return thunar_file_compare_by_name (a, b, case_sensitive);
  else
    return result;
}



static gint
sort_by_mime_type (const ThunarFile *a,
                   const ThunarFile *b,
                   gboolean          case_sensitive)
{
  const gchar *content_type_a;
  const gchar *content_type_b;
  gint         result;

  content_type_a = thunar_file_get_content_type (THUNAR_FILE (a));
  content_type_b = thunar_file_get_content_type (THUNAR_FILE (b));

  if (content_type_a == NULL)
    content_type_a = "";
  if (content_type_b == NULL)
    content_type_b = "";

  result = strcasecmp (content_type_a, content_type_b);

  if (result == 0)
    result = thunar_file_compare_by_name (a, b, case_sensitive);

  return result;
}



static gint
sort_by_owner (const ThunarFile *a,
               const ThunarFile *b,
               gboolean          case_sensitive)
{
  const gchar *name_a;
  const gchar *name_b;
  ThunarUser  *user_a;
  ThunarUser  *user_b;
  guint32      uid_a;
  guint32      uid_b;
  gint         result;

  if (thunar_file_get_info (a) == NULL || thunar_file_get_info (b) == NULL)
    return thunar_file_compare_by_name (a, b, case_sensitive);

  user_a = thunar_file_get_user (a);
  user_b = thunar_file_get_user (b);

  if (user_a != NULL && user_b != NULL)
    {
      /* compare the system names */
      name_a = thunar_user_get_name (user_a);
      name_b = thunar_user_get_name (user_b);

      if (!case_sensitive)
        result = strcasecmp (name_a, name_b);
      else
        result = strcmp (name_a, name_b);
    }
  else
    {
      uid_a = g_file_info_get_attribute_uint32 (thunar_file_get_info (a),
                                                G_FILE_ATTRIBUTE_UNIX_UID);
      uid_b = g_file_info_get_attribute_uint32 (thunar_file_get_info (b),
                                                G_FILE_ATTRIBUTE_UNIX_UID);

      result = CLAMP ((gint) uid_a - (gint) uid_b, -1, 1);
    }

  if (result == 0)
    return thunar_file_compare_by_name (a, b, case_sensitive);
  else
    return result;
}



static gint
sort_by_permissions (const ThunarFile *a,
                     const ThunarFile *b,
                     gboolean          case_sensitive)
{
  ThunarFileMode mode_a;
  ThunarFileMode mode_b;

  mode_a = thunar_file_get_mode (a);
  mode_b = thunar_file_get_mode (b);

  if (mode_a < mode_b)
    return -1;
  else if (mode_a > mode_b)
    return 1;

  return thunar_file_compare_by_name (a, b, case_sensitive);
}



static gint
sort_by_size (const ThunarFile *a,
              const ThunarFile *b,
              gboolean          case_sensitive)
{
  guint64 size_a;
  guint64 size_b;

  size_a = thunar_file_get_size (a);
  size_b = thunar_file_get_size (b);

  if (size_a < size_b)
    return -1;
  else if (size_a > size_b)
    return 1;

  return thunar_file_compare_by_name (a, b, case_sensitive);
}



static gint
sort_by_size_in_bytes (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  return sort_by_size (a, b, case_sensitive);
}



static gint
sort_by_size_and_items_count (ThunarFile *a,
                              ThunarFile *b,
                              gboolean    case_sensitive)
{
  guint32       count_a;
  guint32       count_b;

  if (thunar_file_is_directory (a) && thunar_file_is_directory (b))
  {
    count_a = thunar_file_get_file_count (a, NULL, NULL);
    count_b = thunar_file_get_file_count (b, NULL, NULL);

    if (count_a < count_b)
      return -1;
    else if (count_a > count_b)
      return 1;
    else
      return thunar_file_compare_by_name (a, b, case_sensitive);
  }

  return sort_by_size(a, b, case_sensitive);
}



static gint
sort_by_type (const ThunarFile *a,
              const ThunarFile *b,
              gboolean          case_sensitive)
{
  gchar       *description_a = NULL;
  gchar       *description_b = NULL;
  gint         result;

  /* we alter the description of symlinks here because they are
   * displayed as "... (link)" in the detailed list view as well */

  /* fetch the content type description for @file(s) a & b */
  description_a = thunar_file_get_content_type_desc (THUNAR_FILE (a));
  description_b = thunar_file_get_content_type_desc (THUNAR_FILE (b));

  /* avoid calling strcasecmp with NULL parameters */
  if (description_a == NULL || description_b == NULL)
    {
      g_free (description_a);
      g_free (description_b);

      return 0;
    }

  if (!case_sensitive)
    result = strcasecmp (description_a, description_b);
  else
    result = strcmp (description_a, description_b);

  g_free (description_a);
  g_free (description_b);

  if (result == 0)
    return thunar_file_compare_by_name (a, b, case_sensitive);
  else
    return result;
}



/**
 * thunar_list_model_new:
 *
 * Allocates a new #ThunarListModel not associated with
 * any #ThunarFolder.
 *
 * Return value: the newly allocated #ThunarListModel.
 **/
ThunarListModel*
thunar_list_model_new (void)
{
  return g_object_new (THUNAR_TYPE_LIST_MODEL, NULL);
}



/**
 * thunar_list_model_get_case_sensitive:
 * @store : a valid #ThunarListModel object.
 *
 * Returns %TRUE if the sorting is done in a case-sensitive
 * manner for @store.
 *
 * Return value: %TRUE if sorting is case-sensitive.
 **/
static gboolean
thunar_list_model_get_case_sensitive (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->sort_case_sensitive;
}



/**
 * thunar_list_model_set_case_sensitive:
 * @store          : a valid #ThunarListModel object.
 * @case_sensitive : %TRUE to use case-sensitive sort for @store.
 *
 * If @case_sensitive is %TRUE the sorting in @store will be done
 * in a case-sensitive manner.
 **/
static void
thunar_list_model_set_case_sensitive (ThunarListModel *store,
                                      gboolean         case_sensitive)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* normalize the setting */
  case_sensitive = !!case_sensitive;

  /* check if we have a new setting */
  if (store->sort_case_sensitive != case_sensitive)
    {
      /* apply the new setting */
      store->sort_case_sensitive = case_sensitive;

      /* resort the model with the new setting */
      thunar_list_model_sort (store);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_CASE_SENSITIVE]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new case-sensitive setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



/**
 * thunar_list_model_get_date_style:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the #ThunarDateStyle used to format dates in
 *               the given @store.
 **/
static ThunarDateStyle
thunar_list_model_get_date_style (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), THUNAR_DATE_STYLE_SIMPLE);
  return store->date_style;
}



/**
 * thunar_list_model_set_date_style:
 * @store      : a valid #ThunarListModel object.
 * @date_style : the #ThunarDateStyle that should be used to format
 *               dates in the @store.
 *
 * Chances the style used to format dates in @store to the specified
 * @date_style.
 **/
static void
thunar_list_model_set_date_style (ThunarListModel *store,
                                  ThunarDateStyle  date_style)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if we have a new setting */
  if (store->date_style != date_style)
    {
      /* apply the new setting */
      store->date_style = date_style;

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_DATE_STYLE]);

      /* emit a "changed" signal for each row, so the display is reloaded with the new date style */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



/**
 * thunar_list_model_get_date_custom_style:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the style used to format customdates in the given @store.
 **/
static const char*
thunar_list_model_get_date_custom_style (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  return store->date_custom_style;
}



/**
 * thunar_list_model_set_date_custom_style:
 * @store             : a valid #ThunarListModel object.
 * @date_custom_style : the style that should be used to format
 *                      custom dates in the @store.
 *
 * Changes the style used to format custom dates in @store to the specified @date_custom_style.
 **/
static void
thunar_list_model_set_date_custom_style (ThunarListModel *store,
                                         const char      *date_custom_style)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if we have a new setting */
  if (g_strcmp0 (store->date_custom_style, date_custom_style) != 0)
    {
      /* apply the new setting */
      g_free (store->date_custom_style);
      store->date_custom_style = g_strdup (date_custom_style);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_DATE_CUSTOM_STYLE]);

      /* emit a "changed" signal for each row, so the display is reloaded with the new date style */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



ThunarJob*
thunar_list_model_get_job (ThunarListModel  *store)
{
  return store->recursive_search_job;
}



void
thunar_list_model_set_job (ThunarListModel  *store,
                           ThunarJob        *job)
{
  store->recursive_search_job = job;
}



static gboolean
thunar_list_model_add_search_files (gpointer user_data)
{
  ThunarListModel *model = THUNAR_LIST_MODEL (user_data);

  g_mutex_lock (&model->mutex_files_to_add);

  thunar_list_model_insert_files (model, model->files_to_add);
  g_list_free (model->files_to_add);
  model->files_to_add = NULL;

  g_mutex_unlock (&model->mutex_files_to_add);

  return TRUE;
}


/**
 * thunar_list_model_split_search_query:
 * @search_query: The search query to split.
 * @error: Return location for regex compilation errors.
 *
 * Search terms are split on whitespace. Search queries must be
 * normalized before passing to this function.
 *
 * See also: thunar_g_utf8_normalize_for_search().
 *
 * Return value: a list of search terms which must be freed with g_strfreev()
 **/

static gchar **
thunar_list_model_split_search_query (const gchar  *search_query,
                                      GError      **error)
{
  GRegex *whitespace_regex;
  gchar **search_terms;

  whitespace_regex = g_regex_new ("\\s+", 0, 0, error);
  if (whitespace_regex == NULL)
    return NULL;
  search_terms = g_regex_split (whitespace_regex, search_query, 0);
  g_regex_unref (whitespace_regex);
  return search_terms;
}



/**
 * thunar_list_model_search_terms_match:
 * @terms: The search terms to look for, prepared with thunar_list_model_split_search_query().
 * @str: The string which the search terms might be found in.
 *
 * All search terms must match. Thunar uses simple substring matching
 * for the broadest multilingual support. @str must be normalized before
 * passing to this function.
 *
 * See also: thunar_g_utf8_normalize_for_search().
 *
 * Return value: TRUE if all terms matched, FALSE otherwise.
 **/

static gboolean
thunar_list_model_search_terms_match (gchar **terms,
                                      gchar  *str)
{
  for (gint i = 0; terms[i] != NULL; i++)
    if (g_strrstr (str, terms[i]) == NULL)
      return FALSE;
  return TRUE;
}



static gboolean
_thunar_job_search_directory (ThunarJob  *job,
                               GArray     *param_values,
                               GError    **error)
{
  ThunarListModel            *model;
  ThunarFile                 *directory;
  const char                 *search_query_c;
  gchar                     **search_query_c_terms;
  ThunarPreferences          *preferences;
  gboolean                    is_source_device_local;
  ThunarRecursiveSearchMode   mode;
  enum ThunarListModelSearch  search_type;
  gboolean                    show_hidden;

  search_type = THUNAR_LIST_MODEL_SEARCH_NON_RECURSIVE;

  /* grab a reference on the preferences */
  preferences = thunar_preferences_get ();

  /* determine the current recursive search mode */
  g_object_get (G_OBJECT (preferences), "misc-recursive-search", &mode, NULL);
  g_object_get (G_OBJECT (preferences), "last-show-hidden", &show_hidden, NULL);

  g_object_unref (preferences);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  model = g_value_get_object (&g_array_index (param_values, GValue, 0));
  search_query_c = g_value_get_string (&g_array_index (param_values, GValue, 1));
  directory = g_value_get_object (&g_array_index (param_values, GValue, 2));

  search_query_c_terms = thunar_list_model_split_search_query (search_query_c, error);
  if (search_query_c_terms == NULL)
    return FALSE;

  is_source_device_local = thunar_g_file_is_on_local_device (thunar_file_get_file (directory));
  if (mode == THUNAR_RECURSIVE_SEARCH_ALWAYS || (mode == THUNAR_RECURSIVE_SEARCH_LOCAL && is_source_device_local))
    search_type = THUNAR_LIST_MODEL_SEARCH_RECURSIVE;

  thunar_list_model_search_folder (model, job, thunar_file_dup_uri (directory), search_query_c_terms, search_type, show_hidden);

  g_strfreev (search_query_c_terms);

  return TRUE;
}



static ThunarJob*
thunar_list_model_job_search_directory (ThunarListModel *model,
                                        const gchar     *search_query_c,
                                        ThunarFile      *directory)
{
  return thunar_simple_job_new (_thunar_job_search_directory, 3,
                                THUNAR_TYPE_LIST_MODEL, model,
                                G_TYPE_STRING,          search_query_c,
                                THUNAR_TYPE_FILE,       directory);
}



static void
thunar_list_model_cancel_search_job (ThunarListModel *model)
{
  /* cancel the ongoing search if there is one */
  if (model->recursive_search_job)
    {
      exo_job_cancel (EXO_JOB (model->recursive_search_job));

      g_signal_handlers_disconnect_matched (model->recursive_search_job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
      g_object_unref (model->recursive_search_job);
      model->recursive_search_job = NULL;
    }
}



static void
thunar_list_model_search_error (ThunarJob *job)
{
  g_error ("Error while searching recursively");
}



static void
thunar_list_model_search_finished (ThunarJob       *job,
                                   ThunarListModel *store)
{
  if (store->recursive_search_job)
    {
      g_signal_handlers_disconnect_matched (store->recursive_search_job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (store->recursive_search_job);
      store->recursive_search_job = NULL;
    }

  if (store->update_search_results_timeout_id > 0)
    {
      thunar_list_model_add_search_files (store);
      g_source_remove (store->update_search_results_timeout_id);
      store->update_search_results_timeout_id = 0;
    }

  thunar_g_list_free_full (store->files_to_add);
  store->files_to_add = NULL;

  g_signal_emit_by_name (store, "search-done");
}



static void
thunar_list_model_search_folder (ThunarListModel           *model,
                                 ThunarJob                 *job,
                                 gchar                     *uri,
                                 gchar                    **search_query_c_terms,
                                 enum ThunarListModelSearch search_type,
                                 gboolean                   show_hidden)
{
  GCancellable    *cancellable;
  GFileEnumerator *enumerator;
  GFile           *directory;
  GList           *files_found = NULL; /* contains the matching files in this folder only */
  const gchar     *namespace;
  const gchar     *display_name;
  gchar           *display_name_c; /* converted to ignore case */

  cancellable = exo_job_get_cancellable (EXO_JOB (job));
  directory = g_file_new_for_uri (uri);
  g_free (uri);
  namespace = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
              G_FILE_ATTRIBUTE_STANDARD_TARGET_URI ","
              G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
              G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","
              G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
              G_FILE_ATTRIBUTE_STANDARD_NAME ", recent::*";

  /* The directory enumerator MUST NOT follow symlinks itself, meaning that any symlinks that
   * g_file_enumerator_next_file() emits are the actual symlink entries. This prevents one
   * possible source of infinitely deep recursion.
   *
   * There is otherwise no special handling of entries in the folder which are symlinks,
   * which allows them to appear in the search results. */
  enumerator = g_file_enumerate_children (directory, namespace, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, NULL);
  if (enumerator == NULL)
    return;

  /* go through every file in the folder and check if it matches */
  while (exo_job_is_cancelled (EXO_JOB (job)) == FALSE)
    {
      GFile     *file;
      GFileInfo *info;
      GFileType  type;

      /* get GFile and GFileInfo */
      info = g_file_enumerator_next_file (enumerator, cancellable, NULL);
      if (G_UNLIKELY (info == NULL))
        break;

      if (g_file_has_uri_scheme (directory, "recent"))
        {
          file = g_file_new_for_uri (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI));
          g_object_unref (info);
          info = g_file_query_info (file, namespace, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, NULL);
          if (G_UNLIKELY (info == NULL))
            break;
        }
      else
        file = g_file_get_child (directory, g_file_info_get_name (info));

      /* respect last-show-hidden */
      if (show_hidden == FALSE)
        {
          /* same logic as thunar_file_is_hidden() */
          if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN)
              || g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP))
            {
              g_object_unref (file);
              g_object_unref (info);
              continue;
            }
        }

      type = g_file_info_get_file_type (info);

      /* handle directories */
      if (type == G_FILE_TYPE_DIRECTORY && search_type == THUNAR_LIST_MODEL_SEARCH_RECURSIVE)
        {
          thunar_list_model_search_folder (model, job, g_file_get_uri (file), search_query_c_terms, search_type, show_hidden);
        }

      /* prepare entry display name */
      display_name = g_file_info_get_display_name (info);
      display_name_c = thunar_g_utf8_normalize_for_search (display_name, TRUE, TRUE);

      /* search for all substrings */
      if (thunar_list_model_search_terms_match (search_query_c_terms, display_name_c))
        files_found = g_list_prepend (files_found, thunar_file_get (file, NULL));

      /* free memory */
      g_free (display_name_c);
      g_object_unref (file);
      g_object_unref (info);
    }

  g_object_unref (enumerator);
  g_object_unref (directory);

  if (exo_job_is_cancelled (EXO_JOB (job)))
    return;

  g_mutex_lock (&model->mutex_files_to_add);
  model->files_to_add = g_list_concat (model->files_to_add, files_found);
  g_mutex_unlock (&model->mutex_files_to_add);
}



/**
 * thunar_list_model_get_folder:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the #ThunarFolder @store is associated with
 *               or %NULL if @store has no folder.
 **/
ThunarFolder*
thunar_list_model_get_folder (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  return store->folder;
}



/**
 * thunar_list_model_set_folder:
 * @store                       : a valid #ThunarListModel.
 * @folder                      : a #ThunarFolder or %NULL.
 * @search_query                : a #string or %NULL.
 **/
void
thunar_list_model_set_folder (ThunarListModel *store,
                              ThunarFolder    *folder,
                              gchar           *search_query)
{
  GtkTreePath   *path;
  gboolean       has_handler;
  GList         *files;
  GSequenceIter *row;
  GSequenceIter *end;
  GSequenceIter *next;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (folder == NULL || THUNAR_IS_FOLDER (folder));

  /* unlink from the previously active folder (if any) */
  if (G_LIKELY (store->folder != NULL))
    {
      thunar_list_model_cancel_search_job (store);

      if (store->update_search_results_timeout_id > 0)
        {
          g_source_remove (store->update_search_results_timeout_id);
          store->update_search_results_timeout_id = 0;
        }
      thunar_g_list_free_full (store->files_to_add);
      store->files_to_add = NULL;

      /* check if we have any handlers connected for "row-deleted" */
      has_handler = g_signal_has_handler_pending (G_OBJECT (store), store->row_deleted_id, 0, FALSE);

      row = g_sequence_get_begin_iter (store->rows);
      end = g_sequence_get_end_iter (store->rows);

      /* remove existing entries */
      path = gtk_tree_path_new_first ();
      while (row != end)
        {
          /* remove the row from the list */
          next = g_sequence_iter_next (row);
          g_sequence_remove (row);
          row = next;

          /* notify the view(s) if they're actually
           * interested in the "row-deleted" signal.
           */
          if (G_LIKELY (has_handler))
            gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
        }
      gtk_tree_path_free (path);

      /* remove hidden entries */
      g_slist_free_full (store->hidden, g_object_unref);
      store->hidden = NULL;

      /* unregister signals and drop the reference */
      g_signal_handlers_disconnect_matched (G_OBJECT (store->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (G_OBJECT (store->folder));
    }

  /* ... just to be sure! */
  _thunar_assert (g_sequence_get_length (store->rows) == 0);

#ifndef NDEBUG
  /* new stamp since the model changed */
  store->stamp = g_random_int ();
#endif

  /* activate the new folder */
  store->folder = folder;

  /* freeze */
  g_object_freeze_notify (G_OBJECT (store));

  /* connect to the new folder (if any) */
  if (folder != NULL)
    {
      g_object_ref (G_OBJECT (folder));

      /* get the already loaded files or search for files matching the search_query
       * don't start searching if the query is empty, that would be a waste of resources
       */
      if (search_query == NULL || strlen (g_strstrip (search_query)) == 0)
        {
          files = thunar_folder_get_files (folder);

          if (store->search_terms != NULL)
            {
              g_strfreev (store->search_terms);
              store->search_terms = NULL;
            }
        }
      else
        {
          gchar *search_query_c;  /* normalized */

          search_query_c = thunar_g_utf8_normalize_for_search (search_query, TRUE, TRUE);
          g_strfreev (store->search_terms);
          store->search_terms = thunar_list_model_split_search_query (search_query_c, NULL);
          if (store->search_terms != NULL)
            {
              /* search the current folder
               * start a new recursive_search_job */
              store->recursive_search_job = thunar_list_model_job_search_directory (store, search_query_c, thunar_folder_get_corresponding_file (folder));
              exo_job_launch (EXO_JOB (store->recursive_search_job));

              g_signal_connect (store->recursive_search_job, "error", G_CALLBACK (thunar_list_model_search_error), NULL);
              g_signal_connect (store->recursive_search_job, "finished", G_CALLBACK (thunar_list_model_search_finished), store);

              /* add new results to the model every X ms */
              store->update_search_results_timeout_id = g_timeout_add (500, thunar_list_model_add_search_files, store);
            }
          g_free (search_query_c);
          files = NULL;
        }

      /* insert the files */
      if (files != NULL)
        thunar_list_model_insert_files (store, files);

      /* connect signals to the new folder */
      g_signal_connect (G_OBJECT (store->folder), "destroy", G_CALLBACK (thunar_list_model_folder_destroy), store);
      g_signal_connect (G_OBJECT (store->folder), "error", G_CALLBACK (thunar_list_model_folder_error), store);
      g_signal_connect (G_OBJECT (store->folder), "files-added", G_CALLBACK (thunar_list_model_files_added), store);
      g_signal_connect (G_OBJECT (store->folder), "files-removed", G_CALLBACK (thunar_list_model_files_removed), store);
    }

  /* notify listeners that we have a new folder */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FOLDER]);
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
  g_object_thaw_notify (G_OBJECT (store));
}



/**
 * thunar_list_model_get_folders_first:
 * @store : a #ThunarListModel.
 *
 * Determines whether @store lists folders first.
 *
 * Return value: %TRUE if @store lists folders first.
 **/
static gboolean
thunar_list_model_get_folders_first (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->sort_folders_first;
}



/**
 * thunar_list_model_set_folders_first:
 * @store         : a #ThunarListModel.
 * @folders_first : %TRUE to let @store list folders first.
 **/
void
thunar_list_model_set_folders_first (ThunarListModel *store,
                                     gboolean         folders_first)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if the new setting differs */
  if ((store->sort_folders_first && folders_first)
   || (!store->sort_folders_first && !folders_first))
    return;

  /* apply the new setting (re-sorting the store) */
  store->sort_folders_first = folders_first;
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FOLDERS_FIRST]);
  thunar_list_model_sort (store);

  /* emit a "changed" signal for each row, so the display is
     reloaded with the new folders first setting */
  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                          NULL);
}



/**
 * thunar_list_model_get_show_hidden:
 * @store : a #ThunarListModel.
 *
 * Return value: %TRUE if hidden files will be shown, else %FALSE.
 **/
gboolean
thunar_list_model_get_show_hidden (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->show_hidden;
}



/**
 * thunar_list_model_set_show_hidden:
 * @store       : a #ThunarListModel.
 * @show_hidden : %TRUE if hidden files should be shown, else %FALSE.
 **/
void
thunar_list_model_set_show_hidden (ThunarListModel *store,
                                   gboolean         show_hidden)
{
  GtkTreePath   *path;
  GtkTreeIter    iter;
  ThunarFile    *file;
  GSList        *lp;
  GSequenceIter *row;
  GSequenceIter *next;
  GSequenceIter *end;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if the settings differ */
  if (store->show_hidden == show_hidden)
    return;

  store->show_hidden = show_hidden;

  if (store->show_hidden)
    {
      for (lp = store->hidden; lp != NULL; lp = lp->next)
        {
          file = THUNAR_FILE (lp->data);

          /* insert file in the sorted position */
          row = g_sequence_insert_sorted (store->rows, file,
                                          thunar_list_model_cmp_func, store);

          GTK_TREE_ITER_INIT (iter, store->stamp, row);

          /* tell the view about the new row */
          path = gtk_tree_path_new_from_indices (g_sequence_iter_get_position (row), -1);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);
        }
      g_slist_free (store->hidden);
      store->hidden = NULL;
    }
  else
    {
      _thunar_assert (store->hidden == NULL);

      /* remove all hidden files */
      row = g_sequence_get_begin_iter (store->rows);
      end = g_sequence_get_end_iter (store->rows);

      while (row != end)
        {
          next = g_sequence_iter_next (row);

          file = g_sequence_get (row);
          if (thunar_file_is_hidden (file))
            {
              /* store file in the list */
              store->hidden = g_slist_prepend (store->hidden, g_object_ref (file));

              /* setup path for "row-deleted" */
              path = gtk_tree_path_new_from_indices (g_sequence_iter_get_position (row), -1);

              /* remove file from the model */
              g_sequence_remove (row);

              /* notify the view(s) */
              gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
              gtk_tree_path_free (path);
            }

          row = next;
          _thunar_assert (end == g_sequence_get_end_iter (store->rows));
        }
    }

  /* notify listeners about the new setting */
  g_object_freeze_notify (G_OBJECT (store));
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_SHOW_HIDDEN]);
  g_object_thaw_notify (G_OBJECT (store));
}



/**
 * thunar_list_model_get_file_size_binary:
 * @store : a valid #ThunarListModel object.
 *
 * Returns %TRUE if the file size should be formatted
 * as binary.
 *
 * Return value: %TRUE if file size format is binary.
 **/
gboolean
thunar_list_model_get_file_size_binary (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->file_size_binary;
}



/**
 * thunar_list_model_set_file_size_binary:
 * @store            : a valid #ThunarListModel object.
 * @file_size_binary : %TRUE to format file size as binary.
 *
 * If @file_size_binary is %TRUE the file size should be
 * formatted as binary.
 **/
void
thunar_list_model_set_file_size_binary (ThunarListModel *store,
                                        gboolean         file_size_binary)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* normalize the setting */
  file_size_binary = !!file_size_binary;

  /* check if we have a new setting */
  if (store->file_size_binary != file_size_binary)
    {
      /* apply the new setting */
      store->file_size_binary = file_size_binary;

      /* resort the model with the new setting */
      thunar_list_model_sort (store);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FILE_SIZE_BINARY]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new binary file size setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



/**
 * thunar_list_model_get_folder_item_count:
 * @store : a #ThunarListModel.
 *
 * Return value: A value of the enum #ThunarFolderItemCount
 **/
static gint
thunar_list_model_get_folder_item_count (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->folder_item_count;
}



/**
 * thunar_list_model_set_folder_item_count:
 * @store             : a #ThunarListModel.
 * @count_as_dir_size : a value of the enum #ThunarFolderItemCount
 **/
static void
thunar_list_model_set_folder_item_count (ThunarListModel         *store,
                                         ThunarFolderItemCount    count_as_dir_size)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if the new setting differs */
  if (store->folder_item_count == count_as_dir_size)
    return;

  store->folder_item_count = count_as_dir_size;
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FOLDER_ITEM_COUNT]);

  gtk_tree_model_foreach (GTK_TREE_MODEL (store), (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed, NULL);

  /* re-sorting the store if needed */
  if (store->sort_func == sort_by_size || store->sort_func == (ThunarSortFunc) sort_by_size_and_items_count)
  {
    store->sort_func = (store->folder_item_count != THUNAR_FOLDER_ITEM_COUNT_NEVER) ? (ThunarSortFunc) sort_by_size_and_items_count : sort_by_size;
    thunar_list_model_sort (store);
  }
}



/**
 * thunar_list_model_get_file:
 * @store : a #ThunarListModel.
 * @iter  : a valid #GtkTreeIter for @store.
 *
 * Returns the #ThunarFile referred to by @iter. Free
 * the returned object using #g_object_unref() when
 * you are done with it.
 *
 * Return value: the #ThunarFile.
 **/
ThunarFile*
thunar_list_model_get_file (ThunarListModel *store,
                            GtkTreeIter     *iter)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (iter->stamp == store->stamp, NULL);

  return g_object_ref (g_sequence_get (iter->user_data));
}



/**
 * thunar_list_model_get_num_files:
 * @store : a #ThunarListModel.
 *
 * Counts the number of visible files for @store, taking into account the
 * current setting of "show-hidden".
 *
 * Return value: ahe number of visible files in @store.
 **/
static gint
thunar_list_model_get_num_files (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);
  return g_sequence_get_length (store->rows);
}



/**
 * thunar_list_model_get_paths_for_files:
 * @store : a #ThunarListModel instance.
 * @files : a list of #ThunarFile<!---->s.
 *
 * Determines the list of #GtkTreePath<!---->s for the #ThunarFile<!---->s
 * found in the @files list. If a #ThunarFile from the @files list is not
 * available in @store, no #GtkTreePath will be returned for it. So, in effect,
 * only #GtkTreePath<!---->s for the subset of @files available in @store will
 * be returned.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s for @files.
 **/
GList*
thunar_list_model_get_paths_for_files (ThunarListModel *store,
                                       GList           *files)
{
  GList         *paths = NULL;
  GSequenceIter *row;
  GSequenceIter *end;
  gint           i = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  row = g_sequence_get_begin_iter (store->rows);
  end = g_sequence_get_end_iter (store->rows);

  /* find the rows for the given files */
  while (row != end)
    {
      if (g_list_find (files, g_sequence_get (row)) != NULL)
        {
          _thunar_assert (i == g_sequence_iter_get_position (row));
          paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (i, -1));
        }

      row = g_sequence_iter_next (row);
      i++;
    }

  return paths;
}



/**
 * thunar_list_model_get_paths_for_pattern:
 * @store          : a #ThunarListModel instance.
 * @pattern        : the pattern to match.
 * @case_sensitive    : %TRUE to use case sensitive search.
 * @match_diacritics : %TRUE to use case sensitive search.
 *
 * Looks up all rows in the @store that match @pattern and returns
 * a list of #GtkTreePath<!---->s corresponding to the rows.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s that match @pattern.
 **/
GList*
thunar_list_model_get_paths_for_pattern (ThunarListModel *store,
                                         const gchar     *pattern,
                                         gboolean         case_sensitive,
                                         gboolean         match_diacritics)
{
  GPatternSpec  *pspec;
  gchar         *normalized_pattern;
  GList         *paths = NULL;
  GSequenceIter *row;
  GSequenceIter *end;
  ThunarFile    *file;
  const gchar   *display_name;
  gchar         *normalized_display_name;
  gboolean       name_matched;
  gint           i = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (g_utf8_validate (pattern, -1, NULL), NULL);

  /* compile the pattern */
  normalized_pattern = thunar_g_utf8_normalize_for_search (pattern, !match_diacritics, !case_sensitive);
  pspec = g_pattern_spec_new (normalized_pattern);
  g_free (normalized_pattern);

  row = g_sequence_get_begin_iter (store->rows);
  end = g_sequence_get_end_iter (store->rows);

  /* find all rows that match the given pattern */
  while (row != end)
    {
      file = g_sequence_get (row);
      display_name = thunar_file_get_display_name (file);

      normalized_display_name = thunar_g_utf8_normalize_for_search (display_name, !match_diacritics, !case_sensitive);
      name_matched = g_pattern_match_string (pspec, normalized_display_name);
      g_free (normalized_display_name);

      if (name_matched)
        {
          _thunar_assert (i == g_sequence_iter_get_position (row));
          paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (i, -1));
        }

      row = g_sequence_iter_next (row);
      i++;
    }

  /* release the pattern */
  g_pattern_spec_free (pspec);

  return paths;
}



/**
 * thunar_list_model_get_statusbar_text_for_files:
 * @files                        : list of files for which a text is requested
 * @show_file_size_binary_format : weather the file size should be displayed in binary format
 *
 * Generates the statusbar text for the given @files.
 *
 * The caller is reponsible to free the returned text using
 * g_free() when it's no longer needed.
 *
 * Return value: the statusbar text for @store with the given @files.
 **/
static gchar*
thunar_list_model_get_statusbar_text_for_files (ThunarListModel *store,
                                                GList           *files,
                                                gboolean         show_file_size_binary_format)
{
  guint64            size_summary       = 0;
  gint               folder_count       = 0;
  gint               non_folder_count   = 0;
  GList             *lp;
  GList             *text_list          = NULL;
  gchar             *size_string        = NULL;
  gchar             *temp_string        = NULL;
  gchar             *folder_text        = NULL;
  gchar             *non_folder_text    = NULL;
  ThunarPreferences *preferences;
  guint              active;
  guint64            last_modified_date = 0;
  guint64            temp_last_modified_date;
  ThunarFile        *last_modified_file = NULL;
  gboolean           show_size, show_size_in_bytes, show_last_modified;

  preferences = thunar_preferences_get ();
  g_object_get (G_OBJECT (preferences), "misc-status-bar-active-info", &active, NULL);
  show_size = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE);
  show_size_in_bytes = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES);
  show_last_modified = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_LAST_MODIFIED);
  g_object_unref (preferences);

  /* analyze files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      if (thunar_file_is_directory (lp->data))
        {
          folder_count++;
        }
      else
        {
          non_folder_count++;
          if (thunar_file_is_regular (lp->data))
            size_summary += thunar_file_get_size (lp->data);
        }
      temp_last_modified_date = thunar_file_get_date (lp->data, THUNAR_FILE_DATE_MODIFIED);
      if (last_modified_date <= temp_last_modified_date)
        {
          last_modified_date = temp_last_modified_date;
          last_modified_file = lp->data;
        }
    }

  if (non_folder_count > 0)
    {
      if (show_size == TRUE)
        {
          if (show_size_in_bytes == TRUE)
            {
              size_string = g_format_size_full (size_summary, G_FORMAT_SIZE_LONG_FORMAT
                                                              | (show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS
                                                                                              : G_FORMAT_SIZE_DEFAULT));
            }
          else
            {
              size_string = g_format_size_full (size_summary, show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS
                                                                                           : G_FORMAT_SIZE_DEFAULT);
            }
          non_folder_text = g_strdup_printf (ngettext ("%d file: %s",
                                                       "%d files: %s",
                                                       non_folder_count), non_folder_count, size_string);
          g_free (size_string);
        }
      else
        non_folder_text = g_strdup_printf (ngettext ("%d file", "%d files", non_folder_count), non_folder_count);
    }

  if (folder_count > 0)
    {
      folder_text = g_strdup_printf (ngettext ("%d folder",
                                               "%d folders",
                                               folder_count), folder_count);
    }

  if (non_folder_text == NULL && folder_text == NULL)
    text_list = g_list_append (text_list, g_strdup_printf (_("0 items")));
  if (folder_text != NULL)
    text_list = g_list_append (text_list, folder_text);
  if (non_folder_text != NULL)
    text_list = g_list_append (text_list, non_folder_text);

  if (show_last_modified && last_modified_file != NULL)
    {
      temp_string = thunar_file_get_date_string (last_modified_file, THUNAR_FILE_DATE_MODIFIED, store->date_style, store->date_custom_style);
      text_list = g_list_append (text_list, g_strdup_printf (_("Last Modified: %s"), temp_string));
      g_free (temp_string);
    }

  temp_string = thunar_util_strjoin_list (text_list, "  |  ");
  g_list_free_full (text_list, g_free);
  return temp_string;
}



/**
 * thunar_list_model_get_statusbar_text:
 * @store          : a #ThunarListModel instance.
 * @selected_items : the list of selected items (as GtkTreePath's).
 *
 * Generates the statusbar text for @store with the given
 * @selected_items.
 *
 * This function is used by the #ThunarStandardView (and thereby
 * implicitly by #ThunarIconView and #ThunarDetailsView) to
 * calculate the text to display in the statusbar for a given
 * file selection.
 *
 * The caller is reponsible to free the returned text using
 * g_free() when it's no longer needed.
 *
 * Return value: the statusbar text for @store with the given
 *               @selected_items.
 **/
gchar*
thunar_list_model_get_statusbar_text (ThunarListModel *store,
                                      GList           *selected_items)
{
  const gchar       *content_type;
  const gchar       *original_path;
  GtkTreeIter        iter;
  ThunarFile        *file;
  guint64            size;
  GList             *lp;
  GList             *text_list      = NULL;
  gchar             *temp_string    = NULL;
  gchar             *text           = "";
  gint               height;
  gint               width;
  GSequenceIter     *row;
  GSequenceIter     *end;
  ThunarPreferences *preferences;
  gboolean           show_image_size;
  gboolean           show_file_size_binary_format;
  GList             *relevant_files = NULL;
  guint              active;
  gboolean           show_size, show_size_in_bytes, show_filetype, show_display_name, show_last_modified;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  preferences = thunar_preferences_get ();
  g_object_get (G_OBJECT (preferences), "misc-status-bar-active-info", &active, NULL);
  show_size = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE);
  show_size_in_bytes = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES);
  show_filetype = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_FILETYPE);
  show_display_name = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_DISPLAY_NAME);
  show_last_modified = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_LAST_MODIFIED);
  show_file_size_binary_format = thunar_list_model_get_file_size_binary(store);

  if (selected_items == NULL) /* nothing selected */
    {
      /* build a GList of all files */
      end = g_sequence_get_end_iter (store->rows);
      for (row = g_sequence_get_begin_iter (store->rows); row != end; row = g_sequence_iter_next (row))
        {
          relevant_files = g_list_append (relevant_files, g_sequence_get (row));
        }

      /* try to determine a file for the current folder */
      file = (store->folder != NULL) ? thunar_folder_get_corresponding_file (store->folder) : NULL;
      temp_string = thunar_list_model_get_statusbar_text_for_files (store, relevant_files, show_file_size_binary_format);
      text_list = g_list_append (text_list, temp_string);

      /* check if we can determine the amount of free space for the volume */
      if (G_LIKELY (file != NULL && thunar_g_file_get_free_space (thunar_file_get_file (file), &size, NULL)))
        {
          /* humanize the free space */
          gchar *size_string = g_format_size_full (size, show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
          temp_string = g_strdup_printf (_("Free space: %s"), size_string);
          text_list = g_list_append (text_list, temp_string);
          g_free (size_string);
        }

      g_list_free (relevant_files);
    }
  else if (selected_items->next == NULL) /* only one item selected */
    {
      /* resolve the iter for the single path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, selected_items->data);

      /* get the file for the given iter */
      file = g_sequence_get (iter.user_data);

      /* determine the content type of the file */
      content_type = thunar_file_get_content_type (file);

      if (show_display_name == TRUE)
        {
          temp_string = g_strdup_printf (_("\"%s\""), thunar_file_get_display_name (file));
          text_list = g_list_append (text_list, temp_string);
        }

      if (thunar_file_is_regular (file) || G_UNLIKELY (thunar_file_is_symlink (file)))
        {
          if (show_size == TRUE)
            {
              if (show_size_in_bytes == TRUE)
                temp_string = thunar_file_get_size_string_long (file, show_file_size_binary_format);
              else
                temp_string = thunar_file_get_size_string_formatted (file, show_file_size_binary_format);
              text_list = g_list_append (text_list, temp_string);
            }
        }

      if (show_filetype == TRUE)
        {
          if (G_UNLIKELY (content_type != NULL && g_str_equal (content_type, "inode/symlink")))
            temp_string = g_strdup (_("broken link"));
          else if (G_UNLIKELY (thunar_file_is_symlink (file)))
            temp_string = g_strdup_printf (_("link to %s"), thunar_file_get_symlink_target (file));
          else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_SHORTCUT))
            temp_string = g_strdup (_("shortcut"));
          else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_MOUNTABLE))
            temp_string = g_strdup (_("mountable"));
          else
            {
              gchar *description = g_content_type_get_description (content_type);
              temp_string = g_strdup_printf (_("%s"), description);
              g_free (description);
            }
          text_list = g_list_append (text_list, temp_string);
        }

      /* append the original path (if any) */
      original_path = thunar_file_get_original_path (file);
      if (G_UNLIKELY (original_path != NULL))
        {
          /* append the original path to the statusbar text */
          gchar *original_path_string = g_filename_display_name (original_path);
          temp_string = g_strdup_printf ("%s %s", _("Original Path:"), original_path_string);
          text_list = g_list_append (text_list, temp_string);
          g_free (original_path_string);
        }
      else if (thunar_file_is_local (file)
               && thunar_file_is_regular (file)
               && g_str_has_prefix (content_type, "image/")) /* bug #2913 */
        {
          /* check if the size should be visible in the statusbar, disabled by
           * default to avoid high i/o  */
          g_object_get (preferences, "misc-image-size-in-statusbar", &show_image_size, NULL);
          if (show_image_size)
            {
              /* check if we can determine the dimension of this file (only for image files) */
              gchar *file_path = g_file_get_path (thunar_file_get_file (file));
              if (file_path != NULL && gdk_pixbuf_get_file_info (file_path, &width, &height) != NULL)
                {
                  /* append the image dimensions to the statusbar text */
                  temp_string = g_strdup_printf ("%s %dx%d", _("Image Size:"), width, height);
                  text_list = g_list_append (text_list, temp_string);
                }
              g_free (file_path);
            }
        }

      if (show_last_modified)
        {
          gchar *date_string = thunar_file_get_date_string (file, THUNAR_FILE_DATE_MODIFIED, store->date_style, store->date_custom_style);
          temp_string = g_strdup_printf (_("Last Modified: %s"), date_string);
          text_list = g_list_append (text_list, temp_string);
          g_free (date_string);
        }
    }
  else /* more than one item selected */
    {
      gchar *selected_string;
      /* build GList of files from selection */
      for (lp = selected_items; lp != NULL; lp = lp->next)
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, lp->data);
          relevant_files = g_list_append (relevant_files, g_sequence_get (iter.user_data));
        }
      selected_string = thunar_list_model_get_statusbar_text_for_files (store, relevant_files, show_file_size_binary_format);
      temp_string = g_strdup_printf (_("Selection: %s"), selected_string);
      text_list = g_list_append (text_list, temp_string);
      g_list_free (relevant_files);
      g_free (selected_string);
    }

  text = thunar_util_strjoin_list (text_list, "  |  ");
  g_list_free_full (text_list, g_free);
  g_object_unref (preferences);
  return text;
}



static void
thunar_list_model_file_count_callback (ExoJob  *job,
                                       gpointer model)
{
  GArray     *param_values;
  ThunarFile *file;

  param_values = thunar_simple_job_get_param_values (THUNAR_SIMPLE_JOB (job));
  file = THUNAR_FILE (g_value_get_object (&g_array_index (param_values, GValue, 0)));

  if (file == NULL)
    return;

  thunar_list_model_file_changed (NULL, file, THUNAR_LIST_MODEL (model));
}
