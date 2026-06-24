/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat Inc.
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
 *
 * Authors:
 *    Erwann Chenede  <erwann.chenede@sun.com>
 *    Mark McLoughlin  <mark@skynet.ie>
 */

#ifndef __NETSTATUS_IFACE_H__
#define __NETSTATUS_IFACE_H__

#include <glib.h>
#include <glib-object.h>

#include "netstatus-util.h"

G_BEGIN_DECLS

#define NETSTATUS_TYPE_IFACE         (netstatus_iface_get_type ())
#define NETSTATUS_IFACE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NETSTATUS_TYPE_IFACE, NetstatusIface))
#define NETSTATUS_IFACE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NETSTATUS_TYPE_IFACE, NetstatusIfaceClass))
#define NETSTATUS_IS_IFACE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NETSTATUS_TYPE_IFACE))
#define NETSTATUS_IS_IFACE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NETSTATUS_TYPE_IFACE))
#define NETSTATUS_IFACE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NETSTATUS_TYPE_IFACE, NetstatusIfaceClass))

typedef struct _NetstatusIface        NetstatusIface;
typedef struct _NetstatusIfaceClass   NetstatusIfaceClass;
typedef struct _NetstatusIfacePrivate NetstatusIfacePrivate;

struct _NetstatusIface
{
  GObject                 parent_instance;

  NetstatusIfacePrivate  *priv;
};

struct _NetstatusIfaceClass
{
  GObjectClass parent_class;
};

GList *                netstatus_list_interface_names        (GError         **error);

GType                  netstatus_iface_get_type              (void) G_GNUC_CONST;

NetstatusIface *       netstatus_iface_new                   (const char      *name);

const char *           netstatus_iface_get_name              (NetstatusIface  *iface);
void                   netstatus_iface_set_name              (NetstatusIface  *iface,
							      const char      *name);
NetstatusState         netstatus_iface_get_state             (NetstatusIface  *iface);
void                   netstatus_iface_get_statistics        (NetstatusIface  *iface,
							      NetstatusStats  *stats);
gboolean               netstatus_iface_get_is_wireless       (NetstatusIface  *iface);
int                    netstatus_iface_get_signal_strength   (NetstatusIface  *iface);

void                   netstatus_iface_set_error             (NetstatusIface  *iface,
							      const GError    *error);
const GError *         netstatus_iface_get_error             (NetstatusIface  *iface);
void                   netstatus_iface_clear_error           (NetstatusIface  *iface,
							      NetstatusError   code);

gboolean               netstatus_iface_get_inet4_details     (NetstatusIface  *iface,
							      char           **addr,
							      char           **dest,
							      char           **bcast,
							      char           **mask);
gboolean               netstatus_iface_get_device_details    (NetstatusIface  *iface,
							      const char     **hw_name,
							      char           **hw_addr);
gboolean               netstatus_iface_get_is_loopback       (NetstatusIface  *iface);

G_END_DECLS

#endif /* __NETSTATUS_IFACE_H__ */
