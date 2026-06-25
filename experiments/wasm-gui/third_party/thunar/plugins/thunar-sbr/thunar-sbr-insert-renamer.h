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

#ifndef __THUNAR_SBR_INSERT_RENAMER_H__
#define __THUNAR_SBR_INSERT_RENAMER_H__

#include <thunar-sbr/thunar-sbr-enum-types.h>

G_BEGIN_DECLS;

typedef struct _ThunarSbrInsertRenamerClass ThunarSbrInsertRenamerClass;
typedef struct _ThunarSbrInsertRenamer      ThunarSbrInsertRenamer;

#define THUNAR_SBR_TYPE_INSERT_RENAMER            (thunar_sbr_insert_renamer_get_type ())
#define THUNAR_SBR_INSERT_RENAMER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_SBR_TYPE_INSERT_RENAMER, ThunarSbrInsertRenamer))
#define THUNAR_SBR_INSERT_RENAMER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_SBR_TYPE_INSERT_RENAMER, ThunarSbrInsertRenamerClass))
#define THUNAR_SBR_IS_INSERT_RENAMER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_SBR_TYPE_INSERT_RENAMER))
#define THUNAR_SBR_IS_INSERT_RENAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_SBR_TYPE_INSERT_RENAMER))
#define THUNAR_SBR_INSERT_RENAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_SBR_TYPE_INSERT_RENAMER, ThunarSbrInsertRenamerClass))

GType                   thunar_sbr_insert_renamer_get_type        (void) G_GNUC_CONST;
void                    thunar_sbr_insert_renamer_register_type   (ThunarxProviderPlugin  *plugin);

ThunarSbrInsertRenamer *thunar_sbr_insert_renamer_new             (void) G_GNUC_MALLOC;

ThunarSbrInsertMode     thunar_sbr_insert_renamer_get_mode        (ThunarSbrInsertRenamer *insert_renamer);
void                    thunar_sbr_insert_renamer_set_mode        (ThunarSbrInsertRenamer *insert_renamer,
                                                                   ThunarSbrInsertMode     mode);

guint                   thunar_sbr_insert_renamer_get_offset      (ThunarSbrInsertRenamer *insert_renamer);
void                    thunar_sbr_insert_renamer_set_offset      (ThunarSbrInsertRenamer *insert_renamer,
                                                                   guint                   offset);

ThunarSbrOffsetMode     thunar_sbr_insert_renamer_get_offset_mode (ThunarSbrInsertRenamer *insert_renamer);
void                    thunar_sbr_insert_renamer_set_offset_mode (ThunarSbrInsertRenamer *insert_renamer,
                                                                   ThunarSbrOffsetMode     offset_mode);

const gchar            *thunar_sbr_insert_renamer_get_text        (ThunarSbrInsertRenamer *insert_renamer);
void                    thunar_sbr_insert_renamer_set_text        (ThunarSbrInsertRenamer *insert_renamer,
                                                                   const gchar            *text);

G_END_DECLS;

#endif /* !__THUNAR_SBR_INSERT_RENAMER_H__ */
