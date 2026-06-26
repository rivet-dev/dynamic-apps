/*  $Id$
 *
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
 */

#include "screenshooter-dialogs.h"
#include "screenshooter-actions.h"
#include "screenshooter-custom-actions.h"
#include "screenshooter-format.h"

#include <exo/exo.h>

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define ICON_SIZE 16
#define THUMB_X_SIZE 200
#define THUMB_Y_SIZE 125

/* Prototypes */

static void
cb_fullscreen_screen_toggled       (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_active_window_toggled           (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_rectangle_toggled               (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_radiobutton_activate            (GtkToggleButton    *tb,
                                    GtkWidget          *widget);
static void
cb_show_mouse_toggled              (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_save_toggled                    (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_toggle_set_sensi                (GtkToggleButton    *tb,
                                    GtkWidget          *widget);
static void
cb_toggle_set_insensi              (GtkToggleButton    *tb,
                                    GtkWidget          *widget);
static void
cb_open_toggled                    (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_clipboard_toggled               (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_delay_spinner_changed           (GtkWidget          *spinner,
                                    ScreenshotData     *sd);
static void
cb_combo_active_item_changed       (GtkWidget          *box,
                                    ScreenshotData     *sd);
static void
add_item                           (GAppInfo           *app_info,
                                    GtkWidget          *liststore);
static void
populate_liststore                 (GtkListStore       *liststore);
static void
set_default_item                   (GtkWidget          *combobox,
                                    ScreenshotData     *sd);
static GdkPixbuf
*screenshot_get_thumbnail          (GdkPixbuf          *screenshot);
static void
cb_progress_upload                 (goffset             current_num_bytes,
                                    goffset             total_num_bytes,
                                    gpointer            user_data);
static void
cb_finished_upload                 (GObject            *source_object,
                                    GAsyncResult       *res,
                                    gpointer            user_data);
static void
cb_transfer_dialog_response        (GtkWidget          *dialog,
                                    int                 response,
                                    GCancellable       *cancellable);
static gchar
*save_screenshot_to_local_path     (GdkPixbuf          *screenshot,
                                    GFile              *save_file);
static void
save_screenshot_to_remote_location (GdkPixbuf          *screenshot,
                                    GFile              *save_file);
static void
cb_combo_file_extension_changed    (GtkWidget          *box,
                                    GtkWidget          *chooser);
static void
cb_custom_action_tree_selection    (GtkTreeSelection   *selection,
                                    gpointer            user_data);
static void
cb_custom_action_values_changed    (GtkEditable        *editable,
                                    gpointer            user_data);
static void
cb_custom_action_add               (GtkToolButton      *button,
                                    gpointer            user_data);
static void
cb_custom_action_delete            (GtkToolButton      *button,
                                    gpointer            user_data);



/* Internals */



/* Set the captured area when the button is toggled */
static void cb_fullscreen_screen_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->region = FULLSCREEN;
}



/* Set the captured area when the button is toggled */
static void cb_active_window_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->region = ACTIVE_WINDOW;
}



/* Set the captured area when the button is toggled */
static void cb_rectangle_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->region = SELECT;
}



/* Confirm the selected screenshot options and proceed to capture */
static void cb_radiobutton_activate (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);
}



/* Set whether the window border should be captured when the button is toggled */
static void cb_show_border_toggled (GtkToggleButton *tb, ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->show_border = 1;
  else
    sd->show_border = 0;
}



/* Set whether the mouse should be captured when the button is toggled */
static void cb_show_mouse_toggled (GtkToggleButton *tb, ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->show_mouse = 1;
  else
    sd->show_mouse = 0;
}



/* Set the action when the button is toggled */
static void cb_save_toggled (GtkToggleButton *tb, ScreenshotData  *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = SAVE;
}



/* Set the show_in_folder when the button is toggled */
static void cb_show_in_folder_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  sd->show_in_folder = gtk_toggle_button_get_active (tb);
}



/* Set the widget active if the toggle button is active */
static void
cb_toggle_set_sensi (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (tb));
}



/* Set the widget active if the toggle button is inactive */
static void
cb_toggle_set_insensi (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, !gtk_toggle_button_get_active (tb));
}



static void cb_open_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = OPEN;
}



static void cb_custom_action_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = CUSTOM_ACTION;
}



static void cb_clipboard_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = CLIPBOARD;
}



/* Set the delay according to the spinner */
static void cb_delay_spinner_changed (GtkWidget *spinner, ScreenshotData *sd)
{
  sd->delay = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner));
}



/* Set sd->app as per the active item in the combobox */
static void cb_combo_active_item_changed (GtkWidget *box, ScreenshotData *sd)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (box));
  GtkTreeIter iter;
  gchar *active_command = NULL;
  GAppInfo *app_info = NULL;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (box), &iter);
  gtk_tree_model_get (model, &iter, 2, &active_command, -1);
  gtk_tree_model_get (model, &iter, 3, &app_info, -1);

  g_free (sd->app);
  sd->app = active_command;
  sd->app_info = app_info;
}



static void cb_custom_action_combo_active_item_changed (GtkWidget *box, ScreenshotData *sd)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name;
  gchar *command;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (box), &iter);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (box));
  gtk_tree_model_get (model, &iter,
                      CUSTOM_ACTION_NAME, &name,
                      CUSTOM_ACTION_COMMAND, &command,
                      -1);

  g_free (sd->custom_action_name);
  g_free (sd->custom_action_command);

  sd->custom_action_name = name;
  sd->custom_action_command = command;
}



/* Extract the informations from app_info and add them to the
 * liststore.
 * */
