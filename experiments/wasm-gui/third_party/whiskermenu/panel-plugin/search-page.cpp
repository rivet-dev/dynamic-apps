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

#include "search-page.h"

#include "applications-page.h"
#include "launcher.h"
#include "launcher-view.h"
#include "search-action.h"
#include "settings.h"
#include "slot.h"
#include "util.h"
#include "window.h"

#include <algorithm>

#include <gdk/gdkkeysyms.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

SearchPage::SearchPage(Window* window) :
	Page(window)
{
	view_created();

	connect(window->get_search_entry(), "activate",
		[this](GtkEntry* entry)
		{
			set_filter(gtk_entry_get_text(entry));
			GtkTreePath* path = get_view()->get_selected_path();
			if (path)
			{
				get_view()->activate_path(path);
				gtk_tree_path_free(path);
			}
		});

	connect(window->get_search_entry(), "stop-search",
		[](GtkSearchEntry* entry)
		{
			const gchar* text = gtk_entry_get_text(GTK_ENTRY(entry));
			if (!xfce_str_is_empty(text))
			{
				gtk_entry_set_text(GTK_ENTRY(entry), "");
			}
		});
}

//-----------------------------------------------------------------------------

SearchPage::~SearchPage()
{
	unset_menu_items();
}

//-----------------------------------------------------------------------------

void SearchPage::set_filter(const gchar* filter)
{
	// Clear search results for empty filter
	if (!filter)
	{
		m_query.clear();
		m_matches.clear();
		return;
	}

	// Make sure this is a new search
	std::string query(filter);
	if (m_query.raw_query() == query)
	{
		return;
	}

	// Reset search results if new search does not start with previous search
	if (m_query.raw_query().empty() || !g_str_has_prefix(filter, m_query.raw_query().c_str()))
	{
		update_search_order();
		m_matches.clear();
		m_matches.push_back(&m_run_action);
		for (auto launcher : m_launchers)
		{
			m_matches.push_back(launcher);
		}
	}
	else if (std::find(m_matches.cbegin(), m_matches.cend(), &m_run_action) == m_matches.cend())
	{
		m_matches.insert(m_matches.begin(), &m_run_action);
	}
	m_query.set(query);

	// Create search results
	std::vector<Match> search_action_matches;
	search_action_matches.reserve(wm_settings->search_actions.size());
	for (auto action : wm_settings->search_actions)
	{
		Match match(action);
		match.update(m_query);
		if (!Match::invalid(match))
		{
			search_action_matches.push_back(std::move(match));
		}
	}
	std::stable_sort(search_action_matches.begin(), search_action_matches.end());
	std::reverse(search_action_matches.begin(), search_action_matches.end());

	for (auto& match : m_matches)
	{
		match.update(m_query);
	}
	m_matches.erase(std::remove_if(m_matches.begin(), m_matches.end(), &Match::invalid), m_matches.end());
	std::stable_sort(m_matches.begin(), m_matches.end());

	// Show search results
	GtkListStore* store = gtk_list_store_new(
			LauncherView::N_COLUMNS,
			G_TYPE_ICON,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);
	Element* element = nullptr;
	for (const auto& match : search_action_matches)
	{
		element = match.element();
		gtk_list_store_insert_with_values(
				store, nullptr, G_MAXINT,
				LauncherView::COLUMN_ICON, element->get_icon(),
				LauncherView::COLUMN_TEXT, element->get_text(),
				LauncherView::COLUMN_TOOLTIP, element->get_tooltip(),
				LauncherView::COLUMN_LAUNCHER, element,
				-1);
	}
	for (const auto& match : m_matches)
	{
		element = match.element();
		gtk_list_store_insert_with_values(
				store, nullptr, G_MAXINT,
				LauncherView::COLUMN_ICON, element->get_icon(),
				LauncherView::COLUMN_TEXT, element->get_text(),
				LauncherView::COLUMN_TOOLTIP, element->get_tooltip(),
				LauncherView::COLUMN_LAUNCHER, element,
				-1);
	}
	get_view()->set_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	// Find first result
	select_first();
}

//-----------------------------------------------------------------------------

void SearchPage::set_menu_items()
{
	m_launchers = get_window()->get_applications()->find_all();

	get_view()->unset_model();

	m_matches.clear();
	m_matches.reserve(m_launchers.size() + 1);
}

//-----------------------------------------------------------------------------

void SearchPage::unset_menu_items()
{
	m_launchers.clear();
	m_matches.clear();
	get_view()->unset_model();
}

//-----------------------------------------------------------------------------

unsigned int SearchPage::move_launcher(const std::string& desktop_id, unsigned int pos)
{
	for (auto launcher = m_launchers.begin() + pos, end = m_launchers.end(); launcher != end; ++launcher)
	{
		if (desktop_id == (*launcher)->get_desktop_id())
		{
			std::rotate(m_launchers.begin() + pos, launcher, launcher + 1);
			pos++;
			break;
		}
	}
	return pos;
}

//-----------------------------------------------------------------------------

void SearchPage::update_search_order()
{
	if (wm_settings->recent.is_order_unchanged() && wm_settings->favorites.is_order_unchanged())
	{
		return;
	}
	wm_settings->recent.set_order_unchaged();
	wm_settings->favorites.set_order_unchaged();

	// Reset in case a launcher is no longer in favorites or recent
	std::sort(m_launchers.begin(), m_launchers.end(), &Element::less_than);

	// Move launchers for favorites and recent to front
	unsigned int pos = 0;
	for (const std::string& desktop_id : wm_settings->recent)
	{
		pos = move_launcher(desktop_id, pos);
	}
	for (const std::string& desktop_id : wm_settings->favorites)
	{
		pos = move_launcher(desktop_id, pos);
	}
}

//-----------------------------------------------------------------------------

void SearchPage::view_created()
{
	get_view()->set_selection_mode(GTK_SELECTION_BROWSE);
}

//-----------------------------------------------------------------------------
