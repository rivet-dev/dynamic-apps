/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_NAVIGATOR_H__
#define __THUNAR_NAVIGATOR_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarNavigatorIface ThunarNavigatorIface;
typedef struct _ThunarNavigator      ThunarNavigator;

#define THUNAR_TYPE_NAVIGATOR           (thunar_navigator_get_type ())
#define THUNAR_NAVIGATOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_NAVIGATOR, ThunarNavigator))
#define THUNAR_IS_NAVIGATOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_NAVIGATOR))
#define THUNAR_NAVIGATOR_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_NAVIGATOR, ThunarNavigatorIface))

struct _ThunarNavigatorIface
{
  GTypeInterface __parent__;

  /* methods */
  ThunarFile *(*get_current_directory) (ThunarNavigator *navigator);
  void        (*set_current_directory) (ThunarNavigator *navigator,
                                        ThunarFile      *current_directory);

  /* signals */
  void        (*change_directory)      (ThunarNavigator *navigator,
                                        ThunarFile      *directory);
  void        (*open_new_tab)          (ThunarNavigator *navigator,
                                        ThunarFile      *directory);
};

GType       thunar_navigator_get_type              (void) G_GNUC_CONST;

ThunarFile *thunar_navigator_get_current_directory (ThunarNavigator *navigator);
void        thunar_navigator_set_current_directory (ThunarNavigator *navigator,
                                                    ThunarFile      *current_directory);

void        thunar_navigator_change_directory      (ThunarNavigator *navigator,
                                                    ThunarFile      *directory);

void        thunar_navigator_open_new_tab          (ThunarNavigator *navigator,
                                                    ThunarFile      *directory);

G_END_DECLS;

#endif /* !__THUNAR_NAVIGATOR_H__ */
