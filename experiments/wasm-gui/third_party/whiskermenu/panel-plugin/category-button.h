/*
 * Copyright (C) 2013-2020 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_CATEGORY_BUTTON_H
#define WHISKERMENU_CATEGORY_BUTTON_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class CategoryButton
{
public:
	CategoryButton(GIcon* icon, const gchar* text);
	~CategoryButton();

	CategoryButton(const CategoryButton&) = delete;
	CategoryButton(CategoryButton&&) = delete;
	CategoryButton& operator=(const CategoryButton&) = delete;
	CategoryButton& operator=(CategoryButton&&) = delete;

	GtkWidget* get_widget() const
	{
		return GTK_WIDGET(m_button);
	}

	bool get_active() const
	{
		return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_button));
	}

	void set_active(bool active)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button), active);
	}

	void join_group(CategoryButton* button)
	{
		gtk_radio_button_join_group(m_button, button->m_button);
	}

	void reload_icon_size();

private:
	GtkRadioButton* m_button;
	GtkBox* m_box;
	GtkWidget* m_icon;
	GtkWidget* m_label;
};

}

#endif // WHISKERMENU_CATEGORY_BUTTON_H
