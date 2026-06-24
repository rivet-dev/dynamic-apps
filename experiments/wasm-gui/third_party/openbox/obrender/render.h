/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   render.h for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens
   Copyright (c) 2003        Derek Foreman

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

#ifndef __render_h
#define __render_h

#include <X11/Xlib.h> /* some platforms dont include this as needed for Xft */
#include <pango/pangoxft.h>
#include <glib.h>

G_BEGIN_DECLS

#include "obrender/geom.h"
#include "obrender/version.h"

typedef union  _RrTextureData      RrTextureData;
typedef struct _RrAppearance       RrAppearance;
typedef struct _RrSurface          RrSurface;
typedef struct _RrFont             RrFont;
typedef struct _RrTexture          RrTexture;
typedef struct _RrTextureMask      RrTextureMask;
typedef struct _RrTextureRGBA      RrTextureRGBA;
typedef struct _RrTextureImage     RrTextureImage;
typedef struct _RrTextureText      RrTextureText;
typedef struct _RrTextureLineArt   RrTextureLineArt;
typedef struct _RrPixmapMask       RrPixmapMask;
typedef struct _RrInstance         RrInstance;
typedef struct _RrColor            RrColor;
typedef struct _RrImage            RrImage;
typedef struct _RrImageSet         RrImageSet;
typedef struct _RrImagePic         RrImagePic;
typedef struct _RrImageCache       RrImageCache;
typedef struct _RrButton           RrButton;

typedef guint32 RrPixel32;  /* ARGB format, not premultiplied alpha */
typedef guint16 RrPixel16;
typedef guchar  RrPixel8;

typedef enum {
    RR_RELIEF_FLAT,
    RR_RELIEF_RAISED,
    RR_RELIEF_SUNKEN,
    RR_RELIEF_NUM_TYPES
} RrReliefType;

typedef enum {
    RR_BEVEL_1,
    RR_BEVEL_2,
    RR_BEVEL_NUM_TYPES
} RrBevelType;

typedef enum {
    RR_SURFACE_NONE,
    RR_SURFACE_PARENTREL,
    RR_SURFACE_SOLID,
    RR_SURFACE_SPLIT_VERTICAL,
    RR_SURFACE_HORIZONTAL,
    RR_SURFACE_VERTICAL,
    RR_SURFACE_DIAGONAL,
    RR_SURFACE_CROSS_DIAGONAL,
    RR_SURFACE_PYRAMID,
    RR_SURFACE_MIRROR_HORIZONTAL,
    RR_SURFACE_NUM_TYPES
} RrSurfaceColorType;

typedef enum {
    RR_TEXTURE_NONE,
    RR_TEXTURE_MASK,
    RR_TEXTURE_TEXT,
    RR_TEXTURE_LINE_ART,
    RR_TEXTURE_RGBA,
    RR_TEXTURE_IMAGE,
    RR_TEXTURE_NUM_TYPES
} RrTextureType;

typedef enum {
    RR_JUSTIFY_LEFT,
    RR_JUSTIFY_CENTER,
    RR_JUSTIFY_RIGHT,
    RR_JUSTIFY_NUM_TYPES
} RrJustify;

/* Put middle first so it's the default */
typedef enum {
    RR_ELLIPSIZE_MIDDLE,
    RR_ELLIPSIZE_NONE,
    RR_ELLIPSIZE_START,
    RR_ELLIPSIZE_END,
    RR_ELLIPSIZE_NUM_TYPES
} RrEllipsizeMode;

typedef enum {
    RR_FONTWEIGHT_LIGHT,
    RR_FONTWEIGHT_NORMAL,
    RR_FONTWEIGHT_SEMIBOLD,
    RR_FONTWEIGHT_BOLD,
    RR_FONTWEIGHT_ULTRABOLD,
    RR_FONTWEIGHT_NUM_TYPES
} RrFontWeight;

typedef enum {
    RR_FONTSLANT_NORMAL,
    RR_FONTSLANT_ITALIC,
    RR_FONTSLANT_OBLIQUE,
    RR_FONTSLANT_NUM_TYPES
} RrFontSlant;