static void add_item (GAppInfo *app_info, GtkWidget *liststore)
{
  GtkTreeIter iter;
  const gchar *command = g_app_info_get_executable (app_info);
  const gchar *name = g_app_info_get_name (app_info);
  GIcon *icon = g_app_info_get_icon (app_info);
  GdkPixbuf *pixbuf = NULL;
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
  GtkIconInfo *icon_info;

  icon_info =
    gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                    ICON_SIZE,
                                    GTK_ICON_LOOKUP_FORCE_SIZE);

  pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

  if (G_UNLIKELY (pixbuf == NULL))
    {
      pixbuf = gtk_icon_theme_load_icon (icon_theme, "exec", ICON_SIZE,
                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                         NULL);
    }

  /* Add to the liststore */
  gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);

  gtk_list_store_set (GTK_LIST_STORE (liststore), &iter,
                      0, pixbuf,
                      1, name,
                      2, command,
                      3, g_app_info_dup (app_info),
                      -1);

  /* Free the stuff */
  g_object_unref (pixbuf);
  g_object_unref (icon);
  g_object_unref (icon_info);
}



/* Populate the liststore using the applications which can open image/png. */
static void populate_liststore (GtkListStore *liststore)
{
  const gchar *content_type;
  GList *list_app;

  content_type = "image/png";

  /* Get all applications for image/png.*/
  list_app = g_app_info_get_all_for_type (content_type);

  /* Add them to the liststore */
  if (G_LIKELY (list_app != NULL))
    {
      g_list_foreach (list_app, (GFunc) add_item, liststore);
      g_list_free_full (list_app, g_object_unref);
    }
}



/* Select the sd->app item in the combobox */
static void set_default_item (GtkWidget *combobox, ScreenshotData *sd)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));

  GtkTreeIter iter;

  /* Get the first iter */
  if (G_LIKELY (gtk_tree_model_get_iter_first (model , &iter)))
    {
      gchar *command = NULL;
      gboolean found = FALSE;
      GAppInfo *app_info;

      /* Loop until finding the appropirate item, if any */
      do
        {
          gtk_tree_model_get (model, &iter, 2, &command, -1);
          gtk_tree_model_get (model, &iter, 3, &app_info, -1);

          if (g_str_equal (command, sd->app))
            {
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox),
                                             &iter);
              sd->app_info = app_info;

              found = TRUE;
            }

          g_free (command);
        }
      while (gtk_tree_model_iter_next (model, &iter));

      /* If no suitable item was found, set the first item as active and
       * set sd->app accordingly. */
      if (G_UNLIKELY (!found))
        {
          gtk_tree_model_get_iter_first (model , &iter);
          gtk_tree_model_get (model, &iter, 2, &command, -1);
          gtk_tree_model_get (model, &iter, 3, &app_info, -1);

          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

          g_free (sd->app);

          sd->app = command;
          sd->app_info = app_info;
        }
    }
  else
    {
      g_free (sd->app);

      sd->app = g_strdup ("none");
      sd->app_info = NULL;
    }
}



static void custom_action_load_last_used (GtkWidget *combobox, ScreenshotData *sd)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean found = FALSE;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));

  if (G_UNLIKELY (!gtk_tree_model_get_iter_first (model , &iter)))
    {
      /* No custom action found */

      g_free (sd->custom_action_name);
      g_free (sd->custom_action_command);

      sd->custom_action_name = g_strdup ("none");
      sd->custom_action_command = g_strdup ("none");
    }

  /* Loop until finding the appropriate item, if any */
  do
    {
      gtk_tree_model_get (model, &iter, CUSTOM_ACTION_COMMAND, &command, -1);

      if (g_strcmp0 (command, sd->custom_action_command) == 0)
        {
          gtk_tree_model_get (model, &iter, CUSTOM_ACTION_NAME, &name, -1);
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
          g_free (sd->custom_action_name);
          sd->custom_action_name = name;
          found = TRUE;
        }

      g_free (command);
    }
  while (!found && gtk_tree_model_iter_next (model, &iter));

  /* If no suitable item was found, set the first item as active and
   * set sd->custom_action_name/command accordingly. */
  if (G_UNLIKELY (!found))
    {
      gtk_tree_model_get_iter_first (model , &iter);
      gtk_tree_model_get (model, &iter,
                          CUSTOM_ACTION_NAME, &name,
                          CUSTOM_ACTION_COMMAND, &command,
                          -1);

      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

      g_free (sd->custom_action_name);
      g_free (sd->custom_action_command);

      sd->custom_action_name = name;
      sd->custom_action_command = command;
    }
}



static GdkPixbuf
*screenshot_get_thumbnail (GdkPixbuf *screenshot)
{
  gint w = gdk_pixbuf_get_width (screenshot);
  gint h = gdk_pixbuf_get_height (screenshot);
  gint width = THUMB_X_SIZE;
  gint height = THUMB_Y_SIZE;

  if (G_LIKELY (w >= h))
    height = width * h / w;
  else
    width = height * w / h;

  return gdk_pixbuf_scale_simple (screenshot, width, height, GDK_INTERP_BILINEAR);
}



static void cb_progress_upload (goffset current_num_bytes,
                                goffset total_num_bytes,
                                gpointer user_data)
{
  gdouble fraction = (double) current_num_bytes / (double) total_num_bytes;

  gfloat current = (float) current_num_bytes / 1000;
  gfloat total = (float) total_num_bytes / 1000;

  gchar *bar_text = g_strdup_printf (_("%.2fKb of %.2fKb"), current, total);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (user_data), fraction);

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (user_data), bar_text);

  g_free (bar_text);
}



