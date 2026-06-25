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

#include "favorites-page.h"

#include "applications-page.h"
#include "image-menu-item.h"
#include "launcher.h"
#include "launcher-view.h"
#include "settings.h"
#include "slot.h"
#include "window.h"

#include <algorithm>

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

FavoritesPage::FavoritesPage(Window* window) :
	Page(window, "user-bookmarks", _("Favorites"))
{
	view_created();
}

//-----------------------------------------------------------------------------

FavoritesPage::~FavoritesPage()
{
	unset_menu_items();
}

//-----------------------------------------------------------------------------

bool FavoritesPage::contains(Launcher* launcher) const
{
	if (!launcher)
	{
		return false;
	}

	std::string desktop_id(launcher->get_desktop_id());
	return std::find(wm_settings->favorites.begin(), wm_settings->favorites.end(), desktop_id) != wm_settings->favorites.end();
}

//-----------------------------------------------------------------------------

void FavoritesPage::add(Launcher* launcher)
{
	if (!launcher || contains(launcher))
	{
		return;
	}

	// Append to list of items
	GtkListStore* store = GTK_LIST_STORE(get_view()->get_model());
	gtk_list_store_insert_with_values(
			store, nullptr, G_MAXINT,
			LauncherView::COLUMN_ICON, launcher->get_icon(),
			LauncherView::COLUMN_TEXT, launcher->get_text(),
			LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
			LauncherView::COLUMN_LAUNCHER, launcher,
			-1);
}

//-----------------------------------------------------------------------------

void FavoritesPage::remove(Launcher* launcher)
{
	GtkTreeModel* model = GTK_TREE_MODEL(get_view()->get_model());
	GtkListStore* store = GTK_LIST_STORE(model);
	GtkTreeIter iter;
	Launcher* test_launcher = nullptr;

	bool valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		gtk_tree_model_get(model, &iter, LauncherView::COLUMN_LAUNCHER, &test_launcher, -1);
		if (test_launcher == launcher)
		{
			gtk_list_store_remove(store, &iter);
			break;
		}
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::set_menu_items()
{
	GtkTreeModel* model = get_window()->get_applications()->create_launcher_model(wm_settings->favorites);
	get_view()->set_model(model);

	connect(model, "row-changed",
		[this](GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter)
		{
			on_row_changed(model, path, iter);
		});

	connect(model, "row-inserted",
		[this](GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter)
		{
			on_row_inserted(model, path, iter);
		});

	connect(model, "row-deleted",
		[this](GtkTreeModel*, GtkTreePath* path)
		{
			on_row_deleted(path);
		});

	g_object_unref(model);
}

//-----------------------------------------------------------------------------

void FavoritesPage::unset_menu_items()
{
	// Clear treeview
	get_view()->unset_model();
}

//-----------------------------------------------------------------------------

void FavoritesPage::extend_context_menu(GtkWidget* menu)
{
	GtkWidget* menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("view-sort-ascending", _("Sort Alphabetically A-Z"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			sort_ascending();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("view-sort-descending", _("Sort Alphabetically Z-A"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			sort_descending();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

//-----------------------------------------------------------------------------

bool FavoritesPage::remember_launcher(Launcher* launcher)
{
	return wm_settings->favorites_in_recent ? true : !contains(launcher);
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_row_changed(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter)
{
	const int pos = gtk_tree_path_get_indices(path)[0];
	if (pos >= wm_settings->favorites.size())
	{
		return;
	}

	Element* element = nullptr;
	gtk_tree_model_get(model, iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	if (Launcher* launcher = dynamic_cast<Launcher*>(element))
	{
		wm_settings->favorites.set(pos, launcher->get_desktop_id());
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_row_inserted(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter)
{
	const int pos = gtk_tree_path_get_indices(path)[0];

	std::string desktop_id;
	Element* element = nullptr;
	gtk_tree_model_get(model, iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	if (Launcher* launcher = dynamic_cast<Launcher*>(element))
	{
		desktop_id = launcher->get_desktop_id();
	}

	if (pos >= wm_settings->favorites.size())
	{
		wm_settings->favorites.push_back(desktop_id);
	}
	else if (wm_settings->favorites[pos] != desktop_id)
	{
		wm_settings->favorites.insert(pos, desktop_id);
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_row_deleted(GtkTreePath* path)
{
	const int pos = gtk_tree_path_get_indices(path)[0];
	if (pos < wm_settings->favorites.size())
	{
		wm_settings->favorites.erase(pos);
	}
}

//-----------------------------------------------------------------------------

std::vector<Launcher*> FavoritesPage::sort() const
{
	std::vector<Launcher*> items;
	items.reserve(wm_settings->favorites.size());
	for (const auto& favorite : wm_settings->favorites)
	{
		Launcher* launcher = get_window()->get_applications()->find(favorite);
		if (!launcher)
		{
			continue;
		}
		items.push_back(launcher);
	}
	std::sort(items.begin(), items.end(), &Element::less_than);
	return items;
}

//-----------------------------------------------------------------------------

void FavoritesPage::sort_ascending()
{
	const auto items = sort();

	wm_settings->favorites.clear();
	for (auto launcher : items)
	{
		wm_settings->favorites.push_back(launcher->get_desktop_id());
	}
	set_menu_items();
}

//-----------------------------------------------------------------------------

void FavoritesPage::sort_descending()
{
	const auto items = sort();

	wm_settings->favorites.clear();
	for (auto i = items.rbegin(), end = items.rend(); i != end; ++i)
	{
		wm_settings->favorites.push_back((*i)->get_desktop_id());
	}
	set_menu_items();
}

//-----------------------------------------------------------------------------

void FavoritesPage::view_created()
{
	set_reorderable(true);
}

//-----------------------------------------------------------------------------
