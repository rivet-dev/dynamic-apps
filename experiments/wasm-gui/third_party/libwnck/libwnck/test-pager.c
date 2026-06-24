/* vim: set sw=2 et: */

#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static int n_rows = 1;
static gboolean only_current = FALSE;
static gboolean rtl = FALSE;
static gboolean show_name = FALSE;
static gboolean vertical = FALSE;

static GOptionEntry entries[] = {
	{"n-rows", 'n', 0, G_OPTION_ARG_INT, &n_rows, "Use N_ROWS rows", "N_ROWS"},
	{"only-current", 'c', 0, G_OPTION_ARG_NONE, &only_current, "Only show current workspace", NULL},
	{"rtl", 'r', 0, G_OPTION_ARG_NONE, &rtl, "Use RTL as default direction", NULL},
	{"show-name", 's', 0, G_OPTION_ARG_NONE, &show_name, "Show workspace names instead of workspace contents", NULL},
	{"vertical-orientation", 'v', 0, G_OPTION_ARG_NONE, &vertical, "Use a vertical orientation", NULL},
	{NULL }
};

static void
create_pager_window (GtkOrientation orientation,
		     gboolean       show_all,
		     WnckPagerDisplayMode mode,
		     int n_rows)
{
  GtkWidget *win;
  GtkWidget *pager;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_stick (GTK_WINDOW (win));
#if 0
  wnck_gtk_window_set_dock_type (GTK_WINDOW (win));
#endif

  gtk_window_set_title (GTK_WINDOW (win), "Pager");

  /* very very random */
  gtk_window_move (GTK_WINDOW (win), 0, 0);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  pager = wnck_pager_new ();

  wnck_pager_set_show_all (WNCK_PAGER (pager), show_all);
  wnck_pager_set_display_mode (WNCK_PAGER (pager), mode);
  wnck_pager_set_orientation (WNCK_PAGER (pager), orientation);
  wnck_pager_set_n_rows (WNCK_PAGER (pager), n_rows);
  wnck_pager_set_shadow_type (WNCK_PAGER (pager), GTK_SHADOW_IN);

  gtk_container_add (GTK_CONTAINER (win), pager);

  gtk_widget_show_all (win);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  GtkOrientation  orientation;
  WnckPagerDisplayMode mode;
  WnckScreen *screen;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  gtk_init (&argc, &argv);

  if (rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  screen = wnck_screen_get_default ();

  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);

  if (vertical)
	  orientation = GTK_ORIENTATION_VERTICAL;
  else
	  orientation = GTK_ORIENTATION_HORIZONTAL;

  if (show_name)
	  mode = WNCK_PAGER_DISPLAY_NAME;
  else
	  mode = WNCK_PAGER_DISPLAY_CONTENT;

  create_pager_window (orientation, !only_current, mode, n_rows);

  gtk_main ();

  return 0;
}
