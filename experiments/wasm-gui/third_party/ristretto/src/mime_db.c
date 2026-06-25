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

#include "util.h"
#include "mime_db.h"

#include <libxfce4util/libxfce4util.h>



enum
{
    PROP_0,
    PROP_FILE
};



static void
rstto_mime_db_finalize (GObject *object);
static void
rstto_mime_db_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec);
static void
rstto_mime_db_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec);



struct _RsttoMimeDBPrivate
{
    GFile   *file;
    XfceRc  *rc;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoMimeDB, rstto_mime_db, G_TYPE_OBJECT)



static void
rstto_mime_db_init (RsttoMimeDB *mime_db)
{
    mime_db->priv = rstto_mime_db_get_instance_private (mime_db);
}

static void
rstto_mime_db_class_init (RsttoMimeDBClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = rstto_mime_db_finalize;

    object_class->set_property = rstto_mime_db_set_property;
    object_class->get_property = rstto_mime_db_get_property;
}

/**
 * rstto_mime_db_finalize:
 * @object:
 *
 */
static void
rstto_mime_db_finalize (GObject *object)
{
    RsttoMimeDB *mime_db = RSTTO_MIME_DB (object);

    xfce_rc_close (mime_db->priv->rc);

    G_OBJECT_CLASS (rstto_mime_db_parent_class)->finalize (object);
}

static void
rstto_mime_db_set_property (
        GObject      *object,
        guint         property_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    switch (property_id)
    {
        case PROP_FILE:
            break;
        default:
            break;
    }
}

static void
rstto_mime_db_get_property (
        GObject    *object,
        guint       property_id,
        GValue     *value,
        GParamSpec *pspec)
{

    switch (property_id)
    {
        case PROP_FILE:
            break;
        default:
            break;
    }

}

RsttoMimeDB *
rstto_mime_db_new (const gchar *path, GError **error)
{
    RsttoMimeDB   *mime_db = NULL;
    XfceRc        *rc = NULL;

    rc = xfce_rc_simple_open (path, FALSE);

    if (rc != NULL)
    {
        mime_db = g_object_new (RSTTO_TYPE_MIME_DB, NULL);
        mime_db->priv->rc = rc;

        xfce_rc_set_group (rc, "preferred-editor");
    }

    return mime_db;
}


const gchar *
rstto_mime_db_lookup (
        RsttoMimeDB *mime_db,
        const gchar *key)
{
    return xfce_rc_read_entry (mime_db->priv->rc, key, NULL);
}


void
rstto_mime_db_store (
        RsttoMimeDB *mime_db,
        const gchar *key,
        const gchar *value)
{
    xfce_rc_write_entry (mime_db->priv->rc, key, value);
    xfce_rc_flush (mime_db->priv->rc);
}

