/* XU6 notifyd trigger: a minimal libnotify client. notify_init connects to the D-Bus session bus,
 * notify_notification_show sends org.freedesktop.Notifications.Notify -> xfce4-notifyd pops the popup. */
#include <libnotify/notify.h>
#include <stdio.h>
int main(void) {
  if (!notify_init("secure-exec")) { fprintf(stderr, "NOTIFY-SENDER: notify_init failed\n"); return 1; }
  NotifyNotification *n = notify_notification_new(
    "Hello from secure-exec",
    "A real Xubuntu notification, all wasm in the sandbox.",
    "dialog-information");
  notify_notification_set_timeout(n, 30000);
  GError *err = NULL;
  if (!notify_notification_show(n, &err)) {
    fprintf(stderr, "NOTIFY-SENDER: show failed: %s\n", err ? err->message : "?"); return 2;
  }
  fprintf(stderr, "NOTIFY-SENDER: notification sent\n");
  return 0;
}
