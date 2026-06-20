/*

Copyright 1987, 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

/***********************************************************

Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xmu/Drawing.h>

typedef struct _PixmapCache {
    Screen *screen;
    Pixmap pixmap;
    Pixel foreground, background;
    unsigned int depth;
    int ref_count;
    struct _PixmapCache *next;
  } CacheEntry;

static CacheEntry *pixmapCache = NULL;



Pixmap
XmuCreateStippledPixmap(Screen *screen, Pixel fore, Pixel back,
			unsigned int depth)
/*
 *	Creates a stippled pixmap of specified depth
 *	caches these so that multiple requests share the pixmap
 */
{
    register Display *display = DisplayOfScreen(screen);
    CacheEntry *cachePtr;
    Pixmap stippled_pixmap;
    static unsigned char pixmap_bits[] = {
	0x02, 0x01,
    };

/*
 *	Creates a stippled pixmap of depth DefaultDepth(screen)
 *	caches these so that multiple requests share the pixmap
 */

#define pixmap_width 2
#define pixmap_height 2

    /* see if we already have a pixmap suitable for this screen */
    for (cachePtr = pixmapCache; cachePtr; cachePtr = cachePtr->next) {
	if (cachePtr->screen == screen && cachePtr->foreground == fore &&
	    cachePtr->background == back && cachePtr->depth == depth)
	    return( cachePtr->ref_count++, cachePtr->pixmap );
    }

    stippled_pixmap = XCreatePixmapFromBitmapData (display,
			RootWindowOfScreen(screen), (char *)pixmap_bits,
			pixmap_width, pixmap_height, fore, back, depth);

    /* and insert it at the head of the cache */
    cachePtr = XtNew(CacheEntry);
    cachePtr->screen = screen;
    cachePtr->foreground = fore;
    cachePtr->background = back;
    cachePtr->depth = depth;
    cachePtr->pixmap = stippled_pixmap;
    cachePtr->ref_count = 1;
    cachePtr->next = pixmapCache;
    pixmapCache = cachePtr;

    return( stippled_pixmap );
}

void
XmuReleaseStippledPixmap(Screen *screen, Pixmap pixmap)
{
    register Display *display = DisplayOfScreen(screen);
    CacheEntry *cachePtr, **prevP;
    for (prevP = &pixmapCache, cachePtr = pixmapCache; cachePtr;) {
	if (cachePtr->screen == screen && cachePtr->pixmap == pixmap) {
	    if (--cachePtr->ref_count == 0) {
		XFreePixmap( display, pixmap );
		*prevP = cachePtr->next;
		XtFree( (char*)cachePtr );
		break;
	    }
	}
	prevP = &cachePtr->next;
	cachePtr = *prevP;
    }
}
