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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "xfconf/xfconf-types.h"
#include "xfconf-alias.h"

#include <gobject/gvaluecollector.h>

/**
 * SECTION:xfconf-types
 * @title: Xfconf Types
 * @short_description: GObject types used by the Xfconf daemon and library
 *
 *  libgobject lacks GObject fundamental types for 16-bit signed and unsigned integers, which may be useful to use in an Xfconf store. GObject types for these primitive types are provided here.
 **/


/**
 * XFCONF_TYPE_UINT16:
 *
 * The registered #GType for a 16-bit unsigned type.
 **/

/**
 * XFCONF_TYPE_INT16:
 *
 * The registered #GType for a 16-bit signed type.
 **/


static void
gvalue_from_short(const GValue *src_value,
                  GValue *dest_value)
{
#define HANDLE_TYPE(gtype_s, getter) \
    case G_TYPE_ ## gtype_s: \
        dest = (guint64)g_value_get_ ## getter(src_value); \
        break;

    guint64 dest;  /* use larger type so we can handle int16 & uint16 */

    switch(G_VALUE_TYPE(src_value)) {
        case G_TYPE_STRING:
            dest = atoi(g_value_get_string(src_value));
            break;
        case G_TYPE_BOOLEAN:
            dest = g_value_get_boolean(src_value) == TRUE ? 1 : 0;
            break;
        HANDLE_TYPE(CHAR, schar)
        HANDLE_TYPE(UCHAR, uchar)
        HANDLE_TYPE(INT, int)
        HANDLE_TYPE(UINT, uint)
        HANDLE_TYPE(LONG, long)
        HANDLE_TYPE(ULONG, ulong)
        HANDLE_TYPE(INT64, int64)
        HANDLE_TYPE(UINT64, uint64)
        HANDLE_TYPE(ENUM, enum)
        HANDLE_TYPE(FLAGS, flags)
        HANDLE_TYPE(FLOAT, float)
        HANDLE_TYPE(DOUBLE, double)
        default:
            return;
    }

    if(G_VALUE_TYPE(dest_value) == XFCONF_TYPE_UINT16) {
        if(dest > USHRT_MAX) {
            g_warning("Converting type \"%s\" to \"%s\" results in overflow",
                      G_VALUE_TYPE_NAME(src_value),
                      G_VALUE_TYPE_NAME(dest_value));
        }
        xfconf_g_value_set_uint16(dest_value, (guint16)dest);
    } else if(G_VALUE_TYPE(dest_value) == XFCONF_TYPE_INT16) {
        if(dest > (guint64) SHRT_MAX || dest < (guint64) SHRT_MIN) {
            g_warning("Converting type \"%s\" to \"%s\" results in overflow",
                      G_VALUE_TYPE_NAME(src_value),
                      G_VALUE_TYPE_NAME(dest_value));
        }
        xfconf_g_value_set_int16(dest_value, (gint16)dest);
    }
#undef HANDLE_TYPE
}

static void
short_from_gvalue(const GValue *src_value,
                  GValue *dest_value)
{
#define HANDLE_TYPE(gtype_s, setter) \
    case G_TYPE_ ## gtype_s: \
        g_value_set_ ## setter(dest_value, src); \
        break;

    guint16 src;
    gboolean is_signed = FALSE;

    if(G_VALUE_TYPE(src_value) == XFCONF_TYPE_UINT16)
        src = xfconf_g_value_get_uint16(src_value);
    else if(G_VALUE_TYPE(src_value) == XFCONF_TYPE_INT16) {
        src = xfconf_g_value_get_int16(src_value);
        is_signed = TRUE;
    } else
        return;

    switch(G_VALUE_TYPE(dest_value)) {
        case G_TYPE_STRING: {
            gchar *str = g_strdup_printf(is_signed ? "%d" : "%u",
                                         is_signed ? (gint16)src : src);
            g_value_set_string(dest_value, str);
            g_free(str);
            break;
        }
        case G_TYPE_BOOLEAN:
            g_value_set_boolean(dest_value, src ? TRUE : FALSE);
            break;
        HANDLE_TYPE(CHAR, schar)
        HANDLE_TYPE(UCHAR, uchar)
        HANDLE_TYPE(INT, int)
        HANDLE_TYPE(UINT, uint)
        HANDLE_TYPE(LONG, long)
        HANDLE_TYPE(ULONG, ulong)
        HANDLE_TYPE(INT64, int64)
        HANDLE_TYPE(UINT64, uint64)
        HANDLE_TYPE(ENUM, enum)
        HANDLE_TYPE(FLAGS, flags)
        HANDLE_TYPE(FLOAT, float)
        HANDLE_TYPE(DOUBLE, double)
        default:
            return;
    }
#undef HANDLE_TYPE
}

