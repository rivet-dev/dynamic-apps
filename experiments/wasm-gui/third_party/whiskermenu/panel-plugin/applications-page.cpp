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

#include "applications-page.h"

#include "category.h"
#include "category-button.h"
#include "launcher.h"
#include "launcher-view.h"
#include "settings.h"
#include "slot.h"
#include "window.h"

#include <algorithm>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

ApplicationsPage::ApplicationsPage(Window* window) :
	Page(window, "applications-other", _("All Applications")),
	m_garcon_menu(nullptr),
	m_garcon_settings_menu(nullptr),
	m_status(LoadStatus::Invalid)
{
	garcon_set_environment_xdg(GARCON_ENVIRONMENT_XFCE);

	const decltype(m_categories.size()) index = 0;
	connect(get_button()->get_widget(), "toggled",
		[this](GtkToggleButton* button)
		{
			show_category(button, index);
		});
}

//-----------------------------------------------------------------------------

ApplicationsPage::~ApplicationsPage()
{
	clear();
}

//-----------------------------------------------------------------------------

GtkTreeModel* ApplicationsPage::create_launcher_model(StringList& desktop_ids) const
{
	// Create new model for treeview
	GtkListStore* store = gtk_list_store_new(
			LauncherView::N_COLUMNS,
			G_TYPE_ICON,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);

	// Fetch menu items or remove them from list if missing
	for (int i = 0; i < desktop_ids.size(); ++i)
	{
		const std::string& desktop_id = desktop_ids[i];
		if (desktop_id.empty())
		{
			continue;
		}

		Launcher* launcher = find(desktop_id);
		if (launcher)
		{
			gtk_list_store_insert_with_values(
					store, nullptr, G_MAXINT,
					LauncherView::COLUMN_ICON, launcher->get_icon(),
					LauncherView::COLUMN_TEXT, launcher->get_text(),
					LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
					LauncherView::COLUMN_LAUNCHER, launcher,
					-1);
		}
		else
		{
			desktop_ids.erase(i);
			--i;
		}
	}

	return GTK_TREE_MODEL(store);
}

//-----------------------------------------------------------------------------

Launcher* ApplicationsPage::find(const std::string& desktop_id) const
{
	auto i = m_items.find(desktop_id);
	return (i != m_items.end()) ? i->second : nullptr;
}

//-----------------------------------------------------------------------------

std::vector<Launcher*> ApplicationsPage::find_all() const
{
	std::vector<Launcher*> launchers;
	launchers.reserve(m_items.size());
	for (const auto& i : m_items)
	{
		launchers.push_back(i.second);
	}
	std::sort(launchers.begin(), launchers.end(), &Element::less_than);
	return launchers;
}

//-----------------------------------------------------------------------------

void ApplicationsPage::show_category(GtkToggleButton* togglebutton, std::vector<Category*>::size_type index)
{
	// Only apply filter for active button and valid category
	if (!gtk_toggle_button_get_active(togglebutton) || m_categories.empty())
	{
		return;
	}

	// Apply filter
	Category* category = m_categories[index];
	get_view()->unset_model();
	get_view()->set_fixed_height_mode(!category->has_separators());
	get_view()->set_model(category->get_model());
}

//-----------------------------------------------------------------------------

void ApplicationsPage::invalidate()
{
	if (m_status == LoadStatus::Done)
	{
		m_status = LoadStatus::Invalid;
	}
	else if (m_status == LoadStatus::Loading)
	{
		m_status = LoadStatus::ReloadRequired;
	}
}

//-----------------------------------------------------------------------------

bool ApplicationsPage::load()
{
	// Check if already loaded
	if (m_status == LoadStatus::Done)
	{
		return true;
	}
	// Check if currently loading
	else if (m_status != LoadStatus::Invalid)
	{
		return false;
	}
	m_status = LoadStatus::Loading;

	// Load menu
	clear();

	// Load contents in thread if possible
	GTask* task = g_task_new(nullptr, nullptr,
		+[](GObject*, GAsyncResult*, gpointer user_data)
		{
			static_cast<ApplicationsPage*>(user_data)->load_contents();
		},
		this);
	g_task_set_task_data(task, this, nullptr);
	g_task_run_in_thread(task,
		+[](GTask* task, gpointer, gpointer task_data, GCancellable*)
		{
			static_cast<ApplicationsPage*>(task_data)->load_garcon_menu();
			g_task_return_boolean(task, true);
		});
	g_object_unref(task);

	return false;
}

//-----------------------------------------------------------------------------

void ApplicationsPage::reload_category_icon_size()
{
	for (auto category : m_categories)
	{
		category->get_button()->reload_icon_size();
	}
}

//-----------------------------------------------------------------------------

void ApplicationsPage::clear()
{
	// Free categories
	for (auto category : m_categories)
	{
		delete category;
	}
	m_categories.clear();

	// Free menu items
	get_window()->unset_items();
	get_view()->unset_model();

	for (const auto& i : m_items)
	{
		delete i.second;
	}
	m_items.clear();

	// Free menu
	if (G_LIKELY(m_garcon_menu))
	{
		g_object_unref(m_garcon_menu);
		m_garcon_menu = nullptr;
	}

	// Free settings menu
	if (G_LIKELY(m_garcon_settings_menu))
	{
		g_object_unref(m_garcon_settings_menu);
		m_garcon_settings_menu = nullptr;
	}
}

//-----------------------------------------------------------------------------

