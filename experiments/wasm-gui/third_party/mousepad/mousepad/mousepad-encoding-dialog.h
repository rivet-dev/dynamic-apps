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

#ifndef __MOUSEPAD_ENCODING_DIALOG_H__
#define __MOUSEPAD_ENCODING_DIALOG_H__

G_BEGIN_DECLS

#include <mousepad/mousepad-encoding.h>

#define MOUSEPAD_TYPE_ENCODING_DIALOG (mousepad_encoding_dialog_get_type ())
G_DECLARE_FINAL_TYPE (MousepadEncodingDialog, mousepad_encoding_dialog, MOUSEPAD, ENCODING_DIALOG, GtkDialog)

gint  mousepad_encoding_dialog          (GtkWindow        *parent,
                                         MousepadFile     *file,
                                         gboolean          valid,
                                         MousepadEncoding *encoding);

G_END_DECLS

#endif /* !__MOUSEPAD_ENCODING_DIALOG_H__ */
