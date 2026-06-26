/*  $Id$
 *
 *  Copyright © 2004 German Poo-Caaman~o <gpoo@ubiobio.cl>
 *  Copyright © 2005,2006 Daniel Bobadilla Leal <dbobadil@dcc.uchile.cl>
 *  Copyright © 2005 Jasper Huijsmans <jasper@xfce.org>
 *  Copyright © 2006 Jani Monoses <jani@ubuntu.com>
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
t */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "libscreenshooter.h"

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define SCREENSHOT_ICON_NAME  "org.xfce.screenshooter"

/* Struct containing all panel plugin data */
typedef struct
{
  XfcePanelPlugin *plugin;

  GtkWidget *button;
  GtkWidget *image;

  int style_id;
  ScreenshotData *sd;
}
PluginData;



/* Protoypes */

static void
screenshooter_plugin_construct       (XfcePanelPlugin      *plugin);

static void
screenshooter_plugin_read_rc_file    (XfcePanelPlugin      *plugin,
                                      PluginData           *pd);

static void
screenshooter_plugin_write_rc_file   (XfcePanelPlugin      *plugin,
                                      PluginData           *pd);

static gboolean
cb_set_size                          (XfcePanelPlugin      *plugin,
                                      int                   size,
                                      PluginData           *pd);

static void
cb_properties_dialog                 (XfcePanelPlugin      *plugin,
                                      PluginData           *pd);

static void
cb_dialog_response                   (GtkWidget            *dlg,
                                      int                   response,
                                      PluginData           *pd);

static void
cb_free_data                         (XfcePanelPlugin      *plugin,
                                      PluginData           *pd);

static void
cb_button_clicked                    (GtkWidget            *button,
                                      PluginData           *pd);

static gboolean
cb_button_scrolled                   (GtkWidget            *widget,
                                      GdkEventScroll       *event,
                                      PluginData *pd);

static void
cb_style_set                         (XfcePanelPlugin      *plugin,
                                      gpointer              ignored,
                                      PluginData           *pd);

static void
set_panel_button_tooltip             (PluginData           *pd);



/* Internal functions */

/* Modify the size of the panel button
Returns TRUE if succesful.
*/
static gboolean
cb_set_size (XfcePanelPlugin *plugin, int size, PluginData *pd)
{
  gint icon_size;

  /* reduce the size of the plugin to a single row */
  size /= xfce_panel_plugin_get_nrows (plugin);

  TRACE ("Request size for the plugin");
  gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);

  icon_size = xfce_panel_plugin_get_icon_size (plugin);
  gtk_image_set_pixel_size (GTK_IMAGE (pd->image), icon_size);

  return TRUE;
}



/* Free the panel plugin data stored in pd
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
cb_free_data (XfcePanelPlugin *plugin, PluginData *pd)
{
  if (pd->style_id)
    g_signal_handler_disconnect (plugin, pd->style_id);

  pd->style_id = 0;
  g_free (pd->sd->screenshot_dir);
  g_free (pd->sd->title);
  g_free (pd->sd->app);
  g_free (pd->sd->last_user);
  g_free (pd->sd->last_extension);
  g_free (pd->sd);
  g_free (pd);
}



/* Take the screenshot when the button is clicked.
button: the panel button.
pd: the PluginData storing the options for taking the screenshot.
*/
static void
cb_button_clicked (GtkWidget *button, PluginData *pd)
{
  /* Make the button unclickable so that the user does not press it while
  another screenshot is in progress */
  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

  TRACE ("Start taking the screenshot");

  screenshooter_take_screenshot (pd->sd, TRUE);
}



