/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <cairo-gobject.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "appearance-dialog_ui.h"

#define INCH_MM      25.4

/* Use a fallback DPI of 96 which should be ok-ish on most systems
 * and is only applied on rare occasions */
#define FALLBACK_DPI 96

/* Increase this number if new gtk settings have been added */
#define INITIALIZE_UINT (1)

enum
{
    COLUMN_THEME_PREVIEW,
    COLUMN_THEME_NAME,
    COLUMN_THEME_DISPLAY_NAME,
    COLUMN_THEME_COMMENT,
    COLUMN_THEME_WARNING,
    N_THEME_COLUMNS
};

enum
{
    COLUMN_RGBA_PIXBUF,
    COLUMN_RGBA_NAME,
    N_RGBA_COLUMNS
};

enum {
	COLOR_FG,
	COLOR_BG,
	COLOR_SELECTED_BG,
	NUM_SYMBOLIC_COLORS
};

static const gchar* xft_hint_styles_array[] =
{
    "hintnone", "hintslight", "hintmedium", "hintfull"
};

static const gchar* xft_rgba_array[] =
{
    "none", "rgb", "bgr", "vrgb", "vbgr"
};

static const GtkTargetEntry theme_drop_targets[] =
{
  { "text/uri-list", 0, 0 }
};

/* Option entries */
static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* Global xfconf channel */
static XfconfChannel *xsettings_channel;
static GSettings *desktop_interface_gsettings;

typedef struct
{
    GtkListStore *list_store;
    GtkTreeView *tree_view;
} preview_data;


static void install_theme (GtkWidget *widget, gchar **uris, GtkBuilder *builder);

static preview_data *
preview_data_new (GtkListStore *list_store,
                  GtkTreeView *tree_view)
{
    preview_data *pd;

    g_return_val_if_fail (list_store != NULL, NULL);
    g_return_val_if_fail (tree_view != NULL, NULL);
    g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

    pd = g_slice_new0 (preview_data);
    g_return_val_if_fail (pd != NULL, NULL);

    pd->list_store = list_store;
    pd->tree_view = tree_view;

    g_object_ref (G_OBJECT (pd->list_store));
    g_object_ref (G_OBJECT (pd->tree_view));

    return pd;
}

static void
preview_data_free (preview_data *pd)
{
    if (G_UNLIKELY (pd == NULL))
        return;
    g_object_unref (G_OBJECT (pd->list_store));
    g_object_unref (G_OBJECT (pd->tree_view));
    g_slice_free (preview_data, pd);
}

static int
compute_xsettings_dpi (GtkWidget *widget)
{
    GdkScreen *screen;
    int width_mm, height_mm;
    int width, height;
    int dpi;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    screen = gtk_widget_get_screen (widget);
    width_mm = gdk_screen_get_width_mm (screen);
    height_mm = gdk_screen_get_height_mm (screen);
    dpi = FALLBACK_DPI;

    if (width_mm > 0 && height_mm > 0)
    {
        width = gdk_screen_get_width (screen);
        height = gdk_screen_get_height (screen);
        dpi = MIN (INCH_MM * width  / width_mm,
                   INCH_MM * height / height_mm);
    }
G_GNUC_END_IGNORE_DEPRECATIONS

    return dpi;
}

static void
theme_selection_changed (GtkTreeSelection *selection,
                         const gchar      *property)
{
    GtkTreeModel *model;
    gboolean      has_selection;
    gboolean      has_xfwm4;
    gchar        *name;
    GtkTreeIter   iter;

    /* Get the selected list iter */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        has_xfwm4 = FALSE;

        /* Get the theme name and whether there is a xfwm4 theme as well */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &name, COLUMN_THEME_WARNING, &has_xfwm4, -1);

        /* Store the new theme */
        xfconf_channel_set_string (xsettings_channel, property, name);

        /* Set the matching xfwm4 theme if the selected theme is not an icon theme,
         * the xfconf setting is on, and a matching theme is available */
        if (xfconf_channel_get_bool (xsettings_channel, "/Xfce/SyncThemes", FALSE) == TRUE
            && strcmp (property, "/Net/ThemeName") == 0)
        {
            if (!has_xfwm4)
                xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", name);

            /* Use the default theme if Adwaita is selected */
            else if (strcmp (name, "Adwaita") == 0 || strcmp (name, "Adwaita-dark") == 0)
                xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", "Default");
        }

        /* Cleanup */
        g_free (name);
    }
}

static void
cb_icon_theme_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new icon theme */
    theme_selection_changed (selection, "/Net/IconThemeName");
}

static void
cb_ui_theme_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new UI theme */
    theme_selection_changed (selection, "/Net/ThemeName");
}

static void
cb_xfwm4_sync_switch_toggled (GObject *self, GParamSpec* pspec, gpointer user_data)
{
    /* Set the new XFWM4 theme */
    if (gtk_switch_get_active (GTK_SWITCH (self)))
        theme_selection_changed (GTK_TREE_SELECTION (user_data), "/Net/ThemeName");
    else
        xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", "Default");
}

static void
cb_window_scaling_factor_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo) + 1, 1, 2);

    /* Save setting */
    xfconf_channel_set_int (xsettings_channel, "/Gdk/WindowScalingFactor", active);
}

