/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#define VERSION_STRING VERSION "-" REVISION
#else
#define VERSION_STRING VERSION
#endif

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libnotify/notify.h>

#include <common/xfce-notify-common.h>
#include <common/xfce-notify-enum-types.h>
#include <common/xfce-notify-log-gbus.h>
#include <common/xfce-notify-log-util.h>

#include "xfce-notify-config-ui.h"
#include "xfce-notify-log-viewer.h"

typedef struct
{
    GtkWidget  *log_level;
    GtkWidget  *log_level_apps;
    GtkWidget  *log_level_apps_label;
    GtkWidget  *infobar_label;
} NotificationLogWidgets;

typedef struct {
    XfconfChannel *channel;
    XfceNotifyLogGBus *log;

    GtkWidget *known_applications_listbox;

    NotificationLogWidgets log_widgets;
} SettingsPanel;

typedef struct
{
    const gchar *app_id;
    gchar *app_name;
    gint count;
} LogAppCount;

typedef struct
{
    SettingsPanel *panel;

    gchar *known_application;
    gchar *known_application_name;

    GtkWidget *app_label;
    GtkWidget *mute_switch;
    GtkWidget *allow_criticals;
    GtkWidget *include_in_log;
} KnownApplicationSignalData;

static void handle_app_switch_property_changed(XfconfChannel *channel,
                                               const gchar *property_name,
                                               const GValue *value,
                                               KnownApplicationSignalData *signal_data);

static void
xfce_notifyd_config_show_notification_callback(NotifyNotification *notification,
                                               const char         *action,
                                               gpointer            unused)
{
  /* Don't do anything, we just have a button to show its style */
}

static void
xfce_notifyd_config_show_notification_preview(GtkWindow *parent_window)
{
    NotifyNotification *notification;
    GError             *error = NULL;

    notification =
        notify_notification_new(_("Notification Preview"),
                                _("This is what notifications will look like"),
                                "org.xfce.notification");

    notify_notification_add_action(notification,
                                   "button",
                                   _("Button"),
                                   (NotifyActionCallback) xfce_notifyd_config_show_notification_callback,
                                   NULL,
                                   NULL);

    if (!notify_notification_show(notification, &error)) {
        xfce_dialog_show_error(parent_window, error,
                               _("Notification preview failed"));

        g_error_free(error);
    }

    g_object_unref(notification);
}

static gchar *
xfce4_notifyd_slider_format_value(GtkScale *slider,
                                  gdouble value,
                                  gpointer user_data)
{
    return g_strdup_printf("%d%%", (gint)(value * 100));
}

static void
xfce4_notifyd_config_theme_combo_changed(GtkComboBox *theme_combo,
                                         gpointer user_data)
{
    XfconfChannel *channel = user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *theme = NULL;

    if(!gtk_combo_box_get_active_iter(theme_combo, &iter))
        return;

    model = gtk_combo_box_get_model (theme_combo);

    gtk_tree_model_get(model, &iter, 0, &theme, -1);
    xfconf_channel_set_string(channel, "/theme", theme);
    g_free(theme);
}

static void
xfce4_notifyd_config_theme_changed(XfconfChannel *channel,
                                   const gchar *property,
                                   const GValue *value,
                                   gpointer user_data)
{
    GtkWidget *theme_combo = user_data;
    GtkListStore *ls;
    GtkTreeIter iter;
    gchar *theme;
    const gchar *new_theme;

    new_theme = G_VALUE_TYPE(value) ? g_value_get_string(value) : "Default";

    ls = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(theme_combo)));

    for(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ls), &iter);
        gtk_list_store_iter_is_valid(ls, &iter);
        gtk_tree_model_iter_next(GTK_TREE_MODEL(ls), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 0, &theme, -1);
        if(!strcmp(theme, new_theme)) {

            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(theme_combo),
                                          &iter);
            g_free(theme);

            xfce_notifyd_config_show_notification_preview(GTK_WINDOW(gtk_widget_get_toplevel(theme_combo)));

            return;
        }
        g_free(theme);
    }

    gtk_list_store_append(ls, &iter);
    gtk_list_store_set(ls, &iter, 0, new_theme, -1);
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(theme_combo), &iter);
}

static void
list_store_add_themes_in_dir(GtkListStore *ls,
                             const gchar *path,
                             const gchar *current_theme,
                             GHashTable *themes_seen,
                             GtkTreeIter *current_theme_iter)
{
    GDir *dir;
    const gchar *file;
    gchar *old_filename = NULL;

    dir = g_dir_open(path, 0, NULL);
    if(!dir)
        return;

    while((file = g_dir_read_name(dir))) {
        gchar *filename;

        if(g_hash_table_lookup(themes_seen, file))
            continue;

        filename =
            g_build_filename(path, file, "xfce-notify-4.0", "gtk.css", NULL);

        if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
            GtkTreeIter iter;
            if (old_filename != NULL) {
                if (g_ascii_strcasecmp (old_filename, filename) < 0)
                    gtk_list_store_append(ls, &iter);
                else
                    gtk_list_store_prepend(ls, &iter);
                g_free (old_filename);
                old_filename = NULL;
            }
            else
                gtk_list_store_append(ls, &iter);

            gtk_list_store_set(ls, &iter, 0, file, -1);
            g_hash_table_insert(themes_seen, g_strdup(file),
                                GUINT_TO_POINTER(1));

            if(!strcmp(file, current_theme))
                memcpy(current_theme_iter, &iter, sizeof(iter));
        }

        g_free (old_filename);
        old_filename = filename;
    }
    g_free (old_filename);

    g_dir_close(dir);
}

static void
xfce4_notifyd_config_setup_theme_combo(GtkWidget *theme_combo,
                                       const gchar *current_theme)
{
    GtkListStore *ls;
    gchar *dirname, **dirnames;
    GHashTable *themes_seen;
    gint i;
    GtkTreeIter current_theme_iter;

    ls = gtk_list_store_new(1, G_TYPE_STRING);
    themes_seen = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        (GDestroyNotify)g_free, NULL);

    dirname = g_build_filename(xfce_get_homedir(), ".themes", NULL);
    list_store_add_themes_in_dir(ls, dirname, current_theme,
                                 themes_seen, &current_theme_iter);
    g_free(dirname);

    dirnames = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "themes/");
    for(i = 0; dirnames && dirnames[i]; ++i)
        list_store_add_themes_in_dir(ls, dirnames[i], current_theme,
                                     themes_seen, &current_theme_iter);
    g_strfreev(dirnames);

    g_hash_table_destroy(themes_seen);

    gtk_combo_box_set_model(GTK_COMBO_BOX(theme_combo), GTK_TREE_MODEL(ls));

    if(gtk_list_store_iter_is_valid(ls, &current_theme_iter))
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(theme_combo),
                                      &current_theme_iter);

    g_object_unref(G_OBJECT(ls));
}

