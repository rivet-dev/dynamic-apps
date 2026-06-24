#include <libwnck/libwnck.h>

static void
on_window_opened (WnckScreen *screen,
                  WnckWindow *window,
                  gpointer    data)
{
  /* Note: when this event is emitted while screen is initialized, there is no
   * active window yet. */

  g_print ("%s\n", wnck_window_get_name (window));
}

static void
on_active_window_changed (WnckScreen *screen,
                          WnckWindow *previously_active_window,
                          gpointer    data)
{
  WnckWindow *active_window;

  active_window = wnck_screen_get_active_window (screen);

  if (active_window)
    g_print ("active: %s\n", wnck_window_get_name (active_window));
  else
    g_print ("no active window\n");
}

int
main (int    argc,
      char **argv)
{
  GMainLoop *loop;
  WnckScreen *screen;

  gdk_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  screen = wnck_screen_get_default ();

  g_signal_connect (screen, "window-opened",
                    G_CALLBACK (on_window_opened), NULL);
  g_signal_connect (screen, "active-window-changed",
                    G_CALLBACK (on_active_window_changed), NULL);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  return 0;
}
