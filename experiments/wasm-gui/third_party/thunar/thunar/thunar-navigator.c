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

#include <thunar/thunar-navigator.h>
#include <thunar/thunar-private.h>



enum
{
  CHANGE_DIRECTORY,
  OPEN_NEW_TAB,
  LAST_SIGNAL,
};



static void thunar_navigator_base_init  (gpointer klass);
static void thunar_navigator_class_init (gpointer klass);



static guint navigator_signals[LAST_SIGNAL];



GType
thunar_navigator_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarNavigatorIface),
        (GBaseInitFunc) thunar_navigator_base_init,
        NULL,
        (GClassInitFunc) (void (*)(void)) thunar_navigator_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE, I_("ThunarNavigator"), &info, 0);
      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}



static void
thunar_navigator_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      /**
       * ThunarNavigator::change-directory:
       * @navigator : a #ThunarNavigator instance.
       * @directory : a #ThunarFile referring to the new directory.
       *
       * Invoked by implementing classes whenever the user requests
       * to changed the current directory to @directory from within
       * the @navigator instance (e.g. for the location buttons bar,
       * this signal would be invoked whenever the user clicks on
       * a path button).
       *
       * The @navigator must not apply the @directory to the
       * "current-directory" property directly. But
       * instead, it must wait for the surrounding module (usually
       * a #ThunarWindow instance) to explicitly inform the
       * @navigator to change it's current directory using
       * the #thunar_navigator_set_current_directory() method
       * or the "current-directory" property.
       **/
      navigator_signals[CHANGE_DIRECTORY] =
        g_signal_new (I_("change-directory"),
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarNavigatorIface, change_directory),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, THUNAR_TYPE_FILE);

      navigator_signals[OPEN_NEW_TAB] =
        g_signal_new (I_("open-new-tab"),
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarNavigatorIface, open_new_tab),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, THUNAR_TYPE_FILE);

      initialized = TRUE;
    }
}



static void
thunar_navigator_class_init (gpointer klass)
{
  /**
   * ThunarNavigator:current-directory:
   *
   * The directory currently displayed by this #ThunarNavigator
   * instance or %NULL if no directory is currently displayed
   * (it's up to the implementing class to define the appearance
   * of a navigator that has no directory associated with it).
   *
   * Whenever a navigator wants the surrounding module (usually
   * a #ThunarWindow) to change to another directory, it should
   * invoke the "change-directory" signal using the
   * #thunar_navigator_change_directory() method. It should
   * not directly change the "current-directory" property,
   * but wait for the surrounding module to change the
   * "current-directory" property afterwards.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_object ("current-directory",
                                                            "current-directory",
                                                            "current-directory",
                                                            THUNAR_TYPE_FILE,
                                                            EXO_PARAM_READWRITE));
}



/**
 * thunar_navigator_get_current_directory:
 * @navigator : a #ThunarNavigator instance.
 *
 * Returns the directory currently displayed by @navigator
 * or %NULL, if @navigator does not currently display and
 * directory.
 *
 * Return value: the current directory of @navigator or %NULL.
 **/
ThunarFile*
thunar_navigator_get_current_directory (ThunarNavigator *navigator)
{
  _thunar_return_val_if_fail (THUNAR_IS_NAVIGATOR (navigator), NULL);
  return THUNAR_NAVIGATOR_GET_IFACE (navigator)->get_current_directory (navigator);
}



/**
 * thunar_navigator_set_current_directory:
 * @navigator         : a #ThunarNavigator instance.
 * @current_directory : the new directory to display or %NULL.
 *
 * Sets a new current directory that should be displayed by
 * the @navigator.
 **/
void
thunar_navigator_set_current_directory (ThunarNavigator *navigator,
                                        ThunarFile      *current_directory)
{
  _thunar_return_if_fail (THUNAR_IS_NAVIGATOR (navigator));
  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));
  THUNAR_NAVIGATOR_GET_IFACE (navigator)->set_current_directory (navigator, current_directory);
}



/**
 * thunar_navigator_change_directory:
 * @navigator : a #ThunarNavigator instance.
 * @directory : a #ThunarFile referring to a directory.
 *
 * Emits the "change-directory" signal on @navigator with
 * the specified @directory.
 *
 * Derived classes should invoke this method whenever the user
 * selects a new directory from within @navigator. The derived
 * class should not perform any directory changing operations
 * itself, but leave it up to the surrounding module (usually
 * a #ThunarWindow instance) to change the directory.
 *
 * It should never ever be called from outside a #ThunarNavigator
 * implementation, as that may led to unexpected results!
 **/
void
thunar_navigator_change_directory (ThunarNavigator *navigator,
                                   ThunarFile      *directory)
{
  _thunar_return_if_fail (THUNAR_IS_NAVIGATOR (navigator));
  _thunar_return_if_fail (THUNAR_IS_FILE (directory));
  _thunar_return_if_fail (thunar_file_is_directory (directory));

  g_signal_emit (G_OBJECT (navigator), navigator_signals[CHANGE_DIRECTORY], 0, directory);
}



void
thunar_navigator_open_new_tab (ThunarNavigator *navigator,
                               ThunarFile      *directory)
{
  _thunar_return_if_fail (THUNAR_IS_NAVIGATOR (navigator));
  _thunar_return_if_fail (THUNAR_IS_FILE (directory));
  _thunar_return_if_fail (thunar_file_is_directory (directory));

  g_signal_emit (G_OBJECT (navigator), navigator_signals[OPEN_NEW_TAB], 0, directory);
}
