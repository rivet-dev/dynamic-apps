/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_COLUMN_MODEL_H__
#define __THUNAR_COLUMN_MODEL_H__

#include <thunar/thunar-enum-types.h>

G_BEGIN_DECLS;

typedef struct _ThunarColumnModelClass ThunarColumnModelClass;
typedef struct _ThunarColumnModel      ThunarColumnModel;

#define THUNAR_TYPE_COLUMN_MODEL            (thunar_column_model_get_type ())
#define THUNAR_COLUMN_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_COLUMN_MODEL, ThunarColumnModel))
#define THUNAR_COLUMN_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_COLUMN_MODEL, ThunarColumnModelClass))
#define THUNAR_IS_COLUMN_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_COLUMN_MODEL))
#define THUNAR_IS_COLUMN_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_COLUMN_MODEL))
#define THUNAR_COLUMN_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_COLUMN_MODEL, ThunarColumnModelClass))

/**
 * ThunarColumnModelColumn:
 * @THUNAR_COLUMN_MODEL_COLUMN_NAME    : the name of the column.
 * @THUNAR_COLUMN_MODEL_COLUMN_MUTABLE : TRUE if the visibility can be changed.
 * @THUNAR_COLUMN_MODEL_COLUMN_VISIBLE : whether the column is visible.
 *
 * The #ThunarColumnModel columns used by the #ThunarColumnEditor.
 **/
typedef enum
{
  THUNAR_COLUMN_MODEL_COLUMN_NAME,
  THUNAR_COLUMN_MODEL_COLUMN_MUTABLE,
  THUNAR_COLUMN_MODEL_COLUMN_VISIBLE,
  THUNAR_COLUMN_MODEL_N_COLUMNS,
} ThunarColumnModelColumn;

GType               thunar_column_model_get_type            (void) G_GNUC_CONST;

ThunarColumnModel  *thunar_column_model_get_default         (void);

void                thunar_column_model_exchange            (ThunarColumnModel *column_model,
                                                             GtkTreeIter       *iter1,
                                                             GtkTreeIter       *iter2);

ThunarColumn        thunar_column_model_get_column_for_iter (ThunarColumnModel *column_model,
                                                             GtkTreeIter       *iter);

const ThunarColumn *thunar_column_model_get_column_order    (ThunarColumnModel *column_model);

const gchar        *thunar_column_model_get_column_name     (ThunarColumnModel *column_model,
                                                             ThunarColumn       column);

gboolean            thunar_column_model_get_column_visible  (ThunarColumnModel *column_model,
                                                             ThunarColumn       column);
void                thunar_column_model_set_column_visible  (ThunarColumnModel *column_model,
                                                             ThunarColumn       column,
                                                             gboolean           visible);

gint                thunar_column_model_get_column_width    (ThunarColumnModel *column_model,
                                                             ThunarColumn       column);
void                thunar_column_model_set_column_width    (ThunarColumnModel *column_model,
                                                             ThunarColumn       column,
                                                             gint               width);

G_END_DECLS;

#endif /* !__THUNAR_COLUMN_MODEL_H__ */
