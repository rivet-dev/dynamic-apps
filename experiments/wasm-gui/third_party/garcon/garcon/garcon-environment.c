/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2007-2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <garcon/garcon-environment.h>


/**
 * SECTION: garcon-environment
 * @title: Desktop Environment Configuration
 * @short_description: Desktop Environment Configuration
 * @include: garcon/garcon.h
 *
 * Set the desktop environment used by the OnlyShowIn and NotShowIn
 * desktop files keys.
 **/



static gchar *environment = NULL;



/**
 * garcon_set_environment:
 * @env : Name of the desktop environment for which menus will
 *        be generated (e.g. XFCE, KDE, GNOME or %NULL).
 *
 * Sets (or unsets) the desktop environment for which menus will generated.
 * Menus and menu items belonging to other desktop environments will be
 * ignored. If set to %NULL, all menu items are used.
 */
void
garcon_set_environment (const gchar *env)
{
  if (G_LIKELY (environment != NULL))
    g_free (environment);

  environment = g_strdup (env);
}



/**
 * garcon_get_environment:
 *
 * Get the environment set with garcon_set_environment().
 *
 * Returns: Name of the desktop environment (e.g. XFCE, KDE, GNOME)
 *          which is used or %NULL.
 */
const gchar *
garcon_get_environment (void)
{
  return environment;
}



/**
 * garcon_set_environment_xdg:
 * @fallback_env: fallback value
 *
 * Set the desktop environment to the envvar XDG_CURRENT_DESKTOP.
 * If this variables is not set, it falls back to @default_env.
 *
 * For @fallback_env you can use for example #GARCON_ENVIRONMENT_XFCE.
 *
 * Since: 0.3.0
 */
void
garcon_set_environment_xdg (const gchar *fallback_env)
{
  const gchar *desktop;

  desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  if (G_LIKELY (desktop == NULL))
    desktop = fallback_env;
  else if (*desktop == '\0')
    desktop = NULL;

  garcon_set_environment (desktop);
}
