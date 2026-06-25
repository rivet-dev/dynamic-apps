/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
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

#ifndef __THUNAR_ABSTRACT_ICON_VIEW_H__
#define __THUNAR_ABSTRACT_ICON_VIEW_H__

#include <thunar/thunar-standard-view.h>

G_BEGIN_DECLS;

typedef struct _ThunarAbstractIconViewPrivate ThunarAbstractIconViewPrivate;
typedef struct _ThunarAbstractIconViewClass   ThunarAbstractIconViewClass;
typedef struct _ThunarAbstractIconView        ThunarAbstractIconView;

#define THUNAR_TYPE_ABSTRACT_ICON_VIEW             (thunar_abstract_icon_view_get_type ())
#define THUNAR_ABSTRACT_ICON_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ABSTRACT_ICON_VIEW, ThunarAbstractIconView))
#define THUNAR_ABSTRACT_ICON_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ABSTRACT_ICON_VIEW, ThunarAbstractIconViewClass))
#define THUNAR_IS_ABSTRACT_ICON_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ABSTRACT_ICON_VIEW))
#define THUNAR_IS_ABSTRACT_ICON_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNAR_TYPE_ABSTRACT_ICON_VIEW))
#define THUNAR_ABSTRACT_ICON_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ABSTRACT_ICON_VIEW, ThunarAbstractIconViewClass))

struct _ThunarAbstractIconViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarAbstractIconView
{
  ThunarStandardView             __parent__;
  ThunarAbstractIconViewPrivate *priv;
};

GType thunar_abstract_icon_view_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_ABSTRACT_ICON_VIEW_H__ */