struct _RrSurface {
    RrSurfaceColorType grad;
    RrReliefType relief;
    RrBevelType bevel;
    RrColor *primary;
    RrColor *secondary;
    RrColor *border_color;
    RrColor *bevel_dark;
    RrColor *bevel_light;
    RrColor *interlace_color;
    gboolean interlaced;
    gboolean border;
    RrAppearance *parent;
    gint parentx;
    gint parenty;
    RrPixel32 *pixel_data;
    gint bevel_dark_adjust;  /* 0-255, default is 64 */
    gint bevel_light_adjust; /* 0-255, default is 128 */
    RrColor *split_primary;
    RrColor *split_secondary;
};

struct _RrTextureText {
    RrFont *font;
    RrJustify justify;
    RrColor *color;
    const gchar *string;
    gint shadow_offset_x;
    gint shadow_offset_y;
    RrColor *shadow_color;
    gboolean shortcut; /*!< Underline a character */
    guint shortcut_pos; /*!< Position in bytes of the character to underline */
    RrEllipsizeMode ellipsize;
    gboolean flow; /* allow multiple lines.  must set maxwidth below */
    gint maxwidth;
    guchar shadow_alpha; /* at the bottom to improve alignment */
};

struct _RrPixmapMask {
    const RrInstance *inst;
    Pixmap mask;
    gint width;
    gint height;
    gchar *data;
};

struct _RrTextureMask {
    RrColor *color;
    RrPixmapMask *mask;
};

struct _RrTextureRGBA {
    gint width;
    gint height;
    gint alpha;
    RrPixel32 *data;
    /* size and position to draw at (if these are zero, then it will be
       drawn to fill the entire texture */
    gint tx;
    gint ty;
    gint twidth;
    gint theight;
};

struct _RrTextureImage {
    RrImage *image;
    gint alpha;
    /* size and position to draw at (if these are zero, then it will be
       drawn to fill the entire texture */
    gint tx;
    gint ty;
    gint twidth;
    gint theight;
};

struct _RrTextureLineArt {
    RrColor *color;
    gint x1;
    gint y1;
    gint x2;
    gint y2;
};

union _RrTextureData {
    RrTextureRGBA rgba;
    RrTextureImage image;
    RrTextureText text;
    RrTextureMask mask;
    RrTextureLineArt lineart;
};

struct _RrTexture {
    /* If changing the type of a texture, you should DEFINITELY call
       RrAppearanceClearTextures() first! */
    RrTextureType type;
    RrTextureData data;
};

struct _RrAppearance {
    const RrInstance *inst;
    
    RrSurface surface;
    gint textures;
    RrTexture *texture;
    Pixmap pixmap;
    XftDraw *xftdraw;

    /* cached for internal use */
    gint w, h;
};

/*! Holds a RGBA image picture */
struct _RrImagePic {
    gint width, height;
    RrPixel32 *data;
    /* The sum of all the pixels.  This is used to compare pictures if their
       hashes match. */
    gint sum;
};

typedef void (*RrImageDestroyFunc)(RrImage *image, gpointer data);

/*! An RrImage refers to a RrImageSet.  If multiple RrImageSets end up
  holding the same image data, they will be marged and the RrImages that
  point to them would be updated. */
struct _RrImage {
    gint ref;
    RrImageSet *set;

    /* This function (if not NULL) will be called just before destroying
      RrImage. */
    RrImageDestroyFunc destroy_func;
    gpointer           destroy_data;
};

/*! An RrImage is a sort of meta-image.  It can contain multiple versions
  of an image at different sizes, which may or may not be completely different
  pictures */
struct _RrImageSet
{
    RrImageCache *cache;

    /*! If a picture is loaded by a name, then it has a name attached to it.
      This contains a list of strings, containing all names that have ever
      been associated with the RrImageSet. A name in the RrImageCache can
      only be associated with a single RrImageSet. */
    GSList *names;

    /*! RrImages that point at this RrImageSet. If this is empty, then there
      are no images using the set and it can be freed. */
    GSList *images;

