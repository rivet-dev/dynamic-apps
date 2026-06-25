/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_THUMBNAILER_H__
#define __THUNAR_THUMBNAILER_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS

typedef struct _ThunarThumbnailerClass ThunarThumbnailerClass;
typedef struct _ThunarThumbnailer      ThunarThumbnailer;

#define THUNAR_TYPE_THUMBNAILER            (thunar_thumbnailer_get_type ())
#define THUNAR_THUMBNAILER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_THUMBNAILER, ThunarThumbnailer))
#define THUNAR_THUMBNAILER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_THUMBNAILER, ThunarThumbnailerClass))
#define THUNAR_IS_THUMBNAILER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_THUMBNAILER))
#define THUNAR_IS_THUMBNAILER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_THUMBNAILER))
#define THUNAR_THUMBNAILER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_THUMBNAILER, ThunarThumbnailerClass))

GType              thunar_thumbnailer_get_type        (void) G_GNUC_CONST;

ThunarThumbnailer *thunar_thumbnailer_get             (void) G_GNUC_MALLOC;

gboolean           thunar_thumbnailer_queue_file      (ThunarThumbnailer        *thumbnailer,
                                                       ThunarFile               *file,
                                                       guint                    *request,
                                                       ThunarThumbnailSize       size);
gboolean           thunar_thumbnailer_queue_files     (ThunarThumbnailer        *thumbnailer,
                                                       gboolean                  lazy_checks,
                                                       GList                    *files,
                                                       guint                    *request,
                                                       ThunarThumbnailSize       size);
void               thunar_thumbnailer_dequeue         (ThunarThumbnailer        *thumbnailer,
                                                       guint                     request);

G_END_DECLS

#endif /* !__THUNAR_THUMBNAILER_H__ */
