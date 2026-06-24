/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_MMINTRIN_H
#include <mmintrin.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* required for GdkPixbufFormat */
#ifndef GDK_PIXBUF_ENABLE_BACKEND
#define GDK_PIXBUF_ENABLE_BACKEND
#endif

#include <exo/exo-gdk-pixbuf-extensions.h>
#include <exo/exo-private.h>
#include <exo/exo-alias.h>

/* use g_open() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_open(path, mode, flags) (open ((path), (mode), (flags)))
#endif

/* _O_BINARY is required on some platforms */
#ifndef _O_BINARY
#define _O_BINARY 0
#endif

/**
 * SECTION: exo-gdk-pixbuf-extensions
 * @title: Extensions to gdk-pixbuf
 * @short_description: Miscelleanous extensions to the gdk-pixbuf library
 * @include: exo/exo.h
 *
 * This facility includes several functions to extend the basic functionality
 * provided by the gdk-pixbuf library.
 **/



/**
 * exo_gdk_pixbuf_colorize:
 * @source : the source #GdkPixbuf.
 * @color  : the new color.
 *
 * Creates a new #GdkPixbuf based on @source, which is
 * colorized to @color.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Returns: the colorized #GdkPixbuf.
 *
 * Since: 0.3.1.3
 **/
GdkPixbuf*
exo_gdk_pixbuf_colorize (const GdkPixbuf *source,
                         const GdkColor  *color)
{
  GdkPixbuf *dst;
  gboolean   has_alpha;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i;

  /* determine source parameters */
  width = gdk_pixbuf_get_width (source);
  height = gdk_pixbuf_get_height (source);
  has_alpha = gdk_pixbuf_get_has_alpha (source);

  /* allocate the destination pixbuf */
  dst = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (source), has_alpha, gdk_pixbuf_get_bits_per_sample (source), width, height);

  /* determine row strides on src/dst */
  dst_row_stride = gdk_pixbuf_get_rowstride (dst);
  src_row_stride = gdk_pixbuf_get_rowstride (source);

#if defined(__GNUC__) && defined(__MMX__)
  /* check if there's a good reason to use MMX */
  if (G_LIKELY (has_alpha && dst_row_stride == width * 4 && src_row_stride == width * 4 && (width * height) % 2 == 0))
    {
      __m64 *pixdst = (__m64 *) gdk_pixbuf_get_pixels (dst);
      __m64 *pixsrc = (__m64 *) gdk_pixbuf_get_pixels (source);
      __m64  alpha_mask = _mm_set_pi8 (0xff, 0, 0, 0, 0xff, 0, 0, 0);
      __m64  color_factor = _mm_set_pi16 (0, color->blue, color->green, color->red);
      __m64  zero = _mm_setzero_si64 ();
      __m64  src, alpha, hi, lo;

      /* divide color components by 256 */
      color_factor = _mm_srli_pi16 (color_factor, 8);

      for (i = (width * height) >> 1; i > 0; --i)
        {
          /* read the source pixel */
          src = *pixsrc;

          /* remember the two alpha values */
          alpha = _mm_and_si64 (alpha_mask, src);

          /* extract the hi pixel */
          hi = _mm_unpackhi_pi8 (src, zero);
          hi = _mm_mullo_pi16 (hi, color_factor);

          /* extract the lo pixel */
          lo = _mm_unpacklo_pi8 (src, zero);
          lo = _mm_mullo_pi16 (lo, color_factor);

          /* prefetch the next two pixels */
          __builtin_prefetch (++pixsrc, 0, 1);

          /* divide by 256 */
          hi = _mm_srli_pi16 (hi, 8);
          lo = _mm_srli_pi16 (lo, 8);

          /* combine the 2 pixels again */
          src = _mm_packs_pu16 (lo, hi);

          /* write back the calculated color together with the alpha */
          *pixdst = _mm_or_si64 (alpha, src);

          /* advance the dest pointer */
          ++pixdst;
        }

      _mm_empty ();
    }
  else
