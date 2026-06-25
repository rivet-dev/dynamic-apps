/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Include this file at the top of every source file. Never include it in
 * any other headers. */

#ifndef __MOUSEPAD_PRIVATE_H__
#define __MOUSEPAD_PRIVATE_H__

/* Our configuration header from the build system */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Some really common stdlib headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* In non-debug mode, turn off g_return_*()/g_assert*() debugging checks. */
#ifdef NDEBUG
# ifndef G_DISABLE_CHECKS
#  define G_DISABLE_CHECKS
# endif
# ifndef G_DISABLE_ASSERT
#  define G_DISABLE_ASSERT
# endif
#endif

/* These are the only three headers that can be assumed to always be included
 * since they are so core to the basic and advanced type system that GLib
 * provides us and for all the various i18n includes and macros we need. */
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

/* Mousepad id, common to application, icon, settings, etc. */
#define MOUSEPAD_ID     "org.xfce.mousepad"
#define MOUSEPAD_ID_LEN G_N_ELEMENTS (MOUSEPAD_ID) - 1

/* config file locations */
#define MOUSEPAD_SETTINGS_RELPATH ("Mousepad" G_DIR_SEPARATOR_S "settings.conf")
#define MOUSEPAD_RC_RELPATH       ("Mousepad" G_DIR_SEPARATOR_S "mousepadrc")
#define MOUSEPAD_ACCELS_RELPATH   ("Mousepad" G_DIR_SEPARATOR_S "accels.scm")

/* handling flags */
#define MOUSEPAD_SET_FLAG(flags, flag)   G_STMT_START{ ((flags) |= (flag)); }G_STMT_END
#define MOUSEPAD_UNSET_FLAG(flags, flag) G_STMT_START{ ((flags) &= ~(flag)); }G_STMT_END
#define MOUSEPAD_HAS_FLAG(flags, flag)   (((flags) & (flag)) != 0)

/* for personal testing */
#define TIMER_START     GTimer *__FUNCTION__timer = g_timer_new ();
#define TIMER_SPLIT     g_printerr ("%s: %.2f ms\n", G_STRLOC, \
                                    g_timer_elapsed (__FUNCTION__timer, NULL) * 1000);
#define TIMER_STOP      TIMER_SPLIT g_timer_destroy (__FUNCTION__timer);

#define PRINT_LOCATION  g_printerr ("%s\n", G_STRLOC);
#define RTRACE(fmt, var) G_STMT_START{ g_printerr ("%s:%s: ", G_STRLOC, #var); \
                                       g_printerr (fmt, var); g_printerr ("\n"); }G_STMT_END

/* optimize the properties */
#define MOUSEPAD_PARAM_READWRITE (G_PARAM_READWRITE | G_PARAM_STATIC_NAME \
                                  | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

/* support for canonical strings and quarks */
#define I_(string)  (g_intern_static_string (string))

/* convienient function for setting object data */
#define mousepad_object_set_data(object, key, data)               (g_object_set_qdata (G_OBJECT (object), \
                                                                   g_quark_from_static_string (key), (data)))
#define mousepad_object_set_data_full(object, key, data, destroy) (g_object_set_qdata_full (G_OBJECT (object), \
                                                                   g_quark_from_static_string (key), (data), \
                                                                   (GDestroyNotify) (destroy)))
#define mousepad_object_get_data(object, key)                     (g_object_get_qdata (G_OBJECT (object), \
                                                                   g_quark_try_string (key)))

/* avoid trivial g_value_get_*() function calls */
#ifdef NDEBUG
#define g_value_get_boolean(v)  (((const GValue *) (v))->data[0].v_int)
#define g_value_get_char(v)     (((const GValue *) (v))->data[0].v_int)
#define g_value_get_uchar(v)    (((const GValue *) (v))->data[0].v_uint)
#define g_value_get_int(v)      (((const GValue *) (v))->data[0].v_int)
#define g_value_get_uint(v)     (((const GValue *) (v))->data[0].v_uint)
#define g_value_get_long(v)     (((const GValue *) (v))->data[0].v_long)
#define g_value_get_ulong(v)    (((const GValue *) (v))->data[0].v_ulong)
#define g_value_get_int64(v)    (((const GValue *) (v))->data[0].v_int64)
#define g_value_get_uint64(v)   (((const GValue *) (v))->data[0].v_uint64)
#define g_value_get_enum(v)     (((const GValue *) (v))->data[0].v_long)
#define g_value_get_flags(v)    (((const GValue *) (v))->data[0].v_ulong)
#define g_value_get_float(v)    (((const GValue *) (v))->data[0].v_float)
#define g_value_get_double(v)   (((const GValue *) (v))->data[0].v_double)
#define g_value_get_string(v)   (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_param(v)    (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_boxed(v)    (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_pointer(v)  (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_object(v)   (((const GValue *) (v))->data[0].v_pointer)
#endif

/* properly set guess branch probability for pure booleans */
#undef G_LIKELY
#undef G_UNLIKELY

#if defined (NDEBUG) && G_GNUC_CHECK_VERSION (3, 0)
#define G_LIKELY(expr)   (__builtin_expect (!!(expr), TRUE))
#define G_UNLIKELY(expr) (__builtin_expect (!!(expr), FALSE))
#else
#define G_LIKELY(expr)   (expr)
#define G_UNLIKELY(expr) (expr)
#endif

/* GLib does some questionable (ie. non-standard) stuff when passing function
 * pointers as void pointers and such. This dirty little hack will at least
 * squelch warnings about it so we can easily see warnings that we can
 * actually fix in our code. */
static inline guint
mousepad_disconnect_by_func_ (gpointer  instance,
                              GCallback callback,
                              gpointer  data)
{
  union {
      gpointer  ptr;
      GCallback callback;
  } bad_stuff;
  /* we'll set it in the union as a GCallback type */
  bad_stuff.callback = callback;
  /* but we'll use it via the gpointer type */
  return g_signal_handlers_disconnect_by_func (instance, bad_stuff.ptr, data);
}

#define mousepad_disconnect_by_func(instance, callback, data) \
  mousepad_disconnect_by_func_ (instance, G_CALLBACK (callback), data)

G_END_DECLS

#endif /* !__MOUSEPAD_PRIVATE_H__ */
