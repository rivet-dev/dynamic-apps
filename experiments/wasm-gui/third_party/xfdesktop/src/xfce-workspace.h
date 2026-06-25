/*
 *  xfworkspace - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <bjt23@cornell.edu>
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

#ifndef _XFCE_WORKSPACE_H_
#define _XFCE_WORKSPACE_H_

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "xfce-backdrop.h"

G_BEGIN_DECLS

#define XFCE_TYPE_WORKSPACE              (xfce_workspace_get_type())
#define XFCE_WORKSPACE(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), XFCE_TYPE_WORKSPACE, XfceWorkspace))
#define XFCE_WORKSPACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), XFCE_TYPE_WORKSPACE, XfceWorkspaceClass))
#define XFCE_IS_WORKSPACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), XFCE_TYPE_WORKSPACE))
#define XFCE_IS_WORKSPACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), XFCE_TYPE_WORKSPACE))
#define XFCE_WORKSPACE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), XFCE_TYPE_WORKSPACE, XfceWorkspaceClass))

typedef struct _XfceWorkspace XfceWorkspace;
typedef struct _XfceWorkspaceClass XfceWorkspaceClass;
typedef struct _XfceWorkspacePrivate XfceWorkspacePrivate;

struct _XfceWorkspace
{
    GObject gobject;

    /*< private >*/
    XfceWorkspacePrivate *priv;
};

struct _XfceWorkspaceClass
{
    GObjectClass parent_class;

    /*< signals >*/
    void (*changed)(XfceWorkspace *workspace, XfceBackdrop *backdrop);
};

GType xfce_workspace_get_type                     (void) G_GNUC_CONST;

XfceWorkspace *xfce_workspace_new(GdkScreen *gscreen,
                                  XfconfChannel *channel,
                                  const gchar *property_prefix,
                                  gint number);

gint xfce_workspace_get_workspace_num(XfceWorkspace *workspace);
void xfce_workspace_set_workspace_num(XfceWorkspace *workspace, gint number);

void xfce_workspace_monitors_changed(XfceWorkspace *workspace,
                                     GdkScreen *gscreen);

gboolean xfce_workspace_get_xinerama_stretch(XfceWorkspace *workspace);


XfceBackdrop *xfce_workspace_get_backdrop(XfceWorkspace *workspace,
                                          guint monitor);

G_END_DECLS

#endif
