/*
 *  Copyright (c) 2017 Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>

#include "sn-backend.h"
#include "sn-item.h"
#include "sn-watcher.h"



static void                  sn_backend_finalize                     (GObject                 *object);

static void                  sn_backend_watcher_bus_acquired         (GDBusConnection         *connection,
                                                                      const gchar             *name,
                                                                      gpointer                 user_data);

static void                  sn_backend_watcher_name_lost            (GDBusConnection         *connection,
                                                                      const gchar             *name,
                                                                      gpointer                 user_data);

static gboolean              sn_backend_watcher_register_item        (SnWatcher               *watcher_skeleton,
                                                                      GDBusMethodInvocation   *invocation,
                                                                      const gchar             *service,
                                                                      SnBackend               *backend);

static gboolean              sn_backend_watcher_register_host        (SnWatcher               *watcher_skeleton,
                                                                      GDBusMethodInvocation   *invocation,
                                                                      const gchar             *service);

static void                  sn_backend_watcher_update_items         (SnBackend               *backend);

static void                  sn_backend_watcher_clear_items          (SnBackend               *backend);

static void                  sn_backend_host_name_appeared           (GDBusConnection         *connection,
                                                                      const gchar             *name,
                                                                      const gchar             *name_owner,
                                                                      gpointer                 user_data);

static void                  sn_backend_host_name_vanished           (GDBusConnection        *connection,
                                                                      const gchar            *name,
                                                                      gpointer                user_data);

static void                  sn_backend_host_item_registered         (SnWatcher               *host_proxy,
                                                                      const gchar             *service,
                                                                      SnBackend               *backend);

static void                  sn_backend_host_item_unregistered       (SnWatcher               *host_proxy,
                                                                      const gchar             *service,
                                                                      SnBackend               *backend);

static void                  sn_backend_host_items_changed           (GDBusProxy              *proxy,
                                                                      GVariant                *changed_properties,
                                                                      GStrv                    invalidated_properties,
                                                                      gpointer                 user_data);

static void                  sn_backend_host_add_item                (SnBackend               *backend,
                                                                      const gchar             *service,
                                                                      const gchar             *bus_name,
                                                                      const gchar             *object_path);

static void                  sn_backend_host_remove_item             (SnBackend               *backend,
                                                                      SnItem                  *item,
                                                                      gboolean                 remove_from_table);

static void                  sn_backend_host_clear_items             (SnBackend               *backend);



struct _SnBackendClass
{
  GObjectClass         __parent__;
};

struct _SnBackend
{
  GObject              __parent__;

  guint                watcher_bus_owner_id;
  SnWatcher           *watcher_skeleton;
  GHashTable          *watcher_items;

  guint                host_bus_watcher_id;
  SnWatcher           *host_proxy;
  GHashTable          *host_items;
  GCancellable        *host_cancellable;
};

G_DEFINE_TYPE (SnBackend, sn_backend, G_TYPE_OBJECT)



enum
{
  ITEM_ADDED,
  ITEM_REMOVED,
  LAST_SIGNAL
};

static guint sn_backend_signals[LAST_SIGNAL] = { 0, };



typedef struct
{
  const gchar         *key;
  SnBackend           *backend;
  GDBusConnection     *connection;
  gulong               handler;
}
ItemConnectionContext;



typedef struct
{
  gint                 index;
  gchar              **out;
}
CollectItemKeysContext;



typedef struct
{
  SnBackend           *backend;
  const gchar *const  *items;
}
RemoveComparingContext;



static void
sn_backend_class_init (SnBackendClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = sn_backend_finalize;

  sn_backend_signals[ITEM_ADDED] =
    g_signal_new (g_intern_static_string ("item-added"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, XFCE_TYPE_SN_ITEM);

  sn_backend_signals[ITEM_REMOVED] =
    g_signal_new (g_intern_static_string ("item-removed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, XFCE_TYPE_SN_ITEM);
}



static void
sn_backend_init (SnBackend *backend)
{
  backend->watcher_bus_owner_id = 0;
  backend->watcher_skeleton = NULL;
  backend->watcher_items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  backend->host_bus_watcher_id = 0;
  backend->host_proxy = NULL;
  backend->host_items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  backend->host_cancellable = g_cancellable_new ();
}



static void
sn_backend_finalize (GObject *object)
{
  SnBackend *backend = XFCE_SN_BACKEND (object);

  g_object_unref (backend->host_cancellable);

  sn_backend_host_clear_items (backend);
  sn_backend_watcher_clear_items (backend);
  g_hash_table_destroy (backend->host_items);
  g_hash_table_destroy (backend->watcher_items);

  if (backend->host_proxy != NULL)
    g_object_unref (backend->host_proxy);

  if (backend->watcher_skeleton != NULL)
    g_object_unref (backend->watcher_skeleton);

  if (backend->host_bus_watcher_id != 0)
    g_bus_unwatch_name (backend->host_bus_watcher_id);

  if (backend->watcher_bus_owner_id != 0)
    g_bus_unown_name (backend->watcher_bus_owner_id);

  G_OBJECT_CLASS (sn_backend_parent_class)->finalize (object);
}



SnBackend *
sn_backend_new (void)
{
  return g_object_new (XFCE_TYPE_SN_BACKEND, NULL);
}



void
sn_backend_start (SnBackend *backend)
{
  g_return_if_fail (XFCE_IS_SN_BACKEND (backend));
  g_return_if_fail (backend->watcher_bus_owner_id == 0);
  g_return_if_fail (backend->host_bus_watcher_id == 0);

  backend->watcher_bus_owner_id =
    g_bus_own_name (G_BUS_TYPE_SESSION,
                    "org.kde.StatusNotifierWatcher",
                    G_BUS_NAME_OWNER_FLAGS_NONE,
                    sn_backend_watcher_bus_acquired,
                    NULL, sn_backend_watcher_name_lost,
                    backend, NULL);

  backend->host_bus_watcher_id =
    g_bus_watch_name (G_BUS_TYPE_SESSION,
                      "org.kde.StatusNotifierWatcher",
                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                      sn_backend_host_name_appeared,
                      sn_backend_host_name_vanished,
                      backend, NULL);
}



static void
sn_backend_watcher_bus_acquired (GDBusConnection *connection,
                                 const gchar     *name,
                                 gpointer         user_data)
{
  SnBackend *backend = user_data;
  GError    *error = NULL;

  if (backend->watcher_skeleton != NULL)
    g_object_unref (backend->watcher_skeleton);

  backend->watcher_skeleton = sn_watcher_skeleton_new ();

  sn_watcher_set_is_status_notifier_host_registered (backend->watcher_skeleton, TRUE);
  sn_watcher_set_registered_status_notifier_items (backend->watcher_skeleton, NULL);
  sn_watcher_set_protocol_version (backend->watcher_skeleton, 0);

  g_signal_connect (backend->watcher_skeleton, "handle-register-status-notifier-item",
                    G_CALLBACK (sn_backend_watcher_register_item), backend);
  g_signal_connect (backend->watcher_skeleton, "handle-register-status-notifier-host",
                    G_CALLBACK (sn_backend_watcher_register_host), backend);

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (backend->watcher_skeleton),
                                    connection, "/StatusNotifierWatcher", &error);

  if (error != NULL)
    {
      g_error_free (error);

      g_object_unref (backend->watcher_skeleton);
      backend->watcher_skeleton = NULL;
    }
}



static void
sn_backend_watcher_name_lost (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer         user_data)
{
  SnBackend *backend = user_data;

  sn_backend_watcher_clear_items (backend);

  if (backend->watcher_skeleton != NULL)
    sn_watcher_set_registered_status_notifier_items (backend->watcher_skeleton, NULL);
}



static void
sn_backend_watcher_name_owner_changed (GDBusConnection *connection,
                                       const gchar     *sender_name,
                                       const gchar     *object_path,
                                       const gchar     *interface_name,
                                       const gchar     *signal_name,
                                       GVariant        *parameters,
                                       gpointer         user_data)
{
  ItemConnectionContext *context = user_data;
  SnBackend             *backend = context->backend;
  gchar                 *key;
  gchar                 *new_owner;

  g_variant_get (parameters, "(sss)", NULL, NULL, &new_owner);
  if (new_owner == NULL || strlen (new_owner) == 0)
    {
      key = g_strdup (context->key);
      g_dbus_connection_signal_unsubscribe (context->connection, context->handler);
      /* context and context->key will be freed after this call */
      g_hash_table_remove (backend->watcher_items, key);
      sn_backend_watcher_update_items (backend);
      sn_watcher_emit_status_notifier_item_unregistered (backend->watcher_skeleton, key);
      g_free (key);
    }

  g_free (new_owner);
}



