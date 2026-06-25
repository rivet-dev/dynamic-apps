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

#ifndef __RISTRETTO_MIME_DB_H__
#define __RISTRETTO_MIME_DB_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RSTTO_TYPE_MIME_DB rstto_mime_db_get_type ()
G_DECLARE_FINAL_TYPE (RsttoMimeDB, rstto_mime_db, RSTTO, MIME_DB, GObject)

typedef struct _RsttoMimeDBPrivate RsttoMimeDBPrivate;

struct _RsttoMimeDB
{
    GObject parent;
    RsttoMimeDBPrivate *priv;
};



RsttoMimeDB *
rstto_mime_db_new (const gchar *path,
                   GError **error);

const gchar *
rstto_mime_db_lookup (RsttoMimeDB *mime_db,
                      const gchar *key);

void
rstto_mime_db_store (RsttoMimeDB *mime_db,
                     const gchar *key,
                     const gchar *value);

G_END_DECLS

#endif /* __RSTTO_MIME_DB_H__ */
