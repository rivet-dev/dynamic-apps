/*
 *  xfconfd
 *  
 *  Copyright (c) 2016 Ali Abdallah <ali@xfce.org>
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libxfce4util/libxfce4util.h>

#include "xfconf-daemon.h"
#include "xfconf-backend-factory.h"
#include "xfconf-backend.h"
#include "common/xfconf-marshal.h"
#include "common/xfconf-gvaluefuncs.h"
#include "xfconf/xfconf-errors.h"
#include "common/xfconf-common-private.h"
#include "common/xfconf-gdbus-bindings.h"

struct _XfconfDaemon
{
    XfconfExportedSkeleton parent;
    guint filter_id;
    
    GDBusConnection *conn;

    GList *backends;
};

typedef struct _XfconfDaemonClass
{
    XfconfExportedSkeletonClass parent;
} XfconfDaemonClass;

static void xfconf_daemon_finalize(GObject *obj);

G_DEFINE_TYPE(XfconfDaemon, xfconf_daemon, XFCONF_TYPE_EXPORTED_SKELETON)
  
static void
xfconf_daemon_class_init(XfconfDaemonClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;

    object_class->finalize = xfconf_daemon_finalize;
}

static void
xfconf_daemon_init(XfconfDaemon *instance)
{
    instance->filter_id = 0;
}

static void
xfconf_daemon_finalize(GObject *obj)
{
    XfconfDaemon *xfconfd = XFCONF_DAEMON(obj);
    GList *l;
    for(l = xfconfd->backends; l; l = l->next) {
        xfconf_backend_register_property_changed_func(l->data, NULL, NULL);
        xfconf_backend_flush(l->data, NULL);
        g_object_unref(l->data);
    }
    g_list_free(xfconfd->backends);

    if(xfconfd->filter_id) {
        g_signal_handler_disconnect (xfconfd->conn, xfconfd->filter_id);
    }

    G_OBJECT_CLASS(xfconf_daemon_parent_class)->finalize(obj);
}

typedef struct
{
    XfconfDaemon *xfconfd;
    XfconfBackend *backend;
    gchar *channel;
    gchar *property;
} XfconfPropChangedData;

static gboolean
xfconf_daemon_emit_property_changed_idled(gpointer data)
{
    XfconfPropChangedData *pdata = data;
    GValue value = { 0, };
    xfconf_backend_get(pdata->backend, pdata->channel, pdata->property,
                       &value, NULL);
    if(G_VALUE_TYPE(&value)) {
        GVariant *val, *variant;
        val = xfconf_gvalue_to_gvariant (&value);
        if (val) {
            variant = g_variant_new_variant (val);
            xfconf_exported_emit_property_changed ((XfconfExported*)pdata->xfconfd,
                                                   pdata->channel, pdata->property, variant);
            g_variant_unref (val);
        }
        g_value_unset(&value);
    } else {
        xfconf_exported_emit_property_removed ((XfconfExported*)pdata->xfconfd,
                                               pdata->channel, pdata->property);
    }
    g_object_unref(G_OBJECT(pdata->backend));
    g_free(pdata->channel);
    g_free(pdata->property);
    g_object_unref(G_OBJECT(pdata->xfconfd));
    g_slice_free(XfconfPropChangedData, pdata);

    return FALSE;
}

static void
xfconf_daemon_backend_property_changed(XfconfBackend *backend,
                                       const gchar *channel,
                                       const gchar *property,
                                       gpointer user_data)
{
    XfconfPropChangedData *pdata = g_slice_new0(XfconfPropChangedData);
    pdata->xfconfd = g_object_ref(XFCONF_DAEMON(user_data));
    pdata->backend = g_object_ref(XFCONF_BACKEND(backend));
    pdata->channel = g_strdup(channel);
    pdata->property = g_strdup(property);
    g_idle_add(xfconf_daemon_emit_property_changed_idled, pdata);
}

static gboolean
xfconf_set_property(XfconfExported *skeleton,
                    GDBusMethodInvocation *invocation,
                    const gchar *channel,
                    const gchar *property,
                    GVariant *variant,
                    XfconfDaemon *xfconfd)
{
    GList *l;
    GError *error = NULL;
    GValue *value;

    /* if there's more than one backend, we need to make sure the
     * property isn't locked on ANY of them */
    if(G_UNLIKELY(xfconfd->backends->next)) {
        for(l = xfconfd->backends; l; l = l->next) {
            gboolean locked = FALSE;

            if(!xfconf_backend_is_property_locked(l->data, channel, property,
                                                  &locked, &error))
                break;

            if(locked) {
                g_set_error(&error, XFCONF_ERROR,
                            XFCONF_ERROR_PERMISSION_DENIED,
                            _("Permission denied while modifying property \"%s\" on channel \"%s\""),
                            property, channel);
                break;
            }
        }

        /* there is always an error set if something failed or the
         * property is locked */
        if(error) {
            g_dbus_method_invocation_return_gerror(invocation, error);
            g_error_free(error);
            return G_DBUS_METHOD_INVOCATION_UNHANDLED;
        }
    }
    
    value = xfconf_gvariant_to_gvalue (variant);
    /* only write to first backend */
    if(xfconf_backend_set(xfconfd->backends->data, channel, property,
                          value, &error))
    {
        xfconf_exported_complete_set_property(skeleton, invocation);
    } else {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
    }

    g_value_unset (value);
    g_free (value);
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}


