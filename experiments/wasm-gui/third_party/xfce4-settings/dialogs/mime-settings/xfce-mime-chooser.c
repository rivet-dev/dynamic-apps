/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Parts of the code were taken from Thunar's chooser dialog and model:
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <gio/gdesktopappinfo.h>

#include "xfce-mime-chooser.h"



static void     xfce_mime_chooser_finalize          (GObject           *object);
static void     xfce_mime_chooser_row_activated     (GtkTreeView       *tree_view,
                                                     GtkTreePath       *path,
                                                     GtkTreeViewColumn *column,
                                                     XfceMimeChooser   *chooser);
static gboolean xfce_mime_chooser_row_can_select    (GtkTreeSelection  *selection,
                                                     GtkTreeModel      *model,
                                                     GtkTreePath       *path,
                                                     gboolean           path_currently_selected,
                                                     gpointer           data);
static void     xfce_mime_chooser_update_accept     (XfceMimeChooser   *chooser);
static void     xfce_mime_chooser_notify_expanded   (GtkExpander       *expander,
                                                     GParamSpec        *pspec,
                                                     XfceMimeChooser   *chooser);
static void     xfce_mime_chooser_browse_command    (GtkWidget         *button,
                                                     XfceMimeChooser   *chooser);



struct _XfceMimeChooserClass
{
    GtkDialogClass __parent__;
};

struct _XfceMimeChooser
{
    GtkDialog  __parent__;

    GtkTreeStore *model;

    GtkWidget    *button;
    GtkWidget    *image;
    GtkWidget    *label;
    GtkWidget    *treeview;
    GtkWidget    *expander;
    GtkWidget    *entry;
};

enum
{
    CHOOSER_COLUMN_NAME,
    CHOOSER_COLUMN_APP_INFO,
    CHOOSER_COLUMN_GICON,
    CHOOSER_COLUMN_ATTRS,
    N_CHOOSER_COLUMNS
};



G_DEFINE_TYPE (XfceMimeChooser, xfce_mime_chooser, GTK_TYPE_DIALOG)



static void
xfce_mime_chooser_class_init (XfceMimeChooserClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_mime_chooser_finalize;
}



