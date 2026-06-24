/*
 *  xfconf
 *
 *  Copyright (c) 2016 Ali Abdallah <ali@xfce.org>
 *  Copyright (c) 2007-2008 Brian Tarricone <bjt23@cornell.edu>
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

#include <libxfce4util/libxfce4util.h>

#include "xfconf-channel.h"
#include "xfconf-cache.h"
#include "common/xfconf-gdbus-bindings.h"
#include "common/xfconf-gvaluefuncs.h"
#include "xfconf-private.h"
#include "common/xfconf-marshal.h"
#include "xfconf-types.h"
#include "common/xfconf-common-private.h"
#include "xfconf.h"
#include "common/xfconf-alias.h"

#define IS_SINGLETON_DEFAULT  TRUE

#define ALIGN_VAL(val, align)  ( ((val) + ((align) -1)) & ~((align) - 1) )

#define REAL_PROP(channel, property) ( (channel)->property_base \
                                       ? g_strconcat ((channel)->property_base, \
                                                      (property), NULL) \
                                       : (gchar *)(property) )

/**
 * SECTION:xfconf-channel
 * @title: Xfconf Channel
 * @short_description: An application-defined domain for storing configuration settings
 *
 * An XfconfChannel is a representation of a restricted domain or
 * namespace that an application can define to store configuration
 * settings.  This is to ensure that different applications do not store
 * configuration keys with the same names.
 **/

/**
 * XfconfChannel:
 *
 * An opaque structure that holds state about a channel.
 **/
struct _XfconfChannel
{
    GObject parent;

    guint32 is_singleton:1,
            disposed:1;

    gchar *channel_name;
    gchar *property_base;

    XfconfCache *cache;
};

typedef struct _XfconfChannelClass
{
    GObjectClass parent;

    void (*property_changed)(XfconfChannel *channel,
                             const gchar *property,
                             const GValue *value);
} XfconfChannelClass;

enum
{
    SIG_PROPERTY_CHANGED = 0,
    N_SIGS,
};

enum
{
    PROP0 = 0,
    PROP_CHANNEL_NAME,
    PROP_PROPERTY_BASE,
    PROP_IS_SINGLETON,
};

static GObject *xfconf_channel_constructor(GType type,
                                           guint n_construct_properties,
                                           GObjectConstructParam *construct_properties);
static void xfconf_channel_set_g_property(GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void xfconf_channel_get_g_property(GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec);
static void xfconf_channel_dispose(GObject *obj);
static void xfconf_channel_finalize(GObject *obj);

static void xfconf_channel_property_changed(XfconfCache *cache,
                                            const gchar *channel_name,
                                            const gchar *property,
                                            const GValue *value,
                                            gpointer user_data);


G_LOCK_DEFINE_STATIC(__singletons);
static guint signals[N_SIGS] = { 0, };
static GHashTable *__channel_singletons = NULL;


G_DEFINE_TYPE(XfconfChannel, xfconf_channel, G_TYPE_OBJECT)


static void
xfconf_channel_class_init(XfconfChannelClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;

    object_class->constructor = xfconf_channel_constructor;
    object_class->set_property = xfconf_channel_set_g_property;
    object_class->get_property = xfconf_channel_get_g_property;
    object_class->dispose = xfconf_channel_dispose;
    object_class->finalize = xfconf_channel_finalize;

    /**
     * XfconfChannel::property-changed:
     * @channel: The #XfconfChannel emitting the signal.
     * @property: The property that changed.
     * @value: The new value.
     *
     * Emitted whenever a property on @channel has changed.  If
     * the change was caused by the removal of @property, @value
     * will be unset; you should test this with
     * <informalexample><programlisting>
     * G_VALUE_TYPE(value) == G_TYPE_INVALID
     * </programlisting></informalexample>
     **/
    signals[SIG_PROPERTY_CHANGED] = g_signal_new(I_("property-changed"),
                                                 XFCONF_TYPE_CHANNEL,
                                                 G_SIGNAL_RUN_LAST
                                                 | G_SIGNAL_DETAILED,
                                                 G_STRUCT_OFFSET(XfconfChannelClass,
                                                                 property_changed),
                                                 NULL,
                                                 NULL,
                                                 _xfconf_marshal_VOID__STRING_BOXED,
                                                 G_TYPE_NONE,
                                                 2, G_TYPE_STRING,
                                                 G_TYPE_VALUE);

    /**
     * XfconfChannel:channel-name:
     *
     * The string identifier used for this channel.
     **/
    g_object_class_install_property(object_class, PROP_CHANNEL_NAME,
                                    g_param_spec_string("channel-name",
                                                        "Channel Name",
                                                        "The name of the channel",
                                                        NULL,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME
                                                        | G_PARAM_STATIC_NICK
                                                        | G_PARAM_STATIC_BLURB));

    /**
     * XfconfChannel:property-base:
     *
     * The string identifier used for the property base inside a channel.
     * This can be used to restrict a channel to a subset of properties.
     **/
    g_object_class_install_property(object_class, PROP_PROPERTY_BASE,
                                    g_param_spec_string("property-base",
                                                        "Property Base",
                                                        "Base property path",
                                                        NULL,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME
                                                        | G_PARAM_STATIC_NICK
                                                        | G_PARAM_STATIC_BLURB));

    /**
     * XfconfChannel:is-singleton:
     *
     * Identifies the instance of the class as a singleton instance
     * or not.  This is mainly used internally by #XfconfChannel
     * but may be useful for API users.
     **/
    g_object_class_install_property(object_class, PROP_IS_SINGLETON,
                                    g_param_spec_boolean("is-singleton",
                                                         "Is Singleton",
                                                         "Whether or not this instance is a singleton",
                                                         IS_SINGLETON_DEFAULT,
                                                         G_PARAM_READWRITE
                                                         | G_PARAM_CONSTRUCT_ONLY
                                                         | G_PARAM_STATIC_NAME
                                                         | G_PARAM_STATIC_NICK
                                                         | G_PARAM_STATIC_BLURB));
}

static void
xfconf_channel_init(XfconfChannel *instance)
{
}

static GObject *
xfconf_channel_constructor(GType type,
                           guint n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
    const gchar *channel_name = NULL;
    gboolean is_singleton = IS_SINGLETON_DEFAULT;
    guint i;
    XfconfChannel *channel = NULL;

    for(i = 0; i < n_construct_properties; ++i) {
        if(!strcmp(g_param_spec_get_name(construct_properties[i].pspec), "channel-name"))
            channel_name = g_value_get_string(construct_properties[i].value);
        else if(!strcmp(g_param_spec_get_name(construct_properties[i].pspec), "is-singleton"))
            is_singleton = g_value_get_boolean(construct_properties[i].value);
    }

    if(G_UNLIKELY(!channel_name)) {
        g_warning("Assertion 'channel_name != NULL' failed");
        return NULL;
    }

    if(is_singleton) {
        G_LOCK(__singletons);

        if(!__channel_singletons) {
            __channel_singletons = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                         (GDestroyNotify)g_free,
                                                         (GDestroyNotify)g_object_unref);
        } else
            channel = g_hash_table_lookup(__channel_singletons, channel_name);

        if(!channel) {
            channel = XFCONF_CHANNEL(G_OBJECT_CLASS(xfconf_channel_parent_class)->constructor(type,
                                                                                              n_construct_properties,
                                                                                              construct_properties));
            g_hash_table_insert(__channel_singletons, g_strdup(channel_name),
                                channel);
        }

        G_UNLOCK(__singletons);
    } else {
        channel = XFCONF_CHANNEL(G_OBJECT_CLASS(xfconf_channel_parent_class)->constructor(type,
                                                                                          n_construct_properties,
                                                                                          construct_properties));
    }

    if(!channel->cache) {
        channel->cache = xfconf_cache_new(channel_name);
        xfconf_cache_prefetch(channel->cache, channel->property_base, NULL);
        g_signal_connect(channel->cache, "property-changed",
                         G_CALLBACK(xfconf_channel_property_changed), channel);
    }

    return G_OBJECT(channel);
}

