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
 * $Xorg: gram.y,v 1.4 2001/02/09 02:05:36 xorgcvs Exp $
 *
 * .twmrc command grammar
 *
 * 07-Jan-86 Thomas E. LaStrange        File created
 * 11-Nov-90 Dave Sternlicht            Adding SaveColors
 * 10-Oct-90 David M. Sternlicht        Storing saved colors on root
 *
 * $XFree86: xc/programs/twm/gram.y,v 3.8 2002/10/16 21:33:03 tsi Exp $
 *
 ***********************************************************************/

%{

#include <stdio.h>
#include <ctype.h>
#include "twm.h"
#include "menus.h"
#include "list.h"
#include "util.h"
#include "screen.h"
#include "parse.h"
#include "add_window.h"
#include "icons.h"
#include <X11/Xos.h>
#include <X11/Xmu/CharSet.h>

static char empty[1];
static char *Action = empty;
static char *Name = empty;
static MenuRoot *root, *pull = NULL;

static MenuRoot *GetRoot(const char *name, const char *fore, const char *back);
static void GotButton(int butt, int func);
static void GotKey(char *key, int func);
static void GotTitleButton(char *bitmapname, int func, Bool rightside);
static Bool CheckWarpScreenArg(char *s);
static Bool CheckWarpRingArg(char *s);
static Bool CheckColormapArg(char *s);
static void RemoveDQuote(char *str);

static char *ptr;
static name_list **list;
static int cont = 0;
static int color;
int mods = 0;
unsigned int mods_used = (ShiftMask | ControlMask | Mod1Mask);

extern int yylineno;
static void yyerror(const char *s);

%}

%union
{
    int num;
    char *ptr;
};

%token <num> LB RB LP RP MENUS MENU BUTTON DEFAULT_FUNCTION PLUS MINUS
%token <num> ALL OR CURSORS PIXMAPS ICONS COLOR SAVECOLOR MONOCHROME FUNCTION
%token <num> ICONMGR_SHOW ICONMGR WINDOW_FUNCTION ZOOM ICONMGRS
%token <num> ICONMGR_GEOMETRY ICONMGR_NOSHOW MAKE_TITLE GRAYSCALE
%token <num> ICONIFY_BY_UNMAPPING DONT_ICONIFY_BY_UNMAPPING
%token <num> NO_TITLE AUTO_RAISE NO_HILITE ICON_REGION
%token <num> META SHIFT LOCK CONTROL WINDOW TITLE ICON ROOT FRAME
%token <num> COLON EQUALS SQUEEZE_TITLE DONT_SQUEEZE_TITLE
%token <num> START_ICONIFIED NO_TITLE_HILITE TITLE_HILITE
%token <num> MOVE RESIZE WAIT SELECT KILL LEFT_TITLEBUTTON RIGHT_TITLEBUTTON
%token <num> NUMBER KEYWORD NKEYWORD CKEYWORD CLKEYWORD FKEYWORD FSKEYWORD
%token <num> SKEYWORD DKEYWORD JKEYWORD WINDOW_RING WARP_CURSOR ERRORTOKEN
%token <num> NO_STACKMODE
%token <ptr> STRING
%token <num> error

%type <ptr> string
%type <num> pixmap_list cursor_list color_list save_color_list stmt
%type <num> win_color_list iconm_list win_list icon_list function menu
%type <num> noarg sarg narg squeeze color_entry
%type <num> action button number signed_number full fullkey

%start twmrc

%%
twmrc           : stmts
                ;

stmts           : /* Empty */
                | stmts stmt
                ;

