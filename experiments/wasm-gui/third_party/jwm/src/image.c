/**
 * @file image.c
 * @author Joe Wingbermuehle
 * @date 2005-2007
 *
 * @brief Functions to load images.
 *
 */

#include "jwm.h"

#ifndef MAKE_DEPEND

   /* We should include png.h here. See jwm.h for an explanation. */

#  ifdef USE_XPM
#     include <X11/xpm.h>
#  endif
#  ifdef USE_JPEG
#     include <jpeglib.h>
#  endif
#  ifdef USE_CAIRO
#     include <cairo.h>
#     include <cairo-svg.h>
#  endif
#  ifdef USE_RSVG
#     include <librsvg/rsvg.h>
#  endif
#endif /* MAKE_DEPEND */

#include "image.h"
#include "main.h"
#include "error.h"
#include "color.h"
#include "misc.h"

typedef ImageNode *(*ImageLoader)(const char *fileName,
                                  int rwidth, int rheight,
                                  char preserveAspect);

#ifdef USE_CAIRO
#ifdef USE_RSVG
static ImageNode *LoadSVGImage(const char *fileName, int rwidth, int rheight,
                               char preserveAspect);
#endif
#endif
#ifdef USE_JPEG
static ImageNode *LoadJPEGImage(const char *fileName, int rwidth, int rheight,
                                char preserveAspect);
#endif
#ifdef USE_PNG
static ImageNode *LoadPNGImage(const char *fileName, int rwidth, int rheight,
                               char preserveAspect);
#endif
#ifdef USE_XPM
static ImageNode *LoadXPMImage(const char *fileName, int rwidth, int rheight,
                               char preserveAspect);
#endif
#ifdef USE_XBM
static ImageNode *LoadXBMImage(const char *fileName, int rwidth, int rheight,
                               char preserveAspect);
#endif
#ifdef USE_ICONS
static ImageNode *CreateImageFromXImages(XImage *image, XImage *shape);
#endif

#ifdef USE_XPM
static int AllocateColor(Display *d, Colormap cmap, char *name,
                         XColor *c, void *closure);
static int FreeColors(Display *d, Colormap cmap, Pixel *pixels, int n,
                      void *closure);
#endif

/* File extension to image loader mapping. */
static const struct {
   const char *extension;
   ImageLoader loader;
} IMAGE_LOADERS[] = {
#ifdef USE_PNG
   {".png",       LoadPNGImage      },
#endif
#ifdef USE_JPEG
   {".jpg",       LoadJPEGImage     },
   {".jpeg",      LoadJPEGImage     },
#endif
#ifdef USE_CAIRO
#ifdef USE_RSVG
   {".svg",       LoadSVGImage      },
#endif
#endif
#ifdef USE_XPM
   {".xpm",       LoadXPMImage      },
#endif
#ifdef USE_XBM
   {".xbm",       LoadXBMImage      },
#endif
};
static const unsigned IMAGE_LOADER_COUNT = ARRAY_LENGTH(IMAGE_LOADERS);

/** Load an image from the specified file. */
ImageNode *LoadImage(const char *fileName, int rwidth, int rheight,
                     char preserveAspect)
{
   unsigned i;
   unsigned name_length;
   ImageNode *result = NULL;

   /* Make sure we have a reasonable file name. */
   if(!fileName) {
      return result;
   }
   name_length = strlen(fileName);
   if(JUNLIKELY(name_length == 0)) {
      return result;
   }

   /* Make sure the file exists. */
   if(access(fileName, R_OK) < 0) {
      return result;
   }

   /* First we attempt to use the extension to determine the type
    * to avoid trying all loaders. */
   for(i = 0; i < IMAGE_LOADER_COUNT; i++) {
      const char *ext = IMAGE_LOADERS[i].extension;
      const unsigned ext_length = strlen(ext);
      if(JLIKELY(name_length >= ext_length)) {
         const unsigned offset = name_length - ext_length;
         if(!StrCmpNoCase(&fileName[offset], ext)) {
            const ImageLoader loader = IMAGE_LOADERS[i].loader;
            result = (loader)(fileName, rwidth, rheight, preserveAspect);
            if(JLIKELY(result)) {
               return result;
            }
            break;
         }
      }
   }

   /* We were unable to load by extension, so try everything. */
   for(i = 0; i < IMAGE_LOADER_COUNT; i++) {
      const ImageLoader loader = IMAGE_LOADERS[i].loader;
      result = (loader)(fileName, rwidth, rheight, preserveAspect);
      if(result) {
         /* We were able to load the image, so it must have either the
          * wrong extension or an extension we don't recognize. */
         Warning(_("unrecognized extension for \"%s\", expected \"%s\""),
                 fileName, IMAGE_LOADERS[i].extension);
         return result;
      }
   }

   /* No image could be loaded. */
   return result;
}

