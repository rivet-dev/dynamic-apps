/*
 *  xfdesktop
 *
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
 *  Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libwnck/libwnck.h>
#include <exo/exo.h>

#include "xfdesktop-common.h"
#include "xfdesktop-thumbnailer.h"
#include "xfdesktop-settings-ui.h"
#include "xfdesktop-settings-appearance-frame-ui.h"
/* for XfceBackdropImageStyle && XfceBackdropColorStyle */
#include "xfce-backdrop.h"

#define MAX_ASPECT_RATIO 1.5f
#define PREVIEW_HEIGHT   96
#define PREVIEW_WIDTH    (PREVIEW_HEIGHT * MAX_ASPECT_RATIO)


#define SETTINGS_WINDOW_LAST_WIDTH           "/last/window-width"
#define SETTINGS_WINDOW_LAST_HEIGHT          "/last/window-height"

#define SHOW_DESKTOP_MENU_PROP               "/desktop-menu/show"
#define DESKTOP_MENU_SHOW_ICONS_PROP         "/desktop-menu/show-icons"

#define WINLIST_SHOW_WINDOWS_MENU_PROP       "/windowlist-menu/show"
#define WINLIST_SHOW_APP_ICONS_PROP          "/windowlist-menu/show-icons"
#define WINLIST_SHOW_STICKY_WIN_ONCE_PROP    "/windowlist-menu/show-sticky-once"
#define WINLIST_SHOW_WS_NAMES_PROP           "/windowlist-menu/show-workspace-names"
#define WINLIST_SHOW_WS_SUBMENUS_PROP        "/windowlist-menu/show-submenus"
#define WINLIST_SHOW_ADD_REMOVE_WORKSPACES_PROP "/windowlist-menu/show-add-remove-workspaces"

#define DESKTOP_ICONS_ON_PRIMARY_PROP        "/desktop-icons/primary"
#define DESKTOP_ICONS_STYLE_PROP             "/desktop-icons/style"
#define DESKTOP_ICONS_ICON_SIZE_PROP         "/desktop-icons/icon-size"
#define DESKTOP_ICONS_FONT_SIZE_PROP         "/desktop-icons/font-size"
#define DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP  "/desktop-icons/use-custom-font-size"
#define DESKTOP_ICONS_SHOW_TOOLTIP_PROP      "/desktop-icons/show-tooltips"
#define DESKTOP_ICONS_TOOLTIP_SIZE_PROP      "/desktop-icons/tooltip-size"
#define DESKTOP_ICONS_SINGLE_CLICK_PROP      "/desktop-icons/single-click"
#define DESKTOP_ICONS_GRAVITY_PROP           "/desktop-icons/gravity"

typedef struct
{
    GtkTreeModel *model;
    GtkTreeIter *iter;
    GdkPixbuf *pix;
    gint scale_factor;
} PreviewData;

typedef struct
{
    XfconfChannel *channel;
    gint screen;
    gint monitor;
    gint workspace;
    gchar *monitor_name;
    gulong image_list_loaded:1;

    WnckWindow *wnck_window;
    /* We keep track of the current workspace number because
     * wnck_screen_get_active_workspace sometimes has to return NULL. */
    gint active_workspace;

    GtkWidget *infobar;
    GtkWidget *infobar_label;
    GtkWidget *label_header;
    GtkWidget *image_iconview;
    GtkWidget *btn_folder;
    GtkWidget *btn_folder_apply;
    GtkWidget *chk_apply_to_all;
    GtkWidget *image_style_combo;
    GtkWidget *color_style_combo;
    GtkWidget *color1_btn;
    GtkWidget *color2_btn;

    gulong color1_btn_id;
    gulong color2_btn_id;

    /* backdrop cycling options */
    GtkWidget *backdrop_cycle_chkbox;
    GtkWidget *combo_backdrop_cycle_period;
    GtkWidget *backdrop_cycle_spinbox;
    GtkWidget *random_backdrop_order_chkbox;

    guint preview_id;
    GAsyncQueue *preview_queue;

    XfdesktopThumbnailer *thumbnailer;

    GFile *selected_folder;
    GCancellable *cancel_enumeration;
    guint add_dir_idle_id;

} AppearancePanel;

typedef struct
{
    GFileEnumerator *file_enumerator;
    GtkListStore *ls;
    GtkTreeIter *selected_iter;
    gchar *last_image;
    gchar *file_path;
    AppearancePanel *panel;
} AddDirData;

enum
{
    COL_PIX = 0,
    COL_SURFACE,
    COL_NAME,
    COL_FILENAME,
    COL_THUMBNAIL,
    COL_COLLATE_KEY,
    N_COLS,
};

enum
{
    COL_ICON_PIX = 0,
    COL_ICON_NAME,
    COL_ICON_ENABLED,
    COL_ICON_PROPERTY,
    N_ICON_COLS,
};

static void cb_xfdesktop_chk_apply_to_all(GtkCheckButton *button,
                                          gpointer user_data);
static gchar *xfdesktop_settings_generate_per_workspace_binding_string(AppearancePanel *panel,
                                                                       const gchar* property);
static gchar *xfdesktop_settings_get_backdrop_image(AppearancePanel *panel);



static const gchar *
system_data_lookup (void)
{
    const gchar * const * dirs;
    gchar *path = NULL;
    guint i;

    dirs = g_get_system_data_dirs ();
    for (i = 0; path == NULL && dirs[i] != NULL; ++i) {
        path = g_build_path (G_DIR_SEPARATOR_S, dirs[i], "backgrounds", NULL);
        if (g_path_is_absolute (path) && g_file_test (path, G_FILE_TEST_IS_DIR))
            return path;

        g_free (path);
        path = NULL;
    }

    return path;
}

static void
xfdesktop_settings_free_pdata(gpointer data)
{
    PreviewData *pdata = data;

    g_object_unref(pdata->model);

    gtk_tree_iter_free(pdata->iter);

    if(pdata->pix)
        g_object_unref(pdata->pix);

    g_free(pdata);
}

static void
xfdesktop_settings_do_single_preview(PreviewData *pdata)
{
    GtkTreeModel *model;
    GtkTreeIter *iter;
    gchar *filename = NULL, *thumbnail = NULL;
    GdkPixbuf *pix;

    g_return_if_fail(pdata);

    model = pdata->model;
    iter = pdata->iter;

    gtk_tree_model_get(model, iter,
                       COL_FILENAME, &filename,
                       COL_THUMBNAIL, &thumbnail,
                       -1);

    /* If we didn't create a thumbnail there might not be a thumbnailer service
     * or it may not support that format */
    if(thumbnail == NULL) {
        XF_DEBUG("generating thumbnail for filename %s", filename);
        pix = gdk_pixbuf_new_from_file_at_scale(filename,
                                                PREVIEW_WIDTH * pdata->scale_factor, PREVIEW_HEIGHT * pdata->scale_factor,
                                                TRUE, NULL);
    } else {
        XF_DEBUG("loading thumbnail %s", thumbnail);
        pix = gdk_pixbuf_new_from_file_at_scale(thumbnail,
                                                PREVIEW_WIDTH * pdata->scale_factor, PREVIEW_HEIGHT * pdata->scale_factor,
                                                TRUE, NULL);
        g_free(thumbnail);
    }

    g_free(filename);

    if(pix) {
        cairo_surface_t *surface;

        pdata->pix = pix;

        /* set the image */
        surface = gdk_cairo_surface_create_from_pixbuf(pix, pdata->scale_factor, NULL);
        gtk_list_store_set(GTK_LIST_STORE(pdata->model), pdata->iter,
                           COL_PIX, pdata->pix,
                           COL_SURFACE, surface,
                           -1);
        cairo_surface_destroy(surface);

    }
    xfdesktop_settings_free_pdata(pdata);
}

static gboolean
xfdesktop_settings_create_previews(gpointer data)
{
    AppearancePanel *panel = data;

    if(panel->preview_queue != NULL) {
        PreviewData *pdata = NULL;

        /* try to pull another preview */
        pdata = g_async_queue_try_pop(panel->preview_queue);

        if(pdata != NULL) {
            xfdesktop_settings_do_single_preview(pdata);
            /* Continue on the next idle time */
            return TRUE;
        } else {
            /* Nothing left, remove the queue, we're done with it */
            g_async_queue_unref(panel->preview_queue);
            panel->preview_queue = NULL;
        }
    }

    /* clear the idle source */
    panel->preview_id = 0;

    /* stop this idle source */
    return FALSE;
}

static void
xfdesktop_settings_add_file_to_queue(AppearancePanel *panel, PreviewData *pdata)
{
    TRACE("entering");

    g_return_if_fail(panel != NULL);
    g_return_if_fail(pdata != NULL);

    /* Create the queue if it doesn't exist */
    if(panel->preview_queue == NULL) {
        XF_DEBUG("creating preview queue");
        panel->preview_queue = g_async_queue_new_full(xfdesktop_settings_free_pdata);
    }

    g_async_queue_push(panel->preview_queue, pdata);

    /* Create the previews in an idle callback */
    if(panel->preview_id == 0) {
        panel->preview_id = g_idle_add(xfdesktop_settings_create_previews, panel);
    }
}