static void
cb_finished_upload (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  gboolean success;

  g_return_if_fail (G_IS_FILE (source_object));

  success = g_file_copy_finish (G_FILE (source_object), res, &error);

  TRACE ("The transfer is finished");

  if (G_UNLIKELY (!success))
    {
      TRACE ("An error occurred");

      screenshooter_error ("%s", error->message);

      g_error_free (error);
    }

  gtk_widget_destroy (GTK_WIDGET (user_data));
}



static void
cb_transfer_dialog_response (GtkWidget *dialog, int response, GCancellable *cancellable)
{
  if (G_LIKELY (response == GTK_RESPONSE_CANCEL))
    {
      TRACE ("Cancel the screenshot");

      g_cancellable_cancel (cancellable);

      gtk_widget_destroy (dialog);
    }
}



static gchar
*save_screenshot_to_local_path (GdkPixbuf *screenshot, GFile *save_file)
{
  GError *error = NULL;
  gchar *save_path = g_file_get_path (save_file);
  const char *type = "png";
  char** option_keys = NULL;
  char** option_values = NULL;

  for (ImageFormat *format = screenshooter_get_image_formats (); format->type != NULL; format++)
    {
      if (!format->supported) continue;

      if (screenshooter_image_format_match_extension (format, save_path))
        {
          type = format->type;
          option_keys = format->option_keys;
          option_values = format->option_values;
          break;
        }
    }

  /* Restrict file permission if not saved in a user-owned directory */
  screenshooter_restrict_file_permission (save_file);

  if (G_LIKELY (gdk_pixbuf_savev (screenshot, save_path, type, option_keys, option_values, &error)))
    return save_path;

  if (error)
    {
      /* See bug #8443, looks like error is not set when path is NULL */
      screenshooter_error ("%s", error->message);
      g_error_free (error);
    }

  g_free (save_path);

  return NULL;
}

static void
save_screenshot_to_remote_location (GdkPixbuf *screenshot, GFile *save_file)
{
  gchar *save_basename = g_file_get_basename (save_file);
  gchar *save_path = g_build_filename (g_get_tmp_dir (), save_basename, NULL);
  GFile *save_file_temp = g_file_new_for_path (save_path);

  GFile *save_parent = g_file_get_parent (save_file);
  const gchar *parent_uri = g_file_get_uri (save_parent);
  GCancellable *cancellable = g_cancellable_new ();

  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Transfer"),
                                                   NULL,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "gtk-cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   NULL);

  GtkWidget *progress_bar = gtk_progress_bar_new ();
  GtkWidget *label1= gtk_label_new ("");
  GtkWidget *label2 = gtk_label_new (parent_uri);

  save_screenshot_to_local_path (screenshot, save_file_temp);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (dialog), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 20);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "document-save-symbolic");
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 12);

  gtk_label_set_markup (GTK_LABEL (label1),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">The screenshot "
                          "is being transferred to:</span>"));
  gtk_widget_set_halign (label1, GTK_ALIGN_START);
  gtk_widget_set_valign (label1, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      label1,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show (label1);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      label2,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show (label2);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      progress_bar,
                      FALSE,
                      FALSE,
                      0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);
  gtk_widget_show (progress_bar);

  g_signal_connect (dialog, "response", G_CALLBACK (cb_transfer_dialog_response),
                    cancellable);

  g_file_copy_async (save_file_temp,
                     save_file,
                     G_FILE_COPY_OVERWRITE,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     (GFileProgressCallback) cb_progress_upload, progress_bar,
                     (GAsyncReadyCallback) cb_finished_upload, dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));

  g_file_delete (save_file_temp, NULL, NULL);

  g_object_unref (save_file_temp);
  g_object_unref (save_parent);
  g_object_unref (cancellable);
  g_free (save_basename);
  g_free (save_path);
}

static void
preview_drag_begin (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
  GdkPixbuf *pixbuf = data;
  gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
}

static void
preview_drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data,
                       guint info, guint utime, gpointer data)
{
  GdkPixbuf *pixbuf = data;
  gtk_selection_data_set_pixbuf (selection_data, pixbuf);
}

static void
preview_drag_end (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
  /* Try to put ourself on top again */
  GtkWidget *dlg = data;
  gtk_window_present (GTK_WINDOW (dlg));
}

static void
cb_combo_file_extension_changed (GtkWidget *box, GtkWidget *chooser)
{
  gchar *new_filename;
  gchar *filename = gtk_file_chooser_get_current_name (GTK_FILE_CHOOSER (chooser));
  gchar *found = g_strrstr (filename, ".");

  if (found)
  {
    new_filename = g_strndup (filename, strlen (filename) - strlen (found));
    g_free (filename);
    filename = new_filename;
  }

  new_filename = g_strconcat (filename, ".", gtk_combo_box_get_active_id (GTK_COMBO_BOX (box)), NULL);
  g_free (filename);

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), new_filename);

  g_free (new_filename);
}



static void
cb_dialog_response (GtkWidget *dialog, gint response, ScreenshotData *sd)
{
  if (response == GTK_RESPONSE_HELP)
    {
      g_signal_stop_emission_by_name (dialog, "response");
      screenshooter_open_help (GTK_WINDOW (dialog));
    }
  else if (response == GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (dialog);
      screenshooter_take_screenshot (sd, FALSE);
    }
  else if (response == GTK_RESPONSE_PREFERENCES)
    {
      screenshooter_preference_dialog_run (dialog);
    }
  else
    {
      gtk_widget_destroy (dialog);
      sd->finalize_callback (FALSE, sd->finalize_callback_data);
    }
}



