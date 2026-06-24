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

/*
 * XmuDrawRoundedRectangle, XmuFillRoundedRectangle
 *
 * Draw/Fill a rounded rectangle, where x, y, w, h are the dimensions of
 * the overall rectangle, and ew and eh are the sizes of a bounding box
 * that the corners are drawn inside of.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xmu/Drawing.h>

void
XmuDrawRoundedRectangle(Display *dpy, Drawable draw, GC gc,
			int x, int y, int w, int h, int ew, int eh)
{
	XArc	arcs[8];
  int ew2, eh2;

  if ((ew2 = (ew << 1)) > w)
    ew2 = ew = 0;
  if ((eh2 = (eh << 1)) > h)
    eh2 = eh = 0;

	arcs[0].x = x;
	arcs[0].y = y;
  arcs[0].width = ew2;
  arcs[0].height = eh2;
  arcs[0].angle1 = 180 * 64;
  arcs[0].angle2 = -90 * 64;

	arcs[1].x = x + ew;
	arcs[1].y = y;
  arcs[1].width = w - ew2;
	arcs[1].height = 0;
  arcs[1].angle1 = 180 * 64;
  arcs[1].angle2 = -180 * 64;

  arcs[2].x = x + w - ew2;
	arcs[2].y = y;
  arcs[2].width = ew2;
  arcs[2].height = eh2;
  arcs[2].angle1 = 90 * 64;
  arcs[2].angle2 = -90 * 64;

	arcs[3].x = x + w;
	arcs[3].y = y + eh;
	arcs[3].width = 0;
  arcs[3].height = h - eh2;
	arcs[3].angle1 = 90 * 64;
  arcs[3].angle2 = -180 * 64;

  arcs[4].x = x + w - ew2;
  arcs[4].y = y + h - eh2;
  arcs[4].width = ew2;
  arcs[4].height = eh2;
	arcs[4].angle1 = 0;
  arcs[4].angle2 = -90 * 64;

	arcs[5].x = x + ew;
	arcs[5].y = y + h;
  arcs[5].width = w - ew2;
	arcs[5].height = 0;
	arcs[5].angle1 = 0;
  arcs[5].angle2 = -180 * 64;

	arcs[6].x = x;
  arcs[6].y = y + h - eh2;
  arcs[6].width = ew2;
  arcs[6].height = eh2;
  arcs[6].angle1 = 270 * 64;
  arcs[6].angle2 = -90 * 64;

	arcs[7].x = x;
	arcs[7].y = y + eh;
	arcs[7].width = 0;
  arcs[7].height = h - eh2;
  arcs[7].angle1 = 270 * 64;
  arcs[7].angle2 = -180 * 64;

  XDrawArcs(dpy, draw, gc, arcs, 8);
}

void
XmuFillRoundedRectangle(Display *dpy, Drawable draw, GC gc,
			int x, int y, int w, int h, int ew, int eh)
{
	XArc	arcs[4];
	XRectangle rects[3];
	XGCValues vals;
  int ew2, eh2;

	XGetGCValues(dpy, gc, GCArcMode, &vals);
	if (vals.arc_mode != ArcPieSlice)
	    XSetArcMode(dpy, gc, ArcPieSlice);

  if ((ew2 = (ew << 1)) > w)
    ew2 = ew = 0;
  if ((eh2 = (eh << 1)) > h)
    eh2 = eh = 0;

	arcs[0].x = x;
	arcs[0].y = y;
  arcs[0].width = ew2;
  arcs[0].height = eh2;
  arcs[0].angle1 = 180 * 64;
  arcs[0].angle2 = -90 * 64;

  arcs[1].x = x + w - ew2 - 1;
	arcs[1].y = y;
  arcs[1].width = ew2;
  arcs[1].height = eh2;
  arcs[1].angle1 = 90 * 64;
  arcs[1].angle2 = -90 * 64;

  arcs[2].x = x + w - ew2 - 1;
  arcs[2].y = y + h - eh2 - 1;
  arcs[2].width = ew2;
  arcs[2].height = eh2;
	arcs[2].angle1 = 0;
  arcs[2].angle2 = -90 * 64;

	arcs[3].x = x;
  arcs[3].y = y + h - eh2 - 1;
  arcs[3].width = ew2;
  arcs[3].height = eh2;
  arcs[3].angle1 = 270 * 64;
  arcs[3].angle2 = -90 * 64;

  XFillArcs(dpy, draw, gc, arcs, 4);

	rects[0].x = x + ew;
	rects[0].y = y;
  rects[0].width = w - ew2;
	rects[0].height = h;

	rects[1].x = x;
	rects[1].y = y + eh;
	rects[1].width = ew;
  rects[1].height = h - eh2;

	rects[2].x = x + w - ew;
	rects[2].y = y + eh;
	rects[2].width = ew;
  rects[2].height = h - eh2;

  XFillRectangles(dpy, draw, gc, rects, 3);

	if (vals.arc_mode != ArcPieSlice)
	    XSetArcMode(dpy, gc, vals.arc_mode);
}
