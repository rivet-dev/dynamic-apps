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

#include <sqlite3.h>

#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>

#include "xfce-notify-log.h"

#define TABLE "notifications"

#define COL_ID "id"
#define COL_TIMESTAMP "timestamp"
#define COL_TZ_IDENTIFIER "tz_identifier"
#define COL_APP_ID "app_id"
#define COL_APP_NAME "app_name"
#define COL_ICON_ID "icon_id"
#define COL_SUMMARY "summary"
#define COL_BODY "body"
#define COL_ACTIONS "actions"
#define COL_EXPIRE_TIMEOUT "expire_timeout"
#define COL_IS_READ "is_read"

#define SCHEMA \
    "CREATE TABLE IF NOT EXISTS " TABLE " (" \
        COL_ID " TEXT PRIMARY KEY NOT NULL," \
        COL_TIMESTAMP " INTEGER NOT NULL," \
        COL_TZ_IDENTIFIER " TEXT NOT NULL," \
        COL_APP_ID " TEXT," \
        COL_APP_NAME " TEXT," \
        COL_ICON_ID " TEXT," \
        COL_SUMMARY" TEXT," \
        COL_BODY " TEXT," \
        COL_ACTIONS " BLOB," \
        COL_EXPIRE_TIMEOUT " INTEGER," \
        COL_IS_READ " INTEGER NOT NULL DEFAULT FALSE" \
    ") STRICT"

#define INDEX_TIMESTAMP \
    "CREATE INDEX IF NOT EXISTS idx_" TABLE "_" COL_TIMESTAMP " ON " TABLE "(" COL_TIMESTAMP " DESC)"
#define INDEX_IS_READ \
    "CREATE INDEX IF NOT EXISTS idx_" TABLE "_" COL_IS_READ " ON " TABLE "(" COL_IS_READ ")"

#define BIND_LIMIT  "limit"

#define BIND_INDEX(stmt, param_name) sqlite3_bind_parameter_index(stmt, ":" param_name)

typedef enum {
    XFCE_NOTIFY_QUEUE_ITEM_WRITE,
    XFCE_NOTIFY_QUEUE_ITEM_MARK_READ,
    XFCE_NOTIFY_QUEUE_ITEM_DELETE,
    XFCE_NOTIFY_QUEUE_ITEM_TRUNCATE,
} XfceNotifyLogQueueItemType;

typedef struct {
    XfceNotifyLogQueueItemType type;
    union {
        XfceNotifyLogEntry *entry;
        gchar *id;
        guint count;
    } param;
} XfceNotifyLogQueueItem;

typedef struct _XfceNotifyLog {
    GObject parent;

    /*< private >*/
    sqlite3 *db;
    sqlite3_stmt *stmt_get;
    sqlite3_stmt *stmt_get_timestamp;
    sqlite3_stmt *stmt_read;
    sqlite3_stmt *stmt_read_with_timestamp;
    sqlite3_stmt *stmt_read_unread;
    sqlite3_stmt *stmt_read_unread_with_timestamp;
    sqlite3_stmt *stmt_has_unreads;
    sqlite3_stmt *stmt_count_unreads;
    sqlite3_stmt *stmt_count_app_ids;
    sqlite3_stmt *stmt_write;
    sqlite3_stmt *stmt_mark_read;
    sqlite3_stmt *stmt_mark_all_read;
    sqlite3_stmt *stmt_delete;
    sqlite3_stmt *stmt_delete_before;
    sqlite3_stmt *stmt_delete_all;

    gboolean sqlite_delete_supports_limit_offset;

    GFileMonitor *monitor;

    guint write_queue_id;
    GQueue *write_queue;
} XfceNotifyLog;

enum {
    SIG_ROW_ADDED,
    SIG_ROW_CHANGED,
    SIG_ROW_DELETED,
    SIG_TRUNCATED,
    SIG_CLEARED,

    N_SIGNALS,
};

static void xfce_notify_log_initable_init(GInitableIface *iface);
static gboolean xfce_notify_log_initable_real_init(GInitable *initable,
                                                   GCancellable *cancellable,
                                                   GError **error);

static void xfce_notify_log_finalize(GObject *object);

static void xfce_notify_log_queue_item_free(XfceNotifyLogQueueItem *item);

static GFile *notify_log_dir(void);
static GError *transform_error(sqlite3 *db,
                               int errcode,
                               const gchar *message_fmt);
static gboolean prepare_statements(XfceNotifyLog *log,
                                   GError **error);
static gboolean ensure_tables(XfceNotifyLog *log,
                              GError **error);

static XfceNotifyLogQueueItem *xfce_notify_log_queue_item_new(XfceNotifyLogQueueItemType item_type);
static gboolean process_write_queue(gpointer data);
static void queue_write(XfceNotifyLog *log, XfceNotifyLogQueueItem *item);

static gboolean migrate_old_keyfile(XfceNotifyLog *log);


G_DEFINE_TYPE_WITH_CODE(XfceNotifyLog, xfce_notify_log, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE, xfce_notify_log_initable_init))


static guint log_signals[N_SIGNALS] = { 0, };

static void
xfce_notify_log_class_init(XfceNotifyLogClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = xfce_notify_log_finalize;

    log_signals[SIG_ROW_ADDED] = g_signal_new("row-added",
                                              XFCE_TYPE_NOTIFY_LOG,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_VOID__STRING,
                                              G_TYPE_NONE, 1,
                                              G_TYPE_STRING);

    log_signals[SIG_ROW_CHANGED] = g_signal_new("row-changed",
                                                XFCE_TYPE_NOTIFY_LOG,
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                g_cclosure_marshal_VOID__STRING,
                                                G_TYPE_NONE, 1,
                                                G_TYPE_STRING);

    log_signals[SIG_ROW_DELETED] = g_signal_new("row-deleted",
                                                XFCE_TYPE_NOTIFY_LOG,
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                g_cclosure_marshal_VOID__STRING,
                                                G_TYPE_NONE, 1,
                                                G_TYPE_STRING);


    log_signals[SIG_TRUNCATED] = g_signal_new("truncated",
                                              XFCE_TYPE_NOTIFY_LOG,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_VOID__UINT,
                                              G_TYPE_NONE, 1,
                                              G_TYPE_UINT);

    log_signals[SIG_CLEARED] = g_signal_new("cleared",
                                            XFCE_TYPE_NOTIFY_LOG,
                                            G_SIGNAL_RUN_LAST,
                                            0,
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__VOID,
                                            G_TYPE_NONE, 0);
}

