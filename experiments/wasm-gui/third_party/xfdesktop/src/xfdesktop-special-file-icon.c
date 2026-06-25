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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-special-file-icon.h"

struct _XfdesktopSpecialFileIconPrivate
{
    XfdesktopSpecialFileIconType type;
    gchar *tooltip;
    GFileMonitor *monitor;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    GdkScreen *gscreen;

    /* only needed for trash */
    guint trash_item_count;
};

static void xfdesktop_special_file_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_special_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                          gint width, gint height);
static const gchar *xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon);
static gchar *xfdesktop_special_file_icon_get_identifier(XfdesktopIcon *icon);
static GdkPixbuf *xfdesktop_special_file_icon_peek_tooltip_pixbuf(XfdesktopIcon *icon,
                                                                  gint width, gint height);
static const gchar *xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_special_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_special_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon,
                                                                          GdkDragAction *suggested_action);
static gboolean xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                         GList *src_icons,
                                                         GdkDragAction action);
static gboolean xfdesktop_special_file_icon_populate_context_menu(XfdesktopIcon *icon,
                                                                  GtkWidget *menu);

static GFileInfo *xfdesktop_special_file_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_special_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_special_file_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_special_file_icon_changed(GFileMonitor *monitor,
                                                GFile *file,
                                                GFile *other_file,
                                                GFileMonitorEvent event,
                                                XfdesktopSpecialFileIcon *special_file_icon);
static void xfdesktop_special_file_icon_update_trash_count(XfdesktopSpecialFileIcon *special_file_icon);

#ifdef HAVE_THUNARX
static void xfdesktop_special_file_icon_tfi_init(ThunarxFileInfoIface *iface);

G_DEFINE_TYPE_EXTENDED(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_special_file_icon_tfi_init)
                       G_ADD_PRIVATE(XfdesktopSpecialFileIcon)
                       )
#else
G_DEFINE_TYPE_WITH_PRIVATE(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
                           XFDESKTOP_TYPE_FILE_ICON)
#endif



static void
xfdesktop_special_file_icon_class_init(XfdesktopSpecialFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;

    gobject_class->finalize = xfdesktop_special_file_icon_finalize;

    icon_class->peek_pixbuf = xfdesktop_special_file_icon_peek_pixbuf;
    icon_class->peek_label = xfdesktop_special_file_icon_peek_label;
    icon_class->get_identifier = xfdesktop_special_file_icon_get_identifier;
    icon_class->peek_tooltip_pixbuf = xfdesktop_special_file_icon_peek_tooltip_pixbuf;
    icon_class->peek_tooltip = xfdesktop_special_file_icon_peek_tooltip;
    icon_class->get_allowed_drag_actions = xfdesktop_special_file_icon_get_allowed_drag_actions;
    icon_class->get_allowed_drop_actions = xfdesktop_special_file_icon_get_allowed_drop_actions;
    icon_class->do_drop_dest = xfdesktop_special_file_icon_do_drop_dest;
    icon_class->populate_context_menu = xfdesktop_special_file_icon_populate_context_menu;

    file_icon_class->peek_file_info = xfdesktop_special_file_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_special_file_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_special_file_icon_peek_file;
}

static void
xfdesktop_special_file_icon_init(XfdesktopSpecialFileIcon *icon)
{
    icon->priv = xfdesktop_special_file_icon_get_instance_private(icon);
}

static void
xfdesktop_special_file_icon_finalize(GObject *obj)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);

    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_icon_invalidate_pixbuf),
                                         icon);

    if(icon->priv->monitor) {
        g_signal_handlers_disconnect_by_func(icon->priv->monitor,
                                             G_CALLBACK(xfdesktop_special_file_icon_changed),
                                             icon);
        g_object_unref(icon->priv->monitor);
    }

    g_object_unref(icon->priv->file);

    if(icon->priv->file_info)
        g_object_unref(icon->priv->file_info);

    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);

    G_OBJECT_CLASS(xfdesktop_special_file_icon_parent_class)->finalize(obj);
}

