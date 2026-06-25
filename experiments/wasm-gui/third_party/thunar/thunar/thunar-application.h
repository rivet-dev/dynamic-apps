/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005      Jeff Franks <jcfranks@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_APPLICATION_H__
#define __THUNAR_APPLICATION_H__

#include <thunar/thunar-job-operation.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-thumbnail-cache.h>

G_BEGIN_DECLS;

typedef struct _ThunarApplicationClass ThunarApplicationClass;
typedef struct _ThunarApplication      ThunarApplication;

typedef enum
{
  THUNAR_APPLICATION_LAUNCH_FILES,
  THUNAR_APPLICATION_SELECT_FILES
} ThunarApplicationProcessAction;

#define THUNAR_TYPE_APPLICATION             (thunar_application_get_type ())
#define THUNAR_APPLICATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_APPLICATION, ThunarApplication))
#define THUNAR_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_APPLICATION, ThunarApplicationClass))
#define THUNAR_IS_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_APPLICATION))
#define THUNAR_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_APPLICATION))
#define THUNAR_APPLICATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_APPLICATION, ThunarApplicationClass))

GType                 thunar_application_get_type                   (void) G_GNUC_CONST;

ThunarApplication    *thunar_application_get                        (void);

void                  thunar_application_quit                       (ThunarApplication *application);

gboolean              thunar_application_get_daemon                 (ThunarApplication *application);
void                  thunar_application_set_daemon                 (ThunarApplication *application,
                                                                     gboolean           daemon);

GList                *thunar_application_get_windows                (ThunarApplication *application);

gboolean              thunar_application_has_windows                (ThunarApplication *application);
void                  thunar_application_close_all_windows          (ThunarApplication *application);
void                  thunar_application_take_window                (ThunarApplication *application,
                                                                     GtkWindow         *window);

GtkWidget            *thunar_application_open_window                (ThunarApplication *application,
                                                                     ThunarFile        *directory,
                                                                     GdkScreen         *screen,
                                                                     const gchar       *startup_id,
                                                                     gboolean           force_new_window);

gboolean              thunar_application_bulk_rename                (ThunarApplication *application,
                                                                     const gchar       *working_directory,
                                                                     gchar            **filenames,
                                                                     gboolean           standalone,
                                                                     GdkScreen         *screen,
                                                                     const gchar       *startup_id,
                                                                     GError           **error);

gboolean              thunar_application_process_filenames          (ThunarApplication *application,
                                                                     const gchar       *working_directory,
                                                                     gchar            **filenames,
                                                                     GdkScreen         *screen,
                                                                     const gchar       *startup_id,
                                                                     GError           **error,
                                                                     ThunarApplicationProcessAction action);

void                  thunar_application_rename_file                (ThunarApplication      *application,
                                                                     ThunarFile             *file,
                                                                     GdkScreen              *screen,
                                                                     const gchar            *startup_id,
                                                                     ThunarOperationLogMode  log_mode);

void                  thunar_application_create_file                (ThunarApplication      *application,
                                                                     ThunarFile             *parent_directory,
                                                                     const gchar            *content_type,
                                                                     GdkScreen              *screen,
                                                                     const gchar            *startup_id,
                                                                     ThunarOperationLogMode  log_mode);

void                  thunar_application_create_file_from_template (ThunarApplication       *application,
                                                                    ThunarFile              *parent_directory,
                                                                    ThunarFile              *template_file,
                                                                    GdkScreen               *screen,
                                                                    const gchar             *startup_id,
                                                                    ThunarOperationLogMode   log_mode);

void                  thunar_application_copy_to                   (ThunarApplication       *application,
                                                                    gpointer                 parent,
                                                                    GList                   *source_file_list,
                                                                    GList                   *target_file_list,
                                                                    ThunarOperationLogMode   log_mode,
                                                                    GClosure                *new_files_closure);

void                  thunar_application_copy_into                 (ThunarApplication       *application,
                                                                    gpointer                 parent,
                                                                    GList                   *source_file_list,
                                                                    GFile                   *target_file,
                                                                    ThunarOperationLogMode   log_mode,
                                                                    GClosure                *new_files_closure);

void                  thunar_application_link_into                 (ThunarApplication      *application,
                                                                    gpointer                parent,
                                                                    GList                  *source_file_list,
                                                                    GFile                  *target_file,
                                                                    ThunarOperationLogMode  log_mode,
                                                                    GClosure                *new_files_closure);

void                  thunar_application_move_into                 (ThunarApplication      *application,
                                                                    gpointer                parent,
                                                                    GList                  *source_file_list,
                                                                    GFile                  *target_file,
                                                                    ThunarOperationLogMode  log_mode,
                                                                    GClosure               *new_files_closure);

void                  thunar_application_move_files                (ThunarApplication       *application,
                                                                    gpointer                 parent,
                                                                    GList                   *source_file_list,
                                                                    GList                   *target_file_list,
                                                                    ThunarOperationLogMode   log_mode,
                                                                    GClosure                *new_files_closure);

gboolean              thunar_application_unlink_files              (ThunarApplication       *application,
                                                                    gpointer                 parent,
                                                                    GList                   *file_list,
                                                                    gboolean                 permanently,
                                                                    gboolean                 warn);


void                  thunar_application_trash                     (ThunarApplication       *application,
                                                                    gpointer                 parent,
                                                                    GList                   *file_list,
                                                                    ThunarOperationLogMode   log_mode);

void                  thunar_application_creat                     (ThunarApplication        *application,
                                                                    gpointer                  parent,
                                                                    GList                    *file_list,
                                                                    GFile                    *template_file,
                                                                    GClosure                 *new_files_closure,
                                                                    ThunarOperationLogMode    log_mode);

void                  thunar_application_mkdir                     (ThunarApplication        *application,
                                                                    gpointer                  parent,
                                                                    GList                    *file_list,
                                                                    GClosure                 *new_files_closure,
                                                                    ThunarOperationLogMode    log_mode);

void                  thunar_application_empty_trash               (ThunarApplication *application,
                                                                    gpointer           parent,
                                                                    const gchar       *startup_id);

void                  thunar_application_restore_files             (ThunarApplication *application,
                                                                    gpointer           parent,
                                                                    GList             *trash_file_list,
                                                                    GClosure          *new_files_closure);

ThunarThumbnailCache *thunar_application_get_thumbnail_cache       (ThunarApplication *application);

G_END_DECLS;

#endif /* !__THUNAR_APPLICATION_H__ */
