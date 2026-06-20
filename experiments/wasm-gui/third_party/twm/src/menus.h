/*****************************************************************************/
/*

Copyright 1989,1998  The Open Group

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
 * twm menus include file
 *
 * 17-Nov-87 Thomas E. LaStrange                File created
 *
 ***********************************************************************/

#ifndef _MENUS_
#define _MENUS_

#include "twm.h"

#define TWM_ROOT        "bLoB_GoOp"     /* my private root menu */
#define TWM_WINDOWS     "TwmWindows"    /* for f.menu "TwmWindows" */

#define MAX_FILE_SIZE 4096      /* max chars to read from file for cut */

typedef struct MenuItem {
    struct MenuItem *next;      /* next menu item */
    struct MenuItem *prev;      /* prev menu item */
    struct MenuRoot *sub;       /* MenuRoot of a pull right menu */
    struct MenuRoot *root;      /* back pointer to my MenuRoot */
    const char *item;           /* the character string displayed */
    const char *action;         /* action to be performed */
    Pixel fore;                 /* foreground color */
    Pixel back;                 /* background color */
    Pixel hi_fore;              /* highlight foreground */
    Pixel hi_back;              /* highlight background */
    short item_num;             /* item number of this menu */
    short x;                    /* x coordinate for text */
    short func;                 /* twm built in function */
    short state;                /* video state, 0 = normal, 1 = reversed */
    short strlen;               /* strlen(item) */
    short user_colors;          /* colors were specified */
} MenuItem;

typedef struct MenuRoot {
    struct MenuItem *first;     /* first item in menu */
    struct MenuItem *last;      /* last item in menu */
    struct MenuRoot *prev;      /* previous root menu if pull right */
    struct MenuRoot *next;      /* next in list of root menus */
    const char *name;           /* name of root */
    Window w;                   /* the window of the menu */
    Window shadow;              /* the shadow window */
    Pixel hi_fore;              /* highlight foreground */
    Pixel hi_back;              /* highlight background */
    short mapped;               /* NEVER_MAPPED, UNMAPPED, or MAPPED */
    short height;               /* height of the menu */
    short width;                /* width of the menu */
    short items;                /* number of items in the menu */
    short pull;                 /* is there a pull right entry ? */
    short entered;              /* EnterNotify following pop up */
    short real_menu;            /* this is a real menu */
} MenuRoot;

#define NEVER_MAPPED    0       /* constants for mapped field of MenuRoot */
#define UNMAPPED        1
#define MAPPED          2

typedef struct MouseButton {
    int func;                   /* the function number */
    int mask;                   /* modifier mask */
    MenuRoot *menu;             /* menu if func is F_MENU */
    MenuItem *item;             /* action to perform if func != F_MENU */
} MouseButton;

typedef struct FuncKey {
    struct FuncKey *next;       /* next in the list of function keys */
    char *name;                 /* key name */
    KeySym keysym;              /* X keysym */
    KeyCode keycode;            /* X keycode */
    int cont;                   /* context */
    int mods;                   /* modifiers */
    int func;                   /* function to perform */
    char *win_name;             /* window name (if any) */
    char *action;               /* action string (if any) */
} FuncKey;

extern int RootFunction;
extern MenuRoot *ActiveMenu;
extern MenuItem *ActiveItem;
extern int MoveFunction;
extern int WindowMoved;
extern int ConstMove;
extern int ConstMoveDir;
extern int ConstMoveX;
extern int ConstMoveY;
extern int ConstMoveXL;
extern int ConstMoveXR;
extern int ConstMoveYT;
extern int ConstMoveYB;
extern int menuFromFrameOrWindowOrTitlebar;
extern int ResizeOrigX;
extern int ResizeOrigY;

#define MAXMENUDEPTH    10      /* max number of nested menus */
extern int MenuDepth;

#define MOVE_NONE       0       /* modes of constrained move */
#define MOVE_VERT       1
#define MOVE_HORIZ      2

#define WARPSCREEN_NEXT "next"
#define WARPSCREEN_PREV "prev"
#define WARPSCREEN_BACK "back"

#define COLORMAP_NEXT "next"
#define COLORMAP_PREV "prev"
#define COLORMAP_DEFAULT "default"

extern void InitMenus(void);
extern Bool AddFuncKey(char *name, int cont, int mods, int func, char *win_name,
                       char *action);
extern int CreateTitleButton(const char *name, int func, const char *action,
                             MenuRoot *menuroot, Bool rightside, Bool append);
extern void InitTitlebarButtons(void);
extern void PaintEntry(MenuRoot *mr, MenuItem *mi, int exposure);
extern void PaintMenu(MenuRoot *mr, XEvent *e);
extern void UpdateMenu(void);
extern MenuRoot *NewMenuRoot(const char *name);
extern MenuItem *AddToMenu(MenuRoot *menu, const char *item, const char *action,
                           MenuRoot *sub, int func, const char *fore,
                           const char *back);
extern void MakeMenus(void);
extern Bool PopUpMenu(MenuRoot *menu, int x, int y, Bool center);
extern void PopDownMenu(void);
extern MenuRoot *FindMenuRoot(const char *name);
extern int ExecuteFunction(int func, const char *action, Window w,
                           TwmWindow *tmp_win, XEvent *eventp, int context,
                           int pulldown);
extern void ReGrab(void);
extern void FocusOnRoot(void);
extern void DeIconify(TwmWindow *tmp_win);
extern void Iconify(TwmWindow *tmp_win, int def_x, int def_y);
extern void SetMapStateProp(TwmWindow *tmp_win, int state);
extern void WarpToScreen(int n, int inc);
extern void SetBorder(TwmWindow *tmp, Bool onoroff);
extern void SendDeleteWindowMessage(TwmWindow *tmp, Time timestamp);
extern void SendSaveYourselfMessage(TwmWindow *tmp, Time timestamp);
extern void SendTakeFocusMessage(TwmWindow *tmp, Time timestamp);

#endif                          /* _MENUS_ */
