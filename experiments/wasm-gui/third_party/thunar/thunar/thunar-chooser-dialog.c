/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-chooser-model.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
  PROP_OPEN,
};



static void        thunar_chooser_dialog_dispose             (GObject                  *object);
static void        thunar_chooser_dialog_get_property        (GObject                  *object,
                                                              guint                     prop_id,
                                                              GValue                   *value,
                                                              GParamSpec               *pspec);
static void        thunar_chooser_dialog_set_property        (GObject                  *object,
                                                              guint                     prop_id,
                                                              const GValue             *value,
                                                              GParamSpec               *pspec);
static void        thunar_chooser_dialog_realize             (GtkWidget                *widget);
static void        thunar_chooser_dialog_response            (GtkDialog                *widget,
                                                              gint                      response);
static gboolean    thunar_chooser_dialog_selection_func      (GtkTreeSelection         *selection,
                                                              GtkTreeModel             *model,
                                                              GtkTreePath              *path,
                                                              gboolean                  path_currently_selected,
                                                              gpointer                  user_data);
static gboolean    thunar_chooser_dialog_context_menu        (ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_update_accept       (ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_update_header       (ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_action_remove       (ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_action_forget       (ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_browse_clicked      (GtkWidget                *button,
                                                              ThunarChooserDialog      *dialog);
static gboolean    thunar_chooser_dialog_button_press_event  (GtkWidget                *tree_view,
                                                              GdkEventButton           *event,
                                                              ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_notify_expanded     (GtkExpander              *expander,
                                                              GParamSpec               *pspec,
                                                              ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_expand              (ThunarChooserDialog      *dialog);
static gboolean    thunar_chooser_dialog_popup_menu          (GtkWidget                *tree_view,
                                                              ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_row_activated       (GtkTreeView              *treeview,
                                                              GtkTreePath              *path,
                                                              GtkTreeViewColumn        *column,
                                                              ThunarChooserDialog      *dialog);
static void        thunar_chooser_dialog_selection_changed   (GtkTreeSelection         *selection,
                                                              ThunarChooserDialog      *dialog);
static ThunarFile *thunar_chooser_dialog_get_file            (ThunarChooserDialog *dialog);
static void        thunar_chooser_dialog_set_file            (ThunarChooserDialog *dialog,
                                                              ThunarFile          *file);
static gboolean    thunar_chooser_dialog_get_open            (ThunarChooserDialog *dialog);
static void        thunar_chooser_dialog_set_open            (ThunarChooserDialog *dialog,
                                                              gboolean             open);


struct _ThunarChooserDialogClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarChooserDialog
{
  ThunarAbstractDialog __parent__;

  ThunarFile  *file;
  gboolean     open;

  GtkWidget   *header_image;
  GtkWidget   *header_label;
  GtkWidget   *tree_view;
  GtkWidget   *custom_expander;
  GtkWidget   *custom_entry;
  GtkWidget   *custom_button;
  GtkWidget   *default_button;
  GtkWidget   *cancel_button;
  GtkWidget   *accept_button;
};



G_DEFINE_TYPE (ThunarChooserDialog, thunar_chooser_dialog, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_chooser_dialog_class_init (ThunarChooserDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_chooser_dialog_dispose;
  gobject_class->get_property = thunar_chooser_dialog_get_property;
  gobject_class->set_property = thunar_chooser_dialog_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_chooser_dialog_realize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_chooser_dialog_response;

  /**
   * ThunarChooserDialog::file:
   *
   * The #ThunarFile for which an application should be chosen.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarChooserDialog::open:
   *
   * Whether the chooser should open the specified file.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_OPEN,
                                   g_param_spec_boolean ("open", "open", "open",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT | EXO_PARAM_READWRITE));
}



static void
thunar_chooser_dialog_init (ThunarChooserDialog *dialog)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkWidget         *header;
  GtkWidget         *hbox;
  GtkWidget         *vbox;
  GtkWidget         *box;
  GtkWidget         *swin;

  /* setup basic window properties */
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Open With"));

  /* create the main widget box */
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 6, "spacing", 12, NULL);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* create the header box */
  header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
  gtk_widget_show (header);

  /* create the header image */
  dialog->header_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (header), dialog->header_image, FALSE, FALSE, 0);
  gtk_widget_show (dialog->header_image);

  /* create the header label */
  dialog->header_label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (dialog->header_label), 0.0f);
  gtk_label_set_line_wrap (GTK_LABEL (dialog->header_label), TRUE);
  gtk_widget_set_size_request (dialog->header_label, 350, -1);
  gtk_box_pack_start (GTK_BOX (header), dialog->header_label, FALSE, FALSE, 0);
  gtk_widget_show (dialog->header_label);

  /* create the view box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);
  gtk_widget_show (box);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (swin, -1, 270);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (box), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  dialog->tree_view = g_object_new (GTK_TYPE_TREE_VIEW, "headers-visible", FALSE, NULL);
  g_signal_connect (G_OBJECT (dialog->tree_view), "button-press-event", G_CALLBACK (thunar_chooser_dialog_button_press_event), dialog);
  g_signal_connect (G_OBJECT (dialog->tree_view), "popup-menu", G_CALLBACK (thunar_chooser_dialog_popup_menu), dialog);
  g_signal_connect (G_OBJECT (dialog->tree_view), "row-activated", G_CALLBACK (thunar_chooser_dialog_row_activated), dialog);
  gtk_container_add (GTK_CONTAINER (swin), dialog->tree_view);
  gtk_widget_show (dialog->tree_view);

  /* append the tree view column */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN, "expand", TRUE, NULL);
  renderer = g_object_new (EXO_TYPE_CELL_RENDERER_ICON, "follow-state", FALSE, "size", 24, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "gicon", THUNAR_CHOOSER_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "style", THUNAR_CHOOSER_MODEL_COLUMN_STYLE,
                                       "text", THUNAR_CHOOSER_MODEL_COLUMN_NAME,
                                       "weight", THUNAR_CHOOSER_MODEL_COLUMN_WEIGHT,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree_view), column);

  /* don't show the expanders */
  g_object_set (G_OBJECT (dialog->tree_view),
                "level-indentation", 24,
                "show-expanders", FALSE,
                NULL);

  /* create the "Custom command" expand */
  dialog->custom_expander = gtk_expander_new_with_mnemonic (_("Use a _custom command:"));
  gtk_widget_set_tooltip_text (dialog->custom_expander, _("Use a custom command for an application that is not "
                                                          "available from the above application list."));
  g_object_bind_property (G_OBJECT (dialog->custom_expander), "expanded",
                          G_OBJECT (dialog->tree_view),       "sensitive",
                          G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  g_signal_connect (G_OBJECT (dialog->custom_expander), "notify::expanded", G_CALLBACK (thunar_chooser_dialog_notify_expanded), dialog);
  gtk_box_pack_start (GTK_BOX (box), dialog->custom_expander, FALSE, FALSE, 0);
  gtk_widget_show (dialog->custom_expander);

  /* create the "Custom command" box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_container_add (GTK_CONTAINER (dialog->custom_expander), hbox);
  gtk_widget_show (hbox);

  /* create the "Custom command" entry */
  dialog->custom_entry = g_object_new (GTK_TYPE_ENTRY, "activates-default", TRUE, NULL);
  g_signal_connect_swapped (G_OBJECT (dialog->custom_entry), "changed", G_CALLBACK (thunar_chooser_dialog_update_accept), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->custom_entry, TRUE, TRUE, 0);
  gtk_widget_show (dialog->custom_entry);

  /* create the "Custom command" button */
  dialog->custom_button = gtk_button_new_with_mnemonic (_("_Browse..."));
  g_signal_connect (G_OBJECT (dialog->custom_button), "clicked", G_CALLBACK (thunar_chooser_dialog_browse_clicked), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->custom_button, FALSE, FALSE, 0);
  gtk_widget_show (dialog->custom_button);

  /* create the "Use as default for this kind of file" button */
  dialog->default_button = gtk_check_button_new_with_mnemonic (_("Use as _default for this kind of file"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->default_button), FALSE);
  g_object_bind_property (G_OBJECT (dialog),                 "open",
                          G_OBJECT (dialog->default_button), "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (box), dialog->default_button, FALSE, FALSE, 0);
  gtk_widget_show (dialog->default_button);

  /* add the "Cancel" button */
  dialog->cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);

  /* add the "Ok"/"Open" button */
  dialog->accept_button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  /* update the "Ok"/"Open" button and the custom entry whenever the tree selection changes */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  gtk_tree_selection_set_select_function (selection, thunar_chooser_dialog_selection_func, dialog, NULL);
  g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (thunar_chooser_dialog_selection_changed), dialog);
}



static void
thunar_chooser_dialog_dispose (GObject *object)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (object);

  /* drop the reference on the file */
  thunar_chooser_dialog_set_file (dialog, NULL);

  (*G_OBJECT_CLASS (thunar_chooser_dialog_parent_class)->dispose) (object);
}



static void
thunar_chooser_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_chooser_dialog_get_file (dialog));
      break;

    case PROP_OPEN:
      g_value_set_boolean (value, thunar_chooser_dialog_get_open (dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_chooser_dialog_set_file (dialog, g_value_get_object (value));
      break;

    case PROP_OPEN:
      thunar_chooser_dialog_set_open (dialog, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_dialog_realize (GtkWidget *widget)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (widget);

  /* let the GtkWindow class realize the dialog */
  (*GTK_WIDGET_CLASS (thunar_chooser_dialog_parent_class)->realize) (widget);

  /* update the dialog header */
  thunar_chooser_dialog_update_header (dialog);
}



static void
thunar_chooser_dialog_response (GtkDialog *widget,
                                gint       response)
{
  GdkAppLaunchContext *context;
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (widget);
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreeIter          iter;
  const gchar         *content_type;
  GAppInfo            *app_info = NULL;
  gboolean             succeed = TRUE;
  GError              *error = NULL;
  const gchar         *custom_command;
  gchar               *name;
  GList                list;
  GList               *all_apps, *lp;
  GdkScreen           *screen;
  GAppInfo            *default_app = NULL;

  /* no special processing for non-accept responses */
  if (G_UNLIKELY (response != GTK_RESPONSE_ACCEPT))
    return;

  /* determine the content type for the file */
  content_type = thunar_file_get_content_type (dialog->file);

  /* determine the application that was chosen by the user */
  if (!gtk_expander_get_expanded (GTK_EXPANDER (dialog->custom_expander)))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);
    }
  else
    {
      custom_command = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));

      /* determine the name of the custom command */
      name = g_path_get_basename (custom_command);

      /* try to add an application for the custom command */
      app_info = g_app_info_create_from_commandline (custom_command, name, G_APP_INFO_CREATE_NONE, &error);

      /* cleanup */
      g_free (name);

      /* verify the application */
      if (G_UNLIKELY (app_info == NULL))
        {
          /* display an error to the user */
          thunar_dialogs_show_error (GTK_WIDGET (dialog), error, _("Failed to add new application \"%s\""), custom_command);

          /* release the error */
          g_error_free (error);
          return;
        }

      /* Check if that application already exists in our list */
      all_apps = g_app_info_get_all ();
      for (lp = all_apps; lp != NULL; lp = lp->next)
        {
          if( g_strcmp0 (g_app_info_get_name (lp->data), g_app_info_get_name (app_info)) == 0 &&
              g_strcmp0 (g_app_info_get_commandline (lp->data), g_app_info_get_commandline (app_info)) == 0)
            {
              /* Re-use existing app-info instead of adding the same one again */
              g_object_unref (app_info);
              app_info = g_object_ref (lp->data);
              break;
            }
        }
      g_list_free_full (all_apps, g_object_unref);
    }

  /* verify that we have a valid application */
  if (G_UNLIKELY (app_info == NULL))
    return;

  default_app = g_app_info_get_default_for_type (content_type, FALSE);

  /* check if we should also set the application as default or
   * if application is opened first time, set it as default application */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->default_button))
      || default_app == NULL)
    {
      /* remember the application as default for these kind of file */
      succeed = g_app_info_set_as_default_for_type (app_info, content_type, &error);

      /* verify that we were successful */
      if (G_UNLIKELY (!succeed))
        {
          /* display an error to the user */
          thunar_dialogs_show_error (GTK_WIDGET (dialog),
                                     error,
                                     _("Failed to set default application for \"%s\""),
                                     thunar_file_get_display_name (dialog->file));

          /* release the error */
          g_error_free (error);
        }

      /* emit "changed" on the file if we successfully changed the default application */
      if (G_LIKELY (succeed))
        thunar_file_changed (dialog->file);
    }
  else
    {
      /* simply try to set the app as last used for this type (we do not show any errors here) */
      if (g_app_info_set_as_last_used_for_type (app_info, content_type, NULL))
        {
          /* emit "changed" on the file if we successfully changed the default application */
          thunar_file_changed (dialog->file);
        }
    }

  if (default_app != NULL)
    g_object_unref (default_app);

  /* check if we should also execute the application */
  if (G_LIKELY (succeed && dialog->open))
    {
      GFile *gfile;
      gchar *uri;

      gfile = thunar_file_get_file (dialog->file); list.next = list.prev = NULL;
      uri = g_file_get_uri (gfile);

      /* create launch context */
      screen = gtk_widget_get_screen (GTK_WIDGET (dialog));
      context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
      gdk_app_launch_context_set_screen (context, screen);
      gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());

      /* create fake file list */
      list.data = gfile;

      gtk_recent_manager_add_item (gtk_recent_manager_get_default(), uri);
      g_free (uri);

      if (!g_app_info_launch (app_info, &list, G_APP_LAUNCH_CONTEXT (context), &error))
        {
          /* display an error to the user */
          thunar_dialogs_show_error (GTK_WIDGET (dialog),
                                     error,
                                     _("Failed to execute application \"%s\""),
                                     g_app_info_get_name (app_info));

          /* release the error */
          g_error_free (error);
        }

      /* destroy the launch context */
      g_object_unref (context);
    }

  /* cleanup */
  g_object_unref (app_info);
}



