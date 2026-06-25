/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_COMPONENT_H__
#define __THUNAR_COMPONENT_H__

#include <thunar/thunar-navigator.h>

G_BEGIN_DECLS;

typedef struct _ThunarComponentIface ThunarComponentIface;
typedef struct _ThunarComponent      ThunarComponent;

#define THUNAR_TYPE_COMPONENT           (thunar_component_get_type ())
#define THUNAR_COMPONENT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_COMPONENT, ThunarComponent))
#define THUNAR_IS_COMPONENT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_COMPONENT))
#define THUNAR_COMPONENT_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_COMPONENT, ThunarComponentIface))

struct _ThunarComponentIface
{
  GTypeInterface __parent__;

  /* methods */
  GList        *(*get_selected_files) (ThunarComponent *component);
  void          (*set_selected_files) (ThunarComponent *component,
                                       GList           *selected_files);
};

GType         thunar_component_get_type           (void) G_GNUC_CONST;

GList        *thunar_component_get_selected_files  (ThunarComponent *component);
void          thunar_component_set_selected_files  (ThunarComponent *component,
                                                    GList           *selected_files);

void          thunar_component_restore_selection   (ThunarComponent *component);

G_END_DECLS;

#endif /* !__THUNAR_COMPONENT_H__ */
