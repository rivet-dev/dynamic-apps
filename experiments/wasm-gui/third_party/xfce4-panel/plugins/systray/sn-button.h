/*
 *  Copyright (c) 2012-2013 Andrzej Radecki <andrzejr@xfce.org>
 *  Copyright (c) 2017      Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SN_BUTTON_H__
#define __SN_BUTTON_H__

#include <gtk/gtk.h>

#include "sn-config.h"
#include "sn-item.h"
#include "sn-plugin.h"

G_BEGIN_DECLS

typedef struct _SnButtonClass SnButtonClass;
typedef struct _SnButton      SnButton;

#define XFCE_TYPE_SN_BUTTON            (sn_button_get_type ())
#define XFCE_SN_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SN_BUTTON, SnButton))
#define XFCE_SN_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SN_BUTTON, SnButtonClass))
#define XFCE_IS_SN_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SN_BUTTON))
#define XFCE_IS_SN_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SN_BUTTON))
#define XFCE_SN_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SN_BUTTON, SnButtonClass))

GType                  sn_button_get_type                      (void) G_GNUC_CONST;

SnItem                *sn_button_get_item                      (SnButton                *button);

const gchar           *sn_button_get_name                      (SnButton                *button);

GtkWidget             *sn_button_new                           (SnItem                  *item,
                                                                SnPlugin                *plugin,
                                                                SnConfig                *config);

G_END_DECLS

#endif /* !__SN_BUTTON_H__ */
