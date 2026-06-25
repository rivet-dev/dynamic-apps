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

#include "settings.h"

#include "command.h"
#include "plugin.h"
#include "search-action.h"
#include "slot.h"

#include <algorithm>

#include <exo/exo.h>

#include <cstdio>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Settings* WhiskerMenu::wm_settings = nullptr;

//-----------------------------------------------------------------------------

Settings::Settings(Plugin* plugin) :
	m_plugin(plugin),
	m_change_slot(0),
	m_button_title_default(_("Applications")),
	channel(nullptr),

	favorites("/favorites", {
#if EXO_CHECK_VERSION(4,15,0)
		"xfce4-web-browser.desktop",
		"xfce4-mail-reader.desktop",
		"xfce4-file-manager.desktop",
		"xfce4-terminal-emulator.desktop"
#else
		"exo-web-browser.desktop",
		"exo-mail-reader.desktop",
		"exo-file-manager.desktop",
		"exo-terminal-emulator.desktop"
#endif
	}),
	recent("/recent", { }),

	custom_menu_file("/custom-menu-file"),

	button_title("/button-title", m_button_title_default),
	button_icon_name("/button-icon", "org.xfce.panel.whiskermenu"),
	button_title_visible("/show-button-title", false),
	button_icon_visible("/show-button-icon", true),
	button_single_row("/button-single-row", false),

	launcher_show_name("/launcher-show-name", true),
	launcher_show_description("/launcher-show-description", true),
	launcher_show_tooltip("/launcher-show-tooltip", true),
	launcher_icon_size("/launcher-icon-size", IconSize::Small),

	category_hover_activate("/hover-switch-category", false),
	category_show_name("/category-show-name", true),
	sort_categories("/sort-categories", true),
	category_icon_size("/category-icon-size", IconSize::Smaller),

	view_mode("/view-mode", ViewAsList, ViewAsIcons, ViewAsTree),

	default_category("/default-category", CategoryFavorites, CategoryFavorites, CategoryAll),

	recent_items_max("/recent-items-max", 10, 0, 100),
	favorites_in_recent("/favorites-in-recent", false),

	position_profile_alternate("/position-profile-alternate", false),
	position_search_alternate("/position-search-alternate", false),
	position_commands_alternate("/position-commands-alternate", false),
	position_categories_alternate("/position-categories-alternate", false),
	position_categories_horizontal("/position-categories-horizontal", false),
	stay_on_focus_out("/stay-on-focus-out", false),

	profile_shape("/profile-shape", ProfileRound, ProfileRound, ProfileHidden),

	confirm_session_command("/confirm-session-command", true),

	search_actions {
		new SearchAction(_("Man Pages"), "#", "exo-open --launch TerminalEmulator man %s", false),
		new SearchAction(_("Search the Web"), "?", "exo-open --launch WebBrowser https://duckduckgo.com/?q=%u", false),
		new SearchAction(_("Search for Files"), "-", "catfish --path=~ --start %s", false),
		new SearchAction(_("Wikipedia"), "!w", "exo-open --launch WebBrowser https://en.wikipedia.org/wiki/%u", false),
		new SearchAction(_("Run in Terminal"), "!", "exo-open --launch TerminalEmulator %s", false),
		new SearchAction(_("Open URI"), "^(file|http|https):\\/\\/(.*)$", "exo-open \\0", true)
	},

	menu_width("/menu-width", 450, 10, INT_MAX),
	menu_height("/menu-height", 500, 10, INT_MAX),
	menu_opacity("/menu-opacity", 100, 0, 100)
{
	command[CommandSettings] = new Command("/command-settings", "/show-command-settings",
			"org.xfce.settings.manager", "preferences-desktop",
			_("_Settings Manager"),
			"xfce4-settings-manager", true,
			_("Failed to open settings manager."));
	command[CommandLockScreen] = new Command("/command-lockscreen", "/show-command-lockscreen",
			"xfsm-lock", "system-lock-screen",
			_("_Lock Screen"),
			"xflock4", true,
			_("Failed to lock screen."));
	command[CommandSwitchUser] = new Command("/command-switchuser", "/show-command-switchuser",
			"xfsm-switch-user", "system-users",
			_("Switch _User"),
			"xfce4-session-logout --switch-user", false,
			_("Failed to switch user."));
	command[CommandLogOutUser] = new Command("/command-logoutuser", "/show-command-logoutuser",
			"xfsm-logout", "system-log-out",
			_("Log _Out"),
			"xfce4-session-logout --logout --fast", false,
			_("Failed to log out."),
			_("Are you sure you want to log out?"),
			_("Logging out in %d seconds."));
	command[CommandRestart] = new Command("/command-restart", "/show-command-restart",
			"xfsm-reboot", "system-reboot",
			_("_Restart"),
			"xfce4-session-logout --reboot --fast", false,
			_("Failed to restart."),
			_("Are you sure you want to restart?"),
			_("Restarting computer in %d seconds."));
	command[CommandShutDown] = new Command("/command-shutdown", "/show-command-shutdown",
			"xfsm-shutdown", "system-shutdown",
			_("Shut _Down"),
			"xfce4-session-logout --halt --fast", false,
			_("Failed to shut down."),
			_("Are you sure you want to shut down?"),
			_("Turning off computer in %d seconds."));
	command[CommandSuspend] = new Command("/command-suspend", "/show-command-suspend",
			"xfsm-suspend", "system-suspend",
			_("Suspe_nd"),
			"xfce4-session-logout --suspend", false,
			_("Failed to suspend."),
			_("Do you want to suspend to RAM?"),
			_("Suspending computer in %d seconds."));
	command[CommandHibernate] = new Command("/command-hibernate", "/show-command-hibernate",
			"xfsm-hibernate", "system-hibernate",
			_("_Hibernate"),
			"xfce4-session-logout --hibernate", false,
			_("Failed to hibernate."),
			_("Do you want to suspend to disk?"),
			_("Hibernating computer in %d seconds."));
	command[CommandLogOut] = new Command("/command-logout", "/show-command-logout",
			"xfsm-logout", "system-log-out",
			_("Log Ou_t..."),
			"xfce4-session-logout", true,
			_("Failed to log out."));
	command[CommandMenuEditor] = new Command("/command-menueditor", "/show-command-menueditor",
			"menu-editor", "xfce4-menueditor",
			_("_Edit Applications"),
			"menulibre", true,
			_("Failed to launch menu editor."));
	command[CommandProfile] = new Command("/command-profile", "/show-command-profile",
			"avatar-default", "preferences-desktop-user",
			_("Edit _Profile"),
			"mugshot", true,
			_("Failed to edit profile."));
}

