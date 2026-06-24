/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   image.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "geom.h"
#include "image.h"
#include "color.h"
#include "imagecache.h"
#ifdef USE_IMLIB2
#include <Imlib2.h>
#endif
#ifdef USE_LIBRSVG
#include <cairo.h>
#include <librsvg/rsvg.h>
#endif

#include <glib.h>

#define FRACTION        12
#define FLOOR(i)        ((i) & (~0UL << FRACTION))
#define AVERAGE(a, b)   (((((a) ^ (b)) & 0xfefefefeL) >> 1) + ((a) & (b)))

/************************************************************************
 RrImagePic functions.

 RrImagePics are pictures that are grouped together into RrImageSets.  Each
 RrImagePic in the set has the same logical image inside it, but they are
 of different sizes.  An RrImagePic can be an original (which comes from some
 outside source, such as an image file), or resized from some other RrImagePic
 to meet the needs of the user.
**************************************************************************/


/*! Set up an RrImagePic.
  This does _not_ make a copy of the data. So the value of data must be
  owned by the caller of this function, and not freed afterward.
  This function does not allocate an RrImagePic, and can be used for setting
  up a temporary RrImagePic on the stack.  Such an object would then also
  not be freed with RrImagePicFree.
*/
static void RrImagePicInit(RrImagePic *pic, gint w, gint h, RrPixel32 *data)
{
    gint i;

    pic->width = w;
    pic->height = h;
    pic->data = data;
    pic->sum = 0;
    for (i = w*h; i > 0; --i)
        pic->sum += *(data++);
}

/*! Create a new RrImagePic from some picture data.
  This makes a duplicate of the data.
*/
static RrImagePic* RrImagePicNew(gint w, gint h, RrPixel32 *data)
{
    RrImagePic *pic;

    pic = g_slice_new(RrImagePic);
    RrImagePicInit(pic, w, h, g_memdup(data, w*h*sizeof(RrPixel32)));
    return pic;
}


/*! Destroy an RrImagePic.
  This frees the RrImagePic object and everything inside it.
*/
static void RrImagePicFree(RrImagePic *pic)
{
    if (pic) {
        g_free(pic->data);
        g_slice_free(RrImagePic, pic);
    }
}

/************************************************************************
 RrImageSet functions.

 RrImageSets hold a group of pictures, each of which represent the same logical
 image, but are physically different sizes.
 At any time, it may be discovered that two different RrImageSets are actually
 holding the same logical image.  At that time, they would be merged.
 An RrImageSet holds both original images which come from an outside source,
 and resized images, which are generated when requests for a specific size are
 made, and kept around in case they are request again.  There is a maximum
 number of resized images that an RrImageSet will keep around, however.

 Each RrImage points to a single RrImageSet, which keeps track of which
 RrImages point to it.  If two RrImageSets are merged, then the RrImages which
 pointed to the two RrImageSets will all point at the resulting merged set.
**************************************************************************/


/*! Free an RrImageSet and the stuff inside it.
  This should only occur when there are no more RrImages pointing to the set.
*/
static void RrImageSetFree(RrImageSet *self)
{
    GSList *it;
    gint i;

    if (self) {
        g_assert(self->images == NULL);

        /* remove all names associated with this RrImageSet */
        for (it = self->names; it; it = g_slist_next(it)) {
            g_hash_table_remove(self->cache->name_table, it->data);
            g_free(it->data);
        }
        g_slist_free(self->names);

        /* destroy the RrImagePic objects stored in the RrImageSet.  they will
           be keys in the cache to RrImageSet objects, so remove them from
           the cache's pic_table as well. */
        for (i = 0; i < self->n_original; ++i) {
            g_hash_table_remove(self->cache->pic_table, self->original[i]);
            RrImagePicFree(self->original[i]);
        }
        g_free(self->original);
        for (i = 0; i < self->n_resized; ++i) {
            g_hash_table_remove(self->cache->pic_table, self->resized[i]);
            RrImagePicFree(self->resized[i]);
        }
        g_free(self->resized);

        g_slice_free(RrImageSet, self);
    }
}

