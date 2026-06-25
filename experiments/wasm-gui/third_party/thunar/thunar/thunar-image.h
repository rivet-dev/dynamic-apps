/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_IMAGE_H__
#define __THUNAR_IMAGE_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

#define THUNAR_TYPE_IMAGE            (thunar_image_get_type ())
#define THUNAR_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_IMAGE, ThunarImage))
#define THUNAR_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_IMAGE, ThunarImageClass))
#define THUNAR_IS_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_IMAGE))
#define THUNAR_IS_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_IMAGE)
#define THUNAR_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_IMAGE, ThunarImageClass))

typedef struct _ThunarImagePrivate ThunarImagePrivate;
typedef struct _ThunarImageClass   ThunarImageClass;
typedef struct _ThunarImage        ThunarImage;

GType      thunar_image_get_type (void) G_GNUC_CONST;

GtkWidget *thunar_image_new      (void) G_GNUC_MALLOC;
void       thunar_image_set_file (ThunarImage *image,
                                  ThunarFile  *file);

G_END_DECLS;

#endif /* !__THUNAR_IMAGE_H__ */
