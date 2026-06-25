/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __RISTRETTO_MAIN_WINDOW_H__
#define __RISTRETTO_MAIN_WINDOW_H__

#include <gtk/gtk.h>

#include "image_list.h"
#include "icon_bar.h"

G_BEGIN_DECLS

typedef enum
{
    RSTTO_DESKTOP_NONE,
    RSTTO_DESKTOP_XFCE,
    RSTTO_DESKTOP_GNOME
} RsttoDesktop;



#define RSTTO_TYPE_MAIN_WINDOW rstto_main_window_get_type ()
G_DECLARE_FINAL_TYPE (RsttoMainWindow, rstto_main_window, RSTTO, MAIN_WINDOW, GtkWindow)

typedef struct _RsttoMainWindowPrivate RsttoMainWindowPrivate;

struct _RsttoMainWindow
{
    GtkWindow parent;
    RsttoMainWindowPrivate *priv;
};



RsttoMainWindow *
rstto_main_window_get_app_window (void);

gboolean
rstto_main_window_get_app_exited (void);

RsttoImageList *
rstto_main_window_get_app_image_list (void);

RsttoIconBar *
rstto_main_window_get_app_icon_bar (void);

GtkFileFilter *
rstto_main_window_get_app_file_filter (void);

GtkWidget *
rstto_main_window_new (RsttoImageList *image_list,
                       gboolean fullscreen);

RsttoImageListIter *
rstto_main_window_get_iter (RsttoMainWindow *window);

void
rstto_main_window_open (RsttoMainWindow *window,
                        GSList *files);

gboolean
rstto_main_window_play_slideshow (RsttoMainWindow *window);

void
rstto_main_window_fullscreen (GtkWidget *widget,
                              RsttoMainWindow *window);

G_END_DECLS

#endif /* __RISTRETTO_MAIN_WINDOW_H__ */