#endif
    {
      guchar *dst_pixels = gdk_pixbuf_get_pixels (dst);
      guchar *src_pixels = gdk_pixbuf_get_pixels (source);
      guchar *pixdst;
      guchar *pixsrc;
      gint    red_value = color->red / 255.0;
      gint    green_value = color->green / 255.0;
      gint    blue_value = color->blue / 255.0;
      gint    j;

      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; j > 0; --j)
            {
              *pixdst++ = (*pixsrc++ * red_value) >> 8;
              *pixdst++ = (*pixsrc++ * green_value) >> 8;
              *pixdst++ = (*pixsrc++ * blue_value) >> 8;

              if (has_alpha)
                *pixdst++ = *pixsrc++;
            }
        }
    }

  return dst;
}



static inline void
draw_frame_row (const GdkPixbuf *frame_image,
                gint             target_width,
                gint             source_width,
                gint             source_v_position,
                gint             dest_v_position,
                GdkPixbuf       *result_pixbuf,
                gint             left_offset,
                gint             height)
{
  gint remaining_width;
  gint slab_width;
  gint h_offset;

  for (h_offset = 0, remaining_width = target_width; remaining_width > 0; h_offset += slab_width, remaining_width -= slab_width)
    {
      slab_width = (remaining_width > source_width) ? source_width : remaining_width;
      gdk_pixbuf_copy_area (frame_image, left_offset, source_v_position, slab_width, height, result_pixbuf, left_offset + h_offset, dest_v_position);
    }
}



static inline void
draw_frame_column (const GdkPixbuf *frame_image,
                   gint             target_height,
                   gint             source_height,
                   gint             source_h_position,
                   gint             dest_h_position,
                   GdkPixbuf       *result_pixbuf,
                   gint             top_offset,
                   gint             width)
{
  gint remaining_height;
  gint slab_height;
  gint v_offset;

  for (v_offset = 0, remaining_height = target_height; remaining_height > 0; v_offset += slab_height, remaining_height -= slab_height)
    {
      slab_height = (remaining_height > source_height) ? source_height : remaining_height;
      gdk_pixbuf_copy_area (frame_image, source_h_position, top_offset, width, slab_height, result_pixbuf, dest_h_position, top_offset + v_offset);
    }
}



/**
 * exo_gdk_pixbuf_frame:
 * @source        : the source #GdkPixbuf.
 * @frame         : the frame #GdkPixbuf.
 * @left_offset   : the left frame offset.
 * @top_offset    : the top frame offset.
 * @right_offset  : the right frame offset.
 * @bottom_offset : the bottom frame offset.
 *
 * Embeds @source in @frame and returns the result as new #GdkPixbuf.
 *
 * The caller is responsible to free the returned #GdkPixbuf using g_object_unref().
 *
 * Returns: the framed version of @source.
 *
 * Since: 0.3.1.9
 **/
GdkPixbuf*
exo_gdk_pixbuf_frame (const GdkPixbuf *source,
                      const GdkPixbuf *frame,
                      gint             left_offset,
                      gint             top_offset,
                      gint             right_offset,
                      gint             bottom_offset)
{
  GdkPixbuf *dst;
  gint       dst_width;
  gint       dst_height;
  gint       frame_width;
  gint       frame_height;
  gint       src_width;
  gint       src_height;

  g_return_val_if_fail (GDK_IS_PIXBUF (frame), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);

  src_width = gdk_pixbuf_get_width (source);
  src_height = gdk_pixbuf_get_height (source);

  frame_width = gdk_pixbuf_get_width (frame);
  frame_height = gdk_pixbuf_get_height (frame);

  dst_width = src_width + left_offset + right_offset;
  dst_height = src_height + top_offset + bottom_offset;

  /* allocate the resulting pixbuf */
  dst = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, dst_width, dst_height);

  /* fill the destination if the source has an alpha channel */
  if (G_UNLIKELY (gdk_pixbuf_get_has_alpha (source)))
    gdk_pixbuf_fill (dst, 0xffffffff);

  /* draw the left top cornder and top row */
  gdk_pixbuf_copy_area (frame, 0, 0, left_offset, top_offset, dst, 0, 0);
  draw_frame_row (frame, src_width, frame_width - left_offset - right_offset, 0, 0, dst, left_offset, top_offset);

  /* draw the right top corner and left column */
  gdk_pixbuf_copy_area (frame, frame_width - right_offset, 0, right_offset, top_offset, dst, dst_width - right_offset, 0);
  draw_frame_column (frame, src_height, frame_height - top_offset - bottom_offset, 0, 0, dst, top_offset, left_offset);

  /* draw the bottom right corner and bottom row */
  gdk_pixbuf_copy_area (frame, frame_width - right_offset, frame_height - bottom_offset, right_offset,
                        bottom_offset, dst, dst_width - right_offset, dst_height - bottom_offset);
  draw_frame_row (frame, src_width, frame_width - left_offset - right_offset, frame_height - bottom_offset,
                  dst_height - bottom_offset, dst, left_offset, bottom_offset);

  /* draw the bottom left corner and the right column */
  gdk_pixbuf_copy_area (frame, 0, frame_height - bottom_offset, left_offset, bottom_offset, dst, 0, dst_height - bottom_offset);
  draw_frame_column (frame, src_height, frame_height - top_offset - bottom_offset, frame_width - right_offset,
                     dst_width - right_offset, dst, top_offset, right_offset);

  /* copy the source pixbuf into the framed area */
  gdk_pixbuf_copy_area (source, 0, 0, src_width, src_height, dst, left_offset, top_offset);

  return dst;
}



