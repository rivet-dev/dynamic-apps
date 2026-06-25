/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006      Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfce-desktop.h"
#include "xfdesktop-clipboard-manager.h"
#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-icon-manager.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"
#include "xfdesktop-thumbnailer.h"

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#define SAVE_DELAY  1000
#define BORDER         8

typedef enum
{
    PROP0 = 0,
    PROP_FOLDER,
    PROP_SHOW_FILESYSTEM,
    PROP_SHOW_HOME,
    PROP_SHOW_TRASH,
    PROP_SHOW_REMOVABLE,
    PROP_SHOW_NETWORK_VOLUME,
    PROP_SHOW_DEVICE_VOLUME,
    PROP_SHOW_UNKNOWN_VOLUME,
    PROP_SHOW_THUMBNAILS,
    PROP_SHOW_HIDDEN_FILES,
    PROP_SHOW_DELETE_MENU,
    PROP_MAX_TEMPLATES,
} XfdesktopFileIconManagerProp;

typedef enum
{
    HIDDEN_STATE_CHANGED,
    LAST_SIGNAL,
} XfdesktopFileIconManagerSignals;


struct _XfdesktopFileIconManagerPrivate
{
    gboolean inited;

    XfconfChannel *channel;

    GtkWidget *desktop;
    XfdesktopIconView *icon_view;

    GdkScreen *gscreen;

    GFile *folder;
    XfdesktopFileIcon *desktop_icon;
    GFileMonitor *monitor;
    GFileEnumerator *enumerator;

    GVolumeMonitor *volume_monitor;

    GFileMonitor *metadata_monitor;
    guint metadata_timer;

    GHashTable *icons;
    GHashTable *removable_icons;
    GHashTable *special_icons;

    gboolean show_removable_media;
    gboolean show_network_volumes;
    gboolean show_device_volumes;
    gboolean show_unknown_volumes;
    gboolean show_special[XFDESKTOP_SPECIAL_FILE_ICON_TRASH+1];
    gboolean show_thumbnails;
    gboolean show_hidden_files;
    gboolean show_delete_menu;

    guint save_icons_id;

    GQueue *pending_icons;
    guint pending_icons_id;

    GtkTargetList *drag_targets;
    GtkTargetList *drop_targets;

#ifdef HAVE_THUNARX
    GList *thunarx_menu_providers;
    GList *thunarx_properties_providers;
#endif

    XfdesktopThumbnailer *thumbnailer;

    guint max_templates;
    guint templates_count;
};

static void xfdesktop_file_icon_manager_set_property(GObject *object,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_get_property(GObject *object,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_finalize(GObject *obj);
static void xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface);

static gboolean xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                                      XfdesktopIconView *icon_view);
static void xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager);

static gboolean xfdesktop_file_icon_manager_drag_drop(XfdesktopIconViewManager *manager,
                                                      XfdesktopIcon *drop_icon,
                                                      GdkDragContext *context,
                                                      gint16 row,
                                                      gint16 col,
                                                      guint time_);
static void xfdesktop_file_icon_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                                           XfdesktopIcon *drop_icon,
                                                           GdkDragContext *context,
                                                           gint16 row,
                                                           gint16 col,
                                                           GtkSelectionData *data,
                                                           guint info,
                                                           guint time_);
static void xfdesktop_file_icon_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                                      GList *drag_icons,
                                                      GdkDragContext *context,
                                                      GtkSelectionData *data,
                                                      guint info,
                                                      guint time_);
static GdkDragAction xfdesktop_file_icon_manager_propose_drop_action(XfdesktopIconViewManager *manager,
                                                                     XfdesktopIcon *drop_icon,
                                                                     GdkDragAction action,
                                                                     GdkDragContext *context,
                                                                     GtkSelectionData *data,
                                                                     guint info);
static void xfdesktop_file_icon_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                              GtkMenuShell *menu);

static gboolean xfdesktop_file_icon_manager_check_create_desktop_folder(GFile *file);
static void xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager);


static void xfdesktop_file_icon_manager_set_show_special_file(XfdesktopFileIconManager *manager,
                                                              XfdesktopSpecialFileIconType type,
                                                              gboolean show_special_file);
static void xfdesktop_file_icon_manager_set_show_thumbnails(XfdesktopFileIconManager *manager,
                                                            gboolean show_thumbnails);
static void xfdesktop_file_icon_manager_set_show_hidden_files(XfdesktopFileIconManager *manager,
                                                              gboolean show_hidden_files);
static void xfdesktop_file_icon_manager_set_show_removable_media(XfdesktopFileIconManager *manager,
                                                                 XfdesktopFileIconManagerProp prop,
                                                                 gboolean show);

static void xfdesktop_file_icon_manager_set_show_delete_menu(XfdesktopFileIconManager *manager,
                                                             gboolean show_delete_menu);

static void xfdesktop_file_icon_position_changed(XfdesktopFileIcon *icon,
                                                 gpointer user_data);

static void xfdesktop_file_icon_manager_update_image(GtkWidget *widget,
                                                     gchar *srcfile,
                                                     gchar *thumbfile,
                                                     XfdesktopFileIconManager *fmanager);

static void
xfdesktop_file_icon_manager_set_max_templates(XfdesktopFileIconManager *manager,
                                              gint max_templates);


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIconManager,
                       xfdesktop_file_icon_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON_VIEW_MANAGER,
                                             xfdesktop_file_icon_manager_icon_view_manager_init)
                       G_ADD_PRIVATE(XfdesktopFileIconManager))



enum
{
    TARGET_TEXT_URI_LIST = 0,
    TARGET_XDND_DIRECT_SAVE0,
    TARGET_NETSCAPE_URL,
    TARGET_APPLICATION_OCTET_STREAM,
};

