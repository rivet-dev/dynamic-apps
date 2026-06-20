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
 * parse the .twmrc file
 *
 * 17-Nov-87 Thomas E. LaStrange       File created
 * 10-Oct-90 David M. Sternlicht       Storing saved colors on root
 ***********************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <X11/Xos.h>
#include <X11/Xmu/CharSet.h>

#include "twm.h"
#include "screen.h"
#include "menus.h"
#include "util.h"
#include "gram.h"
#include "parse.h"

#include <X11/Xatom.h>
#include <X11/extensions/sync.h>

#ifndef SYSTEM_INIT_FILE
#define SYSTEM_INIT_FILE "/usr/lib/X11/twm/system.twmrc"
#endif
#define BUF_LEN 300

static FILE *twmrc;
static int ptr = 0;
static int len = 0;
static unsigned char buff[BUF_LEN + 1];
static unsigned char overflowbuff[20];  /* really only need one */
static int overflowlen;
static unsigned char **stringListSource, *currentString;

static int twmFileInput(void);
static int twmStringListInput(void);
static int ParseUsePPosition(char *s);
static int ParseStringList(unsigned char **sl);

extern int yylineno;

int ConstrainedMoveTime = 400;  /* milliseconds, event times */

int (*twmInputFunc) (void);

static const char *twmInputFile;
static const char *twmInputType;

/**
 * parse the .twmrc file
 *  \param filename the filename to parse.  NULL indicates $HOME/.twmrc
 */