static void
cb_thumbnail_ready(XfdesktopThumbnailer *thumbnailer,
                   gchar *src_file, gchar *thumb_file,
                   gpointer user_data)
{
    AppearancePanel *panel = user_data;
    GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(panel->image_iconview));
    GtkTreeIter iter;
    PreviewData *pdata = NULL;

    if(gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gchar *filename = NULL;
            gtk_tree_model_get(model, &iter, COL_FILENAME, &filename, -1);

            /* We're looking for the src_file */
            if(g_strcmp0(filename, src_file) == 0) {
                /* Add the thumb_file to it */
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COL_THUMBNAIL, thumb_file, -1);

                pdata = g_new0(PreviewData, 1);
                pdata->model = GTK_TREE_MODEL(g_object_ref(G_OBJECT(model)));
                pdata->iter = gtk_tree_iter_copy(&iter);
                pdata->pix = NULL;
                pdata->scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(panel->image_iconview));

                /* Create the preview image */
                xfdesktop_settings_add_file_to_queue(panel, pdata);

                g_free(filename);
                return;
            }

            g_free(filename);
        } while(gtk_tree_model_iter_next(model, &iter));
    }
}

static void
xfdesktop_settings_queue_preview(GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 AppearancePanel *panel)
{
    gchar *filename = NULL;

    gtk_tree_model_get(model, iter, COL_FILENAME, &filename, -1);

    /* Attempt to use the thumbnailer if possible */
    if(!xfdesktop_thumbnailer_queue_thumbnail(panel->thumbnailer, filename)) {
        /* Thumbnailing not possible, add it to the queue to be loaded manually */
        PreviewData *pdata;
        pdata = g_new0(PreviewData, 1);
        pdata->model = GTK_TREE_MODEL(g_object_ref(G_OBJECT(model)));
        pdata->iter = gtk_tree_iter_copy(iter);
        pdata->scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(panel->image_iconview));

        XF_DEBUG("Thumbnailing failed, adding %s manually.", filename);
        xfdesktop_settings_add_file_to_queue(panel, pdata);
    }

    if(filename)
        g_free(filename);
}

static void
cb_special_icon_toggled(GtkCellRendererToggle *render, gchar *path, gpointer user_data)
{
    XfconfChannel *channel = g_object_get_data(G_OBJECT(user_data),
                                               "xfconf-channel");
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
    GtkTreeIter iter;
    gboolean show_icon;
    gchar *icon_property = NULL;

    gtk_tree_model_get_iter(model, &iter, tree_path);
    gtk_tree_model_get(model, &iter, COL_ICON_ENABLED, &show_icon,
                       COL_ICON_PROPERTY, &icon_property, -1);

    show_icon = !show_icon;

    xfconf_channel_set_bool(channel, icon_property, show_icon);

    gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
                       COL_ICON_ENABLED, show_icon, -1);

    gtk_tree_path_free(tree_path);
    g_free(icon_property);
}

static void
setup_special_icon_list(GtkBuilder *gxml,
                        XfconfChannel *channel)
{
    GtkWidget *treeview;
    GtkTreeStore *ts;
    GtkTreeViewColumn *col;
    GtkCellRenderer *render;
    GtkTreeIter iter, parent_iter, child_iter;
    const struct {
        const gchar *name;
        const gchar *icon_names[2];
        const gchar *xfconf_property;
        gboolean state;
    } icons[] = {
        { N_("Home"), { "user-home", "gnome-fs-desktop" },
          DESKTOP_ICONS_SHOW_HOME, TRUE },
        { N_("File System"), { "drive-harddisk", "gnome-dev-harddisk" },
          DESKTOP_ICONS_SHOW_FILESYSTEM, TRUE },
        { N_("Trash"), { "user-trash", "gnome-fs-trash-empty" },
          DESKTOP_ICONS_SHOW_TRASH, TRUE },
        { N_("Removable Devices"), { "drive-removable-media", "gnome-dev-removable" },
          DESKTOP_ICONS_SHOW_REMOVABLE, TRUE },
        { N_("Network Shares"), { "gtk-network", "gnome-dev-network" },
          DESKTOP_ICONS_SHOW_NETWORK_REMOVABLE, TRUE },
        { N_("Disks and Drives"), { "drive-harddisk-usb", "gnome-dev-removable-usb" },
          DESKTOP_ICONS_SHOW_DEVICE_REMOVABLE, TRUE },
        { N_("Other Devices"), { "multimedia-player", "phone" },
          DESKTOP_ICONS_SHOW_UNKNWON_REMOVABLE, TRUE },
        { NULL, { NULL, NULL }, NULL, FALSE },
    };
    const int REMOVABLE_DEVICES = 4;
    int i, w;

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, NULL);

    ts = gtk_tree_store_new(N_ICON_COLS, G_TYPE_ICON, G_TYPE_STRING,
                            G_TYPE_BOOLEAN, G_TYPE_STRING);
    for(i = 0; icons[i].name; ++i) {
        GIcon *icon = g_themed_icon_new_from_names((char **)icons[i].icon_names, G_N_ELEMENTS(icons[i].icon_names));

        if(i < REMOVABLE_DEVICES) {
            gtk_tree_store_append(ts, &parent_iter, NULL);
            iter = parent_iter;
        } else {
            gtk_tree_store_append(ts, &child_iter, &parent_iter);
            iter = child_iter;
        }

        gtk_tree_store_set(ts, &iter,
                           COL_ICON_NAME, _(icons[i].name),
                           COL_ICON_PIX, icon,
                           COL_ICON_PROPERTY, icons[i].xfconf_property,
                           COL_ICON_ENABLED,
                           xfconf_channel_get_bool(channel,
                                                   icons[i].xfconf_property,
                                                   icons[i].state),
                           -1);
        if (icon != NULL)
            g_object_unref(icon);
    }

    treeview = GTK_WIDGET(gtk_builder_get_object(gxml, "treeview_default_icons"));
    g_object_set_data(G_OBJECT(treeview), "xfconf-channel", channel);
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_spacing(col, 6);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    render = gtk_cell_renderer_toggle_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_add_attribute(col, render, "active", COL_ICON_ENABLED);

    g_signal_connect(G_OBJECT(render), "toggled",
                     G_CALLBACK(cb_special_icon_toggled), treeview);

    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_add_attribute(col, render, "gicon", COL_ICON_PIX);

    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_add_attribute(col, render, "text", COL_ICON_NAME);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(ts));
    g_object_unref(G_OBJECT(ts));
}


static gint
image_list_compare(GtkTreeModel *model,
                   const gchar *a,
                   GtkTreeIter *b)
{
    gchar *key_b = NULL;
    gint ret;

    gtk_tree_model_get(model, b, COL_COLLATE_KEY, &key_b, -1);

    ret = g_strcmp0(a, key_b);

    g_free(key_b);

    return ret;
}

static GtkTreeIter *
xfdesktop_settings_image_iconview_add(GtkTreeModel *model,
                                      const char *path,
                                      GFileInfo *info,
                                      AppearancePanel *panel)
{
    gboolean added = FALSE, found = FALSE, valid = FALSE;
    GtkTreeIter iter, search_iter;
    gchar *name = NULL, *name_utf8 = NULL, *name_markup = NULL, *size_string = NULL;
    gchar *collate_key = NULL;
    gint position = 0;
    const gchar *content_type = g_file_info_get_content_type(info);
    goffset file_size = g_file_info_get_size(info);

    if(!xfdesktop_image_file_is_valid(path))
        return NULL;

    name = g_path_get_basename(path);
    if(name) {
        guint name_length = strlen(name);
        name_utf8 = g_filename_to_utf8(name, name_length,
                                       NULL, NULL, NULL);
        if(name_utf8) {
            size_string = g_format_size(file_size);

            /* Display the file name, file type, and file size in the tooltip. */
            name_markup = g_markup_printf_escaped(_("<b>%s</b>\nType: %s\nSize: %s"),
                                                  name_utf8, content_type, size_string);

            /* create a case sensitive collation key for sorting filenames like
             * Thunar does */
            collate_key = g_utf8_collate_key_for_filename(name, name_length);

            /* Insert sorted */
            valid = gtk_tree_model_get_iter_first(model, &search_iter);
            while(valid && !found) {
                if(image_list_compare(model, collate_key, &search_iter) <= 0) {
                    found = TRUE;
                } else {
                    valid = gtk_tree_model_iter_next(model, &search_iter);
                    position++;
                }
            }

            gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                              &iter,
                                              position,
                                              COL_NAME, name_markup,
                                              COL_FILENAME, path,
                                              COL_COLLATE_KEY, collate_key,
                                              -1);
            xfdesktop_settings_queue_preview(model, &iter, panel);

            added = TRUE;
        }
    }

    g_free(name);
    g_free(name_utf8);
    g_free(name_markup);
    g_free(size_string);
    g_free(collate_key);

    if(added)
        return gtk_tree_iter_copy(&iter);
    else
        return NULL;
}

static void
cb_destroy_add_dir_enumeration(gpointer user_data)
{
    AddDirData *dir_data = user_data;
    AppearancePanel *panel = dir_data->panel;

    TRACE("entering");

    g_free(dir_data->file_path);
    g_free(dir_data->last_image);

    if(G_IS_FILE_ENUMERATOR(dir_data->file_enumerator))
        g_object_unref(dir_data->file_enumerator);

    g_free(dir_data);

    if(panel->cancel_enumeration) {
        g_object_unref(panel->cancel_enumeration);
        panel->cancel_enumeration = NULL;
    }

    panel->add_dir_idle_id = 0;
}

