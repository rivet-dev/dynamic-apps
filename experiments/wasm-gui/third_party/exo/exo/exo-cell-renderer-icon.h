/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined (EXO_INSIDE_EXO_H) && !defined (EXO_COMPILATION)
#error "Only <exo/exo.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __EXO_CELL_RENDERER_ICON_H__
#define __EXO_CELL_RENDERER_ICON_H__

#include <exo/exo-config.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ExoCellRendererIconPrivate ExoCellRendererIconPrivate;
typedef struct _ExoCellRendererIconClass   ExoCellRendererIconClass;
typedef struct _ExoCellRendererIcon        ExoCellRendererIcon;

#define EXO_TYPE_CELL_RENDERER_ICON             (exo_cell_renderer_icon_get_type ())
#define EXO_CELL_RENDERER_ICON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_CELL_RENDERER_ICON, ExoCellRendererIcon))
#define EXO_CELL_RENDERER_ICON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_CELL_RENDERER_ICON, ExoCellRendererIconClass))
#define EXO_IS_CELL_RENDERER_ICON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_CELL_RENDERER_ICON))
#define EXO_IS_CELL_RENDERER_ICON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_CELL_RENDERER_ICON))
#define EXO_CELL_RENDERER_ICON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_CELL_RENDERER_ICON, ExoCellRendererIconClass))

struct _ExoCellRendererIconClass
{
  /*< private >*/
  GtkCellRendererClass __parent__;

  /* reserved for future expansion */
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
  void (*reserved6) (void);
};

/**
 * ExoCellRendererIcon:
 *
 * The #ExoIconChooserDialog struct contains only private fields and
 * should not be directly accessed.
 **/
struct _ExoCellRendererIcon
{
  /*< private >*/
  GtkCellRenderer __parent__;
};

GType            exo_cell_renderer_icon_get_type (void) G_GNUC_CONST;

GtkCellRenderer *exo_cell_renderer_icon_new      (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* !__EXO_CELL_RENDERER_ICON_H__ */
