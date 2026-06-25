/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2016 Simon Steinbeiß <ochosi@xfce.org>
 *  Copyright (c) 2023 Brian Tarricone <brian@tarricone.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxfce4util/libxfce4util.h>

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "xfce-notify-common.h"
#include "xfce-notify-log-util.h"

#ifndef P_
#define P_(singular, plural, n) ngettext(singular, plural, n)
#endif

static void
notify_free (guchar *pixels,
             gpointer data)
{
  g_free (pixels);
}

GdkPixbuf *
notify_pixbuf_from_image_data (GVariant *image_data)
{
    GdkPixbuf *pix = NULL;
    gint32 width, height, rowstride, bits_per_sample, channels;
    gboolean has_alpha;
    GVariant *pixel_data;
    gsize correct_len;
    guchar *data;

    if (!g_variant_is_of_type (image_data, G_VARIANT_TYPE ("(iiibiiay)")))
    {
        g_warning ("Image data is not the correct type");
        return NULL;
    }

    g_variant_get (image_data,
                   "(iiibii@ay)",
                   &width,
                   &height,
                   &rowstride,
                   &has_alpha,
                   &bits_per_sample,
                   &channels,
                   &pixel_data);

    correct_len = (height - 1) * rowstride + width
                  * ((channels * bits_per_sample + 7) / 8);
    if(correct_len != g_variant_get_size (pixel_data)) {
        g_message ("Pixel data length (%lu) did not match expected value (%u)",
                   g_variant_get_size (pixel_data), (guint)correct_len);
        return NULL;
    }

    data = (guchar *)g_memdup2(g_variant_get_data(pixel_data),
                               g_variant_get_size(pixel_data));
    g_variant_unref(pixel_data);

    if (data != NULL) {
        pix = gdk_pixbuf_new_from_data(data,
                                       GDK_COLORSPACE_RGB, has_alpha,
                                       bits_per_sample, width, height,
                                       rowstride,
                                       notify_free, NULL);
    }

    return pix;
}

static gchar *
notify_read_from_desktop_file (const gchar *desktop_file_path, const gchar *key)
{
    GKeyFile *desktop_file;
    gchar *value = NULL;

    g_return_val_if_fail (g_path_is_absolute (desktop_file_path), NULL);

    desktop_file = g_key_file_new ();
    if (g_key_file_load_from_file (desktop_file,
                                   desktop_file_path,
                                   G_KEY_FILE_NONE,
                                   NULL)) {
        if (g_key_file_has_group (desktop_file, G_KEY_FILE_DESKTOP_GROUP)) {
            if (g_key_file_has_key (desktop_file,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    key,
                                    NULL))
                {
                    value = g_key_file_get_value (desktop_file,
                                                      G_KEY_FILE_DESKTOP_GROUP,
                                                      key,
                                                      NULL);
                }
        }
        g_key_file_free (desktop_file);
    }

    return value;
}

static gchar *
notify_get_from_desktop_file_resolved(const gchar *desktop_file, const gchar *key)
{
    GDesktopAppInfo *appinfo = g_desktop_app_info_new(desktop_file);

    if (appinfo != NULL) {
        gchar *value = notify_read_from_desktop_file(g_desktop_app_info_get_filename(appinfo), key);
        g_object_unref(appinfo);
        return value;
    } else {
        return NULL;
    }
}

gchar *
notify_get_from_desktop_file (const gchar *desktop_file, const gchar *key)
{
    gchar *value = NULL;
    gchar *filename;

    filename = g_strdup_printf ("%s.desktop", desktop_file);
    value = notify_get_from_desktop_file_resolved(filename, key);
    g_free (filename);

    /* Fallback: Try to find the correct desktop file
       As the GIO matching algorithm is unknown and subject to change we naively pick the first match */
    if (value == NULL) {
        gchar ***matches = g_desktop_app_info_search (desktop_file);

        if (matches != NULL) {
            for (gsize i = 0; matches[i] != NULL; ++i) {
                if (value == NULL) {
                    for (gsize j = 0; matches[i][j] != NULL; ++j) {
                        value = notify_get_from_desktop_file_resolved(matches[i][j], key);
                        if (value != NULL) {
                            break;
                        }
                    }
                }

                g_strfreev(matches[i]);
            }

            g_free(matches);
        }
    }

    return value;
}

