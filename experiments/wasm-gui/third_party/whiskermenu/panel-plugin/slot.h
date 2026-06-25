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

#ifndef WHISKERMENU_SLOT_H
#define WHISKERMENU_SLOT_H

#include <glib-object.h>

namespace WhiskerMenu
{
// Connect flags
enum class Connect
{
	Default = 0,
	After = G_CONNECT_AFTER
};

// Lambda with parameters
template<typename Func, typename T>
class Slot
{
};

template<typename Func, typename R, typename T, typename... Args>
class Slot<Func, R(T::*)(Args...) const>
{
	Func m_func;

public:
	Slot(Func func) :
		m_func(func)
	{
	}

	static R invoke(Args... args, gpointer user_data)
	{
		return static_cast<Slot*>(user_data)->m_func(args...);
	}

	static void destroy(gpointer data, GClosure*)
	{
		delete static_cast<Slot*>(data);
	}
};

template<typename Sender, typename Func>
gulong connect(Sender instance, const gchar* detailed_signal, Func func, Connect flags = Connect::Default)
{
	typedef Slot<Func, decltype(&Func::operator())> Receiver;

	return g_signal_connect_data(G_OBJECT(instance),
			detailed_signal,
			G_CALLBACK(&Receiver::invoke),
			new Receiver(func),
			&Receiver::destroy,
			GConnectFlags(flags));
}

}

#endif // WHISKERMENU_SLOT_H