    /*! An array of "originals", that is of RrPictures that have been added
      to the image in various sizes, and that have not been resized.  These
      are explicitly added to the RrImageSet. */
    RrImagePic **original;
    gint n_original;
    /*! An array of "resized" pictures.  When an "original" RrPicture
      needs to be resized for drawing, it is saved in here so that it doesn't
      need to be resized again.  These are automatically added to the
      RrImage. */
    RrImagePic **resized;
    gint n_resized;
};

struct _RrButton {
    const RrInstance *inst;

    /* colors */
    RrColor *focused_unpressed_color;
    RrColor *unfocused_unpressed_color;
    RrColor *focused_pressed_color;
    RrColor *unfocused_pressed_color;
    RrColor *focused_disabled_color;
    RrColor *unfocused_disabled_color;
    RrColor *focused_hover_color;
    RrColor *unfocused_hover_color;
    RrColor *focused_hover_toggled_color;
    RrColor *unfocused_hover_toggled_color;
    RrColor *focused_pressed_toggled_color;
    RrColor *unfocused_pressed_toggled_color;
    RrColor *focused_unpressed_toggled_color;
    RrColor *unfocused_unpressed_toggled_color;
    
    /* masks */
    RrPixmapMask *unpressed_mask;
    RrPixmapMask *pressed_mask;
    RrPixmapMask *disabled_mask;
    RrPixmapMask *hover_mask;
    RrPixmapMask *unpressed_toggled_mask;
    RrPixmapMask *hover_toggled_mask;
    RrPixmapMask *pressed_toggled_mask;
   
    /* textures */
    RrAppearance *a_focused_unpressed;
    RrAppearance *a_unfocused_unpressed;
    RrAppearance *a_focused_pressed;
    RrAppearance *a_unfocused_pressed;
    RrAppearance *a_focused_disabled;
    RrAppearance *a_unfocused_disabled;
    RrAppearance *a_focused_hover;
    RrAppearance *a_unfocused_hover;
    RrAppearance *a_focused_unpressed_toggled;
    RrAppearance *a_unfocused_unpressed_toggled;
    RrAppearance *a_focused_pressed_toggled;
    RrAppearance *a_unfocused_pressed_toggled;
    RrAppearance *a_focused_hover_toggled;
    RrAppearance *a_unfocused_hover_toggled;

};

/* these are the same on all endian machines because it seems to be dependant
   on the endianness of the gfx card, not the cpu. */
#define RrDefaultAlphaOffset 24
#define RrDefaultRedOffset 16
#define RrDefaultGreenOffset 8
#define RrDefaultBlueOffset 0

#define RrDefaultFontFamily       "arial,sans"
#define RrDefaultFontSize         8
#define RrDefaultFontWeight       RR_FONTWEIGHT_NORMAL
#define RrDefaultFontSlant        RR_FONTSLANT_NORMAL

RrInstance* RrInstanceNew (Display *display, gint screen);
void        RrInstanceFree (RrInstance *inst);

Display* RrDisplay      (const RrInstance *inst);
gint     RrScreen       (const RrInstance *inst);
Window   RrRootWindow   (const RrInstance *inst);
Visual*  RrVisual       (const RrInstance *inst);
gint     RrDepth        (const RrInstance *inst);
Colormap RrColormap     (const RrInstance *inst);
gint     RrRedOffset    (const RrInstance *inst);
gint     RrGreenOffset  (const RrInstance *inst);
gint     RrBlueOffset   (const RrInstance *inst);
gint     RrRedShift     (const RrInstance *inst);
gint     RrGreenShift   (const RrInstance *inst);
gint     RrBlueShift    (const RrInstance *inst);
gint     RrRedMask      (const RrInstance *inst);
gint     RrGreenMask    (const RrInstance *inst);
gint     RrBlueMask     (const RrInstance *inst);

RrColor *RrColorNew   (const RrInstance *inst, gint r, gint g, gint b);
RrColor *RrColorCopy  (RrColor *c);
RrColor *RrColorParse (const RrInstance *inst, gchar *colorname);
void     RrColorFree  (RrColor *in);