static gboolean
thunar_chooser_dialog_selection_func (GtkTreeSelection *selection,
                                      GtkTreeModel     *model,
                                      GtkTreePath      *path,
                                      gboolean          path_currently_selected,
                                      gpointer          user_data)
{
  GtkTreeIter iter;
  gboolean    permitted = TRUE;
  GValue      value = { 0, };

  /* we can always change the selection if the path is already selected */
  if (G_UNLIKELY (!path_currently_selected))
    {
      /* check if there's an application for the path */
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get_value (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &value);
      permitted = (g_value_get_object (&value) != NULL);
      g_value_unset (&value);
    }

  return permitted;
}



static gboolean
thunar_chooser_dialog_context_menu (ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkWidget        *item;
  GtkWidget        *menu;
  GAppInfo         *app_info;

  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), FALSE);

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return FALSE;

  /* determine the app info for the row */
  gtk_tree_model_get (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);
  if (G_UNLIKELY (app_info == NULL))
    return FALSE;

  /* prepare the popup menu */
  menu = gtk_menu_new ();

  /* append the "Remove Launcher" item */
  item = gtk_menu_item_new_with_mnemonic (_("_Remove Launcher"));
  gtk_widget_set_sensitive (item, g_app_info_can_delete (app_info));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_chooser_dialog_action_remove), dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* append the "Forget Association" item */
  item = gtk_menu_item_new_with_mnemonic (_("_Forget Association"));
  gtk_widget_set_sensitive (item, g_app_info_can_remove_supports_type (app_info));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_chooser_dialog_action_forget), dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* run the menu (takes over the floating of menu) */
  thunar_gtk_menu_run (GTK_MENU (menu));

  /* clean up */
  g_object_unref (app_info);

  return TRUE;
}



