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

#ifndef __THUNARX_PROVIDER_MODULE_H__
#define __THUNARX_PROVIDER_MODULE_H__

#include <glib-object.h>

typedef struct _ThunarxProviderModuleClass ThunarxProviderModuleClass;
typedef struct _ThunarxProviderModule      ThunarxProviderModule;

#define THUNARX_TYPE_PROVIDER_MODULE            (thunarx_provider_module_get_type ())
#define THUNARX_PROVIDER_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_PROVIDER_MODULE, ThunarxProviderModule))
#define THUNARX_PROVIDER_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_PROVIDER_MODULE, ThunarxProviderModuleClass))
#define THUNARX_IS_PROVIDER_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_PROVIDER_MODULE))
#define THUNARX_IS_PROVIDER_MODULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNARX_TYPE_PROVIDER_MODULE))
#define THUNARX_PROVIDER_MODULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNARX_TYPE_PROVIDER_MODULE, ThunarxProviderModuleClass))

GType                  thunarx_provider_module_get_type   (void) G_GNUC_CONST;

ThunarxProviderModule *thunarx_provider_module_new        (const gchar                 *filename) G_GNUC_MALLOC;

void                   thunarx_provider_module_list_types (const ThunarxProviderModule *module,
                                                           const GType                **types,
                                                           gint                        *n_types);

#endif /* !__THUNARX_PROVIDER_MODULE_H__ */
