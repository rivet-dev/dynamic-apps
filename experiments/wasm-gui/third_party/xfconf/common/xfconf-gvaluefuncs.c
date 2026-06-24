/*
 *  xfconf
 *
 *  Copyright (c) 2016 Ali Abdallah <ali@xfce.org>
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef GETTEXT_PACKAGE
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gio/gio.h>

#include "xfconf-gvaluefuncs.h"
#include "xfconf/xfconf-types.h"
#include "xfconf-common-private.h"

#ifdef CHAR_MIN
#define XFCONF_MINCHAR  CHAR_MIN
#else
#define XFCONF_MINCHAR  (-128)
#endif

#ifdef CHAR_MAX
#define XFCONF_MAXCHAR  CHAR_MAX
#else
#define XFCONF_MAXCHAR  (127)
#endif

#ifdef UCHAR_MAX
#define XFCONF_MAXUCHAR  UCHAR_MAX
#else
#define XFCONF_MAXUCHAR  (255)
#endif

GType
_xfconf_gtype_from_string(const gchar *type)
{
    /* note: move the most frequently used types to the top */
    if(!strcmp(type, "empty"))
        return G_TYPE_NONE;
    else if(!strcmp(type, "string"))
        return G_TYPE_STRING;
    else if(!strcmp(type, "int"))
        return G_TYPE_INT;
    else if(!strcmp(type, "double"))
        return G_TYPE_DOUBLE;
    else if(!strcmp(type, "bool"))
        return G_TYPE_BOOLEAN;
    else if(!strcmp(type, "array"))
        return G_TYPE_PTR_ARRAY;
    else if(!strcmp(type, "uint"))
        return G_TYPE_UINT;
    else if(!strcmp(type, "uchar"))
        return G_TYPE_UCHAR;
    else if(!strcmp(type, "char"))
        return G_TYPE_CHAR;
    else if(!strcmp(type, "uint16"))
        return XFCONF_TYPE_UINT16;
    else if(!strcmp(type, "int16"))
        return XFCONF_TYPE_INT16;
    else if(!strcmp(type, "uint64"))
        return G_TYPE_UINT64;
    else if(!strcmp(type, "int64"))
        return G_TYPE_INT64;
    else if(!strcmp(type, "float"))
        return G_TYPE_FLOAT;

    return G_TYPE_INVALID;
}

const gchar *
_xfconf_string_from_gtype(GType gtype)
{
    switch(gtype) {
        case G_TYPE_STRING:
            return "string";
        case G_TYPE_UCHAR:
            return "uchar";
        case G_TYPE_CHAR:
            return "char";
        case G_TYPE_UINT:
            return "uint";
        case G_TYPE_INT:
            return "int";
        case G_TYPE_UINT64:
            return "uint64";
        case G_TYPE_INT64:
            return "int64";
        case G_TYPE_FLOAT:
            return "float";
        case G_TYPE_DOUBLE:
            return "double";
        case G_TYPE_BOOLEAN:
            return "bool";
        default:
            if(gtype == XFCONF_TYPE_UINT16)
                return "uint16";
            else if(gtype == XFCONF_TYPE_INT16)
                return "int16";
            else if(gtype == G_TYPE_PTR_ARRAY)
                return "array";
            break;
    }

    g_warning("GType %s doesn't map to an Xfconf type",
              g_type_name(gtype));
    return NULL;
}

