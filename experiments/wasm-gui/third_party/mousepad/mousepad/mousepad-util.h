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

#ifndef __MOUSEPAD_UTIL_H__
#define __MOUSEPAD_UTIL_H__

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define mousepad_util_is_symlink(file) \
  (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) \
    == G_FILE_TYPE_SYMBOLIC_LINK)

gboolean     mousepad_util_iter_inside_word                 (const GtkTextIter          *iter);

gboolean     mousepad_util_iter_forward_word_end            (GtkTextIter                *iter);

gboolean     mousepad_util_iter_backward_word_start         (GtkTextIter                *iter);

gboolean     mousepad_util_iter_forward_text_start          (GtkTextIter                *iter);

gboolean     mousepad_util_iter_backward_text_start         (GtkTextIter                *iter);

gchar       *mousepad_util_config_name                      (const gchar                *name);

gchar       *mousepad_util_key_name                         (const gchar                *name);

gchar       *mousepad_util_escape_underscores               (const gchar                *str);

GtkWidget   *mousepad_util_image_button                     (const gchar                *icon_name,
                                                             const gchar                *label);

void         mousepad_util_entry_error                      (GtkWidget                  *widget,
                                                             gboolean                    error);

gchar       *mousepad_util_get_selection                    (GtkTextBuffer              *buffer);

void         mousepad_util_dialog_create_header             (GtkDialog                  *dialog,
                                                             const gchar                *title,
                                                             const gchar                *subtitle,
                                                             const gchar                *icon_name);

void         mousepad_util_dialog_update_header             (GtkDialog                  *dialog,
                                                             const gchar                *title,
                                                             const gchar                *subtitle,
                                                             const gchar                *icon_name);

void         mousepad_util_set_titlebar                     (GtkWindow                  *window);

gint         mousepad_util_get_real_line_offset             (const GtkTextIter          *iter);

void         mousepad_util_set_real_line_offset             (GtkTextIter                *iter,
                                                             gint                        column,
                                                             gboolean                    from_end);

void         mousepad_util_place_cursor                     (GtkTextBuffer              *buffer,
                                                             gint                        line,
                                                             gint                        column);

gchar       *mousepad_util_get_save_location                (const gchar                *relpath,
                                                             gboolean                    create_parents);

void         mousepad_util_save_key_file                    (GKeyFile                   *keyfile,
                                                             const gchar                *filename);

gboolean     mousepad_util_container_has_children           (GtkContainer               *container);

void         mousepad_util_container_clear                  (GtkContainer               *container);

void         mousepad_util_container_move_children          (GtkContainer               *source,
                                                             GtkContainer               *destination);

GSList      *mousepad_util_get_sorted_style_schemes         (void);

GSList      *mousepad_util_get_sorted_language_sections     (void);

GSList      *mousepad_util_get_sorted_languages_for_section (const gchar                *section);

/*
 * Copied from Gedit 3.38.0 and slightly modified:
 * https://gitlab.gnome.org/GNOME/gedit/-/blob/21fac3f0c87db0db104d7af7eaeb6f63d8216a14/gedit/gedit-pango.h#L28
 */
gchar       *mousepad_util_pango_font_description_to_css    (const PangoFontDescription *font_desc);

gboolean     mousepad_util_query_exists                     (GFile                      *file,
                                                             gboolean                    follow_symlink);

gchar       *mousepad_util_get_display_path                 (GFile                      *file);

gpointer     mousepad_util_source_autoremove                (gpointer                    object);

G_END_DECLS

#endif /* !__MOUSEPAD_UTIL_H__ */
