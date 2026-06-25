/*
 * Copyright (C) 2017-2021 Graeme Gott <graeme@gottcode.org>
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

#include "element.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

void Element::set_icon(const gchar* icon)
{
	if (m_icon)
	{
		g_object_unref(m_icon);
		m_icon = nullptr;
	}

	if (G_UNLIKELY(!icon))
	{
		return;
	}

	if (!g_path_is_absolute(icon))
	{
		const gchar* pos = g_strrstr(icon, ".");
		if (!pos)
		{
			m_icon = g_themed_icon_new(icon);
		}
		else
		{
			gchar* suffix = g_utf8_casefold(pos, -1);
			if ((g_strcmp0(suffix, ".png") == 0)
					|| (g_strcmp0(suffix, ".xpm") == 0)
					|| (g_strcmp0(suffix, ".svg") == 0)
					|| (g_strcmp0(suffix, ".svgz") == 0))
			{
				gchar* name = g_strndup(icon, pos - icon);
				m_icon = g_themed_icon_new(name);
				g_free(name);
			}
			else
			{
				m_icon = g_themed_icon_new(icon);
			}
			g_free(suffix);
		}
	}
	else
	{
		GFile* file = g_file_new_for_path(icon);
		m_icon = g_file_icon_new(file);
		g_object_unref(file);
	}
}

//-----------------------------------------------------------------------------

void Element::spawn(GdkScreen* screen, const gchar* command, const gchar* working_directory, gboolean startup_notify, const gchar* icon_name) const
{
	GError* error = nullptr;
	bool result = false;

	gchar** argv;
	if (g_shell_parse_argv(command, nullptr, &argv, &error))
	{
#if LIBXFCE4UI_CHECK_VERSION(4,15,6)
		result = xfce_spawn(screen,
#else
		result = xfce_spawn_on_screen(screen,
#endif
				working_directory,
				argv,
				nullptr,
				G_SPAWN_SEARCH_PATH,
				startup_notify,
				gtk_get_current_event_time(),
				icon_name,
#if LIBXFCE4UI_CHECK_VERSION(4,15,6)
				true,
#endif
				&error);
		g_strfreev(argv);
	}

	if (!result)
	{
		xfce_dialog_show_error(nullptr, error, _("Failed to execute command \"%s\"."), command);
		g_error_free(error);
	}
}

//-----------------------------------------------------------------------------
