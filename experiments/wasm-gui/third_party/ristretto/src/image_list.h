/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __RISTRETTO_IMAGE_LIST_H__
#define __RISTRETTO_IMAGE_LIST_H__

#include <gtk/gtk.h>

#include "file.h"

G_BEGIN_DECLS

#define RSTTO_TYPE_IMAGE_LIST rstto_image_list_get_type ()
G_DECLARE_FINAL_TYPE (RsttoImageList, rstto_image_list, RSTTO, IMAGE_LIST, GObject)

typedef struct _RsttoImageListPrivate RsttoImageListPrivate;

struct _RsttoImageList
{
    GObject parent;
    RsttoImageListPrivate *priv;
};

#define RSTTO_TYPE_IMAGE_LIST_ITER rstto_image_list_iter_get_type ()
G_DECLARE_FINAL_TYPE (RsttoImageListIter, rstto_image_list_iter, RSTTO, IMAGE_LIST_ITER, GObject)

typedef struct _RsttoImageListIterPrivate RsttoImageListIterPrivate;

struct _RsttoImageListIter
{
    GObject parent;
    RsttoImageListIterPrivate *priv;
};



RsttoImageList *
rstto_image_list_new (void);

gint
rstto_image_list_get_n_images (RsttoImageList *image_list);

gboolean
rstto_image_list_is_empty (RsttoImageList *image_list);

gboolean
rstto_image_list_add_file (RsttoImageList *image_list,
                           RsttoFile *file,
                           GError **error);

void
rstto_image_list_remove_file (RsttoImageList *image_list,
                              RsttoFile *file);

gboolean
rstto_image_list_set_directory (RsttoImageList *image_list,
                                GFile *dir,
                                RsttoFile *file,
                                GError **error);

gboolean
rstto_image_list_is_busy (RsttoImageList *list);

/** Built-in Sorting Functions */
void
rstto_image_list_set_sort_by_name (RsttoImageList *image_list);

void
rstto_image_list_set_sort_by_type (RsttoImageList *image_list);

void
rstto_image_list_set_sort_by_date (RsttoImageList *image_list);

RsttoImageListIter *
rstto_image_list_get_iter (RsttoImageList *image_list);



/** Iter functions */
RsttoFile *
rstto_image_list_iter_get_file (RsttoImageListIter *iter);

gboolean
rstto_image_list_iter_previous (RsttoImageListIter *iter);

gboolean
rstto_image_list_iter_next (RsttoImageListIter *iter);

gboolean
rstto_image_list_iter_has_previous (RsttoImageListIter *iter);

gboolean
rstto_image_list_iter_has_next (RsttoImageListIter *iter);

gint
rstto_image_list_iter_get_position (RsttoImageListIter *iter);

void
rstto_image_list_iter_set_position (RsttoImageListIter *iter,
                                    gint pos);

gboolean
rstto_image_list_iter_find_file (RsttoImageListIter *iter,
                                 RsttoFile *file);

RsttoImageListIter *
rstto_image_list_iter_clone (RsttoImageListIter *iter);

gboolean
rstto_image_list_iter_get_sticky (RsttoImageListIter *iter);

G_END_DECLS

#endif /* __RISTRETTO_IMAGE_LIST_H__ */
