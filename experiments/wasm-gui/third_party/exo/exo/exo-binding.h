/*-
 * Copyright (c) 2004 os-cillation e.K.
 * Copyright (c) 2004 Victor Porton (http://ex-code.com/~porton/)
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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

#ifndef __EXO_BINDING_H__
#define __EXO_BINDING_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ExoBinding       ExoBinding;
typedef struct _ExoMutualBinding ExoMutualBinding;

/**
 * ExoBindingTransform:
 * @src_value : Value to transform.
 * @dst_value : Value to store the result of the transformation into.
 * @user_data : User data supplied at binding creation.
 *
 * Function type used for binding transformation functions.
 *
 * Accomplished transformation from @src_value to @dst_value.
 * @src_value and @dst_value are already initialized before
 * this function gets called.
 *
 * Returns: %FALSE if transformation failed, else %TRUE.
 **/
typedef gboolean  (*ExoBindingTransform)  (const GValue *src_value,
                                           GValue       *dst_value,
                                           gpointer      user_data);



ExoBinding        *exo_binding_new                      (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property)
  G_GNUC_DEPRECATED_FOR (g_object_bind_property() with G_BINDING_SYNC_CREATE flag);
ExoBinding        *exo_binding_new_full                 (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property,
                                                         ExoBindingTransform transform,
                                                         GDestroyNotify      destroy_notify,
                                                         gpointer            user_data)
G_GNUC_DEPRECATED_FOR (g_object_bind_property_full() with G_BINDING_SYNC_CREATE flag);
ExoBinding        *exo_binding_new_with_negation        (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property)
  G_GNUC_DEPRECATED_FOR (g_object_bind_property() with G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE flag);
void               exo_binding_unbind                   (ExoBinding         *binding)
  G_GNUC_DEPRECATED_FOR (g_binding_unbind() or g_object_unref());

ExoMutualBinding  *exo_mutual_binding_new               (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2)
  G_GNUC_DEPRECATED_FOR (g_object_bind_property() with G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE flag);
ExoMutualBinding  *exo_mutual_binding_new_full          (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2,
                                                         ExoBindingTransform transform,
                                                         ExoBindingTransform reverse_transform,
                                                         GDestroyNotify      destroy_notify,
                                                         gpointer            user_data)
G_GNUC_DEPRECATED_FOR (g_object_bind_property_full() with G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE flag);
ExoMutualBinding  *exo_mutual_binding_new_with_negation (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2)
  G_GNUC_DEPRECATED_FOR (g_object_bind_property() with G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE flag);
void               exo_mutual_binding_unbind            (ExoMutualBinding   *binding)
  G_GNUC_DEPRECATED_FOR (g_binding_unbind() or g_object_unref());

G_END_DECLS

#endif /* !__EXO_BINDING_H__ */
