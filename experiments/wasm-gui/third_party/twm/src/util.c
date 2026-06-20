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
 * utility routines for twm
 *
 * 28-Oct-87 Thomas E. LaStrange        File created
 *
 ***********************************************************************/

#include "twm.h"
#include "util.h"
#include "gram.h"
#include "screen.h"
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <X11/Xmu/Drawing.h>
#include <X11/Xmu/CharSet.h>

static Pixmap CreateXLogoPixmap(unsigned int *widthp, unsigned int *heightp);
static Pixmap CreateResizePixmap(unsigned int *widthp, unsigned int *heightp);
static Pixmap CreateDotPixmap(unsigned int *widthp, unsigned int *heightp);
static Pixmap CreateQuestionPixmap(unsigned int *widthp, unsigned int *heightp);
static Pixmap CreateMenuPixmap(unsigned int *widthp, unsigned int *heightp);

int HotX, HotY;

/**
 * move a window outline
 *
 *  \param root         window we are outlining
 *  \param x,y          upper left coordinate
 *  \param width,height size of the rectangle
 *  \param bw           border width of the frame
 *  \param th           title height
 */
void
MoveOutline(Window root, int x, int y, int width, int height, int bw, int th)
{
    static int lastx = 0;
    static int lasty = 0;
    static int lastWidth = 0;
    static int lastHeight = 0;
    static int lastBW = 0;
    static int lastTH = 0;
    int xl, xr, yt, yb, xinnerl, xinnerr, yinnert, yinnerb;
    int xthird, ythird;
    XSegment outline[18];
    register XSegment *r;

    if (x == lastx && y == lasty && width == lastWidth && height == lastHeight
        && lastBW == bw && th == lastTH)
        return;

    r = outline;

#define DRAWIT() \
    if (lastWidth || lastHeight)                        \
    {                                                   \
        xl = lastx;                                     \
        xr = lastx + lastWidth - 1;                     \
        yt = lasty;                                     \
        yb = lasty + lastHeight - 1;                    \
        xinnerl = xl + lastBW;                          \
        xinnerr = xr - lastBW;                          \
        yinnert = yt + lastTH + lastBW;                 \
        yinnerb = yb - lastBW;                          \
        xthird = (xinnerr - xinnerl) / 3;               \
        ythird = (yinnerb - yinnert) / 3;               \
                                                        \
        r->x1 = (short)(xl);                            \
        r->y1 = (short)(yt);                            \
        r->x2 = (short)(xr);                            \
        r->y2 = (short)(yt);                            \
        r++;                                            \
                                                        \
        r->x1 = (short)(xl);                            \
        r->y1 = (short)(yb);                            \
        r->x2 = (short)(xr);                            \
        r->y2 = (short)(yb);                            \
        r++;                                            \
                                                        \
        r->x1 = (short)(xl);                            \
        r->y1 = (short)(yt);                            \
        r->x2 = (short)(xl);                            \
        r->y2 = (short)(yb);                            \
        r++;                                            \
                                                        \
        r->x1 = (short)(xr);                            \
        r->y1 = (short)(yt);                            \
        r->x2 = (short)(xr);                            \
        r->y2 = (short)(yb);                            \
        r++;                                            \
                                                        \
        r->x1 = (short)(xinnerl + xthird);              \
        r->y1 = (short)(yinnert);                       \
        r->x2 = (short)(r->x1);                         \
        r->y2 = (short)(yinnerb);                       \
        r++;                                            \
                                                        \
        r->x1 = (short)(xinnerl + (2 * xthird));        \
        r->y1 = (short)(yinnert);                       \
        r->x2 = (short)(r->x1);                         \
        r->y2 = (short)(yinnerb);                       \
        r++;                                            \
                                                        \
        r->x1 = (short)(xinnerl);                       \
        r->y1 = (short)(yinnert + ythird);              \
        r->x2 = (short)(xinnerr);                       \
        r->y2 = (short)(r->y1);                         \
        r++;                                            \
                                                        \
        r->x1 = (short)(xinnerl);                       \
        r->y1 = (short)(yinnert + (2 * ythird));        \
        r->x2 = (short)(xinnerr);                       \
        r->y2 = (short)(r->y1);                         \
        r++;                                            \
                                                        \
        if (lastTH != 0) {                              \
            r->x1 = (short)(xl);                        \
            r->y1 = (short)(yt + lastTH);               \
            r->x2 = (short)(xr);                        \
            r->y2 = (short)(r->y1);                     \
            r++;                                        \
        }                                               \
    }

    /* undraw the old one, if any */
    DRAWIT();

    lastx = x;
    lasty = y;
    lastWidth = width;
    lastHeight = height;
    lastBW = bw;
    lastTH = th;

    /* draw the new one, if any */
    DRAWIT();

#undef DRAWIT

    if (r != outline) {
        XDrawSegments(dpy, root, Scr->DrawGC, outline, (int) (r - outline));
    }
}