static gboolean
xfconf_get_property(XfconfExported *skeleton,
                    GDBusMethodInvocation *invocation,
                    const gchar *channel,
                    const gchar *property,
                    XfconfDaemon *xfconfd)
{
    GList *l;
    GValue value = { 0, };
    GError *error = NULL;

    /* check each backend until we find a value */
    for(l = xfconfd->backends; l; l = l->next) {
        if(xfconf_backend_get(l->data, channel, property, &value, &error)) {
            GVariant *variant, *val;
            val = xfconf_gvalue_to_gvariant (&value);
            if (val){
                variant = g_variant_new_variant (val);
                xfconf_exported_complete_get_property(skeleton, invocation, variant);
                g_variant_unref (val);
            }
            else {
                g_set_error (&error, XFCONF_ERROR, 
                             XFCONF_ERROR_INTERNAL_ERROR, _("GType transformation failed \"%s\""),
                             G_VALUE_TYPE_NAME(&value));
                g_dbus_method_invocation_return_gerror(invocation, error);
                g_error_free(error);
            }
            g_value_unset(&value);
            return G_DBUS_METHOD_INVOCATION_UNHANDLED;
        } else if(l->next)
            g_clear_error(&error);
    }
    g_dbus_method_invocation_return_gerror(invocation, error);
    g_error_free(error);
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}

static gboolean
xfconf_get_all_properties(XfconfExported *skeleton,
                          GDBusMethodInvocation *invocation,
                          const gchar *channel,
                          const gchar *property_base,
                          XfconfDaemon *xfconfd)
{
    GList *l;
    GHashTable *properties;
    GError *error = NULL;
    gboolean succeed = FALSE;
    properties = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        (GDestroyNotify)g_free,
                                        (GDestroyNotify)_xfconf_gvalue_free);
    /* get all properties from all backends */
    for(l = xfconfd->backends; l; l = l->next) {
        if(xfconf_backend_get_all(l->data, channel, property_base,
                                  properties, &error))
            succeed = TRUE;
        else if(l->next) {
            g_clear_error(&error);
        }
    }
    if(succeed) {
        GVariant *variant;
        variant = xfconf_hash_to_gvariant (properties);
        xfconf_exported_complete_get_all_properties (skeleton, invocation, variant);
    }
    else
        g_dbus_method_invocation_return_gerror(invocation, error);

    if(error)
        g_error_free(error);
    g_hash_table_destroy(properties);
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}

