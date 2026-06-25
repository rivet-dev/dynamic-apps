/* XU5 Thunar activation-block bisect. The working gtkapp-probe uses "activate" (FLAGS_NONE) + maps a
 * window. Thunar uses G_APPLICATION_HANDLES_COMMAND_LINE -> its activation goes through "command-line",
 * and during window creation it touches g_volume_monitor_get(). This probe replicates THAT path
 * (HANDLES_COMMAND_LINE + the volume monitor + a window) to isolate where Thunar idles: if "window shown"
 * prints, the command-line path + volume monitor work and Thunar's block is deeper (the folder model);
 * if it stops at "g_volume_monitor_get", that GIO call is the block. Build with the gio-vfs-local wrap. */
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <stdio.h>

static void enum_done(GObject *src, GAsyncResult *res, gpointer d) {
  (void) d;
  g_printerr("CMDPROBE: ★ enumerate_children_async COMPLETION FIRED\n");
  GError *err = NULL;
  GFileEnumerator *e = g_file_enumerate_children_finish(G_FILE(src), res, &err);
  g_printerr("CMDPROBE: async enumerator=%p err=%s\n", (void*) e, err ? err->message : "(none)");
  if (err) g_clear_error(&err);
  if (e) g_object_unref(e);
}

static int command_line(GApplication *app, GApplicationCommandLine *cl, gpointer d) {
  (void) d;
  const char *cwd = g_application_command_line_get_cwd(cl);
  g_printerr("CMDPROBE: command_line FIRED, cwd=%s\n", cwd ? cwd : "(null)");
  g_printerr("CMDPROBE: g_volume_monitor_get...\n");
  GVolumeMonitor *vm = g_volume_monitor_get();
  g_printerr("CMDPROBE: volume monitor=%p\n", (void*) vm);
  GList *mounts = g_volume_monitor_get_mounts(vm);
  g_printerr("CMDPROBE: get_mounts -> %u mounts\n", g_list_length(mounts));
  g_list_free_full(mounts, g_object_unref);
  GtkWidget *w = gtk_application_window_new(GTK_APPLICATION(app));
  gtk_window_set_default_size(GTK_WINDOW(w), 320, 200);
  gtk_widget_show_all(w);
  g_printerr("CMDPROBE: window shown -- COMMAND_LINE PATH + VOLUME MONITOR WORK\n");

  /* Thunar-like folder load: ThunarFolder enumerates the directory ASYNC (g_file_enumerate_children_async).
   * If the completion never fires, the folder never loads = the suspected Thunar block. */
  GFile *root = g_file_new_for_path("/");
  g_printerr("CMDPROBE: enumerate_children SYNC on /...\n");
  GFileEnumerator *se = g_file_enumerate_children(root, "standard::*", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  int n = 0; GFileInfo *fi;
  while (se && (fi = g_file_enumerator_next_file(se, NULL, NULL))) { n++; g_object_unref(fi); }
  g_printerr("CMDPROBE: SYNC enumerated %d entries (enumerator=%p)\n", n, (void*) se);
  if (se) g_object_unref(se);
  g_printerr("CMDPROBE: enumerate_children_async on / (the Thunar path)...\n");
  g_file_enumerate_children_async(root, "standard::*", G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, NULL, enum_done, NULL);
  g_printerr("CMDPROBE: async dispatched -- waiting for the completion in the main loop\n");
  return 0;
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("org.secureexec.cmdprobe", G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect(app, "command-line", G_CALLBACK(command_line), NULL);
  g_printerr("CMDPROBE: g_application_run (register on the session bus)...\n");
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_printerr("CMDPROBE: run returned %d\n", status);
  return status;
}
