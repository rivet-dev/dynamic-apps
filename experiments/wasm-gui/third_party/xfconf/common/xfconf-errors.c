/*
 *  xfconf
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
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

#include <gio/gio.h>

#include "xfconf/xfconf-errors.h"
#include "xfconf-alias.h"
#include "xfconf-common-private.h"

/**
 * SECTION:xfconf-errors
 * @title: Error Reporting
 * @short_description: Xfconf library and daemon error descriptions
 *
 * Both the Xfconf daemon and library provide error information via the use of #GError
 **/


static const GDBusErrorEntry xfconf_daemon_dbus_error_entries[] = 
{
    { XFCONF_ERROR_UNKNOWN, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.Unknown" },
    { XFCONF_ERROR_CHANNEL_NOT_FOUND, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.ChannelNotFound" },
    { XFCONF_ERROR_PROPERTY_NOT_FOUND, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.PropertyNotFound" },
    { XFCONF_ERROR_READ_FAILURE, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.ReadFailure" },
    { XFCONF_ERROR_WRITE_FAILURE, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.WriteFailure" },
    { XFCONF_ERROR_PERMISSION_DENIED, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.PermissionDenied" },
    { XFCONF_ERROR_INTERNAL_ERROR, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.InternalError" },
    { XFCONF_ERROR_NO_BACKEND, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.NoBackend" },
    { XFCONF_ERROR_INVALID_PROPERTY, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.InvalidProperty" },
    { XFCONF_ERROR_INVALID_CHANNEL, XFCONF_SERVICE_NAME_PREFIX ".Xfconf.Error.InvalidChannel" },
};

/**
 * XFCONF_ERROR:
 *
 * The #GError error domain for Xfconf.
 **/



/**
 * XFCONF_TYPE_ERROR:
 *
 * An enum GType for Xfconf errors.
 **/


GQuark
xfconf_get_error_quark(void)
{
    static volatile gsize quark_volatile = 0;
    
    g_dbus_error_register_error_domain ("xfconf_daemon_error",
                                        &quark_volatile,
                                        xfconf_daemon_dbus_error_entries,
                                        G_N_ELEMENTS (xfconf_daemon_dbus_error_entries));
    
    return quark_volatile;
}

/* unfortunately glib-mkenums can't generate types that are compatible with
 * dbus error names -- the 'nick' value is used, which can have dashes in it,
 * which dbus doesn't like. */


GType
xfconf_error_get_type(void)
{
    static GType type = 0;
    
    if(!type) {
        static const GEnumValue values[] = {
            { XFCONF_ERROR_UNKNOWN, "XFCONF_ERROR_UNKNOWN", "Unknown" },
            { XFCONF_ERROR_CHANNEL_NOT_FOUND, "XFCONF_ERROR_CHANNEL_NOT_FOUND", "ChannelNotFound" },
            { XFCONF_ERROR_PROPERTY_NOT_FOUND, "XFCONF_ERROR_PROPERTY_NOT_FOUND", "PropertyNotFound" },
            { XFCONF_ERROR_READ_FAILURE, "XFCONF_ERROR_READ_FAILURE", "ReadFailure" },
            { XFCONF_ERROR_WRITE_FAILURE, "XFCONF_ERROR_WRITE_FAILURE", "WriteFailure" },
            { XFCONF_ERROR_PERMISSION_DENIED, "XFCONF_ERROR_PERMISSION_DENIED", "PermissionDenied" },
            { XFCONF_ERROR_INTERNAL_ERROR, "XFCONF_ERROR_INTERNAL_ERROR", "InternalError" },
            { XFCONF_ERROR_NO_BACKEND, "XFCONF_ERROR_NO_BACKEND", "NoBackend" },
            { XFCONF_ERROR_INVALID_PROPERTY, "XFCONF_ERROR_INVALID_PROPERTY", "InvalidProperty" },
            { XFCONF_ERROR_INVALID_CHANNEL, "XFCONF_ERROR_INVALID_CHANNEL", "InvalidChannel" },
            { 0, NULL, NULL }
        };
        
        type = g_enum_register_static("XfconfError", values);
    }
    
    return type;
}


gboolean
_xfconf_error_from_dbus_error_name(const gchar *error_name,
                                   XfconfError *xfconf_error)
{
    g_return_val_if_fail(error_name != NULL, FALSE);
    g_return_val_if_fail(xfconf_error != NULL, FALSE);

    for (gsize i = 0; i < G_N_ELEMENTS(xfconf_daemon_dbus_error_entries); ++i) {
        if (g_strcmp0(error_name, xfconf_daemon_dbus_error_entries[i].dbus_error_name) == 0) {
            *xfconf_error = xfconf_daemon_dbus_error_entries[i].error_code;
            return TRUE;
        }
    }

    return FALSE;
}

#define __XFCONF_ERRORS_C__
#include "xfconf-aliasdef.c"
