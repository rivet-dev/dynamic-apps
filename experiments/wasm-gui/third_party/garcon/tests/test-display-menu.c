/*-
 * vi:set et ai sts=2 sw=2 cindent:
 *
 * Copyright (c) 2007 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <garcon/garcon.h>
#include <garcon-gtk/garcon-gtk.h>



/* Default icon size */
#define ICON_SIZE 22



/* Root menu */
static GarconMenu *root = NULL;
static GtkWidget  *gtk_root = NULL;




static void
show_menu (GtkButton *button,
           GtkWidget *menu)
{
  /* Display the menu */
  gtk_menu_popup_at_widget (GTK_MENU (menu), GTK_WIDGET (button), GDK_GRAVITY_EAST, GDK_GRAVITY_SOUTH, gtk_get_current_event ());
}



static void
main_window_destroy (GtkWidget *window)
{
  gtk_main_quit ();
}



static void
generic_names_toggled (GtkToggleButton *togglebutton,
                       gpointer         user_data)
{
  garcon_gtk_menu_set_show_generic_names (GARCON_GTK_MENU (gtk_root),
                                          gtk_toggle_button_get_active (togglebutton));
}

static void
menu_icons_toggled (GtkToggleButton *togglebutton,
                    gpointer         user_data)
{
  garcon_gtk_menu_set_show_menu_icons (GARCON_GTK_MENU (gtk_root),
                                       gtk_toggle_button_get_active (togglebutton));
}

static void
tooltips_toggled (GtkToggleButton *togglebutton,
                  gpointer         user_data)
{
  garcon_gtk_menu_set_show_tooltips (GARCON_GTK_MENU (gtk_root),
                                     gtk_toggle_button_get_active (togglebutton));
}

static void
desktop_actions_toggled (GtkToggleButton *togglebutton,
                         gpointer         user_data)
{
  garcon_gtk_menu_set_show_desktop_actions (GARCON_GTK_MENU (gtk_root),
                                            gtk_toggle_button_get_active (togglebutton));
}

static void
edit_launchers_toggled (GtkToggleButton *togglebutton,
                        gpointer         user_data)
{
  garcon_gtk_menu_set_right_click_edits (GARCON_GTK_MENU (gtk_root),
                                         gtk_toggle_button_get_active (togglebutton));
}



static void
create_main_window (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *chk_generic_names, *chk_menu_icons;
  GtkWidget *chk_tooltips, *chk_desktop_actions;
  GtkWidget *chk_edit_launchers;

  /* Create main window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GarconMenu: Display Menu Test");
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  gtk_widget_show (window);

  /* Exit main loop when when the window is closed */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (main_window_destroy), NULL);

  /* Create the box to hold all the stuff */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  /* Create button */
  button = gtk_button_new_with_mnemonic ("_Show menu");
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_widget_show (button);

  /* Create Garcon's root menu to show the applications */
  gtk_root = garcon_gtk_menu_new (NULL);
  garcon_gtk_menu_set_menu (GARCON_GTK_MENU (gtk_root), root);

  /* Display root menu when the button is clicked */
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (show_menu), gtk_root);

  /* Create checkbuttons for the garcon-gtk options */

  /* generic names */
  chk_generic_names = gtk_check_button_new_with_mnemonic ("Show _generic names");
  gtk_container_add (GTK_CONTAINER (box), chk_generic_names);
  gtk_widget_show (chk_generic_names);
  /* have check button match garcon-gtk's default */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_generic_names), FALSE);
  g_signal_connect (G_OBJECT (chk_generic_names), "toggled", G_CALLBACK (generic_names_toggled), NULL);

  /* menu icons */
  chk_menu_icons = gtk_check_button_new_with_mnemonic ("Show menu _icons");
  gtk_container_add (GTK_CONTAINER (box), chk_menu_icons);
  gtk_widget_show (chk_menu_icons);
  /* have check button match garcon-gtk's default */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_menu_icons), TRUE);
  g_signal_connect (G_OBJECT (chk_menu_icons), "toggled", G_CALLBACK (menu_icons_toggled), NULL);

  /* tooltips */
  chk_tooltips = gtk_check_button_new_with_mnemonic ("Show _tooltips");
  gtk_container_add (GTK_CONTAINER (box), chk_tooltips);
  gtk_widget_show (chk_tooltips);
  /* have check button match garcon-gtk's default */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_tooltips), FALSE);
  g_signal_connect (G_OBJECT (chk_tooltips), "toggled", G_CALLBACK (tooltips_toggled), NULL);

  /* desktop actions */
  chk_desktop_actions = gtk_check_button_new_with_mnemonic ("Show _desktop actions");
  gtk_container_add (GTK_CONTAINER (box), chk_desktop_actions);
  gtk_widget_show (chk_desktop_actions);
  /* have check button match garcon-gtk's default */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_desktop_actions), FALSE);
  g_signal_connect (G_OBJECT (chk_desktop_actions), "toggled", G_CALLBACK (desktop_actions_toggled), NULL);

  /* edit launchers */
  chk_edit_launchers = gtk_check_button_new_with_mnemonic ("Right click edit launchers");
  gtk_container_add (GTK_CONTAINER (box), chk_edit_launchers);
  gtk_widget_show (chk_edit_launchers);
  /* have check button match garcon-gtk's default */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_edit_launchers), FALSE);
  g_signal_connect (G_OBJECT (chk_edit_launchers), "toggled", G_CALLBACK (edit_launchers_toggled), NULL);
}



gint
main (gint    argc,
      gchar **argv)
{
  gint    exit_code = EXIT_SUCCESS;

  /* Initialize the menu library */
  garcon_set_environment ("XFCE");

  /* Initialize GTK+ */
  gtk_init (&argc, &argv);

  /* Try to load the menu */
  if (G_UNLIKELY (g_strv_length (argv) > 1))
    root = garcon_menu_new_for_path (argv[1]);
  else
    root = garcon_menu_new_applications ();

  /* create the main window */
  create_main_window ();

  /* Enter main loop */
  gtk_main ();

  /* Destroy the root menu */
  g_object_unref (root);

  return exit_code;
}