static gboolean
xfconf_property_exists(XfconfExported *skeleton,
                       GDBusMethodInvocation *invocation,
                       const gchar *channel,
                       const gchar *property,
                       XfconfDaemon *xfconfd)
{
    gboolean exists = FALSE;
    gboolean succeed = FALSE;
    GList *l;
    GError *error = NULL;

    for(l = xfconfd->backends; !exists && l; l = l->next) {
        if(xfconf_backend_exists(l->data, channel, property, &exists, &error))
            succeed = TRUE;
        else if(l->next)
            g_clear_error(&error);
    }

    if(succeed)
        xfconf_exported_complete_property_exists (skeleton, invocation, exists);
    else {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
    }
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}

static gboolean
xfconf_reset_property(XfconfExported *skeleton,
                      GDBusMethodInvocation *invocation,
                      const gchar *channel,
                      const gchar *property,
                      gboolean recursive,
                      XfconfDaemon *xfconfd)
{
    gboolean succeed = FALSE;
    GList *l;
    GError *error = NULL;
    /* while technically all backends but the first should be opened read-only,
     * we need to reset in all backends so the property doesn't reappear
     * later */

    for(l = xfconfd->backends; l; l = l->next) {
        if(xfconf_backend_reset(l->data, channel, property, recursive, &error))
            succeed = TRUE;
        else if(l->next)
            g_clear_error(&error);
    }

    if(succeed)
        xfconf_exported_complete_reset_property(skeleton, invocation);
    else
        g_dbus_method_invocation_return_gerror(invocation, error);

    if(error)
        g_error_free(error);

    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}

static gboolean
xfconf_list_channels(XfconfExported *skeleton,
                    GDBusMethodInvocation *invocation,
                    XfconfDaemon *xfconfd)
{
    GSList *lchannels = NULL, *chans_tmp, *lc;
    GList *l;
    guint i;
    gchar **channels;
    GError *error = NULL;
    /* FIXME: with multiple backends, this can cause duplicates */
    for(l = xfconfd->backends; l; l = l->next) {
        chans_tmp = NULL;
        if(xfconf_backend_list_channels(l->data, &chans_tmp, &error))
            lchannels = g_slist_concat(lchannels, chans_tmp);
        else if(l->next)
            g_clear_error(&error);
    }

    if(error && !lchannels) {
        /* no channels and an error, something went wrong */
        g_dbus_method_invocation_return_gerror(invocation, error);
    } else {
        channels = g_new (gchar *, g_slist_length(lchannels) + 1);
        for(lc = lchannels, i = 0; lc; lc = lc->next, ++i)
            channels[i] = lc->data;
        channels[i] = NULL;
        
        xfconf_exported_complete_list_channels (skeleton, invocation, (const gchar *const*)channels);

        g_strfreev(channels);
        g_slist_free(lchannels);
    }

    if(error)
        g_error_free(error);

    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}

static gboolean xfconf_is_property_locked(XfconfExported *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          const gchar *channel,
                                          const gchar *property,
                                          XfconfDaemon *xfconfd)
{
    GList *l;
    gboolean locked = FALSE;
    GError *error = NULL;
    gboolean succeed = FALSE;
    for(l = xfconfd->backends; !locked && l; l = l->next) {
        if(xfconf_backend_is_property_locked(l->data, channel, property,
                                             &locked, &error))
            succeed = TRUE;
        else if(l->next)
            g_clear_error(&error);
    }

    if(succeed)
        xfconf_exported_complete_is_property_locked(skeleton, invocation, locked);
    else
        g_dbus_method_invocation_return_gerror(invocation, error);

    if(error)
        g_error_free(error);

    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}

static void
xfconf_daemon_handle_dbus_disconnect(GDBusConnection *conn,
                                     gboolean remote,
                                     GError *error,
                                     gpointer data)
{
    XfconfDaemon *xfconfd = (XfconfDaemon*)data;
    GList *l;
    
    DBG("got dbus disconnect; flushing all channels");
    
    for(l = xfconfd->backends; l; l = l->next) {
        GError *lerror = NULL;
        if(!xfconf_backend_flush(XFCONF_BACKEND(l->data), &lerror)) {
            g_critical("Failed to flush backend on disconnect: %s",
                       lerror->message);
            g_error_free(lerror);
        }
    }
    
}



