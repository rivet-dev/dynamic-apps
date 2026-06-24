/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __WORKSPACES_H__
#define __WORKSPACES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_WORKSPACES_HELPER     (xfce_workspaces_helper_get_type())
#define XFCE_WORKSPACES_HELPER(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCE_TYPE_WORKSPACES_HELPER, XfceWorkspacesHelper))
#define XFCE_IS_WORKSPACES_HELPER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCE_TYPE_WORKSPACES_HELPER))

typedef struct _XfceWorkspacesHelper       XfceWorkspacesHelper;
typedef struct _XfceWorkspacesHelperClass  XfceWorkspacesHelperClass;

GType xfce_workspaces_helper_get_type(void) G_GNUC_CONST;
#ifdef GDK_WINDOWING_X11
void xfce_workspaces_helper_disable_wm_check (gboolean disable_wm_check);
#endif

G_END_DECLS

#endif /* __WORKSPACES_H__ */
