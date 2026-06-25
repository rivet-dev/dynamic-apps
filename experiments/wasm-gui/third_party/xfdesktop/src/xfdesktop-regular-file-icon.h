/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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
 */

#ifndef __XFDESKTOP_REGULAR_FILE_ICON_H__
#define __XFDESKTOP_REGULAR_FILE_ICON_H__

#include <glib-object.h>

#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-icon-manager.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_REGULAR_FILE_ICON     (xfdesktop_regular_file_icon_get_type())
#define XFDESKTOP_REGULAR_FILE_ICON(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_REGULAR_FILE_ICON, XfdesktopRegularFileIcon))
#define XFDESKTOP_IS_REGULAR_FILE_ICON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_REGULAR_FILE_ICON))

typedef struct _XfdesktopRegularFileIcon         XfdesktopRegularFileIcon;
typedef struct _XfdesktopRegularFileIconClass    XfdesktopRegularFileIconClass;
typedef struct _XfdesktopRegularFileIconPrivate  XfdesktopRegularFileIconPrivate;

struct _XfdesktopRegularFileIcon
{
    XfdesktopFileIcon parent;

    /*< private >*/
    XfdesktopRegularFileIconPrivate *priv;
};

struct _XfdesktopRegularFileIconClass
{
    XfdesktopFileIconClass parent;
};

GType xfdesktop_regular_file_icon_get_type(void) G_GNUC_CONST;

XfdesktopRegularFileIcon *xfdesktop_regular_file_icon_new(GFile *file,
                                                          GFileInfo *file_info,
                                                          GdkScreen *screen,
                                                          XfdesktopFileIconManager *fmanager);

void xfdesktop_regular_file_icon_set_pixbuf_opacity(XfdesktopRegularFileIcon *icon,
                                                    guint opacity);


G_END_DECLS

#endif /* __XFDESKTOP_REGULAR_FILE_ICON_H__ */