void ApplicationsPage::load_garcon_menu()
{
	// Create menu
	if (wm_settings->custom_menu_file.empty())
	{
		m_garcon_menu = garcon_menu_new_applications();
	}
	else
	{
		m_garcon_menu = garcon_menu_new_for_path(wm_settings->custom_menu_file);
	}

	// Load menu
	if (m_garcon_menu && !garcon_menu_load(m_garcon_menu, nullptr, nullptr))
	{
		g_object_unref(m_garcon_menu);
		m_garcon_menu = nullptr;
	}

	if (!m_garcon_menu)
	{
		return;
	}

	connect(m_garcon_menu, "reload-required",
		[this](GarconMenu*)
		{
			invalidate();
		});

	load_menu(m_garcon_menu, nullptr, wm_settings->view_mode == Settings::ViewAsTree);

	// Create settings menu
	gchar* path = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, "menus/xfce-settings-manager.menu");
	m_garcon_settings_menu = garcon_menu_new_for_path(path ? path : SETTINGS_MENUFILE);
	g_free(path);

	if (m_garcon_settings_menu)
	{
		connect(m_garcon_settings_menu, "reload-required",
			[this](GarconMenu*)
			{
				invalidate();
			});
	}

	// Load settings menu
	if (m_garcon_settings_menu && garcon_menu_load(m_garcon_settings_menu, nullptr, nullptr))
	{
		Category* category = new Category(nullptr);
		load_menu(m_garcon_settings_menu, category, false);
		delete category;
	}

	// Sort items and categories
	if (wm_settings->view_mode != Settings::ViewAsTree)
	{
		for (auto category : m_categories)
		{
			category->sort();
		}
	}
	if (wm_settings->sort_categories)
	{
		std::sort(m_categories.begin(), m_categories.end(), &Element::less_than);
	}

	// Create all items category
	Category* category = new Category(nullptr);
	category->set_button(get_button());
	category->append_items(find_all());
	m_categories.insert(m_categories.begin(), category);
}

//-----------------------------------------------------------------------------

void ApplicationsPage::load_contents()
{
	if (!m_garcon_menu)
	{
		get_window()->set_loaded();

		m_status = LoadStatus::Invalid;

		return;
	}

	// Set all applications category
	get_view()->set_fixed_height_mode(true);
	get_view()->set_model(m_categories.front()->get_model());

	// Add buttons for categories
	std::vector<CategoryButton*> category_buttons;
	const auto size = m_categories.size();
	for (decltype(m_categories.size()) i = 1; i < size; ++i)
	{
		CategoryButton* category_button = m_categories[i]->get_button();
		connect(category_button->get_widget(), "toggled",
			[this, i](GtkToggleButton* button)
			{
				show_category(button, i);
			});
		category_buttons.push_back(category_button);
	}

	// Add category buttons to window
	get_window()->set_categories(category_buttons);

	// Update menu items of other panels
	get_window()->set_items();
	get_window()->set_loaded();

	m_status = (m_status == LoadStatus::Loading) ? LoadStatus::Done : LoadStatus::Invalid;
}

//-----------------------------------------------------------------------------

bool ApplicationsPage::load_menu(GarconMenu* menu, Category* parent_category, bool load_hierarchy)
{
	bool has_children = false;

	// Add menu elements
	GList* elements = garcon_menu_get_elements(menu);
	for (GList* li = elements; li; li = li->next)
	{
		// Add menu item
		if (GARCON_IS_MENU_ITEM(li->data))
		{
			GarconMenuItem* menuitem = GARCON_MENU_ITEM(li->data);

			// Listen for changes
			connect(menuitem, "changed",
				[this](GarconMenuItem*)
				{
					invalidate();
				});

			// Skip hidden items
			if (!garcon_menu_element_get_visible(GARCON_MENU_ELEMENT(menuitem)))
			{
				continue;
			}

			// Create launcher
			std::string desktop_id(garcon_menu_item_get_desktop_id(menuitem));
			auto iter = m_items.find(desktop_id);
			if (iter == m_items.end())
			{
				iter = m_items.emplace(std::move(desktop_id), new Launcher(menuitem)).first;
			}

			// Add launcher to current category
			if (parent_category)
			{
				parent_category->append_item(iter->second);
			}

			has_children = true;
		}
		// Add separator
		else if (GARCON_IS_MENU_SEPARATOR(li->data) && load_hierarchy && parent_category)
		{
			parent_category->append_separator();
		}
		// Add submenu
		else if (GARCON_IS_MENU(li->data))
		{
			GarconMenu* submenu = GARCON_MENU(li->data);

			// Skip hidden categories
			GarconMenuDirectory* directory = garcon_menu_get_directory(submenu);
			if (directory && !garcon_menu_directory_get_visible(directory))
			{
				continue;
			}

			// Create category
			Category* category = nullptr;
			if (!load_hierarchy && parent_category)
			{
				category = parent_category;
			}
			else
			{
				category = new Category(submenu);
			}

			// Populate category
			if (load_menu(submenu, category, load_hierarchy))
			{
				if (!parent_category)
				{
					m_categories.push_back(category);
				}
				else if (category != parent_category)
				{
					parent_category->append_category(category);
				}

				has_children = true;
			}
			// Remove empty categories
			else if (category != parent_category)
			{
				delete category;
			}
		}
	}
	g_list_free(elements);

	return has_children;
}

//-----------------------------------------------------------------------------