/**
 * zoom in or out of an icon
 *
 *  \param wf window to zoom from
 *  \param wt window to zoom to
 */
void
Zoom(Window wf, Window wt)
{
    int fx, fy, tx, ty;         /* from, to */
    unsigned int fw, fh, tw, th;        /* from, to */
    long dx, dy, dw, dh;
    long z;
    int j;

    if (!Scr->DoZoom || Scr->ZoomCount < 1)
        return;

    if (wf == None || wt == None)
        return;

    XGetGeometry(dpy, wf, &JunkRoot, &fx, &fy, &fw, &fh, &JunkBW, &JunkDepth);
    XGetGeometry(dpy, wt, &JunkRoot, &tx, &ty, &tw, &th, &JunkBW, &JunkDepth);

    dx = ((long) (tx - fx));    /* going from -> to */
    dy = ((long) (ty - fy));    /* going from -> to */
    dw = ((long) (tw - fw));    /* going from -> to */
    dh = ((long) (th - fh));    /* going from -> to */
    z = (long) (Scr->ZoomCount + 1);

    for (j = 0; j < 2; j++) {
        long i;

        XDrawRectangle(dpy, Scr->Root, Scr->DrawGC, fx, fy, fw, fh);
        for (i = 1; i < z; i++) {
            int x = fx + (int) ((dx * i) / z);
            int y = fy + (int) ((dy * i) / z);
            unsigned width = (unsigned) (((long) fw) + (dw * i) / z);
            unsigned height = (unsigned) (((long) fh) + (dh * i) / z);

            XDrawRectangle(dpy, Scr->Root, Scr->DrawGC, x, y, width, height);
        }
        XDrawRectangle(dpy, Scr->Root, Scr->DrawGC, tx, ty, tw, th);
    }
}

/**
 * expand the tilde character to HOME if it is the first
 * character of the filename
 *
 *      \return a pointer to the new name
 *
 *  \param name  the filename to expand
 */
char *
ExpandFilename(const char *name)
{
    char *newname;

    if (name[0] != '~')
        return strdup(name);

    newname = malloc((size_t) HomeLen + strlen(name) + 2);
    if (!newname) {
        twmWarning("unable to allocate %lu bytes to expand filename %s/%s",
                   (unsigned long) HomeLen + (unsigned long) strlen(name) + 2,
                   Home, &name[1]);
    }
    else {
        (void) sprintf(newname, "%s/%s", Home, &name[1]);
    }

    return newname;
}

/**
 * read in the bitmap file for the unknown icon
 *
 * \param name  the filename to read
 */
void
GetUnknownIcon(const char *name)
{
    if ((Scr->UnknownPm = GetBitmap(name)) != None) {
        XGetGeometry(dpy, Scr->UnknownPm, &JunkRoot, &JunkX, &JunkY,
                     (unsigned int *) &Scr->UnknownWidth,
                     (unsigned int *) &Scr->UnknownHeight, &JunkBW, &JunkDepth);
    }
}

