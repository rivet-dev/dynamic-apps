/*
 * Copyright (C) 2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ICON_GRID_H__
#define __ICON_GRID_H__ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_TYPE_ICON_GRID               (panel_icon_grid_get_type())
#define PANEL_ICON_GRID(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                            PANEL_TYPE_ICON_GRID, PanelIconGrid))
#define PANEL_ICON_GRID_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass), \
                                            PANEL_TYPE_ICON_GRID, PanelIconGridClass))
#define PANEL_IS_ICON_GRID(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                            PANEL_TYPE_ICON_GRID))
#define PANEL_IS_ICON_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                            PANEL_TYPE_ICON_GRID))

extern GType panel_icon_grid_get_type   (void) G_GNUC_CONST;

typedef struct _PanelIconGrid           PanelIconGrid;
typedef struct _PanelIconGridClass      PanelIconGridClass;

extern GtkWidget * panel_icon_grid_new(
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension);
						/* Create an icon grid */

/**
 * panel_icon_grid_set_constrain_width
 * @ig: a widget
 * @constrain_width: value to set
 *
 * Changes #PanelIconGrid::constrain-width property on the @ig. This
 * property defines whether children of @ig may be constrained in case if
 * the size allocated to @ig isn't sufficient to give assigned size for
 * all existing children of @ig. All children will receive the same size
 * deficit in described case.
 */
extern void panel_icon_grid_set_constrain_width(PanelIconGrid * ig, gboolean constrain_width);

/**
 * panel_icon_grid_set_aspect_width
 * @ig: a widget
 * @aspect_width: value to set
 *
 * Changes #PanelIconGrid::aspect-width property on the @ig. The property
 * enables or disables taking icons aspect into consideration for @ig.
 * If disabled then all icons will have the same size (child_width) as
 * defined for widget. If enabled then child_width will be ignored but
 * icons aspect will be taken into consideration when children space is
 * allocated.
 * Note that if #PanelIconGrid::constrain-width is set to %TRUE then the
 * #PanelIconGrid::aspect-width is ignored and behavior of widget is that
 * as if #PanelIconGrid::aspect-width is set to %FALSE.
 *
 * Since: 0.8.0
 */
extern void panel_icon_grid_set_aspect_width(PanelIconGrid * ig, gboolean aspect_width);

/* extern void panel_icon_grid_set_fill_width(PanelIconGrid * ig, gboolean fill_width);
						 Set the fill-width property */
extern void panel_icon_grid_set_geometry(PanelIconGrid * ig,
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension);
						/* Change the geometry of an icon grid */
extern gint panel_icon_grid_get_child_position(PanelIconGrid * ig, GtkWidget * child);
						/* Get the index of an icon grid element. */
extern void panel_icon_grid_reorder_child(PanelIconGrid * ig, GtkWidget * child, gint position);
						/* Reorder the position of a child in the icon grid */
extern guint panel_icon_grid_get_n_children(PanelIconGrid * ig);
						/* Count non-internal children */

typedef enum {
    PANEL_ICON_GRID_DROP_LEFT_AFTER,
    PANEL_ICON_GRID_DROP_LEFT_BEFORE,
    PANEL_ICON_GRID_DROP_RIGHT_AFTER,
    PANEL_ICON_GRID_DROP_RIGHT_BEFORE,
    PANEL_ICON_GRID_DROP_BELOW,
    PANEL_ICON_GRID_DROP_ABOVE,
    PANEL_ICON_GRID_DROP_INTO
} PanelIconGridDropPosition;

/**
 * panel_icon_grid_get_dest_at_pos
 * @ig: a widget
 * @x: coordinate to inspect
 * @y: coordinate to inspect
 * @child: (allow-none): (out) (transfer none): pointer to return found child
 * @pos: (allow-none): (out): pointer to return drop position
 *
 * Inspects coordinates @x and @y within @ig to contain some child widget.
 * Returns nearest child widget and position against it.
 *
 * Returns: %FALSE if position is not drawable.
 *
 * Since: 0.9.0
 */
extern gboolean panel_icon_grid_get_dest_at_pos(PanelIconGrid * ig, gint x, gint y,
                            GtkWidget ** child, PanelIconGridDropPosition * pos);

/**
 * panel_icon_grid_set_drag_dest
 * @ig: a widget
 * @child: (allow-none): pointer to a child
 * @pos: drop position to draw
 *
 * Queues drawing of focus for given @child and @pos. If @child is %NULL
 * then nothing will be drawn. In any case previous focus will be wiped.
 *
 * Since: 0.9.0
 */
extern void panel_icon_grid_set_drag_dest(PanelIconGrid * ig, GtkWidget * child,
                                          PanelIconGridDropPosition pos);

/**
 * panel_icon_grid_get_drag_dest
 * @ig: a widget
 * @child: (allow-none): pointer to pointer to a child
 *
 * Retrieves data last set with panel_icon_grid_set_drag_dest()
 *
 * Returns: position to drop.
 *
 * Since: 0.9.0
 */
extern PanelIconGridDropPosition panel_icon_grid_get_drag_dest(PanelIconGrid * ig, GtkWidget ** child);

G_END_DECLS

#endif