static gboolean
sn_backend_watcher_register_item (SnWatcher             *watcher_skeleton,
                                  GDBusMethodInvocation *invocation,
                                  const gchar           *service,
                                  SnBackend             *backend)
{
  const gchar           *bus_name;
  const gchar           *object_path;
  const gchar           *sender;
  gchar                 *key;
  GDBusConnection       *connection;
  ItemConnectionContext *context;

  sender = g_dbus_method_invocation_get_sender (invocation);

  if (service[0] == '/')
    {
      /* /org/ayatana/NotificationItem */
      bus_name = sender;
      object_path = service;
    }
  else
    {
      /* org.kde.StatusNotifierItem */
      bus_name = service;
      object_path = "/StatusNotifierItem";
    }

  if (!g_dbus_is_name (bus_name))
    {
      g_dbus_method_invocation_return_error_literal (invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_INVALID_ARGUMENT,
                                                     "Invalid bus name");
      return FALSE;
    }

  key = g_strdup_printf ("%s%s", bus_name, object_path);
  connection = g_dbus_interface_skeleton_get_connection (G_DBUS_INTERFACE_SKELETON (watcher_skeleton));

  context = g_hash_table_lookup (backend->watcher_items, key);
  if (context != NULL)
    {
      g_dbus_connection_signal_unsubscribe (connection, context->handler);
      g_hash_table_remove (backend->watcher_items, key);
    }

  context = g_new0 (ItemConnectionContext, 1);
  context->key = key;
  context->backend = backend;
  context->connection = connection;
  context->handler =
    g_dbus_connection_signal_subscribe (connection,
                                        "org.freedesktop.DBus",
                                        "org.freedesktop.DBus",
                                        "NameOwnerChanged",
                                        "/org/freedesktop/DBus",
                                        bus_name,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        sn_backend_watcher_name_owner_changed,
                                        context, NULL);

  g_hash_table_insert (backend->watcher_items, key, context);

  sn_backend_watcher_update_items (backend);

  sn_watcher_complete_register_status_notifier_item (watcher_skeleton, invocation);

  sn_watcher_emit_status_notifier_item_registered (watcher_skeleton, key);

  return TRUE;
}



