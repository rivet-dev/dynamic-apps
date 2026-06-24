/* class group object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2003 Ximian, Inc.
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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

#ifndef WNCK_CLASS_GROUP_H
#define WNCK_CLASS_GROUP_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libwnck/screen.h>

G_BEGIN_DECLS

#define WNCK_TYPE_CLASS_GROUP              (wnck_class_group_get_type ())
#define WNCK_CLASS_GROUP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_CLASS_GROUP, WnckClassGroup))
#define WNCK_CLASS_GROUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_CLASS_GROUP, WnckClassGroupClass))
#define WNCK_IS_CLASS_GROUP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_CLASS_GROUP))
#define WNCK_IS_CLASS_GROUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_CLASS_GROUP))
#define WNCK_CLASS_GROUP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_CLASS_GROUP, WnckClassGroupClass))

typedef struct _WnckClassGroupClass   WnckClassGroupClass;
typedef struct _WnckClassGroupPrivate WnckClassGroupPrivate;

/**
 * WnckClassGroup:
 *
 * The #WnckClassGroup struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckClassGroup
{
  GObject parent_instance;

  WnckClassGroupPrivate *priv;
};

struct _WnckClassGroupClass
{
  GObjectClass parent_class;

  void (* name_changed) (WnckClassGroup *group);
  void (* icon_changed) (WnckClassGroup *group);

  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GType wnck_class_group_get_type (void) G_GNUC_CONST;

WnckClassGroup *wnck_class_group_get (const char *id);

GList *wnck_class_group_get_windows (WnckClassGroup *class_group);
const char * wnck_class_group_get_id (WnckClassGroup *class_group);

const char * wnck_class_group_get_name (WnckClassGroup *class_group);

GdkPixbuf *wnck_class_group_get_icon (WnckClassGroup *class_group);
GdkPixbuf *wnck_class_group_get_mini_icon (WnckClassGroup *class_group);

#ifndef WNCK_DISABLE_DEPRECATED
G_DEPRECATED_FOR(wnck_class_group_get_id)
const char * wnck_class_group_get_res_class (WnckClassGroup *class_group);
#endif

G_END_DECLS

#endif /* WNCK_CLASS_GROUP_H */
