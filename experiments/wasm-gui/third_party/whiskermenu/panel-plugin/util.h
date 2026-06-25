/*
 * Copyright (C) 2020 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_UTIL_H
#define WHISKERMENU_UTIL_H

#include <libxfce4ui/libxfce4ui.h>

#if !LIBXFCE4UTIL_CHECK_VERSION(4,15,2)
#include <exo/exo.h>

#define xfce_str_is_empty exo_str_is_empty
#endif

#endif
