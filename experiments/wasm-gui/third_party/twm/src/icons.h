/*
 *
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
 * */

/**********************************************************************
 *
 * Icon releated definitions
 *
 * 10-Apr-89 Tom LaStrange        Initial Version.
 *
 **********************************************************************/

#ifndef ICONS_H
#define ICONS_H

#include "twm.h"

typedef struct IconRegion {
    struct IconRegion *next;
    int x, y, w, h;
    int grav1, grav2;
    int stepx, stepy;           /* allocation granularity */
    struct IconEntry *entries;
} IconRegion;

typedef struct IconEntry {
    struct IconEntry *next;
    int x, y, w, h;
    TwmWindow *twm_win;
    short used;
} IconEntry;

extern void IconUp(TwmWindow *tmp_win);
extern void IconDown(TwmWindow *tmp_win);
extern void AddIconRegion(char *geom, int grav1, int grav2,
                          int stepx, int stepy);
extern void CreateIconWindow(TwmWindow *tmp_win, int def_x, int def_y);

#endif                          /* ICONS_H */
