/*
 *  Copyright (c) 2019 Simon Steinbeiß <simon@xfce.org>
 *                     Florian Schüller <florian.schueller@gmail.com>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <colord.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <gdk/gdkx.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "color-device.h"
#include "color-profile.h"
#include "color-dialog_ui.h"



static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};



typedef
struct _ColorSettings
{
    CdClient      *client;
    CdDevice      *current_device;
    GPtrArray     *devices;
    GCancellable  *cancellable;
    GDBusProxy    *proxy;
    GObject       *dialog;
    GObject       *plug_child;
    GObject       *label_no_devices;
    GObject       *scrolled_devices;
    GObject       *device_icon;
    GObject       *model;
    GObject       *vendor;
    GObject       *colorspace;
    GObject       *device_calibrate;
    GtkListBox    *list_box;
    gchar         *list_box_filter;
    guint          list_box_selected_id;
    guint          list_box_activated_id;
    GtkSizeGroup  *list_box_size;
    GObject       *label_no_profiles;
    GObject       *scrolled_profiles;
    GObject       *profiles_enable;
    GObject       *profiles_add;
    GObject       *profiles_remove;
    GObject       *profiles_info;
    GtkListBox    *profiles_list_box;
    gchar         *profiles_list_box_filter;
    guint          profiles_list_box_selected_id;
    guint          profiles_list_box_activated_id;
    GtkSizeGroup  *profiles_list_box_size;
    GObject       *dialog_assign;
    GObject       *treeview_assign;
    GObject       *liststore_assign;
    GObject       *button_assign_import;
    GObject       *button_assign_info;
    GObject       *button_assign_ok;
    GObject       *button_assign_cancel;
} ColorSettings;



enum {
    COLOR_SETTINGS_COMBO_COLUMN_TEXT,
    COLOR_SETTINGS_COMBO_COLUMN_PROFILE,
    COLOR_SETTINGS_COMBO_COLUMN_TYPE,
    COLOR_SETTINGS_COMBO_COLUMN_WARNING_FILENAME,
    COLOR_SETTINGS_COMBO_COLUMN_NUM_COLUMNS
};



static void color_settings_make_profile_default_cb (GObject       *object,
                                                    GAsyncResult  *res,
                                                    ColorSettings *settings);
static void color_settings_device_changed_cb       (CdDevice      *device,
                                                    ColorSettings *settings);
ColorSettings * color_settings_dialog_init         (GtkBuilder    *builder);


static GFile *
color_settings_file_chooser_get_icc_profile (ColorSettings *settings)
{
    GtkWindow *window;
    GtkWidget *dialog;
    GFile *file = NULL;
    GtkFileFilter *filter;

    /* create new dialog */
    window = GTK_WINDOW (settings->dialog_assign);
    /* TRANSLATORS: an ICC profile is a file containing colorspace data */
    dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("_Import"), GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), g_get_home_dir ());
    gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

    /* setup the filter */
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter, "application/vnd.iccprofile");

    /* TRANSLATORS: filter name on the file->open dialog */
    gtk_file_filter_set_name (filter, _("Supported ICC profiles"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

    /* setup the all files filter */
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    /* TRANSLATORS: filter name on the file->open dialog */
    gtk_file_filter_set_name (filter, _("All files"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

    /* did user choose file */
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
        file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER(dialog));

    /* we're done */
    gtk_widget_destroy (dialog);

    /* or NULL for missing */
    return file;
}



static void
color_settings_liststore_add_profile (ColorSettings *settings,
                                      CdProfile *profile,
                                      GtkTreeIter *iter)
{
    const gchar *id;
    GtkTreeIter iter_tmp;
    g_autoptr(GString) string = NULL;
    gchar *escaped = NULL;
    guint kind = 0;
    const gchar *warning = NULL;
    gchar **warnings;

    /* iter is optional */
    if (iter == NULL)
        iter = &iter_tmp;

    /* use description */
    string = g_string_new (cd_profile_get_title (profile));

    /* any source prefix? */
    id = cd_profile_get_metadata_item (profile,
                                       CD_PROFILE_METADATA_DATA_SOURCE);
    if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0) {
        /* TRANSLATORS: this is a profile prefix to signify the
         * profile has been auto-generated for this hardware */
        g_string_prepend (string, _("Default: "));
        kind = 1;
    }
    if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0) {
        /* TRANSLATORS: this is a profile prefix to signify the
         * profile his a standard space like AdobeRGB */
        g_string_prepend (string, _("Colorspace: "));
        kind = 2;
    }
    if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0) {
        /* TRANSLATORS: this is a profile prefix to signify the
         * profile is a test profile */
        g_string_prepend (string, _("Test profile: "));
        kind = 3;
    }

    /* is the profile faulty */
    warnings = cd_profile_get_warnings (profile);
    if (warnings != NULL && warnings[0] != NULL)
        warning = "dialog-warning-symbolic";

    escaped = g_markup_escape_text (string->str, -1);
    gtk_list_store_append (GTK_LIST_STORE (settings->liststore_assign), iter);
    gtk_list_store_set (GTK_LIST_STORE (settings->liststore_assign), iter,
                        COLOR_SETTINGS_COMBO_COLUMN_TEXT, escaped,
                        COLOR_SETTINGS_COMBO_COLUMN_PROFILE, profile,
                        COLOR_SETTINGS_COMBO_COLUMN_TYPE, kind,
                        COLOR_SETTINGS_COMBO_COLUMN_WARNING_FILENAME, warning,
                        -1);
}