static const GtkTargetEntry drag_targets[] = {
    { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};
static const gint n_drag_targets = (sizeof(drag_targets)/sizeof(drag_targets[0]));
static const GtkTargetEntry drop_targets[] = {
    { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
    { "application/octet-stream", 0, TARGET_APPLICATION_OCTET_STREAM, },
    { "XdndDirectSave0", 0, TARGET_XDND_DIRECT_SAVE0, },
    { "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL, },
};
static const gint n_drop_targets = (sizeof(drop_targets)/sizeof(drop_targets[0]));

static XfdesktopClipboardManager *clipboard_manager = NULL;

static GQuark xfdesktop_app_info_quark = 0;

static guint fmanager_signals[LAST_SIGNAL] = { 0, };


static void
xfdesktop_file_icon_manager_class_init(XfdesktopFileIconManagerClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->set_property = xfdesktop_file_icon_manager_set_property;
    gobject_class->get_property = xfdesktop_file_icon_manager_get_property;
    gobject_class->finalize = xfdesktop_file_icon_manager_finalize;

    fmanager_signals[HIDDEN_STATE_CHANGED] = g_signal_new("hidden-state-changed",
                                                          G_OBJECT_CLASS_TYPE(gobject_class),
                                                          G_SIGNAL_RUN_FIRST,
                                                          G_STRUCT_OFFSET(XfdesktopFileIconManagerClass, hidden_state_changed),
                                                          NULL, NULL,
                                                          g_cclosure_marshal_VOID__VOID,
                                                          G_TYPE_NONE, 0);


    g_object_class_install_property(gobject_class, PROP_FOLDER,
                                    g_param_spec_object("folder", "Desktop Folder",
                                                       "Folder this icon manager manages",
                                                       G_TYPE_FILE,
                                                       G_PARAM_READWRITE
                                                       | G_PARAM_CONSTRUCT_ONLY
                                                       | G_PARAM_STATIC_NAME
                                                       | G_PARAM_STATIC_NICK
                                                       | G_PARAM_STATIC_BLURB));

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_CONSTRUCT \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)
    g_object_class_install_property(gobject_class, PROP_SHOW_FILESYSTEM,
                                    g_param_spec_boolean("show-filesystem",
                                                         "show filesystem",
                                                         "show filesystem",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_HOME,
                                    g_param_spec_boolean("show-home",
                                                         "show home",
                                                         "show home",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_TRASH,
                                    g_param_spec_boolean("show-trash",
                                                         "show trash",
                                                         "show trash",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_REMOVABLE,
                                    g_param_spec_boolean("show-removable",
                                                         "show removable",
                                                         "show removable",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_NETWORK_VOLUME,
                                    g_param_spec_boolean("show-network-volume",
                                                         "show network volume",
                                                         "show network volume",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_DEVICE_VOLUME,
                                    g_param_spec_boolean("show-device-volume",
                                                         "show device volume",
                                                         "show device volume",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_UNKNOWN_VOLUME,
                                    g_param_spec_boolean("show-unknown-volume",
                                                         "show unknown volume",
                                                         "show unknown volume",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_THUMBNAILS,
                                    g_param_spec_boolean("show-thumbnails",
                                                         "show-thumbnails",
                                                         "show-thumbnails",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_HIDDEN_FILES,
                                    g_param_spec_boolean("show-hidden-files",
                                                         "show-hidden-files",
                                                         "show-hidden-files",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_DELETE_MENU,
                                    g_param_spec_boolean("show-delete-menu",
                                                         "show-delete-menu",
                                                         "show-delete-menu",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_MAX_TEMPLATES,
                                    g_param_spec_uint("max-templates",
                                                      "max-templates",
                                                      "max-templates",
                                                      0, G_MAXUSHORT, 16,
                                                      XFDESKTOP_PARAM_FLAGS));
#undef XFDESKTOP_PARAM_FLAGS

    xfdesktop_app_info_quark = g_quark_from_static_string("xfdesktop-app-info-quark");
}

static void
xfdesktop_file_icon_manager_init(XfdesktopFileIconManager *fmanager)
{
    fmanager->priv = xfdesktop_file_icon_manager_get_instance_private(fmanager);

    /* be safe */
    fmanager->priv->gscreen = gdk_screen_get_default();
    fmanager->priv->drag_targets = gtk_target_list_new(drag_targets,
                                                       n_drag_targets);
    fmanager->priv->drop_targets = gtk_target_list_new(drop_targets,
                                                       n_drop_targets);

    fmanager->priv->thumbnailer = xfdesktop_thumbnailer_new();

    g_signal_connect(G_OBJECT(fmanager->priv->thumbnailer), "thumbnail-ready", G_CALLBACK(xfdesktop_file_icon_manager_update_image), fmanager);
}

static void
xfdesktop_file_icon_manager_set_property(GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);

    switch(property_id) {
        case PROP_FOLDER:
            fmanager->priv->folder = g_value_dup_object(value);
            xfdesktop_file_icon_manager_check_create_desktop_folder(fmanager->priv->folder);
            break;

        case PROP_SHOW_FILESYSTEM:
            xfdesktop_file_icon_manager_set_show_special_file(fmanager,
                                                              XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_HOME:
            xfdesktop_file_icon_manager_set_show_special_file(fmanager,
                                                              XFDESKTOP_SPECIAL_FILE_ICON_HOME,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_TRASH:
            xfdesktop_file_icon_manager_set_show_special_file(fmanager,
                                                              XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_REMOVABLE:
        case PROP_SHOW_NETWORK_VOLUME:
        case PROP_SHOW_DEVICE_VOLUME:
        case PROP_SHOW_UNKNOWN_VOLUME:
            xfdesktop_file_icon_manager_set_show_removable_media(fmanager,
                                                                 property_id,
                                                                 g_value_get_boolean(value));
            break;

        case PROP_SHOW_THUMBNAILS:
            xfdesktop_file_icon_manager_set_show_thumbnails(fmanager,
                                                            g_value_get_boolean(value));
            break;

        case PROP_SHOW_HIDDEN_FILES:
            xfdesktop_file_icon_manager_set_show_hidden_files(fmanager,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_DELETE_MENU:
            xfdesktop_file_icon_manager_set_show_delete_menu(fmanager,
                                                             g_value_get_boolean(value));
            break;

        case PROP_MAX_TEMPLATES:
            xfdesktop_file_icon_manager_set_max_templates(fmanager,
                                                          g_value_get_uint(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_get_property(GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);

    switch(property_id) {
        case PROP_FOLDER:
            g_value_set_object(value, fmanager->priv->folder);
            break;

        case PROP_SHOW_FILESYSTEM:
            g_value_set_boolean(value,
                                fmanager->priv->show_special[XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM]);
            break;

        case PROP_SHOW_HOME:
            g_value_set_boolean(value,
                                fmanager->priv->show_special[XFDESKTOP_SPECIAL_FILE_ICON_HOME]);
            break;

        case PROP_SHOW_TRASH:
            g_value_set_boolean(value,
                                fmanager->priv->show_special[XFDESKTOP_SPECIAL_FILE_ICON_TRASH]);
            break;

        case PROP_SHOW_REMOVABLE:
            g_value_set_boolean(value, fmanager->priv->show_removable_media);
            break;

        case PROP_SHOW_NETWORK_VOLUME:
            g_value_set_boolean(value, fmanager->priv->show_network_volumes);
            break;

        case PROP_SHOW_DEVICE_VOLUME:
            g_value_set_boolean(value, fmanager->priv->show_device_volumes);
            break;

        case PROP_SHOW_UNKNOWN_VOLUME:
            g_value_set_boolean(value, fmanager->priv->show_unknown_volumes);
            break;

        case PROP_SHOW_THUMBNAILS:
            g_value_set_boolean(value, fmanager->priv->show_thumbnails);
            break;

        case PROP_SHOW_HIDDEN_FILES:
            g_value_set_boolean(value, fmanager->priv->show_hidden_files);
            break;

        case PROP_SHOW_DELETE_MENU:
            g_value_set_boolean(value, fmanager->priv->show_delete_menu);
            break;

        case PROP_MAX_TEMPLATES:
            g_value_set_int(value, fmanager->priv->max_templates);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_finalize(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);

    if(fmanager->priv->inited)
        xfdesktop_file_icon_manager_fini(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));

    g_object_unref(G_OBJECT(fmanager->priv->channel));

    gtk_target_list_unref(fmanager->priv->drag_targets);
    gtk_target_list_unref(fmanager->priv->drop_targets);

    g_object_unref(fmanager->priv->folder);
    g_object_unref(fmanager->priv->thumbnailer);

    if(fmanager->priv->volume_monitor != NULL)
        g_object_unref(fmanager->priv->volume_monitor);

    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface)
{
    iface->manager_init = xfdesktop_file_icon_manager_real_init;
    iface->manager_fini = xfdesktop_file_icon_manager_fini;
    iface->drag_drop = xfdesktop_file_icon_manager_drag_drop;
    iface->drag_data_received = xfdesktop_file_icon_manager_drag_data_received;
    iface->drag_data_get = xfdesktop_file_icon_manager_drag_data_get;
    iface->propose_drop_action = xfdesktop_file_icon_manager_propose_drop_action;
    iface->populate_context_menu = xfdesktop_file_icon_manager_populate_context_menu;
}

static gboolean
xfdesktop_file_icon_manager_check_create_desktop_folder(GFile *folder)
{
    GFileInfo *info;
    GError *error = NULL;
    gboolean result = TRUE;
    gchar *primary;

    g_return_val_if_fail(G_IS_FILE(folder), FALSE);

    info = g_file_query_info(folder, XFDESKTOP_FILE_INFO_NAMESPACE,
                             G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if(info == NULL) {
        if(!g_file_make_directory_with_parents(folder, NULL, &error)) {
            gchar *uri = g_file_get_uri(folder);
            gchar *display_name = g_filename_display_basename(uri);
            primary = g_markup_printf_escaped(_("Could not create the desktop folder \"%s\""),
                                              display_name);
            g_free(display_name);
            g_free(uri);

            xfce_message_dialog(NULL, _("Desktop Folder Error"),
                                "dialog-warning", primary,
                                error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
            g_free(primary);

            result = FALSE;
        }
    } else {
        if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY) {
            gchar *uri = g_file_get_uri(folder);
            gchar *display_name = g_filename_display_basename(uri);
            primary = g_markup_printf_escaped(_("Could not create the desktop folder \"%s\""),
                                              display_name);
            g_free(display_name);
            g_free(uri);

            xfce_message_dialog(NULL, _("Desktop Folder Error"),
                                "dialog-warning", primary,
                                _("A normal file with the same name already exists. "
                                  "Please delete or rename it."),
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
            g_free(primary);

            result = FALSE;
        }
    }

    g_object_unref(info);

    return result;
}


/* icon signal handlers */

static void
xfdesktop_file_icon_menu_executed(GtkWidget *widget,
                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon;
    GList *selected;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_ICON(selected->data);
    g_list_free(selected);

    xfdesktop_icon_activated(icon);
}

static void
xfdesktop_file_icon_menu_open_all(GtkWidget *widget,
                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);

    g_list_foreach(selected, xfdesktop_icon_activated_g_func, NULL);
    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_rename(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected, *iter, *files = NULL;
    GtkWidget *toplevel;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);

    for(iter = selected; iter != NULL; iter = iter->next) {
        icon = XFDESKTOP_FILE_ICON(iter->data);

        /* create a list of GFiles from selected icons that can be renamed */
        if(xfdesktop_file_icon_can_rename_file(icon))
            files = g_list_append(files, xfdesktop_file_icon_peek_file(icon));
    }

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    if(g_list_length(files) == 1) {
        /* rename dialog for a single icon */
        xfdesktop_file_utils_rename_file(g_list_first(files)->data,
                                         fmanager->priv->gscreen,
                                         GTK_WINDOW(toplevel));
    } else if(g_list_length(files) > 1) {
        /* Bulk rename for multiple icons selected */
        GFile *desktop = xfdesktop_file_icon_peek_file(fmanager->priv->desktop_icon);

        xfdesktop_file_utils_bulk_rename(desktop, files,
                                         fmanager->priv->gscreen,
                                         GTK_WINDOW(toplevel));
    } else {
        /* Nothing valid to rename */
        xfce_message_dialog(GTK_WINDOW(toplevel),
                            _("Rename Error"), "dialog-error",
                            _("The files could not be renamed"),
                            _("None of the icons selected support being renamed."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }

    g_list_free(files);
    g_list_free(selected);
}

enum
{
    COL_PIX = 0,
    COL_NAME,
    N_COLS
};

static void
xfdesktop_file_icon_manager_delete_files(XfdesktopFileIconManager *fmanager,
                                         GList *files)
{
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    GList *gfiles = NULL, *lp;

    for(lp = g_list_last(files); lp != NULL; lp = lp->prev)
        gfiles = g_list_prepend(gfiles, xfdesktop_file_icon_peek_file(lp->data));

    xfdesktop_file_utils_unlink_files(gfiles, fmanager->priv->gscreen,
                                      GTK_WINDOW(toplevel));

    g_list_free(gfiles);
}

static gboolean
xfdesktop_file_icon_manager_trash_files(XfdesktopFileIconManager *fmanager,
                                        GList *files)
{
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    GList *gfiles = NULL, *lp;

    for(lp = g_list_last(files); lp != NULL; lp = lp->prev)
        gfiles = g_list_prepend(gfiles, xfdesktop_file_icon_peek_file(lp->data));

    xfdesktop_file_utils_trash_files(gfiles, fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));

    g_list_free(gfiles);
    return TRUE;
}

static void
xfdesktop_file_icon_manager_delete_selected(XfdesktopFileIconManager *fmanager,
                                            gboolean force_delete)
{
    GList *selected, *l;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(!selected)
        return;

    /* remove anybody that's not deletable */
    for(l = selected; l; ) {
        if(!xfdesktop_file_icon_can_delete_file(XFDESKTOP_FILE_ICON(l->data))) {
            GList *next = l->next;

            if(l->prev)
                l->prev->next = l->next;
            else  /* this is the first item; reset |selected| */
                selected = l->next;

            if(l->next)
                l->next->prev = l->prev;

            l->next = l->prev = NULL;
            g_list_free_1(l);

            l = next;
        } else
            l = l->next;
    }

    if(G_UNLIKELY(!selected))
        return;

    /* make sure the icons don't get destroyed while we're working */
    g_list_foreach(selected, xfdesktop_object_ref, NULL);

    if(!force_delete) {
        xfdesktop_file_icon_manager_trash_files(fmanager, selected);
    } else {
        xfdesktop_file_icon_manager_delete_files(fmanager, selected);
    }

    g_list_free_full(selected, g_object_unref);
}

static void
xfdesktop_file_icon_menu_app_info_executed(GtkWidget *widget,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GdkAppLaunchContext *context;
    GAppInfo *app_info;
    GList *files = NULL, *selected;
    GtkWidget *toplevel;
    GError *error = NULL;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        files = g_list_prepend(files, file);
    }
    g_list_free(selected);
    files = g_list_reverse(files);

    /* get the app info related to this menu item */
    app_info = g_object_get_qdata(G_OBJECT(widget), xfdesktop_app_info_quark);
    if(!app_info)
        return;

    /* prepare the launch context and configure its screen */
    context = gdk_display_get_app_launch_context(gtk_widget_get_display(GTK_WIDGET(fmanager->priv->icon_view)));
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    gdk_app_launch_context_set_screen(context, gtk_widget_get_screen(toplevel));

    /* try to launch the application */
    if(!xfdesktop_file_utils_app_info_launch(app_info, fmanager->priv->folder, files,
                                             G_APP_LAUNCH_CONTEXT(context), &error))
    {
        gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
                                                 g_app_info_get_name(app_info));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            "dialog-error", primary, error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_free(primary);
        g_clear_error(&error);
    }
}

static void
xfdesktop_file_icon_menu_open_folder(GtkWidget *widget,
                                     gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    GFile *file;
    GtkWidget *toplevel;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);

    file = xfdesktop_file_icon_peek_file(icon);

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    xfdesktop_file_utils_open_folder(file, fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_open_desktop(GtkWidget *widget,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon = fmanager->priv->desktop_icon;
    GFile *file;
    GtkWidget *toplevel;

    file = xfdesktop_file_icon_peek_file(icon);
    if(!file)
        return;

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    xfdesktop_file_utils_open_folder(file, fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget,
                                   gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GtkWidget *toplevel;
    GList *selected;
    GFile *file;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    file = xfdesktop_file_icon_peek_file(icon);

    xfdesktop_file_utils_display_app_chooser_dialog(file, TRUE, FALSE,
                                                    fmanager->priv->gscreen,
                                                    GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_set_default_app(GtkWidget *widget,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GtkWidget *toplevel;
    GList *selected;
    GFile *file;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    file = xfdesktop_file_icon_peek_file(icon);

    xfdesktop_file_utils_display_app_chooser_dialog(file, TRUE, TRUE,
                                                fmanager->priv->gscreen,
                                                GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_cut(GtkWidget *widget,
                             gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *files;

    files = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(!files)
        return;

    xfdesktop_clipboard_manager_cut_files(clipboard_manager, files);

    g_list_free(files);
}

static void
xfdesktop_file_icon_menu_copy(GtkWidget *widget,
                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *files;

    files = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(!files)
        return;

    xfdesktop_clipboard_manager_copy_files(clipboard_manager, files);

    g_list_free(files);
}

static void
xfdesktop_file_icon_menu_trash(GtkWidget *widget,
                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    xfdesktop_file_icon_manager_delete_selected(fmanager, FALSE);
}

static void
xfdesktop_file_icon_menu_delete(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    xfdesktop_file_icon_manager_delete_selected(fmanager, TRUE);
}

static void
xfdesktop_file_icon_menu_paste(GtkWidget *widget,
                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    if(widget && fmanager)
        xfdesktop_clipboard_manager_paste_files(clipboard_manager, fmanager->priv->folder, widget, NULL);
}

static void
xfdesktop_file_icon_menu_paste_into_folder(GtkWidget *widget,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GFileInfo *info;
    GFile *file;
    GList *selected;

    if(!fmanager || !XFDESKTOP_IS_FILE_ICON_MANAGER(fmanager))
        return;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);

    info = xfdesktop_file_icon_peek_file_info(icon);

    if(info == NULL || g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
        return;

    file = xfdesktop_file_icon_peek_file(icon);

    if(widget)
        xfdesktop_clipboard_manager_paste_files(clipboard_manager, file, widget, NULL);
}

static void
xfdesktop_file_icon_menu_arrange_icons(GtkWidget *widget,
                                       gpointer user_data)
{

    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget                *window;
    const gchar              *question = _("This will reorder all desktop items and place them on different screen positions.\n"
                                           "Are you sure?");

    window = gtk_widget_get_toplevel(fmanager->priv->desktop);
    if(xfce_dialog_confirm(GTK_WINDOW(window), NULL, _("_OK"), NULL, "%s", question))
        xfdesktop_icon_view_sort_icons(fmanager->priv->icon_view);
}

static void
xfdesktop_file_icon_menu_next_background(GtkWidget *widget,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    xfce_desktop_refresh(XFCE_DESKTOP(fmanager->priv->desktop), TRUE, FALSE);
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget,
                                    gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    GList *selected_files = NULL;

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        selected_files = g_list_prepend(selected_files, file);
    }
    g_list_free(selected);
    selected_files = g_list_reverse(selected_files);

    if (selected_files != NULL) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        xfdesktop_file_utils_show_properties_dialog(selected_files,
                                                    fmanager->priv->gscreen,
                                                    GTK_WINDOW(toplevel));
        g_list_free(selected_files);
    }
}

static void
xfdesktop_file_icon_manager_desktop_properties(GtkWidget *widget,
                                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    GFile *file = xfdesktop_file_icon_peek_file (fmanager->priv->desktop_icon);
    GList file_l = {
        .data = file,
        .next = NULL,
    };

    xfdesktop_file_utils_show_properties_dialog(&file_l, fmanager->priv->gscreen,
                                                GTK_WINDOW(parent));
}

static GtkWidget *
xfdesktop_menu_item_from_app_info(XfdesktopFileIconManager *fmanager,
                                  XfdesktopFileIcon *icon,
                                  GAppInfo *app_info,
                                  gboolean with_mnemonic,
                                  gboolean with_title_prefix)
{
    GtkWidget *mi, *img;
    gchar *title;
    GIcon *gicon;

    if(!with_title_prefix)
        title = g_strdup(g_app_info_get_name(app_info));
    else if(with_mnemonic) {
        title = g_strdup_printf(_("_Open With \"%s\""),
                                g_app_info_get_name(app_info));
    } else {
        title = g_strdup_printf(_("Open With \"%s\""),
                                g_app_info_get_name(app_info));
    }

    gicon = g_app_info_get_icon(app_info);
    img = gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_MENU);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 16);

    if(with_mnemonic)
        mi = xfdesktop_menu_create_menu_item_with_mnemonic(title, img);
    else
        mi = xfdesktop_menu_create_menu_item_with_markup(title, img);
    g_free(title);

    g_object_set_qdata_full(G_OBJECT(mi), xfdesktop_app_info_quark,
                            g_object_ref(app_info), g_object_unref);

    gtk_widget_show(mi);

    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_app_info_executed),
                     fmanager);

    return mi;
}

static gboolean
xfdesktop_file_icon_menu_free_icon_list_idled(gpointer user_data)
{
    GList *icon_list = user_data;

    g_list_free_full(icon_list, g_object_unref);

    return FALSE;
}

static void
xfdesktop_file_icon_menu_free_icon_list(GtkMenu *menu,
                                        gpointer user_data)
{
    g_idle_add(xfdesktop_file_icon_menu_free_icon_list_idled, user_data);
}

static void
xfdesktop_file_icon_menu_create_launcher(GtkWidget *widget,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GFile *file;
    gchar *cmd = NULL, *uri = NULL, *display_name;
    GError *error = NULL;

    display_name = g_strdup (gdk_display_get_name (gdk_screen_get_display (fmanager->priv->gscreen)));

    file = g_object_get_data(G_OBJECT(widget), "file");

    if(file) {
        uri = g_file_get_uri(file);
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" \"%s\"",
                              display_name, uri);
    } else {
        const gchar *type = g_object_get_data(G_OBJECT(widget), "xfdesktop-launcher-type");
        uri = g_file_get_uri(fmanager->priv->folder);
        if(G_UNLIKELY(!type))
            type = "Application";
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" --create-new --type %s \"%s\"",
                              display_name, type, uri);
    }

    if (!xfce_spawn_command_line(fmanager->priv->gscreen, cmd, FALSE, FALSE, TRUE, &error)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            "dialog-error",
                            _("Unable to launch \"exo-desktop-item-edit\", which is required to create and edit launchers and links on the desktop."),
                            error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }

    g_free(display_name);
    g_free(uri);
    g_free(cmd);
    g_clear_error(&error);

}

static void
xfdesktop_file_icon_menu_create_folder(GtkWidget *widget,
                                       gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel;

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    xfdesktop_file_utils_create_file(fmanager->priv->folder, "inode/directory",
                                     fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_template_item_activated(GtkWidget *mi,
                                            gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel;
    GFile *file = g_object_get_data(G_OBJECT(mi), "file");

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    if(file) {
        xfdesktop_file_utils_create_file_from_template(fmanager->priv->folder, file,
                                                       fmanager->priv->gscreen,
                                                        GTK_WINDOW(toplevel));
    } else {
        xfdesktop_file_utils_create_file(fmanager->priv->folder, "text/plain",
                                         fmanager->priv->gscreen,
                                         GTK_WINDOW(toplevel));
    }
}

static gint
compare_template_files(gconstpointer a,
                       gconstpointer b)
{
    GFileInfo *info_a = g_object_get_data(G_OBJECT(a), "info");
    GFileInfo *info_b = g_object_get_data(G_OBJECT(b), "info");
    GFileType type_a = g_file_info_get_file_type(info_a);
    GFileType type_b = g_file_info_get_file_type(info_b);
    const gchar* name_a = g_file_info_get_display_name(info_a);
    const gchar* name_b = g_file_info_get_display_name(info_b);

    if(!info_a || !info_b)
        return 0;

    if(type_a == type_b) {
        return g_strcmp0(name_a, name_b);
    } else {
        if(type_a == G_FILE_TYPE_DIRECTORY)
            return -1;
        else
            return 1;
    }
}

static void
xfdesktop_file_icon_menu_fill_template_menu(GtkWidget *menu,
                                            GFile *template_dir,
                                            XfdesktopFileIconManager *fmanager,
                                            gboolean recursive)
{
    GFileEnumerator *enumerator;
    GtkWidget *item, *image, *submenu = NULL;
    GFileInfo *info;
    GFile *file;
    GIcon *icon;
    GList *files = NULL, *lp;
    gchar *label, *dot;

    g_return_if_fail(G_IS_FILE(template_dir));

    enumerator = g_file_enumerate_children(template_dir,
                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                           G_FILE_QUERY_INFO_NONE,
                                           NULL, NULL);

    if(enumerator == NULL)
        return;

    if(recursive == FALSE)
        fmanager->priv->templates_count = 0;

    /* keep it under fmanager->priv->max_templates otherwise the menu
     * could have tons of items and be unusable. Additionally this should
     * help in instances where the XDG_TEMPLATES_DIR has a large number of
     * files in it. */
    while((info = g_file_enumerator_next_file(enumerator, NULL, NULL))
          && fmanager->priv->templates_count < fmanager->priv->max_templates)
    {
        /* skip hidden & backup files */
        if(g_file_info_get_is_hidden(info) || g_file_info_get_is_backup(info)) {
            g_object_unref(info);
            continue;
        }

        file = g_file_get_child(template_dir, g_file_info_get_name(info));
        g_object_set_data_full(G_OBJECT(file), "info", info, g_object_unref);
        files = g_list_prepend(files, file);

        if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
            fmanager->priv->templates_count++;
    }

    g_object_unref(enumerator);

    files = g_list_sort(files, compare_template_files);

    for(lp = files; lp != NULL; lp = lp->next) {
        file = lp->data;
        info = g_object_get_data(G_OBJECT(file), "info");

        /* create and fill template submenu */
        if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            submenu = gtk_menu_new();

            xfdesktop_file_icon_menu_fill_template_menu(submenu, file,
                                                        fmanager, TRUE);

            if(!gtk_container_get_children((GtkContainer*) submenu)) {
                g_object_unref(file);
                continue;
            }
        }

        /* generate a label by stripping off the extension */
        label = g_strdup(g_file_info_get_display_name(info));
        dot = g_utf8_strrchr(label, -1, '.');
        if(dot)
            *dot = '\0';

        /* determine the icon to display */
        icon = g_file_info_get_icon(info);
        image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);

        /* allocate a new menu item */
        item = xfdesktop_menu_create_menu_item_with_markup(label, image);
        g_free(label);

        /* add the item to the menu */
        gtk_widget_show(item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

        if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        else {
            g_object_set_data_full(G_OBJECT(item), "file",
                                   g_object_ref(file), g_object_unref);

            g_signal_connect(G_OBJECT(item), "activate",
                             G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                             fmanager);
        }

        g_object_unref(file);
    }

    g_list_free(files);

    return;
}

#ifdef HAVE_THUNARX
static void
thunarx_action_callback (GtkAction *action,
                         gpointer callback_data)
{
    thunarx_menu_item_activate (THUNARX_MENU_ITEM (callback_data));
}



static GtkWidget*
xfdesktop_menu_create_menu_item_from_thunarx_menu_item (GObject *item)
{
    gchar *label, *icon_str;
    GtkWidget *mi, *img = NULL;

    g_return_val_if_fail (THUNARX_IS_MENU_ITEM (item), NULL);

    g_object_get (G_OBJECT (item),
                  "label", &label,
                  "icon", &icon_str,
                  NULL);

    img = gtk_image_new_from_icon_name (icon_str == NULL ? "" : icon_str,
                                        GTK_ICON_SIZE_MENU);

    mi = xfdesktop_menu_create_menu_item_with_mnemonic (label, img);

    g_signal_connect_data (mi, "activate",
                           G_CALLBACK (thunarx_action_callback),
                           g_object_ref (item),
                           xfdesktop_object_unref, 0);

    g_free (label);
    g_free (icon_str);

    return mi;
}



static inline void
xfdesktop_menu_shell_append_thunarx_menu_item_list(GtkMenuShell *menu_shell,
                                                   GList *items)
{
    GList *l;
    GtkWidget *mi;

    for(l = items; l; l = l->next) {
        mi = xfdesktop_menu_create_menu_item_from_thunarx_menu_item (l->data);
        gtk_widget_show(mi);
        gtk_menu_shell_append(menu_shell, mi);
    }
}
#endif

static void
xfdesktop_settings_launch(GtkWidget *w,
                          gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar *cmd;
    GError *error = NULL;

    cmd = g_find_program_in_path("xfdesktop-settings");
    if(!cmd)
        cmd = g_strdup(BINDIR "/xfdesktop-settings");

    if (!xfce_spawn_command_line(fmanager->priv->gscreen, cmd, FALSE, TRUE, TRUE, &error)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        /* printf is to be translator-friendly */
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"), cmd);
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            "dialog-error", primary, error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_free(primary);
        g_clear_error(&error);
    }

    g_free(cmd);
}

static void
xfdesktop_file_icon_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                  GtkMenuShell *menu)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    XfdesktopFileIcon *file_icon = NULL;
    GFileInfo *info = NULL;
    GList *selected;
    GtkWidget *mi, *img, *tmpl_menu;
    gboolean multi_sel, multi_sel_special = FALSE, got_custom_menu = FALSE;
    GFile *templates_dir = NULL, *home_dir;
    const gchar *templates_dir_path = NULL;
#ifdef HAVE_THUNARX
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
#endif

    TRACE("ENTERING");

    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(!selected) {
        /* assume click on the desktop itself */
        selected = g_list_append(selected, fmanager->priv->desktop_icon);
    }

    file_icon = selected->data;
    info = xfdesktop_file_icon_peek_file_info(file_icon);

    multi_sel = (g_list_length(selected) > 1);

    if(multi_sel) {
        /* check if special icons are selected */
        for(GList *l = selected; l != NULL; l = l->next) {
            if(XFDESKTOP_IS_SPECIAL_FILE_ICON(l->data)
               || XFDESKTOP_IS_VOLUME_ICON(l->data))
            {
                multi_sel_special = TRUE;
                break;
            }
        }
    }

    if(!multi_sel) {
        got_custom_menu = xfdesktop_icon_populate_context_menu(XFDESKTOP_ICON(selected->data),
                                                               GTK_WIDGET(menu));
    }

    /* make sure icons don't get destroyed while menu is open */
    g_list_foreach(selected, xfdesktop_object_ref, NULL);
    g_object_set_data(G_OBJECT(menu), "--xfdesktop-icon-list", selected);
    g_signal_connect(G_OBJECT(menu), "deactivate",
                     G_CALLBACK(xfdesktop_file_icon_menu_free_icon_list),
                     selected);

    if(!got_custom_menu) {
        gboolean same_app_infos = FALSE;

        if (!multi_sel) {
            same_app_infos = TRUE;
        } else if (info != NULL && g_file_info_get_content_type(info) != NULL) {
            GList *target_app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(info));

            same_app_infos = TRUE;
            for (GList *l = selected->next; l != NULL; l = l->next) {
                XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
                GFileInfo *icon_info = xfdesktop_file_icon_peek_file_info(icon);

                if (icon_info != NULL) {
                    if (!XFDESKTOP_IS_REGULAR_FILE_ICON(icon)) {
                        same_app_infos = FALSE;
                        break;
                    } else if (g_file_info_get_content_type(icon_info) != NULL) {
                        GList *app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(icon_info));
                        GList *tail, *ail;

                        // TODO: This assumes that the app info entries are in the same order.  So far my minimal
                        // TODO: testing shows that seems to be the case, but not sure if we can rely on that.
                        for (tail = target_app_infos, ail = app_infos;
                             tail != NULL && ail != NULL;
                             tail = tail->next, ail = ail->next)
                        {
                            if (!g_app_info_equal(G_APP_INFO(tail->data), G_APP_INFO(ail->data))) {
                                same_app_infos = FALSE;
                                break;
                            }
                        }

                        g_list_free(app_infos);

                        if (!same_app_infos || tail != NULL || ail != NULL) {
                            same_app_infos = FALSE;
                            break;
                        }
                    } else {
                        same_app_infos = FALSE;
                        break;
                    }
                }
            }

            g_list_free_full(target_app_infos, g_object_unref);
        }

        if (!same_app_infos) {
            img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Open all"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_open_all),
                             fmanager);

            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        } else if(info) {
            if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
                if(file_icon == fmanager->priv->desktop_icon) {
                    /* Menu on the root desktop window */
                    GIcon *icon;

                    /* create launcher item */

                    icon = g_themed_icon_new("application-x-executable");
                    img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Create _Launcher..."), img);
                    g_object_set_data(G_OBJECT(mi), "xfdesktop-launcher-type", "Application");
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    gtk_widget_show(mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                     fmanager);


                    /* create link item */

                    icon = g_themed_icon_new("insert-link");
                    img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Create _URL Link..."), img);
                    g_object_set_data(G_OBJECT(mi), "xfdesktop-launcher-type", "Link");
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    gtk_widget_show(mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                     fmanager);


                    /* create folder item */

                    icon = g_themed_icon_new("folder-new");
                    img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Create _Folder..."), img);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    gtk_widget_show(mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_folder),
                                     fmanager);


                    /* create document submenu, 0 disables the sub-menu */
                    if(fmanager->priv->max_templates > 0) {
                        img = gtk_image_new_from_icon_name("document-new", GTK_ICON_SIZE_MENU);
                        mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Create _Document"), img);
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

                        tmpl_menu = gtk_menu_new();
                        gtk_menu_set_reserve_toggle_size(GTK_MENU(tmpl_menu), FALSE);
                        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), tmpl_menu);

                        /* check if XDG_TEMPLATES_DIR="$HOME" and don't show
                         * templates if so. */
                        home_dir = g_file_new_for_path(xfce_get_homedir());
                        templates_dir_path = g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES);
                        if(templates_dir_path) {
                            templates_dir = g_file_new_for_path(templates_dir_path);
                        }

                        if(templates_dir && !g_file_equal(home_dir, templates_dir)) {
                            xfdesktop_file_icon_menu_fill_template_menu(tmpl_menu,
                                                                        templates_dir,
                                                                        fmanager,
                                                                        FALSE);
                        }

                        if(!gtk_container_get_children((GtkContainer*) tmpl_menu)) {
                            img = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
                            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("No templates installed"), img);
                            gtk_widget_set_sensitive(mi, FALSE);
                            gtk_widget_show(mi);
                            gtk_menu_shell_append(GTK_MENU_SHELL(tmpl_menu), mi);
                        }

                        if(templates_dir)
                            g_object_unref(templates_dir);
                        g_object_unref(home_dir);

                        mi = gtk_separator_menu_item_new();
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(tmpl_menu), mi);

                        /* add the "Empty File" template option */
                        img = gtk_image_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_MENU);
                        mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Empty File"), img);
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(tmpl_menu), mi);
                        g_signal_connect(G_OBJECT(mi), "activate",
                                         G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                                         fmanager);
                    }
                } else {
                    /* Menu on folder icons */
                    img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Open"), img);
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_open_folder),
                                     fmanager);
                }

                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            } else {
                /* Menu on non-folder icons */
                GList *app_infos = NULL;

                if(xfdesktop_file_utils_file_is_executable(info)) {
                    img = gtk_image_new_from_icon_name("system-run", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Execute"), img);
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_executed),
                                     fmanager);

                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

                    if(g_content_type_equals(g_file_info_get_content_type(info),
                                             "application/x-desktop"))
                    {
                        GFile *file = xfdesktop_file_icon_peek_file(file_icon);

                        img = gtk_image_new_from_icon_name("gtk-edit", GTK_ICON_SIZE_MENU);
                        mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Edit Launcher"), img);
                        g_object_set_data_full(G_OBJECT(mi), "file",
                                               g_object_ref(file), g_object_unref);
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                        g_signal_connect(G_OBJECT(mi), "activate",
                                         G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                         fmanager);

                        mi = gtk_separator_menu_item_new();
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    }
                }

                app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(info));
                if(app_infos) {
                    GAppInfo *app_info, *default_application;
                    GtkWidget *app_infos_menu;
                    GList *ap;
                    gint list_len = 0;

                    /* move any default application in front of the list */
                    default_application = g_app_info_get_default_for_type (g_file_info_get_content_type(info), FALSE);
                    if(G_LIKELY (default_application != NULL))
                    {
                        for (ap = app_infos; ap != NULL; ap = ap->next)
                        {
                            if(g_app_info_equal (ap->data, default_application))
                            {
                                g_object_unref (ap->data);
                                app_infos = g_list_delete_link (app_infos, ap);
                                break;
                            }
                        }
                        app_infos = g_list_prepend (app_infos, default_application);
                    }

                    app_info = G_APP_INFO(app_infos->data);

                    mi = xfdesktop_menu_item_from_app_info(fmanager, file_icon,
                                                           app_info, TRUE, TRUE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

                    g_object_unref(app_info);

                    if(app_infos->next) {
                        list_len = g_list_length(app_infos->next);

                        if(!xfdesktop_file_utils_file_is_executable(info)
                           && list_len < 3)
                        {
                            mi = gtk_separator_menu_item_new();
                            gtk_widget_show(mi);
                            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                        }

                        if(list_len >= 3) {
                            img = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
                            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Open With"), img);
                            gtk_widget_show(mi);
                            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

                            app_infos_menu = gtk_menu_new();
                            gtk_menu_set_reserve_toggle_size(GTK_MENU(app_infos_menu), FALSE);
                            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), app_infos_menu);
                        } else
                            app_infos_menu = (GtkWidget *)menu;

                        for(GList *l = app_infos->next; l; l = l->next) {
                            app_info = G_APP_INFO(l->data);
                            mi = xfdesktop_menu_item_from_app_info(fmanager,
                                                                   file_icon, app_info,
                                                                   FALSE, TRUE);
                            gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                            g_object_unref(app_info);
                        }

                        if(list_len >= 3) {
                            mi = gtk_separator_menu_item_new();
                            gtk_widget_show(mi);
                            gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                        }
                    }

                    img = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Open With Other _Application..."), img);
                    gtk_widget_show(mi);
                    if(list_len >= 3)
                        gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                    else
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                     fmanager);

                    img = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Set _Default Application..."), img);
                    gtk_widget_show(mi);
                    if(list_len >= 3)
                        gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                    else
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_set_default_app),
                                     fmanager);

                    /* free the app info list */
                    g_list_free(app_infos);
                } else {
                    img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Open With Other _Application..."), img);
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                     fmanager);
                }

                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
        }

        if(file_icon == fmanager->priv->desktop_icon) {
            /* Menu on the root desktop window */
            /* Paste */
            img = gtk_image_new_from_icon_name("edit-paste", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Paste"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(xfdesktop_clipboard_manager_get_can_paste(clipboard_manager)) {
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_paste),
                                 fmanager);
            } else
                gtk_widget_set_sensitive(mi, FALSE);

            /* Separator */
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        } else if(!multi_sel_special) {
            /* Menu popup on an icon */
            /* Cut */
            img = gtk_image_new_from_icon_name("edit-cut", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Cu_t"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(multi_sel || xfdesktop_file_icon_can_delete_file(file_icon)) {
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_cut),
                                 fmanager);
            } else
                gtk_widget_set_sensitive(mi, FALSE);

            /* Copy */
            img = gtk_image_new_from_icon_name("edit-copy", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Copy"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_copy),
                             fmanager);

            /* Paste Into Folder */
            if(!multi_sel && info) {
                if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY
                   && g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
                    img = gtk_image_new_from_icon_name("edit-paste", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Paste Into Folder"), img);
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    if(xfdesktop_clipboard_manager_get_can_paste(clipboard_manager)) {
                        g_signal_connect(G_OBJECT(mi), "activate",
                                         G_CALLBACK(xfdesktop_file_icon_menu_paste_into_folder),
                                         fmanager);
                    } else
                        gtk_widget_set_sensitive(mi, FALSE);
                }
            }

            /* Separator */
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

            /* Trash */
            img = gtk_image_new_from_icon_name("user-trash", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Mo_ve to Trash"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(multi_sel || xfdesktop_file_icon_can_delete_file(file_icon)) {
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_trash),
                                 fmanager);
            } else
                gtk_widget_set_sensitive(mi, FALSE);

            /* Delete */
            if(fmanager->priv->show_delete_menu) {
                img = gtk_image_new_from_icon_name("edit-delete", GTK_ICON_SIZE_MENU);
                mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Delete"), img);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                if(multi_sel || xfdesktop_file_icon_can_delete_file(file_icon)) {
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_delete),
                                     fmanager);
                } else
                    gtk_widget_set_sensitive(mi, FALSE);
            }

            /* Separator */
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

            /* Rename */
            img = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Rename..."), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(!multi_sel && xfdesktop_file_icon_can_rename_file(file_icon)) {
                /* Rename a single icon */
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_rename),
                                 fmanager);
            } else if(multi_sel) {
                /* Bulk rename for multiple icons, the callback will
                 * handle the situation where some icons selected can't
                 * be renamed */
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_rename),
                                 fmanager);
            } else
                gtk_widget_set_sensitive(mi, FALSE);

            /* Separator */
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        }

