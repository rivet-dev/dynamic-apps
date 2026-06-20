/*****************************************************************************/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    names of Evans & Sutherland not be used in advertising               **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND AND DISCLAIMS ALL WARRANTIES WITH REGARD          **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND       **/
/**    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAMAGES        **/
/**    OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA         **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/
/*

Portions Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/***********************************************************************
 *
 * twm include file
 *
 * 28-Oct-87 Thomas E. LaStrange        File created
 * 10-Oct-90 David M. Sternlicht        Storing saved colors on root
 ***********************************************************************/

#ifndef _TWM_
#define _TWM_
/* *INDENT-OFF* */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <X11/Xfuncs.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>

#ifndef GCC_PRINTFLIKE
#if defined(GCC_PRINTF) && !defined(printf)
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#endif

#ifndef GCC_NORETURN
#define GCC_NORETURN _X_NORETURN
#endif

#ifndef WithdrawnState
#define WithdrawnState 0
#endif

#define PIXEL_ALREADY_TYPEDEFED /* for Xmu/Drawing.h */

typedef void (*SigProc) (int);  /* type of function returned by signal() */

#define BW 2                    /* border width */
#define BW2 4                   /* border width  * 2 */

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#define NULLSTR ((char *) NULL)

#define MAX_BUTTONS     16      /* max mouse buttons supported */

/* info strings defines */
#define INFO_LINES 30
#define INFO_SIZE 200

/* contexts for button presses */
#define C_NO_CONTEXT    -1
#define C_WINDOW        0
#define C_TITLE         1
#define C_ICON          2
#define C_ROOT          3
#define C_FRAME         4
#define C_ICONMGR       5
#define C_NAME          6
#define C_IDENTIFY      7
#define NUM_CONTEXTS    8

#define C_WINDOW_BIT    (1 << C_WINDOW)
#define C_TITLE_BIT     (1 << C_TITLE)
#define C_ICON_BIT      (1 << C_ICON)
#define C_ROOT_BIT      (1 << C_ROOT)
#define C_FRAME_BIT     (1 << C_FRAME)
#define C_ICONMGR_BIT   (1 << C_ICONMGR)
#define C_NAME_BIT      (1 << C_NAME)

#define C_ALL_BITS      (C_WINDOW_BIT | C_TITLE_BIT | C_ICON_BIT |\
                         C_ROOT_BIT | C_FRAME_BIT | C_ICONMGR_BIT)

/* modifiers for button presses */
#define MOD_SIZE        ((ShiftMask | ControlMask | Mod1Mask \
                          | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask) + 1)

#define TITLE_BAR_SPACE         1       /* 2 pixel space bordering chars */
#define TITLE_BAR_FONT_HEIGHT   15      /* max of 15 pixel high chars */
#define TITLE_BAR_HEIGHT        (TITLE_BAR_FONT_HEIGHT+(2*TITLE_BAR_SPACE))

/* defines for zooming/unzooming */
#define ZOOM_NONE 0

#define FB(fix_fore, fix_back)\
    Gcv.foreground = fix_fore;\
    Gcv.background = fix_back;\
    XChangeGC(dpy, Scr->NormalGC, GCForeground|GCBackground,&Gcv)

typedef struct MyFont {
    const char *name;           /* name of the font */
    XFontStruct *font;          /* font structure */
    XFontSet fontset;           /* fontset structure */
    int height;                 /* height of the font */
    int y;                      /* Y coordinate to draw characters */
    int ascent;
    int descent;
} MyFont;

typedef struct ColorPair {
    Pixel fore, back;
} ColorPair;

typedef struct _TitleButton {
    struct _TitleButton *next;  /* next link in chain */
    const char *name;           /* bitmap name in case of deferral */
    Pixmap bitmap;              /* image to display in button */
    int srcx, srcy;             /* from where to start copying */
    unsigned int width, height; /* size of pixmap */
    int dstx, dsty;             /* to where to start copying */
    int func;                   /* function to execute */
    const char *action;         /* optional action arg */
    struct MenuRoot *menuroot;  /* menu to pop on F_MENU */
    Bool rightside;             /* t: on right, f: on left */
} TitleButton;

typedef struct _TBWindow {
    Window window;              /* which window in this frame */
    TitleButton *info;          /* description of this window */
} TBWindow;

typedef struct _SqueezeInfo {
    int justify;                /* left, center, right */
    int num;                    /* signed pixel count or numerator */
    int denom;                  /* 0 for pix count or denominator */
} SqueezeInfo;

#define J_LEFT                  1
#define J_CENTER                2
#define J_RIGHT                 3

/* Colormap window entry for each window in WM_COLORMAP_WINDOWS
 * ICCCM property.
 */
typedef struct TwmColormap {
    Colormap c;                 /* Colormap id */
    int state;                  /* install(ability) state */
    unsigned long install_req;  /* request number which installed it */
    Window w;                   /* window causing load of color table */
    int refcnt;
} TwmColormap;