static void
xfce_notifyd_config_dialog_response(GtkWidget *dialog, gint response, gpointer unused)
{
  if (response == 0)
      g_signal_stop_emission_by_name (dialog, "response");
}

static void
xfce_notifyd_config_preview_clicked(GtkButton *button)
{
  GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (button));

  xfce_notifyd_config_show_notification_preview (GTK_WINDOW (window));
}

/* Shows a separator before each row. */
static void
display_header_func (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       user_data)
{
  if (before != NULL)
    {
      GtkWidget *header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (header);

      gtk_list_box_row_set_header (row, header);
    }
}

static gboolean
xfce_g_strv_remove(gchar **strv,
                   const gchar *to_remove)
{
    gint i;

    for (i = 0; strv[i] != NULL; ++i) {
        if (strcmp(strv[i], to_remove) == 0) {
            break;
        }
    }

    if (G_LIKELY(strv[i] != NULL)) {
        gint remaining = g_strv_length(&strv[i+1]);

        g_free(strv[i]);

        if (remaining > 0) {
            memmove(&strv[i], &strv[i+1], remaining * sizeof(gchar *));
        }
        strv[i+remaining] = NULL;

        return TRUE;
    } else {
        return FALSE;
    }
}

static void
xfce_add_to_string_list_property(XfconfChannel *channel,
                                 const gchar *property_name,
                                 gchar **strv,
                                 const gchar *to_add)
{
    if (strv == NULL || !g_strv_contains((const gchar *const *)strv, to_add)) {
        gint len = strv == NULL ? 0 : g_strv_length(strv);
        gchar **new_strv = g_new(gchar *, len + 2);
        if (len > 0) {
            memcpy(new_strv, strv, len * sizeof(gchar *));
        }
        new_strv[len] = (gchar *)to_add;
        new_strv[len + 1] = NULL;
        xfconf_channel_set_string_list(channel, property_name, (const gchar *const *)new_strv);
        g_free(new_strv);
    }
}

static void
xfce_remove_from_string_list_property(XfconfChannel *channel,
                                      const gchar *property_name,
                                      gchar **strv,
                                      const gchar *to_remove)
{
    if (strv != NULL) {
        if (xfce_g_strv_remove(strv, to_remove)) {
            if (strv[0] == NULL) {
                xfconf_channel_reset_property(channel, property_name, FALSE);
            } else {
                xfconf_channel_set_string_list(channel, property_name, (const gchar *const *)strv);
            }
        }
    }
}

static void
handle_app_switch_widget_toggled(KnownApplicationSignalData *signal_data,
                                 const gchar *property_name,
                                 gboolean add_to_list)
{
    gchar **strs = xfconf_channel_get_string_list(signal_data->panel->channel, property_name);

    g_signal_handlers_block_by_func(signal_data->panel->channel,
                                    handle_app_switch_property_changed,
                                    signal_data);
    if (add_to_list) {
        xfce_add_to_string_list_property(signal_data->panel->channel, property_name, strs, signal_data->known_application);
    } else {
        xfce_remove_from_string_list_property(signal_data->panel->channel, property_name, strs, signal_data->known_application);
    }
    g_signal_handlers_unblock_by_func(signal_data->panel->channel,
                                      handle_app_switch_property_changed,
                                      signal_data);


    g_strfreev(strs);
}

static void
xfce4_notifyd_mute_switch_activated(GtkSwitch *sw,
                                    gboolean state,
                                    KnownApplicationSignalData *signal_data)
{
    handle_app_switch_widget_toggled(signal_data, MUTED_APPLICATIONS_PROP, state);
    gtk_widget_set_sensitive(signal_data->app_label, !state);
}

static void
xfce4_notifyd_allow_criticals_switch_activated(GtkSwitch *sw,
                                               gboolean state,
                                               KnownApplicationSignalData *signal_data)
{
    handle_app_switch_widget_toggled(signal_data, DENIED_CRITICAL_NOTIFICATIONS_PROP, !state);
}

static void
xfce4_notifyd_include_in_log_switch_activated(GtkSwitch *sw,
                                              gboolean state,
                                              KnownApplicationSignalData *signal_data)
{
    handle_app_switch_widget_toggled(signal_data, EXCLUDED_FROM_LOG_APPLICATIONS_PROP, !state);
}

static void
handle_app_switch_property_changed(XfconfChannel *channel,
                                   const gchar *property_name,
                                   const GValue *value,
                                   KnownApplicationSignalData *signal_data)
{
    gboolean found = FALSE;
    GtkWidget *switch_to_set;
    GtkWidget *label_to_sensitize = NULL;
    gpointer callback_to_block;
    gboolean reverse_sense;

    if (g_strcmp0(property_name, MUTED_APPLICATIONS_PROP) == 0) {
        switch_to_set = signal_data->mute_switch;
        label_to_sensitize = signal_data->app_label;
        callback_to_block = xfce4_notifyd_mute_switch_activated;
        reverse_sense = FALSE;
    } else if (g_strcmp0(property_name, DENIED_CRITICAL_NOTIFICATIONS_PROP) == 0) {
        switch_to_set = signal_data->allow_criticals;
        callback_to_block = xfce4_notifyd_allow_criticals_switch_activated;
        reverse_sense = TRUE;
    } else if (g_strcmp0(property_name, EXCLUDED_FROM_LOG_APPLICATIONS_PROP) == 0) {
        switch_to_set = signal_data->include_in_log;
        callback_to_block = xfce4_notifyd_include_in_log_switch_activated;
        reverse_sense = TRUE;
    } else {
        return;
    }

    if (G_VALUE_HOLDS(value, G_TYPE_PTR_ARRAY)) {
        GPtrArray *strs = g_value_get_boxed(value);
        for (guint i = 0; i < strs->len; ++i) {
            const GValue *str = g_ptr_array_index(strs, i);
            if (G_VALUE_HOLDS_STRING(str)) {
                if (g_strcmp0(g_value_get_string(str), signal_data->known_application) == 0) {
                    found = TRUE;
                    break;
                }
            }
        }
    }

    g_signal_handlers_block_by_func(signal_data->mute_switch,
                                    callback_to_block,
                                    signal_data);
    gtk_switch_set_active(GTK_SWITCH(switch_to_set), reverse_sense ? !found : found);
    g_signal_handlers_unblock_by_func(signal_data->mute_switch,
                                      callback_to_block,
                                      signal_data);
    if (label_to_sensitize != NULL) {
        gtk_widget_set_sensitive(label_to_sensitize, reverse_sense ? found : !found);
    }
}

