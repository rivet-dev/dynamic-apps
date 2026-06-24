/*
 * Copyright (C) 2009 martyj19 <martyj19@comcast.net>
 *               2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MENU_POLICY_H
#define MENU_POLICY_H

#include <menu-cache.h>

extern MenuCache * panel_menu_cache_new(guint32* visibility_flags); /* Allocate a menu cache */
extern gboolean panel_menu_item_evaluate_visibility(MenuCacheItem * item, guint32 visibility_flags); /* Evaluate the visibility of a menu item */

#endif
