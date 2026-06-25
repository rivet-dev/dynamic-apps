/*
 * Copyright (C) 2020 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WHISKERMENU_IMAGE_MENU_ITEM_H
#define WHISKERMENU_IMAGE_MENU_ITEM_H

#include <gtk/gtk.h>

inline GtkWidget* whiskermenu_image_menu_item_new(const gchar* icon, const gchar* text)
{
	GtkWidget* image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	GtkWidget* menuitem = gtk_image_menu_item_new_with_label(text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
G_GNUC_END_IGNORE_DEPRECATIONS
	return menuitem;
}

inline GtkWidget* whiskermenu_image_menu_item_new_with_mnemonic(const gchar* icon, const gchar* text)
{
	GtkWidget* image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	GtkWidget* menuitem = gtk_image_menu_item_new_with_mnemonic(text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
G_GNUC_END_IGNORE_DEPRECATIONS
	return menuitem;
}

#endif