static void
listbox_remove_all (GtkWidget *widget, gpointer user_data)
{
    GtkWidget *container = user_data;
    gtk_container_remove (GTK_CONTAINER (container), widget);
}

static gint
xfce_notify_sort_apps_in_log (gconstpointer a, gconstpointer b)
{
    const LogAppCount *entry1 = *((LogAppCount **) a);
    const LogAppCount *entry2 = *((LogAppCount **) b);

    gint count_diff = entry2->count - entry1->count;
    if (count_diff != 0) {
        return count_diff;
    } else {
        return g_utf8_collate(entry1->app_name, entry2->app_name);
    }
}

static GPtrArray *
xfce_notify_count_apps_in_log(GHashTable *app_id_counts, GPtrArray *known_applications) {
    GPtrArray *log_stats = g_ptr_array_new();

    if (known_applications != NULL) {
        GHashTable *counts = g_hash_table_new(g_str_hash, g_str_equal);
        GList *app_ids;
        GList *keys;

        for (guint i = 0; i < known_applications->len; ++i) {
            GValue *known_application = g_ptr_array_index(known_applications, i);
            if (G_VALUE_HOLDS_STRING(known_application)) {
                g_hash_table_insert(counts, (gchar *)g_value_get_string(known_application), GUINT_TO_POINTER(0));
            }
        }

        app_ids = g_hash_table_get_keys(app_id_counts);
        for (GList *l = app_ids; l != NULL; l = l->next) {
            gchar *app_id = l->data != NULL ? l->data : "";
            if (g_hash_table_contains(counts, app_id)) {
                g_hash_table_insert(counts, app_id, g_hash_table_lookup(app_id_counts, app_id));
            }
        }
        g_list_free(app_ids);

        keys = g_hash_table_get_keys(counts);
        for (GList *l = keys; l != NULL; l = l->next) {
            LogAppCount *entry = g_new0(LogAppCount, 1);
            const gchar *app_id = l->data;
            gchar *app_name;

            app_name = notify_get_from_desktop_file(app_id, G_KEY_FILE_DESKTOP_KEY_NAME);
            if (app_name == NULL) {
                /* Fallback: Just set the name provided by the application */
                app_name = g_strdup(app_id);
            }

            entry->app_id = app_id;
            entry->app_name = app_name;
            entry->count = GPOINTER_TO_UINT(g_hash_table_lookup(counts, app_id));
            g_ptr_array_add(log_stats, entry);
        }
        g_list_free(keys);

        g_hash_table_destroy(counts);
        g_ptr_array_sort(log_stats, xfce_notify_sort_apps_in_log);
    }

    return log_stats;
}

static void
known_application_signal_data_free(KnownApplicationSignalData *signal_data)
{
    g_signal_handlers_disconnect_by_func(signal_data->panel->channel,
                                         handle_app_switch_property_changed,
                                         signal_data);

    g_free(signal_data->known_application);
    g_free(signal_data->known_application_name);
    g_slice_free(KnownApplicationSignalData, signal_data);
}

static void
known_application_delete_clicked(GtkWidget *button,
                                 KnownApplicationSignalData *signal_data)
{
    gchar *confirm_text = g_strdup_printf( _("Are you sure you want to delete notification settings for application \"%s\"?"),
                                          signal_data->known_application_name);
    if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(button)),
                            "edit-delete",
                            _("Delete"),
                            confirm_text,
                            _("Forget Application")))
    {
        gchar **known_applications = xfconf_channel_get_string_list(signal_data->panel->channel, KNOWN_APPLICATIONS_PROP);

        if (G_LIKELY(known_applications != NULL)) {
            gchar **muted_applications = xfconf_channel_get_string_list(signal_data->panel->channel, MUTED_APPLICATIONS_PROP);
            gchar **denied_critical_notifications = xfconf_channel_get_string_list(signal_data->panel->channel, DENIED_CRITICAL_NOTIFICATIONS_PROP);

            xfce_remove_from_string_list_property(signal_data->panel->channel, MUTED_APPLICATIONS_PROP, muted_applications, signal_data->known_application);
            xfce_remove_from_string_list_property(signal_data->panel->channel, DENIED_CRITICAL_NOTIFICATIONS_PROP, denied_critical_notifications, signal_data->known_application);
            // Remove from known applications list last; as doing so will indirectly free 'signal_data'
            xfce_remove_from_string_list_property(signal_data->panel->channel, KNOWN_APPLICATIONS_PROP, known_applications, signal_data->known_application);

            g_strfreev(muted_applications);
            g_strfreev(denied_critical_notifications);
            g_strfreev(known_applications);
        }
    }

    g_free(confirm_text);
}

static gboolean
test_gicon_exists(GIcon *gicon,
                  gint size,
                  gint scale)
{
    GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon_for_scale(gtk_icon_theme_get_default(),
                                                                      gicon,
                                                                      size,
                                                                      scale,
                                                                      GTK_ICON_LOOKUP_USE_BUILTIN);
    gboolean exists = icon_info != NULL;

    if (icon_info != NULL) {
        g_object_unref(icon_info);
    }

    return exists;
}

