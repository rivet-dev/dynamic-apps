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
#include	<X11/Intrinsic.h>
#include	<X11/StringDefs.h>
#include	<X11/Xmu/Converters.h>
#include	<X11/Xmu/Drawing.h>


/*
 * XmuConvertStringToBitmap:
 *
 * creates a depth-1 Pixmap suitable for window manager icons.
 * "string" represents a bitmap(1) filename which may be absolute,
 * or relative to the global resource bitmapFilePath, class
 * BitmapFilePath.  If the resource is not defined, the default
 * value is the build symbol BITMAPDIR.
 *
 * shares lots of code with XmuConvertStringToCursor.
 *
 * To use, include the following in your ClassInitialize procedure:

static XtConvertArgRec screenConvertArg[] = {
    {XtBaseOffset, (XtPointer) XtOffset(Widget, core.screen), sizeof(Screen *)}
};

    XtAddConverter("String", "Bitmap", XmuCvtStringToBitmap,
		   screenConvertArg, XtNumber(screenConvertArg));
 *
 */

#define	done(address, type) \
	{ (*toVal).size = sizeof(type); (*toVal).addr = (XPointer) address; }


/*ARGSUSED*/
void
XmuCvtStringToBitmap(XrmValuePtr args, Cardinal *num_args,
		     XrmValuePtr fromVal, XrmValuePtr toVal)
{
    static Pixmap pixmap;		/* static for cvt magic */
    char *name = (char *)fromVal->addr;
    Screen *screen;
    Display *dpy;
    XrmDatabase db;
    String fn;
    unsigned int width, height;
    int xhot, yhot;
    unsigned char *data;

    if (*num_args != 1)
     XtErrorMsg("wrongParameters","cvtStringToBitmap","XtToolkitError",
             "String to pixmap conversion needs screen argument",
              (String *)NULL, (Cardinal *)NULL);

    if (strcmp(name, "None") == 0) {
	pixmap = None;
	done(&pixmap, Pixmap);
	return;
    }

    if (strcmp(name, "ParentRelative") == 0) {
	pixmap = ParentRelative;
	done(&pixmap, Pixmap);
	return;
    }

    screen = *((Screen **) args[0].addr);
    pixmap = XmuLocateBitmapFile (screen, name,
				  NULL, 0, NULL, NULL, NULL, NULL);
    if (pixmap == None) {
	dpy = DisplayOfScreen(screen);
	db = XrmGetDatabase(dpy);
	XrmSetDatabase(dpy, XtScreenDatabase(screen));
	fn = XtResolvePathname(dpy, "bitmaps", name, "", NULL, NULL, 0, NULL);
	if (!fn)
	    fn = XtResolvePathname(dpy, "", name, ".xbm", NULL, NULL, 0, NULL);
	XrmSetDatabase(dpy, db);
	if (fn &&
	    XmuReadBitmapDataFromFile (fn, &width, &height, &data,
				       &xhot, &yhot) == BitmapSuccess) {
	    pixmap = XCreatePixmapFromBitmapData (dpy,
						  RootWindowOfScreen(screen),
						  (char *) data, width, height,
						  1, 0, 1);
	    XFree ((char *)data);
	}
    }

    if (pixmap != None) {
	done (&pixmap, Pixmap);
    } else {
	XtStringConversionWarning (name, "Pixmap");
	return;
    }
}