static void
thunar_chooser_dialog_update_accept (ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  const gchar      *text;
  gboolean          sensitive = FALSE;
  GValue            value = { 0, };

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  if (gtk_expander_get_expanded (GTK_EXPANDER (dialog->custom_expander)))
    {
      /* check if the user entered a valid custom command */
      text = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));
      sensitive = (text != NULL && *text != '\0');
    }
  else
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* check if the selected row refers to a valid application */
          gtk_tree_model_get_value (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &value);
          sensitive = (g_value_get_object (&value) != NULL);
          g_value_unset (&value);
        }
    }

  /* update the "Ok"/"Open" button sensitivity */
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, sensitive);
}



static void
thunar_chooser_dialog_update_header (ThunarChooserDialog *dialog)
{
  const gchar *content_type;
  GIcon       *icon;
  gchar       *description;
  gchar       *text;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));
  _thunar_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (dialog)));

  /* check if we have a valid file set */
  if (G_UNLIKELY (dialog->file == NULL))
    {
      gtk_image_clear (GTK_IMAGE (dialog->header_image));
      gtk_label_set_text (GTK_LABEL (dialog->header_label), NULL);
    }
  else
    {
      content_type = thunar_file_get_content_type (dialog->file);
      description = g_content_type_get_description (content_type);

      icon = g_content_type_get_icon (content_type);
      gtk_image_set_from_gicon (GTK_IMAGE (dialog->header_image), icon, GTK_ICON_SIZE_DIALOG);
      g_object_unref (icon);

      /* update the header label */
      text = g_strdup_printf (_("Open <i>%s</i> and other files of type \"%s\" with:"),
                              thunar_file_get_display_name (dialog->file),
                              description);
      gtk_label_set_markup (GTK_LABEL (dialog->header_label), text);
      g_free (text);

      /* update the "Browse..." tooltip */
      thunar_gtk_widget_set_tooltip (dialog->custom_button,
                                     _("Browse the file system to select an "
                                       "application to open files of type \"%s\"."),
                                     description);

      /* update the "Use as default for this kind of file" tooltip */
      thunar_gtk_widget_set_tooltip (dialog->default_button,
                                     _("Change the default application for files "
                                       "of type \"%s\" to the selected application."),
                                     description);

      /* cleanup */
      g_free (description);
    }
}



