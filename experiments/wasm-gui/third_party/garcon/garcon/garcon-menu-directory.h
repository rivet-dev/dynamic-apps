/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2007-2009 Jannis Pohlmann <jannis@xfce.org>
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

#if !defined(_GARCON_INSIDE_GARCON_H) && !defined(GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_MENU_DIRECTORY_H__
#define __GARCON_MENU_DIRECTORY_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define GARCON_TYPE_MENU_DIRECTORY            (garcon_menu_directory_get_type ())
#define GARCON_MENU_DIRECTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU_DIRECTORY, GarconMenuDirectory))
#define GARCON_MENU_DIRECTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_TYPE_MENU_DIRECTORY, GarconMenuDirectoryClass))
#define GARCON_IS_MENU_DIRECTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU_DIRECTORY))
#define GARCON_IS_MENU_DIRECTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_TYPE_MENU_DIRECTORY))
#define GARCON_MENU_DIRECTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_TYPE_MENU_DIRECTORY, GarconMenuDirectoryClass))

typedef struct _GarconMenuDirectoryPrivate GarconMenuDirectoryPrivate;
typedef struct _GarconMenuDirectoryClass   GarconMenuDirectoryClass;
typedef struct _GarconMenuDirectory        GarconMenuDirectory;

struct _GarconMenuDirectoryClass
{
  GObjectClass __parent__;
};

struct _GarconMenuDirectory
{
  GObject __parent__;

  /* < private > */
  GarconMenuDirectoryPrivate *priv;
};



GType                garcon_menu_directory_get_type                (void) G_GNUC_CONST;

GarconMenuDirectory *garcon_menu_directory_new                     (GFile               *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GFile               *garcon_menu_directory_get_file                (GarconMenuDirectory *directory);
const gchar         *garcon_menu_directory_get_name                (GarconMenuDirectory *directory);
void                 garcon_menu_directory_set_name                (GarconMenuDirectory *directory,
                                                                    const gchar         *name);
const gchar         *garcon_menu_directory_get_comment             (GarconMenuDirectory *directory);
void                 garcon_menu_directory_set_comment             (GarconMenuDirectory *directory,
                                                                    const gchar         *comment);
const gchar         *garcon_menu_directory_get_icon_name           (GarconMenuDirectory *directory);
void                 garcon_menu_directory_set_icon_name           (GarconMenuDirectory *directory,
                                                                    const gchar         *icon_name);
gboolean             garcon_menu_directory_get_no_display          (GarconMenuDirectory *directory);
void                 garcon_menu_directory_set_no_display          (GarconMenuDirectory *directory,
                                                                    gboolean             no_display);
gboolean             garcon_menu_directory_get_hidden              (GarconMenuDirectory *directory);
gboolean             garcon_menu_directory_get_show_in_environment (GarconMenuDirectory *directory);
gboolean             garcon_menu_directory_get_visible             (GarconMenuDirectory *directory);
gboolean             garcon_menu_directory_equal                   (GarconMenuDirectory *directory,
                                                                    GarconMenuDirectory *other);

G_END_DECLS

#endif /* !__GARCON_MENU_DIRECTORY_H__ */
