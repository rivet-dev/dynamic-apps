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

#ifndef __XFCONF_PRIVATE_H__
#define __XFCONF_PRIVATE_H__

#include <gio/gio.h>

#ifdef XFCONF_ENABLE_CHECKS

#define ERROR_DEFINE  GError *___error = NULL
#define ERROR         &___error
#define ERROR_CHECK   G_STMT_START{ \
    if(___error) { \
        g_warning("Error check failed at %s():%d: %s", __FUNCTION__, __LINE__, \
                  ___error->message); \
        g_error_free(___error); \
    } \
}G_STMT_END

#else

#define ERROR_DEFINE  G_STMT_START{ }G_STMT_END
#define ERROR         NULL
#define ERROR_CHECK   G_STMT_START{ }G_STMT_END

#endif

typedef struct
{
    guint n_members;
    GType *member_types;
} XfconfNamedStruct;


GDBusConnection *_xfconf_get_gdbus_connection(void);
GDBusProxy *_xfconf_get_gdbus_proxy(void);

XfconfNamedStruct *_xfconf_named_struct_lookup(const gchar *struct_name);

void _xfconf_channel_shutdown(void);
const gchar *_xfconf_channel_get_name(XfconfChannel *channel);
const gchar *_xfconf_channel_get_property_base(XfconfChannel *channel);

void _xfconf_g_bindings_shutdown(void);

#endif  /* __XFCONF_PRIVATE_H__ */
