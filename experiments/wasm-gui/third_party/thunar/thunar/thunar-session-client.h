/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_SESSION_CLIENT_H__
#define __THUNAR_SESSION_CLIENT_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _ThunarSessionClientClass ThunarSessionClientClass;
typedef struct _ThunarSessionClient      ThunarSessionClient;

#define THUNAR_TYPE_SESSION_CLIENT            (thunar_session_client_get_type ())
#define THUNAR_SESSION_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SESSION_CLIENT, ThunarSessionClient))
#define THUNAR_SESSION_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SESSION_CLIENT, ThunarSessionClientClass))
#define THUNAR_IS_SESSION_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SESSION_CLIENT))
#define THUNAR_IS_SESSION_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SESSION_CLIENT))
#define THUNAR_SESSION_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_SESSION_CLIENT, ThunarSessionClientClass))

GType                thunar_session_client_get_type (void) G_GNUC_CONST;
ThunarSessionClient *thunar_session_client_new      (const gchar *session_id) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_SESSION_CLIENT_H__ */