static gboolean
sn_backend_watcher_register_host (SnWatcher             *watcher_skeleton,
                                  GDBusMethodInvocation *invocation,
                                  const gchar           *service)
{
  sn_watcher_complete_register_status_notifier_host (watcher_skeleton, invocation);

  return TRUE;
}



static void
sn_backend_watcher_collect_item_keys (gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
  CollectItemKeysContext *context = user_data;
  context->out[context->index++] = key;
}



static void
sn_backend_watcher_update_items (SnBackend *backend)
{
  CollectItemKeysContext context;

  if (backend->watcher_skeleton != NULL)
    {
      context.index = 0;
      context.out = g_new0 (gchar *, g_hash_table_size (backend->watcher_items) + 1);
      g_hash_table_foreach (backend->watcher_items,
                            sn_backend_watcher_collect_item_keys,
                            &context);
      sn_watcher_set_registered_status_notifier_items (backend->watcher_skeleton,
                                                       (gpointer)context.out);
      g_free (context.out);
    }
}



static gboolean
sn_backend_watcher_clear_item (gpointer key,
                               gpointer value,
                               gpointer user_data)
{
  ItemConnectionContext *context = value;

  g_dbus_connection_signal_unsubscribe (context->connection, context->handler);

  return TRUE;
}



static void
sn_backend_watcher_clear_items (SnBackend *backend)
{
  g_hash_table_foreach_remove (backend->watcher_items, sn_backend_watcher_clear_item, NULL);
}



