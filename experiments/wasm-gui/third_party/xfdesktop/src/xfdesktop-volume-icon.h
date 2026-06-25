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

#ifndef __XFDESKTOP_VOLUME_ICON_H__
#define __XFDESKTOP_VOLUME_ICON_H__

#include <gio/gio.h>

#include "xfdesktop-file-icon.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_VOLUME_ICON     (xfdesktop_volume_icon_get_type())
#define XFDESKTOP_VOLUME_ICON(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_VOLUME_ICON, XfdesktopVolumeIcon))
#define XFDESKTOP_IS_VOLUME_ICON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_VOLUME_ICON))

typedef struct _XfdesktopVolumeIcon         XfdesktopVolumeIcon;
typedef struct _XfdesktopVolumeIconClass    XfdesktopVolumeIconClass;
typedef struct _XfdesktopVolumeIconPrivate  XfdesktopVolumeIconPrivate;

struct _XfdesktopVolumeIcon
{
    XfdesktopFileIcon parent;

    /*< private >*/
    XfdesktopVolumeIconPrivate *priv;
};

struct _XfdesktopVolumeIconClass
{
    XfdesktopFileIconClass parent;
};

GType xfdesktop_volume_icon_get_type(void) G_GNUC_CONST;

XfdesktopVolumeIcon *xfdesktop_volume_icon_new(GVolume *volume,
                                               GdkScreen *screen);

GVolume *xfdesktop_volume_icon_peek_volume(XfdesktopVolumeIcon *icon);


G_END_DECLS

#endif /* __XFDESKTOP_VOLUME_ICON_H__ */
