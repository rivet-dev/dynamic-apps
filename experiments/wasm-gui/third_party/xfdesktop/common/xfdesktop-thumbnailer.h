/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *  xfdesktop-thumbnailer is based on thumbnailer code from Ristretto
 *  Copyright (c) Stephan Arts 2009-2011 <stephan@xfce.org>
 */

#ifndef __XFDESKTOP_THUMBNAILER_H__
#define __XFDESKTOP_THUMBNAILER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_THUMBNAILER             (xfdesktop_thumbnailer_get_type())
#define XFDESKTOP_THUMBNAILER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_THUMBNAILER, XfdesktopThumbnailer))
#define XFDESKTOP_IS_THUMBNAILER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_THUMBNAILER))
#define XFDESKTOP_THUMBNAILER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), XFDESKTOP_TYPE_THUMBNAILER, XfdesktopThumbnailerClass))
#define XFDESKTOP_IS_THUMBNAILER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), XFDESKTOP_TYPE_THUMBNAILER()))

typedef struct _XfdesktopThumbnailer XfdesktopThumbnailer;
typedef struct _XfdesktopThumbnailerPriv XfdesktopThumbnailerPriv;

struct _XfdesktopThumbnailer
{
    GObject parent;

    XfdesktopThumbnailerPriv *priv;
};

typedef struct _XfdesktopThumbnailerClass XfdesktopThumbnailerClass;

struct _XfdesktopThumbnailerClass
{
    GObjectClass parent_class;

    /*< signals >*/
    void (*thumbnail_ready)(gchar *src_file, gchar *thumb_file);
};

XfdesktopThumbnailer * xfdesktop_thumbnailer_new(void);

GType xfdesktop_thumbnailer_get_type(void);

gboolean xfdesktop_thumbnailer_service_available(XfdesktopThumbnailer *thumbnailer);

gboolean xfdesktop_thumbnailer_is_supported(XfdesktopThumbnailer *thumbnailer,
                                            gchar *file);

gboolean xfdesktop_thumbnailer_queue_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                               gchar *file);
void xfdesktop_thumbnailer_dequeue_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                             gchar *file);
void xfdesktop_thumbnailer_dequeue_all_thumbnails(XfdesktopThumbnailer *thumbnailer);

void xfdesktop_thumbnailer_delete_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                            gchar *src_file);

G_END_DECLS

#endif /* __XFDESKTOP_THUMBNAILER_H__ */