static int
doparse(int (*ifunc) (void), const char *srctypename, const char *srcname)
{
    mods = 0;
    ptr = 0;
    len = 0;
    yylineno = 1;
    ParseError = FALSE;
    twmInputFunc = ifunc;
    twmInputFile = srcname;
    twmInputType = srctypename;
    overflowlen = 0;

    yyparse();

    if (Scr->PointerForeground.pixel != Scr->Black ||
        Scr->PointerBackground.pixel != Scr->White) {
        XRecolorCursor(dpy, UpperLeftCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, RightButt,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, LeftButt,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, MiddleButt,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->FrameCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->TitleCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->IconCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->IconMgrCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->MoveCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->ResizeCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->MenuCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->ButtonCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->WaitCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->SelectCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
        XRecolorCursor(dpy, Scr->DestroyCursor,
                       &Scr->PointerForeground, &Scr->PointerBackground);
    }
    if (ParseError) {
        if (srcname) {
            twmWarning("errors found in twm %s \"%s\"", srctypename, srcname);
        }
        else {
            twmWarning("errors found in twm %s", srctypename);
        }
    }
    return (ParseError ? 0 : 1);
}

int
ParseTwmrc(char *filename)
{
    int i;
    char *home = NULL;
    int homelen = 0;
    const char *cp = NULL;
    char tmpfilename[257];

    /*
     * If filename given, try it, else try ~/.twmrc.# then ~/.twmrc.  Then
     * try system.twmrc; finally using built-in defaults.
     */
    for (twmrc = NULL, i = 0; !twmrc && i < 4; i++) {
        switch (i) {
        case 0:                /* -f filename */
            cp = filename;
            break;

        case 1:                /* ~/.twmrc.screennum */
            if (!filename) {
                home = getenv("HOME");
                if (home) {
                    homelen = (int) strlen(home);
                    cp = tmpfilename;
                    (void) snprintf(tmpfilename, sizeof(tmpfilename),
                                    "%s/.twmrc.%d", home, Scr->screen);
                    break;
                }
            }
            continue;

        case 2:                /* ~/.twmrc */
            if (home) {
                tmpfilename[homelen + 7] = '\0';
            }
            break;

        case 3:                /* system.twmrc */
            cp = SYSTEM_INIT_FILE;
            break;
        }

        if (cp)
            twmrc = fopen(cp, "r");
    }

    if (twmrc) {
        int status;

        if (filename && cp != filename) {
            twmWarning("unable to open twmrc file %s, using %s instead",
                       filename, cp);
        }
        status = doparse(twmFileInput, "file", cp);
        fclose(twmrc);
        return status;
    }
    else {
        if (filename) {
            twmWarning("unable to open twmrc file %s,"
                       " using built-in defaults instead", filename);
        }
        return ParseStringList(defTwmrc);
    }
}

static int
ParseStringList(unsigned char **sl)
{
    stringListSource = sl;
    currentString = *sl;
    return doparse(twmStringListInput, "string list", (char *) NULL);
}

/**
 *  redefinition of the lex input routine for file input
 *
 *  \return the next input character
 */
static int
twmFileInput(void)
{
    if (overflowlen)
        return (int) overflowbuff[--overflowlen];

    while (ptr == len) {
        if (fgets((char *) buff, BUF_LEN, twmrc) == NULL)
            return 0;

        ptr = 0;
        len = (int) strlen((char *) buff);
    }
    return ((int) buff[ptr++]);
}

static int
twmStringListInput(void)
{
    if (overflowlen)
        return (int) overflowbuff[--overflowlen];

    /*
     * return the character currently pointed to
     */
    if (currentString) {
        unsigned int c = (unsigned int) *currentString++;

        if (c)
            return (int) c;     /* if non-nul char */
        currentString = *++stringListSource;    /* advance to next bol */
        return '\n';            /* but say that we hit last eol */
    }
    return 0;                   /* eof */
}

/*
 * redefinition of the lex unput routine
 *
 *  \param c the character to push back onto the input stream
 */
void
twmUnput(int c)
{
    if ((size_t) overflowlen < sizeof overflowbuff) {
        overflowbuff[overflowlen++] = (unsigned char) c;
    }
    else {
        parseWarning("unable to unput character (%d)", c);
    }
}

/**
 * redefinition of the lex output routine
 *
 * \param c  the character to print
 */

void
TwmOutput(int c)
{
    putchar(c);
}

/**********************************************************************
 *  Parsing table and routines
 ***********************************************************************/

typedef struct _TwmKeyword {
    const char *name;
    int value;
    int subnum;
} TwmKeyword;

#define kw0_NoDefaults                  1
#define kw0_AutoRelativeResize          2
#define kw0_ForceIcons                  3
#define kw0_NoIconManagers              4
#define kw0_OpaqueMove                  5
#define kw0_InterpolateMenuColors       6
#define kw0_NoVersion                   7
#define kw0_SortIconManager             8
#define kw0_NoGrabServer                9
#define kw0_NoMenuShadows               10
#define kw0_NoRaiseOnMove               11
#define kw0_NoRaiseOnResize             12
#define kw0_NoRaiseOnDeiconify          13
#define kw0_DontMoveOff                 14
#define kw0_NoBackingStore              15
#define kw0_NoSaveUnders                16
#define kw0_RestartPreviousState        17
#define kw0_ClientBorderWidth           18
#define kw0_NoTitleFocus                19
#define kw0_RandomPlacement             20
#define kw0_DecorateTransients          21
#define kw0_ShowIconManager             22
#define kw0_NoCaseSensitive             23
#define kw0_NoRaiseOnWarp               24
#define kw0_WarpUnmapped                25

#define kws_UsePPosition                1
#define kws_IconFont                    2
#define kws_ResizeFont                  3
#define kws_MenuFont                    4
#define kws_TitleFont                   5
#define kws_IconManagerFont             6
#define kws_UnknownIcon                 7
#define kws_IconDirectory               8
#define kws_MaxWindowSize               9

#define kwn_ConstrainedMoveTime         1
#define kwn_MoveDelta                   2
#define kwn_XorValue                    3
#define kwn_FramePadding                4
#define kwn_TitlePadding                5
#define kwn_ButtonIndent                6
#define kwn_BorderWidth                 7
#define kwn_IconBorderWidth             8
#define kwn_TitleButtonBorderWidth      9
#define kwn_Priority                    10
#define kwn_MenuBorderWidth             11

#define kwcl_BorderColor                1
#define kwcl_IconManagerHighlight       2
#define kwcl_BorderTileForeground       3
#define kwcl_BorderTileBackground       4
#define kwcl_TitleForeground            5
#define kwcl_TitleBackground            6
#define kwcl_IconForeground             7
#define kwcl_IconBackground             8
#define kwcl_IconBorderColor            9
#define kwcl_IconManagerForeground      10
#define kwcl_IconManagerBackground      11

#define kwc_DefaultForeground           1
#define kwc_DefaultBackground           2
#define kwc_MenuForeground              3
#define kwc_MenuBackground              4
#define kwc_MenuTitleForeground         5
#define kwc_MenuTitleBackground         6
#define kwc_MenuShadowColor             7
#define kwc_PointerForeground           8
#define kwc_PointerBackground           9
#define kwc_MenuBorderColor             10

/*
 * The following is sorted alphabetically according to name (which must be
 * in lowercase and only contain the letters a-z).  It is fed to a binary
 * search to parse keywords.
 */
/* *INDENT-OFF* */
static TwmKeyword keytable[] = {
    { "all",                    ALL, 0 },
    { "autoraise",              AUTO_RAISE, 0 },
    { "autorelativeresize",     KEYWORD, kw0_AutoRelativeResize },
    { "bordercolor",            CLKEYWORD, kwcl_BorderColor },
    { "bordertilebackground",   CLKEYWORD, kwcl_BorderTileBackground },
    { "bordertileforeground",   CLKEYWORD, kwcl_BorderTileForeground },
    { "borderwidth",            NKEYWORD, kwn_BorderWidth },
    { "button",                 BUTTON, 0 },
    { "buttonindent",           NKEYWORD, kwn_ButtonIndent },
    { "c",                      CONTROL, 0 },
    { "center",                 JKEYWORD, J_CENTER },
    { "clientborderwidth",      KEYWORD, kw0_ClientBorderWidth },
    { "color",                  COLOR, 0 },
    { "constrainedmovetime",    NKEYWORD, kwn_ConstrainedMoveTime },
    { "control",                CONTROL, 0 },
    { "cursors",                CURSORS, 0 },
    { "decoratetransients",     KEYWORD, kw0_DecorateTransients },
    { "defaultbackground",      CKEYWORD, kwc_DefaultBackground },
    { "defaultforeground",      CKEYWORD, kwc_DefaultForeground },
    { "defaultfunction",        DEFAULT_FUNCTION, 0 },
    { "destroy",                KILL, 0 },
    { "donticonifybyunmapping", DONT_ICONIFY_BY_UNMAPPING, 0 },
    { "dontmoveoff",            KEYWORD, kw0_DontMoveOff },
    { "dontsqueezetitle",       DONT_SQUEEZE_TITLE, 0 },
    { "east",                   DKEYWORD, D_EAST },
    { "f",                      FRAME, 0 },
    { "f.autoraise",            FKEYWORD, F_AUTORAISE },
    { "f.backiconmgr",          FKEYWORD, F_BACKICONMGR },
    { "f.beep",                 FKEYWORD, F_BEEP },
    { "f.bottomzoom",           FKEYWORD, F_BOTTOMZOOM },
    { "f.circledown",           FKEYWORD, F_CIRCLEDOWN },
    { "f.circleup",             FKEYWORD, F_CIRCLEUP },
    { "f.colormap",             FSKEYWORD, F_COLORMAP },
    { "f.cut",                  FSKEYWORD, F_CUT },
    { "f.cutfile",              FKEYWORD, F_CUTFILE },
    { "f.deiconify",            FKEYWORD, F_DEICONIFY },
    { "f.delete",               FKEYWORD, F_DELETE },
    { "f.deltastop",            FKEYWORD, F_DELTASTOP },
    { "f.destroy",              FKEYWORD, F_DESTROY },
    { "f.downiconmgr",          FKEYWORD, F_DOWNICONMGR },
    { "f.exec",                 FSKEYWORD, F_EXEC },
    { "f.file",                 FSKEYWORD, F_FILE },
    { "f.focus",                FKEYWORD, F_FOCUS },
    { "f.forcemove",            FKEYWORD, F_FORCEMOVE },
    { "f.forwiconmgr",          FKEYWORD, F_FORWICONMGR },
    { "f.fullzoom",             FKEYWORD, F_FULLZOOM },
    { "f.function",             FSKEYWORD, F_FUNCTION },
    { "f.hbzoom",               FKEYWORD, F_BOTTOMZOOM },
    { "f.hideiconmgr",          FKEYWORD, F_HIDELIST },
    { "f.horizoom",             FKEYWORD, F_HORIZOOM },
    { "f.htzoom",               FKEYWORD, F_TOPZOOM },
    { "f.hzoom",                FKEYWORD, F_HORIZOOM },
    { "f.iconify",              FKEYWORD, F_ICONIFY },
    { "f.identify",             FKEYWORD, F_IDENTIFY },
    { "f.lefticonmgr",          FKEYWORD, F_LEFTICONMGR },
    { "f.leftzoom",             FKEYWORD, F_LEFTZOOM },
    { "f.lower",                FKEYWORD, F_LOWER },
    { "f.menu",                 FSKEYWORD, F_MENU },
    { "f.move",                 FKEYWORD, F_MOVE },
    { "f.nexticonmgr",          FKEYWORD, F_NEXTICONMGR },
    { "f.nop",                  FKEYWORD, F_NOP },
    { "f.previconmgr",          FKEYWORD, F_PREVICONMGR },
    { "f.priority",             FSKEYWORD, F_PRIORITY },
    { "f.quit",                 FKEYWORD, F_QUIT },
    { "f.raise",                FKEYWORD, F_RAISE },
    { "f.raiselower",           FKEYWORD, F_RAISELOWER },
    { "f.refresh",              FKEYWORD, F_REFRESH },
    { "f.resize",               FKEYWORD, F_RESIZE },
    { "f.restart",              FKEYWORD, F_RESTART },
    { "f.righticonmgr",         FKEYWORD, F_RIGHTICONMGR },
    { "f.rightzoom",            FKEYWORD, F_RIGHTZOOM },
    { "f.saveyourself",         FKEYWORD, F_SAVEYOURSELF },
    { "f.showiconmgr",          FKEYWORD, F_SHOWLIST },
    { "f.sorticonmgr",          FKEYWORD, F_SORTICONMGR },
    { "f.source",               FSKEYWORD, F_BEEP },  /* XXX - don't work */
    { "f.startwm",              FSKEYWORD, F_STARTWM },
    { "f.title",                FKEYWORD, F_TITLE },
    { "f.topzoom",              FKEYWORD, F_TOPZOOM },
    { "f.twmrc",                FKEYWORD, F_RESTART },
    { "f.unfocus",              FKEYWORD, F_UNFOCUS },
    { "f.upiconmgr",            FKEYWORD, F_UPICONMGR },
    { "f.version",              FKEYWORD, F_VERSION },
    { "f.vlzoom",               FKEYWORD, F_LEFTZOOM },
    { "f.vrzoom",               FKEYWORD, F_RIGHTZOOM },
    { "f.warpnext",             FKEYWORD, F_WARPNEXT },
    { "f.warpprev",             FKEYWORD, F_WARPPREV },
    { "f.warpring",             FSKEYWORD, F_WARPRING },
    { "f.warpto",               FSKEYWORD, F_WARPTO },
    { "f.warptoiconmgr",        FSKEYWORD, F_WARPTOICONMGR },
    { "f.warptoscreen",         FSKEYWORD, F_WARPTOSCREEN },
    { "f.winrefresh",           FKEYWORD, F_WINREFRESH },
    { "f.zoom",                 FKEYWORD, F_ZOOM },
    { "forceicons",             KEYWORD, kw0_ForceIcons },
    { "frame",                  FRAME, 0 },
    { "framepadding",           NKEYWORD, kwn_FramePadding },
    { "function",               FUNCTION, 0 },
    { "grayscale",              GRAYSCALE, 0 },
    { "greyscale",              GRAYSCALE, 0 },
    { "i",                      ICON, 0 },
    { "icon",                   ICON, 0 },
    { "iconbackground",         CLKEYWORD, kwcl_IconBackground },
    { "iconbordercolor",        CLKEYWORD, kwcl_IconBorderColor },
    { "iconborderwidth",        NKEYWORD, kwn_IconBorderWidth },
    { "icondirectory",          SKEYWORD, kws_IconDirectory },
    { "iconfont",               SKEYWORD, kws_IconFont },
    { "iconforeground",         CLKEYWORD, kwcl_IconForeground },
    { "iconifybyunmapping",     ICONIFY_BY_UNMAPPING, 0 },
    { "iconmanagerbackground",  CLKEYWORD, kwcl_IconManagerBackground },
    { "iconmanagerdontshow",    ICONMGR_NOSHOW, 0 },
    { "iconmanagerfont",        SKEYWORD, kws_IconManagerFont },
    { "iconmanagerforeground",  CLKEYWORD, kwcl_IconManagerForeground },
    { "iconmanagergeometry",    ICONMGR_GEOMETRY, 0 },
    { "iconmanagerhighlight",   CLKEYWORD, kwcl_IconManagerHighlight },
    { "iconmanagers",           ICONMGRS, 0 },
    { "iconmanagershow",        ICONMGR_SHOW, 0 },
    { "iconmgr",                ICONMGR, 0 },
    { "iconregion",             ICON_REGION, 0 },
    { "icons",                  ICONS, 0 },
    { "interpolatemenucolors",  KEYWORD, kw0_InterpolateMenuColors },
    { "l",                      LOCK, 0 },
    { "left",                   JKEYWORD, J_LEFT },
    { "lefttitlebutton",        LEFT_TITLEBUTTON, 0 },
    { "lock",                   LOCK, 0 },
    { "m",                      META, 0 },
    { "maketitle",              MAKE_TITLE, 0 },
    { "maxwindowsize",          SKEYWORD, kws_MaxWindowSize },
    { "menu",                   MENU, 0 },
    { "menubackground",         CKEYWORD, kwc_MenuBackground },
    { "menubordercolor",        CKEYWORD, kwc_MenuBorderColor },
    { "menuborderwidth",        NKEYWORD, kwn_MenuBorderWidth },
    { "menufont",               SKEYWORD, kws_MenuFont },
    { "menuforeground",         CKEYWORD, kwc_MenuForeground },
    { "menushadowcolor",        CKEYWORD, kwc_MenuShadowColor },
    { "menutitlebackground",    CKEYWORD, kwc_MenuTitleBackground },
    { "menutitleforeground",    CKEYWORD, kwc_MenuTitleForeground },
    { "meta",                   META, 0 },
    { "mod",                    META, 0 },  /* fake it */
    { "monochrome",             MONOCHROME, 0 },
    { "move",                   MOVE, 0 },
    { "movedelta",              NKEYWORD, kwn_MoveDelta },
    { "nobackingstore",         KEYWORD, kw0_NoBackingStore },
    { "nocasesensitive",        KEYWORD, kw0_NoCaseSensitive },
    { "nodefaults",             KEYWORD, kw0_NoDefaults },
    { "nograbserver",           KEYWORD, kw0_NoGrabServer },
    { "nohighlight",            NO_HILITE, 0 },
    { "noiconmanagers",         KEYWORD, kw0_NoIconManagers },
    { "nomenushadows",          KEYWORD, kw0_NoMenuShadows },
    { "noraiseondeiconify",     KEYWORD, kw0_NoRaiseOnDeiconify },
    { "noraiseonmove",          KEYWORD, kw0_NoRaiseOnMove },
    { "noraiseonresize",        KEYWORD, kw0_NoRaiseOnResize },
    { "noraiseonwarp",          KEYWORD, kw0_NoRaiseOnWarp },
    { "north",                  DKEYWORD, D_NORTH },
    { "nosaveunders",           KEYWORD, kw0_NoSaveUnders },
    { "nostackmode",            NO_STACKMODE, 0 },
    { "notitle",                NO_TITLE, 0 },
    { "notitlefocus",           KEYWORD, kw0_NoTitleFocus },
    { "notitlehighlight",       NO_TITLE_HILITE, 0 },
    { "noversion",              KEYWORD, kw0_NoVersion },
    { "opaquemove",             KEYWORD, kw0_OpaqueMove },
    { "pixmaps",                PIXMAPS, 0 },
    { "pointerbackground",      CKEYWORD, kwc_PointerBackground },
    { "pointerforeground",      CKEYWORD, kwc_PointerForeground },
    { "priority",               NKEYWORD, kwn_Priority },
    { "r",                      ROOT, 0 },
    { "randomplacement",        KEYWORD, kw0_RandomPlacement },
    { "resize",                 RESIZE, 0 },
    { "resizefont",             SKEYWORD, kws_ResizeFont },
    { "restartpreviousstate",   KEYWORD, kw0_RestartPreviousState },
    { "right",                  JKEYWORD, J_RIGHT },
    { "righttitlebutton",       RIGHT_TITLEBUTTON, 0 },
    { "root",                   ROOT, 0 },
    { "s",                      SHIFT, 0 },
    { "savecolor",              SAVECOLOR, 0},
    { "select",                 SELECT, 0 },
    { "shift",                  SHIFT, 0 },
    { "showiconmanager",        KEYWORD, kw0_ShowIconManager },
    { "sorticonmanager",        KEYWORD, kw0_SortIconManager },
    { "south",                  DKEYWORD, D_SOUTH },
    { "squeezetitle",           SQUEEZE_TITLE, 0 },
    { "starticonified",         START_ICONIFIED, 0 },
    { "t",                      TITLE, 0 },
    { "title",                  TITLE, 0 },
    { "titlebackground",        CLKEYWORD, kwcl_TitleBackground },
    { "titlebuttonborderwidth", NKEYWORD, kwn_TitleButtonBorderWidth },
    { "titlefont",              SKEYWORD, kws_TitleFont },
    { "titleforeground",        CLKEYWORD, kwcl_TitleForeground },
    { "titlehighlight",         TITLE_HILITE, 0 },
    { "titlepadding",           NKEYWORD, kwn_TitlePadding },
    { "unknownicon",            SKEYWORD, kws_UnknownIcon },
    { "usepposition",           SKEYWORD, kws_UsePPosition },
    { "w",                      WINDOW, 0 },
    { "wait",                   WAIT, 0 },
    { "warpcursor",             WARP_CURSOR, 0 },
    { "warpunmapped",           KEYWORD, kw0_WarpUnmapped },
    { "west",                   DKEYWORD, D_WEST },
    { "window",                 WINDOW, 0 },
    { "windowfunction",         WINDOW_FUNCTION, 0 },
    { "windowring",             WINDOW_RING, 0 },
    { "xorvalue",               NKEYWORD, kwn_XorValue },
    { "zoom",                   ZOOM, 0 },
};
/* *INDENT-ON* */

static int numkeywords = (sizeof(keytable) / sizeof(keytable[0]));

int
parse_keyword(char *s, int *nump)
{
    register int lower = 0, upper = numkeywords - 1;

    XmuCopyISOLatin1Lowered(s, s);
    while (lower <= upper) {
        int middle = (lower + upper) / 2;
        TwmKeyword *p = &keytable[middle];
        int res = strcmp(p->name, s);

        if (res < 0) {
            lower = middle + 1;
        }
        else if (res == 0) {
            *nump = p->subnum;
            return p->value;
        }
        else {
            upper = middle - 1;
        }
    }
    return ERRORTOKEN;
}

/*
 * action routines called by grammar
 */

int
do_single_keyword(int keyword)
{
    switch (keyword) {
    case kw0_NoDefaults:
        Scr->NoDefaults = TRUE;
        return 1;

    case kw0_AutoRelativeResize:
        Scr->AutoRelativeResize = TRUE;
        return 1;

    case kw0_ForceIcons:
        if (Scr->FirstTime)
            Scr->ForceIcon = TRUE;
        return 1;

    case kw0_NoIconManagers:
        Scr->NoIconManagers = TRUE;
        return 1;

    case kw0_OpaqueMove:
        Scr->OpaqueMove = TRUE;
        return 1;

    case kw0_InterpolateMenuColors:
        if (Scr->FirstTime)
            Scr->InterpolateMenuColors = TRUE;
        return 1;

    case kw0_NoVersion:
        /* obsolete */
        return 1;

    case kw0_SortIconManager:
        if (Scr->FirstTime)
            Scr->SortIconMgr = TRUE;
        return 1;

    case kw0_NoGrabServer:
        Scr->NoGrabServer = TRUE;
        return 1;

    case kw0_NoMenuShadows:
        if (Scr->FirstTime)
            Scr->Shadow = FALSE;
        return 1;

    case kw0_NoRaiseOnMove:
        if (Scr->FirstTime)
            Scr->NoRaiseMove = TRUE;
        return 1;

    case kw0_NoRaiseOnResize:
        if (Scr->FirstTime)
            Scr->NoRaiseResize = TRUE;
        return 1;

    case kw0_NoRaiseOnDeiconify:
        if (Scr->FirstTime)
            Scr->NoRaiseDeicon = TRUE;
        return 1;

    case kw0_DontMoveOff:
        Scr->DontMoveOff = TRUE;
        return 1;

    case kw0_NoBackingStore:
        Scr->BackingStore = FALSE;
        return 1;

    case kw0_NoSaveUnders:
        Scr->SaveUnder = FALSE;
        return 1;

    case kw0_RestartPreviousState:
        RestartPreviousState = True;
        return 1;

    case kw0_ClientBorderWidth:
        if (Scr->FirstTime)
            Scr->ClientBorderWidth = TRUE;
        return 1;

    case kw0_NoTitleFocus:
        Scr->TitleFocus = FALSE;
        return 1;

    case kw0_RandomPlacement:
        Scr->RandomPlacement = TRUE;
        return 1;

    case kw0_DecorateTransients:
        Scr->DecorateTransients = TRUE;
        return 1;

    case kw0_ShowIconManager:
        Scr->ShowIconManager = TRUE;
        return 1;

    case kw0_NoCaseSensitive:
        Scr->CaseSensitive = FALSE;
        return 1;

    case kw0_NoRaiseOnWarp:
        Scr->NoRaiseWarp = TRUE;
        return 1;

    case kw0_WarpUnmapped:
        Scr->WarpUnmapped = TRUE;
        return 1;
    }

    return 0;
}

int
do_string_keyword(int keyword, char *s)
{
    switch (keyword) {
    case kws_UsePPosition:
    {
        int ppos = ParseUsePPosition(s);

        if (ppos < 0) {
            parseWarning("ignoring invalid UsePPosition argument \"%s\"", s);
        }
        else {
            Scr->UsePPosition = (short) ppos;
        }
        return 1;
    }

    case kws_IconFont:
        if (!Scr->HaveFonts)
            Scr->IconFont.name = s;
        return 1;

    case kws_ResizeFont:
        if (!Scr->HaveFonts)
            Scr->SizeFont.name = s;
        return 1;

    case kws_MenuFont:
        if (!Scr->HaveFonts)
            Scr->MenuFont.name = s;
        return 1;

    case kws_TitleFont:
        if (!Scr->HaveFonts)
            Scr->TitleBarFont.name = s;
        return 1;

    case kws_IconManagerFont:
        if (!Scr->HaveFonts)
            Scr->IconManagerFont.name = s;
        return 1;

    case kws_UnknownIcon:
        if (Scr->FirstTime)
            GetUnknownIcon(s);
        return 1;

    case kws_IconDirectory:
        if (Scr->FirstTime)
            Scr->IconDirectory = ExpandFilename(s);
        return 1;

    case kws_MaxWindowSize:
        JunkMask =
            (unsigned) XParseGeometry(s, &JunkX, &JunkY, &JunkWidth,
                                      &JunkHeight);
        if ((JunkMask & (WidthValue | HeightValue)) !=
            (WidthValue | HeightValue)) {
            parseWarning("bad MaxWindowSize \"%s\"", s);
            return 0;
        }
        if (JunkWidth <= 0 || JunkHeight <= 0) {
            parseWarning("MaxWindowSize \"%s\" must be positive", s);
            return 0;
        }
        Scr->MaxWindowWidth = (int) JunkWidth;
        Scr->MaxWindowHeight = (int) JunkHeight;
        return 1;
    }

    return 0;
}

int
do_number_keyword(int keyword, int num)
{
    switch (keyword) {
    case kwn_ConstrainedMoveTime:
        ConstrainedMoveTime = num;
        return 1;

    case kwn_MoveDelta:
        Scr->MoveDelta = (short) num;
        return 1;

    case kwn_XorValue:
        if (Scr->FirstTime)
            Scr->XORvalue = (unsigned long) num;
        return 1;

    case kwn_FramePadding:
        if (Scr->FirstTime)
            Scr->FramePadding = num;
        return 1;

    case kwn_TitlePadding:
        if (Scr->FirstTime)
            Scr->TitlePadding = num;
        return 1;

    case kwn_ButtonIndent:
        if (Scr->FirstTime)
            Scr->ButtonIndent = num;
        return 1;

    case kwn_BorderWidth:
        if (Scr->FirstTime)
            Scr->BorderWidth = num;
        return 1;

    case kwn_IconBorderWidth:
        if (Scr->FirstTime)
            Scr->IconBorderWidth = num;
        return 1;

    case kwn_MenuBorderWidth:
        if (Scr->FirstTime)
            Scr->MenuBorderWidth = num;
        return 1;

    case kwn_TitleButtonBorderWidth:
        if (Scr->FirstTime)
            Scr->TBInfo.border = num;
        return 1;

    case kwn_Priority:
        if (HasSync)
            XSyncSetPriority(dpy, /*self */ None, num);
        return 1;
    }

    return 0;
}

name_list **
do_colorlist_keyword(int keyword, int colormode, char *s)
{
    switch (keyword) {
    case kwcl_BorderColor:
        GetColor(colormode, &Scr->BorderColor, s);
        return &Scr->BorderColorL;

    case kwcl_IconManagerHighlight:
        GetColor(colormode, &Scr->IconManagerHighlight, s);
        return &Scr->IconManagerHighlightL;

    case kwcl_BorderTileForeground:
        GetColor(colormode, &Scr->BorderTileC.fore, s);
        return &Scr->BorderTileForegroundL;

    case kwcl_BorderTileBackground:
        GetColor(colormode, &Scr->BorderTileC.back, s);
        return &Scr->BorderTileBackgroundL;

    case kwcl_TitleForeground:
        GetColor(colormode, &Scr->TitleC.fore, s);
        return &Scr->TitleForegroundL;

    case kwcl_TitleBackground:
        GetColor(colormode, &Scr->TitleC.back, s);
        return &Scr->TitleBackgroundL;

    case kwcl_IconForeground:
        GetColor(colormode, &Scr->IconC.fore, s);
        return &Scr->IconForegroundL;

    case kwcl_IconBackground:
        GetColor(colormode, &Scr->IconC.back, s);
        return &Scr->IconBackgroundL;

    case kwcl_IconBorderColor:
        GetColor(colormode, &Scr->IconBorderColor, s);
        return &Scr->IconBorderColorL;

    case kwcl_IconManagerForeground:
        GetColor(colormode, &Scr->IconManagerC.fore, s);
        return &Scr->IconManagerFL;

    case kwcl_IconManagerBackground:
        GetColor(colormode, &Scr->IconManagerC.back, s);
        return &Scr->IconManagerBL;
    }
    return NULL;
}

int
do_color_keyword(int keyword, int colormode, char *s)
{
    switch (keyword) {
    case kwc_DefaultForeground:
        GetColor(colormode, &Scr->DefaultC.fore, s);
        return 1;

    case kwc_DefaultBackground:
        GetColor(colormode, &Scr->DefaultC.back, s);
        return 1;

    case kwc_MenuForeground:
        GetColor(colormode, &Scr->MenuC.fore, s);
        return 1;

    case kwc_MenuBackground:
        GetColor(colormode, &Scr->MenuC.back, s);
        return 1;

    case kwc_MenuBorderColor:
        GetColor(colormode, &Scr->MenuBorderColor, s);
        return 1;

    case kwc_MenuTitleForeground:
        GetColor(colormode, &Scr->MenuTitleC.fore, s);
        return 1;

    case kwc_MenuTitleBackground:
        GetColor(colormode, &Scr->MenuTitleC.back, s);
        return 1;

    case kwc_MenuShadowColor:
        GetColor(colormode, &Scr->MenuShadowColor, s);
        return 1;

    case kwc_PointerForeground:
        GetColorValue(colormode, &Scr->PointerForeground, s);
        return 1;

    case kwc_PointerBackground:
        GetColorValue(colormode, &Scr->PointerBackground, s);
        return 1;
    }

    return 0;
}

/**
 * Save a pixel value in twm root window color property.
 */
static void
put_pixel_on_root(Pixel pixel)
{
    int addPixel = 1;
    Atom pixelAtom, retAtom;
    int retFormat;
    unsigned long nPixels, retAfter;
    unsigned char *retProp;

    pixelAtom = XInternAtom(dpy, "_MIT_PRIORITY_COLORS", True);
    if (XGetWindowProperty(dpy, Scr->Root, pixelAtom, 0, 8192,
                           False, XA_CARDINAL, &retAtom,
                           &retFormat, &nPixels, &retAfter,
                           &retProp) == Success) {
        int i;
        Pixel *pixelProp = (Pixel *) retProp;

        for (i = 0; (unsigned long) i < nPixels; i++)
            if (pixel == pixelProp[i])
                addPixel = 0;
    }
    if (addPixel)
        XChangeProperty(dpy, Scr->Root, _XA_MIT_PRIORITY_COLORS,
                        XA_CARDINAL, 32, PropModeAppend,
                        (unsigned char *) &pixel, 1);
}

/**
 * save a color from a string in the twmrc file.
 */
void
do_string_savecolor(int colormode, char *s)
{
    Pixel p;

    GetColor(colormode, &p, s);
    put_pixel_on_root(p);
}

typedef struct _cnode {
    int i;
    struct _cnode *next;
} Cnode, *Cptr;
static Cptr chead = NULL;

/**
 * save a color from a var in the twmrc file
 */
void
do_var_savecolor(int key)
{
    Cptr cptrav, cpnew;

    if (!chead) {
        chead = malloc(sizeof(Cnode));
        chead->i = key;
        chead->next = NULL;
    }
    else {
        cptrav = chead;
        while (cptrav->next != NULL) {
            cptrav = cptrav->next;
        }
        cpnew = malloc(sizeof(Cnode));
        cpnew->i = key;
        cpnew->next = NULL;
        cptrav->next = cpnew;
    }
}

/**
 * traverse the var save color list placeing the pixels
 *                        in the root window property.
 */
void
assign_var_savecolor(void)
{
    Cptr cp = chead;

    while (cp != NULL) {
        switch (cp->i) {
        case kwcl_BorderColor:
            put_pixel_on_root(Scr->BorderColor);
            break;
        case kwcl_IconManagerHighlight:
            put_pixel_on_root(Scr->IconManagerHighlight);
            break;
        case kwcl_BorderTileForeground:
            put_pixel_on_root(Scr->BorderTileC.fore);
            break;
        case kwcl_BorderTileBackground:
            put_pixel_on_root(Scr->BorderTileC.back);
            break;
        case kwcl_TitleForeground:
            put_pixel_on_root(Scr->TitleC.fore);
            break;
        case kwcl_TitleBackground:
            put_pixel_on_root(Scr->TitleC.back);
            break;
        case kwcl_IconForeground:
            put_pixel_on_root(Scr->IconC.fore);
            break;
        case kwcl_IconBackground:
            put_pixel_on_root(Scr->IconC.back);
            break;
        case kwcl_IconBorderColor:
            put_pixel_on_root(Scr->IconBorderColor);
            break;
        case kwcl_IconManagerForeground:
            put_pixel_on_root(Scr->IconManagerC.fore);
            break;
        case kwcl_IconManagerBackground:
            put_pixel_on_root(Scr->IconManagerC.back);
            break;
        }
        cp = cp->next;
    }
    if (chead) {
        free(chead);
        chead = NULL;
    }
}

static int
ParseUsePPosition(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, "off") == 0) {
        return PPOS_OFF;
    }
    else if (strcmp(s, "on") == 0) {
        return PPOS_ON;
    }
    else if (strcmp(s, "non-zero") == 0 || strcmp(s, "nonzero") == 0) {
        return PPOS_NON_ZERO;
    }

    return -1;
}

