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

#ifndef __EXO_THUMBNAIL_H__
#define __EXO_THUMBNAIL_H__

#include <exo/exo-config.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/**
 * ExoThumbnailSize:
 * @EXO_THUMBNAIL_SIZE_NORMAL : normal sized thumbnails (up to 128px).
 * @EXO_THUMBNAIL_SIZE_LARGE  : large sized thumbnails.
 *
 * Thumbnail sizes used by the thumbnail database.
 **/
typedef enum /*< skip >*/
{
  EXO_THUMBNAIL_SIZE_NORMAL = 128,
  EXO_THUMBNAIL_SIZE_LARGE  = 256,
} ExoThumbnailSize;

G_GNUC_INTERNAL GdkPixbuf *_exo_thumbnail_get_for_file (const gchar     *filename,
                                                        ExoThumbnailSize size,
                                                        GError         **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
G_GNUC_INTERNAL GdkPixbuf *_exo_thumbnail_get_for_uri  (const gchar     *uri,
                                                        ExoThumbnailSize size,
                                                        GError         **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* !__EXO_THUMBNAIL_H__ */