static void
xfconf_channel_set_g_property(GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    XfconfChannel *channel = XFCONF_CHANNEL(object);

    switch(property_id) {
        case PROP_CHANNEL_NAME:
            g_assert(channel->channel_name == NULL);
            channel->channel_name = g_value_dup_string(value);
            break;

        case PROP_PROPERTY_BASE:
            g_assert(channel->property_base == NULL);
            channel->property_base = g_value_dup_string(value);
            break;

        case PROP_IS_SINGLETON:
            channel->is_singleton = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfconf_channel_get_g_property(GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    XfconfChannel *channel = XFCONF_CHANNEL(object);

    switch(property_id) {
        case PROP_CHANNEL_NAME:
            g_value_set_string(value, channel->channel_name);
            break;

        case PROP_PROPERTY_BASE:
            g_value_set_string(value, channel->property_base);
            break;

        case PROP_IS_SINGLETON:
            g_value_set_boolean(value, channel->is_singleton);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfconf_channel_dispose(GObject *obj)
{
    XfconfChannel *channel = XFCONF_CHANNEL(obj);

    if(!channel->disposed) {
        channel->disposed = TRUE;

        g_signal_handlers_disconnect_by_func(channel->cache,
                                             xfconf_channel_property_changed,
                                             channel);
        g_object_unref(G_OBJECT(channel->cache));
    }

    G_OBJECT_CLASS(xfconf_channel_parent_class)->dispose(obj);
}

static void
xfconf_channel_finalize(GObject *obj)
{
    XfconfChannel *channel = XFCONF_CHANNEL(obj);

    g_free(channel->channel_name);
    g_free(channel->property_base);

    /* no need to remove the channel from the hash table if it's a
     * singleton, since the hash table owns the channel's only reference */

    G_OBJECT_CLASS(xfconf_channel_parent_class)->finalize(obj);
}



void
_xfconf_channel_shutdown(void)
{
    G_LOCK(__singletons);
    if(G_LIKELY(__channel_singletons)) {
        g_hash_table_destroy(__channel_singletons);
        __channel_singletons = NULL;
    }
    G_UNLOCK(__singletons);
}


static void
xfconf_channel_property_changed(XfconfCache *cache,
                                const gchar *channel_name,
                                const gchar *property,
                                const GValue *value,
                                gpointer user_data)
{
    XfconfChannel *channel = XFCONF_CHANNEL(user_data);

    if(strcmp(channel_name, channel->channel_name)
       || (channel->property_base
           && !g_str_has_prefix(property, channel->property_base)))
    {
        return;
    }

    if(channel->property_base) {
        property += strlen(channel->property_base);
        if(!*property)
            property = "/";
    }

    g_signal_emit(G_OBJECT(channel), signals[SIG_PROPERTY_CHANGED],
                  g_quark_from_string(property), property, value);
}


static gboolean
xfconf_channel_set_internal(XfconfChannel *channel,
                            const gchar *property,
                            GValue *value)
{
    gboolean ret;
    gchar *real_property = REAL_PROP(channel, property);
    ERROR_DEFINE;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);

    ret = xfconf_cache_set(channel->cache, real_property, value, ERROR);
    if(!ret)
        ERROR_CHECK;

    if(real_property != property)
        g_free(real_property);

    return ret;
}

static gboolean
xfconf_channel_get_internal(XfconfChannel *channel,
                            const gchar *property,
                            GValue *value)
{
    GValue tmp_val = { 0, }, *val;
    gboolean ret;
    gchar *real_property = REAL_PROP(channel, property);
    ERROR_DEFINE;

    /* we support 2 ways of using this function:
     * 1.  |value| is unset, and we just take the property from xfconf
     *     and give it to the caller as-is, in the type xfconf gives
     * 2.  |value| is initialised, so we (try to) transform the value
     *     returned into the type the caller requested
     */

    if(G_VALUE_TYPE(value))
        val = &tmp_val;
    else
        val = value;

    ret = xfconf_cache_lookup(channel->cache, real_property, val, ERROR);
    if(!ret)
        ERROR_CHECK;

    if(ret && val == &tmp_val) {
        if(!g_value_transform(val, value)) {
            g_warning("Unable to transform value of type \"%s\" to type \"%s\" for property %s",
                      G_VALUE_TYPE_NAME(val), G_VALUE_TYPE_NAME(value),
                      real_property);
            ret = FALSE;
        }
        g_value_unset(val);
    }

    if(real_property != property)
        g_free(real_property);

    return ret;
}


static GPtrArray *
xfconf_transform_array(GPtrArray *arr_src,
                       GType gtype)
{
    GPtrArray *arr_dest;
    guint i;

    g_return_val_if_fail(arr_src && arr_src->len, NULL);
    g_return_val_if_fail(gtype != G_TYPE_INVALID, NULL);

    arr_dest = g_ptr_array_sized_new(arr_src->len);
    for(i = 0; i < arr_src->len; ++i) {
        GValue *value_src = g_ptr_array_index(arr_src, i);
        GValue *value_dest = g_new0(GValue, 1);

        g_value_init(value_dest, gtype);
        if(G_VALUE_TYPE(value_src) == gtype)
            g_value_copy(value_src, value_dest);
        else if(!g_value_transform(value_src, value_dest)) {
            g_warning("Unable to convert array member %d from type \"%s\" to type \"%s\"",
                      i, G_VALUE_TYPE_NAME(value_src), g_type_name(gtype));
            _xfconf_gvalue_free(value_dest);
            /* reuse i; we're returning anyway */
            for(i = 0; i < arr_dest->len; ++i) {
                g_value_unset(g_ptr_array_index(arr_dest, i));
                g_free(g_ptr_array_index(arr_dest, i));
            }
            g_ptr_array_free(arr_dest, TRUE);
            return NULL;
        }

        g_ptr_array_add(arr_dest, value_dest);
    }

    return arr_dest;
}



/**
 * xfconf_channel_get:
 * @channel_name: A channel name.
 *
 * Either creates a new channel, or fetches a singleton object for
 * @channel_name.  This function always returns a valid object; no
 * checking is done to see if the channel exists or has a valid name.
 *
 * The reference count of the returned channel is owned by libxfconf.
 *
 * Returns: (transfer none): An #XfconfChannel singleton.
 *
 * Since: 4.5.91
 */
XfconfChannel *
xfconf_channel_get(const gchar *channel_name)
{
    return g_object_new(XFCONF_TYPE_CHANNEL,
                        "channel-name", channel_name,
                        NULL);
}

/**
 * xfconf_channel_new:
 * @channel_name: A channel name.
 *
 * Creates a new channel using @name as the channel's identifier.
 * This function always returns a valid object; no checking is done
 * to see if the channel exists or has a valid name.
 *
 * Note: use of this function is not recommended, in favor of
 * xfconf_channel_get(), which returns a singleton object and
 * saves a little memory.  However, xfconf_channel_new() can be
 * useful in some cases where you want to tie an #XfconfChannel's
 * lifetime (and thus the lifetime of connected signals and bound
 * #GObject properties) to the lifetime of another object.
 *
 * Also note that each channel has its own cache, so if you create
 * 2 new channels with the same name, it will double the dbus traffic,
 * so in this cases it is highly recommended to use xfconf_channel_get().
 *
 * Returns: A new #XfconfChannel.  Release with g_object_unref()
 *          when no longer needed.
 **/
XfconfChannel *
xfconf_channel_new(const gchar *channel_name)
{
    return g_object_new(XFCONF_TYPE_CHANNEL,
                        "channel-name", channel_name,
                        "is-singleton", FALSE,
                        NULL);
}

/**
 * xfconf_channel_new_with_property_base:
 * @channel_name: A channel name.
 * @property_base: A property root name.
 *
 * Creates a new channel using @name as the channel's identifier,
 * restricting the accessible properties to be rooted at
 * @property_base.  This function always returns a valid object;
 * no checking is done to see if the channel exists or has a valid
 * name.
 *
 * Returns: A new #XfconfChannel.  Release with g_object_unref()
 *          when no longer needed.
 *
 * Since: 4.5.92
 **/
XfconfChannel *
xfconf_channel_new_with_property_base(const gchar *channel_name,
                                      const gchar *property_base)
{
    return g_object_new(XFCONF_TYPE_CHANNEL,
                        "channel-name", channel_name,
                        "property-base", property_base,
                        "is-singleton", FALSE,
                        NULL);
}

/**
 * xfconf_channel_has_property:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 *
 * Checks to see if @property exists on @channel.
 *
 * Returns: %TRUE if @property exists, %FALSE otherwise.
 **/
gboolean
xfconf_channel_has_property(XfconfChannel *channel,
                            const gchar *property)
{
    gboolean exists;
    gchar *real_property = REAL_PROP(channel, property);
    ERROR_DEFINE;

    exists = xfconf_cache_lookup(channel->cache, real_property, NULL, ERROR);
    if(!exists)
        ERROR_CHECK;

    if(real_property != property)
        g_free(real_property);

    return exists;
}

/**
 * xfconf_channel_is_property_locked:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 *
 * Queries whether or not @property on @channel is locked by system
 * policy.  If the property is locked, calls to
 * xfconf_channel_set_property() (or any of the "set" family of functions)
 * or xfconf_channel_reset_property() will fail.
 *
 * Returns: %TRUE if the property is locked, %FALSE otherwise.
 *
 * Since: 4.5.91
 **/
gboolean
xfconf_channel_is_property_locked(XfconfChannel *channel,
                                  const gchar *property)
{
    GDBusProxy *proxy = _xfconf_get_gdbus_proxy();
    gboolean locked = FALSE;
    gchar *real_property = REAL_PROP(channel, property);
    ERROR_DEFINE;
    
    if (!xfconf_exported_call_is_property_locked_sync ((XfconfExported*)proxy, channel->channel_name,
                                                       property, &locked, NULL, ERROR))
    {
        ERROR_CHECK;
        locked = FALSE;
    }

    if(real_property != property)
        g_free(real_property);

    return locked;
}

/**
 * xfconf_channel_reset_property:
 * @channel: An #XfconfChannel.
 * @property_base: A property tree root or property name.
 * @recursive: Whether to reset properties recursively.
 *
 * Resets properties starting at (and including) @property_base.
 * If @recursive is %TRUE, will also reset all properties that are
 * under @property_base in the property hierarchy.
 *
 * A bit of an explanation as to what this function actually does:
 * Since Xfconf backends are expected to support setting defaults
 * via what you might call "optional schema," you can't really
 * "remove" properties.  Since the client library can't know if a
 * channel provides default values (or even if the backend supports
 * it!), at best it can only reset properties to their default values.
 *
 * The @property_base parameter can be %NULL or the empty string (""),
 * in which case the channel root ("/") will be assumed.  Of course,
 * %TRUE must be passed for @recursive in this case.
 *
 * Since: 4.5.91
 **/
void
xfconf_channel_reset_property(XfconfChannel *channel,
                              const gchar *property_base,
                              gboolean recursive)
{
    gchar *real_property_base = REAL_PROP(channel, property_base);
    ERROR_DEFINE;

    g_return_if_fail(XFCONF_IS_CHANNEL(channel) &&
                     ((property_base && property_base[0] && property_base[1])
                      || recursive));

    if(!xfconf_cache_reset(channel->cache, real_property_base, recursive, ERROR))
        ERROR_CHECK;

    if(real_property_base != property_base)
        g_free(real_property_base);
}

/**
 * xfconf_channel_get_properties:
 * @channel: An #XfconfChannel.
 * @property_base: The base property name of properties to retrieve.
 *
 * Retrieves multiple properties from @channel and stores
 * them in a #GHashTable in which the keys correspond to
 * the string (gchar *) property names, and the values
 * correspond to variant (GValue *) values.  The keys and
 * values are owned by the hash table and should be copied
 * if needed.  The value of the property specified by
 * @property_base (if it exists) and all sub-properties are
 * retrieved.  To retrieve all properties in the channel,
 * specify "/" or %NULL for @property_base.
 *
 * Returns: (element-type utf8 GValue) (transfer container): A newly-allocated #GHashTable, which should be freed with
 *          g_hash_table_destroy() when no longer needed.
 */
GHashTable *
xfconf_channel_get_properties(XfconfChannel *channel,
                              const gchar *property_base)
{
    GDBusProxy *proxy = _xfconf_get_gdbus_proxy ();
    GHashTable *properties = NULL;
    GVariant *variant;
    gchar *real_property_base;
    ERROR_DEFINE;

    if(!property_base || (property_base[0] == '/' && !property_base[1]))
        real_property_base = channel->property_base;
    else
        real_property_base = REAL_PROP(channel, property_base);

    if(!xfconf_exported_call_get_all_properties_sync ((XfconfExported*)proxy, channel->channel_name,
                                                      real_property_base
                                                      ? real_property_base : "/",
                                                      &variant, NULL, ERROR))
    {
        ERROR_CHECK;
        variant = NULL;
    }
   
    if (variant) {
        properties = xfconf_gvariant_to_hash (variant);
        g_variant_unref (variant);
    }
        
    if(real_property_base != property_base
       && real_property_base != channel->property_base)
    {
        g_free(real_property_base);
    }

    return properties;
}

/**
 * xfconf_channel_get_string:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @default_value: A fallback value.
 *
 * Retrieves the string value associated with @property on @channel.
 *
 * Returns: A newly-allocated string which should be freed with g_free()
 *          when no longer needed.  If @property is not in @channel,
 *          a g_strdup()ed copy of @default_value is returned.
 **/
gchar *
xfconf_channel_get_string(XfconfChannel *channel,
                          const gchar *property,
                          const gchar *default_value)
{
    gchar *value = NULL;
    GValue val = { 0, };

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, NULL);

    if(xfconf_channel_get_internal(channel, property, &val)) {
        if(G_VALUE_TYPE(&val) == G_TYPE_STRING)
            value = g_value_dup_string(&val);
        g_value_unset(&val);
    }

    if(!value)
        value = g_strdup(default_value);

    return value;
}

/**
 * xfconf_channel_get_string_list:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 *
 * Retrieves the string list value associated with @property on @channel.
 *
 * Returns: (transfer full) (element-type utf8) (array zero-terminated=1): A newly-allocated string list which should be freed with
 *          g_strfreev() when no longer needed.  If @property is not in
 *          @channel, %NULL is returned.
 */
gchar **
xfconf_channel_get_string_list(XfconfChannel *channel,
                               const gchar *property)
{
    gchar **values = NULL;
    GPtrArray *arr;
    guint i;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, NULL);

    arr = xfconf_channel_get_arrayv(channel, property);
    if(!arr)
        return NULL;

    values = g_new0(gchar *, arr->len + 1);
    for(i = 0; i < arr->len; ++i) {
        GValue *val = g_ptr_array_index(arr, i);

        if(G_VALUE_TYPE(val) != G_TYPE_STRING) {
            xfconf_array_free(arr);
            g_strfreev(values);
            return NULL;
        }

        values[i] = g_value_dup_string(val);  /* FIXME: avoid copy */
    }

    xfconf_array_free(arr);

    return values;
}

/**
 * xfconf_channel_get_int:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @default_value: A fallback value.
 *
 * Retrieves the int value associated with @property on @channel.
 *
 * Returns: The int value, or, if @property is not in @channel,
 *          @default_value is returned.
 **/
gint
xfconf_channel_get_int(XfconfChannel *channel,
                       const gchar *property,
                       gint default_value)
{
    gint value = default_value;
    GValue val = { 0, };

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, value);

    if(xfconf_channel_get_internal(channel, property, &val)) {
        if(G_VALUE_TYPE(&val) == G_TYPE_INT)
            value = g_value_get_int(&val);
        g_value_unset(&val);
    }

    return value;
}

