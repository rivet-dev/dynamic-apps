/**
 * @file icon.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Icon functions.
 *
 */

#include "jwm.h"
#include "icon.h"
#include "client.h"
#include "render.h"
#include "main.h"
#include "image.h"
#include "misc.h"
#include "hint.h"
#include "color.h"
#include "settings.h"
#include "border.h"

IconNode emptyIcon;

#ifdef USE_ICONS

/* Must be a power of two. */
#define HASH_SIZE 128

/** Linked list of icon paths. */
typedef struct IconPathNode {
   char *path;
   struct IconPathNode *next;
} IconPathNode;

/* These extensions are appended to icon names during search. */
const char *ICON_EXTENSIONS[] = {
   "",
#ifdef USE_PNG
   ".png",
   ".PNG",
#endif
#if defined(USE_CAIRO) && defined(USE_RSVG)
   ".svg",
   ".SVG",
#endif
#ifdef USE_XPM
   ".xpm",
   ".XPM",
#endif
#ifdef USE_JPEG
   ".jpg",
   ".JPG",
   ".jpeg",
   ".JPEG",
#endif
#ifdef USE_XBM
   ".xbm",
   ".XBM",
#endif
};
static const unsigned EXTENSION_COUNT = ARRAY_LENGTH(ICON_EXTENSIONS);
static const unsigned MAX_EXTENSION_LENGTH = 5;

static IconNode **iconHash;
static IconPathNode *iconPaths;
static IconPathNode *iconPathsTail;
static GC iconGC;
static char iconSizeSet = 0;
static char *defaultIconName;

static void DoDestroyIcon(int index, IconNode *icon);
static IconNode *ReadNetWMIcon(Window win);
static IconNode *ReadWMHintIcon(Window win);
static IconNode *CreateIcon(const ImageNode *image);
static IconNode *CreateIconFromDrawable(Drawable d, Pixmap mask);
static IconNode *CreateIconFromBinary(const unsigned long *data,
                                      unsigned int length);
static IconNode *LoadNamedIconHelper(const char *name, const char *path,
                                     char save, char preserveAspect);

static ImageNode *GetBestImage(IconNode *icon, int rwidth, int rheight);
static ScaledIconNode *GetScaledIcon(IconNode *icon, long fg,
                                     int rwidth, int rheight);

static void InsertIcon(IconNode *icon);
static IconNode *FindIcon(const char *name);
static unsigned int GetHash(const char *str);

/** Initialize icon data.
 * This must be initialized before parsing the configuration.
 */
void InitializeIcons(void)
{
   unsigned int x;
   iconPaths = NULL;
   iconPathsTail = NULL;
   iconHash = Allocate(sizeof(IconNode*) * HASH_SIZE);
   for(x = 0; x < HASH_SIZE; x++) {
      iconHash[x] = NULL;
   }
   memset(&emptyIcon, 0, sizeof(emptyIcon));
   iconSizeSet = 0;
   defaultIconName = NULL;
}

/** Startup icon support. */
void StartupIcons(void)
{
   XGCValues gcValues;
   XIconSize iconSize;
   unsigned long gcMask;
   gcMask = GCGraphicsExposures;
   gcValues.graphics_exposures = False;
   iconGC = JXCreateGC(display, rootWindow, gcMask, &gcValues);

   iconSize.min_width = GetBorderIconSize();
   iconSize.min_height = GetBorderIconSize();
   iconSize.max_width = iconSize.min_width;
   iconSize.max_height = iconSize.min_height;
   iconSize.width_inc = 1;
   iconSize.height_inc = 1;
   JXSetIconSizes(display, rootWindow, &iconSize, 1);
}

/** Shutdown icon support. */
void ShutdownIcons(void)
{
   unsigned int x;
   for(x = 0; x < HASH_SIZE; x++) {
      while(iconHash[x]) {
         DoDestroyIcon(x, iconHash[x]);
      }
   }
   JXFreeGC(display, iconGC);
}

/** Destroy icon data. */
void DestroyIcons(void)
{
   IconPathNode *pn;
   while(iconPaths) {
      pn = iconPaths->next;
      Release(iconPaths->path);
      Release(iconPaths);
      iconPaths = pn;
   }
   iconPathsTail = NULL;
   if(iconHash) {
      Release(iconHash);
      iconHash = NULL;
   }
   if(defaultIconName) {
      Release(defaultIconName);
      defaultIconName = NULL;
   }
}

