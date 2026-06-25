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

#include "command.h"

#include "image-menu-item.h"
#include "slot.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Command::Command(const gchar* property, const gchar* show_property,
		const gchar* icon, const gchar* fallback_icon,
		const gchar* text,
		const gchar* command, bool shown,
		const gchar* error_text,
		const gchar* confirm_question, const gchar* confirm_status) :
	m_button(nullptr),
	m_menuitem(nullptr),
	m_mnemonic(g_strdup(text)),
	m_command(property, command),
	m_error_text(g_strdup(error_text)),
	m_shown(show_property, shown),
	m_status(CommandStatus::Unchecked),
	m_timeout_details({nullptr, g_strdup(confirm_question), g_strdup(confirm_status), 0})
{
	if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon))
	{
		m_icon = g_strdup(icon);
	}
	else
	{
		m_icon = g_strdup(fallback_icon);
	}

	std::string tooltip(text ? text : "");
	for (auto i = tooltip.begin(); i != tooltip.end(); ++i)
	{
		if (*i == '_')
		{
			i = tooltip.erase(i);
		}
	}
	m_text = g_strdup(tooltip.c_str());

	check();
}

//-----------------------------------------------------------------------------

Command::~Command()
{
	if (m_button)
	{
		gtk_widget_destroy(m_button);
		g_object_unref(m_button);
	}
	if (m_menuitem)
	{
		gtk_widget_destroy(m_menuitem);
		g_object_unref(m_menuitem);
	}

	g_free(m_icon);
	g_free(m_mnemonic);
	g_free(m_text);
	g_free(m_error_text);
	g_free(m_timeout_details.question);
	g_free(m_timeout_details.status);
}

//-----------------------------------------------------------------------------