static void
cb_custom_action_tree_selection (GtkTreeSelection *selection,
                                 gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name, *cmd;
  CustomActionDialogData *dialog_data = user_data;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          CUSTOM_ACTION_NAME, &name,
                          CUSTOM_ACTION_COMMAND, &cmd,
                          -1);

      gtk_widget_set_sensitive (dialog_data->name, TRUE);
      gtk_entry_set_text (GTK_ENTRY (dialog_data->name), name);

      gtk_widget_set_sensitive (dialog_data->cmd, TRUE);
      gtk_entry_set_text (GTK_ENTRY (dialog_data->cmd), cmd);

      g_free (name);
      g_free (cmd);
    }
  else
    {
      gtk_widget_set_sensitive (dialog_data->name, FALSE);
      gtk_widget_set_sensitive (dialog_data->cmd, FALSE);
    }
}



static void
cb_custom_action_values_changed (GtkEditable *editable,
                                 gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  const gchar *name, *cmd;
  CustomActionDialogData *dialog_data = user_data;

  if (gtk_tree_selection_get_selected (dialog_data->selection, &model, &iter))
    {
      name = gtk_entry_get_text (GTK_ENTRY (dialog_data->name));
      cmd = gtk_entry_get_text (GTK_ENTRY (dialog_data->cmd));

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          CUSTOM_ACTION_NAME, name,
                          CUSTOM_ACTION_COMMAND, cmd,
                          -1);
    }
}



static void
cb_custom_action_add (GtkToolButton *button,
                      gpointer user_data)
{
  GtkTreeIter iter;
  CustomActionDialogData *dialog_data = user_data;

  gtk_list_store_append (dialog_data->liststore, &iter);
  gtk_list_store_set (dialog_data->liststore, &iter, CUSTOM_ACTION_NAME, "", CUSTOM_ACTION_COMMAND, "", -1);
  gtk_tree_selection_select_iter (dialog_data->selection, &iter);
  gtk_entry_set_text (GTK_ENTRY (dialog_data->name), "");
  gtk_entry_set_text (GTK_ENTRY (dialog_data->cmd), "");
  gtk_widget_grab_focus (dialog_data->name);
}



