/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2006 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2004-2006 Mike Hearn <mike@navi.cx>
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright © 2010 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gio.h>

#include "notify.h"
#include "internal.h"
#include "notify-marshal.h"

static gboolean         _initted = FALSE;
static char            *_app_name = NULL;
static char            *_snap_name = NULL;
static char            *_snap_app = NULL;
static char            *_flatpak_app = NULL;
static GDBusProxy      *_proxy = NULL;
static GList           *_active_notifications = NULL;
static int              _spec_version_major = 0;
static int              _spec_version_minor = 0;
static int              _portal_version = 0;

gboolean
_notify_check_spec_version (int major,
                            int minor)
{
       if (_spec_version_major > major)
               return TRUE;
       if (_spec_version_major < major)
               return FALSE;
       return _spec_version_minor >= minor;
}

static gboolean
_notify_get_server_info (char **ret_name,
                         char **ret_vendor,
                         char **ret_version,
                         char **ret_spec_version,
                         GError **error)
{
        GDBusProxy *proxy;
        GVariant   *result;

        proxy = _notify_get_proxy (error);
        if (proxy == NULL) {
                return FALSE;
        }

        if (_notify_uses_portal_notifications ()) {
                if (ret_name) {
                        *ret_name = g_strdup ("Portal Notification");
                }

                if (ret_vendor) {
                        *ret_vendor = g_strdup ("Freedesktop");
                }

                if (ret_version) {
                        *ret_version = g_strdup_printf ("%u", _portal_version);
                }

                if (ret_spec_version) {
                        *ret_spec_version = g_strdup ("1.2");
                }

                return TRUE;
        }

        result = g_dbus_proxy_call_sync (proxy,
                                         "GetServerInformation",
                                         g_variant_new ("()"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1 /* FIXME shorter timeout? */,
                                         NULL,
                                         error);
        if (result == NULL) {
                return FALSE;
        }
        if (!g_variant_is_of_type (result, G_VARIANT_TYPE ("(ssss)"))) {
                g_variant_unref (result);
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                             "Unexpected reply type");
                return FALSE;
        }

        g_variant_get (result, "(ssss)",
                       ret_name,
                       ret_vendor,
                       ret_version,
                       ret_spec_version);
        g_variant_unref (result);
        return TRUE;
}

static gboolean
_notify_update_spec_version (GError **error)
{
       char *spec_version;

       if (!_notify_get_server_info (NULL, NULL, NULL, &spec_version, error)) {
               return FALSE;
       }

       sscanf (spec_version,
               "%d.%d",
               &_spec_version_major,
               &_spec_version_minor);

       g_free (spec_version);

       return TRUE;
}

static gboolean
set_app_name (const char *app_name)
{
        g_return_val_if_fail (app_name != NULL, FALSE);
        g_return_val_if_fail (*app_name != '\0', FALSE);

        g_free (_app_name);
        _app_name = g_strdup (app_name);

        return TRUE;
}


/**
 * notify_set_app_name:
 * @app_name: The name of the application
 *
 * Sets the application name.
 *
 */
void
notify_set_app_name (const char *app_name)
{
        set_app_name (app_name);
}

/**
 * notify_init:
 * @app_name: (nullable): The name of the application initializing libnotify.
 *
 * Initialized libnotify. This must be called before any other functions.
 *
 * Starting from 0.8, if the provided @app_name is %NULL, libnotify will
 * try to figure it out from the running application.
 * Before it was not allowed, and was causing libnotify not to be initialized.
 *
 * Returns: %TRUE if successful, or %FALSE on error.
 */
gboolean
notify_init (const char *app_name)
{
        if (_initted)
                return TRUE;

        if (app_name == NULL) {
                GApplication *application;

                app_name = _notify_get_snap_app ();
                if (app_name == NULL) {
                        app_name = _notify_get_flatpak_app ();
                }

                if (app_name == NULL &&
                    (application = g_application_get_default ())) {
                        app_name = g_application_get_application_id (application);
                }
        }

        if (!set_app_name (app_name)) {
                return FALSE;
        }

        _initted = TRUE;

        return TRUE;
}