/** Load an image from a pixmap. */
#ifdef USE_ICONS
ImageNode *LoadImageFromDrawable(Drawable pmap, Pixmap mask)
{
   ImageNode *result = NULL;
   XImage *mask_image = NULL;
   XImage *icon_image = NULL;
   Window rwindow;
   int x, y;
   unsigned int width, height;
   unsigned int border_width;
   unsigned int depth;

   JXGetGeometry(display, pmap, &rwindow, &x, &y, &width, &height,
                 &border_width, &depth);
   icon_image = JXGetImage(display, pmap, 0, 0, width, height,
                           AllPlanes, ZPixmap);
   if(mask != None) {
      mask_image = JXGetImage(display, mask, 0, 0, width, height, 1, ZPixmap);
   }
   if(icon_image) {
      result = CreateImageFromXImages(icon_image, mask_image);
      JXDestroyImage(icon_image);
   }
   if(mask_image) {
      JXDestroyImage(mask_image);
   }
   return result;
}
#endif

/** Load a PNG image from the given file name.
 * Since libpng uses longjmp, this function is not reentrant to simplify
 * the issues surrounding longjmp and local variables.
 */
#ifdef USE_PNG
ImageNode *LoadPNGImage(const char *fileName, int rwidth, int rheight,
                        char preserveAspect)
{

   static ImageNode *result;
   static FILE *fd;
   static unsigned char **rows;
   static png_structp pngData;
   static png_infop pngInfo;
   static png_infop pngEndInfo;

   unsigned char header[8];
   unsigned long rowBytes;
   int bitDepth, colorType;
   unsigned int x, y;
   png_uint_32 width;
   png_uint_32 height;

   Assert(fileName);

   result = NULL;
   fd = NULL;
   rows = NULL;
   pngData = NULL;
   pngInfo = NULL;
   pngEndInfo = NULL;

   fd = fopen(fileName, "rb");
   if(!fd) {
      return NULL;
   }

   x = fread(header, 1, sizeof(header), fd);
   if(x != sizeof(header) || png_sig_cmp(header, 0, sizeof(header))) {
      fclose(fd);
      return NULL;
   }

   pngData = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if(JUNLIKELY(!pngData)) {
      fclose(fd);
      Warning(_("could not create read struct for PNG image: %s"), fileName);
      return NULL;
   }

   if(JUNLIKELY(setjmp(png_jmpbuf(pngData)))) {
      png_destroy_read_struct(&pngData, &pngInfo, &pngEndInfo);
      if(fd) {
         fclose(fd);
      }
      if(rows) {
         ReleaseStack(rows);
      }
      DestroyImage(result);
      Warning(_("error reading PNG image: %s"), fileName);
      return NULL;
   }

   pngInfo = png_create_info_struct(pngData);
   if(JUNLIKELY(!pngInfo)) {
      png_destroy_read_struct(&pngData, NULL, NULL);
      fclose(fd);
      Warning(_("could not create info struct for PNG image: %s"), fileName);
      return NULL;
   }

   pngEndInfo = png_create_info_struct(pngData);
   if(JUNLIKELY(!pngEndInfo)) {
      png_destroy_read_struct(&pngData, &pngInfo, NULL);
      fclose(fd);
      Warning("could not create end info struct for PNG image: %s", fileName);
      return NULL;
   }

   png_init_io(pngData, fd);
   png_set_sig_bytes(pngData, sizeof(header));

   png_read_info(pngData, pngInfo);

   png_get_IHDR(pngData, pngInfo, &width, &height,
                &bitDepth, &colorType, NULL, NULL, NULL);
   result = CreateImage(width, height, 0);

   png_set_expand(pngData);

   if(bitDepth == 16) {
      png_set_strip_16(pngData);
   } else if(bitDepth < 8) {
      png_set_packing(pngData);
   }

   png_set_swap_alpha(pngData);
   png_set_filler(pngData, 0xFF, PNG_FILLER_BEFORE);

   if(colorType == PNG_COLOR_TYPE_GRAY
      || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(pngData);
   }

   png_read_update_info(pngData, pngInfo);

   rowBytes = png_get_rowbytes(pngData, pngInfo);
   rows = AllocateStack(result->height * sizeof(result->data));
   y = 0;
   for(x = 0; x < result->height; x++) {
      rows[x] = &result->data[y];
      y += rowBytes;
   }

   png_read_image(pngData, rows);

   png_read_end(pngData, pngInfo);
   png_destroy_read_struct(&pngData, &pngInfo, &pngEndInfo);

   fclose(fd);

   ReleaseStack(rows);
   rows = NULL;

   return result;

}
#endif /* USE_PNG */

