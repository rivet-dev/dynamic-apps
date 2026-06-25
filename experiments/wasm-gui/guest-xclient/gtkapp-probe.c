/* XU4/task#11 diagnostic: a MINIMAL GtkApplication. Unlike gtk-hello (plain gtk_init+gtk_main, which
 * works), this uses g_application_run() -> GApplication::register, which acquires a unique D-Bus name on
 * the session bus. Hypothesis: that registration (over GDBus, before the main loop) is the task #11
 * deadlock that blocks xfdesktop + Thunar (both GtkApplications), while plain-GTK apps (panel core,
 * pcmanfm) are unaffected. If this stops after "run (register)" and never reaches "activated", the
 * GApplication registration is the universal blocker -- isolated in a tiny binary. */
#include <gtk/gtk.h>
#ifdef PROBE_XFCONF
#include <xfconf/xfconf.h>
#endif

#include <gio/gio.h>
static void activate(GtkApplication *app, gpointer data) {
  (void) data;
  g_printerr("GTKAPP: activated (register succeeded)\n");
  /* Bisection step: xfdesktop's file-icon parts call g_volume_monitor_get(). With
   * GIO_USE_VOLUME_MONITOR=null but the null monitor not resolving, GIO may fall back to the union/native
   * monitor whose init deadlocks (the M8.5 issue). Test that here. */
  g_printerr("GTKAPP: g_volume_monitor_get (suspect deadlock)\n");
  GVolumeMonitor *vm = g_volume_monitor_get();
  g_printerr("GTKAPP: volume monitor = %p (no deadlock)\n", (void *) vm);
#ifdef PROBE_XFCONF
  /* Bisection step: replicate xfdesktop's xfconf usage -- init + get a channel + read + WATCH it (the
   * property-changed signal subscription, which the panel core may not do the same way). */
  g_printerr("GTKAPP: xfconf_init (suspect deadlock)\n");
  GError *xerr = NULL;
  if (!xfconf_init(&xerr)) { g_printerr("GTKAPP: xfconf_init failed: %s\n", xerr ? xerr->message : "?"); }
  else {
    g_printerr("GTKAPP: xfconf_init ok; get channel + read\n");
    XfconfChannel *ch = xfconf_channel_get("xfce4-desktop");
    gchar *s = xfconf_channel_get_string(ch, "/backdrop/screen0/monitor0/workspace0/last-image", "(none)");
    g_printerr("GTKAPP: xfconf read last-image=%s; subscribe property-changed\n", s);
    g_signal_connect(ch, "property-changed", G_CALLBACK(g_message), NULL);
    g_printerr("GTKAPP: xfconf done (no deadlock)\n");
  }
#endif
  /* Bisection step: garcon_menu_load sets up GFileMonitor on the menu dirs. GLib's inotify backend
   * spawns a helper thread the monitor init may wait on -- if it doesn't get scheduled, this blocks. */
  g_printerr("GTKAPP: g_file_monitor_directory /usr/share/applications (suspect deadlock)\n");
  GFile *dir = g_file_new_for_path("/usr/share/applications");
  GError *merr = NULL;
  GFileMonitor *mon = g_file_monitor_directory(dir, G_FILE_MONITOR_NONE, NULL, &merr);
  g_printerr("GTKAPP: file monitor = %p err=%s (no deadlock)\n", (void *) mon, merr ? merr->message : "(none)");
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
