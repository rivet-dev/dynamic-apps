/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_ENUM_TYPES_H__
#define __THUNAR_ENUM_TYPES_H__

#include <exo/exo.h>

G_BEGIN_DECLS;


gboolean transform_enum_value_to_index (GBinding     *binding,
                                        const GValue *src_value,
                                        GValue       *dst_value,
                                        gpointer      user_data);

gboolean transform_index_to_enum_value (GBinding     *binding,
                                        const GValue *src_value,
                                        GValue       *dst_value,
                                        gpointer      user_data);

#define THUNAR_TYPE_RENAMER_MODE (thunar_renamer_mode_get_type ())

/**
 * ThunarRenamerMode:
 * @THUNAR_RENAMER_MODE_NAME      : only the name should be renamed.
 * @THUNAR_RENAMER_MODE_EXTENSION : only the extension should be renamed.
 * @THUNAR_RENAMER_MODE_BOTH      : the name and the extension should be renamed.
 *
 * The rename mode for a #ThunarRenamerModel instance.
 **/
typedef enum
{
  THUNAR_RENAMER_MODE_NAME,
  THUNAR_RENAMER_MODE_EXTENSION,
  THUNAR_RENAMER_MODE_BOTH,
} ThunarRenamerMode;

GType thunar_renamer_mode_get_type (void) G_GNUC_CONST;



#define THUNAR_TYPE_DATE_STYLE (thunar_date_style_get_type ())

/**
 * ThunarDateStyle:
 * @THUNAR_DATE_STYLE_SIMPLE : display only the date.
 * @THUNAR_DATE_STYLE_SHORT  : display date and time in a short manner.
 * @THUNAR_DATE_STYLE_LONG   : display date and time in a long manner.
 * @THUNAR_DATE_STYLE_ISO    : display date and time in ISO standard form.
 *
 * The style used to display dates (e.g. modification dates) to the user.
 **/
typedef enum
{
  THUNAR_DATE_STYLE_SIMPLE,
  THUNAR_DATE_STYLE_SHORT,
  THUNAR_DATE_STYLE_LONG,
  THUNAR_DATE_STYLE_YYYYMMDD,
  THUNAR_DATE_STYLE_MMDDYYYY,
  THUNAR_DATE_STYLE_DDMMYYYY,
  THUNAR_DATE_STYLE_CUSTOM,
  THUNAR_DATE_STYLE_CUSTOM_SIMPLE,
} ThunarDateStyle;

GType thunar_date_style_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_COLUMN (thunar_column_get_type ())

/**
 * ThunarColumn:
 * @THUNAR_COLUMN_DATE_CREATED  : created time.
 * @THUNAR_COLUMN_DATE_ACCESSED : last access time.
 * @THUNAR_COLUMN_DATE_MODIFIED : last modification time.
 * @THUNAR_COLUMN_DATE_DELETED  : deletion time.
 * @THUNAR_COLUMN_RECENCY       : time of modification of recent info.
 * @THUNAR_COLUMN_LOCATION      : file location.
 * @THUNAR_COLUMN_GROUP         : group's name.
 * @THUNAR_COLUMN_MIME_TYPE     : mime type (e.g. "text/plain").
 * @THUNAR_COLUMN_NAME          : display name.
 * @THUNAR_COLUMN_OWNER         : owner's name.
 * @THUNAR_COLUMN_PERMISSIONS   : permissions bits.
 * @THUNAR_COLUMN_SIZE          : file size.
 * @THUNAR_COLUMN_SIZE_IN_BYTES : file size in bytes.
 * @THUNAR_COLUMN_TYPE          : file type (e.g. 'plain text document').
 * @THUNAR_COLUMN_FILE          : #ThunarFile object.
 * @THUNAR_COLUMN_FILE_NAME     : real file name.
 *
 * Columns exported by #ThunarListModel using the #GtkTreeModel
 * interface.
 **/