/** Load a JPEG image from the specified file. */
#ifdef USE_JPEG

typedef struct {
   struct jpeg_error_mgr pub;
   jmp_buf jbuffer;
} JPEGErrorStruct;

static void JPEGErrorHandler(j_common_ptr cinfo) {
   JPEGErrorStruct *es = (JPEGErrorStruct*)cinfo->err;
   longjmp(es->jbuffer, 1);
}

ImageNode *LoadJPEGImage(const char *fileName,
                         int rwidth, int rheight,
                         char preserveAspect)
{
   static ImageNode *result;
   static struct jpeg_decompress_struct cinfo;
   static FILE *fd;
   static JSAMPARRAY buffer;
   static JPEGErrorStruct jerr;

   int rowStride;
   int x;
   int inIndex, outIndex;

   /* Open the file. */
   fd = fopen(fileName, "rb");
   if(fd == NULL) {
      return NULL;
   }

   /* Make sure everything is initialized so we can recover from errors. */
   result = NULL;
   buffer = NULL;

   /* Setup the error handler. */
   cinfo.err = jpeg_std_error(&jerr.pub);
   jerr.pub.error_exit = JPEGErrorHandler;

   /* Control will return here if an error was encountered. */
   if(setjmp(jerr.jbuffer)) {
      DestroyImage(result);
      jpeg_destroy_decompress(&cinfo);
      fclose(fd);
      return NULL;
   }

   /* Prepare to load the file. */
   jpeg_create_decompress(&cinfo);
   jpeg_stdio_src(&cinfo, fd);

   /* Check the header. */
   jpeg_read_header(&cinfo, TRUE);

   /* Pick an appropriate scale for the image.
    * We scale the image by the scale value for the dimension with
    * the smallest absolute change.
    */
   jpeg_calc_output_dimensions(&cinfo);
   if(rwidth != 0 && rheight != 0 &&
      (!( (rwidth == cinfo.output_width) &&
          (rheight == cinfo.output_height) ))
      ) {
      /* Scale using n/8 with n in [1..8]. */
      int ratio;
      if(abs((int)cinfo.output_width - rwidth)
            < abs((int)cinfo.output_height - rheight)) {
         ratio = (rwidth << 4) / cinfo.output_width;
      } else {
         ratio = (rheight << 4) / cinfo.output_height;
      }
      cinfo.scale_num = Max(1, Min(8, (ratio >> 2)));
      cinfo.scale_denom = 8;
   }

   /* Start decompression. */
   jpeg_start_decompress(&cinfo);
   rowStride = cinfo.output_width * cinfo.output_components;
   buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                       JPOOL_IMAGE, rowStride, 1);

   result = CreateImage(cinfo.output_width, cinfo.output_height, 0);

   /* Read lines. */
   outIndex = 0;
   while(cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      inIndex = 0;
      for(x = 0; x < result->width; x++) {
         switch(cinfo.output_components) {
         case 1:  /* Grayscale. */
            result->data[outIndex + 1] = GETJSAMPLE(buffer[0][inIndex]);
            result->data[outIndex + 2] = GETJSAMPLE(buffer[0][inIndex]);
            result->data[outIndex + 3] = GETJSAMPLE(buffer[0][inIndex]);
            inIndex += 1;
            break;
         default: /* RGB */
            result->data[outIndex + 1] = GETJSAMPLE(buffer[0][inIndex + 0]);
            result->data[outIndex + 2] = GETJSAMPLE(buffer[0][inIndex + 1]);
            result->data[outIndex + 3] = GETJSAMPLE(buffer[0][inIndex + 2]);
            inIndex += 3;
            break;
         }
         result->data[outIndex + 0] = 0xFF;
         outIndex += 4;
      }
   }

   /* Clean up. */
   jpeg_destroy_decompress(&cinfo);
   fclose(fd);

   return result;

}
#endif /* USE_JPEG */

