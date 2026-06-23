/* Tier (e): cross-thread GMainContext dispatch — the libfm FmJob pattern that blocks pcmanfm.
 *
 * A worker GThread does some work, then calls g_main_context_invoke() to post a callback back to
 * the MAIN thread's default GMainContext. The main thread is blocked in g_main_loop_run() and must
 * WAKE (glib does this by writing to its GWakeup eventfd/pipe, which the main thread's poll() is
 * blocked on) to run the callback. This is exactly how FmDirListJob / FmPlacesModel signal job
 * completion back to the GTK main loop.
 *
 * Tiers a-d (pthread join, atomics, g_thread_new/join, GThreadPool) already PASS, so the futex/join
 * primitive works. This isolates the remaining suspect: cross-thread main-context wakeup through the
 * kernel poll. A safety g_timeout_add quits the loop after 5s, so a hang shows up as the invoke
 * callback never firing (timed_out=1, invoked=0) rather than a real wall-clock hang.
 */
#include <glib.h>
#include <stdio.h>

static GMainLoop *loop;
static volatile int invoked = 0;
static volatile int timed_out = 0;

static gboolean invoke_cb(gpointer d) {
  (void) d;
  fprintf(stderr, "GMI: invoke_cb ran ON MAIN THREAD -> quit\n"); fflush(stderr);
  invoked = 1;
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

static gboolean timeout_cb(gpointer d) {
  (void) d;
  fprintf(stderr, "GMI: TIMEOUT fired (cross-thread invoke NEVER arrived) -> quit\n"); fflush(stderr);
  timed_out = 1;
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

static gpointer worker(gpointer d) {
  (void) d;
  fprintf(stderr, "GMI: worker running; sleep 200ms then g_main_context_invoke()\n"); fflush(stderr);
  g_usleep(200 * 1000);
  g_main_context_invoke(NULL, invoke_cb, NULL);
  fprintf(stderr, "GMI: worker posted invoke, exiting\n"); fflush(stderr);
  return NULL;
}

int main(void) {
  fprintf(stderr, "GMI: start\n"); fflush(stderr);
  loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add(5000, timeout_cb, NULL);
  GThread *t = g_thread_new("w", worker, NULL);
  fprintf(stderr, "GMI: main entering g_main_loop_run (must wake on worker's cross-thread post)\n"); fflush(stderr);
  g_main_loop_run(loop);
  g_thread_join(t);
  printf("GLIB-INVOKE: invoked=%d timed_out=%d\n", invoked, timed_out);
  printf("GLIB-INVOKE: %s\n", (invoked && !timed_out) ? "PASS" : "FAIL");
  return 0;
}
