/*
 *  xfconf
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2009 Nick Schermer <nick@xfce.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; version 2
 *  of the License ONLY.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "xfconf.h"
#include "xfconf-private.h"
#include "common/xfconf-alias.h"
#include "common/xfconf-common-private.h"


/**
 * SECTION:xfconf-binding
 * @title: Xfconf-GObject Binding
 * @short_description: Functions to bind Xfconf properties to GObject properties
 *
 * Often it may be useful to bind an Xfconf property to a GObject property.
 * Settings dialogs often display the current value of an Xfconf property,
 * and a user may edit the value to change the value in the Xfconf store.
 * If the Xfconf property changes outside the settings dialog, the user will
 * usually want to see the settings dialog automatically update to reflect
 * the new value.
 *
 * With a single line of code, Xfconf's binding functionality can automate
 * all this.
 **/

typedef struct
{
    XfconfChannel *channel;
    gchar *xfconf_property;
    GType xfconf_property_type;
    gulong channel_handler;

    GObject *object;
    gchar *object_property;
    GType object_property_type;
    gulong object_handler;
} XfconfGBinding;

/* same structure as in gdk, but we don't link to gdk */
typedef struct
{
    guint32 pixel;
    guint16 red;
    guint16 green;
    guint16 blue;
} FakeGdkColor;

/* same structure as in gdk, but we don't link to gdk */
typedef struct
{
    gdouble red;
    gdouble green;
    gdouble blue;
    gdouble alpha;
} FakeGdkRGBA;


static void xfconf_g_property_object_notify(GObject *object,
                                            GParamSpec *pspec,
                                            gpointer user_data);
static void xfconf_g_property_object_disconnect(gpointer user_data,
                                                GClosure *closure);
static void xfconf_g_property_channel_notify(XfconfChannel *channel,
                                             const gchar *property,
                                             const GValue *value,
                                             gpointer user_data);
static void xfconf_g_property_channel_disconnect(gpointer user_data,
                                                 GClosure *closure);



G_LOCK_DEFINE_STATIC(__bindings);
static GSList *__bindings = NULL;
static GType   __gdkcolor_gtype = 0;
static GType   __gdkrgba_gtype = 0;



static void
xfconf_g_property_object_notify_gdkcolor(XfconfGBinding *binding)
{
    FakeGdkColor *color = NULL;
    guint16 alpha = 0xffff;

    g_object_get(G_OBJECT(binding->object), binding->object_property, &color, NULL);
    if(G_UNLIKELY(!color)) {
        g_warning("Weird, returned GdkColor is NULL");
        return;
    }

    g_signal_handler_block(G_OBJECT(binding->channel), binding->channel_handler);
    xfconf_channel_set_array(binding->channel, binding->xfconf_property,
                             XFCONF_TYPE_UINT16, &color->red,
                             XFCONF_TYPE_UINT16, &color->green,
                             XFCONF_TYPE_UINT16, &color->blue,
                             XFCONF_TYPE_UINT16, &alpha,
                             G_TYPE_INVALID);
    g_signal_handler_unblock(G_OBJECT(binding->channel), binding->channel_handler);
}

static void
xfconf_g_property_object_notify_gdkrgba(XfconfGBinding *binding)
{
    FakeGdkRGBA *color = NULL;

    g_object_get(G_OBJECT(binding->object), binding->object_property, &color, NULL);
    if(G_UNLIKELY(!color)) {
        g_warning("Weird, returned GdkRGBA is NULL");
        return;
    }

    g_signal_handler_block(G_OBJECT(binding->channel), binding->channel_handler);
    xfconf_channel_set_array(binding->channel, binding->xfconf_property,
                             G_TYPE_DOUBLE, &color->red,
                             G_TYPE_DOUBLE, &color->green,
                             G_TYPE_DOUBLE, &color->blue,
                             G_TYPE_DOUBLE, &color->alpha,
                             G_TYPE_INVALID);
    g_signal_handler_unblock(G_OBJECT(binding->channel), binding->channel_handler);
}