static void
color_settings_add_profiles_columns (ColorSettings *settings,
                                     GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* text */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_add_attribute (column, renderer,
                                        "markup", COLOR_SETTINGS_COMBO_COLUMN_TEXT);
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_column_set_title (column, _("Compatible Profiles"));
    gtk_tree_view_append_column (treeview, column);

    /* image */
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_add_attribute (column, renderer,
                                        "icon-name", COLOR_SETTINGS_COMBO_COLUMN_WARNING_FILENAME);
    gtk_tree_view_append_column (treeview, column);
}



static gboolean
color_settings_profile_exists_in_array (GPtrArray *array, CdProfile *profile)
{
    CdProfile *profile_tmp;
    guint i;

    for (i = 0; i < array->len; i++) {
        profile_tmp = g_ptr_array_index (array, i);
        if (cd_profile_equal (profile, profile_tmp))
           return TRUE;
    }
    return FALSE;
}



static gboolean
color_settings_is_profile_suitable_for_device (CdProfile *profile,
                                               CdDevice *device)
{
    const gchar *data_source;
    CdProfileKind profile_kind_tmp;
    CdProfileKind profile_kind;
    CdColorspace profile_colorspace;
    CdColorspace device_colorspace = 0;
    CdDeviceKind device_kind;
    CdStandardSpace standard_space;

    /* not the right colorspace */
    device_colorspace = cd_device_get_colorspace (device);
    profile_colorspace = cd_profile_get_colorspace (profile);
    if (device_colorspace != profile_colorspace)
        return FALSE;

    /* if this is a display matching with one of the standard spaces that displays
     * could emulate, also mark it as suitable */
    if (cd_device_get_kind (device) == CD_DEVICE_KIND_DISPLAY &&
        cd_profile_get_kind (profile) == CD_PROFILE_KIND_DISPLAY_DEVICE) {
        data_source = cd_profile_get_metadata_item (profile,
                                                    CD_PROFILE_METADATA_STANDARD_SPACE);
        standard_space = cd_standard_space_from_string (data_source);
        if (standard_space == CD_STANDARD_SPACE_SRGB ||
            standard_space == CD_STANDARD_SPACE_ADOBE_RGB) {
            return TRUE;
        }
    }

    /* not the correct kind */
    device_kind = cd_device_get_kind (device);
    profile_kind_tmp = cd_profile_get_kind (profile);
    profile_kind = cd_device_kind_to_profile_kind (device_kind);
    if (profile_kind_tmp != profile_kind)
        return FALSE;

    /* ignore the colorspace profiles */
    data_source = cd_profile_get_metadata_item (profile,
                                                CD_PROFILE_METADATA_DATA_SOURCE);
    if (g_strcmp0 (data_source, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
        return FALSE;

    /* success */
    return TRUE;
}



static void
color_settings_add_profiles_suitable_for_devices (ColorSettings *settings,
                                                  GPtrArray *profiles)
{
    CdProfile *profile_tmp;
    gboolean ret;
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) profile_array = NULL;
    GtkTreeIter iter;
    guint i;

    gtk_list_store_clear (GTK_LIST_STORE (settings->liststore_assign));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (settings->liststore_assign),
                                          COLOR_SETTINGS_COMBO_COLUMN_TEXT,
                                          GTK_SORT_ASCENDING);
  /*  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (settings->liststore_assign),
                                     COLOR_SETTINGS_COMBO_COLUMN_TEXT,
                                     color_settings_combo_sort_func_cb,
                                     settings->liststore_assign, NULL);
  */
    /* get profiles */
    profile_array = cd_client_get_profiles_sync (settings->client,
                                                 settings->cancellable,
                                                 &error);
    if (profile_array == NULL) {
        g_warning ("failed to get profiles: %s",
             error->message);
        return;
    }

    /* add profiles of the right kind */
    for (i = 0; i < profile_array->len; i++) {
        profile_tmp = g_ptr_array_index (profile_array, i);

        /* get properties */
        ret = cd_profile_connect_sync (profile_tmp,
                                       settings->cancellable,
                                       &error);
        if (!ret) {
            g_warning ("failed to get profile: %s", error->message);
            return;
        }

        /* don't add any of the already added profiles */
        if (profiles != NULL) {
            if (color_settings_profile_exists_in_array (profiles, profile_tmp))
              continue;
        }

        /* only add correct types */
        ret = color_settings_is_profile_suitable_for_device (profile_tmp,
                                                        settings->current_device);
        if (!ret)
            continue;

        /* ignore profiles from other user accounts */
        if (!cd_profile_has_access (profile_tmp))
            continue;

        /* add */
        color_settings_liststore_add_profile (settings,
                                        profile_tmp,
                                        &iter);
    }
}



static void
color_settings_profiles_treeview_clicked_cb (GtkTreeSelection *selection,
                                        ColorSettings *settings)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    /* get selection */
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
      return;

    /* as soon as anything is selected, make the Add button sensitive */
    gtk_widget_set_sensitive (GTK_WIDGET (settings->button_assign_ok), TRUE);
}



static void
color_settings_button_assign_cancel_cb (GtkWidget *widget, ColorSettings *settings)
{
    gtk_widget_hide (GTK_WIDGET (settings->dialog_assign));
}