static void
thunar_chooser_dialog_action_remove (ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  const gchar      *name;
  GtkWidget        *message;
  GAppInfo         *app_info;
  GError           *error = NULL;
  gint              response;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  /* determine the app info for the row */
  gtk_tree_model_get (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);
  if (G_UNLIKELY (app_info == NULL))
    return;

  if (g_app_info_can_delete (app_info))
    {
      /* determine the name of the app info */
      name = g_app_info_get_name (app_info);

      /* ask the user whether to remove the application launcher */
      message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_NONE,
                                        _("Are you sure that you want to remove \"%s\"?"), name);
      gtk_window_set_title (GTK_WINDOW (message), _("Remove application launcher"));
      gtk_dialog_add_buttons (GTK_DIALOG (message),
                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                              _("_Remove"), GTK_RESPONSE_YES,
                              NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (message), GTK_RESPONSE_YES);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("This will remove the application launcher that appears in the file "
                                                                                "context menu, but will not uninstall the application itself.\n\n"
                                                                                "You can only remove application launchers that were created using "
                                                                                "the custom command box in the \"Open With\" dialog of the file "
                                                                                "manager."));
      response = gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);

      /* check if the user confirmed the removal */
      if (G_LIKELY (response == GTK_RESPONSE_YES))
        {
          /* try to delete the application from the model */
          if (!thunar_chooser_model_remove (THUNAR_CHOOSER_MODEL (model), &iter, TRUE, &error))
            {
              /* display an error to the user */
              thunar_dialogs_show_error (dialog, error, _("Failed to remove \"%s\""), name);
              g_error_free (error);
            }
          else if (G_LIKELY (dialog->file != NULL))
            {
              /* emit "changed" for the file, so the context menu is updated */
              thunar_file_changed (dialog->file);
            }
        }
    }

  /* cleanup */
  g_object_unref (app_info);
}