/**
 * exo_gdk_pixbuf_lucent:
 * @source  : the source #GdkPixbuf.
 * @percent : the percentage of translucency.
 *
 * Returns a version of @source, whose pixels translucency is
 * @percent of the original @source pixels.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Returns: a translucent version of @source.
 *
 * Since: 0.3.1.3
 **/
GdkPixbuf*
exo_gdk_pixbuf_lucent (const GdkPixbuf *source,
                       guint            percent)
{
  GdkPixbuf *dst;
  guchar    *dst_pixels;
  guchar    *src_pixels;
  guchar    *pixdst;
  guchar    *pixsrc;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i, j;

  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
  g_return_val_if_fail ((gint) percent >= 0 && percent <= 100, NULL);

  /* determine source parameters */
  width = gdk_pixbuf_get_width (source);
  height = gdk_pixbuf_get_height (source);

  /* allocate the destination pixbuf */
  dst = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (source), TRUE, gdk_pixbuf_get_bits_per_sample (source), width, height);

  /* determine row strides on src/dst */
  dst_row_stride = gdk_pixbuf_get_rowstride (dst);
  src_row_stride = gdk_pixbuf_get_rowstride (source);

  /* determine pixels on src/dst */
  dst_pixels = gdk_pixbuf_get_pixels (dst);
  src_pixels = gdk_pixbuf_get_pixels (source);

  /* check if the source already contains an alpha channel */
  if (G_LIKELY (gdk_pixbuf_get_has_alpha (source)))
    {
      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; --j >= 0; )
            {
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = ((guint) *pixsrc++ * percent) / 100u;
            }
        }
    }
  else
    {
      /* pre-calculate the alpha value */
      percent = (255u * percent) / 100u;

      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; --j >= 0; )
            {
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = percent;
            }
        }
    }

  return dst;
}



static inline guchar
lighten_channel (guchar cur_value)
{
  gint new_value = cur_value;

  new_value += 24 + (new_value >> 3);
  if (G_UNLIKELY (new_value > 255))
    new_value = 255;

  return (guchar) new_value;
}



/**
 * exo_gdk_pixbuf_spotlight:
 * @source : the source #GdkPixbuf.
 *
 * Creates a lightened version of @source, suitable for
 * prelit state display of icons.
 *
 * The caller is responsible to free the returned
 * pixbuf using #g_object_unref().
 *
 * Returns: the lightened version of @source.
 *
 * Since: 0.3.1.3
 **/
GdkPixbuf*
exo_gdk_pixbuf_spotlight (const GdkPixbuf *source)
{
  GdkPixbuf *dst;
  gboolean   has_alpha;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i;

  /* determine source parameters */
  width = gdk_pixbuf_get_width (source);
  height = gdk_pixbuf_get_height (source);
  has_alpha = gdk_pixbuf_get_has_alpha (source);

  /* allocate the destination pixbuf */
  dst = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (source), has_alpha, gdk_pixbuf_get_bits_per_sample (source), width, height);

  /* determine src/dst row strides */
  dst_row_stride = gdk_pixbuf_get_rowstride (dst);
  src_row_stride = gdk_pixbuf_get_rowstride (source);