/**
 *      FindBitmap - read in a bitmap file and return size
 *
 *  \return pixmap associated with bitmap
 *
 *  \param name          filename to read
 *  \param[out] widthp   pointer to width of bitmap
 *  \param[out] heightp  pointer to height of bitmap
 */
Pixmap
FindBitmap(const char *name, unsigned *widthp, unsigned *heightp)
{
    char *bigname;
    Pixmap pm;

    if (!name)
        return None;

    /*
     * Names of the form :name refer to hardcoded images that are scaled to
     * look nice in title buttons.  Eventually, it would be nice to put in a
     * menu symbol as well....
     */
    if (name[0] == ':') {
        int i;
	/* *INDENT-OFF* */
        static struct {
            const char *name;
            Pixmap (*proc)(unsigned int *, unsigned int *);
        } pmtab[] = {
            { TBPM_DOT,         CreateDotPixmap },
            { TBPM_ICONIFY,     CreateDotPixmap },
            { TBPM_RESIZE,      CreateResizePixmap },
            { TBPM_XLOGO,       CreateXLogoPixmap },
            { TBPM_DELETE,      CreateXLogoPixmap },
            { TBPM_MENU,        CreateMenuPixmap },
            { TBPM_QUESTION,    CreateQuestionPixmap },
        };
	/* *INDENT-ON* */

        for (i = 0; (size_t) i < (sizeof pmtab) / (sizeof pmtab[0]); i++) {
            if (XmuCompareISOLatin1(pmtab[i].name, name) == 0)
                return (*pmtab[i].proc) (widthp, heightp);
        }
        twmWarning("no such built-in bitmap \"%s\"", name);
        return None;
    }

    /*
     * Generate a full pathname if any special prefix characters (such as ~)
     * are used.  If the bigname is different from name, bigname will need to
     * be freed.
     */
    bigname = ExpandFilename(name);
    if (!bigname)
        return None;

    /*
     * look along bitmapFilePath resource same as toolkit clients
     */
    pm = XmuLocateBitmapFile(ScreenOfDisplay(dpy, Scr->screen), bigname, NULL,
                             0, (int *) widthp, (int *) heightp, &HotX, &HotY);
    if (pm == None && Scr->IconDirectory && bigname[0] != '/') {
        free(bigname);
        /*
         * Attempt to find icon in old IconDirectory (now obsolete)
         */
        bigname = malloc(strlen(name) + strlen(Scr->IconDirectory) + 2);
        if (!bigname) {
            twmWarning("unable to allocate memory for \"%s/%s\"",
                       Scr->IconDirectory, name);
            return None;
        }
        (void) sprintf(bigname, "%s/%s", Scr->IconDirectory, name);
        if (XReadBitmapFile(dpy, Scr->Root, bigname, widthp, heightp, &pm,
                            &HotX, &HotY) != BitmapSuccess) {
            pm = None;
        }
    }
    free(bigname);
    if (pm == None) {
        twmWarning("unable to find bitmap \"%s\"", name);
    }

    return pm;
}

Pixmap
GetBitmap(const char *name)
{
    return FindBitmap(name, &JunkWidth, &JunkHeight);
}

void
InsertRGBColormap(Atom a, XStandardColormap *maps, int nmaps, Bool replace)
{
    StdCmap *sc = NULL;

    if (replace) {              /* locate existing entry */
        for (sc = Scr->StdCmapInfo.head; sc; sc = sc->next) {
            if (sc->atom == a)
                break;
        }
    }

    if (!sc) {                  /* no existing, allocate new */
        sc = malloc(sizeof(StdCmap));
        if (!sc) {
            twmWarning("unable to allocate %lu bytes for StdCmap",
                       (unsigned long) sizeof(StdCmap));
            return;
        }
        replace = False;
    }

    if (replace) {              /* just update contents */
        if (sc->maps)
            XFree(sc->maps);
        if (sc == Scr->StdCmapInfo.mru)
            Scr->StdCmapInfo.mru = NULL;
    }
    else {                      /* else appending */
        sc->next = NULL;
        sc->atom = a;
        if (Scr->StdCmapInfo.tail) {
            Scr->StdCmapInfo.tail->next = sc;
        }
        else {
            Scr->StdCmapInfo.head = sc;
        }
        Scr->StdCmapInfo.tail = sc;
    }
    sc->nmaps = nmaps;
    sc->maps = maps;

    return;
}