typedef enum
{
  /* visible columns */
  THUNAR_COLUMN_DATE_CREATED,
  THUNAR_COLUMN_DATE_ACCESSED,
  THUNAR_COLUMN_DATE_MODIFIED,
  THUNAR_COLUMN_DATE_DELETED,
  THUNAR_COLUMN_RECENCY,
  THUNAR_COLUMN_LOCATION,
  THUNAR_COLUMN_GROUP,
  THUNAR_COLUMN_MIME_TYPE,
  THUNAR_COLUMN_NAME,
  THUNAR_COLUMN_OWNER,
  THUNAR_COLUMN_PERMISSIONS,
  THUNAR_COLUMN_SIZE,
  THUNAR_COLUMN_SIZE_IN_BYTES,
  THUNAR_COLUMN_TYPE,

  /* special internal columns */
  THUNAR_COLUMN_FILE,
  THUNAR_COLUMN_FILE_NAME,

  /* number of columns */
  THUNAR_N_COLUMNS,

  /* number of visible columns */
  THUNAR_N_VISIBLE_COLUMNS = THUNAR_COLUMN_FILE,
} ThunarColumn;

GType        thunar_column_get_type          (void)                      G_GNUC_CONST;
const gchar* thunar_column_string_from_value (ThunarColumn  value);
gboolean     thunar_column_value_from_string (const gchar   *value_string,
                                              ThunarColumn  *value);


#define THUNAR_TYPE_ICON_SIZE (thunar_icon_size_get_type ())

/**
 * ThunarIconSize:
 * Icon sizes matching the various #ThunarZoomLevel<!---->s.
 **/
typedef enum
{
  THUNAR_ICON_SIZE_16    =   16,
  THUNAR_ICON_SIZE_24    =   24,
  THUNAR_ICON_SIZE_32    =   32,
  THUNAR_ICON_SIZE_48    =   48,
  THUNAR_ICON_SIZE_64    =   64,
  THUNAR_ICON_SIZE_96    =   96,
  THUNAR_ICON_SIZE_128   =  128,
  THUNAR_ICON_SIZE_160   =  160,
  THUNAR_ICON_SIZE_192   =  192,
  THUNAR_ICON_SIZE_256   =  256,
  THUNAR_ICON_SIZE_512   =  512,
  THUNAR_ICON_SIZE_1024  = 1024,
} ThunarIconSize;

GType thunar_icon_size_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_THUMBNAIL_MODE (thunar_thumbnail_mode_get_type ())

/**
 * ThunarThumbnailMode:
 * @THUNAR_THUMBNAIL_MODE_NEVER      : never show thumbnails.
 * @THUNAR_THUMBNAIL_MODE_ONLY_LOCAL : only show thumbnails on local filesystems.
 * @THUNAR_THUMBNAIL_MODE_ALWAYS     : always show thumbnails (everywhere).
 **/
typedef enum
{
  THUNAR_THUMBNAIL_MODE_NEVER,
  THUNAR_THUMBNAIL_MODE_ONLY_LOCAL,
  THUNAR_THUMBNAIL_MODE_ALWAYS
} ThunarThumbnailMode;

GType thunar_thumbnail_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_THUMBNAIL_SIZE (thunar_thumbnail_size_get_type ())

/**
 * ThunarThumbnailSize:
 * @THUNAR_THUMBNAIL_NORMAL      : max 128px x 128px
 * @THUNAR_THUMBNAIL_LARGE       : max 256px x 256px
 * @THUNAR_THUMBNAIL_X_LARGE     : max 512px x 512px
 * @THUNAR_THUMBNAIL_XX_LARGE    : max 1024px x 1024px
 **/
typedef enum
{
  THUNAR_THUMBNAIL_SIZE_NORMAL,
  THUNAR_THUMBNAIL_SIZE_LARGE,
  THUNAR_THUMBNAIL_SIZE_X_LARGE,
  THUNAR_THUMBNAIL_SIZE_XX_LARGE,

  THUNAR_THUMBNAIL_SIZE_DEFAULT = -1
} ThunarThumbnailSize;

GType       thunar_thumbnail_size_get_type (void)                               G_GNUC_CONST;
const char* thunar_thumbnail_size_get_nick (ThunarThumbnailSize thumbnail_size) G_GNUC_CONST;


#define THUNAR_TYPE_PARALLEL_COPY_MODE (thunar_parallel_copy_mode_get_type ())

/**
 * ThunarParallelCopyMode:
 * @THUNAR_PARALLEL_COPY_MODE_NEVER                   : copies will be done consecutively, one after another.
 * @THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL              : only do parallel copies when source and destination are local files.
 * @THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL_SAME_DEVICES : same as only local, but only if source and destination devices are the same.
 * @THUNAR_PARALLEL_COPY_MODE_ALWAYS                  : all copies will be started immediately.
 **/