static void
xfce_mime_chooser_init (XfceMimeChooser *chooser)
{
    GtkWidget         *vbox;
    GtkWidget         *scroll;
    GtkWidget         *area;
    GtkWidget         *expander;
    GtkWidget         *hbox;
    GtkWidget         *entry;
    GtkWidget         *image;
    GtkWidget         *label;
    GtkWidget         *button;
    GtkWidget         *treeview;
    GtkTreeViewColumn *column;
    GtkTreeSelection  *selection;
    GtkCellRenderer   *renderer;

    chooser->model = gtk_tree_store_new (N_CHOOSER_COLUMNS,
                                         G_TYPE_STRING,
                                         G_TYPE_APP_INFO,
                                         G_TYPE_ICON,
                                         PANGO_TYPE_ATTR_LIST);

    gtk_window_set_title (GTK_WINDOW (chooser), _("Select Application"));
    gtk_window_set_icon_name (GTK_WINDOW (chooser), "application-x-executable");
    gtk_window_set_default_size (GTK_WINDOW (chooser), 400, 350);
    gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);

    gtk_dialog_add_button (GTK_DIALOG (chooser),
                           _("Cancel"), GTK_RESPONSE_CANCEL);
    chooser->button = gtk_dialog_add_button (GTK_DIALOG (chooser),
                                             _("Open"), GTK_RESPONSE_YES);
    gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_YES);
    gtk_widget_set_sensitive (chooser->button, FALSE);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    area = gtk_dialog_get_content_area (GTK_DIALOG (chooser));
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
    gtk_box_pack_start (GTK_BOX (area), vbox, TRUE, TRUE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    chooser->image = image = gtk_image_new ();
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
    gtk_widget_show (image);

    chooser->label = label = gtk_label_new ("");
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_widget_set_halign (GTK_WIDGET (label), GTK_ALIGN_START);
    gtk_widget_set_valign (GTK_WIDGET (label), GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_widget_set_size_request (label, 350, -1);
    gtk_widget_show (label);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_widget_show (scroll);

    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (chooser->model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_level_indentation (GTK_TREE_VIEW (treeview), 24);
    gtk_container_add (GTK_CONTAINER (scroll), treeview);
    g_signal_connect (G_OBJECT (treeview), "row-activated",
        G_CALLBACK (xfce_mime_chooser_row_activated), chooser);
    gtk_widget_show (treeview);
    chooser->treeview = treeview;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_select_function (selection,
        xfce_mime_chooser_row_can_select, NULL, NULL);
    g_signal_connect_swapped (G_OBJECT (selection), "changed",
        G_CALLBACK (xfce_mime_chooser_update_accept), chooser);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    gtk_tree_view_column_set_spacing (column, 2);
    gtk_tree_view_column_set_expand (column, TRUE);

    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "gicon", CHOOSER_COLUMN_GICON,
                                         NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "text", CHOOSER_COLUMN_NAME,
                                         "attributes", CHOOSER_COLUMN_ATTRS,
                                         NULL);

    chooser->expander = expander = gtk_expander_new_with_mnemonic (_("Use a c_ustom command:"));
    gtk_widget_set_tooltip_text (expander, _("Use a custom command for an application that is not "
                                             "available from the above application list."));
    g_signal_connect (G_OBJECT (expander), "notify::expanded",
        G_CALLBACK (xfce_mime_chooser_notify_expanded), chooser);
    gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, TRUE, 0);
    gtk_expander_set_expanded (GTK_EXPANDER (expander), FALSE);
    gtk_widget_show (expander);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add (GTK_CONTAINER (expander), hbox);
    gtk_widget_show (hbox);

    chooser->entry = entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    g_signal_connect_swapped (G_OBJECT (entry), "changed",
        G_CALLBACK (xfce_mime_chooser_update_accept), chooser);
    gtk_widget_show (entry);

    button = gtk_button_new_with_mnemonic (_("_Browse..."));
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
         G_CALLBACK (xfce_mime_chooser_browse_command), chooser);
    gtk_widget_show (button);
}



static void
xfce_mime_chooser_finalize (GObject *object)
{
    XfceMimeChooser *chooser = XFCE_MIME_CHOOSER (object);

    g_object_unref (G_OBJECT (chooser->model));

    (*G_OBJECT_CLASS (xfce_mime_chooser_parent_class)->finalize) (object);
}



static GAppInfo *
xfce_mime_chooser_get_selected (XfceMimeChooser *chooser)
{
    GtkTreeIter       iter;
    GAppInfo         *app_info = NULL;
    GtkTreeSelection *selection;

    g_return_val_if_fail (XFCE_IS_MIME_CHOOSER (chooser), NULL);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        gtk_tree_model_get (GTK_TREE_MODEL (chooser->model), &iter,
                            CHOOSER_COLUMN_APP_INFO, &app_info, -1);

    }

    return app_info;
}



static void
xfce_mime_chooser_row_activated (GtkTreeView       *tree_view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column,
                                 XfceMimeChooser   *chooser)
{
    if (gtk_widget_is_sensitive (chooser->button))
        gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_YES);
}



static gboolean
xfce_mime_chooser_row_can_select (GtkTreeSelection  *selection,
                                  GtkTreeModel      *model,
                                  GtkTreePath       *path,
                                  gboolean           path_currently_selected,
                                  gpointer           data)
{
    gboolean    permitted = TRUE;
    GtkTreeIter iter;
    GValue      value = { 0, };

    /* we can always change the selection if the path is already selected */
    if (G_UNLIKELY (!path_currently_selected))
    {
        /* check if there's an application for the path */
        if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
        {
            gtk_tree_model_get_value (model, &iter, CHOOSER_COLUMN_APP_INFO, &value);
            permitted = (g_value_get_object (&value) != NULL);
            g_value_unset (&value);
        }
    }

    return permitted;
}



