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

#ifndef __MOUSEPAD_VIEW_H__
#define __MOUSEPAD_VIEW_H__

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_VIEW (mousepad_view_get_type ())
G_DECLARE_FINAL_TYPE (MousepadView, mousepad_view, MOUSEPAD, VIEW, GtkSourceView)

enum
{
  SPACES_TO_TABS,
  TABS_TO_SPACES
};

gboolean        mousepad_view_scroll_to_cursor          (gpointer           data);

void            mousepad_view_transpose                 (MousepadView      *view);

void            mousepad_view_custom_paste              (MousepadView      *view,
                                                         const gchar       *string);

void            mousepad_view_convert_spaces_and_tabs   (MousepadView      *view,
                                                         gint               type);

void            mousepad_view_strip_trailing_spaces     (MousepadView      *view);

void            mousepad_view_duplicate                 (MousepadView      *view);

gint            mousepad_view_get_selection_length      (MousepadView      *view);

G_END_DECLS

#endif /* !__MOUSEPAD_VIEW_H__ */
