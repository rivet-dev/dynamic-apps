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
#include "file.h"
#include "thumbnailer.h"
#include "main_window.h"

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#include <libxfce4util/libxfce4util.h>


enum
{
    PIXBUF_STATE_NONE,
    PIXBUF_STATE_PREVIOUS,
    PIXBUF_STATE_UPDATED,
};

enum
{
    RSTTO_FILE_SIGNAL_CHANGED = 0,
    RSTTO_FILE_SIGNAL_COUNT
};

static gint rstto_file_signals[RSTTO_FILE_SIGNAL_COUNT];



static void
rstto_file_finalize (GObject *object);



struct _RsttoFilePrivate
{
    GFile *file;

    gchar *display_name;
    gchar *content_type;
    gboolean final_content_type;

    gchar *uri;
    gchar *path;
    gchar *collate_key;

    RsttoThumbnailState thumbnail_states[RSTTO_THUMBNAIL_FLAVOR_COUNT];
    gchar *thumbnail_paths[RSTTO_THUMBNAIL_FLAVOR_COUNT];
    GdkPixbuf *pixbufs[RSTTO_THUMBNAIL_SIZE_COUNT];
    gint pixbuf_states[RSTTO_THUMBNAIL_FLAVOR_COUNT][RSTTO_THUMBNAIL_SIZE_COUNT];

    ExifData *exif_data;
    RsttoImageOrientation orientation;
    gdouble scale;
    RsttoScale auto_scale;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoFile, rstto_file, G_TYPE_OBJECT)



static void
rstto_file_init (RsttoFile *r_file)
{
    r_file->priv = rstto_file_get_instance_private (r_file);
    r_file->priv->final_content_type = FALSE;
    r_file->priv->orientation = RSTTO_IMAGE_ORIENT_NOT_DETERMINED;
    r_file->priv->scale = RSTTO_SCALE_NONE;
    r_file->priv->auto_scale = RSTTO_SCALE_NONE;
    for (gint n = 0; n < RSTTO_THUMBNAIL_FLAVOR_COUNT; n++)
        r_file->priv->thumbnail_states[n] = RSTTO_THUMBNAIL_STATE_UNPROCESSED;
}


static void
rstto_file_class_init (RsttoFileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = rstto_file_finalize;

    rstto_file_signals[RSTTO_FILE_SIGNAL_CHANGED] = g_signal_new ("changed",
            G_TYPE_FROM_CLASS (object_class),
            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0,
            NULL);
}

/**
 * rstto_file_finalize:
 * @object:
 *
 */
static void
rstto_file_finalize (GObject *object)
{
    RsttoFile *r_file = RSTTO_FILE (object);
    gint i = 0;

    if (r_file->priv->file)
    {
        g_object_unref (r_file->priv->file);
        r_file->priv->file = NULL;
    }
    if (r_file->priv->display_name)
    {
        g_free (r_file->priv->display_name);
        r_file->priv->display_name = NULL;
    }
    if (r_file->priv->content_type)
    {
        g_free (r_file->priv->content_type);
        r_file->priv->content_type = NULL;
    }
    if (r_file->priv->path)
    {
        g_free (r_file->priv->path);
        r_file->priv->path = NULL;
    }
    if (r_file->priv->uri)
    {
        g_free (r_file->priv->uri);
        r_file->priv->uri = NULL;
    }
    if (r_file->priv->collate_key)
    {
        g_free (r_file->priv->collate_key);
        r_file->priv->collate_key = NULL;
    }
    if (r_file->priv->exif_data)
    {
        exif_data_free (r_file->priv->exif_data);
        r_file->priv->exif_data = NULL;
    }

    for (i = 0; i < RSTTO_THUMBNAIL_FLAVOR_COUNT; ++i)
    {
        if (r_file->priv->thumbnail_paths[i])
        {
            g_free (r_file->priv->thumbnail_paths[i]);
            r_file->priv->thumbnail_paths[i] = NULL;
        }
    }

    for (i = 0; i < RSTTO_THUMBNAIL_SIZE_COUNT; ++i)
    {
        if (r_file->priv->pixbufs[i])
        {
            g_object_unref (r_file->priv->pixbufs[i]);
            r_file->priv->pixbufs[i] = NULL;
        }
    }

    G_OBJECT_CLASS (rstto_file_parent_class)->finalize (object);
}