void
RemoveRGBColormap(Atom a)
{
    StdCmap *sc, *prev;

    prev = NULL;
    for (sc = Scr->StdCmapInfo.head; sc; sc = sc->next) {
        if (sc->atom == a)
            break;
        prev = sc;
    }
    if (sc) {                   /* found one */
        if (sc->maps)
            XFree(sc->maps);
        if (prev)
            prev->next = sc->next;
        if (Scr->StdCmapInfo.head == sc)
            Scr->StdCmapInfo.head = sc->next;
        if (Scr->StdCmapInfo.tail == sc)
            Scr->StdCmapInfo.tail = prev;
        if (Scr->StdCmapInfo.mru == sc)
            Scr->StdCmapInfo.mru = NULL;
    }
    return;
}

void
LocateStandardColormaps(void)
{
    Atom *atoms;
    int natoms;
    int i;

    atoms = XListProperties(dpy, Scr->Root, &natoms);
    for (i = 0; i < natoms; i++) {
        XStandardColormap *maps = NULL;
        int nmaps;

        if (XGetRGBColormaps(dpy, Scr->Root, &maps, &nmaps, atoms[i])) {
            /* if got one, then append to current list */
            InsertRGBColormap(atoms[i], maps, nmaps, False);
        }
    }
    if (atoms)
        XFree(atoms);
    return;
}

void
GetColor(int kind, Pixel *what, const char *name)
{
    XColor color, junkcolor;
    Status stat = 0;
    Colormap cmap = Scr->TwmRoot.cmaps.cwins[0]->colormap->c;

#ifndef TOM
    if (!Scr->FirstTime)
        return;
#endif

    if (Scr->Monochrome != kind)
        return;

    if (!XAllocNamedColor(dpy, cmap, name, &color, &junkcolor)) {
        /* if we could not allocate the color, let's see if this is a
         * standard colormap
         */
        XStandardColormap *stdcmap = NULL;

        /* parse the named color */
        if (name[0] != '#')
            stat = XParseColor(dpy, cmap, name, &color);
        if (!stat) {
            twmWarning("invalid color name \"%s\"", name);
            return;
        }

        /*
         * look through the list of standard colormaps (check cache first)
         */
        if (Scr->StdCmapInfo.mru && Scr->StdCmapInfo.mru->maps &&
            (Scr->StdCmapInfo.mru->maps[Scr->StdCmapInfo.mruindex].colormap ==
             cmap)) {
            stdcmap = &(Scr->StdCmapInfo.mru->maps[Scr->StdCmapInfo.mruindex]);
        }
        else {
            StdCmap *sc;

            for (sc = Scr->StdCmapInfo.head; sc; sc = sc->next) {
                int i;

                for (i = 0; i < sc->nmaps; i++) {
                    if (sc->maps[i].colormap == cmap) {
                        Scr->StdCmapInfo.mru = sc;
                        Scr->StdCmapInfo.mruindex = i;
                        stdcmap = &(sc->maps[i]);
                        goto gotit;
                    }
                }
            }
        }

 gotit:
        if (stdcmap) {
            color.pixel = (stdcmap->base_pixel +
                           ((Pixel) (((float) color.red / 65535.0) *
                                     (double) stdcmap->red_max + 0.5) *
                            stdcmap->red_mult) +
                           ((Pixel) (((float) color.green / 65535.0) *
                                     (double) stdcmap->green_max + 0.5) *
                            stdcmap->green_mult) +
                           ((Pixel) (((float) color.blue / 65535.0) *
                                     (double) stdcmap->blue_max + 0.5) *
                            stdcmap->blue_mult));
        }
        else {
            twmWarning("unable to allocate color \"%s\"", name);
            return;
        }
    }

    *what = color.pixel;
}

