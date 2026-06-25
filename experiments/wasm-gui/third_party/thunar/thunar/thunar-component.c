/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <exo/exo.h>

#include <thunar/thunar-component.h>
#include <thunar/thunar-private.h>



static void thunar_component_class_init (gpointer klass);



GType
thunar_component_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarComponent"),
                                            sizeof (ThunarComponentIface),
                                            (GClassInitFunc) (void (*)(void)) thunar_component_class_init,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, THUNAR_TYPE_NAVIGATOR);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}



static void
thunar_component_class_init (gpointer klass)
{
  /**
   * ThunarComponent:selected-files:
   *
   * The list of currently selected files for the #ThunarWindow to
   * which this #ThunarComponent belongs.
   *
   * The exact semantics of this property depend on the implementor
   * of this interface. For example, #ThunarComponent<!---->s will update
   * the property depending on the users selection with the
   * #GtkTreeComponent or #ExoIconComponent. While other components in a window,
   * like the #ThunarShortcutsPane, will not update this property on
   * their own, but rely on #ThunarWindow to synchronize the selected
   * files list with the selected files list from the active #ThunarComponent.
   *
   * This way all components can behave properly depending on the
   * set of selected files even though they don't have direct access
   * to the #ThunarComponent.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boxed ("selected-files",
                                                           "selected-files",
                                                           "selected-files",
                                                           THUNARX_TYPE_FILE_INFO_LIST,
                                                           EXO_PARAM_READWRITE));
}



/**
 * thunar_component_get_selected_files:
 * @component : a #ThunarComponent instance.
 *
 * Returns the set of selected #ThunarFile<!---->s. Check the description
 * of the :selected-files property for details.
 *
 * Return value: the set of selected #ThunarFile<!---->s.
 **/
GList*
thunar_component_get_selected_files (ThunarComponent *component)
{
  _thunar_return_val_if_fail (THUNAR_IS_COMPONENT (component), NULL);
  if (THUNAR_COMPONENT_GET_IFACE (component)->get_selected_files != NULL)
    return (*THUNAR_COMPONENT_GET_IFACE (component)->get_selected_files) (component);
  else
    return NULL;
}



/**
 * thunar_component_set_selected_files:
 * @component      : a #ThunarComponent instance.
 * @selected_files : a #GList of #ThunarFile<!---->s.
 *
 * Sets the selected files for @component to @selected_files.
 * Check the description of the :selected-files property for
 * details.
 **/
void
thunar_component_set_selected_files (ThunarComponent *component,
                                     GList           *selected_files)
{
  _thunar_return_if_fail (THUNAR_IS_COMPONENT (component));
  if (THUNAR_COMPONENT_GET_IFACE (component)->set_selected_files != NULL)
    (*THUNAR_COMPONENT_GET_IFACE (component)->set_selected_files) (component, selected_files);
}



/**
 * thunar_component_restore_selection:
 * @component      : a #ThunarComponent instance.
 *
 * Make sure that the @selected_files stay selected when a @component
 * updates. This may be necessary on row changes etc.
 **/
void
thunar_component_restore_selection (ThunarComponent *component)
{
  GList           *selected_files;

  _thunar_return_if_fail (THUNAR_IS_COMPONENT (component));

  selected_files = thunar_g_list_copy_deep (thunar_component_get_selected_files (component));
  thunar_component_set_selected_files (component, selected_files);
  thunar_g_list_free_full (selected_files);
}
