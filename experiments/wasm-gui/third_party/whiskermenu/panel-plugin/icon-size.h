/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_ICON_SIZE_H
#define WHISKERMENU_ICON_SIZE_H

#include <string>
#include <vector>

#include <glib-object.h>

extern "C"
{
#include <libxfce4util/libxfce4util.h>
}

namespace WhiskerMenu
{

class IconSize
{
public:
	enum Size
	{
		NONE = -1,
		Smallest,
		Smaller,
		Small,
		Normal,
		Large,
		Larger,
		Largest
	};

	explicit IconSize(const gchar* property, const int size);

	int get_size() const;

	static std::vector<std::string> get_strings();

	operator int() const
	{
		return m_size;
	}

	IconSize& operator=(const int size)
	{
		set(size, true);
		return *this;
	}

	void load(XfceRc* rc, bool is_default);
	bool load(const gchar* property, const GValue* value);

private:
	void set(int size, bool store);

private:
	const gchar* const m_property;
	int m_default;
	int m_size;
};

}

#endif // WHISKERMENU_ICON_SIZE_H