void
GetColorValue(int kind, XColor *what, const char *name)
{
    XColor junkcolor;
    Colormap cmap = Scr->TwmRoot.cmaps.cwins[0]->colormap->c;

#ifndef TOM
    if (!Scr->FirstTime)
        return;
#endif

    if (Scr->Monochrome != kind)
        return;

    if (!XLookupColor(dpy, cmap, name, what, &junkcolor)) {
        twmWarning("invalid color name \"%s\"", name);
    }
    else {
        what->pixel = AllPlanes;
    }
}

static Boolean
FindFontSet(MyFont *font, const char *fontname)
{
    char **missing_charset_list_return;
    int missing_charset_count_return;
    char *def_string_return;
    XFontSetExtents *font_extents;
    XFontStruct **xfonts;
    char **font_names;

    if (use_fontset) {
        int ascent;
        int descent;
        int fnum;
        register int i;

        if (font->fontset != NULL) {
            XFreeFontSet(dpy, font->fontset);
        }

        if ((font->fontset = XCreateFontSet(dpy, fontname,
                                            &missing_charset_list_return,
                                            &missing_charset_count_return,
                                            &def_string_return)) == NULL) {
            return False;
        }
        if (missing_charset_count_return) {
            twmVerbose("%d fonts are missing from fontset",
                       missing_charset_count_return);
            for (i = 0; i < missing_charset_count_return; i++) {
                twmVerbose("font for charset %s is lacking.",
                           missing_charset_list_return[i]);
            }
        }

        font_extents = XExtentsOfFontSet(font->fontset);
        fnum = XFontsOfFontSet(font->fontset, &xfonts, &font_names);
        for (i = 0, ascent = 0, descent = 0; i < fnum; i++) {
            if (ascent < (*xfonts)->ascent)
                ascent = (*xfonts)->ascent;
            if (descent < (*xfonts)->descent)
                descent = (*xfonts)->descent;
            xfonts++;
        }
        font->height = font_extents->max_logical_extent.height;
        font->y = ascent;
        font->ascent = ascent;
        font->descent = descent;
        twmMessage("created fontset with %d fonts (%d missing) for \"%s\"",
                   fnum, missing_charset_count_return,
                   fontname ? fontname : "NULL");
        return True;
    }

    if (font->font != NULL)
        XFreeFont(dpy, font->font);

    if ((font->font = XLoadQueryFont(dpy, fontname)) == NULL) {
        return False;
    }
    font->height = font->font->ascent + font->font->descent;
    font->y = font->font->ascent;
    font->ascent = font->font->ascent;
    font->descent = font->font->descent;
    return True;
}

/*
 * The following functions are sensible to 'use_fontset'.
 * When 'use_fontset' is True,
 *  - XFontSet-related internationalized functions are used
 *     so as multibyte languages can be displayed.
 * When 'use_fontset' is False,
 *  - XFontStruct-related conventional functions are used
 *     so as 8-bit characters can be displayed even when
 *     locale is not set properly.
 */
void
GetFont(MyFont *font)
{

    if (!FindFontSet(font, font->name)) {
        const char *what = "fonts";
        const char *deffontname = "fixed";

        if (use_fontset) {
            what = "fontsets";
        }
        else if (Scr->DefaultFont.name) {
            deffontname = Scr->DefaultFont.name;
        }
        if (!FindFontSet(font, deffontname)) {
            twmError("unable to open %s \"%s\" or \"%s\"",
                     what, font->name, deffontname);
        }
    }
}

int
MyFont_TextWidth(MyFont *font, const char *string, int len)
{
    XRectangle ink_rect;
    XRectangle logical_rect;

    if (use_fontset) {
        XmbTextExtents(font->fontset, string, len, &ink_rect, &logical_rect);
        return logical_rect.width;
    }
    return XTextWidth(font->font, string, len);
}

