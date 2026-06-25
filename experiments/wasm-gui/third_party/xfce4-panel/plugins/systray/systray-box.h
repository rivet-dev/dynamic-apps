/*
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __SYSTRAY_BOX_H__
#define __SYSTRAY_BOX_H__

typedef struct _SystrayBoxClass SystrayBoxClass;
typedef struct _SystrayBox      SystrayBox;

/* keep those in sync with the glade file too! */
#define SIZE_MAX_MIN     (12)
#define SIZE_MAX_MAX     (64)
#define SIZE_MAX_DEFAULT (22)

#define XFCE_TYPE_SYSTRAY_BOX            (systray_box_get_type ())
#define XFCE_SYSTRAY_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SYSTRAY_BOX, SystrayBox))
#define XFCE_SYSTRAY_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SYSTRAY_BOX, SystrayBoxClass))
#define XFCE_IS_SYSTRAY_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SYSTRAY_BOX))
#define XFCE_IS_SYSTRAY_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SYSTRAY_BOX))
#define XFCE_SYSTRAY_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SYSTRAY_BOX, SystrayBoxClass))

GType      systray_box_get_type        (void) G_GNUC_CONST;

void       systray_box_register_type   (XfcePanelTypeModule *module);

GtkWidget *systray_box_new             (void) G_GNUC_MALLOC;

void       systray_box_set_orientation (SystrayBox          *box,
                                        GtkOrientation       orientation);

void       systray_box_set_dimensions  (SystrayBox          *box,
                                        gint                 icon_size,
                                        gint                 n_rows,
                                        gint                 row_size,
                                        gint                 padding);

void       systray_box_set_size_alloc  (SystrayBox          *box,
                                        gint                 size_alloc);

void       systray_box_set_show_hidden (SystrayBox          *box,
                                        gboolean             show_hidden);

gboolean   systray_box_get_show_hidden (SystrayBox          *box);

void       systray_box_set_squared     (SystrayBox          *box,
                                        gboolean             square_icons);

gboolean   systray_box_get_squared     (SystrayBox          *box);

void       systray_box_update          (SystrayBox          *box,
                                        GSList              *names_ordered);

gboolean   systray_box_has_hidden_items (SystrayBox         *box);

void       systray_box_set_single_row  (SystrayBox          *box,
                                        gboolean             single_row);

#endif /* !__SYSTRAY_BOX_H__ */
