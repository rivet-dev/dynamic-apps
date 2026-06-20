/*

Copyright 1989, 1998  The Open Group

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
#include <X11/Xatom.h>

#include <X11/Xmu/WinUtil.h>

/*
 * Prototypes
 */
static Window TryChildren(Display*, Window, Atom);

/* Find a window with WM_STATE, else return win itself, as per ICCCM */

Window
XmuClientWindow(Display *dpy, Window win)
{
    Atom WM_STATE;
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    Window inf;

    WM_STATE = XInternAtom(dpy, "WM_STATE", True);
    if (!WM_STATE)
	return win;
    XGetWindowProperty(dpy, win, WM_STATE, 0, 0, False, AnyPropertyType,
		       &type, &format, &nitems, &after, &data);
    if (data)
	XFree(data);
    if (type)
	return win;
    inf = TryChildren(dpy, win, WM_STATE);
    if (!inf)
	inf = win;
    return inf;
}

static Window
TryChildren(Display *dpy, Window win, Atom WM_STATE)
{
    Window root, parent;
    Window *children;
    unsigned int nchildren;
    unsigned int i;
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data;
    Window inf = 0;

    if (!XQueryTree(dpy, win, &root, &parent, &children, &nchildren))
	return 0;
    for (i = 0; !inf && (i < nchildren); i++) {
	data = NULL;
	XGetWindowProperty(dpy, children[i], WM_STATE, 0, 0, False,
			   AnyPropertyType, &type, &format, &nitems,
			   &after, &data);
	if (data)
	    XFree(data);
	if (type)
	    inf = children[i];
    }
    for (i = 0; !inf && (i < nchildren); i++)
	inf = TryChildren(dpy, children[i], WM_STATE);
    if (children)
	XFree(children);
    return inf;
}
