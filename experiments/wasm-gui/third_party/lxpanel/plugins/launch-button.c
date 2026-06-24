/**
 * Copyright (C) 2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "launch-button.h"
#include "misc.h"

#include <string.h>

/* Representative of one launch button.
 * Note that the launch parameters come from the specified desktop file, or from the configuration file.
 * This structure is also used during the "add to launchtaskbar" dialog to hold menu items. */
struct _LaunchButton
{
    GtkEventBox parent;
    LXPanel * panel;                    /* Back pointer to panel (grandparent widget) */
    GtkWidget * plugin;                 /* Back pointer to the plugin */
    FmJob * job;                        /* Async job to retrieve file info */
    FmFileInfo * fi;                    /* Launcher application descriptor */
    config_setting_t * settings;        /* Button settings */
};


static void launch_button_job_finished(FmJob *job, LaunchButton *self)
{
    GtkWidget *image;

    if (self->job == NULL)
        return; // duplicate call? seems a bug in libfm

    if (FM_IS_FILE_INFO_JOB(job))
    {
        /* absolute path */
        self->fi = fm_file_info_list_pop_head(FM_FILE_INFO_JOB(job)->file_infos);
    }
    else
    {
        /* search for id */
        self->fi = fm_file_info_list_pop_head(FM_DIR_LIST_JOB(job)->files);
    }
    self->job = NULL;
    g_object_unref(job);
    if (self->fi == NULL)
    {
        g_warning("launchbar: desktop entry does not exist");
        return;
    }
    image = lxpanel_image_new_for_fm_icon(self->panel, fm_file_info_get_icon(self->fi),
                                          -1, NULL);
    lxpanel_button_compose(GTK_WIDGET(self), image, NULL, NULL);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self), fm_file_info_get_disp_name(self->fi));
}


/* -----------------------------------------------------------------------------
 * Class implementation
 */

G_DEFINE_TYPE(LaunchButton, launch_button, GTK_TYPE_EVENT_BOX)

static void launch_button_dispose(GObject *object)
{
    LaunchButton *self = (LaunchButton *)object;

    if (self->job)
    {
        g_signal_handlers_disconnect_by_func(self->job,
                                             launch_button_job_finished, object);
        fm_job_cancel(self->job);
        self->job = NULL;
    }

    if (self->fi)
    {
        fm_file_info_unref(self->fi);
        self->fi = NULL;
    }

    G_OBJECT_CLASS(launch_button_parent_class)->dispose(object);
}

static gboolean launch_button_release_event(GtkWidget *widget, GdkEventButton *event)
{
    LaunchButton *btn = PANEL_LAUNCH_BUTTON(widget);

    if (event->button == 1) /* left button */
    {
        if (btn->job) /* The job is still running */
            ;
        else if (btn->fi == NULL)  /* The bootstrap button */
            lxpanel_plugin_show_config_dialog(btn->plugin);
        else
            lxpanel_launch_path(btn->panel, fm_file_info_get_path(btn->fi));
        return TRUE;
    }
    return FALSE;
}

static void launch_button_init(LaunchButton *self)
{
    gtk_container_set_border_width(GTK_CONTAINER(self), 0);
    gtk_widget_set_can_focus(GTK_WIDGET(self), FALSE);
}

static void launch_button_class_init(LaunchButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = launch_button_dispose;
    widget_class->button_release_event = launch_button_release_event;
}


/* -----------------------------------------------------------------------------
 * Interface functions
 */

/* creates new button */
LaunchButton *launch_button_new(LXPanel *panel, GtkWidget *plugin, FmPath *id,
                                config_setting_t *settings)
{
    LaunchButton *self = g_object_new(PANEL_TYPE_LAUNCH_BUTTON, NULL);
    GtkWidget *image;

    self->panel = panel;
    self->plugin = plugin;
    self->settings = settings;
    if (id == NULL)
    {
        /* a bootstrap button */
        image = lxpanel_image_new_for_icon(panel, GTK_STOCK_ADD, -1, NULL);
        lxpanel_button_compose(GTK_WIDGET(self), image, NULL, NULL);
    }
    else
    {
        /* g_debug("LaunchButton: trying file %s in scheme %s", fm_path_get_basename(id),
                fm_path_get_basename(fm_path_get_scheme_path(id))); */
        if (fm_path_is_native(id) ||
            strncmp(fm_path_get_basename(fm_path_get_scheme_path(id)), "search:", 7) != 0)
        {
            FmFileInfoJob *job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_NONE);

            fm_file_info_job_add(job, id);
            self->job = FM_JOB(job);
        }
        else /* it is a search job */
        {
            FmDirListJob *job = fm_dir_list_job_new2(id, FM_DIR_LIST_JOB_FAST);

            self->job = FM_JOB(job);
        }
        g_signal_connect(self->job, "finished",
                         G_CALLBACK(launch_button_job_finished), self);
        if (!fm_job_run_async(self->job))
        {
            g_object_unref(self->job);
            self->job = NULL;
            gtk_widget_destroy(GTK_WIDGET(self));
            g_warning("launchbar: problem running file search job");
            return NULL;
        }
    }
    return self;
}

FmFileInfo *launch_button_get_file_info(LaunchButton *btn)
{
    if (PANEL_IS_LAUNCH_BUTTON(btn))
        return btn->fi;
    return NULL;
}

const char *launch_button_get_disp_name(LaunchButton *btn)
{
    if (PANEL_IS_LAUNCH_BUTTON(btn) && btn->fi != NULL)
        return fm_file_info_get_disp_name(btn->fi);
    return NULL;
}

FmIcon *launch_button_get_icon(LaunchButton *btn)
{
    if (PANEL_IS_LAUNCH_BUTTON(btn) && btn->fi != NULL)
        return fm_file_info_get_icon(btn->fi);
    return NULL;
}

config_setting_t *launch_button_get_settings(LaunchButton *btn)
{
    if (PANEL_IS_LAUNCH_BUTTON(btn))
        return btn->settings;
    return NULL;
}

void launch_button_set_settings(LaunchButton *btn, config_setting_t *settings)
{
    if (PANEL_IS_LAUNCH_BUTTON(btn))
        btn->settings = settings;
}

/**
 * launch_button_wait_load
 * @btn: a button instance
 *
 * If @btn does not have pending file info then returns. Otherwise waits
 * for it. If loading the info failed then destroys @btn and associated
 * settings.
 *
 * Returns: %TRUE if button is fully loaded.
 *
 * Since: 0.9.0
 */
gboolean launch_button_wait_load(LaunchButton *btn)
{
    if (!PANEL_IS_LAUNCH_BUTTON(btn) || btn->job == NULL)
        return TRUE;
    if (fm_job_run_sync(btn->job))
        return TRUE;

    if (btn->settings)
        config_setting_destroy(btn->settings);
    gtk_widget_destroy(GTK_WIDGET(btn));
    return FALSE;
}
