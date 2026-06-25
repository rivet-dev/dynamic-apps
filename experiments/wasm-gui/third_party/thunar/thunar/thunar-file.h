/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_FILE_H__
#define __THUNAR_FILE_H__

#include <glib.h>

#include <thunarx/thunarx.h>

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-user.h>

G_BEGIN_DECLS;

typedef struct _ThunarFileClass ThunarFileClass;
typedef struct _ThunarFile      ThunarFile;

#define THUNAR_TYPE_FILE            (thunar_file_get_type ())
#define THUNAR_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FILE, ThunarFile))
#define THUNAR_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FILE, ThunarFileClass))
#define THUNAR_IS_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FILE))
#define THUNAR_IS_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FILE))
#define THUNAR_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_FILE, ThunarFileClass))

/**
 * ThunarFileDateType:
 * @THUNAR_FILE_DATE_ACCESSED : date of last access to the file.
 * @THUNAR_FILE_DATE_CHANGED  : date of last change to the file meta data or the content.
 * @THUNAR_FILE_DATE_CREATED  : date of file creation.
 * @THUNAR_FILE_DATE_MODIFIED : date of last modification of the file's content.
 * @THUNAR_FILE_DATE_DELETED  : date of file deletion.
 *
 * The various dates that can be queried about a #ThunarFile. Note, that not all
 * #ThunarFile implementations support all types listed above. See the documentation
 * of the thunar_file_get_date() method for details.
 **/
typedef enum
{
  THUNAR_FILE_DATE_ACCESSED,
  THUNAR_FILE_DATE_CHANGED,
  THUNAR_FILE_DATE_CREATED,
  THUNAR_FILE_DATE_MODIFIED,
  THUNAR_FILE_DATE_DELETED,
  THUNAR_FILE_RECENCY,
} ThunarFileDateType;

/**
 * ThunarFileIconState:
 * @THUNAR_FILE_ICON_STATE_DEFAULT : the default icon for the file.
 * @THUNAR_FILE_ICON_STATE_DROP    : the drop accept icon for the file.
 * @THUNAR_FILE_ICON_STATE_OPEN    : the folder is expanded.
 *
 * The various file icon states that are used within the file manager
 * views.
 **/
typedef enum /*< enum >*/
{
  THUNAR_FILE_ICON_STATE_DEFAULT,
  THUNAR_FILE_ICON_STATE_DROP,
  THUNAR_FILE_ICON_STATE_OPEN,
} ThunarFileIconState;

/**
 * ThunarFileThumbState:
 * @THUNAR_FILE_THUMB_STATE_UNKNOWN : unknown whether there's a thumbnail.
 * @THUNAR_FILE_THUMB_STATE_NONE    : no thumbnail is available.
 * @THUNAR_FILE_THUMB_STATE_READY   : a thumbnail is available.
 * @THUNAR_FILE_THUMB_STATE_LOADING : a thumbnail is being generated.
 *
 * The state of the thumbnailing for a given #ThunarFile.
 **/
typedef enum /*< flags >*/
{
  THUNAR_FILE_THUMB_STATE_UNKNOWN = 0,
  THUNAR_FILE_THUMB_STATE_NONE    = 1,
  THUNAR_FILE_THUMB_STATE_READY   = 2,
  THUNAR_FILE_THUMB_STATE_LOADING = 3,
} ThunarFileThumbState;



#define THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK "emblem-symbolic-link"
#define THUNAR_FILE_EMBLEM_NAME_CANT_READ     "emblem-noread"
#define THUNAR_FILE_EMBLEM_NAME_CANT_WRITE    "emblem-nowrite"
#define THUNAR_FILE_EMBLEM_NAME_DESKTOP       "emblem-desktop"



/**
 * ThunarFileGetFunc:
 *
 * Callback type for loading #ThunarFile<!---->s asynchronously. If you
 * want to keep the #ThunarFile, you need to ref it, else it will be
 * destroyed.
 **/
