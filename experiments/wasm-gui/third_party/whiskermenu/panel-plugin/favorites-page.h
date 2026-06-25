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

#ifndef WHISKERMENU_FAVORITES_PAGE_H
#define WHISKERMENU_FAVORITES_PAGE_H

#include "page.h"

#include <vector>

namespace WhiskerMenu
{

class FavoritesPage : public Page
{
public:
	explicit FavoritesPage(Window* window);
	~FavoritesPage();

	bool contains(Launcher* launcher) const;

	void add(Launcher* launcher);
	void remove(Launcher* launcher);
	void set_menu_items();
	void unset_menu_items();

private:
	void extend_context_menu(GtkWidget* menu) override;
	bool remember_launcher(Launcher* launcher) override;
	void on_row_changed(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter);
	void on_row_inserted(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter);
	void on_row_deleted(GtkTreePath* path);
	std::vector<Launcher*> sort() const;
	void sort_ascending();
	void sort_descending();
	void view_created() override;
};

}

#endif // WHISKERMENU_FAVORITES_PAGE_H
