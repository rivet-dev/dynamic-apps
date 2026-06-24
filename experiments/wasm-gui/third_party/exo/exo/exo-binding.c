/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 * Copyright (c) 2004      Victor Porton (http://ex-code.com/~porton/)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo-binding.h>
#include <exo/exo-gobject-extensions.h>
#include <exo/exo-private.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-binding
 * @title: Binding Properties Functions
 * @short_description: Functions used to bind two object properties together
 * @include: exo/exo.h
 * @see_also: <ulink url="http://library.gnome.org/devel/gobject/stable/">
 *            GObject Reference Manual</ulink>,
 *            <link linkend="exo-Extensions-to-GObject">Extensions to GObject</link>
 *
 * Binding properties is synchronizing values of several properties,
 * so that when one of the bound properties changes, the other
 * bound properties are automatically changed to the new value as
 * well. These functions eliminate the need to write property
 * change notification callbacks manually. It also increases the
 * reliability of your project as you don't need to repeat similar
 * code (and errors) manually.
 *
 * Both uni-directional and mutual
 * bindings are supported and you can specify functions to perform
 * explicit transformation of values if required. Multiple properties
 * can be bound together in a complex way and infinite loops are
 * eliminated automatically.
 *
 * For example, lets say, your program has a #GtkEntry widget that allows
 * the user to enter some text for the program, but this entry widget should
 * only be sensitive if a #GtkCheckButton is active.
 *
 * <example>
 * <title>Connecting a <structname>GtkCheckButton</structname> and a
 * <structname>GtkEntry</structname></title>
 * <programlisting>
 * {
 *   GtkWidget *button;
 *   GtkWidget *entry;
 *
 *   button = gtk_check_button_new_with_label ("Activate me");
 *   entry = gtk_entry_new ();
 *
 *   exo_binding_new (G_OBJECT (button), "active",
 *                    G_OBJECT (entry), "sensitive");
 * }
 * </programlisting>
 * </example>
 *
 * As you can see, all you need to do is to call one function to connect
 * the sensitivity of the entry widget with the state of the check
 * button. No need to write signal handlers for this purpose any more.
 **/

typedef struct
{
  GObject             *dst_object;
  GParamSpec          *dst_pspec;
  gulong               dst_handler; /* only set for mutual bindings */
  gulong               handler;
  ExoBindingTransform  transform;
  gpointer             user_data;
}
ExoBindingLink;

/**
 * ExoBinding:
 *
 * Opaque structure representing a one-way binding between two properties.
 * It is automatically removed if one of the bound objects is finalized.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 **/
struct _ExoBinding
{
  GObject         *src_object;
  GDestroyNotify   destroy;
  ExoBindingLink   blink;
};

/**
 * ExoMutualBinding:
 *
 * Opaque structure representing a mutual binding between two properties.
 * It is automatically freed if one of the bound objects is finalized.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 **/
struct _ExoMutualBinding
{
  GDestroyNotify  destroy;
  ExoBindingLink  direct;
  ExoBindingLink  reverse;
};



static void
exo_bind_properties_transfer (GObject             *src_object,
                              GParamSpec          *src_pspec,
                              GObject             *dst_object,
                              GParamSpec          *dst_pspec,
                              ExoBindingTransform  transform,
                              gpointer             user_data)
{
  const gchar *src_name;
  const gchar *dst_name;
  gboolean     result;
  GValue       src_value = { 0, };
  GValue       dst_value = { 0, };

  src_name = g_param_spec_get_name (src_pspec);
  dst_name = g_param_spec_get_name (dst_pspec);

  g_value_init (&src_value, G_PARAM_SPEC_VALUE_TYPE (src_pspec));
  g_object_get_property (src_object, src_name, &src_value);

  g_value_init (&dst_value, G_PARAM_SPEC_VALUE_TYPE (dst_pspec));
  result = (*transform) (&src_value, &dst_value, user_data);

  g_value_unset (&src_value);

  g_return_if_fail (result);

  g_param_value_validate (dst_pspec, &dst_value);
  g_object_set_property (dst_object, dst_name, &dst_value);
  g_value_unset (&dst_value);
}



static void
exo_bind_properties_notify (GObject    *src_object,
                            GParamSpec *src_pspec,
                            gpointer    data)
{
  ExoBindingLink *blink = data;

  /* block the destination handler for mutual bindings,
   * so we don't recurse here.
   */
  if (blink->dst_handler != 0)
    g_signal_handler_block (blink->dst_object, blink->dst_handler);

  exo_bind_properties_transfer (src_object,
                                src_pspec,
                                blink->dst_object,
                                blink->dst_pspec,
                                blink->transform,
                                blink->user_data);

  /* unblock destination handler */
  if (blink->dst_handler != 0)
    g_signal_handler_unblock (blink->dst_object, blink->dst_handler);
}



static void
exo_binding_on_dst_object_destroy (gpointer  data,
                                   GObject  *object)
{
  ExoBinding *binding = data;

  binding->blink.dst_object = NULL;

  /* calls exo_binding_on_disconnect() */
  g_signal_handler_disconnect (binding->src_object, binding->blink.handler);
}



static void
exo_binding_on_disconnect (gpointer  data,
                           GClosure *closure)
{
  ExoBindingLink *blink = data;
  ExoBinding     *binding;

  binding = (ExoBinding *) (((gchar *) blink) - G_STRUCT_OFFSET (ExoBinding, blink));

  if (binding->destroy != NULL)
    binding->destroy (blink->user_data);

  if (blink->dst_object != NULL)
    g_object_weak_unref (blink->dst_object, exo_binding_on_dst_object_destroy, binding);

  g_slice_free (ExoBinding, binding);
}



/* recursively calls exo_mutual_binding_on_disconnect_object2() */
static void
exo_mutual_binding_on_disconnect_object1 (gpointer  data,
                                          GClosure *closure)
{
  ExoMutualBinding *binding;
  ExoBindingLink   *blink = data;
  GObject          *object2;

  binding = (ExoMutualBinding *) (((gchar *) blink) - G_STRUCT_OFFSET (ExoMutualBinding, direct));
  binding->reverse.dst_object = NULL;

  object2 = binding->direct.dst_object;
  if (object2 != NULL)
    {
      if (binding->destroy != NULL)
        binding->destroy (binding->direct.user_data);
      binding->direct.dst_object = NULL;
      g_signal_handler_disconnect (object2, binding->reverse.handler);
      g_slice_free (ExoMutualBinding, binding);
    }
}



/* recursively calls exo_mutual_binding_on_disconnect_object1() */
static void
exo_mutual_binding_on_disconnect_object2 (gpointer  data,
                                          GClosure *closure)
{
  ExoMutualBinding *binding;
  ExoBindingLink   *blink = data;
  GObject          *object1;

  binding = (ExoMutualBinding *) (((gchar *) blink) - G_STRUCT_OFFSET (ExoMutualBinding, reverse));
  binding->direct.dst_object = NULL;

  object1 = binding->reverse.dst_object;
  if (object1 != NULL)
    {
      binding->reverse.dst_object = NULL;
      g_signal_handler_disconnect (object1, binding->direct.handler);
    }
}



static void
exo_binding_link_init (ExoBindingLink     *blink,
                       GObject            *src_object,
                       const gchar        *src_property,
                       GObject            *dst_object,
                       GParamSpec         *dst_pspec,
                       ExoBindingTransform transform,
                       GClosureNotify      destroy_notify,
                       gpointer            user_data)
{
  gchar *signal_name;

  blink->dst_object  = dst_object;
  blink->dst_pspec   = dst_pspec;
  blink->dst_handler = 0;
  blink->transform   = transform;
  blink->user_data   = user_data;

  signal_name = g_strconcat ("notify::", src_property, NULL);
  blink->handler = g_signal_connect_data (src_object,
                                          signal_name,
                                          G_CALLBACK (exo_bind_properties_notify),
                                          blink,
                                          destroy_notify,
                                          0);
  g_free (signal_name);
}



/**
 * exo_binding_new:
 * @src_object:   The source #GObject.
 * @src_property: The name of the property to bind from.
 * @dst_object:   The destination #GObject.
 * @dst_property: The name of the property to bind to.
 *
 * One-way binds @src_property in @src_object to @dst_property
 * in @dst_object.
 *
 * Before binding the value of @dst_property is set to the
 * value of @src_property.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
ExoBinding*
exo_binding_new (GObject      *src_object,
                 const gchar  *src_property,
                 GObject      *dst_object,
                 const gchar  *dst_property)
{
  return exo_binding_new_full (src_object, src_property,
                               dst_object, dst_property,
                               NULL, NULL, NULL);
}



/**
 * exo_binding_new_full:
 * @src_object:     The source #GObject.
 * @src_property:   The name of the property to bind from.
 * @dst_object:     The destination #GObject.
 * @dst_property:   The name of the property to bind to.
 * @transform:      Transformation function or %NULL.
 * @destroy_notify: Callback function that is called on disconnection with @user_data or %NULL.
 * @user_data:      User data associated with the binding.
 *
 * One-way binds @src_property in @src_object to @dst_property
 * in @dst_object.
 *
 * Before binding the value of @dst_property is set to the
 * value of @src_property.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
ExoBinding*
exo_binding_new_full (GObject            *src_object,
                      const gchar        *src_property,
                      GObject            *dst_object,
                      const gchar        *dst_property,
                      ExoBindingTransform transform,
                      GDestroyNotify      destroy_notify,
                      gpointer            user_data)
{
  ExoBinding  *binding;
  GParamSpec  *src_pspec;
  GParamSpec  *dst_pspec;

  g_return_val_if_fail (G_IS_OBJECT (src_object), NULL);
  g_return_val_if_fail (G_IS_OBJECT (dst_object), NULL);

  src_pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (src_object), src_property);
  dst_pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (dst_object), dst_property);

  if (transform == NULL)
    transform = (ExoBindingTransform) (void (*)(void)) g_value_transform;

  exo_bind_properties_transfer (src_object,
                                src_pspec,
                                dst_object,
                                dst_pspec,
                                transform,
                                user_data);

  binding = g_slice_new (ExoBinding);
  binding->src_object = src_object;
  binding->destroy = destroy_notify;

  exo_binding_link_init (&binding->blink,
                         src_object,
                         src_property,
                         dst_object,
                         dst_pspec,
                         transform,
                         exo_binding_on_disconnect,
                         user_data);

  g_object_weak_ref (dst_object, exo_binding_on_dst_object_destroy, binding);

  return binding;
}



/**
 * exo_binding_new_with_negation:
 * @src_object:   The source #GObject.
 * @src_property: The name of the property to bind from.
 * @dst_object:   The destination #GObject.
 * @dst_property: The name of the property to bind to.
 *
 * Convenience function for binding with boolean negation of value.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
ExoBinding*
exo_binding_new_with_negation (GObject      *src_object,
                               const gchar  *src_property,
                               GObject      *dst_object,
                               const gchar  *dst_property)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  ExoBindingTransform transform = (ExoBindingTransform) (void (*)(void)) exo_g_value_transform_negate;

  return exo_binding_new_full (src_object, src_property,
                               dst_object, dst_property,
                               transform, NULL, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
}



/**
 * exo_binding_unbind:
 * @binding: An #ExoBinding to unbind.
 *
 * Disconnects the binding between two properties. Should be
 * rarely used by applications.
 *
 * This functions also calls the @destroy_notify function that
 * was specified when @binding was created.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 **/
void
exo_binding_unbind (ExoBinding *binding)
{
  g_signal_handler_disconnect (binding->src_object, binding->blink.handler);
}



/**
 * exo_mutual_binding_new:
 * @object1:   The first #GObject.
 * @property1: The first property to bind.
 * @object2:   The second #GObject.
 * @property2: The second property to bind.
 *
 * Mutually binds values of two properties.
 *
 * Before binding the value of @property2 is set to the value
 * of @property1.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
ExoMutualBinding*
exo_mutual_binding_new (GObject     *object1,
                        const gchar *property1,
                        GObject     *object2,
                        const gchar *property2)
{
  return exo_mutual_binding_new_full (object1, property1,
                                      object2, property2,
                                      NULL, NULL, NULL, NULL);
}



/**
 * exo_mutual_binding_new_full:
 * @object1:           The first #GObject.
 * @property1:         The first property to bind.
 * @object2:           The second #GObject.
 * @property2:         The second property to bind.
 * @transform:         Transformation function or %NULL.
 * @reverse_transform: The inverse transformation function or %NULL.
 * @destroy_notify:    Callback function called on disconnection with @user_data as argument or %NULL.
 * @user_data:         User data associated with the binding.
 *
 * Mutually binds values of two properties.
 *
 * Before binding the value of @property2 is set to the value of
 * @property1.
 *
 * Both @transform and @reverse_transform should simultaneously be
 * %NULL or non-%NULL. If they are non-%NULL, they should be reverse
 * in each other.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 *
 * Returns: The descriptor of the binding. It is automatically
 *          removed if one of the objects is finalized.
 **/
ExoMutualBinding*
exo_mutual_binding_new_full (GObject            *object1,
                             const gchar        *property1,
                             GObject            *object2,
                             const gchar        *property2,
                             ExoBindingTransform transform,
                             ExoBindingTransform reverse_transform,
                             GDestroyNotify      destroy_notify,
                             gpointer            user_data)
{
  ExoMutualBinding  *binding;
  GParamSpec        *pspec1;
  GParamSpec        *pspec2;

  g_return_val_if_fail (G_IS_OBJECT (object1), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object2), NULL);

  pspec1 = g_object_class_find_property (G_OBJECT_GET_CLASS (object1), property1);
  pspec2 = g_object_class_find_property (G_OBJECT_GET_CLASS (object2), property2);

  if (transform == NULL)
    transform = (ExoBindingTransform) (void (*)(void)) g_value_transform;

  if (reverse_transform == NULL)
    reverse_transform = (ExoBindingTransform) (void (*)(void)) g_value_transform;

  exo_bind_properties_transfer (object1,
                                pspec1,
                                object2,
                                pspec2,
                                transform,
                                user_data);

  binding = g_slice_new (ExoMutualBinding);
  binding->destroy = destroy_notify;

  exo_binding_link_init (&binding->direct,
                         object1,
                         property1,
                         object2,
                         pspec2,
                         transform,
                         exo_mutual_binding_on_disconnect_object1,
                         user_data);

  exo_binding_link_init (&binding->reverse,
                         object2,
                         property2,
                         object1,
                         pspec1,
                         reverse_transform,
                         exo_mutual_binding_on_disconnect_object2,
                         user_data);

  /* tell each link about the reverse link for mutual
   * bindings, to make sure that we do not ever recurse
   * in notify (yeah, the GObject notify dispatching is
   * really weird!).
   */
  binding->direct.dst_handler = binding->reverse.handler;
  binding->reverse.dst_handler = binding->direct.handler;

  return binding;
}



