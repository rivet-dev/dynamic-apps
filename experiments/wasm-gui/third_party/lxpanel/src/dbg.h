/*
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2012 Henry Gebhardt <hsggebhardt@gmail.com>
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

#include <stdio.h>
#include <cairo/cairo.h>

/*
 * Macros ENTER, RET(), and DBG() are for low-level debug. Use them in
 * combination with defining DEBUG to add some messages that normally
 * would never be shown. Usually those messages will spam your console
 * and are just for finding problem, not for production usage.
 */

#ifdef DEBUG

#define ENTER          do { fprintf(stderr, "%s:%s:%-5d: ENTER\n",  __FILE__,__FUNCTION__, __LINE__); } while(0)
#define RET(args...)   do { fprintf(stderr, "%s:%s:%-5d: RETURN\n", __FILE__, __FUNCTION__, __LINE__);\
return args; } while(0)
#define DBG(fmt, args...) fprintf(stderr, "%s:%s:%-5d: " fmt,  __FILE__,__FUNCTION__, __LINE__, ## args)

#else

#define ENTER         do {  } while(0)
#define RET(args...)   return args
#define DBG(fmt, args...)   do {  } while(0)

#endif


void _check_cairo_status(cairo_t* cr, char const* file, char const* func, int line);
void _check_cairo_surface_status(cairo_surface_t** surf, char const* file, char const* func, int line);

#define check_cairo_status(cr) _check_cairo_status(cr, __FILE__, __FUNCTION__, __LINE__)
#define check_cairo_surface_status(surf) _check_cairo_surface_status(surf, __FILE__, __FUNCTION__, __LINE__)