static void
xfce_notify_log_initable_init(GInitableIface *iface) {
    iface->init = xfce_notify_log_initable_real_init;
}

static void
xfce_notify_log_init(XfceNotifyLog *log) {
    log->sqlite_delete_supports_limit_offset = TRUE;
    log->write_queue = g_queue_new();
}

static gboolean
xfce_notify_log_initable_real_init(GInitable *initable, GCancellable *cancellable, GError **error) {
    XfceNotifyLog *log = XFCE_NOTIFY_LOG(initable);
    gboolean success = TRUE;
    GFile *log_dir = notify_log_dir();

    if (!g_file_query_exists(log_dir, NULL)) {
        if (G_UNLIKELY(!g_file_make_directory_with_parents(log_dir, NULL, error))) {
            success = FALSE;
        }
    } else if (G_UNLIKELY(g_file_query_file_type(log_dir, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY)) {
        if (error != NULL) {
            *error = g_error_new(G_IO_ERROR,
                                 G_IO_ERROR_NOT_DIRECTORY,
                                 _("The notification log directory (%s) is not a directory"),
                                 g_file_peek_path(log_dir));
        }
        success = FALSE;
    }

    if (G_LIKELY(success)) {
        GFile *log_file = g_file_get_child(log_dir, "log.sqlite");
        int rc = sqlite3_open(g_file_peek_path(log_file), &log->db);

        if (G_UNLIKELY(rc != SQLITE_OK)) {
            if (error != NULL) {
                *error = transform_error(log->db, rc, _("Failed to open notification log: %s"));
            }
            success = FALSE;
        } else if (G_UNLIKELY(!ensure_tables(log, error) || !prepare_statements(log, error))) {
            success = FALSE;
        } else {
            sqlite3_db_config(log->db, SQLITE_DBCONFIG_DEFENSIVE, 1, NULL);

            migrate_old_keyfile(log);
        }

        g_object_unref(log_file);
    }

    g_object_unref(log_dir);

    return success;
}

static inline void
xn_sqlite3_finalize(sqlite3_stmt *stmt) {
    if (G_LIKELY(stmt != NULL)) {
        sqlite3_finalize(stmt);
    }
}

static void
xfce_notify_log_finalize(GObject *object) {
    XfceNotifyLog *log = XFCE_NOTIFY_LOG(object);
    gint queue_drain_attempts = 20;

    if (log->write_queue_id != 0) {
        g_source_remove(log->write_queue_id);
        log->write_queue_id = 0;
    }

    while (!g_queue_is_empty(log->write_queue) && queue_drain_attempts > 0) {
        process_write_queue(log);
        --queue_drain_attempts;
    }
    if (!g_queue_is_empty(log->write_queue)) {
        g_critical("Unable to write all queued operations to log before finalizing");
    }
    g_queue_free_full(log->write_queue, (GDestroyNotify)xfce_notify_log_queue_item_free);

    if (log->monitor != NULL) {
        g_object_unref(log->monitor);
    }

    xn_sqlite3_finalize(log->stmt_get);
    xn_sqlite3_finalize(log->stmt_get_timestamp);
    xn_sqlite3_finalize(log->stmt_read);
    xn_sqlite3_finalize(log->stmt_read_with_timestamp);
    xn_sqlite3_finalize(log->stmt_read_unread);
    xn_sqlite3_finalize(log->stmt_read_unread_with_timestamp);
    xn_sqlite3_finalize(log->stmt_has_unreads);
    xn_sqlite3_finalize(log->stmt_count_unreads);
    xn_sqlite3_finalize(log->stmt_count_app_ids);
    xn_sqlite3_finalize(log->stmt_write);
    xn_sqlite3_finalize(log->stmt_mark_read);
    xn_sqlite3_finalize(log->stmt_mark_all_read);
    xn_sqlite3_finalize(log->stmt_delete);
    xn_sqlite3_finalize(log->stmt_delete_before);
    xn_sqlite3_finalize(log->stmt_delete_all);

    if (log->db != NULL) {
        sqlite3_close(log->db);
    }

    G_OBJECT_CLASS(xfce_notify_log_parent_class)->finalize(object);
}

static GError *
transform_error(sqlite3 *db, int errcode, const gchar *message_fmt) {
    return g_error_new(G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       message_fmt,
                       db != NULL ? sqlite3_errmsg(db) : sqlite3_errstr(errcode));

}

static sqlite3_stmt *
prepare_statement(sqlite3 *db, const gchar *sql, GError **error) {
    sqlite3_stmt *stmt = NULL;
    int rc;
    const char *tail = NULL;
    const gchar *error_fmt_fmt = _("Failed to prepare SQL statement: %%s (%s)");

    g_return_val_if_fail(db != NULL, NULL);
    g_return_val_if_fail(sql != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, &tail);
    if (G_UNLIKELY(rc != SQLITE_OK)) {
        if (stmt != NULL) {
            sqlite3_finalize(stmt);
            stmt = NULL;
        }

        if (error != NULL) {
            gchar *fmt = g_strdup_printf(error_fmt_fmt, sql);
            *error = transform_error(db, rc, fmt);
            g_free(fmt);
        }
    } else if (tail != NULL && *tail != '\0') {
        // This means that there were extra characters at the end of the SQL
        // statement that were not parsed.  Since we never intend that to be
        // the case, that's an error.
        if (stmt != NULL) {
            sqlite3_finalize(stmt);
            stmt = NULL;
        }

        if (error != NULL && *error == NULL) {
            gchar *fmt = g_strdup_printf(error_fmt_fmt, sql);
            *error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, fmt, _("trailing characters at end of statement"));
            g_free(fmt);
        }
    }

    return stmt;
}