/**
 * xfconf_channel_get_uint:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @default_value: A fallback value.
 *
 * Retrieves the unsigned int value associated with @property on @channel.
 *
 * Returns: The uint value, or, if @property is not in @channel,
 *          @default_value is returned.
 **/
guint32
xfconf_channel_get_uint(XfconfChannel *channel,
                        const gchar *property,
                        guint32 default_value)
{
    gint value = default_value;
    GValue val = { 0, };

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, value);

    if(xfconf_channel_get_internal(channel, property, &val)) {
        if(G_VALUE_TYPE(&val) == G_TYPE_UINT)
            value = g_value_get_uint(&val);
        g_value_unset(&val);
    }

    return value;
}

/**
 * xfconf_channel_get_uint64:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @default_value: A fallback value.
 *
 * Retrieves the 64-bit int value associated with @property on @channel.
 *
 * Returns: The uint64 value, or, if @property is not in @channel,
 *          @default_value is returned.
 **/
guint64
xfconf_channel_get_uint64(XfconfChannel *channel,
                          const gchar *property,
                          guint64 default_value)
{
    gint64 value = default_value;
    GValue val = { 0, };

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, value);

    if(xfconf_channel_get_internal(channel, property, &val)) {
        if(G_VALUE_TYPE(&val) == G_TYPE_UINT64)
            value = g_value_get_uint64(&val);
        g_value_unset(&val);
    }

    return value;
}