const gchar *
xfce_notify_log_get_icon_folder(void) {
    static gchar *folder = NULL;

    if (G_UNLIKELY(folder == NULL)) {
        folder = g_strconcat(g_get_user_cache_dir(), G_DIR_SEPARATOR_S,
                             "xfce4", G_DIR_SEPARATOR_S,
                             "notifyd", G_DIR_SEPARATOR_S,
                             "icons",
                             NULL);
    }

    return folder;
}

gchar *
xfce_notify_log_cache_icon(GVariant *v_image_data,
                           const gchar *image_path,
                           const gchar *app_icon,
                           const gchar *desktop_id)
{
    const gchar *notify_log_icon_folder = xfce_notify_log_get_icon_folder();

    if (v_image_data != NULL) {
        GBytes *image_bytes;
        gchar *icon_name;
        GdkPixbuf *pixbuf;

        image_bytes = g_variant_get_data_as_bytes (v_image_data);
        icon_name = g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, image_bytes);
        g_bytes_unref(image_bytes);
        pixbuf = notify_pixbuf_from_image_data (v_image_data);
        if (pixbuf) {
            gchar *notify_log_icon_path = g_strconcat (notify_log_icon_folder, G_DIR_SEPARATOR_S, icon_name, ".png", NULL);
            if (!g_file_test (notify_log_icon_path, G_FILE_TEST_EXISTS)) {
                if (!gdk_pixbuf_save (pixbuf, notify_log_icon_path, "png", NULL, NULL)) {
                    g_warning ("Could not save the pixbuf to: %s", notify_log_icon_path);
                }
            }
            g_free(notify_log_icon_path);
            g_object_unref (G_OBJECT (pixbuf));
        }
        return icon_name;
    } else if (image_path != NULL) {
        /* If the image path is in the tmp directory we copy it to the cache directory to make it persistent
           (e.g. Chrome/Chromium uses the tmp directory to store and reference icons, see https://bugzilla.xfce.org/show_bug.cgi?id=15215)*/
        gchar *image_dir = g_path_get_dirname (image_path);
        gboolean is_in_tmp = g_strcmp0 ("/tmp", image_dir) == 0;

        g_free(image_dir);

        if (is_in_tmp) {
            gchar *image_data = NULL;
            gsize image_data_size = 0;
            if (g_file_get_contents (image_path, &image_data, &image_data_size, NULL)) {
                /* TODO: convert the image to PNG if it isn't a PNG image */
                gchar *image_data_sha1 = g_compute_checksum_for_data(G_CHECKSUM_SHA1, (const guchar *)image_data, image_data_size);
                gchar *filename = g_strconcat (notify_log_icon_folder, G_DIR_SEPARATOR_S, image_data_sha1, ".png", NULL);
                if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
                    if (!g_file_set_contents (filename, image_data, image_data_size, NULL)) {
                        g_warning ("Failed to copy the image from /tmp to the cache directory: %s", filename);
                    }
                }
                g_free (filename);
                g_free (image_data);

                return image_data_sha1;
            } else {
                g_warning ("Could not read image: %s", image_path);
                return NULL;
            }
        } else {
            return g_strdup(image_path);
        }
    } else if (app_icon && (g_strcmp0 (app_icon, "") != 0)) {
        return g_strdup(app_icon);
    } else if (desktop_id) {
        gchar *icon_name = notify_get_from_desktop_file (desktop_id, G_KEY_FILE_DESKTOP_KEY_ICON);

        if (icon_name) {
            return icon_name;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

/* Returns NULL if unable to determine the icon cache size */
static gchar *
xfce_notify_get_icon_cache_size (void)
{
    gchar *notify_icon_cache_path;

    notify_icon_cache_path = xfce_resource_save_location (XFCE_RESOURCE_CACHE,
                                                          XFCE_NOTIFY_ICON_PATH, FALSE);
    if (notify_icon_cache_path)
    {
        GFile *icon_folder;
        guint64 disk_usage, num_files;
        gboolean status;

        icon_folder = g_file_new_for_path (notify_icon_cache_path);
        g_free (notify_icon_cache_path);
        notify_icon_cache_path = NULL;

        status = g_file_measure_disk_usage (icon_folder,
                                   G_FILE_MEASURE_NONE,
                                   NULL, NULL, NULL,
                                   &disk_usage,
                                   NULL,
                                   &num_files,
                                   NULL);
        g_object_unref (icon_folder);
        icon_folder = NULL;

        if (status == TRUE) {
            return g_strdup_printf ("%d icons / %.1f MB", (int) num_files, disk_usage / 1e6);
        }
    }

    return NULL;
}

static void xfce_notify_clear_icon_cache (void)
{
    gchar *notify_icon_cache_path = NULL;

    notify_icon_cache_path = xfce_resource_save_location (XFCE_RESOURCE_CACHE,
                                                          XFCE_NOTIFY_ICON_PATH, FALSE);

    if (notify_icon_cache_path)
    {
        GFile *icon_folder;
        GFileEnumerator *folder_contents;


        icon_folder = g_file_new_for_path (notify_icon_cache_path);
        folder_contents = g_file_enumerate_children (icon_folder,
                                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                     G_FILE_QUERY_INFO_NONE,
                                                     NULL,
                                                     NULL);
        /* Iterate over the folder and delete each file */
        while (TRUE)
        {
            GFile *icon_file;
            if (!g_file_enumerator_iterate (folder_contents, NULL, &icon_file, NULL, NULL))
                goto out;
            if (!icon_file)
                break;
            if (!g_file_delete (icon_file, NULL, NULL))
                g_warning ("Could not delete a notification icon: %s", notify_icon_cache_path);
        }
        out:
            g_object_unref (folder_contents);

        /* Delete the empty folder */
        if (!g_file_delete (icon_folder, NULL, NULL))
            g_warning ("Could not delete the notification icon cache: %s", notify_icon_cache_path);

        g_object_unref (icon_folder);
        g_free (notify_icon_cache_path);
    }
}

typedef struct {
    XfceNotifyLogGBus *log;
    GtkWidget *include_log;
} ClearLogResponseData;


static void
xfce_notify_clear_log_dialog_cb(GtkWidget *dialog, gint response, ClearLogResponseData *rdata) {
    if (response != GTK_RESPONSE_DELETE_EVENT && response != GTK_RESPONSE_CANCEL) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rdata->include_log))) {
            xfce_notify_clear_icon_cache();
        }
        xfce_notify_log_gbus_call_clear(rdata->log, NULL, NULL, NULL);
    }
}