//-----------------------------------------------------------------------------

Settings::~Settings()
{
	for (auto i : command)
	{
		delete i;
	}

	if (channel)
	{
		g_object_unref(channel);
		xfconf_shutdown();
	}
}

//-----------------------------------------------------------------------------

void Settings::load(const gchar* file, bool is_default)
{
	if (!file)
	{
		return;
	}

	XfceRc* rc = xfce_rc_simple_open(file, true);
	if (!rc)
	{
		return;
	}
	xfce_rc_set_group(rc, nullptr);

	favorites.load(rc, is_default);
	recent.load(rc, is_default);

	custom_menu_file.load(rc, is_default);

	button_title.load(rc, is_default);
	button_icon_name.load(rc, is_default);
	button_single_row.load(rc, is_default);
	button_title_visible.load(rc, is_default);
	button_icon_visible.load(rc, is_default);

	launcher_show_name.load(rc, is_default);
	launcher_show_description.load(rc, is_default);
	launcher_show_tooltip.load(rc, is_default);
	if (xfce_rc_has_entry(rc, "item-icon-size"))
	{
		launcher_icon_size = xfce_rc_read_int_entry(rc, "item-icon-size", launcher_icon_size);
	}
	launcher_icon_size.load(rc, is_default);

	category_hover_activate.load(rc, is_default);
	category_show_name.load(rc, is_default);
	category_icon_size.load(rc, is_default);

	if (!xfce_rc_has_entry(rc, "view-mode"))
	{
		if (xfce_rc_read_bool_entry(rc, "load-hierarchy", view_mode == ViewAsTree))
		{
			view_mode = ViewAsTree;
			if (!xfce_rc_has_entry(rc, "sort-categories"))
			{
				sort_categories = false;
			}
		}
		else if (xfce_rc_read_bool_entry(rc, "view-as-icons", view_mode == ViewAsIcons))
		{
			view_mode = ViewAsIcons;
		}
	}
	view_mode.load(rc, is_default);
	sort_categories.load(rc, is_default);

	if (xfce_rc_has_entry(rc, "display-recent-default"))
	{
		default_category = xfce_rc_read_bool_entry(rc, "display-recent-default", default_category);
	}
	default_category.load(rc, is_default);

	recent_items_max.load(rc, is_default);
	favorites_in_recent.load(rc, is_default);

	position_profile_alternate.load(rc, is_default);
	position_search_alternate.load(rc, is_default);
	position_commands_alternate.load(rc, is_default);
	position_categories_alternate.load(rc, is_default);
	position_categories_horizontal.load(rc, is_default);
	stay_on_focus_out.load(rc, is_default);

	profile_shape.load(rc, is_default);

	confirm_session_command.load(rc, is_default);

	menu_width.load(rc, is_default);
	menu_height.load(rc, is_default);
	menu_opacity.load(rc, is_default);

	for (auto i : command)
	{
		i->load(rc, is_default);
	}

	search_actions.load(rc, is_default);

	xfce_rc_close(rc);

	prevent_invalid();

	if (!is_default)
	{
		favorites.save();
		recent.save();
		search_actions.save();
	}
	else if (!button_title.empty())
	{
		m_button_title_default = button_title;
	}
}