typedef void (*ThunarFileGetFunc) (GFile      *location,
                                   ThunarFile *file,
                                   GError     *error,
                                   gpointer    user_data);



GType             thunar_file_get_type                   (void) G_GNUC_CONST;

ThunarFile       *thunar_file_get                        (GFile                  *file,
                                                          GError                **error);
ThunarFile       *thunar_file_get_with_info              (GFile                  *file,
                                                          GFileInfo              *info,
                                                          GFileInfo              *recent_info,
                                                          gboolean                not_mounted);
ThunarFile       *thunar_file_get_for_uri                (const gchar            *uri,
                                                          GError                **error);
void              thunar_file_get_async                  (GFile                  *location,
                                                          GCancellable           *cancellable,
                                                          ThunarFileGetFunc       func,
                                                          gpointer                user_data);

GFile            *thunar_file_get_file                   (const ThunarFile       *file) G_GNUC_PURE;

GFileInfo        *thunar_file_get_info                   (const ThunarFile       *file) G_GNUC_PURE;

ThunarFile       *thunar_file_get_parent                 (const ThunarFile       *file,
                                                          GError                **error);

gboolean          thunar_file_check_loaded               (ThunarFile             *file);

gboolean          thunar_file_execute                    (ThunarFile             *file,
                                                          GFile                  *working_directory,
                                                          gpointer                parent,
                                                          GList                  *path_list,
                                                          const gchar            *startup_id,
                                                          GError                **error);

gboolean          thunar_file_launch                     (ThunarFile             *file,
                                                          gpointer                parent,
                                                          const gchar            *startup_id,
                                                          GError                **error);

gboolean          thunar_file_rename                     (ThunarFile             *file,
                                                          const gchar            *name,
                                                          GCancellable           *cancellable,
                                                          gboolean                called_from_job,
                                                          GError                **error);

GdkDragAction     thunar_file_accepts_drop               (ThunarFile             *file,
                                                          GList                  *path_list,
                                                          GdkDragContext         *context,
                                                          GdkDragAction          *suggested_action_return);

guint64           thunar_file_get_date                   (const ThunarFile       *file,
                                                          ThunarFileDateType      date_type) G_GNUC_PURE;

