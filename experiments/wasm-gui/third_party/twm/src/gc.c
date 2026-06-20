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
/*       Copyright 1988 by Evans & Sutherland Computer Corporation,        */
/*                          Salt Lake City, Utah                           */
/*                        Cambridge, Massachusetts                         */
/*                                                                         */
/*                           All Rights Reserved                           */
/*                                                                         */
/*    Permission to use, copy, modify, and distribute this software and    */
/*    its documentation  for  any  purpose  and  without  fee is hereby    */
/*    granted, provided that the above copyright notice appear  in  all    */
/*    copies and that both  that  copyright  notice  and  this  permis-    */
/*    sion  notice appear in supporting  documentation,  and  that  the    */
/*    name of Evans & Sutherland not be used in advertising                */
/*    in publicity pertaining to distribution of the  software  without    */
/*    specific, written prior permission.                                  */
/*                                                                         */
/*    EVANS & SUTHERLAND DISCLAIMs ALL WARRANTIES WITH REGARD              */
/*    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    */
/*    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND       */
/*    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-           */
/*    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    */
/*    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    */
/*    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    */
/*    OR PERFORMANCE OF THIS SOFTWARE.                                     */
/*********************************************************************(*****/


/**********************************************************************
 *
 * Open the fonts and create the GCs
 *
 * 31-Mar-88 Tom LaStrange        Initial Version.
 *
 **********************************************************************/

#include <stdio.h>
#include "twm.h"
#include "util.h"
#include "screen.h"
#include "gc.h"

/** \fn CreateGCs
 * open fonts and create all the needed GC's.  I only
 *		    want to do this once, hence the first_time flag.
 */
void
CreateGCs(void)
{
    static ScreenInfo *prevScr = NULL;
    XGCValues	    gcv;
    unsigned long   gcm;

    if (!Scr->FirstTime || prevScr == Scr)
	return;

    prevScr = Scr;

    /* create GC's */

    gcm = 0;
    gcm |= GCFunction;	    gcv.function = GXxor;
    gcm |= GCLineWidth;	    gcv.line_width = 0;
    gcm |= GCForeground;    gcv.foreground = Scr->XORvalue;
    gcm |= GCSubwindowMode; gcv.subwindow_mode = IncludeInferiors;

    Scr->DrawGC = XCreateGC(dpy, Scr->Root, gcm, &gcv);

    gcm = 0;
    gcm |= GCForeground;    gcv.foreground = Scr->MenuC.fore;
    gcm |= GCBackground;    gcv.background = Scr->MenuC.back;
    if (!use_fontset)
	{gcm |= GCFont;	    gcv.font =  Scr->MenuFont.font->fid;}

    Scr->MenuGC = XCreateGC(dpy, Scr->Root, gcm, &gcv);

    gcm = 0;
    gcm |= GCPlaneMask;	    gcv.plane_mask = AllPlanes;
    /*
     * Prevent GraphicsExpose and NoExpose events.  We'd only get NoExpose
     * events anyway;  they cause BadWindow errors from XGetWindowAttributes
     * call in FindScreenInfo (events.c) (since drawable is a pixmap).
     */
    gcm |= GCGraphicsExposures;  gcv.graphics_exposures = False;
    gcm |= GCLineWidth;	    gcv.line_width = 0;

    Scr->NormalGC = XCreateGC(dpy, Scr->Root, gcm, &gcv);
}