/*! Remove a picture from an RrImageSet as a given position.
   @param set The RrImageSet to remove the picture from.
   @param i The index of the picture in the RrImageSet in the list of
     originals (if @original is TRUE), or in the list of resized pictures (if
     @original is FALSE).
   @param original TRUE if the picture is an original, FALSE if it is a resized
     version of another picture in the RrImageSet.
 */
static void RrImageSetRemovePictureAt(RrImageSet *self, gint i,
                                      gboolean original)
{
    RrImagePic ***list;
    gint *len;

    if (original) {
        list = &self->original;
        len = &self->n_original;
    }
    else {
        list = &self->resized;
        len = &self->n_resized;
    }

    g_assert(i >= 0 && i < *len);

    /* remove the picture data as a key in the cache */
    g_hash_table_remove(self->cache->pic_table, (*list)[i]);

    /* free the picture being removed */
    RrImagePicFree((*list)[i]);

    /* copy the elements after the removed one in the array forward one space
       and shrink the array down one size */
    for (i = i+1; i < *len; ++i)
        (*list)[i-1] = (*list)[i];
    --(*len);
    *list = g_renew(RrImagePic*, *list, *len);
}

/*! Add an RrImagePic to an RrImageSet.
  The RrImagePic should _not_ exist in the image cache already.
  Pictures are added to the front of the list, to maintain the ordering of
  newest to oldest.
*/
static void RrImageSetAddPicture(RrImageSet *self, RrImagePic *pic,
                                 gboolean original)
{
    gint i;
    RrImagePic ***list;
    gint *len;

    g_assert(pic->width > 0 && pic->height > 0);
    g_assert(g_hash_table_lookup(self->cache->pic_table, pic) == NULL);

    /* choose which list in the RrImageSet to add the new picture to. */
    if (original) {
        /* remove the resized picture of the same size if one exists */
        for (i = 0; i < self->n_resized; ++i)
            if (self->resized[i]->width == pic->width ||
                self->resized[i]->height == pic->height)
            {
                RrImageSetRemovePictureAt(self, i, FALSE);
                break;
            }

        list = &self->original;
        len = &self->n_original;
    }
    else {
        list = &self->resized;
        len = &self->n_resized;
    }

    /* grow the list by one spot, shift everything down one, and insert the new
       picture at the front of the list */
    *list = g_renew(RrImagePic*, *list, ++*len);
    for (i = *len-1; i > 0; --i)
        (*list)[i] = (*list)[i-1];
    (*list)[0] = pic;

    /* add the picture as a key to point to this image in the cache */
    g_hash_table_insert(self->cache->pic_table, (*list)[0], self);

/*
#ifdef DEBUG
    g_debug("Adding %s picture to the cache:\n    "
            "Image 0x%lx, w %d h %d Hash %u",
            (*list == self->original ? "ORIGINAL" : "RESIZED"),
            (gulong)self, pic->width, pic->height, RrImagePicHash(pic));
#endif
*/
}