#ifdef HAVE_THUNARX
        if(!multi_sel_special && fmanager->priv->thunarx_menu_providers) {
            GList *menu_items = NULL;
            ThunarxMenuProvider *provider;

            if(selected->data == fmanager->priv->desktop_icon) {
                /* click on the desktop itself, only show folder actions */
                for(GList *l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
                    provider = THUNARX_MENU_PROVIDER(l->data);
                    menu_items = g_list_concat(menu_items,
                                                 thunarx_menu_provider_get_folder_menu_items(provider,
                                                                                             toplevel,
                                                                                             THUNARX_FILE_INFO(file_icon)));
                }
            } else {
                /* thunar file specific actions (allows them to operate on folders
                 * that are on the desktop as well) */
                for(GList *l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
                    provider = THUNARX_MENU_PROVIDER(l->data);
                    menu_items = g_list_concat(menu_items,
                                                 thunarx_menu_provider_get_file_menu_items(provider,
                                                                                           toplevel,
                                                                                           selected));
                }
            }

            if(menu_items) {
                xfdesktop_menu_shell_append_thunarx_menu_item_list(GTK_MENU_SHELL(menu),
                                                        menu_items);
                g_list_free_full(menu_items, g_object_unref);

                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
        }
#endif

        if(file_icon == fmanager->priv->desktop_icon) {
            /* Menu on the root desktop window */
            img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Open in New Window"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_open_desktop),
                             fmanager);

            /* show arrange desktop icons option */
            img = gtk_image_new_from_icon_name("view-sort-ascending", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Arrange Desktop _Icons"), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_arrange_icons),
                             fmanager);

            if(xfce_desktop_get_cycle_backdrop(XFCE_DESKTOP(fmanager->priv->desktop))) {
                /* show next background option */
                img = gtk_image_new_from_icon_name("go-next", GTK_ICON_SIZE_MENU);
                mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Next Background"), img);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_next_background),
                                 fmanager);
            }

            /* Desktop settings window */
            img = gtk_image_new_from_icon_name("preferences-desktop-wallpaper", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("Desktop _Settings..."), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_settings_launch), fmanager);
        } else {
            /* Properties - applies only to icons on the desktop */
            img = gtk_image_new_from_icon_name("document-properties", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("P_roperties..."), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if (!info) {
                gtk_widget_set_sensitive(mi, FALSE);
            } else {
                g_signal_connect(G_OBJECT(mi), "activate",
                                file_icon == fmanager->priv->desktop_icon
                                ? G_CALLBACK(xfdesktop_file_icon_manager_desktop_properties)
                                : G_CALLBACK(xfdesktop_file_icon_menu_properties),
                                fmanager);
            }
        }
    }

    /* don't free |selected|.  the menu deactivated handler does that */
}