static void
notify_closure_free(gpointer data,
                    GClosure *closure)
{
    g_free(data);
}

GtkWidget *
xfce_notify_clear_log_dialog(XfceNotifyLogGBus *log, GtkWindow *parent) {
    GtkWidget *dialog, *grid, *icon, *label, *content_area, *checkbutton;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;
    gchar *icon_cache_size;
    const char *str = _("Do you really want to clear the notification log?");
    const char *format = "<span weight='bold' size='large'>%s</span>";
    char *markup;
    ClearLogResponseData *rdata;

    dialog = gtk_dialog_new_with_buttons (_("Clear notification log"),
                                          parent,
                                          flags,
                                          _("Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("Clear"),
                                          GTK_RESPONSE_OK,
                                          NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_widget_set_margin_start (grid, 12);
    gtk_widget_set_margin_end (grid, 12);
    gtk_widget_set_margin_top (grid, 12);
    gtk_widget_set_margin_bottom (grid, 12);

    icon = gtk_image_new_from_icon_name ("edit-clear", GTK_ICON_SIZE_DIALOG);

    icon_cache_size = xfce_notify_get_icon_cache_size ();
    if (icon_cache_size)
    {
        gchar *message = g_strdup_printf ("%s (%s)", _("include icon cache"), icon_cache_size);
        g_free (icon_cache_size);
        icon_cache_size = NULL;
        checkbutton = gtk_check_button_new_with_label (message);
        g_free (message);
    }
    else
    {
        checkbutton = gtk_check_button_new_with_label (_("include icon cache"));
    }
    label = gtk_label_new (NULL);
    markup = g_markup_printf_escaped (format, str);
    gtk_label_set_markup (GTK_LABEL (label), markup);
    g_free (markup);

    gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 2);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), checkbutton, 1, 1, 1, 1);

    gtk_container_add (GTK_CONTAINER (content_area), grid);
    gtk_widget_show_all (dialog);

    GtkWidget *cancel = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    gtk_widget_grab_focus(cancel);

    rdata = g_new0(ClearLogResponseData, 1);
    rdata->log = log;
    rdata->include_log = checkbutton;
    g_signal_connect_data(dialog, "response",
                          G_CALLBACK(xfce_notify_clear_log_dialog_cb), rdata,
                          notify_closure_free, 0);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), "edit-clear");

    return dialog;
}