void
MyFont_DrawImageString(Display *dpy2, Drawable d, MyFont *font, GC gc,
                       int x, int y, const char *string, int len)
{
    if (use_fontset) {
        XmbDrawImageString(dpy2, d, font->fontset, gc, x, y, string, len);
        return;
    }
    XDrawImageString(dpy2, d, gc, x, y, string, len);
}

void
MyFont_DrawString(Display *dpy2, Drawable d, MyFont *font, GC gc,
                  int x, int y, const char *string, int len)
{
    if (use_fontset) {
        XmbDrawString(dpy2, d, font->fontset, gc, x, y, string, len);
        return;
    }
    XDrawString(dpy2, d, gc, x, y, string, len);
}

void
MyFont_ChangeGC(unsigned long fix_fore, unsigned long fix_back,
                MyFont *fix_font)
{
    Gcv.foreground = fix_fore;
    Gcv.background = fix_back;
    if (use_fontset) {
        XChangeGC(dpy, Scr->NormalGC, GCForeground | GCBackground, &Gcv);
        return;
    }
    Gcv.font = fix_font->font->fid;
    XChangeGC(dpy, Scr->NormalGC, GCFont | GCForeground | GCBackground, &Gcv);
}

/*
 * The following functions are internationalized substitutions
 * for XFetchName and XGetIconName using XGetWMName and
 * XGetWMIconName.
 *
 * Please note that the third arguments have to be freed using free(),
 * not XFree().
 */
Status
I18N_FetchName(Display *dpy2, Window w, char **winname)
{
    int status;
    XTextProperty text_prop;
    int rc = 0;

    *winname = NULL;

    status = XGetWMName(dpy2, w, &text_prop);
    if (status && text_prop.value && text_prop.nitems) {
        char **list = NULL;
        int num;

        status = XmbTextPropertyToTextList(dpy2, &text_prop, &list, &num);
        if (status >= Success && num && list && *list) {
            XFree(text_prop.value);
            *winname = strdup(*list);
            XFreeStringList(list);
            rc = 1;
        }
        else {
            char *value = NULL;

            /*
             * If the system's locale support is broken (e.g., missing useful
             * parts), the preceding Xmb call may fail.
             */
            if (XFetchName(dpy2, w, &value) && value != NULL) {
                *winname = strdup(value);
                XFree(value);
                rc = 1;
            }
        }
    }

    return rc;
}

Status
I18N_GetIconName(Display *dpy2, Window w, char **iconname)
{
    int status;
    XTextProperty text_prop;
    char **list;
    int num;

    status = XGetWMIconName(dpy2, w, &text_prop);
    if (!status || !text_prop.value || !text_prop.nitems)
        return 0;
    status = XmbTextPropertyToTextList(dpy2, &text_prop, &list, &num);
    if (status < Success || !num || !*list)
        return 0;
    XFree(text_prop.value);
    *iconname = (char *) strdup(*list);
    XFreeStringList(list);
    return 1;
}

/**
 * separate routine to set focus to make things more understandable
 * and easier to debug
 */
void
SetFocus(TwmWindow *tmp_win, Time time)
{
    Window w = (tmp_win ? tmp_win->w : PointerRoot);

#ifdef TRACE
    if (tmp_win) {
        twmMessage("Focusing on window \"%s\"", tmp_win->full_name);
    }
    else {
        twmMessage("Unfocusing; Scr->Focus was \"%s\"",
                   Scr->Focus ? Scr->Focus->full_name : "(nil)");
    }
#endif

    XSetInputFocus(dpy, w, RevertToPointerRoot, time);
}