static void
thunar_chooser_dialog_action_forget (ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  const gchar      *name;
  GtkWidget        *message;
  GAppInfo         *app_info;
  const gchar      *content_type;
  GError           *error = NULL;
  gint              response;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  /* determine the app info for the row */
  gtk_tree_model_get (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);
  if (G_UNLIKELY (app_info == NULL))
    return;

  if (g_app_info_can_remove_supports_type (app_info))
    {
      /* determine the name of the app info */
      name = g_app_info_get_name (app_info);

      /* ask the user whether to forget the application launcher */
      message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_NONE,
                                        _("Are you sure that you want to forget \"%s\"?"), name);
      gtk_window_set_title (GTK_WINDOW (message), _("Forget application launcher"));
      gtk_dialog_add_buttons (GTK_DIALOG (message),
                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                              _("_Forget"), GTK_RESPONSE_YES,
                              NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (message), GTK_RESPONSE_YES);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("This will dissociate the application launcher for this file type, "
                                                                                "but will not uninstall or remove the application launcher itself."));
      response = gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);

      /* check if the user confirmed */
      if (G_LIKELY (response == GTK_RESPONSE_YES))
        {

          /* Dont support this mime-type any more with that application */
          content_type = thunar_file_get_content_type (dialog->file);
          g_app_info_remove_supports_type (app_info, content_type, NULL);

          /* try to delete the application from the model */
          if (!thunar_chooser_model_remove (THUNAR_CHOOSER_MODEL (model), &iter, FALSE, &error))
            {
              /* display an error to the user */
              thunar_dialogs_show_error (dialog, error, _("Failed to forget \"%s\""), name);
              g_error_free (error);
            }
          else if (G_LIKELY (dialog->file != NULL))
            {
              /* emit "changed" for the file, so the context menu is updated */
              thunar_file_changed (dialog->file);
            }
        }
    }

  /* cleanup */
  g_object_unref (app_info);
}



