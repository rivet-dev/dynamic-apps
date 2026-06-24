/* application object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
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

#ifndef WNCK_APPLICATION_H
#define WNCK_APPLICATION_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libwnck/screen.h>

G_BEGIN_DECLS

#define WNCK_TYPE_APPLICATION              (wnck_application_get_type ())
#define WNCK_APPLICATION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_APPLICATION, WnckApplication))
#define WNCK_APPLICATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_APPLICATION, WnckApplicationClass))
#define WNCK_IS_APPLICATION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_APPLICATION))
#define WNCK_IS_APPLICATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_APPLICATION))
#define WNCK_APPLICATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_APPLICATION, WnckApplicationClass))

typedef struct _WnckApplicationClass   WnckApplicationClass;
typedef struct _WnckApplicationPrivate WnckApplicationPrivate;

/**
 * WnckApplication:
 *
 * The #WnckApplication struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckApplication
{
  GObject parent_instance;

  WnckApplicationPrivate *priv;
};

struct _WnckApplicationClass
{
  GObjectClass parent_class;

  /* app name or icon name changed */
  void (* name_changed) (WnckApplication *app);

  /* icon changed */
  void (* icon_changed) (WnckApplication *app);
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GType wnck_application_get_type (void) G_GNUC_CONST;

WnckApplication* wnck_application_get (gulong xwindow);

gulong wnck_application_get_xid (WnckApplication *app);

GList* wnck_application_get_windows   (WnckApplication *app);
int    wnck_application_get_n_windows (WnckApplication *app);

/* application_get_name, application_get_pid, etc.; prefer to read
 * properties straight off the group leader, and failing that, if the
 * prop is the same for all windows in the app, return the values for
 * the window. Failing that, they make stuff up.
 */
const char* wnck_application_get_name      (WnckApplication *app);
const char* wnck_application_get_icon_name (WnckApplication *app);
int         wnck_application_get_pid       (WnckApplication *app);
GdkPixbuf*  wnck_application_get_icon      (WnckApplication *app);
GdkPixbuf*  wnck_application_get_mini_icon (WnckApplication *app);
gboolean    wnck_application_get_icon_is_fallback (WnckApplication *app);
const char* wnck_application_get_startup_id (WnckApplication *app);

G_END_DECLS

#endif /* WNCK_APPLICATION_H */
