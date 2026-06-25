/* XU4/task#11 diagnostic: a MINIMAL GtkApplication. Unlike gtk-hello (plain gtk_init+gtk_main, which
 * works), this uses g_application_run() -> GApplication::register, which acquires a unique D-Bus name on
 * the session bus. Hypothesis: that registration (over GDBus, before the main loop) is the task #11
 * deadlock that blocks xfdesktop + Thunar (both GtkApplications), while plain-GTK apps (panel core,
 * pcmanfm) are unaffected. If this stops after "run (register)" and never reaches "activated", the
 * GApplication registration is the universal blocker -- isolated in a tiny binary. */
#include <gtk/gtk.h>

static void activate(GtkApplication *app, gpointer data) {
  (void) data;
  g_printerr("GTKAPP: activated (register succeeded)\n");
  GtkWidget *w = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(w), 320, 200);
  gtk_widget_show_all(w);
  g_printerr("GTKAPP: window shown\n");
}

int main(int argc, char **argv) {
  g_printerr("GTKAPP: new\n");
  GtkApplication *app = gtk_application_new("org.secureexec.gtkappprobe", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  g_printerr("GTKAPP: run -- GApplication::register happens here (suspect deadlock)\n");
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_printerr("GTKAPP: run returned %d (no deadlock)\n", status);
  return status;
}