static Pixmap
CreateXLogoPixmap(unsigned *widthp, unsigned *heightp)
{
    int h = Scr->TBInfo.width - Scr->TBInfo.border * 2;

    if (h < 0)
        h = 0;

    *widthp = *heightp = (unsigned int) h;
    if (Scr->tbpm.xlogo == None) {
        GC gc, gcBack;

        Scr->tbpm.xlogo =
            XCreatePixmap(dpy, Scr->Root, (unsigned) h, (unsigned) h, 1);
        gc = XCreateGC(dpy, Scr->tbpm.xlogo, 0L, NULL);
        XSetForeground(dpy, gc, 0);
        XFillRectangle(dpy, Scr->tbpm.xlogo, gc, 0, 0, (unsigned) h,
                       (unsigned) h);
        XSetForeground(dpy, gc, 1);
        gcBack = XCreateGC(dpy, Scr->tbpm.xlogo, 0L, NULL);
        XSetForeground(dpy, gcBack, 0);

        /*
         * draw the logo large so that it gets as dense as possible; then white
         * out the edges so that they look crisp
         */
        XmuDrawLogo(dpy, Scr->tbpm.xlogo, gc, gcBack, -1, -1,
                    (unsigned) (h + 2), (unsigned) (h + 2));
        XDrawRectangle(dpy, Scr->tbpm.xlogo, gcBack, 0, 0, (unsigned) (h - 1),
                       (unsigned) (h - 1));

        /*
         * done drawing
         */
        XFreeGC(dpy, gc);
        XFreeGC(dpy, gcBack);
    }
    return Scr->tbpm.xlogo;
}

static Pixmap
CreateResizePixmap(unsigned *widthp, unsigned *heightp)
{
    int h = Scr->TBInfo.width - Scr->TBInfo.border * 2;

    if (h < 1)
        h = 1;

    *widthp = *heightp = (unsigned int) h;
    if (Scr->tbpm.resize == None) {
        XPoint points[3];
        GC gc;
        int w;
        int lw;

        /*
         * create the pixmap
         */
        Scr->tbpm.resize =
            XCreatePixmap(dpy, Scr->Root, (unsigned) h, (unsigned) h, 1);
        gc = XCreateGC(dpy, Scr->tbpm.resize, 0L, NULL);
        XSetForeground(dpy, gc, 0);
        XFillRectangle(dpy, Scr->tbpm.resize, gc, 0, 0, (unsigned) h,
                       (unsigned) h);
        XSetForeground(dpy, gc, 1);
        lw = h / 16;
        if (lw == 1)
            lw = 0;
        XSetLineAttributes(dpy, gc, (unsigned) lw, LineSolid, CapButt,
                           JoinMiter);

        /*
         * draw the resize button,
         */
        w = (h * 2) / 3;
        points[0].x = (short) w;
        points[0].y = 0;
        points[1].x = (short) w;
        points[1].y = (short) w;
        points[2].x = 0;
        points[2].y = (short) w;
        XDrawLines(dpy, Scr->tbpm.resize, gc, points, 3, CoordModeOrigin);
        w = w / 2;
        points[0].x = (short) w;
        points[0].y = 0;
        points[1].x = (short) w;
        points[1].y = (short) w;
        points[2].x = 0;
        points[2].y = (short) w;
        XDrawLines(dpy, Scr->tbpm.resize, gc, points, 3, CoordModeOrigin);

        /*
         * done drawing
         */
        XFreeGC(dpy, gc);
    }
    return Scr->tbpm.resize;
}

static Pixmap
CreateDotPixmap(unsigned *widthp, unsigned *heightp)
{
    int h = Scr->TBInfo.width - Scr->TBInfo.border * 2;

    h = h * 3 / 4;
    if (h < 1)
        h = 1;
    if (!(h & 1))
        h--;
    *widthp = *heightp = (unsigned int) h;
    if (Scr->tbpm.delete == None) {
        GC gc;
        Pixmap pix;

        pix = Scr->tbpm.delete =
            XCreatePixmap(dpy, Scr->Root, (unsigned) h, (unsigned) h, 1);
        gc = XCreateGC(dpy, pix, 0L, NULL);
        XSetLineAttributes(dpy, gc, (unsigned) h, LineSolid, CapRound,
                           JoinRound);
        XSetForeground(dpy, gc, 0L);
        XFillRectangle(dpy, pix, gc, 0, 0, (unsigned) h, (unsigned) h);
        XSetForeground(dpy, gc, 1L);
        XDrawLine(dpy, pix, gc, h / 2, h / 2, h / 2, h / 2);
        XFreeGC(dpy, gc);
    }
    return Scr->tbpm.delete;
}

