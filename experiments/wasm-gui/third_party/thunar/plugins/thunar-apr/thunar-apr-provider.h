/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_APR_PROVIDER_H__
#define __THUNAR_APR_PROVIDER_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _ThunarAprProviderClass ThunarAprProviderClass;
typedef struct _ThunarAprProvider      ThunarAprProvider;

#define THUNAR_APR_TYPE_PROVIDER            (thunar_apr_provider_get_type ())
#define THUNAR_APR_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_APR_TYPE_PROVIDER, ThunarAprProvider))
#define THUNAR_APR_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_APR_TYPE_PROVIDER, ThunarAprProviderClass))
#define THUNAR_APR_IS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_APR_TYPE_PROVIDER))
#define THUNAR_APR_IS_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_APR_TYPE_PROVIDER))
#define THUNAR_APR_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_APR_TYPE_PROVIDER, ThunarAprProviderClass))

GType thunar_apr_provider_get_type      (void) G_GNUC_CONST;
void  thunar_apr_provider_register_type (ThunarxProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__THUNAR_APR_PROVIDER_H__ */