static void
_initialize_snap_names (void)
{
        gchar *cgroup_contents = NULL;
        gchar *found_snap_name = NULL;
        gchar **lines;
        gint i;

        if (!g_file_get_contents ("/proc/self/cgroup", &cgroup_contents,
                                  NULL, NULL)) {
                g_free (cgroup_contents);
                return;
        }

        lines = g_strsplit (cgroup_contents, "\n", -1);
        g_free (cgroup_contents);

        for (i = 0; lines[i]; ++i) {
                gchar **parts = g_strsplit (lines[i], ":", 3);
                gchar *basename;
                gchar **ns;
                guint ns_length;

                if (g_strv_length (parts) != 3) {
                        g_strfreev (parts);
                        continue;
                }

                basename = g_path_get_basename (parts[2]);
                g_strfreev (parts);

                if (!basename) {
                        continue;
                }

                ns = g_strsplit (basename, ".", -1);
                ns_length = g_strv_length (ns);
                g_free (basename);

                if (ns_length < 2 || !g_str_equal (ns[0], "snap")) {
                        g_strfreev (ns);
                        continue;
                }

                if (_snap_name == NULL) {
                        g_free (found_snap_name);
                        found_snap_name = g_strdup (ns[1]);
                }

                if (ns_length < 3) {
                        g_strfreev (ns);
                        continue;
                }

                if (_snap_name == NULL) {
                        _snap_name = found_snap_name;
                        found_snap_name = NULL;
                        g_debug ("SNAP name: %s", _snap_name);
                }

                if (g_str_equal (ns[1], _snap_name)) {
                        _snap_app = g_strdup (ns[2]);
                        g_strfreev (ns);
                        break;
                }

                g_strfreev (ns);
        }

        if (_snap_name == NULL && found_snap_name != NULL) {
                _snap_name = found_snap_name;
                found_snap_name = NULL;
                g_debug ("SNAP name: %s", _snap_name);
        }

        if (_snap_app == NULL) {
                _snap_app = g_strdup (_snap_name);
        }

        g_debug ("SNAP app: %s", _snap_app);

        g_strfreev (lines);
        g_free (found_snap_name);
}

const char *
_notify_get_snap_path (void)
{
        static const char *snap_path = NULL;
        static gsize snap_path_set = FALSE;

        if (g_once_init_enter (&snap_path_set)) {
                snap_path = g_getenv ("SNAP");

                if (!snap_path || *snap_path == '\0' ||
                    !strchr (snap_path, G_DIR_SEPARATOR)) {
                        snap_path = NULL;
                } else {
                        g_debug ("SNAP path: %s", snap_path);
                }

                g_once_init_leave (&snap_path_set, TRUE);
        }

        return snap_path;
}

const char *
_notify_get_snap_name (void)
{
        static gsize snap_name_set = FALSE;

        if (g_once_init_enter (&snap_name_set)) {
                if (!_snap_name) {
                        const char *snap_name_env = g_getenv ("SNAP_NAME");

                        if (!snap_name_env || *snap_name_env == '\0')
                                snap_name_env = NULL;

                        _snap_name = g_strdup (snap_name_env);
                        g_debug ("SNAP name: %s", _snap_name);
                }

                g_once_init_leave (&snap_name_set, TRUE);
        }

        return _snap_name;
}

const char *
_notify_get_snap_app (void)
{
        static gsize snap_app_set = FALSE;

        if (g_once_init_enter (&snap_app_set)) {
                _initialize_snap_names ();
                g_once_init_leave (&snap_app_set, TRUE);
        }

        return _snap_app;
}

const char *
_notify_get_flatpak_app (void)
{
        static gsize flatpak_app_set = FALSE;

        if (g_once_init_enter (&flatpak_app_set)) {
                GKeyFile *info = g_key_file_new ();

                if (g_key_file_load_from_file (info, "/.flatpak-info",
                                               G_KEY_FILE_NONE, NULL)) {
                        const char *group = "Application";

                        if (g_key_file_has_group (info, "Runtime")) {
                                group = "Runtime";
                        }

                        _flatpak_app = g_key_file_get_string (info, group,
                                                              "name", NULL);
                }

                g_key_file_free (info);
                g_once_init_leave (&flatpak_app_set, TRUE);
        }

        return _flatpak_app;
}