static void
xfconf_g_property_object_notify(GObject *object,
                                GParamSpec *pspec,
                                gpointer user_data)
{
    XfconfGBinding *binding = user_data;
    GValue src_val = { 0, };
    GValue dst_val = { 0, };

    g_return_if_fail(G_IS_OBJECT(object));
    g_return_if_fail(binding->object == object);
    g_return_if_fail(XFCONF_IS_CHANNEL(binding->channel));

    if(G_PARAM_SPEC_VALUE_TYPE(pspec) == __gdkcolor_gtype) {
        /* we need to handle this in a different way */
        xfconf_g_property_object_notify_gdkcolor(binding);
        return;
    }

    if(G_PARAM_SPEC_VALUE_TYPE(pspec) == __gdkrgba_gtype) {
        /* we need to handle this in a different way */
        xfconf_g_property_object_notify_gdkrgba(binding);
        return;
    }

    /* this can do auto-conversion for us, but we can't easily tell if
     * the conversion worked */
    g_value_init(&src_val, G_PARAM_SPEC_VALUE_TYPE(pspec));
    g_object_get_property(object, g_param_spec_get_name(pspec), &src_val);

    g_value_init(&dst_val, binding->xfconf_property_type);
    if(g_value_transform(&src_val, &dst_val)) {
        g_signal_handler_block(G_OBJECT(binding->channel),
                               binding->channel_handler);
        xfconf_channel_set_property(binding->channel,
                                    binding->xfconf_property,
                                    &dst_val);
        g_signal_handler_unblock(G_OBJECT(binding->channel),
                                 binding->channel_handler);
    }

    g_value_unset(&dst_val);
    g_value_unset(&src_val);
}

static void
xfconf_g_property_object_disconnect(gpointer user_data,
                                    GClosure *closure)
{
    XfconfGBinding *binding = user_data;

    g_return_if_fail(G_IS_OBJECT(binding->object));
    g_return_if_fail(!binding->channel || XFCONF_IS_CHANNEL(binding->channel));

    /* remove the binding from the internal list */
    if(G_LIKELY(__bindings)) {
        G_LOCK(__bindings);
        __bindings = g_slist_remove(__bindings, binding);
        G_UNLOCK(__bindings);
    }

    /* unset the prevent recursing in channel_disconnect */
    binding->object = NULL;

    if(binding->channel) {
        g_signal_handler_disconnect(G_OBJECT(binding->channel),
                                    binding->channel_handler);
    } else {
        /* only release the binding if channel_disconnect() has run */
        g_free(binding->xfconf_property);
        g_free(binding->object_property);
        g_slice_free(XfconfGBinding, binding);
    }
}

static void
xfconf_g_property_channel_notify_gdkcolor(XfconfGBinding *binding,
                                          const GValue *value)
{
    GPtrArray *arr;
    FakeGdkColor color = { 0, };

    if(G_VALUE_TYPE(value) == G_TYPE_INVALID)
        return;

    arr = g_value_get_boxed(value);
    if(G_UNLIKELY(!arr || arr->len < 3))
        return;

    color.red = g_value_get_uint(g_ptr_array_index(arr, 0));
    color.green = g_value_get_uint(g_ptr_array_index(arr, 1));
    color.blue = g_value_get_uint(g_ptr_array_index(arr, 2));

    g_signal_handler_block(G_OBJECT(binding->object),
                           binding->object_handler);
    g_object_set(G_OBJECT(binding->object),
                 binding->object_property, &color, NULL);
    g_signal_handler_unblock(G_OBJECT(binding->object),
                             binding->object_handler);
}

static void
xfconf_g_property_channel_notify_gdkrgba(XfconfGBinding *binding,
                                         const GValue *value)
{
    GPtrArray *arr;
    FakeGdkRGBA color = { 0, };

    if(G_VALUE_TYPE(value) == G_TYPE_INVALID)
        return;

    arr = g_value_get_boxed(value);
    if(G_UNLIKELY(!arr || arr->len < 4))
        return;

    color.red = g_value_get_double(g_ptr_array_index(arr, 0));
    color.green = g_value_get_double(g_ptr_array_index(arr, 1));
    color.blue = g_value_get_double(g_ptr_array_index(arr, 2));
    color.alpha = g_value_get_double(g_ptr_array_index(arr, 3));

    g_signal_handler_block(G_OBJECT(binding->object),
                           binding->object_handler);
    g_object_set(G_OBJECT(binding->object),
                 binding->object_property, &color, NULL);
    g_signal_handler_unblock(G_OBJECT(binding->object),
                             binding->object_handler);
}

