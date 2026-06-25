/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_APPFINDER_CATEGORY_MODEL_H__
#define __XFCE_APPFINDER_CATEGORY_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceAppfinderCategoryModelClass XfceAppfinderCategoryModelClass;
typedef struct _XfceAppfinderCategoryModel      XfceAppfinderCategoryModel;

#define XFCE_TYPE_APPFINDER_CATEGORY_MODEL            (xfce_appfinder_category_model_get_type ())
#define XFCE_APPFINDER_CATEGORY_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_APPFINDER_CATEGORY_MODEL, XfceAppfinderCategoryModel))
#define XFCE_APPFINDER_CATEGORY_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_APPFINDER_CATEGORY_MODEL, XfceAppfinderCategoryModelClass))
#define XFCE_IS_APPFINDER_CATEGORY_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_APPFINDER_CATEGORY_MODEL))
#define XFCE_IS_APPFINDER_CATEGORY_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_APPFINDER_CATEGORY_MODEL))
#define XFCE_APPFINDER_CATEGORY_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_APPFINDER_CATEGORY_MODEL, XfceAppfinderCategoryModelClass))



enum
{
  XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME,
  XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_ICON,
  XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_DIRECTORY,
  XFCE_APPFINDER_CATEGORY_MODEL_N_COLUMNS,
};



GType                       xfce_appfinder_category_model_get_type           (void) G_GNUC_CONST;

XfceAppfinderCategoryModel *xfce_appfinder_category_model_new                (gint                        scale_factor) G_GNUC_MALLOC;

void                        xfce_appfinder_category_model_set_categories     (XfceAppfinderCategoryModel *model,
                                                                              GSList                     *categories);

gboolean                    xfce_appfinder_category_model_row_separator_func (GtkTreeModel               *tree_model,
                                                                              GtkTreeIter                *iter,
                                                                              gpointer                    user_data);

void                        xfce_appfinder_category_model_icon_theme_changed (XfceAppfinderCategoryModel *model);

GtkTreePath                *xfce_appfinder_category_model_find_category      (XfceAppfinderCategoryModel *model,
                                                                              const gchar                *name);

G_END_DECLS

#endif /* !__XFCE_APPFINDER_CATEGORY_MODEL_H__ */
