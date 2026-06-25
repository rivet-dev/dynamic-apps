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

#ifndef __XFDESKTOP_APPLICATION_H__
#define __XFDESKTOP_APPLICATION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_APPLICATION            (xfdesktop_application_get_type())
#define XFDESKTOP_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_APPLICATION, XfdesktopApplication))
#define XFDESKTOP_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), XFDESKTOP_TYPE_APPLICATION, XfdesktopApplicationClass))
#define XFDESKTOP_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_APPLICATION))
#define XFDESKTOP_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_APPLICATION, XfdesktopApplicationClass))

typedef struct _XfdesktopApplication        XfdesktopApplication;
typedef struct _XfdesktopApplicationClass   XfdesktopApplicationClass;


GType xfdesktop_application_get_type(void) G_GNUC_CONST;

XfdesktopApplication *xfdesktop_application_get(void);

gint xfdesktop_application_run(XfdesktopApplication *app, int argc, char **argv);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_H__ */