/**
 * rstto_file_new:
 *
 *
 * Singleton
 */
RsttoFile *
rstto_file_new (GFile *file)
{
    RsttoFile *r_file;

    r_file = g_object_new (RSTTO_TYPE_FILE, NULL);
    r_file->priv->file = file;
    g_object_ref (file);

    return r_file;
}

gboolean
rstto_file_is_valid (RsttoFile *r_file)
{
    GtkFileFilter *filter;
    GtkFileFilterInfo filter_info;
    const gchar *content_type = NULL;

    if (! r_file->priv->final_content_type)
    {
#ifdef HAVE_MAGIC_H
        magic_t magic = magic_open (MAGIC_MIME_TYPE | MAGIC_SYMLINK);
        if (NULL != magic)
        {
            const gchar *file_path = rstto_file_get_path (r_file);
            if (NULL != file_path && magic_load (magic, NULL) == 0)
            {
                content_type = magic_file (magic, file_path);

                /* if mime type is not recognized, magic_file() returns
                 * "application/octet-stream", not NULL */
                if (g_strcmp0 (content_type, "application/octet-stream") == 0)
                    content_type = NULL;

                if (NULL != content_type)
                {
                    /* mime types that require post-processing */
                    if (g_strcmp0 (content_type, "image/x-ms-bmp") == 0)
                    {
                        /* translation for the pixbuf loader: see
                         * https://bugzilla.xfce.org/show_bug.cgi?id=13489 */
                        content_type = "image/bmp";
                    }
                    else if (g_strcmp0 (content_type, "image/x-portable-greymap") == 0)
                    {
                        /* translation for the pixbuf loader: see
                         * https://bugzilla.xfce.org/show_bug.cgi?id=14709 */
                        content_type = "image/x-portable-graymap";
                    }
                    else if (g_strcmp0 (content_type, "application/gzip") == 0)
                    {
                        /* see if it is a compressed SVG file */
                        magic_setflags (magic, magic_getflags (magic) | MAGIC_COMPRESS);
                        if (g_strcmp0 (magic_file (magic, file_path), "image/svg+xml") == 0)
                            content_type = "image/svg+xml";
                    }

                    g_free (r_file->priv->content_type);
                    r_file->priv->content_type = g_strdup (content_type);
                }
            }
            magic_close (magic);
        }
#endif

        if (NULL == content_type)
        {
            GFileInfo *file_info = g_file_query_info (
                    r_file->priv->file,
                    "standard::content-type",
                    0,
                    NULL,
                    NULL);
            if (NULL != file_info)
            {
                content_type = g_file_info_get_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
                if (NULL != content_type)
                {
                    g_free (r_file->priv->content_type);
                    r_file->priv->content_type = g_strdup (content_type);
                }
                g_object_unref (file_info);
            }
        }

        r_file->priv->final_content_type = TRUE;
    }

    filter = rstto_main_window_get_app_file_filter ();
    filter_info.contains = GTK_FILE_FILTER_MIME_TYPE;
    filter_info.mime_type = r_file->priv->content_type;

    return gtk_file_filter_filter (filter, &filter_info);
}

GFile *
rstto_file_get_file (RsttoFile *r_file)
{
    return r_file->priv->file;
}

const gchar *
rstto_file_get_display_name (RsttoFile *r_file)
{
    GFileInfo *file_info = NULL;
    const gchar *display_name;

    if (NULL == r_file->priv->display_name)
    {
        file_info = g_file_query_info (
                r_file->priv->file,
                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                0,
                NULL,
                NULL);
        if (NULL != file_info)
        {
            display_name = g_file_info_get_display_name (file_info);
            if (NULL != display_name)
            {
                r_file->priv->display_name = g_strdup (display_name);
            }
            g_object_unref (file_info);
        }
    }

    return r_file->priv->display_name;
}

