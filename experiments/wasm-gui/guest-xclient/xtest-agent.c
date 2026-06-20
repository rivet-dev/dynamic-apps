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
#include <time.h>

static void mark(const char *s) { write(2, s, strlen(s)); }

static void nap(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { mark("XT:no-display\n"); return 1; }
    int ev = 0, err = 0, major = 0, minor = 0;
    if (!XTestQueryExtension(dpy, &ev, &err, &major, &minor)) {
        mark("XT:no-xtest\n");
        return 2;
    }
    mark("XT:ready\n");

    /* `xtest-agent wander` continuously walks the pointer along a path (with a click partway), so the
     * software-rendered cursor sprite moves around the framebuffer for a screen-capture video. Runs
     * until killed. */
    if (argc >= 2 && strcmp(argv[1], "wander") == 0) {
        /* Path wanders across the screen; index 3 sits over the input-target window so the click
         * there lands on it (it repaints orange on camera). The clock lives bottom-right (~400,260). */
        static const int pts[][2] = {
            {120,110},{210,150},{165,130},{165,130},{300,120},{470,300},
            {440,330},{360,280},{260,250},{120,300},{200,180},{330,150},
        };
        const int np = (int)(sizeof(pts)/sizeof(pts[0]));
        mark("XT:wander\n");
        /* Keep the injection rate LOW: a chatty XTEST stream starves the animated xclock's redraws on
         * the shared sync-RPC service thread, so we glide in only 2 steps and pause ~450ms between
         * points. The cursor still visibly travels; the clock keeps ticking. */
        for (int round = 0; ; round++) {
            for (int i = 0; i < np; i++) {
                static int cx = 60, cy = 60;
                int tx = pts[i][0], ty = pts[i][1];
                int mx = (cx + tx) / 2, my = (cy + ty) / 2;
                XTestFakeMotionEvent(dpy, -1, mx, my, 0); XFlush(dpy); nap(220);
                XTestFakeMotionEvent(dpy, -1, tx, ty, 0); XFlush(dpy);
                cx = tx; cy = ty;
                if (i == 3) {  /* click once over the input target so it reacts on camera */
                    XTestFakeButtonEvent(dpy, 1, True, 0);
                    XTestFakeButtonEvent(dpy, 1, False, 0);
                    XFlush(dpy);
                }
                nap(450);
            }
        }
    }

    /* `xtest-agent follow [path]` tails a host-written command file and injects each new line as it
     * appears. This is the LIVE input channel for the interactive desktop: the native host appends
     * winit mouse/keyboard events to /data/input-cmds (a host-backed VFS file), and the agent turns
     * them into real X input via XTEST. Host->guest stdin is unreliable for wasm guests (two-pipe
     * mismatch), but write_file -> VFS -> guest-read is proven (it's how /fonts is delivered). */
    if (argc >= 2 && strcmp(argv[1], "follow") == 0) {
        const char *path = (argc >= 3) ? argv[2] : "/data/input-cmds";
        long off = 0;
        char buf[4096];
        mark("XT:follow\n");
        { FILE *pf = fopen("/data/Xvfb_screen0", "rb"); mark(pf ? "XT:probe-fb-ok\n" : "XT:probe-fb-fail\n"); if (pf) fclose(pf); }
        int dbg_opened = 0;
        for (;;) {
            FILE *f = fopen(path, "rb");
            if (f) {
                if (!dbg_opened) { dbg_opened = 1; mark("XT:follow-opened\n"); }
                if (fseek(f, off, SEEK_SET) == 0) {
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                        mark("XT:follow-read\n");
                        off += (long)n;
                        /* Process complete lines in this chunk. */
                        size_t start = 0;
                        for (size_t i = 0; i < n; i++) {
                            if (buf[i] == '\n') {
                                char line[256];
                                size_t len = i - start;
                                if (len > 0 && len < sizeof(line)) {
                                    memcpy(line, buf + start, len);
                                    line[len] = 0;
                                    char cmd[32]; int a = 0, x = 0, y = 0;
                                    int parsed = sscanf(line, "%31s %d %d %d", cmd, &a, &x, &y);
                                    if (parsed >= 3 && strcmp(cmd, "motion") == 0) {
                                        XTestFakeMotionEvent(dpy, -1, a, x, 0); /* a=x,x=y */
                                        XFlush(dpy);
                                    } else if (parsed >= 4 && strcmp(cmd, "button") == 0) {
                                        XTestFakeMotionEvent(dpy, -1, x, y, 0);
                                        XTestFakeButtonEvent(dpy, (unsigned)a, True, 0);
                                        XTestFakeButtonEvent(dpy, (unsigned)a, False, 0);
                                        XFlush(dpy);
                                    } else if (parsed >= 2 && strcmp(cmd, "buttondn") == 0) {
                                        XTestFakeButtonEvent(dpy, (unsigned)a, True, 0); XFlush(dpy);
                                    } else if (parsed >= 2 && strcmp(cmd, "buttonup") == 0) {
                                        XTestFakeButtonEvent(dpy, (unsigned)a, False, 0); XFlush(dpy);
                                    } else if (parsed >= 2 && strcmp(cmd, "key") == 0) {
                                        XTestFakeKeyEvent(dpy, (unsigned)a, True, 0);
                                        XTestFakeKeyEvent(dpy, (unsigned)a, False, 0);
                                        XFlush(dpy);
                                    }
                                }
                                start = i + 1;
                            }
                        }
                        /* rewind to the start of any partial trailing line for next pass */
                        if (start < n) { off -= (long)(n - start); }
                    }
                }
                fclose(f);
            }
            nap(15);
        }
    }

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
