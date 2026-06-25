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

#ifndef __MOUSEPAD_DOCUMENT_H__
#define __MOUSEPAD_DOCUMENT_H__

#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-view.h>

G_BEGIN_DECLS

typedef struct _MousepadDocumentPrivate MousepadDocumentPrivate;

#define MOUSEPAD_TYPE_DOCUMENT (mousepad_document_get_type ())
G_DECLARE_FINAL_TYPE (MousepadDocument, mousepad_document, MOUSEPAD, DOCUMENT, GtkScrolledWindow)

#define MOUSEPAD_TYPE_SEARCH_FLAGS (mousepad_document_search_flags_get_type ())

typedef enum
{
  /* search area */
  MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA        = 1 << 0,  /* search the whole area */
  MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION     = 1 << 1,  /* search inside selection */
  MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS = 1 << 2,  /* search all documents */

  /* iter start point */
  MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START     = 1 << 3,  /* start at from the beginning of the selection */
  MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END       = 1 << 4,  /* start at from the end of the selection */

  /* direction */
  MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD        = 1 << 5,  /* search forward to end of area */
  MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD       = 1 << 6,  /* search backwards to start of area */
  MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND        = 1 << 7,  /* used only by the search bar */

  /* actions */
  MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT      = 1 << 8,  /* select the match */
  MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE     = 1 << 9,  /* replace the match */
  MOUSEPAD_SEARCH_FLAGS_ACTION_NONE        = 1 << 10, /* silent search */
}
MousepadSearchFlags;

GType mousepad_document_search_flags_get_type (void) G_GNUC_CONST;

struct _MousepadDocument
{
  GtkScrolledWindow        __parent__;

  /* private structure */
  MousepadDocumentPrivate *priv;

  /* file */
  MousepadFile            *file;

  /* text buffer */
  GtkTextBuffer           *buffer;

  /* text view */
  MousepadView            *textview;
};

MousepadDocument *mousepad_document_new            (void);

void              mousepad_document_set_overwrite  (MousepadDocument    *document,
                                                    gboolean             overwrite);

void              mousepad_document_focus_textview (MousepadDocument    *document);

void              mousepad_document_send_signals   (MousepadDocument    *document);

GtkWidget        *mousepad_document_get_tab_label  (MousepadDocument    *document);

const gchar      *mousepad_document_get_basename   (MousepadDocument    *document);

const gchar      *mousepad_document_get_filename   (MousepadDocument    *document);

gboolean          mousepad_document_get_word_wrap  (MousepadDocument    *document);

void              mousepad_document_search         (MousepadDocument    *document,
                                                    const gchar         *string,
                                                    const gchar         *replace,
                                                    MousepadSearchFlags  flags);

G_END_DECLS

#endif /* !__MOUSEPAD_DOCUMENT_H__ */
