/*
 * Copyright (C) 2006-2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2009 Marty Jack <martyj19@comcast.net>
 *               2010 Julien Lavergne <julien.lavergne@gmail.com>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>

#include <glib/gi18n.h>
#include <libfm/fm-gtk.h>

#include "space.h"

#include "private.h"

struct _PanelSpace
{
    GtkEventBox parent;
    config_setting_t *settings;
    FmDndDest *dd;
    int size;
};

struct _PanelSpaceClass
{
    GtkEventBoxClass parent_class;
};


/* Drag&drop support to create launchers */
enum {
    LAUNCHER_DND_TARGET = N_FM_DND_DEST_DEFAULT_TARGETS
};

static const GtkTargetEntry dnd_targets[] = {
    { "application/x-lxpanel-launcher", GTK_TARGET_SAME_APP, LAUNCHER_DND_TARGET }
};

static GdkAtom launcher_dnd_atom;

static gboolean panel_space_drag_drop(GtkWidget *widget, GdkDragContext *context,
                                      gint x, gint y, guint time)
{
    PanelSpace *sp = PANEL_SPACE(widget);
    GdkAtom target;

    target = gtk_drag_dest_find_target(widget, context, NULL);
    if (target == launcher_dnd_atom)
    {
        /* request for data, it will be processed on "drag-data-received" signal */
        gtk_drag_get_data(widget, context, launcher_dnd_atom, time);
        return TRUE;
    }
    target = fm_dnd_dest_find_target(sp->dd, context);
    if (G_LIKELY(target != GDK_NONE))
        return fm_dnd_dest_drag_drop(sp->dd, context, target, x, y, time);
    return FALSE;
}

static gboolean panel_space_make_launcher(GtkWidget *widget, gint x, gint y, const char *str)
{
    PanelSpace *sp = PANEL_SPACE(widget);
    GtkWidget *panel_box, *launchbar, *sp2;
    LXPanel *panel;
    config_setting_t *cfg;
    LXPanelPluginInit *init;
    char *cmd;
    GtkAllocation alloc;
    int idx = -1, size, size2 = 0;
    int icon_size;
    gboolean expand = FALSE;

    /* find position of this widget */
    panel_box = gtk_widget_get_parent(widget);
    gtk_container_child_get(GTK_CONTAINER(panel_box), widget, "position", &idx,
                                                              "expand", &expand,
                                                              NULL);
    if (idx < 1)
        // g_warning
        return FALSE;
    panel = PLUGIN_PANEL(widget);
    /* prepare config for new widget and create launcher before PS */
    cfg = config_group_add_subgroup(config_root_setting(panel->priv->config),
                                    "Plugin");
    config_group_set_string(cfg, "type", "launchbar");
    config_setting_move_elem(cfg, config_setting_get_parent(cfg), idx + 1);
    launchbar = lxpanel_add_plugin(panel, "launchbar", cfg, idx);
    if (launchbar == NULL) /* failed to create */
    {
        config_setting_destroy(cfg);
        return FALSE;
    }
    init = PLUGIN_CLASS(launchbar);
    if (!init->control) /* cannot create a launcher */
    {
        lxpanel_remove_plugin(panel, launchbar);
        return FALSE;
    }
    if (strncmp(str, "menu://applications/", 20) == 0)
        cmd = g_strdup_printf("add %s", strrchr(str, '/') + 1);
    else
        cmd = g_strdup_printf("add %s", str);
    if (!init->control(launchbar, cmd)) /* failed to create a launcher */
    {
        g_free(cmd);
        lxpanel_remove_plugin(panel, launchbar);
        return FALSE;
    }
    /* success */
    g_free(cmd);
    /* now to find where to insert the launcher */
    icon_size = panel_get_icon_size(panel);
    if (!expand && sp->size <= icon_size/2 + 4) //just drop this PS
    {
        lxpanel_remove_plugin(panel, widget);
        return TRUE;
    }
    gtk_widget_get_allocation(widget, &alloc);
    if (panel_get_orientation(panel) == GTK_ORIENTATION_HORIZONTAL)
    {
        size = alloc.width;
    }
    else
    {
        size = alloc.height;
        x = y; /* use x below as a position value */
    }
    /* g_debug("making launcher at %d on PanelSpace of size %d", x, size); */
    if (x <= icon_size/2 + 4) //leave launchbar at idx (before PS), size -= icon_size+3
    {
        lxpanel_config_save(panel);
    }
    else if (x >= size - icon_size/2 - 4) //move launchbar to idx+1 (after PS), size -= icon_size+3
    {
        gtk_box_reorder_child(GTK_BOX(panel_box), launchbar, idx + 1);
        config_setting_move_elem(cfg, config_setting_get_parent(cfg), idx + 2);
        lxpanel_config_save(panel);
    }
    else if (expand && x < size/2) //create another PS at idx of size pos-icon_size/2-2, shifting launchbar
    {
        cfg = config_group_add_subgroup(config_root_setting(panel->priv->config),
                                        "Plugin");
        config_group_set_string(cfg, "type", "space");
        sp2 = lxpanel_add_plugin(panel, "space", cfg, idx);
        size2 = x - icon_size/2 - 2;
        /* g_debug("adding new PanelSpace of size %d before Launcher", size2); */
        config_setting_move_elem(cfg, config_setting_get_parent(cfg), idx + 1);
        if (sp2 == NULL)
            //FIXME: is it ever possible?
            config_setting_destroy(cfg);
        else
            _panel_space_resize(sp2, size2);
    }
    else //move launchbar to idx+1, then create another PS at idx+2 of size size-pos-icon_size/2-2
    {
        gtk_box_reorder_child(GTK_BOX(panel_box), launchbar, idx + 1);
        config_setting_move_elem(cfg, config_setting_get_parent(cfg), idx + 2);
        cfg = config_group_add_subgroup(config_root_setting(panel->priv->config),
                                        "Plugin");
        config_group_set_string(cfg, "type", "space");
        sp2 = lxpanel_add_plugin(panel, "space", cfg, idx + 2);
        size2 = size - x - icon_size/2 - 2;
        /* g_debug("adding new PanelSpace of size %d after Launcher", size2); */
        config_setting_move_elem(cfg, config_setting_get_parent(cfg), idx + 3);
        if (sp2 == NULL)
            //FIXME: is it ever possible?
            config_setting_destroy(cfg);
        else
            _panel_space_resize(sp2, size2);
    }
    if (!expand) //resize to sp->size - icon_size - 3 - size2, then queue resize
    {
        /* g_debug("resizing this space to %d", sp->size - icon_size - 3 - size2); */
        _panel_space_resize(widget, sp->size - icon_size - 3 - size2);
    }
    return TRUE;
}