stmt            : error
                | noarg
                | sarg
                | narg
                | squeeze
                | ICON_REGION string DKEYWORD DKEYWORD number number
                                        { AddIconRegion($2, $3, $4, $5, $6); }
                | ICONMGR_GEOMETRY string number        { if (Scr->FirstTime)
                                                  {
                                                    Scr->iconmgr.geometry=$2;
                                                    Scr->iconmgr.columns=$3;
                                                  }
                                                }
                | ICONMGR_GEOMETRY string       { if (Scr->FirstTime)
                                                    Scr->iconmgr.geometry = $2;
                                                }
                | ZOOM number           { if (Scr->FirstTime)
                                          {
                                                Scr->DoZoom = TRUE;
                                                Scr->ZoomCount = (short)$2;
                                          }
                                        }
                | ZOOM                  { if (Scr->FirstTime)
                                                Scr->DoZoom = TRUE; }
                | PIXMAPS pixmap_list   {}
                | CURSORS cursor_list   {}
                | ICONIFY_BY_UNMAPPING  { list = &Scr->IconifyByUn; }
                  win_list
                | ICONIFY_BY_UNMAPPING  { if (Scr->FirstTime)
                    Scr->IconifyByUnmapping = TRUE; }
                | LEFT_TITLEBUTTON string EQUALS action {
                                          GotTitleButton ($2, $4, False);
                                        }
                | RIGHT_TITLEBUTTON string EQUALS action {
                                          GotTitleButton ($2, $4, True);
                                        }
                | button string         { root = GetRoot($2, NULLSTR, NULLSTR);
                                          Scr->Mouse[$1][C_ROOT][0].func = F_MENU;
                                          Scr->Mouse[$1][C_ROOT][0].menu = root;
                                        }
                | button action         { Scr->Mouse[$1][C_ROOT][0].func = $2;
                                          if ($2 == F_MENU)
                                          {
                                            pull->prev = NULL;
                                            Scr->Mouse[$1][C_ROOT][0].menu = pull;
                                          }
                                          else
                                          {
                                            root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
                                            Scr->Mouse[$1][C_ROOT][0].item =
                                                AddToMenu(root,"x",Action,
                                                          NULL,$2,NULLSTR,NULLSTR);
                                          }
                                          Action = empty;
                                          pull = NULL;
                                        }
                | string fullkey        { GotKey($1, $2); }
                | button full           { GotButton($1, $2); }
                | DONT_ICONIFY_BY_UNMAPPING { list = &Scr->DontIconify; }
                  win_list
                | ICONMGR_NOSHOW        { list = &Scr->IconMgrNoShow; }
                  win_list
                | ICONMGR_NOSHOW        { Scr->IconManagerDontShow = TRUE; }
                | ICONMGRS              { list = &Scr->IconMgrs; }
                  iconm_list
                | ICONMGR_SHOW          { list = &Scr->IconMgrShow; }
                  win_list
                | NO_TITLE_HILITE       { list = &Scr->NoTitleHighlight; }
                  win_list
                | NO_TITLE_HILITE       { if (Scr->FirstTime)
                                                Scr->TitleHighlight = FALSE; }
                | NO_HILITE             { list = &Scr->NoHighlight; }
                  win_list
                | NO_HILITE             { if (Scr->FirstTime)
                                                Scr->Highlight = FALSE; }
                | NO_STACKMODE          { list = &Scr->NoStackModeL; }
                  win_list
                | NO_STACKMODE          { if (Scr->FirstTime)
                                                Scr->StackMode = FALSE; }
                | NO_TITLE              { list = &Scr->NoTitle; }
                  win_list
                | NO_TITLE              { if (Scr->FirstTime)
                                                Scr->NoTitlebar = TRUE; }
                | MAKE_TITLE            { list = &Scr->MakeTitle; }
                  win_list
                | START_ICONIFIED       { list = &Scr->StartIconified; }
                  win_list
                | AUTO_RAISE            { list = &Scr->AutoRaise; }
                  win_list
                | MENU string LP string COLON string RP {
                                        root = GetRoot($2, $4, $6); }
                  menu                  { root->real_menu = TRUE;}
                | MENU string           { root = GetRoot($2, NULLSTR, NULLSTR); }
                  menu                  { root->real_menu = TRUE; }
                | FUNCTION string       { root = GetRoot($2, NULLSTR, NULLSTR); }
                  function
                | ICONS                 { list = &Scr->IconNames; }
                  icon_list
                | COLOR                 { color = COLOR; }
                  color_list
                | GRAYSCALE             { color = GRAYSCALE; }
                  color_list
                | SAVECOLOR
                  save_color_list
                | MONOCHROME            { color = MONOCHROME; }
                  color_list
                | DEFAULT_FUNCTION action { Scr->DefaultFunction.func = $2;
                                          if ($2 == F_MENU)
                                          {
                                            pull->prev = NULL;
                                            Scr->DefaultFunction.menu = pull;
                                          }
                                          else
                                          {
                                            root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
                                            Scr->DefaultFunction.item =
                                                AddToMenu(root,"x",Action,
                                                          NULL,$2, NULLSTR, NULLSTR);
                                          }
                                          Action = empty;
                                          pull = NULL;
                                        }
                | WINDOW_FUNCTION action { Scr->WindowFunction.func = $2;
                                           root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
                                           Scr->WindowFunction.item =
                                                AddToMenu(root,"x",Action,
                                                          NULL,$2, NULLSTR, NULLSTR);
                                           Action = empty;
                                           pull = NULL;
                                        }
                | WARP_CURSOR           { list = &Scr->WarpCursorL; }
                  win_list
                | WARP_CURSOR           { if (Scr->FirstTime)
                                            Scr->WarpCursor = TRUE; }
                | WINDOW_RING           { list = &Scr->WindowRingL; }
                  win_list
                ;