static gboolean
prepare_statements(XfceNotifyLog *log, GError **error) {
#define COLUMN_NAMES COL_ID ", " \
                     COL_TIMESTAMP ", " \
                     COL_TZ_IDENTIFIER ", " \
                     COL_APP_ID ", " \
                     COL_APP_NAME ", " \
                     COL_ICON_ID ", " \
                     COL_SUMMARY ", " \
                     COL_BODY ", " \
                     COL_ACTIONS ", " \
                     COL_EXPIRE_TIMEOUT ", " \
                     COL_IS_READ
#define PREPARE_CHECKED(dest, sql) G_STMT_START{ \
    dest = prepare_statement(log->db, sql, error); \
    if (G_UNLIKELY(dest == NULL)) { \
        return FALSE; \
    } \
}G_STMT_END

    PREPARE_CHECKED(log->stmt_get, "SELECT " COLUMN_NAMES " FROM " TABLE " WHERE " COL_ID " = :" COL_ID);

    PREPARE_CHECKED(log->stmt_get_timestamp, "SELECT timestamp FROM " TABLE " WHERE " COL_ID " = :" COL_ID);

    PREPARE_CHECKED(log->stmt_read, "SELECT " COLUMN_NAMES " FROM " TABLE " ORDER BY " COL_TIMESTAMP " DESC LIMIT :" BIND_LIMIT);

    PREPARE_CHECKED(log->stmt_read_with_timestamp, "SELECT " COLUMN_NAMES " FROM " TABLE " WHERE " COL_TIMESTAMP " < :" COL_TIMESTAMP " ORDER BY " COL_TIMESTAMP " DESC LIMIT :" BIND_LIMIT);

    PREPARE_CHECKED(log->stmt_read_unread, "SELECT " COLUMN_NAMES " FROM " TABLE " WHERE " COL_IS_READ " = FALSE ORDER BY " COL_TIMESTAMP " DESC LIMIT :" BIND_LIMIT);

    PREPARE_CHECKED(log->stmt_read_unread_with_timestamp, "SELECT " COLUMN_NAMES " FROM " TABLE " WHERE " COL_IS_READ " = FALSE AND " COL_TIMESTAMP " < :" COL_TIMESTAMP " ORDER BY " COL_TIMESTAMP " DESC LIMIT :" BIND_LIMIT);

    PREPARE_CHECKED(log->stmt_has_unreads, "SELECT COUNT(id) FROM " TABLE " WHERE " COL_IS_READ " = FALSE LIMIT 1");

    PREPARE_CHECKED(log->stmt_count_unreads, "SELECT COUNT(id) FROM " TABLE " WHERE " COL_IS_READ " = FALSE");

    PREPARE_CHECKED(log->stmt_count_app_ids, "SELECT " COL_APP_ID ", count(" COL_APP_ID ") FROM " TABLE " GROUP BY " COL_APP_ID);

    PREPARE_CHECKED(log->stmt_write, "INSERT INTO " TABLE " (" COLUMN_NAMES ") VALUES ("
                                     ":" COL_ID ", "
                                     ":" COL_TIMESTAMP ", "
                                     ":" COL_TZ_IDENTIFIER ", "
                                     ":" COL_APP_ID ", "
                                     ":" COL_APP_NAME ", "
                                     ":" COL_ICON_ID ", "
                                     ":" COL_SUMMARY ", "
                                     ":" COL_BODY ", "
                                     ":" COL_ACTIONS ", "
                                     ":" COL_EXPIRE_TIMEOUT ", "
                                     ":" COL_IS_READ ")");

    PREPARE_CHECKED(log->stmt_mark_read, "UPDATE " TABLE " SET " COL_IS_READ " = TRUE WHERE " COL_ID " = :" COL_ID);

    PREPARE_CHECKED(log->stmt_mark_all_read, "UPDATE " TABLE " SET " COL_IS_READ " = TRUE");

    PREPARE_CHECKED(log->stmt_delete, "DELETE FROM " TABLE " WHERE " COL_ID " = :" COL_ID);

    PREPARE_CHECKED(log->stmt_delete_before, "DELETE FROM " TABLE " WHERE " COL_TIMESTAMP " < :" COL_TIMESTAMP);

    PREPARE_CHECKED(log->stmt_delete_all, "DELETE FROM " TABLE);

    return TRUE;
#undef COLUMN_NAMES
#undef PREPARE_CHECKED
}

static gboolean
stmt_run_oneshot(XfceNotifyLog *log, const gchar *sql, const gchar *error_message_fmt, GError **error) {
    gboolean success = FALSE;
    sqlite3_stmt *stmt;

    stmt = prepare_statement(log->db, sql, error);
    if (G_LIKELY(stmt != NULL)) {
        int rc = sqlite3_step(stmt);
        if (G_LIKELY(rc == SQLITE_DONE)) {
            success = TRUE;
        } else {
            if (error != NULL) {
                *error = transform_error(log->db, rc, error_message_fmt);
            }
        }

        sqlite3_finalize(stmt);
    }

    return success;
}

static gboolean
ensure_tables(XfceNotifyLog *log, GError **error) {
    return stmt_run_oneshot(log, SCHEMA, _("Failed to create 'notifications' table: %s"), error)
        && stmt_run_oneshot(log, INDEX_TIMESTAMP, _("Failed to create DB timestamp index: %s"), error)
        && stmt_run_oneshot(log, INDEX_IS_READ, _("Failed to create DB is_read index: %s"), error);
}

static GFile *
notify_log_dir(void) {
    gchar *path = g_strconcat(g_get_user_cache_dir(), G_DIR_SEPARATOR_S,
                              "xfce4", G_DIR_SEPARATOR_S,
                              "notifyd",
                              NULL);
    GFile *file = g_file_new_for_path(path);
    g_free(path);
    return file;
}

XfceNotifyLog *
xfce_notify_log_open(GError **error) {
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    return g_initable_new(XFCE_TYPE_NOTIFY_LOG, NULL, error, NULL);
}