static void
file_icon_hash_write_icons(gpointer key,
                           gpointer value,
                           gpointer data)
{
    XfceRc *rcfile = data;
    XfdesktopIcon *icon = value;
    gint16 row, col;
    gchar *identifier = xfdesktop_icon_get_identifier(icon);

    if(xfdesktop_icon_get_position(icon, &row, &col)) {
        /* Attempt to use the identifier, fall back to using the labels. */
        if(identifier)
            xfce_rc_set_group(rcfile, identifier);
        else
            xfce_rc_set_group(rcfile, xfdesktop_icon_peek_label(icon));

        xfce_rc_write_int_entry(rcfile, "row", row);
        xfce_rc_write_int_entry(rcfile, "col", col);
    }

    if(identifier)
        g_free(identifier);
}

static gboolean
xfdesktop_file_icon_manager_save_icons(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar relpath[PATH_MAX], *tmppath, *path, *last_path;
    XfceRc *rcfile;
    gint x = 0, y = 0, width = 0, height = 0;

    fmanager->priv->save_icons_id = 0;

    if(!xfdesktop_get_workarea_single(fmanager->priv->icon_view,
                                       0,
                                       &x,
                                       &y,
                                       &width,
                                       &height))
    {
        GdkScreen *screen;
        GdkDisplay *display;
        GdkMonitor *monitor;
        GdkRectangle rectangle;

        screen = gtk_widget_get_screen (GTK_WIDGET (fmanager->priv->icon_view));
        display = gdk_screen_get_display (screen);
        monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_parent_window(GTK_WIDGET(fmanager->priv->icon_view)));
        gdk_monitor_get_workarea (monitor, &rectangle);
        width = rectangle.width;
        height = rectangle.height;
    }


    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d-%dx%d.rc",
               0,
               width,
               height);

    path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, relpath, TRUE);
    if(!path)
        return FALSE;

    XF_DEBUG("saving to: %s", path);

    tmppath = g_strconcat(path, ".new", NULL);

    rcfile = xfce_rc_simple_open(tmppath, FALSE);
    if(!rcfile) {
        g_warning("Unable to determine location of icon position cache file.  " \
                  "Icon positions will not be saved.");
        g_free(path);
        g_free(tmppath);
        return FALSE;
    }

    xfce_rc_set_group(rcfile, XFDESKTOP_RC_VERSION_STAMP);
    xfce_rc_write_bool_entry(rcfile, "4.10.3+", TRUE);

    g_hash_table_foreach(fmanager->priv->icons,
                         file_icon_hash_write_icons, rcfile);
    if(fmanager->priv->show_removable_media) {
        g_hash_table_foreach(fmanager->priv->removable_icons,
                             file_icon_hash_write_icons, rcfile);
    }
    g_hash_table_foreach(fmanager->priv->special_icons,
                         file_icon_hash_write_icons, rcfile);

    xfce_rc_flush(rcfile);
    xfce_rc_close(rcfile);

    if(g_file_test(tmppath, G_FILE_TEST_EXISTS)) {
        if(rename(tmppath, path)) {
            g_warning("Unable to rename temp file to %s: %s", path,
                      strerror(errno));
            unlink(tmppath);
        }
        else {
            last_path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, "xfce4/desktop/icons.screen.latest.rc", TRUE);
            if(last_path != NULL) {
                unlink(last_path);
                if(symlink(path, last_path) != 0)
                   g_warning("Unable to create symbolic link: %s",
                             strerror(errno));
                g_free(last_path);
            }
        }
    } else {
        XF_DEBUG("didn't write anything in the RC file, desktop is probably empty");
    }

    g_free(path);
    g_free(tmppath);

    return FALSE;
}

static void
xfdesktop_file_icon_position_changed(XfdesktopFileIcon *icon,
                                     gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    if(fmanager->priv->save_icons_id)
        g_source_remove(fmanager->priv->save_icons_id);

    fmanager->priv->save_icons_id = g_timeout_add(SAVE_DELAY,
                                                  xfdesktop_file_icon_manager_save_icons,
                                                  fmanager);
}


/*   *****   */

void
xfdesktop_file_icon_save(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    if(fmanager->priv->pending_icons == NULL)
        xfdesktop_file_icon_position_changed(NULL, user_data);
}

gboolean
xfdesktop_file_icon_manager_get_cached_icon_position(XfdesktopFileIconManager *fmanager,
                                                     const gchar *name,
                                                     const gchar *identifier,
                                                     gint16 *row,
                                                     gint16 *col)
{
    gchar relpath[PATH_MAX];
    gchar *filename = NULL;
    gboolean ret = FALSE;
    gint x = 0, y = 0, width = 0, height = 0;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(fmanager) && fmanager->priv != NULL, FALSE);
    g_return_val_if_fail(row != NULL && col != NULL, FALSE);

    if(!xfdesktop_get_workarea_single(fmanager->priv->icon_view,
                                       0,
                                       &x,
                                       &y,
                                       &width,
                                       &height))
    {
        GdkScreen *screen;
        GdkDisplay *display;
        GdkMonitor *monitor;
        GdkRectangle rectangle;

        screen = gtk_widget_get_screen (GTK_WIDGET (fmanager->priv->icon_view));
        display = gdk_screen_get_display (screen);
        monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_parent_window(GTK_WIDGET(fmanager->priv->icon_view)));
        gdk_monitor_get_workarea (monitor, &rectangle);
        width = rectangle.width;
        height = rectangle.height;
    }

    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d-%dx%d.rc",
               0,
               width,
               height);

    filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, relpath);

    /* Check if we have to migrate from the old file format */
    if(filename == NULL) {
        g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc", 0);
        filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, relpath);
    }

    /* Still nothing ? Just use the latest available file as fallback */
    if(filename == NULL) {
        filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, "xfce4/desktop/icons.screen.latest.rc");
    }

    if(filename != NULL) {
        XfceRc *rcfile = xfce_rc_simple_open(filename, TRUE);

        if (rcfile != NULL) {
            gboolean has_group = FALSE;

            /* Newer versions use the identifier rather than the icon label when
             * possible */
            if (xfce_rc_has_group(rcfile, XFDESKTOP_RC_VERSION_STAMP)
                && identifier != NULL
                && xfce_rc_has_group(rcfile, identifier))
            {
                xfce_rc_set_group(rcfile, identifier);
                has_group = TRUE;
            } else if (xfce_rc_has_group(rcfile, name)) {
                xfce_rc_set_group(rcfile, name);
                has_group = TRUE;
            }

            if (has_group) {
                *row = xfce_rc_read_int_entry(rcfile, "row", -1);
                *col = xfce_rc_read_int_entry(rcfile, "col", -1);
                if(*row >= 0 && *col >= 0)
                    ret = TRUE;
            }
            xfce_rc_close(rcfile);
        }

        g_free(filename);
    }

    return ret;
}


#if defined(DEBUG) && DEBUG > 0
static GList *_alive_icon_list = NULL;

static void
_icon_notify_destroy(gpointer data,
                     GObject *obj)
{
    g_assert(g_list_find(_alive_icon_list, obj));
    _alive_icon_list = g_list_remove(_alive_icon_list, obj);

    XF_DEBUG("icon finalized: '%s'", xfdesktop_icon_peek_label(XFDESKTOP_ICON(obj)));
}
#endif

static void
xfdesktop_file_icon_manager_queue_thumbnail(XfdesktopFileIconManager *fmanager,
                                            XfdesktopFileIcon *icon)
{
    GFile *file;
    gchar *path = NULL;

    file = xfdesktop_file_icon_peek_file(icon);

    if(file != NULL)
        path = g_file_get_path(file);

    if(fmanager->priv->show_thumbnails && path != NULL) {
        xfdesktop_thumbnailer_queue_thumbnail(fmanager->priv->thumbnailer, path);
    }

    if(path) {
        g_free(path);
        path = NULL;
    }
}

static void
add_icon_to_iconview(XfdesktopFileIconManager *fmanager,
                     XfdesktopIcon *icon)
{
    /* Pay attention to position changes and add the icon to the icon view */
    g_signal_connect(G_OBJECT(icon), "position-changed",
                     G_CALLBACK(xfdesktop_file_icon_position_changed),
                     fmanager);

    /* Tell the icon view about the icon */
    xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                 XFDESKTOP_ICON(icon));

#if defined(DEBUG) && DEBUG > 0
    _alive_icon_list = g_list_prepend(_alive_icon_list, icon);
    g_object_weak_ref(G_OBJECT(icon), _icon_notify_destroy, NULL);
#endif
}

/* Adds a single icon to the icon view, popping from the top of the stack.
 * Will continue to run until it runs out of icons to add at which point
 * it will free the queue and return FALSE */
static gboolean
process_icon_from_queue(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager;
    XfdesktopFileIcon *icon;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(user_data), FALSE);

    fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    /* Free our queue and return FALSE when we run out of items */
    if(g_queue_is_empty(fmanager->priv->pending_icons)) {
        g_queue_free(fmanager->priv->pending_icons);
        fmanager->priv->pending_icons = NULL;
        fmanager->priv->pending_icons_id = 0;
        return FALSE;
    }

    icon = g_queue_pop_head(fmanager->priv->pending_icons);

    /* skip bad icons */
    if(icon == NULL || !XFDESKTOP_IS_FILE_ICON(icon))
        return TRUE;

    add_icon_to_iconview(fmanager, XFDESKTOP_ICON(icon));

    return TRUE;
}

/* When the icon view gets resized we need to sort all the icons so they
 * are placed in the correct spot for the new resolution */
