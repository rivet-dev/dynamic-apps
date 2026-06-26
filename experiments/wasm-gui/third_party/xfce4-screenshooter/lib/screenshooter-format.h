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

#ifndef __HAVE_FORMAT_H__
#define __HAVE_FORMAT_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>



typedef struct
{
  gchar *type; /* format type to be passed to gdk_pixbuf_save, e.g. "jpeg" */
  gchar *name; /* file format name, should be translatable, e.g. "JPEG File" */
  gchar *extensions[3]; /* supported extensions, first one is the preferred, e.g. "jpeg" */
  gchar *option_keys[3]; /* option keys to be passed to gdk_pixbuf_save, must be NULL terminated */
  gchar *option_values[3]; /* option values to be passed to gdk_pixbuf_save, must be NULL terminated */
  gboolean supported; /* whether this format is supported */
}
ImageFormat;



ImageFormat *screenshooter_get_image_formats             (void);
gboolean     screenshooter_image_format_match_extension  (ImageFormat *format,
                                                          gchar       *filename);

#endif
