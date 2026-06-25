/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <libxfce4util/libxfce4util.h>

#include <thunarx/thunarx-renamer-provider.h>
#include <thunarx/thunarx-private.h>

/**
 * SECTION: thunarx-renamer-provider
 * @short_description: The interface to extensions that provide additional bulk renamers
 * @title: ThunarxRenamerProvider
 * @include: thunarx/thunarx.h
 *
 * The <interface>ThunarxRenamerProvider</interface> interface is implemented by
 * extensions which provide additional bulk renamers that should be used by the
 * bulk rename dialog in Thunar.
 */

GType
thunarx_renamer_provider_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarxRenamerProvider"),
                                            sizeof (ThunarxRenamerProviderIface),
                                            NULL,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}



/**
 * thunarx_renamer_provider_get_renamers: (skip)
 * @provider : a #ThunarxRenamerProvider.
 *
 * Returns the list of #ThunarxRenamer<!---->s provided by the
 * specified @provider.
 *
 * The real implementation of this method MUST return the #ThunarxRenamer<!---->s
 * with floating references (the default for g_object_new() on #GtkWidget
 * derived types).
 *
 * The returned #ThunarxRenamer<!---->s will be reffed and sinked automatically
 * by this function.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #ThunarxRenamer returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for atleast the time that the renamers are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of renamers using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Returns: (transfer full) (element-type ThunarxRenamer): the list of #ThunarxRenamer<!---->s provided by the
 *               specified @provider.
 **/
GList*
thunarx_renamer_provider_get_renamers (ThunarxRenamerProvider *provider)
{
  GList *renamers;
  GList *lp;

  g_return_val_if_fail (THUNARX_IS_RENAMER_PROVIDER (provider), NULL);

  /* determine the renamers from the real implementation */
  renamers = (*THUNARX_RENAMER_PROVIDER_GET_IFACE (provider)->get_renamers) (provider);

  /* ref&sink all returned renamers */
  for (lp = renamers; lp != NULL; lp = lp->next)
    g_object_ref_sink (G_OBJECT (lp->data));

  /* take a reference for all renamers on the provider */
  thunarx_object_list_take_reference (renamers, provider);

  /* and return the list of renamers */
  return renamers;
}