static void
icon_view_resized(XfdesktopIconView *icon_view,
                  XfdesktopFileIconManager *fmanager)
{
    GQueue *new_queue;
    XfdesktopIcon *icon;
    const gchar *name;
    gchar *identifier;
    gint16 row, col;

    XF_DEBUG("icon view - resize event!");

    if(!XFDESKTOP_IS_FILE_ICON_MANAGER(fmanager) || !XFDESKTOP_IS_ICON_VIEW(icon_view))
        return;

    /* No pending icons, nothing to do */
    if(fmanager->priv->pending_icons == NULL || g_queue_is_empty(fmanager->priv->pending_icons))
        return;

    new_queue = g_queue_new();

    while((icon = g_queue_pop_head(fmanager->priv->pending_icons))) {
        name = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
        identifier = xfdesktop_icon_get_identifier(XFDESKTOP_ICON(icon));

        if(xfdesktop_file_icon_manager_get_cached_icon_position(fmanager,
                                                                name, identifier,
                                                                &row, &col))
        {
            /* The icon has spot in the new resolution, add it to the front of
             * the queue. */
            XF_DEBUG("attempting to set icon '%s' to position (%d,%d) [location in cache]", name, row, col);
            xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
            g_queue_push_head(new_queue, icon);
        } else {
            /* Didn't have a spot, push it to the end of the stack. These will be
             * added last. */
            XF_DEBUG("icon '%s' didn't have a previous position", name);
            g_queue_push_tail(new_queue, icon);
        }

        if(identifier)
            g_free(identifier);
    }

    /* Free the old queue and replace it with the new one */
    g_queue_free(fmanager->priv->pending_icons);
    fmanager->priv->pending_icons = new_queue;
}

static void
xfdesktop_file_icon_manager_add_icon(XfdesktopFileIconManager *fmanager,
                                     XfdesktopFileIcon *icon,
                                     gint16 row, gint16 col)
{
    const gchar *name;
    gchar *identifier;

    /* Start thumbnail creation as early as possible */
    xfdesktop_file_icon_manager_queue_thumbnail(fmanager, icon);

    name = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
    identifier = xfdesktop_icon_get_identifier(XFDESKTOP_ICON(icon));

    /* Create our pending icon queue */
    if(fmanager->priv->pending_icons == NULL)
        fmanager->priv->pending_icons = g_queue_new();

    if(row >= 0 && col >= 0) {
        /* The row and col have been hard-set when adding the icon to the
         * icon view, probably by the user (like an icon rename). We assume
         * this is a top priority so bypass the queue and add it now */
        XF_DEBUG("attempting to set icon '%s' to position (%d,%d) [assigned location]", name, row, col);
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
        add_icon_to_iconview(fmanager, XFDESKTOP_ICON(icon));
    } else if(xfdesktop_file_icon_manager_get_cached_icon_position(fmanager,
                                                                   name, identifier,
                                                                   &row, &col))
    {
        /* The icon has been looked up in the cache add it to the front of
         * the queue. The queue may get sorted if the icon view sends a
         * resize-event */
        XF_DEBUG("attempting to set icon '%s' to position (%d,%d) [location in cache]", name, row, col);
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
        g_queue_push_head(fmanager->priv->pending_icons, icon);
    } else {
        /* Didn't have a spot, push it to the end of the stack. These will be
         * added last. */
        g_queue_push_tail(fmanager->priv->pending_icons, icon);
        XF_DEBUG("icon '%s' didn't have a previous position", name);
    }

    if(fmanager->priv->pending_icons_id != 0)
        g_source_remove(fmanager->priv->pending_icons_id);

    /* While xfdesktop is idle we'll add icons to the icon view */
    fmanager->priv->pending_icons_id = g_idle_add_full(G_PRIORITY_LOW,
                                                       process_icon_from_queue,
                                                       fmanager,
                                                       NULL);

    if(identifier)
        g_free(identifier);

}

static void
xfdesktop_file_icon_manager_remove_icon(XfdesktopFileIconManager *fmanager,
                                        XfdesktopFileIcon *icon)
{
    GList *item = NULL;

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(fmanager));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    /* find out if the icon was pending creation */
    if(fmanager->priv->pending_icons)
        item = g_queue_find(fmanager->priv->pending_icons, icon);

    if(item && item->data && XFDESKTOP_IS_FILE_ICON(item->data)) {
        gchar *filename = g_file_get_path(xfdesktop_file_icon_peek_file(item->data));

        XF_DEBUG("removing %s from pending queue", filename);

        /* Icon was pending creation, dequeue the thumbnail and
         * remove it from the pending icons queue */
        xfdesktop_thumbnailer_dequeue_thumbnail(fmanager->priv->thumbnailer,
                                                filename);

        g_queue_remove(fmanager->priv->pending_icons, icon);

        g_free(filename);
    } else {
        XF_DEBUG("removing icon %s from icon view", xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)));
        /* Remove the icon from the icon_view */
        xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                        XFDESKTOP_ICON(icon));
    }

    /* Remove the icon from the hash table */
    g_hash_table_remove(fmanager->priv->icons, xfdesktop_file_icon_peek_file(icon));
}

/* If row and col are set then they will be used, otherwise set them to -1
 * and it will lookup the position in the rc file */
static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_regular_icon(XfdesktopFileIconManager *fmanager,
                                             GFile *file,
                                             GFileInfo *info,
                                             gint16 row, gint16 col,
                                             gboolean defer_if_missing)
{
    XfdesktopRegularFileIcon *icon = NULL;
    gboolean is_desktop_file = FALSE;

    g_return_val_if_fail(fmanager && G_IS_FILE(file) && G_IS_FILE_INFO(info), NULL);

    if(g_content_type_equals(g_file_info_get_content_type(info),
                             "application/x-desktop"))
    {
        is_desktop_file = TRUE;
    }
    else
    {
      gchar *uri = g_file_get_uri(file);
      if(g_str_has_suffix(uri, ".desktop"))
          is_desktop_file = TRUE;
      g_free(uri);
    }

    /* if it's a .desktop file, and it has Hidden=true, or an
     * OnlyShowIn Or NotShowIn that would hide it from Xfce, don't
     * show it on the desktop (bug #4022) */
    if(is_desktop_file && !fmanager->priv->show_hidden_files)
    {
        gchar *path = g_file_get_path(file);
        XfceRc *rcfile = xfce_rc_simple_open(path, TRUE);
        g_free(path);

        if(rcfile) {
            const gchar *value;

            xfce_rc_set_group(rcfile, "Desktop Entry");
            if(xfce_rc_read_bool_entry(rcfile, "Hidden", FALSE)) {
                xfce_rc_close(rcfile);
                XF_DEBUG("Not adding icon because it has the Hidden Desktop Entry set");
                return NULL;
            }

            value = xfce_rc_read_entry(rcfile, "OnlyShowIn", NULL);
            if(value && strncmp(value, "XFCE;", 5) && !strstr(value, ";XFCE;")) {
                xfce_rc_close(rcfile);
                XF_DEBUG("Not adding icon because it has the OnlyShowIn Desktop Entry set");
                return NULL;
            }

            value = xfce_rc_read_entry(rcfile, "NotShowIn", NULL);
            if(value && (!strncmp(value, "XFCE;", 5) || strstr(value, ";XFCE;"))) {
                xfce_rc_close(rcfile);
                XF_DEBUG("Not adding icon because it has the NotShowIn Desktop Entry set");
                return NULL;
            }

            xfce_rc_close(rcfile);
        }
    }

    /* If it's a hidden or backup file don't show it on the desktop */
    if(g_file_info_get_is_hidden(info) || g_file_info_get_is_backup(info)) {
        if(!fmanager->priv->show_hidden_files) {
            XF_DEBUG("Not adding icon because it is either hidden or a backup file");
            return NULL;
        }
    }

    /* should never return NULL */
    icon = xfdesktop_regular_file_icon_new(file, info, fmanager->priv->gscreen, fmanager);

    xfdesktop_file_icon_manager_add_icon(fmanager,
                                         XFDESKTOP_FILE_ICON(icon),
                                         row, col);

    g_hash_table_replace(fmanager->priv->icons, g_object_ref(file), icon);
    return XFDESKTOP_FILE_ICON(icon);
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_volume_icon(XfdesktopFileIconManager *fmanager,
                                            GVolume *volume)
{
    XfdesktopVolumeIcon *icon;
    gchar *volume_type;

    g_return_val_if_fail(fmanager && G_IS_VOLUME(volume), NULL);

    /* If we aren't showing any media exit now */
    if(!fmanager->priv->show_removable_media)
        return NULL;

    volume_type = g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

    /* Check if we should filter out the volume icon based on what kind it is */
    if(g_strcmp0(volume_type, "network") == 0 && !fmanager->priv->show_network_volumes) {
        g_free(volume_type);
        return NULL;
    }
    if(g_strcmp0(volume_type, "device") == 0 && !fmanager->priv->show_device_volumes) {
        g_free(volume_type);
        return NULL;
    }
    if(volume_type == NULL && !fmanager->priv->show_unknown_volumes) {
        g_free(volume_type);
        return NULL;
    }

    if(volume_type)
        g_free(volume_type);

    /* should never return NULL */
    icon = xfdesktop_volume_icon_new(volume, fmanager->priv->gscreen);

    xfdesktop_file_icon_manager_add_icon(fmanager,
                                         XFDESKTOP_FILE_ICON(icon),
                                         -1, -1);

    g_hash_table_replace(fmanager->priv->removable_icons,
                         g_object_ref(G_OBJECT(volume)), icon);
    return XFDESKTOP_FILE_ICON(icon);
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_special_file_icon(XfdesktopFileIconManager *fmanager,
                                                  XfdesktopSpecialFileIconType type)
{
    XfdesktopSpecialFileIcon *icon;

    /* can return NULL if it's the trash icon and dbus isn't around */
    icon = xfdesktop_special_file_icon_new(type, fmanager->priv->gscreen);
    if(!icon)
        return NULL;

    xfdesktop_file_icon_manager_add_icon(fmanager,
                                         XFDESKTOP_FILE_ICON(icon),
                                         -1, -1);

    g_hash_table_replace(fmanager->priv->special_icons,
                         GINT_TO_POINTER(type), icon);
    return XFDESKTOP_FILE_ICON(icon);
}

static gboolean
xfdesktop_remove_icons_ht(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon = XFDESKTOP_ICON(value);
    GList *item = NULL;

    /* find out if the icon was pending creation */
    if(fmanager->priv->pending_icons)
        item = g_queue_find(fmanager->priv->pending_icons, icon);

    /* Remove the icon if it was in the icon view */
    if(item == NULL) {
        xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                        XFDESKTOP_ICON(value));
    }

    return TRUE;
}

static void
xfdesktop_file_icon_manager_refresh_icons(XfdesktopFileIconManager *fmanager)
{
    gint i;

    /* if a save is pending, flush icon positions */
    if(fmanager->priv->save_icons_id) {
        g_source_remove(fmanager->priv->save_icons_id);
        fmanager->priv->save_icons_id = 0;
        xfdesktop_file_icon_manager_save_icons(fmanager);
    }

    /* ditch removable media */
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_remove_removable_media(fmanager);

    /* ditch special icons */
    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        XfdesktopIcon *icon = g_hash_table_lookup(fmanager->priv->special_icons,
                                                  GINT_TO_POINTER(i));
        if(icon) {
            xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
            g_hash_table_remove(fmanager->priv->special_icons,
                                GINT_TO_POINTER(i));
        }
    }

    /* ditch normal icons */
    if(fmanager->priv->icons) {
        g_hash_table_foreach_remove(fmanager->priv->icons,
                                    (GHRFunc)xfdesktop_remove_icons_ht,
                                    fmanager);
    }

#if defined(DEBUG) && DEBUG > 0
    g_assert(_xfdesktop_icon_view_n_items(fmanager->priv->icon_view) == 0);
    g_assert(g_list_length(_alive_icon_list) == 0);
#endif

    /* clear out anything left in the icon view */
    xfdesktop_icon_view_remove_all(fmanager->priv->icon_view);

    /* add back the special icons */
    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        if(fmanager->priv->show_special[i])
            xfdesktop_file_icon_manager_add_special_file_icon(fmanager, i);
    }

    /* add back removable media */
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(fmanager);

    /* reload and add ~/Desktop/ */
    xfdesktop_file_icon_manager_load_desktop_folder(fmanager);
}

