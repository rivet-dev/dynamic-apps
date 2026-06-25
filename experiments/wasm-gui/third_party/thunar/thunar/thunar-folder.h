/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_FOLDER_H__
#define __THUNAR_FOLDER_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarFolderClass ThunarFolderClass;
typedef struct _ThunarFolder      ThunarFolder;

#define THUNAR_TYPE_FOLDER            (thunar_folder_get_type ())
#define THUNAR_FOLDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FOLDER, ThunarFolder))
#define THUNAR_FOLDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FOLDER, ThunarFolderClass))
#define THUNAR_IS_FOLDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FOLDER))
#define THUNAR_IS_FOLDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FOLDER))
#define THUNAR_FOLDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_FOLDER, ThunarFolderClass))

GType         thunar_folder_get_type               (void) G_GNUC_CONST;

ThunarFolder *thunar_folder_get_for_file           (ThunarFile         *file);

ThunarFile   *thunar_folder_get_corresponding_file (const ThunarFolder *folder);
GList        *thunar_folder_get_files              (const ThunarFolder *folder);
gboolean      thunar_folder_get_loading            (const ThunarFolder *folder);
gboolean      thunar_folder_has_folder_monitor     (const ThunarFolder *folder);

void          thunar_folder_reload                 (ThunarFolder       *folder,
                                                    gboolean            reload_info);

G_END_DECLS;

#endif /* !__THUNAR_FOLDER_H__ */
