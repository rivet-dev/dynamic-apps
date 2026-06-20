/*****************************************************************************/
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
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    name of Evans & Sutherland not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND DISCLAIMs ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND    **/
/**    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/

/***********************************************************************
 *
 * utility routines header file
 *
 * 28-Oct-87 Thomas E. LaStrange                File created
 *
 ***********************************************************************/

#ifndef _UTIL_
#define _UTIL_

#include "twm.h"

extern void MoveOutline(Window root, int x, int y, int width, int height,
                        int bw, int th);
extern void Zoom(Window wf, Window wt);
extern char *ExpandFilename(const char *name);
extern void GetUnknownIcon(const char *name);
extern Pixmap FindBitmap(const char *name, unsigned int *widthp,
                         unsigned int *heightp);
extern Pixmap GetBitmap(const char *name);
extern void InsertRGBColormap(Atom a, XStandardColormap *maps, int nmaps,
                              Bool replace);
extern void RemoveRGBColormap(Atom a);
extern void LocateStandardColormaps(void);
extern void GetColor(int kind, Pixel *what, const char *name);
extern void GetColorValue(int kind, XColor *what, const char *name);
extern void GetFont(MyFont *font);
extern int MyFont_TextWidth(MyFont *font, const char *string, int len);
extern void MyFont_DrawImageString(Display *dpy, Drawable d, MyFont *font,
                                   GC gc, int x, int y, const char *string,
                                   int len);
extern void MyFont_DrawString(Display *dpy, Drawable d, MyFont *font, GC gc,
                              int x, int y, const char *string, int len);
extern void MyFont_ChangeGC(unsigned long fix_fore, unsigned long fix_back,
                            MyFont *fix_font);
extern Status I18N_FetchName(Display *dpy, Window win, char **winname);
extern Status I18N_GetIconName(Display *dpy, Window win, char **iconname);
extern void SetFocus(TwmWindow *tmp_win, Time time);
extern void Bell(int type, int percent, Window win);

extern int HotX, HotY;

#define WM_BELL                 0
#define MINOR_ERROR_BELL        1
#define MAJOR_ERROR_BELL        2
#define INFO_BELL               3
#define NUM_BELLS               4

#define QUIET_BELL              -100
#define MODERATE_BELL           0
#define LOUD_BELL               100

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#else
#define XkbBI_Info              0
#define XkbBI_MinorError        1
#define XkbBI_MajorError        2
#endif

#endif                          /* _UTIL_ */
