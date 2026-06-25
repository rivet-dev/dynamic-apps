/*
 * Copyright (C) 2013-2023 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_SETTINGS_DIALOG_H
#define WHISKERMENU_SETTINGS_DIALOG_H

#include <vector>

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class CommandEdit;
class Plugin;
class SearchAction;

class SettingsDialog
{
public:
	explicit SettingsDialog(Plugin* plugin);
	~SettingsDialog();

	SettingsDialog(const SettingsDialog&) = delete;
	SettingsDialog(SettingsDialog&&) = delete;
	SettingsDialog& operator=(const SettingsDialog&) = delete;
	SettingsDialog& operator=(SettingsDialog&&) = delete;

	GtkWidget* get_widget() const
	{
		return m_window;
	}

private:
	void choose_icon();

	SearchAction* get_selected_action(GtkTreeIter* iter = nullptr) const;
	void add_action();
	void remove_action();

	void response(int response_id);

	GtkWidget* init_general_tab();
	GtkWidget* init_appearance_tab();
	GtkWidget* init_behavior_tab();
	GtkWidget* init_commands_tab();
	GtkWidget* init_search_actions_tab();

private:
	Plugin* m_plugin;
	GtkWidget* m_window;

	// Appearance
	GtkWidget* m_show_as_icons;
	GtkWidget* m_show_as_list;
	GtkWidget* m_show_as_tree;
	GtkWidget* m_show_generic_names;
	GtkWidget* m_show_category_names;
	GtkWidget* m_show_descriptions;
	GtkWidget* m_show_tooltips;
	GtkWidget* m_category_icon_size;
	GtkWidget* m_item_icon_size;
	GtkWidget* m_background_opacity;

	// Layout
	GtkWidget* m_position_categories_horizontal;
	GtkWidget* m_position_categories_alternate;
	GtkWidget* m_position_search_alternate;
	GtkWidget* m_position_commands_alternate;
	GtkWidget* m_position_profile_alternate;
	GtkWidget* m_profile_shape;
	GtkWidget* m_menu_width;
	GtkWidget* m_menu_height;

	// Panel Button
	GtkWidget* m_button_style;
	GtkWidget* m_title;
	GtkWidget* m_icon;
	GtkWidget* m_icon_button;
	GtkWidget* m_button_single_row;

	// Behavior
	GtkWidget* m_hover_switch_category;
	GtkWidget* m_stay_on_focus_out;
	GtkWidget* m_sort_categories;

	// Default Display
	GtkWidget* m_display_favorites;
	GtkWidget* m_display_recent;
	GtkWidget* m_display_applications;

	// Recently Used
	GtkWidget* m_remember_favorites;
	GtkWidget* m_recent_items_max;

	// Session Commands
	GtkWidget* m_confirm_session_command;

	std::vector<CommandEdit*> m_commands;

	// Search Actions
	GtkTreeView* m_actions_view;
	GtkListStore* m_actions_model;
	GtkWidget* m_action_add;
	GtkWidget* m_action_remove;
	GtkWidget* m_action_name;
	GtkWidget* m_action_pattern;
	GtkWidget* m_action_command;
	GtkWidget* m_action_regex;
};

}

#endif // WHISKERMENU_SETTINGS_DIALOG_H