/** Add an icon search path. */
void AddIconPath(char *path)
{

   IconPathNode *ip;
   int length;
   char addSep;

   if(!path) {
      return;
   }

   Trim(path);

   length = strlen(path);
   if(path[length - 1] != '/') {
      addSep = 1;
   } else {
      addSep = 0;
   }

   ip = Allocate(sizeof(IconPathNode));
   ip->path = Allocate(length + addSep + 1);
   memcpy(ip->path, path, length + 1);
   if(addSep) {
      ip->path[length] = '/';
      ip->path[length + 1] = 0;
   }
   ExpandPath(&ip->path);
   ip->next = NULL;

   if(iconPathsTail) {
      iconPathsTail->next = ip;
   } else {
      iconPaths = ip;
   }
   iconPathsTail = ip;

}

/** Draw an icon. */
void PutIcon(IconNode *icon, Drawable d, long fg,
             int x, int y, int width, int height)
{
   ScaledIconNode *node;

   Assert(icon);

   if(!icon->width || !icon->height) {
      return;
   }

   /* Scale the icon. */
   node = GetScaledIcon(icon, fg, width, height);
   if(node) {

      /* If we support xrender, use it. */
#ifdef USE_XRENDER
      if(icon->render) {
         PutScaledRenderIcon(icon, node, d, x, y, width, height);
         return;
      }
#endif

      /* Draw the icon the old way. */
      if(node->image != None) {

         const int ix = x + (width - node->width) / 2;
         const int iy = y + (height - node->height) / 2;

         /* Set the clip mask. */
         if(node->mask != None) {
            JXSetClipOrigin(display, iconGC, ix, iy);
            JXSetClipMask(display, iconGC, node->mask);
         }

         /* Draw the icon. */
         JXCopyArea(display, node->image, d, iconGC, 0, 0,
                    node->width, node->height, ix, iy);

         /* Reset the clip mask. */
         if(node->mask != None) {
            JXSetClipMask(display, iconGC, None);
            JXSetClipOrigin(display, iconGC, 0, 0);
         }

      }

   }

}

/** Load the icon for a client. */
void LoadIcon(ClientNode *np)
{
   /* If client already has an icon, destroy it first. */
   DestroyIcon(np->icon);
   np->icon = NULL;

   /* Attempt to read _NET_WM_ICON for an icon. */
   np->icon = ReadNetWMIcon(np->window);
   if(np->icon) {
      return;
   }
   if(np->owner != None) {
      np->icon = ReadNetWMIcon(np->owner);
      if(np->icon) {
         return;
      }
   }

   /* Attempt to read an icon from XWMHints. */
   np->icon = ReadWMHintIcon(np->window);
   if(np->icon) {
      return;
   }
   if(np->owner != None) {
      np->icon = ReadNetWMIcon(np->owner);
      if(np->icon) {
         return;
      }
   }

   /* Attempt to read an icon based on the window name. */
   if(np->instanceName) {
      np->icon = LoadNamedIcon(np->instanceName, 1, 1);
      if(np->icon) {
         return;
      }
   }
}

/** Load an icon from a file. */
IconNode *LoadNamedIcon(const char *name, char save, char preserveAspect)
{

   IconNode *icon;
   IconPathNode *ip;

   Assert(name);

   /* If no icon is specified, return an empty icon. */
   if(name[0] == 0) {
      return &emptyIcon;
   }

   /* See if this icon has already been loaded. */
   icon = FindIcon(name);
   if(icon) {
      return icon;
   }

   /* Check for an absolute file name. */
   if(name[0] == '/') {
      ImageNode *image = LoadImage(name, 0, 0, 1);
      if(image) {
         icon = CreateIcon(image);
         icon->preserveAspect = preserveAspect;
         icon->name = CopyString(name);
         if(save) {
            InsertIcon(icon);
         }
         DestroyImage(image);
         return icon;
      } else {
         return &emptyIcon;
      }
   }

   /* Try icon paths. */
   for(ip = iconPaths; ip; ip = ip->next) {
      icon = LoadNamedIconHelper(name, ip->path, save, preserveAspect);
      if(icon) {
         return icon;
      }
   }

   /* The default icon. */
   return NULL;
}

