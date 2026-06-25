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

#ifndef __THUNAR_APR_IMAGE_PAGE_H__
#define __THUNAR_APR_IMAGE_PAGE_H__

#include <thunar-apr/thunar-apr-abstract-page.h>

G_BEGIN_DECLS;

typedef struct _ThunarAprImagePageClass ThunarAprImagePageClass;
typedef struct _ThunarAprImagePage      ThunarAprImagePage;

#define THUNAR_APR_TYPE_IMAGE_PAGE            (thunar_apr_image_page_get_type ())
#define THUNAR_APR_IMAGE_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_APR_TYPE_IMAGE_PAGE, ThunarAprImagePage))
#define THUNAR_APR_IMAGE_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_APR_TYPE_IMAGE_PAGE, ThunarAprImagePageClass))
#define THUNAR_APR_IS_IMAGE_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_APR_TYPE_IMAGE_PAGE))
#define THUNAR_APR_IS_IMAGE_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_APR_TYPE_IMAGE_PAGE))
#define THUNAR_APR_IMAGE_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_APR_TYPE_IMAGE_PAGE, ThunarAprImagePageClass))

GType thunar_apr_image_page_get_type      (void) G_GNUC_CONST;
void  thunar_apr_image_page_register_type (ThunarxProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__THUNAR_APR_IMAGE_PAGE_H__ */
