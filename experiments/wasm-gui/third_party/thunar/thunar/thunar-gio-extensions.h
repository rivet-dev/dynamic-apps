/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_GIO_EXTENSIONS_H__
#define __THUNAR_GIO_EXTENSIONS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

GFile       *thunar_g_file_new_for_home             (void);
GFile       *thunar_g_file_new_for_root             (void);
GFile       *thunar_g_file_new_for_recent           (void);
GFile       *thunar_g_file_new_for_trash            (void);
GFile       *thunar_g_file_new_for_desktop          (void);
GFile       *thunar_g_file_new_for_computer         (void);
GFile       *thunar_g_file_new_for_network          (void);
GFile       *thunar_g_file_new_for_bookmarks        (void);

GFile       *thunar_g_file_resolve_symlink          (GFile                *file);

gboolean     thunar_g_file_is_root                  (GFile                *file);
gboolean     thunar_g_file_is_trashed               (GFile                *file);
gboolean     thunar_g_file_is_in_recent             (GFile                *file);
gboolean     thunar_g_file_is_home                  (GFile                *file);
gboolean     thunar_g_file_is_trash                 (GFile                *file);
gboolean     thunar_g_file_is_recent                (GFile                *file);
gboolean     thunar_g_file_is_computer              (GFile                *file);
gboolean     thunar_g_file_is_network               (GFile                *file);

GKeyFile    *thunar_g_file_query_key_file           (GFile                *file,
                                                     GCancellable         *cancellable,
                                                     GError              **error);
gboolean     thunar_g_file_write_key_file           (GFile                *file,
                                                     GKeyFile             *key_file,
                                                     GCancellable         *cancellable,
                                                     GError              **error);

gchar       *thunar_g_file_get_location             (GFile                *file);

const gchar *thunar_g_file_guess_device_type        (GFile                *file);
gchar       *thunar_g_file_get_display_name         (GFile                *file);

gchar       *thunar_g_file_get_display_name_remote  (GFile                *file);

gboolean     thunar_g_vfs_is_uri_scheme_supported   (const gchar          *scheme);

gboolean     thunar_g_file_get_free_space           (GFile                *file,
                                                     guint64              *fs_free_return,
                                                     guint64              *fs_size_return);

gchar       *thunar_g_file_get_free_space_string    (GFile                *file,
                                                     gboolean              file_size_binary);

gboolean     thunar_g_file_copy                     (GFile                *source,
                                                     GFile                *destination,
                                                     GFileCopyFlags        flags,
                                                     gboolean              use_partial,
                                                     GCancellable         *cancellable,
                                                     GFileProgressCallback progress_callback,
                                                     gpointer              progress_callback_data,
                                                     GError              **error);

gboolean     thunar_g_file_compare_checksum         (GFile                *file_a,
                                                     GFile                *file_b,
                                                     GCancellable         *cancellable,
                                                     GError              **error);

/**
 * THUNAR_TYPE_G_FILE_LIST:
 *
 * Returns the type ID for #GList<!---->s of #GFile<!---->s which is a
 * boxed type.
 **/
#define THUNAR_TYPE_G_FILE_LIST (thunar_g_file_list_get_type ())

GType        thunar_g_file_list_get_type               (void);

GList       *thunar_g_file_list_new_from_string        (const gchar       *string);
gchar      **thunar_g_file_list_to_stringv             (GList             *list);
GList       *thunar_g_file_list_get_parents            (GList             *list);
gboolean     thunar_g_file_is_descendant               (GFile             *descendant,
                                                        GFile             *ancestor);


/* deep copy jobs for GLists */
#define      thunar_g_list_append_deep(list,object)    g_list_append (list, g_object_ref (G_OBJECT (object)))
#define      thunar_g_list_prepend_deep(list,object)   g_list_prepend (list, g_object_ref (G_OBJECT (object)))
#define      thunar_g_list_copy_deep                   thunarx_file_info_list_copy
#define      thunar_g_list_free_full                   thunarx_file_info_list_free

gboolean     thunar_g_app_info_launch                  (GAppInfo          *info,
                                                        GFile             *working_directory,
                                                        GList             *path_list,
                                                        GAppLaunchContext *context,
                                                        GError           **error);

gboolean     thunar_g_app_info_should_show             (GAppInfo          *info);

gboolean     thunar_g_vfs_metadata_is_supported        (void);

gboolean     thunar_g_file_is_on_local_device          (GFile             *file);
gboolean     thunar_g_file_set_executable_flags        (GFile             *file,
                                                        GError           **error);
gboolean     thunar_g_file_is_in_xdg_data_dir          (GFile             *file);
gboolean     thunar_g_file_is_desktop_file             (GFile             *file);
char        *thunar_g_file_get_link_path_for_symlink   (GFile             *file_to_link,
                                                        GFile             *symlink);
G_END_DECLS

#endif /* !__THUNAR_GIO_EXTENSIONS_H__ */
