/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jeromeg@xfce.org>
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

#ifndef HAVE_GLOBAL_H
#define HAVE_GLOBAL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>



/* Possible actions */
enum {
  NONE = 0,
  SAVE = 1,
  CLIPBOARD = 2,
  OPEN = 4,
  CUSTOM_ACTION = 8,
};



/* Callback function to be called when execution is finalized */
typedef void (*ScreenshotFinalizeCallback) (gboolean action_executed , gpointer data);



/* Struct to store the screenshot options */
typedef struct
{
  gint region;
  gint show_save_dialog;
  gint show_mouse;
  gint show_border;
  gint delay;
  gint action;
  gboolean action_specified;
  gboolean region_specified;
  gboolean timestamp;
  gboolean path_is_dir;
  gboolean show_in_folder;
  gchar *screenshot_dir;
  gchar *title;
  gchar *app;
  gchar *custom_action_name;
  gchar *custom_action_command;
  GAppInfo *app_info;
  gchar *last_user;
  gchar *last_extension;
  GdkPixbuf *screenshot;
  ScreenshotFinalizeCallback finalize_callback;
  gpointer finalize_callback_data;
}
ScreenshotData;



/* Screenshot regions */
enum {
  REGION_0,
  FULLSCREEN,
  ACTIVE_WINDOW,
  SELECT,
};



/* Struct to store the preference dialog options */
typedef struct {
    GtkWidget *name, *cmd, *tree_view;
    GtkListStore *liststore;
    GtkTreeSelection *selection;
}
CustomActionDialogData;


/* Application-defined ids for dialog response */
enum {
    GTK_RESPONSE_PREFERENCES = 0
};

#endif