gboolean
_xfconf_gvalue_from_string(GValue *value,
                           const gchar *str)
{
#define CHECK_CONVERT_STATUS() \
    if(*str == 0 || *endptr != 0) \
        return FALSE
#define CHECK_CONVERT_VALUE(val, minval, maxval) \
    if((val) < (minval) || (val) > (maxval)) \
        return FALSE

#define REAL_HANDLE_INT(minval, maxval, convertfunc, setfunc) \
    G_STMT_START{ \
        errno = 0; \
        intval = convertfunc(str, &endptr, 0); \
        if(0 == intval && ERANGE == errno) \
            return FALSE; \
        CHECK_CONVERT_STATUS(); \
        CHECK_CONVERT_VALUE(intval, minval, maxval); \
        setfunc(value, intval); \
        return TRUE; \
    }G_STMT_END

#define HANDLE_UINT(minval, maxval, setfunc)  REAL_HANDLE_INT(minval, maxval, strtoul, setfunc)
#define HANDLE_INT(minval, maxval, setfunc)  REAL_HANDLE_INT(minval, maxval, strtol, setfunc)

    guint64 uintval;
    gint64 intval;
    gdouble dval;
    gchar *endptr = NULL;

    switch(G_VALUE_TYPE(value)) {
        case G_TYPE_STRING:
            g_value_set_string(value, str);
            return TRUE;

        case G_TYPE_UCHAR:
            HANDLE_UINT(0, XFCONF_MAXUCHAR, g_value_set_uchar);
        case G_TYPE_CHAR:
            HANDLE_INT(G_MININT8, G_MAXINT8, g_value_set_schar);
        case G_TYPE_UINT:
            HANDLE_UINT(0, G_MAXUINT, g_value_set_uint);
        case G_TYPE_INT:
            HANDLE_INT(G_MININT, G_MAXINT, g_value_set_int);

        case G_TYPE_UINT64:
            errno = 0;
            uintval = g_ascii_strtoull(str, &endptr, 0);
            if(0 == uintval && ERANGE == errno)
                return FALSE;
            CHECK_CONVERT_STATUS();
            g_value_set_uint64(value, uintval);
            return TRUE;

        case G_TYPE_INT64:
            errno = 0;
            intval = g_ascii_strtoll(str, &endptr, 0);
            if(0 == intval && ERANGE == errno)
                return FALSE;
            CHECK_CONVERT_STATUS();
            g_value_set_int64(value, intval);
            return TRUE;

        case G_TYPE_FLOAT:
            errno = 0;
            dval = g_ascii_strtod(str, &endptr);
            if(0.0 == dval && ERANGE == errno)
                return FALSE;
            CHECK_CONVERT_STATUS();
            if(dval < G_MINFLOAT || dval > G_MAXFLOAT)
                return FALSE;
            g_value_set_float(value, (gfloat)dval);
            return TRUE;

        case G_TYPE_DOUBLE:
            errno = 0;
            dval = g_ascii_strtod(str, &endptr);
            if(0.0 == dval && ERANGE == errno)
                return FALSE;
            CHECK_CONVERT_STATUS();
            g_value_set_double(value, dval);
            return TRUE;

        case G_TYPE_BOOLEAN:
            if(!strcmp(str, "true")) {
                g_value_set_boolean(value, TRUE);
                return TRUE;
            } else if(!strcmp(str, "false")) {
                g_value_set_boolean(value, FALSE);
                return TRUE;
            } else
                return FALSE;

        default:
            if(XFCONF_TYPE_UINT16 == G_VALUE_TYPE(value)) {
                HANDLE_INT(0, G_MAXUSHORT, xfconf_g_value_set_uint16);
                return TRUE;
            } else if(XFCONF_TYPE_INT16 == G_VALUE_TYPE(value)) {
                HANDLE_INT(G_MINSHORT, G_MAXSHORT, xfconf_g_value_set_int16);
                return TRUE;
            } else if(G_TYPE_PTR_ARRAY == G_VALUE_TYPE(value)) {
                GPtrArray *arr = g_ptr_array_sized_new(1);
                g_value_take_boxed(value, arr);
                return TRUE;
            }
            return FALSE;
    }

#undef CHECK_CONVERT_STATUS
#undef CHECK_CONVERT_VALUE
#undef REAL_HANDLE_INT
#undef HANDLE_INT
#undef HANDLE_UINT
}

gchar *
_xfconf_string_from_gvalue(GValue *val)
{
    g_return_val_if_fail(val && G_VALUE_TYPE(val), NULL);

    switch(G_VALUE_TYPE(val)) {
        case G_TYPE_STRING:
            return g_value_dup_string(val);
        case G_TYPE_UCHAR:
            return g_strdup_printf("%u", (guint)g_value_get_uchar(val));
        case G_TYPE_CHAR:
            return g_strdup_printf("%d", g_value_get_schar(val));
        case G_TYPE_UINT:
            return g_strdup_printf("%u", g_value_get_uint(val));
        case G_TYPE_INT:
            return g_strdup_printf("%d", g_value_get_int(val));
        case G_TYPE_UINT64:
            return g_strdup_printf("%" G_GUINT64_FORMAT,
                                   g_value_get_uint64(val));
        case G_TYPE_INT64:
            return g_strdup_printf("%" G_GINT64_FORMAT,
                                   g_value_get_int64(val));
        case G_TYPE_FLOAT:
            return g_strdup_printf("%f", (gdouble)g_value_get_float(val));
        case G_TYPE_DOUBLE:
            return g_strdup_printf("%f", g_value_get_double(val));
        case G_TYPE_BOOLEAN:
            return g_strdup(g_value_get_boolean(val) ? "true" : "false");
        default:
            if(G_VALUE_TYPE(val) == XFCONF_TYPE_UINT16) {
                return g_strdup_printf("%u",
                                       (guint)xfconf_g_value_get_uint16(val));
            } else if(G_VALUE_TYPE(val) == XFCONF_TYPE_INT16) {
                return g_strdup_printf("%d",
                                       (gint)xfconf_g_value_get_int16(val));
            }
            break;
    }

    g_warning("Unable to convert GValue to string");
    return NULL;
}

