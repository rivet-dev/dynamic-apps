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

#ifndef __THUNAR_ICON_RENDERER_H__
#define __THUNAR_ICON_RENDERER_H__

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarIconRendererClass ThunarIconRendererClass;
typedef struct _ThunarIconRenderer      ThunarIconRenderer;

#define THUNAR_TYPE_ICON_RENDERER            (thunar_icon_renderer_get_type ())
#define THUNAR_ICON_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ICON_RENDERER, ThunarIconRenderer))
#define THUNAR_ICON_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ICON_RENDERER, ThunarIconRendererClass))
#define THUNAR_IS_ICON_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ICON_RENDERER))
#define THUNAR_IS_ICON_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_ICON_RENDERER))
#define THUNAR_ICON_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ICON_RENDERER, ThunarIconRendererClass))

struct _ThunarIconRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _ThunarIconRenderer
{
  GtkCellRenderer __parent__;

  ThunarFile    *drop_file;
  ThunarFile    *file;
  gboolean       emblems;
  gboolean       follow_state;
  ThunarIconSize size;
  gchar         *highlight_color;
  gboolean       rounded_corners;
  gboolean       highlighting_enabled;
};

GType            thunar_icon_renderer_get_type (void) G_GNUC_CONST;

GtkCellRenderer *thunar_icon_renderer_new      (void) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_ICON_RENDERER_H__ */