static gboolean
xfdesktop_image_list_add_item(gpointer user_data)
{
    AddDirData *dir_data = user_data;
    AppearancePanel *panel = dir_data->panel;
    GFileInfo *info;
    GtkTreeIter *iter;

    /* If the enumeration gets canceled/destroyed return and
     * cb_destroy_add_dir_enumeration will get called to clean up */
    if(!G_IS_FILE_ENUMERATOR(dir_data->file_enumerator))
        return FALSE;

    /* Add one item to the icon view at a time so we don't block the UI */
    if((info = g_file_enumerator_next_file(dir_data->file_enumerator, NULL, NULL))) {
        const gchar *file_name = g_file_info_get_name(info);
        gchar *buf = g_strconcat(dir_data->file_path, "/", file_name, NULL);

        iter = xfdesktop_settings_image_iconview_add(GTK_TREE_MODEL(dir_data->ls), buf, info, panel);
        if(iter) {
            if(!dir_data->selected_iter &&
               !strcmp(buf, dir_data->last_image))
            {
                dir_data->selected_iter = iter;
            } else {
                gtk_tree_iter_free(iter);
            }
        }

        g_free(buf);
        g_object_unref(info);

        /* continue on the next idle callback so the user's events get priority */
        return TRUE;
    }

    /* If we get here we're done enumerating files in the directory */

    gtk_icon_view_set_model(GTK_ICON_VIEW(panel->image_iconview),
                            GTK_TREE_MODEL(dir_data->ls));

    /* last_image is in the directory added then it should be selected */
    if(dir_data->selected_iter) {
        GtkTreePath *path;
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(dir_data->ls), dir_data->selected_iter);
        gtk_tree_iter_free(dir_data->selected_iter);
        if(path) {
            gtk_icon_view_select_path(GTK_ICON_VIEW(panel->image_iconview), path);
            gtk_tree_path_free(path);
        }
     }

    if (dir_data->selected_iter == NULL) {
        gchar *prop_name = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-enable");
        gboolean backdrop_cycle_enable = xfconf_channel_get_bool(panel->channel, prop_name, FALSE);

        if (backdrop_cycle_enable) {
            gtk_widget_show(panel->btn_folder_apply);
        }

        g_free(prop_name);
    }

    /* cb_destroy_add_dir_enumeration will get called to clean up */
    return FALSE;
}

static void
xfdesktop_image_list_add_dir(GObject *source_object,
                             GAsyncResult *res,
                             gpointer user_data)
{
    AppearancePanel *panel = user_data;
    AddDirData *dir_data = g_new0(AddDirData, 1);

    TRACE("entering");

    dir_data->panel = panel;

    dir_data->ls = gtk_list_store_new(N_COLS, GDK_TYPE_PIXBUF, CAIRO_GOBJECT_TYPE_SURFACE, G_TYPE_STRING,
                                      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* Get the last image/current image displayed so we can select it in the
     * icon view */
    dir_data->last_image = xfdesktop_settings_get_backdrop_image(panel);

    dir_data->file_path = g_file_get_path(panel->selected_folder);

    dir_data->file_enumerator = g_file_enumerate_children_finish(panel->selected_folder,
                                                                 res,
                                                                 NULL);

    /* Individual items are added in an idle callback so everything is more
     * responsive */
    panel->add_dir_idle_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                             xfdesktop_image_list_add_item,
                                             dir_data,
                                             cb_destroy_add_dir_enumeration);
}

static void
xfdesktop_settings_update_iconview_frame_name(AppearancePanel *panel,
                                              WnckWorkspace *wnck_workspace)
{
    gchar buf[1024];
    gchar *workspace_name;
    WnckScreen *screen;
    WnckWorkspace *workspace;

    /* Don't update the name until we find our window */
    if(panel->wnck_window == NULL)
        return;

    g_return_if_fail(panel->monitor >= 0 && panel->workspace >= 0);

    screen = wnck_window_get_screen(panel->wnck_window);

    /* If it's a pinned window get the active workspace */
    if(wnck_workspace == NULL) {
        workspace = wnck_screen_get_workspace(screen, panel->active_workspace);
    } else {
        workspace = wnck_workspace;
    }

    workspace_name = g_strdup(wnck_workspace_get_name(workspace));

    if(gdk_display_get_n_monitors(gtk_widget_get_display(panel->chk_apply_to_all)) > 1) {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all))) {
            /* Multi-monitor single workspace */
            if(panel->monitor_name) {
                g_snprintf(buf, sizeof(buf),
                           _("Wallpaper for Monitor %d (%s)"),
                           panel->monitor, panel->monitor_name);
            } else {
                g_snprintf(buf, sizeof(buf), _("Wallpaper for Monitor %d"), panel->monitor);
            }

            /* This is for the infobar letting the user know how to configure
             * multiple monitor setups */
            gtk_label_set_text(GTK_LABEL(panel->infobar_label),
                               _("Move this dialog to the display you "
                                 "want to edit the settings for."));
            gtk_widget_set_visible(panel->infobar, TRUE);
        } else {
            /* Multi-monitor per workspace wallpaper */
            if(panel->monitor_name) {
                g_snprintf(buf, sizeof(buf),
                           _("Wallpaper for %s on Monitor %d (%s)"),
                           workspace_name, panel->monitor, panel->monitor_name);
            } else {
                g_snprintf(buf, sizeof(buf),
                           _("Wallpaper for %s on Monitor %d"),
                           workspace_name, panel->monitor);
            }

            /* This is for the infobar letting the user know how to configure
             * multiple monitor/workspace setups */
            gtk_label_set_text(GTK_LABEL(panel->infobar_label),
                               _("Move this dialog to the display and "
                                 "workspace you want to edit the settings for."));
            gtk_widget_set_visible(panel->infobar, TRUE);
        }
    } else {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all)) ||
           (wnck_screen_get_workspace_count(screen) == 1)) {
            /* Single monitor and single workspace */
            g_snprintf(buf, sizeof(buf), _("Wallpaper for my desktop"));

            /* No need for the infobar */
            gtk_widget_set_visible(panel->infobar, FALSE);
        } else {
            /* Single monitor and per workspace wallpaper */
            g_snprintf(buf, sizeof(buf), _("Wallpaper for %s"), workspace_name);

            /* This is for the infobar letting the user know how to configure
             * multiple workspace setups */
            gtk_label_set_text(GTK_LABEL(panel->infobar_label),
                               _("Move this dialog to the workspace you "
                                 "want to edit the settings for."));
            gtk_widget_set_visible(panel->infobar, TRUE);
        }
    }

    /* This label is for which workspace/monitor we're on */
    gtk_label_set_text(GTK_LABEL(panel->label_header), buf);

    g_free(workspace_name);
}

/* Free the returned string when done using it */
static gchar*
xfdesktop_settings_generate_per_workspace_binding_string(AppearancePanel *panel,
                                                         const gchar* property)
{
    gchar *buf = NULL;

    if(panel->monitor_name == NULL) {
        buf = g_strdup_printf("/backdrop/screen%d/monitor%d/workspace%d/%s",
                              panel->screen, panel->monitor, panel->workspace,
                              property);
    } else {
        buf = xfdesktop_remove_whitspaces(g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/%s",
                              panel->screen, panel->monitor_name, panel->workspace,
                              property));
    }

    XF_DEBUG("name %s", buf);

    return buf;
}

static gchar*
xfdesktop_settings_generate_old_binding_string(AppearancePanel *panel,
                                               const gchar* property)
{
    gchar *buf = NULL;

    buf = g_strdup_printf("/backdrop/screen%d/monitor%d/%s",
                          panel->screen, panel->monitor, property);

    XF_DEBUG("name %s", buf);

    return buf;
}

/* Attempts to load the backdrop from the current location followed by using
 * how previous versions of xfdesktop (before 4.11) did. This matches how
 * xfdesktop searches for backdrops.
 * Free the returned string when done using it. */
static gchar *
xfdesktop_settings_get_backdrop_image(AppearancePanel *panel)
{
    gchar *last_image;
    gchar *property, *old_property = NULL;

    /* Get the last image/current image displayed, if available */
    property = xfdesktop_settings_generate_per_workspace_binding_string(panel, "last-image");

    last_image = xfconf_channel_get_string(panel->channel, property, NULL);

    /* Try the previous version or fall back to our provided default */
    if(last_image == NULL) {
        old_property = xfdesktop_settings_generate_old_binding_string(panel,
                                                                      "image-path");
        last_image = xfconf_channel_get_string(panel->channel,
                                               old_property,
                                               DEFAULT_BACKDROP);
    }

    g_free(property);
    if(old_property)
        g_free(old_property);

    return last_image;
}

static void
cb_image_selection_changed(GtkIconView *icon_view,
                           gpointer user_data)
{
    AppearancePanel *panel = user_data;
    GtkTreeModel *model = gtk_icon_view_get_model(icon_view);
    GtkTreeIter iter;
    GList *selected_items = NULL;
    gchar *filename = NULL, *current_filename = NULL;
    gchar *buf = NULL;

    TRACE("entering");

    if(panel->image_list_loaded && GTK_IS_TREE_MODEL(model))
        return;

    selected_items = gtk_icon_view_get_selected_items(icon_view);

    /* We only care about the first selected item because the iconview
     * should be set to single selection mode */
    if(!selected_items || g_list_first(selected_items) == NULL)
        return;

    if(!gtk_tree_model_get_iter(model, &iter, g_list_first(selected_items)->data))
        return;

    gtk_widget_hide(panel->btn_folder_apply);

    gtk_tree_model_get(model, &iter, COL_FILENAME, &filename, -1);

    /* Get the current/last image, handles migrating from old versions */
    current_filename = xfdesktop_settings_get_backdrop_image(panel);

    /* check to see if the selection actually did change */
    if(g_strcmp0(current_filename, filename) != 0) {
        if(panel->monitor_name == NULL) {
            XF_DEBUG("got %s, applying to screen %d monitor %d workspace %d", filename,
                     panel->screen, panel->monitor, panel->workspace);
        } else {
            XF_DEBUG("got %s, applying to screen %d monitor %s workspace %d", filename,
                     panel->screen, panel->monitor_name, panel->workspace);
        }

        /* Get the property location to save our changes, always save to new
         * location */
        buf = xfdesktop_settings_generate_per_workspace_binding_string(panel,
                                                                       "last-image");
        XF_DEBUG("Saving to %s/%s", buf, filename);
        xfconf_channel_set_string(panel->channel, buf, filename);
    }

    g_list_free_full(selected_items, xfdesktop_tree_path_free);
    g_free(current_filename);
    if(buf)
        g_free(buf);
}