static void
xfce4_notifyd_known_application_insert_row (SettingsPanel *panel,
                                            GtkWidget *known_applications_listbox,
                                            GPtrArray *muted_applications,
                                            const gchar *const *denied_critical_applications,
                                            const gchar *const *excluded_from_log_applications,
                                            const gchar *known_application,
                                            const gchar *app_name,
                                            gint count)
{
    GtkBuilder *builder;
    GtkWidget *row;
    GtkWidget *expander;
    GtkWidget *app_label;
    GtkWidget *log_count_label;
    GtkWidget *icon;
    GtkWidget *mute_switch;
    GtkWidget *allow_criticals;
    GtkWidget *include_in_log;
    GtkWidget *delete_button;
    guint i;
    gint icon_width, icon_height, icon_size;
    gint scale_factor = gtk_widget_get_scale_factor(known_applications_listbox);
    gchar *desktop_icon_name = NULL;

    gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_width, &icon_height);
    icon_size = MIN(icon_width, icon_height);

    builder = gtk_builder_new_from_resource("/org/xfce/notifyd/settings/xfce4-notifyd-config-known-app.glade");
    if (G_UNLIKELY(builder == NULL)) {
        g_critical("Unable to load known app UI description");
        return;
    }

    expander = GTK_WIDGET(gtk_builder_get_object(builder, "app_item"));
    icon = GTK_WIDGET(gtk_builder_get_object(builder, "app_icon"));
    app_label = GTK_WIDGET(gtk_builder_get_object(builder, "app_name"));
    log_count_label = GTK_WIDGET(gtk_builder_get_object(builder, "app_log_count"));
    mute_switch = GTK_WIDGET(gtk_builder_get_object(builder, "app_mute"));
    allow_criticals = GTK_WIDGET(gtk_builder_get_object(builder, "allow_critical_notifications"));
    include_in_log = GTK_WIDGET(gtk_builder_get_object(builder, "include_in_log"));
    delete_button = GTK_WIDGET(gtk_builder_get_object(builder, "delete_app"));

    row = gtk_list_box_row_new();
    gtk_list_box_insert(GTK_LIST_BOX(known_applications_listbox), row, -1);
    gtk_container_add(GTK_CONTAINER(row), expander);

    /* Number of notifications in the log (if enabled) */
    if (count > 0) {
        gtk_label_set_text (GTK_LABEL(log_count_label), g_strdup_printf("%d", count));
    }

    /* All applications that don't supply their name at all */
    if (xfce_str_is_empty (known_application)) {
        const char *format = "<span style=\"italic\">\%s</span>";
        char *markup;

        known_application = g_strdup(_("Unspecified applications"));
        markup = g_markup_printf_escaped (format, known_application);
        gtk_label_set_markup (GTK_LABEL (app_label), markup);
        g_free (markup);

        gtk_widget_set_sensitive(mute_switch, FALSE);
        gtk_widget_set_sensitive(allow_criticals, FALSE);
        gtk_widget_set_sensitive(include_in_log, FALSE);
        gtk_widget_set_sensitive(delete_button, FALSE);
    } else {
        KnownApplicationSignalData *signal_data = g_slice_new0(KnownApplicationSignalData);
        GIcon *gicon = NULL;

        signal_data->panel = panel;
        signal_data->known_application = g_strdup(known_application);
        signal_data->app_label = app_label;
        signal_data->mute_switch = mute_switch;
        signal_data->allow_criticals = allow_criticals;
        signal_data->include_in_log = include_in_log;

        /* Try to find the correct icon based on the desktop file */
        desktop_icon_name = notify_get_from_desktop_file (known_application, G_KEY_FILE_DESKTOP_KEY_ICON);
        if (desktop_icon_name != NULL) {
            if (g_path_is_absolute(desktop_icon_name)
                && g_file_test(desktop_icon_name, G_FILE_TEST_EXISTS)
                && !g_file_test(desktop_icon_name, G_FILE_TEST_IS_DIR))
            {
                GFile *file = g_file_new_for_path(desktop_icon_name);
                gicon = g_file_icon_new(file);
                g_object_unref(file);
            } else {
                gicon = g_themed_icon_new_with_default_fallbacks(desktop_icon_name);
            }

            if (!test_gicon_exists(gicon, icon_size, scale_factor)) {
                g_clear_object(&gicon);
            }
        }

        /* Fallback: Try to naively load icon names */
        if (gicon == NULL) {
            /* Find icons in the right priority:
               1. normal icon name with fallback
               2. lowercase icon name with fallback */

            gicon = g_themed_icon_new_with_default_fallbacks(known_application);
            if (!test_gicon_exists(gicon, icon_size, scale_factor)) {
                g_clear_object(&gicon);
            }

            if (gicon == NULL) {
                gchar *icon_name_dashed = g_strdelimit(g_strdup(known_application), " .", '-');

                gicon = g_themed_icon_new_with_default_fallbacks(icon_name_dashed);
                if (!test_gicon_exists(gicon, icon_size, scale_factor)) {
                    gchar *icon_name_dashed_lower = g_ascii_strdown(icon_name_dashed, -1);

                    g_clear_object(&gicon);
                    gicon = g_themed_icon_new_with_default_fallbacks(icon_name_dashed_lower);
                    if (!test_gicon_exists(gicon, icon_size, scale_factor)) {
                        g_clear_object(&gicon);
                    }

                    g_free(icon_name_dashed_lower);
                }

                g_free(icon_name_dashed);
            }
        }

        if (G_LIKELY(gicon != NULL)) {
            gtk_image_set_pixel_size(GTK_IMAGE(icon), icon_size);
            gtk_image_set_from_gicon(GTK_IMAGE(icon), gicon, GTK_ICON_SIZE_LARGE_TOOLBAR);
            g_object_unref(gicon);
        }

        signal_data->known_application_name = g_strdup(app_name);
        gtk_label_set_text(GTK_LABEL(app_label), app_name);

        /* Set correct initial value as to whether an application is muted */
        if (muted_applications != NULL) {
            for (i = 0; i < muted_applications->len; i++) {
                GValue *muted_application;
                muted_application = g_ptr_array_index (muted_applications, i);
                if (g_str_match_string (g_value_get_string (muted_application), known_application, FALSE) == TRUE) {
                    gtk_widget_set_sensitive(app_label, FALSE);
                    gtk_switch_set_active (GTK_SWITCH (mute_switch), TRUE);
                    break;
                }
            }
        }
        g_signal_connect(mute_switch, "state-set",
                         G_CALLBACK(xfce4_notifyd_mute_switch_activated), signal_data);
        g_signal_connect(panel->channel, "property-changed::" MUTED_APPLICATIONS_PROP,
                         G_CALLBACK(handle_app_switch_property_changed), signal_data);

        gtk_switch_set_active(GTK_SWITCH(allow_criticals),
                              denied_critical_applications == NULL
                              || !g_strv_contains(denied_critical_applications, known_application));
        g_signal_connect(allow_criticals, "state-set",
                         G_CALLBACK(xfce4_notifyd_allow_criticals_switch_activated), signal_data);
        g_signal_connect(panel->channel, "property-changed::" DENIED_CRITICAL_NOTIFICATIONS_PROP,
                         G_CALLBACK(handle_app_switch_property_changed), signal_data);

        gtk_switch_set_active(GTK_SWITCH(include_in_log),
                              excluded_from_log_applications == NULL
                              || !g_strv_contains(excluded_from_log_applications, known_application));
        g_signal_connect(include_in_log, "state-set",
                         G_CALLBACK(xfce4_notifyd_include_in_log_switch_activated), signal_data);
        g_signal_connect(panel->channel, "property-changed::" EXCLUDED_FROM_LOG_APPLICATIONS_PROP,
                         G_CALLBACK(handle_app_switch_property_changed), signal_data);

        g_signal_connect(delete_button, "clicked",
                         G_CALLBACK(known_application_delete_clicked), signal_data);

        g_object_set_data_full(G_OBJECT(row),
                               "xfce4-notifyd-known-application-signal-data", signal_data,
                               (GDestroyNotify)known_application_signal_data_free);
    }

    g_free(desktop_icon_name);
}

