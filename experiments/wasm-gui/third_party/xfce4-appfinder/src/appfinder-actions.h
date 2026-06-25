/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_APPFINDER_ACTIONS_H__
#define __XFCE_APPFINDER_ACTIONS_H__

#include <gtk/gtk.h>
#include <garcon/garcon.h>

G_BEGIN_DECLS

typedef struct _XfceAppfinderActionsClass XfceAppfinderActionsClass;
typedef struct _XfceAppfinderActions      XfceAppfinderActions;
typedef struct _XfceAppfinderAction       XfceAppfinderAction;

#define XFCE_TYPE_APPFINDER_ACTIONS            (xfce_appfinder_actions_get_type ())
#define XFCE_APPFINDER_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_APPFINDER_ACTIONS, XfceAppfinderActions))
#define XFCE_APPFINDER_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_APPFINDER_ACTIONS, XfceAppfinderActionsClass))
#define XFCE_IS_APPFINDER_ACTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_APPFINDER_ACTIONS))
#define XFCE_IS_APPFINDER_ACTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_APPFINDER_ACTIONS))
#define XFCE_APPFINDER_ACTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_APPFINDER_ACTIONS, XfceAppfinderActionsClass))

GType                       xfce_appfinder_actions_get_type      (void) G_GNUC_CONST;

XfceAppfinderActions       *xfce_appfinder_actions_get           (void) G_GNUC_MALLOC;

gint                        xfce_appfinder_actions_get_unique_id (XfceAppfinderActions  *actions);

gchar                      *xfce_appfinder_actions_execute       (XfceAppfinderActions  *actions,
                                                                  const gchar           *text,
                                                                  gboolean              *save_cmd,
                                                                  GError               **error);

G_END_DECLS

#endif /* !__XFCE_APPFINDER_ACTIONS_H__ */