static gboolean
xfdesktop_file_icon_manager_key_press(GtkWidget *widget,
                                      GdkEventKey *evt,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    gboolean force_delete = FALSE;

    switch(evt->keyval) {
        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete:
            if(evt->state & GDK_SHIFT_MASK)
                force_delete = TRUE;
            xfdesktop_file_icon_manager_delete_selected(fmanager, force_delete);
            break;

        case GDK_KEY_c:
        case GDK_KEY_C:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
            if(selected) {
                xfdesktop_clipboard_manager_copy_files(clipboard_manager,
                                                       selected);
                g_list_free(selected);
            }
            break;

        case GDK_KEY_x:
        case GDK_KEY_X:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
            if(selected) {
                xfdesktop_clipboard_manager_cut_files(clipboard_manager,
                                                       selected);
                g_list_free(selected);
            }
            return TRUE;

        case GDK_KEY_v:
        case GDK_KEY_V:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            if(xfdesktop_clipboard_manager_get_can_paste(clipboard_manager)) {
                xfdesktop_clipboard_manager_paste_files(clipboard_manager, fmanager->priv->folder, widget, NULL);
            }
            return TRUE;

        case GDK_KEY_n:
        case GDK_KEY_N:
            if(!(evt->state & GDK_CONTROL_MASK && evt->state & GDK_SHIFT_MASK)
               || (evt->state & (GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            xfdesktop_file_icon_menu_create_folder(NULL, fmanager);
            return TRUE;

        case GDK_KEY_r:
        case GDK_KEY_R:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            /* fall through */
        case GDK_KEY_F5:
            xfdesktop_file_icon_manager_refresh_icons(fmanager);
            return TRUE;

        case GDK_KEY_F2:
            selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
            if(g_list_length(selected) == 1) {
                XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
                if(xfdesktop_file_icon_can_rename_file(icon)) {
                    xfdesktop_file_icon_menu_rename(NULL, fmanager);
                    return TRUE;
                }
            }
            if(selected)
                g_list_free(selected);
            break;
    }

    return FALSE;
}

static void
xfdesktop_file_icon_manager_file_changed(GFileMonitor     *monitor,
                                         GFile            *file,
                                         GFile            *other_file,
                                         GFileMonitorEvent event,
                                         gpointer          user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon, *moved_icon;
    GFileInfo *file_info;
    gint16 row = 0, col = 0;
    gchar *filename;

    switch(event) {
        case G_FILE_MONITOR_EVENT_RENAMED:
        case G_FILE_MONITOR_EVENT_MOVED_OUT:
            XF_DEBUG("got a moved event");

            icon = g_hash_table_lookup(fmanager->priv->icons, file);

            if(icon) {
                /* Get the old position so we can use it for the new icon */
                if(!xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col)) {
                    /* Failed to get position... not supported? */
                    row = col = 0;
                }
                XF_DEBUG("row %d, col %d", row, col);

                /* Remove the old icon */
                xfdesktop_file_icon_manager_remove_icon(fmanager, icon);
            }

            /* In case of MOVED_OUT, other_file will be NULL */
            if(other_file == NULL)
                return;

            file_info = g_file_query_info(other_file, XFDESKTOP_FILE_INFO_NAMESPACE,
                                            G_FILE_QUERY_INFO_NONE, NULL, NULL);

            /* Check to see if there's already an other_file represented on
             * the desktop and remove it so there aren't duplicated icons
             * present. */
            moved_icon = g_hash_table_lookup(fmanager->priv->icons, other_file);
            if(moved_icon) {
                /* Since we're replacing an existing icon, get that location
                 * to use instead */
                if(!xfdesktop_icon_get_position(XFDESKTOP_ICON(moved_icon), &row, &col)) {
                    /* Failed to get position... not supported? */
                    row = col = 0;
                }
                XF_DEBUG("row %d, col %d", row, col);

                xfdesktop_file_icon_manager_remove_icon(fmanager, moved_icon);
            }

            if(xfdesktop_compare_paths(g_file_get_parent(other_file), fmanager->priv->folder)) {
                XF_DEBUG("icon moved off the desktop");
                /* Nothing moved, this is actually a delete */
                if(file_info)
                    g_object_unref(file_info);

                return;
            }

            if(file_info) {
                /* Add the icon adding the row/col info */
                icon = xfdesktop_file_icon_manager_add_regular_icon(fmanager,
                                                                    other_file,
                                                                    file_info,
                                                                    row,
                                                                    col,
                                                                    FALSE);
                if(icon)
                    xfdesktop_file_icon_position_changed(icon, fmanager);

                g_object_unref(file_info);
            }
            break;
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
            XF_DEBUG("got changed event");

            icon = g_hash_table_lookup(fmanager->priv->icons, file);
            if(icon) {
                file_info = g_file_query_info(file, XFDESKTOP_FILE_INFO_NAMESPACE,
                                              G_FILE_QUERY_INFO_NONE, NULL, NULL);

                if(file_info) {
                    /* update the icon if the file still exists */
                    xfdesktop_file_icon_update_file_info(icon, file_info);
                    g_object_unref(file_info);

                    /* update thumbnail */
                    xfdesktop_file_icon_manager_queue_thumbnail (fmanager, icon);
                } else {
                    /* Remove the icon as it doesn't seem to exist */
                    xfdesktop_file_icon_manager_remove_icon(fmanager, icon);
                }
            }
            break;
        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_CREATED:
            XF_DEBUG("got created event");

            /* make sure it's not the desktop folder itself */
            if(g_file_equal(fmanager->priv->folder, file))
                return;

            /* first make sure we don't already have an icon for this path.
             * this seems to be necessary to avoid inconsistencies */
            icon = g_hash_table_lookup(fmanager->priv->icons, file);
            if(icon) {
                /* Remove the old icon */
                xfdesktop_file_icon_manager_remove_icon(fmanager, icon);
            }

            file_info = g_file_query_info(file, XFDESKTOP_FILE_INFO_NAMESPACE,
                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);
            if(file_info) {
                xfdesktop_file_icon_manager_add_regular_icon(fmanager,
                                                             file, file_info,
                                                             -1, -1,
                                                             TRUE);

                g_object_unref(file_info);
            }
            break;
        case G_FILE_MONITOR_EVENT_DELETED:
            XF_DEBUG("got deleted event");

            filename = g_file_get_path(file);

            icon = g_hash_table_lookup(fmanager->priv->icons, file);
            if(icon) {
                GList *item = NULL;

                /* find out if the icon was pending creation */
                if(fmanager->priv->pending_icons)
                    item = g_queue_find(fmanager->priv->pending_icons, icon);

                if(item) {
                    /* Icon was pending creation, dequeue the thumbnail and
                     * remove it from the pending icons queue */
                    xfdesktop_thumbnailer_dequeue_thumbnail(fmanager->priv->thumbnailer,
                                                            filename);

                    g_queue_remove(fmanager->priv->pending_icons, icon);
                } else {
                    /* Always try to remove thumbnail so it doesn't take up
                     * space on the user's disk. */
                    xfdesktop_thumbnailer_delete_thumbnail(fmanager->priv->thumbnailer,
                                                           filename);

                    /* Remove icon from the icon view */
                    xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                    XFDESKTOP_ICON(icon));
                }

                /* always remove from the hash table */
                g_hash_table_remove(fmanager->priv->icons, file);

                xfdesktop_file_icon_position_changed(NULL, fmanager);
            } else {
                if(g_file_equal(file, fmanager->priv->folder)) {
                    XF_DEBUG("~/Desktop disappeared!");
                    /* yes, refresh before and after is correct */
                    xfdesktop_file_icon_manager_refresh_icons(fmanager);
                    xfdesktop_file_icon_manager_check_create_desktop_folder(fmanager->priv->folder);
                    xfdesktop_file_icon_manager_refresh_icons(fmanager);
                }
            }

            if(filename)
                g_free(filename);
            break;
        default:
            break;
    }
}

static void
xfdesktop_file_icon_manager_update_file_info(gpointer key,
                                             gpointer value,
                                             gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(value);
    GFileInfo *file_info;

    if(icon) {
        file_info = g_file_query_info(key, XFDESKTOP_FILE_INFO_NAMESPACE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);

        if(file_info) {
            /* update the icon if the file still exists */
            xfdesktop_file_icon_update_file_info(icon, file_info);
            g_object_unref(file_info);
        }
    }
}

static gboolean
xfdesktop_file_icon_manager_metadata_timer(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    g_hash_table_foreach(fmanager->priv->icons,
                         (GHFunc)xfdesktop_file_icon_manager_update_file_info,
                         fmanager);

    fmanager->priv->metadata_timer = 0;

    return FALSE;
}

static void
xfdesktop_file_icon_manager_metadata_changed(GFileMonitor     *monitor,
                                             GFile            *file,
                                             GFile            *other_file,
                                             GFileMonitorEvent event,
                                             gpointer          user_data)
{
    XfdesktopFileIconManager *fmanager;
    guint timer;

    /* We only care about changed events */
    if(event != G_FILE_MONITOR_EVENT_CHANGED)
        return;

    /* Sanity check */
    if(user_data == NULL || !XFDESKTOP_IS_FILE_ICON_MANAGER(user_data))
        return;

    fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    XF_DEBUG("metadata file changed event");

     /* remove any pending metadata changes */
    if(fmanager->priv->metadata_timer != 0) {
        g_source_remove(fmanager->priv->metadata_timer);
    }

    /* cool down timer so we don't call this due to multiple file
     * changes at the same time. */
    timer = g_timeout_add_seconds(5, xfdesktop_file_icon_manager_metadata_timer, fmanager);

    fmanager->priv->metadata_timer = timer;
}

static void
xfdesktop_file_icon_manager_files_ready(GFileEnumerator *enumerator,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
    XfdesktopFileIconManager *fmanager;
    GError *error = NULL;
    GList *files, *l;

    /* Sanity check */
    if(user_data == NULL || !XFDESKTOP_IS_FILE_ICON_MANAGER(user_data))
        return;

    fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    if(enumerator != fmanager->priv->enumerator)
        return;

    files = g_file_enumerator_next_files_finish(enumerator, result, &error);

    if(!files) {
        if(error) {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

            xfce_message_dialog(gtk_widget_is_toplevel(toplevel) ? GTK_WINDOW(toplevel) : NULL,
                                _("Load Error"),
                                "dialog-warning",
                                _("Failed to load the desktop folder"), error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
        }

        g_object_unref(fmanager->priv->enumerator);
        fmanager->priv->enumerator = NULL;

        /* initialize the file monitor */
        if(!fmanager->priv->monitor) {
            fmanager->priv->monitor = g_file_monitor(fmanager->priv->folder,
                                                     G_FILE_MONITOR_WATCH_MOVES,
                                                     NULL, NULL);
            g_signal_connect(fmanager->priv->monitor, "changed",
                             G_CALLBACK(xfdesktop_file_icon_manager_file_changed),
                             fmanager);
        }

        /* initialize the metadata watching the gvfs files since it doesn't
         * send notification messages when monitored files change */
        if(!fmanager->priv->metadata_monitor) {
            gchar *location = xfce_resource_lookup(XFCE_RESOURCE_DATA, "gvfs-metadata/");
            GFile *metadata_location;

            if(location == NULL)
                return;

            metadata_location = g_file_new_for_path(location);

            fmanager->priv->metadata_monitor = g_file_monitor(metadata_location,
                                                              G_FILE_MONITOR_NONE,
                                                              NULL, NULL);
            g_signal_connect(fmanager->priv->metadata_monitor, "changed",
                             G_CALLBACK(xfdesktop_file_icon_manager_metadata_changed),
                             fmanager);

            g_object_unref(metadata_location);
            g_free(location);
        }
    } else {
        for(l = files; l; l = l->next) {
            const gchar *name = g_file_info_get_name(l->data);
            GFile *file = g_file_get_child(fmanager->priv->folder, name);

            XF_DEBUG("got a GFileInfo: %s", g_file_info_get_display_name(l->data));

            xfdesktop_file_icon_manager_add_regular_icon(fmanager,
                                                         file, l->data,
                                                         -1, -1,
                                                         TRUE);

            g_object_unref(file);

            g_object_unref(l->data);
        }

        g_list_free(files);

        g_file_enumerator_next_files_async(fmanager->priv->enumerator,
                                           10, G_PRIORITY_DEFAULT, NULL,
                                           (GAsyncReadyCallback) xfdesktop_file_icon_manager_files_ready,
                                           fmanager);
    }
}

static void
xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager)
{

    if(fmanager->priv->enumerator) {
        g_object_unref(fmanager->priv->enumerator);
        fmanager->priv->enumerator = NULL;
    }

    fmanager->priv->enumerator = g_file_enumerate_children(fmanager->priv->folder,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    if(fmanager->priv->enumerator) {
        g_file_enumerator_next_files_async(fmanager->priv->enumerator,
                                           10, G_PRIORITY_DEFAULT, NULL,
                                           (GAsyncReadyCallback) xfdesktop_file_icon_manager_files_ready,
                                           fmanager);

    }
}

static void
xfdesktop_file_icon_manager_check_icons_opacity(gpointer key,
                                                gpointer value,
                                                gpointer data)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(value);
    XfdesktopClipboardManager *cmanager = XFDESKTOP_CLIPBOARD_MANAGER(data);

    if(G_UNLIKELY(xfdesktop_clipboard_manager_has_cutted_file(cmanager, XFDESKTOP_FILE_ICON(icon))))
        xfdesktop_regular_file_icon_set_pixbuf_opacity(icon, 50);
    else
        xfdesktop_regular_file_icon_set_pixbuf_opacity(icon, 100);
}

static void
xfdesktop_file_icon_manager_clipboard_changed(XfdesktopClipboardManager *cmanager,
                                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    TRACE("entering");

    /* slooow? */
    g_hash_table_foreach(fmanager->priv->icons,
                         xfdesktop_file_icon_manager_check_icons_opacity,
                         cmanager);
}

static void
xfdesktop_file_icon_manager_volume_added(GVolumeMonitor *monitor,
                                         GVolume *volume,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    xfdesktop_file_icon_manager_add_volume_icon(fmanager, volume);
}

static void
xfdesktop_file_icon_manager_volume_removed(GVolumeMonitor *monitor,
                                           GVolume *volume,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon;

    icon = g_hash_table_lookup(fmanager->priv->removable_icons, volume);
    if(icon) {
        xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
        g_hash_table_remove(fmanager->priv->removable_icons, volume);
    }
}

static void
xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager)
{
    GList *volumes, *l;

    /* ensure we don't re-enter if we're already set up */
    if(fmanager->priv->removable_icons)
        return;

    if(!fmanager->priv->volume_monitor)
        fmanager->priv->volume_monitor = g_volume_monitor_get();

    fmanager->priv->removable_icons = g_hash_table_new_full(g_direct_hash,
                                                            g_direct_equal,
                                                            (GDestroyNotify)g_object_unref,
                                                            (GDestroyNotify)g_object_unref);

    volumes = g_volume_monitor_get_volumes(fmanager->priv->volume_monitor);
    for(l = volumes; l; l = l->next) {
        GVolume *volume = G_VOLUME(l->data);
        xfdesktop_file_icon_manager_add_volume_icon(fmanager, volume);
        g_object_unref(l->data);
    }
    g_list_free(volumes);

    g_signal_connect(G_OBJECT(fmanager->priv->volume_monitor), "volume-added",
                     G_CALLBACK(xfdesktop_file_icon_manager_volume_added),
                     fmanager);
    g_signal_connect(G_OBJECT(fmanager->priv->volume_monitor), "volume-removed",
                     G_CALLBACK(xfdesktop_file_icon_manager_volume_removed),
                     fmanager);
}

static void
xfdesktop_file_icon_manager_ht_remove_removable_media(gpointer key,
                                                      gpointer value,
                                                      gpointer user_data)
{
    XfdesktopIcon *icon = XFDESKTOP_ICON(value);
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
}

static void
xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager)
{
    if(fmanager->priv->removable_icons) {
        g_hash_table_foreach(fmanager->priv->removable_icons,
                             xfdesktop_file_icon_manager_ht_remove_removable_media,
                             fmanager);
        g_hash_table_destroy(fmanager->priv->removable_icons);
        fmanager->priv->removable_icons = NULL;
    }

    if(fmanager->priv->volume_monitor) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->volume_monitor),
                                             G_CALLBACK(xfdesktop_file_icon_manager_volume_added),
                                             fmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->volume_monitor),
                                             G_CALLBACK(xfdesktop_file_icon_manager_volume_removed),
                                             fmanager);

        g_object_unref(fmanager->priv->volume_monitor);
        fmanager->priv->volume_monitor = NULL;
    }
}


/* virtual functions */

static gboolean
xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                      XfdesktopIconView *icon_view)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    GFileInfo *desktop_info;
    gint i;
#ifdef HAVE_THUNARX
    ThunarxProviderFactory *thunarx_pfac;