static void
color_settings_button_assign_ok_cb (GtkWidget *widget, ColorSettings *settings)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    g_autoptr(CdProfile) profile = NULL;
    gboolean ret = FALSE;
    g_autoptr(GError) error = NULL;
    GtkTreeSelection *selection;

    /* hide window */
    widget = GTK_WIDGET (settings->dialog_assign);
    gtk_widget_hide (widget);

    /* get the selected profile */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (settings->treeview_assign));
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return;
    gtk_tree_model_get (model, &iter,
                        COLOR_SETTINGS_COMBO_COLUMN_PROFILE, &profile,
                        -1);
    if (profile == NULL) {
        g_warning ("failed to get the active profile");
        return;
    }

    /* if the device is disabled, enable the device so that we can
     * add color profiles to it */
    if (!cd_device_get_enabled (settings->current_device)) {
        ret = cd_device_set_enabled_sync (settings->current_device,
                                          TRUE,
                                          settings->cancellable,
                                          &error);
        if (!ret) {
            g_warning ("failed to enabled device: %s", error->message);
            return;
        }
    }

    /* just add it, the list store will get ::changed */
    ret = cd_device_add_profile_sync (settings->current_device,
                                      CD_DEVICE_RELATION_HARD,
                                      profile,
                                      settings->cancellable,
                                      &error);
    if (!ret) {
        g_warning ("failed to add: %s", error->message);
        return;
    }

    /* make it default */
    cd_device_make_profile_default (settings->current_device,
                                    profile,
                                    settings->cancellable,
                                    (GAsyncReadyCallback) color_settings_make_profile_default_cb,
                                    settings);
}



static void
color_settings_profiles_row_activated_cb (GtkTreeView *tree_view,
                                          GtkTreePath *path,
                                          GtkTreeViewColumn *column,
                                          ColorSettings *settings)
{
    GtkTreeIter iter;
    gboolean ret;

    ret = gtk_tree_model_get_iter (gtk_tree_view_get_model (tree_view), &iter, path);
    if (!ret)
        return;
    color_settings_button_assign_ok_cb (NULL, settings);
}



static void
color_settings_profile_add_cb (GtkButton *button, ColorSettings *settings)
{
    g_autoptr(GPtrArray) profiles = NULL;
    gchar *window_title;
    int response;

    /* add profiles of the right kind */
    profiles = cd_device_get_profiles (settings->current_device);
    color_settings_add_profiles_suitable_for_devices (settings, profiles);

    /* make insensitive until we have a selection */
    gtk_widget_set_sensitive (GTK_WIDGET (settings->button_assign_ok), FALSE);

    /* show the dialog */
    gtk_window_set_icon_name (GTK_WINDOW (settings->dialog_assign), color_device_get_type_icon (settings->current_device));
    window_title = g_strdup_printf ("Add Color Profile to %s",
                                    color_device_get_kind (settings->current_device) ? color_device_get_kind (settings->current_device) : "Device");
    gtk_window_set_title (GTK_WINDOW (settings->dialog_assign), window_title);
    g_free (window_title);

    response = gtk_dialog_run (GTK_DIALOG (settings->dialog_assign));
    if (response == GTK_RESPONSE_DELETE_EVENT)
        gtk_widget_hide (GTK_WIDGET (settings->dialog_assign));
}



static void
color_settings_profile_import_cb (GtkWidget *widget,
                                  ColorSettings *settings)
{
    g_autoptr(GFile) file = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(CdProfile) profile = NULL;

    file = color_settings_file_chooser_get_icc_profile (settings);
    if (file == NULL) {
        g_warning ("failed to get ICC file");
        widget = GTK_WIDGET (settings->dialog_assign);
        gtk_widget_hide (widget);
        return;
    }

    profile = cd_client_import_profile_sync (settings->client,
                                             file,
                                             settings->cancellable,
                                             &error);
    if (profile == NULL) {
        g_warning ("failed to get imported profile: %s", error->message);
        return;
    }

    color_settings_profile_add_cb (NULL, settings);
}



static void
color_settings_profile_remove_cb (GtkWidget *widget, ColorSettings *settings)
{
    CdProfile *profile;
    gboolean ret = FALSE;
    g_autoptr(GError) error = NULL;
    GtkListBoxRow *row;

    /* get the selected profile */
    row = gtk_list_box_get_selected_row (settings->profiles_list_box);
    if (row == NULL)
        return;
    profile = color_profile_get_profile (SETTINGS_COLOR_PROFILE (row));
    if (profile == NULL) {
          g_warning ("failed to get the active profile");
          return;
    }

    /* just remove it, the list store will get ::changed */
    ret = cd_device_remove_profile_sync (settings->current_device,
                                         profile,
                                         settings->cancellable,
                                         &error);
    if (!ret)
        g_warning ("failed to remove profile: %s", error->message);

    /* as there are no items selected by default after removing a profile, we disable
       the "remove" and "enable" buttons */
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_remove), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_enable), FALSE);
}



static void
color_settings_device_calibrate_cb (CdProfile *profile, ColorSettings *settings)
{
    gchar *cli;
    guint xid;
    GAppInfo *app_info;
    GError *error = NULL;

    if (GTK_IS_WIDGET (settings->dialog))
        xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (settings->dialog)));
    else if (GTK_IS_WIDGET (settings->plug_child))
        xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (settings->plug_child)));
    else
        return;

    cli = g_strdup_printf ("gcm-calibrate --device %s --parent-window %i", cd_device_get_id (settings->current_device), xid);

    /* open up gcm-viewer */
    app_info = g_app_info_create_from_commandline (cli, "Gnome Color Manager Calibration",
                                                   G_APP_INFO_CREATE_NONE, NULL);
    if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
        if (error != NULL) {
            g_warning ("gcm-calibrate could not be launched. %s", error->message);
            g_error_free (error);
        }
    }

    g_free (cli);
}