static gboolean cb_button_scrolled (GtkWidget *widget,
                                    GdkEventScroll *event,
                                    PluginData *pd)
{
  switch (event->direction)
    {
      case GDK_SCROLL_UP:
      case GDK_SCROLL_RIGHT:

#ifdef ENABLE_WAYLAND
        if (!GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
          {
             pd->sd->region += 1;
             if (pd->sd->region > SELECT)
               pd->sd->region = FULLSCREEN;
          }
#else
        pd->sd->region += 1;
        if (pd->sd->region > SELECT)
          pd->sd->region = FULLSCREEN;
#endif

        set_panel_button_tooltip (pd);
        gtk_widget_trigger_tooltip_query (pd->button);
        return TRUE;
      case GDK_SCROLL_DOWN:
      case GDK_SCROLL_LEFT:

#ifdef ENABLE_WAYLAND
        if (!GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
          {
            pd->sd->region -= 1;
            if (pd->sd->region == REGION_0)
              pd->sd->region = SELECT;
          }
#else
        pd->sd->region -= 1;
        if (pd->sd->region == REGION_0)
          pd->sd->region = SELECT;
#endif

        set_panel_button_tooltip (pd);
        gtk_widget_trigger_tooltip_query (pd->button);
        return TRUE;
      default:
        return FALSE;
    }

  return FALSE;
}



/* Set the style of the panel plugin.
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
cb_style_set (XfcePanelPlugin *plugin, gpointer ignored, PluginData *pd)
{
  cb_set_size (plugin, xfce_panel_plugin_get_size (plugin), pd);
}



/* Read the rc file associated to the panel plugin and store the options in pd.
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
screenshooter_plugin_read_rc_file (XfcePanelPlugin *plugin, PluginData *pd)
{
  gchar *rc_file = xfce_panel_plugin_lookup_rc_file (plugin);

  screenshooter_read_rc_file (rc_file, pd->sd);
  g_free (rc_file);
}



/* Write the pd options in the rc file associated to plugin
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
screenshooter_plugin_write_rc_file (XfcePanelPlugin *plugin, PluginData *pd)
{
  gchar *rc_file = xfce_panel_plugin_save_location (plugin, TRUE);

  screenshooter_write_rc_file (rc_file, pd->sd);
  g_free (rc_file);
}



/* Callback for dialog response:
   Update the tooltips.
   Unblock the plugin contextual menu.
   Save the options in the rc file.*/
static void
cb_dialog_response (GtkWidget *dlg, int response, PluginData *pd)
{
  if (response == GTK_RESPONSE_PREFERENCES)
    {
      screenshooter_preference_dialog_run (dlg);
      return;
    }

  g_object_set_data (G_OBJECT (pd->plugin), "dialog", NULL);
  gtk_widget_destroy (dlg);

  /* Unblock the menu */
  xfce_panel_plugin_unblock_menu (pd->plugin);

  if (response == GTK_RESPONSE_OK)
    {
      /* Update tooltips according to the chosen option */
      set_panel_button_tooltip (pd);

      /* Save options */
      screenshooter_plugin_write_rc_file (pd->plugin, pd);
    }
  else if (response == GTK_RESPONSE_HELP)
    screenshooter_open_help (NULL);
}



/* Properties dialog to set the plugin options */
static void
cb_properties_dialog (XfcePanelPlugin *plugin, PluginData *pd)
{
  GtkWidget *dlg;

  TRACE ("Create the dialog");
  dlg = screenshooter_region_dialog_new (pd->sd, TRUE);

  /* Block the menu to prevent the user from launching several dialogs at
  the same time */
  TRACE ("Block the menu");
  xfce_panel_plugin_block_menu (plugin);

  TRACE ("Run the dialog");
  g_object_set_data (G_OBJECT (plugin), "dialog", dlg);
  g_signal_connect (dlg, "response", G_CALLBACK (cb_dialog_response), pd);
  g_signal_connect (dlg, "key-press-event",
                    (GCallback) screenshooter_f1_key, NULL);

  gtk_widget_show (dlg);
}



static void
set_panel_button_tooltip (PluginData *pd)
{
  if (pd->sd->region == FULLSCREEN)
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                   _("Take a screenshot of the entire screen"));
    }
  else if (pd->sd->region == ACTIVE_WINDOW)
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                   _("Take a screenshot of the active window"));
    }
  else if (pd->sd->region == SELECT)
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                   _("Select a region to be captured by clicking a "
                                     "point of the screen without releasing the mouse "
                                     "button, dragging your mouse to the other corner "
                                     "of the region, and releasing the mouse button."));
    }
}



/* Called when the screenshooter flow is finalized, successful or not
action_executed: whether the action was executed successfully.
data: what was defined in sd->finalize_callback_data, in this case PluginData is expected.
*/
static void
cb_finalize (gboolean action_executed, gpointer data)
{
  PluginData *pd = data;

  TRACE ("Execute finalize callback");

  /* Make the panel button clickable */
  gtk_widget_set_sensitive (GTK_WIDGET (pd->button), TRUE);

  /* Update preferences if the action was executed successfully */
  if (action_executed)
    screenshooter_plugin_write_rc_file (pd->plugin, pd);
}



/* Create the plugin button */
static void
screenshooter_plugin_construct (XfcePanelPlugin *plugin)
{
  /* Initialise the data structs */
  gint icon_size;
  PluginData *pd = g_new0 (PluginData, 1);
  ScreenshotData *sd = g_new0 (ScreenshotData, 1);
  sd->app_info = NULL;
  sd->path_is_dir = TRUE;

  pd->sd = sd;
  pd->plugin = plugin;

  /* make the plugin fit a single row */
  xfce_panel_plugin_set_small (plugin, TRUE);

  TRACE ("Initialize the text domain");
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* Read the options */
  TRACE ("Read the preferences file");
  screenshooter_plugin_read_rc_file (plugin, pd);

  /* We want the actions dialog to be always displayed */
  pd->sd->action_specified = FALSE;

  /* Create the panel button */
  TRACE ("Create the panel button");
  pd->button = xfce_panel_create_button ();

  icon_size = xfce_panel_plugin_get_icon_size (plugin);
  pd->image = gtk_image_new_from_icon_name (SCREENSHOT_ICON_NAME, icon_size);

  gtk_container_add (GTK_CONTAINER (pd->button), GTK_WIDGET (pd->image));

  /* Set the tooltips */
  TRACE ("Set the default tooltip");
  set_panel_button_tooltip (pd);

  TRACE ("Add the button to the panel");
  gtk_container_add (GTK_CONTAINER (plugin), pd->button);
  xfce_panel_plugin_add_action_widget (plugin, pd->button);
  gtk_widget_show_all (pd->button);
  gtk_widget_add_events (pd->button, GDK_SCROLL_MASK);

  /* Set the callbacks */
  g_signal_connect (pd->button, "clicked",
                    G_CALLBACK (cb_button_clicked), pd);
  g_signal_connect (pd->button, "scroll-event",
                    G_CALLBACK (cb_button_scrolled), pd);
  g_signal_connect (plugin, "free-data",
                    G_CALLBACK (cb_free_data), pd);
  g_signal_connect (plugin, "size-changed",
                    G_CALLBACK (cb_set_size), pd);

  pd->style_id = g_signal_connect (plugin, "style-set",
                                   G_CALLBACK (cb_style_set), pd);

  /* Set the configuration menu */
  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (plugin, "configure-plugin",
                    G_CALLBACK (cb_properties_dialog), pd);

  /* Set the callback function */
  pd->sd->finalize_callback = cb_finalize;
  pd->sd->finalize_callback_data = pd;
}
XFCE_PANEL_PLUGIN_REGISTER (screenshooter_plugin_construct);
