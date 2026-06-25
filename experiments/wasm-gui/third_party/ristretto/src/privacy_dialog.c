/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "util.h"
#include "privacy_dialog.h"

#include <libxfce4ui/libxfce4ui.h>



enum
{
    PROP_0,
    PROP_FILTER,
    PROP_LIMIT,
    PROP_LOCAL_ONLY,
    PROP_SELECT_MULTIPLE,
    PROP_RECENT_MANAGER,
    PROP_SORT_TYPE,
    PROP_SHOW_TIPS,
    PROP_SHOW_ICONS,
    PROP_SHOW_NOT_FOUND,
    PROP_SHOW_PRIVATE,
};



static void
rstto_recent_chooser_init (GtkRecentChooserIface *iface);

static void
rstto_privacy_dialog_finalize (GObject *object);
static void
rstto_privacy_dialog_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec);
static void
rstto_privacy_dialog_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec);

static void
rstto_recent_chooser_add_filter (GtkRecentChooser *chooser,
                                 GtkRecentFilter *filter);
static GList *
rstto_recent_chooser_get_items (GtkRecentChooser *chooser);
static void
cb_rstto_privacy_dialog_combobox_timeframe_changed (GtkComboBox *combobox,
                                                    gpointer user_data);
gboolean
cb_rstto_recent_filter_filter_timeframe (const GtkRecentFilterInfo *filter_info,
                                         gpointer user_data);



struct _RsttoPrivacyDialogPrivate
{
    RsttoSettings *settings;

    GtkWidget *cleanup_frame;
    GtkWidget *cleanup_vbox;
    GtkWidget *cleanup_timeframe_combo;

    GtkRecentManager *recent_manager;
    GSList           *filters;
    GtkRecentFilter  *timeframe_filter;
    time_t            time_now;
    time_t            time_offset;
};



G_DEFINE_TYPE_WITH_CODE (RsttoPrivacyDialog, rstto_privacy_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (RsttoPrivacyDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER, rstto_recent_chooser_init))