static GList *
stmt_parse_actions(sqlite3_stmt *stmt, int col_num) {
    const char *actions_blob = sqlite3_column_blob(stmt, col_num);
    int actions_blob_len = sqlite3_column_bytes(stmt, col_num);

    if (actions_blob == NULL || actions_blob_len <= 0) {
        return NULL;
    } else {
        GList *actions = NULL;
        const char *cur = actions_blob;
        const char *actions_blob_end = actions_blob + actions_blob_len;

        while (cur < actions_blob_end) {
            const char *id_end = memchr(cur, '\0', actions_blob_end - cur);

            if (id_end != NULL && G_LIKELY(id_end < actions_blob_end)) {
                const char *label_end = memchr(id_end + 1, '\0', actions_blob_end - id_end - 1);

                if (G_LIKELY(label_end != NULL)) {
                    XfceNotifyLogEntryAction *action = g_new0(XfceNotifyLogEntryAction, 1);
                    action->id = g_strndup(cur, id_end - cur);
                    action->label = g_strndup(id_end + 1, label_end - id_end - 1);
                    actions = g_list_prepend(actions, action);

                    cur = label_end + 1;
                } else {
                    g_warning("Malformed actions blob in DB: no NUL byte for label end");
                    break;
                }
            } else {
                g_warning("Malformed actions blob in DB: blob truncated looking for ID end");
                break;
            }
        }

        return g_list_reverse(actions);
    }
}

static XfceNotifyLogEntry *
stmt_get_log_entry(sqlite3_stmt *stmt, GTimeZone *default_tz) {
    XfceNotifyLogEntry *entry = xfce_notify_log_entry_new_empty();
    gint64 timestamp_utc;
    const unsigned char *tz_identifier;
    GTimeZone *tz = NULL;
    GDateTime *dt_utc_no_us, *dt_utc;
    GHashTable *column_names = g_hash_table_new(g_str_hash, g_str_equal);

    // this is dumb
    for (guint i = 0; ; ++i) {
        const char *name = sqlite3_column_name(stmt, i);
        if (name != NULL) {
            g_hash_table_insert(column_names, (gpointer)name, GUINT_TO_POINTER(i));
        } else {
            break;
        }
    }

#define COL_INDEX(name) GPOINTER_TO_UINT(g_hash_table_lookup(column_names, name))

    timestamp_utc = sqlite3_column_int64(stmt, COL_INDEX(COL_TIMESTAMP));
    dt_utc_no_us = g_date_time_new_from_unix_utc(timestamp_utc / 1000000);
    dt_utc = g_date_time_add(dt_utc_no_us, timestamp_utc % 1000000);
    tz_identifier = sqlite3_column_text(stmt, COL_INDEX(COL_TZ_IDENTIFIER));
    if (G_LIKELY(tz_identifier != NULL)) {
        tz = g_time_zone_new_identifier((const gchar *)tz_identifier);
    }
    if (G_UNLIKELY(tz == NULL)) {
        tz = g_time_zone_ref(default_tz);
    }

    entry->id = g_strdup((const gchar *)sqlite3_column_text(stmt, COL_INDEX(COL_ID)));
    entry->timestamp = g_date_time_to_timezone(dt_utc, tz);
    entry->app_id = g_strdup((const gchar *)sqlite3_column_text(stmt, COL_INDEX(COL_APP_ID)));
    entry->app_name = g_strdup((const gchar *)sqlite3_column_text(stmt, COL_INDEX(COL_APP_NAME)));
    entry->icon_id = g_strdup((const gchar *)sqlite3_column_text(stmt, COL_INDEX(COL_ICON_ID)));
    entry->summary = g_strdup((const gchar *)sqlite3_column_text(stmt, COL_INDEX(COL_SUMMARY)));
    entry->body = g_strdup((const gchar *)sqlite3_column_text(stmt, COL_INDEX(COL_BODY)));
    entry->actions = stmt_parse_actions(stmt, COL_INDEX(COL_ACTIONS));
    entry->expire_timeout = sqlite3_column_int(stmt, COL_INDEX(COL_EXPIRE_TIMEOUT));
    entry->is_read = sqlite3_column_int(stmt, COL_INDEX(COL_IS_READ)) != 0 ? TRUE : FALSE;

    if (G_UNLIKELY(entry->id == NULL || entry->timestamp == NULL)) {
        xfce_notify_log_entry_unref(entry);
        entry = NULL;
    }

    g_date_time_unref(dt_utc_no_us);
    g_date_time_unref(dt_utc);
    g_time_zone_unref(tz);
    g_hash_table_destroy(column_names);

    return entry;

#undef COL_INDEX
}

XfceNotifyLogEntry *
xfce_notify_log_get(XfceNotifyLog *log, const gchar *id) {
    XfceNotifyLogEntry *entry = NULL;
    int rc;

    g_return_val_if_fail(XFCE_IS_NOTIFY_LOG(log), NULL);
    g_return_val_if_fail(id != NULL, NULL);

    rc = sqlite3_bind_text(log->stmt_get,
                           BIND_INDEX(log->stmt_get, COL_ID),
                           g_strdup(id),
                           -1,
                           g_free);

    if (G_LIKELY(rc == SQLITE_OK)) {
        rc = sqlite3_step(log->stmt_get);
    }
    if (rc == SQLITE_ROW) {
        GTimeZone *default_tz = g_time_zone_new_local();
        entry = stmt_get_log_entry(log->stmt_get, default_tz);
        g_time_zone_unref(default_tz);
    }

    sqlite3_reset(log->stmt_get);
    sqlite3_clear_bindings(log->stmt_get);

    return entry;
}