#define CM_INSTALLABLE          1
#define CM_INSTALLED            2
#define CM_INSTALL              4

typedef struct ColormapWindow {
    Window w;                   /* Window id */
    TwmColormap *colormap;      /* Colormap for this window */
    int visibility;             /* Visibility of this window */
    int refcnt;
} ColormapWindow;

typedef struct Colormaps {
    ColormapWindow **cwins;     /* current list of colormap windows */
    int number_cwins;           /* number of elements in current list */
    char *scoreboard;           /* conflicts between installable colortables */
} Colormaps;

#define ColormapsScoreboardLength(cm) (size_t)((cm)->number_cwins * \
                                               ((cm)->number_cwins - 1) / 2)

/* for each window that is on the display, one of these structures
 * is allocated and linked into a list
 */
typedef struct TwmWindow {
    struct TwmWindow *next;     /* next twm window */
    struct TwmWindow *prev;     /* previous twm window */
    Window w;                   /* the child window */
    int old_bw;                 /* border width before reparenting */
    Window frame;               /* the frame window */
    Window title_w;             /* the title bar window */
    Window hilite_w;            /* the hilite window */
    Pixmap gray;
    Window icon_w;              /* the icon window */
    Window icon_bm_w;           /* the icon bitmap window */
    int frame_x;                /* x position of frame */
    int frame_y;                /* y position of frame */
    int frame_width;            /* width of frame */
    int frame_height;           /* height of frame */
    int frame_bw;               /* borderwidth of frame */
    int title_x;
    int title_y;
    int icon_x;                 /* icon text x coordinate */
    int icon_y;                 /* icon text y coordinate */
    int icon_w_width;           /* width of the icon window */
    int icon_w_height;          /* height of the icon window */
    int icon_width;             /* width of the icon bitmap */
    int icon_height;            /* height of the icon bitmap */
    int title_height;           /* height of the title bar */
    int title_width;            /* width of the title bar */
    char *full_name;            /* full name of the window */
    char *name;                 /* name of the window */
    char *icon_name;            /* name of the icon */
    int name_width;             /* width of name text */
    int highlightx;             /* start of highlight window */
    int rightx;                 /* start of right buttons */
    XWindowAttributes attr;     /* the child window attributes */
    XSizeHints hints;           /* normal hints */
    XWMHints *wmhints;          /* WM hints */
    Window group;               /* group ID */
    XClassHint class;
    struct WList *list;
    /***********************************************************************
     * color definitions per window
     **********************************************************************/
    Pixel border;               /* border color */
    Pixel icon_border;          /* border color */
    ColorPair border_tile;
    ColorPair title;
    ColorPair iconc;
    short iconified;            /* has the window ever been iconified? */
    short icon;                 /* is the window an icon now ? */
    short icon_on;              /* is the icon visible */
    short mapped;               /* is the window mapped ? */
    short auto_raise;           /* should we auto-raise this window ? */
    short forced;               /* has had an icon forced upon it */
    short icon_not_ours;        /* icon pixmap or window supplied to us */
    short icon_moved;           /* user explicitly moved the icon */
    short highlight;            /* should highlight this window */
    short stackmode;            /* honor stackmode requests */
    short iconify_by_unmapping; /* unmap window to iconify it */
    short iconmgr;              /* this is an icon manager window */
    short transient;            /* this is a transient window */
    Window transientfor;        /* window contained in XA_XM_TRANSIENT_FOR */
    short titlehighlight;       /* should I highlight the title bar */
    struct IconMgr *iconmgrp;   /* pointer to it if this is an icon manager */
    int save_frame_x;           /* x position of frame */
    int save_frame_y;           /* y position of frame */
    int save_frame_width;       /* width of frame */
    int save_frame_height;      /* height of frame */
    short zoomed;               /* is the window zoomed? */
    short wShaped;              /* this window has a bounding shape */
    unsigned long protocols;    /* which protocols this window handles */
    Colormaps cmaps;            /* colormaps for this application */
    TBWindow *titlebuttons;
    SqueezeInfo *squeeze_info;  /* should the title be squeezed? */
    struct {
        struct TwmWindow *next, *prev;
        Bool cursor_valid;
        int curs_x, curs_y;
    } ring;

    Bool nameChanged;           /* did WM_NAME ever change? */

    /* did the user ever change the width/height? {yes, no, or unknown} */

    Bool widthEverChangedByUser;
    Bool heightEverChangedByUser;

} TwmWindow;

typedef struct TWMWinConfigEntry {
    struct TWMWinConfigEntry *next;
    int tag;
    char *client_id;
    char *window_role;
    XClassHint class;
    char *wm_name;
    int wm_command_count;
    char **wm_command;
    short x, y;
    unsigned short width, height;
    short icon_x, icon_y;
    Bool iconified;
    Bool icon_info_present;
    Bool width_ever_changed_by_user;
    Bool height_ever_changed_by_user;
} TWMWinConfigEntry;

