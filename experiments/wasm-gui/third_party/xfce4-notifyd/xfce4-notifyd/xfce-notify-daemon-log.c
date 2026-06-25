/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2023 Brian Tarricone <brian@tarricone.org>
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

#include <glib/gi18n.h>

#include "xfce-notify-daemon-log.h"

struct _XfceNotifyDaemonLog {
    XfceNotifyLogGBusSkeleton parent;

    GDBusConnection *bus;
    XfceNotifyLog *log;
};

enum {
    PROP0,
    PROP_BUS_CONNECTION,
    PROP_LOG,
};

static void xfce_notify_daemon_log_initable_init(GInitableIface *iface);

static void xfce_notify_daemon_log_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void xfce_notify_daemon_log_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec);
static void xfce_notify_daemon_log_finalize(GObject *object);

static gboolean xfce_notify_daemon_log_real_init(GInitable *initable,
                                                 GCancellable *cancellable,
                                                 GError **error);

G_DEFINE_TYPE_WITH_CODE(XfceNotifyDaemonLog,
                        xfce_notify_daemon_log,
                        XFCE_TYPE_NOTIFY_LOG_GBUS_SKELETON,
                        G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE, xfce_notify_daemon_log_initable_init))

static void
xfce_notify_daemon_log_class_init(XfceNotifyDaemonLogClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = xfce_notify_daemon_log_finalize;
    gobject_class->set_property = xfce_notify_daemon_log_set_property;
    gobject_class->get_property = xfce_notify_daemon_log_get_property;

    g_object_class_install_property(gobject_class,
                                    PROP_BUS_CONNECTION,
                                    g_param_spec_object("bus-connection",
                                                        "bus-connection",
                                                        "bus-connection",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_LOG,
                                    g_param_spec_object("log",
                                                        "log",
                                                        "log",
                                                        XFCE_TYPE_NOTIFY_LOG,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
xfce_notify_daemon_log_initable_init(GInitableIface *iface) {
    iface->init = xfce_notify_daemon_log_real_init;
}

static void
xfce_notify_daemon_log_init(XfceNotifyDaemonLog *xndlog) {}

static void
xfce_notify_daemon_log_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    XfceNotifyDaemonLog *xndlog = XFCE_NOTIFY_DAEMON_LOG(object);

    switch (prop_id) {
        case PROP_BUS_CONNECTION:
            xndlog->bus = g_value_dup_object(value);
            break;

        case PROP_LOG:
            xndlog->log = g_value_dup_object(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfce_notify_daemon_log_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    XfceNotifyDaemonLog *xndlog = XFCE_NOTIFY_DAEMON_LOG(object);

    switch (prop_id) {
        case PROP_BUS_CONNECTION:
            g_value_set_object(value, xndlog->bus);
            break;

        case PROP_LOG:
            g_value_set_object(value, xndlog->log);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfce_notify_daemon_log_finalize(GObject *object) {
    XfceNotifyDaemonLog *xndlog = XFCE_NOTIFY_DAEMON_LOG(object);

    if (g_dbus_interface_skeleton_has_connection(G_DBUS_INTERFACE_SKELETON(xndlog), xndlog->bus)) {
        g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(xndlog));
    }
    g_object_unref(xndlog->bus);

    if (xndlog->log != NULL) {
        g_signal_handlers_disconnect_by_data(xndlog->log, xndlog);
        g_object_unref(xndlog->log);
    }

    G_OBJECT_CLASS(xfce_notify_daemon_log_parent_class)->finalize(object);
}

static void
variant_build_log_entry(GVariantBuilder *builder, XfceNotifyLogEntry *entry) {
#define SAFES(str) ((str) != NULL ? (str) : "")
    g_variant_builder_add(builder, "s", SAFES(entry->id));
    g_variant_builder_add(builder, "x", g_date_time_to_unix(entry->timestamp) * 1000000 + g_date_time_get_microsecond(entry->timestamp));
    g_variant_builder_add(builder, "s", SAFES(g_time_zone_get_identifier(g_date_time_get_timezone(entry->timestamp))));
    g_variant_builder_add(builder, "s", SAFES(entry->app_id));
    g_variant_builder_add(builder, "s", SAFES(entry->app_name));
    g_variant_builder_add(builder, "s", SAFES(entry->icon_id));
    g_variant_builder_add(builder, "s", SAFES(entry->summary));
    g_variant_builder_add(builder, "s", SAFES(entry->body));
    g_variant_builder_open(builder, G_VARIANT_TYPE("a(ss)"));
    for (GList *l = entry->actions; l != NULL; l = l->next) {
        XfceNotifyLogEntryAction *action = l->data;
        g_variant_builder_open(builder, G_VARIANT_TYPE("(ss)"));
        g_variant_builder_add(builder, "s", SAFES(action->id));
        g_variant_builder_add(builder, "s", SAFES(action->label));
        g_variant_builder_close(builder);
    }
    g_variant_builder_close(builder);
    g_variant_builder_add(builder, "i", entry->expire_timeout);
    g_variant_builder_add(builder, "b", entry->is_read);
#undef SAFES
}


static gboolean
notify_log_get(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation, const gchar *arg_id) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        XfceNotifyLogEntry *entry = xfce_notify_log_get(xndlog->log, arg_id);

        if (entry == NULL) {
            g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, _("Log entry not found"));
        } else {
            GVariantBuilder builder;
            GVariant *ret;

            g_variant_builder_init(&builder, G_VARIANT_TYPE("(sxssssssa(ss)ib)"));
            variant_build_log_entry(&builder, entry);
            ret = g_variant_builder_end(&builder);
            xfce_notify_log_gbus_complete_get(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation, ret);

            g_variant_builder_clear(&builder);
            xfce_notify_log_entry_unref(entry);
        }
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_list(XfceNotifyDaemonLog *xndlog,
                GDBusMethodInvocation *invocation,
                const gchar *arg_start_after_id,
                guint arg_count,
                gboolean arg_only_unread)
{
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        GList *entries;
        GVariantBuilder builder;
        GVariant *ret;
        const gchar *start_after_id = arg_start_after_id != NULL && arg_start_after_id[0] != '\0' ? arg_start_after_id : NULL;

        if (arg_only_unread) {
            entries = xfce_notify_log_read_unread(xndlog->log, start_after_id, arg_count);
        } else {
            entries = xfce_notify_log_read(xndlog->log, start_after_id, arg_count);
        }

        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sxssssssa(ss)ib)"));
        for (GList *l = entries; l != NULL; l = l->next) {
            XfceNotifyLogEntry *entry = l->data;
            g_variant_builder_open(&builder, G_VARIANT_TYPE("(sxssssssa(ss)ib)"));
            variant_build_log_entry(&builder, entry);
            g_variant_builder_close(&builder);
        }

        ret = g_variant_builder_end(&builder);
        xfce_notify_log_gbus_complete_list(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation, ret);

        g_variant_builder_clear(&builder);
        g_list_free_full(entries, (GDestroyNotify)xfce_notify_log_entry_unref);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_has_unread(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        xfce_notify_log_gbus_complete_has_unread(XFCE_NOTIFY_LOG_GBUS(xndlog),
                                                 invocation,
                                                 xfce_notify_log_has_unread_messages(xndlog->log));
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_get_app_id_counts(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        GHashTable *counts = xfce_notify_log_get_app_id_counts(xndlog->log);
        GVariantBuilder builder;
        GVariant *ret;
        GHashTableIter iter;
        gpointer key, value;

        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{su}"));

        g_hash_table_iter_init(&iter, counts);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            const gchar *app_id = key;
            guint count = GPOINTER_TO_UINT(value);

            g_variant_builder_open(&builder, G_VARIANT_TYPE("{su}"));
            g_variant_builder_add(&builder, "s", app_id);
            g_variant_builder_add(&builder, "u", count);
            g_variant_builder_close(&builder);
        }

        ret = g_variant_builder_end(&builder);
        xfce_notify_log_gbus_complete_get_app_id_counts(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation, ret);

        g_variant_builder_clear(&builder);
        g_hash_table_destroy(counts);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_mark_read(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation, const gchar *const *arg_ids) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        for (guint i = 0; arg_ids[i] != NULL; ++i) {
            xfce_notify_log_mark_read(xndlog->log, arg_ids[i]);
        }
        xfce_notify_log_gbus_complete_mark_read(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_mark_all_read(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        xfce_notify_log_mark_all_read(xndlog->log);
        xfce_notify_log_gbus_complete_mark_all_read(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_delete(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation, const gchar *const *arg_ids) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        for (guint i = 0; arg_ids[i] != NULL; ++i) {
            xfce_notify_log_delete(xndlog->log, arg_ids[i]);
        }
        xfce_notify_log_gbus_complete_delete(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_truncate(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation, guint arg_entries_to_keep) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        xfce_notify_log_truncate(xndlog->log, arg_entries_to_keep);
        xfce_notify_log_gbus_complete_truncate(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
notify_log_clear(XfceNotifyDaemonLog *xndlog, GDBusMethodInvocation *invocation) {
    if (xndlog->log == NULL) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_FAILED, _("Log is unavailable"));
    } else {
        xfce_notify_log_clear(xndlog->log);
        xfce_notify_log_gbus_complete_clear(XFCE_NOTIFY_LOG_GBUS(xndlog), invocation);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
log_row_added(XfceNotifyLog *log, const gchar *entry_id, XfceNotifyDaemonLog *xndlog) {
    xfce_notify_log_gbus_emit_row_added(XFCE_NOTIFY_LOG_GBUS(xndlog), entry_id);
}

static void
log_row_changed(XfceNotifyLog *log, const gchar *entry_id, XfceNotifyDaemonLog *xndlog) {
    xfce_notify_log_gbus_emit_row_changed(XFCE_NOTIFY_LOG_GBUS(xndlog), entry_id != NULL ? entry_id : "");
}

static void
log_row_deleted(XfceNotifyLog *log, const gchar *entry_id, XfceNotifyDaemonLog *xndlog) {
    xfce_notify_log_gbus_emit_row_deleted(XFCE_NOTIFY_LOG_GBUS(xndlog), entry_id);
}

static void
log_truncated(XfceNotifyLog *log, guint n_kept_entries, XfceNotifyDaemonLog *xndlog) {
    xfce_notify_log_gbus_emit_truncated(XFCE_NOTIFY_LOG_GBUS(xndlog), n_kept_entries);
}

static void
log_cleared(XfceNotifyLog *log, XfceNotifyDaemonLog *xndlog) {
    xfce_notify_log_gbus_emit_cleared(XFCE_NOTIFY_LOG_GBUS(xndlog));
}

static gboolean
xfce_notify_daemon_log_real_init(GInitable *initable, GCancellable *cancellable, GError **error) {
    XfceNotifyDaemonLog *xndlog = XFCE_NOTIFY_DAEMON_LOG(initable);
    gboolean exported;

    exported = g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(xndlog),
                                                xndlog->bus,
                                                "/org/xfce/Notifyd",
                                                error);
    if (exported) {
        g_signal_connect(xndlog, "handle-get",
                         G_CALLBACK(notify_log_get), NULL);
        g_signal_connect(xndlog, "handle-list",
                         G_CALLBACK(notify_log_list), NULL);
        g_signal_connect(xndlog, "handle-has-unread",
                         G_CALLBACK(notify_log_has_unread), NULL);
        g_signal_connect(xndlog, "handle-get-app-id-counts",
                         G_CALLBACK(notify_log_get_app_id_counts), NULL);
        g_signal_connect(xndlog, "handle-mark-read",
                         G_CALLBACK(notify_log_mark_read), NULL);
        g_signal_connect(xndlog, "handle-mark-all-read",
                         G_CALLBACK(notify_log_mark_all_read), NULL);
        g_signal_connect(xndlog, "handle-delete",
                         G_CALLBACK(notify_log_delete), NULL);
        g_signal_connect(xndlog, "handle-truncate",
                         G_CALLBACK(notify_log_truncate), NULL);
        g_signal_connect(xndlog, "handle-clear",
                         G_CALLBACK(notify_log_clear), NULL);

        g_signal_connect(xndlog->log, "row-added",
                         G_CALLBACK(log_row_added), xndlog);
        g_signal_connect(xndlog->log, "row-changed",
                         G_CALLBACK(log_row_changed), xndlog);
        g_signal_connect(xndlog->log, "row-deleted",
                         G_CALLBACK(log_row_deleted), xndlog);
        g_signal_connect(xndlog->log, "truncated",
                         G_CALLBACK(log_truncated), xndlog);
        g_signal_connect(xndlog->log, "cleared",
                         G_CALLBACK(log_cleared), xndlog);

        return TRUE;
    } else {
        return FALSE;
    }
}

XfceNotifyDaemonLog *
xfce_notify_daemon_log_new(GDBusConnection *bus, XfceNotifyLog *log, GError **error) {
    g_return_val_if_fail(G_IS_DBUS_CONNECTION(bus), NULL);
    g_return_val_if_fail(log == NULL || XFCE_IS_NOTIFY_LOG(log), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    return g_initable_new(XFCE_TYPE_NOTIFY_DAEMON_LOG, NULL, error,
                          "bus-connection", bus,
                          "log", log,
                          NULL);
}