/**
 * exo_mutual_binding_new_with_negation:
 * @object1:   The first #GObject.
 * @property1: The first property to bind.
 * @object2:   The second #GObject.
 * @property2: The second property to bind.
 *
 * Convenience function for binding with boolean negation of value.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 *
 * Returns: The descriptor of the binding. It is automatically removed
 *          if one of the objects if finalized.
 **/
ExoMutualBinding*
exo_mutual_binding_new_with_negation (GObject     *object1,
                                      const gchar *property1,
                                      GObject     *object2,
                                      const gchar *property2)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  ExoBindingTransform transform = (ExoBindingTransform) (void (*)(void)) exo_g_value_transform_negate;

  return exo_mutual_binding_new_full (object1, property1,
                                      object2, property2,
                                      transform, transform,
                                      NULL, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
}



/**
 * exo_mutual_binding_unbind:
 * @binding: An #ExoMutualBinding to unbind.
 *
 * Disconnects the binding between two properties. Should be
 * rarely used by applications.
 *
 * This functions also calls the @destroy_notify function that
 * was specified when @binding was created.
 *
 * Deprecated: xfce 4.18: In favor of GBinding
 **/
void
exo_mutual_binding_unbind (ExoMutualBinding *binding)
{
  g_signal_handler_disconnect (binding->reverse.dst_object, binding->direct.handler);
}



#define __EXO_BINDING_C__
#include <exo/exo-aliasdef.c>
