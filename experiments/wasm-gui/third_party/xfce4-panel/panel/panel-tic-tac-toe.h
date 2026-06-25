/*
 * Copyright (C) 2010 Nick Schermer <nick@xfce.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __PANEL_TIC_TAC_TOE_H__
#define __PANEL_TIC_TAC_TOE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PanelTicTacToeClass PanelTicTacToeClass;
typedef struct _PanelTicTacToe      PanelTicTacToe;

#define PANEL_TYPE_TIC_TAC_TOE            (panel_tic_tac_toe_get_type ())
#define PANEL_TIC_TAC_TOE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_TIC_TAC_TOE, PanelTicTacToe))
#define PANEL_TIC_TAC_TOE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_TIC_TAC_TOE, PanelTicTacToeClass))
#define PANEL_IS_TIC_TAC_TOE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_TIC_TAC_TOE))
#define PANEL_IS_TIC_TAC_TOE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_TIC_TAC_TOE))
#define PANEL_TIC_TAC_TOE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_TIC_TAC_TOE, PanelTicTacToeClass))

GType      panel_tic_tac_toe_get_type (void) G_GNUC_CONST;

void       panel_tic_tac_toe_show     (void);

G_END_DECLS

#endif /* !__PANEL_TIC_TAC_TOE_H__ */