gboolean
_xfconf_gvalue_is_equal(const GValue *value1,
                        const GValue *value2)
{
    if(G_UNLIKELY(!value1 && !value2))
        return TRUE;
    if(G_UNLIKELY(!value1 || !value2))
        return FALSE;
    if(G_VALUE_TYPE(value1) != G_VALUE_TYPE(value2))
        return FALSE;
    if(G_VALUE_TYPE(value1) == G_TYPE_INVALID
       || G_VALUE_TYPE(value1) == G_TYPE_NONE)
    {
        return TRUE;
    }

    switch(G_VALUE_TYPE(value1)) {
#define HANDLE_CMP_GV(TYPE, getter) \
        case G_TYPE_ ## TYPE: \
            return g_value_get_ ## getter(value1) == g_value_get_ ## getter(value2)

        HANDLE_CMP_GV(CHAR, schar);
        HANDLE_CMP_GV(UCHAR, uchar);
        HANDLE_CMP_GV(BOOLEAN, boolean);
        HANDLE_CMP_GV(INT, int);
        HANDLE_CMP_GV(UINT, uint);
        HANDLE_CMP_GV(INT64, int64);
        HANDLE_CMP_GV(UINT64, uint64);
        HANDLE_CMP_GV(FLOAT, float);
        HANDLE_CMP_GV(DOUBLE, double);

        case G_TYPE_STRING:
            return !g_strcmp0(g_value_get_string(value1), g_value_get_string(value2));

        default:
            if(G_VALUE_TYPE(value1) == XFCONF_TYPE_INT16)
                return xfconf_g_value_get_int16(value1) == xfconf_g_value_get_uint16(value2);
            else if(G_VALUE_TYPE(value1) == XFCONF_TYPE_UINT16)
                return xfconf_g_value_get_uint16(value1) == xfconf_g_value_get_uint16(value2);
            break;
#undef HANDLE_CMP_GV
    }

    return FALSE;
}

void
_xfconf_gvalue_free(GValue *value)
{
    if(!value)
        return;
    g_value_unset(value);
    g_free(value);
}


GVariant *
xfconf_basic_gvalue_to_gvariant (const GValue *value) {

    const GVariantType *type = NULL;

    switch (G_VALUE_TYPE(value)){
    case G_TYPE_UINT:
        type = G_VARIANT_TYPE_UINT32;
        break;
    case G_TYPE_INT:
        type = G_VARIANT_TYPE_INT32;
        break;
    case G_TYPE_BOOLEAN:
        type = G_VARIANT_TYPE_BOOLEAN;
        break;
    case G_TYPE_UCHAR:
        type = G_VARIANT_TYPE_BYTE;
        break;
    case G_TYPE_INT64:
        type = G_VARIANT_TYPE_INT64;
        break;
    case G_TYPE_UINT64:
        type = G_VARIANT_TYPE_UINT64;
        break;
    case G_TYPE_DOUBLE:
        type = G_VARIANT_TYPE_DOUBLE;
        break;
    case G_TYPE_STRING:
        type = G_VARIANT_TYPE_STRING;
        break;
    default:
        if (G_VALUE_TYPE(value) == XFCONF_TYPE_INT16)
            type = G_VARIANT_TYPE_INT16;
        else if (G_VALUE_TYPE(value) == XFCONF_TYPE_UINT16)
            type = G_VARIANT_TYPE_UINT16;
        break;
    }

    if (type) {
        return g_dbus_gvalue_to_gvariant (value, type);
    }
    /* there is no g_variant_type_char! */
    else if (G_VALUE_TYPE(value) == G_TYPE_CHAR) {
        return g_variant_ref_sink(g_variant_new_int16(g_value_get_schar(value)));
    }

    g_warning ("Unable to convert GType '%s' to GVariant", _xfconf_string_from_gtype(G_VALUE_TYPE(value)));

    return NULL;
}

