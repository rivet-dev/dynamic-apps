/* XTEST input-injection agent. Reads newline-delimited commands from stdin and synthesizes the
 * corresponding X input events via the XTEST extension, exactly as a host driving the desktop's
 * native window would forward real mouse/keyboard input. This is the in-VM end of the SPEC M6.1
 * input path: the native Rust host writes commands to this guest's stdin (kernel pipe) and they
 * become real X input events delivered to whichever client has the focus / pointer.
 *
 * Commands (one per line):
 *   key <keycode>          press+release a key by keycode
 *   button <n> <x> <y>     move pointer to x,y then press+release button n
 *   motion <x> <y>         move pointer to x,y
 */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void mark(const char *s) { write(2, s, strlen(s)); }

int main(int argc, char **argv) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { mark("XT:no-display\n"); return 1; }
    int ev = 0, err = 0, major = 0, minor = 0;
    if (!XTestQueryExtension(dpy, &ev, &err, &major, &minor)) {
        mark("XT:no-xtest\n");
        return 2;
    }
    mark("XT:ready\n");

    /* Optional startup injection: `xtest-agent <cmd> <a> [x] [y]` injects one event immediately
     * (the host launches this agent last, after the target has mapped, so no settle wait is needed).
     * Further commands still arrive on stdin for host-driven input. */
    if (argc >= 3) {
        unsigned a = (unsigned)atoi(argv[2]);
        int x = argc > 3 ? atoi(argv[3]) : 0;
        int y = argc > 4 ? atoi(argv[4]) : 0;
        if (strcmp(argv[1], "key") == 0) {
            XTestFakeKeyEvent(dpy, a, True, 0);
            XTestFakeKeyEvent(dpy, a, False, 0);
            XFlush(dpy); mark("XT:key\n");
        } else if (strcmp(argv[1], "button") == 0) {
            XTestFakeMotionEvent(dpy, -1, x, y, 0);
            XTestFakeButtonEvent(dpy, a, True, 0);
            XTestFakeButtonEvent(dpy, a, False, 0);
            XFlush(dpy); mark("XT:button\n");
        }
    }

    char line[256];
    /* Line-buffered blocking reads from stdin (kernel pipe fed by the host). */
    while (fgets(line, sizeof(line), stdin)) {
        char cmd[32]; int a = 0, x = 0, y = 0;
        int n = sscanf(line, "%31s %d %d %d", cmd, &a, &x, &y);
        if (n < 1) continue;
        if (strcmp(cmd, "key") == 0 && n >= 2) {
            XTestFakeKeyEvent(dpy, (unsigned)a, True, 0);
            XTestFakeKeyEvent(dpy, (unsigned)a, False, 0);
            XFlush(dpy);
            mark("XT:key\n");
        } else if (strcmp(cmd, "button") == 0 && n >= 4) {
            XTestFakeMotionEvent(dpy, -1, x, y, 0);
            XTestFakeButtonEvent(dpy, (unsigned)a, True, 0);
            XTestFakeButtonEvent(dpy, (unsigned)a, False, 0);
            XFlush(dpy);
            mark("XT:button\n");
        } else if (strcmp(cmd, "motion") == 0 && n >= 3) {
            XTestFakeMotionEvent(dpy, -1, a, x, 0);  /* note: a=x, x=y for 2-arg form */
            XFlush(dpy);
            mark("XT:motion\n");
        }
    }
    mark("XT:eof\n");
    return 0;
}
