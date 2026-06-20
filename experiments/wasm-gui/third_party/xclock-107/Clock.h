/*
* $Xorg: Clock.h,v 1.4 2001/02/09 02:05:39 xorgcvs Exp $
*/


/***********************************************************

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
/* $XFree86: xc/programs/xclock/Clock.h,v 1.11 2002/10/17 01:00:01 dawes Exp $ */

#ifndef _XawClock_h
#define _XawClock_h

/***********************************************************************
 *
 * Clock Widget
 *
 ***********************************************************************/

#include <X11/Xmu/Converters.h>

/* Parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 twentyfour	     Boolean		Boolean		True
 analog		     Boolean		Boolean		True
 background	     Background		Pixel		white
 backingStore	     BackingStore	BackingStore	default
 border		     BorderColor	Pixel		Black
 borderWidth	     BorderWidth	Dimension	1
 chime		     Boolean		Boolean		False
 destroyCallback     Callback		Pointer		NULL
 font		     Font		XFontStruct*	fixed
 foreground	     Foreground		Pixel		black
 hand		     Foreground		Pixel		black
 height		     Height		Dimension	164
 highlight	     Foreground		Pixel		black
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 padding	     Margin		int		8
 utime		     Boolean		Boolean		False
 update		     Interval		int		60 (seconds)
 width		     Width		Dimension	164
 x		     Position		Position	0
 y		     Position		Position	0

*/

/* Resource names used to the clock widget */

		/* color of hands */
#define XtNhand "hands"


		/* Boolean: 24-hour if TRUE */
#define XtNtwentyfour "twentyfour"

		/* Boolean: digital if FALSE */
#define XtNanalog "analog"

		/* Boolean: only hour/minute if TRUE */
#define XtNbrief  "brief"

                /* String: will be used as format arg to
                   "strftime" if not empty string */
#define XtNstrftime "strftime"

		/* Boolean: show seconds since Epoch if TRUE */
#define XtNutime  "utime"

		/* Boolean:  */
#define XtNchime "chime"

		/* Int: amount of space around outside of clock */
#define XtNpadding "padding"

		/* Boolean: use Render extension if TRUE */
#define XtNrender "render"

		/* Boolean: use backing pixmap for double buffering */
#define XtNbuffer "buffer"

		/* RenderColor: colors for various clock elements */
#define XtNhourColor "hourColor"
#define XtNminuteColor "minuteColor"
#define XtNsecondColor "secondColor"
#define XtNmajorColor "majorColor"
#define XtNminorColor "minorColor"

#define XtRXftColor "XftColor"

#define XtNface "face"
#define XtCFace "Face"
#define XtRXftFont "XftFont"

		/* Boolean: use sharp rendering for Render polygons */
#define XtNsharp "sharp"
#define XtCSharp "Sharp"

typedef struct _ClockRec *ClockWidget;  /* completely defined in ClockP.h */
typedef struct _ClockClassRec *ClockWidgetClass;  /* completely defined in ClockP.h */

extern WidgetClass clockWidgetClass;

#endif /* _XawClock_h */
/* DON'T ADD STUFF AFTER THIS #endif */
