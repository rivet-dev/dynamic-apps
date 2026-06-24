/* workspace object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_WORKSPACE_H
#define WNCK_WORKSPACE_H

#include <glib-object.h>
#include <libwnck/screen.h>

G_BEGIN_DECLS

#define WNCK_TYPE_WORKSPACE              (wnck_workspace_get_type ())
#define WNCK_WORKSPACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_WORKSPACE, WnckWorkspace))
#define WNCK_WORKSPACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_WORKSPACE, WnckWorkspaceClass))
#define WNCK_IS_WORKSPACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_WORKSPACE))
#define WNCK_IS_WORKSPACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_WORKSPACE))
#define WNCK_WORKSPACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_WORKSPACE, WnckWorkspaceClass))

typedef struct _WnckWorkspaceClass   WnckWorkspaceClass;
typedef struct _WnckWorkspacePrivate WnckWorkspacePrivate;

/**
 * WnckWorkspace:
 *
 * The #WnckWorkspace struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckWorkspace
{
  GObject parent_instance;

  WnckWorkspacePrivate *priv;
};

struct _WnckWorkspaceClass
{
  GObjectClass parent_class;

  void (* name_changed) (WnckWorkspace *space);
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * WnckMotionDirection:
 * @WNCK_MOTION_UP: search a neighbor #WnckWorkspace above another
 * #WnckWorkspace. 
 * @WNCK_MOTION_DOWN: search a neighbor #WnckWorkspace below another
 * #WnckWorkspace.
 * @WNCK_MOTION_LEFT: search a neighbor #WnckWorkspace at the left of another
 * #WnckWorkspace.
 * @WNCK_MOTION_RIGHT: search a neighbor #WnckWorkspace at the right of another
 * #WnckWorkspace.
 *
 * Type defining a direction in which to search a neighbor #WnckWorkspace.
 *
 * Since: 2.14
 */
typedef enum
{
  WNCK_MOTION_UP = -1,
  WNCK_MOTION_DOWN = -2,
  WNCK_MOTION_LEFT = -3,
  WNCK_MOTION_RIGHT = -4
} WnckMotionDirection;

GType wnck_workspace_get_type (void) G_GNUC_CONST;

int         wnck_workspace_get_number     (WnckWorkspace *space);
const char* wnck_workspace_get_name       (WnckWorkspace *space);
void        wnck_workspace_change_name    (WnckWorkspace *space,
                                           const char    *name);
WnckScreen* wnck_workspace_get_screen     (WnckWorkspace *space);
void        wnck_workspace_activate       (WnckWorkspace *space,
                                           guint32        timestamp);
int         wnck_workspace_get_width      (WnckWorkspace *space);
int         wnck_workspace_get_height     (WnckWorkspace *space);
int         wnck_workspace_get_viewport_x (WnckWorkspace *space);
int         wnck_workspace_get_viewport_y (WnckWorkspace *space);
gboolean    wnck_workspace_is_virtual     (WnckWorkspace *space);


int wnck_workspace_get_layout_row          (WnckWorkspace       *space);
int wnck_workspace_get_layout_column       (WnckWorkspace       *space);
WnckWorkspace* wnck_workspace_get_neighbor (WnckWorkspace       *space,
                                            WnckMotionDirection  direction);

G_END_DECLS

#endif /* WNCK_WORKSPACE_H */