static gint
xfdesktop_settings_get_active_workspace(AppearancePanel *panel,
                                        WnckWindow *wnck_window)
{
    WnckWorkspace *wnck_workspace;
    gboolean single_workspace;
    gint workspace_num, single_workspace_num;
    WnckScreen *wnck_screen = wnck_window_get_screen(wnck_window);

    wnck_workspace = wnck_window_get_workspace(wnck_window);

    /* If wnck_workspace is NULL that means it's pinned and we just need to
     * use the active/current workspace */
    if(wnck_workspace != NULL) {
        workspace_num = wnck_workspace_get_number(wnck_workspace);
    } else {
        workspace_num = panel->active_workspace;
    }

    single_workspace = xfconf_channel_get_bool(panel->channel,
                                               SINGLE_WORKSPACE_MODE,
                                               TRUE);

    /* If we're in single_workspace mode we need to return the workspace that
     * it was set to, if that workspace exists, otherwise return the current
     * workspace and turn off the single workspace mode */
    if(single_workspace) {
        single_workspace_num = xfconf_channel_get_int(panel->channel,
                                                      SINGLE_WORKSPACE_NUMBER,
                                                      0);
        if(single_workspace_num < wnck_screen_get_workspace_count(wnck_screen)) {
            return single_workspace_num;
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all),
                                         FALSE);
        }
    }

    return workspace_num;
}

/* This works for both the custom font size and show tooltips check buttons,
 * it just enables the associated spin button */
static void
cb_xfdesktop_chk_button_toggled(GtkCheckButton *button,
                                gpointer user_data)
{
    GtkWidget *spin_button = GTK_WIDGET(user_data);

    TRACE("entering");

    gtk_widget_set_sensitive(spin_button,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

static void
update_backdrop_random_order_chkbox(AppearancePanel *panel)
{
    gboolean sensitive = FALSE;
    gint period;

    /* For the random check box to be active the combo_backdrop_cycle_period
     * needs to be active and needs to not be set to chronological */
    if(gtk_widget_get_sensitive(panel->combo_backdrop_cycle_period)) {
        period = gtk_combo_box_get_active(GTK_COMBO_BOX(panel->combo_backdrop_cycle_period));
        if(period != XFCE_BACKDROP_PERIOD_CHRONOLOGICAL)
            sensitive = TRUE;
    }

    gtk_widget_set_sensitive(panel->random_backdrop_order_chkbox, sensitive);
}

static void
update_backdrop_cycle_spinbox(AppearancePanel *panel)
{
    gboolean sensitive = FALSE;
    gint period;

    /* For the spinbox to be active the combo_backdrop_cycle_period needs to be
     * active and needs to be set to something where the spinbox would apply */
    if(gtk_widget_get_sensitive(panel->combo_backdrop_cycle_period)) {
        period = gtk_combo_box_get_active(GTK_COMBO_BOX(panel->combo_backdrop_cycle_period));
        if(period == XFCE_BACKDROP_PERIOD_SECONDS ||
           period == XFCE_BACKDROP_PERIOD_MINUTES  ||
           period == XFCE_BACKDROP_PERIOD_HOURS)
        {
            sensitive = TRUE;
        }
    }

    gtk_widget_set_sensitive(panel->backdrop_cycle_spinbox, sensitive);
}

static void
cb_combo_backdrop_cycle_period_change(GtkComboBox *combo,
                                      gpointer user_data)
{
    AppearancePanel *panel = user_data;

    /* determine if the spin box should be sensitive */
    update_backdrop_cycle_spinbox(panel);
    /* determine if the random check box should be sensitive */
    update_backdrop_random_order_chkbox(panel);
}

static void
cb_xfdesktop_chk_cycle_backdrop_toggled(GtkCheckButton *button,
                                        gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gboolean sensitive = FALSE;

    TRACE("entering");

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(panel->backdrop_cycle_chkbox))) {
           sensitive = TRUE;
    }

    /* The cycle backdrop toggles the period and random widgets */
    gtk_widget_set_sensitive(panel->combo_backdrop_cycle_period, sensitive);
    /* determine if the spin box should be sensitive */
    update_backdrop_cycle_spinbox(panel);
    /* determine if the random check box should be sensitive */
    update_backdrop_random_order_chkbox(panel);

    if (!sensitive) {
        gtk_widget_hide(panel->btn_folder_apply);
    } else {
        GList *selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(panel->image_iconview));
        if (selected == NULL) {
            gtk_widget_show(panel->btn_folder_apply);
        } else {
            g_list_free(selected);
        }
    }
}

static gboolean
xfdesktop_spin_icon_size_timer(gpointer user_data)
{
    GtkSpinButton *button = user_data;
    XfconfChannel *channel = g_object_get_data(G_OBJECT(button), "xfconf-chanel");

    TRACE("entering");

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), FALSE);

    xfconf_channel_set_uint(channel,
                            DESKTOP_ICONS_ICON_SIZE_PROP,
                            gtk_spin_button_get_value(button));

    g_object_set_data(G_OBJECT(button), "timer-id", NULL);

    return FALSE;
}

static void
cb_xfdesktop_spin_icon_size_changed(GtkSpinButton *button,
                                    gpointer user_data)
{
    guint timer_id = 0;

    TRACE("entering");

    g_object_set_data(G_OBJECT(button), "xfconf-chanel", user_data);

    timer_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "timer-id"));
    if(timer_id != 0)
        g_source_remove(timer_id);

    timer_id = g_timeout_add(500, xfdesktop_spin_icon_size_timer, button);

    g_object_set_data(G_OBJECT(button), "timer-id", GUINT_TO_POINTER(timer_id));
}

static void
xfdesktop_settings_stop_image_loading(AppearancePanel *panel)
{
    XF_DEBUG("xfdesktop_settings_stop_image_loading");
    /* stop any thumbnailing in progress */
    xfdesktop_thumbnailer_dequeue_all_thumbnails(panel->thumbnailer);

    /* stop the idle preview callback */
    if(panel->preview_id != 0) {
        g_source_remove(panel->preview_id);
        panel->preview_id = 0;
    }

    /* Remove the previews in the message queue */
    if(panel->preview_queue != NULL) {
        g_async_queue_unref(panel->preview_queue);
        panel->preview_queue = NULL;
    }

    /* Cancel any file enumeration that's running */
    if(panel->cancel_enumeration != NULL) {
        g_cancellable_cancel(panel->cancel_enumeration);
        g_object_unref(panel->cancel_enumeration);
        panel->cancel_enumeration = NULL;
    }

    /* Cancel the file enumeration for populating the icon view */
    if(panel->add_dir_idle_id != 0) {
        g_source_remove(panel->add_dir_idle_id);
        panel->add_dir_idle_id = 0;
    }
}

static void
cb_xfdesktop_bnt_exit_clicked(GtkButton *button, gpointer user_data)
{
    AppearancePanel *panel = user_data;

    xfdesktop_settings_stop_image_loading(panel);
}

static void
cb_folder_selection_changed(GtkWidget *button,
                            gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gchar *new_folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
    gchar *previous_folder = NULL;

    TRACE("entering");

    if(panel->selected_folder != NULL)
        previous_folder = g_file_get_path(panel->selected_folder);

    /* Check to see if the folder actually did change */
    if(g_strcmp0(new_folder, previous_folder) == 0) {
        const gchar *current_filename = xfdesktop_settings_get_backdrop_image(panel);
        gchar *current_folder = g_path_get_dirname(current_filename);

        /* workaround another gtk bug - if the user sets the file chooser
         * button to something then it can't be changed with a set folder
         * call anymore. */
        if(g_strcmp0(new_folder, current_folder) == 0) {
            XF_DEBUG("folder didn't change");
            g_free(current_folder);
            g_free(new_folder);
            g_free(previous_folder);
            return;
        } else {
            g_free(new_folder);
            new_folder = current_folder;
        }
    }

    TRACE("folder changed to: %s", new_folder);

    if(panel->selected_folder != NULL)
        g_object_unref(panel->selected_folder);

    panel->selected_folder = g_file_new_for_path(new_folder);

    /* Stop any previous loading since something changed */
    xfdesktop_settings_stop_image_loading(panel);

    panel->cancel_enumeration = g_cancellable_new();

    g_file_enumerate_children_async(panel->selected_folder,
                                    XFDESKTOP_FILE_INFO_NAMESPACE,
                                    G_FILE_QUERY_INFO_NONE,
                                    G_PRIORITY_DEFAULT,
                                    panel->cancel_enumeration,
                                    xfdesktop_image_list_add_dir,
                                    panel);

    g_free(new_folder);
    g_free(previous_folder);
}