#define DoesWmTakeFocus         (1L << 0)
#define DoesWmSaveYourself      (1L << 1)
#define DoesWmDeleteWindow      (1L << 2)

#define TBPM_DOT ":dot"         /* name of titlebar pixmap for dot */
#define TBPM_ICONIFY ":iconify" /* same image as dot */
#define TBPM_RESIZE ":resize"   /* name of titlebar pixmap for resize button */
#define TBPM_XLOGO ":xlogo"     /* name of titlebar pixmap for xlogo */
#define TBPM_DELETE ":delete"   /* same image as xlogo */
#define TBPM_MENU ":menu"       /* name of titlebar pixmap for menus */
#define TBPM_QUESTION ":question"       /* name of unknown titlebar pixmap */

#include <X11/Xosdefs.h>
#include <stdlib.h>
extern void CreateFonts(void);
extern void RestoreWithdrawnLocation(TwmWindow *tmp);
extern void Reborder(Time time);
extern void
Done(XtPointer, XtSignalId *) _X_NORETURN;
extern void
ComputeCommonTitleOffsets(void);
extern void
ComputeTitleLocation(TwmWindow *tmp);
extern void
ComputeWindowTitleOffsets(TwmWindow *tmp_win, int width, Bool squeeze);
extern char *ProgramName;
extern Display *dpy;
extern XtAppContext appContext;
extern Window ResizeWindow;     /* the window we are resizing */
extern int HasShape;            /* this server supports Shape extension */
extern int HasSync;             /* this server supports SYNC extension */

#ifdef HAVE_XRANDR
extern int HasXrandr;           /* this server supports Xrandr extension */
#endif

extern int PreviousScreen;

extern Cursor UpperLeftCursor;
extern Cursor RightButt;
extern Cursor MiddleButt;
extern Cursor LeftButt;

extern XClassHint NoClass;

extern XContext TwmContext;
extern XContext MenuContext;
extern XContext IconManagerContext;
extern XContext ScreenContext;
extern XContext ColormapContext;

extern const char *Home;
extern int HomeLen;
extern int ParseError;

extern int HandlingEvents;

extern Window JunkRoot;
extern Window JunkChild;
extern int JunkX;
extern int JunkY;
extern unsigned int JunkWidth, JunkHeight, JunkBW, JunkDepth, JunkMask;
extern XGCValues Gcv;
extern int InfoLines;
extern char Info[][INFO_SIZE];
extern int Argc;
extern char **Argv;
extern void
NewFontCursor(Cursor *cp, const char *str);
extern void
NewBitmapCursor(Cursor *cp, char *source, char *mask);
extern Pixmap
CreateMenuIcon(int height, unsigned int *widthp, unsigned int *heightp);

extern void twmError(const char *, ...) GCC_PRINTFLIKE(1,2) GCC_NORETURN;
extern void twmWarning(const char *, ...) GCC_PRINTFLIKE(1,2);
extern void twmVerbose(const char *, ...) GCC_PRINTFLIKE(1,2);
extern void twmMessage(const char *, ...) GCC_PRINTFLIKE(1,2);

extern Bool ErrorOccurred;
extern XErrorEvent LastErrorEvent;

#define ResetError() (ErrorOccurred = False)

extern Bool RestartPreviousState;
extern Bool
GetWMState(Window w, int *statep, Window *iwp);

extern int
yyparse(void);
extern int
yylex(void);

extern void parseWarning(const char *, ...) GCC_PRINTFLIKE(1,2);

extern Atom TwmAtoms[];

extern int message_level;
extern Bool use_fontset;

extern int ShapeEventBase;
extern int ShapeErrorBase;

#ifdef HAVE_XRANDR
extern int XrandrEventBase;
extern int XrandrErrorBase;
#endif

#define _XA_MIT_PRIORITY_COLORS         TwmAtoms[0]
#define _XA_WM_CHANGE_STATE             TwmAtoms[1]
#define _XA_WM_STATE                    TwmAtoms[2]
#define _XA_WM_COLORMAP_WINDOWS         TwmAtoms[3]
#define _XA_WM_PROTOCOLS                TwmAtoms[4]
#define _XA_WM_TAKE_FOCUS               TwmAtoms[5]
#define _XA_WM_SAVE_YOURSELF            TwmAtoms[6]
#define _XA_WM_DELETE_WINDOW            TwmAtoms[7]
#define _XA_SM_CLIENT_ID                TwmAtoms[8]
#define _XA_WM_CLIENT_LEADER            TwmAtoms[9]
#define _XA_WM_WINDOW_ROLE              TwmAtoms[10]

/* *INDENT-ON* */
#endif                          /* _TWM_ */
