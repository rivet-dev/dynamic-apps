/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Pratyaksh Gautam <pratyakshgautam11@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <thunar/thunar-application.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-job-operation.h>
#include <thunar/thunar-private.h>

/**
 * SECTION:thunar-job-operation
 * @Short_description: Stores a job operation (copy, move, trash, rename, etc.)
 * @Title: ThunarJobOperation
 *
 * The #ThunarJobOperation class represents a single 'job operation', a file operation like copying, moving
 * trashing, renaming etc. and its source/target locations.
 *
 */

static void                   thunar_job_operation_finalize           (GObject            *object);
static gint                   thunar_job_operation_is_ancestor        (gconstpointer       descendant,
                                                                       gconstpointer       ancestor);
static void                   thunar_job_operation_restore_from_trash (ThunarJobOperation *operation,
                                                                       GError            **error);



struct _ThunarJobOperation
{
  GObject                 __parent__;

  ThunarJobOperationKind  operation_kind;
  GList                  *source_file_list;
  GList                  *target_file_list;

  /* Files overwritten as a part of an operation */
  GList                  *overwritten_files;

  /**
   * Optional timestamps (in seconds) which tell when the operation was started and ended.
   * Only used for trash/restore operations.
   **/
  gint64                  start_timestamp;
  gint64                  end_timestamp;
};

G_DEFINE_TYPE (ThunarJobOperation, thunar_job_operation, G_TYPE_OBJECT)



static void
thunar_job_operation_class_init (ThunarJobOperationClass *klass)
{
  GObjectClass      *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_job_operation_finalize;
}



static void
thunar_job_operation_init (ThunarJobOperation *self)
{
  self->operation_kind = THUNAR_JOB_OPERATION_KIND_COPY;
  self->source_file_list = NULL;
  self->target_file_list = NULL;
  self->overwritten_files = NULL;
}



static void
thunar_job_operation_finalize (GObject *object)
{
  ThunarJobOperation *op;

  op = THUNAR_JOB_OPERATION (object);

  g_list_free_full (op->source_file_list, g_object_unref);
  g_list_free_full (op->target_file_list, g_object_unref);
  g_list_free_full (op->overwritten_files, g_object_unref);

  (*G_OBJECT_CLASS (thunar_job_operation_parent_class)->finalize) (object);
}



/**
 * thunar_job_operation_new:
 * @kind: The kind of operation being created.
 *
 * Creates a new #ThunarJobOperation of the given kind. Should be unref'd by the caller after use.
 *
 * Return value: (transfer full): the newly created #ThunarJobOperation
 **/
ThunarJobOperation *
thunar_job_operation_new (ThunarJobOperationKind kind)
{
  ThunarJobOperation *operation;

  operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
  operation->operation_kind = kind;

  /* we store the start timestamp in seconds, so we need to divide by 1e6 */
  operation->start_timestamp = g_get_real_time () / (gint64) 1e6 ;

  return operation;
}



/**
 * thunar_job_operation_add:
 * @job_operation: a #ThunarJobOperation
 * @source_file:   a #GFile representing the source file
 * @target_file:   a #GFile representing the target file
 *
 * Adds the specified @source_file-@target_file pair to the given job operation.
 **/
void
thunar_job_operation_add (ThunarJobOperation *job_operation,
                          GFile              *source_file,
                          GFile              *target_file)
{

  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));
  _thunar_return_if_fail (source_file == NULL || G_IS_FILE (source_file));
  _thunar_return_if_fail (target_file == NULL || G_IS_FILE (target_file));

  /* When a directory has a file operation applied to it (for e.g. deletion),
   * the operation will also automatically get applied to its descendants.
   * If the descendant of a that directory is then found, it will try to apply the operation
   * to it again then, meaning the operation is attempted multiple times on the same file.
   *
   * So to avoid such issues on executing a job operation, if the source file is
   * a descendant of an existing file, do not add it to the job operation. */
  if (g_list_find_custom (job_operation->source_file_list, source_file, thunar_job_operation_is_ancestor) != NULL)
    return;

  /* Don't add NULL files to the list, since it causes memory management issues on object destruction */
  if (source_file != NULL)
    job_operation->source_file_list = g_list_append (job_operation->source_file_list, g_object_ref (source_file));
  if (target_file != NULL)
    job_operation->target_file_list = g_list_append (job_operation->target_file_list, g_object_ref (target_file));
}



/***
 * thunar_job_operation_overwrite:
 * @job_operation:    a #ThunarJobOperation
 * @overwritten_file: a #GFile representing the file that has been overwritten
 *
 * Logs a file overwritten as a part of an operation.
 **/