static void
xfconf_g_property_channel_notify(XfconfChannel *channel,
                                 const gchar *property,
                                 const GValue *value,
                                 gpointer user_data)
{
    XfconfGBinding *binding = user_data;
    GParamSpec *pspec;
    GValue dst_val = { 0, };

    g_return_if_fail(XFCONF_IS_CHANNEL(channel));
    g_return_if_fail(binding->channel == channel);
    g_return_if_fail(G_IS_OBJECT(binding->object));

   if(__gdkcolor_gtype == binding->xfconf_property_type) {
       /* we need to handle this in a different way */
        xfconf_g_property_channel_notify_gdkcolor(binding, value);
        return;
    }

    if(__gdkrgba_gtype == binding->xfconf_property_type) {
       /* we need to handle this in a different way */
        xfconf_g_property_channel_notify_gdkrgba(binding, value);
        return;
    }

    g_value_init(&dst_val, binding->object_property_type);

    if(G_VALUE_TYPE(value) == G_TYPE_INVALID) {
        /* try to reset to the object property to the default value.
         * boxed types don't have default, so bail if that's the case. */
        if(g_type_is_a(binding->object_property_type, G_TYPE_BOXED)) {
            g_value_unset(&dst_val);
            return;
        }

        pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(binding->object),
                                             binding->object_property);
        if(G_UNLIKELY(!pspec)) {
            g_warning("Unable to find property \"%s\" on object of type \"%s\".",
                      binding->object_property,
                      G_OBJECT_TYPE_NAME(binding->object));
            g_value_unset(&dst_val);
            return;
        }

        g_param_value_set_default(pspec, &dst_val);
    } else if(!g_value_transform(value, &dst_val)) {
        g_value_unset(&dst_val);
        g_warning("Unable to transform the value of property \"%s\" from type \"%s\" to \"%s\".",
                  binding->object_property,
                  G_VALUE_TYPE_NAME(value),
                  g_type_name(binding->object_property_type));
        return;
    }

    g_signal_handler_block(G_OBJECT(binding->object),
                           binding->object_handler);
    g_object_set_property(G_OBJECT(binding->object),
                          binding->object_property, &dst_val);
    g_signal_handler_unblock(G_OBJECT(binding->object),
                             binding->object_handler);

    g_value_unset(&dst_val);
}

static void
xfconf_g_property_channel_disconnect(gpointer user_data,
                                     GClosure *closure)
{
    XfconfGBinding *binding = user_data;

    g_return_if_fail(XFCONF_IS_CHANNEL(binding->channel));
    g_return_if_fail(!binding->object || G_IS_OBJECT(binding->object));

    /* unset the prevent recursing in object_disconnect */
    binding->channel = NULL;

    if(binding->object) {
        g_signal_handler_disconnect(G_OBJECT(binding->object),
                                    binding->object_handler);
    } else {
        /* only release the binding if object_disconnect() has run */
        g_free(binding->xfconf_property);
        g_free(binding->object_property);
        g_slice_free(XfconfGBinding, binding);
    }
}

static gulong
xfconf_g_property_init(XfconfChannel *channel,
                       const gchar *xfconf_property,
                       GType xfconf_property_type,
                       GObject *object,
                       const gchar *object_property,
                       GType object_property_type)
{
    XfconfGBinding *binding;
    gchar *detailed_signal;
    GValue value = { 0, };

    binding = g_slice_new(XfconfGBinding);
    binding->channel = channel;
    binding->xfconf_property_type = xfconf_property_type;
    binding->xfconf_property = g_strdup(xfconf_property);
    binding->object = object;
    binding->object_property = g_strdup(object_property);
    binding->object_property_type = object_property_type;

    /* monitor object for property changes */
    detailed_signal = g_strconcat("notify::", object_property, NULL);
    binding->object_handler = g_signal_connect_data(G_OBJECT(object),
                                                    detailed_signal,
                                                    G_CALLBACK(xfconf_g_property_object_notify),
                                                    binding,
                                                    xfconf_g_property_object_disconnect, 0);
    g_free(detailed_signal);

    /* transfer channel property to the object */
    if(xfconf_channel_get_property(channel, xfconf_property, &value)) {
        xfconf_g_property_channel_notify(channel, xfconf_property,
                                         &value, binding);
        g_value_unset(&value);
    }

    /* monitor channel for property changes */
    detailed_signal = g_strconcat("property-changed::", xfconf_property, NULL);
    binding->channel_handler = g_signal_connect_data(G_OBJECT(channel),
                                                     detailed_signal,
                                                     G_CALLBACK(xfconf_g_property_channel_notify),
                                                     binding,
                                                     xfconf_g_property_channel_disconnect, 0);
    g_free(detailed_signal);

    /* add binding to internal list */
    G_LOCK(__bindings);
    __bindings = g_slist_prepend(__bindings, binding);
    G_UNLOCK(__bindings);

    /* we use the channel signal id as binding id  */
    return binding->channel_handler;
}