/** Helper for loading icons by name. */
IconNode *LoadNamedIconHelper(const char *name, const char *path,
                              char save, char preserveAspect)
{
   ImageNode *image;
   char *temp;
   const unsigned nameLength = strlen(name);
   const unsigned pathLength = strlen(path);
   const char hasExtension = strchr(name, '.') != NULL;
   unsigned i;

   /* Full file name. */
   temp = AllocateStack(nameLength + pathLength + MAX_EXTENSION_LENGTH + 1);
   memcpy(&temp[0], path, pathLength);
   memcpy(&temp[pathLength], name, nameLength + 1);

   /* Attempt to load the image. */
   image = NULL;
   if(hasExtension) {
      image = LoadImage(temp, 0, 0, 1);
   }
   if(!image) {
      for(i = 0; i < EXTENSION_COUNT; i++) {
         const unsigned len = strlen(ICON_EXTENSIONS[i]);
         memcpy(&temp[pathLength + nameLength], ICON_EXTENSIONS[i], len + 1);
         image = LoadImage(temp, 0, 0, 1);
         if(image) {
            break;
         }
      }
   }
   ReleaseStack(temp);

   /* Create the icon if we were able to load the image. */
   if(image) {
      IconNode *result = CreateIcon(image);
      result->preserveAspect = preserveAspect;
      result->name = CopyString(temp);
      if(save) {
         InsertIcon(result);
      }
      DestroyImage(image);
      return result;
   }

   return NULL;
}

/** Read the icon property from a client. */
IconNode *ReadNetWMIcon(Window win)
{
   static const long MAX_LENGTH = 1 << 20;
   IconNode *icon = NULL;
   unsigned long count;
   int status;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned char *data;
   status = JXGetWindowProperty(display, win, atoms[ATOM_NET_WM_ICON],
                                0, MAX_LENGTH, False, XA_CARDINAL,
                                &realType, &realFormat, &count, &extra, &data);
   if(status == Success && realFormat != 0 && data) {
      icon = CreateIconFromBinary((unsigned long*)data, count);
      JXFree(data);
   }
   return icon;
}

/** Read the icon WMHint property from a client. */
IconNode *ReadWMHintIcon(Window win)
{
   IconNode *icon = NULL;
   XWMHints *hints = JXGetWMHints(display, win);
   if(hints) {
      Drawable d = None;
      Pixmap mask = None;
      if(hints->flags & IconMaskHint) {
         mask = hints->icon_mask;
      }
      if(hints->flags & IconPixmapHint) {
         d = hints->icon_pixmap;
      }
      if(d != None) {
         icon = CreateIconFromDrawable(d, mask);
      }
      JXFree(hints);
   }
   return icon;
}

/** Create an icon from XPM image data. */
IconNode *GetDefaultIcon(void)
{
   static const char * const name = "default";
   const unsigned width = 8;
   const unsigned height = 8;
   const unsigned border = 1;
   ImageNode *image;
   IconNode *result;
   unsigned bytes;
   unsigned x, y;

   /* Load the specified default, if configured. */
   if(defaultIconName) {
      result = LoadNamedIcon(defaultIconName, 1, 1);
      return result ? result : &emptyIcon;
   }

   /* Check if this icon has already been loaded */
   result = FindIcon(name);
   if(result) {
      return result;
   }

   /* Allocate image data. */
   bytes = (width * height + 7) / 8;
   image = CreateImage(width, height, 1);
   memset(image->data, 0, bytes);
#ifdef USE_XRENDER
   image->render = 0;
#endif

   /* Allocate the icon node. */
   result = CreateIcon(image);
   result->name = CopyString(name);
   result->images = image;
   InsertIcon(result);

   /* Draw the icon. */
   for(y = border; y < height - border; y++) {
      const unsigned pixel_left = y * width + border;
      const unsigned pixel_right = y * width + width - 1 - border;
      const unsigned offset_left = pixel_left / 8;
      const unsigned mask_left = 1 << (pixel_left % 8);
      const unsigned offset_right = pixel_right / 8;
      const unsigned mask_right = 1 << (pixel_right % 8);
      image->data[offset_left] |= mask_left;
      image->data[offset_right] |= mask_right;
   }
   for(x = border; x < width - border; x++) {
      const unsigned pixel_top = x + border * width;
      const unsigned pixel_bottom = x + width * (height - 1 - border);
      const unsigned offset_top = pixel_top / 8;
      const unsigned mask_top = 1 << (pixel_top % 8);
      const unsigned offset_bottom = pixel_bottom / 8;
      const unsigned mask_bottom = 1 << (pixel_bottom % 8);
      image->data[offset_top] |= mask_top;
      image->data[offset_bottom] |= mask_bottom;
   }

   return result;
}