static void
cb_folder_apply_clicked(GtkWidget *button,
                        AppearancePanel *panel)
{
    GList *selected;

    selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(panel->image_iconview));
    if (G_LIKELY(selected == NULL)) {
        GtkTreeModel *model;
        GtkTreeIter first;

        model = gtk_icon_view_get_model(GTK_ICON_VIEW(panel->image_iconview));
        if (gtk_tree_model_get_iter_first(model, &first)) {
            GtkTreePath *path = gtk_tree_model_get_path(model, &first);
            if (G_LIKELY(path != NULL)) {
                gtk_icon_view_select_path(GTK_ICON_VIEW(panel->image_iconview), path);
                gtk_tree_path_free(path);
            }
        }
    } else {
        g_list_free(selected);
    }

    gtk_widget_hide(button);
}

static void
cb_xfdesktop_combo_image_style_changed(GtkComboBox *combo,
                                       gpointer user_data)
{
    AppearancePanel *panel = user_data;

    TRACE("entering");

    if(gtk_combo_box_get_active(combo) == XFCE_BACKDROP_IMAGE_NONE) {
        /* No wallpaper so set the iconview to insensitive so the user doesn't
         * pick wallpapers that have no effect. Stop popping up tooltips for
         * the now insensitive iconview and provide a tooltip explaining why
         * the iconview is insensitive. */
        gtk_widget_set_sensitive(panel->image_iconview, FALSE);
        g_object_set(G_OBJECT(panel->image_iconview),
                     "tooltip-column", -1,
                     NULL);
        gtk_widget_set_tooltip_text(panel->image_iconview,
                                    _("Image selection is unavailable while the image style is set to None."));
    } else {
        gint tooltip_column;

        /* We are expected to provide a wallpaper so make the iconview active.
         * Additionally, if we were insensitive then we need to remove the
         * global iconview tooltip and enable the individual tooltips again. */
        gtk_widget_set_sensitive(panel->image_iconview, TRUE);
        g_object_get(G_OBJECT(panel->image_iconview),
                     "tooltip-column", &tooltip_column,
                     NULL);
        if(tooltip_column == -1) {
            gtk_widget_set_tooltip_text(panel->image_iconview, NULL);
            g_object_set(G_OBJECT(panel->image_iconview),
                         "tooltip-column", COL_NAME,
                         NULL);
        }
    }
}

static void
cb_xfdesktop_combo_color_changed(GtkComboBox *combo,
                                 gpointer user_data)
{
    enum {
        COLORS_SOLID = 0,
        COLORS_HGRADIENT,
        COLORS_VGRADIENT,
        COLORS_NONE,
    };
    AppearancePanel *panel = user_data;

    TRACE("entering");

    if(gtk_combo_box_get_active(combo) == COLORS_SOLID) {
        gtk_widget_set_sensitive(panel->color1_btn, TRUE);
        gtk_widget_set_sensitive(panel->color2_btn, FALSE);
    } else if(gtk_combo_box_get_active(combo) == COLORS_NONE) {
        gtk_widget_set_sensitive(panel->color1_btn, FALSE);
        gtk_widget_set_sensitive(panel->color2_btn, FALSE);
    } else {
        gtk_widget_set_sensitive(panel->color1_btn, TRUE);
        gtk_widget_set_sensitive(panel->color2_btn, TRUE);
    }
}

static void
xfdesktop_settings_update_iconview_folder(AppearancePanel *panel)
{
    gchar *current_folder, *dirname;

    /* If we haven't found our window return now and wait for that */
    if(panel->wnck_window == NULL)
        return;

    TRACE("entering");

    current_folder = xfdesktop_settings_get_backdrop_image(panel);
    dirname = g_path_get_dirname(current_folder);

    XF_DEBUG("current_folder %s, dirname %s", current_folder, dirname);

    gtk_file_chooser_set_current_folder((GtkFileChooser*)panel->btn_folder, dirname);

    /* Workaround for a bug in GTK */
    cb_folder_selection_changed(panel->btn_folder, panel);

    g_free(current_folder);
    g_free(dirname);
}

/* This function is to add or remove all the bindings for the background
 * tab. It's intended to be used when the app changes monitors or workspaces.
 * It reverts the items back to their defaults before binding any new settings
 * that way if the setting isn't present, the default correctly displays. */
static void
xfdesktop_settings_background_tab_change_bindings(AppearancePanel *panel,
                                                  gboolean remove_binding)
{
    gchar *buf, *old_property = NULL;
    XfconfChannel *channel = panel->channel;

    /* Image style combo box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "image-style");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                               G_OBJECT(panel->image_style_combo), "active");
    } else {
        /* If the current image style doesn't exist, try to load the old one */
        if(!xfconf_channel_has_property(channel, buf)) {
            gint image_style;
            old_property = xfdesktop_settings_generate_old_binding_string(panel, "image-style");

            /* default to zoomed when trying to migrate (zoomed was part of how
             * auto worked in 4.10)*/
            image_style = xfconf_channel_get_int(channel, old_property, XFCE_BACKDROP_IMAGE_ZOOMED);

            /* xfce_translate_image_styles will do sanity checking */
            gtk_combo_box_set_active(GTK_COMBO_BOX(panel->image_style_combo),
                                     xfce_translate_image_styles(image_style));

            g_free(old_property);
        }

        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(panel->image_style_combo), "active");
        /* determine if the iconview is sensitive */
        cb_xfdesktop_combo_image_style_changed(GTK_COMBO_BOX(panel->image_style_combo), panel);
    }
    g_free(buf);

    /* Color style combo box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "color-style");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                           G_OBJECT(panel->color_style_combo), "active");
    } else {
        /* If the current color style doesn't exist, try to load the old one */
        if(!xfconf_channel_has_property(channel, buf)) {
            gint color_style;
            old_property = xfdesktop_settings_generate_old_binding_string(panel, "color-style");

            /* default to solid when trying to migrate */
            color_style = xfconf_channel_get_int(channel, old_property, XFCE_BACKDROP_COLOR_SOLID);

            /* sanity check */
            if(color_style < 0 || color_style > XFCE_BACKDROP_COLOR_TRANSPARENT) {
                g_warning("invalid color style, setting to solid");
                color_style = XFCE_BACKDROP_COLOR_SOLID;
            }

            gtk_combo_box_set_active(GTK_COMBO_BOX(panel->color_style_combo),
                                     color_style);
            g_free(old_property);
        }

        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(panel->color_style_combo), "active");
        /* update the color button sensitivity */
        cb_xfdesktop_combo_color_changed(GTK_COMBO_BOX(panel->color_style_combo), panel);
    }
    g_free(buf);

    /* color 1 button */
    /* Fixme: we will need to migrate from color1 to rgba1 (GdkColor to GdkRGBA) */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "rgba1");
    if(remove_binding) {
        xfconf_g_property_unbind(panel->color1_btn_id);
    } else {
        /* If the first color doesn't exist, try to load the old one */
        if(!xfconf_channel_has_property(channel, buf)) {
            GValue value = { 0, };
            old_property = xfdesktop_settings_generate_old_binding_string(panel, "rgba1");

            xfconf_channel_get_property(channel, old_property, &value);

            if(G_VALUE_HOLDS_BOXED(&value)) {
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(panel->color1_btn),
                                           g_value_get_boxed(&value));
                g_value_unset(&value);
            } else {
                /* revert to showing our default color */
                GdkRGBA color1;
                color1.red = 0.5f;
                color1.green = 0.5f;
                color1.blue = 0.5f;
                color1.alpha = 1.0f;
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(panel->color1_btn), &color1);
            }

            g_free(old_property);
        }

        /* Now bind to the new value */
        panel->color1_btn_id = xfconf_g_property_bind_gdkrgba(channel, buf,
                                                              G_OBJECT(panel->color1_btn),
                                                              "rgba");
    }
    g_free(buf);

    /* color 2 button */
    /* Fixme: we will need to migrate from color1 to rgba1 (GdkColor to GdkRGBA) */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "rgba2");
    if(remove_binding) {
        xfconf_g_property_unbind(panel->color2_btn_id);
    } else {
        /* If the 2nd color doesn't exist, try to load the old one */
        if(!xfconf_channel_has_property(channel, buf)) {
            GValue value = { 0, };
            old_property = xfdesktop_settings_generate_old_binding_string(panel, "rgba2");

            xfconf_channel_get_property(channel, old_property, &value);

            if(G_VALUE_HOLDS_BOXED(&value)) {
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(panel->color2_btn),
                                           g_value_get_boxed(&value));
                g_value_unset(&value);
            } else {
                /* revert to showing our default color */
                GdkRGBA color2;
                color2.red = 0.5f;
                color2.green = 0.5f;
                color2.blue = 0.5f;
                color2.alpha = 1.0f;
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(panel->color2_btn), &color2);
            }

            g_free(old_property);
        }

        /* Now bind to the new value */
        panel->color2_btn_id = xfconf_g_property_bind_gdkrgba(channel, buf,
                                                              G_OBJECT(panel->color2_btn),
                                                              "rgba");
    }
    g_free(buf);

    /* enable cycle timer check box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-enable");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                           G_OBJECT(panel->backdrop_cycle_chkbox), "active");
    } else {
        /* The default is disable cycling, set that before we bind to anything */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->backdrop_cycle_chkbox), FALSE);

        xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                               G_OBJECT(panel->backdrop_cycle_chkbox), "active");
    }
    g_free(buf);

    /* backdrop cycle period combo box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-period");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                               G_OBJECT(panel->combo_backdrop_cycle_period), "active");
    } else {
        /* default is minutes, set that before we bind to it */
        gtk_combo_box_set_active(GTK_COMBO_BOX(panel->combo_backdrop_cycle_period), XFCE_BACKDROP_PERIOD_MINUTES);

        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(panel->combo_backdrop_cycle_period), "active");
        /* determine if the cycle timer spin box is sensitive */
        cb_combo_backdrop_cycle_period_change(GTK_COMBO_BOX(panel->combo_backdrop_cycle_period), panel);
    }
    g_free(buf);

    /* cycle timer spin box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-timer");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                   G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(panel->backdrop_cycle_spinbox))),
                   "value");
    } else {
        guint current_timer = xfconf_channel_get_uint(channel, buf, 10);
        /* Update the spin box before we bind to it */
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel->backdrop_cycle_spinbox), current_timer);

        xfconf_g_property_bind(channel, buf, G_TYPE_UINT,
                       G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(panel->backdrop_cycle_spinbox))),
                       "value");
    }
    g_free(buf);

    /* cycle random order check box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-random-order");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                           G_OBJECT(panel->random_backdrop_order_chkbox), "active");
    } else {
        /* The default is sequential, set that before we bind to anything */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->random_backdrop_order_chkbox), FALSE);

        xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                               G_OBJECT(panel->random_backdrop_order_chkbox), "active");
    }
    g_free(buf);
}

