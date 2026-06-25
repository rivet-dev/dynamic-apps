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

#ifndef __MOUSEPAD_DIALOGS_H__
#define __MOUSEPAD_DIALOGS_H__

#include <mousepad/mousepad-encoding.h>
#include <mousepad/mousepad-file.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* error dialog messages */
#define MOUSEPAD_MESSAGE_IO_ERROR_OPEN        _("Failed to open the document")
#define MOUSEPAD_MESSAGE_IO_ERROR_SAVE        _("Failed to save the document")
#define MOUSEPAD_MESSAGE_UNSUPPORTED_ENCODING _("Unsupported character set")

/* button labels */
#define MOUSEPAD_LABEL_CANCEL  _("_Cancel")
#define MOUSEPAD_LABEL_OK      _("_OK")
#define MOUSEPAD_LABEL_SAVE    _("_Save")
#define MOUSEPAD_LABEL_SAVE_AS _("Save _As")
#define MOUSEPAD_LABEL_REVERT  _("Re_vert")
#define MOUSEPAD_LABEL_RELOAD  _("Re_load")

/* dialog responses */
enum {
  MOUSEPAD_RESPONSE_CANCEL,
  MOUSEPAD_RESPONSE_CLEAR,
  MOUSEPAD_RESPONSE_CLOSE,
  MOUSEPAD_RESPONSE_DONT_SAVE,
  MOUSEPAD_RESPONSE_ENTRY_CHANGED,
  MOUSEPAD_RESPONSE_FIND,
  MOUSEPAD_RESPONSE_REVERSE_FIND,
  MOUSEPAD_RESPONSE_JUMP_TO,
  MOUSEPAD_RESPONSE_OK,
  MOUSEPAD_RESPONSE_OVERWRITE,
  MOUSEPAD_RESPONSE_RELOAD,
  MOUSEPAD_RESPONSE_REPLACE,
  MOUSEPAD_RESPONSE_SAVE,
  MOUSEPAD_RESPONSE_SAVE_AS,
};


void       mousepad_dialogs_destroy_with_parent (GtkWidget         *dialog,
                                                 GtkWindow         *parent);

void       mousepad_dialogs_show_about          (GtkWindow         *parent);

void       mousepad_dialogs_show_error          (GtkWindow         *parent,
                                                 const GError      *error,
                                                 const gchar       *message);

void       mousepad_dialogs_show_help           (GtkWindow         *parent,
                                                 const gchar       *page,
                                                 const gchar       *offset);

gint       mousepad_dialogs_other_tab_size      (GtkWindow         *parent,
                                                 gint               active_size);

gboolean   mousepad_dialogs_go_to               (GtkWindow         *parent,
                                                 GtkTextBuffer     *buffer);

gboolean   mousepad_dialogs_clear_recent        (GtkWindow         *parent);

gint       mousepad_dialogs_save_changes        (GtkWindow         *parent,
                                                 gboolean           closing,
                                                 gboolean           readonly);

gint       mousepad_dialogs_externally_modified (GtkWindow         *parent,
                                                 gboolean           saving,
                                                 gboolean           modified);

gint       mousepad_dialogs_revert              (GtkWindow         *parent);

gint       mousepad_dialogs_confirm_encoding    (const gchar       *charset,
                                                 const gchar       *user_charset);

gint       mousepad_dialogs_session_restore     (void);

gint       mousepad_dialogs_save_as             (GtkWindow         *parent,
                                                 MousepadFile      *current_file,
                                                 GFile             *last_save_location,
                                                 GFile            **file,
                                                 MousepadEncoding  *encoding);

gint       mousepad_dialogs_open                (GtkWindow         *parent,
                                                 GFile             *file,
                                                 GSList           **files,
                                                 MousepadEncoding  *encoding);

void       mousepad_dialogs_select_font         (GtkWindow         *parent);

G_END_DECLS

#endif /* !__MOUSEPAD_DIALOGS_H__ */