static gboolean
_notify_is_running_under_flatpak (void)
{
        return !!_notify_get_flatpak_app ();
}

static gboolean
_notify_is_running_under_snap (void)
{
        return !!_notify_get_snap_app ();
}

static gboolean
_notify_is_running_in_sandbox (void)
{
        static gsize use_portal = 0;
        enum {
                IGNORE_PORTAL = 1,
                TRY_USE_PORTAL = 2,
                FORCE_PORTAL = 3
        };

        if (g_once_init_enter (&use_portal)) {
                if (G_UNLIKELY (g_getenv ("NOTIFY_IGNORE_PORTAL"))) {
                        g_once_init_leave (&use_portal, IGNORE_PORTAL);
                } else if (G_UNLIKELY (g_getenv ("NOTIFY_FORCE_PORTAL"))) {
                        g_once_init_leave (&use_portal, FORCE_PORTAL);
                } else {
                        g_once_init_leave (&use_portal, TRY_USE_PORTAL);
                }
        }

        if (use_portal == IGNORE_PORTAL) {
                return FALSE;
        }

        return use_portal == FORCE_PORTAL ||
               _notify_is_running_under_flatpak () ||
               _notify_is_running_under_snap ();
}

gboolean
_notify_uses_portal_notifications (void)
{
        return _portal_version != 0;
}


/**
 * notify_get_app_name:
 *
 * Gets the application name registered.
 *
 * Returns: The registered application name, passed to [func@init].
 */
const char *
notify_get_app_name (void)
{
        return _app_name;
}

/**
 * notify_uninit:
 *
 * Uninitializes libnotify.
 *
 * This should be called when the program no longer needs libnotify for
 * the rest of its lifecycle, typically just before exitting.
 */
void
notify_uninit (void)
{
        GList *l;

        if (!_initted) {
                return;
        }

        if (_app_name != NULL) {
                g_free (_app_name);
                _app_name = NULL;
        }

        for (l = _active_notifications; l != NULL; l = l->next) {
                NotifyNotification *n = NOTIFY_NOTIFICATION (l->data);

                if (_notify_notification_get_timeout (n) == 0 ||
                    _notify_notification_has_nondefault_actions (n)) {
                        notify_notification_close (n, NULL);
                }
        }

        if (_proxy != NULL) {
            g_object_unref (_proxy);
            _proxy = NULL;
        }

        g_free (_snap_name);
        _snap_name = NULL;

        g_free (_snap_app);
        _snap_app = NULL;

        g_free (_flatpak_app);
        _flatpak_app = NULL;

        _initted = FALSE;
}

/**
 * notify_is_initted:
 *
 * Gets whether or not libnotify is initialized.
 *
 * Returns: %TRUE if libnotify is initialized, or %FALSE otherwise.
 */
gboolean
notify_is_initted (void)
{
        return _initted;
}

GDBusProxy *
_get_portal_proxy (GError **error)
{
        GError *local_error = NULL;
        GDBusProxy *proxy;
        GVariant *res;

        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               NOTIFY_PORTAL_DBUS_NAME,
                                               NOTIFY_PORTAL_DBUS_CORE_OBJECT,
                                               NOTIFY_PORTAL_DBUS_CORE_INTERFACE,
                                               NULL,
                                               &local_error);

        if (proxy == NULL) {
                g_debug ("Failed to get portal proxy: %s", local_error->message);
                g_clear_error (&local_error);

                return NULL;
        }

        res = g_dbus_proxy_get_cached_property (proxy, "version");
        if (!res) {
                g_object_unref (proxy);
                return NULL;
        }

        _portal_version = g_variant_get_uint32 (res);
        g_assert (_portal_version > 0);

        g_warning ("Running in confined mode, using Portal notifications. "
                   "Some features and hints won't be supported");

        g_variant_unref (res);

        return proxy;
}