static void
rstto_privacy_dialog_init (RsttoPrivacyDialog *dialog)
{
    GtkWidget *display_main_hbox;
    GtkWidget *display_main_lbl;
    GtkWidget *button;

    dialog->priv = rstto_privacy_dialog_get_instance_private (dialog);

    dialog->priv->settings = rstto_settings_new ();
    dialog->priv->time_now = time (0);
    dialog->priv->timeframe_filter = gtk_recent_filter_new ();

    /* Add recent-filter function to filter in access-time */
    gtk_recent_filter_add_custom (
            dialog->priv->timeframe_filter,
            GTK_RECENT_FILTER_URI,
            cb_rstto_recent_filter_filter_timeframe,
            dialog,
            NULL);

    display_main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    display_main_lbl = gtk_label_new (_("Time range to clear:"));

    dialog->priv->cleanup_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    dialog->priv->cleanup_frame = xfce_gtk_frame_box_new_with_content (_("Cleanup"), dialog->priv->cleanup_vbox);
    gtk_widget_set_margin_start (dialog->priv->cleanup_frame, 6);
    dialog->priv->cleanup_timeframe_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (dialog->priv->cleanup_timeframe_combo), 0, _("Last Hour"));
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (dialog->priv->cleanup_timeframe_combo), 1, _("Last Two Hours"));
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (dialog->priv->cleanup_timeframe_combo), 2, _("Last Four Hours"));
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (dialog->priv->cleanup_timeframe_combo), 3, _("Today"));
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (dialog->priv->cleanup_timeframe_combo), 4, _("Everything"));
    g_signal_connect (dialog->priv->cleanup_timeframe_combo, "changed",
                      G_CALLBACK (cb_rstto_privacy_dialog_combobox_timeframe_changed), dialog);
    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->cleanup_timeframe_combo), 0);

    gtk_box_pack_start (GTK_BOX (dialog->priv->cleanup_vbox),
                        display_main_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (display_main_hbox),
                        display_main_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (display_main_hbox),
                        dialog->priv->cleanup_timeframe_combo, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                       dialog->priv->cleanup_frame);

    gtk_widget_show_all (dialog->priv->cleanup_frame);

    /* Window should not be resizable */
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    button = xfce_gtk_button_new_mixed ("gtk-cancel", _("_Cancel"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
    gtk_widget_show (button);
    button = xfce_gtk_button_new_mixed ("gtk-apply", _("_Apply"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
    gtk_widget_show (button);
}

static void
rstto_privacy_dialog_class_init (RsttoPrivacyDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GParamSpec   *pspec;

    object_class->finalize = rstto_privacy_dialog_finalize;
    object_class->set_property = rstto_privacy_dialog_set_property;
    object_class->get_property = rstto_privacy_dialog_get_property;

    pspec = g_param_spec_object ("filter",
                                 "",
                                 "",
                                 GTK_TYPE_RECENT_FILTER,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_FILTER,
                                     pspec);
    pspec = g_param_spec_object ("recent-manager",
                                 "",
                                 "",
                                 GTK_TYPE_RECENT_MANAGER,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_RECENT_MANAGER,
                                     pspec);
    pspec = g_param_spec_int    ("limit",
                                 "",
                                 "",
                                 -1,
                                 100,
                                 -1,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_LIMIT,
                                     pspec);
    pspec = g_param_spec_boolean ("local-only",
                                  "",
                                  "",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_LOCAL_ONLY,
                                     pspec);

    pspec = g_param_spec_boolean ("select-multiple",
                                  "",
                                  "",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_SELECT_MULTIPLE,
                                     pspec);
    pspec = g_param_spec_boolean ("show-tips",
                                  "",
                                  "",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_SHOW_TIPS,
                                     pspec);
    pspec = g_param_spec_boolean ("show-icons",
                                  "",
                                  "",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_SHOW_ICONS,
                                     pspec);
    pspec = g_param_spec_boolean ("show-not-found",
                                  "",
                                  "",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_SHOW_NOT_FOUND,
                                     pspec);
    pspec = g_param_spec_boolean ("show-private",
                                  "",
                                  "",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_SHOW_PRIVATE,
                                     pspec);
    pspec = g_param_spec_enum   ("sort-type",
                                 "",
                                 "",
                                 GTK_TYPE_RECENT_SORT_TYPE,
                                 GTK_RECENT_SORT_NONE,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_SORT_TYPE,
                                     pspec);
}

static void
rstto_recent_chooser_init (GtkRecentChooserIface *iface)
{
    iface->add_filter = rstto_recent_chooser_add_filter;
    iface->get_items  = rstto_recent_chooser_get_items;
}

static void
rstto_privacy_dialog_finalize (GObject *object)
{
    RsttoPrivacyDialog *dialog = RSTTO_PRIVACY_DIALOG (object);

    if (dialog->priv->settings)
    {
        g_object_unref (dialog->priv->settings);
        dialog->priv->settings = NULL;
    }

    if (dialog->priv->filters)
    {
        g_slist_free (dialog->priv->filters);
        dialog->priv->filters = NULL;
    }

    G_OBJECT_CLASS (rstto_privacy_dialog_parent_class)->finalize (object);
}


static void
rstto_privacy_dialog_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    RsttoPrivacyDialog *dialog = RSTTO_PRIVACY_DIALOG (object);

    switch (property_id)
    {
        case PROP_RECENT_MANAGER:
            dialog->priv->recent_manager =  g_value_get_object (value);
            break;
    }

}

static void
rstto_privacy_dialog_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
}

/******************************************/
/* GtkRecentChooserIface static functions */
/*                                        */
/******************************************/

static void
rstto_recent_chooser_add_filter (
        GtkRecentChooser  *chooser,
        GtkRecentFilter   *filter)
{
    RsttoPrivacyDialog *dialog = RSTTO_PRIVACY_DIALOG (chooser);

    /* Add the filter to the list of available filters */
    dialog->priv->filters =
            g_slist_append (dialog->priv->filters, filter);
}

static GList *
rstto_recent_chooser_get_items (
        GtkRecentChooser  *chooser)
{
    RsttoPrivacyDialog *dialog = RSTTO_PRIVACY_DIALOG (chooser);
    GList *all_items = gtk_recent_manager_get_items (dialog->priv->recent_manager);
    GList *all_items_iter = all_items;
    GList *items = g_list_copy (all_items);
    GList *lp;
    GSList *filters;
    GtkRecentInfo *info;
    GtkRecentFilterInfo filter_info;
    gsize n_applications;

    for (lp = items; lp != NULL; lp = lp->next)
        gtk_recent_info_ref (lp->data);

    while (NULL != all_items_iter)
    {
        info = all_items_iter->data;

        filter_info.contains = GTK_RECENT_FILTER_URI | GTK_RECENT_FILTER_APPLICATION;
        filter_info.uri = gtk_recent_info_get_uri (info);
        filter_info.applications = (const gchar **) gtk_recent_info_get_applications (info, &n_applications);

        if (!gtk_recent_filter_filter (dialog->priv->timeframe_filter, &filter_info))
        {
            items = g_list_remove (items, info);
            gtk_recent_info_unref (info);
        }
        else
        {
            filters = dialog->priv->filters;

            while (NULL != filters)
            {
                if (!gtk_recent_filter_filter (filters->data, &filter_info))
                {
                    items = g_list_remove (items, info);
                    gtk_recent_info_unref (info);
                    break;
                }

                filters = g_slist_next (filters);
            }
        }

        g_strfreev ((gchar **) filter_info.applications);
        all_items_iter = g_list_next (all_items_iter);
    }

    g_list_free_full (all_items, (GDestroyNotify) gtk_recent_info_unref);

    return items;
}




/***************/
/*  CALLBACKS  */
/***************/
static void
cb_rstto_privacy_dialog_combobox_timeframe_changed (GtkComboBox *combobox, gpointer user_data)
{
    RsttoPrivacyDialog *dialog = user_data;
    struct tm *time_info;

    switch (gtk_combo_box_get_active (combobox))
    {
        case 0:
            dialog->priv->time_offset = 3600;
            break;
        case 1:
            dialog->priv->time_offset = 7200;
            break;
        case 2:
            dialog->priv->time_offset = 14200;
            break;
        case 3:
            /* Convert to localtime */
            time_info = localtime (&(dialog->priv->time_now));

            dialog->priv->time_offset = (time_info->tm_hour * 3600) +
                                        (time_info->tm_min * 60) +
                                        time_info->tm_sec;
            break;
        case 4:
            dialog->priv->time_offset = dialog->priv->time_now;
            break;
    }
}

gboolean
cb_rstto_recent_filter_filter_timeframe (
        const GtkRecentFilterInfo *filter_info,
        gpointer user_data)
{
    RsttoPrivacyDialog *dialog = user_data;
    GtkRecentInfo *info = gtk_recent_manager_lookup_item (dialog->priv->recent_manager, filter_info->uri, NULL);
    const time_t visited = gtk_recent_info_get_visited (info);

    gtk_recent_info_unref (info);

    return (dialog->priv->time_now - visited) < dialog->priv->time_offset;
}

/********************/
/* Public functions */
/********************/

GtkWidget *
rstto_privacy_dialog_new (GtkWindow *parent, GtkRecentManager *recent_manager)
{
    GtkWidget *dialog = g_object_new (RSTTO_TYPE_PRIVACY_DIALOG,
                                      "title", _("Clear private data"),
                                      "icon-name", "edit-clear",
                                      "recent-manager", recent_manager,
                                      NULL);

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

    return dialog;
}