/**
 * xfconf_channel_get_double:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @default_value: A fallback value.
 *
 * Retrieves the double value associated with @property on @channel.
 *
 * Returns: The double value, or, if @property is not in @channel,
 *          @default_value is returned.
 **/
gdouble
xfconf_channel_get_double(XfconfChannel *channel,
                          const gchar *property,
                          gdouble default_value)
{
    gdouble value = default_value;
    GValue val = { 0, };

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, value);

    if(xfconf_channel_get_internal(channel, property, &val)) {
        if(G_VALUE_TYPE(&val) == G_TYPE_DOUBLE)
            value = g_value_get_double(&val);
        g_value_unset(&val);
    }

    return value;
}

/**
 * xfconf_channel_get_bool:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @default_value: A fallback value.
 *
 * Retrieves the boolean value associated with @property on @channel.
 *
 * Returns: The boolean value, or, if @property is not in @channel,
 *          @default_value is returned.
 **/
gboolean
xfconf_channel_get_bool(XfconfChannel *channel,
                        const gchar *property,
                        gboolean default_value)
{
    gboolean value = default_value;
    GValue val = { 0, };

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, value);

    if(xfconf_channel_get_internal(channel, property, &val)) {
        if(G_VALUE_TYPE(&val) == G_TYPE_BOOLEAN)
            value = g_value_get_boolean(&val);
        g_value_unset(&val);
    }

    return value;
}

/**
 * xfconf_channel_set_string:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @value: The value to set.
 *
 * Sets @value for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_string(XfconfChannel *channel,
                          const gchar *property,
                          const gchar *value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);
    g_return_val_if_fail(value == NULL || g_utf8_validate(value, -1, NULL), FALSE);

    g_value_init(&val, G_TYPE_STRING);
    g_value_set_static_string(&val, value);

    ret = xfconf_channel_set_internal(channel, property, &val);

    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_set_string_list:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @values: The value to set.
 *
 * Sets @values for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_string_list(XfconfChannel *channel,
                               const gchar *property,
                               const gchar * const *values)
{
    GPtrArray *arr;
    GValue *val;
    gint i;
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && values
                         && values[0], FALSE);

    /* count strings so we can prealloc */
    for(i = 0; values[i]; ++i)
        (void)0;
    
    arr = g_ptr_array_sized_new(i);
    for(i = 0; values[i]; ++i) {
        val = g_new0(GValue, 1);
        g_value_init(val, G_TYPE_STRING);
        g_value_set_static_string(val, values[i]);
        g_ptr_array_add(arr, val);
    }

    ret = xfconf_channel_set_arrayv(channel, property, arr);

    xfconf_array_free(arr);

    return ret;
}

/**
 * xfconf_channel_set_int:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @value: The value to set.
 *
 * Sets @value for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_int(XfconfChannel *channel,
                       const gchar *property,
                       gint value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);

    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, value);

    ret = xfconf_channel_set_internal(channel, property, &val);

    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_set_uint:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @value: The value to set.
 *
 * Sets @value for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_uint(XfconfChannel *channel,
                        const gchar *property,
                        guint32 value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);

    g_value_init(&val, G_TYPE_UINT);
    g_value_set_uint(&val, value);

    ret = xfconf_channel_set_internal(channel, property, &val);

    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_set_uint64:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @value: The value to set.
 *
 * Sets @value for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_uint64(XfconfChannel *channel,
                          const gchar *property,
                          guint64 value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);

    g_value_init(&val, G_TYPE_UINT64);
    g_value_set_uint64(&val, value);

    ret = xfconf_channel_set_internal(channel, property, &val);

    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_set_double:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @value: The value to set.
 *
 * Sets @value for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_double(XfconfChannel *channel,
                          const gchar *property,
                          gdouble value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);

    g_value_init(&val, G_TYPE_DOUBLE);
    g_value_set_double(&val, value);

    ret = xfconf_channel_set_internal(channel, property, &val);

    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_set_bool:
 * @channel: An #XfconfChannel.
 * @property: A property name.
 * @value: The value to set.
 *
 * Sets @value for @property on @channel in the configuration store.
 *
 * Returns: %TRUE on success, %FALSE if an error occured.
 **/
gboolean
xfconf_channel_set_bool(XfconfChannel *channel,
                        const gchar *property,
                        gboolean value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, FALSE);

    g_value_init(&val, G_TYPE_BOOLEAN);
    g_value_set_boolean(&val, value);

    ret = xfconf_channel_set_internal(channel, property, &val);

    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_get_property:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value: A #GValue.
 *
 * Gets a property on @channel and stores it in @value.  The caller is
 * responsible for calling g_value_unset() when finished with @value.
 *
 * This function can be called with an initialized or uninitialized
 * @value.  If @value is initialized to a particular type, libxfconf
 * will attempt to convert the value returned from the configuration
 * store to that type if they don't match.  If the value type returned
 * from the configuration store is an array type, each element of the
 * array will be converted to the type of @value.  If @value is
 * uninitialized, the value in the configuration store will be returned
 * in its native type.
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE otherwise.
 **/
