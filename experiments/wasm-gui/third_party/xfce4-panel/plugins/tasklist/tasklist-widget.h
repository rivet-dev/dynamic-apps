/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCE_TASKLIST_H__
#define __XFCE_TASKLIST_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceTasklistClass     XfceTasklistClass;
typedef struct _XfceTasklist          XfceTasklist;
typedef enum   _XfceTasklistSortOrder XfceTasklistSortOrder;
typedef enum   _XfceTasklistMClick    XfceTasklistMClick;

#define XFCE_TYPE_TASKLIST            (xfce_tasklist_get_type ())
#define XFCE_TASKLIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_TASKLIST, XfceTasklist))
#define XFCE_TASKLIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_TASKLIST, XfceTasklistClass))
#define XFCE_IS_TASKLIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_TASKLIST))
#define XFCE_IS_TASKLIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_TASKLIST))
#define XFCE_TASKLIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_TASKLIST, XfceTasklistClass))



enum _XfceTasklistSortOrder
{
  XFCE_TASKLIST_SORT_ORDER_TIMESTAMP, /* sort by unique_id */
  XFCE_TASKLIST_SORT_ORDER_GROUP_TIMESTAMP, /* sort by group and then by timestamp */
  XFCE_TASKLIST_SORT_ORDER_TITLE, /* sort by window title */
  XFCE_TASKLIST_SORT_ORDER_GROUP_TITLE, /* sort by group and then by title */
  XFCE_TASKLIST_SORT_ORDER_DND, /* append and support dnd */

  XFCE_TASKLIST_SORT_ORDER_MIN = XFCE_TASKLIST_SORT_ORDER_TIMESTAMP,
  XFCE_TASKLIST_SORT_ORDER_MAX = XFCE_TASKLIST_SORT_ORDER_DND,
  XFCE_TASKLIST_SORT_ORDER_DEFAULT = XFCE_TASKLIST_SORT_ORDER_GROUP_TIMESTAMP
};

enum _XfceTasklistMClick
{
  XFCE_TASKLIST_MIDDLE_CLICK_NOTHING, /* do nothing */
  XFCE_TASKLIST_MIDDLE_CLICK_CLOSE_WINDOW, /* close the window */
  XFCE_TASKLIST_MIDDLE_CLICK_MINIMIZE_WINDOW, /* minimize, never minimize with button 1 */
  XFCE_TASKLIST_MIDDLE_CLICK_NEW_INSTANCE, /* launches a new instance of the window */

  XFCE_TASKLIST_MIDDLE_CLICK_MIN = XFCE_TASKLIST_MIDDLE_CLICK_NOTHING,
  XFCE_TASKLIST_MIDDLE_CLICK_MAX = XFCE_TASKLIST_MIDDLE_CLICK_NEW_INSTANCE,
  XFCE_TASKLIST_MIDDLE_CLICK_DEFAULT = XFCE_TASKLIST_MIDDLE_CLICK_NOTHING
};



GType xfce_tasklist_get_type                (void) G_GNUC_CONST;

void  xfce_tasklist_set_mode                (XfceTasklist        *tasklist,
                                             XfcePanelPluginMode  mode);

void  xfce_tasklist_set_size                (XfceTasklist   *tasklist,
                                             gint            size);

void  xfce_tasklist_set_nrows               (XfceTasklist   *tasklist,
                                             gint            nrows);

void  xfce_tasklist_update_monitor_geometry (XfceTasklist   *tasklist);

G_END_DECLS

#endif /* !__XFCE_TASKLIST_H__ */
