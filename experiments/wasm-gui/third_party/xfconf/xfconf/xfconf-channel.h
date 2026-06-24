/*
 *  xfconf
 *
 *  Copyright (c) 2007-2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifndef __XFCONF_CHANNEL_H__
#define __XFCONF_CHANNEL_H__

#if !defined(LIBXFCONF_COMPILATION) && !defined(__XFCONF_IN_XFCONF_H__)
#error "Do not include xfconf-channel.h, as this file may change or disappear in the future.  Include <xfconf/xfconf.h> instead."
#endif

#include <glib-object.h>

#define XFCONF_TYPE_CHANNEL             (xfconf_channel_get_type())
#define XFCONF_CHANNEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCONF_TYPE_CHANNEL, XfconfChannel))
#define XFCONF_IS_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCONF_TYPE_CHANNEL))
#define XFCONF_CHANNEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), XFCONF_TYPE_CHANNEL, XfconfChannelClass))
#define XFCONF_IS_CHANNEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), XFCONF_TYPE_CHANNEL))
#define XFCONF_CHANNEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), XFCONF_TYPE_CHANNEL, XfconfChannelClass))

G_BEGIN_DECLS

typedef struct _XfconfChannel         XfconfChannel;

GType xfconf_channel_get_type(void) G_GNUC_CONST;

XfconfChannel *xfconf_channel_get(const gchar *channel_name);

XfconfChannel *xfconf_channel_new(const gchar *channel_name) G_GNUC_WARN_UNUSED_RESULT;

XfconfChannel *xfconf_channel_new_with_property_base(const gchar *channel_name,
                                                     const gchar *property_base) G_GNUC_WARN_UNUSED_RESULT;

gboolean xfconf_channel_has_property(XfconfChannel *channel,
                                     const gchar *property);

gboolean xfconf_channel_is_property_locked(XfconfChannel *channel,
                                           const gchar *property);

void xfconf_channel_reset_property(XfconfChannel *channel,
                                   const gchar *property_base,
                                   gboolean recursive);

GHashTable *xfconf_channel_get_properties(XfconfChannel *channel,
                                          const gchar *property_base) G_GNUC_WARN_UNUSED_RESULT;

/* basic types */

gchar *xfconf_channel_get_string(XfconfChannel *channel,
                                 const gchar *property,
                                 const gchar *default_value) G_GNUC_WARN_UNUSED_RESULT;
gboolean xfconf_channel_set_string(XfconfChannel *channel,
                                   const gchar *property,
                                   const gchar *value);

gint32 xfconf_channel_get_int(XfconfChannel *channel,
                              const gchar *property,
                              gint32 default_value);
gboolean xfconf_channel_set_int(XfconfChannel *channel,
                                const gchar *property,
                                gint32 value);

guint32 xfconf_channel_get_uint(XfconfChannel *channel,
                                const gchar *property,
                                guint32 default_value);
gboolean xfconf_channel_set_uint(XfconfChannel *channel,
                                 const gchar *property,
                                 guint32 value);

guint64 xfconf_channel_get_uint64(XfconfChannel *channel,
                                  const gchar *property,
                                  guint64 default_value);
gboolean xfconf_channel_set_uint64(XfconfChannel *channel,
                                   const gchar *property,
                                   guint64 value);

gdouble xfconf_channel_get_double(XfconfChannel *channel,
                                  const gchar *property,
                                  gdouble default_value);
gboolean xfconf_channel_set_double(XfconfChannel *channel,
                                   const gchar *property,
                                   gdouble value);

gboolean xfconf_channel_get_bool(XfconfChannel *channel,
                                 const gchar *property,
                                 gboolean default_value);
gboolean xfconf_channel_set_bool(XfconfChannel *channel,
                                 const gchar *property,
                                 gboolean value);

/* this is just convenience API for the array stuff, where
 * all the values are G_TYPE_STRING */
gchar **xfconf_channel_get_string_list(XfconfChannel *channel,
                                       const gchar *property) G_GNUC_WARN_UNUSED_RESULT;
gboolean xfconf_channel_set_string_list(XfconfChannel *channel,
                                        const gchar *property,
                                        const gchar * const *values);

/* really generic API - can set some value types that aren't
 * supported by the basic type API, e.g., char, signed short,
 * unsigned int, etc.  no, you can't set arbitrary GTypes. */
gboolean xfconf_channel_get_property(XfconfChannel *channel,
                                     const gchar *property,
                                     GValue *value);
gboolean xfconf_channel_set_property(XfconfChannel *channel,
                                     const gchar *property,
                                     const GValue *value);

/* array types - arrays can be made up of values of arbitrary
 * (and mixed) types, even some not supported by the basic
 * type API */

gboolean xfconf_channel_get_array(XfconfChannel *channel,
                                  const gchar *property,
                                  GType first_value_type,
                                  ...);
gboolean xfconf_channel_get_array_valist(XfconfChannel *channel,
                                         const gchar *property,
                                         GType first_value_type,
                                         va_list var_args);
GPtrArray *xfconf_channel_get_arrayv(XfconfChannel *channel,
                                     const gchar *property) G_GNUC_WARN_UNUSED_RESULT;

gboolean xfconf_channel_set_array(XfconfChannel *channel,
                                  const gchar *property,
                                  GType first_value_type,
                                  ...);
gboolean xfconf_channel_set_array_valist(XfconfChannel *channel,
                                         const gchar *property,
                                         GType first_value_type,
                                         va_list var_args);
gboolean xfconf_channel_set_arrayv(XfconfChannel *channel,
                                   const gchar *property,
                                   GPtrArray *values);

/* struct types */

gboolean xfconf_channel_get_named_struct(XfconfChannel *channel,
                                         const gchar *property,
                                         const gchar *struct_name,
                                         gpointer value_struct);
gboolean xfconf_channel_set_named_struct(XfconfChannel *channel,
                                         const gchar *property,
                                         const gchar *struct_name,
                                         gpointer value_struct);

gboolean xfconf_channel_get_struct(XfconfChannel *channel,
                                   const gchar *property,
                                   gpointer value_struct,
                                   GType first_member_type,
                                   ...);
gboolean xfconf_channel_get_struct_valist(XfconfChannel *channel,
                                          const gchar *property,
                                          gpointer value_struct,
                                          GType first_member_type,
                                          va_list var_args);
gboolean xfconf_channel_get_structv(XfconfChannel *channel,
                                    const gchar *property,
                                    gpointer value_struct,
                                    guint n_members,
                                    GType *member_types);

gboolean xfconf_channel_set_struct(XfconfChannel *channel,
                                   const gchar *property,
                                   const gpointer value_struct,
                                   GType first_member_type,
                                   ...);
gboolean xfconf_channel_set_struct_valist(XfconfChannel *channel,
                                          const gchar *property,
                                          const gpointer value_struct,
                                          GType first_member_type,
                                          va_list var_args);
gboolean xfconf_channel_set_structv(XfconfChannel *channel,
                                    const gchar *property,
                                    const gpointer value_struct,
                                    guint n_members,
                                    GType *member_types);

#if 0  /* future (maybe) */

//gboolean xfconf_channel_begin_transaction(XfconfChannel *channel);
//gboolean xfconf_channel_commit_transaction(XfconfChannel *channel);
//void xfconf_channel_cancel_transaction(XfconfChannel *channel);

#endif

G_END_DECLS

#endif  /* __XFCONF_CHANNEL_H__ */