gchar            *thunar_file_get_date_string            (const ThunarFile       *file,
                                                          ThunarFileDateType      date_type,
                                                          ThunarDateStyle         date_style,
                                                          const gchar            *date_custom_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_mode_string            (const ThunarFile       *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_size_string            (const ThunarFile       *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_size_in_bytes_string   (const ThunarFile       *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_size_string_formatted  (const ThunarFile       *file,
                                                          const gboolean          file_size_binary);
gchar            *thunar_file_get_size_string_long       (const ThunarFile       *file,
                                                          const gboolean          file_size_binary);

GVolume          *thunar_file_get_volume                 (const ThunarFile       *file);

ThunarGroup      *thunar_file_get_group                  (const ThunarFile       *file);
ThunarUser       *thunar_file_get_user                   (const ThunarFile       *file);

const gchar      *thunar_file_get_content_type           (ThunarFile             *file);
gchar            *thunar_file_get_content_type_desc      (ThunarFile             *file);
gboolean          thunar_file_load_content_type          (ThunarFile             *file);
const gchar      *thunar_file_get_symlink_target         (const ThunarFile       *file);
const gchar      *thunar_file_get_basename               (const ThunarFile       *file) G_GNUC_CONST;
gboolean          thunar_file_is_symlink                 (const ThunarFile       *file);
guint64           thunar_file_get_size                   (const ThunarFile       *file);
GAppInfo         *thunar_file_get_default_handler        (const ThunarFile       *file);
GFileType         thunar_file_get_kind                   (const ThunarFile       *file) G_GNUC_PURE;
GFile            *thunar_file_get_target_location        (const ThunarFile       *file);
ThunarFileMode    thunar_file_get_mode                   (const ThunarFile       *file);
gboolean          thunar_file_is_mounted                 (const ThunarFile       *file);
gboolean          thunar_file_exists                     (const ThunarFile       *file);
gboolean          thunar_file_is_directory               (const ThunarFile       *file) G_GNUC_PURE;
gboolean          thunar_file_is_shortcut                (const ThunarFile       *file) G_GNUC_PURE;
gboolean          thunar_file_is_mountable               (const ThunarFile       *file) G_GNUC_PURE;
gboolean          thunar_file_is_local                   (const ThunarFile       *file);
gboolean          thunar_file_is_parent                  (const ThunarFile       *file,
                                                          const ThunarFile       *child);
gboolean          thunar_file_is_gfile_ancestor          (const ThunarFile       *file,
                                                          GFile                  *ancestor);
gboolean          thunar_file_is_ancestor                (const ThunarFile       *file,
                                                          const ThunarFile       *ancestor);
gboolean          thunar_file_can_execute                (ThunarFile             *file);
gboolean          thunar_file_is_writable                (const ThunarFile       *file);
gboolean          thunar_file_is_hidden                  (const ThunarFile       *file);
gboolean          thunar_file_is_home                    (const ThunarFile       *file);
gboolean          thunar_file_is_regular                 (const ThunarFile       *file) G_GNUC_PURE;
gboolean          thunar_file_is_trash                   (const ThunarFile       *file);
gboolean          thunar_file_is_trashed                 (const ThunarFile       *file);
gboolean          thunar_file_is_recent                  (const ThunarFile       *file);
gboolean          thunar_file_is_in_recent               (const ThunarFile       *file);
gboolean          thunar_file_is_desktop_file            (const ThunarFile       *file);
const gchar      *thunar_file_get_display_name           (const ThunarFile       *file) G_GNUC_CONST;

gchar            *thunar_file_get_deletion_date          (const ThunarFile       *file,
                                                          ThunarDateStyle         date_style,
                                                          const gchar            *date_custom_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_recency                (const ThunarFile       *file,
                                                          ThunarDateStyle         date_style,
                                                          const gchar            *date_custom_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
const gchar      *thunar_file_get_original_path          (const ThunarFile       *file);
guint32           thunar_file_get_item_count             (const ThunarFile       *file);

gboolean          thunar_file_is_chmodable               (const ThunarFile       *file);
gboolean          thunar_file_is_renameable              (const ThunarFile       *file);
gboolean          thunar_file_can_be_trashed             (const ThunarFile       *file);


guint             thunar_file_get_file_count             (ThunarFile             *file,
                                                          GCallback               callback,
                                                          gpointer                data);
void              thunar_file_set_file_count             (ThunarFile             *file,
                                                          const guint             count);

GList            *thunar_file_get_emblem_names           (ThunarFile              *file);
void              thunar_file_set_emblem_names           (ThunarFile              *file,
                                                          GList                   *emblem_names);

const gchar      *thunar_file_get_custom_icon            (const ThunarFile        *file);
gboolean          thunar_file_set_custom_icon            (ThunarFile              *file,
                                                          const gchar             *custom_icon,
                                                          GError                 **error);

const gchar     *thunar_file_get_thumbnail_path          (ThunarFile              *file,
                                                          ThunarThumbnailSize      thumbnail_size);
gchar           *thunar_file_get_thumbnail_path_forced   (ThunarFile              *file,
                                                          ThunarThumbnailSize      thumbnail_size);
ThunarFileThumbState thunar_file_get_thumb_state         (const ThunarFile        *file);
void             thunar_file_set_thumb_state             (ThunarFile              *file,
                                                          ThunarFileThumbState     state);
GIcon            *thunar_file_get_preview_icon           (const ThunarFile        *file);
GFilesystemPreviewType thunar_file_get_preview_type      (const ThunarFile *file);
const gchar      *thunar_file_get_icon_name              (ThunarFile              *file,
                                                          ThunarFileIconState      icon_state,
                                                          GtkIconTheme            *icon_theme);
const gchar      *thunar_file_get_device_type            (ThunarFile              *file);

void              thunar_file_watch                      (ThunarFile              *file);
void              thunar_file_unwatch                    (ThunarFile              *file);

gboolean          thunar_file_reload                     (ThunarFile              *file);
void              thunar_file_reload_idle                (ThunarFile              *file);
void              thunar_file_reload_idle_unref          (ThunarFile              *file);
void              thunar_file_reload_parent              (ThunarFile              *file);

void              thunar_file_destroy                    (ThunarFile              *file);

gint              thunar_file_compare_by_type            (ThunarFile              *file_a,
                                                          ThunarFile              *file_b);
gint              thunar_file_compare_by_name            (const ThunarFile        *file_a,
                                                          const ThunarFile        *file_b,
                                                          gboolean                 case_sensitive) G_GNUC_PURE;

ThunarFile       *thunar_file_cache_lookup               (const GFile             *file);
gchar            *thunar_file_cached_display_name        (const GFile             *file);


GList            *thunar_file_list_get_applications      (GList                  *file_list);
GList            *thunar_file_list_to_thunar_g_file_list (GList                  *file_list);

gboolean          thunar_file_is_desktop                 (const ThunarFile *file);

const gchar*      thunar_file_get_metadata_setting       (ThunarFile             *file,
                                                          const gchar            *setting_name);
void              thunar_file_set_metadata_setting       (ThunarFile             *file,
                                                          const gchar            *setting_name,
                                                          const gchar            *setting_value,
                                                          gboolean                async);
void              thunar_file_clear_metadata_setting     (ThunarFile             *file,
                                                          const gchar            *setting_name);
void              thunar_file_clear_directory_specific_settings (ThunarFile      *file);
gboolean          thunar_file_has_directory_specific_settings   (ThunarFile      *file);

/**
 * thunar_file_is_root:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to the root directory.
 *
 * Return value: %TRUE if @file is the root directory.
 **/
#define thunar_file_is_root(file) (thunar_g_file_is_root (thunar_file_get_file (file)))

/**
 * thunar_file_has_parent:
 * @file : a #ThunarFile instance.
 *
 * Checks whether it is possible to determine the parent #ThunarFile
 * for @file.
 *
 * Return value: whether @file has a parent.
 **/
#define thunar_file_has_parent(file) (!thunar_file_is_root (THUNAR_FILE ((file))))

/**
 * thunar_file_dup_uri:
 * @file : a #ThunarFile instance.
 *
 * Returns the URI for the @file. The caller is responsible
 * to free the returned string when no longer needed.
 *
 * Return value: the URI for @file.
 **/
#define thunar_file_dup_uri(file) (g_file_get_uri (thunar_file_get_file (file)))

/**
 * thunar_file_has_uri_scheme:
 * @file       : a #ThunarFile instance.
 * @uri_scheme : a URI scheme string.
 *
 * Checks whether the URI scheme of the file matches @uri_scheme.
 *
 * Return value: TRUE, if the schemes match, FALSE otherwise.
 **/
#define thunar_file_has_uri_scheme(file, uri_scheme) (g_file_has_uri_scheme (thunar_file_get_file (file), (uri_scheme)))

/**
 * thunar_file_changed:
 * @file : a #ThunarFile instance.
 *
 * Emits the ::changed signal on @file. This function is meant to be called
 * by derived classes whenever they notice changes to the @file.
 **/
#define thunar_file_changed(file)                         \
G_STMT_START{                                             \
  thunarx_file_info_changed (THUNARX_FILE_INFO ((file))); \
}G_STMT_END


G_END_DECLS;

#endif /* !__THUNAR_FILE_H__ */
