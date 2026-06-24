/*
 *  xfconf
 *
 *  Copyright (c) 2009 Brian Tarricone <brian@tarricone.org>
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

#ifndef __XFCONF_CACHE_H__
#define __XFCONF_CACHE_H__

#include <glib-object.h>

#define XFCONF_TYPE_CACHE             (xfconf_cache_get_type())
#define XFCONF_CACHE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCONF_TYPE_CACHE, XfconfCache))
#define XFCONF_IS_CACHE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCONF_TYPE_CACHE))
#define XFCONF_CACHE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), XFCONF_TYPE_CACHE, XfconfCacheClass))
#define XFCONF_IS_CACHE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), XFCONF_TYPE_CACHE))
#define XFCONF_CACHE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), XFCONF_TYPE_CACHE, XfconfCacheClass))

G_BEGIN_DECLS

typedef struct _XfconfCache         XfconfCache;

G_GNUC_INTERNAL
GType xfconf_cache_get_type(void) G_GNUC_CONST;

G_GNUC_INTERNAL
XfconfCache *xfconf_cache_new(const gchar *channel_name) G_GNUC_MALLOC;

G_GNUC_INTERNAL
gboolean xfconf_cache_prefetch(XfconfCache *cache,
                               const gchar *property_base,
                               GError **error);

G_GNUC_INTERNAL
gboolean xfconf_cache_lookup(XfconfCache *cache,
                             const gchar *property,
                             GValue *value,
                             GError **error);

G_GNUC_INTERNAL
gboolean xfconf_cache_set(XfconfCache *cache,
                          const gchar *property,
                          const GValue *value,
                          GError **error);

G_GNUC_INTERNAL
gboolean xfconf_cache_reset(XfconfCache *cache,
                            const gchar *property_base,
                            gboolean recursive,
                            GError **error);
#if 0
G_GNUC_INTERNAL
void xfconf_cache_set_max_entries(XfconfCache *cache,
                                  gint max_entries);
G_GNUC_INTERNAL
gint xfconf_cache_get_max_entries(XfconfCache *cache);

G_GNUC_INTERNAL
void xfconf_cache_set_max_age(XfconfCache *cache,
                              gint max_age);
G_GNUC_INTERNAL
gint xfconf_cache_get_max_age(XfconfCache *cache);
#endif
G_END_DECLS

#endif  /* __XFCONF_CACHE_H__ */
