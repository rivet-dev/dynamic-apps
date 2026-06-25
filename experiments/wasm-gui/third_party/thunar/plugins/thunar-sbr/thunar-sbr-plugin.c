/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-sbr/thunar-sbr-case-renamer.h>
#include <thunar-sbr/thunar-sbr-insert-renamer.h>
#include <thunar-sbr/thunar-sbr-number-renamer.h>
#include <thunar-sbr/thunar-sbr-provider.h>
#include <thunar-sbr/thunar-sbr-remove-renamer.h>
#include <thunar-sbr/thunar-sbr-replace-renamer.h>
#include <thunar-sbr/thunar-sbr-date-renamer.h>



G_MODULE_EXPORT void thunar_extension_initialize (ThunarxProviderPlugin  *plugin);
G_MODULE_EXPORT void thunar_extension_shutdown   (void);
G_MODULE_EXPORT void thunar_extension_list_types (const GType           **types,
                                                  gint                   *n_types);



static GType type_list[1];



G_MODULE_EXPORT void
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

#ifdef G_ENABLE_DEBUG
  g_message ("Initializing ThunarSbr extension");
#endif

  /* register the enum types for this plugin */
  thunar_sbr_register_enum_types (plugin);

  /* register the classes provided by this plugin */
  thunar_sbr_case_renamer_register_type (plugin);
  thunar_sbr_insert_renamer_register_type (plugin);
  thunar_sbr_number_renamer_register_type (plugin);
  thunar_sbr_provider_register_type (plugin);
  thunar_sbr_remove_renamer_register_type (plugin);
  thunar_sbr_replace_renamer_register_type (plugin);
  thunar_sbr_date_renamer_register_type (plugin);

  /* setup the plugin provider type list */
  type_list[0] = THUNAR_SBR_TYPE_PROVIDER;
}



G_MODULE_EXPORT void
thunar_extension_shutdown (void)
{
#ifdef G_ENABLE_DEBUG
  g_message ("Shutting down ThunarSbr extension");
#endif
}



G_MODULE_EXPORT void
thunar_extension_list_types (const GType **types,
                             gint         *n_types)
{
  *types = type_list;
  *n_types = G_N_ELEMENTS (type_list);
}