static GList *
xfce_notify_log_read_internal(XfceNotifyLog *log, const gchar *start_after_id, gboolean only_unread, guint count) {
    GList *entries = NULL;
    sqlite3_stmt *stmt;
    int rc  = SQLITE_OK;

    g_return_val_if_fail(XFCE_IS_NOTIFY_LOG(log), NULL);
    g_return_val_if_fail(count > 0, NULL);

    if (start_after_id == NULL) {
        stmt = only_unread ? log->stmt_read_unread : log->stmt_read;
    } else {
        gint64 start_after_timestamp = G_MININT64;

        rc = sqlite3_bind_text(log->stmt_get_timestamp,
                               BIND_INDEX(log->stmt_get_timestamp, COL_ID),
                               g_strdup(start_after_id),
                               -1,
                               g_free);
        if (G_LIKELY(rc == SQLITE_OK)) {
            rc = sqlite3_step(log->stmt_get_timestamp);
            if (rc == SQLITE_ROW) {
                start_after_timestamp = sqlite3_column_int64(log->stmt_get_timestamp, 0);
                rc = SQLITE_OK;
            }
        }

        sqlite3_reset(log->stmt_get_timestamp);
        sqlite3_clear_bindings(log->stmt_get_timestamp);

        stmt = only_unread ? log->stmt_read_unread_with_timestamp : log->stmt_read_with_timestamp;
        if (rc == SQLITE_OK) {
            rc = sqlite3_bind_int64(stmt, BIND_INDEX(stmt, COL_TIMESTAMP), start_after_timestamp);
        }
    }

    if (G_LIKELY(rc == SQLITE_OK)) {
        rc = sqlite3_bind_int(stmt,
                              BIND_INDEX(stmt, BIND_LIMIT),
                              count);
    }

    if (G_LIKELY(rc == SQLITE_OK)) {
        GTimeZone *default_tz = g_time_zone_new_local();

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            XfceNotifyLogEntry *entry = stmt_get_log_entry(stmt, default_tz);
            if (entry != NULL) {
                entries = g_list_prepend(entries, entry);
            }
        }
        entries = g_list_reverse(entries);

        g_time_zone_unref(default_tz);
    }

    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to fetch entries: %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    return entries;
}

GList *
xfce_notify_log_read(XfceNotifyLog *log, const gchar *start_after_id, guint count) {
    return xfce_notify_log_read_internal(log, start_after_id, FALSE, count);
}

GList *
xfce_notify_log_read_unread(XfceNotifyLog *log, const gchar *start_after_id, guint count) {
    return xfce_notify_log_read_internal(log, start_after_id, TRUE, count);
}

gboolean
xfce_notify_log_has_unread_messages(XfceNotifyLog *log) {
    guint count = 0;
    int rc;

    g_return_val_if_fail(XFCE_IS_NOTIFY_LOG(log), FALSE);

    rc = sqlite3_step(log->stmt_has_unreads);
    if (G_LIKELY(rc == SQLITE_ROW)) {
        count = sqlite3_column_int(log->stmt_has_unreads, 0);
    } else {
        g_warning("Failed to get unread count: %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_has_unreads);

    return count > 0;
}

guint
xfce_notify_log_count_unread_messages(XfceNotifyLog *log) {
    guint count = 0;
    int rc;

    g_return_val_if_fail(XFCE_IS_NOTIFY_LOG(log), 0);

    rc = sqlite3_step(log->stmt_count_unreads);
    if (G_LIKELY(rc == SQLITE_ROW)) {
        count = sqlite3_column_int(log->stmt_count_unreads, 0);
    } else {
        g_warning("Failed to get unread count: %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_count_unreads);

    return count;
}

GHashTable *
xfce_notify_log_get_app_id_counts(XfceNotifyLog *log) {
    GHashTable *app_id_counts;
    int rc;

    g_return_val_if_fail(XFCE_IS_NOTIFY_LOG(log), NULL);

    app_id_counts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    while ((rc = sqlite3_step(log->stmt_count_app_ids)) == SQLITE_ROW) {
        const unsigned char *app_id = sqlite3_column_text(log->stmt_count_app_ids, 0);
        guint count = sqlite3_column_int(log->stmt_count_app_ids, 1);
        g_hash_table_insert(app_id_counts, g_strdup(app_id != NULL ? (const gchar *)app_id : ""), GUINT_TO_POINTER(count));
    }

    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to enumerate all app IDs: %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_count_app_ids);

    return app_id_counts;
}

static void *
serialize_actions(GList *actions, guint *bytes) {
    char *data;
    guint len = 0;

    for (GList *l = actions; l != NULL; l = l->next) {
        XfceNotifyLogEntryAction *action = l->data;
        len += (action->id != NULL ? strlen(action->id) : 0) + 1;
        len += (action->label != NULL ? strlen(action->label) : 0) + 1;
    }

    data = g_malloc0(len);
    *bytes = len;

    len = 0;
    for (GList *l = actions; l != NULL; l = l->next) {
        XfceNotifyLogEntryAction *action = l->data;
        if (action->id != NULL) {
            guint n = strlen(action->id);
            memcpy(data + len, action->id, n);
            len += n;
        }
        len += 1;
        if (action->label != NULL) {
            guint n = strlen(action->label);
            memcpy(data + len, action->label, n);
            len += n;
        }
        len += 1;
    }

    return data;
}

static int
xfce_notify_log_real_write(XfceNotifyLog *log, XfceNotifyLogEntry *entry) {
    int rc;
    gint64 timestamp;
    void *actions_blob;
    guint actions_blob_len = 0;

    DBG("writing new entry with id %s", entry->id);

    timestamp = g_date_time_to_unix(entry->timestamp) * 1000000 + g_date_time_get_microsecond(entry->timestamp);
    actions_blob = serialize_actions(entry->actions, &actions_blob_len);

    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_ID), g_strdup(entry->id), -1, g_free);
    sqlite3_bind_int64(log->stmt_write, BIND_INDEX(log->stmt_write, COL_TIMESTAMP), timestamp);
    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_TZ_IDENTIFIER), g_strdup(g_time_zone_get_identifier(g_date_time_get_timezone(entry->timestamp))), -1, g_free);
    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_APP_ID), g_strdup(entry->app_id), -1, g_free);
    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_APP_NAME), g_strdup(entry->app_name), -1, g_free);
    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_ICON_ID), g_strdup(entry->icon_id), -1, g_free);
    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_SUMMARY), g_strdup(entry->summary), -1, g_free);
    sqlite3_bind_text(log->stmt_write, BIND_INDEX(log->stmt_write, COL_BODY), g_strdup(entry->body), -1, g_free);
    sqlite3_bind_blob(log->stmt_write, BIND_INDEX(log->stmt_write, COL_ACTIONS), actions_blob, actions_blob_len, g_free);
    sqlite3_bind_int(log->stmt_write, BIND_INDEX(log->stmt_write, COL_EXPIRE_TIMEOUT), entry->expire_timeout);
    sqlite3_bind_int(log->stmt_write, BIND_INDEX(log->stmt_write, COL_IS_READ), entry->is_read ? 1 : 0);

    rc = sqlite3_step(log->stmt_write);
    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to write new log entry to DB: %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_write);
    sqlite3_clear_bindings(log->stmt_write);

    return rc;
}