/*! Merges two image sets, destroying one, and returning the other. */
RrImageSet* RrImageSetMergeSets(RrImageSet *b, RrImageSet *a)
{
    gint a_i, b_i, merged_i;
    RrImagePic **original, **resized;
    gint n_original, n_resized, tmp;
    GSList *it;

    gint max_resized;

    if (!a)
        return b;
    if (!b)
        return a;
    if (a == b)
        return b;
    /* the original and resized picture lists in an RrImageSet are kept ordered
       as newest to oldest.  we don't have timestamps for them, so we cannot
       preserve this in the merged RrImageSet exactly.  a decent approximation,
       i think, is to add them in alternating order (one from a, one from b,
       repeat).  this way, the newest from each will be near the front at
       least, and in the resized list, when we drop an old picture, we will
       not always only drop from a or b only, but from each of them equally (or
       from whichever has more resized pictures.
    */

    g_assert(b->cache == a->cache);

    max_resized = a->cache->max_resized_saved;

    a_i = b_i = merged_i = 0;
    n_original = a->n_original + b->n_original;
    original = g_new(RrImagePic*, n_original);
    while (merged_i < n_original) {
        if (a_i < a->n_original)
            original[merged_i++] = a->original[a_i++];
        if (b_i < b->n_original)
            original[merged_i++] = b->original[b_i++];
    }

    a_i = b_i = merged_i = 0;
    n_resized = MIN(max_resized, a->n_resized + b->n_resized);
    resized = g_new(RrImagePic*, n_resized);
    while (merged_i < n_resized) {
        if (a_i < a->n_resized)
            resized[merged_i++] = a->resized[a_i++];
        if (b_i < b->n_resized && merged_i < n_resized)
            resized[merged_i++] = b->resized[b_i++];
    }

    /* if there are any RrImagePic objects left over in a->resized or
       b->resized, they need to be disposed of, and removed from the cache.

       updates the size of the list, as we want to remember which pointers
       were merged from which list (and don't want to remember the ones we
       did not merge and have freed).
    */
    tmp = a_i;
    for (; a_i < a->n_resized; ++a_i) {
        g_hash_table_remove(a->cache->pic_table, a->resized[a_i]);
        RrImagePicFree(a->resized[a_i]);
    }
    a->n_resized = tmp;

    tmp = b_i;
    for (; b_i < b->n_resized; ++b_i) {
        g_hash_table_remove(a->cache->pic_table, b->resized[b_i]);
        RrImagePicFree(b->resized[b_i]);
    }
    b->n_resized = tmp;

    /* we will use the a object as the merge destination, so things in b will
       be moving.

       the cache's name_table will point to b for all the names in b->names,
       so these need to be updated to point at a instead.
       also, the cache's pic_table will point to b for all the pictures in b,
       so these need to be updated to point at a as well.

       any RrImage objects that were using b should now use a instead.

       the names and images will be all moved into a, and the merged picture
       lists will be placed in a.  the pictures in a and b are moved to new
       arrays, so the arrays in a and b need to be freed explicitly (the
       RrImageSetFree function would free the picture data too which we do not
       want here). then b can be freed.
    */

    for (it = b->names; it; it = g_slist_next(it))
        g_hash_table_insert(a->cache->name_table, it->data, a);
    for (b_i = 0; b_i < b->n_original; ++b_i)
        g_hash_table_insert(a->cache->pic_table, b->original[b_i], a);
    for (b_i = 0; b_i < b->n_resized; ++b_i)
        g_hash_table_insert(a->cache->pic_table, b->resized[b_i], a);

    for (it = b->images; it; it = g_slist_next(it))
        ((RrImage*)it->data)->set = a;

    a->images = g_slist_concat(a->images, b->images);
    b->images = NULL;
    a->names = g_slist_concat(a->names, b->names);
    b->names = NULL;

    a->n_original = a->n_resized = 0;
    g_free(a->original);
    g_free(a->resized);
    a->original = a->resized = NULL;
    b->n_original = b->n_resized = 0;
    g_free(b->original);
    g_free(b->resized);
    b->original = b->resized = NULL;

    a->n_original = n_original;
    a->original = original;
    a->n_resized = n_resized;
    a->resized = resized;

    RrImageSetFree(b);

    return a;
}

static void RrImageSetAddName(RrImageSet *set, const gchar *name)
{
    gchar *n;

    n = g_strdup(name);
    set->names = g_slist_prepend(set->names, n);

    /* add the new name to the hash table */
    g_assert(g_hash_table_lookup(set->cache->name_table, n) == NULL);
    g_hash_table_insert(set->cache->name_table, n, set);
}


/************************************************************************
 RrImage functions.
**************************************************************************/


void RrImageRef(RrImage *self)
{
    ++self->ref;
}

void RrImageUnref(RrImage *self)
{
    if (self && --self->ref == 0) {
        RrImageSet *set;
/*
#ifdef DEBUG
        g_debug("Refcount to 0, removing ALL pictures from the cache:\n    "
                "Image 0x%lx", (gulong)self);
#endif
*/
        if (self->destroy_func)
            self->destroy_func(self, self->destroy_data);

        set = self->set;
        set->images = g_slist_remove(set->images, self);

        /* free the set as well if there are no images pointing to it */
        if (!set->images)
            RrImageSetFree(set);
        g_slice_free(RrImage, self);
    }
}