/*
 * _notify_get_proxy:
 * @error: (nullable): a location to store a #GError, or %NULL
 *
 * Synchronously creates the #GDBusProxy for the notification service,
 * and caches the result.
 *
 * Returns: (nullable): the #GDBusProxy for the notification service, or %NULL on error
 */
GDBusProxy *
_notify_get_proxy (GError **error)
{
        if (_proxy != NULL)
                return _proxy;

        if (_notify_is_running_in_sandbox ()) {
                _proxy = _get_portal_proxy (error);

                if (_proxy != NULL) {
                        goto out;
                }
        }

        _proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                NULL,
                                                NOTIFY_DBUS_NAME,
                                                NOTIFY_DBUS_CORE_OBJECT,
                                                NOTIFY_DBUS_CORE_INTERFACE,
                                                NULL,
                                                error);

out:
        if (_proxy == NULL) {
                return NULL;
        }

        if (!_notify_update_spec_version (error)) {
               g_object_unref (_proxy);
               _proxy = NULL;
               return NULL;
        }

        g_object_add_weak_pointer (G_OBJECT (_proxy), (gpointer *) &_proxy);

        return _proxy;
}

/**
 * notify_get_server_caps:
 *
 * Queries the server capabilities.
 *
 * Synchronously queries the server for its capabilities and returns them in a
 * list.
 *
 * Returns: (transfer full) (element-type utf8): a list of server capability strings.
 */
GList *
notify_get_server_caps (void)
{
        GDBusProxy *proxy;
        GVariant   *result;
        char      **cap, **caps;
        GList      *list = NULL;

        proxy = _notify_get_proxy (NULL);
        if (proxy == NULL) {
                g_warning ("Failed to connect to proxy");
                return NULL;
        }

        if (_notify_uses_portal_notifications ()) {
                list = g_list_prepend (list, g_strdup ("actions"));
                list = g_list_prepend (list, g_strdup ("body"));
                list = g_list_prepend (list, g_strdup ("body-images"));
                list = g_list_prepend (list, g_strdup ("icon-static"));

                return list;
        }

        result = g_dbus_proxy_call_sync (proxy,
                                         "GetCapabilities",
                                         g_variant_new ("()"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1 /* FIXME shorter timeout? */,
                                         NULL,
                                         NULL);
        if (result == NULL) {
                return NULL;
        }
        if (!g_variant_is_of_type (result, G_VARIANT_TYPE ("(as)"))) {
                g_variant_unref (result);
                return NULL;
        }

        g_variant_get (result, "(^as)", &caps);

        for (cap = caps; *cap != NULL; cap++) {
                list = g_list_prepend (list, *cap);
        }

        g_free (caps);
        g_variant_unref (result);

        return g_list_reverse (list);
}

/**
 * notify_get_server_info:
 * @ret_name: (out) (optional) (transfer full): a location to store the server name, or %NULL
 * @ret_vendor: (out) (optional) (transfer full): a location to store the server vendor, or %NULL
 * @ret_version: (out) (optional) (transfer full): a location to store the server version, or %NULL
 * @ret_spec_version: (out) (optional) (transfer full): a location to store the version the service is compliant with, or %NULL
 *
 * Queries the server for information.
 *
 * Synchronously queries the server for its information, specifically, the name,
 * vendor, server version, and the version of the notifications specification
 * that it is compliant with.
 *
 * Returns: %TRUE if successful, and the variables passed will be set, %FALSE
 *   on error. The returned strings must be freed with g_free
 */
gboolean
notify_get_server_info (char **ret_name,
                        char **ret_vendor,
                        char **ret_version,
                        char **ret_spec_version)
{
        return _notify_get_server_info (ret_name, ret_vendor, ret_version, ret_spec_version, NULL);
}

void
_notify_cache_add_notification (NotifyNotification *n)
{
        _active_notifications = g_list_prepend (_active_notifications, n);
}

void
_notify_cache_remove_notification (NotifyNotification *n)
{
        _active_notifications = g_list_remove (_active_notifications, n);
}