IconNode *CreateIconFromDrawable(Drawable d, Pixmap mask)
{
   ImageNode *image;

   image = LoadImageFromDrawable(d, mask);
   if(image) {
      IconNode *result = CreateIcon(image);
      result->images = image;
      return result;
   } else {
      return NULL;
   }
}

/** Get the best image for the requested size. */
ImageNode *GetBestImage(IconNode *icon, int rwidth, int rheight)
{
   ImageNode *best;
   ImageNode *ip;

   /* If we don't have an image loaded, load one. */
   if(icon->images == NULL) {
      return LoadImage(icon->name, rwidth, rheight, icon->preserveAspect);
   }

   /* Find the best image to use.
    * Select the smallest image to completely cover the
    * requested size.  If no image completely covers the
    * requested size, select the one that overlaps the most area.
    * If no size is specified, use the largest. */
   best = icon->images;
   for(ip = icon->images->next; ip; ip = ip->next) {
      const int best_area = best->width * best->height;
      const int other_area = ip->width * ip->height;
      int best_overlap;
      int other_overlap;
      if(rwidth == 0 && rheight == 0) {
         best_overlap = 0;
         other_overlap = 0;
      } else if(rwidth == 0) {
         best_overlap = Min(best->height, rheight);
         other_overlap = Min(ip->height, rheight);
      } else if(rheight == 0) {
         best_overlap = Min(best->width, rwidth);
         other_overlap = Min(ip->width, rwidth);
      } else {
         best_overlap = Min(best->width, rwidth)
                      * Min(best->height, rheight);
         other_overlap = Min(ip->width, rwidth)
                       * Min(ip->height, rheight);
      }
      if(other_overlap > best_overlap) {
         best = ip;
      } else if(other_overlap == best_overlap) {
         if(other_area < best_area) {
            best = ip;
         }
      }
   }
   return best;
}