static void
cb_custom_action_delete (GtkToolButton *button,
                         gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  CustomActionDialogData *dialog_data = user_data;

  if (gtk_tree_selection_get_selected (dialog_data->selection, &model, &iter))
    {
      gtk_entry_set_text (GTK_ENTRY (dialog_data->name), "");
      gtk_entry_set_text (GTK_ENTRY (dialog_data->cmd), "");
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}



/* Public */



void screenshooter_region_dialog_show (ScreenshotData *sd, gboolean plugin)
{
  GtkWidget *dialog = screenshooter_region_dialog_new (sd, plugin);
  g_signal_connect (dialog, "response",
                  G_CALLBACK (cb_dialog_response), sd);
  g_signal_connect (dialog, "key-press-event",
                  G_CALLBACK (screenshooter_f1_key), NULL);
  gtk_widget_show (dialog);
  if (gtk_main_level() == 0)
    gtk_main ();
}



GtkWidget *screenshooter_region_dialog_new (ScreenshotData *sd, gboolean plugin)
{
  GtkWidget *dlg, *grid, *main_box, *box, *label, *checkbox;
  GtkWidget *fullscreen_button, *active_window_button, *rectangle_button;
  GtkWidget *spinner_box, *spinner;

  /* Create the dialog */
  if (plugin)
    {
      dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        "", _("_Preferences"), GTK_RESPONSE_PREFERENCES,
        "help-browser-symbolic", _("_Help"), GTK_RESPONSE_HELP,
        "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
        NULL);
    }
  else
    {
      dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        "help-browser-symbolic", _("_Help"), GTK_RESPONSE_HELP,
        "", _("_Preferences"), GTK_RESPONSE_PREFERENCES,
        "", _("_Cancel"), GTK_RESPONSE_CANCEL,
        "", _("_OK"), GTK_RESPONSE_OK,
        NULL);
    }

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 0);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "org.xfce.screenshooter");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);

  /* Create the main box for the dialog */
  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_hexpand (main_box, TRUE);
  gtk_widget_set_vexpand (main_box, TRUE);
  gtk_widget_set_margin_top (main_box, 6);
  gtk_widget_set_margin_bottom (main_box, 0);
  gtk_widget_set_margin_start (main_box, 12);
  gtk_widget_set_margin_end (main_box, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_box), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), main_box, TRUE, TRUE, 0);

  /* Create the grid to align the different parts of the top of the UI */
  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 100);
  gtk_box_pack_start (GTK_BOX (main_box), grid, TRUE, TRUE, 0);

  /* Create the main box for the regions */
  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_grid_attach (GTK_GRID (grid), main_box, 0, 0, 1, 2);

  /* Create area label */
  label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Region to capture</span>"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (main_box), label);

  /* Create area box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);
  gtk_widget_set_margin_top (box, 0);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 0);
  gtk_container_add (GTK_CONTAINER (main_box), box);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);

  /* Create radio buttons for areas to screenshot */

  /* Fullscreen */
  fullscreen_button =
    gtk_radio_button_new_with_mnemonic (NULL, _("Entire screen"));
  gtk_box_pack_start (GTK_BOX (box), fullscreen_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fullscreen_button),
                                (sd->region == FULLSCREEN));
  gtk_widget_set_tooltip_text (fullscreen_button,
                               _("Take a screenshot of the entire screen"));
  g_signal_connect (G_OBJECT (fullscreen_button), "toggled",
                    G_CALLBACK (cb_fullscreen_screen_toggled), sd);
  g_signal_connect (G_OBJECT (fullscreen_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);

  /* Active window */
  active_window_button =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (fullscreen_button),
                                                 _("Active window"));
  gtk_box_pack_start (GTK_BOX (box), active_window_button, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (active_window_button,
                               _("Take a screenshot of the active window"));
  g_signal_connect (G_OBJECT (active_window_button), "toggled",
                    G_CALLBACK (cb_active_window_toggled), sd);
  g_signal_connect (G_OBJECT (active_window_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);
#ifdef ENABLE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      gtk_widget_set_sensitive (active_window_button, FALSE);
      gtk_widget_set_tooltip_text (active_window_button, _("Not supported in Wayland"));
    }
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_window_button), (sd->region == ACTIVE_WINDOW));
#else
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_window_button), (sd->region == ACTIVE_WINDOW));
#endif

  /* Rectangle */
  rectangle_button =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (fullscreen_button),
                                                 _("Select a region"));
  gtk_box_pack_start (GTK_BOX (box), rectangle_button, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (rectangle_button,
                               _("Select a region to be captured by clicking a point of "
                                 "the screen without releasing the mouse button, "
                                 "dragging your mouse to the other corner of the region, "
                                 "and releasing the mouse button.\n\n"
                                 "Press Ctrl while dragging to move the region."));
  g_signal_connect (G_OBJECT (rectangle_button), "toggled",
                    G_CALLBACK (cb_rectangle_toggled), sd);
  g_signal_connect (G_OBJECT (rectangle_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);
#ifdef ENABLE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      gtk_widget_set_sensitive (rectangle_button, FALSE);
      gtk_widget_set_tooltip_text (rectangle_button, _("Not supported in Wayland"));
    }
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rectangle_button), (sd->region == SELECT));
#else
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rectangle_button), (sd->region == SELECT));
#endif

  /* Create options label */
  label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Options</span>"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (main_box), label);

  /* Create options box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);
  gtk_widget_set_margin_top (box, 0);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 0);
  gtk_container_add (GTK_CONTAINER (main_box), box);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);

  /* Create show mouse checkbox */
  checkbox =
    gtk_check_button_new_with_label (_("Capture the mouse pointer"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
                                (sd->show_mouse == 1));
  gtk_widget_set_tooltip_text (checkbox,
                               _("Display the mouse pointer on the screenshot"));
  gtk_box_pack_start (GTK_BOX (box), checkbox, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (checkbox), "toggled",
                    G_CALLBACK (cb_show_mouse_toggled), sd);

  /* Create show border checkbox */
  checkbox =
    gtk_check_button_new_with_label (_("Capture the window border"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
                                (sd->show_border == 1));
  gtk_widget_set_sensitive (checkbox, (sd->region == ACTIVE_WINDOW));
  gtk_widget_set_tooltip_text (checkbox,
                               _("Display the window border on the screenshot.\n"
                                 "Disabling this option has no effect for CSD windows."));
  gtk_box_pack_start (GTK_BOX (box), checkbox, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (checkbox), "toggled",
                    G_CALLBACK (cb_show_border_toggled), sd);
  g_signal_connect (G_OBJECT (fullscreen_button), "toggled",
                    G_CALLBACK (cb_toggle_set_insensi), checkbox);
  g_signal_connect (G_OBJECT (rectangle_button), "toggled",
                    G_CALLBACK (cb_toggle_set_insensi), checkbox);
#ifdef ENABLE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), FALSE);
      gtk_widget_set_sensitive (checkbox, FALSE);
    }
#endif

  /* Create the main box for the delay stuff */
  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_grid_attach (GTK_GRID (grid), main_box, 1, 0, 1, 1);

  /* Create delay label */
  label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL(label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Delay before capturing</span>"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 0);

  /* Create delay box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand (box, FALSE);
  gtk_widget_set_vexpand (box, FALSE);
  gtk_widget_set_margin_top (box, 0);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 0);
  gtk_box_pack_start (GTK_BOX (main_box), box, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);

  /* Create delay spinner */
  spinner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (box), spinner_box, FALSE, FALSE, 0);

  spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), sd->delay);
  gtk_widget_set_tooltip_text (spinner,
                               _("Delay in seconds before the screenshot is taken"));
  gtk_box_pack_start (GTK_BOX (spinner_box), spinner, FALSE, FALSE, 0);

  label = gtk_label_new (_("seconds"));
  gtk_box_pack_start (GTK_BOX (spinner_box), label, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
                    G_CALLBACK (cb_delay_spinner_changed), sd);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dlg)));

  /* Set focus to the selected radio button */
  switch (sd->region)
    {
      case FULLSCREEN:
        gtk_widget_grab_focus (fullscreen_button);
        break;
      case ACTIVE_WINDOW:
        gtk_widget_grab_focus (active_window_button);
        break;
      case SELECT:
        gtk_widget_grab_focus (rectangle_button);
        break;
      default:
        break;
    }

  return dlg;
}