void
thunar_job_operation_overwrite (ThunarJobOperation *job_operation,
                                GFile              *overwritten_file)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  job_operation->overwritten_files = thunar_g_list_append_deep (job_operation->overwritten_files, overwritten_file);
}



/**
 * thunar_job_operation_get_timestamps:
 * @job_operation: a #ThunarJobOperation
 * @start_timestamp: Will be set to the current start_timestamp of the #ThunarJobOperation
 * @end_timestamp: Will be set to the current end_timestamp of the #ThunarJobOperation
 *
 * Getter for both timestamps
 **/
void
thunar_job_operation_get_timestamps (ThunarJobOperation *job_operation,
                                     gint64             *start_timestamp,
                                     gint64             *end_timestamp)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  if (start_timestamp != NULL)
    *start_timestamp = job_operation->start_timestamp;
  if (end_timestamp != NULL)
    *end_timestamp = job_operation->end_timestamp;
}



/**
 * thunar_job_operation_set_start_timestamp:
 * @job_operation: a #ThunarJobOperation
 * @start_timestamp: the new start_timestamp of the #ThunarJobOperation
 *
 * Setter for start_timestamp
 **/
void
thunar_job_operation_set_start_timestamp (ThunarJobOperation *job_operation,
                                          gint64              start_timestamp)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  job_operation->start_timestamp = start_timestamp;
}



/**
 * thunar_job_operation_set_end_timestamp:
 * @job_operation: a #ThunarJobOperation
 * @start_timestamp: the new end_timestamp of the #ThunarJobOperation
 *
 * Setter for end_timestamp
 **/
void
thunar_job_operation_set_end_timestamp (ThunarJobOperation *job_operation,
                                        gint64              end_timestamp)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  job_operation->end_timestamp = end_timestamp;
}



/* thunar_job_operation_get_kind:
 * @job_operation: A #ThunarJobOperation
 *
 * Get the kind of the operation
 *
 * Return value: The kind of the operation
 **/
ThunarJobOperationKind
thunar_job_operation_get_kind (ThunarJobOperation *job_operation)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB_OPERATION (job_operation), THUNAR_JOB_OPERATION_KIND_COPY);
   return job_operation->operation_kind;
}



/* thunar_job_operation_get_overwritten_files:
 * @job_operation: A #ThunarJobOperation
 *
 * Get the overwritten_files of the operation
 *
 * Return value: The overwritten_files of the operation
 **/
const GList*
thunar_job_operation_get_overwritten_files (ThunarJobOperation *job_operation)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB_OPERATION (job_operation), NULL);
   return job_operation->overwritten_files;
}



gboolean
thunar_job_operation_empty (ThunarJobOperation *job_operation)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB_OPERATION (job_operation), TRUE);

  if (job_operation->source_file_list == NULL && job_operation->target_file_list == NULL)
    return TRUE;

  return FALSE;
}

/* thunar_job_operation_get_kind_nick:
 * @job_operation: A #ThunarJobOperation
 *
 * Get the nick name of the operation's kind in string format.
 * The string returned should NOT be freed.
 *
 * Return value: A string containing the nick name of the job operation's kind
 **/
const gchar *
thunar_job_operation_get_kind_nick (ThunarJobOperation *job_operation)
{
  GEnumClass         *enum_class;
  GEnumValue         *enum_value;

  /* the enum value of the operation kind, which will be used to get its nick name */
  enum_class = g_type_class_ref (THUNAR_TYPE_JOB_OPERATION_KIND);
  enum_value = g_enum_get_value (enum_class, job_operation->operation_kind);

  return _(enum_value->value_nick);
}



/* thunar_job_operation_new_invert:
 * @job_operation: a #ThunarJobOperation
 *
 * Creates a new job operation which is the inverse of @job_operation.
 * Should be unref'd by the caller after use.
 *
 * Return value: (transfer full): a newly created #ThunarJobOperation which is the inverse of @job_operation
 **/