//-----------------------------------------------------------------------------

void Settings::load(const gchar* base)
{
	// Set up Xfconf channel
	if (base && xfconf_init(nullptr))
	{
		channel = xfconf_channel_new_with_property_base(xfce_panel_get_channel_name(), base);
		m_change_slot = connect(channel, "property-changed",
			[this](XfconfChannel*, const gchar* property, const GValue* value)
			{
				property_changed(property, value);
				prevent_invalid();
			});
	}
	else
	{
		return;
	}

	// Fetch all settings
	GHashTable* properties = xfconf_channel_get_properties(channel, nullptr);
	if (!properties)
	{
		return;
	}

	// Fetch length of property base
	const int base_len = strlen(base);

	// Load settings
	GHashTableIter iter;
	gpointer key, value;
	g_hash_table_iter_init(&iter, properties);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		property_changed(static_cast<const gchar*>(key) + base_len, static_cast<GValue*>(value));
	}

	prevent_invalid();
}

//-----------------------------------------------------------------------------

void Settings::prevent_invalid()
{
	// Prevent empty categories
	if (!category_show_name && (category_icon_size == -1))
	{
		category_show_name = true;
	}

	// Reset default category if recent is hidden
	if (!recent_items_max && (default_category == CategoryRecent))
	{
		default_category = CategoryFavorites;
	}

	// Prevent empty panel button
	if (!button_icon_visible)
	{
		if (!button_title_visible)
		{
			button_icon_visible = true;
		}
		else if (button_title.empty())
		{
			button_title = m_button_title_default;
		}
	}
}

//-----------------------------------------------------------------------------

