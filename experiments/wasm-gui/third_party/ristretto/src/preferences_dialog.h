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

#ifndef __RISTRETTO_PREFERENCES_DIALOG_H__
#define __RISTRETTO_PREFERENCES_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
    DESKTOP_TYPE_NONE = 0,
    DESKTOP_TYPE_XFCE,
    DESKTOP_TYPE_GNOME,
    DESKTOP_TYPE_COUNT
} RsttoDesktopType;



#define RSTTO_TYPE_PREFERENCES_DIALOG rstto_preferences_dialog_get_type ()
G_DECLARE_FINAL_TYPE (RsttoPreferencesDialog, rstto_preferences_dialog, RSTTO, PREFERENCES_DIALOG, GtkDialog)

typedef struct _RsttoPreferencesDialogPrivate RsttoPreferencesDialogPrivate;

struct _RsttoPreferencesDialog
{
    GtkDialog parent;
    RsttoPreferencesDialogPrivate *priv;
};



GtkWidget *
rstto_preferences_dialog_new (GtkWindow *parent);

G_END_DECLS

#endif /* __RISTRETTO_PREFERENCES_DIALOG_H__ */
