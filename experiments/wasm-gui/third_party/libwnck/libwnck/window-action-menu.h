/* window action menu (ops on a single window) */
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

#ifndef WNCK_WINDOW_ACTION_MENU_H
#define WNCK_WINDOW_ACTION_MENU_H

#include <libwnck/window.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WNCK_TYPE_ACTION_MENU              (wnck_action_menu_get_type ())
#define WNCK_ACTION_MENU(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_ACTION_MENU, WnckActionMenu))
#define WNCK_ACTION_MENU_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_ACTION_MENU, WnckActionMenuClass))
#define WNCK_IS_ACTION_MENU(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_ACTION_MENU))
#define WNCK_IS_ACTION_MENU_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_ACTION_MENU))
#define WNCK_ACTION_MENU_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_ACTION_MENU, WnckActionMenuClass))

typedef struct _WnckActionMenu        WnckActionMenu;
typedef struct _WnckActionMenuClass   WnckActionMenuClass;
typedef struct _WnckActionMenuPrivate WnckActionMenuPrivate;

/**
 * WnckActionMenu:
 *
 * The #WnckActionMenu struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckActionMenu
{
  GtkMenu parent_instance;

  WnckActionMenuPrivate *priv;
};

struct _WnckActionMenuClass
{
  GtkMenuClass parent_class;

  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GType wnck_action_menu_get_type (void) G_GNUC_CONST;

GtkWidget* wnck_action_menu_new (WnckWindow *window);

G_END_DECLS

#endif /* WNCK_WINDOW_MENU_H */