void Settings::property_changed(const gchar* property, const GValue* value)
{
	bool reload = true;
	if (favorites.load(property, value, reload)
			|| recent.load(property, value, reload)
			|| launcher_show_name.load(property, value)
			|| launcher_show_description.load(property, value)
			|| sort_categories.load(property, value)
			|| view_mode.load(property, value))
	{
		if (reload)
		{
			m_plugin->reload_menu();
		}
	}

	else if (button_title.load(property, value)
			|| button_icon_name.load(property, value)
			|| button_title_visible.load(property, value)
			|| button_icon_visible.load(property, value)
			|| button_single_row.load(property, value))
	{
		m_plugin->reload_button();
	}

	else if (custom_menu_file.load(property, value)
			|| launcher_show_tooltip.load(property, value)
			|| launcher_icon_size.load(property, value)
			|| category_hover_activate.load(property, value)
			|| category_show_name.load(property, value)
			|| category_icon_size.load(property, value)
			|| default_category.load(property, value)
			|| recent_items_max.load(property, value)
			|| favorites_in_recent.load(property, value)
			|| position_profile_alternate.load(property, value)
			|| position_search_alternate.load(property, value)
			|| position_commands_alternate.load(property, value)
			|| position_categories_alternate.load(property, value)
			|| position_categories_horizontal.load(property, value)
			|| stay_on_focus_out.load(property, value)
			|| profile_shape.load(property, value)
			|| confirm_session_command.load(property, value)
			|| menu_width.load(property, value)
			|| menu_height.load(property, value)
			|| menu_opacity.load(property, value)
			|| search_actions.load(property, value))
	{
	}

	else
	{
		for (auto i : command)
		{
			if (i->load(property, value))
			{
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------

Boolean::Boolean(const gchar* property, bool data) :
	m_property(property),
	m_default(data),
	m_data(m_default)
{
}

//-----------------------------------------------------------------------------

void Boolean::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_bool_entry(rc, m_property + 1, m_data), !is_default);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool Boolean::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_BOOLEAN(value) ? g_value_get_boolean(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void Boolean::set(bool data, bool store)
{
	if (m_data == data)
	{
		return;
	}

	m_data = data;

	if (store && wm_settings->channel)
	{
		wm_settings->begin_property_update();
		xfconf_channel_set_bool(wm_settings->channel, m_property, m_data);
		wm_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------

Integer::Integer(const gchar* property, int data, int min, int max) :
	m_property(property),
	m_min(min),
	m_max(max),
	m_default(CLAMP(data, min, max)),
	m_data(m_default)
{
}

//-----------------------------------------------------------------------------

void Integer::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_int_entry(rc, m_property + 1, m_data), !is_default);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool Integer::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_INT(value) ? g_value_get_int(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void Integer::set(int data, bool store)
{
	data = CLAMP(data, m_min, m_max);
	if (m_data == data)
	{
		return;
	}

	m_data = data;

	if (store && wm_settings->channel)
	{
		wm_settings->begin_property_update();
		xfconf_channel_set_int(wm_settings->channel, m_property, m_data);
		wm_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------

String::String(const gchar* property, const std::string& data) :
	m_property(property),
	m_default(data),
	m_data(m_default)
{
}

//-----------------------------------------------------------------------------

void String::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_entry(rc, m_property + 1, m_data.c_str()), !is_default);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool String::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_STRING(value) ? g_value_get_string(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void String::set(const std::string& data, bool store)
{
	if (m_data == data)
	{
		return;
	}

	m_data = data;

	if (store && wm_settings->channel)
	{
		wm_settings->begin_property_update();
		xfconf_channel_set_string(wm_settings->channel, m_property, m_data.c_str());
		wm_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------

StringList::StringList(const gchar* property, std::initializer_list<std::string> data) :
	m_property(property),
	m_default(data),
	m_data(m_default),
	m_modified(false),
	m_saved(false),
	m_order_unchanged(false)
{
}

//-----------------------------------------------------------------------------

void StringList::clear()
{
	m_data.clear();
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::erase(int pos)
{
	m_data.erase(m_data.begin() + pos);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::insert(int pos, const std::string& value)
{
	m_data.insert(m_data.begin() + pos, value);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::push_back(const std::string& value)
{
	m_data.push_back(value);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::resize(int count)
{
	m_data.resize(count);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::set(int pos, const std::string& value)
{
	m_data[pos] = value;
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::load(XfceRc* rc, bool is_default)
{
	if (!xfce_rc_has_entry(rc, m_property + 1))
	{
		return;
	}

	gchar** data = xfce_rc_read_list_entry(rc, m_property + 1, ",");
	if (!data)
	{
		return;
	}

	std::vector<std::string> strings;
	for (int i = 0; data[i]; ++i)
	{
		strings.push_back(data[i]);
	}
	set(strings, !is_default);

	g_strfreev(data);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool StringList::load(const gchar* property, const GValue* value, bool& reload_menu)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	// Ignore own changes to prevent extra menu reload
	if (m_saved)
	{
		m_saved = false;
		reload_menu = false;
		return true;
	}

	// Handle resetting to default
	if (G_VALUE_TYPE(value) == G_TYPE_INVALID)
	{
		m_modified = false;
		m_order_unchanged = false;
		m_data = m_default;
		reload_menu = true;
		return true;
	}

	// Convert GValue to string list
	std::vector<std::string> strings;
	if (G_VALUE_HOLDS(value, G_TYPE_PTR_ARRAY))
	{
		const GPtrArray* values = static_cast<const GPtrArray*>(g_value_get_boxed(value));
		for (guint i = 0; i < values->len; ++i)
		{
			const GValue* string = static_cast<const GValue*>(g_ptr_array_index(values, i));
			if (G_VALUE_HOLDS_STRING(string))
			{
				strings.push_back(g_value_get_string(string));
			}
		}
	}
	else if (G_VALUE_HOLDS(value, G_TYPE_STRV))
	{
		const gchar** values = static_cast<const gchar**>(g_value_get_boxed(value));
		for (int i = 0; values[i]; ++i)
		{
			strings.push_back(values[i]);
		}
	}
	else if (G_VALUE_HOLDS_STRING(value))
	{
		strings.push_back(g_value_get_string(value));
	}

	// Load string list
	set(strings, false);
	reload_menu = true;

	return true;
}

//-----------------------------------------------------------------------------

void StringList::save()
{
	if (!m_modified || !wm_settings->channel)
	{
		return;
	}

	wm_settings->begin_property_update();

	const int size = m_data.size();
	GPtrArray* array = g_ptr_array_sized_new(size);

	for (int i = 0; i < size; ++i)
	{
		GValue* value = g_new0(GValue, 1);
		g_value_init(value, G_TYPE_STRING);
		g_value_set_static_string(value, m_data[i].c_str());
		g_ptr_array_add(array, value);
	}

	xfconf_channel_set_arrayv(wm_settings->channel, m_property, array);
	xfconf_array_free(array);

	m_saved = true;
	m_modified = false;

	wm_settings->end_property_update();
}

//-----------------------------------------------------------------------------

void StringList::set(std::vector<std::string>& data, bool store)
{
	m_data.clear();

	for (auto& desktop_id : data)
	{
#if EXO_CHECK_VERSION(4,15,0)
		if (desktop_id == "exo-web-browser.desktop")
		{
			desktop_id = "xfce4-web-browser.desktop";
		}
		else if (desktop_id == "exo-mail-reader.desktop")
		{
			desktop_id = "xfce4-mail-reader.desktop";
		}
		else if (desktop_id == "exo-file-manager.desktop")
		{
			desktop_id = "xfce4-file-manager.desktop";
		}
		else if (desktop_id == "exo-terminal-emulator.desktop")
		{
			desktop_id = "xfce4-terminal-emulator.desktop";
		}
#endif
		if (std::find(m_data.begin(), m_data.end(), desktop_id) == m_data.end())
		{
			m_data.push_back(std::move(desktop_id));
		}
	}

	m_modified = store;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

SearchActionList::SearchActionList(std::initializer_list<SearchAction*> data) :
	m_data(data),
	m_modified(false)
{
	clone(m_data, m_default);
}

//-----------------------------------------------------------------------------

SearchActionList::~SearchActionList()
{
	for (auto action : m_default)
	{
		delete action;
	}

	for (auto action : m_data)
	{
		delete action;
	}
}

//-----------------------------------------------------------------------------

void SearchActionList::erase(SearchAction* value)
{
	m_data.erase(std::find(m_data.begin(), m_data.end(), value));
	m_modified = true;
}

//-----------------------------------------------------------------------------

void SearchActionList::push_back(SearchAction* value)
{
	m_data.push_back(value);
	m_modified = true;
}

//-----------------------------------------------------------------------------

void SearchActionList::load(XfceRc* rc, bool is_default)
{
	const int size = xfce_rc_read_int_entry(rc, "search-actions", -1);
	if (size < 0)
	{
		return;
	}

	for (int i = 0; i < size; ++i)
	{
		gchar* key = g_strdup_printf("action%i", i);
		if (!xfce_rc_has_group(rc, key))
		{
			g_free(key);
			continue;
		}
		xfce_rc_set_group(rc, key);
		g_free(key);

		SearchAction* action = new SearchAction(
				xfce_rc_read_entry(rc, "name", ""),
				xfce_rc_read_entry(rc, "pattern", ""),
				xfce_rc_read_entry(rc, "command", ""),
				xfce_rc_read_bool_entry(rc, "regex", false));

		bool found = false;
		for (auto current : m_data)
		{
			if (*current == *action)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			m_data.push_back(action);
			m_modified = true;
		}
		else
		{
			delete action;
		}
	}

	if (is_default)
	{
		clone(m_data, m_default);
		m_modified = false;
	}
}

//-----------------------------------------------------------------------------

void SearchActionList::load()
{
	const int size = xfconf_channel_get_int(wm_settings->channel, "/search-actions", -1);
	if (size < 0)
	{
		return;
	}

	for (auto action : m_data)
	{
		delete action;
	}
	m_data.clear();

	gchar* property = nullptr;
	gchar* name = nullptr;
	gchar* pattern = nullptr;
	gchar* command = nullptr;
	bool regex;

	for (int i = 0; i < size; ++i)
	{
		property = g_strdup_printf("/search-actions/action-%d/name", i);
		name = xfconf_channel_get_string(wm_settings->channel, property, nullptr);
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/pattern", i);
		pattern = xfconf_channel_get_string(wm_settings->channel, property, nullptr);
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/command", i);
		command = xfconf_channel_get_string(wm_settings->channel, property, nullptr);
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/regex", i);
		regex = xfconf_channel_get_bool(wm_settings->channel, property, false);
		g_free(property);

		m_data.push_back(new SearchAction(name, pattern, command, regex));

		g_free(name);
		g_free(pattern);
		g_free(command);
	}

	m_modified = false;
}

//-----------------------------------------------------------------------------

bool SearchActionList::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0("/search-actions", property) == 0)
	{
		if (G_VALUE_TYPE(value) != G_TYPE_INVALID)
		{
			load();
		}
		else
		{
			clone(m_default, m_data);
		}
		return true;
	}

	int index = 0;
	char field[16];
	if (std::sscanf(property, "/search-actions/action-%d/%14s", &index, field) != 2)
	{
		return false;
	}

	if (index >= size())
	{
		return true;
	}
	SearchAction* action = m_data[index];

	if ((g_strcmp0(field, "name") == 0) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_name(g_value_get_string(value));
	}
	else if ((g_strcmp0(field, "pattern") == 0) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_pattern(g_value_get_string(value));
	}
	else if ((g_strcmp0(field, "command") == 0) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_command(g_value_get_string(value));
	}
	else if ((g_strcmp0(field, "regex") == 0) && G_VALUE_HOLDS_BOOLEAN(value))
	{
		action->set_is_regex(g_value_get_boolean (value));
	}

	return true;
}

//-----------------------------------------------------------------------------

void SearchActionList::save()
{
	if (!m_modified || !wm_settings->channel)
	{
		return;
	}

	wm_settings->begin_property_update();

	xfconf_channel_reset_property(wm_settings->channel, "/search-actions", true);

	const int size = m_data.size();
	xfconf_channel_set_int(wm_settings->channel, "/search-actions", size);

	gchar* property = nullptr;
	const SearchAction* action = nullptr;

	for (int i = 0; i < size; ++i)
	{
		action = m_data[i];

		property = g_strdup_printf("/search-actions/action-%d/name", i);
		xfconf_channel_set_string(wm_settings->channel, property, action->get_name());
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/pattern", i);
		xfconf_channel_set_string(wm_settings->channel, property, action->get_pattern());
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/command", i);
		xfconf_channel_set_string(wm_settings->channel, property, action->get_command());
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/regex", i);
		xfconf_channel_set_bool(wm_settings->channel, property, action->get_is_regex());
		g_free(property);
	}

	m_modified = false;

	wm_settings->end_property_update();
}

//-----------------------------------------------------------------------------

void SearchActionList::clone(const std::vector<SearchAction*>& in, std::vector<SearchAction*>& out) const
{
	// Remove previous actions
	for (auto action : out)
	{
		delete action;
	}
	out.clear();

	// Copy actions
	out.reserve(in.size());
	for (auto action : in)
	{
		out.push_back(new SearchAction(
				action->get_name(),
				action->get_pattern(),
				action->get_command(),
				action->get_is_regex()));
	}
}

//-----------------------------------------------------------------------------