GtkWidget *screenshooter_actions_dialog_new (ScreenshotData *sd)
{
  GtkWidget *dlg, *grid, *box, *evbox, *label, *radio, *checkbox;
  GtkWidget *actions_grid;

  GtkTreeIter iter;
  GtkListStore *liststore;
  GtkWidget *combobox;
  GtkCellRenderer *renderer, *renderer_pixbuf;

  GtkWidget *preview;
  GdkPixbuf *thumbnail;

  dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
    NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
    "help-browser-symbolic", _("_Help"), GTK_RESPONSE_HELP,
    "", _("_Back"), GTK_RESPONSE_REJECT,
    "", _("_Cancel"), GTK_RESPONSE_CANCEL,
    "", _("_OK"), GTK_RESPONSE_OK,
    NULL);

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 0);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "org.xfce.screenshooter");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);

  /* Create the main box for the dialog */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 0);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), box, TRUE, TRUE, 0);

  /* Create the grid to align the two columns of the UI */
  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 20);
  gtk_box_pack_start (GTK_BOX (box), grid, TRUE, TRUE, 0);

  /* Create the box which containes the left part of the UI */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);
  gtk_widget_set_margin_top (box, 0);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 0);
  gtk_grid_attach (GTK_GRID (grid), box, 0, 0, 1, 1);

  /* Create actions label */
  label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">Action"
                          "</span>"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  /* Create the actions box */
  actions_grid = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (box), actions_grid, TRUE, TRUE, 0);

  gtk_grid_set_row_spacing (GTK_GRID (actions_grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (actions_grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (actions_grid), 0);

  /* Save option radio button */
  radio = gtk_radio_button_new_with_mnemonic (NULL, _("Save"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), (sd->action & SAVE));
  g_signal_connect (G_OBJECT (radio), "toggled",
                    G_CALLBACK (cb_save_toggled), sd);
  g_signal_connect (G_OBJECT (radio), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);
  gtk_widget_set_tooltip_text (radio, _("Save the screenshot to a file"));
  gtk_grid_attach (GTK_GRID (actions_grid), radio, 0, 0, 1, 1);

  /* Show in folder checkbox */
  checkbox = gtk_check_button_new_with_label (_("Show in Folder"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), sd->show_in_folder);
  gtk_widget_set_margin_start (checkbox, 25);
  g_signal_connect (G_OBJECT (checkbox), "toggled",
                    G_CALLBACK (cb_show_in_folder_toggled), sd);
  g_signal_connect (G_OBJECT (radio), "toggled",
                    G_CALLBACK (cb_toggle_set_sensi), checkbox);
  gtk_widget_set_tooltip_text (checkbox, _("Shows the saved file in the folder"));
  gtk_grid_attach (GTK_GRID (actions_grid), checkbox, 0, 1, 1, 1);

  if (gdk_display_supports_clipboard_persistence (gdk_display_get_default ()))
    {
      /* Copy to clipboard radio button */
      radio =
        gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio),
                                                     _("Copy to the clipboard"));
      gtk_widget_set_tooltip_text (radio,
                                   _("Copy the screenshot to the clipboard so that it can be "
                                     "pasted later"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio),
                                    (sd->action & CLIPBOARD));
      g_signal_connect (G_OBJECT (radio), "toggled",
                        G_CALLBACK (cb_clipboard_toggled), sd);
      g_signal_connect (G_OBJECT (radio), "activate",
                        G_CALLBACK (cb_radiobutton_activate), dlg);
      gtk_grid_attach (GTK_GRID (actions_grid), radio, 0, 2, 1, 1);
    }

  /* Open with radio button */
  radio =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio),
                                                 _("Open with:"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio),
                                (sd->action & OPEN));
  g_signal_connect (G_OBJECT (radio), "toggled",
                    G_CALLBACK (cb_open_toggled), sd);
  g_signal_connect (G_OBJECT (radio), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);
  gtk_widget_set_tooltip_text (radio,
                               _("Open the screenshot with the chosen application"));
  gtk_grid_attach (GTK_GRID (actions_grid), radio, 0, 3, 1, 1);

  /* Open with combobox */
  liststore = gtk_list_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_APP_INFO);
  combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (liststore));
  renderer = gtk_cell_renderer_text_new ();
  renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer_pixbuf, FALSE);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer, "text", 1, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer_pixbuf,
                                  "pixbuf", 0, NULL);
  populate_liststore (liststore);
  set_default_item (combobox, sd);
  gtk_grid_attach (GTK_GRID (actions_grid), combobox, 1, 3, 1, 1);

  g_signal_connect (G_OBJECT (combobox), "changed",
                    G_CALLBACK (cb_combo_active_item_changed), sd);
  gtk_widget_set_tooltip_text (combobox, _("Application to open the screenshot"));
  gtk_widget_set_sensitive (combobox, sd->action & OPEN);
  g_signal_connect (G_OBJECT (radio), "toggled",
                    G_CALLBACK (cb_toggle_set_sensi), combobox);

  /* Check if there are any custom actions */
  liststore = gtk_list_store_new (CUSTOM_ACTION_N_COLUMN, G_TYPE_STRING, G_TYPE_STRING);
  screenshooter_custom_action_load (liststore);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter))
    {
      /* Custom Actions radio button */
      radio =
        gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio),
                                                    _("Custom Action:"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio),
                                    (sd->action & CUSTOM_ACTION));
      g_signal_connect (G_OBJECT (radio), "toggled",
                        G_CALLBACK (cb_custom_action_toggled), sd);
      g_signal_connect (G_OBJECT (radio), "activate",
                        G_CALLBACK (cb_radiobutton_activate), dlg);
      gtk_widget_set_tooltip_text (radio,
                                  _("Execute the selected custom action"));
      gtk_grid_attach (GTK_GRID (actions_grid), radio, 0, 4, 1, 1);

      /* Custom Actions combobox */
      combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (liststore));
      renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer, "text", CUSTOM_ACTION_NAME, NULL);
      gtk_grid_attach (GTK_GRID (actions_grid), combobox, 1, 4, 1, 1);

      custom_action_load_last_used (combobox, sd);

      gtk_widget_set_tooltip_text (combobox, _("Custom action to execute"));
      gtk_widget_set_sensitive (combobox, sd->action & CUSTOM_ACTION);
      g_signal_connect (G_OBJECT (combobox), "changed",
                        G_CALLBACK (cb_custom_action_combo_active_item_changed), sd);
      g_signal_connect (G_OBJECT (radio), "toggled",
                        G_CALLBACK (cb_toggle_set_sensi), combobox);
    }
  else
    {
      g_object_unref (liststore);
    }

  /* Run the callback functions to grey/ungrey the correct widgets */
  cb_toggle_set_sensi (GTK_TOGGLE_BUTTON (radio), combobox);

  /* Preview box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);
  gtk_grid_attach (GTK_GRID (grid), box, 1, 0, 1, 1);

  /* Preview label*/
  label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Preview</span>"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  /* The preview image */
  thumbnail = screenshot_get_thumbnail (sd->screenshot);
  evbox = gtk_event_box_new ();
  preview = gtk_image_new_from_pixbuf (thumbnail);
  g_object_unref (thumbnail);
  gtk_container_add (GTK_CONTAINER (evbox), preview);
  gtk_box_pack_start (GTK_BOX (box), evbox, FALSE, FALSE, 0);

  /* DND for the preview image */
  gtk_drag_source_set (evbox, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_source_add_image_targets (evbox);
  g_signal_connect (evbox, "drag-begin", G_CALLBACK (preview_drag_begin), thumbnail);
  g_signal_connect (evbox, "drag-data-get", G_CALLBACK (preview_drag_data_get), sd->screenshot);
  g_signal_connect (evbox, "drag-end", G_CALLBACK (preview_drag_end), dlg);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dlg)));

  return dlg;
}