void
_xfconf_g_bindings_shutdown(void)
{
    GSList *bindings, *l;
    guint n;
    XfconfGBinding *binding;

    if(G_UNLIKELY(__bindings)) {
        G_LOCK(__bindings);
        bindings = __bindings;

        /* don't remove bindings in object disconnect */
        __bindings = NULL;

        /* remove all the remaining bindings */
        for(l = bindings, n = 0; l; l = g_slist_next(l), n++) {
            binding = l->data;
            g_signal_handler_disconnect(G_OBJECT(binding->object),
                                        binding->object_handler);
        }
        g_slist_free(bindings);

#ifndef NDEBUG
        /* scare the developer a bit */
        g_debug("%d xfconf binding(s) are still connected. Are you sure all xfconf "
                "channels are released before calling xfconf_shutdown()?", n);
#endif

        G_UNLOCK(__bindings);
    }
}

/**
 * xfconf_g_property_bind:
 * @channel: An #XfconfChannel.
 * @xfconf_property: A property on @channel.
 * @xfconf_property_type: The type of @xfconf_property.
 * @object: A #GObject.
 * @object_property: A valid property on @object.
 *
 * Binds an Xfconf property to a #GObject property.  If the property
 * is changed via either the #GObject or Xfconf, the corresponding
 * property will also be updated. The binding is initialized from the
 * Xfconf property, i.e. the initial value of the #GObject property is
 * overwritten.
 *
 * Note that @xfconf_property_type is required since @xfconf_property
 * may or may not already exist in the Xfconf store.  The type of
 * @object_property will be determined automatically.  If the two
 * types do not match, a conversion will be attempted.
 *
 * Returns: an ID number that can be used to later remove the
 *          binding.
 **/
gulong
xfconf_g_property_bind(XfconfChannel *channel,
                       const gchar *xfconf_property,
                       GType xfconf_property_type,
                       gpointer object,
                       const gchar *object_property)
{
    GParamSpec *pspec;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), 0UL);
    g_return_val_if_fail(xfconf_property && *xfconf_property == '/', 0UL);
    g_return_val_if_fail(xfconf_property_type != G_TYPE_NONE, 0UL);
    g_return_val_if_fail(xfconf_property_type != G_TYPE_INVALID, 0UL);
    g_return_val_if_fail(G_IS_OBJECT(object), 0UL);
    g_return_val_if_fail(object_property && *object_property != '\0', 0UL);

    pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(object),
                                         object_property);
    if(G_UNLIKELY(!pspec)) {
        g_warning("Property \"%s\" is not valid for GObject type \"%s\"",
                  object_property, G_OBJECT_TYPE_NAME(object));
        return 0UL;
    }

    if(G_UNLIKELY(!g_value_type_transformable(xfconf_property_type,
                                              G_PARAM_SPEC_VALUE_TYPE(pspec))))
    {
        g_warning("Converting from type \"%s\" to type \"%s\" is not supported",
                  g_type_name(xfconf_property_type),
                  g_type_name(G_PARAM_SPEC_VALUE_TYPE(pspec)));
        return 0UL;
    }

    if(G_UNLIKELY(!g_value_type_transformable(G_PARAM_SPEC_VALUE_TYPE(pspec),
                                              xfconf_property_type)))
    {
        g_warning("Converting from type \"%s\" to type \"%s\" is not supported",
                  g_type_name(G_PARAM_SPEC_VALUE_TYPE(pspec)),
                  g_type_name(xfconf_property_type));
        return 0UL;
    }

    return xfconf_g_property_init(channel, xfconf_property,
                                  xfconf_property_type, G_OBJECT(object),
                                  object_property,
                                  G_PARAM_SPEC_VALUE_TYPE(pspec));
}

