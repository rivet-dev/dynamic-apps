/*
 * Copyright (C) 2013-2021 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_APPLICATIONS_PAGE_H
#define WHISKERMENU_APPLICATIONS_PAGE_H

#include "page.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <garcon/garcon.h>

namespace WhiskerMenu
{

class Category;
class StringList;

class ApplicationsPage : public Page
{

public:
	explicit ApplicationsPage(Window* window);
	~ApplicationsPage();

	GtkTreeModel* create_launcher_model(StringList& desktop_ids) const;
	Launcher* find(const std::string& desktop_id) const;
	std::vector<Launcher*> find_all() const;

	void invalidate();
	bool load();
	void reload_category_icon_size();

private:
	void show_category(GtkToggleButton* togglebutton, std::vector<Category*>::size_type index);
	void clear();
	void load_garcon_menu();
	void load_contents();
	bool load_menu(GarconMenu* menu, Category* parent_category, bool load_hierarchy);

private:
	GarconMenu* m_garcon_menu;
	GarconMenu* m_garcon_settings_menu;
	std::vector<Category*> m_categories;
	std::unordered_map<std::string, Launcher*> m_items;

	enum class LoadStatus
	{
		Invalid,
		Loading,
		ReloadRequired,
		Done
	}
	m_status;
};

}

#endif // WHISKERMENU_APPLICATIONS_PAGE_H