#if defined(__GNUC__) && defined(__MMX__)
  /* check if there's a good reason to use MMX */
  if (G_LIKELY (has_alpha && dst_row_stride == width * 4 && src_row_stride == width * 4 && (width * height) % 2 == 0))
    {
      __m64 *pixdst = (__m64 *) gdk_pixbuf_get_pixels (dst);
      __m64 *pixsrc = (__m64 *) gdk_pixbuf_get_pixels (source);
      __m64  alpha_mask = _mm_set_pi8 (0xff, 0, 0, 0, 0xff, 0, 0, 0);
      __m64  twentyfour = _mm_set_pi8 (0, 24, 24, 24, 0, 24, 24, 24);
      __m64  zero = _mm_setzero_si64 ();

      for (i = (width * height) >> 1; i > 0; --i)
        {
          /* read the source pixel */
          __m64 src = *pixsrc;

          /* remember the two alpha values */
          __m64 alpha = _mm_and_si64 (alpha_mask, src);

          /* extract the hi pixel */
          __m64 hi = _mm_unpackhi_pi8 (src, zero);

          /* extract the lo pixel */
          __m64 lo = _mm_unpacklo_pi8 (src, zero);

          /* add (x >> 3) to x */
          hi = _mm_adds_pu16 (hi, _mm_srli_pi16 (hi, 3));
          lo = _mm_adds_pu16 (lo, _mm_srli_pi16 (lo, 3));

          /* prefetch next value */
          __builtin_prefetch (++pixsrc, 0, 1);

          /* combine the two pixels again */
          src = _mm_packs_pu16 (lo, hi);

          /* add 24 (with saturation) */
          src = _mm_adds_pu8 (src, twentyfour);

          /* drop the alpha channel from the temp color */
          src = _mm_andnot_si64 (alpha_mask, src);

          /* write back the calculated color */
          *pixdst = _mm_or_si64 (alpha, src);

          /* advance the dest pointer */
          ++pixdst;
        }

      _mm_empty ();
    }
  else
#endif
    {
      guchar *dst_pixels = gdk_pixbuf_get_pixels (dst);
      guchar *src_pixels = gdk_pixbuf_get_pixels (source);
      guchar *pixdst;
      guchar *pixsrc;
      gint    j;

      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; j > 0; --j)
            {
              *pixdst++ = lighten_channel (*pixsrc++);
              *pixdst++ = lighten_channel (*pixsrc++);
              *pixdst++ = lighten_channel (*pixsrc++);

              if (G_LIKELY (has_alpha))
                *pixdst++ = *pixsrc++;
            }
        }
    }

  return dst;
}



/**
 * exo_gdk_pixbuf_scale_down:
 * @source                : the source #GdkPixbuf.
 * @preserve_aspect_ratio : %TRUE to preserve aspect ratio.
 * @dest_width            : the max width for the result.
 * @dest_height           : the max height for the result.
 *
 * Scales down the @source to fit into the given @width and
 * @height. If @aspect_ratio is %TRUE then the aspect ratio
 * of @source will be preserved.
 *
 * If @width is larger than the width of @source and @height
 * is larger than the height of @source, a reference to
 * @source will be returned, as it's unneccesary then to
 * scale down.
 *
 * The caller is responsible to free the returned #GdkPixbuf
 * using g_object_unref() when no longer needed.
 *
 * Returns: the resulting #GdkPixbuf.
 *
 * Since: 0.3.1.1
 **/
GdkPixbuf*
exo_gdk_pixbuf_scale_down (GdkPixbuf *source,
                           gboolean   preserve_aspect_ratio,
                           gint       dest_width,
                           gint       dest_height)
{
  gdouble wratio;
  gdouble hratio;
  gint    source_width;
  gint    source_height;

  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
  g_return_val_if_fail (dest_width > 0, NULL);
  g_return_val_if_fail (dest_height > 0, NULL);

  source_width = gdk_pixbuf_get_width (source);
  source_height = gdk_pixbuf_get_height (source);

  /* check if we need to scale */
  if (G_UNLIKELY (source_width <= dest_width && source_height <= dest_height))
    return GDK_PIXBUF (g_object_ref (G_OBJECT (source)));

  /* check if aspect ratio should be preserved */
  if (G_LIKELY (preserve_aspect_ratio))
    {
      /* calculate the new dimensions */
      wratio = (gdouble) source_width  / (gdouble) dest_width;
      hratio = (gdouble) source_height / (gdouble) dest_height;

      if (hratio > wratio)
        dest_width  = rint (source_width / hratio);
      else
        dest_height = rint (source_height / wratio);
    }

  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);
}



