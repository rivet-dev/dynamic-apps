/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_SBR_REPLACE_RENAMER_H__
#define __THUNAR_SBR_REPLACE_RENAMER_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _ThunarSbrReplaceRenamerClass ThunarSbrReplaceRenamerClass;
typedef struct _ThunarSbrReplaceRenamer      ThunarSbrReplaceRenamer;

#define THUNAR_SBR_TYPE_REPLACE_RENAMER            (thunar_sbr_replace_renamer_get_type ())
#define THUNAR_SBR_REPLACE_RENAMER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_SBR_TYPE_REPLACE_RENAMER, ThunarSbrReplaceRenamer))
#define THUNAR_SBR_REPLACE_RENAMER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_SBR_TYPE_REPLACE_RENAMER, ThunarSbrReplaceRenamerClass))
#define THUNAR_SBR_IS_REPLACE_RENAMER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_SBR_TYPE_REPLACE_RENAMER))
#define THUNAR_SBR_IS_REPLACE_RENAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_SBR_TYPE_REPLACE_RENAMER))
#define THUNAR_SBR_REPLACE_RENAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_SBR_TYPE_REPLACE_RENAMER, ThunarSbrReplaceRenamerClass))

GType                    thunar_sbr_replace_renamer_get_type            (void) G_GNUC_CONST;
void                     thunar_sbr_replace_renamer_register_type       (ThunarxProviderPlugin   *plugin);

ThunarSbrReplaceRenamer *thunar_sbr_replace_renamer_new                 (void) G_GNUC_MALLOC;

gboolean                 thunar_sbr_replace_renamer_get_case_sensitive  (ThunarSbrReplaceRenamer *replace_renamer);
void                     thunar_sbr_replace_renamer_set_case_sensitive  (ThunarSbrReplaceRenamer *replace_renamer,
                                                                         gboolean                 case_sensitive);

const gchar             *thunar_sbr_replace_renamer_get_pattern         (ThunarSbrReplaceRenamer *replace_renamer);
void                     thunar_sbr_replace_renamer_set_pattern         (ThunarSbrReplaceRenamer *replace_renamer,
                                                                         const gchar             *pattern);

gboolean                 thunar_sbr_replace_renamer_get_regexp          (ThunarSbrReplaceRenamer *replace_renamer);
void                     thunar_sbr_replace_renamer_set_regexp          (ThunarSbrReplaceRenamer *replace_renamer,
                                                                         gboolean                 regexp);

const gchar             *thunar_sbr_replace_renamer_get_replacement     (ThunarSbrReplaceRenamer *replace_renamer);
void                     thunar_sbr_replace_renamer_set_replacement     (ThunarSbrReplaceRenamer *replace_renamer,
                                                                         const gchar             *replacement);

G_END_DECLS;

#endif /* !__THUNAR_SBR_REPLACE_RENAMER_H__ */
