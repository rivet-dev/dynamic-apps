/**
 * @file render.c
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Functions to render icons using the XRender extension.
 *
 */

#include "jwm.h"
#include "render.h"
#include "icon.h"
#include "image.h"
#include "main.h"
#include "color.h"
#include "misc.h"

/** Draw a scaled icon. */
void PutScaledRenderIcon(const IconNode *icon,
                         const ScaledIconNode *node,
                         Drawable d, int x, int y, int width, int height)
{

#ifdef USE_XRENDER

   Picture source;

   Assert(icon);
   Assert(haveRender);

   source = node->image;
   if(source != None) {

      XRenderPictureAttributes pa;
      XTransform xf;
      int xscale, yscale;
      int nwidth, nheight;
      Picture dest;
      Picture alpha = node->mask;
      XRenderPictFormat *fp = JXRenderFindVisualFormat(display, rootVisual);
      Assert(fp);

      pa.subwindow_mode = IncludeInferiors;
      dest = JXRenderCreatePicture(display, d, fp, CPSubwindowMode, &pa);

      width = width == 0 ? node->width : width;
      height = height == 0 ? node->height : height;
      if(icon->preserveAspect) {
         const int ratio = (icon->width << 16) / icon->height;
         nwidth = Min(width, (height * ratio) >> 16);
         nheight = Min(height, (nwidth << 16) / ratio);
         nwidth = (nheight * ratio) >> 16;
         nwidth = Max(1, nwidth);
         nheight = Max(1, nheight);
         x += (width - nwidth) / 2;
         y += (height - nheight) / 2;
      } else {
         nwidth = width;
         nheight = height;
      }
      xscale = (node->width << 16) / nwidth;
      yscale = (node->height << 16) / nheight;

      memset(&xf, 0, sizeof(xf));
      xf.matrix[0][0] = xscale;
      xf.matrix[1][1] = yscale;
      xf.matrix[2][2] = 65536;
      XRenderSetPictureTransform(display, source, &xf);
      XRenderSetPictureFilter(display, source, FilterBest, NULL, 0);
      XRenderSetPictureTransform(display, alpha, &xf);
      XRenderSetPictureFilter(display, alpha, FilterBest, NULL, 0);

      JXRenderComposite(display, PictOpOver, source, alpha, dest,
                        0, 0, 0, 0, x, y, width, height);

      JXRenderFreePicture(display, dest);

   }

#endif

}

/** Create a scaled icon. */
ScaledIconNode *CreateScaledRenderIcon(ImageNode *image, long fg)
{

   ScaledIconNode *result = NULL;

#ifdef USE_XRENDER

   XRenderPictFormat *fp;
   XColor color;
   GC maskGC;
   XImage *destImage;
   XImage *destMask;
   Pixmap pmap, mask;
   const unsigned width = image->width;
   const unsigned height = image->height;
   unsigned perLine;
   int x, y;
   int maskLine;

   Assert(haveRender);

   result = Allocate(sizeof(ScaledIconNode));
   result->fg = fg;
   result->width = width;
   result->height = height;

   mask = JXCreatePixmap(display, rootWindow, width, height, 8);
   maskGC = JXCreateGC(display, mask, 0, NULL);
   pmap = JXCreatePixmap(display, rootWindow, width, height, rootDepth);

   destImage = JXCreateImage(display, rootVisual, rootDepth,
                             ZPixmap, 0, NULL, width, height, 8, 0);
   destImage->data = Allocate(sizeof(unsigned long) * width * height);

   destMask = JXCreateImage(display, rootVisual, 8, ZPixmap,
                            0, NULL, width, height, 8, 0);
   destMask->data = Allocate(width * height);

   if(image->bitmap) {
      perLine = (image->width >> 3) + ((image->width & 7) ? 1 : 0);
   } else {
      perLine = image->width;
   }
   maskLine = 0;
   for(y = 0; y < height; y++) {
      const int yindex = y * perLine;
      for(x = 0; x < width; x++) {
         if(image->bitmap) {

            const int offset = yindex + (x >> 3);
            const int mask = 1 << (x & 7);
            unsigned long alpha = 0;
            if(image->data[offset] & mask) {
               alpha = 255;
               XPutPixel(destImage, x, y, fg);
            }
            destMask->data[maskLine + x] = alpha;

         } else {

            const int index = 4 * (yindex + x);
            const unsigned long alpha = image->data[index];
            color.red = image->data[index + 1];
            color.red |= color.red << 8;
            color.green = image->data[index + 2];
            color.green |= color.green << 8;
            color.blue = image->data[index + 3];
            color.blue |= color.blue << 8;

            color.red = (color.red * alpha) >> 8;
            color.green = (color.green * alpha) >> 8;
            color.blue = (color.blue * alpha) >> 8;

            GetColor(&color);
            XPutPixel(destImage, x, y, color.pixel);
            destMask->data[maskLine + x] = alpha;
         }
      }
      maskLine += destMask->bytes_per_line;
   }

   /* Render the image data to the image pixmap. */
   JXPutImage(display, pmap, rootGC, destImage, 0, 0, 0, 0, width, height);
   Release(destImage->data);
   destImage->data = NULL;
   JXDestroyImage(destImage);

   /* Render the alpha data to the mask pixmap. */
   JXPutImage(display, mask, maskGC, destMask, 0, 0, 0, 0, width, height);
   Release(destMask->data);
   destMask->data = NULL;
   JXDestroyImage(destMask);
   JXFreeGC(display, maskGC);

   /* Create the alpha picture. */
   fp = JXRenderFindStandardFormat(display, PictStandardA8);
   Assert(fp);
   result->mask = JXRenderCreatePicture(display, mask, fp, 0, NULL);
   JXFreePixmap(display, mask);

   /* Create the render picture. */
   fp = JXRenderFindVisualFormat(display, rootVisual);
   Assert(fp);
   result->image = JXRenderCreatePicture(display, pmap, fp, 0, NULL);
   JXFreePixmap(display, pmap);

#endif

   return result;

}
