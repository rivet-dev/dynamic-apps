/* Minimal repro of the xfconfd busy-spin: a pure GDBus client that connects to the session bus
 * (g_bus_get_sync, which spawns the GDBusWorker thread + its own GMainContext — exactly xfconfd's
 * setup) and then sits in a PERSISTENT g_main_loop_run (like xfconfd owning its name and serving).
 * xfconfd spins both threads on a kernel wakeup pipe that's perpetually readable but never read; this
 * isolates whether a bare GDBus connection + idle persistent loop reproduces that spin, with far fewer
 * fds than full xfconfd so the culprit pipe is identifiable. A 12s timeout quits the loop.
 */
#include <gio/gio.h>
#include <stdio.h>

static GMainLoop *loop;

static gboolean quit_cb(gpointer d) {
  (void) d;
  fprintf(stderr, "GDBUS-LOOP: 12s elapsed -> quit\n"); fflush(stderr);
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

int main(void) {
  GError *err = NULL;
  fprintf(stderr, "GDBUS-LOOP: connecting to session bus\n"); fflush(stderr);
  GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
  if (!conn) {
    fprintf(stderr, "GDBUS-LOOP: g_bus_get_sync FAILED: %s\n", err ? err->message : "(null)");
    return 1;
  }
  fprintf(stderr, "GDBUS-LOOP: connected, unique name = %s; entering persistent main loop\n",
          g_dbus_connection_get_unique_name(conn));
  fflush(stderr);
  loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add(12000, quit_cb, NULL);
  g_main_loop_run(loop);
  printf("GDBUS-LOOP: done\n");
  return 0;
}
