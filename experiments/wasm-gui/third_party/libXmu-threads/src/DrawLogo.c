/*

Copyright 1988, 1998  The Open Group

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xmu/Drawing.h>

/*
 *  Draw the "official" X Window System Logo, designed by Danny Chong
 *
 *  Written by Ollie Jones, Apollo Computer
 *
 *  Does some fancy stuff to make the logo look acceptable even
 *  if it is tiny.  Also makes the various linear elements of
 *  the logo line up as well as possible considering rasterization.
 */
void
XmuDrawLogo(Display *dpy, Drawable drawable, GC gcFore, GC gcBack,
	    int x, int y, unsigned int width, unsigned int height)
{
    unsigned int size;
    int thin, gap, d31;
    XPoint poly[4];

    XFillRectangle(dpy, drawable, gcBack, x, y, width, height);

    /* for now, do a centered even-sized square, at least for now */
    size = width;
    if (height < width)
	 size = height;
    size &= ~1;
    x += (width - size) >> 1;
    y += (height - size) >> 1;

/*
 * Draw what will be the thin strokes.
 *
 *           -----
 *          /    /
 *         /    /
 *        /    /
 *       /    /
 *      /____/
 *           d
 *
 * Point d is 9/44 (~1/5) of the way across.
 */

    thin = (size / 11);
    if (thin < 1) thin = 1;
    gap = (thin+3) / 4;
    d31 = thin + thin + gap;
    poly[0].x = x + size;              poly[0].y = y;
    poly[1].x = x + size-d31;          poly[1].y = y;
    poly[2].x = x + 0;                 poly[2].y = y + size;
    poly[3].x = x + d31;               poly[3].y = y + size;
    XFillPolygon(dpy, drawable, gcFore, poly, 4, Convex, CoordModeOrigin);

/*
 * Erase area not needed for lower thin stroke.
 *
 *           ------
 *          /     /
 *         /  __ /
 *        /  /  /
 *       /  /  /
 *      /__/__/
 */

    poly[0].x = x + d31/2;                       poly[0].y = y + size;
    poly[1].x = x + size / 2;                    poly[1].y = y + size/2;
    poly[2].x = x + (size/2)+(d31-(d31/2));      poly[2].y = y + size/2;
    poly[3].x = x + d31;                         poly[3].y = y + size;
    XFillPolygon(dpy, drawable, gcBack, poly, 4, Convex, CoordModeOrigin);

/*
 * Erase area not needed for upper thin stroke.
 *
 *           ------
 *          /  /  /
 *         /--/  /
 *        /     /
 *       /     /
 *      /_____/
 */

    poly[0].x = x + size - d31/2;                poly[0].y = y;
    poly[1].x = x + size / 2;                    poly[1].y = y + size/2;
    poly[2].x = x + (size/2)-(d31-(d31/2));      poly[2].y = y + size/2;
    poly[3].x = x + size - d31;                  poly[3].y = y;
    XFillPolygon(dpy, drawable, gcBack, poly, 4, Convex, CoordModeOrigin);

/*
 * Draw thick stroke.
 * Point b is 1/4 of the way across.
 *
 *      b
 * -----
 * \    \
 *  \    \
 *   \    \
 *    \    \
 *     \____\
 */

    poly[0].x = x;                     poly[0].y = y;
    poly[1].x = x + size/4;            poly[1].y = y;
    poly[2].x = x + size;              poly[2].y = y + size;
    poly[3].x = x + size - size/4;     poly[3].y = y + size;
    XFillPolygon(dpy, drawable, gcFore, poly, 4, Convex, CoordModeOrigin);

/*
 * Erase to create gap.
 *
 *          /
 *         /
 *        /
 *       /
 *      /
 */

    poly[0].x = x + size- thin;        poly[0].y = y;
    poly[1].x = x + size-( thin+gap);  poly[1].y = y;
    poly[2].x = x + thin;              poly[2].y = y + size;
    poly[3].x = x + thin + gap;        poly[3].y = y + size;
    XFillPolygon(dpy, drawable, gcBack, poly, 4, Convex, CoordModeOrigin);
}