gboolean
xfconf_channel_get_property(XfconfChannel *channel,
                            const gchar *property,
                            GValue *value)
{
    GValue val1 = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && value,
                         FALSE);

    ret = xfconf_channel_get_internal(channel, property, &val1);

    if(ret) {
        if(G_VALUE_TYPE(value) != G_TYPE_INVALID
           && G_VALUE_TYPE(value) != G_VALUE_TYPE(&val1))
        {
            /* caller wants to convert the returned value into a diff type */

            if(G_VALUE_TYPE(&val1) == G_TYPE_PTR_ARRAY) {
                /* we got an array back, so let's convert each item in
                 * the array to the target type */
                GPtrArray *arr = xfconf_transform_array(g_value_get_boxed(&val1),
                                                        G_VALUE_TYPE(value));

                if(arr) {
                    g_value_unset(value);
                    g_value_init(value, G_TYPE_PTR_ARRAY);
                    g_value_take_boxed(value, arr);
                } else
                    ret = FALSE;
            } else {
                ret = g_value_transform(&val1, value);
                if(!ret) {
                    g_warning("Unable to convert property \"%s\" from type \"%s\" to type \"%s\"",
                              property, G_VALUE_TYPE_NAME(&val1),
                              G_VALUE_TYPE_NAME(value));
                }
            }
        } else {
            /* either the caller wants the native type, or specified the
             * native type to convert to */
            if(G_VALUE_TYPE(value) == G_VALUE_TYPE(&val1))
                g_value_unset(value);
            g_value_copy(&val1, g_value_init(value, G_VALUE_TYPE(&val1)));
            ret = TRUE;
        }
    }

    if(G_VALUE_TYPE(&val1))
        g_value_unset(&val1);

    return ret;
}

/**
 * xfconf_channel_set_property:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value: A #GValue.
 *
 * Sets the value stored in @value  to a property on @channel.
 *
 * Note: The configuration store backend almost certainly supports
 * only a restricted set of value types.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 **/
gboolean
xfconf_channel_set_property(XfconfChannel *channel,
                            const gchar *property,
                            const GValue *value)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel)
                         && property
                         && G_IS_VALUE(value), FALSE);
    g_return_val_if_fail(!G_VALUE_HOLDS_STRING(value)
                         || g_value_get_string(value) == NULL
                         || g_utf8_validate(g_value_get_string(value), -1, NULL),
                         FALSE);
    
    g_value_init(&val, G_VALUE_TYPE(value));
    g_value_copy(value, &val);
    ret = xfconf_channel_set_internal(channel, property, &val);
    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_get_array:
 * @channel: An #XfconfChannel.
 * @property: A property string.
 * @first_value_type: The type of the first argument in the array.
 * @...: A variable argument list of types and values.
 *
 * Gets an array property on @channel.  The @first_value_type
 * argument specifies the type of the first value in the variable
 * argument list.  The variable argument list should alternate between
 * pointers to locations to store the values, and the GType of the
 * next value.  The argument list should be terminated with
 * G_TYPE_INVALID.
 *
 * Note: The configuration store backend almost certainly supports
 * only a restricted set of value types.
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE otherwise.
 **/
gboolean
xfconf_channel_get_array(XfconfChannel *channel,
                         const gchar *property,
                         GType first_value_type,
                         ...)
{
    va_list var_args;
    gboolean ret;

    va_start(var_args, first_value_type);
    ret = xfconf_channel_get_array_valist(channel, property, first_value_type,
                                          var_args);
    va_end(var_args);

    return ret;
}

/**
 * xfconf_channel_get_array_valist:
 * @channel: An #XfconfChannel.
 * @property: A property string.
 * @first_value_type: The type of the first argument in the array.
 * @var_args: A variable argument list of types and values.
 *
 * Gets an array property on @channel.  See xfconf_channel_get_array()
 * for details.
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE otherwise.
 **/
gboolean
xfconf_channel_get_array_valist(XfconfChannel *channel,
                                const gchar *property,
                                GType first_value_type,
                                va_list var_args)
{
    gboolean ret = FALSE;
    GPtrArray *arr = NULL;
    GType cur_value_type;
    GValue *val;
    guint i;

    arr = xfconf_channel_get_arrayv(channel, property);
    if(!arr)
        return FALSE;

    for(cur_value_type = first_value_type, i = 0;
        cur_value_type != G_TYPE_INVALID;
        cur_value_type = va_arg(var_args, GType), ++i)
    {
        if(i > arr->len - 1) {
#ifdef XFCONF_ENABLE_CHECKS
            g_warning("Too many parameters passed, or config store doesn't " \
                      "have enough elements in array (it only provided %d).",
                       arr->len);
#endif
            goto out;
        }

        val = g_ptr_array_index(arr, i);

        /* special case: uint16/int16 are stored as uint/int */
        if(G_VALUE_TYPE(val) != cur_value_type
           && !((G_VALUE_TYPE(val) == G_TYPE_UINT && cur_value_type == XFCONF_TYPE_UINT16)
                || (G_VALUE_TYPE(val) == G_TYPE_INT && cur_value_type == XFCONF_TYPE_INT16)))
        {
#ifdef XFCONF_ENABLE_CHECKS
            g_warning("Value types don't match (%d != %d) at parameter %d",
                       (int)G_VALUE_TYPE(val), (int)cur_value_type, i);
#endif
            goto out;
        }

#define HANDLE_CASE(ctype, GTYPE, valtype) \
    case G_TYPE_ ## GTYPE: { \
        ctype *__val_p = va_arg(var_args, ctype *); \
        *__val_p = g_value_get_ ## valtype(val); \
        break; \
    }

        switch(cur_value_type) {
            HANDLE_CASE(guchar, UCHAR, uchar)
            HANDLE_CASE(gchar, CHAR, schar)
            HANDLE_CASE(guint32, UINT, uint)
            HANDLE_CASE(gint32, INT, int)
            HANDLE_CASE(guint64, UINT64, uint64)
            HANDLE_CASE(gint64, INT64, int64)
            HANDLE_CASE(gfloat, FLOAT, float)
            HANDLE_CASE(gdouble, DOUBLE, double)
            HANDLE_CASE(gboolean, BOOLEAN, boolean)
#undef HANDLE_CASE

            case G_TYPE_STRING: {
                gchar **__val_p = va_arg(var_args, gchar **);
                *__val_p = g_value_dup_string(val);
                break;
            }

            default:
                if(XFCONF_TYPE_UINT16 == cur_value_type) {
                    /* uint16 is stored as uint */
                    guint16 *__val_p = va_arg(var_args, guint16 *);
                    *__val_p = (guint16)g_value_get_uint(val);
                } else if(XFCONF_TYPE_INT16 == cur_value_type) {
                    /* int16 is stored as int */
                    gint16 *__val_p = va_arg(var_args, gint16 *);
                    *__val_p = (gint16)g_value_get_int(val);
                } else if(G_TYPE_STRV == cur_value_type) {
                    gchar ***__val_p = va_arg(var_args, gchar ***);
                    *__val_p = g_value_dup_boxed(val);
                } else {
                    g_warning("Unknown value type %d (%s) in value array.",
                              (gint)G_VALUE_TYPE(val), G_VALUE_TYPE_NAME(val));
                    goto out;
                }
                break;
        }
    }

    if(i < arr->len) {
#ifdef XFCONF_ENABLE_CHECKS
        g_warning("Too few parameters passed, or config store has too " \
                  "many elements in array (it provided %d).",
                   arr->len);
#endif
        goto out;
    }

    ret = TRUE;

out:
    xfconf_array_free(arr);

    return ret;
}

/**
 * xfconf_channel_get_arrayv:
 * @channel: An #XfconfChannel.
 * @property: A property string.
 *
 * Gets an array property on @channel and returns it as
 * a #GPtrArray, which can be freed with xfconf_array_free()
 * when no longer needed.
 *
 * Returns: (transfer container) (element-type GValue) (nullable): A newly-allocated #GPtrArray on success,
 * or %NULL on failure.
 **/
GPtrArray *
xfconf_channel_get_arrayv(XfconfChannel *channel,
                          const gchar *property)
{
    GValue val = { 0, };
    GPtrArray *arr = NULL;
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property, NULL);
    
    ret = xfconf_channel_get_internal(channel, property, &val);
    
    if(!ret)
        return NULL;
    
    if(G_TYPE_PTR_ARRAY != G_VALUE_TYPE(&val)) {
        g_warning ("Unexpected value type %s\n", G_VALUE_TYPE_NAME(&val));
        g_value_unset(&val);
        return NULL;
    }
    
    /* Do not free it, it is owned by the GValue in cache */
    arr = g_value_get_boxed(&val);
    if(!arr->len) {
        g_ptr_array_free(arr, TRUE);
        return NULL;
    }
    return arr;
}

