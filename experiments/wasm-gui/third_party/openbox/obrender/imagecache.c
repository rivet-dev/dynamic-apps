/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   imagecache.c for the Openbox window manager
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

#include "render.h"
#include "imagecache.h"
#include "image.h"

static gboolean RrImagePicEqual(const RrImagePic *p1,
                                const RrImagePic *p2);

RrImageCache* RrImageCacheNew(gint max_resized_saved)
{
    RrImageCache *self;

    g_assert(max_resized_saved >= 0);

    self = g_slice_new(RrImageCache);
    self->ref = 1;
    self->max_resized_saved = max_resized_saved;
    self->pic_table = g_hash_table_new((GHashFunc)RrImagePicHash,
                                       (GEqualFunc)RrImagePicEqual);
    self->name_table = g_hash_table_new(g_str_hash, g_str_equal);
    return self;
}

void RrImageCacheRef(RrImageCache *self)
{
    ++self->ref;
}

void RrImageCacheUnref(RrImageCache *self)
{
    if (self && --self->ref == 0) {
        g_assert(g_hash_table_size(self->pic_table) == 0);
        g_hash_table_unref(self->pic_table);
        self->pic_table = NULL;

        g_assert(g_hash_table_size(self->name_table) == 0);
        g_hash_table_destroy(self->name_table);
        self->name_table = NULL;

        g_slice_free(RrImageCache, self);
    }
}

#define hashsize(n) ((RrPixel32)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
/* mix -- mix 3 32-bit values reversibly. */
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}
/* final -- final mixing of 3 32-bit values (a,b,c) into c */
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/* This is a fast, reversable hash function called "lookup3", found here:
   http://burtleburtle.net/bob/c/lookup3.c, by Bob Jenkins

   This hashing algorithm is "reversible", that is, not cryptographically
   secure at all.  But we don't care about that, we just want something to
   tell when images are the same or different relatively quickly.
*/
guint32 hashword(const guint32 *key, gint length, guint32 initval)
{
    guint32 a,b,c;

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (((guint32)length)<<2) + initval;

    /* handle most of the key */
    while (length > 3)
    {
        a += key[0];
        b += key[1];
        c += key[2];
        mix(a,b,c);
        length -= 3;
        key += 3;
    }

    /* handle the last 3 guint32's */
    switch(length)      /* all the case statements fall through */
    { 
    case 3: c+=key[2];
    case 2: b+=key[1];
    case 1: a+=key[0];
        final(a,b,c);
    case 0:             /* case 0: nothing left to add */
        break;
    }
    /* report the result */
    return c;
}

/*! This is some arbitrary initial value for the hashing function.  It's
  constant so that you get the same result from the same data each time.
*/
#define HASH_INITVAL 0xf00d

guint RrImagePicHash(const RrImagePic *p)
{
    return hashword(p->data, p->width * p->height, HASH_INITVAL);
}

static gboolean RrImagePicEqual(const RrImagePic *p1,
                                const RrImagePic *p2)
{
    return p1->width == p2->width && p1->height == p2->height &&
        p1->sum == p2->sum;
}