noarg           : KEYWORD               { if (!do_single_keyword ($1)) {
                                            parseWarning (
                                                 "unknown singleton keyword %d",
                                                 $1);
                                            ParseError = 1;
                                          }
                                        }
                ;

sarg            : SKEYWORD string       { if (!do_string_keyword ($1, $2)) {
                                            parseWarning (
                                                 "unknown string keyword %d (value \"%s\")",
                                                 $1, $2);
                                            ParseError = 1;
                                          }
                                        }
                ;

narg            : NKEYWORD number       { if (!do_number_keyword ($1, $2)) {
                                            parseWarning (
                                                 "unknown numeric keyword %d (value %d)",
                                                 $1, $2);
                                            ParseError = 1;
                                          }
                                        }
                ;



full            : EQUALS keys COLON contexts COLON action  { $$ = $6; }
                ;

fullkey         : EQUALS keys COLON contextkeys COLON action  { $$ = $6; }
                ;

keys            : /* Empty */
                | keys key
                ;

key             : META                  { mods |= Mod1Mask; }
                | SHIFT                 { mods |= ShiftMask; }
                | LOCK                  { mods |= LockMask; }
                | CONTROL               { mods |= ControlMask; }
                | META number           { if ($2 < 1 || $2 > 5) {
                                             parseWarning (
                                                  "bad modifier number (%d), must be 1-5",
                                                  $2);
                                             ParseError = 1;
                                          } else {
                                             mods |= (Mod1Mask << ($2 - 1));
                                          }
                                        }
                | OR                    { }
                ;

contexts        : /* Empty */
                | contexts context
                ;

context         : WINDOW                { cont |= C_WINDOW_BIT; }
                | TITLE                 { cont |= C_TITLE_BIT; }
                | ICON                  { cont |= C_ICON_BIT; }
                | ROOT                  { cont |= C_ROOT_BIT; }
                | FRAME                 { cont |= C_FRAME_BIT; }
                | ICONMGR               { cont |= C_ICONMGR_BIT; }
                | META                  { cont |= C_ICONMGR_BIT; }
                | ALL                   { cont |= C_ALL_BITS; }
                | OR                    {  }
                ;

contextkeys     : /* Empty */
                | contextkeys contextkey
                ;

contextkey      : WINDOW                { cont |= C_WINDOW_BIT; }
                | TITLE                 { cont |= C_TITLE_BIT; }
                | ICON                  { cont |= C_ICON_BIT; }
                | ROOT                  { cont |= C_ROOT_BIT; }
                | FRAME                 { cont |= C_FRAME_BIT; }
                | ICONMGR               { cont |= C_ICONMGR_BIT; }
                | META                  { cont |= C_ICONMGR_BIT; }
                | ALL                   { cont |= C_ALL_BITS; }
                | OR                    { }
                | string                { Name = $1; cont |= C_NAME_BIT; }
                ;


pixmap_list     : LB pixmap_entries RB
                ;

pixmap_entries  : /* Empty */
                | pixmap_entries pixmap_entry
                ;

pixmap_entry    : TITLE_HILITE string { SetHighlightPixmap ($2); }
                ;


cursor_list     : LB cursor_entries RB
                ;

cursor_entries  : /* Empty */
                | cursor_entries cursor_entry
                ;