/**
 * xfconf_g_property_bind_gdkcolor:
 * @channel: An #XfconfChannel.
 * @xfconf_property: A property on @channel.
 * @object: A #GObject.
 * @object_property: A valid property on @object.
 *
 * Binds an Xfconf property to a #GObject property of type
 * GDK_TYPE_COLOR (aka a #GdkColor struct).  If the property
 * is changed via either the #GObject or Xfconf, the corresponding
 * property will also be updated. The binding is initialized from the
 * Xfconf property, i.e. the initial value of the #GObject property is
 * overwritten.
 *
 * This is a special-case binding; the GdkColor struct is not
 * ideal as-is for binding to a property, so it is stored in the
 * Xfconf store as four 16-bit unsigned ints (red, green, blue, alpha).
 * Since GdkColor (currently) only supports RGB and not RGBA,
 * the last value will always be set to 0xFFFF.
 *
 * Returns: an ID number that can be used to later remove the
 *          binding.
 *
 **/
gulong
xfconf_g_property_bind_gdkcolor(XfconfChannel *channel,
                                const gchar *xfconf_property,
                                gpointer object,
                                const gchar *object_property)
{
    GParamSpec *pspec;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), 0UL);
    g_return_val_if_fail(xfconf_property && *xfconf_property == '/', 0UL);
    g_return_val_if_fail(G_IS_OBJECT(object), 0UL);
    g_return_val_if_fail(object_property && *object_property != '\0', 0UL);

    if(!__gdkcolor_gtype) {
        __gdkcolor_gtype = g_type_from_name("GdkColor");
        if(G_UNLIKELY(__gdkcolor_gtype == 0)) {
            g_critical("Unable to look up GType for GdkColor: something is very wrong");
            return 0UL;
        }
    }

    pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(object),
                                         object_property);
    if(G_UNLIKELY(!pspec)) {
        g_warning("Property \"%s\" is not valid for GObject type \"%s\"",
                  object_property, G_OBJECT_TYPE_NAME(object));
        return 0UL;
    }

    if(G_UNLIKELY(G_PARAM_SPEC_VALUE_TYPE(pspec) != __gdkcolor_gtype)) {
        g_warning("Property \"%s\" for GObject type \"%s\" is not \"%s\", it's \"%s\"",
                  object_property, G_OBJECT_TYPE_NAME(object),
                  g_type_name(__gdkcolor_gtype),
                  g_type_name(G_PARAM_SPEC_VALUE_TYPE(pspec)));
        return 0UL;
    }

    return xfconf_g_property_init(channel, xfconf_property,
                                  __gdkcolor_gtype, G_OBJECT(object),
                                  object_property, __gdkcolor_gtype);
}

/**
 * xfconf_g_property_bind_gdkrgba:
 * @channel: An #XfconfChannel.
 * @xfconf_property: A property on @channel.
 * @object: A #GObject.
 * @object_property: A valid property on @object.
 *
 * Binds an Xfconf property to a #GObject property of type
 * GDK_TYPE_RGBA (aka a #GdkRGBA struct).  If the property
 * is changed via either the #GObject or Xfconf, the corresponding
 * property will also be updated. The binding is initialized from the
 * Xfconf property, i.e. the initial value of the #GObject property is
 * overwritten.
 *
 * This is a special-case binding; the GdkRGBA struct is not
 * ideal as-is for binding to a property, so it is stored in the
 * Xfconf store as four 16-bit unsigned ints (red, green, blue, alpha).
 *
 * Returns: an ID number that can be used to later remove the
 *          binding.
 *
 * Since: 4.12.1
 **/
gulong
xfconf_g_property_bind_gdkrgba(XfconfChannel *channel,
                               const gchar *xfconf_property,
                               gpointer object,
                               const gchar *object_property)
{
    GParamSpec *pspec;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), 0UL);
    g_return_val_if_fail(xfconf_property && *xfconf_property == '/', 0UL);
    g_return_val_if_fail(G_IS_OBJECT(object), 0UL);
    g_return_val_if_fail(object_property && *object_property != '\0', 0UL);

    if(!__gdkrgba_gtype) {
        __gdkrgba_gtype = g_type_from_name("GdkRGBA");
        if(G_UNLIKELY(__gdkrgba_gtype == 0)) {
            g_critical("Unable to look up GType for GdkRGBA: something is very wrong");
            return 0UL;
        }
    }

    pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(object),
                                         object_property);
    if(G_UNLIKELY(!pspec)) {
        g_warning("Property \"%s\" is not valid for GObject type \"%s\"",
                  object_property, G_OBJECT_TYPE_NAME(object));
        return 0UL;
    }

    if(G_UNLIKELY(G_PARAM_SPEC_VALUE_TYPE(pspec) != __gdkrgba_gtype)) {
        g_warning("Property \"%s\" for GObject type \"%s\" is not \"%s\", it's \"%s\"",
                  object_property, G_OBJECT_TYPE_NAME(object),
                  g_type_name(__gdkrgba_gtype),
                  g_type_name(G_PARAM_SPEC_VALUE_TYPE(pspec)));
        return 0UL;
    }

    return xfconf_g_property_init(channel, xfconf_property,
                                  __gdkrgba_gtype, G_OBJECT(object),
                                  object_property, __gdkrgba_gtype);
}

