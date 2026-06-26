/*  $Id$
 *
 *  Copyright © 2022 Yogesh Kaushik <masterlukeskywalker02@gmail.com>
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
 * */

#ifndef __HAVE_CUSTOM_ACTIONS_H__
#define __HAVE_CUSTOM_ACTIONS_H__

#include <gtk/gtk.h>



enum {
    CUSTOM_ACTION_NAME,
    CUSTOM_ACTION_COMMAND,
    CUSTOM_ACTION_N_COLUMN
};



void screenshooter_custom_action_save    (GtkTreeModel   *list_store);
void screenshooter_custom_action_load    (GtkListStore   *list_store);
void screenshooter_custom_action_execute (gchar          *save_location,
                                          gchar          *name,
                                          gchar          *command);

#endif
