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

#include "recent-page.h"

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

RecentPage::RecentPage(Window* window) :
	Page(window, "document-open-recent", _("Recently Used"))
{
	// Prevent going over max
	if (wm_settings->recent.size() > wm_settings->recent_items_max)
	{
		wm_settings->recent.resize(wm_settings->recent_items_max);
	}
}

//-----------------------------------------------------------------------------

RecentPage::~RecentPage()
{
	unset_menu_items();
}

//-----------------------------------------------------------------------------

void RecentPage::add(Launcher* launcher)
{
	if (!wm_settings->recent_items_max || !launcher)
	{
		return;
	}

	std::string desktop_id = launcher->get_desktop_id();
	if (!wm_settings->recent.empty())
	{
		auto i = std::find(wm_settings->recent.begin(), wm_settings->recent.end(), desktop_id);

		// Skip if already first launcher
		if (i == wm_settings->recent.begin())
		{
			return;
		}
		// Move to front if already in list
		else if (i != wm_settings->recent.end())
		{
			const int pos = std::distance(wm_settings->recent.begin(), i);
			GtkTreeModel* model = get_view()->get_model();
			GtkTreeIter iter;
			gtk_tree_model_iter_nth_child(model, &iter, nullptr, pos);
			gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, nullptr);
			wm_settings->recent.erase(pos);
			wm_settings->recent.insert(0, desktop_id);
			return;
		}
	}

	// Prepend to list of items
	GtkListStore* store = GTK_LIST_STORE(get_view()->get_model());
	gtk_list_store_insert_with_values(
			store, nullptr, 0,
			LauncherView::COLUMN_ICON, launcher->get_icon(),
			LauncherView::COLUMN_TEXT, launcher->get_text(),
			LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
			LauncherView::COLUMN_LAUNCHER, launcher,
			-1);
	wm_settings->recent.insert(0, desktop_id);

	// Prevent going over max
	enforce_item_count();
}

//-----------------------------------------------------------------------------

void RecentPage::enforce_item_count()
{
	if (wm_settings->recent.size() <= wm_settings->recent_items_max)
	{
		return;
	}

	GtkListStore* store = GTK_LIST_STORE(get_view()->get_model());
	for (int i = wm_settings->recent.size() - 1, end = wm_settings->recent_items_max; i >= end; --i)
	{
		GtkTreeIter iter;
		if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, nullptr, i))
		{
			gtk_list_store_remove(store, &iter);
		}
	}

	wm_settings->recent.resize(wm_settings->recent_items_max);
}

//-----------------------------------------------------------------------------

void RecentPage::set_menu_items()
{
	GtkTreeModel* model = get_window()->get_applications()->create_launcher_model(wm_settings->recent);
	get_view()->set_model(model);
	g_object_unref(model);
}

//-----------------------------------------------------------------------------

void RecentPage::unset_menu_items()
{
	// Clear treeview
	get_view()->unset_model();
}

//-----------------------------------------------------------------------------

void RecentPage::extend_context_menu(GtkWidget* menu)
{
	GtkWidget* menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("edit-clear", _("Clear Recently Used"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			gtk_list_store_clear(GTK_LIST_STORE(get_view()->get_model()));
			wm_settings->recent.clear();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

//-----------------------------------------------------------------------------
