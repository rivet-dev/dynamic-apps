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

#include "category.h"

#include "category-button.h"
#include "launcher-view.h"
#include "util.h"

#include <algorithm>

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Category::Category(GarconMenu* menu) :
	m_button(nullptr),
	m_model(nullptr),
	m_has_separators(false),
	m_has_subcategories(false),
	m_owns_button(true)
{
	const gchar* icon = nullptr;
	const gchar* text = nullptr;
	const gchar* tooltip = nullptr;
	if (menu)
	{
		GarconMenuElement* element = GARCON_MENU_ELEMENT(menu);
		icon = garcon_menu_element_get_icon_name(element);
		text = garcon_menu_element_get_name(element);
		tooltip = garcon_menu_element_get_comment(element);
	}
	else
	{
		text = _("All Applications");
	}
	set_icon(!xfce_str_is_empty(icon) ? icon : "applications-other");
	set_text(text ? text : "");
	set_tooltip(tooltip ? tooltip : "");
}

//-----------------------------------------------------------------------------

Category::~Category()
{
	unset_model();

	if (m_owns_button)
	{
		delete m_button;
	}

	for (auto element : m_items)
	{
		if (Category* category = dynamic_cast<Category*>(element))
		{
			delete category;
		}
	}
}

//-----------------------------------------------------------------------------

CategoryButton* Category::get_button()
{
	if (!m_button)
	{
		m_button = new CategoryButton(get_icon(), get_text());
	}

	return m_button;
}

//-----------------------------------------------------------------------------

GtkTreeModel* Category::get_model()
{
	if (!m_model)
	{
		if (m_has_subcategories)
		{
			GtkTreeStore* model = gtk_tree_store_new(
					LauncherView::N_COLUMNS,
					G_TYPE_ICON,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_POINTER);
			insert_items(model, nullptr);
			m_model = GTK_TREE_MODEL(model);
		}
		else
		{
			GtkListStore* model = gtk_list_store_new(
					LauncherView::N_COLUMNS,
					G_TYPE_ICON,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_POINTER);
			insert_items(model);
			m_model = GTK_TREE_MODEL(model);
		}
	}

	return m_model;
}

//-----------------------------------------------------------------------------

void Category::append_separator()
{
	if (!m_items.empty() && m_items.back())
	{
		unset_model();
		m_items.push_back(nullptr);
		m_has_separators = true;
	}
}

//-----------------------------------------------------------------------------

void Category::set_button(CategoryButton* button)
{
	if (m_owns_button)
	{
		delete m_button;
	}

	m_owns_button = false;

	m_button = button;
}

//-----------------------------------------------------------------------------

void Category::sort()
{
	unset_model();
	std::sort(m_items.begin(), m_items.end(), &Element::less_than);
}

//-----------------------------------------------------------------------------

void Category::insert_items(GtkTreeStore* model, GtkTreeIter* parent)
{
	if (!m_items.empty() && !m_items.back())
	{
		m_items.pop_back();
	}

	for (auto element : m_items)
	{
		if (Category* category = dynamic_cast<Category*>(element))
		{
			gchar* text = g_markup_escape_text(category->get_text(), -1);
			const gchar* tooltip = category->get_tooltip();

			GtkTreeIter iter;
			gtk_tree_store_insert_with_values(model,
					&iter, parent, G_MAXINT,
					LauncherView::COLUMN_ICON, category->get_icon(),
					LauncherView::COLUMN_TEXT, text,
					LauncherView::COLUMN_TOOLTIP, tooltip,
					LauncherView::COLUMN_LAUNCHER, nullptr,
					-1);
			g_free(text);
			category->insert_items(model, &iter);
		}
		else if (Launcher* launcher = dynamic_cast<Launcher*>(element))
		{
			gtk_tree_store_insert_with_values(model,
					nullptr, parent, G_MAXINT,
					LauncherView::COLUMN_ICON, launcher->get_icon(),
					LauncherView::COLUMN_TEXT, launcher->get_text(),
					LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
					LauncherView::COLUMN_LAUNCHER, launcher,
					-1);
		}
		else
		{
			gtk_tree_store_insert_with_values(model,
					nullptr, parent, G_MAXINT,
					LauncherView::COLUMN_ICON, nullptr,
					LauncherView::COLUMN_TEXT, nullptr,
					LauncherView::COLUMN_TOOLTIP, nullptr,
					LauncherView::COLUMN_LAUNCHER, nullptr,
					-1);
		}
	}
}

//-----------------------------------------------------------------------------

void Category::insert_items(GtkListStore* model)
{
	if (!m_items.empty() && !m_items.back())
	{
		m_items.pop_back();
	}

	for (auto element : m_items)
	{
		if (Launcher* launcher = dynamic_cast<Launcher*>(element))
		{
			gtk_list_store_insert_with_values(model,
					nullptr, G_MAXINT,
					LauncherView::COLUMN_ICON, launcher->get_icon(),
					LauncherView::COLUMN_TEXT, launcher->get_text(),
					LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
					LauncherView::COLUMN_LAUNCHER, launcher,
					-1);
		}
		else
		{
			gtk_list_store_insert_with_values(model,
					nullptr, G_MAXINT,
					LauncherView::COLUMN_ICON, nullptr,
					LauncherView::COLUMN_TEXT, nullptr,
					LauncherView::COLUMN_TOOLTIP, nullptr,
					LauncherView::COLUMN_LAUNCHER, nullptr,
					-1);
		}
	}
}

//-----------------------------------------------------------------------------

void Category::unset_model()
{
	if (m_model)
	{
		g_object_unref(m_model);
		m_model = nullptr;
	}
}

//-----------------------------------------------------------------------------