#ifdef USE_CAIRO
#ifdef USE_RSVG
ImageNode *LoadSVGImage(const char *fileName, int rwidth, int rheight,
                        char preserveAspect)
{

#if !GLIB_CHECK_VERSION(2, 35, 0)
   static char initialized = 0;
#endif
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
   RsvgRectangle viewbox;
   RsvgLength pwidth, pheight;
   gboolean has_width, has_height, has_viewbox;
#endif
   ImageNode *result = NULL;
   RsvgHandle *rh;
   RsvgDimensionData dim;
   GError *e;
   cairo_surface_t *target;
   cairo_t *context;
   int stride;
   int i;
   float xscale, yscale;

   Assert(fileName);

#if !GLIB_CHECK_VERSION(2, 35, 0)
   if(!initialized) {
      initialized = 1;
      g_type_init();
   }
#endif

   /* Load the image from the file. */
   e = NULL;
   rh = rsvg_handle_new_from_file(fileName, &e);
   if(!rh) {
      g_error_free(e);
      return NULL;
   }

#if LIBRSVG_CHECK_VERSION(2, 46, 0)
   rsvg_handle_get_intrinsic_dimensions(rh, &has_width, &pwidth, &has_height, &pheight,
      &has_viewbox, &viewbox);
   if(has_width && has_height && pwidth.unit == RSVG_UNIT_PX && pheight.unit == RSVG_UNIT_PX) {
      dim.width = pwidth.length;
      dim.height = pheight.length;
   } else if(has_viewbox) {
      dim.width = viewbox.width;
      dim.height = viewbox.height;
   } else if(rwidth == 0 && rheight == 0) {
#if LIBRSVG_CHECK_VERSION(2, 52, 0)
      rsvg_handle_get_intrinsic_size_in_pixels(rh, &pwidth.length, &pheight.length);
      dim.width = pwidth.length;
      dim.height = pheight.length;
#else
      rsvg_handle_get_dimensions(rh, &dim);
#endif
   } else {
      dim.width = rwidth;
      dim.height = rheight;
   }
#else
   rsvg_handle_get_dimensions(rh, &dim);
#endif
   if(rwidth == 0 && rheight == 0) {
      rwidth = dim.width;
      rheight = dim.height;
      xscale = 1.0;
      yscale = 1.0;
   } else if(rwidth == 0) {
      yscale = (float)rheight / dim.height;
      xscale = yscale;
      rwidth = dim.width * xscale;
   } else if(rheight == 0) {
      xscale = (float)rwidth / dim.width;
      yscale = xscale;
      rheight = dim.height * yscale;
   } else if(preserveAspect) {
      if(abs(dim.width - rwidth) < abs(dim.height - rheight)) {
         xscale = (float)rwidth / dim.width;
         rheight = dim.height * xscale;
      } else {
         xscale = (float)rheight / dim.height;
         rwidth = dim.width * xscale;
      }
      yscale = xscale;
   } else {
      xscale = (float)rwidth / dim.width;
      yscale = (float)rheight / dim.height;
   }

   result = CreateImage(rwidth, rheight, 0);
   memset(result->data, 0, rwidth * rheight * 4);

   /* Create the target surface. */
   stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, rwidth);
   target = cairo_image_surface_create_for_data(result->data,
                                                CAIRO_FORMAT_ARGB32,
                                                rwidth, rheight, stride);
   context = cairo_create(target);
   cairo_paint_with_alpha(context, 0.0);
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
   viewbox.x = 0;
   viewbox.y = 0;
   viewbox.width = rwidth;
   viewbox.height = rheight;
   rsvg_handle_render_document(rh, context, &viewbox, NULL);
#else
   cairo_scale(context, xscale, yscale);
   rsvg_handle_render_cairo(rh, context);
#endif
   cairo_destroy(context);
   cairo_surface_destroy(target);
   g_object_unref(rh);

   for(i = 0; i < 4 * rwidth * rheight; i += 4) {
      const unsigned int temp = *(unsigned int*)&result->data[i];
      const unsigned int alpha  = (temp >> 24) & 0xFF;
      const unsigned int red    = (temp >> 16) & 0xFF;
      const unsigned int green  = (temp >>  8) & 0xFF;
      const unsigned int blue   = (temp >>  0) & 0xFF;
      result->data[i + 0] = alpha;
      if(alpha > 0) {
         result->data[i + 1] = (red * 255) / alpha;
         result->data[i + 2] = (green * 255) / alpha;
         result->data[i + 3] = (blue * 255) / alpha;
      }
   }

   return result;

}
#endif /* USE_RSVG */
#endif /* USE_CAIRO */