#ifdef HAVE_THUNARX
static gchar *
xfdesktop_special_file_icon_tfi_get_uri_scheme(ThunarxFileInfo *file_info)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(file_info);

    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == icon->priv->type)
        return g_strdup("trash");
    else
        return g_strdup("file");
}

static void
xfdesktop_special_file_icon_tfi_init(ThunarxFileInfoIface *iface)
{
    iface->get_name = xfdesktop_thunarx_file_info_get_name;
    iface->get_uri = xfdesktop_thunarx_file_info_get_uri;
    iface->get_parent_uri = xfdesktop_thunarx_file_info_get_parent_uri;
    iface->get_uri_scheme = xfdesktop_special_file_icon_tfi_get_uri_scheme;
    iface->get_mime_type = xfdesktop_thunarx_file_info_get_mime_type;
    iface->has_mime_type = xfdesktop_thunarx_file_info_has_mime_type;
    iface->is_directory = xfdesktop_thunarx_file_info_is_directory;
    iface->get_file_info = xfdesktop_thunarx_file_info_get_file_info;
    iface->get_filesystem_info = xfdesktop_thunarx_file_info_get_filesystem_info;
    iface->get_location = xfdesktop_thunarx_file_info_get_location;
}
#endif  /* HAVE_THUNARX */


static GIcon *
xfdesktop_special_file_icon_load_icon(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    gchar *icon_name = NULL;
    GFile *parent = NULL;
    GIcon *gicon = NULL;

    TRACE("entering");

    /* use a custom icon name for the local filesystem root */
    parent = g_file_get_parent(special_icon->priv->file);
    if(!parent && g_file_has_uri_scheme(special_icon->priv->file, "file"))
        icon_name = g_strdup("drive-harddisk");

    if(parent)
        g_object_unref(parent);

    /* use a custom icon for the trash, based on it having files
     * the user can delete */
    if(special_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH) {
        if(special_icon->priv->trash_item_count == 0)
            icon_name = g_strdup("user-trash");
        else
            icon_name = g_strdup("user-trash-full");
    }

    /* Create the themed icon for it */
    if(icon_name) {
        gicon = g_themed_icon_new(icon_name);
        g_free(icon_name);
    }

    /* If we still don't have an icon, use the default */
    if(!G_IS_ICON(gicon)) {
        gicon = g_file_info_get_icon(special_icon->priv->file_info);
        if(G_IS_ICON(gicon))
            g_object_ref(gicon);
    }

    g_object_set(file_icon, "gicon", gicon, NULL);

    /* Add any user set emblems */
    gicon = xfdesktop_file_icon_add_emblems(file_icon);

    return gicon;
}

static GdkPixbuf *
xfdesktop_special_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                        gint width, gint height)
{
    GIcon *gicon = NULL;
    GdkPixbuf *pix = NULL;

    if(!xfdesktop_file_icon_has_gicon(XFDESKTOP_FILE_ICON(icon)))
        gicon = xfdesktop_special_file_icon_load_icon(icon);
    else
        g_object_get(XFDESKTOP_FILE_ICON(icon), "gicon", &gicon, NULL);

    pix = xfdesktop_file_utils_get_icon(gicon, height, height, 100);

    return pix;
}

static GdkPixbuf *
xfdesktop_special_file_icon_peek_tooltip_pixbuf(XfdesktopIcon *icon,
                                                gint width, gint height)
{
    GIcon *gicon = NULL;
    GdkPixbuf *tooltip_pix = NULL;

    if(!xfdesktop_file_icon_has_gicon(XFDESKTOP_FILE_ICON(icon)))
        gicon = xfdesktop_special_file_icon_load_icon(icon);
    else
        g_object_get(XFDESKTOP_FILE_ICON(icon), "gicon", &gicon, NULL);

    tooltip_pix = xfdesktop_file_utils_get_icon(gicon, height, height, 100);

    return tooltip_pix;
}

