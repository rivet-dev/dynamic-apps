/* XU6 notifyd trigger: a minimal libnotify client that RETRIES until xfce4-notifyd has owned
 * org.freedesktop.Notifications (notifyd makes no X window to settle on, so we cannot gate on X-quiet). */
#include <libnotify/notify.h>
#include <glib.h>
#include <stdio.h>
int main(void) {
  if (!notify_init("secure-exec")) { fprintf(stderr, "NOTIFY-SENDER: notify_init failed\n"); return 1; }
  NotifyNotification *n = notify_notification_new(
    "Hello from secure-exec",
    "A real Xubuntu notification, all wasm in the sandbox.",
    "dialog-information");
  notify_notification_set_timeout(n, 60000);
  for (int i = 0; i < 40; i++) {
    GError *err = NULL;
    if (notify_notification_show(n, &err)) { fprintf(stderr, "NOTIFY-SENDER: notification sent (try %d)\n", i); return 0; }
    fprintf(stderr, "NOTIFY-SENDER: try %d failed: %s\n", i, err ? err->message : "?");
    g_clear_error(&err);
    g_usleep(1000000);
  }
  fprintf(stderr, "NOTIFY-SENDER: gave up\n");
  return 2;
}