typedef enum
{
  THUNAR_PARALLEL_COPY_MODE_NEVER,
  THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL,
  THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL_SAME_DEVICES,
  THUNAR_PARALLEL_COPY_MODE_ALWAYS
} ThunarParallelCopyMode;

GType thunar_parallel_copy_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_RECURSIVE_PERMISSIONS (thunar_recursive_permissions_get_type ())

/**
 * ThunarRecursivePermissionsMode:
 * @THUNAR_RECURSIVE_PERMISSIONS_ASK    : ask the user every time permissions are changed.
 * @THUNAR_RECURSIVE_PERMISSIONS_ALWAYS : always apply the change recursively.
 * @THUNAR_RECURSIVE_PERMISSIONS_NEVER  : never apply the change recursively.
 *
 * Modus operandi when changing permissions.
 **/
typedef enum
{
  THUNAR_RECURSIVE_PERMISSIONS_ASK,
  THUNAR_RECURSIVE_PERMISSIONS_ALWAYS,
  THUNAR_RECURSIVE_PERMISSIONS_NEVER,
} ThunarRecursivePermissionsMode;

GType thunar_recursive_permissions_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_RECURSIVE_SEARCH (thunar_recursive_search_get_type ())

/**
 * ThunarRecursiveSearchMode:
 * @THUNAR_RECURSIVE_SEARCH_LOCAL       : do recursive search only locally.
 * @THUNAR_RECURSIVE_SEARCH_ALWAYS      : always do recursive search.
 * @THUNAR_RECURSIVE_SEARCH_NEVER       : never do recursive search.
 *
 **/
typedef enum
{
  THUNAR_RECURSIVE_SEARCH_LOCAL,
  THUNAR_RECURSIVE_SEARCH_ALWAYS,
  THUNAR_RECURSIVE_SEARCH_NEVER,
} ThunarRecursiveSearchMode;

GType thunar_recursive_search_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_ZOOM_LEVEL (thunar_zoom_level_get_type ())

/**
 * ThunarZoomLevel:
 * Lists the various zoom levels supported by Thunar's
 * folder views.
 **/
typedef enum
{
  THUNAR_ZOOM_LEVEL_25_PERCENT,
  THUNAR_ZOOM_LEVEL_38_PERCENT,
  THUNAR_ZOOM_LEVEL_50_PERCENT,
  THUNAR_ZOOM_LEVEL_75_PERCENT,
  THUNAR_ZOOM_LEVEL_100_PERCENT,
  THUNAR_ZOOM_LEVEL_150_PERCENT,
  THUNAR_ZOOM_LEVEL_200_PERCENT,
  THUNAR_ZOOM_LEVEL_250_PERCENT,
  THUNAR_ZOOM_LEVEL_300_PERCENT,
  THUNAR_ZOOM_LEVEL_400_PERCENT,
  THUNAR_ZOOM_LEVEL_800_PERCENT,
  THUNAR_ZOOM_LEVEL_1600_PERCENT,

  /*< private >*/
  THUNAR_ZOOM_N_LEVELS,
} ThunarZoomLevel;

GType               thunar_zoom_level_get_type            (void)                       G_GNUC_CONST;
ThunarThumbnailSize thunar_zoom_level_to_thumbnail_size   (ThunarZoomLevel zoom_level) G_GNUC_CONST;
const gchar*        thunar_zoom_level_string_from_value   (ThunarZoomLevel zoom_level);
gboolean            thunar_zoom_level_value_from_string   (const gchar    *value_string,
                                                           gint           *value);


#define THUNAR_TYPE_JOB_RESPONSE (thunar_job_response_get_type ())

/**
 * ThunarJobResponse:
 * @THUNAR_JOB_RESPONSE_YES         :
 * @THUNAR_JOB_RESPONSE_YES_ALL     :
 * @THUNAR_JOB_RESPONSE_NO          :
 * @THUNAR_JOB_RESPONSE_NO_ALL      :
 * @THUNAR_JOB_RESPONSE_CANCEL      :
 * @THUNAR_JOB_RESPONSE_RETRY       :
 * @THUNAR_JOB_RESPONSE_FORCE       :
 * @THUNAR_JOB_RESPONSE_REPLACE     :
 * @THUNAR_JOB_RESPONSE_REPLACE_ALL :
 * @THUNAR_JOB_RESPONSE_SKIP        :
 * @THUNAR_JOB_RESPONSE_SKIP_ALL    :
 * @THUNAR_JOB_RESPONSE_RENAME      :
 * @THUNAR_JOB_RESPONSE_RENAME_ALL  :
 *
 * Possible responses for the ThunarJob::ask signal.
 **/
