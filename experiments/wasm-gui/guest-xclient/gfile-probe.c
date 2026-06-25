/* XU3/task#11 MINIMAL REPRO: the panel applicationsmenu stalls dispatching ~46 threaded GLocalFile
 * async I/O ops (g_file_*_async) whose completions are not delivered to the iterated GMainContext.
 * This probe does exactly that: a g_file_load_contents_async on a local file, then iterates a pushed
 * thread-default context waiting for the completion. If it STALLS (done=0) it reproduces task#11 in a
 * tiny binary -- a fast harness to test the runtime fix. (gtask-probe with a trivial no-I/O task WORKS,
 * so the GLocalFile-async thread-pool path is the differentiator.) */
#include <gio/gio.h>
#include <stdio.h>

static volatile gboolean done = FALSE;

static void loaded(GObject *src, GAsyncResult *res, gpointer data) {
  (void) data;
  GError *err = NULL;
  char *contents = NULL; gsize len = 0;
  gboolean ok = g_file_load_contents_finish(G_FILE(src), res, &contents, &len, NULL, &err);
  g_printerr("PROBE: ***load completion FIRED*** ok=%d len=%zu err=%s\n", ok, (size_t) len, err ? err->message : "(none)");
  g_clear_error(&err);
  if (contents) g_free(contents);
  done = TRUE;
}

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "/etc/machine-id";
  GMainContext *c = g_main_context_new();
  g_main_context_push_thread_default(c);
  g_printerr("PROBE: g_file_load_contents_async path=%s ctx=%p\n", path, (void*) c);
  GFile *f = g_file_new_for_path(path);
  g_file_load_contents_async(f, NULL, loaded, NULL);
  g_printerr("PROBE: iterating pushed context, waiting for the async file load...\n");
  int n = 0;
  while (!done && n++ < 800) g_main_context_iteration(c, TRUE);
  g_printerr("PROBE: RESULT done=%d after %d iterations -> %s\n", done, n,
             done ? "loaded (GLocalFile async works)" : "STALLED (GLocalFile async completion NOT delivered = task#11 repro)");
  return done ? 0 : 1;
}