static void
thunar_chooser_dialog_browse_clicked (GtkWidget           *button,
                                      ThunarChooserDialog *dialog)
{
  GtkFileFilter *filter;
  GtkWidget     *chooser;
  gchar         *filename;
  gchar         *filename_escaped;
  gchar         *s;

  chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
                                         GTK_WINDOW (dialog),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  /* add file chooser filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* use the bindir as default folder */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* setup the currently selected file */
  filename = gtk_editable_get_chars (GTK_EDITABLE (dialog->custom_entry), 0, -1);
  if (G_LIKELY (filename != NULL))
    {
      /* use only the first argument */
      s = strchr (filename, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* check if we have a file name */
      if (G_LIKELY (*filename != '\0'))
        {
          /* check if the filename is not an absolute path */
          if (G_LIKELY (!g_path_is_absolute (filename)))
            {
              /* try to lookup the filename in $PATH */
              s = g_find_program_in_path (filename);
              if (G_LIKELY (s != NULL))
                {
                  /* use the absolute path instead */
                  g_free (filename);
                  filename = s;
                }
            }

          /* check if we have an absolute path now */
          if (G_LIKELY (g_path_is_absolute (filename)))
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
        }

      /* release the filename */
      g_free (filename);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      filename_escaped = thunar_g_strescape (filename);
      gtk_entry_set_text (GTK_ENTRY (dialog->custom_entry), filename_escaped);
      g_free (filename_escaped);
      g_free (filename);
    }

  gtk_widget_destroy (chooser);
}



static gboolean
thunar_chooser_dialog_button_press_event (GtkWidget           *tree_view,
                                          GdkEventButton      *event,
                                          ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreePath      *path;

  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), FALSE);
  _thunar_return_val_if_fail (dialog->tree_view == tree_view, FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* check if we should popup the context menu */
  if (G_LIKELY (event->button == 3 && event->type == GDK_BUTTON_PRESS))
    {
      /* determine the path for the clicked row */
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL))
        {
          /* be sure to select exactly this row... */
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
          gtk_tree_selection_unselect_all (selection);
          gtk_tree_selection_select_path (selection, path);
          gtk_tree_path_free (path);

          /* ...and popup the context menu */
          return thunar_chooser_dialog_context_menu (dialog);
        }
    }

  return FALSE;
}



static void
thunar_chooser_dialog_notify_expanded (GtkExpander         *expander,
                                       GParamSpec          *pspec,
                                       ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (GTK_IS_EXPANDER (expander));
  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* clear the application selection whenever the expander
   * is expanded to avoid confusion for the user.
   */
  if (gtk_expander_get_expanded (expander))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      gtk_tree_selection_unselect_all (selection);
    }

  /* update the sensitivity of the "Ok"/"Open" button */
  thunar_chooser_dialog_update_accept (dialog);
}