static void
known_application_log_counts_fetched(GObject *source, GAsyncResult *res, SettingsPanel *panel) {
    GtkWidget *known_applications_listbox = panel->known_applications_listbox;
    GtkCallback func = listbox_remove_all;
    GPtrArray *known_applications;
    GPtrArray *known_applications_sorted;
    GPtrArray *muted_applications;
    gchar **denied_critical_applications;
    gchar **excluded_from_log_applications;
    guint i;
    GVariant *app_id_countsv = NULL;
    GHashTable *app_id_counts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GError *error = NULL;

    if (source != NULL
        && res != NULL
        && xfce_notify_log_gbus_call_get_app_id_counts_finish(XFCE_NOTIFY_LOG_GBUS(source),
                                                              &app_id_countsv,
                                                              res,
                                                              &error))
    {
        if (g_variant_is_of_type(app_id_countsv, G_VARIANT_TYPE("a{su}"))) {
            GVariantIter *iter = g_variant_iter_new(app_id_countsv);
            gchar *app_id = NULL;
            guint count = 0;

            while (g_variant_iter_next(iter, "{su}", &app_id, &count)) {
                g_hash_table_replace(app_id_counts, app_id != NULL ? app_id : g_strdup(""), GUINT_TO_POINTER(count));
            }
            g_variant_iter_free(iter);
        } else {
            g_warning("Returned known app ID counts signature is wrong (%s)", g_variant_get_type_string(app_id_countsv));
        }
        g_variant_unref(app_id_countsv);
    } else {
        if (error != NULL) {
            g_warning("Failed to fetch known app ID counts from log: %s", error->message);
            g_clear_error(&error);
        }
    }

    known_applications = xfconf_channel_get_arrayv (panel->channel, KNOWN_APPLICATIONS_PROP);
    muted_applications = xfconf_channel_get_arrayv (panel->channel, MUTED_APPLICATIONS_PROP);
    denied_critical_applications = xfconf_channel_get_string_list(panel->channel, DENIED_CRITICAL_NOTIFICATIONS_PROP);
    excluded_from_log_applications = xfconf_channel_get_string_list(panel->channel, EXCLUDED_FROM_LOG_APPLICATIONS_PROP);

    /* TODO: Check the old list versus the new one and only add/remove rows
       as needed instead instead of cleaning up the whole widget */
    /* Clean up the list and re-fill it */
    gtk_container_foreach (GTK_CONTAINER (known_applications_listbox), func, known_applications_listbox);

    if (known_applications != NULL) {
        /* Sort the apps based on their appearance in the log */
        known_applications_sorted = xfce_notify_count_apps_in_log(app_id_counts, known_applications);

        for (i = 0; i < known_applications_sorted->len; i++) {
            LogAppCount *application = g_ptr_array_index(known_applications_sorted, i);
            xfce4_notifyd_known_application_insert_row(panel,
                                                       known_applications_listbox,
                                                       muted_applications,
                                                       (const gchar *const *)denied_critical_applications,
                                                       (const gchar *const *)excluded_from_log_applications,
                                                       application->app_id,
                                                       application->app_name,
                                                       application->count);
            g_free(application->app_name);
        }
        g_ptr_array_free(known_applications_sorted, TRUE);
    }

    xfconf_array_free (known_applications);
    xfconf_array_free (muted_applications);
    g_strfreev(denied_critical_applications);
    g_strfreev(excluded_from_log_applications);
    gtk_widget_show_all (known_applications_listbox);
    g_hash_table_destroy(app_id_counts);
}

static void
xfce4_notifyd_known_applications_changed(SettingsPanel *panel) {
    if (panel->log != NULL) {
        xfce_notify_log_gbus_call_get_app_id_counts(panel->log,
                                                    NULL,
                                                    (GAsyncReadyCallback)known_application_log_counts_fetched,
                                                    panel);
    } else {
        known_application_log_counts_fetched(NULL, NULL, panel);
    }
}

static void
xfce4_notifyd_do_not_disturb_activated (GtkSwitch *do_not_disturb_switch,
                                        gboolean state,
                                        gpointer user_data)
{
    GtkWidget *do_not_disturb_info = user_data;

    gtk_revealer_set_reveal_child (GTK_REVEALER (do_not_disturb_info),
                                    gtk_switch_get_active (GTK_SWITCH (do_not_disturb_switch)));
    gtk_switch_set_active (GTK_SWITCH (do_not_disturb_switch), state);
}

static void
xfce4_notifyd_log_activated (GtkSwitch *log_switch,
                             gboolean state,
                             SettingsPanel *panel)
{
    NotificationLogWidgets *log_widgets = &panel->log_widgets;
    const char *format = _("<b>Currently only urgent notifications are shown.</b>\nNotification logging is \%s.");
    char *markup;

    gtk_switch_set_state (GTK_SWITCH (log_switch), state);
    gtk_widget_set_sensitive (GTK_WIDGET (log_widgets->log_level), state);
    gtk_widget_set_sensitive (GTK_WIDGET (log_widgets->log_level_apps), state);
    gtk_widget_set_sensitive (GTK_WIDGET (log_widgets->log_level_apps_label), state);
    markup = g_markup_printf_escaped (format, state ? _("enabled") : _("disabled"));
    gtk_label_set_markup (GTK_LABEL (log_widgets->infobar_label), markup);
    g_free (markup);
}

static void xfce4_notifyd_show_help(GtkButton *button,
                                    GtkWidget *dialog)
{
    xfce_dialog_show_help_with_version(GTK_WINDOW(dialog), "notifyd", "start", NULL, NULL);
}

