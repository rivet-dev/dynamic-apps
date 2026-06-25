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



static void   thunar_sbr_provider_renamer_provider_init (ThunarxRenamerProviderIface *iface);
static GList *thunar_sbr_provider_get_renamers          (ThunarxRenamerProvider      *renamer_provider);



struct _ThunarSbrProviderClass
{
  GObjectClass __parent__;
};

struct _ThunarSbrProvider
{
  GObject __parent__;
};



THUNARX_DEFINE_TYPE_WITH_CODE (ThunarSbrProvider,
                               thunar_sbr_provider,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_RENAMER_PROVIDER,
                                                            thunar_sbr_provider_renamer_provider_init));



static void
thunar_sbr_provider_class_init (ThunarSbrProviderClass *klass)
{
}



static void
thunar_sbr_provider_renamer_provider_init (ThunarxRenamerProviderIface *iface)
{
  iface->get_renamers = thunar_sbr_provider_get_renamers;
}



static void
thunar_sbr_provider_init (ThunarSbrProvider *sbr_provider)
{
}



static GList*
thunar_sbr_provider_get_renamers (ThunarxRenamerProvider *renamer_provider)
{
  GList *renamers = NULL;

  renamers = g_list_prepend (renamers, thunar_sbr_replace_renamer_new ());
  renamers = g_list_prepend (renamers, thunar_sbr_remove_renamer_new ());
  renamers = g_list_prepend (renamers, thunar_sbr_number_renamer_new ());
  renamers = g_list_prepend (renamers, thunar_sbr_insert_renamer_new ());
  renamers = g_list_prepend (renamers, thunar_sbr_case_renamer_new ());
  renamers = g_list_prepend (renamers, thunar_sbr_date_renamer_new ());

  return renamers;
}