static void
thunar_chooser_dialog_expand (ThunarChooserDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));

  /* expand the first tree view row (the default application) */
  if (G_LIKELY (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->tree_view), path);
      gtk_tree_path_free (path);
    }

  /* expand the second tree view row (the recommended applications) */
  if (G_LIKELY (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter)))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->tree_view), path);
      gtk_tree_path_free (path);
    }

  /* expand the third tree view row (the other applications) */
  if (G_LIKELY (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter)))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->tree_view), path);
      gtk_tree_path_free (path);
    }

  /* reset the cursor */
  if (G_LIKELY (gtk_widget_get_realized (GTK_WIDGET (dialog))))
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog)), NULL);

  /* grab focus to the tree view widget */
  if (G_LIKELY (gtk_widget_get_realized (dialog->tree_view)))
    gtk_widget_grab_focus (dialog->tree_view);
}



static gboolean
thunar_chooser_dialog_popup_menu (GtkWidget           *tree_view,
                                  ThunarChooserDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), FALSE);
  _thunar_return_val_if_fail (dialog->tree_view == tree_view, FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* popup the context menu */
  return thunar_chooser_dialog_context_menu (dialog);
}



static void
thunar_chooser_dialog_row_activated (GtkTreeView         *treeview,
                                     GtkTreePath         *path,
                                     GtkTreeViewColumn   *column,
                                     ThunarChooserDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GValue        value = { 0, };

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));
  _thunar_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  /* determine the current chooser model */
  model = gtk_tree_view_get_model (treeview);
  if (G_UNLIKELY (model == NULL))
    return;

  /* determine the application for the tree path */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get_value (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &value);

  /* check if the row refers to a valid application */
  if (G_LIKELY (g_value_get_object (&value) != NULL))
    {
      /* emit the accept dialog response */
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    }
  else if (gtk_tree_view_row_expanded (treeview, path))
    {
      /* collapse the path that were double clicked */
      gtk_tree_view_collapse_row (treeview, path);
    }
  else
    {
      /* expand the path that were double clicked */
      gtk_tree_view_expand_to_path (treeview, path);
    }

  /* cleanup */
  g_value_unset (&value);
}



static void
thunar_chooser_dialog_selection_changed (GtkTreeSelection    *selection,
                                         ThunarChooserDialog *dialog)
{
  GAppInfo     *app_info;
  GtkTreeModel *model;
  const gchar  *exec;
  GtkTreeIter   iter;

  /* determine the iterator for the selected row */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the app info for the selected row */
      gtk_tree_model_get (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);

      /* determine the command for the app info */
      exec = g_app_info_get_executable (app_info);
      if (G_LIKELY (exec != NULL && g_utf8_validate (exec, -1, NULL)))
        {
          /* setup the command as default for the custom command box */
          gtk_entry_set_text (GTK_ENTRY (dialog->custom_entry), exec);
        }

      /* cleanup */
      g_object_unref (app_info);
    }

  /* update the sensitivity of the "Ok"/"Open" button */
  thunar_chooser_dialog_update_accept (dialog);
}



/**
 * thunar_chooser_dialog_get_file:
 * @dialog : a #ThunarChooserDialog.
 *
 * Returns the #ThunarFile currently associated with @dialog or
 * %NULL if there's no such association.
 *
 * Return value: the #ThunarFile associated with @dialog.
 **/
static ThunarFile*
thunar_chooser_dialog_get_file (ThunarChooserDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), NULL);
  return dialog->file;
}



/**
 * thunar_chooser_dialog_set_file:
 * @dialog : a #ThunarChooserDialog.
 * @file   : a #ThunarFile or %NULL.
 *
 * Associates @dialog with @file.
 **/