/**
 * exo_gdk_pixbuf_scale_ratio:
 * @source    : The source #GdkPixbuf.
 * @dest_size : The target size in pixel.
 *
 * Scales @source to @dest_size while preserving the aspect ratio of
 * @source.
 *
 * Returns: A newly created #GdkPixbuf.
 **/
GdkPixbuf*
exo_gdk_pixbuf_scale_ratio (GdkPixbuf *source,
                            gint       dest_size)
{
  gdouble wratio;
  gdouble hratio;
  gint    source_width;
  gint    source_height;
  gint    dest_width;
  gint    dest_height;

  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
  g_return_val_if_fail (dest_size > 0, NULL);

  source_width  = gdk_pixbuf_get_width  (source);
  source_height = gdk_pixbuf_get_height (source);

  wratio = (gdouble) source_width  / (gdouble) dest_size;
  hratio = (gdouble) source_height / (gdouble) dest_size;

  if (hratio > wratio)
    {
      dest_width  = rint (source_width / hratio);
      dest_height = dest_size;
    }
  else
    {
      dest_width  = dest_size;
      dest_height = rint (source_height / wratio);
    }

  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);
}



typedef struct
{
  gint     max_width;
  gint     max_height;
  gboolean preserve_aspect_ratio;
} SizePreparedInfo;



static void
size_prepared (GdkPixbufLoader  *loader,
               gint              width,
               gint              height,
               SizePreparedInfo *info)
{
  gboolean scalable;
  gdouble  wratio;
  gdouble  hratio;

  /* check if the loader format is scalable */
  scalable = ((gdk_pixbuf_loader_get_format (loader)->flags & GDK_PIXBUF_FORMAT_SCALABLE) != 0);

  /* check if we need to scale down (scalable formats are special) */
  if (scalable || width > info->max_width || height > info->max_height)
    {
      if (G_LIKELY (info->preserve_aspect_ratio))
        {
          /* calculate the new dimensions */
          wratio = (gdouble) width / (gdouble) info->max_width;
          hratio = (gdouble) height / (gdouble) info->max_height;

          if (hratio > wratio)
            {
              width = rint (width / hratio);
              height = info->max_height;
            }
          else
            {
              width = info->max_width;
              height = rint (height / wratio);
            }
        }
      else
        {
          /* just scale down to the dimension (scalable is special) */
          if (scalable || width > info->max_width)
            width = info->max_width;
          if (scalable || height > info->max_height)
            height = info->max_height;
        }
    }

  /* apply the new dimensions */
  gdk_pixbuf_loader_set_size (loader, MAX (width, 1), MAX (height, 1));
}



/**
 * exo_gdk_pixbuf_new_from_file_at_max_size:
 * @filename              : name of the file to load, in the GLib file
 *                          name encoding.
 * @max_width             : the maximum width of the loaded image.
 * @max_height            : the maximum height of the loaded image.
 * @preserve_aspect_ratio : %TRUE to preserve the image's aspect ratio
 *                          while scaling to fit into @max_width and @max_height.
 * @error                 : return location for errors or %NULL.
 *
 * Creates a new #GdkPixbuf by loading an image from the file at
 * @filename. The file format is detected automatically. If %NULL is
 * returned, then @error will be set. Possible errors are in the
 * #GDK_PIXBUF_ERROR and #G_FILE_ERROR domains. If the image dimensions
 * exceed @max_width or @max_height, the image will be scaled down to
 * fit into the dimensions, optionally preservingthe image's aspect
 * ratio. The image may still be larger, depending on the loader.
 *
 * The advantage of using this function over
 * gdk_pixbuf_new_from_file_at_scale() is that images will never be
 * scaled up, whichwould otherwise result in ugly images.
 *
 * Returns: a newly created #GdkPixbuf with a reference count or 1, or
 *          %NULL if any of several error conditions occurred: the file
 *          could not be opened, there was no loader for the file's format,
 *          there was not enough memory to allocate the buffer for the
 *          image, or the image file contained invalid data.
 *
 * Since: 0.3.1.9
 **/