typedef enum /*< flags >*/
{
  THUNAR_JOB_RESPONSE_YES         = 1 << 0,
  THUNAR_JOB_RESPONSE_YES_ALL     = 1 << 1,
  THUNAR_JOB_RESPONSE_NO          = 1 << 2,
  THUNAR_JOB_RESPONSE_CANCEL      = 1 << 3,
  THUNAR_JOB_RESPONSE_NO_ALL      = 1 << 4,
  THUNAR_JOB_RESPONSE_RETRY       = 1 << 5,
  THUNAR_JOB_RESPONSE_FORCE       = 1 << 6,
  THUNAR_JOB_RESPONSE_REPLACE     = 1 << 7,
  THUNAR_JOB_RESPONSE_REPLACE_ALL = 1 << 8,
  THUNAR_JOB_RESPONSE_SKIP        = 1 << 9,
  THUNAR_JOB_RESPONSE_SKIP_ALL    = 1 << 10,
  THUNAR_JOB_RESPONSE_RENAME      = 1 << 11,
  THUNAR_JOB_RESPONSE_RENAME_ALL  = 1 << 12,
} ThunarJobResponse;
#define THUNAR_JOB_RESPONSE_MAX_INT 12

GType thunar_job_response_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_FILE_MODE (thunar_file_mode_get_type ())

/**
 * ThunarFileMode:
 *
 * Special flags and permissions of a filesystem entity.
 **/
typedef enum /*< flags >*/
{
  THUNAR_FILE_MODE_SUID       = 04000,
  THUNAR_FILE_MODE_SGID       = 02000,
  THUNAR_FILE_MODE_STICKY     = 01000,
  THUNAR_FILE_MODE_USR_ALL    = 00700,
  THUNAR_FILE_MODE_USR_READ   = 00400,
  THUNAR_FILE_MODE_USR_WRITE  = 00200,
  THUNAR_FILE_MODE_USR_EXEC   = 00100,
  THUNAR_FILE_MODE_GRP_ALL    = 00070,
  THUNAR_FILE_MODE_GRP_READ   = 00040,
  THUNAR_FILE_MODE_GRP_WRITE  = 00020,
  THUNAR_FILE_MODE_GRP_EXEC   = 00010,
  THUNAR_FILE_MODE_OTH_ALL    = 00007,
  THUNAR_FILE_MODE_OTH_READ   = 00004,
  THUNAR_FILE_MODE_OTH_WRITE  = 00002,
  THUNAR_FILE_MODE_OTH_EXEC   = 00001,
} ThunarFileMode;

GType thunar_file_mode_get_type (void) G_GNUC_CONST;



#define THUNAR_TYPE_USE_PARTIAL_MODE (thunar_use_partial_get_type ())

/**
 * ThunarUsePartialMode:
 * @THUNAR_USE_PARTIAL_MODE_DISABLED    : Disable *.partial~
 * @THUNAR_USE_PARTIAL_MODE_REMOTE_ONLY : Only when src/dst is remote
 * @THUNAR_USE_PARTIAL_MODE_ALWAYS      : Always copy to *.partial~
 **/
typedef enum
{
  THUNAR_USE_PARTIAL_MODE_DISABLED,
  THUNAR_USE_PARTIAL_MODE_REMOTE_ONLY,
  THUNAR_USE_PARTIAL_MODE_ALWAYS,
} ThunarUsePartialMode;

GType thunar_use_partial_get_type (void) G_GNUC_CONST;



#define THUNAR_TYPE_VERIFY_FILE_MODE (thunar_verify_file_get_type ())

/**
 * ThunarUsePartialMode:
 * @THUNAR_VERIFY_FILE_MODE_DISABLED    : Disable file checksum
 * @THUNAR_VERIFY_FILE_MODE_REMOTE_ONLY : Only when src/dst is remote
 * @THUNAR_VERIFY_FILE_MODE_ALWAYS      : Always verify file checksum
 **/
typedef enum
{
  THUNAR_VERIFY_FILE_MODE_DISABLED,
  THUNAR_VERIFY_FILE_MODE_REMOTE_ONLY,
  THUNAR_VERIFY_FILE_MODE_ALWAYS,
} ThunarVerifyFileMode;

GType thunar_verify_file_get_type (void) G_GNUC_CONST;



