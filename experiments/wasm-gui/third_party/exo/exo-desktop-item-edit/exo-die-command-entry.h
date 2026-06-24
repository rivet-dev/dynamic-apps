/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __EXO_DIE_COMMAND_ENTRY_H__
#define __EXO_DIE_COMMAND_ENTRY_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ExoDieCommandEntryClass ExoDieCommandEntryClass;
typedef struct _ExoDieCommandEntry      ExoDieCommandEntry;

#define EXO_DIE_TYPE_COMMAND_ENTRY            (exo_die_command_entry_get_type ())
#define EXO_DIE_COMMAND_ENTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_DIE_TYPE_COMMAND_ENTRY, ExoDieCommandEntry))
#define EXO_DIE_COMMAND_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_DIE_TYPE_COMMAND_ENTRY, ExoDieCommandEntryClass))
#define EXO_DIE_IS_COMMAND_ENTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_DIE_TYPE_COMMAND_ENTRY))
#define EXO_DIE_IS_COMMAND_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_DIE_TYPE_COMMAND_ENTRY))
#define EXO_DIE_COMMAND_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_DIE_TYPE_COMMAND_ENTRY, ExoDieCommandEntryClass))

GType        exo_die_command_entry_get_type (void) G_GNUC_CONST;

GtkWidget   *exo_die_command_entry_new      (void) G_GNUC_MALLOC;

const gchar *exo_die_command_entry_get_text (ExoDieCommandEntry *command_entry);
void         exo_die_command_entry_set_text (ExoDieCommandEntry *command_entry,
                                             const gchar        *text);

G_END_DECLS;

#endif /* !__EXO_DIE_COMMAND_ENTRY_H__ */
