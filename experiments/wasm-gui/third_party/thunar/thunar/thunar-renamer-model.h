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

#ifndef __THUNAR_RENAMER_MODEL_H__
#define __THUNAR_RENAMER_MODEL_H__

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarRenamerModelClass ThunarRenamerModelClass;
typedef struct _ThunarRenamerModel      ThunarRenamerModel;

#define THUNAR_TYPE_RENAMER_MODEL             (thunar_renamer_model_get_type ())
#define THUNAR_RENAMER_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_RENAMER_MODEL, ThunarRenamerModel))
#define THUNAR_RENAMER_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_RENAMER_MODEL, ThunarRenamerModelClass))
#define THUNAR_IS_RENAMER_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_RENAMER_MODEL))
#define THUNAR_IS_RENAMER_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_RENAMER_MODEL))
#define THUNAR_RENAMER_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_RENAMER_MODEL, ThunarRenamerModelClass))

/**
 * ThunarRenamerModelColumn:
 * @THUNAR_RENAMER_MODEL_COLUMN_CONFLICT        : the column which tells whether there's a name conflict.
 * @THUNAR_RENAMER_MODEL_COLUMN_CONFLICT_WEIGHT : Use to set the text to bold in case of a conflict
 * @THUNAR_RENAMER_MODEL_COLUMN_FILE            : the column with the #ThunarFile.
 * @THUNAR_RENAMER_MODEL_COLUMN_NEWNAME         : the column with the new name.
 * @THUNAR_RENAMER_MODEL_COLUMN_OLDNAME         : the column with the old name.
 *
 * The column ids provided by #ThunarRenamerModel instances.
 **/
typedef enum
{
  THUNAR_RENAMER_MODEL_COLUMN_CONFLICT,
  THUNAR_RENAMER_MODEL_COLUMN_CONFLICT_WEIGHT,
  THUNAR_RENAMER_MODEL_COLUMN_FILE,
  THUNAR_RENAMER_MODEL_COLUMN_NEWNAME,
  THUNAR_RENAMER_MODEL_COLUMN_OLDNAME,
  THUNAR_RENAMER_MODEL_N_COLUMNS,
} ThunarRenamerModelColumn;

GType                thunar_renamer_model_get_type       (void) G_GNUC_CONST;

ThunarRenamerModel  *thunar_renamer_model_new            (void) G_GNUC_MALLOC;

ThunarRenamerMode    thunar_renamer_model_get_mode       (ThunarRenamerModel *renamer_model);

ThunarxRenamer      *thunar_renamer_model_get_renamer    (ThunarRenamerModel *renamer_model);
void                 thunar_renamer_model_set_renamer    (ThunarRenamerModel *renamer_model,
                                                          ThunarxRenamer     *renamer);

void                 thunar_renamer_model_insert         (ThunarRenamerModel *renamer_model,
                                                          ThunarFile         *file,
                                                          gint                position);
void                 thunar_renamer_model_reorder        (ThunarRenamerModel *renamer_model,
                                                          GList              *tree_paths,
                                                          gint                position);
void                 thunar_renamer_model_sort           (ThunarRenamerModel *renamer_model,
                                                          GtkSortType         sort_order);
void                 thunar_renamer_model_clear          (ThunarRenamerModel *renamer_model);
void                 thunar_renamer_model_remove         (ThunarRenamerModel *renamer_model,
                                                          GtkTreePath        *path);


/**
 * thunar_renamer_model_append:
 * @model : a #ThunarRenamerModel.
 * @file  : a #ThunarFile instance.
 *
 * Appends the @file to the @renamer_model.
 **/
#define thunar_renamer_model_append(model,file) thunar_renamer_model_insert (model, file, -1)

G_END_DECLS;

#endif /* !__THUNAR_RENAMER_MODEL_H__ */
