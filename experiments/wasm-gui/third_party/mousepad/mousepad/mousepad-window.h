/*
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

#ifndef __MOUSEPAD_WINDOW_H__
#define __MOUSEPAD_WINDOW_H__

#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-application.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_WINDOW (mousepad_window_get_type ())
G_DECLARE_FINAL_TYPE (MousepadWindow, mousepad_window, MOUSEPAD, WINDOW, GtkApplicationWindow)

/* sanity checks */
#define mousepad_is_application_window(window) \
  (g_list_find (gtk_application_get_windows (GTK_APPLICATION (g_application_get_default ())), \
                window) != NULL)

enum
{
  TARGET_TEXT_URI_LIST,
  TARGET_GTK_NOTEBOOK_TAB
};

static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST },
  { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, TARGET_GTK_NOTEBOOK_TAB }
};

GtkWidget      *mousepad_window_new                        (MousepadApplication  *application);

void            mousepad_window_add                        (MousepadWindow       *window,
                                                            MousepadDocument     *document);

gint            mousepad_window_open_files                 (MousepadWindow       *window,
                                                            GFile               **files,
                                                            gint                  n_files,
                                                            MousepadEncoding      encoding,
                                                            gint                  line,
                                                            gint                  column,
                                                            gboolean              must_exist);

void            mousepad_window_show_preferences           (MousepadWindow       *window);

GtkWidget      *mousepad_window_get_languages_menu         (MousepadWindow       *window);

void            mousepad_window_update_document_menu_items (MousepadWindow       *window);

void            mousepad_window_update_window_menu_items   (MousepadWindow       *window);

/* for plugins */
GtkWidget      *mousepad_window_get_notebook               (MousepadWindow       *window);

GtkWidget      *mousepad_window_menu_item_realign          (MousepadWindow       *window,
                                                            GtkWidget            *item,
                                                            const gchar          *action_name,
                                                            GtkWidget            *menu,
                                                            gint                  index);

G_END_DECLS

#endif /* !__MOUSEPAD_WINDOW_H__ */
