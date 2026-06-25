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

#ifndef WHISKERMENU_COMMAND_H
#define WHISKERMENU_COMMAND_H

#include "settings.h"

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class Command
{
public:
	explicit Command(const gchar* property, const gchar* show_property,
			const gchar* icon, const gchar* fallback_icon,
			const gchar* text,
			const gchar* command, bool shown,
			const gchar* error_text,
			const gchar* confirm_question = nullptr, const gchar* confirm_status = nullptr);
	~Command();

	Command(const Command&) = delete;
	Command(Command&&) = delete;
	Command& operator=(const Command&) = delete;
	Command& operator=(Command&&) = delete;

	GtkWidget* get_button();
	GtkWidget* get_menuitem();

	const gchar* get() const
	{
		return m_command;
	}

	bool get_shown() const
	{
		return m_shown;
	}

	const gchar* get_text() const
	{
		return m_mnemonic;
	}

	const gchar* get_tooltip() const
	{
		return m_text;
	}

	void set(const gchar* command);

	void set_shown(bool shown);

	void check();

	void activate();

	void load(XfceRc* rc, bool is_default);
	bool load(const gchar* property, const GValue* value);

private:
	bool confirm();
	static gboolean confirm_countdown(gpointer data);

private:
	GtkWidget* m_button;
	GtkWidget* m_menuitem;
	gchar* m_icon;
	gchar* m_mnemonic;
	gchar* m_text;
	String m_command;
	gchar* m_error_text;
	Boolean m_shown;

	enum class CommandStatus
	{
		Unchecked,
		Invalid,
		Valid
	}
	m_status;

	struct TimeoutDetails
	{
		GtkWidget* dialog;
		gchar* question;
		gchar* status;
		gint time_left;
	}
	m_timeout_details;
};

}

#endif // WHISKERMENU_COMMAND_H
