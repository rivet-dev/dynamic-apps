/* vim: set sw=2 et: */

#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static gboolean skip_tasklist = FALSE;

static GOptionEntry entries[] = {
        /* Translators: "tasklist" is the list of running applications (the
         * window list) */
	{"skip-tasklist", 's', 0, G_OPTION_ARG_NONE, &skip_tasklist, "Don't show window in tasklist", NULL},
	{NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  WnckScreen *screen;
  GtkWidget *win;
  GtkWidget *frame;
  GtkWidget *selector;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  gtk_init (&argc, &argv);

  screen = wnck_screen_get_default ();

  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 32);
  gtk_window_stick (GTK_WINDOW (win));
  /*   wnck_gtk_window_set_dock_type (GTK_WINDOW (win)); */

  gtk_window_set_title (GTK_WINDOW (win), "Window Selector");
  gtk_window_set_resizable (GTK_WINDOW (win), TRUE);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  selector = wnck_selector_new ();

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (win), frame);

  gtk_container_add (GTK_CONTAINER (frame), selector);

  gtk_widget_show (selector);
  gtk_widget_show (frame);

  gtk_window_move (GTK_WINDOW (win), 0, 0);

  if (skip_tasklist)
  {
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (win), TRUE);
    gtk_window_set_keep_above (GTK_WINDOW (win), TRUE);
  }

  gtk_widget_show (win);

  gtk_main ();

  return 0;
}