static gboolean
sn_backend_host_parse_name_path (const gchar  *service,
                                 gchar       **bus_name,
                                 gchar       **object_path)
{
  const gchar *substring;
  gchar       *bus_name_val;
  gint         index;

  substring = strstr (service, "/");

  if (substring != NULL)
    {
      index = (gint) (substring - service);
      bus_name_val = g_strndup (service, index);

      if (!g_dbus_is_name (bus_name_val))
        {
          g_free (bus_name_val);
          return FALSE;
        }

      *bus_name = bus_name_val;
      *object_path = g_strdup (&service[index]);
      return TRUE;
    }

  return FALSE;
}



static void
sn_backend_host_callback (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  SnBackend          *backend = user_data;
  gchar              *bus_name;
  gchar              *object_path;
  const gchar *const *items;
  gint                i;

  backend->host_proxy = sn_watcher_proxy_new_finish (res, NULL);

  if (backend->host_proxy != NULL)
    {
      g_signal_connect (backend->host_proxy, "status-notifier-item-registered",
                        G_CALLBACK (sn_backend_host_item_registered), backend);
      g_signal_connect (backend->host_proxy, "status-notifier-item-unregistered",
                        G_CALLBACK (sn_backend_host_item_unregistered), backend);
      g_signal_connect_after (backend->host_proxy, "g-properties-changed",
                              G_CALLBACK (sn_backend_host_items_changed), backend);

      items = sn_watcher_get_registered_status_notifier_items (backend->host_proxy);
      if (items != NULL)
        {
          for (i = 0; items[i] != NULL; i++)
            {
              if (sn_backend_host_parse_name_path (items[i], &bus_name, &object_path))
                {
                  sn_backend_host_add_item (backend, items[i], bus_name, object_path);

                  g_free (bus_name);
                  g_free (object_path);
                }
            }
        }
    }
}



static void
sn_backend_host_name_appeared (GDBusConnection *connection,
                               const gchar     *name,
                               const gchar     *name_owner,
                               gpointer         user_data)
{
  SnBackend *backend = user_data;

  sn_watcher_proxy_new (connection,
                        G_DBUS_PROXY_FLAGS_NONE,
                        name, "/StatusNotifierWatcher",
                        backend->host_cancellable,
                        sn_backend_host_callback,
                        backend);
}



static void
sn_backend_host_name_vanished (GDBusConnection *connection,
                               const gchar     *name,
                               gpointer         user_data)
{
  SnBackend *backend = user_data;

  if (backend->host_proxy != NULL)
    {
      g_object_unref (backend->host_proxy);
      backend->host_proxy = NULL;
    }

  sn_backend_host_clear_items (backend);
}



static void
sn_backend_host_item_expose (SnItem    *item,
                             SnBackend *backend)
{
  g_signal_emit (G_OBJECT (backend), sn_backend_signals[ITEM_ADDED], 0, item);
}



static void
sn_backend_host_item_seal (SnItem    *item,
                           SnBackend *backend)
{
  g_signal_emit (G_OBJECT (backend), sn_backend_signals[ITEM_REMOVED], 0, item);
}



static void
sn_backend_host_item_finish (SnItem    *item,
                             SnBackend *backend)
{
  sn_backend_host_remove_item (backend, item, TRUE);
}



static void
sn_backend_host_item_registered (SnWatcher   *host_proxy,
                                 const gchar *service,
                                 SnBackend   *backend)
{
  gchar *bus_name;
  gchar *object_path;

  if (!sn_backend_host_parse_name_path (service, &bus_name, &object_path))
    return;

  sn_backend_host_add_item (backend, service, bus_name, object_path);

  g_free (bus_name);
  g_free (object_path);
}