/** Load an XPM image from the specified file. */
#ifdef USE_XPM
ImageNode *LoadXPMImage(const char *fileName, int rwidth, int rheight,
                        char preserveAspect)
{

   ImageNode *result = NULL;

   XpmAttributes attr;
   XImage *image;
   XImage *shape;
   int rc;

   Assert(fileName);

   attr.valuemask = XpmAllocColor | XpmFreeColors | XpmColorClosure;
   attr.alloc_color = AllocateColor;
   attr.free_colors = FreeColors;
   attr.color_closure = NULL;
   rc = XpmReadFileToImage(display, (char*)fileName, &image, &shape, &attr);
   if(rc == XpmSuccess) {
      result = CreateImageFromXImages(image, shape);
      JXDestroyImage(image);
      if(shape) {
         JXDestroyImage(shape);
      }
   }

   return result;

}
#endif /* USE_XPM */

/** Load an XBM image from the specified file. */
#ifdef USE_XBM
ImageNode *LoadXBMImage(const char *fileName, int rwidth, int rheight,
                        char preserveAspect)
{
   ImageNode *result = NULL;
   unsigned char *data;
   unsigned width, height;
   int xhot, yhot;
   int rc;

   rc = XReadBitmapFileData(fileName, &width, &height, &data, &xhot, &yhot);
   if(rc == BitmapSuccess) {
      result = CreateImage(width, height, 1);
      memcpy(result->data, data, (width * height + 7) / 8);
      XFree(data);
   }

   return result;
}
#endif /* USE_XBM */

/** Create an image from XImages giving color and shape information. */
#ifdef USE_ICONS
#define HASH_SIZE 16
ImageNode *CreateImageFromXImages(XImage *image, XImage *shape)
{
   XColor colors[HASH_SIZE];
   ImageNode *result;
   unsigned char *dest;
   int x, y;

   memset(colors, 0xFF, sizeof(colors));
   result = CreateImage(image->width, image->height, 0);
   dest = result->data;
   for(y = 0; y < image->height; y++) {
      for(x = 0; x < image->width; x++) {
         const unsigned long pixel = XGetPixel(image, x, y);
         *dest++ = (!shape || XGetPixel(shape, x, y)) ? 255 : 0;
         if(image->depth == 1) {
            const unsigned char value = pixel ? 0 : 255;
            *dest++ = value;
            *dest++ = value;
            *dest++ = value;
         } else{
            const unsigned index = pixel % HASH_SIZE;
            if(colors[index].pixel != pixel) {
               colors[index].pixel = pixel;
               JXQueryColor(display, rootColormap, &colors[index]);
            }
            *dest++ = (unsigned char)(colors[index].red   >> 8);
            *dest++ = (unsigned char)(colors[index].green >> 8);
            *dest++ = (unsigned char)(colors[index].blue  >> 8);
         }
      }
   }

   return result;
}
#undef HASH_SIZE
#endif /* USE_ICONS */

ImageNode *CreateImage(unsigned width, unsigned height, char bitmap)
{
   unsigned image_size;
   if(bitmap) {
      image_size = (width * height + 7) / 8;
   } else {
      image_size = 4 * width * height;
   }
   ImageNode *image = Allocate(sizeof(ImageNode));
   image->data = Allocate(image_size);
   image->next = NULL;
   image->bitmap = bitmap;
   image->width = width;
   image->height = height;
#ifdef USE_XRENDER
   image->render = haveRender;
#endif
   return image;
}

/** Destroy an image node. */
void DestroyImage(ImageNode *image) {
   while(image) {
      ImageNode *next = image->next;
      if(image->data) {
         Release(image->data);
      }
      Release(image);
      image = next;
   }
}

/** Callback to allocate a color for libxpm. */
#ifdef USE_XPM
int AllocateColor(Display *d, Colormap cmap, char *name,
                  XColor *c, void *closure)
{
   if(name) {
      if(!JXParseColor(d, cmap, name, c)) {
         return -1;
      }
   }

   GetColor(c);
   return 1;
}
#endif /* USE_XPM */

/** Callback to free colors allocated by libxpm.
 * We don't need to do anything here since color.c takes care of this.
 */
#ifdef USE_XPM
int FreeColors(Display *d, Colormap cmap, Pixel *pixels, int n,
               void *closure)
{
   return 1;
}
#endif /* USE_XPM */