void
xfce_notify_log_write(XfceNotifyLog *log, XfceNotifyLogEntry *entry) {
    XfceNotifyLogQueueItem *item;

    g_return_if_fail(XFCE_IS_NOTIFY_LOG(log));
    g_return_if_fail(entry != NULL);

    if (entry->id == NULL) {
        entry->id = g_uuid_string_random();
    }

    item = xfce_notify_log_queue_item_new(XFCE_NOTIFY_QUEUE_ITEM_WRITE);
    item->param.entry = xfce_notify_log_entry_ref(entry);
    queue_write(log, item);
}

static int
xfce_notify_log_real_mark_read(XfceNotifyLog *log, const gchar *id) {
    int rc;

    rc = sqlite3_bind_text(log->stmt_mark_read,
                           BIND_INDEX(log->stmt_mark_read, COL_ID),
                           g_strdup(id),
                           -1,
                           g_free);
    if (G_LIKELY(rc == SQLITE_OK)) {
        rc = sqlite3_step(log->stmt_mark_read);
    }

    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to mark log entry %s read: %s", id, sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_mark_read);
    sqlite3_clear_bindings(log->stmt_mark_read);

    return rc;
}

void
xfce_notify_log_mark_read(XfceNotifyLog *log, const gchar *id) {
    XfceNotifyLogQueueItem *item;

    g_return_if_fail(XFCE_IS_NOTIFY_LOG(log));
    g_return_if_fail(id != NULL && id[0] != '\0');

    item = xfce_notify_log_queue_item_new(XFCE_NOTIFY_QUEUE_ITEM_MARK_READ);
    item->param.id = g_strdup(id);
    queue_write(log, item);
}

static int
xfce_notify_log_real_mark_all_read(XfceNotifyLog *log) {
    int rc;

    rc = sqlite3_step(log->stmt_mark_all_read);
    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to mark all log entries read: %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_mark_all_read);

    return rc;
}

void
xfce_notify_log_mark_all_read(XfceNotifyLog *log) {
    XfceNotifyLogQueueItem *item;

    g_return_if_fail(XFCE_IS_NOTIFY_LOG(log));

    item = xfce_notify_log_queue_item_new(XFCE_NOTIFY_QUEUE_ITEM_MARK_READ);
    queue_write(log, item);
}

static int
xfce_notify_log_real_delete(XfceNotifyLog *log, const gchar *id) {
    int rc;

    rc = sqlite3_bind_text(log->stmt_delete,
                           BIND_INDEX(log->stmt_delete, COL_ID),
                           g_strdup(id),
                           -1,
                           g_free);
    if (G_LIKELY(rc == SQLITE_OK)) {
        rc = sqlite3_step(log->stmt_delete);
    }

    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to delete log entry ID %s: %s", id, sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_delete);
    sqlite3_clear_bindings(log->stmt_delete);

    return rc;
}

void
xfce_notify_log_delete(XfceNotifyLog *log, const gchar *id) {
    XfceNotifyLogQueueItem *item;

    g_return_if_fail(XFCE_IS_NOTIFY_LOG(log));
    g_return_if_fail(id != NULL && id[0] != '\0');

    item = xfce_notify_log_queue_item_new(XFCE_NOTIFY_QUEUE_ITEM_DELETE);
    item->param.id = g_strdup(id);
    queue_write(log, item);
}

static int
xfce_notify_log_real_clear(XfceNotifyLog *log) {
    int rc;

    rc = sqlite3_step(log->stmt_delete_all);
    if (G_UNLIKELY(rc != SQLITE_DONE)) {
        g_warning("Failed to delete log entries %s", sqlite3_errmsg(log->db));
    }

    sqlite3_reset(log->stmt_delete_all);

    return rc;
}

void
xfce_notify_log_clear(XfceNotifyLog *log) {
    XfceNotifyLogQueueItem *item;

    g_return_if_fail(XFCE_IS_NOTIFY_LOG(log));

    item = xfce_notify_log_queue_item_new(XFCE_NOTIFY_QUEUE_ITEM_DELETE);
    queue_write(log, item);
}

static GList *
xfce_notify_g_list_last_length(GList *list, guint *length) {
    guint n = 0;

    while (list != NULL) {
        ++n;
        if (list->next == NULL) {
            *length = n;
            return list;
        }
        list = list->next;
    }

    *length = 0;
    return NULL;
}