static void xfce_notify_bus_name_appeared_cb (GDBusConnection *connection,
                                              const gchar *name,
                                              const gchar *name_owner,
                                              gpointer user_data)
{
    GtkWidget  *notifyd_running = user_data;

    gtk_revealer_set_reveal_child (GTK_REVEALER (notifyd_running), FALSE);
}

static void xfce_notify_bus_name_vanished_cb (GDBusConnection *connection,
                                              const gchar *name,
                                              gpointer user_data)
{
    GtkWidget *notifyd_running = user_data;

    gtk_revealer_set_reveal_child (GTK_REVEALER (notifyd_running), TRUE);
}

static void
datetime_format_changed(GtkWidget *combo, GtkWidget *entry) {
    gtk_widget_set_sensitive(entry, gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) == XFCE_NOTIFY_DATETIME_CUSTOM);
}

static GtkWidget *
xfce4_notifyd_config_setup_dialog(SettingsPanel *panel, GtkBuilder *builder) {
    GtkWidget *dlg;
    GtkWidget *btn;
    GtkWidget *sbtn;
    GtkWidget *slider;
    GtkWidget *theme_combo;
    GtkWidget *position_combo;
    GtkWidget *help_button;
    GtkWidget *known_applications_scrolled_window;
    GtkWidget *known_applications_listbox;
    GtkWidget *placeholder_label;
    GtkWidget *display_fields_combo;
    GtkWidget *do_not_disturb_switch;
    GtkWidget *do_not_disturb_info;
    GtkWidget *log_switch;
    GtkWidget *log_viewer_box;
    GtkWidget *show_notifications_on;
    GtkWidget *mute_sounds;
    GtkWidget *do_fadeout;
    GtkWidget *do_slideout;
    GtkWidget *show_text_with_gauge;
    GtkWidget *datetime_format;
    GtkWidget *custom_datetime_format;
    GtkAdjustment *adj;
    gchar *current_theme;

    gtk_builder_connect_signals(builder, NULL);

    dlg = GTK_WIDGET(gtk_builder_get_object(builder, "notifyd_settings_dlg"));
    g_signal_connect(G_OBJECT(dlg), "response",
                     G_CALLBACK(xfce_notifyd_config_dialog_response), NULL);

    btn = GTK_WIDGET(gtk_builder_get_object(builder, "close_btn"));
    g_signal_connect_swapped(G_OBJECT(btn), "clicked",
                             G_CALLBACK(gtk_dialog_response), dlg);

    help_button = GTK_WIDGET(gtk_builder_get_object(builder, "help_btn"));
    g_signal_connect(G_OBJECT(help_button), "clicked",
                    G_CALLBACK(xfce4_notifyd_show_help), dlg);

    /**************
        GENERAL   *
     **************/
    // Behavior
    display_fields_combo = GTK_WIDGET(gtk_builder_get_object(builder, "display_fields"));
    xfconf_g_property_bind(panel->channel, NOTIFICATION_DISPLAY_FIELDS_PROP, G_TYPE_STRING,
                           G_OBJECT(display_fields_combo), "active-id");
    if (gtk_combo_box_get_active_id(GTK_COMBO_BOX(display_fields_combo)) == NULL) {
        gchar *nick = xfce_notify_enum_nick_from_value(XFCE_TYPE_NOTIFY_DISPLAY_FIELDS, DISPLAY_FIELDS_DEFAULT);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(display_fields_combo), nick);
        g_free(nick);
    }

    do_not_disturb_switch = GTK_WIDGET (gtk_builder_get_object (builder, "do_not_disturb"));
    xfconf_g_property_bind (panel->channel, "/do-not-disturb", G_TYPE_BOOLEAN,
                            G_OBJECT (do_not_disturb_switch), "active");
    /* Manually control the revealer for the infobar because of https://bugzilla.gnome.org/show_bug.cgi?id=710888 */
    do_not_disturb_info = GTK_WIDGET (gtk_builder_get_object (builder, "do_not_disturb_info"));
    gtk_revealer_set_reveal_child (GTK_REVEALER (do_not_disturb_info),
                                   gtk_switch_get_active (GTK_SWITCH (do_not_disturb_switch)));
    g_signal_connect (G_OBJECT (do_not_disturb_switch), "state-set",
                      G_CALLBACK (xfce4_notifyd_do_not_disturb_activated), do_not_disturb_info);

    btn = GTK_WIDGET(gtk_builder_get_object(builder, "gauge_ignores_dnd"));
    xfconf_g_property_bind(panel->channel, GAUGE_IGNORES_DND_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(btn), "active");

    show_notifications_on = GTK_WIDGET(gtk_builder_get_object(builder, "show_notifications_on"));
    xfconf_g_property_bind(panel->channel, SHOW_NOTIFICATIONS_ON_PROP, G_TYPE_STRING,
                           G_OBJECT(show_notifications_on), "active-id");
    if (gtk_combo_box_get_active_id(GTK_COMBO_BOX(show_notifications_on)) == NULL) {
        gchar *nick = xfce_notify_enum_nick_from_value(XFCE_TYPE_NOTIFY_SHOW_ON, SHOW_NOTIFICATIONS_ON_DEFAULT);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(show_notifications_on), nick);
        g_free(nick);
    }

    mute_sounds = GTK_WIDGET(gtk_builder_get_object(builder, "mute_sounds"));
#ifdef ENABLE_SOUND
    xfconf_g_property_bind(panel->channel, MUTE_SOUNDS_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(mute_sounds), "active");
#else
    gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "mute_sounds_label")));
    gtk_widget_hide(mute_sounds);
