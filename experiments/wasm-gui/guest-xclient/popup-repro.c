/* Minimal repro of the notifyd-popup construction pattern (RGBA + app-paintable + monitor-positioned
 * GtkWindow), with per-step markers, to ISOLATE the shared construction hang (Thunar/notifyd). Mine, so
 * the markers are allowed (constraint #5 is about UNMODIFIED components). */
#include <gtk/gtk.h>
#include <stdio.h>
#define M(s) do { fprintf(stderr, "POPUP-REPRO: " s "\n"); } while(0)
static gboolean quit_cb(gpointer d){ (void)d; M("timeout->quit"); gtk_main_quit(); return G_SOURCE_REMOVE; }
int main(int argc, char **argv){
  M("gtk_init"); gtk_init(&argc,&argv);
  M("icon_theme_default"); GtkIconTheme *it = gtk_icon_theme_get_default();
  M("load_icon dialog-information"); GError *ierr=NULL;
  GdkPixbuf *pb = gtk_icon_theme_load_icon(it, "dialog-information", 48, 0, &ierr);
  fprintf(stderr, "POPUP-REPRO: icon=%p err=%s\n", (void*)pb, ierr?ierr->message:"none");
  M("window_new"); GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GdkScreen *scr = gtk_widget_get_screen(w);
  M("rgba_visual"); GdkVisual *rgba = gdk_screen_get_rgba_visual(scr);
  if (rgba){ M("set_visual(rgba)"); gtk_widget_set_visual(w, rgba); } else M("no rgba -> fallback");
  M("app_paintable"); gtk_widget_set_app_paintable(w, TRUE);
  M("get_display"); GdkDisplay *disp = gdk_screen_get_display(scr);
  M("get_primary_monitor"); GdkMonitor *mon = gdk_display_get_primary_monitor(disp);
  GdkRectangle geo = {0,0,700,500};
  if (mon){ M("monitor_get_geometry"); gdk_monitor_get_geometry(mon, &geo); }
  fprintf(stderr, "POPUP-REPRO: geo %dx%d+%d+%d\n", geo.width, geo.height, geo.x, geo.y);
  M("label+add"); gtk_container_add(GTK_CONTAINER(w), gtk_label_new("Hello notification, all wasm."));
  M("show_all"); gtk_widget_show_all(w);
  M("move"); gtk_window_move(GTK_WINDOW(w), geo.x + geo.width - 320, geo.y + 40);
  M("main loop start"); g_timeout_add(6000, quit_cb, NULL); gtk_main();
  M("DONE"); return 0;
}
