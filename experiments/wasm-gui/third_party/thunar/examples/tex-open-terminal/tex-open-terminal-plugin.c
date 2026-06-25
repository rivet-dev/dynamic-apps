/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <tex-open-terminal/tex-open-terminal.h>



G_MODULE_EXPORT void thunar_extension_initialize (ThunarxProviderPlugin *plugin);
G_MODULE_EXPORT void thunar_extension_shutdown   (void);
G_MODULE_EXPORT void thunar_extension_list_types (const GType         **types,
                                                  gint                 *n_types);




static GType type_list[1];



void
thunar_extension_initialize (ThunarxProviderPlugin *plugin)
{
  const gchar *mismatch;

  /* verify that the thunarx versions are compatible */
  mismatch = thunarx_check_version (THUNARX_MAJOR_VERSION, THUNARX_MINOR_VERSION, THUNARX_MICRO_VERSION);
  if (G_UNLIKELY (mismatch != NULL))
    {
      g_warning ("Version mismatch: %s", mismatch);
      return;
    }

  g_message ("Initializing TexOpenTerminal extension");

  /* register the types provided by this plugin */
  tex_open_terminal_register_type (plugin);

  /* setup the plugin type list */
  type_list[0] = TEX_TYPE_OPEN_TERMINAL;
}



void
thunar_extension_shutdown (void)
{
  g_message ("Shutting down TexOpenTerminal extension");
}



void
thunar_extension_list_types (const GType **types,
                             gint         *n_types)
{
  *types = type_list;
  *n_types = G_N_ELEMENTS (type_list);
}

