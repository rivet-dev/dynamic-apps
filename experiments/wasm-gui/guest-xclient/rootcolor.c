/* Constraint #4 discriminator (no cairo): paint the X root window a distinctive color so we can tell
 * WHY xfwm4's decorations are invisible. With xfwm4 running, screenshot:
 *   - frame area shows the ROOT COLOR  -> the decoration windows are SHAPED AWAY (empty mask) or absent.
 *   - frame area is BLACK              -> the decoration windows exist but their pixmap is painted black
 *                                         (the cairo image draw into pm->pixmap failed).
 * This separates the "shape mask empty" hypothesis from the "decoration pixmap black" hypothesis. */
#include <X11/Xlib.h>
#include <unistd.h>
#include <string.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

int main(void) {
    Display *dpy = XOpenDisplay(":0");
    if (!dpy) { mark("RC:open_failed\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    /* Keep repainting the root magenta -- if a WM or app clears it, we restore the marker color so the
     * framebuffer at capture time still shows it behind any shaped-away decoration. */
    for (;;) {
        XSetWindowBackground(dpy, root, 0xC000C0);   /* magenta */
        XClearWindow(dpy, root);
        XFlush(dpy);
        mark("RC:painted\n");
        sleep(3);
    }
    return 0;
}
