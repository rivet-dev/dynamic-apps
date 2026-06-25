/*
 * Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 * Copyright (c) 2008 Mike Massonnet <mmassonnet@xfce.org>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA
 */

#ifndef __TWP_PROVIDER_H__
#define __TWP_PROVIDER_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _TwpProviderClass TwpProviderClass;
typedef struct _TwpProvider      TwpProvider;

#define TWP_TYPE_PROVIDER             (twp_provider_get_type ())
#define TWP_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TWP_TYPE_PROVIDER, TwpProvider))
#define TWP_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TWP_TYPE_PROVIDER, TwpProviderClass))
#define TWP_IS_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TWP_TYPE_PROVIDER))
#define TWP_IS_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TWP_TYPE_PROVIDER))
#define TWP_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TWP_TYPE_PROVIDER, TwpProviderClass))

GType twp_provider_get_type      (void) G_GNUC_CONST;
void  twp_provider_register_type (ThunarxProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__TWP_PROVIDER_H__ */