static void
register_transforms(GType gtype)
{
    GType types[] = {
        G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_UINT,
        G_TYPE_LONG, G_TYPE_ULONG, G_TYPE_INT64, G_TYPE_UINT64,
        G_TYPE_ENUM, G_TYPE_FLAGS, G_TYPE_FLOAT, G_TYPE_DOUBLE,
        G_TYPE_STRING, G_TYPE_INVALID,
    };
    gint i;

    for(i = 0; types[i] != G_TYPE_INVALID; ++i) {
        g_value_register_transform_func(gtype, types[i], gvalue_from_short);
        g_value_register_transform_func(types[i], gtype, short_from_gvalue);
    }
}

static void
ushort_value_init(GValue *value)
{
    value->data[0].v_int = 0;
}

static void
ushort_value_copy(const GValue *src_value,
                  GValue *dest_value)
{
    dest_value->data[0].v_int = src_value->data[0].v_int;
}

static gchar *
ushort_value_collect(GValue *value,
                     guint n_collect_values,
                     GTypeCValue *collect_values,
                     guint collect_flags)
{
    value->data[0].v_int = collect_values[0].v_int;
    return NULL;
}

static gchar *
ushort_value_lcopy(const GValue *value,
                   guint n_collect_values,
                   GTypeCValue *collect_values,
                   guint collect_flags)
{
    guint16 *uint16_p = collect_values[0].v_pointer;

    if(!uint16_p) {
        return g_strdup_printf("value location for `%s' passed as NULL",
                               G_VALUE_TYPE_NAME(value));
    }

    *uint16_p = value->data[0].v_int;

    return NULL;
}


GType
xfconf_uint16_get_type(void)
{
    static GType uint16_type = 0;
    GTypeFundamentalInfo finfo = { 0 };
    GTypeInfo info = { 0, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, };
    static const GTypeValueTable value_table = {
        ushort_value_init,
        NULL,
        ushort_value_copy,
        NULL,
        (gchar *)"i",
        ushort_value_collect,
        (gchar *)"p",
        ushort_value_lcopy
    };

    if(!uint16_type) {
        info.value_table = &value_table;
        uint16_type = g_type_register_fundamental(g_type_fundamental_next(),
                                                  "XfconfUint16", &info,
                                                   &finfo, 0);
        register_transforms(uint16_type);
    }

    return uint16_type;
}

/**
 * xfconf_g_value_get_uint16:
 * @value: A #GValue.
 *
 * Retrieves a 16-bit unsigned value from @value.
 *
 * Returns: A guint16.
 **/
guint16
xfconf_g_value_get_uint16(const GValue *value)
{
    g_return_val_if_fail(G_VALUE_HOLDS(value, XFCONF_TYPE_UINT16), 0);
    return (guint16)value->data[0].v_int;
}

/**
 * xfconf_g_value_set_uint16:
 * @value: A #GValue.
 * @v_uint16: A guint16.
 *
 * Sets @value using an unsigned 16-bit integer.
 **/
void
xfconf_g_value_set_uint16(GValue *value,
                          guint16 v_uint16)
{
    g_return_if_fail(G_VALUE_HOLDS(value, XFCONF_TYPE_UINT16));
    value->data[0].v_int = v_uint16;
}

GType
xfconf_int16_get_type(void)
{
    static GType int16_type = 0;
    GTypeFundamentalInfo finfo = { 0 };
    GTypeInfo info = { 0, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, };
    static const GTypeValueTable value_table = {
        ushort_value_init,
        NULL,
        ushort_value_copy,
        NULL,
        (gchar *)"i",
        ushort_value_collect,
        (gchar *)"p",
        ushort_value_lcopy
    };

    if(!int16_type) {
        info.value_table = &value_table;
        int16_type = g_type_register_fundamental(g_type_fundamental_next(),
                                                 "XfconfInt16", &info,
                                                  &finfo, 0);
        register_transforms(int16_type);
    }

    return int16_type;
}

/**
 * xfconf_g_value_get_int16:
 * @value: A #GValue.
 *
 * Retrieves a 16-bit signed value from @value.
 *
 * Returns: A gint16.
 **/
gint16
xfconf_g_value_get_int16(const GValue *value)
{
    g_return_val_if_fail(G_VALUE_HOLDS(value, XFCONF_TYPE_INT16), 0);
    return (gint16)value->data[0].v_int;
}

/**
 * xfconf_g_value_set_int16:
 * @value: A #GValue.
 * @v_int16: A gint16.
 *
 * Sets @value using a signed 16-bit integer.
 **/
void
xfconf_g_value_set_int16(GValue *value,
                         gint16 v_int16)
{
    g_return_if_fail(G_VALUE_HOLDS(value, XFCONF_TYPE_INT16));
    value->data[0].v_int = v_int16;
}


#ifdef LIBXFCONF_COMPILATION
#define __XFCONF_TYPES_C__
#include "xfconf-aliasdef.c"
#endif