static void
suboptions_set_sensitive(GtkToggleButton *btn,
                         gpointer user_data)
{
    GtkWidget *box = user_data;
    gtk_widget_set_sensitive(box, gtk_toggle_button_get_active(btn));
}

static void
cb_update_background_tab(WnckWindow *wnck_window,
                         gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gint screen_num, monitor_num, workspace_num;
    gchar *monitor_name = NULL;
    WnckWorkspace *wnck_workspace = NULL;
    WnckScreen    *wnck_screen = NULL;
    GdkWindow     *window = NULL;
    GdkDisplay    *display = NULL;
    GdkMonitor    *monitor = NULL;

    /* If we haven't found our window return now and wait for that */
    if(panel->wnck_window == NULL)
        return;

    /* Get all the new settings for comparison */
    wnck_screen = wnck_window_get_screen(panel->wnck_window);
    wnck_workspace = wnck_window_get_workspace(panel->wnck_window);

    workspace_num = xfdesktop_settings_get_active_workspace(panel, wnck_window);
    screen_num = wnck_screen_get_number(wnck_screen);
    window = gtk_widget_get_window(panel->image_iconview);
    display = gdk_window_get_display(window);
    monitor = gdk_display_get_monitor_at_window(display, window);
    monitor_num = xfdesktop_get_monitor_num(display, monitor);
    monitor_name = xfdesktop_get_monitor_name_from_gtk_widget(panel->image_iconview, monitor_num);

    /* Most of the time we won't change monitor, screen, or workspace so try
     * to bail out now if we can */
    /* Check to see if we haven't changed workspace or screen */
    if(panel->workspace == workspace_num && panel->screen == screen_num)
    {
        /* do we support monitor names? */
        if(panel->monitor_name != NULL || monitor_name != NULL) {
            /* Check if we haven't changed monitors (by name) */
            if(g_strcmp0(panel->monitor_name, monitor_name) == 0) {
                g_free(monitor_name);
                return;
            }
        }
    }

    TRACE("screen, monitor, or workspace changed");

    /* remove the old bindings if there are any */
    if(panel->color1_btn_id || panel->color2_btn_id) {
        XF_DEBUG("removing old bindings");
        xfdesktop_settings_background_tab_change_bindings(panel,
                                                          TRUE);
    }

    if(panel->monitor_name != NULL)
        g_free(panel->monitor_name);
    if(monitor_name != NULL)
        g_free(monitor_name);

    panel->workspace = workspace_num;
    panel->screen = screen_num;
    panel->monitor = monitor_num;
    panel->monitor_name = xfdesktop_get_monitor_name_from_gtk_widget(panel->image_iconview, monitor_num);

    /* The first monitor has the option of doing the "spanning screens" style,
     * but only if there's multiple monitors attached. Remove it in all other cases.
     *
     * Remove the spanning screens option before we potentially add it again
     */
    gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(panel->image_style_combo),
                              XFCE_BACKDROP_IMAGE_SPANNING_SCREENS);

    if(panel->monitor == 0 && gdk_display_get_n_monitors(gtk_widget_get_display(panel->image_style_combo)) > 1) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(panel->image_style_combo),
                                       _("Spanning screens"));
    }


    /* connect the new bindings */
    xfdesktop_settings_background_tab_change_bindings(panel,
                                                      FALSE);

    xfdesktop_settings_update_iconview_frame_name(panel, wnck_workspace);
    xfdesktop_settings_update_iconview_folder(panel);
}

static void
cb_workspace_changed(WnckScreen *screen,
                     WnckWorkspace *workspace,
                     gpointer user_data)
{
    AppearancePanel *panel = user_data;
    WnckWorkspace *active_workspace;
    gint new_num = -1;

    g_return_if_fail(WNCK_IS_SCREEN(screen));

    /* Update the current workspace number */
    active_workspace = wnck_screen_get_active_workspace(screen);
    if(WNCK_IS_WORKSPACE(active_workspace))
        new_num = wnck_workspace_get_number(active_workspace);

    /* This will sometimes fail to update */
    if(new_num != -1)
        panel->active_workspace = new_num;

    XF_DEBUG("active_workspace now %d", panel->active_workspace);

    /* Call cb_update_background_tab in case this is a pinned window */
    if(panel->wnck_window != NULL)
        cb_update_background_tab(panel->wnck_window, panel);
}

static void
cb_workspace_count_changed(WnckScreen *screen,
                           WnckWorkspace *workspace,
                           gpointer user_data)
{
    AppearancePanel *panel = user_data;

    /* Update background because the single workspace mode may have
     * changed due to the addition/removal of a workspace */
    if(panel->wnck_window != NULL)
        cb_update_background_tab(panel->wnck_window, user_data);
}

static void
cb_window_opened(WnckScreen *screen,
                 WnckWindow *window,
                 gpointer user_data)
{
    AppearancePanel *panel = user_data;
    GtkWidget *toplevel;
    WnckWorkspace *workspace;

    /* If we already found our window, exit */
    if(panel->wnck_window != NULL)
        return;

    toplevel = gtk_widget_get_toplevel(panel->image_iconview);

    /* If they don't match then it's not our window, exit */
    if(wnck_window_get_xid(window) != GDK_WINDOW_XID(gtk_widget_get_window(toplevel)))
        return;

    XF_DEBUG("Found our window");
    panel->wnck_window = window;

    /* These callbacks are for updating the image_iconview when the window
     * moves to another monitor or workspace */
    g_signal_connect(panel->wnck_window, "geometry-changed",
                     G_CALLBACK(cb_update_background_tab), panel);
    g_signal_connect(panel->wnck_window, "workspace-changed",
                     G_CALLBACK(cb_update_background_tab), panel);

    workspace = wnck_window_get_workspace(window);

    /* Update the active workspace number */
    cb_workspace_changed(screen, workspace, panel);

    /* Update the background settings */
    cb_update_background_tab(window, panel);

    /* If it's a pinned window get the active workspace */
    if(workspace == NULL)
        workspace = wnck_screen_get_workspace(screen, panel->active_workspace);

    /* Update the frame name */
    xfdesktop_settings_update_iconview_frame_name(panel, workspace);
}

static void
cb_monitor_changed(GdkScreen *gscreen,
                   gpointer user_data)
{
    AppearancePanel *panel = user_data;

    /* Update background because the monitor we're on may have changed */
    cb_update_background_tab(panel->wnck_window, user_data);

    /* Update the frame name because we may change from/to a single monitor */
    xfdesktop_settings_update_iconview_frame_name(panel, wnck_window_get_workspace(panel->wnck_window));
}

static void
cb_xfdesktop_chk_apply_to_all(GtkCheckButton *button,
                              gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gboolean active;
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

    TRACE("entering");

    xfconf_channel_set_bool(panel->channel,
                            SINGLE_WORKSPACE_MODE,
                            active);

    if(active) {
        xfconf_channel_set_int(panel->channel,
                               SINGLE_WORKSPACE_NUMBER,
                               panel->workspace);
    } else {
        cb_update_background_tab(panel->wnck_window, panel);
    }

    /* update the frame name to since we changed to/from single workspace mode */
    xfdesktop_settings_update_iconview_frame_name(panel, wnck_window_get_workspace(panel->wnck_window));
}

static void
xfdesktop_settings_setup_image_iconview(AppearancePanel *panel)
{
    GtkIconView *iconview = GTK_ICON_VIEW(panel->image_iconview);
    GList *cells;

    TRACE("entering");

    panel->thumbnailer = xfdesktop_thumbnailer_new();

    g_object_set(G_OBJECT(iconview),
                "pixbuf-column", COL_PIX,
                "tooltip-column", COL_NAME,
                "selection-mode", GTK_SELECTION_BROWSE,
                "column-spacing", 1,
                "row-spacing", 1,
                "item-padding", 10,
                "margin", 2,
                NULL);

    cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(iconview));
    for (GList *l = cells; l != NULL; l = l->next) {
        if (GTK_IS_CELL_RENDERER_PIXBUF(l->data)) {
            gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(iconview),
                                           GTK_CELL_RENDERER(l->data),
                                           "surface", COL_SURFACE,
                                           NULL);
            break;
        }
    }
    g_list_free(cells);

    g_signal_connect(G_OBJECT(iconview), "selection-changed",
                     G_CALLBACK(cb_image_selection_changed), panel);

    g_signal_connect(panel->thumbnailer, "thumbnail-ready",
                     G_CALLBACK(cb_thumbnail_ready), panel);
}

