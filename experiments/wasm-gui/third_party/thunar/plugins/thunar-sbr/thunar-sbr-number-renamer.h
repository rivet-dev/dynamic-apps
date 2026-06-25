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

#ifndef __THUNAR_SBR_NUMBER_RENAMER_H__
#define __THUNAR_SBR_NUMBER_RENAMER_H__

#include <thunar-sbr/thunar-sbr-enum-types.h>

G_BEGIN_DECLS;

typedef struct _ThunarSbrNumberRenamerClass ThunarSbrNumberRenamerClass;
typedef struct _ThunarSbrNumberRenamer      ThunarSbrNumberRenamer;

#define THUNAR_SBR_TYPE_NUMBER_RENAMER            (thunar_sbr_number_renamer_get_type ())
#define THUNAR_SBR_NUMBER_RENAMER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_SBR_TYPE_NUMBER_RENAMER, ThunarSbrNumberRenamer))
#define THUNAR_SBR_NUMBER_RENAMER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_SBR_TYPE_NUMBER_RENAMER, ThunarSbrNumberRenamerClass))
#define THUNAR_SBR_IS_NUMBER_RENAMER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_SBR_TYPE_NUMBER_RENAMER))
#define THUNAR_SBR_IS_NUMBER_RENAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_SBR_TYPE_NUMBER_RENAMER))
#define THUNAR_SBR_NUMBER_RENAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_SBR_TYPE_NUMBER_RENAMER, ThunarSbrNumberRenamerClass))

GType                   thunar_sbr_number_renamer_get_type        (void) G_GNUC_CONST;
void                    thunar_sbr_number_renamer_register_type   (ThunarxProviderPlugin  *plugin);

ThunarSbrNumberRenamer *thunar_sbr_number_renamer_new             (void) G_GNUC_MALLOC;

ThunarSbrNumberMode     thunar_sbr_number_renamer_get_mode        (ThunarSbrNumberRenamer *number_renamer);
void                    thunar_sbr_number_renamer_set_mode        (ThunarSbrNumberRenamer *number_renamer,
                                                                   ThunarSbrNumberMode     mode);

const gchar            *thunar_sbr_number_renamer_get_start        (ThunarSbrNumberRenamer *number_renamer);
void                    thunar_sbr_number_renamer_set_start        (ThunarSbrNumberRenamer *number_renamer,
                                                                   const gchar            *start);

const gchar            *thunar_sbr_number_renamer_get_text        (ThunarSbrNumberRenamer *number_renamer);
void                    thunar_sbr_number_renamer_set_text        (ThunarSbrNumberRenamer *number_renamer,
                                                                   const gchar            *text);

ThunarSbrTextMode       thunar_sbr_number_renamer_get_text_mode   (ThunarSbrNumberRenamer *number_renamer);
void                    thunar_sbr_number_renamer_set_text_mode   (ThunarSbrNumberRenamer *number_renamer,
                                                                   ThunarSbrTextMode       text_mode);

G_END_DECLS;

#endif /* !__THUNAR_SBR_NUMBER_RENAMER_H__ */