static void
xfce_mime_chooser_update_accept (XfceMimeChooser *chooser)
{
    gboolean     can_open = FALSE;
    GAppInfo    *app_info;
    const gchar *text;

    if (gtk_expander_get_expanded (GTK_EXPANDER (chooser->expander)))
    {
        text = gtk_entry_get_text (GTK_ENTRY (chooser->entry));
        can_open = (text != NULL && *text != '\0');
    }
    else
    {
        app_info = xfce_mime_chooser_get_selected (chooser);
        if (app_info != NULL)
        {
            can_open = TRUE;
            g_object_unref (G_OBJECT (app_info));
        }
    }

    gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_YES, can_open);
}



static void
xfce_mime_chooser_notify_expanded (GtkExpander     *expander,
                                   GParamSpec      *pspec,
                                   XfceMimeChooser *chooser)
{
    gboolean          expanded;
    GtkTreeSelection *selection;
    GAppInfo         *app_info;
    const gchar      *exec;

    expanded = gtk_expander_get_expanded (expander);
    gtk_widget_set_sensitive (chooser->treeview, !expanded);

    if (expanded)
    {
        /* use the command of the selected item */
        app_info = xfce_mime_chooser_get_selected (chooser);
        if (app_info != NULL)
        {
            exec = g_app_info_get_executable (app_info);
            if (G_LIKELY (exec != NULL && g_utf8_validate (exec, -1, NULL)))
                gtk_entry_set_text (GTK_ENTRY (chooser->entry), exec);

            g_object_unref (G_OBJECT (app_info));
        }

        /* unselect all item in the treeview */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->treeview));
        gtk_tree_selection_unselect_all (selection);
    }

    xfce_mime_chooser_update_accept (chooser);
}



static void
xfce_mime_chooser_browse_command (GtkWidget       *button,
                                  XfceMimeChooser *dialog)
{
    GtkWidget     *chooser;
    GtkFileFilter *filter;
    gchar         *filename;
    gchar         *s;

    chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
                                           GTK_WINDOW (dialog),
                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                           _("Cancel"), GTK_RESPONSE_CANCEL,
                                           _("Open"), GTK_RESPONSE_ACCEPT,
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
    filename = gtk_editable_get_chars (GTK_EDITABLE (dialog->entry), 0, -1);
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
        gtk_entry_set_text (GTK_ENTRY (dialog->entry), filename);
        g_free (filename);
    }

    gtk_widget_destroy (chooser);
}



static void
xfce_mime_chooser_model_append (GtkTreeStore *model,
                                const gchar  *title,
                                const gchar  *icon_name,
                                GList        *app_infos)
{
    GIcon         *icon;
    GtkTreeIter    child_iter;
    GtkTreeIter    parent_iter;
    GList         *li;
    PangoAttrList *attrs;

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

    icon = g_themed_icon_new (icon_name);
    gtk_tree_store_append (model, &parent_iter, NULL);
    gtk_tree_store_set (model, &parent_iter,
                        CHOOSER_COLUMN_NAME, title,
                        CHOOSER_COLUMN_GICON, icon,
                        CHOOSER_COLUMN_ATTRS, attrs, -1);
    g_object_unref (G_OBJECT (icon));
    pango_attr_list_unref (attrs);

    if (G_LIKELY (app_infos != NULL))
    {
        /* insert the program items */
        for (li = app_infos; li != NULL; li = li->next)
        {
            /* append the tree row with the program data */
            gtk_tree_store_append (model, &child_iter, &parent_iter);
            gtk_tree_store_set (model, &child_iter,
                                CHOOSER_COLUMN_NAME, g_app_info_get_name (li->data),
                                CHOOSER_COLUMN_GICON, g_app_info_get_icon (li->data),
                                CHOOSER_COLUMN_APP_INFO, li->data,
                                -1);
        }
    }
    else
    {
        attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, pango_attr_style_new (PANGO_STYLE_ITALIC));

        /* tell the user that we don't have any applications for this category */
        gtk_tree_store_append (model, &child_iter, &parent_iter);
        gtk_tree_store_set (model, &child_iter,
                            CHOOSER_COLUMN_NAME, _("None available"),
                            CHOOSER_COLUMN_ATTRS, attrs, -1);
        pango_attr_list_unref (attrs);
    }
}