static void
cb_xfdesktop_icon_orientation_changed(GtkComboBox *combo,
                                      gpointer user_data)
{
    const gchar *cmd = "xfdesktop --arrange";
    /* TRANSLATORS: Please split the message in half with '\n' so the dialog will not be too wide. */
    const gchar *question = _("Would you like to arrange all existing\n"
                              "icons according to the selected orientation?");
    GError *error = NULL;

    GtkWindow *window = GTK_WINDOW (gtk_widget_get_toplevel(GTK_WIDGET(combo)));

    if(!xfce_dialog_confirm(window, "view-sort-ascending", _("Arrange icons"),
                            NULL, "%s", question))
    {
        return;
    }

    if(!g_spawn_command_line_async(cmd, &error))
    {
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"), cmd);
        xfce_message_dialog(window, _("Launch Error"),
                            "dialog-error", primary, error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"),
                            GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_clear_error(&error);
    }
}

static void
xfdesktop_settings_dialog_setup_tabs(GtkBuilder *main_gxml,
                                     AppearancePanel *panel)
{
    GtkWidget *appearance_container, *chk_custom_font_size,
              *spin_font_size, *w, *box, *spin_icon_size,
              *chk_show_thumbnails, *chk_single_click, *appearance_settings,
              *chk_show_tooltips, *spin_tooltip_size, *bnt_exit, *content_area,
              *chk_show_hidden_files;
    GtkBuilder *appearance_gxml;
    GError *error = NULL;
    GtkFileFilter *filter;
    GdkScreen *screen;
    WnckScreen *wnck_screen;
    XfconfChannel *channel = panel->channel;
    const gchar *path;
    GFile *file;
    gchar *uri_path;

    TRACE("entering");

    appearance_container = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                             "notebook_screens"));

    bnt_exit = GTK_WIDGET(gtk_builder_get_object(main_gxml, "bnt_exit"));

    g_signal_connect(G_OBJECT(bnt_exit), "clicked",
                     G_CALLBACK(cb_xfdesktop_bnt_exit_clicked),
                     panel);

    /* Icons tab */
    /* icon size */
    spin_icon_size = GTK_WIDGET(gtk_builder_get_object(main_gxml, "spin_icon_size"));

    g_signal_connect(G_OBJECT(spin_icon_size), "value-changed",
                     G_CALLBACK(cb_xfdesktop_spin_icon_size_changed),
                     channel);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_icon_size),
                              xfconf_channel_get_uint(channel,
                                                      DESKTOP_ICONS_ICON_SIZE_PROP,
                                                      DEFAULT_ICON_SIZE));

    /* font size */
    chk_custom_font_size = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                             "chk_custom_font_size"));
    spin_font_size = GTK_WIDGET(gtk_builder_get_object(main_gxml, "spin_font_size"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_font_size), 12);

    /* single click */
    chk_single_click = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                         "chk_single_click"));

    /* show hidden files */
    chk_show_hidden_files = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                              "chk_show_hidden_files"));

    /* tooltip options */
    chk_show_tooltips = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_tooltips"));
    spin_tooltip_size = GTK_WIDGET(gtk_builder_get_object(main_gxml, "spin_tooltip_size"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_tooltip_size), 64);

    /* connect up the signals */
    g_signal_connect(G_OBJECT(chk_custom_font_size), "toggled",
                     G_CALLBACK(cb_xfdesktop_chk_button_toggled),
                     spin_font_size);

    g_signal_connect(G_OBJECT(chk_show_tooltips), "toggled",
                     G_CALLBACK(cb_xfdesktop_chk_button_toggled),
                     spin_tooltip_size);

    /* thumbnails */
    chk_show_thumbnails = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                            "chk_show_thumbnails"));
    /* The default value when this property is not set, is 'TRUE'.
     * the bind operation defaults to 'FALSE' for unset boolean properties.
     *
     * Make the checkbox correspond to the default behaviour.
     */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(chk_show_thumbnails),
                                  TRUE);

    screen = gtk_widget_get_screen(appearance_container);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    wnck_screen = wnck_screen_get(panel->screen);
G_GNUC_END_IGNORE_DEPRECATIONS

    /* watch for workspace changes */
    g_signal_connect(wnck_screen, "workspace-created",
                     G_CALLBACK(cb_workspace_count_changed), panel);
    g_signal_connect(wnck_screen, "workspace-destroyed",
                     G_CALLBACK(cb_workspace_count_changed), panel);
    g_signal_connect(wnck_screen, "active-workspace-changed",
                     G_CALLBACK(cb_workspace_changed), panel);

    /* watch for window-opened so we can find our window and track it's changes */
    g_signal_connect(wnck_screen, "window-opened",
                     G_CALLBACK(cb_window_opened), panel);

    /* watch for monitor changes */
    g_signal_connect(G_OBJECT(screen), "monitors-changed",
                     G_CALLBACK(cb_monitor_changed), panel);

    appearance_gxml = gtk_builder_new();
    if(!gtk_builder_add_from_string(appearance_gxml,
                                    xfdesktop_settings_appearance_frame_ui,
                                    xfdesktop_settings_appearance_frame_ui_length,
                                    &error))
    {
        g_printerr("Failed to parse appearance settings UI description: %s\n",
                   error->message);
        g_clear_error(&error);
        exit(1);
    }

    appearance_settings = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                            "alignment_settings"));

    /* Add the background tab widgets to the main window and don't display the
     * notebook label/tab */
    gtk_notebook_append_page(GTK_NOTEBOOK(appearance_container),
                             appearance_settings, NULL);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(appearance_container), FALSE);

    /* icon view area */
    panel->infobar = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                       "infobar_header"));

    panel->infobar_label = gtk_label_new("This is some text");
    gtk_widget_show(panel->infobar_label);
    gtk_widget_show(panel->infobar);

    /* Add the panel's infobar label to the infobar, with this setup
     * it's easy to update the text for the infobar. */
    content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(panel->infobar));
    gtk_container_add(GTK_CONTAINER(content_area), panel->infobar_label);

    panel->label_header = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                            "label_header"));

    panel->image_iconview = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                              "iconview_imagelist"));
    xfdesktop_settings_setup_image_iconview(panel);

    /* folder: file chooser button */
    panel->btn_folder = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "btn_folder"));
    g_signal_connect(G_OBJECT(panel->btn_folder), "selection-changed",
                     G_CALLBACK(cb_folder_selection_changed), panel);
    panel->btn_folder_apply = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "btn_folder_apply"));
    g_signal_connect(G_OBJECT(panel->btn_folder_apply), "clicked",
                     G_CALLBACK(cb_folder_apply_clicked), panel);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Image files"));
    gtk_file_filter_add_pixbuf_formats(filter);
    gtk_file_filter_add_mime_type(filter, "inode/directory");
    gtk_file_filter_add_mime_type(filter, "application/x-directory");
    gtk_file_filter_add_mime_type(filter, "text/directory");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(panel->btn_folder), filter);

    /* Change the title of the file chooser dialog */
    gtk_file_chooser_button_set_title(GTK_FILE_CHOOSER_BUTTON(panel->btn_folder), _("Select a Directory"));

    /* Get default wallpaper folder */
    path = system_data_lookup ();

    if (path != NULL) {
        file = g_file_new_for_path (path);
        uri_path = g_file_get_uri (file);

        gtk_file_chooser_add_shortcut_folder_uri (GTK_FILE_CHOOSER(panel->btn_folder),
                                                  uri_path, NULL);

        g_free (uri_path);
        g_object_unref (file);
    }

    /* Image and color style options */
    panel->image_style_combo = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "combo_style"));

    panel->color_style_combo = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                 "combo_colors"));

    panel->color1_btn = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                          "color1_btn"));

    panel->color2_btn = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                          "color2_btn"));

    g_signal_connect(G_OBJECT(panel->image_style_combo), "changed",
                     G_CALLBACK(cb_xfdesktop_combo_image_style_changed),
                     panel);

    g_signal_connect(G_OBJECT(panel->color_style_combo), "changed",
                     G_CALLBACK(cb_xfdesktop_combo_color_changed),
                     panel);

    /* Pick the first entries so something shows up */
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->image_style_combo), XFCE_BACKDROP_IMAGE_ZOOMED);
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->color_style_combo), XFCE_BACKDROP_COLOR_SOLID);

    /* Use these settings for all workspaces checkbox */
    panel->chk_apply_to_all =  GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                 "chk_apply_to_all"));

    if(xfconf_channel_get_bool(channel, SINGLE_WORKSPACE_MODE, TRUE)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all), TRUE);
    }

    g_signal_connect(G_OBJECT(panel->chk_apply_to_all), "toggled",
                    G_CALLBACK(cb_xfdesktop_chk_apply_to_all),
                    panel);

    /* background cycle timer */
    panel->backdrop_cycle_chkbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                     "chk_cycle_backdrop"));
    panel->combo_backdrop_cycle_period = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                           "combo_cycle_backdrop_period"));
    panel->backdrop_cycle_spinbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                     "spin_backdrop_time"));
    panel->random_backdrop_order_chkbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                     "chk_random_backdrop_order"));

    /* Pick the first entry so something shows up */
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->combo_backdrop_cycle_period), XFCE_BACKDROP_PERIOD_MINUTES);

    g_signal_connect(G_OBJECT(panel->backdrop_cycle_chkbox), "toggled",
                    G_CALLBACK(cb_xfdesktop_chk_cycle_backdrop_toggled),
                    panel);

    g_signal_connect(G_OBJECT(panel->combo_backdrop_cycle_period), "changed",
                     G_CALLBACK(cb_combo_backdrop_cycle_period_change),
                     panel);

    g_object_unref(G_OBJECT(appearance_gxml));


    /* Menus Tab */
    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_delete_option"));
    xfconf_g_property_bind(channel, DESKTOP_MENU_DELETE, G_TYPE_BOOLEAN,
                           G_OBJECT(w), "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_desktop_menu"));
    xfconf_g_property_bind(channel, SHOW_DESKTOP_MENU_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(w), "active");
    box = GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_menu_subopts"));
    g_signal_connect(G_OBJECT(w), "toggled",
                     G_CALLBACK(suboptions_set_sensitive), box);
    suboptions_set_sensitive(GTK_TOGGLE_BUTTON(w), box);

    xfconf_g_property_bind(channel, DESKTOP_MENU_SHOW_ICONS_PROP,
                           G_TYPE_BOOLEAN,
                           G_OBJECT(GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                                      "chk_menu_show_app_icons"))),
                           "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_winlist_menu"));
    xfconf_g_property_bind(channel, WINLIST_SHOW_WINDOWS_MENU_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(w), "active");
    box = GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_winlist_subopts"));
    g_signal_connect(G_OBJECT(w), "toggled",
                     G_CALLBACK(suboptions_set_sensitive), box);
    suboptions_set_sensitive(GTK_TOGGLE_BUTTON(w), box);

    xfconf_g_property_bind(channel, WINLIST_SHOW_APP_ICONS_PROP, G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_winlist_show_app_icons"),
                           "active");

    xfconf_g_property_bind(channel, WINLIST_SHOW_STICKY_WIN_ONCE_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_show_winlist_sticky_once"),
                           "active");

    xfconf_g_property_bind(channel, WINLIST_SHOW_ADD_REMOVE_WORKSPACES_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_show_app_remove_workspaces"),
                           "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_winlist_ws_names"));
    xfconf_g_property_bind(channel, WINLIST_SHOW_WS_NAMES_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(w), "active");
    box = GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_winlist_names_subopts"));
    g_signal_connect(G_OBJECT(w), "toggled",
                     G_CALLBACK(suboptions_set_sensitive), box);
    suboptions_set_sensitive(GTK_TOGGLE_BUTTON(w), box);

    xfconf_g_property_bind(channel, WINLIST_SHOW_WS_SUBMENUS_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_show_winlist_ws_submenus"),
                           "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "primary"));
    xfconf_g_property_bind(channel, DESKTOP_ICONS_ON_PRIMARY_PROP, G_TYPE_BOOLEAN,
                          G_OBJECT(w), "active");
    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "combo_icons"));