GtkWidget* Command::get_button()
{
	if (m_button)
	{
		return m_button;
	}

	m_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(m_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(m_button, m_text);

	connect(m_button, "clicked",
		[this](GtkButton*)
		{
			activate();
		},
		Connect::After);

	GtkWidget* image = gtk_image_new_from_icon_name(m_icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_container_add(GTK_CONTAINER(m_button), GTK_WIDGET(image));

	gtk_style_context_add_class(gtk_widget_get_style_context(m_button), "command-button");

	gtk_widget_set_visible(m_button, m_shown);
	gtk_widget_set_sensitive(m_button, m_status == CommandStatus::Valid);

	g_object_ref_sink(m_button);

	return m_button;
}

//-----------------------------------------------------------------------------

GtkWidget* Command::get_menuitem()
{
	if (m_menuitem)
	{
		return m_menuitem;
	}

	m_menuitem = whiskermenu_image_menu_item_new_with_mnemonic(m_icon, m_mnemonic);

	connect(m_menuitem, "activate",
		[this](GtkMenuItem*)
		{
			activate();
		});

	gtk_widget_set_visible(m_menuitem, m_shown);
	gtk_widget_set_sensitive(m_menuitem, m_status == CommandStatus::Valid);

	g_object_ref_sink(m_menuitem);

	return m_menuitem;
}

//-----------------------------------------------------------------------------

void Command::set(const gchar* command)
{
	if (m_command == command)
	{
		return;
	}

	m_command = command;
	m_status = CommandStatus::Unchecked;
}

//-----------------------------------------------------------------------------

void Command::set_shown(bool shown)
{
	if (shown == m_shown)
	{
		return;
	}

	m_shown = shown;

	if (m_button)
	{
		gtk_widget_set_visible(m_button, m_shown);
	}
	if (m_menuitem)
	{
		gtk_widget_set_visible(m_menuitem, m_shown);
	}
}

//-----------------------------------------------------------------------------

void Command::check()
{
	if (m_status == CommandStatus::Unchecked)
	{
		gchar** argv;
		if (g_shell_parse_argv(m_command, nullptr, &argv, nullptr))
		{
			gchar* path = g_find_program_in_path(argv[0]);
			m_status = path ? CommandStatus::Valid : CommandStatus::Invalid;
			g_free(path);
			g_strfreev(argv);
		}
		else
		{
			m_status = CommandStatus::Invalid;
		}
	}

	if (m_button)
	{
		gtk_widget_set_visible(m_button, m_shown);
		gtk_widget_set_sensitive(m_button, m_status == CommandStatus::Valid);
	}
	if (m_menuitem)
	{
		gtk_widget_set_visible(m_menuitem, m_shown);
		gtk_widget_set_sensitive(m_menuitem, m_status == CommandStatus::Valid);
	}
}

//-----------------------------------------------------------------------------

void Command::activate()
{
	if (wm_settings->confirm_session_command
			&& m_timeout_details.question
			&& m_timeout_details.status
			&& !confirm())
	{
		return;
	}

	GError* error = nullptr;
	if (!g_spawn_command_line_async(m_command, &error))
	{
		xfce_dialog_show_error(nullptr, error, m_error_text, nullptr);
		g_error_free(error);
	}
}

//-----------------------------------------------------------------------------

void Command::load(XfceRc* rc, bool is_default)
{
	m_command.load(rc, is_default);
	m_shown.load(rc, is_default);
	m_status = CommandStatus::Unchecked;
}

//-----------------------------------------------------------------------------

bool Command::load(const gchar* property, const GValue* value)
{
	// Update command
	if (m_command.load(property, value))
	{
		m_status = CommandStatus::Unchecked;
		return true;
	}

	// Update shown
	if (m_shown.load(property, value))
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------

// Adapted from https://git.xfce.org/xfce/xfce4-panel/tree/plugins/actions/actions.c
bool Command::confirm()
{
	// Create dialog
	m_timeout_details.dialog = gtk_message_dialog_new(nullptr, GtkDialogFlags(0),
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
			"%s", m_timeout_details.question);
	GtkDialog* dialog = GTK_DIALOG(m_timeout_details.dialog);

	GtkWindow* window = GTK_WINDOW(m_timeout_details.dialog);
	gtk_window_set_deletable(window, false);
	gtk_window_set_keep_above(window, true);
	gtk_window_set_skip_taskbar_hint(window, true);
	gtk_window_stick(window);

	GtkWidget* header = gtk_header_bar_new();
	gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(header), false);
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), false);
	gtk_widget_show(header);
	gtk_window_set_titlebar(window, header);

	// Add icon
	GtkWidget* image = gtk_image_new_from_icon_name(m_icon, GTK_ICON_SIZE_DIALOG);
	gtk_widget_show(image);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog), image);
G_GNUC_END_IGNORE_DEPRECATIONS

	// Create accept button
	GtkWidget* button = gtk_dialog_add_button(dialog, m_mnemonic, GTK_RESPONSE_ACCEPT);
	image = gtk_image_new_from_icon_name(m_icon, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_dialog_set_default_response(dialog, GTK_RESPONSE_ACCEPT);

	// Run dialog
	m_timeout_details.time_left = 60;
	guint timeout_id = g_timeout_add(1000, &Command::confirm_countdown, &m_timeout_details);
	confirm_countdown(&m_timeout_details);

	gint result = gtk_dialog_run(dialog);

	g_source_remove(timeout_id);
	gtk_widget_destroy(m_timeout_details.dialog);
	m_timeout_details.dialog = nullptr;

	return result == GTK_RESPONSE_ACCEPT;
}

//-----------------------------------------------------------------------------

gboolean Command::confirm_countdown(gpointer data)
{
	TimeoutDetails* details = static_cast<TimeoutDetails*>(data);

	if (details->time_left == 0)
	{
		gtk_dialog_response(GTK_DIALOG(details->dialog), GTK_RESPONSE_ACCEPT);
	}
	else
	{
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(details->dialog),
				details->status, details->time_left);
	}

	return --details->time_left >= 0;
}

//-----------------------------------------------------------------------------
