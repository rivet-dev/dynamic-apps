/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#if !defined (EXO_COMPILATION)
#error "Only <exo/exo.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __EXO_ICON_CHOOSER_MODEL_H__
#define __EXO_ICON_CHOOSER_MODEL_H__

#include <exo/exo-config.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ExoIconChooserModelClass ExoIconChooserModelClass;
typedef struct _ExoIconChooserModel      ExoIconChooserModel;

#define EXO_TYPE_ICON_CHOOSER_MODEL             (exo_icon_chooser_model_get_type ())
#define EXO_ICON_CHOOSER_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_ICON_CHOOSER_MODEL, ExoIconChooserModel))
#define EXO_ICON_CHOOSER_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_ICON_CHOOSER_MODEL, ExoIconChooserModelClass))
#define EXO_IS_ICON_CHOOSER_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_ICON_CHOOSER_MODEL))
#define EXO_IS_ICON_CHOOSER_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_ICON_CHOOSER_MODEL))
#define EXO_ICON_CHOOSER_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_ICON_CHOOSER_MODEL, ExoIconChooserModelClass))

/**
 * ExoIconChooserContexts:
 *
 * The list of default contexts for the icon themes
 * according to the Icon Naming Spec, Version 0.7.
 **/
typedef enum
{
  /* the contexts provided by the model */
  EXO_ICON_CHOOSER_CONTEXT_ACTIONS,
  EXO_ICON_CHOOSER_CONTEXT_ANIMATIONS,
  EXO_ICON_CHOOSER_CONTEXT_APPLICATIONS,
  EXO_ICON_CHOOSER_CONTEXT_CATEGORIES,
  EXO_ICON_CHOOSER_CONTEXT_DEVICES,
  EXO_ICON_CHOOSER_CONTEXT_EMBLEMS,
  EXO_ICON_CHOOSER_CONTEXT_EMOTES,
  EXO_ICON_CHOOSER_CONTEXT_MIME_TYPES,
  EXO_ICON_CHOOSER_CONTEXT_PLACES,
  EXO_ICON_CHOOSER_CONTEXT_STATUS,
  EXO_ICON_CHOOSER_CONTEXT_STOCK,
  EXO_ICON_CHOOSER_CONTEXT_OTHER,
  EXO_ICON_CHOOSER_N_CONTEXTS,

  /* not provided by the model (plus separators before them) */
  EXO_ICON_CHOOSER_CONTEXT_ALL     = EXO_ICON_CHOOSER_CONTEXT_OTHER + 2,
  EXO_ICON_CHOOSER_CONTEXT_FILE    = EXO_ICON_CHOOSER_CONTEXT_OTHER + 4,
  EXO_ICON_CHOOSER_CONTEXT_NO_ICON = EXO_ICON_CHOOSER_CONTEXT_OTHER + 6,
} ExoIconChooserContext;

/**
 * ExoIconChooserModelColumns:
 * @EXO_ICON_CHOOSER_MODEL_COLUMN_CONTEXT      : the context of the icon.
 * @EXO_ICON_CHOOSER_MODEL_COLUMN_ICON_NAME    : the name of the icon.
 * @EXO_ICON_CHOOSER_MODEL_N_COLUMNS           : the number of columns.
 *
 * The columns provided by the #ExoIconChooserModel.
 **/
typedef enum
{
  EXO_ICON_CHOOSER_MODEL_COLUMN_CONTEXT,
  EXO_ICON_CHOOSER_MODEL_COLUMN_ICON_NAME,
  EXO_ICON_CHOOSER_MODEL_N_COLUMNS,
} ExoIconChooserModelColumn;

G_GNUC_INTERNAL GType                  exo_icon_chooser_model_get_type                (void) G_GNUC_CONST;

G_GNUC_INTERNAL ExoIconChooserModel   *_exo_icon_chooser_model_get_for_widget         (GtkWidget           *widget) G_GNUC_WARN_UNUSED_RESULT;
G_GNUC_INTERNAL ExoIconChooserModel   *_exo_icon_chooser_model_get_for_icon_theme     (GtkIconTheme        *icon_theme) G_GNUC_WARN_UNUSED_RESULT;

G_GNUC_INTERNAL gboolean               _exo_icon_chooser_model_get_iter_for_icon_name (ExoIconChooserModel *model,
                                                                                       GtkTreeIter         *iter,
                                                                                       const gchar         *icon_name);

G_END_DECLS

#endif /* !__EXO_ICON_CHOOSER_MODEL_H__ */
