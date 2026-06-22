/* Localize the gtk_init hang: drive GDK directly (no gtk_init settings/IM/modules). */
#include <stdio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(int argc, char**argv){
    m("GP:start\n");
    gdk_init(&argc,&argv);
    m("GP:gdk_init_done\n");
    GdkDisplay *dpy = gdk_display_get_default();
    fprintf(stderr,"GP:display=%p\n",(void*)dpy); fflush(stderr);
    GdkScreen *scr = gdk_screen_get_default();
    fprintf(stderr,"GP:screen=%p w=%d h=%d\n",(void*)scr,
        scr?gdk_screen_get_width(scr):-1, scr?gdk_screen_get_height(scr):-1); fflush(stderr);
    GdkWindowAttr attr; attr.window_type=GDK_WINDOW_TOPLEVEL;
    attr.x=0; attr.y=0; attr.width=640; attr.height=480;
    attr.wclass=GDK_INPUT_OUTPUT; attr.event_mask=GDK_EXPOSURE_MASK;
    GdkWindow *win = gdk_window_new(NULL,&attr,GDK_WA_X|GDK_WA_Y);
    fprintf(stderr,"GP:window=%p\n",(void*)win); fflush(stderr);
    gdk_window_show(win);
    cairo_t *cr = gdk_cairo_create(win);
    cairo_set_source_rgb(cr,0.15,0.5,0.3); cairo_paint(cr);
    cairo_set_source_rgb(cr,1,1,1); cairo_rectangle(cr,80,80,480,320); cairo_fill(cr);
    cairo_destroy(cr);
    gdk_display_flush(dpy);
    m("GP:drawn\n");
    for(int i=0;i<5;i++){ gdk_display_flush(dpy); g_usleep(100000); }
    m("GP:done\n");
    return 0;
}
