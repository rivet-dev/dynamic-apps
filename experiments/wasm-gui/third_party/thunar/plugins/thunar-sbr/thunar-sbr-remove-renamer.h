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

#ifndef __THUNAR_SBR_REMOVE_RENAMER_H__
#define __THUNAR_SBR_REMOVE_RENAMER_H__

#include <thunar-sbr/thunar-sbr-enum-types.h>

G_BEGIN_DECLS;

typedef struct _ThunarSbrRemoveRenamerClass ThunarSbrRemoveRenamerClass;
typedef struct _ThunarSbrRemoveRenamer      ThunarSbrRemoveRenamer;

#define THUNAR_SBR_TYPE_REMOVE_RENAMER            (thunar_sbr_remove_renamer_get_type ())
#define THUNAR_SBR_REMOVE_RENAMER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_SBR_TYPE_REMOVE_RENAMER, ThunarSbrRemoveRenamer))
#define THUNAR_SBR_REMOVE_RENAMER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_SBR_TYPE_REMOVE_RENAMER, ThunarSbrRemoveRenamerClass))
#define THUNAR_SBR_IS_REMOVE_RENAMER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_SBR_TYPE_REMOVE_RENAMER))
#define THUNAR_SBR_IS_REMOVE_RENAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_SBR_TYPE_REMOVE_RENAMER))
#define THUNAR_SBR_REMOVE_RENAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_SBR_TYPE_REMOVE_RENAMER, ThunarSbrRemoveRenamerClass))

GType                   thunar_sbr_remove_renamer_get_type              (void) G_GNUC_CONST;
void                    thunar_sbr_remove_renamer_register_type         (ThunarxProviderPlugin  *plugin);

ThunarSbrRemoveRenamer *thunar_sbr_remove_renamer_new                   (void) G_GNUC_MALLOC;

guint                   thunar_sbr_remove_renamer_get_end_offset        (ThunarSbrRemoveRenamer *remove_renamer);
void                    thunar_sbr_remove_renamer_set_end_offset        (ThunarSbrRemoveRenamer *remove_renamer,
                                                                         guint                   end_offset);

ThunarSbrOffsetMode     thunar_sbr_remove_renamer_get_end_offset_mode   (ThunarSbrRemoveRenamer *remove_renamer);
void                    thunar_sbr_remove_renamer_set_end_offset_mode   (ThunarSbrRemoveRenamer *remove_renamer,
                                                                         ThunarSbrOffsetMode     end_offset_mode);

guint                   thunar_sbr_remove_renamer_get_start_offset      (ThunarSbrRemoveRenamer *remove_renamer);
void                    thunar_sbr_remove_renamer_set_start_offset      (ThunarSbrRemoveRenamer *remove_renamer,
                                                                         guint                   start_offset);

ThunarSbrOffsetMode     thunar_sbr_remove_renamer_get_start_offset_mode (ThunarSbrRemoveRenamer *remove_renamer);
void                    thunar_sbr_remove_renamer_set_start_offset_mode (ThunarSbrRemoveRenamer *remove_renamer,
                                                                         ThunarSbrOffsetMode     start_offset_mode);

G_END_DECLS;

#endif /* !__THUNAR_SBR_REMOVE_RENAMER_H__ */