const gchar *
rstto_file_get_path (RsttoFile *r_file)
{
    g_return_val_if_fail (RSTTO_IS_FILE (r_file), NULL);
    g_return_val_if_fail (G_IS_FILE (r_file->priv->file), NULL);

    if (NULL == r_file->priv->path)
    {
        r_file->priv->path = g_file_get_path (r_file->priv->file);
    }
    return r_file->priv->path;
}

const gchar *
rstto_file_get_uri (RsttoFile *r_file)
{
    if (NULL == r_file->priv->uri)
    {
        r_file->priv->uri = g_file_get_uri (r_file->priv->file);
    }
    return r_file->priv->uri;
}

const gchar *
rstto_file_get_collate_key (RsttoFile *r_file)
{
    if (NULL == r_file->priv->collate_key)
    {
        gchar *basename = g_file_get_basename (rstto_file_get_file (r_file));
        if (NULL != basename)
        {
            if (g_utf8_validate (basename, -1, NULL))
            {
                /* If we can use casefold for case insenstivie sorting, then
                 * do so */
                gchar *casefold = g_utf8_casefold (basename, -1);
                if (NULL != casefold)
                {
                    r_file->priv->collate_key = g_utf8_collate_key_for_filename (casefold, -1);
                    g_free (casefold);
                }
                else
                {
                    r_file->priv->collate_key = g_utf8_collate_key_for_filename (basename, -1);
                }
            }
            else
            {
                r_file->priv->collate_key = g_strdup (basename);
            }
            g_free (basename);
        }
    }
    return r_file->priv->collate_key;
}

const gchar *
rstto_file_get_content_type (RsttoFile *r_file)
{
    return r_file->priv->content_type;
}

void
rstto_file_set_content_type (RsttoFile *r_file,
                             const gchar *type)
{
    g_free (r_file->priv->content_type);
    r_file->priv->content_type = g_strdup (type);
}

guint64
rstto_file_get_modified_time (RsttoFile *r_file)
{
    guint64 time_ = 0;
    GFileInfo *file_info = g_file_query_info (r_file->priv->file, "time::modified", 0, NULL, NULL);

    time_ = g_file_info_get_attribute_uint64 (file_info, "time::modified");

    g_object_unref (file_info);

    return time_;
}

goffset
rstto_file_get_size (RsttoFile *r_file)
{
    goffset size = 0;
    GFileInfo *file_info = g_file_query_info (r_file->priv->file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, NULL, NULL);

    size = g_file_info_get_size (file_info);

    g_object_unref (file_info);

    return size;
}

ExifEntry *
rstto_file_get_exif (RsttoFile *r_file, ExifTag id)
{
    /* If there is no exif-data object, try to create it */
    if (NULL == r_file->priv->exif_data)
    {
        r_file->priv->exif_data = exif_data_new_from_file (
                rstto_file_get_path (r_file));
    }

    if (NULL != r_file->priv->exif_data)
    {
        return exif_data_get_entry (
                r_file->priv->exif_data,
                id);
    }

    /* If there is no exif-data, return NULL */
    return NULL;
}

RsttoImageOrientation
rstto_file_get_orientation (RsttoFile *r_file)
{
    ExifEntry *exif_entry = NULL;
    if (r_file->priv->orientation == RSTTO_IMAGE_ORIENT_NOT_DETERMINED)
    {
        /* Try to get the default orientation from the EXIF tag */
        exif_entry = rstto_file_get_exif (r_file, EXIF_TAG_ORIENTATION);
        if (NULL != exif_entry)
        {
            r_file->priv->orientation = exif_get_short (
                    exif_entry->data,
                    exif_data_get_byte_order (exif_entry->parent->parent));
        }

        /* If the orientation-tag is not set, default to NONE */
        if (r_file->priv->orientation == RSTTO_IMAGE_ORIENT_NOT_DETERMINED)
        {
            /* Default orientation */
            r_file->priv->orientation = RSTTO_IMAGE_ORIENT_NONE;
        }

    }
    return r_file->priv->orientation;
}