/**
 * ThunarNewTabBehavior:
 * @THUNAR_NEW_TAB_BEHAVIOR_FOLLOW_PREFERENCE   : switching to the new tab or not is controlled by a preference.
 * @THUNAR_NEW_TAB_BEHAVIOR_SWITCH              : switch to the new tab.
 * @THUNAR_NEW_TAB_BEHAVIOR_STAY                : stay at the current tab.
 **/
typedef enum
{
    THUNAR_NEW_TAB_BEHAVIOR_FOLLOW_PREFERENCE,
    THUNAR_NEW_TAB_BEHAVIOR_SWITCH,
    THUNAR_NEW_TAB_BEHAVIOR_STAY
} ThunarNewTabBehavior;



typedef enum
{
    THUNAR_STATUS_BAR_INFO_SIZE           = 1 << 0,
    THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES  = 1 << 1,
    THUNAR_STATUS_BAR_INFO_FILETYPE       = 1 << 2,
    THUNAR_STATUS_BAR_INFO_DISPLAY_NAME   = 1 << 3,
    THUNAR_STATUS_BAR_INFO_LAST_MODIFIED  = 1 << 4,
} ThunarStatusBarInfo;

guint    thunar_status_bar_info_toggle_bit    (guint               info,
                                               ThunarStatusBarInfo mask);
gboolean thunar_status_bar_info_check_active  (guint               info,
                                               ThunarStatusBarInfo mask);



#define THUNAR_TYPE_JOB_OPERATION_KIND (thunar_job_operation_kind_get_type ())

typedef enum
{
  THUNAR_JOB_OPERATION_KIND_COPY,
  THUNAR_JOB_OPERATION_KIND_MOVE,
  THUNAR_JOB_OPERATION_KIND_RENAME,
  THUNAR_JOB_OPERATION_KIND_CREATE_FILE,
  THUNAR_JOB_OPERATION_KIND_CREATE_FOLDER,
  THUNAR_JOB_OPERATION_KIND_DELETE,
  THUNAR_JOB_OPERATION_KIND_TRASH,
  THUNAR_JOB_OPERATION_KIND_RESTORE,
  THUNAR_JOB_OPERATION_KIND_LINK,
  THUNAR_JOB_OPERATION_KIND_UNLINK
} ThunarJobOperationKind;

GType thunar_job_operation_kind_get_type (void) G_GNUC_CONST;

/**
 * ThunarOperationLogMode:
 *
 * Specify control logging for operations.
 **/

#define THUNAR_TYPE_OPERATION_LOG_MODE (thunar_operation_log_mode_get_type ())

typedef enum
{
  THUNAR_OPERATION_LOG_NO_OPERATIONS,
  THUNAR_OPERATION_LOG_OPERATIONS,
  THUNAR_OPERATION_LOG_ONLY_TIMESTAMPS,
} ThunarOperationLogMode;

GType thunar_operation_log_mode_get_type (void) G_GNUC_CONST;

/**
 * ThunarImagePreviewMode:
 *
 * Specify control logging for operations.
 **/

#define THUNAR_TYPE_IMAGE_PREVIEW_MODE (thunar_image_preview_mode_get_type ())

typedef enum
{
  THUNAR_IMAGE_PREVIEW_MODE_STANDALONE,
  THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED,
} ThunarImagePreviewMode;

GType thunar_image_preview_mode_get_type (void) G_GNUC_CONST;

/**
 * ThunarFolderItemCount
 *
 * Specify when the size column on a folder
 * should instead show the item count of the folder
 **/

#define THUNAR_TYPE_FOLDER_ITEM_COUNT (thunar_folder_item_count_get_type ())

/**
 * ThunarFolderItemCount:
 * @THUNAR_FOLDER_ITEM_COUNT_NEVER,        : never show number of items as the size of the folder
 * @THUNAR_FOLDER_ITEM_COUNT_ONLY_LOCAL,   : only show number of items as size of folder for local folders
 * @THUNAR_FOLDER_ITEM_COUNT_ALWAYS,       : always show the number of items as the size of the folder
 **/
typedef enum
{
  THUNAR_FOLDER_ITEM_COUNT_NEVER,
  THUNAR_FOLDER_ITEM_COUNT_ONLY_LOCAL,
  THUNAR_FOLDER_ITEM_COUNT_ALWAYS,
} ThunarFolderItemCount;

GType thunar_folder_item_count_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_ENUM_TYPES_H__ */