static const gchar *
xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GFileInfo *info = special_file_icon->priv->file_info;

    if(XFDESKTOP_SPECIAL_FILE_ICON_HOME == special_file_icon->priv->type)
        return _("Home");
    else if(XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM == special_file_icon->priv->type)
        return _("File System");
    else if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == special_file_icon->priv->type)
        return _("Trash");
    else
        return info ? g_file_info_get_display_name(info) : NULL;
}

static gchar *
xfdesktop_special_file_icon_get_identifier(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    if(xfdesktop_file_icon_peek_file(file_icon) == NULL)
        return NULL;

    return g_file_get_path(xfdesktop_file_icon_peek_file(file_icon));
}

static GdkDragAction
xfdesktop_special_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GdkDragAction actions = 0;

    switch(special_file_icon->priv->type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            /* move is just impossible, and copy seems a bit retarded.  link
             * is possible */
            actions = GDK_ACTION_LINK;
            break;

        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            /* user shouldn't be able to move their own homedir.  copy might
             * be a little silly, but allow it anyway.  link is fine. */
            actions = GDK_ACTION_COPY | GDK_ACTION_LINK;
            break;

        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            /* move is impossible, but we can copy and link the trash root
             * anywhere */
            actions = GDK_ACTION_COPY | GDK_ACTION_LINK;
            break;
    }

    return actions;
}

static GdkDragAction
xfdesktop_special_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon,
                                                     GdkDragAction *suggested_action)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GFileInfo *info;
    GdkDragAction actions = 0;

    if(special_file_icon->priv->type != XFDESKTOP_SPECIAL_FILE_ICON_TRASH) {
        info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
        if(info) {
            if(g_file_info_get_attribute_boolean(info,
                                                 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
            {
                XF_DEBUG("can move, copy, link and ask");
                actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK;
                if(suggested_action)
                    *suggested_action = GDK_ACTION_MOVE;
            }
        }
    } else {
        XF_DEBUG("can move");
        actions = GDK_ACTION_MOVE; /* everything else is just silly */
        if(suggested_action)
            *suggested_action = GDK_ACTION_MOVE;
    }

    if(suggested_action)
        *suggested_action = 0;

    return actions;
}

static gboolean
xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                         GList *src_icons,
                                         GdkDragAction action)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    gboolean result = FALSE;

    TRACE("entering");

    g_return_val_if_fail(special_file_icon != NULL && src_icons != NULL, FALSE);
    g_return_val_if_fail(xfdesktop_special_file_icon_get_allowed_drop_actions(icon, NULL),
                         FALSE);

    if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH) {
        GList *files = NULL;

        XF_DEBUG("doing trash");

        for (GList *l = src_icons; l != NULL; l = l->next) {
            GFile *file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(l->data));
            if (file != NULL) {
                files = g_list_prepend(files, file);
            }
        }
        files = g_list_reverse(files);

        if (files != NULL) {
            /* let the trash service handle the trash operation */
            xfdesktop_file_utils_trash_files(files, special_file_icon->priv->gscreen, NULL);
            result = TRUE;
        }
    } else {
        GList *src_files = NULL;
        GList *dest_files = NULL;

        xfdesktop_file_utils_build_transfer_file_lists(action, src_icons, XFDESKTOP_FILE_ICON(icon), &src_files, &dest_files);

        /* let the file manager service move/copy/link the file */
        if (src_files != NULL && dest_files != NULL) {
            xfdesktop_file_utils_transfer_files(action, src_files, dest_files,
                                               special_file_icon->priv->gscreen);
            result = TRUE;
        }

        g_list_free_full(dest_files, g_object_unref);
        g_list_free(src_files);
    }

    return result;
}

