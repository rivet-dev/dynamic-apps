/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __EXO_DIE_DESKTOP_MODEL_H__
#define __EXO_DIE_DESKTOP_MODEL_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ExoDieDesktopModelClass ExoDieDesktopModelClass;
typedef struct _ExoDieDesktopModel      ExoDieDesktopModel;

#define EXO_DIE_TYPE_DESKTOP_MODEL            (exo_die_desktop_model_get_type ())
#define EXO_DIE_DESKTOP_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_DIE_TYPE_DESKTOP_MODEL, ExoDieDesktopModel))
#define EXO_DIE_DESKTOP_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_DIE_TYPE_DESKTOP_MODEL, ExoDieDesktopModelClass))
#define EXO_DIE_IS_DESKTOP_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_DIE_TYPE_DESKTOP_MODEL))
#define EXO_DIE_IS_DESKTOP_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_DIE_TYPE_DESKTOP_MODEL))
#define EXO_DIE_DESKTOP_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_DIE_TYPE_DESKTOP_MODEL, ExoDieDesktopModelClass))

/**
 * ExoDieDesktopModelColumn:
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_ABSTRACT : the column with the markup text for the renderer.
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_COMMAND  : the column with the application command.
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_COMMENT  : the column with the application comment.
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_ICON     : the column with the application icon.
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_NAME     : the column with the application name.
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_SNOTIFY  : the column with the applications StartupNotify setting.
 * @EXO_DIE_DESKTOP_MODEL_COLUMN_TERMINAL : the column with the applications Terminal setting.
 *
 * The columns provided by the #ExoDieDesktopModel.
 **/
typedef enum /*< enum >*/
{
  EXO_DIE_DESKTOP_MODEL_COLUMN_ABSTRACT,
  EXO_DIE_DESKTOP_MODEL_COLUMN_COMMAND,
  EXO_DIE_DESKTOP_MODEL_COLUMN_COMMENT,
  EXO_DIE_DESKTOP_MODEL_COLUMN_ICON,
  EXO_DIE_DESKTOP_MODEL_COLUMN_NAME,
  EXO_DIE_DESKTOP_MODEL_COLUMN_SNOTIFY,
  EXO_DIE_DESKTOP_MODEL_COLUMN_TERMINAL,
  EXO_DIE_DESKTOP_MODEL_N_COLUMNS,
} ExoDieDesktopModelColumn;

GType               exo_die_desktop_model_get_type    (void) G_GNUC_CONST;

ExoDieDesktopModel *exo_die_desktop_model_new         (void) G_GNUC_MALLOC;

gboolean            exo_die_desktop_model_match_func  (GtkEntryCompletion *completion,
                                                       const gchar        *key,
                                                       GtkTreeIter        *iter,
                                                       gpointer            user_data);

G_END_DECLS;

#endif /* !__EXO_DIE_DESKTOP_MODEL_H__ */