/*! Set function that will be called just before RrImage is destroyed. */
void RrImageSetDestroyFunc(RrImage *self, RrImageDestroyFunc func,
                           gpointer data)
{
    self->destroy_func = func;
    self->destroy_data = data;
}

void RrImageAddFromData(RrImage *self, RrPixel32 *data, gint w, gint h)
{
    RrImagePic pic, *ppic;
    RrImageSet *set;

    g_return_if_fail(self != NULL);
    g_return_if_fail(data != NULL);
    g_return_if_fail(w > 0 && h > 0);

    RrImagePicInit(&pic, w, h, data);
    set = g_hash_table_lookup(self->set->cache->pic_table, &pic);
    if (set)
        self->set = RrImageSetMergeSets(self->set, set);
    else {
        ppic = RrImagePicNew(w, h, data);
        RrImageSetAddPicture(self->set, ppic, TRUE);
    }
}

RrImage* RrImageNewFromData(RrImageCache *cache, RrPixel32 *data,
                            gint w, gint h)
{
    RrImagePic pic, *ppic;
    RrImage *self;
    RrImageSet *set;

    g_return_val_if_fail(cache != NULL, NULL);
    g_return_val_if_fail(data != NULL, NULL);
    g_return_val_if_fail(w > 0 && h > 0, NULL);

    /* finds a picture in the cache, if it is already in there, and use the
       RrImageSet the picture lives in. */
    RrImagePicInit(&pic, w, h, data);
    set = g_hash_table_lookup(cache->pic_table, &pic);
    if (set) {
        self = set->images->data; /* just grab any RrImage from the list */
        RrImageRef(self);
        return self;
    }

    /* the image does not exist in any RrImageSet in the cache, so make
       a new RrImageSet, and a new RrImage that points to it, and place the
       new image inside the new RrImageSet */

    self = g_slice_new0(RrImage);
    self->ref = 1;
    self->set = g_slice_new0(RrImageSet);
    self->set->cache = cache;
    self->set->images = g_slist_append(self->set->images, self);

    ppic = RrImagePicNew(w, h, data);
    RrImageSetAddPicture(self->set, ppic, TRUE);

    return self;
}

#if defined(USE_IMLIB2)
typedef struct _ImlibLoader ImlibLoader;

struct _ImlibLoader
{
    Imlib_Image img;
};

void DestroyImlibLoader(ImlibLoader *loader)
{
    if (!loader)
        return;

    imlib_free_image();
    g_slice_free(ImlibLoader, loader);
}

ImlibLoader* LoadWithImlib(gchar *path,
                           RrPixel32 **pixel_data,
                           gint *width,
                           gint *height)
{
    ImlibLoader *loader = g_slice_new0(ImlibLoader);
    if (!(loader->img = imlib_load_image(path))) {
        DestroyImlibLoader(loader);
        return NULL;
    }

    /* Get data and dimensions of the image.

       WARNING: This stuff is NOT threadsafe !!
    */
    imlib_context_set_image(loader->img);
    *pixel_data = imlib_image_get_data_for_reading_only();
    *width = imlib_image_get_width();
    *height = imlib_image_get_height();

    return loader;
}
#endif  /* USE_IMLIB2 */

#if defined(USE_LIBRSVG)
typedef struct _RsvgLoader RsvgLoader;

struct _RsvgLoader
{
    RsvgHandle *handle;
    cairo_surface_t *surface;
    RrPixel32 *pixel_data;
};

void DestroyRsvgLoader(RsvgLoader *loader)
{
    if (!loader)
        return;

    if (loader->pixel_data)
        g_free(loader->pixel_data);
    if (loader->surface)
        cairo_surface_destroy(loader->surface);
    if (loader->handle)
        g_object_unref(loader->handle);
    g_slice_free(RsvgLoader, loader);
}

