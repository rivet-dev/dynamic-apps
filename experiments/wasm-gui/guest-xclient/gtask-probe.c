/* XU3/task#11 minimal repro: replicate the GIO sync-over-async pattern that stalls the panel menu +
 * Thunar window. push a fresh thread-default GMainContext, dispatch a g_task_run_in_thread worker, then
 * iterate that context waiting for the completion. On real Linux the completion fires. If it does NOT
 * fire here (done stays 0) while match=1, the worker's g_main_context_invoke targets a different context
 * than the one we iterate -> the confirmed GMainContext mismatch, in a tiny symbolizable binary. */
#include <gio/gio.h>
#include <stdio.h>

static volatile gboolean done = FALSE;
static GMainContext *pushed = NULL;

static void task_func(GTask *task, gpointer src, gpointer data, GCancellable *c) {
  (void) src; (void) data; (void) c;
  GMainContext *worker_td = g_main_context_get_thread_default();
  g_printerr("PROBE: worker running; worker thread_default=%p (pushed=%p)\n", (void*) worker_td, (void*) pushed);
  g_usleep(50000);
  g_task_return_boolean(task, TRUE);
  g_printerr("PROBE: worker called g_task_return\n");
}

static void task_done(GObject *src, GAsyncResult *res, gpointer data) {
  (void) src; (void) res; (void) data;
  g_printerr("PROBE: ***COMPLETION FIRED*** on the iterated context\n");
  done = TRUE;
}

int main(void) {
  pushed = g_main_context_new();
  g_main_context_push_thread_default(pushed);
  GMainContext *td = g_main_context_get_thread_default();
  g_printerr("PROBE: pushed=%p thread_default=%p match=%d\n", (void*) pushed, (void*) td, pushed == td);

  GTask *task = g_task_new(NULL, NULL, task_done, NULL);
  g_printerr("PROBE: g_task_run_in_thread (dispatch worker)\n");
  g_task_run_in_thread(task, task_func);
  g_object_unref(task);

  g_printerr("PROBE: iterating the pushed context, waiting for completion...\n");
  int n = 0;
  while (!done && n++ < 600) g_main_context_iteration(pushed, TRUE);
  g_printerr("PROBE: RESULT done=%d after %d iterations -> %s\n", done, n,
             done ? "completion delivered (works)" : "STALLED (completion NOT delivered to iterated context)");
  return done ? 0 : 1;
}
