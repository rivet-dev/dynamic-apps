/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __THUNARX_H__
#define __THUNARX_H__

#define THUNARX_INSIDE_THUNARX_H

#include <thunarx/thunarx-config.h>
#include <thunarx/thunarx-file-info.h>
#include <thunarx/thunarx-menu.h>
#include <thunarx/thunarx-menu-provider.h>
#include <thunarx/thunarx-preferences-provider.h>
#include <thunarx/thunarx-property-page.h>
#include <thunarx/thunarx-property-page-provider.h>
#include <thunarx/thunarx-provider-factory.h>
#include <thunarx/thunarx-provider-plugin.h>
#include <thunarx/thunarx-renamer.h>
#include <thunarx/thunarx-renamer-provider.h>

#undef THUNARX_INSIDE_THUNARX_H

#define THUNARX_DEFINE_TYPE(TN, t_n, T_P)                         THUNARX_DEFINE_TYPE_EXTENDED (TN, t_n, T_P, 0, {})
#define THUNARX_DEFINE_TYPE_WITH_CODE(TN, t_n, T_P, _C_)          THUNARX_DEFINE_TYPE_EXTENDED (TN, t_n, T_P, 0, _C_)
#define THUNARX_DEFINE_ABSTRACT_TYPE(TN, t_n, T_P)                THUNARX_DEFINE_TYPE_EXTENDED (TN, t_n, T_P, G_TYPE_FLAG_ABSTRACT, {})
#define THUNARX_DEFINE_ABSTRACT_TYPE_WITH_CODE(TN, t_n, T_P, _C_) THUNARX_DEFINE_TYPE_EXTENDED (TN, t_n, T_P, G_TYPE_FLAG_ABSTRACT, _C_)

#define THUNARX_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, CODE) \
static gpointer type_name##_parent_class = NULL; \
static GType    type_name##_type = G_TYPE_INVALID; \
\
static void     type_name##_init              (TypeName        *self); \
static void     type_name##_class_init        (TypeName##Class *klass); \
static void     type_name##_class_intern_init (TypeName##Class *klass) \
{ \
  type_name##_parent_class = g_type_class_peek_parent (klass); \
  type_name##_class_init (klass); \
} \
\
GType \
type_name##_get_type (void) \
{ \
  return type_name##_type; \
} \
\
void \
type_name##_register_type (ThunarxProviderPlugin *thunarx_define_type_plugin) \
{ \
  GType thunarx_define_type_id; \
  static const GTypeInfo thunarx_define_type_info = \
  { \
    sizeof (TypeName##Class), \
    NULL, \
    NULL, \
    (GClassInitFunc) (void (*)(void)) type_name##_class_intern_init, \
    NULL, \
    NULL, \
    sizeof (TypeName), \
    0, \
    (GInstanceInitFunc) (void (*)(void)) type_name##_init, \
    NULL, \
  }; \
  thunarx_define_type_id = thunarx_provider_plugin_register_type (thunarx_define_type_plugin, TYPE_PARENT, \
                                                                  #TypeName, &thunarx_define_type_info, flags); \
  { CODE ; } \
  type_name##_type = thunarx_define_type_id; \
}

#define THUNARX_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
{ \
  static const GInterfaceInfo thunarx_implement_interface_info = \
  { \
    .interface_init = (GInterfaceInitFunc) (void (*)(void)) iface_init \
  }; \
  thunarx_provider_plugin_add_interface (thunarx_define_type_plugin, thunarx_define_type_id, TYPE_IFACE, &thunarx_implement_interface_info); \
}

#endif /* !__THUNARX_H__ */
