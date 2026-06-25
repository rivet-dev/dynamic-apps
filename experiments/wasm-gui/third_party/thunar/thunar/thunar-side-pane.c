/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-private.h>
#include <thunar/thunar-side-pane.h>



static void thunar_side_pane_class_init (gpointer klass);



GType
thunar_side_pane_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarSidePane"),
                                            sizeof (ThunarSidePaneIface),
                                            (GClassInitFunc) (void (*)(void)) thunar_side_pane_class_init,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
      g_type_interface_add_prerequisite (type, THUNAR_TYPE_COMPONENT);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}



static void
thunar_side_pane_class_init (gpointer klass)
{
  /**
   * ThunarSidePane:show-hidden:
   *
   * Tells whether hidden folders will be displayed in
   * the #ThunarSidePane instance.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("show-hidden",
                                                             "show-hidden",
                                                             "show-hidden",
                                                             FALSE,
                                                             EXO_PARAM_READWRITE));
}



/**
 * thunar_side_pane_get_show_hidden:
 * @side_pane : a #ThunarSidePane.
 *
 * Returns %TRUE if hidden folders are shown
 * in the @side_pane.
 *
 * Return value: %TRUE if hidden folders are
 *               shown in the @side_pane.
 **/
gboolean
thunar_side_pane_get_show_hidden (ThunarSidePane *side_pane)
{
  _thunar_return_val_if_fail (THUNAR_IS_SIDE_PANE (side_pane), FALSE);
  if (THUNAR_SIDE_PANE_GET_IFACE (side_pane)->get_show_hidden != NULL)
    return (*THUNAR_SIDE_PANE_GET_IFACE (side_pane)->get_show_hidden) (side_pane);

  return FALSE;
}



/**
 * thunar_side_pane_set_show_hidden:
 * @side_pane   : a #ThunarSidePane.
 * @show_hidden : %TRUE to display hidden folders.
 *
 * If @show_hidden is %TRUE, hidden folders will be
 * shown in the @side_pane.
 **/
void
thunar_side_pane_set_show_hidden (ThunarSidePane *side_pane,
                                  gboolean        show_hidden)
{
  _thunar_return_if_fail (THUNAR_IS_SIDE_PANE (side_pane));
  if (THUNAR_SIDE_PANE_GET_IFACE (side_pane)->set_show_hidden != NULL)
    (*THUNAR_SIDE_PANE_GET_IFACE (side_pane)->set_show_hidden) (side_pane, show_hidden);
}