/**
 * xfconf_channel_set_array:
 * @channel: An #XfconfChannel.
 * @property: A property string.
 * @first_value_type: The type of the first argument in the array.
 * @...: A variable argument list of types and values.
 *
 * Sets an array property on @channel.  The @first_value_type
 * argument specifies the type of the first value in the variable
 * argument list.  Note that all values specified MUST be pointers
 * to variables holding the correct value, and may not be, e.g.,
 * numeric constants.  The argument list should be terminated with
 * G_TYPE_INVALID.
 *
 * Note: The configuration store backend almost certainly supports
 * only a restricted set of value types.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 **/
gboolean
xfconf_channel_set_array(XfconfChannel *channel,
                         const gchar *property,
                         GType first_value_type,
                         ...)
{
    gboolean ret;
    va_list var_args;

    va_start(var_args, first_value_type);
    ret = xfconf_channel_set_array_valist(channel, property, first_value_type,
                                          var_args);
    va_end(var_args);

    return ret;
}

/**
 * xfconf_channel_set_array_valist:
 * @channel: An #XfconfChannel.
 * @property: A property string.
 * @first_value_type: The type of the first argument in the array.
 * @var_args: A variable argument list of types and values.
 *
 * Sets an array property on @channel.  See xfconf_channel_set_array()
 * for details.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 **/
gboolean
xfconf_channel_set_array_valist(XfconfChannel *channel,
                                const gchar *property,
                                GType first_value_type,
                                va_list var_args)
{
    GPtrArray *arr;
    GType cur_value_type;
    GValue *val;
    gboolean ret = FALSE;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property
                         && G_TYPE_INVALID != first_value_type, FALSE);

    arr = g_ptr_array_sized_new(3);  /* this is somewhat arbitrary... */

    for(cur_value_type = first_value_type;
        cur_value_type != G_TYPE_INVALID;
        cur_value_type = va_arg(var_args, GType))
    {
#define HANDLE_CASE(ctype, GTYPE, valtype) \
    case G_TYPE_ ## GTYPE: { \
        ctype *__val = va_arg(var_args, ctype *); \
        val = g_new0(GValue, 1); \
        g_value_init(val, G_TYPE_ ## GTYPE); \
        g_value_set_ ## valtype(val, *__val); \
        g_ptr_array_add(arr, val); \
        break; \
    }

        switch(cur_value_type) {
            HANDLE_CASE(guchar, UCHAR, uchar)
            HANDLE_CASE(gchar, CHAR, schar)
            HANDLE_CASE(guint32, UINT, uint)
            HANDLE_CASE(gint32, INT, int)
            HANDLE_CASE(guint64, UINT64, uint64)
            HANDLE_CASE(gint64, INT64, int64)
            HANDLE_CASE(gfloat, FLOAT, float)
            HANDLE_CASE(gdouble, DOUBLE, double)
            HANDLE_CASE(gboolean, BOOLEAN, boolean)
#undef HANDLE_CASE
            
            case G_TYPE_STRING: {
                gchar *__val = va_arg(var_args, gchar *);
                val = g_new0(GValue, 1);
                g_value_init(val, G_TYPE_STRING);
                g_value_set_static_string(val, __val);
                g_ptr_array_add(arr, val);
                break;
            }

            default:
                if(XFCONF_TYPE_UINT16 == cur_value_type) {
                    /* uint16 is stored as uint */
                    guint16 *__val = va_arg(var_args, guint16 *);
                    val = g_new0(GValue, 1);
                    g_value_init(val, G_TYPE_UINT);
                    g_value_set_uint(val, (guint)*__val);
                    g_ptr_array_add(arr, val);
                } else if(XFCONF_TYPE_INT16 == cur_value_type) {
                    /* int16 is stored as int */
                    gint16 *__val = va_arg(var_args, gint16 *);
                    val = g_new0(GValue, 1);
                    g_value_init(val, G_TYPE_INT);
                    g_value_set_int(val, (gint)*__val);
                    g_ptr_array_add(arr, val);
                } else if(G_TYPE_STRV == cur_value_type) {
                    gchar **__val = va_arg(var_args, gchar **);
                    val = g_new0(GValue, 1);
                    g_value_init(val, G_TYPE_STRV);
                    g_value_set_static_boxed(val, __val);
                    g_ptr_array_add(arr, val);
                } else {
                    g_warning("Unknown value type %d (%s) in parameter list.",
                              (gint)cur_value_type, g_type_name(cur_value_type));
                    goto out;
                }
                break;
        }
    }

    ret = xfconf_channel_set_arrayv(channel, property, arr);

out:
    xfconf_array_free(arr);

    return ret;
}

/**
 * xfconf_channel_set_arrayv:
 * @channel: An #XfconfChannel.
 * @property: A property string.
 * @values: (element-type GValue): A #GPtrArray of #GValue<!-- -->s.
 *
 * Sets an array property on @channel, using the values in the
 * provided @values array.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 **/
gboolean
xfconf_channel_set_arrayv(XfconfChannel *channel,
                          const gchar *property,
                          GPtrArray *values)
{
    GValue val = { 0, };
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && values,
                         FALSE);

    g_value_init(&val, G_TYPE_PTR_ARRAY);
    g_value_set_static_boxed(&val, values);
    
    ret = xfconf_channel_set_internal(channel, property, &val);
    
    g_value_unset(&val);

    return ret;
}

/**
 * xfconf_channel_get_named_struct:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @struct_name: A string struct name.
 * @value_struct: A pointer to a struct.
 *
 * Gets a property from @channel and fills in @value_struct using
 * the retrieved values.  The @struct_name parameter is the same
 * name that must have been used to register the struct's layout
 * with xfconf_named_struct_register().
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE otherwise.
 **/
gboolean
xfconf_channel_get_named_struct(XfconfChannel *channel,
                                const gchar *property,
                                const gchar *struct_name,
                                gpointer value_struct)
{
    XfconfNamedStruct *ns = _xfconf_named_struct_lookup(struct_name);

    if(!ns)
        return FALSE;

    return xfconf_channel_get_structv(channel, property, value_struct,
                                      ns->n_members, ns->member_types);
}

/**
 * xfconf_channel_set_named_struct:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @struct_name: A string struct name.
 * @value_struct: A pointer to a struct.
 *
 * Sets a property on @channel using the members of @value_struct
 * as the array of values.  The @struct_name parameter is the same
 * name that must have been used to register the struct's layout
 * with xfconf_named_struct_register().
 *
 * Returns: %TRUE if the property was set successfully,
 *          %FALSE otherwise.
 **/
gboolean
xfconf_channel_set_named_struct(XfconfChannel *channel,
                                const gchar *property,
                                const gchar *struct_name,
                                gpointer value_struct)
{
    XfconfNamedStruct *ns = _xfconf_named_struct_lookup(struct_name);

    if(!ns)
        return FALSE;

    return xfconf_channel_set_structv(channel, property, value_struct,
                                      ns->n_members, ns->member_types);
}


/**
 * xfconf_channel_get_struct:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value_struct: A pointer to a struct in which to store values.
 * @first_member_type: The GType of the first member of @value_struct.
 * @...: A variable argument list of #GType<!-- -->s.
 *
 * Gets a property on @channel and stores it as members of the
 * @value_struct struct.  The @first_member_type argument
 * specifies the #GType of the first member of the struct.  The
 * variable argument list specifies the #GType<!-- -->s of the
 * rest of the struct members, and should be terminated with
 * G_TYPE_INVALID.
 *
 * Note: This function takes your compiler's and platform's
 * struct member alignment rules into account when storing values
 * in @value_struct.  Therefore, it cannot be used with structs that
 * are declared as "packed" in such a way that the alignment rules
 * are ignored by the compiler.
 *
 * Note: Struct members can only be non-pointer types such as int,
 * boolean, double, etc.
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE oherwise.
 **/
