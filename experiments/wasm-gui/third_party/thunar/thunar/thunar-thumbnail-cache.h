/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_THUMBNAIL_CACHE_H__
#define __THUNAR_THUMBNAIL_CACHE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define THUNAR_TYPE_THUMBNAIL_CACHE            (thunar_thumbnail_cache_get_type ())
#define THUNAR_THUMBNAIL_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_THUMBNAIL_CACHE, ThunarThumbnailCache))
#define THUNAR_THUMBNAIL_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_THUMBNAIL_CACHE, ThunarThumbnailCacheClass))
#define THUNAR_IS_THUMBNAIL_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_THUMBNAIL_CACHE))
#define THUNAR_IS_THUMBNAIL_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_THUMBNAIL_CACHE)
#define THUNAR_THUMBNAIL_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_THUMBNAIL_CACHE, ThunarThumbnailCacheClass))

typedef struct _ThunarThumbnailCachePrivate ThunarThumbnailCachePrivate;
typedef struct _ThunarThumbnailCacheClass   ThunarThumbnailCacheClass;
typedef struct _ThunarThumbnailCache        ThunarThumbnailCache;

GType                 thunar_thumbnail_cache_get_type     (void) G_GNUC_CONST;

ThunarThumbnailCache *thunar_thumbnail_cache_new          (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void                  thunar_thumbnail_cache_move_file    (ThunarThumbnailCache *cache,
                                                           GFile                *source_file,
                                                           GFile                *target_file);
void                  thunar_thumbnail_cache_copy_file    (ThunarThumbnailCache *cache,
                                                           GFile                *source_file,
                                                           GFile                *target_file);
void                  thunar_thumbnail_cache_delete_file  (ThunarThumbnailCache *cache,
                                                           GFile                *file);
void                  thunar_thumbnail_cache_cleanup_file (ThunarThumbnailCache *cache,
                                                           GFile                *file);

G_END_DECLS

#endif /* !__THUNAR_THUMBNAIL_CACHE_H__ */
