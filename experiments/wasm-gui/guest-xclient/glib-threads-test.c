/* Threaded GLib smoke (WASM-THREADS-SPEC.md DoD 9.7): GLib's own thread API (g_thread_new -> pthread
 * -> wasi.thread-spawn) + a GThreadPool job, running on the wasm-threads runtime. This is the direct
 * proof that GLib's worker threads (the M8/GTK blocker) work in secure-exec. */
#include <glib.h>
#include <stdio.h>

static gpointer worker(gpointer data) {
  gint *counter = (gint *) data;
  g_atomic_int_inc(counter);
  return NULL;
}

static void pool_job(gpointer item, gpointer user) {
  (void) item;
  g_atomic_int_inc((gint *) user);
}

int main(void) {
  gint counter = 0;
  GThread *threads[4];
  for (int i = 0; i < 4; i++) threads[i] = g_thread_new("w", worker, &counter);
  for (int i = 0; i < 4; i++) g_thread_join(threads[i]);

  gint pool_count = 0;
  GThreadPool *pool = g_thread_pool_new(pool_job, &pool_count, 3, FALSE, NULL);
  for (int i = 0; i < 6; i++) g_thread_pool_push(pool, GINT_TO_POINTER(i + 1), NULL);
  g_thread_pool_free(pool, FALSE, TRUE);  /* wait for all jobs */

  int threads_ok = g_atomic_int_get(&counter) == 4;
  int pool_ok = g_atomic_int_get(&pool_count) == 6;
  printf("GLIB-THREADS: g_thread=%d/4 pool=%d/6\n", g_atomic_int_get(&counter), g_atomic_int_get(&pool_count));
  printf("GLIB-THREADS: %s\n", (threads_ok && pool_ok) ? "PASS" : "FAIL");
  return 0;
}