cursor_entry    : FRAME string string {
                        NewBitmapCursor(&Scr->FrameCursor, $2, $3); }
                | FRAME string  {
                        NewFontCursor(&Scr->FrameCursor, $2); }
                | TITLE string string {
                        NewBitmapCursor(&Scr->TitleCursor, $2, $3); }
                | TITLE string {
                        NewFontCursor(&Scr->TitleCursor, $2); }
                | ICON string string {
                        NewBitmapCursor(&Scr->IconCursor, $2, $3); }
                | ICON string {
                        NewFontCursor(&Scr->IconCursor, $2); }
                | ICONMGR string string {
                        NewBitmapCursor(&Scr->IconMgrCursor, $2, $3); }
                | ICONMGR string {
                        NewFontCursor(&Scr->IconMgrCursor, $2); }
                | BUTTON string string {
                        NewBitmapCursor(&Scr->ButtonCursor, $2, $3); }
                | BUTTON string {
                        NewFontCursor(&Scr->ButtonCursor, $2); }
                | MOVE string string {
                        NewBitmapCursor(&Scr->MoveCursor, $2, $3); }
                | MOVE string {
                        NewFontCursor(&Scr->MoveCursor, $2); }
                | RESIZE string string {
                        NewBitmapCursor(&Scr->ResizeCursor, $2, $3); }
                | RESIZE string {
                        NewFontCursor(&Scr->ResizeCursor, $2); }
                | WAIT string string {
                        NewBitmapCursor(&Scr->WaitCursor, $2, $3); }
                | WAIT string {
                        NewFontCursor(&Scr->WaitCursor, $2); }
                | MENU string string {
                        NewBitmapCursor(&Scr->MenuCursor, $2, $3); }
                | MENU string {
                        NewFontCursor(&Scr->MenuCursor, $2); }
                | SELECT string string {
                        NewBitmapCursor(&Scr->SelectCursor, $2, $3); }
                | SELECT string {
                        NewFontCursor(&Scr->SelectCursor, $2); }
                | KILL string string {
                        NewBitmapCursor(&Scr->DestroyCursor, $2, $3); }
                | KILL string {
                        NewFontCursor(&Scr->DestroyCursor, $2); }
                ;

color_list      : LB color_entries RB
                ;


color_entries   : /* Empty */
                | color_entries color_entry
                ;

color_entry     : CLKEYWORD string      { if (!do_colorlist_keyword ($1, color,
                                                                     $2)) {
                                            parseWarning (
                                                 "unhandled list color keyword %d (string \"%s\")",
                                                 $1, $2);
                                            ParseError = 1;
                                          }
                                        }
                | CLKEYWORD string      { list = do_colorlist_keyword($1,color,
                                                                      $2);
                                          if (!list) {
                                            parseWarning (
                                                 "unhandled color list keyword %d (string \"%s\")",
                                                 $1, $2);
                                            ParseError = 1;
                                          }
                                        }
                  win_color_list        { /* No action */; }
                | CKEYWORD string       { if (!do_color_keyword ($1, color,
                                                                 $2)) {
                                            parseWarning (
                                                 "unhandled color keyword %d (string \"%s\")",
                                                 $1, $2);
                                            ParseError = 1;
                                          }
                                        }
                ;

save_color_list : LB s_color_entries RB
                ;

s_color_entries : /* Empty */
                | s_color_entries s_color_entry
                ;

s_color_entry   : string            { do_string_savecolor(color, $1); }
                | CLKEYWORD         { do_var_savecolor($1); }
                ;

win_color_list  : LB win_color_entries RB
                ;

win_color_entries       : /* Empty */
                | win_color_entries win_color_entry
                ;

win_color_entry : string string         { if (Scr->FirstTime &&
                                              color == Scr->Monochrome)
                                            AddToList(list, $1, $2); }
                ;

squeeze         : SQUEEZE_TITLE {
                                    if (HasShape) Scr->SqueezeTitle = TRUE;
                                }
                | SQUEEZE_TITLE { list = &Scr->SqueezeTitleL;
                                  if (HasShape && Scr->SqueezeTitle == -1)
                                    Scr->SqueezeTitle = TRUE;
                                }
                  LB win_sqz_entries RB
                | DONT_SQUEEZE_TITLE { Scr->SqueezeTitle = FALSE; }
                | DONT_SQUEEZE_TITLE { list = &Scr->DontSqueezeTitleL; }
                  win_list
                ;

