/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.

 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __NETSTATUS_DIALOG_H__
#define __NETSTATUS_DIALOG_H__

#include <gtk/gtk.h>

#include "netstatus-iface.h"

G_BEGIN_DECLS

GtkWidget *netstatus_dialog_new (NetstatusIface *iface);

/* 2006.10.08 Add by Hong Jen Yee (PCMan) to be used in lxpanel plugin */
void netstatus_dialog_set_configuration_tool( GtkWidget* dialog, const char* tool );
const char* netstatus_dialog_get_configuration_tool( GtkWidget* dialog );

/* 2009.05.10 Add by Hong Jen Yee (PCMan) to be used in lxpanel plugin */
const char* netstatus_dialog_get_iface_name( GtkWidget* dialog );

G_END_DECLS

#endif /* __NETSTATUS_DIALOG_H__ */
