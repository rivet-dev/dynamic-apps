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

/*
 * This file also serves as a private.h, so it should be included at the
 * top of every source file, but never in any other header.
 */

#ifndef __RISTRETTO_UTIL_H__
#define __RISTRETTO_UTIL_H__

/* disable debug checks in release mode (before including GLib/GTK headers) */
#ifdef NDEBUG
#ifndef G_DISABLE_CHECKS
#define G_DISABLE_CHECKS
#endif
#ifndef G_DISABLE_ASSERT
#define G_DISABLE_ASSERT
#endif
#endif

/* our config header */
#include <config.h>

/* standard headers */
#include <string.h>

/* external non standard headers */
#include <gtk/gtk.h>

/* internal headers */
#include "settings.h"
#include "file.h"

G_BEGIN_DECLS

#define RISTRETTO_APP_ID "org.xfce.ristretto"

/* for personal testing */
#define TIMER_START      GTimer *__FUNCTION__timer = g_timer_new ();
#define TIMER_SPLIT      g_printerr ("%s: %.2f ms\n", G_STRLOC, \
                                     g_timer_elapsed (__FUNCTION__timer, NULL) * 1000);
#define TIMER_STOP       TIMER_SPLIT g_timer_destroy (__FUNCTION__timer);

#define PRINT_LOCATION   g_printerr ("%s\n", G_STRLOC)
#define RTRACE(fmt, var) G_STMT_START{ g_printerr ("%s:%s: ", G_STRLOC, #var); \
                                       g_printerr (fmt, var); g_printerr ("\n"); }G_STMT_END

/* Macro to remove and clear a source id */
#define REMOVE_SOURCE(ID) G_STMT_START{ g_source_remove (ID); ID = 0; }G_STMT_END

/* convenient macros for setting object data */
#define rstto_object_set_data(object, key, data) \
    g_object_set_qdata (G_OBJECT (object), g_quark_from_static_string (key), data)
#define rstto_object_set_data_full(object, key, data, destroy) \
    g_object_set_qdata_full (G_OBJECT (object), g_quark_from_static_string (key), data, destroy)
#define rstto_object_get_data(object, key) \
    g_object_get_qdata (G_OBJECT (object), g_quark_try_string (key))



gpointer
rstto_util_source_autoremove (gpointer object);

cairo_pattern_t *
rstto_util_set_source_pixbuf (cairo_t *ctx,
                              const GdkPixbuf *pixbuf,
                              gdouble pixbuf_x,
                              gdouble pixbuf_y);

void
rstto_util_paint_background_color (GtkWidget *widget,
                                   RsttoSettings *settings,
                                   cairo_t *ctx);

void
rstto_util_dialog_error (const gchar *message,
                         GError *error);

void
rstto_util_set_scale_factor (gint scale_factor);

RsttoThumbnailFlavor
rstto_util_get_thumbnail_flavor (RsttoThumbnailSize size);

const gchar *
rstto_util_get_thumbnail_flavor_name (RsttoThumbnailFlavor flavor);

guint
rstto_util_get_thumbnail_n_pixels (RsttoThumbnailSize size);

G_END_DECLS

#endif /* __RSTTO_UTIL_H__ */
