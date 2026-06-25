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

#ifndef WHISKERMENU_PLUGIN_H
#define WHISKERMENU_PLUGIN_H

#define PLUGIN_WEBSITE "https://docs.xfce.org/panel-plugins/xfce4-whiskermenu-plugin"

#include <string>

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

namespace WhiskerMenu
{

class Window;

class Plugin
{
public:
	explicit Plugin(XfcePanelPlugin* plugin);
	~Plugin();

	Plugin(const Plugin&) = delete;
	Plugin(Plugin&&) = delete;
	Plugin& operator=(const Plugin&) = delete;
	Plugin& operator=(Plugin&&) = delete;

	GtkWidget* get_button() const
	{
		return m_button;
	}

	enum ButtonStyle
	{
		ShowIcon = 0x1,
		ShowText = 0x2,
		ShowIconAndText = ShowIcon | ShowText
	};

	ButtonStyle get_button_style() const;
	static std::string get_button_title_default();
	void get_menu_position(int* x, int* y) const;

	void menu_hidden();
	void reload_button();
	void reload_menu();
	void set_button_style(ButtonStyle style);
	void set_button_title(const std::string& title);
	void set_button_icon_name(const std::string& icon);
	void set_configure_enabled(bool enabled);
	void set_loaded(bool loaded);

private:
	void configure();
	void icon_changed(const gchar* icon);
	void mode_changed(XfcePanelPluginMode mode);
	gboolean remote_event(const gchar* name, const GValue* value);
	void show_about();
	gboolean size_changed(gint size);
	void update_size();
	void show_menu(int position);

private:
	XfcePanelPlugin* m_plugin;
	Window* m_window;

	GtkWidget* m_button;
	GtkBox* m_button_box;
	GtkLabel* m_button_label;
	GtkImage* m_button_icon;

	int m_opacity;
	bool m_file_icon;
	gint64 m_hide_time;
};

}

#endif // WHISKERMENU_PLUGIN_H