#endif

    if(fmanager->priv->inited) {
        g_warning("Initializing icon manager when already inited");
        return FALSE;
    }

    fmanager->priv->icon_view = icon_view;

    /* Hook up to the resize-event so we can sort the icon queue and place
     * new icons where they belong */
    g_signal_connect(G_OBJECT(fmanager->priv->icon_view), "resize-event", G_CALLBACK(icon_view_resized), fmanager);

    fmanager->priv->desktop = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));
    fmanager->priv->gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));

    if(!clipboard_manager) {
        GdkDisplay *gdpy = gdk_screen_get_display(fmanager->priv->gscreen);
        clipboard_manager = xfdesktop_clipboard_manager_get_for_display(gdpy);
        g_object_add_weak_pointer(G_OBJECT(clipboard_manager),
                                  (gpointer)&clipboard_manager);
    } else
        g_object_ref(G_OBJECT(clipboard_manager));

    g_signal_connect(G_OBJECT(clipboard_manager), "changed",
                     G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                     fmanager);

    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_MULTIPLE);
    xfdesktop_icon_view_enable_drag_source(icon_view,
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK
                                           | GDK_BUTTON1_MASK,
                                           drag_targets, n_drag_targets,
                                           GDK_ACTION_LINK | GDK_ACTION_COPY
                                           | GDK_ACTION_MOVE);
    xfdesktop_icon_view_enable_drag_dest(icon_view, drop_targets,
                                         n_drop_targets, GDK_ACTION_LINK
                                         | GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect(G_OBJECT(xfdesktop_icon_view_get_window_widget(icon_view)),
                     "key-press-event",
                     G_CALLBACK(xfdesktop_file_icon_manager_key_press),
                     fmanager);

    fmanager->priv->icons = g_hash_table_new_full((GHashFunc)g_file_hash,
                                                  (GEqualFunc)g_file_equal,
                                                  (GDestroyNotify)g_object_unref,
                                                  (GDestroyNotify)g_object_unref);

    fmanager->priv->special_icons = g_hash_table_new_full(g_direct_hash,
                                                          g_direct_equal,
                                                          NULL,
                                                          (GDestroyNotify)g_object_unref);

    if(!xfdesktop_file_utils_dbus_init())
        g_warning("Unable to initialise D-Bus.  Some xfdesktop features may be unavailable.");

    /* do this in the reverse order stuff should be displayed */
    xfdesktop_file_icon_manager_load_desktop_folder(fmanager);
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(fmanager);
    for(i = XFDESKTOP_SPECIAL_FILE_ICON_TRASH; i >= 0; --i) {
        if(fmanager->priv->show_special[i])
            xfdesktop_file_icon_manager_add_special_file_icon(fmanager, i);
    }

#ifdef HAVE_THUNARX
    thunarx_pfac = thunarx_provider_factory_get_default();

    fmanager->priv->thunarx_menu_providers =
        thunarx_provider_factory_list_providers(thunarx_pfac,
                                                THUNARX_TYPE_MENU_PROVIDER);
    fmanager->priv->thunarx_properties_providers =
        thunarx_provider_factory_list_providers(thunarx_pfac,
                                                THUNARX_TYPE_PROPERTY_PAGE_PROVIDER);

    g_object_unref(G_OBJECT(thunarx_pfac));
#endif

    desktop_info = g_file_query_info(fmanager->priv->folder,
                                     XFDESKTOP_FILE_INFO_NAMESPACE,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, NULL);

    fmanager->priv->desktop_icon = XFDESKTOP_FILE_ICON(xfdesktop_regular_file_icon_new(fmanager->priv->folder,
                                                                                       desktop_info,
                                                                                       fmanager->priv->gscreen,
                                                                                       fmanager));

    g_object_unref(desktop_info);

    fmanager->priv->inited = TRUE;

    return TRUE;
}

static void
xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    gint i;

    if(!fmanager->priv->inited) {
        g_warning("Trying to de-init icon manager when it was never inited");
        return;
    }

    fmanager->priv->inited = FALSE;

    if(fmanager->priv->enumerator) {
        g_object_unref(fmanager->priv->enumerator);
        fmanager->priv->enumerator = NULL;
    }

    g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->icon_view),
                                         G_CALLBACK(icon_view_resized),
                                         fmanager);

    if(fmanager->priv->save_icons_id) {
        g_source_remove(fmanager->priv->save_icons_id);
        fmanager->priv->save_icons_id = 0;
        xfdesktop_file_icon_manager_save_icons(fmanager);
    }

    g_signal_handlers_disconnect_by_func(G_OBJECT(clipboard_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                                         fmanager);

    g_object_unref(G_OBJECT(clipboard_manager));

    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_remove_removable_media(fmanager);

    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        XfdesktopIcon *icon = g_hash_table_lookup(fmanager->priv->special_icons,
                                                  GINT_TO_POINTER(i));
        if(icon) {
            xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
            g_hash_table_remove(fmanager->priv->special_icons,
                                GINT_TO_POINTER(i));
        }
    }

    if(fmanager->priv->icons) {
        g_hash_table_foreach_remove(fmanager->priv->icons,
                                    (GHRFunc)xfdesktop_remove_icons_ht,
                                    fmanager);
    }

    /* Stop the idle callback adding pending icons */
    if(fmanager->priv->pending_icons_id != 0) {
        g_source_remove(fmanager->priv->pending_icons_id);
        fmanager->priv->pending_icons_id = 0;
    }

    /* Free anything left in the pending_icons queue */
    if(fmanager->priv->pending_icons) {
        g_queue_free_full(fmanager->priv->pending_icons, g_object_unref);
        fmanager->priv->pending_icons = NULL;
    }

    /* disconnect from the file monitor and release it */
    if(fmanager->priv->monitor) {
        g_signal_handlers_disconnect_by_func(fmanager->priv->monitor,
                                             G_CALLBACK(xfdesktop_file_icon_manager_file_changed),
                                             fmanager);
        g_object_unref(fmanager->priv->monitor);
        fmanager->priv->monitor = NULL;
    }

    /* Same for the file metadata monitor */
    if(fmanager->priv->metadata_monitor) {
        g_signal_handlers_disconnect_by_func(fmanager->priv->metadata_monitor,
                                             G_CALLBACK(xfdesktop_file_icon_manager_metadata_changed),
                                             fmanager);
        g_object_unref(fmanager->priv->metadata_monitor);
        fmanager->priv->metadata_monitor = NULL;
    }

    /* remove any pending metadata changes */
    if(fmanager->priv->metadata_timer != 0) {
        g_source_remove(fmanager->priv->metadata_timer);
        fmanager->priv->metadata_timer = 0;
    }

    g_object_unref(G_OBJECT(fmanager->priv->desktop_icon));
    fmanager->priv->desktop_icon = NULL;

#ifdef HAVE_THUNARX
    g_list_free_full(fmanager->priv->thunarx_menu_providers, g_object_unref);
    g_list_free_full(fmanager->priv->thunarx_properties_providers, g_object_unref);
#endif

    g_hash_table_destroy(fmanager->priv->special_icons);
    fmanager->priv->special_icons = NULL;

    g_hash_table_destroy(fmanager->priv->icons);
    fmanager->priv->icons = NULL;

    xfdesktop_file_utils_dbus_cleanup();

    g_signal_handlers_disconnect_by_func(G_OBJECT(xfdesktop_icon_view_get_window_widget(fmanager->priv->icon_view)),
                                         G_CALLBACK(xfdesktop_file_icon_manager_key_press),
                                         fmanager);

    xfdesktop_icon_view_unset_drag_source(fmanager->priv->icon_view);
    xfdesktop_icon_view_unset_drag_dest(fmanager->priv->icon_view);
}

static gboolean
xfdesktop_file_icon_manager_drag_drop(XfdesktopIconViewManager *manager,
                                      XfdesktopIcon *drop_icon,
                                      GdkDragContext *context,
                                      gint16 row,
                                      gint16 col,
                                      guint time_)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    GtkWidget *widget = GTK_WIDGET(fmanager->priv->icon_view);
    GdkAtom target;

    TRACE("entering");

    target = gtk_drag_dest_find_target(widget, context,
                                       fmanager->priv->drop_targets);
    if(target == GDK_NONE)
        return FALSE;
    else if(target == gdk_atom_intern("XdndDirectSave0", FALSE)) {
        /* X direct save protocol implementation copied more or less from
         * Thunar, Copyright (c) Benedikt Meurer */
        gint prop_len;
        guchar *prop_text = NULL;
        GFile *source_file, *file;
        gchar *uri = NULL;

        if(drop_icon) {
            GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info)
                return FALSE;

            if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
                return FALSE;

            source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(drop_icon));

        } else
            source_file = fmanager->priv->folder;

        if(gdk_property_get(gdk_drag_context_get_source_window(context),
                            gdk_atom_intern("XdndDirectSave0", FALSE),
                            gdk_atom_intern("text/plain", FALSE),
                            0, 1024, FALSE, NULL, NULL, &prop_len,
                            &prop_text) && prop_text)
        {
            prop_text = g_realloc(prop_text, prop_len + 1);
            prop_text[prop_len] = 0;

            file = g_file_resolve_relative_path(source_file, (const gchar *)prop_text);
            uri = g_file_get_uri(file);
            g_object_unref(file);

            gdk_property_change(gdk_drag_context_get_source_window(context),
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)uri,
                                strlen(uri));

            g_free(prop_text);
            g_free(uri);
        }

        if(!uri)
            return FALSE;
    } else if(target == gdk_atom_intern("_NETSCAPE_URL", FALSE)) {
        if(drop_icon) {
            /* don't allow a drop on an icon that isn't a folder (i.e., not
             * on an icon that's an executable */
            GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info || g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
                return FALSE;
        }
    }

    TRACE("target good");

    gtk_drag_get_data(widget, context, target, time_);

    return TRUE;
}

static void xfdesktop_dnd_item(GtkWidget *item, GdkDragAction *action)
{
    *action = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "action"));
}

static void xfdesktop_dnd_item_cancel(GtkWidget *item, GdkDragAction *action)
{
    *action = 0;
}

/**
 * xfdesktop_dnd_menu:
 * @manager     : the #XfdesktopIconViewManager instance
 * @drop_icon   : the #XfdesktopIcon to which is being dropped.
 * @context     : the #GdkDragContext of the icons being dropped.
 * @action      : the #GdkDragAction that the user selected. [out]
 * @row         : the row on the desktop to drop to.
 * @col         : the col on the desktop to drop to.
 * @ time_      : the starting time of the drag event.
 * Pops up a menu that asks the user to choose one of the
 * actions or to cancel the drop. Sets context->action to
 * the new action the user selected or 0 on cancel.
 * Portions of this code was copied from thunar-dnd.c
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 **/
void xfdesktop_dnd_menu (XfdesktopIconViewManager *manager,
                         XfdesktopIcon *drop_icon,
                         GdkDragContext *context,
                         GdkDragAction *action,
                         gint16 row,
                         gint16 col,
                         guint time_)
{
    static GdkDragAction    actions[] = { GDK_ACTION_COPY, GDK_ACTION_MOVE, GDK_ACTION_LINK };
    static const gchar      *action_names[] = { N_ ("Copy _Here") , N_ ("_Move Here") , N_ ("_Link Here") };
    static const gchar      *action_icons[] = { "stock_folder-copy", "stock_folder-move", "insert-link" };
    GtkWidget *menu;
    GtkWidget *item;
    GtkWidget  *image;
    guint menu_item, signal_id;
    GMainLoop *loop;
    gint response;

    menu = gtk_menu_new();
    gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

    /* This adds the Copy, Move, & Link options */
    for(menu_item = 0; menu_item < G_N_ELEMENTS(actions); menu_item++) {
        if(G_LIKELY(action_icons[menu_item] != NULL)) {
            image = gtk_image_new_from_icon_name(action_icons[menu_item], GTK_ICON_SIZE_MENU);
        } else {
            image = NULL;
        }

        item = xfdesktop_menu_create_menu_item_with_mnemonic(_(action_names[menu_item]), image);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xfdesktop_dnd_item), &response);
        g_object_set_data(G_OBJECT(item), "action", GUINT_TO_POINTER(actions[menu_item]));

        gtk_widget_show(item);
    }

    /* Add a seperator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Cancel option */
    image = gtk_image_new_from_icon_name("gtk-cancel", GTK_ICON_SIZE_MENU);
    item = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Cancel"), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xfdesktop_dnd_item_cancel), &response);
    gtk_widget_show(item);

    gtk_widget_show(menu);
    g_object_ref_sink(G_OBJECT(menu));

    /* Loop until we get a user response */
    loop = g_main_loop_new(NULL, FALSE);
    signal_id = g_signal_connect_swapped(G_OBJECT(menu), "deactivate", G_CALLBACK(g_main_loop_quit), loop);
    xfce_gtk_menu_popup_until_mapped(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, time_);
    g_main_loop_run(loop);
    g_signal_handler_disconnect(G_OBJECT(menu), signal_id);
    g_main_loop_unref(loop);

    *action = response;

    g_object_unref(G_OBJECT(menu));
}