static void
thunar_chooser_dialog_set_file (ThunarChooserDialog *dialog,
                                ThunarFile          *file)
{
  ThunarChooserModel *model;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* disconnect from the previous file */
  if (G_LIKELY (dialog->file != NULL))
    {
      /* unset the chooser model */
      gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view), NULL);

      /* disconnect us from the file */
      g_signal_handlers_disconnect_by_func (G_OBJECT (dialog->file), gtk_widget_destroy, dialog);
      thunar_file_unwatch (THUNAR_FILE (dialog->file));
      g_object_unref (G_OBJECT (dialog->file));
    }

  /* activate the new file */
  dialog->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the file */
      g_object_ref (G_OBJECT (file));

      /* watch the file for changes */
      thunar_file_watch (dialog->file);

      /* destroy the chooser dialog if the file is deleted */
      g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (gtk_widget_destroy), dialog);

      /* allocate the new chooser model */
      model = thunar_chooser_model_new (thunar_file_get_content_type (file));
      gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view), GTK_TREE_MODEL (model));
      thunar_chooser_dialog_expand (dialog);
      g_object_unref (G_OBJECT (model));
    }

  /* update the header */
  if (gtk_widget_get_realized (GTK_WIDGET (dialog)))
    thunar_chooser_dialog_update_header (dialog);

  /* notify listeners */
  g_object_notify (G_OBJECT (dialog), "file");
}



/**
 * thunar_chooser_dialog_get_open:
 * @dialog : a #ThunarChooserDialog.
 *
 * Tells whether the chooser @dialog should open the file.
 *
 * Return value: %TRUE if @dialog should open the file.
 **/
static gboolean
thunar_chooser_dialog_get_open (ThunarChooserDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), FALSE);
  return dialog->open;
}



/**
 * thunar_chooser_dialog_set_open:
 * @dialog : a #ThunarChooserDialog.
 * @open   : %TRUE if the chooser @dialog should open the file.
 *
 * Sets whether the chooser @dialog should open the file.
 **/
static void
thunar_chooser_dialog_set_open (ThunarChooserDialog *dialog,
                                gboolean             open)
{
  _thunar_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* apply the new state */
  dialog->open = open;

  /* change the accept button label text */
  gtk_button_set_label (GTK_BUTTON (dialog->accept_button), open ? _("_Open") : _("_OK"));

  /* notify listeners */
  g_object_notify (G_OBJECT (dialog), "open");
}



/**
 * thunar_show_chooser_dialog:
 * @parent                      : the #GtkWidget or the #GdkScreen on which to open the
 *                                dialog. May also be %NULL in which case the default
 *                                #GdkScreen will be used.
 * @file                        : the #ThunarFile for which an application should be chosen.
 * @open                        : whether to also open the @file.
 * @preselect_default_checkbox  : Check the checkbox by default and
 *                                set the title "Set Default Application" of chooser dialog box.
 *
 * Convenience function to display a #ThunarChooserDialog with the
 * given parameters.
 *
 * If @parent is a #GtkWidget the chooser dialog will be opened as
 * modal dialog above the @parent. Else if @parent is a screen (if
 * @parent is %NULL the default screen is used), the dialog won't
 * be modal and it will simply popup on the specified screen.
 **/
void
thunar_show_chooser_dialog (gpointer    parent,
                            ThunarFile *file,
                            gboolean    open,
                            gboolean    preselect_default_checkbox)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GtkWidget         *dialog;
  GtkWidget         *window = NULL;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the screen for the dialog */
  if (G_UNLIKELY (parent == NULL))
    {
      /* just use the default screen, no toplevel window */
      screen = gdk_screen_get_default ();
    }
  else if (GTK_IS_WIDGET (parent))
    {
      /* use the screen for the widget and the toplevel window */
      screen = gtk_widget_get_screen (parent);
      window = gtk_widget_get_toplevel (parent);
    }
  else
    {
      /* parent is a screen, no toplevel window */
      screen = GDK_SCREEN (parent);
    }

  /* display the chooser dialog */
  dialog = g_object_new (THUNAR_TYPE_CHOOSER_DIALOG,
                         "file", file,
                         "open", open,
                         "screen", screen,
                         NULL);

  /* change title of chooser dialog box and checked the checkbox when it is used for select default application */
  if (preselect_default_checkbox)
  {
    gtk_window_set_title (GTK_WINDOW (dialog), _("Set Default Application"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (THUNAR_CHOOSER_DIALOG (dialog)->default_button), preselect_default_checkbox);
  }

  /* check if we have a toplevel window */
  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* dialog is transient for toplevel window and modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    }

  /* destroy the dialog after a user interaction */
  g_signal_connect_after (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), NULL);

  /* let the application handle the dialog */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* display the dialog */
  gtk_widget_show (dialog);
}