#define questionmark_width 8
#define questionmark_height 8
static char questionmark_bits[] = {
    0x38, 0x7c, 0x64, 0x30, 0x18, 0x00, 0x18, 0x18
};

static Pixmap
CreateQuestionPixmap(unsigned *widthp, unsigned *heightp)
{
    *widthp = questionmark_width;
    *heightp = questionmark_height;
    if (Scr->tbpm.question == None) {
        Scr->tbpm.question = XCreateBitmapFromData(dpy, Scr->Root,
                                                   questionmark_bits,
                                                   questionmark_width,
                                                   questionmark_height);
    }
    /*
     * this must succeed or else we are in deep trouble elsewhere
     */
    return Scr->tbpm.question;
}

static Pixmap
CreateMenuPixmap(unsigned *widthp, unsigned *heightp)
{
    return CreateMenuIcon(Scr->TBInfo.width - Scr->TBInfo.border * 2,
                          widthp, heightp);
}

Pixmap
CreateMenuIcon(int height, unsigned *widthp, unsigned *heightp)
{
    int h, w;

    h = height;
    w = h * 7 / 8;
    if (h < 1)
        h = 1;
    if (w < 1)
        w = 1;
    *widthp = (unsigned) w;
    *heightp = (unsigned) h;
    if (Scr->tbpm.menu == None) {
        Pixmap pix;
        GC gc;
        int ih, iw;
        int ix, iy;
        int mh, mw;
        int tw, th;
        int lw, lh;
        int lx, ly;
        int lines, dly;
        int off;
        int bw;

        pix = Scr->tbpm.menu =
            XCreatePixmap(dpy, Scr->Root, (unsigned) w, (unsigned) h, 1);
        gc = XCreateGC(dpy, pix, 0L, NULL);
        XSetForeground(dpy, gc, 0L);
        XFillRectangle(dpy, pix, gc, 0, 0, (unsigned) w, (unsigned) h);
        XSetForeground(dpy, gc, 1L);
        ix = 1;
        iy = 1;
        ih = h - iy * 2;
        iw = w - ix * 2;
        off = ih / 8;
        mh = ih - off;
        mw = iw - off;
        bw = mh / 16;
        if (bw == 0 && mw > 2)
            bw = 1;
        tw = mw - bw * 2;
        th = mh - bw * 2;
        XFillRectangle(dpy, pix, gc, ix, iy, (unsigned) mw, (unsigned) mh);
        XFillRectangle(dpy, pix, gc, ix + iw - mw, iy + ih - mh, (unsigned) mw,
                       (unsigned) mh);
        XSetForeground(dpy, gc, 0L);
        XFillRectangle(dpy, pix, gc, ix + bw, iy + bw, (unsigned) tw,
                       (unsigned) th);
        XSetForeground(dpy, gc, 1L);
        lw = tw / 2;
        if ((tw & 1) ^ (lw & 1))
            lw++;
        lx = ix + bw + (tw - lw) / 2;

        lh = th / 2 - bw;
        if ((lh & 1) ^ ((th - bw) & 1))
            lh++;
        ly = iy + bw + (th - bw - lh) / 2;

        lines = 3;
        if ((lh & 1) && lh < 6) {
            lines--;
        }
        dly = lh / (lines - 1);
        while (lines--) {
            XFillRectangle(dpy, pix, gc, lx, ly, (unsigned) lw, (unsigned) bw);
            ly += dly;
        }
        XFreeGC(dpy, gc);
    }
    return Scr->tbpm.menu;
}

void
Bell(int type _X_UNUSED, int percent, Window win _X_UNUSED)
{
#ifdef XKB
    XkbStdBell(dpy, win, percent, type);
#else
    XBell(dpy, percent);
#endif
    return;
}
