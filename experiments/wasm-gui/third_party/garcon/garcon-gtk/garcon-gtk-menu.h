/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2013 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(_GARCON_INSIDE_GARCON_GTK_H) && !defined(GARCON_COMPILATION)
#error "Only <garcon-gtk/garcon-gtk.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_GTK_MENU_H__
#define __GARCON_GTK_MENU_H__

#include <gtk/gtk.h>
#include <garcon/garcon.h>

G_BEGIN_DECLS

#define GARCON_GTK_TYPE_MENU            (garcon_gtk_menu_get_type ())
#define GARCON_GTK_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_GTK_TYPE_MENU, GarconGtkMenu))
#define GARCON_GTK_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_GTK_TYPE_MENU, GarconGtkMenuClass))
#define GARCON_GTK_IS_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_GTK_TYPE_MENU))
#define GARCON_GTK_IS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_GTK_TYPE_MENU))
#define GARCON_GTK_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_GTK_TYPE_MENU, GarconGtkMenuClass))

typedef struct _GarconGtkMenuPrivate GarconGtkMenuPrivate;
typedef struct _GarconGtkMenuClass   GarconGtkMenuClass;
typedef struct _GarconGtkMenu        GarconGtkMenu;

struct _GarconGtkMenuClass
{
  GtkMenuClass __parent__;
};

struct _GarconGtkMenu
{
  GtkMenu              __parent__;

  /* < private > */
  GarconGtkMenuPrivate *priv;
};

GType                garcon_gtk_menu_get_type                 (void) G_GNUC_CONST;

GtkWidget           *garcon_gtk_menu_new                      (GarconMenu    *garcon_menu) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void                 garcon_gtk_menu_set_menu                 (GarconGtkMenu *menu,
                                                               GarconMenu    *garcon_menu);

GarconMenu          *garcon_gtk_menu_get_menu                 (GarconGtkMenu *menu);

void                 garcon_gtk_menu_set_show_generic_names   (GarconGtkMenu *menu,
                                                               gboolean       show_generic_names);
gboolean             garcon_gtk_menu_get_show_generic_names   (GarconGtkMenu *menu);

void                 garcon_gtk_menu_set_show_menu_icons      (GarconGtkMenu *menu,
                                                               gboolean       show_menu_icons);
gboolean             garcon_gtk_menu_get_show_menu_icons      (GarconGtkMenu *menu);

void                 garcon_gtk_menu_set_show_tooltips        (GarconGtkMenu *menu,
                                                               gboolean       show_tooltips);
gboolean             garcon_gtk_menu_get_show_tooltips        (GarconGtkMenu *menu);

void                 garcon_gtk_menu_set_show_desktop_actions (GarconGtkMenu *menu,
                                                               gboolean       show_desktop_actions);
gboolean             garcon_gtk_menu_get_show_desktop_actions (GarconGtkMenu *menu);

GtkMenu             *garcon_gtk_menu_get_desktop_actions_menu (GarconMenuItem *item);

void                 garcon_gtk_menu_set_right_click_edits    (GarconGtkMenu *menu,
                                                               gboolean       enable_right_click_edits);
gboolean             garcon_gtk_menu_get_right_click_edits    (GarconGtkMenu *menu);

G_END_DECLS

#endif /* !__GARCON_GTK_MENU_H__ */