ThunarJobOperation *
thunar_job_operation_new_invert (ThunarJobOperation *job_operation)
{
  ThunarJobOperation *inverted_operation;

  _thunar_return_val_if_fail (THUNAR_IS_JOB_OPERATION (job_operation), NULL);

  switch (job_operation->operation_kind)
    {
      case THUNAR_JOB_OPERATION_KIND_COPY:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_DELETE;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_MOVE:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_MOVE;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        inverted_operation->target_file_list = thunar_g_list_copy_deep (job_operation->source_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_RENAME:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_RENAME;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        inverted_operation->target_file_list = thunar_g_list_copy_deep (job_operation->source_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_TRASH:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_RESTORE;
        inverted_operation->target_file_list = thunar_g_list_copy_deep (job_operation->source_file_list);
        inverted_operation->start_timestamp = job_operation->start_timestamp;
        inverted_operation->end_timestamp = job_operation->end_timestamp;
        break;

      case THUNAR_JOB_OPERATION_KIND_CREATE_FILE:
      case THUNAR_JOB_OPERATION_KIND_CREATE_FOLDER:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_DELETE;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_LINK:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_UNLINK;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        inverted_operation->target_file_list = thunar_g_list_copy_deep (job_operation->source_file_list);
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  return inverted_operation;
}



/* thunar_job_operation_execute:
 * @job_operation: a #ThunarJobOperation
 * @error: A #GError to propagate any errors encountered.
 *
 * Executes the given @job_operation, depending on what kind of an operation it is.
 *
 * Return value: TRUE if the operation was canceled, FALSE otherwise
 **/
gboolean
thunar_job_operation_execute (ThunarJobOperation *job_operation,
                              GError            **error)
{
  ThunarApplication *application;
  GList             *thunar_file_list = NULL;
  GError            *err              = NULL;
  ThunarJob         *job              = NULL;
  ThunarFile        *thunar_file;
  GFile             *parent_dir;
  gchar             *display_name;
  GFile             *template_file;
  gboolean           operation_canceled = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_JOB_OPERATION (job_operation), FALSE);

  application = thunar_application_get ();

  switch (job_operation->operation_kind)
    {
      case THUNAR_JOB_OPERATION_KIND_DELETE:
      case THUNAR_JOB_OPERATION_KIND_UNLINK:
        for (GList *lp = job_operation->source_file_list; lp != NULL; lp = lp->next)
          {
            if (!G_IS_FILE (lp->data))
              {
                g_warning ("One of the files in the job operation list was not a valid GFile");
                continue;
              }

            thunar_file = thunar_file_get (lp->data, &err);

            if (err != NULL)
              {
                g_warning ("Failed to convert GFile to ThunarFile: %s", err->message);
                g_clear_error (&err);
              }

            if (!THUNAR_IS_FILE (thunar_file))
              {
                g_warning ("One of the files in the job operation list did not convert to a valid ThunarFile");
                continue;
              }

            thunar_file_list = g_list_append (thunar_file_list, thunar_file);
          }

        if (thunar_file_list == NULL)
          {
            *error = g_error_new (G_FILE_ERROR,
                                  G_FILE_ERROR_NOENT,
                                  "No files for deletion found.");
          }
        else
          {
            operation_canceled = thunar_application_unlink_files (application, NULL, thunar_file_list, TRUE, TRUE);
            g_list_free_full (thunar_file_list, g_object_unref);
          }

        break;

      case THUNAR_JOB_OPERATION_KIND_MOVE:
        /* ensure that all the targets have parent directories which exist */
        for (GList *lp = job_operation->target_file_list; lp != NULL; lp = lp->next)
          {
            parent_dir = g_file_get_parent (lp->data);
            g_file_make_directory_with_parents (parent_dir, NULL, &err);
            g_object_unref (parent_dir);

            if (err != NULL)
            {
              /* there is no issue if the target directory already exists */
              if (err->code == G_IO_ERROR_EXISTS)
                {
                  g_clear_error (&err);
                  continue;
                }

              /* output the err message to console otherwise and abort */
              g_warning ("Error while moving files: %s\n"
                         "Aborting operation\n",
                         err->message);
              g_propagate_error (error, err);
              g_object_unref (application);
              return operation_canceled;
            }
          }

        thunar_application_move_files (application, NULL,
                                       job_operation->source_file_list, job_operation->target_file_list,
                                       THUNAR_OPERATION_LOG_NO_OPERATIONS, NULL);
        break;

      case THUNAR_JOB_OPERATION_KIND_RENAME:
        for (GList *slp = job_operation->source_file_list, *tlp = job_operation->target_file_list;
             slp != NULL && tlp != NULL;
             slp = slp->next, tlp = tlp->next)
          {
            display_name = thunar_g_file_get_display_name (tlp->data);
            thunar_file = thunar_file_get (slp->data, &err);

            if (err != NULL)
              {
                g_warning ("Error while renaming files: %s\n", err->message);
                g_propagate_error (error, err);

                g_free (display_name);
                if (thunar_file)
                  g_object_unref (thunar_file);

                continue;
              }

            job = thunar_io_jobs_rename_file (thunar_file, display_name, THUNAR_OPERATION_LOG_NO_OPERATIONS);
            exo_job_launch (EXO_JOB (job));

            g_free (display_name);
            g_object_unref (thunar_file);
          }
        break;

      case THUNAR_JOB_OPERATION_KIND_RESTORE:
        thunar_job_operation_restore_from_trash (job_operation, &err);

        if (err != NULL)
          {
            g_warning ("Error while restoring files: %s\n", err->message);
            g_propagate_error (error, err);
          }
        break;

      case THUNAR_JOB_OPERATION_KIND_COPY:
        thunar_application_copy_to (application, NULL,
                                   job_operation->source_file_list, job_operation->target_file_list,
                                   THUNAR_OPERATION_LOG_NO_OPERATIONS, NULL);
        break;

      case THUNAR_JOB_OPERATION_KIND_CREATE_FILE:
        template_file = NULL;
        if (job_operation->source_file_list != NULL)
          template_file = job_operation->source_file_list->data;
        thunar_application_creat (application, NULL,
                                  job_operation->target_file_list,
                                  template_file,
                                  NULL, THUNAR_OPERATION_LOG_NO_OPERATIONS);
        break;

      case THUNAR_JOB_OPERATION_KIND_CREATE_FOLDER:
        thunar_application_mkdir (application, NULL,
                                  job_operation->target_file_list,
                                  NULL,  THUNAR_OPERATION_LOG_NO_OPERATIONS);
        break;

      case THUNAR_JOB_OPERATION_KIND_TRASH:
        /* Special case: 'THUNAR_JOB_OPERATION_KIND_TRASH' only can be triggered by redo */
        /* Since we as well need to update the timestamps, we have to use THUNAR_OPERATION_LOG_ONLY_TIMESTAMPS */
        /* 'thunar_job_operation_history_update_trash_timestamps' will then take care on update the existing job operation instead of adding a new one */
        thunar_application_trash (application, NULL,
                                  job_operation->source_file_list,
                                  THUNAR_OPERATION_LOG_ONLY_TIMESTAMPS);
        break;

      case THUNAR_JOB_OPERATION_KIND_LINK:
        for (GList* target_file = job_operation->target_file_list; target_file != NULL; target_file = target_file->next)
          {
            GFile* target_folder = g_file_get_parent (target_file->data);
            thunar_application_link_into (application, NULL,
                                          job_operation->source_file_list, target_folder,
                                          THUNAR_OPERATION_LOG_NO_OPERATIONS, NULL);
            g_object_unref (target_folder);
          }
        break;

      default:
        _thunar_assert_not_reached ();
        break;
    }

  g_object_unref (application);
  return operation_canceled;
}



/* thunar_job_operation_is_ancestor:
 * @ancestor:     potential ancestor of @descendant. A #GFile
 * @descendant:   potential descendant of @ancestor. A #GFile
 *
 * Helper function for #g_list_find_custom.
 *
 * Return value: %0 if @ancestor is actually the ancestor of @descendant,
 *               %1 otherwise
 **/
static gint
thunar_job_operation_is_ancestor (gconstpointer ancestor,
                                  gconstpointer descendant)
{
  if (thunar_g_file_is_descendant (G_FILE (descendant), G_FILE (ancestor)))
    return 0;
  else
    return 1;
}



/* thunar_job_operation_compare:
 * @operation1: First operation for comparison
 * @operation2: Second operation for comparison
 *
 * Helper function to compare operation kind, source and target list of two job operations.
 * Note that timestamps are not compared on purpose.
 *
 * Return value: %0 if both operations match
 *               %1 otherwise
 **/
gint
thunar_job_operation_compare (ThunarJobOperation *operation1,
                              ThunarJobOperation *operation2)
{
  if (operation1->operation_kind != operation2->operation_kind)
    return 1;

  if (g_list_length (operation1->source_file_list) != g_list_length (operation2->source_file_list))
    return 1;

  if (g_list_length (operation1->target_file_list) != g_list_length (operation2->target_file_list))
    return 1;

  for (GList *lp1 = operation1->source_file_list, *lp2 = operation2->source_file_list;
      lp1 != NULL && lp2 != NULL;
      lp1 = lp1->next, lp2 = lp2->next)
    {
      if (!g_file_equal (lp1->data,lp2->data))
        return 1;
    }

  for (GList *lp1 =operation1->target_file_list, *lp2 = operation2->target_file_list;
      lp1 != NULL && lp2 != NULL;
      lp1 = lp1->next, lp2 = lp2->next)
    {
      if (!g_file_equal (lp1->data,lp2->data))
        return 1;
    }
  return 0;
}



/* thunar_job_operation_restore_from_trash::
 * @operation: operation containing the information for the files which must be restored
 * @error:     a GError instance for error handling
 *
 * Helper function to restore files based on the given @operation
 **/
static void
thunar_job_operation_restore_from_trash (ThunarJobOperation  *operation,
                                         GError             **error)
{
  GFileEnumerator   *enumerator;
  GFileInfo         *info;
  GFile             *trash;
  GFile             *trashed_file;
  GFile             *original_file;
  const char        *original_path;
  GDateTime         *date;
  GError            *err = NULL;
  gint64             deletion_time;
  gpointer           lookup;
  GHashTable        *files_trashed;
  ThunarApplication *application;
  GList             *source_file_list = NULL;
  GList             *target_file_list = NULL;


  /* enumerate over the files in the trash */
  trash = g_file_new_for_uri ("trash:///");
  enumerator = g_file_enumerate_children (trash,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                          G_FILE_ATTRIBUTE_TRASH_DELETION_DATE ","
                                          G_FILE_ATTRIBUTE_TRASH_ORIG_PATH,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL, &err);

  if (err != NULL)
    {
      g_object_unref (trash);
      g_object_unref (enumerator);

      g_propagate_error (error, err);
      return;
    }

  /* set up a hash table for the files we deleted */
  files_trashed = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);

  /* add all the files that were deleted in the hash table so we can check if a file
   * was deleted as a part of this operation or not in constant time. */
  for (GList *lp = operation->target_file_list; lp != NULL; lp = lp->next)
  {
    GFile *parent = g_file_get_parent (lp->data);
    gchar *real_path = NULL;
    
    /* Try to resolve symlinks, otherwise Gfiles wont match */
    /* (All files located in trash have symlinks resolved) */
    if (parent != NULL)
      {
        GFile *parent_resolved = NULL;
        gchar *basename = g_file_get_basename (lp->data);
        parent_resolved = thunar_g_file_resolve_symlink (parent);
        g_object_unref (parent);
        if (parent_resolved != NULL && basename != NULL)
          real_path = g_build_filename (g_file_get_path (parent_resolved), basename, NULL);
        g_free (basename);
        g_object_unref (parent_resolved);
      }

    if (real_path != NULL)
      g_hash_table_add (files_trashed, g_file_new_for_path (real_path));
    else
      g_hash_table_add (files_trashed, g_object_ref (lp->data));
    g_free (real_path);
  }

  /* iterate over the files in the trash, adding them to source and target lists of
   * the files which are to be restored and their original paths */
  for (info = g_file_enumerator_next_file (enumerator, NULL, &err); info != NULL; info = g_file_enumerator_next_file (enumerator, NULL, &err))
    {
      if (err != NULL)
        {
          g_object_unref (trash);
          g_object_unref (enumerator);
          g_hash_table_unref (files_trashed);
          g_object_unref (info);

          g_propagate_error (error, err);
          return;
        }

      /* get the original path of the file before deletion */
      original_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_TRASH_ORIG_PATH);
      original_file = g_file_new_for_path (original_path);

      /* get the deletion date reported by the file */
      date = g_file_info_get_deletion_date (info);
      deletion_time = g_date_time_to_unix (date);
      g_date_time_unref (date);

      /* see if we deleted the file in this session */
      lookup = g_hash_table_lookup (files_trashed, original_file);

      /* if we deleted the file in this session, and the current file we're looking at was deleted
       * during the time the operation occured, we conclude we found the right file */
      if (lookup != NULL && operation->start_timestamp <= deletion_time && deletion_time <= operation->end_timestamp)
        {
          trashed_file = g_file_get_child (trash, g_file_info_get_name (info));

          source_file_list = thunar_g_list_append_deep (source_file_list, trashed_file);
          target_file_list = thunar_g_list_append_deep (target_file_list, original_file);

        }

      g_object_unref (original_file);
    }

  g_object_unref (trash);
  g_object_unref (enumerator);

  if (source_file_list != NULL && target_file_list != NULL)
    {
      /* restore the lists asynchronously using a move operation */
      application = thunar_application_get ();
      thunar_application_move_files (application, NULL, source_file_list, target_file_list, THUNAR_OPERATION_LOG_NO_OPERATIONS, NULL);
      g_object_unref (application);

      thunar_g_list_free_full (source_file_list);
      thunar_g_list_free_full (target_file_list);
    }

  g_hash_table_unref (files_trashed);
}