static const gchar *
xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);

    if(!special_file_icon->priv->tooltip) {
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));

        if(!info)
            return NULL;

        if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == special_file_icon->priv->type) {
            if(special_file_icon->priv->trash_item_count == 0) {
                special_file_icon->priv->tooltip = g_strdup(_("Trash is empty"));
            } else {
                special_file_icon->priv->tooltip = g_strdup_printf(g_dngettext(GETTEXT_PACKAGE,
                                                                               _("Trash contains one item"),
                                                                               _("Trash contains %d items"),
                                                                               special_file_icon->priv->trash_item_count),

                                                                   special_file_icon->priv->trash_item_count);
            }
        } else {
            const gchar *description;
            gchar *size_string, *time_string;
            guint64 size, mtime;

            if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM)
                description = _("File System");
            else if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_HOME)
                description = _("Home");
            else {
                description = g_file_info_get_attribute_string(info,
                                                               G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION);
            }

            size = g_file_info_get_attribute_uint64(info,
                                                    G_FILE_ATTRIBUTE_STANDARD_SIZE);

            size_string = g_format_size(size);

            mtime = g_file_info_get_attribute_uint64(info,
                                                     G_FILE_ATTRIBUTE_TIME_MODIFIED);
            time_string = xfdesktop_file_utils_format_time_for_display(mtime);

            special_file_icon->priv->tooltip =
                g_strdup_printf(_("%s\nSize: %s\nLast modified: %s"),
                                description, size_string, time_string);

            g_free(size_string);
            g_free(time_string);
        }
    }

    return special_file_icon->priv->tooltip;
}

static void
xfdesktop_special_file_icon_trash_open(GtkWidget *w,
                                       gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    GtkWidget *icon_view, *toplevel;

    icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(file_icon));
    toplevel = gtk_widget_get_toplevel(icon_view);

    xfdesktop_file_utils_open_folder(file_icon->priv->file,
                                     file_icon->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_special_file_icon_trash_empty(GtkWidget *w,
                                        gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    GtkWidget *icon_view, *toplevel;

    icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(file_icon));
    toplevel = gtk_widget_get_toplevel(icon_view);

    xfdesktop_file_utils_empty_trash(file_icon->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static gboolean
xfdesktop_special_file_icon_populate_context_menu(XfdesktopIcon *icon,
                                                  GtkWidget *menu)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GtkWidget *mi, *img;

    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH != special_file_icon->priv->type)
        return FALSE;

    img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Open"), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_special_file_icon_trash_open), icon);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    if(special_file_icon->priv->trash_item_count == 0) {
        img = gtk_image_new_from_icon_name("user-trash", GTK_ICON_SIZE_MENU);
    } else {
        img = gtk_image_new_from_icon_name("user-trash-full", GTK_ICON_SIZE_MENU);
    }

    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Empty Trash"), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(special_file_icon->priv->trash_item_count > 0) {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_special_file_icon_trash_empty),
                         icon);
    } else
        gtk_widget_set_sensitive(mi, FALSE);

    return TRUE;
}

static GFileInfo *
xfdesktop_special_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->file_info;
}

static GFileInfo *
xfdesktop_special_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->filesystem_info;
}

static GFile *
xfdesktop_special_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->file;
}