static void
xfdesktop_file_icon_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                               XfdesktopIcon *drop_icon,
                                               GdkDragContext *context,
                                               gint16 row,
                                               gint16 col,
                                               GtkSelectionData *data,
                                               guint info,
                                               guint time_)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    XfdesktopFileIcon *file_icon = NULL;
    GFileInfo *tinfo = NULL;
    GFile *tfile = NULL;
    gboolean copy_only = TRUE, drop_ok = FALSE;
    GList *file_list;
    GdkDragAction action;

    TRACE("entering");

    action = gdk_drag_context_get_selected_action(context);

    if(action == GDK_ACTION_ASK) {
        xfdesktop_dnd_menu(manager, drop_icon, context, &action, row, col, time_);

        if(action == 0) {
            gtk_drag_finish(context, FALSE, FALSE, time_);
            return;
        }
    }

    if(info == TARGET_XDND_DIRECT_SAVE0) {
        /* FIXME: we don't support XdndDirectSave stage 3, result F, i.e.,
         * the app has to save the data itself given the filename we provided
         * in stage 1 */
        if(8 == gtk_selection_data_get_format(data)
           && 1 == gtk_selection_data_get_length(data)
           && 'F' == gtk_selection_data_get_data(data)[0])
        {
            gdk_property_change(gdk_drag_context_get_source_window(context),
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
        }

        drop_ok = TRUE;
    } else if(info == TARGET_NETSCAPE_URL) {
        /* data is "URL\nTITLE" */
        GFile *source_file = NULL;
        gchar *exo_desktop_item_edit = g_find_program_in_path("exo-desktop-item-edit");

        if(drop_icon) {
            GFileInfo *finfo = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(finfo != NULL && g_file_info_get_file_type(finfo) == G_FILE_TYPE_DIRECTORY)
                source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(drop_icon));
        } else
            source_file = fmanager->priv->folder;

        if(source_file && exo_desktop_item_edit) {
            gchar **parts = g_strsplit((const gchar *)gtk_selection_data_get_data(data), "\n", -1);

            if(2 == g_strv_length(parts)) {
                gchar *cwd = g_file_get_uri(source_file);
                gchar *myargv[16];
                gint i = 0;

                /* use the argv form so we don't have to worry about quoting
                 * the link title */
                myargv[i++] = exo_desktop_item_edit;
                myargv[i++] = "--type=Link";
                myargv[i++] = "--url";
                myargv[i++] = parts[0];
                myargv[i++] = "--name";
                myargv[i++] = parts[1];
                myargv[i++] = "--create-new";
                myargv[i++] = cwd;
                myargv[i++] = NULL;

                drop_ok = xfce_spawn(fmanager->priv->gscreen, NULL, myargv,
                                     NULL, G_SPAWN_SEARCH_PATH, TRUE,
                                     gtk_get_current_event_time(),
                                     NULL, TRUE, NULL);

                g_free(cwd);
            }

            g_strfreev(parts);
        }

        g_free(exo_desktop_item_edit);
    } else if(info == TARGET_APPLICATION_OCTET_STREAM) {
        const gchar *folder;
        gchar *filename;
        gchar *filepath;
        gchar *tmp;
        gint length;
        const gchar *content;
        GFile *dest;
        GFileOutputStream *out;

        if(gdk_property_get(gdk_drag_context_get_source_window(context),
                            gdk_atom_intern("XdndDirectSave0", FALSE),
                            gdk_atom_intern("text/plain", FALSE), 0, 1024,
                            FALSE, NULL, NULL, &length,
                            (guchar **)&filename) && length > 0) {
            filename = g_realloc(filename, length + 1);
            filename[length] = '\0';
        } else {
            filename = g_strdup(_("Untitled document"));
        }

        folder = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);

        /* get unique filename in case of duplicate */
        tmp = xfdesktop_file_utils_next_new_file_name(filename, folder);
        g_free(filename);
        filename = tmp;
        tmp = NULL;

        filepath = g_strdup_printf("%s/%s", folder, filename);

        dest = g_file_new_for_path(filepath);
        out = g_file_create(dest, G_FILE_CREATE_NONE, NULL, NULL);

        if(out) {
            content = (const gchar *)gtk_selection_data_get_data(data);
            length = gtk_selection_data_get_length(data);

            if(g_output_stream_write_all(G_OUTPUT_STREAM(out), content, length,
                                         NULL, NULL, NULL)) {
                g_output_stream_close(G_OUTPUT_STREAM(out), NULL, NULL);
            }

            g_object_unref(out);
        }

        g_free(filename);
        g_free(filepath);
        g_object_unref(dest);
    } else if(info == TARGET_TEXT_URI_LIST) {
        if(drop_icon) {
            file_icon = XFDESKTOP_FILE_ICON(drop_icon);
            tfile = xfdesktop_file_icon_peek_file(file_icon);
            tinfo = xfdesktop_file_icon_peek_file_info(file_icon);
        }

        copy_only = (action == GDK_ACTION_COPY);

        if(tfile && g_file_has_uri_scheme(tfile, "trash") && copy_only) {
            gtk_drag_finish(context, FALSE, FALSE, time_);
            return;
        }

        file_list = xfdesktop_file_utils_file_list_from_string((const gchar *)gtk_selection_data_get_data(data));
        if(file_list) {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

            if(tinfo && xfdesktop_file_utils_file_is_executable(tinfo)) {
                drop_ok = xfdesktop_file_utils_execute(fmanager->priv->folder,
                                                       tfile, file_list,
                                                       fmanager->priv->gscreen,
                                                       GTK_WINDOW(toplevel));
            } else if(tfile && g_file_has_uri_scheme(tfile, "trash")) {
                /* move files to the trash */
                xfdesktop_file_utils_trash_files(file_list,
                                                 fmanager->priv->gscreen,
                                                 GTK_WINDOW(toplevel));
            } else {
                GFile *base_dest_file = NULL;
                GList *l, *dest_file_list = NULL;
                gboolean dest_is_volume = (drop_icon
                                           && XFDESKTOP_IS_VOLUME_ICON(drop_icon));

                /* if it's a volume, but we don't have |tinfo|, this just isn't
                 * going to work */
                if(!tinfo && dest_is_volume) {
                    xfdesktop_file_utils_file_list_free(file_list);
                    gtk_drag_finish(context, FALSE, FALSE, time_);
                    return;
                }

                if(tinfo && g_file_info_get_file_type(tinfo) == G_FILE_TYPE_DIRECTORY) {
                    base_dest_file = g_object_ref(tfile);
                } else {
                    base_dest_file = g_object_ref(fmanager->priv->folder);
                }

                for (l = file_list; l; l = l->next) {
                    gchar *dest_basename = g_file_get_basename(l->data);

                    if(dest_basename && *dest_basename != '\0') {
                        /* If we copy a file, we need to use the new absolute filename
                         * as the destination. If we move or link, we need to use the destination
                         * directory. */
                        if(copy_only) {
                            GFile *dest_file = g_file_get_child(base_dest_file, dest_basename);
                            dest_file_list = g_list_prepend(dest_file_list, dest_file);
                        } else {
                            dest_file_list = g_list_prepend(dest_file_list, base_dest_file);
                        }
                    }

                    g_free(dest_basename);
                }

                g_object_unref(base_dest_file);

                if(dest_file_list) {
                    dest_file_list = g_list_reverse(dest_file_list);

                    xfdesktop_file_utils_transfer_files(action,
                                                        file_list,
                                                        dest_file_list,
                                                        fmanager->priv->gscreen);
                    drop_ok = TRUE;
                }

                if(copy_only) {
                    xfdesktop_file_utils_file_list_free(dest_file_list);
                } else {
                    g_list_free(dest_file_list);
                }
            }
        }
    }

    XF_DEBUG("finishing drop on desktop from external source: drop_ok=%s, copy_only=%s",
             drop_ok?"TRUE":"FALSE", copy_only?"TRUE":"FALSE");

    gtk_drag_finish(context, drop_ok, !copy_only, time_);
}

static void
xfdesktop_file_icon_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                          GList *drag_icons,
                                          GdkDragContext *context,
                                          GtkSelectionData *data,
                                          guint info,
                                          guint time_)
{
    GList *file_list;
    gchar *str;

    TRACE("entering");

    g_return_if_fail(drag_icons);

    file_list = xfdesktop_file_utils_file_icon_list_to_file_list(drag_icons);
    str = xfdesktop_file_utils_file_list_to_string(file_list);

    gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8, (guchar *)str, strlen(str));

    g_free(str);
    xfdesktop_file_utils_file_list_free(file_list);
}

static GdkDragAction
xfdesktop_file_icon_manager_propose_drop_action(XfdesktopIconViewManager *manager,
                                                XfdesktopIcon *drop_icon,
                                                GdkDragAction action,
                                                GdkDragContext *context,
                                                GtkSelectionData *data,
                                                guint info)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    XfdesktopFileIcon *file_icon = NULL;
    GFileInfo *tinfo = NULL;
    GFile *tfile = NULL;
    GList *file_list;
    GFileInfo *src_info, *dest_info;
    const gchar *src_name, *dest_name;

    if(info == TARGET_TEXT_URI_LIST && action == GDK_ACTION_COPY
            && (gdk_drag_context_get_actions(context) & GDK_ACTION_MOVE) != 0) {

        if(drop_icon) {
            file_icon = XFDESKTOP_FILE_ICON(drop_icon);
            tfile = xfdesktop_file_icon_peek_file(file_icon);
            tinfo = xfdesktop_file_icon_peek_file_info(file_icon);

            /* if it's a volume, but we don't have |tinfo|, this just isn't
             * going to work */
            if(!tinfo && XFDESKTOP_IS_VOLUME_ICON(drop_icon)) {
                return 0;
            }

            if(tfile && !g_file_has_uri_scheme(tfile, "file")) {
                return action;
            }

            if(tinfo && g_file_info_get_file_type(tinfo) != G_FILE_TYPE_DIRECTORY) {
                return action;
            }
        }

        file_list = xfdesktop_file_utils_file_list_from_string((const gchar *)gtk_selection_data_get_data(data));
        if(file_list) {
            GFile *base_dest_file = NULL;

            /* always move files from the trash */
            if(g_file_has_uri_scheme(file_list->data, "trash")) {
                xfdesktop_file_utils_file_list_free(file_list);
                return GDK_ACTION_MOVE;
            }

            /* source must be local file */
            if(!g_file_has_uri_scheme(file_list->data, "file")) {
                xfdesktop_file_utils_file_list_free(file_list);
                return action;
            }

            if(tinfo) {
                base_dest_file = g_object_ref(tfile);
            } else {
                base_dest_file = g_object_ref(fmanager->priv->folder);
            }

            /* dropping on ourselves? */
            if(g_strcmp0(g_file_get_uri(file_list->data), g_file_get_uri(base_dest_file)) == 0) {
                g_object_unref(base_dest_file);
                xfdesktop_file_utils_file_list_free(file_list);
                return 0;
            }

            /* Determine if we should move/copy by checking if the files
             * are on the same filesystem and are writable by the user.
             */

            dest_info = g_file_query_info(base_dest_file,
                                          XFDESKTOP_FILE_INFO_NAMESPACE,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          NULL);
            src_info = g_file_query_info(file_list->data,
                                         XFDESKTOP_FILE_INFO_NAMESPACE,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL,
                                         NULL);

            if(dest_info != NULL && src_info != NULL) {
                dest_name = g_file_info_get_attribute_string(dest_info,
                                        G_FILE_ATTRIBUTE_ID_FILESYSTEM);
                src_name = g_file_info_get_attribute_string(src_info,
                                        G_FILE_ATTRIBUTE_ID_FILESYSTEM);

                if((g_strcmp0(src_name, dest_name) == 0)
                   && g_file_info_get_attribute_boolean(src_info,
                                    G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
                {
                    action = GDK_ACTION_MOVE;
                }
            }

            if(dest_info != NULL)
                g_object_unref(dest_info);
            if(src_info != NULL)
                g_object_unref(src_info);

            g_object_unref(base_dest_file);
            xfdesktop_file_utils_file_list_free(file_list);

        }
    }

    return action;
}


/* public api */

XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(GFile *folder,
                                XfconfChannel *channel)
{
    XfdesktopFileIconManager *fmanager;

    g_return_val_if_fail(folder && channel, NULL);

    fmanager = g_object_new(XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                            "folder", folder,
                            NULL);
    fmanager->priv->channel = XFCONF_CHANNEL(g_object_ref(G_OBJECT(channel)));

    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_FILESYSTEM, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-filesystem");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_HOME, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-home");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_TRASH, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-trash");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_REMOVABLE, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-removable");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_NETWORK_REMOVABLE, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-network-volume");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_DEVICE_REMOVABLE, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-device-volume");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_UNKNWON_REMOVABLE, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-unknown-volume");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_THUMBNAILS, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-thumbnails");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_HIDDEN_FILES, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-hidden-files");
    xfconf_g_property_bind(channel, DESKTOP_MENU_DELETE, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-delete-menu");
    xfconf_g_property_bind(channel, DESKTOP_MENU_MAX_TEMPLATE_FILES, G_TYPE_INT,
                           G_OBJECT(fmanager), "max-templates");

    return XFDESKTOP_ICON_VIEW_MANAGER(fmanager);
}

static void
xfdesktop_file_icon_manager_set_show_removable_media(XfdesktopFileIconManager *manager,
                                                     XfdesktopFileIconManagerProp prop,
                                                     gboolean show)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));

    switch(prop) {
        case PROP_SHOW_REMOVABLE:
            if(show == manager->priv->show_removable_media)
                return;

            manager->priv->show_removable_media = show;
            break;
        case PROP_SHOW_NETWORK_VOLUME:
            if(show == manager->priv->show_network_volumes)
                return;

            manager->priv->show_network_volumes = show;
            break;
        case PROP_SHOW_DEVICE_VOLUME:
            if(show == manager->priv->show_device_volumes)
                return;

            manager->priv->show_device_volumes = show;
            break;
        case PROP_SHOW_UNKNOWN_VOLUME:
            if(show == manager->priv->show_unknown_volumes)
                return;

            manager->priv->show_unknown_volumes = show;
            break;
        default:
            break;
    }

    if(!manager->priv->inited)
        return;

    /* Always remove all the icons when a setting changes */
    xfdesktop_file_icon_manager_remove_removable_media(manager);

    /* Then re-add the icons the user wants */
    if(manager->priv->show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(manager);
}


static void
xfdesktop_file_icon_manager_requeue_thumbnails(gpointer key,
                                               gpointer value,
                                               gpointer data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(data);
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(value);

    xfdesktop_file_icon_manager_queue_thumbnail(fmanager, icon);
}

static void
xfdesktop_file_icon_manager_remove_thumbnails(gpointer key,
                                              gpointer value,
                                              gpointer data)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(value);

    xfdesktop_icon_delete_thumbnail(XFDESKTOP_ICON(icon));
}

static void
xfdesktop_file_icon_manager_set_show_thumbnails(XfdesktopFileIconManager *manager,
                                                gboolean show_thumbnails)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));

    if(show_thumbnails == manager->priv->show_thumbnails)
        return;

    manager->priv->show_thumbnails = show_thumbnails;

    if(!manager->priv->inited)
        return;

    if(show_thumbnails) {
        /* We have to request to create the thumbnails everytime. */
         g_hash_table_foreach(manager->priv->icons,
                         xfdesktop_file_icon_manager_requeue_thumbnails,
                         manager);
    } else {
        /* We have to remove the thumbnails because the regular file
         * icons can't easily check if thumbnails are allowed.
         */
         g_hash_table_foreach(manager->priv->icons,
                         xfdesktop_file_icon_manager_remove_thumbnails,
                         manager);
    }
}

static void
xfdesktop_file_icon_manager_set_show_hidden_files(XfdesktopFileIconManager *manager,
                                                  gboolean show_hidden_files)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));

    TRACE("entering show_hidden_files %s", show_hidden_files ? "TRUE" : "FALSE");

    if(show_hidden_files == manager->priv->show_hidden_files)
        return;

    manager->priv->show_hidden_files = show_hidden_files;

    if(!manager->priv->inited)
        return;

    /* Emit a signal so the desktop knows to reload the icons */
    g_signal_emit(G_OBJECT(manager), fmanager_signals[HIDDEN_STATE_CHANGED], 0);
}

static void
xfdesktop_file_icon_manager_set_show_delete_menu(XfdesktopFileIconManager *manager,
                                                 gboolean show_delete_menu)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));

    manager->priv->show_delete_menu = show_delete_menu;
}

static void
xfdesktop_file_icon_manager_set_max_templates(XfdesktopFileIconManager *manager,
                                              gint max_templates)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));

    if(max_templates < 0 || max_templates > G_MAXUSHORT)
        return;

    manager->priv->max_templates = max_templates;
}

static void
xfdesktop_file_icon_manager_set_show_special_file(XfdesktopFileIconManager *manager,
                                                  XfdesktopSpecialFileIconType type,
                                                  gboolean show_special_file)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));
    g_return_if_fail((int)type >= 0 && type <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH);

    if(manager->priv->show_special[type] == show_special_file)
        return;

    manager->priv->show_special[type] = show_special_file;

    if(!manager->priv->inited)
        return;

    if(show_special_file) {
        g_return_if_fail(!g_hash_table_lookup(manager->priv->special_icons,
                                              GINT_TO_POINTER(type)));
        xfdesktop_file_icon_manager_add_special_file_icon(manager, type);
    } else {
        XfdesktopIcon *icon = g_hash_table_lookup(manager->priv->special_icons,
                                                  GINT_TO_POINTER(type));
        if(icon) {
            xfdesktop_icon_view_remove_item(manager->priv->icon_view, icon);
            g_hash_table_remove(manager->priv->special_icons,
                                GINT_TO_POINTER(type));
        }
    }
}

static void
xfdesktop_file_icon_manager_update_image(GtkWidget *widget,
                                         gchar *srcfile,
                                         gchar *thumbfile,
                                         XfdesktopFileIconManager *manager)
{
    GFile *file;
    XfdesktopIcon *icon;

    g_return_if_fail(srcfile && thumbfile);
    g_return_if_fail(XFDESKTOP_FILE_ICON_MANAGER(manager));

    file = g_file_new_for_path(srcfile);

    icon = g_hash_table_lookup(manager->priv->icons, file);
    if(icon)
    {
        g_object_unref(file);
        file = g_file_new_for_path(thumbfile);
        xfdesktop_icon_set_thumbnail_file(icon, g_object_ref(file));
    }

    g_object_unref(file);
}