static gchar *
notify_log_format_timestamp_relative(GDateTime *timestamp) {
    gchar *formatted = NULL;
    GDateTime *now = g_date_time_new_now_local();
    gint64 now_s = g_date_time_to_unix(now);
    gint64 timestamp_s = g_date_time_to_unix(timestamp);
    gint diff = MAX(0, now_s - timestamp_s);

    if (diff == 0) {
        formatted = g_strdup(_("Now"));
    } else if (diff < 60) {  // one minute
        formatted = g_strdup_printf(P_("%d second ago", "%d seconds ago", diff), diff);
    } else if (diff < 3600) {  // one hour
        formatted = g_strdup_printf(P_("%d minute ago", "%d minutes ago", diff / 60), diff / 60);
    } else if (diff < 86400) {  // one day
        formatted = g_strdup_printf(P_("%d hour ago", "%d hours ago", diff / 3600), diff / 3600);
    } else if (diff < 604800) { // one week
        formatted = g_strdup_printf(P_("%d day ago", "%d days ago", diff / 86400), diff / 86400);
    } else {
        formatted = g_date_time_format_iso8601(timestamp);
    }

    g_date_time_unref(now);

    return formatted;
}

gchar *
notify_log_format_timestamp(GDateTime *timestamp, XfceNotifyDatetimeFormat format, const gchar *custom_format) {
    gchar *formatted = NULL;
    GDateTime *local_timestamp = g_date_time_to_local(timestamp);

    if (G_UNLIKELY(local_timestamp == NULL)) {
        local_timestamp = g_date_time_ref(timestamp);
    }

    if (G_UNLIKELY(format < XFCE_NOTIFY_DATETIME_LOCALE_DEFAULT || format > XFCE_NOTIFY_DATETIME_CUSTOM)) {
        g_warning("Invalid datetime format %d; using default", format);
        format = XFCE_NOTIFY_DATETIME_LOCALE_DEFAULT;
    }

    if (G_UNLIKELY(format == XFCE_NOTIFY_DATETIME_CUSTOM && (custom_format == NULL || custom_format[0] == '\0'))) {
        g_warning("Custom format requested, but no custom format provided; using default");
        format = XFCE_NOTIFY_DATETIME_LOCALE_DEFAULT;
    }

    switch (format) {
        case XFCE_NOTIFY_DATETIME_LOCALE_DEFAULT:
            formatted = g_date_time_format(local_timestamp, "%c");
            break;
        case XFCE_NOTIFY_DATETIME_ISO8601:
            formatted = g_date_time_format_iso8601(local_timestamp);
            break;
        case XFCE_NOTIFY_DATETIME_RELATIVE_TIMES:
            formatted = notify_log_format_timestamp_relative(local_timestamp);
            break;
        case XFCE_NOTIFY_DATETIME_CUSTOM:
            formatted = g_date_time_format(local_timestamp, custom_format);
            break;
        default:
            g_assert_not_reached();
            break;
    }

    g_date_time_unref(local_timestamp);

    return formatted;
}

gchar *
notify_log_format_summary(const gchar *summary) {
    return g_markup_printf_escaped("<b>%s</b>", summary);
}

gchar *
notify_log_format_body(const gchar *body) {
    if (body == NULL || body[0] == '\0') {
        return NULL;
    } else {
        return xfce_notify_sanitize_markup(body);
    }
}

