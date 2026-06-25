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

#include "icon-size.h"

#include "settings.h"

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

IconSize::IconSize(const gchar* property, const int size) :
	m_property(property),
	m_default(CLAMP(size, NONE, Largest)),
	m_size(m_default)
{
}

//-----------------------------------------------------------------------------

int IconSize::get_size() const
{
	int size = 0;
	switch (m_size)
	{
		case NONE:     size =   1; break;
		case Smallest: size =  16; break;
		case Smaller:  size =  24; break;
		case Small:    size =  32; break;
		case Normal:   size =  48; break;
		case Large:    size =  64; break;
		case Larger:   size =  96; break;
		case Largest:  size = 128; break;
		default:       size =   0; break;
	}
	return size;
}

//-----------------------------------------------------------------------------

std::vector<std::string> IconSize::get_strings()
{
	return {
		_("None"),
		_("Very Small"),
		_("Smaller"),
		_("Small"),
		_("Normal"),
		_("Large"),
		_("Larger"),
		_("Very Large")
	};
}

//-----------------------------------------------------------------------------

void IconSize::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_int_entry(rc, m_property + 1, m_size), !is_default);

	if (is_default)
	{
		m_default = m_size;
	}
}

//-----------------------------------------------------------------------------

bool IconSize::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_INT(value) ? g_value_get_int(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void IconSize::set(int size, bool store)
{
	size = CLAMP(size, NONE, Largest);
	if (m_size == size)
	{
		return;
	}

	m_size = size;

	if (store && wm_settings->channel)
	{
		wm_settings->begin_property_update();
		xfconf_channel_set_int(wm_settings->channel, m_property, m_size);
		wm_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------
