/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_PROVIDER_FACTORY_H__
#define __THUNARX_PROVIDER_FACTORY_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ThunarxProviderFactoryClass ThunarxProviderFactoryClass;
typedef struct _ThunarxProviderFactory      ThunarxProviderFactory;

#define THUNARX_TYPE_PROVIDER_FACTORY             (thunarx_provider_factory_get_type ())
#define THUNARX_PROVIDER_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_PROVIDER_FACTORY, ThunarxProviderFactory))
#define THUNARX_PROVIDER_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_PROVIDER_FACTORY, ThunarxProviderFactoryClass))
#define THUNARX_IS_PROVIDER_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_PROVIDER_FACTORY))
#define THUNARX_IS_PROVIDER_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNARX_TYPE_PROVIDER_FACTORY))
#define THUNARX_PROVIDER_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNARX_TYPE_PROVIDER_FACTORY, ThunarxProviderFactoryClass))

GType                   thunarx_provider_factory_get_type       (void) G_GNUC_CONST;

ThunarxProviderFactory *thunarx_provider_factory_get_default    (void);

GList                  *thunarx_provider_factory_list_providers (ThunarxProviderFactory *factory,
                                                                 GType                   type) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__THUNARX_PROVIDER_FACTORY_H__ */