/** Get a scaled icon. */
ScaledIconNode *GetScaledIcon(IconNode *icon, long fg,
                              int rwidth, int rheight)
{

   XColor color;
   XImage *image;
   XPoint *points;
   ImageNode *imageNode;
   ScaledIconNode *np;
   GC maskGC;
   int x, y;
   int scalex, scaley;     /* Fixed point. */
   int srcx, srcy;         /* Fixed point. */
   int nwidth, nheight;
   unsigned char *data;
   unsigned perLine;

   if(rwidth == 0) {
      rwidth = icon->width;
   }
   if(rheight == 0) {
      rheight = icon->height;
   }

   if(icon->preserveAspect) {
      const int ratio = (icon->width << 16) / icon->height;
      nwidth = Min(rwidth, (rheight * ratio) >> 16);
      nheight = Min(rheight, (nwidth << 16) / ratio);
      nwidth = (nheight * ratio) >> 16;
   } else {
      nheight = rheight;
      nwidth = rwidth;
   }
   nwidth = Max(1, nwidth);
   nheight = Max(1, nheight);

   /* Check if this size already exists. */
   for(np = icon->nodes; np; np = np->next) {
      if(!icon->bitmap || np->fg == fg) {
#ifdef USE_XRENDER
         /* If we are using xrender and only have one image size
          * available, we can simply scale the existing icon. */
         if(icon->render) {
            if(icon->images == NULL || icon->images->next == NULL) {
               return np;
            }
         }
#endif
         if(np->width == nwidth && np->height == nheight) {
            return np;
         }
      }
   }

   /* Need to load the image. */
   imageNode = GetBestImage(icon, nwidth, nheight);
   if(JUNLIKELY(!imageNode)) {
      return NULL;
   }

   /* See if we can use XRender to create the icon. */
#ifdef USE_XRENDER
   if(icon->render) {
      np = CreateScaledRenderIcon(imageNode, fg);
      np->next = icon->nodes;
      icon->nodes = np;

      /* Don't keep the image data around after creating the icon. */
      if(icon->images == NULL) {
         DestroyImage(imageNode);
      }

      return np;
   }
#endif

   /* Create a new ScaledIconNode the old-fashioned way. */
   np = Allocate(sizeof(ScaledIconNode));
   np->fg = fg;
   np->width = nwidth;
   np->height = nheight;
   np->next = icon->nodes;
   icon->nodes = np;

   /* Create a mask. */
   np->mask = JXCreatePixmap(display, rootWindow, nwidth, nheight, 1);
   maskGC = JXCreateGC(display, np->mask, 0, NULL);
   JXSetForeground(display, maskGC, 0);
   JXFillRectangle(display, np->mask, maskGC, 0, 0, nwidth, nheight);
   JXSetForeground(display, maskGC, 1);

   /* Create a temporary XImage for scaling. */
   image = JXCreateImage(display, rootVisual, rootDepth,
                         ZPixmap, 0, NULL, nwidth, nheight, 8, 0);
   image->data = Allocate(sizeof(unsigned long) * nwidth * nheight);

   /* Determine the scale factor. */
   scalex = (imageNode->width << 16) / nwidth;
   scaley = (imageNode->height << 16) / nheight;

   points = Allocate(sizeof(XPoint) * nwidth);
   data = imageNode->data;
   if(imageNode->bitmap) {
      perLine = (imageNode->width >> 3) + ((imageNode->width & 7) ? 1 : 0);
   } else {
      perLine = imageNode->width;
   }
   srcy = 0;
   for(y = 0; y < nheight; y++) {
      const int yindex = (srcy >> 16) * perLine;
      int pindex = 0;
      srcx = 0;
      for(x = 0; x < nwidth; x++) {
         if(imageNode->bitmap) {
            const int tx = srcx >> 16;
            const int offset = yindex + (tx >> 3);
            const int mask = 1 << (tx & 7);
            if(data[offset] & mask) {
               points[pindex].x = x;
               points[pindex].y = y;
               XPutPixel(image, x, y, fg);
               pindex += 1;
            }
         } else {
            const int yindex = (srcy >> 16) * imageNode->width;
            const int index = 4 * (yindex + (srcx >> 16));
            color.red = data[index + 1];
            color.red |= color.red << 8;
            color.green = data[index + 2];
            color.green |= color.green << 8;
            color.blue = data[index + 3];
            color.blue |= color.blue << 8;
            GetColor(&color);
            XPutPixel(image, x, y, color.pixel);
            if(data[index] >= 128) {
               points[pindex].x = x;
               points[pindex].y = y;
               pindex += 1;
            }
         }
         srcx += scalex;
      }
      JXDrawPoints(display, np->mask, maskGC, points, pindex, CoordModeOrigin);
      srcy += scaley;
   }
   Release(points);

   /* Release the mask GC. */
   JXFreeGC(display, maskGC);
 
   /* Create the color data pixmap. */
   np->image = JXCreatePixmap(display, rootWindow, nwidth, nheight,
                              rootDepth);

   /* Render the image to the color data pixmap. */
   JXPutImage(display, np->image, rootGC, image, 0, 0, 0, 0, nwidth, nheight);   
   /* Release the XImage. */
   Release(image->data);
   image->data = NULL;
   JXDestroyImage(image);

   if(icon->images == NULL) {
      DestroyImage(imageNode);
   }

   return np;

}

