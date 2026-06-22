/* M8 render proof: the threaded cairo + libX11 stack drawing real pixels to the wasm X server.
 * Opens :0, creates a window, draws with cairo (filled rects + text via toy font), flushes. */
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(void){
    m("CX:start\n");
    Display *dpy = XOpenDisplay(":0");
    if(!dpy){ m("CX:open_failed\n"); return 1; }
    m("CX:opened\n");
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int W=640,H=480;
    Window win = XCreateSimpleWindow(dpy, root, 0,0, W,H, 0,
        BlackPixel(dpy,scr), WhitePixel(dpy,scr));
    XSelectInput(dpy, win, ExposureMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    m("CX:mapped\n");
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, win,
        DefaultVisual(dpy,scr), W, H);
    cairo_t *cr = cairo_create(surf);
    /* background */
    cairo_set_source_rgb(cr, 0.18, 0.20, 0.28); cairo_paint(cr);
    /* a panel */
    cairo_set_source_rgb(cr, 0.93, 0.94, 0.96);
    cairo_rectangle(cr, 60, 60, 520, 360); cairo_fill(cr);
    /* a button */
    cairo_set_source_rgb(cr, 0.20, 0.45, 0.85);
    cairo_rectangle(cr, 220, 300, 200, 60); cairo_fill(cr);
    /* title text */
    cairo_set_source_rgb(cr, 0.1,0.1,0.1);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 28);
    cairo_move_to(cr, 100, 130);
    cairo_show_text(cr, "secure-exec: GTK stack (threaded wasm)");
    cairo_set_font_size(cr, 20);
    cairo_set_source_rgb(cr, 1,1,1);
    cairo_move_to(cr, 270, 338);
    cairo_show_text(cr, "Click me");
    cairo_surface_flush(surf);
    XFlush(dpy);
    m("CX:drawn\n");
    /* pump a few times so the server composites */
    for(int i=0;i<5;i++){ XFlush(dpy); usleep(100000); }
    m("CX:done\n");
    return 0;
}
