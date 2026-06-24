/* vim: set sw=2 et: */

#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static gboolean display_all = FALSE;
static gboolean never_group = FALSE;
static gboolean always_group = FALSE;
static gboolean rtl = FALSE;
static gboolean skip_tasklist = FALSE;
static gboolean transparent = FALSE;
static gboolean vertical = FALSE;
static gint icon_size = WNCK_DEFAULT_MINI_ICON_SIZE;

static GOptionEntry entries[] = {
	{"always-group", 'g', 0, G_OPTION_ARG_NONE, &always_group, "Always group windows", NULL},
	{"never-group", 'n', 0, G_OPTION_ARG_NONE, &never_group, "Never group windows", NULL},
	{"display-all", 'a', 0, G_OPTION_ARG_NONE, &display_all, "Display windows from all workspaces", NULL},
	{"icon-size", 'i', 0, G_OPTION_ARG_INT, &icon_size, "Icon size for tasklist", NULL},
	{"rtl", 'r', 0, G_OPTION_ARG_NONE, &rtl, "Use RTL as default direction", NULL},
	{"skip-tasklist", 's', 0, G_OPTION_ARG_NONE, &skip_tasklist, "Don't show window in tasklist", NULL},
	{"vertical", 'v', 0, G_OPTION_ARG_NONE, &vertical, "Show in vertical mode", NULL},
	{"transparent", 't', 0, G_OPTION_ARG_NONE, &transparent, "Enable Transparency", NULL},
	{NULL }
};

static gboolean
window_draw (GtkWidget      *widget,
             cairo_t        *cr,
             gpointer        user_data)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1., 1., 1., .5);
  cairo_fill (cr);

  return FALSE;
}

static void
window_composited_changed (GtkWidget *widget,
                           gpointer   user_data)
{
  gboolean composited;

  composited = gtk_widget_is_composited (widget);

  gtk_widget_set_app_paintable (widget, composited);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  WnckScreen *screen;
  GtkWidget *win;
  GtkWidget *frame;
  GtkWidget *tasklist;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  gtk_init (&argc, &argv);

  if (rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  wnck_set_default_mini_icon_size (icon_size);
  screen = wnck_screen_get_default ();

  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 100);
  gtk_window_stick (GTK_WINDOW (win));
  /*   wnck_gtk_window_set_dock_type (GTK_WINDOW (win)); */

  gtk_window_set_title (GTK_WINDOW (win), "Task List");
  gtk_window_set_resizable (GTK_WINDOW (win), TRUE);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  tasklist = wnck_tasklist_new ();

  wnck_tasklist_set_include_all_workspaces (WNCK_TASKLIST (tasklist), display_all);
  if (always_group)
    wnck_tasklist_set_grouping (WNCK_TASKLIST (tasklist),
                                WNCK_TASKLIST_ALWAYS_GROUP);
  else if (never_group)
    wnck_tasklist_set_grouping (WNCK_TASKLIST (tasklist),
                                WNCK_TASKLIST_NEVER_GROUP);
  else
    wnck_tasklist_set_grouping (WNCK_TASKLIST (tasklist),
                                WNCK_TASKLIST_AUTO_GROUP);

  wnck_tasklist_set_middle_click_close (WNCK_TASKLIST (tasklist), TRUE);

  wnck_tasklist_set_orientation (WNCK_TASKLIST (tasklist),
                                 (vertical ? GTK_ORIENTATION_VERTICAL :
                                             GTK_ORIENTATION_HORIZONTAL));

  if (transparent)
    {
      GdkVisual *visual;

      visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (win));

      if (visual != NULL)
        {
          gtk_widget_set_visual (win, visual);

          g_signal_connect (win, "composited-changed",
                            G_CALLBACK (window_composited_changed),
                            NULL);

          /* draw even if we are not app-painted.
           * this just makes my life a lot easier :)
           */
          g_signal_connect (win, "draw",
                            G_CALLBACK (window_draw),
                            NULL);

          window_composited_changed (win, NULL);
        }

        wnck_tasklist_set_button_relief (WNCK_TASKLIST (tasklist),
                                         GTK_RELIEF_NONE);
    }

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (win), frame);

  gtk_container_add (GTK_CONTAINER (frame), tasklist);

  gtk_widget_show (tasklist);
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
