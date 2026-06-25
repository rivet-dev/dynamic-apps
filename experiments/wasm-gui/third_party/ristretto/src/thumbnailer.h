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

#ifndef __RISTRETTO_THUMBNAILER_H__
#define __RISTRETTO_THUMBNAILER_H__

#include <gtk/gtk.h>

#include "image_list.h"

G_BEGIN_DECLS

#define RSTTO_TYPE_THUMBNAILER rstto_thumbnailer_get_type ()
G_DECLARE_FINAL_TYPE (RsttoThumbnailer, rstto_thumbnailer, RSTTO, THUMBNAILER, GObject)

typedef struct _RsttoThumbnailerPrivate RsttoThumbnailerPrivate;

struct _RsttoThumbnailer
{
    GObject parent;
    RsttoThumbnailerPrivate *priv;
};



RsttoThumbnailer *
rstto_thumbnailer_new (void);

void
rstto_thumbnailer_queue_file (RsttoThumbnailer *thumbnailer,
                              RsttoThumbnailFlavor flavor,
                              RsttoFile *file);

G_END_DECLS

#endif /* __RISTRETTO_THUMBNAILER_H__ */
