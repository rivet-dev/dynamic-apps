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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __NETSTATUS_ICON_H__
#define __NETSTATUS_ICON_H__

#include <gtk/gtk.h>

#include "netstatus-iface.h"

G_BEGIN_DECLS

#define NETSTATUS_TYPE_ICON         (netstatus_icon_get_type ())
#define NETSTATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NETSTATUS_TYPE_ICON, NetstatusIcon))
#define NETSTATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NETSTATUS_TYPE_ICON, NetstatusIconClass))
#define NETSTATUS_IS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NETSTATUS_TYPE_ICON))
#define NETSTATUS_IS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NETSTATUS_TYPE_ICON))
#define NETSTATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NETSTATUS_TYPE_ICON, NetstatusIconClass))

typedef struct _NetstatusIcon        NetstatusIcon;
typedef struct _NetstatusIconClass   NetstatusIconClass;
typedef struct _NetstatusIconPrivate NetstatusIconPrivate;

struct _NetstatusIcon
{
  GtkBox                 box_instance;

  NetstatusIconPrivate  *priv;
};

struct _NetstatusIconClass
{
  GtkBoxClass      box_class;

  void (*invoked) (NetstatusIcon *icon);
};

GType           netstatus_icon_get_type            (void) G_GNUC_CONST;

GtkWidget *     netstatus_icon_new                  (NetstatusIface *iface);

void            netstatus_icon_invoke               (NetstatusIcon  *icon);

void            netstatus_icon_set_iface            (NetstatusIcon  *icon,
						     NetstatusIface *interface);
NetstatusIface *netstatus_icon_get_iface            (NetstatusIcon  *icon);

void            netstatus_icon_set_orientation      (NetstatusIcon  *icon,
						     GtkOrientation  orientation);
GtkOrientation  netstatus_icon_get_orientation      (NetstatusIcon  *icon);

void            netstatus_icon_set_tooltips_enabled (NetstatusIcon  *icon,
						     gboolean        enabled);
gboolean        netstatus_icon_get_tooltips_enabled (NetstatusIcon  *icon);

void            netstatus_icon_set_show_signal      (NetstatusIcon  *icon,
						     gboolean        show_signal);
gboolean        netstatus_icon_get_show_signal      (NetstatusIcon  *icon);

G_END_DECLS

#endif /* __NETSTATUS_ICON_H__ */
