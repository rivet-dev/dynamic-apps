/*
 * Copyright (C) 2014-2021 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_PROFILE_H
#define WHISKERMENU_PROFILE_H

#include <gtk/gtk.h>

#ifdef HAVE_ACCOUNTS_SERVICE
extern "C"
{
#include <act/act.h>
}
#endif

namespace WhiskerMenu
{

class Window;

class Profile
{
public:
	explicit Profile(Window* window);
	~Profile();

	Profile(const Profile&) = delete;
	Profile(Profile&&) = delete;
	Profile& operator=(const Profile&) = delete;
	Profile& operator=(Profile&&) = delete;

	GtkWidget* get_picture() const
	{
		return m_container;
	}

	GtkWidget* get_username() const
	{
		return m_username;
	}

	void reset_tooltip();

	void update_picture();

private:
	void init_fallback();
	void set_username(const gchar* name);
#ifdef HAVE_ACCOUNTS_SERVICE
	void on_user_changed(ActUser* user);
	void on_user_info_loaded();
#endif

private:
	GtkWidget* m_container;
	GtkWidget* m_image;
	GtkWidget* m_username;
#ifdef HAVE_ACCOUNTS_SERVICE
	ActUserManager* m_act_user_manager;
	ActUser* m_act_user;
#endif
	GFileMonitor* m_file_monitor;
	gchar* m_file_path;
};

}

#endif // WHISKERMENU_PROFILE_H