gboolean xfconf_basic_gvariant_to_gvalue (GVariant *variant, GValue *value)
{
  gboolean ret = TRUE;
    switch (g_variant_classify (variant)){
    case G_VARIANT_CLASS_UINT16:
        g_value_init(value, G_TYPE_UINT);
        g_value_set_uint (value, g_variant_get_uint16 (variant));
        break;
    case G_VARIANT_CLASS_INT16:
        g_value_init(value, G_TYPE_INT);
        g_value_set_int (value, g_variant_get_int16 (variant));
        break;
    case G_VARIANT_CLASS_UINT32:
        g_value_init(value, G_TYPE_UINT);
        g_value_set_uint (value, g_variant_get_uint32 (variant));
        break;
    case G_VARIANT_CLASS_INT32:
        g_value_init(value, G_TYPE_INT);
        g_value_set_int (value, g_variant_get_int32 (variant));
        break;
    case G_VARIANT_CLASS_UINT64:
        g_value_init(value, G_TYPE_UINT64);
        g_value_set_uint64 (value, g_variant_get_uint64 (variant));
        break;
    case G_VARIANT_CLASS_INT64:
        g_value_init(value, G_TYPE_INT64);
        g_value_set_int64 (value, g_variant_get_int64 (variant));
        break;
    case G_VARIANT_CLASS_BOOLEAN:
        g_value_init(value, G_TYPE_BOOLEAN);
        g_value_set_boolean (value, g_variant_get_boolean (variant));
        break;
    case G_VARIANT_CLASS_BYTE:
        g_value_init(value, G_TYPE_UCHAR);
        g_value_set_uchar (value, g_variant_get_byte (variant));
        break;
    case G_VARIANT_CLASS_STRING:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_string (value, g_variant_get_string (variant, NULL));
        break;
    case G_VARIANT_CLASS_DOUBLE:
        g_value_init(value, G_TYPE_DOUBLE);
        g_value_set_double (value, g_variant_get_double (variant));
        break;
    default:
        ret = FALSE;
        break;
    }
    return (ret);
}

GVariant *
xfconf_gvalue_to_gvariant (const GValue *value)
{
    GVariant *variant = NULL;

    if (G_VALUE_TYPE(value) == G_TYPE_PTR_ARRAY) {
        GPtrArray *arr;

        arr = (GPtrArray*)g_value_get_boxed (value);

        /* Check for array  */
        g_return_val_if_fail (arr, NULL);

        if (arr->len == 0) {
            variant = g_variant_ref_sink(g_variant_new ("av", NULL));
        }
        else {
            GVariantBuilder builder;
            guint i = 0;

            g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

            for (i=0; i < arr->len; ++i) {
                GValue *v = g_ptr_array_index (arr, i);
                GVariant *var = NULL;

                var = xfconf_basic_gvalue_to_gvariant (v);
                if (var) {
                    g_variant_builder_add (&builder, "v", var, NULL);
                    g_variant_unref (var);
                }
            }

            variant = g_variant_ref_sink(g_variant_builder_end (&builder));
        }
    }
    else if (G_VALUE_TYPE(value) == G_TYPE_STRV) {
        gchar **strlist;

        strlist = g_value_get_boxed(value);
        variant = g_variant_ref_sink(g_variant_new_strv ((const gchar**)strlist, g_strv_length(strlist)));
    }
    else
        variant = xfconf_basic_gvalue_to_gvariant(value);

    return variant;
}

