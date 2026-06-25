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

#ifndef WHISKERMENU_RECENT_PAGE_H
#define WHISKERMENU_RECENT_PAGE_H

#include "page.h"

namespace WhiskerMenu
{

class RecentPage : public Page
{
public:
	explicit RecentPage(Window* window);
	~RecentPage();

	void add(Launcher* launcher);
	void enforce_item_count();
	void set_menu_items();
	void unset_menu_items();

private:
	void extend_context_menu(GtkWidget* menu) override;
};

}

#endif // WHISKERMENU_RECENT_PAGE_H
