/*-
 * Copyright (c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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
#ifndef __THUNAR_MENU_H__
#define __THUNAR_MENU_H__

#include <gtk/gtk.h>

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarMenuClass ThunarMenuClass;
typedef struct _ThunarMenu      ThunarMenu;

#define THUNAR_TYPE_MENU             (thunar_menu_get_type ())
#define THUNAR_MENU(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_MENU, ThunarMenu))
#define THUNAR_MENU_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  THUNAR_TYPE_MENU, ThunarMenuClass))
#define THUNAR_IS_MENU(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_MENU))
#define THUNAR_IS_MENU_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  THUNAR_TYPE_MENU))
#define THUNAR_MENU_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  THUNAR_TYPE_MENU, ThunarMenu))

/* For window menu, some items are shown insensitive, instead of hidden */
typedef enum
{
  THUNAR_MENU_TYPE_WINDOW,
  THUNAR_MENU_TYPE_CONTEXT_STANDARD_VIEW,
  THUNAR_MENU_TYPE_CONTEXT_LOCATION_BUTTONS,
  THUNAR_MENU_TYPE_CONTEXT_RENAMER,
  THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW,
  THUNAR_MENU_TYPE_CONTEXT_SHORTCUTS_VIEW,
  N_THUNAR_MENU_TYPE,
} ThunarMenuType;

/* Bundles of #GtkMenuItems, which can be created by this widget */
typedef enum
{
  THUNAR_MENU_SECTION_OPEN                = 1 << 0,
  THUNAR_MENU_SECTION_SENDTO              = 1 << 1,
  THUNAR_MENU_SECTION_CREATE_NEW_FILES    = 1 << 2,
  THUNAR_MENU_SECTION_CUT                 = 1 << 3,
  THUNAR_MENU_SECTION_COPY_PASTE          = 1 << 4,
  THUNAR_MENU_SECTION_TRASH_DELETE        = 1 << 5,
  THUNAR_MENU_SECTION_EMPTY_TRASH         = 1 << 6,
  THUNAR_MENU_SECTION_RESTORE             = 1 << 7,
  THUNAR_MENU_SECTION_DUPLICATE           = 1 << 8,
  THUNAR_MENU_SECTION_MAKELINK            = 1 << 9,
  THUNAR_MENU_SECTION_RENAME              = 1 << 10,
  THUNAR_MENU_SECTION_CUSTOM_ACTIONS      = 1 << 11,
  THUNAR_MENU_SECTION_ZOOM                = 1 << 12,
  THUNAR_MENU_SECTION_PROPERTIES          = 1 << 13,
  THUNAR_MENU_SECTION_MOUNTABLE           = 1 << 14,
  THUNAR_MENU_SECTION_REMOVE_FROM_RECENT  = 1 << 15,
  THUNAR_MENU_SECTION_EDIT_LAUNCHER       = 1 << 17,

} ThunarMenuSections;


GType      thunar_menu_get_type          (void) G_GNUC_CONST;

gboolean   thunar_menu_add_sections      (ThunarMenu         *menu,
                                          ThunarMenuSections  menu_sections);
GtkWidget* thunar_menu_get_action_manager(ThunarMenu         *menu);

G_END_DECLS;

#endif /* !__THUNAR_CONTEXT_MENU_H__ */
