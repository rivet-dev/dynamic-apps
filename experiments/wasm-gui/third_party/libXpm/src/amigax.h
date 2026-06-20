/*
 * Copyright (C) 1996 Lorens Younes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * Lorens Younes BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Lorens Younes shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Lorens Younes.
 */

/*****************************************************************************\
* amigax.h:                                                                   *
*                                                                             *
*  XPM library                                                                *
*  Emulates some Xlib functionality for Amiga.                                *
*                                                                             *
*  Developed by Lorens Younes (d93-hyo@nada.kth.se) 7/95                      *
*  Revised 4/96                                                               *
\*****************************************************************************/

#ifndef AMIGA_X
#define AMIGA_X


#include <intuition/screens.h>

#include <proto/exec.h>
#include <proto/graphics.h>


#define Success   0

/* really never used */
#define ZPixmap   2

#define Bool     int
#define Status   int
#define True     1
#define False    0

typedef struct ColorMap  *Colormap;

typedef void  *Visual;

typedef struct {
    int               width, height;
    struct RastPort  *rp;
}   XImage;

typedef struct {
    unsigned long    pixel;
    unsigned short   red, green, blue;
}   XColor;

typedef struct Screen   Display;


#define XGrabServer(dpy)     (Forbid ())
#define XUngrabServer(dpy)   (Permit ())

#define XDefaultScreen(dpy)          (0)
#define XDefaultVisual(dpy, scr)     (NULL)
#define XDefaultColormap(dpy, scr)   (dpy->ViewPort.ColorMap)
#define XDefaultDepth(dpy, scr)      (dpy->RastPort.BitMap->Depth)

#define XCreateImage(dpy, vi, depth, format, offset, data, width, height, pad, bpl) \
	(AllocXImage (width, height, depth))
#define XDestroyImage(img)   (FreeXImage (img))

#define XAllocColor(dpy, cm, xc) \
	(AllocBestPen (cm, xc, PRECISION_EXACT, True))
#define XFreeColors(dpy, cm, pixels, npixels, planes) \
	(FreePens (cm, pixels, npixels))
#define XParseColor(dpy, cm, spec, exact_def_return) \
	(ParseColor (spec, exact_def_return))
#define XQueryColor(dpy, cm, def_in_out) \
	(QueryColor(cm, def_in_out))
#define XQueryColors(dpy, cm, defs_in_out, ncolors) \
	(QueryColors(cm, defs_in_out, ncolors))


XImage *
AllocXImage (
    unsigned int   width,
    unsigned int   height,
    unsigned int   depth);


int
FreeXImage (
    XImage  *ximage);


int
XPutPixel (
    XImage         *ximage,
    int             x,
    int             y,
    unsigned long   pixel);


Status
AllocBestPen (
    Colormap        colormap,
    XColor         *screen_in_out,
    unsigned long   precision,
    Bool            fail_if_bad);


int
FreePens (
    Colormap        colormap,
    unsigned long  *pixels,
    int             npixels);


Status
ParseColor (
    char      *spec,
    XColor    *exact_def_return);


int
QueryColor (
    Colormap   colormap,
    XColor    *def_in_out);


int
QueryColors (
    Colormap   colormap,
    XColor    *defs_in_out,
    int        ncolors);


#endif /* AMIGA_X */