/**
 * xfconf_g_property_unbind:
 * @id: A binding ID number.
 *
 * Removes an Xfconf/GObject property binding based on the binding
 * ID number.  See xfconf_g_property_bind().
 **/
void
xfconf_g_property_unbind(gulong id)
{
    GSList *l;
    XfconfGBinding *binding;

    G_LOCK(__bindings);
    for(l = __bindings; l; l = g_slist_next(l)) {
        binding = l->data;
        if(G_UNLIKELY(binding->channel_handler == id))
            break;
    }
    G_UNLOCK(__bindings);

    if(G_LIKELY(l)) {
        binding = l->data;
        g_signal_handler_disconnect(G_OBJECT(binding->object),
                                    binding->object_handler);
    } else {
        g_warning("No binding with id %ld was found", id);
    }
}

/**
 * xfconf_g_property_unbind_by_property:
 * @channel: An #XfconfChannel.
 * @xfconf_property: A bound property on @channel.
 * @object: A #GObject.
 * @object_property: A bound property on @object.
 *
 * Causes an Xfconf channel previously bound to a #GObject property
 * (see xfconf_g_property_bind()) to no longer be bound.
 **/
void
xfconf_g_property_unbind_by_property(XfconfChannel *channel,
                                     const gchar *xfconf_property,
                                     gpointer object,
                                     const gchar *object_property)
{
    GSList *l;
    XfconfGBinding *binding;

    g_return_if_fail(XFCONF_IS_CHANNEL(channel));
    g_return_if_fail(xfconf_property && *xfconf_property == '/');
    g_return_if_fail(G_IS_OBJECT(object));
    g_return_if_fail(object_property && *object_property != '\0');

    G_LOCK(__bindings);
    for(l = __bindings; l; l = g_slist_next(l)) {
        binding = l->data;
        if(binding->object == object
           && binding->channel == channel
           && !strcmp(xfconf_property, binding->xfconf_property)
           && !strcmp(object_property, binding->object_property))
            break;
    }
    G_UNLOCK(__bindings);

    if(G_LIKELY(l)) {
        binding = l->data;
        g_signal_handler_disconnect(G_OBJECT(binding->object),
                                    binding->object_handler);
    } else {
        g_warning("No binding with the given properties was found");
    }
}

/**
 * xfconf_g_property_unbind_all:
 * @channel_or_object: A #GObject or #XfconfChannel.
 *
 * Unbinds all Xfconf channel bindings (see xfconf_g_property_bind())
 * to @object.  If @object is an #XfconfChannel, it will unbind all
 * xfconf properties on that channel.  If @object is a regular #GObject
 * with properties bound to a channel, all those bindings will be
 * removed.
 **/
void
xfconf_g_property_unbind_all(gpointer channel_or_object)
{
    guint n;

    g_return_if_fail(G_IS_OBJECT(channel_or_object));

    if(XFCONF_IS_CHANNEL(channel_or_object)) {
        n = g_signal_handlers_disconnect_matched(channel_or_object, G_SIGNAL_MATCH_FUNC,
                                                 0, 0, NULL,
                                                 xfconf_g_property_channel_notify,
                                                 NULL);
    } else {
        n = g_signal_handlers_disconnect_matched(channel_or_object, G_SIGNAL_MATCH_FUNC,
                                                 0, 0, NULL,
                                                 xfconf_g_property_object_notify,
                                                 NULL);
    }

    if(G_UNLIKELY(!n)) {
        g_warning("No bindings were found on the %s",
                  XFCONF_IS_CHANNEL(channel_or_object) ? "channel" : "object");
    }
}



#define __XFCONF_BINDING_C__
#include "common/xfconf-aliasdef.c"