static void
xfdesktop_special_file_icon_changed(GFileMonitor *monitor,
                                    GFile *file,
                                    GFile *other_file,
                                    GFileMonitorEvent event,
                                    XfdesktopSpecialFileIcon *special_file_icon)
{
    g_return_if_fail(G_IS_FILE_MONITOR(monitor));
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(special_file_icon));

    /* We don't care about change events only created/deleted */
    if(event == G_FILE_MONITOR_EVENT_CHANGED ||
       event == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED ||
       event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        return;

    /* release the old file information */
    if(special_file_icon->priv->file_info) {
        g_object_unref(special_file_icon->priv->file_info);
        special_file_icon->priv->file_info = NULL;
    }

    /* release the old file system information */
    if(special_file_icon->priv->filesystem_info) {
        g_object_unref(special_file_icon->priv->filesystem_info);
        special_file_icon->priv->filesystem_info = NULL;
    }

    /* reload the file information */
    special_file_icon->priv->file_info = g_file_query_info(special_file_icon->priv->file,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    /* reload the file system information */
    special_file_icon->priv->filesystem_info = g_file_query_filesystem_info(special_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    /* update the trash full state */
    if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH)
        xfdesktop_special_file_icon_update_trash_count(special_file_icon);

    /* invalidate the tooltip */
    g_free(special_file_icon->priv->tooltip);
    special_file_icon->priv->tooltip = NULL;

    /* update the icon */
    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(special_file_icon));
    xfdesktop_icon_invalidate_pixbuf(XFDESKTOP_ICON(special_file_icon));
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(special_file_icon));
}

static void
xfdesktop_special_file_icon_update_trash_count(XfdesktopSpecialFileIcon *special_file_icon)
{
    GFileEnumerator *enumerator;
    GFileInfo *f_info;
    gint n = 0;

    g_return_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(special_file_icon));

    if(special_file_icon->priv->file_info == NULL
       || special_file_icon->priv->type != XFDESKTOP_SPECIAL_FILE_ICON_TRASH)
    {
        return;
    }

    /* The trash count may return a number of files the user can't
     * currently delete, for example if the file is in a removable
     * drive that isn't mounted.
     */
    enumerator = g_file_enumerate_children(special_file_icon->priv->file,
                                           G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
                                           G_FILE_QUERY_INFO_NONE,
                                           NULL,
                                           NULL);
    if(enumerator == NULL)
        return;

    for(f_info = g_file_enumerator_next_file(enumerator, NULL, NULL);
        f_info != NULL;
        f_info = g_file_enumerator_next_file(enumerator, NULL, NULL))
    {
          n++;
          g_object_unref(f_info);
    }

    g_file_enumerator_close(enumerator, NULL, NULL);
    g_object_unref(enumerator);

    special_file_icon->priv->trash_item_count = n;
    TRACE("exiting, trash count %d", n);
}

/* public API */

XfdesktopSpecialFileIcon *
xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                GdkScreen *screen)
{
    XfdesktopSpecialFileIcon *special_file_icon;
    GFile *file = NULL;

    switch(type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            file = g_file_new_for_uri("file:///");
            break;

        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            file = g_file_new_for_path(xfce_get_homedir());
            break;

        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            file = g_file_new_for_uri("trash:///");
            break;

        default:
            g_return_val_if_reached(NULL);
    }

    special_file_icon = g_object_new(XFDESKTOP_TYPE_SPECIAL_FILE_ICON, NULL);
    special_file_icon->priv->type = type;
    special_file_icon->priv->gscreen = screen;
    special_file_icon->priv->file = file;

    special_file_icon->priv->file_info = g_file_query_info(file,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    if(!special_file_icon->priv->file_info) {
        g_object_unref(special_file_icon);
        return NULL;
    }

    /* query file system information from GIO */
    special_file_icon->priv->filesystem_info = g_file_query_filesystem_info(special_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);
    /* update the trash full state */
    if(type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH)
        xfdesktop_special_file_icon_update_trash_count(special_file_icon);

    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_icon_invalidate_pixbuf),
                             special_file_icon);

    special_file_icon->priv->monitor = g_file_monitor(special_file_icon->priv->file,
                                                      G_FILE_MONITOR_NONE,
                                                      NULL, NULL);
    if(special_file_icon->priv->monitor) {
        g_signal_connect(special_file_icon->priv->monitor,
                         "changed",
                         G_CALLBACK(xfdesktop_special_file_icon_changed),
                         special_file_icon);
    }

    return special_file_icon;
}

XfdesktopSpecialFileIconType
xfdesktop_special_file_icon_get_icon_type(XfdesktopSpecialFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), -1);
    return icon->priv->type;
}