#endif

    // Appearance
    theme_combo = GTK_WIDGET(gtk_builder_get_object(builder, "theme_combo"));
    current_theme = xfconf_channel_get_string(panel->channel, "/theme", "Default");
    xfce4_notifyd_config_setup_theme_combo(theme_combo, current_theme);
    g_free(current_theme);
    g_signal_connect(G_OBJECT(theme_combo), "changed",
                     G_CALLBACK(xfce4_notifyd_config_theme_combo_changed),
                     panel->channel);
    g_signal_connect(G_OBJECT(panel->channel), "property-changed::/theme",
                     G_CALLBACK(xfce4_notifyd_config_theme_changed),
                     theme_combo);

    position_combo = GTK_WIDGET(gtk_builder_get_object(builder, "position_combo"));
    xfconf_g_property_bind(panel->channel, NOTIFY_LOCATION_PROP, G_TYPE_STRING,
                           G_OBJECT(position_combo), "active-id");

    GtkWidget *min_width_enabled = GTK_WIDGET(gtk_builder_get_object(builder, "min_width_enabled"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(min_width_enabled), FALSE);
    xfconf_g_property_bind(panel->channel, MIN_WIDTH_ENABLED_PROP, G_TYPE_BOOLEAN, min_width_enabled, "active");

    GtkWidget *min_width = GTK_WIDGET(gtk_builder_get_object(builder, "min_width"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(min_width), MIN_WIDTH_DEFAULT);
    xfconf_g_property_bind(panel->channel, MIN_WIDTH_PROP, G_TYPE_UINT, min_width, "value");
    g_object_bind_property(min_width_enabled, "active", min_width, "sensitive", G_BINDING_SYNC_CREATE);

    slider = GTK_WIDGET(gtk_builder_get_object(builder, "opacity_slider"));
    g_signal_connect(G_OBJECT(slider), "format-value",
                     G_CALLBACK(xfce4_notifyd_slider_format_value), NULL);
    adj = gtk_range_get_adjustment(GTK_RANGE(slider));
    xfconf_g_property_bind(panel->channel, "/initial-opacity", G_TYPE_DOUBLE,
                           G_OBJECT(adj), "value");

    btn = GTK_WIDGET(gtk_builder_get_object(builder, "expire_timeout_enabled"));
    xfconf_g_property_bind(panel->channel, EXPIRE_TIMEOUT_ENABLED_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(btn), "active");

    sbtn = GTK_WIDGET(gtk_builder_get_object(builder, "expire_timeout_sbtn"));
    xfconf_g_property_bind(panel->channel, EXPIRE_TIMEOUT_PROP, G_TYPE_INT,
                           G_OBJECT(sbtn), "value");
    g_object_bind_property(btn, "active",
                           sbtn, "sensitive",
                           G_BINDING_SYNC_CREATE);

    sbtn = GTK_WIDGET(gtk_builder_get_object(builder, "expire_timeout_allow_override"));
    xfconf_g_property_bind(panel->channel, EXPIRE_TIMEOUT_ALLOW_OVERRIDE_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(sbtn), "active");

    do_fadeout = GTK_WIDGET(gtk_builder_get_object(builder, "do_fadeout"));
    gtk_switch_set_active (GTK_SWITCH (do_fadeout), TRUE);
    xfconf_g_property_bind(panel->channel, "/do-fadeout", G_TYPE_BOOLEAN,
                           G_OBJECT(do_fadeout), "active");

    do_slideout = GTK_WIDGET(gtk_builder_get_object(builder, "do_slideout"));
    xfconf_g_property_bind(panel->channel, "/do-slideout", G_TYPE_BOOLEAN,
                           G_OBJECT(do_slideout), "active");

    show_text_with_gauge = GTK_WIDGET(gtk_builder_get_object(builder, "show_text_with_gauge"));
    xfconf_g_property_bind(panel->channel, "/show-text-with-gauge", G_TYPE_BOOLEAN,
                           G_OBJECT(show_text_with_gauge), "active");

    datetime_format = GTK_WIDGET(gtk_builder_get_object(builder, "datetime_format"));
    xfconf_g_property_bind(panel->channel, DATETIME_FORMAT_PROP, G_TYPE_STRING,
                           G_OBJECT(datetime_format), "active-id");
    if (gtk_combo_box_get_active_id(GTK_COMBO_BOX(datetime_format)) == NULL) {
        gchar *nick = xfce_notify_enum_nick_from_value(XFCE_TYPE_NOTIFY_DATETIME_FORMAT, DATETIME_FORMAT_DEFAULT);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(datetime_format), nick);
        g_free(nick);
    }

    custom_datetime_format = GTK_WIDGET(gtk_builder_get_object(builder, "custom_datetime_format"));
    xfconf_g_property_bind(panel->channel, DATETIME_CUSTOM_FORMAT_PROP, G_TYPE_STRING,
                           G_OBJECT(custom_datetime_format), "text");
    if (g_strcmp0(gtk_entry_get_text(GTK_ENTRY(custom_datetime_format)), "") == 0) {
        gtk_entry_set_text(GTK_ENTRY(custom_datetime_format), DATETIME_CUSTOM_FORMAT_DEFAULT);
    }
    gtk_widget_set_sensitive(custom_datetime_format, gtk_combo_box_get_active(GTK_COMBO_BOX(datetime_format)) == XFCE_NOTIFY_DATETIME_CUSTOM);
    g_signal_connect(datetime_format, "changed",
                     G_CALLBACK(datetime_format_changed), custom_datetime_format);

    btn = GTK_WIDGET(gtk_builder_get_object(builder, "preview_button"));
    g_signal_connect(G_OBJECT(btn), "clicked",
                     G_CALLBACK(xfce_notifyd_config_preview_clicked), NULL);

    /*******************
        APPLICATIONS   *
     *******************/
    known_applications_scrolled_window = GTK_WIDGET (gtk_builder_get_object (builder, "known_applications_scrolled_window"));
    panel->known_applications_listbox = known_applications_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(known_applications_listbox), GTK_SELECTION_NONE);
    gtk_container_add (GTK_CONTAINER (known_applications_scrolled_window), known_applications_listbox);
    gtk_list_box_set_header_func (GTK_LIST_BOX (known_applications_listbox), display_header_func, NULL, NULL);

    placeholder_label = xfce_notify_create_placeholder_label(_("<big><b>Currently there are no known applications.</b></big>"
                                                               "\nAs soon as an application sends a notification"
                                                               "\nit will appear in this list."));
    /* Initialize the list of known applications */
    xfce4_notifyd_known_applications_changed(panel);
    gtk_list_box_set_placeholder (GTK_LIST_BOX (known_applications_listbox), placeholder_label);
    gtk_widget_show_all (placeholder_label);
    g_signal_connect_swapped(G_OBJECT (panel->channel), "property-changed::" KNOWN_APPLICATIONS_PROP,
                             G_CALLBACK(xfce4_notifyd_known_applications_changed), panel);

    /**********
        LOG   *
     **********/
    panel->log_widgets.infobar_label = GTK_WIDGET (gtk_builder_get_object (builder, "infobar_label"));

    log_switch = GTK_WIDGET (gtk_builder_get_object (builder, "log_switch"));
    xfconf_g_property_bind (panel->channel, "/notification-log", G_TYPE_BOOLEAN,
                            G_OBJECT (log_switch), "active");
    g_signal_connect (G_OBJECT (log_switch), "state-set",
                      G_CALLBACK (xfce4_notifyd_log_activated), panel);

    panel->log_widgets.log_level = GTK_WIDGET (gtk_builder_get_object (builder, "log_level"));
    xfconf_g_property_bind(panel->channel, LOG_LEVEL_PROP, G_TYPE_STRING,
                           G_OBJECT(panel->log_widgets.log_level), "active-id");

    panel->log_widgets.log_level_apps_label = GTK_WIDGET (gtk_builder_get_object (builder, "log_level_apps_label"));
    panel->log_widgets.log_level_apps = GTK_WIDGET (gtk_builder_get_object (builder, "log_level_apps"));
    xfconf_g_property_bind(panel->channel, LOG_LEVEL_APPS_PROP, G_TYPE_STRING,
                          G_OBJECT(panel->log_widgets.log_level_apps), "active-id");

    btn = GTK_WIDGET(gtk_builder_get_object(builder, "log_max_size_enabled"));
    xfconf_g_property_bind(panel->channel, LOG_MAX_SIZE_ENABLED_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(btn), "active");

    sbtn = GTK_WIDGET (gtk_builder_get_object (builder, "log_max_size"));
    g_object_bind_property(btn, "active", sbtn, "sensitive", G_BINDING_SYNC_CREATE);
    xfconf_g_property_bind(panel->channel, LOG_MAX_SIZE_PROP, G_TYPE_UINT,
                           G_OBJECT(sbtn), "value");

    xfce4_notifyd_log_activated (GTK_SWITCH (log_switch), gtk_switch_get_active (GTK_SWITCH(log_switch)), panel);

    log_viewer_box = GTK_WIDGET(gtk_builder_get_object(builder, "log_viewer_box"));
    gtk_box_pack_start(GTK_BOX(log_viewer_box),
                      xfce_notify_log_viewer_new(panel->channel, panel->log),
                      TRUE, TRUE, 0);
    gtk_widget_show_all(log_viewer_box);

    return dlg;
}

