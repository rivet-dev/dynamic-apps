/*
 * Copyright (C) 2018 - Ali Abdallah <ali@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>

#include "xfconf-gsettings-backend.h"
#include "xfconf/xfconf.h"

G_MODULE_EXPORT XFCONF_EXPORT void g_io_module_load (GIOModule *module)
{
  g_type_module_use(G_TYPE_MODULE(module));

  g_io_extension_point_implement(G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                 xfconf_gsettings_backend_get_type(),
                                 "xfconf",
                                 -1);
}

G_MODULE_EXPORT XFCONF_EXPORT void g_io_module_unload (GIOModule *module)
{
}

/* Module query */
G_MODULE_EXPORT XFCONF_EXPORT gchar** g_io_module_query (void)
{
  char *eps[] = {
    G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
    NULL
  };
  return g_strdupv (eps);
}