static gint
xfce_mime_chooser_compare_app_info (gconstpointer a,
                                    gconstpointer b)
{
    return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}



static gint
xfce_mime_chooser_sort_app_info (gconstpointer a,
                                 gconstpointer b)
{
    return g_utf8_collate (g_app_info_get_name (G_APP_INFO (a)),
                           g_app_info_get_name (G_APP_INFO (b)));
}



void
xfce_mime_chooser_set_mime_type (XfceMimeChooser *chooser,
                                 const gchar     *mime_type)
{
    GList *recommended;
    GList *all, *li;
    GList *other = NULL;
    GIcon *icon;
    gchar *label;
    gchar *description;

    g_return_if_fail (XFCE_IS_MIME_CHOOSER (chooser));
    g_return_if_fail (mime_type != NULL);
    g_return_if_fail (GTK_IS_TREE_STORE (chooser->model));

    gtk_tree_store_clear (chooser->model);

    /* add recommended types */
    recommended = g_app_info_get_all_for_type (mime_type);
    xfce_mime_chooser_model_append (chooser->model,
                                    _("Recommended Applications"),
                                    "org.xfce.settings.default-applications",
                                    recommended);

    /* filter out recommended apps from all apps */
    all = g_app_info_get_all ();
    for (li = all; li != NULL; li = li->next)
    {
        if (g_list_find_custom (recommended, li->data,
                                xfce_mime_chooser_compare_app_info) == NULL)
        {
            other = g_list_prepend (other, li->data);
        }
    }

    /* add the other applications */
    other = g_list_sort (other, xfce_mime_chooser_sort_app_info);
    xfce_mime_chooser_model_append (chooser->model,
                                    _("Other Applications"),
                                    "gnome-applications",
                                    other);

    /* open all */
    gtk_tree_view_expand_all (GTK_TREE_VIEW (chooser->treeview));

    /* cleanup */
    g_list_foreach (recommended, (GFunc) (void (*)(void)) g_object_unref, NULL);
    g_list_foreach (all, (GFunc) (void (*)(void)) g_object_unref, NULL);
    g_list_free (recommended);
    g_list_free (all);
    g_list_free (other);

    /* set label and icon */
    icon = g_content_type_get_icon (mime_type);
    gtk_image_set_from_gicon (GTK_IMAGE (chooser->image), icon, GTK_ICON_SIZE_DIALOG);
    g_object_unref (G_OBJECT (icon));

    description = g_content_type_get_description (mime_type);
    label = g_strdup_printf (_("Open <i>%s</i> and other files of type \"%s\" with:"),
                             mime_type, description);
    gtk_label_set_markup (GTK_LABEL (chooser->label), label);
    g_free (label);
    g_free (description);
}



GAppInfo *
xfce_mime_chooser_get_app_info (XfceMimeChooser *chooser)
{
    const gchar *exec;
    GAppInfo    *app_info;
    gchar       *path;
    gchar       *name;
    gchar       *s;
    GError      *error = NULL;

    if (gtk_expander_get_expanded (GTK_EXPANDER (chooser->expander)))
    {
        exec = gtk_entry_get_text (GTK_ENTRY (chooser->entry));

        /* determine the path for the custom command */
        path = g_strdup (exec);
        s = strchr (path, ' ');
        if (G_UNLIKELY (s != NULL))
            *s = '\0';

        /* determine the name from the path of the custom command */
        name = g_path_get_basename (path);

        /* try to add an application for the custom command */
        app_info = g_app_info_create_from_commandline (exec, name, G_APP_INFO_CREATE_NONE, &error);
        if (G_UNLIKELY (app_info == NULL))
        {
            xfce_dialog_show_error (GTK_WINDOW (chooser), error, _("Failed to add new application \"%s\""), name);
            g_error_free (error);
        }

        g_free (path);
        g_free (name);

        return app_info;
    }
    else
    {
        return xfce_mime_chooser_get_selected (chooser);
    }
}
