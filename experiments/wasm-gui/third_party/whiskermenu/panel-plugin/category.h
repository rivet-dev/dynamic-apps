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

#ifndef WHISKERMENU_CATEGORY_H
#define WHISKERMENU_CATEGORY_H

#include "launcher.h"

#include <vector>

#include <garcon/garcon.h>
#include <gtk/gtk.h>

namespace WhiskerMenu
{

class CategoryButton;

class Category : public Element
{
public:
	explicit Category(GarconMenu* menu);
	~Category();

	CategoryButton* get_button();

	GtkTreeModel* get_model();

	bool has_separators() const
	{
		return m_has_separators;
	}

	void append_category(Category* category)
	{
		unset_model();
		m_has_subcategories = true;
		m_items.push_back(category);
	}

	void append_item(Launcher* launcher)
	{
		unset_model();
		m_items.push_back(launcher);
	}

	void append_items(const std::vector<Launcher*>& launchers)
	{
		unset_model();
		m_items.reserve(m_items.size() + launchers.size());
		m_items.insert(m_items.end(), launchers.begin(), launchers.end());
	}

	void append_separator();

	void set_button(CategoryButton* button);

	void sort();

private:
	void insert_items(GtkTreeStore* model, GtkTreeIter* parent);
	void insert_items(GtkListStore* model);
	void unset_model();

private:
	CategoryButton* m_button;
	std::vector<Element*> m_items;
	GtkTreeModel* m_model;
	bool m_has_separators;
	bool m_has_subcategories;
	bool m_owns_button;
};

}

#endif // WHISKERMENU_CATEGORY_H