GdkPixbuf*
exo_gdk_pixbuf_new_from_file_at_max_size (const gchar *filename,
                                          gint         max_width,
                                          gint         max_height,
                                          gboolean     preserve_aspect_ratio,
                                          GError     **error)
{
  SizePreparedInfo info;
  GdkPixbufLoader *loader;
  struct stat      statb;
  GdkPixbuf       *pixbuf;
  guchar          *buffer;
  gchar           *display_name;
  gint             sverrno;
  gint             fd;
  gint             n;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (max_height > 0, NULL);
  g_return_val_if_fail (max_width > 0, NULL);

  /* try to open the file for reading */
  fd = g_open (filename, _O_BINARY | O_RDONLY, 0000);
  if (G_UNLIKELY (fd < 0))
    {
err0: /* remember the errno value */
      sverrno = errno;

err1: /* initialize the library's i18n support */
      _exo_i18n_init ();

      /* generate a useful error message */
      display_name = g_filename_display_name (filename);
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (sverrno), _("Failed to open file \"%s\": %s"), display_name, g_strerror (sverrno));
      g_free (display_name);

      if (fd >= 0)
        {
          close(fd);
        }

      return NULL;
    }

  /* try to stat the file */
  if (fstat (fd, &statb) < 0)
    goto err0;

  /* verify that we have a regular file here */
  if (!S_ISREG (statb.st_mode))
    {
      sverrno = EINVAL;
      goto err1;
    }

  /* setup the size-prepared info */
  info.max_width = max_width;
  info.max_height = max_height;
  info.preserve_aspect_ratio = preserve_aspect_ratio;

  /* allocate a new pixbuf loader */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (G_OBJECT (loader), "size-prepared", G_CALLBACK (size_prepared), &info);

#ifdef HAVE_MMAP
  /* try to mmap() the file it's not too large */
  buffer = mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (G_LIKELY (buffer != MAP_FAILED))
    {
      /* feed the data into the loader */
      if (!gdk_pixbuf_loader_write (loader, buffer, statb.st_size, error))
        {
          /* something went wrong */
          munmap (buffer, statb.st_size);
          goto err2;
        }

      /* unmap the file */
      munmap (buffer, statb.st_size);
    }
  else
#endif
    {
      /* allocate the read buffer */
      buffer = g_newa (guchar, 8192);

      /* read the file content */
      for (;;)
        {
          /* read the next chunk */
          n = read (fd, buffer, 8192);
          if (G_UNLIKELY (n < 0))
            {
              /* remember the errno value */
              sverrno = errno;

              /* initialize the library's i18n support */
              _exo_i18n_init ();

              /* generate a useful error message */
              display_name = g_filename_display_name (filename);
              g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (sverrno), _("Failed to read file \"%s\": %s"), display_name, g_strerror (sverrno));
              g_free (display_name);

              /* close the loader and the file */
err2:         gdk_pixbuf_loader_close (loader, NULL);
              close (fd);
              goto err3;
            }
          else if (n == 0)
            {
              /* file read completely */
              break;
            }

          /* feed the data into the loader */
          if (!gdk_pixbuf_loader_write (loader, buffer, n, error))
            goto err2;
        }
    }

  /* close the file */
  close (fd);

  /* finalize the loader */
  if (!gdk_pixbuf_loader_close (loader, error))
    {
err3: /* we failed for some reason */
      g_object_unref (G_OBJECT (loader));
      return NULL;
    }

  /* check if we have a pixbuf now */
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (G_UNLIKELY (pixbuf == NULL))
    {
      /* initialize the library's i18n support */
      _exo_i18n_init ();

      /* generate a useful error message */
      display_name = g_filename_display_name (filename);
      g_set_error (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                   _("Failed to load image \"%s\": Unknown reason, probably a corrupt image file"),
                   display_name);
      g_free (display_name);
    }
  else
    {
      /* take a reference for the caller */
      g_object_ref (G_OBJECT (pixbuf));
    }

  /* release the loader */
  g_object_unref (G_OBJECT (loader));

  return pixbuf;
}



#define __EXO_GDK_PIXBUF_EXTENSIONS_C__
#include <exo/exo-aliasdef.c>
