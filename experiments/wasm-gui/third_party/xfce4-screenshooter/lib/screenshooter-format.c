/*  $Id$
 *
 *  Copyright © 2023 André Miranda <andreldm@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "screenshooter-format.h"

#include <libxfce4ui/libxfce4ui.h>



/* Internals */



static ImageFormat IMAGE_FORMATS[] = {
  { "png", N_("PNG File"), { "png", NULL }, { NULL }, { NULL }, TRUE },
  { "jpeg", N_("JPEG File"), { "jpg", "jpeg", NULL }, { NULL }, { NULL }, TRUE },
  { "bmp", N_("BMP File"), { "bmp", NULL }, { NULL }, { NULL }, TRUE },
  { "webp", N_("WebP File"), { "webp", NULL }, { NULL }, { NULL }, FALSE },
  { "jxl", N_("JPEG XL File"), { "jxl", NULL }, { "quality", NULL }, { "100", NULL }, FALSE },
  { "avif", N_("AVIF File"), { "avif", NULL }, { NULL }, { NULL }, FALSE },
  { NULL, NULL, { NULL }, { NULL }, { NULL }, FALSE }
};



static gboolean
screenshooter_is_format_supported (const gchar *format)
{
  gboolean result = FALSE;
  GSList *supported_formats;
  gchar *name;

  supported_formats = gdk_pixbuf_get_formats ();

  for (GSList *lp = supported_formats; lp != NULL; lp = lp->next)
    {
      name = gdk_pixbuf_format_get_name (lp->data);
      if (G_UNLIKELY (g_strcmp0 (name, format) == 0 && gdk_pixbuf_format_is_writable (lp->data)))
        {
          result = TRUE;
          g_free (name);
          break;
        }
      g_free (name);
    }

  g_slist_free_1 (supported_formats);

  return result;
}



/* Public */



ImageFormat
*screenshooter_get_image_formats (void)
{
  static gboolean supported_formats_checked = FALSE;

  if (!supported_formats_checked)
    {
      for (ImageFormat *format = IMAGE_FORMATS; format->type != NULL; format++)
        {
          if (format->supported) continue;
          format->supported = screenshooter_is_format_supported(format->type);
        }
      supported_formats_checked = TRUE;
    }

  return IMAGE_FORMATS;
}



gboolean
screenshooter_image_format_match_extension (ImageFormat *format, gchar *filepath)
{
  for (gchar **ext = format->extensions; *ext != NULL; ext++)
    {
      gchar *ext_with_dot = g_strdup_printf (".%s", *ext);
      gboolean match = g_str_has_suffix (filepath, ext_with_dot);
      g_free (ext_with_dot);

      if (match)
        return TRUE;
    }
  
  return FALSE;
}