void
rstto_file_set_orientation (
        RsttoFile *r_file,
        RsttoImageOrientation orientation)
{
    r_file->priv->orientation = orientation;
}

gdouble
rstto_file_get_scale (RsttoFile *r_file)
{
    return r_file->priv->scale;
}

void
rstto_file_set_scale (RsttoFile *r_file,
                      gdouble scale)
{
    r_file->priv->scale = scale;
}

RsttoScale
rstto_file_get_auto_scale (RsttoFile *r_file)
{
    return r_file->priv->auto_scale;
}

void
rstto_file_set_auto_scale (RsttoFile *r_file,
                           RsttoScale auto_scale)
{
    r_file->priv->auto_scale = auto_scale;
}

gboolean
rstto_file_has_exif (RsttoFile *r_file)
{
    if (NULL == r_file->priv->exif_data)
    {
        r_file->priv->exif_data = exif_data_new_from_file (rstto_file_get_path (r_file));
    }
    if (NULL == r_file->priv->exif_data)
    {
        return FALSE;
    }
    return TRUE;
}

void
rstto_file_set_thumbnail_state (RsttoFile *r_file,
                                RsttoThumbnailFlavor flavor,
                                RsttoThumbnailState state)
{
    r_file->priv->thumbnail_states[flavor] = state;
}

static const gchar *
rstto_file_get_thumbnail_path (RsttoFile *r_file,
                               RsttoThumbnailFlavor flavor)
{
    const gchar *uri, *flavor_name;
    gchar *path, *checksum, *filename;

    if (r_file->priv->thumbnail_paths[flavor] == NULL)
    {
        flavor_name = rstto_util_get_thumbnail_flavor_name (flavor);
        uri = rstto_file_get_uri (r_file);
        checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, strlen (uri));
        filename = g_strconcat (checksum, ".png", NULL);

        /* build and check if the thumbnail is in the new location */
        path = g_build_path ("/", g_get_user_cache_dir (), "thumbnails",
                             flavor_name, filename, NULL);
        if (! g_file_test (path, G_FILE_TEST_EXISTS))
        {
            /* fallback to old version */
            g_free (path);
            path = g_build_path ("/", g_get_home_dir (), ".thumbnails",
                                 flavor_name, filename, NULL);
            if (! g_file_test (path, G_FILE_TEST_EXISTS))
            {
#if LIBXFCE4UTIL_CHECK_VERSION (4, 17, 1)
                /* fallback to shared repository */
                g_free (path);
                path = xfce_create_shared_thumbnail_path (uri, flavor_name);
                if (path == NULL || ! g_file_test (path, G_FILE_TEST_EXISTS))
#endif
                {
                    /* thumbnail doesn't exist in either spot */
                    g_free (path);
                    path = NULL;
                }
            }
        }

        g_free (checksum);
        g_free (filename);

        r_file->priv->thumbnail_paths[flavor] = path;
    }

    return r_file->priv->thumbnail_paths[flavor];
}