static void
cb_antialias_check_button_toggled (GtkToggleButton *toggle)
{
    gint active;

    /* Don't allow an inconsistent button anymore */
    gtk_toggle_button_set_inconsistent (toggle, FALSE);

    /* Get active */
    active = gtk_toggle_button_get_active (toggle) ? 1 : 0;

    /* Save setting */
    xfconf_channel_set_int (xsettings_channel, "/Xft/Antialias", active);
}

static void
cb_hinting_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_hint_styles_array)-1);

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Xft/HintStyle", xft_hint_styles_array[active]);

    /* Also update /Xft/Hinting to match */
    xfconf_channel_set_int (xsettings_channel, "/Xft/Hinting", active > 0 ? 1 : 0);
}

static void
cb_rgba_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_rgba_array)-1);

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Xft/RGBA", xft_rgba_array[active]);
}

static void
cb_custom_dpi_check_button_toggled (GtkToggleButton *custom_dpi_toggle,
                                    GtkSpinButton   *custom_dpi_spin)
{
    gint dpi;

    if (gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        /* Custom DPI is activated, so restore the last custom DPI we know about */
        dpi = xfconf_channel_get_int (xsettings_channel, "/Xfce/LastCustomDPI", -1);

        /* Unfortunately, we don't have a valid custom DPI value to use, so compute it */
        if (dpi <= 0)
            dpi = compute_xsettings_dpi (GTK_WIDGET (custom_dpi_toggle));

        /* Apply the computed custom DPI value */
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);

        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), TRUE);
    }
    else
    {
        /* Custom DPI is deactivated, so remember the current value as the last custom DPI */
        dpi = gtk_spin_button_get_value_as_int (custom_dpi_spin);
        xfconf_channel_set_int (xsettings_channel, "/Xfce/LastCustomDPI", dpi);

        /* Tell xfsettingsd to compute the value itself */
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", -1);

        /* Make the spin button insensitive */
        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), FALSE);
    }
}

static void
cb_custom_dpi_spin_button_changed (GtkSpinButton   *custom_dpi_spin,
                                   GtkToggleButton *custom_dpi_toggle)
{
    gint dpi = gtk_spin_button_get_value_as_int (custom_dpi_spin);

    if (gtk_widget_is_sensitive (GTK_WIDGET(custom_dpi_spin)) && gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        /* Custom DPI is turned on and the spin button has changed, so remember the value */
        xfconf_channel_set_int (xsettings_channel, "/Xfce/LastCustomDPI", dpi);
    }

    /* Tell xfsettingsd to apply the custom DPI value */
    xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);
}

#ifdef ENABLE_SOUND_SETTINGS
static void
cb_enable_event_sounds_check_button_toggled (GtkToggleButton *toggle, GtkWidget *button)
{
    gboolean active;

    active = gtk_toggle_button_get_active (toggle);
    gtk_widget_set_sensitive (button, active);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
}
#endif

