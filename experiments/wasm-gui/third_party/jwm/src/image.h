/**
 * @file image.h
 * @author Joe Wingbermuehle
 * @date 2005-2014
 *
 * @brief Functions to load images.
 *
 */

#ifndef IMAGE_H
#define IMAGE_H

/** Structure to represent an image. */
typedef struct ImageNode {

   struct ImageNode *next;       /**< Next image node (if multiple sizes). */
   unsigned char *data;          /**< Image data. */
   int width;                    /**< Width of the image. */
   int height;                   /**< Height of the image. */
   char bitmap;                  /**< 1 if a bitmap, 0 otherwise. */
#ifdef USE_XRENDER
   char render;                  /**< 1 to use render, 0 otherwise. */
#endif

} ImageNode;

/** Load an image from a file.
 * @param fileName The file containing the image.
 * @param rwidth The preferred width.
 * @param rheight The preferred height.
 * @param preserveAspect Set to preserve image aspect when scaling.
 * @return A new image node (NULL if the image could not be loaded).
 */
ImageNode *LoadImage(const char *fileName, int rwidth, int rheight,
                     char preserveAspect);

/** Load an image from a Drawable.
 * @param pmap The drawable.
 * @param mask The mask (may be None).
 * @return a new image node (NULL if there were errors).
 */
ImageNode *LoadImageFromDrawable(Drawable pmap, Pixmap mask);

/** Create an image node.
 * @param width The image width.
 * @param height The image height.
 * @param bitmap 1 if a bitmap, 0 otherwise.
 * @return A newly allocated image node.
 */
ImageNode *CreateImage(unsigned int width, unsigned int height, char bitmap);

/** Destroy an image node.
 * @param image The image to destroy.
 */
void DestroyImage(ImageNode *image);

#endif /* IMAGE_H */