/* Saves the @screenshot in the given @directory using
 * @title and @timestamp to generate the file name.
 *
 * @screenshot: a GdkPixbuf containing the screenshot.
 * @directory: the save location.
 * @title: the title of the screenshot.
 * @timestamp: whether the date and the hour should be added to
 * the file name.
 * @save_dialog: whether a save dialog should be displayed to
 * let the user set a custom save location
 * @show_preview: if @save_dialog is true, @show_preview will
 * decide whether the save dialog should display a preview of
 * @screenshot.
 *
 * Returns: a string containing the path to the saved file.
 */
gchar
*screenshooter_save_screenshot (GdkPixbuf *screenshot,
                                const gchar *directory,
                                const gchar *filename,
                                const gchar *extension,
                                gboolean save_dialog,
                                gboolean show_preview)
{
  gchar *result;
  gchar *save_uri = g_build_filename (directory, filename, NULL);

  if (save_dialog)
  {
    GtkWidget *chooser, *combobox;
    gint dialog_response;

    chooser =
      gtk_file_chooser_dialog_new (_("Save screenshot as..."),
                                   NULL,
                                   GTK_FILE_CHOOSER_ACTION_SAVE,
                                   "gtk-cancel",
                                   GTK_RESPONSE_CANCEL,
                                   "gtk-save",
                                   GTK_RESPONSE_ACCEPT,
                                   NULL);

    gtk_window_set_icon_name (GTK_WINDOW (chooser), "org.xfce.screenshooter");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser),
                                                    TRUE);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
    gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (chooser), directory);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), filename);

    combobox = gtk_combo_box_text_new ();

    for (ImageFormat *format = screenshooter_get_image_formats (); format->type != NULL; format++)
      {
        if (!format->supported) continue;
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combobox), format->extensions[0], format->name);
      }

    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combobox), extension);
    g_signal_connect (combobox, "changed", G_CALLBACK (cb_combo_file_extension_changed), chooser);
    gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (chooser), combobox);

    if (show_preview)
      exo_gtk_file_chooser_add_thumbnail_preview (GTK_FILE_CHOOSER (chooser));

    dialog_response = gtk_dialog_run (GTK_DIALOG (chooser));

    if (G_LIKELY (dialog_response == GTK_RESPONSE_ACCEPT))
      {
        g_free (save_uri);
        save_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
        result = screenshooter_save_screenshot_to (screenshot, save_uri);
      }
    else
      result = NULL;

    gtk_widget_destroy (chooser);
  }
  else
    result = screenshooter_save_screenshot_to (screenshot, save_uri);

  g_free (save_uri);

  return result;
}

gchar
*screenshooter_save_screenshot_to (GdkPixbuf   *screenshot,
                                   const gchar *save_uri)
{
  GFile *save_file = NULL;
  gchar *result = NULL;

  g_return_val_if_fail (save_uri != NULL, NULL);

  save_file = g_file_new_for_uri (save_uri);

  /* If the URI is a local one, we save directly */
  if (!screenshooter_is_remote_uri (save_uri))
    result = save_screenshot_to_local_path (screenshot, save_file);
  else
    save_screenshot_to_remote_location (screenshot, save_file);

  g_object_unref (save_file);

  return result;
}