static gboolean
appearance_settings_load_icon_themes (gpointer user_data)
{
    preview_data *pd = user_data;
    GtkListStore *list_store;
    GtkTreeView  *tree_view;
    GDir         *dir;
    GtkTreePath  *tree_path;
    GtkTreeIter   iter;
    XfceRc       *index_file;
    const gchar  *file;
    gchar       **icon_theme_dirs;
    gchar        *index_filename;
    const gchar  *theme_name;
    const gchar  *theme_comment;
    gchar        *name_escaped;
    gchar        *comment_escaped;
    gchar        *visible_name;
    gchar        *active_theme_name;
    gsize         i;
    gsize         p;
    GSList       *check_list = NULL;
    gchar        *cache_filename;
    gboolean      has_cache;
    gchar        *cache_tooltip;
    GtkIconTheme *icon_theme;
    cairo_surface_t *preview;
    cairo_t      *cr;
    GdkPixbuf    *icon;
    gchar*        preview_icons[4] = { "folder", "go-down", "audio-volume-high", "web-browser" };
    int           coords[4][2] = { { 4, 4 }, { 24, 4 }, { 4, 24 }, { 24, 24 } };
    gint          scale_factor;

    g_return_val_if_fail (pd != NULL, FALSE);

    list_store = pd->list_store;
    tree_view = pd->tree_view;
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (tree_view));

    /* Determine current theme */
    active_theme_name = xfconf_channel_get_string (xsettings_channel, "/Net/IconThemeName", "Rodent");

    /* Determine directories to look in for icon themes */
    xfce_resource_push_path (XFCE_RESOURCE_ICONS, DATADIR G_DIR_SEPARATOR_S "icons");
    icon_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_ICONS);
    xfce_resource_pop_path (XFCE_RESOURCE_ICONS);

    /* Iterate over all base directories */
    for (i = 0; icon_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (icon_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build filename for the index.theme of the current icon theme directory */
            index_filename = g_build_filename (icon_theme_dirs[i], file, "index.theme", NULL);

            /* Try to open the theme index file */
            index_file = xfce_rc_simple_open (index_filename, TRUE);

            if (index_file != NULL
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Set the icon theme group */
                xfce_rc_set_group (index_file, "Icon Theme");

                /* Check if the icon theme is valid and visible to the user */
                if (G_LIKELY (xfce_rc_has_entry (index_file, "Directories")
                              && !xfce_rc_read_bool_entry (index_file, "Hidden", FALSE)))
                {
                    /* Insert the theme in the check list */
                    check_list = g_slist_prepend (check_list, g_strdup (file));

                    /* Create the icon-theme preview */
                    preview = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 44 * scale_factor, 44 * scale_factor);
                    cairo_surface_set_device_scale (preview, scale_factor, scale_factor);
                    cr = cairo_create (preview);

                    icon_theme = gtk_icon_theme_new ();
                    gtk_icon_theme_set_custom_theme (icon_theme, file);

                    for (p = 0; p < 4; p++)
                    {
                        icon = NULL;
                        if (gtk_icon_theme_has_icon (icon_theme, preview_icons[p]))
                            icon = gtk_icon_theme_load_icon_for_scale (icon_theme, preview_icons[p], 16, scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
                        else if (gtk_icon_theme_has_icon (icon_theme, "image-missing"))
                            icon = gtk_icon_theme_load_icon_for_scale (icon_theme, "image-missing", 16, scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

                        if (icon)
                        {
                            cairo_save (cr);

                            cairo_translate (cr, coords[p][0], coords[p][1]);
                            cairo_scale (cr, 1.0 / scale_factor, 1.0 / scale_factor);

                            gdk_cairo_set_source_pixbuf (cr, icon, 0, 0);
                            cairo_paint (cr);

                            cairo_restore (cr);
                            g_object_unref (icon);
                        }
                    }

                    cairo_destroy (cr);

                    /* Get translated icon theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the theme's name and comment, since they are markup, not text */
                    name_escaped = g_markup_escape_text (theme_name, -1);
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;
                    visible_name = g_strdup_printf ("<b>%s</b>\n%s", name_escaped, comment_escaped);
                    g_free (name_escaped);
                    g_free (comment_escaped);

                    /* Cache filename */
                    cache_filename = g_build_filename (icon_theme_dirs[i], file, "icon-theme.cache", NULL);
                    has_cache = g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR);
                    g_free (cache_filename);

                    /* If the theme has no cache, mention this in the tooltip */
                    if (!has_cache)
                        cache_tooltip = g_strdup_printf (_("Warning: this icon theme has no cache file. You can create this by "
                                                           "running <i>gtk-update-icon-cache %s/%s/</i> in a terminal emulator."),
                                                         icon_theme_dirs[i], file);
                    else
                        cache_tooltip = NULL;

                    /* Append icon theme to the list store */
                    gtk_list_store_append (list_store, &iter);
                    gtk_list_store_set (list_store, &iter,
                                        COLUMN_THEME_PREVIEW, preview,
                                        COLUMN_THEME_NAME, file,
                                        COLUMN_THEME_DISPLAY_NAME, visible_name,
                                        COLUMN_THEME_WARNING, !has_cache,
                                        COLUMN_THEME_COMMENT, cache_tooltip,
                                        -1);

                    /* Check if this is the active theme, if so, select it */
                    if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
                    {
                        tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                        gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                        gtk_tree_view_scroll_to_cell (tree_view, tree_path, NULL, TRUE, 0.5, 0);
                        gtk_tree_path_free (tree_path);
                    }

                    g_object_unref (icon_theme);
                    cairo_surface_destroy (preview);
                }
            }

            /* Close theme index file */
            if (G_LIKELY (index_file))
                xfce_rc_close (index_file);

            /* Free theme index filename */
            g_free (index_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (icon_theme_dirs);

    /* Free the check list */
    if (G_LIKELY (check_list))
    {
        g_slist_foreach (check_list, (GFunc) (void (*)(void)) g_free, NULL);
        g_slist_free (check_list);
    }

    return FALSE;
}

static gboolean
appearance_settings_load_ui_themes (gpointer user_data)
{
    preview_data *pd = user_data;
    GtkListStore *list_store;
    GtkTreeView  *tree_view;
    GDir         *dir;
    GtkTreePath  *tree_path;
    GtkTreeIter   iter;
    XfceRc       *index_file;
    const gchar  *file;
    gchar       **ui_theme_dirs;
    gchar        *index_filename;
    const gchar  *theme_name;
    const gchar  *theme_comment;
    gchar        *active_theme_name;
    gchar        *gtkrc_filename;
    gchar        *gtkcss_filename;
    gchar        *xfwm4_filename;
    gchar        *notifyd_filename;
    gchar        *theme_name_markup;
    gchar        *comment_escaped;
    gint          i;
    GSList       *check_list = NULL;
    gboolean      has_gtk2;
    gboolean      has_xfwm4;
    gboolean      has_notifyd;

    list_store = pd->list_store;
    tree_view = pd->tree_view;

    /* Determine current theme */
    active_theme_name = xfconf_channel_get_string (xsettings_channel, "/Net/ThemeName", "Default");

    /* Determine directories to look in for ui themes */
    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    ui_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    /* Iterate over all base directories */
    for (i = 0; ui_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (ui_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build the filenames for theme components */
            gtkrc_filename = g_build_filename (ui_theme_dirs[i], file, "gtk-2.0", "gtkrc", NULL);
            gtkcss_filename = g_build_filename (ui_theme_dirs[i], file, "gtk-3.0", "gtk.css", NULL);
            xfwm4_filename = g_build_filename (ui_theme_dirs[i], file, "xfwm4", "themerc", NULL);
            notifyd_filename = g_build_filename (ui_theme_dirs[i], file, "xfce-notify-4.0", "gtk.css", NULL);

            /* Check if the gtk.css file exists and the theme is not already in the list */
            if (g_file_test (gtkcss_filename, G_FILE_TEST_EXISTS)
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Insert the theme in the check list */
                check_list = g_slist_prepend (check_list, g_strdup (file));

                /* Build filename for the index.theme of the current ui theme directory */
                index_filename = g_build_filename (ui_theme_dirs[i], file, "index.theme", NULL);

                /* Try to open the theme index file */
                index_file = xfce_rc_simple_open (index_filename, TRUE);

                if (G_LIKELY (index_file != NULL))
                {
                    /* Get translated ui theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the comment because tooltips are markup, not text */
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;
                }
                else
                {
                    /* Set defaults */
                    theme_name = file;
                    comment_escaped = NULL;
                }

                /* Check if the gtk2 gtkrc, xfwm4 themerc, etc. files exist */
                has_gtk2 = FALSE;
                has_xfwm4 = FALSE;
                has_notifyd = FALSE;

                if (g_file_test (gtkrc_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
                    has_gtk2 = TRUE;
                if (g_file_test (xfwm4_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
                    has_xfwm4 = TRUE;
                if (g_file_test (notifyd_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
                    has_notifyd = TRUE;

                /* Compose the final markup text */
                theme_name_markup = g_strdup_printf ("<b>%s</b>\nGtk3", theme_name);

                if (has_gtk2)
                    theme_name_markup = g_strconcat (theme_name_markup, ", Gtk2", NULL);
                if (has_xfwm4)
                    theme_name_markup = g_strconcat (theme_name_markup, ", Xfwm4", NULL);
                if (has_notifyd)
                    theme_name_markup = g_strconcat (theme_name_markup, ", Xfce4-notifyd", NULL);

                /* Append ui theme to the list store */
                gtk_list_store_append (list_store, &iter);
                gtk_list_store_set (list_store, &iter,
                                    COLUMN_THEME_NAME, file,
                                    COLUMN_THEME_DISPLAY_NAME, theme_name_markup,
                                    COLUMN_THEME_WARNING, !has_xfwm4,
                                    COLUMN_THEME_COMMENT, comment_escaped, -1);

                /* Cleanup */
                if (G_LIKELY (index_file != NULL))
                    xfce_rc_close (index_file);
                g_free (comment_escaped);
                g_free (theme_name_markup);

                /* Check if this is the active theme, if so, select it */
                if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
                {
                    tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                    gtk_tree_view_scroll_to_cell (tree_view, tree_path, NULL, TRUE, 0.5, 0);
                    gtk_tree_path_free (tree_path);
                }

                /* Free theme index filename */
                g_free (index_filename);
            }

            /* Free filenames */
            g_free (gtkrc_filename);
            g_free (gtkcss_filename);
            g_free (xfwm4_filename);
            g_free (notifyd_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (ui_theme_dirs);

    /* Free the check list */
    if (G_LIKELY (check_list))
    {
        g_slist_foreach (check_list, (GFunc) (void (*)(void)) g_free, NULL);
        g_slist_free (check_list);
    }

    return FALSE;
}

static void
appearance_settings_dialog_channel_property_changed (XfconfChannel *channel,
                                                     const gchar   *property_name,
                                                     const GValue  *value,
                                                     GtkBuilder    *builder)
{
    GObject      *object;
    gchar        *str;
    guint         i;
    gint          antialias, dpi, custom_dpi;
    GtkTreeModel *model;

    g_return_if_fail (property_name != NULL);
    g_return_if_fail (GTK_IS_BUILDER (builder));

    if (strcmp (property_name, "/Xft/RGBA") == 0)
    {
        str = xfconf_channel_get_string (xsettings_channel, property_name, xft_rgba_array[0]);
        for (i = 0; i < G_N_ELEMENTS (xft_rgba_array); i++)
        {
            if (strcmp (str, xft_rgba_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "xft_rgba_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Gdk/WindowScalingFactor") == 0)
    {
        i = xfconf_channel_get_int (xsettings_channel, property_name, 1);
        object = gtk_builder_get_object (builder, "gdk_window_scaling_factor_combo_box");
        gtk_combo_box_set_active (GTK_COMBO_BOX (object), i - 1);
    }
    else if (strcmp (property_name, "/Xft/HintStyle") == 0)
    {
        str = xfconf_channel_get_string (xsettings_channel, property_name, xft_hint_styles_array[0]);
        for (i = 0; i < G_N_ELEMENTS (xft_hint_styles_array); i++)
        {
            if (strcmp (str, xft_hint_styles_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "xft_hinting_style_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Xft/Antialias") == 0)
    {
        object = gtk_builder_get_object (builder, "xft_antialias_check_button");
        antialias = xfconf_channel_get_int (xsettings_channel, property_name, -1);
        switch (antialias)
        {
            case 1:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), TRUE);
                break;

            case 0:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), FALSE);
                break;

            default: /* -1 */
                gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (object), TRUE);
                break;
        }
    }
    else if (strcmp (property_name, "/Xft/DPI") == 0)
    {
        /* The DPI has changed, so get its value and the last known custom value */
        dpi = xfconf_channel_get_int (xsettings_channel, property_name, FALLBACK_DPI);
        custom_dpi = xfconf_channel_get_int (xsettings_channel, "/Xfce/LastCustomDPI", -1);

        /* Activate the check button if we're using a custom DPI */
        object = gtk_builder_get_object (builder, "xft_custom_dpi_check_button");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), dpi >= 0);

        /* If we're not using a custom DPI, compute the future custom DPI automatically */
        if (custom_dpi == -1)
            custom_dpi = compute_xsettings_dpi (GTK_WIDGET (object));

        object = gtk_builder_get_object (builder, "xft_custom_dpi_spin_button");

        if (dpi > 0)
        {
            /* We're using a custom DPI, so use the current DPI setting for the spin value */
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (object), dpi);
        }
        else
        {
            /* Set the spin button value to the last custom DPI */
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (object), custom_dpi);
        }
    }
    else if (strcmp (property_name, "/Net/ThemeName") == 0)
    {
        GtkTreeIter iter;

        object = gtk_builder_get_object (builder, "gtk_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)),
                                             &model,
                                             &iter))
        {
            gchar *selected_name;
            gchar *new_name;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &selected_name, -1);

            new_name = xfconf_channel_get_string (channel, property_name, NULL);

            g_free (selected_name);
            g_free (new_name);
        }

        /* Keep gsettings in sync */
        if (desktop_interface_gsettings != NULL)
        {
            GSettingsSchema *schema;

            str = xfconf_channel_get_string (channel, property_name, NULL);
            g_settings_set_string (desktop_interface_gsettings, "gtk-theme", str);

            /* Also set the preferred color scheme (needed for GTK4) */
            g_object_get (desktop_interface_gsettings, "settings-schema", &schema, NULL);
            if (g_settings_schema_has_key (schema, "color-scheme"))
              {
                if (str != NULL)
                {
                    gchar *strdown = g_ascii_strdown (str, -1);
                    if (g_strstr_len (strdown, -1, "-dark-") || g_str_has_suffix (strdown, "-dark"))
                        g_settings_set_string (desktop_interface_gsettings, "color-scheme", "prefer-dark");
                    else
                        g_settings_reset (desktop_interface_gsettings, "color-scheme");
                    g_free (strdown);
                }
                else
                    g_settings_reset (desktop_interface_gsettings, "color-scheme");
              }

            g_free (str);
            g_settings_schema_unref (schema);
        }
    }
    else if (strcmp (property_name, "/Net/IconThemeName") == 0)
    {
        GtkTreeIter iter;
        gboolean    reload;

        reload = TRUE;

        object = gtk_builder_get_object (builder, "icon_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)),
                                             &model,
                                             &iter))
        {
            gchar *selected_name;
            gchar *new_name;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &selected_name, -1);

            new_name = xfconf_channel_get_string (channel, property_name, NULL);

            reload = (strcmp (new_name, selected_name) != 0);

            g_free (selected_name);
            g_free (new_name);
        }


        if (reload)
        {
            preview_data *pd;

            gtk_list_store_clear (GTK_LIST_STORE (model));
            pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
            if (pd)
                g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                 appearance_settings_load_icon_themes,
                                 pd,
                                 (GDestroyNotify) preview_data_free);
        }

        /* Keep gsettings in sync */
        if (desktop_interface_gsettings != NULL)
        {
            str = xfconf_channel_get_string (channel, property_name, NULL);
            g_settings_set_string (desktop_interface_gsettings, "icon-theme", str);
            g_free (str);
        }
    }
    else if (strcmp (property_name, "/Gtk/FontName") == 0)
    {
        /* Keep gsettings in sync */
        if (desktop_interface_gsettings != NULL)
        {
            str = xfconf_channel_get_string (channel, property_name, NULL);
            g_settings_set_string (desktop_interface_gsettings, "font-name", str);
            g_free (str);
        }
    }
    else if (strcmp (property_name, "/Gtk/MonospaceFontName") == 0)
    {
        /* Keep gsettings in sync */
        if (desktop_interface_gsettings != NULL)
        {
            str = xfconf_channel_get_string (channel, property_name, NULL);
            g_settings_set_string (desktop_interface_gsettings, "monospace-font-name", str);
            g_free (str);
        }
    }
}

static void
cb_theme_uri_dropped (GtkWidget        *widget,
                      GdkDragContext   *drag_context,
                      gint              x,
                      gint              y,
                      GtkSelectionData *data,
                      guint             info,
                      guint             timestamp,
                      GtkBuilder       *builder)
{
    gchar        **uris;

    uris = gtk_selection_data_get_uris (data);

    if (uris)
        install_theme (widget, uris, builder);
    else
        return;
}

static void
install_theme (GtkWidget *widget, gchar **uris, GtkBuilder *builder)
{
    gchar         *argv[3];
    guint          i;
    GError        *error = NULL;
    gint           status;
    GtkWidget     *toplevel = gtk_widget_get_toplevel (widget);
    gchar         *filename;
    GdkCursor     *cursor;
    GdkWindow     *gdkwindow;
    gboolean       something_installed = FALSE;
    GObject       *object;
    GtkTreeModel  *model;
    preview_data  *pd;

    argv[0] = HELPERDIR G_DIR_SEPARATOR_S "appearance-install-theme";
    argv[2] = NULL;

    /* inform the user we are installing the theme */
    gdkwindow = gtk_widget_get_window (widget);
    cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_WATCH);
    gdk_window_set_cursor (gdkwindow, cursor);

    /* iterate main loop to show cursor */
    while (gtk_events_pending ())
        gtk_main_iteration ();

    for (i = 0; uris[i] != NULL; i++)
    {
        filename = g_filename_from_uri (uris[i], NULL, NULL);
        if (filename == NULL)
            continue;

        argv[1] = filename;

        if (g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, &status, &error)
            && status > 0)
        {
            switch (WEXITSTATUS (status))
            {
                case 2:
                    g_set_error (&error, G_SPAWN_ERROR, 0,
                        _("File is too large, installation aborted"));
                    break;

                case 3:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Failed to create temporary directory"));
                    break;

                case 4:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Failed to extract archive"));
                    break;

                case 5:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Unknown format, only archives and directories are supported"));
                    break;

                case 6:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Not a valid theme package"));
                    break;

                default:
                    g_set_error (&error, G_SPAWN_ERROR,
                        0, _("An unknown error, exit code is %d"), WEXITSTATUS (status));
                    break;
            }
        }

        if (error != NULL)
        {
            xfce_dialog_show_error (GTK_WINDOW (toplevel), error, _("Failed to install theme"));
            g_clear_error (&error);
        }
        else
        {
            something_installed = TRUE;
        }

        g_free (filename);
    }

    g_strfreev (uris);
    gdk_window_set_cursor (gdkwindow, NULL);
    g_object_unref (cursor);

    if (something_installed)
    {
        /* reload icon theme treeview in an idle loop */
        object = gtk_builder_get_object (builder, "icon_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
        if (pd)
            g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                             appearance_settings_load_icon_themes,
                             pd,
                             (GDestroyNotify) preview_data_free);

        /* reload gtk theme treeview */
        object = gtk_builder_get_object (builder, "gtk_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
        if (pd)
            g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                             appearance_settings_load_ui_themes,
                             pd,
                             (GDestroyNotify) preview_data_free);
    }
}

