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

#ifndef __XFDESKTOP_SPECIAL_FILE_ICON_H__
#define __XFDESKTOP_SPECIAL_FILE_ICON_H__

#include <glib-object.h>

#include "xfdesktop-file-icon.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_SPECIAL_FILE_ICON     (xfdesktop_special_file_icon_get_type())
#define XFDESKTOP_SPECIAL_FILE_ICON(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_SPECIAL_FILE_ICON, XfdesktopSpecialFileIcon))
#define XFDESKTOP_IS_SPECIAL_FILE_ICON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_SPECIAL_FILE_ICON))

typedef struct _XfdesktopSpecialFileIcon         XfdesktopSpecialFileIcon;
typedef struct _XfdesktopSpecialFileIconClass    XfdesktopSpecialFileIconClass;
typedef struct _XfdesktopSpecialFileIconPrivate  XfdesktopSpecialFileIconPrivate;

struct _XfdesktopSpecialFileIcon
{
    XfdesktopFileIcon parent;

    /*< private >*/
    XfdesktopSpecialFileIconPrivate *priv;
};

struct _XfdesktopSpecialFileIconClass
{
    XfdesktopFileIconClass parent;
};

typedef enum
{
    XFDESKTOP_SPECIAL_FILE_ICON_HOME = 0,
    XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM,
    XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
} XfdesktopSpecialFileIconType;

GType xfdesktop_special_file_icon_get_type(void) G_GNUC_CONST;

XfdesktopSpecialFileIcon *xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                                          GdkScreen *screen);

XfdesktopSpecialFileIconType xfdesktop_special_file_icon_get_icon_type(XfdesktopSpecialFileIcon *icon);

G_END_DECLS

#endif /* __XFDESKTOP_SPECIAL_FILE_ICON_H__ */