int
main(int argc,
     char **argv)
{
    SettingsPanel *panel = g_new0(SettingsPanel, 1);
    GtkWidget *settings_dialog = NULL;
    GtkWidget *notifyd_running;
    GtkBuilder *builder;
    gboolean opt_version = FALSE;
    gint32 opt_socket_id = 0;
    guint watch_handle_id;
    GOptionEntry option_entries[] = {
        { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Display version information"), NULL },
        { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET_ID") },
        { NULL, },
    };
    GError *error = NULL;

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args(&argc, &argv, NULL, option_entries, PACKAGE, &error)) {
        if(G_LIKELY(error)) {
            g_printerr("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr("\n");
            g_error_free(error);
        } else
            g_error("Unable to open display.");

        return EXIT_FAILURE;
    }

    if(G_UNLIKELY(opt_version)) {
        g_print("%s %s\n", G_LOG_DOMAIN, VERSION_STRING);
        g_print("Copyright (c) 2008-2011,2023 Brian Tarricone <brian@tarricone.org>\n");
        g_print("Copyright (c) 2010 Jérôme Guelfucci <jeromeg@xfce.org>\n");
        g_print("Copyright (c) 2016 Ali Abdallah <ali@xfce.org>\n");
        g_print("Copyright (c) 2016 Simon Steinbeiß <simon@xfce.org>\n");
        g_print(_("Released under the terms of the GNU General Public License, version 2\n"));
        g_print(_("Please report bugs to %s.\n"), PACKAGE_BUGREPORT);

        return EXIT_SUCCESS;
    }

    if(G_UNLIKELY(!xfconf_init(&error))) {
        xfce_message_dialog(NULL, _("Xfce Notify Daemon"),
                            "dialog-error",
                            _("Settings daemon is unavailable"),
                            error->message,
                            "application-exit", GTK_RESPONSE_ACCEPT,
                            NULL);
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    if (!notify_init ("Xfce4-notifyd settings")) {
      g_warning("Failed to initialize libnotify.");
    }

    xfce_notify_config_ui_register_resource();

    builder = gtk_builder_new_from_resource("/org/xfce/notifyd/settings/xfce4-notifyd-config.glade");
    if(G_UNLIKELY(!builder)) {
        g_error("Unable to read embedded UI definition file");
        return EXIT_FAILURE;
    }

    panel->channel = xfconf_channel_new("xfce4-notifyd");
    xfce_notify_migrate_settings(panel->channel);

    panel->log = xfce_notify_log_gbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                             0,
                                                             "org.xfce.Notifyd",
                                                             "/org/xfce/Notifyd",
                                                             NULL,
                                                             &error);
    if (panel->log == NULL) {
        g_warning("Failed to connect to notifiication log over DBus: %s", error != NULL ? error->message : "(unknown error)");
        g_clear_error(&error);
    } else {
        g_dbus_proxy_set_default_timeout(G_DBUS_PROXY(panel->log), 1500);
    }

    settings_dialog = xfce4_notifyd_config_setup_dialog(panel, builder);

    notifyd_running = GTK_WIDGET (gtk_builder_get_object (builder, "notifyd_running"));
    gtk_revealer_set_reveal_child (GTK_REVEALER (notifyd_running), FALSE);
    watch_handle_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                        "org.freedesktop.Notifications",
                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                        xfce_notify_bus_name_appeared_cb,
                                        xfce_notify_bus_name_vanished_cb,
                                        notifyd_running,
                                        NULL);

    if(opt_socket_id) {
        GtkWidget *plug, *plug_child;

        plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show(plug);
        g_signal_connect(G_OBJECT(plug), "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);

        plug_child = GTK_WIDGET(gtk_builder_get_object(builder, "plug-child"));

        /* In the glade file, plug_child has parent, so remove it first */
        gtk_container_remove (GTK_CONTAINER(gtk_widget_get_parent(plug_child)), plug_child);
        gtk_container_add (GTK_CONTAINER(plug), plug_child);

        gdk_notify_startup_complete();
        g_object_unref(G_OBJECT(builder));
        gtk_widget_destroy(settings_dialog);

        gtk_main();
    } else {
        g_object_unref(G_OBJECT(builder));

        gtk_dialog_run(GTK_DIALOG(settings_dialog));
    }

    g_bus_unwatch_name (watch_handle_id);
    if (panel->log != NULL) {
        g_object_unref(panel->log);
    }
    notify_uninit();
    xfconf_shutdown();
    g_free(panel);

    return EXIT_SUCCESS;
}
