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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

#include "xfconf.h"
#include "common/xfconf-marshal.h"
#include "xfconf-private.h"
#include "common/xfconf-alias.h"

static guint xfconf_refcnt = 0;

static GDBusConnection *gdbus = NULL;
static GDBusProxy *gproxy = NULL;
static GHashTable *named_structs = NULL;

#define XFCONF_DBUS_NAME XFCONF_SERVICE_NAME_PREFIX ".Xfconf"
#define XFCONF_DBUS_NAME_TEST XFCONF_SERVICE_NAME_PREFIX ".XfconfTest"


/* private api */

GDBusConnection *
_xfconf_get_gdbus_connection(void)
{
    if(!xfconf_refcnt) {
        g_critical("xfconf_init() must be called before attempting to use libxfconf!");
        return NULL;
    }

    return gdbus;
}


GDBusProxy *
_xfconf_get_gdbus_proxy(void)
{
    if(!xfconf_refcnt) {
        g_critical("xfconf_init() must be called before attempting to use libxfconf!");
        return NULL;
    }

    return gproxy;
}

XfconfNamedStruct *
_xfconf_named_struct_lookup(const gchar *struct_name)
{
    return named_structs ? g_hash_table_lookup(named_structs, struct_name) : NULL;
}

static void
_xfconf_named_struct_free(XfconfNamedStruct *ns)
{
    g_free(ns->member_types);
    g_slice_free(XfconfNamedStruct, ns);
}

/* public api */

/**
 * SECTION:xfconf
 * @title: Xfconf Library Core
 * @short_description: Init routines and core functionality for libxfconf
 *
 * Before libxfconf can be used, it must be initialized by calling
 * xfconf_init().  To free resources used by the library, call
 * xfconf_shutdown().  These calls are "recursive": multiple calls to
 * xfconf_init() are allowed, but each call must be matched by a
 * separate call to xfconf_shutdown() to really free the library's
 * resources.
 **/

/**
 * xfconf_init:
 * @error: An error return.
 *
 * Initializes the Xfconf library.  Can be called multiple times with no
 * adverse effects.
 *
 * Returns: %TRUE if the library was initialized succesfully, %FALSE on
 *          error.  If there is an error @error will be set.
 **/
gboolean
xfconf_init(GError **error)
{
    const gchar *is_test_mode;

    if(xfconf_refcnt) {
        ++xfconf_refcnt;
        return TRUE;
    }

    gdbus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

    if (!gdbus)
        return FALSE;

    is_test_mode = g_getenv ("XFCONF_RUN_IN_TEST_MODE");
    gproxy = g_dbus_proxy_new_sync(gdbus,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                   is_test_mode == NULL ? XFCONF_DBUS_NAME : XFCONF_DBUS_NAME_TEST,
                                   XFCONF_SERVICE_PATH_PREFIX "/Xfconf",
                                   XFCONF_SERVICE_NAME_PREFIX ".Xfconf",
                                   NULL,
                                   NULL);

    ++xfconf_refcnt;
    return TRUE;
}

/**
 * xfconf_shutdown:
 *
 * Shuts down and frees any resources consumed by the Xfconf library.
 * If xfconf_init() is called multiple times, xfconf_shutdown() must be
 * called an equal number of times to shut down the library.
 **/
void
xfconf_shutdown(void)
{
    if(xfconf_refcnt <= 0) {
        return;
    }

    if(xfconf_refcnt > 1) {
        --xfconf_refcnt;
        return;
    }

    /* Flush pending dbus calls */
    g_dbus_connection_flush_sync (gdbus, NULL, NULL);

    _xfconf_channel_shutdown();
    _xfconf_g_bindings_shutdown();

    if(named_structs) {
        g_hash_table_destroy(named_structs);
        named_structs = NULL;
    }

    --xfconf_refcnt;
}

/**
 * xfconf_named_struct_register:
 * @struct_name: The unique name of the struct to register.
 * @n_members: The number of data members in the struct.
 * @member_types: An array of the #GType<!-- -->s of the struct members.
 *
 * Registers a named struct for use with xfconf_channel_get_named_struct()
 * and xfconf_channel_set_named_struct().
 **/
void
xfconf_named_struct_register(const gchar *struct_name,
                             guint n_members,
                             const GType *member_types)
{
    XfconfNamedStruct *ns;

    g_return_if_fail(struct_name && *struct_name && n_members && member_types);

    /* lazy initialize the hash table */
    if(named_structs == NULL)
        named_structs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              (GDestroyNotify)g_free,
                                              (GDestroyNotify)_xfconf_named_struct_free);

    if(G_UNLIKELY(g_hash_table_lookup(named_structs, struct_name)))
        g_critical("The struct '%s' is already registered", struct_name);
    else {
        ns = g_slice_new(XfconfNamedStruct);
        ns->n_members = n_members;
        ns->member_types = g_new(GType, n_members);
        memcpy(ns->member_types, member_types, sizeof(GType) * n_members);

        g_hash_table_insert(named_structs, g_strdup(struct_name), ns);
    }
}

#if 0
/**
 * xfconf_array_new:
 * @n_preallocs: Number of entries to preallocate space for.
 *
 * Convenience function to greate a new #GArray to hold
 * #GValue<!-- -->s.  Normal #GArray functions may be used on
 * the returned array.  For convenience, see also xfconf_array_free().
 *
 * Returns: A new #GArray.
 **/
GArray *
xfconf_array_new(gint n_preallocs)
{
    return g_array_sized_new(FALSE, TRUE, sizeof(GValue), n_preallocs);
}
#endif

/**
 * xfconf_array_free:
 * @arr: (element-type GValue): A #GPtrArray of #GValue<!-- -->s.
 *
 * Properly frees a #GPtrArray structure containing a list of
 * #GValue<!-- -->s.  This will also cause the contents of the
 * values to be freed as well.
 **/
void
xfconf_array_free(GPtrArray *arr)
{
    guint i;

    if(!arr)
        return;

    for(i = 0; i < arr->len; ++i) {
        GValue *val = g_ptr_array_index(arr, i);
        g_value_unset(val);
        g_free(val);
    }

    g_ptr_array_free(arr, TRUE);
}



#define __XFCONF_C__
#include "common/xfconf-aliasdef.c"