#ifdef ENABLE_FILE_ICONS
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), 2);
#else
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), 1);
#endif
    xfconf_g_property_bind(channel, DESKTOP_ICONS_STYLE_PROP, G_TYPE_INT,
                           G_OBJECT(w), "active");

    /* Orientation combo */
    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "combo_orientation"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
    xfconf_g_property_bind(channel, DESKTOP_ICONS_GRAVITY_PROP, G_TYPE_INT,
                           G_OBJECT(w), "active");
    g_signal_connect(G_OBJECT(w), "changed",
                     G_CALLBACK(cb_xfdesktop_icon_orientation_changed), NULL);

    /* bindings */
    xfconf_g_property_bind(channel, DESKTOP_ICONS_FONT_SIZE_PROP, G_TYPE_DOUBLE,
                           G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin_font_size))),
                           "value");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_custom_font_size),
                           "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_TOOLTIP_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_tooltips),
                           "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_TOOLTIP_SIZE_PROP, G_TYPE_DOUBLE,
                           G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin_tooltip_size))),
                           "value");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_HIDDEN_FILES,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_hidden_files),
                           "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_THUMBNAILS,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_thumbnails),
                           "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SINGLE_CLICK_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_single_click),
                           "active");

    setup_special_icon_list(main_gxml, channel);
    cb_update_background_tab(panel->wnck_window, panel);
}

static void
xfdesktop_settings_response(GtkWidget *dialog, gint response_id, gpointer user_data)
{
    if(response_id == GTK_RESPONSE_HELP) {
        xfce_dialog_show_help_with_version(GTK_WINDOW(dialog),
                                           "xfdesktop",
                                           "start",
                                           NULL,
                                           XFDESKTOP_VERSION_SHORT);
    } else {
        XfconfChannel *channel = (XfconfChannel*) user_data;
        GdkWindowState state;
        gint width, height;

        /* don't save the state for full-screen windows */
        state = gdk_window_get_state(gtk_widget_get_window(dialog));

        if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0) {
            /* save window size */
            gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);
            xfconf_channel_set_int(channel, SETTINGS_WINDOW_LAST_WIDTH, width);
            xfconf_channel_set_int(channel, SETTINGS_WINDOW_LAST_HEIGHT, height);
        }

        gtk_main_quit();
    }
}

static Window opt_socket_id = 0;
static gboolean opt_version = FALSE;
static gboolean opt_enable_debug = FALSE;
static GOptionEntry option_entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "enable-debug", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_enable_debug, N_("Enable debug messages"), NULL },
    { NULL, },
};

int
main(int argc, char **argv)
{
    XfconfChannel *channel;
    GtkBuilder *gxml;
    gint screen;
    GError *error = NULL;
    AppearancePanel *panel;

#ifdef G_ENABLE_DEBUG
    /* do NOT remove this line. If something doesn't work,
     * fix your code instead! */
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args(&argc, &argv, "", option_entries, PACKAGE, &error)) {
        if(G_LIKELY(error)) {
            g_printerr("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr("\n");
            g_clear_error(&error);
        } else
            g_error("Unable to open display.");

        return EXIT_FAILURE;
    }

    if(G_UNLIKELY(opt_version)) {
        g_print("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION, xfce_version_string());
        g_print("%s\n", "Copyright (c) 2004-2022");
        g_print("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");

        return EXIT_SUCCESS;
    }

    if(!xfconf_init(&error)) {
        xfce_message_dialog(NULL, _("Desktop Settings"),
                            "dialog-error",
                            _("Unable to contact settings server"),
                            error->message,
                            XFCE_BUTTON_TYPE_MIXED, "application-exit", _("Quit"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_clear_error(&error);
        return 1;
    }


    gxml = gtk_builder_new();
    if(!gtk_builder_add_from_string(gxml, xfdesktop_settings_ui,
                                    xfdesktop_settings_ui_length,
                                    &error))
    {
        g_printerr("Failed to parse UI description: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    channel = xfconf_channel_new(XFDESKTOP_CHANNEL);
    panel = g_new0(AppearancePanel, 1);

    if(opt_enable_debug)
        xfdesktop_debug_set(TRUE);

    if(opt_socket_id == 0) {
        GtkWidget *dialog;
        dialog = GTK_WIDGET(gtk_builder_get_object(gxml, "prefs_dialog"));
        g_signal_connect(dialog, "response",
                         G_CALLBACK(xfdesktop_settings_response),
                         channel);
        gtk_window_set_default_size
            (GTK_WINDOW(dialog),
             xfconf_channel_get_int(channel, SETTINGS_WINDOW_LAST_WIDTH, -1),
             xfconf_channel_get_int(channel, SETTINGS_WINDOW_LAST_HEIGHT, -1));
        gtk_window_present(GTK_WINDOW (dialog));

        screen = XScreenNumberOfScreen(gdk_x11_screen_get_xscreen(gtk_widget_get_screen(dialog)));

        /* To prevent the settings dialog to be saved in the session */
        gdk_x11_set_sm_client_id("FAKE ID");

    } else {
        GtkWidget *plug, *plug_child;
        WnckScreen *wnck_screen;

        plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show(plug);
        g_signal_connect(G_OBJECT(plug), "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);

        gdk_notify_startup_complete();

        plug_child = GTK_WIDGET(gtk_builder_get_object(gxml, "alignment1"));
        xfce_widget_reparent(plug_child, plug);
        gtk_widget_show(plug_child);

        screen = XScreenNumberOfScreen(gdk_x11_screen_get_xscreen(gtk_widget_get_screen(plug)));

        /* In a GtkPlug setting there isn't an easy way to find our window
         * in cb_window_opened so we'll just force wnck to init and get the
         * active window */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        wnck_screen = wnck_screen_get(screen);
G_GNUC_END_IGNORE_DEPRECATIONS
        wnck_screen_force_update(wnck_screen);
        panel->wnck_window = wnck_screen_get_active_window(wnck_screen);

        /* These callbacks are for updating the image_iconview when the window
         * moves to another monitor or workspace */
        g_signal_connect(panel->wnck_window, "geometry-changed",
                         G_CALLBACK(cb_update_background_tab), panel);
        g_signal_connect(panel->wnck_window, "workspace-changed",
                         G_CALLBACK(cb_update_background_tab), panel);
    }

    panel->channel = channel;
    panel->screen = screen;

    xfdesktop_settings_dialog_setup_tabs(gxml, panel);

    gtk_main();

    g_object_unref(G_OBJECT(gxml));

    g_object_unref(G_OBJECT(channel));
    xfconf_shutdown();

    g_free(panel);

    return 0;
}