RsvgLoader* LoadWithRsvg(gchar *path,
                         RrPixel32 **pixel_data,
                         gint *width,
                         gint *height)
{
    RsvgLoader *loader = g_slice_new0(RsvgLoader);

    if (!(loader->handle = rsvg_handle_new_from_file(path, NULL))) {
        DestroyRsvgLoader(loader);
        return NULL;
    }

    if (!rsvg_handle_close(loader->handle, NULL)) {
        DestroyRsvgLoader(loader);
        return NULL;
    }

    RsvgDimensionData dimension_data;
    rsvg_handle_get_dimensions(loader->handle, &dimension_data);
    *width = dimension_data.width;
    *height = dimension_data.height;

    loader->surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, *width, *height);

    cairo_t* context = cairo_create(loader->surface);
    gboolean success = rsvg_handle_render_cairo(loader->handle, context);
    cairo_destroy(context);

    if (!success) {
        DestroyRsvgLoader(loader);
        return NULL;
    }

    loader->pixel_data = g_new(guint32, *width * *height);

    /*
      Cairo has its data in ARGB with premultiplied alpha, but RrPixel32
      non-premultipled, so convert that. Also, RrPixel32 doesn't allow
      strides not equal to the width of the image.
    */

    /* Verify that RrPixel32 has the same ordering as cairo. */
    g_assert(RrDefaultAlphaOffset == 24);
    g_assert(RrDefaultRedOffset == 16);
    g_assert(RrDefaultGreenOffset == 8);
    g_assert(RrDefaultBlueOffset == 0);

    guint32 *out_row = loader->pixel_data;

    guint32 *in_row =
        (guint32*)cairo_image_surface_get_data(loader->surface);
    gint in_stride = cairo_image_surface_get_stride(loader->surface);

    gint y;
    for (y = 0; y < *height; ++y) {
        gint x;
        for (x = 0; x < *width; ++x) {
            guchar a = in_row[x] >> 24;
            guchar r = (in_row[x] >> 16) & 0xff;
            guchar g = (in_row[x] >> 8) & 0xff;
            guchar b = in_row[x] & 0xff;
            out_row[x] =
                ((r * 256 / (a + 1)) << RrDefaultRedOffset) +
                ((g * 256 / (a + 1)) << RrDefaultGreenOffset) +
                ((b * 256 / (a + 1)) << RrDefaultBlueOffset) +
                (a << RrDefaultAlphaOffset);
        }
        in_row += in_stride / 4;
        out_row += *width;
    }

    *pixel_data = loader->pixel_data;

    return loader;
}
#endif  /* USE_LIBRSVG */

