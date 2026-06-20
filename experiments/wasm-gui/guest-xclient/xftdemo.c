/* Proves the cross-compiled Xft + fontconfig + freetype stack renders ANTIALIASED text through the
 * wasm X server. Opens a window, resolves a font via a fontconfig pattern (XftFontOpenName), and
 * draws a string with XftDrawStringUtf8. Antialiasing shows up as intermediate grey pixels along the
 * glyph edges (not just pure fg/bg), which the test asserts. Event-driven: redraws on Expose. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void mark(const char *s) { write(2, s, strlen(s)); }

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { mark("XFT:no-display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    Visual *vis = DefaultVisual(dpy, scr);
    Colormap cmap = DefaultColormap(dpy, scr);

    Window win = XCreateSimpleWindow(dpy, root, 30, 30, 360, 120, 1,
                                     BlackPixel(dpy, scr), 0xFFFFFF /* white bg */);
    XStoreName(dpy, win, "secure-exec Xft");
    XSizeHints h; memset(&h, 0, sizeof(h)); h.flags = PPosition; h.x = 30; h.y = 30;
    XSetWMNormalHints(dpy, win, &h);
    XSelectInput(dpy, win, ExposureMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    mark("XFT:mapped\n");

    /* Diagnostics: did fontconfig init + find any fonts? */
    if (!FcInit()) mark("XFT:fcinit-failed\n");
    {
        FcConfig *cfg = FcConfigGetCurrent();
        FcFontSet *sys = cfg ? FcConfigGetFonts(cfg, FcSetSystem) : NULL;
        char buf[64];
        int nfonts = sys ? sys->nfont : -1;
        snprintf(buf, sizeof(buf), "XFT:fc-fonts=%d\n", nfonts);
        mark(buf);
        FcStrList *dirs = cfg ? FcConfigGetFontDirs(cfg) : NULL;
        if (dirs) {
            FcChar8 *d;
            while ((d = FcStrListNext(dirs))) { mark("XFT:dir="); mark((char *)d); mark("\n"); }
        }
    }

    /* Resolve a font via fontconfig (name -> file + size + antialias). */
    XftFont *font = XftFontOpenName(dpy, scr, "DejaVu Sans-22");
    if (!font) font = XftFontOpenName(dpy, scr, "sans-22");
    if (!font) font = XftFontOpenName(dpy, scr, "monospace-22");
    if (!font) { mark("XFT:no-font\n"); return 2; }
    mark("XFT:font-opened\n");

    XftDraw *draw = XftDrawCreate(dpy, win, vis, cmap);
    XftColor black;
    XRenderColor rc = { 0x0000, 0x0000, 0x0000, 0xffff };
    XftColorAllocValue(dpy, vis, cmap, &rc, &black);

    const char *msg = "secure-exec wasm";
    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) {
            XClearWindow(dpy, win);
            XftDrawStringUtf8(draw, &black, font, 20, 70,
                              (const FcChar8 *)msg, (int)strlen(msg));
            XFlush(dpy);
            mark("XFT:drawn\n");
        }
    }
    return 0;
}