static void
sn_backend_host_item_unregistered (SnWatcher   *host_proxy,
                                   const gchar *service,
                                   SnBackend   *backend)
{
  SnItem *item;

  item = g_hash_table_lookup (backend->host_items, service);
  if (item != NULL)
    sn_backend_host_remove_item (backend, item, TRUE);
}



static gboolean
sn_backend_host_items_changed_remove_item (gpointer key,
                                           gpointer value,
                                           gpointer user_data)
{
  RemoveComparingContext *context = user_data;
  SnItem                 *item = value;
  gint                    i;

  for (i = 0; context->items[i] != NULL; i++)
    {
      if (!g_strcmp0 (key, context->items[i]))
        return FALSE;
    }

  sn_backend_host_remove_item (context->backend, item, FALSE);

  return TRUE;
}



static void
sn_backend_host_items_changed (GDBusProxy *proxy,
                               GVariant   *changed_properties,
                               GStrv       invalidated_properties,
                               gpointer    user_data)
{
  SnBackend              *backend = user_data;
  const gchar *const     *items;
  gchar                  *bus_name;
  gchar                  *object_path;
  gint                    i;
  RemoveComparingContext  context;

  items = sn_watcher_get_registered_status_notifier_items (backend->host_proxy);
  if (items != NULL)
    {
      for (i = 0; items[i] != NULL; i++)
        {
          /* add new items */
          if (!g_hash_table_contains (backend->host_items, items[i]))
            {
              if (sn_backend_host_parse_name_path (items[i], &bus_name, &object_path))
                {
                  sn_backend_host_add_item (backend, items[i], bus_name, object_path);

                  g_free (bus_name);
                  g_free (object_path);
                }
            }
        }

      /* remove old items */
      context.backend = backend;
      context.items = items;
      g_hash_table_foreach_remove (backend->host_items,
                                   sn_backend_host_items_changed_remove_item,
                                   &context);
    }
  else
    {
      sn_backend_host_clear_items (backend);
    }
}



static void
sn_backend_host_add_item (SnBackend   *backend,
                          const gchar *service,
                          const gchar *bus_name,
                          const gchar *object_path)
{
  SnItem *item;

  item = g_hash_table_lookup (backend->host_items, service);
  if (item != NULL)
    {
      sn_item_invalidate (item, TRUE);
    }
  else
    {
      item = g_object_new (XFCE_TYPE_SN_ITEM,
                           "bus-name", bus_name,
                           "object-path", object_path,
                           "key", service,
                           NULL);
      g_signal_connect (item, "expose",
                        G_CALLBACK (sn_backend_host_item_expose), backend);
      g_signal_connect (item, "seal",
                        G_CALLBACK (sn_backend_host_item_seal), backend);
      g_signal_connect (item, "finish",
                        G_CALLBACK (sn_backend_host_item_finish), backend);
      sn_item_start (item);
      g_hash_table_insert (backend->host_items, g_strdup (service), item);
    }
}



static void
sn_backend_host_remove_item (SnBackend *backend,
                             SnItem    *item,
                             gboolean   remove_from_table)
{
  gchar   *key;
  gboolean exposed;

  g_object_get (item,
                "key", &key,
                "exposed", &exposed,
                NULL);

  if (exposed)
    g_signal_emit (G_OBJECT (backend), sn_backend_signals[ITEM_REMOVED], 0, item);

  if (remove_from_table)
    g_hash_table_remove (backend->host_items, key);

  g_object_unref (item);

  g_free (key);
}



static gboolean
sn_backend_host_clear_item (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
  SnBackend *backend = user_data;
  SnItem    *item = value;

  sn_backend_host_remove_item (backend, item, FALSE);

  return TRUE;
}



static void
sn_backend_host_clear_items (SnBackend *backend)
{
  g_hash_table_foreach_remove (backend->host_items, sn_backend_host_clear_item, backend);
}