static int
xfce_notify_log_real_truncate(XfceNotifyLog *log, guint n_entries_to_keep) {
    int rc;

    if (n_entries_to_keep == 0) {
        rc = xfce_notify_log_real_clear(log);
    } else {
        if (log->sqlite_delete_supports_limit_offset) {
            gchar *sql = g_strdup_printf("DELETE FROM " TABLE " ORDER BY " COL_TIMESTAMP " DESC LIMIT -1 OFFSET %u", n_entries_to_keep);
            struct sqlite3_stmt *stmt = prepare_statement(log->db, sql, NULL);
            if (stmt != NULL) {
                rc = sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            } else {
                g_message("Your sqlite library does not support OFFSET/LIMIT with DELETE; falling back to less-efficient deletion method");
                log->sqlite_delete_supports_limit_offset = FALSE;
            }
        }

        if (!log->sqlite_delete_supports_limit_offset) {
            GList *entries = xfce_notify_log_read(log, NULL, n_entries_to_keep + 1);
            guint n_entries;
            GList *last = xfce_notify_g_list_last_length(entries, &n_entries);

            if (n_entries > n_entries_to_keep) {
                // n_entries guaranteed to be >= 2 here, thus entries != NULL and last != NULL and last->prev != NULL
                XfceNotifyLogEntry *last_entry_to_keep = last->prev->data;
                rc = sqlite3_bind_int64(log->stmt_delete_before,
                                        BIND_INDEX(log->stmt_delete_before, COL_TIMESTAMP),
                                        g_date_time_to_unix(last_entry_to_keep->timestamp) * 1000000 + g_date_time_get_microsecond(last_entry_to_keep->timestamp));
                if (rc == SQLITE_OK) {
                    rc = sqlite3_step(log->stmt_delete_before);
                }

                sqlite3_reset(log->stmt_delete_before);
                sqlite3_clear_bindings(log->stmt_delete_before);
            } else {
                rc = SQLITE_OK;
            }

            g_list_free_full(entries, (GDestroyNotify)xfce_notify_log_entry_unref);
        }
    }

    return rc;
}

void
xfce_notify_log_truncate(XfceNotifyLog *log, guint n_entries_to_keep) {
    XfceNotifyLogQueueItem *item;

    g_return_if_fail(XFCE_IS_NOTIFY_LOG(log));

    item = xfce_notify_log_queue_item_new(XFCE_NOTIFY_QUEUE_ITEM_TRUNCATE);
    item->param.count = n_entries_to_keep;
    queue_write(log, item);
}

static XfceNotifyLogQueueItem *
xfce_notify_log_queue_item_new(XfceNotifyLogQueueItemType item_type) {
    XfceNotifyLogQueueItem *item = g_new0(XfceNotifyLogQueueItem, 1);
    item->type = item_type;
    return item;
}

static void
xfce_notify_log_queue_item_free(XfceNotifyLogQueueItem *item) {
    g_return_if_fail(item != NULL);

    switch (item->type) {
        case XFCE_NOTIFY_QUEUE_ITEM_WRITE:
            xfce_notify_log_entry_unref(item->param.entry);
            break;

        case XFCE_NOTIFY_QUEUE_ITEM_MARK_READ:
        case XFCE_NOTIFY_QUEUE_ITEM_DELETE:
            g_free(item->param.id);
            break;

        case XFCE_NOTIFY_QUEUE_ITEM_TRUNCATE:
            break;

        default:
            g_assert_not_reached();
            break;
    }

    g_free(item);
}

static gboolean
process_write_queue(gpointer data) {
    XfceNotifyLog *log = XFCE_NOTIFY_LOG(data);
    XfceNotifyLogQueueItem *item;
    gboolean ret = G_SOURCE_REMOVE;

    while ((item = g_queue_pop_head(log->write_queue)) != NULL) {
        int rc;
        GValue signal_params[2] = { G_VALUE_INIT, G_VALUE_INIT };
        guint sig_id = 0;

        switch (item->type) {
            case XFCE_NOTIFY_QUEUE_ITEM_WRITE:
                rc = xfce_notify_log_real_write(log, item->param.entry);
                g_value_init(&signal_params[1], G_TYPE_STRING);
                g_value_set_string(&signal_params[1], item->param.entry->id);
                sig_id = log_signals[SIG_ROW_ADDED];
                break;

            case XFCE_NOTIFY_QUEUE_ITEM_MARK_READ:
                if (item->param.id != NULL) {
                    rc = xfce_notify_log_real_mark_read(log, item->param.id);
                    g_value_init(&signal_params[1], G_TYPE_STRING);
                    g_value_set_string(&signal_params[1], item->param.id);
                } else {
                    rc = xfce_notify_log_real_mark_all_read(log);
                }
                sig_id = log_signals[SIG_ROW_CHANGED];
                break;

            case XFCE_NOTIFY_QUEUE_ITEM_DELETE:
                if (item->param.id != NULL) {
                    rc = xfce_notify_log_real_delete(log, item->param.id);
                    g_value_init(&signal_params[1], G_TYPE_STRING);
                    g_value_set_string(&signal_params[1], item->param.id);
                    sig_id = log_signals[SIG_ROW_DELETED];
                } else {
                    rc = xfce_notify_log_real_clear(log);
                    sig_id = log_signals[SIG_CLEARED];
                }
                break;

            case XFCE_NOTIFY_QUEUE_ITEM_TRUNCATE:
                rc = xfce_notify_log_real_truncate(log, item->param.count);
                g_value_init(&signal_params[1], G_TYPE_UINT);
                g_value_set_uint(&signal_params[1], item->param.count);
                sig_id = log_signals[SIG_TRUNCATED];
                break;

            default:
                g_assert_not_reached();
                break;
        }

        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            g_message("Log DB busy/locked; requeueing write");
            g_queue_push_head(log->write_queue, item);
            ret = G_SOURCE_CONTINUE;
            break;
        } else {
            if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
                switch (item->type) {
                    case XFCE_NOTIFY_QUEUE_ITEM_WRITE:
                        g_warning("Failed to write new entry to DB: %s", sqlite3_errstr(rc));
                        break;

                    case XFCE_NOTIFY_QUEUE_ITEM_MARK_READ:
                        g_warning("Failed to mark item(s) read in DB: %s", sqlite3_errstr(rc));
                        break;

                    case XFCE_NOTIFY_QUEUE_ITEM_DELETE:
                        g_warning("Failed to delete item(s) from DB: %s", sqlite3_errstr(rc));
                        break;

                    case XFCE_NOTIFY_QUEUE_ITEM_TRUNCATE:
                        g_warning("Failed to truncate DB: %s", sqlite3_errstr(rc));
                        break;

                    default:
                        g_assert_not_reached();
                        break;
                }
            } else if (sig_id != 0 && sqlite3_changes(log->db) > 0) {
                g_value_init_from_instance(&signal_params[0], log);
                g_signal_emitv(signal_params, sig_id, 0, NULL);
            }

            g_value_unset(&signal_params[0]);
            g_value_unset(&signal_params[1]);
            xfce_notify_log_queue_item_free(item);
        }

    }

    if (ret == G_SOURCE_REMOVE) {
        log->write_queue_id = 0;
    }

    return ret;
}