static void
color_settings_profile_info_view (CdProfile *profile, ColorSettings *settings)
{
    gchar *cli;
    guint xid;
    GAppInfo *app_info;
    GError *error = NULL;

    /* determine if we're launching from the regular or the assign dialog */
    if (gtk_widget_get_visible (GTK_WIDGET (settings->dialog_assign)))
        xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (settings->dialog_assign)));
    else if (GTK_IS_WIDGET (settings->dialog))
        xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (settings->dialog)));
    else if (GTK_IS_WIDGET (settings->plug_child))
        xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (settings->plug_child)));
    else
        return;

    cli = g_strdup_printf ("gcm-viewer --profile %s --parent-window %i", cd_profile_get_id (profile), xid);

    /* open up gcm-viewer */
    app_info = g_app_info_create_from_commandline (cli, "Gnome Color Manager Viewer",
                                                   G_APP_INFO_CREATE_NONE, NULL);
    if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
        if (error != NULL) {
            g_warning ("gcm-viewer could not be launched. %s", error->message);
            g_error_free (error);
        }
    }

    g_free (cli);
}



static void
color_settings_assign_profile_info_cb (GtkWidget *widget, ColorSettings *settings)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    g_autoptr(CdProfile) profile = NULL;
    GtkTreeSelection *selection;

    /* get the selected profile */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (settings->treeview_assign));
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return;
    gtk_tree_model_get (model, &iter,
                        COLOR_SETTINGS_COMBO_COLUMN_PROFILE, &profile,
                        -1);
    if (profile == NULL) {
        g_warning ("failed to get the active profile");
        return;
    }
    color_settings_profile_info_view (profile, settings);
}



static void
color_settings_profile_info_cb (GtkWidget *widget, ColorSettings *settings)
{
    CdProfile *profile;
    GtkListBoxRow *row;

    /* get the selected profile */
    row = gtk_list_box_get_selected_row (settings->profiles_list_box);
    if (row == NULL)
        return;
    profile = color_profile_get_profile (SETTINGS_COLOR_PROFILE (row));
    if (profile == NULL) {
          g_warning ("failed to get the active profile");
          return;
    }
    color_settings_profile_info_view (profile, settings);
}



static void
color_settings_make_profile_default_cb (GObject *object,
                                        GAsyncResult *res,
                                        ColorSettings *settings)
{
    CdDevice *device = CD_DEVICE (object);
    gboolean ret = FALSE;
    g_autoptr(GError) error = NULL;

    ret = cd_device_make_profile_default_finish (device,
                                                 res,
                                                 &error);
    if (!ret) {
        g_warning ("failed to set default profile on %s: %s",
                   cd_device_get_id (device),
                   error->message);
    }
}



static void
color_settings_device_profile_enable_cb (GtkWidget *widget, ColorSettings *settings)
{
    CdProfile *profile;
    GtkListBoxRow *row;

    /* get the selected profile */
    row = gtk_list_box_get_selected_row (settings->profiles_list_box);
    if (row == NULL)
        return;
    profile = color_profile_get_profile (SETTINGS_COLOR_PROFILE (row));
    if (profile == NULL) {
        g_warning ("failed to get the active profile");
        return;
    }

    /* just set it default */
    g_debug ("setting %s default on %s",
             cd_profile_get_id (profile),
             cd_device_get_id (settings->current_device));
    cd_device_make_profile_default (settings->current_device,
                                    profile,
                                    settings->cancellable,
                                    (GAsyncReadyCallback) color_settings_make_profile_default_cb,
                                    settings);
}



static void
color_settings_add_device_profile (ColorSettings *settings,
                                   CdDevice      *device,
                                   CdProfile     *profile,
                                   gboolean       is_default)
{
    gboolean ret;
    g_autoptr(GError) error = NULL;
    GtkWidget *widget;

    /* get properties */
    ret = cd_profile_connect_sync (profile,
                                   settings->cancellable,
                                   &error);
    if (!ret) {
        g_warning ("failed to get profile: %s", error->message);
        return;
    }

    /* ignore profiles from other user accounts */
    if (!cd_profile_has_access (profile)) {
        /* only print the filename if it exists */
        if (cd_profile_get_filename (profile) != NULL) {
            g_warning ("%s is not usable by this user",
                       cd_profile_get_filename (profile));
        }
        else {
            g_warning ("%s is not usable by this user",
                       cd_profile_get_id (profile));
        }
        return;
    }

    /* add to listbox */
    widget = color_profile_new (device, profile, is_default);
    gtk_widget_show (widget);
    gtk_container_add (GTK_CONTAINER (settings->profiles_list_box), widget);
    gtk_size_group_add_widget (settings->profiles_list_box_size, widget);
}