cairo_surface_t *
notify_log_load_icon(const gchar *notify_log_icon_folder,
                     const gchar *icon_id,
                     const gchar *app_id,
                     gint size,
                     gint scale)
{
    cairo_surface_t *surface = NULL;
    GdkPixbuf *pixbuf = NULL;
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

    if (icon_id != NULL && icon_id[0] != '\0') {
        if (g_path_is_absolute(icon_id)
            && g_file_test(icon_id, G_FILE_TEST_EXISTS)
            && !g_file_test(icon_id, G_FILE_TEST_IS_DIR))
        {
            pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_id, size * scale, size * scale, TRUE, NULL);
        }

        if (pixbuf == NULL) {
            gchar *icon_path = g_strconcat(notify_log_icon_folder, G_DIR_SEPARATOR_S, icon_id, ".png", NULL);

            if (g_file_test(icon_path, G_FILE_TEST_EXISTS) && !g_file_test(icon_path, G_FILE_TEST_IS_DIR)) {
                pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_path, size * scale, size * scale, TRUE, NULL);
            }

            if (pixbuf == NULL && gtk_icon_theme_has_icon(icon_theme, icon_id)) {
                pixbuf = gtk_icon_theme_load_icon_for_scale(icon_theme,
                                                            icon_id,
                                                            size,
                                                            scale,
                                                            GTK_ICON_LOOKUP_FORCE_SIZE,
                                                            NULL);
            }

            g_free(icon_path);
        }
    }

    if (pixbuf == NULL && app_id != NULL && app_id[0] != '\0') {
        gchar *app_icon_name = notify_get_from_desktop_file(app_id, "Icon");

        if (app_icon_name != NULL) {
            if (g_path_is_absolute(app_icon_name)
                && g_file_test(app_icon_name, G_FILE_TEST_EXISTS)
                && !g_file_test(app_icon_name, G_FILE_TEST_IS_DIR))
            {
                pixbuf = gdk_pixbuf_new_from_file_at_scale(app_icon_name, size * scale, size * scale, TRUE, NULL);
            }

            if (pixbuf == NULL && gtk_icon_theme_has_icon(icon_theme, app_icon_name)) {
                pixbuf = gtk_icon_theme_load_icon_for_scale(icon_theme,
                                                            app_icon_name,
                                                            size,
                                                            scale,
                                                            GTK_ICON_LOOKUP_FORCE_SIZE,
                                                            NULL);
            }
        }

        g_free(app_icon_name);
    }

    if (pixbuf != NULL) {
        surface = gdk_cairo_surface_create_from_pixbuf(pixbuf, scale, NULL);
        g_object_unref(pixbuf);
    }

    return surface;
}

static void
draw_unread_emblem_fallback(cairo_surface_t *surface,
                            GtkStyleContext *style_context,
                            gint icon_size,
                            gdouble alpha)
{
  GdkRGBA color;
  cairo_t *cr;

  if (!gtk_style_context_lookup_color(style_context, "error_color", &color)) {
      color.red = 1.0;
      color.green = 0.0;
      color.blue = 0.0;
  }
  color.alpha = alpha;

  cr = cairo_create(surface);

  cairo_arc(cr, 3.0 * icon_size / 4.0, icon_size / 4.0, icon_size / 4.0, 0.0, 2 * M_PI);
  gdk_cairo_set_source_rgba(cr, &color);
  cairo_fill(cr);

  cairo_destroy(cr);
}

void
notify_log_icon_add_unread_emblem(cairo_surface_t *surface,
                                  GtkStyleContext *style_context,
                                  gint size,
                                  gint scale_factor,
                                  gdouble alpha)
{
    GIcon *emblem = g_themed_icon_new("org.xfce.notification.unread-emblem-symbolic");
    GtkIconInfo *emblem_info = gtk_icon_theme_lookup_by_gicon_for_scale(gtk_icon_theme_get_default(),
                                                                        emblem,
                                                                        size,
                                                                        scale_factor,
                                                                        GTK_ICON_LOOKUP_FORCE_SIZE);

    if (G_LIKELY(emblem_info != NULL)) {
        GError *error = NULL;
        GdkPixbuf *emblem_pix = gtk_icon_info_load_symbolic_for_context(emblem_info, style_context, NULL, &error);

        if (G_LIKELY(emblem_pix != NULL)) {
            cairo_t *cr = cairo_create(surface);

            cairo_scale(cr, 1.0 / scale_factor, 1.0 / scale_factor);
            gdk_cairo_set_source_pixbuf(cr, emblem_pix, 0, 0);
            cairo_paint_with_alpha(cr, alpha);

            cairo_destroy(cr);
            g_object_unref(emblem_pix);
        } else {
            g_warning("Failed to load unread notification emblem: %s", error->message);
            g_error_free(error);
            draw_unread_emblem_fallback(surface, style_context, size, alpha);
        }

        g_object_unref(emblem_info);
    } else {
        g_warning("Failed to look up unread notification emblem");
        draw_unread_emblem_fallback(surface, style_context, size, alpha);
    }

    g_object_unref(emblem);
}