const GdkPixbuf *
rstto_file_get_thumbnail (RsttoFile *r_file,
                          RsttoThumbnailSize size)
{
    RsttoThumbnailer *thumbnailer;
    RsttoThumbnailFlavor flavor;
    GError *error = NULL;
    const gchar *thumbnail_path;
    guint n_pixels;

    /* get the flavor, trying to fall back on lower qualities if necessary */
    flavor = rstto_util_get_thumbnail_flavor (size);
    while (r_file->priv->thumbnail_states[flavor] == RSTTO_THUMBNAIL_STATE_ERROR
           && flavor > RSTTO_THUMBNAIL_FLAVOR_NORMAL)
        flavor--;

    switch (r_file->priv->thumbnail_states[flavor])
    {
        case RSTTO_THUMBNAIL_STATE_PROCESSED:
            if (r_file->priv->pixbuf_states[flavor][size] == PIXBUF_STATE_UPDATED)
                return r_file->priv->pixbufs[size];
            else
                r_file->priv->pixbuf_states[flavor][size] = PIXBUF_STATE_UPDATED;

            break;

        case RSTTO_THUMBNAIL_STATE_IN_PROCESS:
            if (r_file->priv->pixbuf_states[flavor][size] == PIXBUF_STATE_PREVIOUS)
                return r_file->priv->pixbufs[size];
            else
                r_file->priv->pixbuf_states[flavor][size] = PIXBUF_STATE_PREVIOUS;

            break;

        case RSTTO_THUMBNAIL_STATE_UNPROCESSED:
            thumbnailer = rstto_thumbnailer_new ();
            rstto_thumbnailer_queue_file (thumbnailer, flavor, r_file);
            g_object_unref (thumbnailer);

            r_file->priv->thumbnail_states[flavor] = RSTTO_THUMBNAIL_STATE_IN_PROCESS;
            for (gint i = 0; i < RSTTO_THUMBNAIL_SIZE_COUNT; i++)
                r_file->priv->pixbuf_states[flavor][i] = PIXBUF_STATE_NONE;

            r_file->priv->pixbuf_states[flavor][size] = PIXBUF_STATE_PREVIOUS;
            break;

        case RSTTO_THUMBNAIL_STATE_ERROR:
            return r_file->priv->pixbufs[size];
            break;

        default:
            g_warn_if_reached ();
            return NULL;
    }

    thumbnail_path = rstto_file_get_thumbnail_path (r_file, flavor);
    if (thumbnail_path == NULL)
    {
        if (r_file->priv->thumbnail_states[flavor] == RSTTO_THUMBNAIL_STATE_PROCESSED)
            g_warn_if_reached ();

        return NULL;
    }

    if (r_file->priv->pixbufs[size] != NULL)
        g_object_unref (r_file->priv->pixbufs[size]);

    n_pixels = rstto_util_get_thumbnail_n_pixels (size);
    r_file->priv->pixbufs[size] =
        gdk_pixbuf_new_from_file_at_scale (thumbnail_path, n_pixels, n_pixels, TRUE, &error);
    if (error != NULL)
    {
        g_warning ("Thumbnail pixbuf generation failed for '%s': %s",
                   r_file->priv->uri, error->message);
        g_error_free (error);
    }

    return r_file->priv->pixbufs[size];
}

void
rstto_file_changed (RsttoFile *r_file)
{
    if (r_file->priv->content_type != NULL)
    {
        g_free (r_file->priv->content_type);
        r_file->priv->content_type = NULL;
    }

    if (r_file->priv->exif_data != NULL)
    {
        exif_data_free (r_file->priv->exif_data);
        r_file->priv->exif_data = NULL;
    }

    r_file->priv->final_content_type = FALSE;
    r_file->priv->orientation = RSTTO_IMAGE_ORIENT_NOT_DETERMINED;
    r_file->priv->scale = RSTTO_SCALE_NONE;
    for (gint n = 0; n < RSTTO_THUMBNAIL_FLAVOR_COUNT; n++)
        r_file->priv->thumbnail_states[n] = RSTTO_THUMBNAIL_STATE_UNPROCESSED;

    if (rstto_file_is_valid (r_file))
    {
        /* this will send a request to the thumbnailer, which will trigger the necessary
         * updates with its "ready" signal */
        rstto_file_get_thumbnail (r_file, RSTTO_THUMBNAIL_SIZE_LARGE);

        g_signal_emit (r_file, rstto_file_signals[RSTTO_FILE_SIGNAL_CHANGED], 0, NULL);
    }
    else
        rstto_image_list_remove_file (rstto_main_window_get_app_image_list (), r_file);
}
