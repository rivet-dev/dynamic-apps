/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* implement exo-utils's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __EXO_UTILS_C__
#include <exo/exo-utils.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-utils
 * @title: Miscellaneous Utility Functions
 * @short_description: Various utility functions
 * @include: exo/exo.h
 * @see_also: <ulink type="http" url="http://library.gnome.org/devel/glib/stable/glib-Atomic-Operations.html">
 *            GLib Atomic Operations</ulink>
 *
 * This module contains various utility functions that extend the basic
 * utility functions provided by the <ulink type="http"
 * url="http://library.gnome.org/devel/glib/stable/">GLib</ulink> library.
 **/



/**
 * exo_noop:
 *
 * This function has no effect. It does nothing but
 * returning instantly. It is mostly useful in
 * situations that require a function to be called,
 * but that function does not need to do anything
 * useful.
 *
 * Deprecated: xfce 4.18: Use NULL pointer instead
 *
 * Since: 0.3.1
 **/
void
exo_noop (void)
{
}



/**
 * exo_noop_one:
 *
 * This function has no effect but simply returns
 * the integer value %1. It is mostly useful in
 * situations where you just need a function that
 * returns %1, but don't want to perform any other
 * actions.
 *
 * Deprecated: xfce 4.18: Use NULL pointer instead
 *
 * Returns: the integer value %1.
 *
 * Since: 0.3.1
 **/
gint
exo_noop_one (void)
{
  return 1;
}



/**
 * exo_noop_zero:
 *
 * This function has no effect but simply returns
 * the integer value %0. It is mostly useful in
 * situations where you just need a function that
 * returns %0, but don't want to perform any other
 * actions.
 *
 * Deprecated: xfce 4.18: Use NULL pointer instead
 *
 * Returns: the integer value %0.
 *
 * Since: 0.3.1
 **/
gint
exo_noop_zero (void)
{
  return 0;
}



/**
 * exo_noop_null:
 *
 * This function has no effect but simply returns
 * a %NULL pointer. It is mostly useful in
 * situations where you just need a function that
 * returns %NULL, but don't want to perform any
 * other actions.
 *
 * Deprecated: xfce 4.18: Use NULL pointer instead
 *
 * Returns: a %NULL pointer.
 *
 * Since: 0.3.1
 **/
gpointer
exo_noop_null (void)
{
  return NULL;
}



/**
 * exo_noop_true:
 *
 * This function has no effect, but simply returns
 * the boolean value %TRUE. It is mostly useful in
 * situations where you just need a function that
 * returns %TRUE, but don't want to perform any
 * other actions.
 *
 * Deprecated: xfce 4.18: Use NULL pointer instead
 *
 * Returns: the boolean value %TRUE.
 *
 * Since: 0.3.1
 **/
gboolean
exo_noop_true (void)
{
  return TRUE;
}



/**
 * exo_noop_false:
 *
 * This function has no effect, but simply returns
 * the boolean value %FALSE. It is mostly useful in
 * situations where you just need a function that
 * returns %FALSE, but don't want to perform any
 * other actions.
 *
 * Deprecated: xfce 4.18: Use NULL pointer instead
 *
 * Returns: the boolean value %FALSE.
 *
 * Since: 0.3.1
 **/
gboolean
exo_noop_false (void)
{
  return FALSE;
}



#define __EXO_UTILS_C__
#include <exo/exo-aliasdef.c>