static void
queue_write(XfceNotifyLog *log, XfceNotifyLogQueueItem *item) {
    g_queue_push_tail(log->write_queue, item);

    if (log->write_queue_id == 0) {
        log->write_queue_id = g_idle_add(process_write_queue, log);
    }
}

static GList *
parse_keyfile_actions(GKeyFile *keyfile, const gchar *group) {
    GList *actions = NULL;

    for (gint i = 0; ; ++i) {
        gchar *action_id_key = g_strdup_printf("action-id-%d", i);
        gchar *action_label_key = g_strdup_printf("action-label-%d", i);
        gchar *action_id = g_key_file_get_string(keyfile, group, action_id_key, NULL);
        gchar *action_label = g_key_file_get_string(keyfile, group, action_label_key, NULL);

        g_free(action_id_key);
        g_free(action_label_key);

        if (action_id != NULL && action_label != NULL) {
            XfceNotifyLogEntryAction *action = g_new0(XfceNotifyLogEntryAction, 1);
            action->id = action_id;
            action->label = action_label;
            actions = g_list_prepend(actions, action);
        } else {
            g_free(action_id);
            g_free(action_label);
            break;
        }
    }

    return g_list_reverse(actions);
}

static gboolean
migrate_old_keyfile(XfceNotifyLog *log) {
    gboolean migrated = FALSE;
    GFile *log_dir = notify_log_dir();
    GFile *log_file = g_file_get_child(log_dir, "log");
    GFile *log_file_migrating = g_file_get_child(log_dir, "log.migrating");
    GError *error = NULL;

    if (g_file_query_exists(log_file, NULL)) {
        if (g_file_move(log_file, log_file_migrating, G_FILE_COPY_NO_FALLBACK_FOR_MOVE, NULL, NULL, NULL, &error)) {
            GKeyFile *keyfile = g_key_file_new();

            if (G_LIKELY(g_key_file_load_from_file(keyfile, g_file_peek_path(log_file_migrating), G_KEY_FILE_NONE, &error))) {
                guint n_entries_migrated = 0;
                guint drain_attempts = 50;
                GTimeZone *default_tz = g_time_zone_new_local();
                gchar **groups = g_key_file_get_groups(keyfile, NULL);

                for (guint i = 0; groups[i] != NULL; ++i, ++n_entries_migrated) {
                    gchar *group = groups[i];
                    XfceNotifyLogEntry *entry = xfce_notify_log_entry_new_empty();
                    entry->timestamp = g_date_time_new_from_iso8601(group, default_tz);
                    entry->app_id = g_key_file_get_string(keyfile, group, "app_name", NULL);
                    entry->icon_id = g_key_file_get_string(keyfile, group, "app_icon", NULL);
                    entry->summary = g_key_file_get_string(keyfile, group, "summary", NULL);
                    entry->body = g_key_file_get_string(keyfile, group, "body", NULL);
                    entry->actions = parse_keyfile_actions(keyfile, group);
                    entry->expire_timeout = g_key_file_get_integer(keyfile, group, "expire-timeout", NULL);
                    entry->is_read = TRUE;

                    xfce_notify_log_write(log, entry);
                    xfce_notify_log_entry_unref(entry);
                }

                if (n_entries_migrated > 0) {
                    // Manually drain the write queue so we can tell immediately if the existing log
                    // was migrated successfully.

                    if (log->write_queue_id != 0) {
                        g_source_remove(log->write_queue_id);
                        log->write_queue_id = 0;
                    }
                    while (drain_attempts > 0 && process_write_queue(log) == G_SOURCE_CONTINUE) {
                        --drain_attempts;
                    }

                    if (process_write_queue(log) == G_SOURCE_CONTINUE) {
                        g_warning("Failed to migrate all old log entries; DB keeps being busy/locked for some reason");
                        log->write_queue_id = g_idle_add(process_write_queue, log);
                    } else {
                        GList *migrated_entries = xfce_notify_log_read(log, NULL, n_entries_migrated);

                        if (g_list_length(migrated_entries) < n_entries_migrated) {
                            g_warning("Failed to migrate some old log entries (expected %d, but only migrated %d)",
                                      n_entries_migrated,
                                      g_list_length(migrated_entries));
                        } else {
                            migrated = TRUE;
                        }

                        g_list_free_full(migrated_entries, (GDestroyNotify)xfce_notify_log_entry_unref);
                    }
                } else {
                    // Old log file existed and was readable, but was empty
                    g_file_delete(log_file_migrating, NULL, NULL);
                }

                g_time_zone_unref(default_tz);
                g_strfreev(groups);
            } else {
                g_warning("Unable to read old log keyfile: %s", error != NULL ? error->message : "unknown error");
                g_clear_error(&error);
            }

            if (G_LIKELY(migrated)) {
                GFile *dest = g_file_get_child(log_dir, "log.old.safe-to-delete");
                if (!g_file_move(log_file_migrating, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
                    g_warning("Failed to move old log out of the way; you may get duplicate log entries next time (%s)", error != NULL ? error->message : "unknown error");
                    g_clear_error(&error);
                }
                g_object_unref(dest);
            } else {
                g_file_move(log_file_migrating, log_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
            }

            g_key_file_unref(keyfile);
        } else {
            if (error != NULL) {
                if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_NOT_FOUND) {
                    g_warning("Failed to move/lock old log file; will not be able to migrate it: %s", error->message);
                }
                g_clear_error(&error);
            } else {
                g_warning("Failed to move/lock old log file; will not be able to migrate it");
            }
        }
    }

    g_object_unref(log_dir);
    g_object_unref(log_file);
    g_object_unref(log_file_migrating);

    return migrated;
}
