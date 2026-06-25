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

#ifndef __MOUSEPAD_SEARCH_BAR_H__
#define __MOUSEPAD_SEARCH_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_SEARCH_BAR (mousepad_search_bar_get_type ())
G_DECLARE_FINAL_TYPE (MousepadSearchBar, mousepad_search_bar, MOUSEPAD, SEARCH_BAR, GtkToolbar)

GtkWidget      *mousepad_search_bar_new             (void);

void            mousepad_search_bar_focus           (MousepadSearchBar *bar);

void            mousepad_search_bar_find_next       (MousepadSearchBar *bar);

void            mousepad_search_bar_find_previous   (MousepadSearchBar *bar);

void            mousepad_search_bar_page_switched   (MousepadSearchBar *bar,
                                                     GtkTextBuffer     *old_buffer,
                                                     GtkTextBuffer     *new_buffer,
                                                     gboolean           search);

void            mousepad_search_bar_set_text        (MousepadSearchBar *bar,
                                                     const gchar       *text);

G_END_DECLS

#endif /* !__MOUSEPAD_SEARCH_BAR_H__ */