static void
color_settings_update_device_list_extra_entry (ColorSettings *settings)
{
    g_autoptr(GList) device_widgets = NULL;
    guint number_of_devices;

    /* any devices to show? */
    device_widgets = gtk_container_get_children (GTK_CONTAINER (settings->list_box));
    number_of_devices = g_list_length (device_widgets);
    gtk_widget_set_visible (GTK_WIDGET (settings->label_no_devices), number_of_devices == 0);

    if (number_of_devices > 0) {
        GList *selected_rows;

        gtk_widget_set_visible (GTK_WIDGET (settings->scrolled_devices), TRUE);

        /* if no device is selected yet select the first one in the list */
        selected_rows = gtk_list_box_get_selected_rows (GTK_LIST_BOX (settings->list_box));
        if (g_list_length (selected_rows) == 0)
            gtk_list_box_select_row (GTK_LIST_BOX (settings->list_box),
                                 gtk_list_box_get_row_at_index (GTK_LIST_BOX (settings->list_box), 0));
        g_list_free (selected_rows);
    }
}



static void
color_settings_update_profile_list_extra_entry (ColorSettings *settings)
{
    g_autoptr(GList) profile_widgets = NULL;
    guint number_of_profiles;

    if (CD_IS_DEVICE (settings->current_device)) {
        const gchar *model = cd_device_get_model (settings->current_device);
        const gchar *vendor = cd_device_get_vendor (settings->current_device);
        const gchar *colorspace = cd_colorspace_to_string (cd_device_get_colorspace (settings->current_device));

        gtk_image_set_from_icon_name (GTK_IMAGE (settings->device_icon),
                                      color_device_get_type_icon (settings->current_device),
                                      GTK_ICON_SIZE_DIALOG);
        gtk_label_set_text (GTK_LABEL (settings->model), model ? model : _("Unknown"));
        gtk_label_set_text (GTK_LABEL (settings->vendor), vendor ? vendor : _("Unknown"));
        gtk_label_set_text (GTK_LABEL (settings->colorspace), colorspace ? colorspace : _("Unknown"));
    }

    /* any profiles to show? */
    profile_widgets = gtk_container_get_children (GTK_CONTAINER (settings->profiles_list_box));
    number_of_profiles = g_list_length (profile_widgets);
    gtk_widget_set_visible (GTK_WIDGET (settings->label_no_profiles), number_of_profiles == 0);
    gtk_widget_set_visible (GTK_WIDGET (settings->scrolled_profiles), number_of_profiles > 0);
}



static void
color_settings_list_box_row_activated_cb (GtkListBox *list_box,
                                          GtkListBoxRow *row,
                                          ColorSettings *settings)
{
    g_object_get (row, "device", &settings->current_device, NULL);
    if (cd_device_get_enabled (settings->current_device))
    {
        color_settings_device_changed_cb (settings->current_device, settings);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->device_calibrate), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_add), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_remove), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_info), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_enable), FALSE);
    }
    else
    {
        gtk_widget_show (GTK_WIDGET (settings->label_no_profiles));
        gtk_widget_set_sensitive (GTK_WIDGET (settings->device_calibrate), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_add), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_remove), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_info), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_enable), FALSE);
        gtk_widget_hide (GTK_WIDGET (settings->scrolled_profiles));
    }
}




static void
color_settings_device_enabled_changed_cb (ColorDevice *widget,
                                          gboolean is_enabled,
                                          ColorSettings *settings)
{
    gtk_list_box_select_row (settings->list_box, GTK_LIST_BOX_ROW (widget));
    gtk_widget_set_visible (GTK_WIDGET (settings->label_no_profiles), !is_enabled);
    gtk_widget_set_visible (GTK_WIDGET (settings->scrolled_profiles), is_enabled);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->device_calibrate), is_enabled);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_add), is_enabled);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_remove), is_enabled);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_info), is_enabled);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_enable), is_enabled);
}



static void
color_settings_profiles_list_box_row_selected_cb (GtkListBox *list_box,
                                                  GtkListBoxRow *row,
                                                  ColorSettings *settings)
{
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_remove), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_info), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_enable), TRUE);
}



static void
color_settings_profiles_list_box_row_activated_cb (GtkListBox *list_box,
                                                   GtkListBoxRow *row,
                                                   ColorSettings *settings)
{
    if (SETTINGS_IS_COLOR_PROFILE (row)) {
        color_settings_device_profile_enable_cb (NULL, settings);
    }
}



static void
color_settings_dialog_destroy (ColorSettings *settings)
{
    gtk_widget_destroy (GTK_WIDGET (settings->dialog_assign));
    g_clear_object (&settings->cancellable);
    g_clear_object (&settings->client);
    g_clear_object (&settings->current_device);
    g_clear_object (&settings->list_box_size);
    gtk_main_quit ();
}



static void
color_settings_dialog_response (GtkWidget *dialog,
                                gint       response_id,
                                ColorSettings *settings)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "color",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else
        color_settings_dialog_destroy (settings);
}



/* find the profile in the array -- for flicker-free changes */
static gboolean
color_settings_find_profile_by_object_path (GPtrArray *profiles,
                                            const gchar *object_path)
{
    CdProfile *profile_tmp;
    guint i;

    for (i = 0; i < profiles->len; i++) {
        profile_tmp = g_ptr_array_index (profiles, i);
        if (g_strcmp0 (cd_profile_get_object_path (profile_tmp), object_path) == 0)
            return TRUE;
    }
    return FALSE;
}