gboolean
xfconf_channel_get_struct(XfconfChannel *channel,
                          const gchar *property,
                          gpointer value_struct,
                          GType first_member_type,
                          ...)
{
    gboolean ret;
    va_list var_args;

    va_start(var_args, first_member_type);
    ret = xfconf_channel_get_struct_valist(channel, property, value_struct,
                                           first_member_type, var_args);
    va_end(var_args);

    return ret;
}

/**
 * xfconf_channel_get_struct_valist:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value_struct: A pointer to a struct in which to store values.
 * @first_member_type: The GType of the first member of @value_struct.
 * @var_args: A variable argument list of #GType<!-- -->s.
 *
 * Gets a property on @channel and stores it as members of the
 * @value_struct struct.  See xfconf_channel_get_struct() for details.
 *
 * Note: Struct members can only be non-pointer types such as int,
 * boolean, double, etc.
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE oherwise.
 **/
gboolean
xfconf_channel_get_struct_valist(XfconfChannel *channel,
                                 const gchar *property,
                                 gpointer value_struct,
                                 GType first_member_type,
                                 va_list var_args)
{
    GType cur_member_type;
    GType *member_types;
    guint n_members;
    gsize cur_size = 5;  /* FIXME: arbitrary... */
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && value_struct
                         && G_TYPE_INVALID != first_member_type, FALSE);

    member_types = g_malloc(sizeof(GType) * cur_size);

    for(cur_member_type = first_member_type, n_members = 0;
        cur_member_type != G_TYPE_INVALID;
        cur_member_type = va_arg(var_args, GType), ++n_members)
    {
        if(n_members == cur_size) {
            cur_size += 5;
            member_types = g_realloc(member_types, sizeof(GType) * cur_size);
        }

        member_types[n_members] = cur_member_type;
    }

    ret = xfconf_channel_get_structv(channel, property, value_struct,
                                     n_members, member_types);
    g_free(member_types);

    return ret;
}

/**
 * xfconf_channel_get_structv:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value_struct: A pointer to a struct in which to store values.
 * @n_members: The number of data members in the struct.
 * @member_types: An array of @n_members #GType<!-- -->s.
 *
 * Gets a property on @channel and stores it as members of the
 * @value_struct struct.  The @member_types array should hold
 * a #GType for each member of the struct.
 *
 * Note: Struct members can only be non-pointer types such as int,
 * boolean, double, etc.
 *
 * Returns: %TRUE if the property was retrieved successfully,
 *          %FALSE oherwise.
 **/
gboolean
xfconf_channel_get_structv(XfconfChannel *channel,
                           const gchar *property,
                           gpointer value_struct,
                           guint n_members,
                           GType *member_types)
{
    GPtrArray *arr;
    guint i;
    GValue *val;
    gboolean ret = FALSE;
    gsize cur_offset = 0;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && value_struct
                         && n_members && member_types, FALSE);

    arr = xfconf_channel_get_arrayv(channel, property);
    if(!arr)
        return FALSE;

    if(arr->len != n_members) {
#ifdef XFCONF_ENABLE_CHECKS
        g_warning("Returned value array does not match the number of struct " \
                  "members (%d != %d)", arr->len, n_members);
#endif
        goto out;
    }

    for(i = 0; i < n_members; ++i) {
        typedef struct { guchar a; } DummyStruct;
#ifdef XFCONF_ENABLE_CHECKS
#define CHECK_VALUE_TYPES(val, GTYPE) G_STMT_START{ \
    if(G_VALUE_TYPE((val)) != (GTYPE)) { \
        g_warning("Returned value type does not match specified struct member type"); \
        goto out; \
    } \
}G_STMT_END
#else
#define CHECK_VALUE_TYPES(val, GTYPE) G_STMT_START{ \
    if(G_VALUE_TYPE((val)) != (GTYPE)) \
        goto out; \
}G_STMT_END
#endif

#define SET_STRUCT_VAL(ctype, GTYPE, alignment, cvalgetter)  G_STMT_START{ \
    ctype *__val_p; \
    val = g_ptr_array_index(arr, i); \
    CHECK_VALUE_TYPES(val, GTYPE); \
    cur_offset = ALIGN_VAL(cur_offset, alignment); \
    __val_p = (ctype *)(gpointer)(((guchar *)(&(((DummyStruct *)value_struct)->a)))+cur_offset); \
    *__val_p = cvalgetter(val); \
    cur_offset += sizeof(ctype); \
}G_STMT_END

        switch(member_types[i]) {
            case G_TYPE_STRING:
                SET_STRUCT_VAL(gchar *, G_TYPE_STRING, ALIGNOF_GPOINTER,
                               g_value_dup_string);
                break;
            
            case G_TYPE_UCHAR:
                SET_STRUCT_VAL(guchar, G_TYPE_UCHAR, ALIGNOF_GUCHAR,
                               g_value_get_uchar);
                break;

            case G_TYPE_CHAR:
                SET_STRUCT_VAL(gchar, G_TYPE_CHAR, ALIGNOF_GCHAR,
                               g_value_get_schar);
                break;

            case G_TYPE_UINT:
                SET_STRUCT_VAL(guint32, G_TYPE_UINT, ALIGNOF_GUINT32,
                               g_value_get_uint);
                break;

            case G_TYPE_INT:
                SET_STRUCT_VAL(gint32, G_TYPE_INT, ALIGNOF_GINT32,
                               g_value_get_int);
                break;

            case G_TYPE_UINT64:
                SET_STRUCT_VAL(guint64, G_TYPE_UINT64, ALIGNOF_GUINT64,
                               g_value_get_uint64);
                break;

            case G_TYPE_INT64:
                SET_STRUCT_VAL(gint64, G_TYPE_INT64, ALIGNOF_GINT64,
                               g_value_get_int64);
                break;

            case G_TYPE_FLOAT:
                SET_STRUCT_VAL(gfloat, G_TYPE_FLOAT, ALIGNOF_GFLOAT,
                               g_value_get_float);
                break;

            case G_TYPE_DOUBLE:
                SET_STRUCT_VAL(gdouble, G_TYPE_DOUBLE, ALIGNOF_GDOUBLE,
                               g_value_get_double);
                break;

            case G_TYPE_BOOLEAN:
                SET_STRUCT_VAL(gboolean, G_TYPE_BOOLEAN, ALIGNOF_GBOOLEAN,
                               g_value_get_boolean);
                break;

            default:
                if(XFCONF_TYPE_UINT16 == member_types[i]) {
                    /* uint16 is stored as uint */
                    SET_STRUCT_VAL(guint16, G_TYPE_UINT,
                                   ALIGNOF_GUINT16, g_value_get_uint);
                } else if(XFCONF_TYPE_INT16 == member_types[i]) {
                    /* int16 is stored as int */
                    SET_STRUCT_VAL(gint16, G_TYPE_INT,
                                   ALIGNOF_GINT16, g_value_get_int);
                } else {
#ifdef XFCONF_ENABLE_CHECKS
                    g_warning("Unable to handle value type %ld (%s) when " \
                              "setting a struct value", (long)member_types[i],
                              g_type_name(member_types[i]));
#endif
                    goto out;
                }
                break;
        }
    }

    ret = TRUE;

out:
    xfconf_array_free(arr);

    return ret;
}

