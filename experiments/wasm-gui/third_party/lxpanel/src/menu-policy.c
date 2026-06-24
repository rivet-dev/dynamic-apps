/*
 * Copyright (C) 2009-2010 martyj19 <martyj19@comcast.net>
 *               2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "menu-policy.h"
#include "private.h"

/* support for libmenu-cache 0.4.x */
#ifndef MENU_CACHE_CHECK_VERSION
# ifdef HAVE_MENU_CACHE_DIR_LIST_CHILDREN
#  define MENU_CACHE_CHECK_VERSION(_a,_b,_c) (_a == 0 && _b < 5) /* < 0.5.0 */
# else
#  define MENU_CACHE_CHECK_VERSION(_a,_b,_c) 0 /* not even 0.4.0 */
# endif
#endif

/* Allocate a menu cache. */
MenuCache * panel_menu_cache_new(guint32* visibility_flags)
{
    MenuCache* cache;
    gboolean need_prefix = (g_getenv("XDG_MENU_PREFIX") == NULL);

#if MENU_CACHE_CHECK_VERSION(0, 5, 0)
    /* do it the same way menu:// VFS plugin in libfm does */
    cache = menu_cache_lookup(need_prefix ? "lxde-applications.menu+hidden" : "applications.menu+hidden");
#else
    cache = menu_cache_lookup(need_prefix ? "lxde-applications.menu" : "applications.menu");
#endif
    if(visibility_flags)
    {
        if(is_in_lxde)
            *visibility_flags = SHOW_IN_LXDE;
        else
        {
            const char* de_name = g_getenv("XDG_CURRENT_DESKTOP");
            if(de_name)
                *visibility_flags = menu_cache_get_desktop_env_flag(cache, de_name);
            else
                *visibility_flags = SHOW_IN_LXDE|SHOW_IN_GNOME|SHOW_IN_KDE|SHOW_IN_XFCE;
        }
    }
    return cache;
}

/* Evaluate the visibility of a menu item. */
gboolean panel_menu_item_evaluate_visibility(MenuCacheItem * item, guint32 visibility_flags)
{
    return menu_cache_app_get_is_visible(MENU_CACHE_APP(item), visibility_flags);
}
