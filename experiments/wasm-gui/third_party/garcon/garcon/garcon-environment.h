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

#if !defined(_GARCON_INSIDE_GARCON_H) && !defined(GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_MENU_ENVIRONMENT_H__
#define __GARCON_MENU_ENVIRONMENT_H__

/**
 * GARCON_ENVIRONMENT_XFCE:
 *
 * Macro for garcon_set_environment or garcon_set_environment_xdg
 * to set the Xfce Desktop Environment.
 *
 * Since: 0.3.0
 **/
#define GARCON_ENVIRONMENT_XFCE "XFCE"

#include <glib.h>

G_BEGIN_DECLS

void         garcon_set_environment     (const gchar *env);

const gchar *garcon_get_environment     (void);

void         garcon_set_environment_xdg (const gchar *fallback_env);

G_END_DECLS

#endif /* !__GARCON_MENU_ENVIRONMENT_H__ */