/* find the profile in the list view -- for flicker-free changes */
static gboolean
color_settings_find_widget_by_object_path (GList *list,
                                      const gchar *object_path_device,
                                      const gchar *object_path_profile)
{
    GList *l;
    CdDevice *device_tmp;
    CdProfile *profile_tmp;

    for (l = list; l != NULL; l = l->next) {
        if (!SETTINGS_IS_COLOR_PROFILE (l->data))
            continue;

        /* correct device ? */
        device_tmp = color_profile_get_device (SETTINGS_COLOR_PROFILE (l->data));
        if (g_strcmp0 (object_path_device,
                       cd_device_get_object_path (device_tmp)) != 0) {
            continue;
        }

        /* this profile */
        profile_tmp = color_profile_get_profile (SETTINGS_COLOR_PROFILE (l->data));
        if (g_strcmp0 (object_path_profile,
                       cd_profile_get_object_path (profile_tmp)) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}



static void
color_settings_device_changed_cb (CdDevice *device,
                                  ColorSettings *settings)
{
    CdDevice *device_tmp;
    CdProfile *profile_tmp;
    gboolean ret;
    GList *l;
    g_autoptr(GList) list = NULL;
    GPtrArray *profiles;
    guint i;

    /* remove anything in the list view that's not in Device.Profiles */
    profiles = cd_device_get_profiles (device);
    list = gtk_container_get_children (GTK_CONTAINER (settings->profiles_list_box));
    for (l = list; l != NULL; l = l->next) {
        if (!SETTINGS_IS_COLOR_PROFILE (l->data))
            continue;

        /* remove profiles from other devices from the list */
        device_tmp = color_profile_get_device (SETTINGS_COLOR_PROFILE (l->data));
        if (g_strcmp0 (cd_device_get_id (device),
                       cd_device_get_id (device_tmp)) != 0) {
            gtk_widget_destroy (GTK_WIDGET (l->data));
            /* Don't look at the destroyed widget below */
            l->data = NULL;
            continue;
        }

        /* if profile is not in Device.Profiles then remove */
        profile_tmp = color_profile_get_profile (SETTINGS_COLOR_PROFILE (l->data));
        ret = color_settings_find_profile_by_object_path (profiles,
                                                     cd_profile_get_object_path (profile_tmp));
        if (!ret) {
            gtk_widget_destroy (GTK_WIDGET (l->data));
            /* Don't look at the destroyed widget below */
            l->data = NULL;
        }
    }

    /* add anything in Device.Profiles that's not in the list view */
    for (i = 0; i < profiles->len; i++) {
        profile_tmp = g_ptr_array_index (profiles, i);
        ret = color_settings_find_widget_by_object_path (list,
                                                    cd_device_get_object_path (device),
                                                    cd_profile_get_object_path (profile_tmp));
        if (!ret)
            color_settings_add_device_profile (settings, device, profile_tmp, i == 0);
    }

    color_settings_update_profile_list_extra_entry (settings);
}



static void
color_settings_add_device (ColorSettings *settings, CdDevice *device)
{
    gboolean ret;
    g_autoptr(GError) error = NULL;
    GtkWidget *widget;

    /* get device properties */
    ret = cd_device_connect_sync (device, settings->cancellable, &error);
    if (!ret) {
        g_warning ("failed to connect to the device: %s", error->message);
        return;
    }

    /* add device */
    widget = color_device_new (device);
    g_signal_connect (G_OBJECT (widget), "enabled-changed",
                      G_CALLBACK (color_settings_device_enabled_changed_cb), settings);
    gtk_widget_show (widget);
    gtk_container_add (GTK_CONTAINER (settings->list_box), widget);
    gtk_size_group_add_widget (settings->list_box_size, widget);

    /* watch for changes */
    g_ptr_array_add (settings->devices, g_object_ref (device));
    g_signal_connect (device, "changed",
                      G_CALLBACK (color_settings_device_changed_cb), settings);
}



static void
color_settings_remove_device (ColorSettings *settings, CdDevice *device)
{
    CdDevice *device_tmp;
    GList *l;
    g_autoptr(GList) list = NULL;

    list = gtk_container_get_children (GTK_CONTAINER (settings->list_box));
    for (l = list; l != NULL; l = l->next) {
        device_tmp = color_device_get_device (SETTINGS_COLOR_DEVICE (l->data));

        if (g_strcmp0 (cd_device_get_object_path (device),
                       cd_device_get_object_path (device_tmp)) == 0) {
            gtk_widget_destroy (GTK_WIDGET (l->data));
        }
    }
    g_signal_handlers_disconnect_by_func (device,
                                          G_CALLBACK (color_settings_device_changed_cb),
                                          settings);
    g_ptr_array_remove (settings->devices, device);
    color_settings_update_profile_list_extra_entry (settings);
}



static void
list_box_update_header_func (GtkListBoxRow *row,
                             GtkListBoxRow *before,
                             gpointer user_data)
{
    GtkWidget *current;

    if (before == NULL) {
        gtk_list_box_row_set_header (row, NULL);
        return;
    }

    current = gtk_list_box_row_get_header (row);
    if (current == NULL) {
        current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_show (current);
        gtk_list_box_row_set_header (row, current);
    }
}



static void
color_settings_device_added_cb (CdClient *client,
                                CdDevice *device,
                                ColorSettings *settings)
{
    /* add the device */
    color_settings_add_device (settings, device);

    /* ensure we're not showing the 'No devices detected' entry */
    color_settings_update_device_list_extra_entry (settings);
}



static void
color_settings_device_removed_cb (CdClient *client,
                                  CdDevice *device,
                                  ColorSettings *settings)
{
    /* remove from the UI */
    color_settings_remove_device (settings, device);

    /* ensure we're showing the 'No devices detected' entry if required */
    color_settings_update_device_list_extra_entry (settings);
}



static gint
color_settings_sort_func (GtkListBoxRow *a,
                          GtkListBoxRow *b,
                          gpointer user_data)
{
    const gchar *sort_a = NULL;
    const gchar *sort_b = NULL;

    sort_a = color_device_get_sortable (SETTINGS_COLOR_DEVICE (a));
    sort_b = color_device_get_sortable (SETTINGS_COLOR_DEVICE (b));

    return g_strcmp0 (sort_b, sort_a);
}



static void
color_settings_get_devices_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
    ColorSettings *settings = (ColorSettings *) user_data;
    CdClient *client = CD_CLIENT (object);
    CdDevice *device;
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) devices = NULL;
    guint i;

    /* get devices and add them */
    devices = cd_client_get_devices_finish (client, res, &error);
    if (devices == NULL) {
        g_warning ("failed to add connected devices: %s",
                   error->message);
        return;
    }
    for (i = 0; i < devices->len; i++) {
        device = g_ptr_array_index (devices, i);
        color_settings_add_device (settings, device);
    }
    /* ensure we showing the 'No devices detected' entry if required */
    color_settings_update_device_list_extra_entry (settings);
    color_settings_update_profile_list_extra_entry (settings);
}