gint     RrColorRed   (const RrColor *c);
gint     RrColorGreen (const RrColor *c);
gint     RrColorBlue  (const RrColor *c);
gulong   RrColorPixel (const RrColor *c);
GC       RrColorGC    (RrColor *c);

RrAppearance *RrAppearanceNew  (const RrInstance *inst, gint numtex);
RrAppearance *RrAppearanceCopy (RrAppearance *a);
void          RrAppearanceFree (RrAppearance *a);
void          RrAppearanceRemoveTextures(RrAppearance *a);
void          RrAppearanceAddTextures(RrAppearance *a, gint numtex);
/*! Always call this when changing the type of a texture in an appearance */
void          RrAppearanceClearTextures(RrAppearance *a);

RrButton *RrButtonNew (const RrInstance *inst);
void      RrButtonFree(RrButton *b);

RrFont *RrFontOpen          (const RrInstance *inst, const gchar *name,
                             gint size, RrFontWeight weight, RrFontSlant slant);
RrFont *RrFontOpenDefault   (const RrInstance *inst);
void    RrFontClose         (RrFont *f);
/*! Returns an RrSize, that was allocated with g_slice_new().  Use g_slice_free() to
  free it. */
RrSize *RrFontMeasureString (const RrFont *f, const gchar *str,
                             gint shadow_offset_x, gint shadow_offset_y,
                             gboolean flow, gint maxwidth);
gint    RrFontHeight        (const RrFont *f, gint shadow_offset_y);
gint    RrFontMaxCharWidth  (const RrFont *f);

/* Paint into the appearance. The old pixmap is returned (if there was one). It
   is the responsibility of the caller to call XFreePixmap on the return when
   it is non-null. */
Pixmap RrPaintPixmap (RrAppearance *a, gint w, gint h);
void   RrPaint       (RrAppearance *a, Window win, gint w, gint h);
void   RrMinSize     (RrAppearance *a, gint *w, gint *h);
gint   RrMinWidth    (RrAppearance *a);
/* For text textures, if flow is TRUE, then the string must be set before
   calling this, otherwise it doesn't need to be */
gint   RrMinHeight   (RrAppearance *a);
void   RrMargins     (RrAppearance *a, gint *l, gint *t, gint *r, gint *b);

gboolean RrPixmapToRGBA(const RrInstance *inst,
                        Pixmap pmap, Pixmap mask,
                        gint *w, gint *h, RrPixel32 **data);

/*! Create a new image cache for RrImages.
  @param max_resized_saved The number of resized copies of an image to save
*/
RrImageCache* RrImageCacheNew(gint max_resized_saved);
void          RrImageCacheRef(RrImageCache *self);
void          RrImageCacheUnref(RrImageCache *self);

/*! Create a new image, or return one from the cache that matches.
  @param cache The image cache.
  @param old The current RrImage, which the new image should be added to.
    Use this if loading a different sized version of the same image.
    The returned RrImage should replace the one passed in as old.
    Pass NULL here if adding an image which is (or may be) entirely new.
  @param name The name of the icon to be loaded off disk, or used in the cache
  @return Returns NULL if unable to load an image by the name and it is not in
    the cache already
*/
RrImage* RrImageNewFromName(RrImageCache *cache, const gchar *name);

/*! Create a new image, or return one from the cache that matches.
  @param cache The image cache.
  @param data The image data in RGBA32 format.  There should be @w * @h many
    values in the data array.
  @param w The width of the image data.
  @param h The height of the image data.
  @return Returns NULL if unable to load an image by the name and it is not in
    the cache already
*/
RrImage* RrImageNewFromData(RrImageCache *cache, RrPixel32 *data,
                            gint w, gint h);

/*! Add a new size of a picture to an image.
  If a picture has multiple versions of different sizes (example 16x16, 32x32
  and so on), they should all be under the same RrImage.  This adds a new
  size to an existing RrImage, associating the newly sized picture with the
  others in the RrImage - classifying them as being the same logical image at a
  different dimention.
*/
void RrImageAddFromData(RrImage *image, RrPixel32 *data, gint w, gint h);

void RrImageRef(RrImage *im);
void RrImageUnref(RrImage *im);

G_END_DECLS

#endif /*__render_h*/
