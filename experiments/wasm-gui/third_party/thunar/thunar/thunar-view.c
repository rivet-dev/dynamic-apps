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
#include <thunar/thunar-view.h>



static void thunar_view_class_init (gpointer klass);



GType
thunar_view_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarView"),
                                            sizeof (ThunarViewIface),
                                            (GClassInitFunc) (void (*)(void)) thunar_view_class_init,
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
thunar_view_class_init (gpointer klass)
{
  /**
   * ThunarView:loading:
   *
   * Indicates whether the given #ThunarView is currently loading or
   * layouting its contents. Implementations should invoke
   * #g_object_notify() on this property whenever they start to load
   * the contents and then once they have finished loading.
   *
   * Other modules can use this property to display some kind of
   * user visible notification about the loading state, e.g. a
   * progress bar or an animated image.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("loading",
                                                             "loading",
                                                             "loading",
                                                             FALSE,
                                                             EXO_PARAM_READABLE));

  /**
   * ThunarView:statusbar-text:
   *
   * The text to be displayed in the status bar, which is associated
   * with this #ThunarView instance. Implementations should invoke
   * #g_object_notify() on this property, whenever they have a new
   * text to be display in the status bar (e.g. the selection changed
   * or similar).
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_string ("statusbar-text",
                                                            "statusbar-text",
                                                            "statusbar-text",
                                                            NULL,
                                                            EXO_PARAM_READABLE));

  /**
   * ThunarView:show-hidden:
   *
   * Tells whether to display hidden and backup files in the
   * #ThunarView or whether to hide them.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("show-hidden",
                                                             "show-hidden",
                                                             "show-hidden",
                                                             FALSE,
                                                             EXO_PARAM_READWRITE));

  /**
   * ThunarView:zoom-level:
   *
   * The #ThunarZoomLevel at which the items within this
   * #ThunarView should be displayed.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_enum ("zoom-level",
                                                          "zoom-level",
                                                          "zoom-level",
                                                          THUNAR_TYPE_ZOOM_LEVEL,
                                                          THUNAR_ZOOM_LEVEL_100_PERCENT,
                                                          EXO_PARAM_READWRITE));
}



/**
 * thunar_view_get_loading:
 * @view : a #ThunarView instance.
 *
 * Tells whether the given #ThunarView is currently loading or
 * layouting its contents.
 *
 * Return value: %TRUE if @view is currently being loaded, else %FALSE.
 **/
gboolean
thunar_view_get_loading (ThunarView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), FALSE);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_loading) (view);
}



/**
 * thunar_view_get_statusbar_text:
 * @view : a #ThunarView instance.
 *
 * Queries the text that should be displayed in the status bar
 * associated with @view.
 *
 * Return value: the text to be displayed in the status bar
 *               asssociated with @view.
 **/
const gchar*
thunar_view_get_statusbar_text (ThunarView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), NULL);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_statusbar_text) (view);
}



/**
 * thunar_view_get_show_hidden:
 * @view : a #ThunarView instance.
 *
 * Returns %TRUE if hidden and backup files are shown
 * in @view. Else %FALSE is returned.
 *
 * Return value: whether @view displays hidden files.
 **/
gboolean
thunar_view_get_show_hidden (ThunarView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), FALSE);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_show_hidden) (view);
}



/**
 * thunar_view_set_show_hidden:
 * @view        : a #ThunarView instance.
 * @show_hidden : &TRUE to display hidden files, else %FALSE.
 *
 * If @show_hidden is %TRUE then @view will display hidden and
 * backup files, else those files will be hidden from the user
 * interface.
 **/
void
thunar_view_set_show_hidden (ThunarView *view,
                             gboolean    show_hidden)
{
  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  (*THUNAR_VIEW_GET_IFACE (view)->set_show_hidden) (view, show_hidden);
}



/**
 * thunar_view_get_zoom_level:
 * @view : a #ThunarView instance.
 *
 * Returns the #ThunarZoomLevel currently used for the @view.
 *
 * Return value: the #ThunarZoomLevel currently used for the @view.
 **/
ThunarZoomLevel
thunar_view_get_zoom_level (ThunarView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), THUNAR_ZOOM_LEVEL_100_PERCENT);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_zoom_level) (view);
}



/**
 * thunar_view_set_zoom_level:
 * @view       : a #ThunarView instance.
 * @zoom_level : the new #ThunarZoomLevel for @view.
 *
 * Sets the zoom level used for @view to @zoom_level.
 **/
void
thunar_view_set_zoom_level (ThunarView     *view,
                            ThunarZoomLevel zoom_level)
{
  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (zoom_level < THUNAR_ZOOM_N_LEVELS);
  (*THUNAR_VIEW_GET_IFACE (view)->set_zoom_level) (view, zoom_level);
}



/**
 * thunar_view_reset_zoom_level:
 * @view : a #ThunarView instance.
 *
 * Resets the zoom level of @view to the default
 * #ThunarZoomLevel for @view.
 **/
void
thunar_view_reset_zoom_level (ThunarView *view)
{
  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  (*THUNAR_VIEW_GET_IFACE (view)->reset_zoom_level) (view);
}



/**
 * thunar_view_reload:
 * @view : a #ThunarView instance.
 * @reload_info : whether to reload file info for all files too
 *
 * Tells @view to reread the currently displayed folder
 * contents from the underlying media. If reload_info is
 * TRUE, it will reload information for all files too.
 **/
void
thunar_view_reload (ThunarView *view,
                    gboolean    reload_info)
{
  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  (*THUNAR_VIEW_GET_IFACE (view)->reload) (view, reload_info);
}



/**
 * thunar_view_get_visible_range:
 * @view       : a #ThunarView instance.
 * @start_file : return location for start of region, or %NULL.
 * @end_file   : return location for end of region, or %NULL.
 *
 * Sets @start_file and @end_file to be the first and last visible
 * #ThunarFile.
 *
 * The files should be freed with g_object_unref() when no
 * longer needed.
 *
 * Return value: %TRUE if valid files were placed in @start_file
 *               and @end_file.
 **/
gboolean
thunar_view_get_visible_range (ThunarView  *view,
                               ThunarFile **start_file,
                               ThunarFile **end_file)
{
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), FALSE);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_visible_range) (view, start_file, end_file);
}



/**
 * thunar_view_scroll_to_file:
 * @view        : a #ThunarView instance.
 * @file        : the #ThunarFile to scroll to.
 * @select_file : %TRUE to also select the @file in the @view.
 * @use_align   : whether to use alignment arguments.
 * @row_align   : the vertical alignment.
 * @col_align   : the horizontal alignment.
 *
 * Tells @view to scroll to the @file. If @view is currently
 * loading, it'll remember to scroll to @file later when
 * the contents are loaded.
 **/
void
thunar_view_scroll_to_file (ThunarView *view,
                            ThunarFile *file,
                            gboolean    select_file,
                            gboolean    use_align,
                            gfloat      row_align,
                            gfloat      col_align)
{
  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (row_align >= 0.0f && row_align <= 1.0f);
  _thunar_return_if_fail (col_align >= 0.0f && col_align <= 1.0f);
  (*THUNAR_VIEW_GET_IFACE (view)->scroll_to_file) (view, file, select_file, use_align, row_align, col_align);
}



GList*
thunar_view_get_selected_files (ThunarView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), NULL);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_selected_files) (view);
}



void
thunar_view_set_selected_files (ThunarView *view,
                                GList      *path_list)
{
  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  (*THUNAR_VIEW_GET_IFACE (view)->set_selected_files) (view, path_list);
}
