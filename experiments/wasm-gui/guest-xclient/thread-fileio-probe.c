/* XU3/task#11 actionable repro: the panel's 46 GLocalFile async file-loads run on worker threads and
 * NEVER complete (g_task_return is never reached) -- the worker blocks in the file read. This probe
 * isolates that: spawn a GThread (= a wasi-thread worker isolate) and do a SYNCHRONOUS file read in it.
 * If the thread hangs (no "thread file read ok=" line, never joins), the worker-isolate file-I/O path
 * is the bug -- a worker's blocking path_open/fd_read sync-RPC is not serviced. The main thread's file
 * I/O works (pcmanfm), so this pinpoints worker-isolate vs main-isolate syscall servicing. */
#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>

static gpointer thread_func(gpointer data) {
  (void) data;
  const char *path = "/etc/machine-id";
  g_printerr("PROBE: worker thread started; reading %s via g_file_get_contents...\n", path);
  char *contents = NULL; gsize len = 0; GError *err = NULL;
  gboolean ok = g_file_get_contents(path, &contents, &len, &err);
  g_printerr("PROBE: ***worker file read returned*** ok=%d len=%zu err=%s\n",
             ok, (size_t) len, err ? err->message : "(none)");
  g_clear_error(&err);
  g_free(contents);
  return GINT_TO_POINTER(ok ? 1 : 0);
}

#ifdef POOL_MAIN
/* Mimic the menu's GThreadPool: N concurrent workers each doing M file ops, while the main busy-spins a
 * GMainContext. This is the closest minimal mimic of the 46-GLocalFile-async / 4-pool-thread stall. */
#include <stdatomic.h>
static atomic_int g_completed = 0;
static atomic_int g_started = 0;
static void pool_worker(gpointer item, gpointer user) {
  (void) item; (void) user;
  atomic_fetch_add(&g_started, 1);
  GError *e = NULL;
  /* Mimic garcon: query_info (stat) + enumerate_children (readdir) -- the ops the menu actually runs,
   * not just a plain read. */
  GFile *f = g_file_new_for_path("/etc");
  GFileInfo *info = g_file_query_info(f, "standard::*", 0, NULL, &e);
  if (info) g_object_unref(info);
  g_clear_error(&e);
  GFileEnumerator *en = g_file_enumerate_children(f, "standard::name", 0, NULL, &e);
  int n = 0;
  if (en) {
    GFileInfo *ci;
    while ((ci = g_file_enumerator_next_file(en, NULL, &e)) != NULL) { g_object_unref(ci); n++; }
    g_object_unref(en);
  }
  g_clear_error(&e);
  g_object_unref(f);
  if (n >= 0 && info != NULL) atomic_fetch_add(&g_completed, 1);
}
int main(void) {
  const int NTASK = 46;
  g_printerr("PROBE: [pool-main] %d file ops via GThreadPool + busy-spinning main\n", NTASK);
  GMainContext *c = g_main_context_new();
  g_main_context_push_thread_default(c);
  GThreadPool *pool = g_thread_pool_new(pool_worker, NULL, 4, FALSE, NULL);
  for (int i = 0; i < NTASK; i++) g_thread_pool_push(pool, GINT_TO_POINTER(i + 1), NULL);
  int spins = 0;
  while (atomic_load(&g_completed) < NTASK && spins++ < 50000000) {
    g_main_context_iteration(c, FALSE);
    if ((spins % 2000000) == 0)
      g_printerr("PROBE: [pool-main] spins=%d started=%d completed=%d\n", spins, atomic_load(&g_started), atomic_load(&g_completed));
  }
  g_printerr("PROBE: [pool-main] RESULT completed=%d/%d started=%d after %d spins -> %s\n",
             atomic_load(&g_completed), NTASK, atomic_load(&g_started), spins,
             atomic_load(&g_completed) == NTASK ? "ALL completed (not the bug)" : "STALLED (pooled file I/O + busy main = task#11 repro)");
  return 0;
}
#else

static volatile int worker_done = 0;
static gpointer thread_func_busy(gpointer data) {
  gpointer r = thread_func(data);
  worker_done = 1;
  return r;
}

int main(void) {
#ifdef BUSY_MAIN
  /* Mimic the menu: the main thread busy-spins a GMainContext (non-blocking iteration) while the worker
   * does its file read. If pump_process_events pumps sequentially, the busy main starves the worker. */
  g_printerr("PROBE: [busy-main] spawning worker + busy-spinning the main GMainContext\n");
  GMainContext *c = g_main_context_new();
  g_main_context_push_thread_default(c);
  GThread *t = g_thread_new("fileio-worker", thread_func_busy, NULL);
  int spins = 0;
  while (!worker_done && spins++ < 20000000) {
    g_main_context_iteration(c, FALSE);  /* non-blocking: a tight busy spin like the stalled menu */
  }
  g_printerr("PROBE: [busy-main] RESULT worker_done=%d after %d busy spins -> %s\n",
             worker_done, spins, worker_done ? "worker completed despite busy main" : "STARVED (busy main blocks worker file I/O = task#11 root)");
  g_thread_join(t);
#else
  g_printerr("PROBE: spawning worker thread for the file read\n");
  GThread *t = g_thread_new("fileio-worker", thread_func, NULL);
  g_printerr("PROBE: main thread joining the worker...\n");
  gpointer r = g_thread_join(t);
  g_printerr("PROBE: RESULT worker joined ok=%d -> %s\n", GPOINTER_TO_INT(r),
             GPOINTER_TO_INT(r) ? "worker file I/O WORKS" : "worker read failed");
#endif
  return 0;
}
#endif /* POOL_MAIN */