/**
 * xfconf_channel_set_struct:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value_struct: A pointer to a struct from which to take values.
 * @first_member_type: The GType of the first member of @value_struct.
 * @...: A variable argument list of #GType<!-- -->s.
 *
 * Sets a property on @channel using the members of @value_struct
 * as a value array.  The @first_member_type argument specifies
 * the #GType of the first member of the struct.  The variable
 * argument list specifies the #GType<!-- -->s of the rest of the
 * struct members, and should be terminated with G_TYPE_INVALID.
 *
 * Note: This function takes your compiler's and platform's
 * struct member alignment rules into account when taking values
 * in @value_struct.  Therefore, it cannot be used with structs that
 * are declared as "packed" such that the alignment rules are ignored
 * by the compiler.
 *
 * Returns: %TRUE if the property was set successfully,
 *          %FALSE oherwise.
 **/
gboolean
xfconf_channel_set_struct(XfconfChannel *channel,
                          const gchar *property,
                          const gpointer value_struct,
                          GType first_member_type,
                          ...)
{
    gboolean ret;
    va_list var_args;

    va_start(var_args, first_member_type);
    ret = xfconf_channel_set_struct_valist(channel, property, value_struct,
                                           first_member_type, var_args);
    va_end(var_args);

    return ret;
}

/**
 * xfconf_channel_set_struct_valist:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value_struct: A pointer to a struct from which to take values.
 * @first_member_type: The GType of the first member of @value_struct.
 * @var_args: A variable argument list of #GType<!-- -->s.
 *
 * Sets a property on @channel using the members of @value_struct
 * as a value array.  See xfconf_channel_set_struct() for details.
 *
 * Returns: %TRUE if the property was set successfully,
 *          %FALSE oherwise.
 **/
gboolean
xfconf_channel_set_struct_valist(XfconfChannel *channel,
                                 const gchar *property,
                                 const gpointer value_struct,
                                 GType first_member_type,
                                 va_list var_args)
{
    GType cur_member_type;
    GType *member_types;
    guint n_members;
    gsize cur_size = 5;  /* FIXME: arbitrary... */
    gboolean ret;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && value_struct
                         && G_TYPE_INVALID != first_member_type, FALSE);

    member_types = g_malloc(sizeof(GType) * cur_size);

    for(cur_member_type = first_member_type, n_members = 0;
        cur_member_type != G_TYPE_INVALID;
        cur_member_type = va_arg(var_args, GType), ++n_members)
    {
        if(n_members == cur_size) {
            cur_size += 5;
            member_types = g_realloc(member_types, sizeof(GType) * cur_size);
        }

        member_types[n_members] = cur_member_type;
    }

    ret = xfconf_channel_set_structv(channel, property, value_struct,
                                     n_members, member_types);
    g_free(member_types);

    return ret;
}

/**
 * xfconf_channel_set_structv:
 * @channel: An #XfconfChannel.
 * @property: A string property name.
 * @value_struct: A pointer to a struct from which to take values.
 * @n_members: The number of data members in the struct.
 * @member_types: An array of @n_members #GType<!-- -->s.
 *
 * Sets a property on @channel using the members of @value_struct
 * as a value array.  The @member_types array should hold a #GType
 * for each member of the struct.
 *
 * Returns: %TRUE if the property was set successfully,
 *          %FALSE oherwise.
 **/
gboolean
xfconf_channel_set_structv(XfconfChannel *channel,
                           const gchar *property,
                           const gpointer value_struct,
                           guint n_members,
                           GType *member_types)
{
    GPtrArray *arr;
    guint i;
    GValue *val;
    gboolean ret = FALSE;
    gsize cur_offset = 0;

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel) && property && value_struct
                         && n_members && member_types, FALSE);

    arr = g_ptr_array_sized_new(n_members);

    for(i = 0; i < n_members; ++i) {
        typedef struct { guchar a; } DummyStruct;

#define GET_STRUCT_VAL(ctype, GTYPE, alignment, cvalsetter)  G_STMT_START{ \
    ctype *__val_p; \
    val = g_new0(GValue, 1); \
    g_value_init(val, GTYPE); \
    cur_offset = ALIGN_VAL(cur_offset, alignment); \
    __val_p = (ctype *)(gpointer)(((guchar *)(&(((DummyStruct *)value_struct)->a)))+cur_offset); \
    cvalsetter(val, *__val_p); \
    g_ptr_array_add(arr, val); \
    cur_offset += sizeof(ctype); \
}G_STMT_END

        switch(member_types[i]) {
            case G_TYPE_STRING:
                GET_STRUCT_VAL(gchar *, G_TYPE_STRING, ALIGNOF_GPOINTER,
                               g_value_set_static_string);
                break;
            
            case G_TYPE_UCHAR:
                GET_STRUCT_VAL(guchar, G_TYPE_UCHAR, ALIGNOF_GUCHAR,
                               g_value_set_uchar);
                break;

            case G_TYPE_CHAR:
                GET_STRUCT_VAL(gchar, G_TYPE_CHAR, ALIGNOF_GCHAR,
                               g_value_set_schar);
                break;

            case G_TYPE_UINT:
                GET_STRUCT_VAL(guint32, G_TYPE_UINT, ALIGNOF_GUINT32,
                               g_value_set_uint);
                break;

            case G_TYPE_INT:
                GET_STRUCT_VAL(gint32, G_TYPE_INT, ALIGNOF_GINT32,
                               g_value_set_int);
                break;

            case G_TYPE_UINT64:
                GET_STRUCT_VAL(guint64, G_TYPE_UINT64, ALIGNOF_GUINT64,
                               g_value_set_uint64);
                break;

            case G_TYPE_INT64:
                GET_STRUCT_VAL(gint64, G_TYPE_INT64, ALIGNOF_GINT64,
                               g_value_set_int64);
                break;

            case G_TYPE_FLOAT:
                GET_STRUCT_VAL(gfloat, G_TYPE_FLOAT, ALIGNOF_GFLOAT,
                               g_value_set_float);
                break;

            case G_TYPE_DOUBLE:
                GET_STRUCT_VAL(gdouble, G_TYPE_DOUBLE, ALIGNOF_GDOUBLE,
                               g_value_set_double);
                break;

            case G_TYPE_BOOLEAN:
                GET_STRUCT_VAL(gboolean, G_TYPE_BOOLEAN, ALIGNOF_GBOOLEAN,
                               g_value_set_boolean);
                break;

            default:
                if(XFCONF_TYPE_UINT16 == member_types[i]) {
                    /* _set_arrayv() will convert these */
                    GET_STRUCT_VAL(guint16, XFCONF_TYPE_UINT16,
                                   ALIGNOF_GUINT16, xfconf_g_value_set_uint16);
                } else if(XFCONF_TYPE_INT16 == member_types[i]) {
                    /* _set_arrayv() will convert these */
                    GET_STRUCT_VAL(gint16, XFCONF_TYPE_INT16,
                                   ALIGNOF_GINT16, xfconf_g_value_set_int16);
                } else {
#ifdef XFCONF_ENABLE_CHECKS
                    g_warning("Unable to handle value type %ld (%s) when " \
                              "getting a struct value", (long)member_types[i],
                              g_type_name(member_types[i]));
#endif
                    goto out;
                }
                break;
        }
    }

    ret = xfconf_channel_set_arrayv(channel, property, arr);

out:
    xfconf_array_free(arr);

    return ret;
}

/**
 * xfconf_list_channels:
 *
 * Lists all channels known in the Xfconf configuration store.
 *
 * Returns: (transfer full) (array zero-terminated=1) (type utf8): A newly-allocated array of strings.
 *                                                                 Free with g_strfreev() when no longer needed.
 */
gchar **
xfconf_list_channels(void)
{
    GDBusProxy *proxy = _xfconf_get_gdbus_proxy();
    gchar **channels = NULL;
    ERROR_DEFINE;

    if(!xfconf_exported_call_list_channels_sync ((XfconfExported*)proxy, 
                                               &channels, NULL, ERROR))
        ERROR_CHECK;

    return channels;
}



#define __XFCONF_CHANNEL_C__
#include "common/xfconf-aliasdef.c"
