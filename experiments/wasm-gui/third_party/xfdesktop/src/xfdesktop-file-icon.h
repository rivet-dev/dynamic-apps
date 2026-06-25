/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright (c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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

#ifndef __XFDESKTOP_FILE_ICON_H__
#define __XFDESKTOP_FILE_ICON_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "xfdesktop-icon.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_FILE_ICON            (xfdesktop_file_icon_get_type())
#define XFDESKTOP_FILE_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_FILE_ICON, XfdesktopFileIcon))
#define XFDESKTOP_IS_FILE_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_FILE_ICON))
#define XFDESKTOP_FILE_ICON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_FILE_ICON, XfdesktopFileIconClass))

typedef struct _XfdesktopFileIcon        XfdesktopFileIcon;
typedef struct _XfdesktopFileIconClass   XfdesktopFileIconClass;
typedef struct _XfdesktopFileIconPrivate XfdesktopFileIconPrivate;

struct _XfdesktopFileIcon
{
    XfdesktopIcon parent;

    /*< private >*/
    XfdesktopFileIconPrivate *priv;
};

struct _XfdesktopFileIconClass
{
    XfdesktopIconClass parent;

    /*< virtual functions >*/
    GFileInfo *(*peek_file_info)(XfdesktopFileIcon *icon);
    GFileInfo *(*peek_filesystem_info)(XfdesktopFileIcon *icon);
    GFile *(*peek_file)(XfdesktopFileIcon *icon);
    void (*update_file_info)(XfdesktopFileIcon *icon, GFileInfo *info);

    gboolean (*can_rename_file)(XfdesktopFileIcon *icon);
    gboolean (*can_delete_file)(XfdesktopFileIcon *icon);
};

GType xfdesktop_file_icon_get_type(void) G_GNUC_CONST;

GFileInfo *xfdesktop_file_icon_peek_file_info(XfdesktopFileIcon *icon);
GFileInfo *xfdesktop_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
GFile *xfdesktop_file_icon_peek_file(XfdesktopFileIcon *icon);
void xfdesktop_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                          GFileInfo *info);

gboolean xfdesktop_file_icon_can_rename_file(XfdesktopFileIcon *icon);

gboolean xfdesktop_file_icon_can_delete_file(XfdesktopFileIcon *icon);

GIcon *xfdesktop_file_icon_add_emblems(XfdesktopFileIcon *icon);

void xfdesktop_file_icon_invalidate_icon(XfdesktopFileIcon *icon);

gboolean xfdesktop_file_icon_has_gicon(XfdesktopFileIcon *icon);

G_END_DECLS

#endif  /* __XFDESKTOP_FILE_ICON_H__ */