RrImage* RrImageNewFromName(RrImageCache *cache, const gchar *name)
{
    RrImage *self;
    RrImageSet *set;
    gint w, h;
    RrPixel32 *data;
    gchar *path;
    gboolean loaded;

#if defined(USE_IMLIB2)
    ImlibLoader *imlib_loader = NULL;
#endif
#if defined(USE_LIBRSVG)
    RsvgLoader *rsvg_loader = NULL;
#endif

    g_return_val_if_fail(cache != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    set = g_hash_table_lookup(cache->name_table, name);
    if (set) {
        self = set->images->data;
        RrImageRef(self);
        return self;
    }

    /* XXX find the path via freedesktop icon spec (use obt) ! */
    path = g_strdup(name);

    loaded = FALSE;
#if defined(USE_LIBRSVG)
    if (!loaded) {
        rsvg_loader = LoadWithRsvg(path, &data, &w, &h);
        loaded = !!rsvg_loader;
    }
#endif
#if defined(USE_IMLIB2)
    if (!loaded) {
        imlib_loader = LoadWithImlib(path, &data, &w, &h);
        loaded = !!imlib_loader;
    }
#endif

    if (!loaded) {
        g_message("Cannot load image \"%s\" from file \"%s\"", name, path);
        g_free(path);
#if defined(USE_LIBRSVG)
        DestroyRsvgLoader(rsvg_loader);
#endif
#if defined(USE_IMLIB2)
        DestroyImlibLoader(imlib_loader);
#endif
        return NULL;
    }

    g_free(path);

    /* get an RrImage that contains an RrImageSet with this picture in it.
       the RrImage might be new, or reused if the picture was already in the
       cache.

       either way, we get back an RrImageSet (via the RrImage), and we must add
       the name to that RrImageSet.  because of the check above, we know that
       there is no RrImageSet in the cache which already has the given name
       asosciated with it.
    */

    self = RrImageNewFromData(cache, data, w, h);
    RrImageSetAddName(self->set, name);

#if defined(USE_LIBRSVG)
    DestroyRsvgLoader(rsvg_loader);
#endif
#if defined(USE_IMLIB2)
    DestroyImlibLoader(imlib_loader);
#endif

    return self;
}

/************************************************************************
 Image drawing and resizing operations.
**************************************************************************/

/*! Given a picture in RGBA format, of a specified size, resize it to the new
  requested size (but keep its aspect ratio).  If the image does not need to
  be resized (it is already the right size) then this returns NULL.  Otherwise
  it returns a newly allocated RrImagePic with the resized picture inside it
  @return Returns a newly allocated RrImagePic object with a new version of the
    image in the requested size (keeping aspect ratio).
*/
static RrImagePic* ResizeImage(RrPixel32 *src,
                               gulong srcW, gulong srcH,
                               gulong dstW, gulong dstH)
{
    RrPixel32 *dst, *dststart;
    RrImagePic *pic;
    gulong dstX, dstY, srcX, srcY;
    gulong srcX1, srcX2, srcY1, srcY2;
    gulong ratioX, ratioY;
    gulong aspectW, aspectH;

    g_assert(srcW > 0);
    g_assert(srcH > 0);
    g_assert(dstW > 0);
    g_assert(dstH > 0);

    /* keep the aspect ratio */
    aspectW = dstW;
    aspectH = (gint)(dstW * ((gdouble)srcH / srcW));
    if (aspectH > dstH) {
        aspectH = dstH;
        aspectW = (gint)(dstH * ((gdouble)srcW / srcH));
    }
    dstW = aspectW ? aspectW : 1;
    dstH = aspectH ? aspectH : 1;

    if (srcW == dstW && srcH == dstH)
        return NULL; /* no scaling needed! */

    dststart = dst = g_new(RrPixel32, dstW * dstH);

    ratioX = (srcW << FRACTION) / dstW;
    ratioY = (srcH << FRACTION) / dstH;

    srcY2 = 0;
    for (dstY = 0; dstY < dstH; dstY++) {
        srcY1 = srcY2;
        srcY2 += ratioY;

        srcX2 = 0;
        for (dstX = 0; dstX < dstW; dstX++) {
            gulong red = 0, green = 0, blue = 0, alpha = 0;
            gulong portionX, portionY, portionXY, sumXY = 0;
            RrPixel32 pixel;

            srcX1 = srcX2;
            srcX2 += ratioX;

            for (srcY = srcY1; srcY < srcY2; srcY += (1UL << FRACTION)) {
                if (srcY == srcY1) {
                    srcY = FLOOR(srcY);
                    portionY = (1UL << FRACTION) - (srcY1 - srcY);
                    if (portionY > srcY2 - srcY1)
                        portionY = srcY2 - srcY1;
                }
                else if (srcY == FLOOR(srcY2))
                    portionY = srcY2 - srcY;
                else
                    portionY = (1UL << FRACTION);

                for (srcX = srcX1; srcX < srcX2; srcX += (1UL << FRACTION)) {
                    if (srcX == srcX1) {
                        srcX = FLOOR(srcX);
                        portionX = (1UL << FRACTION) - (srcX1 - srcX);
                        if (portionX > srcX2 - srcX1)
                            portionX = srcX2 - srcX1;
                    }
                    else if (srcX == FLOOR(srcX2))
                        portionX = srcX2 - srcX;
                    else
                        portionX = (1UL << FRACTION);

                    portionXY = (portionX * portionY) >> FRACTION;
                    sumXY += portionXY;

                    pixel = *(src + (srcY >> FRACTION) * srcW
                            + (srcX >> FRACTION));
                    red   += ((pixel >> RrDefaultRedOffset)   & 0xFF)
                             * portionXY;
                    green += ((pixel >> RrDefaultGreenOffset) & 0xFF)
                             * portionXY;
                    blue  += ((pixel >> RrDefaultBlueOffset)  & 0xFF)
                             * portionXY;
                    alpha += ((pixel >> RrDefaultAlphaOffset) & 0xFF)
                             * portionXY;
                }
            }

            g_assert(sumXY != 0);
            red   /= sumXY;
            green /= sumXY;
            blue  /= sumXY;
            alpha /= sumXY;

            *dst++ = (red   << RrDefaultRedOffset)   |
                     (green << RrDefaultGreenOffset) |
                     (blue  << RrDefaultBlueOffset)  |
                     (alpha << RrDefaultAlphaOffset);
        }
    }

    pic = g_slice_new(RrImagePic);
    RrImagePicInit(pic, dstW, dstH, dststart);

    return pic;
}

/*! This draws an RGBA picture into the target, within the rectangle specified
  by the area parameter.  If the area's size differs from the source's then it
  will be centered within the rectangle */
void DrawRGBA(RrPixel32 *target, gint target_w, gint target_h,
              RrPixel32 *source, gint source_w, gint source_h,
              gint alpha, RrRect *area)
{
    RrPixel32 *dest;
    gint col, num_pixels;
    gint dw, dh;

    g_assert(source_w <= area->width && source_h <= area->height);
    g_assert(area->x + area->width <= target_w);
    g_assert(area->y + area->height <= target_h);

    /* keep the aspect ratio */
    dw = area->width;
    dh = (gint)(dw * ((gdouble)source_h / source_w));
    if (dh > area->height) {
        dh = area->height;
        dw = (gint)(dh * ((gdouble)source_w / source_h));
    }

    /* copy source -> dest, and apply the alpha channel.
       center the image if it is smaller than the area */
    col = 0;
    num_pixels = dw * dh;
    dest = target + area->x + (area->width - dw) / 2 +
        (target_w * (area->y + (area->height - dh) / 2));
    while (num_pixels-- > 0) {
        guchar a, r, g, b, bgr, bgg, bgb;

        /* apply the rgba's opacity as well */
        a = ((*source >> RrDefaultAlphaOffset) * alpha) >> 8;
        r = *source >> RrDefaultRedOffset;
        g = *source >> RrDefaultGreenOffset;
        b = *source >> RrDefaultBlueOffset;

        /* background color */
        bgr = *dest >> RrDefaultRedOffset;
        bgg = *dest >> RrDefaultGreenOffset;
        bgb = *dest >> RrDefaultBlueOffset;

        r = bgr + (((r - bgr) * a) >> 8);
        g = bgg + (((g - bgg) * a) >> 8);
        b = bgb + (((b - bgb) * a) >> 8);

        *dest = ((r << RrDefaultRedOffset) |
                 (g << RrDefaultGreenOffset) |
                 (b << RrDefaultBlueOffset));

        dest++;
        source++;

        if (++col >= dw) {
            col = 0;
            dest += target_w - dw;
        }
    }
}

/*! Draw an RGBA texture into a target pixel buffer. */
void RrImageDrawRGBA(RrPixel32 *target, RrTextureRGBA *rgba,
                     gint target_w, gint target_h,
                     RrRect *area)
{
    RrImagePic *scaled;

    scaled = ResizeImage(rgba->data, rgba->width, rgba->height,
                         area->width, area->height);

    if (scaled) {
#ifdef DEBUG
            g_warning("Scaling an RGBA! You should avoid this and just make "
                      "it the right size yourself!");
#endif
            DrawRGBA(target, target_w, target_h,
                     scaled->data, scaled->width, scaled->height,
                     rgba->alpha, area);
            RrImagePicFree(scaled);
    }
    else
        DrawRGBA(target, target_w, target_h,
                 rgba->data, rgba->width, rgba->height,
                 rgba->alpha, area);
}

/*! Draw an RrImage texture into a target pixel buffer.  If the RrImage does
  not contain a picture of the appropriate size, then one of its "original"
  pictures will be resized and used (and stored in the RrImage as a "resized"
  picture).
 */
void RrImageDrawImage(RrPixel32 *target, RrTextureImage *img,
                      gint target_w, gint target_h,
                      RrRect *area)
{
    gint i, min_diff, min_i, min_aspect_diff, min_aspect_i;
    RrImage *self;
    RrImageSet *set;
    RrImagePic *pic;
    gboolean free_pic;

    self = img->image;
    set = self->set;
    pic = NULL;
    free_pic = FALSE;

    /* is there an original of this size? (only the larger of
       w or h has to be right cuz we maintain aspect ratios) */
    for (i = 0; i < set->n_original; ++i)
        if ((set->original[i]->width >= set->original[i]->height &&
             set->original[i]->width == area->width) ||
            (set->original[i]->width <= set->original[i]->height &&
             set->original[i]->height == area->height))
        {
            pic = set->original[i];
            break;
        }

    /* is there a resize of this size? */
    for (i = 0; i < set->n_resized; ++i)
        if ((set->resized[i]->width >= set->resized[i]->height &&
             set->resized[i]->width == area->width) ||
            (set->resized[i]->width <= set->resized[i]->height &&
             set->resized[i]->height == area->height))
        {
            gint j;
            RrImagePic *saved;

            /* save the selected one */
            saved = set->resized[i];

            /* shift all the others down */
            for (j = i; j > 0; --j)
                set->resized[j] = set->resized[j-1];

            /* and move the selected one to the top of the list */
            set->resized[0] = saved;

            pic = set->resized[0];
            break;
        }

    if (!pic) {
        gdouble aspect;
        RrImageSet *cache_set;

        /* find an original with a close size */
        min_diff = min_aspect_diff = -1;
        min_i = min_aspect_i = 0;
        aspect = ((gdouble)area->width) / area->height;
        for (i = 0; i < set->n_original; ++i) {
            gint diff;
            gint wdiff, hdiff;
            gdouble myasp;

            /* our size difference metric.. */
            wdiff = set->original[i]->width - area->width;
            if (wdiff < 0) wdiff *= 2; /* prefer scaling down than up */
            hdiff = set->original[i]->height - area->height;
            if (hdiff < 0) hdiff *= 2; /* prefer scaling down than up */
            diff = (wdiff * wdiff) + (hdiff * hdiff);

            /* find the smallest difference */
            if (min_diff < 0 || diff < min_diff) {
                min_diff = diff;
                min_i = i;
            }
            /* and also find the smallest difference with the same aspect
               ratio (and prefer this one) */
            myasp = ((gdouble)set->original[i]->width) /
                set->original[i]->height;
            if (ABS(aspect - myasp) < 0.0000001 &&
                (min_aspect_diff < 0 || diff < min_aspect_diff))
            {
                min_aspect_diff = diff;
                min_aspect_i = i;
            }
        }

        /* use the aspect ratio correct source if there is one */
        if (min_aspect_i >= 0)
            min_i = min_aspect_i;

        /* resize the original to the given area */
        pic = ResizeImage(set->original[min_i]->data,
                          set->original[min_i]->width,
                          set->original[min_i]->height,
                          area->width, area->height);

        /* is it already in the cache ? */
        cache_set = g_hash_table_lookup(set->cache->pic_table, pic);
        if (cache_set) {
            /* merge this set with the one found in the cache - they are
               apparently the same image !  then next time we won't have to do
               this resizing, we will use the cache_set's pic instead. */
            set = RrImageSetMergeSets(set, cache_set);
            free_pic = TRUE;
        }
        else {
            /* add the resized image to the image, as the first in the resized
               list */
            while (set->n_resized >= set->cache->max_resized_saved)
                /* remove the last one (last used one) to make space for
                 adding our resized picture */
                RrImageSetRemovePictureAt(set, set->n_resized-1, FALSE);
            if (set->cache->max_resized_saved)
                /* add it to the resized list */
                RrImageSetAddPicture(set, pic, FALSE);
            else
                free_pic = TRUE; /* don't leak mem! */
        }
    }

    /* The RrImageSet may have changed if we merged it with another, so the
       RrImage object needs to be updated to use the new merged RrImageSet. */
    self->set = set;

    g_assert(pic != NULL);

    DrawRGBA(target, target_w, target_h,
             pic->data, pic->width, pic->height,
             img->alpha, area);
    if (free_pic)
        RrImagePicFree(pic);
}