static void
color_settings_connect_cb (GObject *object,
                           GAsyncResult *res,
                           gpointer user_data)
{
    ColorSettings *settings = (ColorSettings *) user_data;
    gboolean ret;
    g_autoptr(GError) error = NULL;

    ret = cd_client_connect_finish (CD_CLIENT (object),
                                    res,
                                    &error);
    if (!ret) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("failed to connect to colord: %s", error->message);
        return;
    }

    /* get devices */
    cd_client_get_devices (settings->client,
                           settings->cancellable,
                           color_settings_get_devices_cb,
                           settings);
}



ColorSettings *
color_settings_dialog_init (GtkBuilder *builder)
{
    ColorSettings *settings;
    GtkTreeSelection *selection;
    GObject *paned;
    GtkCssProvider *provider;

    settings = g_new0 (ColorSettings, 1);
    settings->cancellable = g_cancellable_new ();
    settings->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

    /* use a device client array */
    settings->client = cd_client_new ();
    g_signal_connect_data (settings->client, "device-added",
                           G_CALLBACK (color_settings_device_added_cb), settings, 0, 0);
    g_signal_connect_data (settings->client, "device-removed",
                           G_CALLBACK (color_settings_device_removed_cb), settings, 0, 0);

    /* brighten the background of the GtkPaned for better visual grouping */
    paned = gtk_builder_get_object (builder, "paned");
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (paned)), "color-profiles");
    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider,
                                     "paned.color-profiles { background: shade(@theme_bg_color, 1.05); }",
                                     -1, NULL);
    gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (paned)),
                                    GTK_STYLE_PROVIDER (provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    settings->label_no_devices = gtk_builder_get_object (builder, "label-no-devices");

    /* Devices ListBox */
    settings->device_icon = gtk_builder_get_object (builder, "device-icon");
    settings->model = gtk_builder_get_object (builder, "model");
    settings->vendor = gtk_builder_get_object (builder, "vendor");
    settings->colorspace = gtk_builder_get_object (builder, "colorspace");
    settings->scrolled_devices = gtk_builder_get_object (builder, "scrolled-devices");
    settings->list_box = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_list_box_set_sort_func (settings->list_box,
                                color_settings_sort_func,
                                settings,
                                NULL);
    gtk_list_box_set_header_func (settings->list_box,
                                  list_box_update_header_func,
                                  settings, NULL);
    gtk_list_box_set_selection_mode (settings->list_box,
                                     GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click (settings->list_box, TRUE);
    settings->list_box_selected_id =
        g_signal_connect (settings->list_box, "row-selected",
                          G_CALLBACK (color_settings_list_box_row_activated_cb),
                          settings);
    settings->list_box_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    gtk_container_add (GTK_CONTAINER (settings->scrolled_devices), GTK_WIDGET (settings->list_box));
    gtk_widget_show_all (GTK_WIDGET (settings->list_box));

    /* Conditionally show/hide the calibrate button, based on the availability of gnome-color-manager */
    settings->device_calibrate = gtk_builder_get_object (builder, "device-calibrate");
    if (g_find_program_in_path ("gcm-calibrate") == NULL)
        gtk_widget_hide (GTK_WIDGET (settings->device_calibrate));
    gtk_widget_set_sensitive (GTK_WIDGET (settings->device_calibrate), FALSE);
    g_signal_connect (settings->device_calibrate, "clicked", G_CALLBACK (color_settings_device_calibrate_cb), settings);

    /* Profiles ListBox */
    settings->profiles_add = gtk_builder_get_object (builder, "profiles-add");
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_add), FALSE);
    g_signal_connect (settings->profiles_add, "clicked", G_CALLBACK (color_settings_profile_add_cb), settings);

    settings->profiles_remove = gtk_builder_get_object (builder, "profiles-remove");
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_remove), FALSE);
    g_signal_connect (settings->profiles_remove, "clicked", G_CALLBACK (color_settings_profile_remove_cb), settings);

    settings->profiles_info = gtk_builder_get_object (builder, "profiles-info");
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_info), FALSE);
    g_signal_connect (settings->profiles_info, "clicked", G_CALLBACK (color_settings_profile_info_cb), settings);

    settings->profiles_enable = gtk_builder_get_object (builder, "profiles-enable");
    gtk_widget_set_sensitive (GTK_WIDGET (settings->profiles_enable), FALSE);
    g_signal_connect (settings->profiles_enable, "clicked", G_CALLBACK (color_settings_device_profile_enable_cb), settings);

    settings->label_no_profiles = gtk_builder_get_object (builder, "label-no-profiles");
    settings->profiles_list_box = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_list_box_set_header_func (settings->profiles_list_box,
                                  list_box_update_header_func,
                                  settings, NULL);
    gtk_list_box_set_selection_mode (settings->profiles_list_box,
                                     GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click (settings->profiles_list_box, FALSE);
    settings->profiles_list_box_selected_id =
        g_signal_connect (settings->profiles_list_box, "row-selected",
                          G_CALLBACK (color_settings_profiles_list_box_row_selected_cb),
                          settings);
    settings->profiles_list_box_activated_id =
    g_signal_connect (settings->profiles_list_box, "row-activated",
                      G_CALLBACK (color_settings_profiles_list_box_row_activated_cb),
                      settings);
    settings->profiles_list_box_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    settings->scrolled_profiles = gtk_builder_get_object (builder, "scrolled-profiles");
    gtk_container_add (GTK_CONTAINER (settings->scrolled_profiles), GTK_WIDGET (settings->profiles_list_box));
    gtk_widget_show (GTK_WIDGET (settings->profiles_list_box));

    /* Treeview of all colord profiles */
    settings->dialog_assign = gtk_builder_get_object (builder, "dialog-assign");
    settings->liststore_assign = gtk_builder_get_object (builder, "liststore-assign");
    settings->treeview_assign = gtk_builder_get_object (builder, "treeview-assign");
    color_settings_add_profiles_columns (settings, GTK_TREE_VIEW (settings->treeview_assign));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (settings->treeview_assign));
    g_signal_connect (selection, "changed",
                      G_CALLBACK (color_settings_profiles_treeview_clicked_cb),
                      settings);
    g_signal_connect (GTK_TREE_VIEW (settings->treeview_assign), "row-activated",
                      G_CALLBACK (color_settings_profiles_row_activated_cb),
                      settings);
    settings->button_assign_import = gtk_builder_get_object (builder, "assign-import");
    g_signal_connect (settings->button_assign_import, "clicked", G_CALLBACK (color_settings_profile_import_cb), settings);
    settings->button_assign_info = gtk_builder_get_object (builder, "assign-info");
    g_signal_connect (settings->button_assign_info, "clicked", G_CALLBACK (color_settings_assign_profile_info_cb), settings);
    settings->button_assign_ok = gtk_builder_get_object (builder, "assign-ok");
    g_signal_connect (settings->button_assign_ok, "clicked",
                      G_CALLBACK (color_settings_button_assign_ok_cb), settings);
    settings->button_assign_cancel = gtk_builder_get_object (builder, "assign-cancel");
    g_signal_connect (settings->button_assign_cancel, "clicked",
                      G_CALLBACK (color_settings_button_assign_cancel_cb), settings);

    /* Conditionally show/hide the info buttons, based on the availability of gnome-color-manager */
    if (g_find_program_in_path ("gcm-viewer") == NULL) {
        gtk_widget_hide (GTK_WIDGET (settings->profiles_info));
        gtk_widget_hide (GTK_WIDGET (settings->button_assign_info));
    }

    cd_client_connect (settings->client,
                       settings->cancellable,
                       color_settings_connect_cb,
                       settings);
    return settings;
}