static gboolean
xfconf_daemon_start(XfconfDaemon *xfconfd,
                    GError **error)
{
    int ret;

    xfconfd->conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
    if (G_UNLIKELY(!xfconfd->conn))
    {
        return FALSE;
    }
    
    ret = 
    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON(xfconfd),
                                      xfconfd->conn,
                                      XFCONF_SERVICE_PATH_PREFIX "/Xfconf",
                                      error);
    
    if (ret == FALSE)
        return FALSE;
    
    xfconfd->filter_id = g_signal_connect (xfconfd->conn, "closed",
                                           G_CALLBACK(xfconf_daemon_handle_dbus_disconnect),
                                           xfconfd);

    return TRUE;
}

static gboolean
xfconf_daemon_load_config(XfconfDaemon *xfconfd,
                          gchar * const *backend_ids,
                          GError **error)
{
    gint i;

    for(i = 0; backend_ids[i]; ++i) {
        GError *error1 = NULL;
        XfconfBackend *backend = xfconf_backend_factory_get_backend(backend_ids[i],
                                                                    &error1);
        if(!backend) {
            g_warning("Unable to start backend \"%s\": %s", backend_ids[i],
                      error1->message);
            g_clear_error(&error1);
        } else {
            xfconfd->backends = g_list_prepend(xfconfd->backends, backend);
            xfconf_backend_register_property_changed_func(backend,
                                                          xfconf_daemon_backend_property_changed,
                                                          xfconfd);
        }
    }

    if(!xfconfd->backends) {
        if(error) {
            g_set_error(error, XFCONF_ERROR, XFCONF_ERROR_NO_BACKEND,
                        _("No backends could be started"));
        }
        return FALSE;
    }

    xfconfd->backends = g_list_reverse(xfconfd->backends);

    return TRUE;
}


#define XFCONF_DAEMON_CONNECT(signal_name, signal_handler) \
    G_STMT_START{ \
        g_signal_connect_swapped (xfconfd, signal_name, \
                                  G_CALLBACK (xfconf_lifecycle_manager_increment_use_count), \
                                  manager); \
        g_signal_connect (xfconfd, signal_name, signal_handler, xfconfd); \
        g_signal_connect_swapped (xfconfd, signal_name, \
                                  G_CALLBACK (xfconf_lifecycle_manager_keep_alive), manager); \
        g_signal_connect_swapped (xfconfd, signal_name, \
                                  G_CALLBACK (xfconf_lifecycle_manager_decrement_use_count), \
                                  manager); \
    }G_STMT_END

typedef struct
{
    gchar *name;
    GCallback handler;
} XfconfExportedSignal;

static const XfconfExportedSignal xfconf_exported_signals[] =
{
    { "handle-get-all-properties", G_CALLBACK (xfconf_get_all_properties) },
    { "handle-get-property", G_CALLBACK (xfconf_get_property) },
    { "handle-is-property-locked", G_CALLBACK (xfconf_is_property_locked) },
    { "handle-list-channels", G_CALLBACK (xfconf_list_channels) },
    { "handle-property-exists", G_CALLBACK (xfconf_property_exists) },
    { "handle-reset-property", G_CALLBACK (xfconf_reset_property) },
    { "handle-set-property", G_CALLBACK (xfconf_set_property) },
};

XfconfDaemon *
xfconf_daemon_new_unique(gchar * const *backend_ids,
                         XfconfLifecycleManager *manager,
                         GError **error)
{
    XfconfDaemon *xfconfd;

    g_return_val_if_fail(backend_ids && backend_ids[0], NULL);

    xfconfd = g_object_new(XFCONF_TYPE_DAEMON, NULL);

    if(!xfconf_daemon_start(xfconfd, error)
       || !xfconf_daemon_load_config(xfconfd, backend_ids, error))
    {
        g_object_unref(G_OBJECT(xfconfd));
        return NULL;
    }

    for (guint n = 0; n < G_N_ELEMENTS (xfconf_exported_signals); n++)
        XFCONF_DAEMON_CONNECT (xfconf_exported_signals[n].name,
                               xfconf_exported_signals[n].handler);

    return xfconfd;
}
