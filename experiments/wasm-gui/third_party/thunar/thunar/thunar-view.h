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

#ifndef __THUNAR_VIEW_H__
#define __THUNAR_VIEW_H__

#include <thunar/thunar-component.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-navigator.h>

G_BEGIN_DECLS;

typedef struct _ThunarViewIface ThunarViewIface;
typedef struct _ThunarView      ThunarView;

#define THUNAR_TYPE_VIEW            (thunar_view_get_type ())
#define THUNAR_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_VIEW, ThunarView))
#define THUNAR_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_VIEW))
#define THUNAR_VIEW_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_VIEW, ThunarViewIface))

struct _ThunarViewIface
{
  GTypeInterface __parent__;

  /* virtual methods */
  gboolean        (*get_loading)        (ThunarView     *view);
  const gchar    *(*get_statusbar_text) (ThunarView     *view);

  gboolean        (*get_show_hidden)    (ThunarView     *view);
  void            (*set_show_hidden)    (ThunarView     *view,
                                         gboolean        show_hidden);

  ThunarZoomLevel (*get_zoom_level)     (ThunarView     *view);
  void            (*set_zoom_level)     (ThunarView     *view,
                                         ThunarZoomLevel zoom_level);
  void            (*reset_zoom_level)   (ThunarView     *view);

  void            (*reload)             (ThunarView     *view,
                                         gboolean        reload_info);

  gboolean        (*get_visible_range)  (ThunarView     *view,
                                         ThunarFile    **start_file,
                                         ThunarFile    **end_file);

  void            (*scroll_to_file)     (ThunarView     *view,
                                         ThunarFile     *file,
                                         gboolean        select,
                                         gboolean        use_align,
                                         gfloat          row_align,
                                         gfloat          col_align);
  GList*          (*get_selected_files) (ThunarView     *view);
  void            (*set_selected_files) (ThunarView     *view,
                                         GList          *path_list);
};

GType           thunar_view_get_type            (void) G_GNUC_CONST;

gboolean        thunar_view_get_loading         (ThunarView     *view);
const gchar    *thunar_view_get_statusbar_text  (ThunarView     *view);

gboolean        thunar_view_get_show_hidden     (ThunarView     *view);
void            thunar_view_set_show_hidden     (ThunarView     *view,
                                                 gboolean        show_hidden);

ThunarZoomLevel thunar_view_get_zoom_level      (ThunarView     *view);
void            thunar_view_set_zoom_level      (ThunarView     *view,
                                                 ThunarZoomLevel zoom_level);
void            thunar_view_reset_zoom_level    (ThunarView     *view);

void            thunar_view_reload              (ThunarView     *view,
                                                 gboolean        reload_info);

gboolean        thunar_view_get_visible_range   (ThunarView     *view,
                                                 ThunarFile    **start_file,
                                                 ThunarFile    **end_file);

void            thunar_view_scroll_to_file      (ThunarView     *view,
                                                 ThunarFile     *file,
                                                 gboolean        select_file,
                                                 gboolean        use_align,
                                                 gfloat          row_align,
                                                 gfloat          col_align);
GList*          thunar_view_get_selected_files  (ThunarView     *view);
void            thunar_view_set_selected_files  (ThunarView     *view,
                                                 GList          *path_list);

G_END_DECLS;

#endif /* !__THUNAR_VIEW_H__ */
