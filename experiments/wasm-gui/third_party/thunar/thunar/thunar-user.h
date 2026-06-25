/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __THUNAR_USER_H__
#define __THUNAR_USER_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _ThunarGroupClass ThunarGroupClass;
typedef struct _ThunarGroup      ThunarGroup;

#define THUNAR_TYPE_GROUP            (thunar_group_get_type ())
#define THUNAR_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_GROUP, ThunarGroup))
#define THUNAR_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_GROUP, ThunarGroupClass))
#define THUNAR_IS_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_GROUP))
#define THUNAR_IS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_GROUP))
#define THUNAR_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_GROUP, ThunarGroupClass))

GType         thunar_group_get_type  (void) G_GNUC_CONST;

guint32       thunar_group_get_id    (ThunarGroup *group);
const gchar  *thunar_group_get_name  (ThunarGroup *group);


typedef struct _ThunarUserClass ThunarUserClass;
typedef struct _ThunarUser      ThunarUser;

#define THUNAR_TYPE_USER            (thunar_user_get_type ())
#define THUNAR_USER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_USER, ThunarUser))
#define THUNAR_USER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_USER, ThunarUserClass))
#define THUNAR_IS_USER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_USER))
#define THUNAR_IS_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_USER))
#define THUNAR_USER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_USER, ThunarUserClass))

GType         thunar_user_get_type          (void) G_GNUC_CONST;

GList        *thunar_user_get_groups        (ThunarUser *user);
const gchar  *thunar_user_get_name          (ThunarUser *user);
const gchar  *thunar_user_get_real_name     (ThunarUser *user);
gboolean      thunar_user_is_me             (ThunarUser *user);


typedef struct _ThunarUserManagerClass ThunarUserManagerClass;
typedef struct _ThunarUserManager      ThunarUserManager;

#define THUNAR_TYPE_USER_MANAGER            (thunar_user_manager_get_type ())
#define THUNAR_USER_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_USER_MANAGER, ThunarUserManager))
#define THUNAR_USER_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_USER_MANAGER, ThunarUserManagerClass))
#define THUNAR_IS_USER_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_USER_MANAGER))
#define THUNAR_IS_USER_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_USER_MANAGER))
#define THUNAR_USER_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_USER_MANAGER, ThunarUserManagerClass))

GType              thunar_user_manager_get_type        (void) G_GNUC_CONST;

ThunarUserManager *thunar_user_manager_get_default     (void) G_GNUC_WARN_UNUSED_RESULT;

ThunarGroup       *thunar_user_manager_get_group_by_id (ThunarUserManager *manager,
                                                        guint32            id) G_GNUC_WARN_UNUSED_RESULT;
ThunarUser        *thunar_user_manager_get_user_by_id  (ThunarUserManager *manager,
                                                        guint32            id) G_GNUC_WARN_UNUSED_RESULT;

GList             *thunar_user_manager_get_all_groups  (ThunarUserManager *manager) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_USER_H__ */