static GtkWidget
*screenshooter_preference_dialog_new (CustomActionDialogData *dialog_data, GtkWidget *parent)
{
  GtkWidget *dlg, *mbox, *label, *grid, *image, *frame, *hbox, *box;
  GtkWidget *toolbar, *name, *cmd, *tree_view, *scrolled_window;
  GtkToolButton *tool_button;

  GtkListStore *liststore;
  GtkTreeViewColumn *tree_col;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;

  dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Preferences"),
    GTK_WINDOW (parent), GTK_DIALOG_MODAL,
    "", _("_Close"), GTK_RESPONSE_CLOSE,
    NULL);

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (dlg), 380, -1);
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 0);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "org.xfce.screenshooter");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_CLOSE);

  /* Create the main box for the dialog */
  mbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_hexpand (mbox, TRUE);
  gtk_widget_set_vexpand (mbox, TRUE);
  gtk_widget_set_margin_top (mbox, 6);
  gtk_widget_set_margin_bottom (mbox, 0);
  gtk_widget_set_margin_start (mbox, 12);
  gtk_widget_set_margin_end (mbox, 12);
  gtk_container_set_border_width (GTK_CONTAINER (mbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), mbox, TRUE, TRUE, 0);

  /* Create custom actions label */
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">Custom Actions"
                          "</span>"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

  /* Create the grid to show information */
  grid = gtk_grid_new ();
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (grid), 0);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_widget_set_margin_end (GTK_WIDGET (grid), 12);
  gtk_box_pack_start (GTK_BOX (mbox), grid, TRUE, TRUE, 0);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 0);

  image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DND);
  label = gtk_label_new (_("You can configure custom actions that will be available to handle screenshots after they are captured."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), image, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

  /* Create the box to show custom actions and toolbar */
  frame = gtk_frame_new (NULL);
  gtk_widget_set_margin_top (frame, 6);
  gtk_widget_set_margin_bottom (frame, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_widget_set_vexpand (hbox, TRUE);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_box_pack_start (GTK_BOX (mbox), frame, FALSE, FALSE, 0);

  /* Add box for custom actions */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 200);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (scrolled_window), box);
  gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);

  /* Liststore and tree view for custom actions */
  liststore = dialog_data->liststore;
  tree_view = dialog_data->tree_view = gtk_tree_view_new ();
  tree_col = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_set_title (tree_col, _("Custom Action"));
  gtk_tree_view_column_pack_start(tree_col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(tree_col, renderer, "text", CUSTOM_ACTION_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), tree_col);
  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (liststore));
  gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (tree_view), TRUE, TRUE, 0);

  /* Add toolbar and its buttons */
  toolbar =  gtk_toolbar_new();
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_VERTICAL);
  tool_button = GTK_TOOL_BUTTON (gtk_tool_button_new (NULL, NULL));
  gtk_widget_set_tooltip_text (GTK_WIDGET (tool_button), _("Add custom action"));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (tool_button), "list-add-symbolic");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (tool_button), -1);
  g_signal_connect (G_OBJECT (tool_button), "clicked",
                    G_CALLBACK (cb_custom_action_add),
                    dialog_data);
  tool_button = GTK_TOOL_BUTTON (gtk_tool_button_new (NULL, NULL));
  gtk_widget_set_tooltip_text (GTK_WIDGET (tool_button), _("Remove selected custom action"));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (tool_button), "list-remove-symbolic");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (tool_button), -1);
  gtk_box_pack_end (GTK_BOX (hbox), toolbar, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (tool_button), "clicked",
                    G_CALLBACK (cb_custom_action_delete),
                    dialog_data);

  /* Add grid to show details of the custom action */
  grid = gtk_grid_new ();
  gtk_widget_set_margin_top (GTK_WIDGET (grid), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (grid), 0);
  gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
  gtk_widget_set_margin_end (GTK_WIDGET (grid), 12);
  gtk_box_pack_start (GTK_BOX (mbox), grid, TRUE, TRUE, 0);

  gtk_widget_set_vexpand (GTK_WIDGET (grid), TRUE);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 0);

  /* Add custom action name */
  label = gtk_label_new (_("Name"));
  gtk_widget_set_tooltip_text (label, _("Name of the action that will be displayed in Actions dialog"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  name = dialog_data->name = gtk_entry_new ();
  gtk_widget_set_sensitive (name, FALSE);
  gtk_widget_set_hexpand (name, TRUE);
  gtk_grid_attach (GTK_GRID (grid), name, 1, 0, 1, 1);

  /* Add custom action command */
  label = gtk_label_new (_("Command"));
  gtk_widget_set_tooltip_text (label, _("Command that will be executed for this custom action"));
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  cmd = dialog_data->cmd = gtk_entry_new ();
  gtk_widget_set_sensitive (cmd, FALSE);
  gtk_widget_set_hexpand (cmd, TRUE);
  gtk_grid_attach (GTK_GRID (grid), cmd, 1, 1, 1, 1);

  label = gtk_label_new (_("Use \%f as a placeholder for location of the screenshot captured"));
  gtk_widget_set_hexpand (label, TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

  /* Attach signal to update text for name and command when tree selection changes */
  selection = dialog_data->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_object_ref (G_OBJECT (selection));
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (cb_custom_action_tree_selection),
                    dialog_data);

  /* Attach signals on name and command to update model accordingly */
  g_signal_connect (G_OBJECT (name), "changed",
                    G_CALLBACK (cb_custom_action_values_changed),
                    dialog_data);
  g_signal_connect (G_OBJECT (cmd), "changed",
                    G_CALLBACK (cb_custom_action_values_changed),
                    dialog_data);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dlg)));

  return dlg;
}



void
screenshooter_preference_dialog_run (GtkWidget *parent)
{
  GtkWidget *dlg;
  CustomActionDialogData *dialog_data;

  dialog_data= g_new0 (CustomActionDialogData, 1);
  dialog_data->liststore = gtk_list_store_new (CUSTOM_ACTION_N_COLUMN, G_TYPE_STRING, G_TYPE_STRING);
  screenshooter_custom_action_load (dialog_data->liststore);

  dlg = screenshooter_preference_dialog_new (dialog_data, parent);
  gtk_dialog_run (GTK_DIALOG (dlg));
  screenshooter_custom_action_save (GTK_TREE_MODEL (dialog_data->liststore));

  gtk_widget_destroy (dlg);
  g_free (dialog_data);
}