static void
appearance_settings_install_theme_cb (GtkButton *widget, GtkBuilder *builder)
{
    GtkWidget *window;
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkFileFilter *filter;
    gint res;
    gchar *theme;
    gchar *title;

    window = gtk_widget_get_toplevel (GTK_WIDGET (widget));
    g_object_get (G_OBJECT (widget), "name", &theme, NULL);
    title = g_strdup_printf (_("Install %s theme"), theme);
    dialog = gtk_file_chooser_dialog_new (title,
                                          GTK_WINDOW (window),
                                          action,
                                          _("_Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("_Open"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*.tar*");
    gtk_file_filter_add_pattern (filter, "*.zip");
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);

    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        gchar *filename;
        gchar **uris;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

        uris = g_new0 (gchar *, 2);
        filename = gtk_file_chooser_get_filename (chooser);
        uris[0] = g_filename_to_uri (filename, NULL, NULL);
        install_theme (window, uris, builder);
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
    g_free (title);
    g_free (theme);
}

static cairo_surface_t *
appearance_settings_draw_subpixel_icon (gboolean is_rgb,
                                        gboolean is_vertical,
                                        gint     size,
                                        gint     scale_factor)
{
    cairo_surface_t *surface;
    cairo_t         *cr;
    gint             color_width;
    double           colors[3][3] = { { 0.0, }, };

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size * scale_factor, size * scale_factor);
    cairo_surface_set_device_scale (surface, scale_factor, scale_factor);

    cr = cairo_create (surface);
    cairo_scale (cr, 1.0 / scale_factor, 1.0 / scale_factor);

    if (is_rgb)
    {
        colors[0][0] = 1.0;  // red
        colors[1][1] = 1.0;  // green
        colors[2][2] = 1.0;  // blue
    }
    else
    {
        colors[0][2] = 1.0;  // blue
        colors[1][1] = 1.0;  // green
        colors[2][0] = 1.0;  // red
    }

    color_width = (size * scale_factor) / 3;

    for (gsize i = 0; i < G_N_ELEMENTS (colors); ++i)
    {
        cairo_save (cr);
        cairo_set_source_rgb (cr, colors[i][0], colors[i][1], colors[i][2]);
        cairo_rectangle (cr,
                         is_vertical ? 0 : color_width * i,
                         is_vertical ? color_width * i : 0,
                         is_vertical ? size * scale_factor : color_width,
                         is_vertical ? color_width : size * scale_factor);
        cairo_fill (cr);
        cairo_restore (cr);
    }

    cairo_destroy (cr);

    return surface;
}