/** Create an icon from binary data (as specified via window properties). */
IconNode *CreateIconFromBinary(const unsigned long *input,
                               unsigned int length)
{
   IconNode *result = NULL;
   unsigned int offset = 0;

   if(!input) {
      return NULL;
   }

   while(offset < length) {

      const unsigned width = input[offset + 0];
      const unsigned height = input[offset + 1];
      unsigned char *data;
      ImageNode *image;
      unsigned x;

      if(JUNLIKELY(width * height + 2 > length - offset)) {
         Debug("invalid image size: %d x %d + 2 > %d",
               width, height, length - offset);
         return result;
      } else if(JUNLIKELY(width == 0 || height == 0)) {
         Debug("invalid image size: %d x %d", width, height);
         return result;
      }

      image = CreateImage(width, height, 0);
      if(result == NULL) {
         result = CreateIcon(image);
      }
      image->next = result->images;
      result->images = image;
      data = image->data;

      /* Note: the data types here might be of different sizes. */
      offset += 2;
      for(x = 0; x < width * height; x++) {
         *data++ = (input[offset] >> 24) & 0xFF;
         *data++ = (input[offset] >> 16) & 0xFF;
         *data++ = (input[offset] >>  8) & 0xFF;
         *data++ = (input[offset] >>  0) & 0xFF;
         offset += 1;
      }

      /* Don't insert this icon into the hash since it is transient. */

   }

   return result;
}

/** Create an empty icon node. */
IconNode *CreateIcon(const ImageNode *image)
{
   IconNode *icon;
   icon = Allocate(sizeof(IconNode));
   icon->nodes = NULL;
   icon->name = NULL;
   icon->images = NULL;
   icon->next = NULL;
   icon->prev = NULL;
   icon->width = image->width;
   icon->height = image->height;
   icon->bitmap = image->bitmap;
#ifdef USE_XRENDER
   icon->render = image->render;
#endif
   icon->preserveAspect = 1;
   icon->transient = 1;
   return icon;
}

/** Helper method for destroy icons. */
void DoDestroyIcon(int index, IconNode *icon)
{
   if(icon && icon != &emptyIcon) {
      while(icon->nodes) {
         ScaledIconNode *np = icon->nodes;
#ifdef USE_XRENDER
         if(icon->render) {
            if(np->image != None) {
               JXRenderFreePicture(display, np->image);
            }
            if(np->mask != None) {
               JXRenderFreePicture(display, np->mask);
            }
#else
         if(0) {
#endif
         } else {
            if(np->image != None) {
               JXFreePixmap(display, np->image);
            }
            if(np->mask != None) {
               JXFreePixmap(display, np->mask);
            }
         }
         icon->nodes = np->next;
         Release(np);
      }
      DestroyImage(icon->images);
      if(icon->name) {
         Release(icon->name);
      }

      if(icon->prev) {
         icon->prev->next = icon->next;
      } else {
         iconHash[index] = icon->next;
      }
      if(icon->next) {
         icon->next->prev = icon->prev;
      }
      Release(icon);
   }
}

/** Destroy an icon. */
void DestroyIcon(IconNode *icon)
{
   if(icon && icon->transient) {
      const unsigned int index = GetHash(icon->name);
      DoDestroyIcon(index, icon);
   }
}

/** Insert an icon to the icon hash table. */
void InsertIcon(IconNode *icon)
{
   unsigned int index;
   Assert(icon);
   Assert(icon->name);
   index = GetHash(icon->name);
   icon->prev = NULL;
   if(iconHash[index]) {
      iconHash[index]->prev = icon;
   }
   icon->next = iconHash[index];
   icon->transient = 0;
   iconHash[index] = icon;
}

/** Find a icon in the icon hash table. */
IconNode *FindIcon(const char *name)
{
   const unsigned int index = GetHash(name);
   IconNode *icon = iconHash[index];
   while(icon) {
      if(!strcmp(icon->name, name)) {
         return icon;
      }
      icon = icon->next;
   }

   return NULL;
}

/** Get the hash for a string. */
unsigned int GetHash(const char *str)
{
   unsigned int hash = 0;
   if(str) {
      unsigned int x;
      for(x = 0; str[x]; x++) {
         hash = (hash + (hash << 5)) ^ (unsigned int)str[x];
      }
      hash &= (HASH_SIZE - 1);
   }
   return hash;
}

/** Set the name of the default icon. */
void SetDefaultIcon(const char *name)
{
   if(defaultIconName) {
      Release(defaultIconName);
   }
   defaultIconName = CopyString(name);
}

#endif /* USE_ICONS */
