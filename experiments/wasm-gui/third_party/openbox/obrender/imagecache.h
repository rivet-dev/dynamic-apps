/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   imagecache.h for the Openbox window manager
   Copyright (c) 2008        Dana Jansens

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

#ifndef __imagecache_h
#define __imagecache_h

#include <glib.h>

struct _RrImagePic;

guint RrImagePicHash(const struct _RrImagePic *p);

/*! Create a new image cache.  An image cache is basically a hash table to look
  up RrImages.  Each RrImage in the cache may contain one or more Pictures,
  that is one or more actual copies of image data at various sizes.  For eg,
  for a window, all of its various icons are loaded into the same RrImage.
  When an RrImage is drawn and a picture inside it needs to be resized, that
  is also saved within the RrImage.

  For each picture that an RrImage has, the picture is hashed and that is used
  as a key to find the RrImage.  So, given any picture in any RrImage in the
  cache, if you hash it, you will find the RrImage.
*/
struct _RrImageCache {
    gint ref;
    /*! When an original picture is resized for an RrImage, the resized picture
      is saved in the RrImage.  This specifies how many pictures should be
      saved at a time.  When this is exceeded, the least recently used
      "resized" picture is deleted.
    */
    gint max_resized_saved;

    /*! A hash table of image sets in the cache that don't have a file name
      attached to them, with their key being a hash of the contents of the
      image. */
    GHashTable *pic_table;

    /*! Used to find out if an image file has already been loaded into an
      image set. Provides a quick file_name -> RrImageSet lookup. */
    GHashTable *name_table;
};

#endif
