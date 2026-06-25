/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_SBR_DATE_RENAMER_H__
#define __THUNAR_SBR_DATE_RENAMER_H__

#include <thunar-sbr/thunar-sbr-enum-types.h>

G_BEGIN_DECLS;

typedef struct _ThunarSbrDateRenamerClass ThunarSbrDateRenamerClass;
typedef struct _ThunarSbrDateRenamer      ThunarSbrDateRenamer;

#define THUNAR_SBR_TYPE_DATE_RENAMER            (thunar_sbr_date_renamer_get_type ())
#define THUNAR_SBR_DATE_RENAMER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_SBR_TYPE_DATE_RENAMER, ThunarSbrDateRenamer))
#define THUNAR_SBR_DATE_RENAMER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_SBR_TYPE_DATE_RENAMER, ThunarSbrDateRenamerClass))
#define THUNAR_SBR_IS_DATE_RENAMER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_SBR_TYPE_DATE_RENAMER))
#define THUNAR_SBR_IS_DATE_RENAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_SBR_TYPE_DATE_RENAMER))
#define THUNAR_SBR_DATE_RENAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_SBR_TYPE_DATE_RENAMER, ThunarSbrDateRenamerClass))

GType                 thunar_sbr_date_renamer_get_type        (void) G_GNUC_CONST;

void                  thunar_sbr_date_renamer_register_type   (ThunarxProviderPlugin  *plugin);

ThunarSbrDateRenamer *thunar_sbr_date_renamer_new             (void) G_GNUC_MALLOC;

ThunarSbrDateMode     thunar_sbr_date_renamer_get_mode        (ThunarSbrDateRenamer *date_renamer);
void                  thunar_sbr_date_renamer_set_mode        (ThunarSbrDateRenamer *date_renamer,
                                                               ThunarSbrDateMode     mode);

const gchar          *thunar_sbr_date_renamer_get_format      (ThunarSbrDateRenamer *date_renamer);
void                  thunar_sbr_date_renamer_set_format      (ThunarSbrDateRenamer *date_renamer,
                                                               const gchar          *format);

guint                 thunar_sbr_date_renamer_get_offset      (ThunarSbrDateRenamer *date_renamer);
void                  thunar_sbr_date_renamer_set_offset      (ThunarSbrDateRenamer *date_renamer,
                                                               guint                 offset);

ThunarSbrOffsetMode   thunar_sbr_date_renamer_get_offset_mode (ThunarSbrDateRenamer *date_renamer);
void                  thunar_sbr_date_renamer_set_offset_mode (ThunarSbrDateRenamer *date_renamer,
                                                               ThunarSbrOffsetMode   offset_mode);

G_END_DECLS;

#endif /* !__THUNAR_SBR_DATE_RENAMER_H__ */