win_sqz_entries : /* Empty */
                | win_sqz_entries string JKEYWORD signed_number number  {
                                if (Scr->FirstTime) {
                                   do_squeeze_entry (list, $2, $3, $4, $5);
                                }
                        }
                ;


iconm_list      : LB iconm_entries RB
                ;

iconm_entries   : /* Empty */
                | iconm_entries iconm_entry
                ;

iconm_entry     : string string number  { if (Scr->FirstTime)
                                            AddToList(list, $1, (char *)
                                                AllocateIconManager($1, NULLSTR,
                                                        $2,$3));
                                        }
                | string string string number
                                        { if (Scr->FirstTime)
                                            AddToList(list, $1, (char *)
                                                AllocateIconManager($1,$2,
                                                $3, $4));
                                        }
                ;

win_list        : LB win_entries RB
                ;

win_entries     : /* Empty */
                | win_entries win_entry
                ;

win_entry       : string                { if (Scr->FirstTime)
                                            AddToList(list, $1, 0);
                                        }
                ;

icon_list       : LB icon_entries RB
                ;

icon_entries    : /* Empty */
                | icon_entries icon_entry
                ;

icon_entry      : string string         { if (Scr->FirstTime) AddToList(list, $1, $2); }
                ;

function        : LB function_entries RB
                ;

function_entries: /* Empty */
                | function_entries function_entry
                ;

function_entry  : action                { AddToMenu(root, empty, Action, NULL, $1,
                                                NULLSTR, NULLSTR);
                                          Action = empty;
                                        }
                ;

menu            : LB menu_entries RB
                ;

menu_entries    : /* Empty */
                | menu_entries menu_entry
                ;

menu_entry      : string action         { AddToMenu(root, $1, Action, pull, $2,
                                                NULLSTR, NULLSTR);
                                          Action = empty;
                                          pull = NULL;
                                        }
                | string LP string COLON string RP action {
                                          AddToMenu(root, $1, Action, pull, $7,
                                                $3, $5);
                                          Action = empty;
                                          pull = NULL;
                                        }
                ;

action          : FKEYWORD      { $$ = $1; }
                | FSKEYWORD string {
                                $$ = $1;
                                Action = $2;
                                switch ($1) {
                                  case F_MENU:
                                    pull = GetRoot ($2, NULLSTR,NULLSTR);
                                    pull->prev = root;
                                    break;
                                  case F_WARPRING:
                                    if (!CheckWarpRingArg (Action)) {
                                        parseWarning (
                                             "ignoring invalid f.warptoring argument \"%s\"",
                                             Action);
                                        $$ = F_NOP;
                                    }
                                    break;
                                  case F_WARPTOSCREEN:
                                    if (!CheckWarpScreenArg (Action)) {
                                        parseWarning (
                                             "ignoring invalid f.warptoscreen argument \"%s\"",
                                             Action);
                                        $$ = F_NOP;
                                    }
                                    break;
                                  case F_COLORMAP:
                                    if (CheckColormapArg (Action)) {
                                        $$ = F_COLORMAP;
                                    } else {
                                        parseWarning (
                                             "ignoring invalid f.colormap argument \"%s\"",
                                             Action);
                                        $$ = F_NOP;
                                    }
                                    break;
                                } /* end switch */
                                   }
                ;


signed_number   : number                { $$ = $1; }
                | PLUS number           { $$ = $2; }
                | MINUS number          { $$ = -($2); }
                ;

button          : BUTTON number         { $$ = $2;
                                          if ($2 == 0)
                                                yyerror("bad button 0");

                                          if ($2 > MAX_BUTTONS)
                                          {
                                                $$ = 0;
                                                yyerror("button number too large");
                                          }
                                        }
                ;

string          : STRING                { ptr = strdup($1);
                                          RemoveDQuote(ptr);
                                          $$ = ptr;
                                        }
                ;
number          : NUMBER                { $$ = $1; }
                ;

%%

static void
yyerror(const char *s)
{
    parseWarning("error in input file:  %s", s ? s : "");
    ParseError = 1;
}