gchar *
notify_log_format_tooltip(const gchar *app_name, const gchar *timestamp, const gchar *body_text) {
    if (timestamp != NULL && body_text != NULL) {
        return g_strdup_printf("<b>%s</b> - %s\n%s", app_name, timestamp, body_text);
    } else if (timestamp != NULL) {
        return g_strdup_printf("<b>%s</b> - %s", app_name, timestamp);
    } else if (body_text == NULL) {
        return g_strdup_printf("<b>%s</b>\n%s", app_name, body_text);
    } else {
        return g_strdup_printf("<b>%s</b>", app_name);
    }
}

GList *
notify_log_variant_to_entries(GVariant *variant) {
    GList *entries = NULL;
    GVariantIter *iter = g_variant_iter_new(variant);
    GVariant *entryv = NULL;

    while ((entryv = g_variant_iter_next_value(iter)) != NULL) {
        XfceNotifyLogEntry *entry = notify_log_variant_to_entry(entryv);
        if (G_LIKELY(entry != NULL)) {
            entries = g_list_prepend(entries, entry);
        }
        g_variant_unref(entryv);
    }
    entries = g_list_reverse(entries);

    g_variant_iter_free(iter);

    return entries;
}

XfceNotifyLogEntry *
notify_log_variant_to_entry(GVariant *variant) {
    XfceNotifyLogEntry *entry;
    gint64 timestamp_utc = 0;
    gchar *tz_identifier = NULL;
    GVariantIter *actions = NULL;
    GDateTime *dt_utc_no_us;
    GDateTime *dt_utc;
    GTimeZone *tz = NULL;

    g_return_val_if_fail(g_variant_is_of_type(variant, G_VARIANT_TYPE("(sxssssssa(ss)ib)")), NULL);

    entry = xfce_notify_log_entry_new_empty();
    g_variant_get(variant,
                  "(sxssssssa(ss)ib)",
                  &entry->id,
                  &timestamp_utc,
                  &tz_identifier,
                  &entry->app_id,
                  &entry->app_name,
                  &entry->icon_id,
                  &entry->summary,
                  &entry->body,
                  &actions,
                  &entry->expire_timeout,
                  &entry->is_read);

    dt_utc_no_us = g_date_time_new_from_unix_utc(timestamp_utc / 1000000);
    dt_utc = g_date_time_add(dt_utc_no_us, timestamp_utc % 1000000);
    if (G_LIKELY(tz_identifier != NULL && tz_identifier[0] != '\0')) {
        tz = g_time_zone_new_identifier(tz_identifier);
    }
    if (G_UNLIKELY(tz == NULL)) {
        tz = g_time_zone_new_local();
    }

    entry->timestamp = g_date_time_to_timezone(dt_utc, tz);
    g_date_time_unref(dt_utc);
    g_date_time_unref(dt_utc_no_us);
    g_time_zone_unref(tz);

    if (actions != NULL) {
        gchar *id = NULL;
        gchar *label = NULL;

        while (g_variant_iter_next(actions, "(ss)", &id, &label)) {
            XfceNotifyLogEntryAction *action = g_new0(XfceNotifyLogEntryAction, 1);
            action->id = id;
            action->label = label;
            entry->actions = g_list_prepend(entry->actions, action);
            id = NULL;
            label = NULL;
        }
        entry->actions = g_list_reverse(entry->actions);

        g_variant_iter_free(actions);
    }

    return entry;
}
