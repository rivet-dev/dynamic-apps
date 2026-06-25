/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __THUNAR_UCA_MODEL_H__
#define __THUNAR_UCA_MODEL_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _ThunarUcaModelClass ThunarUcaModelClass;
typedef struct _ThunarUcaModel      ThunarUcaModel;

#define THUNAR_UCA_TYPE_MODEL             (thunar_uca_model_get_type ())
#define THUNAR_UCA_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_UCA_TYPE_MODEL, ThunarUcaModel))
#define THUNAR_UCA_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_UCA_TYPE_MODEL, ThunarUcaModelClass))
#define THUNAR_UCA_IS_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_UCA_TYPE_MODEL))
#define THUNAR_UCA_IS_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_UCA_TYPE_MODEL))
#define THUNAR_UCA_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_UCA_TYPE_MODEL, ThunarUcaModelClass))

typedef enum
{
  THUNAR_UCA_MODEL_COLUMN_NAME,
  THUNAR_UCA_MODEL_COLUMN_SUB_MENU,
  THUNAR_UCA_MODEL_COLUMN_DESCRIPTION,
  THUNAR_UCA_MODEL_COLUMN_GICON,
  THUNAR_UCA_MODEL_COLUMN_ICON_NAME,
  THUNAR_UCA_MODEL_COLUMN_UNIQUE_ID,
  THUNAR_UCA_MODEL_COLUMN_COMMAND,
  THUNAR_UCA_MODEL_COLUMN_STARTUP_NOTIFY,
  THUNAR_UCA_MODEL_COLUMN_PATTERNS,
  THUNAR_UCA_MODEL_COLUMN_RANGE,
  THUNAR_UCA_MODEL_COLUMN_TYPES,
  THUNAR_UCA_MODEL_COLUMN_STOCK_LABEL,
  THUNAR_UCA_MODEL_N_COLUMNS,
} ThunarUcaModelColumn;

/**
 * ThunarUcaTypes:
 * @THUNAR_UCA_TYPE_DIRECTORIES : directories.
 * @THUNAR_UCA_TYPE_AUDIO_FILES : audio files.
 * @THUNAR_UCA_TYPE_IMAGE_FILES : image files.
 * @THUNAR_UCA_TYPE_OTHER_FILES : other files.
 * @THUNAR_UCA_TYPE_TEXT_FILES  : text files.
 * @THUNAR_UCA_TYPE_VIDEO_FILES : video files.
 **/
typedef enum /*< flags >*/
{
  THUNAR_UCA_TYPE_DIRECTORIES = 1 << 0,
  THUNAR_UCA_TYPE_AUDIO_FILES = 1 << 1,
  THUNAR_UCA_TYPE_IMAGE_FILES = 1 << 2,
  THUNAR_UCA_TYPE_OTHER_FILES = 1 << 3,
  THUNAR_UCA_TYPE_TEXT_FILES  = 1 << 4,
  THUNAR_UCA_TYPE_VIDEO_FILES = 1 << 5,
} ThunarUcaTypes;

GType           thunar_uca_model_get_type       (void) G_GNUC_CONST;
void            thunar_uca_model_register_type  (ThunarxProviderPlugin  *plugin);

ThunarUcaModel *thunar_uca_model_get_default    (void);

GList          *thunar_uca_model_match          (ThunarUcaModel         *uca_model,
                                                 GList                  *file_infos);

void            thunar_uca_model_append         (ThunarUcaModel         *uca_model,
                                                 GtkTreeIter            *iter);

void            thunar_uca_model_exchange       (ThunarUcaModel         *uca_model,
                                                 GtkTreeIter            *iter_a,
                                                 GtkTreeIter            *iter_b);

void            thunar_uca_model_remove         (ThunarUcaModel         *uca_model,
                                                 GtkTreeIter            *iter);

void            thunar_uca_model_update         (ThunarUcaModel         *uca_model,
                                                 GtkTreeIter            *iter,
                                                 const gchar            *name,
                                                 const gchar            *submenu,
                                                 const gchar            *unique_id,
                                                 const gchar            *description,
                                                 const gchar            *icon,
                                                 const gchar            *command,
                                                 gboolean                startup_notify,
                                                 const gchar            *patterns,
                                                 const gchar            *range,
                                                 ThunarUcaTypes          types,
                                                 guint                   accel_key,
                                                 GdkModifierType         accel_mods);

gboolean        thunar_uca_model_save           (ThunarUcaModel         *uca_model,
                                                 GError                **error);

gboolean        thunar_uca_model_parse_argv     (ThunarUcaModel         *uca_model,
                                                 GtkTreeIter            *iter,
                                                 GList                  *file_infos,
                                                 gint                   *argcp,
                                                 gchar                ***argvp,
                                                 GError                **error);

G_END_DECLS;

#endif /* !__THUNAR_UCA_MODEL_H__ */