static void panel_space_drag_data_received(GtkWidget *widget,
                                           GdkDragContext *context, gint x,
                                           gint y, GtkSelectionData *sel_data,
                                           guint info, guint time)
{
    char *str;

    switch(info)
    {
    case LAUNCHER_DND_TARGET:
        /* get data from selection */
        str = (char *)gtk_selection_data_get_data(sel_data);
        if (str == NULL)
            break;
        /* try to make a launcher */
        if (panel_space_make_launcher(widget, x, y, str))
            gtk_drag_finish(context, TRUE, TRUE, time);
        else
            gtk_drag_finish(context, FALSE, FALSE, time);
        break;
    default:
        fm_dnd_dest_drag_data_received(PANEL_SPACE(widget)->dd, context, x, y,
                                       sel_data, info, time);
    }
}

/* Handler for "drag-motion" event from launchtaskbar button. */
static gboolean panel_space_drag_motion(GtkWidget *widget,
                                        GdkDragContext *context, gint x, gint y,
                                        guint time)
{
    PanelSpace *sp = PANEL_SPACE(widget);
    GdkAtom target;
    GdkDragAction action = 0;

    target = fm_dnd_dest_find_target(sp->dd, context);
    if (target == GDK_NONE)
    {
        target = gtk_drag_dest_find_target(widget, context, NULL);
        if (target == launcher_dnd_atom)
            action = GDK_ACTION_MOVE; /* see launchbar plugin */
    }
    else if (fm_dnd_dest_is_target_supported(sp->dd, target))
    {
        fm_dnd_dest_get_default_action(sp->dd, context, target);
        action = GDK_ACTION_COPY;
    }
    gdk_drag_status(context, action, time);

    return TRUE;
}

static void panel_space_drag_leave(GtkWidget *widget,
                                   GdkDragContext *drag_context, guint time)
{
    fm_dnd_dest_drag_leave(PANEL_SPACE(widget)->dd, drag_context, time);
}

