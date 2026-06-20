/* Minimal libX11 client: open the display, report the screen geometry, draw a filled rect on the
 * root window via real Xlib calls, sync, and close. Isolates the libX11 + libxcb transport. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>

static void mark(const char *m) { write(2, m, __builtin_strlen(m)); }

int main(void) {
    mark("XO:start\n");
    Display *dpy = XOpenDisplay(":0");
    if (!dpy) { mark("XO:open_failed\n"); return 1; }
    mark("XO:opened\n");

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int w = DisplayWidth(dpy, scr), h = DisplayHeight(dpy, scr);
    char buf[64];
    int n = 0; const char *p = "XO:size="; while (*p) buf[n++] = *p++;
    int v = w; char t[8]; int ti = 0; if(!v)t[ti++]='0'; while(v){t[ti++]='0'+v%10;v/=10;} while(ti)buf[n++]=t[--ti];
    buf[n++]='x'; v=h; ti=0; if(!v)t[ti++]='0'; while(v){t[ti++]='0'+v%10;v/=10;} while(ti)buf[n++]=t[--ti]; buf[n++]='\n';
    write(2, buf, n);

    GC gc = XCreateGC(dpy, root, 0, NULL);
    XSetForeground(dpy, gc, 0x00CC44u);   /* green-ish */
    XFillRectangle(dpy, root, gc, 0, 0, w, h);
    XSync(dpy, False);
    mark("XO:drawn\n");
    XCloseDisplay(dpy);
    mark("XO:done\n");
    return 0;
}
