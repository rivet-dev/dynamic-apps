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

#ifndef __THUNAR_HISTORY_H__
#define __THUNAR_HISTORY_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarHistoryClass ThunarHistoryClass;
typedef struct _ThunarHistory      ThunarHistory;

#define THUNAR_TYPE_HISTORY             (thunar_history_get_type ())
#define THUNAR_HISTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_HISTORY, ThunarHistory))
#define THUNAR_HISTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_HISTORY, ThunarHistoryClass))
#define THUNAR_IS_HISTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_HISTORY))
#define THUNAR_IS_HISTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_HISTORY))
#define THUNAR_HISTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_HISTORY, ThunarHistoryClass))

typedef enum
{
  THUNAR_HISTORY_MENU_BACK,
  THUNAR_HISTORY_MENU_FORWARD,


} ThunarHistoryMenuType;

struct _ThunarHistoryClass
{
  GObjectClass __parent__;

  /* external signals */
  void         (*history_changed)   (ThunarHistory *history,
                                     const gchar   *initial_text);
};

GType           thunar_history_get_type         (void) G_GNUC_CONST;

ThunarHistory  *thunar_history_copy             (ThunarHistory         *history);
gboolean        thunar_history_has_back         (ThunarHistory         *history);
gboolean        thunar_history_has_forward      (ThunarHistory         *history);
ThunarFile     *thunar_history_peek_back        (ThunarHistory         *history);
ThunarFile     *thunar_history_peek_forward     (ThunarHistory         *history);
void            thunar_history_action_back      (ThunarHistory         *history);
void            thunar_history_action_forward   (ThunarHistory         *history);
void            thunar_history_show_menu        (ThunarHistory         *history,
                                                 ThunarHistoryMenuType  type,
                                                 GtkWidget             *parent);
void            thunar_history_add              (ThunarHistory         *history,
                                                 ThunarFile            *directory);

G_END_DECLS;

#endif /* !__THUNAR_HISTORY_H__ */