static gboolean panel_space_files_dropped(FmDndDest *dd, int x, int y, GdkDragAction action,
                                          FmDndDestTargetType info_type,
                                          FmPathList *files, GtkWidget *sp)
{
    FmPath *path;
    char *path_str;

    if (action != GDK_ACTION_COPY)
        return FALSE;
    path = fm_path_list_peek_head(files);
    if (!path)
        return FALSE;
    path_str = fm_path_to_str(path);
    /* g_debug("*** path '%s' pos %d", path_str, i); */
    panel_space_make_launcher(sp, x, y, path_str);
    g_free(path_str);
    return TRUE;
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void panel_space_get_preferred_size(GtkWidget *widget,
                                           gint *minimal_width,
                                           gint *natural_width)
{
    PanelSpace *p = PANEL_SPACE(widget);

    if (minimal_width)
        *minimal_width = 2;
    if (natural_width)
        *natural_width = p->size;
}
#else
static void panel_space_size_request(GtkWidget *widget,
                                     GtkRequisition *requisition)
{
    PanelSpace *p = PANEL_SPACE(widget);

    requisition->width = requisition->height = p->size;
}
#endif

G_DEFINE_TYPE(PanelSpace, panel_space, GTK_TYPE_EVENT_BOX)

static void panel_space_dispose(GObject *object)
{
    PanelSpace *self = (PanelSpace *)object;

    if (self->dd)
    {
        g_signal_handlers_disconnect_by_func(self->dd,
                                             panel_space_files_dropped, self);
        g_object_unref(self->dd);
        self->dd = NULL;
    }

    G_OBJECT_CLASS(panel_space_parent_class)->dispose(object);
}

static void panel_space_class_init(PanelSpaceClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

#if GTK_CHECK_VERSION(3, 0, 0)
    widget_class->get_preferred_width = panel_space_get_preferred_size;
    widget_class->get_preferred_height = panel_space_get_preferred_size;
#else
    widget_class->size_request = panel_space_size_request;
#endif
    widget_class->drag_drop = panel_space_drag_drop;
    widget_class->drag_data_received = panel_space_drag_data_received;
    widget_class->drag_motion = panel_space_drag_motion;
    widget_class->drag_leave = panel_space_drag_leave;

    object_class->dispose = panel_space_dispose;

    launcher_dnd_atom = gdk_atom_intern_static_string("application/x-lxpanel-launcher");
}

static void panel_space_init(PanelSpace *self)
{
    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
    self->dd = fm_dnd_dest_new(GTK_WIDGET(self));
    fm_dnd_dest_add_targets(GTK_WIDGET(self), dnd_targets, G_N_ELEMENTS(dnd_targets));
    g_signal_connect(self->dd, "files-dropped",
                     G_CALLBACK(panel_space_files_dropped), self);
}

/* Plugin constructor. */
GtkWidget *_panel_space_new(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    PanelSpace * p = g_object_new(PANEL_TYPE_SPACE, NULL);

    /* Load parameters from the configuration file. */
    config_setting_lookup_int(settings, "Size", &p->size);

    /* Save construction pointers */
    p->settings = settings;

    /* Default the size parameter. */
    if (p->size == 0)
        p->size = 2;

    return GTK_WIDGET(p);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean space_apply_configuration(gpointer user_data)
{
    PanelSpace * p = user_data;

    /* Apply settings. */
    gtk_widget_queue_resize(user_data);
    /* Save config values */
    config_group_set_int(p->settings, "Size", p->size);
    return FALSE;
}

void _panel_space_resize(GtkWidget *spacer, gint size)
{
    PanelSpace * p = PANEL_SPACE(spacer);

    p->size = MAX(0, size);
    space_apply_configuration(p);
    lxpanel_config_save(PLUGIN_PANEL(spacer));
}

gint _panel_space_get_size(GtkWidget *spacer)
{
    return PANEL_SPACE(spacer)->size;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *space_configure(LXPanel *panel, GtkWidget *instance)
{
    PanelSpace * p = PANEL_SPACE(instance);
    GtkWidget * dlg;

    dlg = lxpanel_generic_config_dlg(_("Spacer"), panel,
                                     space_apply_configuration, instance,
                                     _("Size"), &p->size, CONF_TYPE_INT, NULL);
    gtk_widget_set_size_request(dlg, 200, -1);	/* Improve geometry */
    return dlg;
}

/* Plugin descriptor. */
LXPanelPluginInit _lxpanel_static_plugin_space = {
    .name = N_("Spacer"),
    .description = N_("Allocate space"),

    /* Stretch is available but not default for this plugin. */
    .expand_available = TRUE,

    .new_instance = _panel_space_new,
    .config = space_configure,
};