static void
RemoveDQuote(char *str)
{
    register char *i, *o;
    register int n;
    register int count;

    for (i = str + 1, o = str; *i && *i != '\"'; o++) {
        if (*i == '\\') {
            switch (*++i) {
            case 'n':
                *o = '\n';
                i++;
                break;
            case 'b':
                *o = '\b';
                i++;
                break;
            case 'r':
                *o = '\r';
                i++;
                break;
            case 't':
                *o = '\t';
                i++;
                break;
            case 'f':
                *o = '\f';
                i++;
                break;
            case '0':
                if (*++i == 'x')
                    goto hex;
                else
                    --i;
                /* FALLTHRU */
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                n = 0;
                count = 0;
                while (*i >= '0' && *i <= '7' && count < 3) {
                    n = (n << 3) + (*i++ - '0');
                    count++;
                }
                *o = (char) n;
                break;
              hex:
            case 'x':
                n = 0;
                count = 0;
                while (i++, count++ < 2) {
                    if (*i >= '0' && *i <= '9')
                        n = (n << 4) + (*i - '0');
                    else if (*i >= 'a' && *i <= 'f')
                        n = (n << 4) + (*i - 'a') + 10;
                    else if (*i >= 'A' && *i <= 'F')
                        n = (n << 4) + (*i - 'A') + 10;
                    else
                        break;
                }
                *o = (char) n;
                break;
            case '\n':
                i++;            /* punt */
                o--;            /* to account for o++ at end of loop */
                break;
            case '\"':
            case '\'':
            case '\\':
            default:
                *o = *i++;
                break;
            }
        }
        else
            *o = *i++;
    }
    *o = '\0';
}

static MenuRoot *
GetRoot(const char *name, const char *fore, const char *back)
{
    MenuRoot *tmp;

    tmp = FindMenuRoot(name);
    if (tmp == NULL)
        tmp = NewMenuRoot(name);

    if (fore) {
        int save;

        save = Scr->FirstTime;
        Scr->FirstTime = TRUE;
        GetColor(COLOR, &tmp->hi_fore, fore);
        GetColor(COLOR, &tmp->hi_back, back);
        Scr->FirstTime = (short) save;
    }

    return tmp;
}

static void
GotButton(int butt, int func)
{
    int i;

    for (i = 0; i < NUM_CONTEXTS; i++) {
        if ((cont & (1 << i)) == 0)
            continue;

        Scr->Mouse[butt][i][mods].func = func;
        if (func == F_MENU) {
            pull->prev = NULL;
            Scr->Mouse[butt][i][mods].menu = pull;
        }
        else {
            root = GetRoot(TWM_ROOT, NULLSTR, NULLSTR);
            Scr->Mouse[butt][i][mods].item = AddToMenu(root, "x", Action,
                                                       NULL, func, NULLSTR, NULLSTR);
        }
    }
    Action = empty;
    pull = NULL;
    cont = 0;
    mods_used |= (unsigned) mods;
    mods = 0;
}

static void
GotKey(char *key, int func)
{
    int i;

    for (i = 0; i < NUM_CONTEXTS; i++) {
        if ((cont & (1 << i)) == 0)
            continue;
        if (!AddFuncKey(key, i, mods, func, Name, Action))
            break;
    }

    Action = empty;
    pull = NULL;
    cont = 0;
    mods_used |= (unsigned) mods;
    mods = 0;
}

static void
GotTitleButton(char *bitmapname, int func, Bool rightside)
{
    if (!CreateTitleButton(bitmapname, func, Action, pull, rightside, True)) {
        parseWarning("unable to create %s titlebutton \"%s\"",
                     rightside ? "right" : "left", bitmapname);
    }
    Action = empty;
    pull = NULL;
}

static Bool
CheckWarpScreenArg(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, WARPSCREEN_NEXT) == 0 ||
        strcmp(s, WARPSCREEN_PREV) == 0 ||
        strcmp(s, WARPSCREEN_BACK) == 0)
        return True;

    for (; *s && isascii(*s) && isdigit(*s); s++);      /* SUPPRESS 530 */
    return (*s ? False : True);
}

static Bool
CheckWarpRingArg(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, WARPSCREEN_NEXT) == 0 ||
        strcmp(s, WARPSCREEN_PREV) == 0)
        return True;

    return False;
}

static Bool
CheckColormapArg(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, COLORMAP_NEXT) == 0 ||
        strcmp(s, COLORMAP_PREV) == 0 ||
        strcmp(s, COLORMAP_DEFAULT) == 0)
        return True;

    return False;
}
