/* Closer repro for the xfconfd busy-spin: TWO persistent GMainContexts (main + a worker thread that
 * runs its OWN non-default context in g_main_loop_run) exchanging wakeups continuously — GDBus's
 * GDBusWorker pattern, which plain g_main_context_invoke (glib-invoke-test, PASSES) does NOT exercise.
 *
 * main: runs the DEFAULT context loop. worker: runs its OWN context loop. They ping-pong: each side,
 * on receiving a ping, schedules a ping back to the other context (which must WAKE that context's
 * blocked poll via its GWakeup). If the cross-context wakeup spins/stalls the way xfconfd does, we'll
 * see it (low PINGS reached + high CPU/poll). A 6s timeout bounds a hang.
 */
#include <glib.h>
#include <stdio.h>

static GMainContext *worker_ctx;
static GMainLoop *worker_loop;
static GMainLoop *main_loop;
static volatile int pings = 0;
static volatile int timed_out = 0;
static const int TARGET = 2000;

static gboolean ping_to_worker(gpointer d);

/* runs ON MAIN's default context */
static gboolean ping_to_main(gpointer d) {
  (void) d;
  pings++;
  if (pings >= TARGET) {
    g_main_loop_quit(main_loop);
    g_main_loop_quit(worker_loop);
    return G_SOURCE_REMOVE;
  }
  /* schedule a ping back onto the WORKER context (must wake the worker's blocked poll) */
  GSource *s = g_idle_source_new();
  g_source_set_callback(s, ping_to_worker, NULL, NULL);
  g_source_attach(s, worker_ctx);
  g_source_unref(s);
  return G_SOURCE_REMOVE;
}

/* runs ON the WORKER context */
static gboolean ping_to_worker(gpointer d) {
  (void) d;
  /* schedule a ping back onto MAIN's default context (must wake main's blocked poll) */
  GSource *s = g_idle_source_new();
  g_source_set_callback(s, ping_to_main, NULL, NULL);
  g_source_attach(s, NULL); /* NULL = global default == main's context */
  g_source_unref(s);
  return G_SOURCE_REMOVE;
}

static gboolean timeout_cb(gpointer d) {
  (void) d;
  timed_out = 1;
  fprintf(stderr, "TWOCTX: TIMEOUT (ping-pong stalled at %d) -> quit\n", pings); fflush(stderr);
  g_main_loop_quit(main_loop);
  g_main_loop_quit(worker_loop);
  return G_SOURCE_REMOVE;
}

static gpointer worker_main(gpointer d) {
  (void) d;
  worker_ctx = g_main_context_new();
  g_main_context_push_thread_default(worker_ctx);
  worker_loop = g_main_loop_new(worker_ctx, FALSE);
  /* kick off the first ping back to main */
  GSource *s = g_idle_source_new();
  g_source_set_callback(s, ping_to_main, NULL, NULL);
  g_source_attach(s, NULL);
  g_source_unref(s);
  fprintf(stderr, "TWOCTX: worker entering its own-context loop\n"); fflush(stderr);
  g_main_loop_run(worker_loop);
  g_main_context_pop_thread_default(worker_ctx);
  return NULL;
}

int main(void) {
  fprintf(stderr, "TWOCTX: start\n"); fflush(stderr);
  main_loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add(6000, timeout_cb, NULL);
  GThread *t = g_thread_new("w", worker_main, NULL);
  fprintf(stderr, "TWOCTX: main entering default-context loop\n"); fflush(stderr);
  g_main_loop_run(main_loop);
  g_thread_join(t);
  printf("TWOCTX: pings=%d timed_out=%d\n", pings, timed_out);
  printf("TWOCTX: %s\n", (pings >= TARGET && !timed_out) ? "PASS" : "FAIL");
  return 0;
}
