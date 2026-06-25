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

#ifndef __RISTRETTO_IMAGE_VIEWER_H__
#define __RISTRETTO_IMAGE_VIEWER_H__

#include <gtk/gtk.h>

#include "file.h"

G_BEGIN_DECLS

#define RSTTO_SCALE_FACTOR 1.1



#define RSTTO_TYPE_IMAGE_VIEWER rstto_image_viewer_get_type ()
G_DECLARE_FINAL_TYPE (RsttoImageViewer, rstto_image_viewer, RSTTO, IMAGE_VIEWER, GtkWidget)

typedef struct _RsttoImageViewerPrivate RsttoImageViewerPrivate;

struct _RsttoImageViewer
{
    GtkWidget parent;
    RsttoImageViewerPrivate *priv;
};



GtkWidget *
rstto_image_viewer_new (void);

void
rstto_image_viewer_set_file (RsttoImageViewer *viewer,
                             RsttoFile *file,
                             gdouble scale,
                             RsttoScale auto_scale,
                             RsttoImageOrientation orientation);

void
rstto_image_viewer_set_scale (RsttoImageViewer *viewer,
                              gdouble scale);

GdkPixbuf *
rstto_image_viewer_get_pixbuf (RsttoImageViewer *viewer);

gdouble
rstto_image_viewer_get_scale (RsttoImageViewer *viewer);

void
rstto_image_viewer_set_orientation (RsttoImageViewer *viewer,
                                    RsttoImageOrientation orientation);

RsttoImageOrientation
rstto_image_viewer_get_orientation (RsttoImageViewer *viewer);

gint
rstto_image_viewer_get_width (RsttoImageViewer *viewer);

gint
rstto_image_viewer_get_height (RsttoImageViewer *viewer);

void
rstto_image_viewer_set_menu (RsttoImageViewer *viewer,
                             GtkMenu *menu);

GError *
rstto_image_viewer_get_error (RsttoImageViewer *viewer);

void
rstto_image_viewer_set_show_clock (RsttoImageViewer *viewer,
                                   gboolean value);

gboolean
rstto_image_viewer_is_busy (RsttoImageViewer *viewer);

G_END_DECLS

#endif /* __RISTRETTO_IMAGE_VIEWER_H__ */
