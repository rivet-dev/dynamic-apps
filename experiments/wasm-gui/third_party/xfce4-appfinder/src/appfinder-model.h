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

#ifndef __XFCE_APPFINDER_MODEL_H__
#define __XFCE_APPFINDER_MODEL_H__

#include <gtk/gtk.h>
#include <garcon/garcon.h>

G_BEGIN_DECLS

typedef struct _XfceAppfinderModelClass XfceAppfinderModelClass;
typedef struct _XfceAppfinderModel      XfceAppfinderModel;

#define XFCE_TYPE_APPFINDER_MODEL            (xfce_appfinder_model_get_type ())
#define XFCE_APPFINDER_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_APPFINDER_MODEL, XfceAppfinderModel))
#define XFCE_APPFINDER_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_APPFINDER_MODEL, XfceAppfinderModelClass))
#define XFCE_IS_APPFINDER_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_APPFINDER_MODEL))
#define XFCE_IS_APPFINDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_APPFINDER_MODEL))
#define XFCE_APPFINDER_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_APPFINDER_MODEL, XfceAppfinderModelClass))

enum
{
  XFCE_APPFINDER_MODEL_COLUMN_ABSTRACT,
  XFCE_APPFINDER_MODEL_COLUMN_TITLE,
  XFCE_APPFINDER_MODEL_COLUMN_ICON,
  XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE,
  XFCE_APPFINDER_MODEL_COLUMN_PIXBUF,
  XFCE_APPFINDER_MODEL_COLUMN_COMMAND,
  XFCE_APPFINDER_MODEL_COLUMN_URI,
  XFCE_APPFINDER_MODEL_COLUMN_BOOKMARK,
  XFCE_APPFINDER_MODEL_COLUMN_FREQUENCY,
  XFCE_APPFINDER_MODEL_COLUMN_RECENCY,
  XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP,
  XFCE_APPFINDER_MODEL_N_COLUMNS,
};

typedef enum
{
  XFCE_APPFINDER_ICON_SIZE_SMALLEST, /* 16 */
  XFCE_APPFINDER_ICON_SIZE_SMALLER,  /* 24 */
  XFCE_APPFINDER_ICON_SIZE_SMALL,    /* 36 */
  XFCE_APPFINDER_ICON_SIZE_NORMAL,   /* 48 */
  XFCE_APPFINDER_ICON_SIZE_LARGE,    /* 64 */
  XFCE_APPFINDER_ICON_SIZE_LARGER,   /* 96 */
  XFCE_APPFINDER_ICON_SIZE_LARGEST   /* 128 */
}
XfceAppfinderIconSize;

#define XFCE_APPFINDER_ICON_SIZE_DEFAULT_CATEGORY XFCE_APPFINDER_ICON_SIZE_SMALLER
#define XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM     XFCE_APPFINDER_ICON_SIZE_SMALL
#define XFCE_APPFINDER_ICON_SIZE_48               XFCE_APPFINDER_ICON_SIZE_NORMAL



GType                xfce_appfinder_model_get_type               (void) G_GNUC_CONST;

XfceAppfinderModel  *xfce_appfinder_model_get                    (gboolean                   sort_by_frecency,
                                                                  gint                       scale_factor) G_GNUC_MALLOC;

GSList              *xfce_appfinder_model_get_categories         (XfceAppfinderModel        *model);

gboolean             xfce_appfinder_model_get_visible            (XfceAppfinderModel        *model,
                                                                  const GtkTreeIter         *iter,
                                                                  const GarconMenuDirectory *category,
                                                                  const gchar               *string,
                                                                  const gchar               *string_casefold);

gboolean             xfce_appfinder_model_get_visible_command    (XfceAppfinderModel        *model,
                                                                  const GtkTreeIter         *iter,
                                                                  const gchar               *string);

gboolean             xfce_appfinder_model_execute                (XfceAppfinderModel        *model,
                                                                  const GtkTreeIter         *iter,
                                                                  GdkScreen                 *screen,
                                                                  gboolean                  *is_regular_command,
                                                                  GError                   **error);

GdkPixbuf           *xfce_appfinder_model_load_pixbuf            (const gchar               *icon_name,
                                                                  XfceAppfinderIconSize      icon_size,
                                                                  gint                       scale_factor) G_GNUC_MALLOC;

gboolean             xfce_appfinder_model_save_command           (XfceAppfinderModel        *model,
                                                                  const gchar               *command,
                                                                  GError                   **error);

cairo_surface_t     *xfce_appfinder_model_get_icon_for_command   (XfceAppfinderModel        *model,
                                                                  const gchar               *command);

void                 xfce_appfinder_model_icon_theme_changed     (XfceAppfinderModel        *model);

void                 xfce_appfinder_model_history_clear          (XfceAppfinderModel        *model);

gboolean             xfce_appfinder_model_bookmark_toggle        (XfceAppfinderModel        *model,
                                                                  const gchar               *desktop_id,
                                                                  GError                   **error);

GarconMenuDirectory *xfce_appfinder_model_get_command_category   (void);

GarconMenuDirectory *xfce_appfinder_model_get_bookmarks_category (void);
void                 xfce_appfinder_model_update_frecency        (XfceAppfinderModel       *model,
                                                                  const gchar              *desktop_id,
                                                                  GError                  **error);
guint                xfce_appfinder_model_calculate_frecency     (guint                     frequency,
                                                                  guint64                   recency);

G_END_DECLS

#endif /* !__XFCE_APPFINDER_MODEL_H__ */
