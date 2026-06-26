/* Minimal VTE terminal: a VteTerminal widget in a window, spawn /bin/sh. Validates libvte + the syscompat
 * shims (pty/spawn/fork) end to end. */
#include <vte/vte.h>
#include <gtk/gtk.h>
#include <stdio.h>
static void child_ready(VteTerminal *t, GPid pid, GError *err, gpointer d) {
  fprintf(stderr, "VTE-TEST: spawn child_ready pid=%d err=%s\n", (int)pid, err ? err->message : "none");
}
static gboolean quitcb(gpointer d){ gtk_main_quit(); return G_SOURCE_REMOVE; }
int main(int argc, char **argv) {
  gtk_init(&argc, &argv);
  fprintf(stderr, "VTE-TEST: gtk_init done\n");
  GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(win), 640, 400);
  VteTerminal *term = VTE_TERMINAL(vte_terminal_new());
  fprintf(stderr, "VTE-TEST: vte_terminal_new ok\n");
  gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(term));
  char *sh_argv[] = { (char*)"/bin/sh", NULL };
  vte_terminal_spawn_async(term, VTE_PTY_DEFAULT, NULL, sh_argv, NULL, 0, NULL, NULL, NULL, -1, NULL, child_ready, NULL);
  fprintf(stderr, "VTE-TEST: spawn_async called\n");
  gtk_widget_show_all(win);
  g_timeout_add(9000, quitcb, NULL);
  gtk_main();
  fprintf(stderr, "VTE-TEST: DONE\n");
  return 0;
}