GVariant *xfconf_hash_to_gvariant (GHashTable *hash)
{
    GHashTableIter iter;
    GVariantBuilder builder;
    GVariant *variant;
    const gchar *key;
    const GValue *value;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

    g_hash_table_iter_init (&iter, hash);

    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
        GVariant *v;

        if (G_VALUE_TYPE (value) == G_TYPE_PTR_ARRAY) {
            GPtrArray *arr;
            GVariantBuilder arr_builder;
            uint i;

            arr = g_value_get_boxed(value);

            g_variant_builder_init (&arr_builder, G_VARIANT_TYPE ("av"));

            for (i = 0; i < arr->len; ++i) {
                GValue *item_value = g_ptr_array_index(arr, i);

                if(item_value)
                {
                    v = xfconf_basic_gvalue_to_gvariant(item_value);
                    if (v) {
                        g_variant_builder_add (&arr_builder, "v", v);
                        g_variant_unref (v);
                    }
                }
            }

            v = g_variant_builder_end (&arr_builder);
            g_variant_builder_add (&builder, "{sv}", key, v);
        }
        else if (G_VALUE_TYPE (value) == G_TYPE_STRV) {
            gchar **strlist;

            strlist = g_value_get_boxed(value);
            variant = g_variant_new_strv ((const gchar**)strlist, g_strv_length(strlist));
            g_variant_builder_add (&builder, "{sv}", key, variant);
        }
        else {

            v = xfconf_basic_gvalue_to_gvariant(value);
            if (v) {
                g_variant_builder_add (&builder, "{sv}", key, v);
                g_variant_unref (v);
            }
        }
    }

    variant = g_variant_builder_end (&builder);
    return variant;
}

static void xfonf_free_array_elem_val (gpointer data) {
    GValue *val = (GValue*)data;
    g_value_unset (val);
    g_free (val);
}


GPtrArray *
xfconf_dup_value_array (GPtrArray *arr, gboolean auto_destroy_value) {

    GPtrArray *retArr;
    uint i;

    if (auto_destroy_value)
        retArr = g_ptr_array_new_full(arr->len, (GDestroyNotify)xfonf_free_array_elem_val);
    else
        retArr = g_ptr_array_sized_new(arr->len);

    for (i = 0; i< arr->len; i++) {
        GValue *v, *vi;
        v = g_new0(GValue, 1);
        vi = g_ptr_array_index(arr, i);
        g_value_init (v, G_VALUE_TYPE(vi));
        g_value_copy (vi, v);
        g_ptr_array_add(retArr, v);
    }

    return retArr;
}


GValue * xfconf_gvariant_to_gvalue (GVariant *in_variant)
{
    GValue *value ;
    GVariant *variant;
    value = g_new0(GValue, 1);

    if (g_variant_is_of_type(in_variant, G_VARIANT_TYPE ("v")))
        variant = g_variant_get_variant (in_variant);
    else
        variant = in_variant;

    if (g_variant_is_of_type (variant, G_VARIANT_TYPE("av"))) {
        GPtrArray *arr;
        GVariant *var;
        gsize nchild;
        gsize idx = 0;

        g_value_init(value, G_TYPE_PTR_ARRAY);

        nchild = g_variant_n_children (variant);
        arr = g_ptr_array_new_full(nchild, (GDestroyNotify)xfonf_free_array_elem_val);

        while (idx < nchild ) {
            GVariant *v;
            GValue *arr_val;

            arr_val = g_new0(GValue, 1);

            var = g_variant_get_child_value (variant, idx);
            v = g_variant_get_variant (var);
            xfconf_basic_gvariant_to_gvalue (v, arr_val);

            g_variant_unref (v);
            g_variant_unref (var);
            g_ptr_array_add (arr, arr_val);
            idx++;
        }

        g_value_take_boxed(value, arr);
    }
    else if (g_variant_is_of_type (variant, G_VARIANT_TYPE("as"))) {
        g_value_init(value, G_TYPE_STRV);
        g_value_set_boxed (value, g_variant_get_strv (variant, NULL));
    }
    else {/* Should be a basic type */
      if (!xfconf_basic_gvariant_to_gvalue(variant, value)) {
        g_free(value);
        return NULL;
      }
    }

    return value;
}

GHashTable *xfconf_gvariant_to_hash (GVariant *variant)
{
    GHashTable *properties;
    GVariantIter iter;
    GVariant *v;
    gchar *key;

    g_return_val_if_fail (g_variant_is_of_type(variant, G_VARIANT_TYPE ("a{sv}")), NULL);

    properties = g_hash_table_new_full(g_str_hash, g_str_equal,
                                       (GDestroyNotify)g_free,(GDestroyNotify)_xfconf_gvalue_free);

    g_variant_iter_init (&iter, variant);

    while (g_variant_iter_next (&iter, "{sv}", &key, &v)) {
        GValue *value;

        value = xfconf_gvariant_to_gvalue (v);
        g_hash_table_insert (properties,
                             g_strdup(key),
                             value);
        g_variant_unref (v);
        g_free(key);
    }
    return properties;
}
