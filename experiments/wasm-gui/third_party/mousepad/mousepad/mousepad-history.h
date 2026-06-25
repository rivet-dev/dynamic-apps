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

#ifndef __MOUSEPAD_HISTORY_H__
#define __MOUSEPAD_HISTORY_H__

#include <mousepad/mousepad-encoding.h>
#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-application.h>

G_BEGIN_DECLS

enum
{
  MOUSEPAD_SESSION_RESTORE_NEVER,
  MOUSEPAD_SESSION_RESTORE_CRASH,
  MOUSEPAD_SESSION_RESTORE_UNSAVED,
  MOUSEPAD_SESSION_RESTORE_SAVED,
  MOUSEPAD_SESSION_RESTORE_ALWAYS,
};

enum
{
  MOUSEPAD_SESSION_QUITTING_NO,
  MOUSEPAD_SESSION_QUITTING_INTERACTIVE,
  MOUSEPAD_SESSION_QUITTING_NON_INTERACTIVE,
};

void         mousepad_history_init                             (void);

void         mousepad_history_finalize                         (void);

void         mousepad_history_recent_add                       (MousepadFile               *file);

void         mousepad_history_recent_get_language              (GFile                      *file,
                                                                gchar                     **language);

void         mousepad_history_recent_get_encoding              (GFile                      *file,
                                                                MousepadEncoding           *encoding);

void         mousepad_history_recent_get_cursor                (GFile                      *file,
                                                                gint                       *line,
                                                                gint                       *column);

void         mousepad_history_recent_clear                     (void);

void         mousepad_history_session_set_quitting             (gboolean                    quitting);

gint         mousepad_history_session_get_quitting             (void);

void         mousepad_history_session_save                     (void);

gboolean     mousepad_history_session_restore                  (MousepadApplication        *application);

GFile       *mousepad_history_autosave_get_location            (void);

void         mousepad_history_search_fill_search_box           (GtkComboBoxText            *box);

void         mousepad_history_search_fill_replace_box          (GtkComboBoxText            *box);

guint        mousepad_history_search_insert_search_text        (const gchar                *text);

guint        mousepad_history_search_insert_replace_text       (const gchar                *text);

G_END_DECLS

#endif /* !__MOUSEPAD_HISTORY_H__ */