gint
main (gint argc, gchar **argv)
{
    GtkWidget     *plug;
    GtkBuilder    *builder;
    GError        *error = NULL;
    ColorSettings *settings;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, entries, PACKAGE, &error)) {
        if (G_LIKELY (error)) {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* check if we should print version information */
    if (G_UNLIKELY (opt_version)) {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-2023");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, color_dialog_ui,
                                     color_dialog_ui_length, &error) != 0) {
        /* Initialize the dialog */
        settings = color_settings_dialog_init (builder);

        if (G_UNLIKELY (opt_socket_id == 0)) {
            /* Get the dialog widget */
            settings->dialog = gtk_builder_get_object (builder, "dialog");

            g_signal_connect (settings->dialog, "response",
                              G_CALLBACK (color_settings_dialog_response), settings);
            gtk_window_present (GTK_WINDOW (settings->dialog));

            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id ("FAKE ID");

            gtk_main ();
        }
        else {
            /* Create plug widget */
            plug = gtk_plug_new (opt_socket_id);
            g_signal_connect_swapped (plug, "delete-event",
                                      G_CALLBACK (color_settings_dialog_destroy), settings);
            gtk_widget_show (plug);

            /* Stop startup notification */
            gdk_notify_startup_complete ();

            /* Get plug child widget */
            settings->plug_child = gtk_builder_get_object (builder, "plug-child");
            xfce_widget_reparent (GTK_WIDGET (settings->plug_child), plug);
            gtk_widget_show (GTK_WIDGET (settings->plug_child));

            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id ("FAKE ID");

            /* Enter main loop */
            gtk_main ();
        }
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    /* Release Builder */
    g_object_unref (G_OBJECT (builder));

    return EXIT_SUCCESS;
}