/**
 *
 *  \param list    squeeze or dont-squeeze list
 *  \param name    window name
 *  \param justify left, center, or right
 *  \param num     signed num
 *  \param denom   0 or indicates fraction denom
 */
void
do_squeeze_entry(name_list ** list, char *name, int justify, int num, int denom)
{
    int absnum = (num < 0 ? -num : num);

    if (denom < 0) {
        parseWarning("negative SqueezeTitle denominator %d", denom);
        return;
    }
    if (absnum > denom && denom != 0) {
        parseWarning("SqueezeTitle fraction %d/%d outside window", num, denom);
        return;
    }
    if (denom == 1) {
        parseWarning("useless SqueezeTitle faction %d/%d, assuming 0/0",
                     num, denom);
        num = 0;
        denom = 0;
    }

    if (HasShape) {
        SqueezeInfo *sinfo;

        sinfo = malloc(sizeof(SqueezeInfo));

        if (!sinfo) {
            parseWarning("unable to allocate %lu bytes for squeeze info",
                         (unsigned long) sizeof(SqueezeInfo));
            return;
        }
        sinfo->justify = justify;
        sinfo->num = num;
        sinfo->denom = denom;
        AddToList(list, name, (char *) sinfo);
    }
}

void
parseWarning(const char *format, ...)
{
    if (message_level > 0) {
        va_list ap;

        va_start(ap, format);
        if (twmInputFile != NULL) {
            fprintf(stderr, "%s: line %d: ", twmInputFile, yylineno);
        }
        else {
            fprintf(stderr, "%s[%s]: line %d: ", ProgramName, twmInputType,
                    yylineno);
        }
        vfprintf(stderr, format, ap);
        fputc('\n', stderr);
        va_end(ap);
    }
}