static void
appearance_settings_dialog_configure_widgets (GtkBuilder *builder)
{
    GObject           *object, *object2;
    GtkListStore      *list_store;
    GtkCellRenderer   *renderer;
    cairo_surface_t   *surface;
    GtkTreeSelection  *selection;
    GtkTreeViewColumn *column;
    preview_data      *pd;
    gchar             *path;
    gint               menu_icon_size;
    gint               scale_factor;

    /* Icon themes list */
    object = gtk_builder_get_object (builder, "install_icon_theme");
    g_object_set (object, "name", "icon", NULL);
    g_signal_connect (G_OBJECT (object), "clicked", G_CALLBACK (appearance_settings_install_theme_cb), builder);

    object = gtk_builder_get_object (builder, "icon_theme_treeview");
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (object));

    list_store = gtk_list_store_new (N_THEME_COLUMNS, CAIRO_GOBJECT_TYPE_SURFACE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (list_store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_THEME_COMMENT);

    /* Single-column layout */
    column = gtk_tree_view_column_new ();

    /* Icon Previews */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "surface", COLUMN_THEME_PREVIEW, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (object), column);

    /* Theme Name and Description */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "markup", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Warning Icon */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "visible", COLUMN_THEME_WARNING, NULL);
    g_object_set (G_OBJECT (renderer), "icon-name", "dialog-warning", NULL);

    pd = preview_data_new (GTK_LIST_STORE (list_store), GTK_TREE_VIEW (object));
    if (pd)
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         appearance_settings_load_icon_themes,
                         pd,
                         (GDestroyNotify) preview_data_free);

    g_object_unref (G_OBJECT (list_store));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (object), TRUE);
    g_signal_connect_swapped (G_OBJECT (object), "row-activated", G_CALLBACK (cb_icon_theme_selection_changed), selection);

    gtk_drag_dest_set (GTK_WIDGET (object), GTK_DEST_DEFAULT_ALL,
                       theme_drop_targets, G_N_ELEMENTS (theme_drop_targets),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (object), "drag-data-received", G_CALLBACK (cb_theme_uri_dropped), builder);

    /* Gtk (UI) themes */
    object = gtk_builder_get_object (builder, "gtk_theme_treeview");

    list_store = gtk_list_store_new (N_THEME_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (list_store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_THEME_COMMENT);

    /* Single-column layout */
    column = gtk_tree_view_column_new ();

    /* Icon Previews */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", COLUMN_THEME_PREVIEW, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (object), column);

    /* Theme Name and Description */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "markup", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    pd = preview_data_new (list_store, GTK_TREE_VIEW (object));
    if (pd)
        g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                         appearance_settings_load_ui_themes,
                         pd,
                         (GDestroyNotify) preview_data_free);

    g_object_unref (G_OBJECT (list_store));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (object), TRUE);
    g_signal_connect_swapped (G_OBJECT (object), "row-activated", G_CALLBACK (cb_ui_theme_selection_changed), selection);

    gtk_drag_dest_set (GTK_WIDGET (object), GTK_DEST_DEFAULT_ALL,
                       theme_drop_targets, G_N_ELEMENTS (theme_drop_targets),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (object), "drag-data-received", G_CALLBACK (cb_theme_uri_dropped), builder);
    object = gtk_builder_get_object (builder, "install_gtk_theme");
    g_object_set (object, "name", "Gtk", NULL);
    g_signal_connect (G_OBJECT (object), "clicked", G_CALLBACK (appearance_settings_install_theme_cb), builder);

    /* Switch for xfwm4 theme matching, gets hidden if xfwm4 is not installed */
    path = g_find_program_in_path ("xfwm4");
    if (path != NULL)
    {
        object = gtk_builder_get_object (builder, "xfwm4_sync_switch");
        xfconf_g_property_bind (xsettings_channel, "/Xfce/SyncThemes", G_TYPE_BOOLEAN, G_OBJECT (object), "active");
        g_signal_connect (G_OBJECT (object), "notify::active", G_CALLBACK (cb_xfwm4_sync_switch_toggled), selection);
    }
    else
    {
        object = gtk_builder_get_object (builder, "xfwm4_sync");
        gtk_widget_hide (GTK_WIDGET (object));
    }
    g_free (path);

    /* Subpixel (rgba) hinting Combo */
    object = gtk_builder_get_object (builder, "xft_rgba_store");

    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &menu_icon_size, &menu_icon_size);

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, menu_icon_size * scale_factor, menu_icon_size * scale_factor);
    cairo_surface_set_device_scale (surface, scale_factor, scale_factor);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 0, 0, surface, 1, _("None"), -1);
    cairo_surface_destroy (surface);

    surface = appearance_settings_draw_subpixel_icon (TRUE, FALSE, menu_icon_size, scale_factor);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 1, 0, surface, 1, _("RGB"), -1);
    cairo_surface_destroy (surface);

    surface = appearance_settings_draw_subpixel_icon (FALSE, FALSE, menu_icon_size, scale_factor);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 2, 0, surface, 1, _("BGR"), -1);
    cairo_surface_destroy (surface);

    surface = appearance_settings_draw_subpixel_icon (TRUE, TRUE, menu_icon_size, scale_factor);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 3, 0, surface, 1, _("Vertical RGB"), -1);
    cairo_surface_destroy (surface);

    surface = appearance_settings_draw_subpixel_icon (FALSE, TRUE, menu_icon_size, scale_factor);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 4, 0, surface, 1, _("Vertical BGR"), -1);
    cairo_surface_destroy (surface);

    object = gtk_builder_get_object (builder, "xft_rgba_combo_box");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/RGBA", NULL, builder);
    g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_rgba_style_combo_changed), NULL);

    /* Enable buttons in native GTK dialog headers */
    object = gtk_builder_get_object (builder, "gtk_dialog_button_header_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/DialogsUseHeader", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Enable editable menu accelerators */
    object = gtk_builder_get_object (builder, "gtk_caneditaccels_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/CanChangeAccels", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Show menu images */
    object = gtk_builder_get_object (builder, "gtk_menu_images_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/MenuImages", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Show button images */
    object = gtk_builder_get_object (builder, "gtk_button_images_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/ButtonImages", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Font name */
    object = gtk_builder_get_object (builder, "gtk_fontname_button");
    xfconf_g_property_bind (xsettings_channel,  "/Gtk/FontName", G_TYPE_STRING,
                            G_OBJECT (object), "font-name");

    /* Monospace font name */
    object = gtk_builder_get_object (builder, "gtk_monospace_fontname_button");
    xfconf_g_property_bind (xsettings_channel,  "/Gtk/MonospaceFontName", G_TYPE_STRING,
                            G_OBJECT (object), "font-name");

    /* Hinting style */
    object = gtk_builder_get_object (builder, "xft_hinting_style_combo_box");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/HintStyle", NULL, builder);
    g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_hinting_style_combo_changed), NULL);

    /* Hinting */
    object = gtk_builder_get_object (builder, "xft_antialias_check_button");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/Antialias", NULL, builder);
    g_signal_connect (G_OBJECT (object), "toggled", G_CALLBACK (cb_antialias_check_button_toggled), NULL);

    /* DPI */
    object = gtk_builder_get_object (builder, "xft_custom_dpi_check_button");
    object2 = gtk_builder_get_object (builder, "xft_custom_dpi_spin_button");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/DPI", NULL, builder);
    gtk_widget_set_sensitive (GTK_WIDGET (object2), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
    g_signal_connect (G_OBJECT (object), "toggled", G_CALLBACK (cb_custom_dpi_check_button_toggled), object2);
    g_signal_connect (G_OBJECT (object2), "value-changed", G_CALLBACK (cb_custom_dpi_spin_button_changed), object);

    /* Window scaling factor */
    object = gtk_builder_get_object (builder, "gdk_window_scaling_factor_combo_box");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Gdk/WindowScalingFactor", NULL, builder);
    g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK(cb_window_scaling_factor_combo_changed), NULL);

#ifdef ENABLE_SOUND_SETTINGS
    /* Sounds */
    object = gtk_builder_get_object (builder, "event_sounds_frame");
    gtk_widget_show (GTK_WIDGET (object));

    object = gtk_builder_get_object (builder, "enable_event_sounds_check_button");
    object2  = gtk_builder_get_object (builder, "enable_input_feedback_sounds_button");

    g_signal_connect (G_OBJECT (object), "toggled",
                      G_CALLBACK (cb_enable_event_sounds_check_button_toggled), object2);

    xfconf_g_property_bind (xsettings_channel, "/Net/EnableEventSounds", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");
    xfconf_g_property_bind (xsettings_channel, "/Net/EnableInputFeedbackSounds", G_TYPE_BOOLEAN,
                            G_OBJECT (object2), "active");

    gtk_widget_set_sensitive (GTK_WIDGET (object2), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
#endif
}

static void
appearance_settings_dialog_response (GtkWidget *dialog,
                                     gint       response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "appearance",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else
        gtk_main_quit ();
}

gint
main (gint argc, gchar **argv)
{
    GObject    *dialog, *plug_child;
    GtkWidget  *plug;
    GtkBuilder *builder;
    GError     *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-2023");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    if (G_LIKELY (xsettings_channel))
    {
        GSettingsSchemaSource *source;

        /* hook to make sure the libxfce4ui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
            return EXIT_FAILURE;

        /* to synchronize some properties with GSettings */
        source = g_settings_schema_source_get_default ();
        if (source != NULL)
          {
            GSettingsSchema *schema = g_settings_schema_source_lookup (source, "org.gnome.desktop.interface", TRUE);
            if (schema != NULL)
              {
                desktop_interface_gsettings = g_settings_new ("org.gnome.desktop.interface");
                g_settings_schema_unref (schema);
              }
          }

        /* load the gtk user interface file*/
        builder = gtk_builder_new ();
        if (gtk_builder_add_from_string (builder, appearance_dialog_ui,
                                         appearance_dialog_ui_length, &error) != 0)
          {
            /* connect signal to monitor the channel */
            g_signal_connect (G_OBJECT (xsettings_channel), "property-changed",
                G_CALLBACK (appearance_settings_dialog_channel_property_changed), builder);

            appearance_settings_dialog_configure_widgets (builder);

            if (G_UNLIKELY (opt_socket_id == 0))
            {
                /* build the dialog */
                dialog = gtk_builder_get_object (builder, "dialog");

                g_signal_connect (dialog, "response",
                    G_CALLBACK (appearance_settings_dialog_response), NULL);
                gtk_window_present (GTK_WINDOW (dialog));

                /* To prevent the settings dialog to be saved in the session */
                gdk_x11_set_sm_client_id ("FAKE ID");

                gtk_main ();
            }
            else
            {
                /* Create plug widget */
                plug = gtk_plug_new (opt_socket_id);
                g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
                gtk_widget_show (plug);

                /* Stop startup notification */
                gdk_notify_startup_complete ();

                /* Get plug child widget */
                plug_child = gtk_builder_get_object (builder, "plug-child");
                xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
                gtk_widget_show (GTK_WIDGET (plug_child));

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

        /* release the channel */
        g_object_unref (G_OBJECT (xsettings_channel));
        if (desktop_interface_gsettings != NULL)
          g_object_unref (desktop_interface_gsettings);
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
