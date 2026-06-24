/*
 * Copyright (C) 2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2008 Frank ENDRES <frank_endres@yahoo.fr>
 *
 * Copyright (C) 2008 Fred Chien <fred@lxde.org>
 *               2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2010-2012 Julien Lavergne <julien.lavergne@gmail.com>
 *               2012 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2014 Peter <ombalaxitabou@users.sf.net>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2020 TheZxcv <the.uint64.t@gmail.com>
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

#define _ISOC99_SOURCE /* lrint() */
#define _GNU_SOURCE /* exp10() */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#ifdef DISABLE_ALSA
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#elif defined(HAVE_LINUX_SOUNDCARD_H)
#include <linux/soundcard.h>
#else
#error "Not supported platform"
#endif
#ifndef SOUND_MIXER_PHONEOUT
# define SOUND_MIXER_PHONEOUT SOUND_MIXER_MONO
#endif
//TODO: support OSSv4
#else
#include <alsa/asoundlib.h>
#include <poll.h>
#endif
#include <math.h>
#include <libfm/fm-gtk.h>

#include "plugin.h"
#include "misc.h"
#include "gtk-compat.h"

#define ICONS_VOLUME_HIGH   "volume-high"
#define ICONS_VOLUME_MEDIUM "volume-medium"
#define ICONS_VOLUME_LOW    "volume-low"
#define ICONS_MUTE          "mute"

#ifdef __UCLIBC__
/* 10^x = 10^(log e^x) = (e^x)^log10 = e^(x * log 10) */
# define M_LN10		2.30258509299404568402	/* log_e 10 */
#define exp10(x) (exp((x) * log(10)))
#endif /* __UCLIBC__ */

#define MAX_LINEAR_DB_SCALE 24

#ifdef DISABLE_ALSA
typedef union
{
    struct
    {
        unsigned char left;
        unsigned char right;
    };
    int value;
} StereoVolume;
#endif

typedef struct {

    /* Graphics. */
    GtkWidget * plugin;				/* Back pointer to the widget */
    LXPanel * panel;				/* Back pointer to panel */
    config_setting_t * settings;		/* Plugin settings */
    GtkWidget * tray_icon;			/* Displayed image */
    GtkWidget * popup_window;			/* Top level window for popup */
    GtkWidget * volume_scale;			/* Scale for volume */
    GtkWidget * mute_check;			/* Checkbox for mute state */
    gboolean show_popup;			/* Toggle to show and hide the popup on left click */
    guint volume_scale_handler;			/* Handler for vscale widget */
    guint mute_check_handler;			/* Handler for mute_check widget */

#ifdef DISABLE_ALSA
    int mixer_fd;				/* The mixer FD */
    gdouble vol_before_mute;			/* Save value when muted */

    guint master_channel;
#else
    /* ALSA interface. */
    snd_mixer_t * mixer;			/* The mixer */
    snd_mixer_elem_t * master_element;		/* The Master element */
    guint mixer_evt_idle;			/* Timer to handle restarting poll */
    guint restart_idle;
    gint alsamixer_mapping;

    /* unloading and error handling */
    GIOChannel **channels;                      /* Channels that we listen to */
    guint *watches;                             /* Watcher IDs for channels */
    guint num_channels;                         /* Number of channels */

    gint used_device;
    char *master_channel;
#endif

    /* Icons */
    const char* icon_panel;
    const char* icon_fallback;

    /* Clicks */
    int mute_click;
    GdkModifierType mute_click_mods;
    int mixer_click;
    GdkModifierType mixer_click_mods;
    int slider_click;
    GdkModifierType slider_click_mods;

    /* Hotkeys */
    char * hotkey_up;
    char * hotkey_down;
    char * hotkey_mute;

    GtkWidget *channel_selector;                /* Used by configure dialog */
} VolumeALSAPlugin;

#ifndef DISABLE_ALSA
static gboolean asound_restart(gpointer vol_gpointer);
#endif
static gboolean asound_initialize(VolumeALSAPlugin * vol);
static void asound_deinitialize(VolumeALSAPlugin * vol);
static void volumealsa_update_display(VolumeALSAPlugin * vol);
static void volumealsa_destructor(gpointer user_data);

/*** ALSA ***/

#ifndef DISABLE_ALSA
static gboolean asound_find_element(VolumeALSAPlugin * vol, const char ** ename, int n)
{
    int i;
    snd_mixer_selem_id_t * sid;			/* The element ID */

    snd_mixer_selem_id_alloca(&sid);
    for (i = 0; i < n; i++)
    {
        for (vol->master_element = snd_mixer_first_elem(vol->mixer);
             vol->master_element != NULL;
             vol->master_element = snd_mixer_elem_next(vol->master_element))
        {
            snd_mixer_selem_get_id(vol->master_element, sid);
            if (snd_mixer_selem_is_active(vol->master_element) &&
                strcmp(ename[i], snd_mixer_selem_id_get_name(sid)) == 0)
                    return TRUE;
        }
    }
    return FALSE;
}

/* NOTE by PCMan:
 * This is magic! Since ALSA uses its own machanism to handle this part.
 * After polling of mixer fds, it requires that we should call
 * snd_mixer_handle_events to clear all pending mixer events.
 * However, when using the glib IO channels approach, we don't have
 * poll() and snd_mixer_poll_descriptors_revents(). Due to the design of
 * glib, on_mixer_event() will be called for every fd whose status was
 * changed. So, after each poll(), it's called for several times,
 * not just once. Therefore, we cannot call snd_mixer_handle_events()
 * directly in the event handler. Otherwise, it will get called for
 * several times, which might clear unprocessed pending events in the queue.
 * So, here we call it once in the event callback for the first fd.
 * Then, we don't call it for the following fds. After all fds with changed
 * status are handled, we remove this restriction in an idle handler.
 * The next time the event callback is involked for the first fs, we can
 * call snd_mixer_handle_events() again. Racing shouldn't happen here
 * because the idle handler has the same priority as the io channel callback.
 * So, io callbacks for future pending events should be in the next gmain
 * iteration, and won't be affected.
 */

static gboolean asound_reset_mixer_evt_idle(VolumeALSAPlugin * vol)
{
    if (!g_source_is_destroyed(g_main_current_source()))
        vol->mixer_evt_idle = 0;
    return FALSE;
}

/* Handler for I/O event on ALSA channel. */
static gboolean asound_mixer_event(GIOChannel * channel, GIOCondition cond, gpointer vol_gpointer)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *) vol_gpointer;
    int res = 0;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    if (vol->mixer_evt_idle == 0)
    {
        vol->mixer_evt_idle = g_idle_add_full(G_PRIORITY_DEFAULT, (GSourceFunc) asound_reset_mixer_evt_idle, vol, NULL);
        res = snd_mixer_handle_events(vol->mixer);
    }

    if (cond & G_IO_IN)
    {
        /* the status of mixer is changed. update of display is needed. */
        volumealsa_update_display(vol);
    }

    if ((cond & G_IO_HUP) || (res < 0))
    {
        /* This means there're some problems with alsa. */
        g_warning("volumealsa: ALSA (or pulseaudio) had a problem: "
                "volumealsa: snd_mixer_handle_events() = %d,"
                " cond 0x%x (IN: 0x%x, HUP: 0x%x).", res, cond,
                G_IO_IN, G_IO_HUP);
        gtk_widget_set_tooltip_text(vol->plugin, _("ALSA (or pulseaudio) had a problem."
                " Please check the lxpanel logs."));

        if (vol->restart_idle == 0)
            vol->restart_idle = g_timeout_add_seconds(1, asound_restart, vol);

        return FALSE;
    }

    return TRUE;
}

static gboolean asound_restart(gpointer vol_gpointer)
{
    VolumeALSAPlugin * vol = vol_gpointer;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    asound_deinitialize(vol);

    if (!asound_initialize(vol)) {
        g_warning("volumealsa: Re-initialization failed.");
        return TRUE; // try again in a second
    }

    g_warning("volumealsa: Restarted ALSA interface...");

    vol->restart_idle = 0;
    return FALSE;
}
#endif

/* Initialize the ALSA interface. */
static gboolean asound_initialize(VolumeALSAPlugin * vol)
{
#ifdef DISABLE_ALSA
    /* Access the "default" device. */
    vol->mixer_fd = open ("/dev/mixer", O_RDWR, 0);
    if (vol->mixer_fd < 0)
    {
        g_warning("cannot initialize OSS mixer: %s", strerror(errno));
        return FALSE;
    }

    //FIXME: is there a way to watch volume with OSS?
#else
    snd_mixer_open(&vol->mixer, 0);
    if (vol->used_device < 0)
        snd_mixer_attach(vol->mixer, "default");
    else
    {
        char id[16];

        snprintf(id, sizeof(id), "hw:%d", vol->used_device);
        snd_mixer_attach(vol->mixer, id);
    }
    snd_mixer_selem_register(vol->mixer, NULL, NULL);
    snd_mixer_load(vol->mixer);

    if (vol->master_channel)
    {
        /* If user defined the channel then use it */
        if (!asound_find_element(vol, (const char **)&vol->master_channel, 1))
            return FALSE;
    }
    else
    {
        const char * def_channels[] = { "Master", "Front", "PCM", "LineOut" };
    /* Find Master element, or Front element, or PCM element, or LineOut element.
     * If one of these succeeds, master_element is valid. */
        if (!asound_find_element(vol, def_channels, G_N_ELEMENTS(def_channels)))
        {
            /* Could not find any predefined, let choose any available */
            for (vol->master_element = snd_mixer_first_elem(vol->mixer);
                 vol->master_element != NULL;
                 vol->master_element = snd_mixer_elem_next(vol->master_element))
            {
                if (snd_mixer_selem_is_active(vol->master_element) &&
                    snd_mixer_selem_has_playback_volume(vol->master_element) &&
                    !snd_mixer_selem_has_capture_volume(vol->master_element) &&
                    !snd_mixer_selem_has_capture_switch(vol->master_element))
                    break;
            }
            if (vol->master_element == NULL)
                return FALSE;
        }
    }

    /* Set the playback volume range as we wish it. */
    if ( ! vol->alsamixer_mapping)
        snd_mixer_selem_set_playback_volume_range(vol->master_element, 0, 100);

    /* Listen to events from ALSA. */
    int n_fds = snd_mixer_poll_descriptors_count(vol->mixer);
    struct pollfd * fds = g_new0(struct pollfd, n_fds);

    vol->channels = g_new0(GIOChannel *, n_fds);
    vol->watches = g_new0(guint, n_fds);
    vol->num_channels = n_fds;

    snd_mixer_poll_descriptors(vol->mixer, fds, n_fds);
    int i;
    for (i = 0; i < n_fds; ++i)
    {
        GIOChannel* channel = g_io_channel_unix_new(fds[i].fd);
        vol->watches[i] = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, asound_mixer_event, vol);
        vol->channels[i] = channel;
    }
    g_free(fds);
#endif
    return TRUE;
}

static void asound_deinitialize(VolumeALSAPlugin * vol)
{
#ifdef DISABLE_ALSA
    if (vol->mixer_fd >= 0)
        close(vol->mixer_fd);
    vol->mixer_fd = -1;
#else
    guint i;

    if (vol->mixer_evt_idle != 0) {
        g_source_remove(vol->mixer_evt_idle);
        vol->mixer_evt_idle = 0;
    }

    for (i = 0; i < vol->num_channels; i++) {
        g_source_remove(vol->watches[i]);
        g_io_channel_shutdown(vol->channels[i], FALSE, NULL);
        g_io_channel_unref(vol->channels[i]);
    }
    g_free(vol->channels);
    g_free(vol->watches);
    vol->channels = NULL;
    vol->watches = NULL;
    vol->num_channels = 0;

    if (vol->mixer)
        snd_mixer_close(vol->mixer);
    vol->mixer = NULL;
    vol->master_element = NULL;
#endif
}

/* Get the presence of the mute control from the sound system. */
static gboolean asound_has_mute(VolumeALSAPlugin * vol)
{
#ifdef DISABLE_ALSA
    /* it's emulated with OSS */
    return TRUE;
#else
    return ((vol->master_element != NULL) ? snd_mixer_selem_has_playback_switch(vol->master_element) : FALSE);
#endif
}

/* Get the condition of the mute control from the sound system. */
static gboolean asound_is_muted(VolumeALSAPlugin * vol)
{
    /* The switch is on if sound is not muted, and off if the sound is muted.
     * Initialize so that the sound appears unmuted if the control does not exist. */
    int value = 1;
#ifdef DISABLE_ALSA
    StereoVolume levels;

    ioctl(vol->mixer_fd, MIXER_READ(vol->master_channel), &levels.value);
    value = (levels.left + levels.right) >> 1;
#else
    if (vol->master_element != NULL)
        snd_mixer_selem_get_playback_switch(vol->master_element, 0, &value);
#endif
    return (value == 0);
}

#ifndef DISABLE_ALSA
static long lrint_dir(double x, int dir)
{
    if (dir > 0)
        return lrint(ceil(x));
    else if (dir < 0)
        return lrint(floor(x));
    else
        return lrint(x);
}

static inline gboolean use_linear_dB_scale(long dBmin, long dBmax)
{
    return dBmax - dBmin <= MAX_LINEAR_DB_SCALE * 100;
}

static long get_normalized_volume(snd_mixer_elem_t *elem,
                                    snd_mixer_selem_channel_id_t channel)
{
    long min, max, value;
    double normalized, min_norm;
    int err;

    err = snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
    if (err < 0 || min >= max) {
        err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        if (err < 0 || min == max)
            return 0;

        err = snd_mixer_selem_get_playback_volume(elem, channel, &value);
        if (err < 0)
            return 0;

        return lrint(100.0 * (value - min) / (double)(max - min));
    }

    err = snd_mixer_selem_get_playback_dB(elem, channel, &value);
    if (err < 0)
        return 0;

    if (use_linear_dB_scale(min, max))
        return lrint(100.0 * (value - min) / (double)(max - min));

    normalized = exp10((value - max) / 6000.0);
    if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
        min_norm = exp10((min - max) / 6000.0);
        normalized = (normalized - min_norm) / (1 - min_norm);
    }

    return lrint(100.0 * normalized);
}
#endif

/* Get the volume from the sound system.
 * This implementation returns the average of the Front Left and Front Right channels. */
static int asound_get_volume(VolumeALSAPlugin * vol)
{
#ifdef DISABLE_ALSA
    StereoVolume levels;

    ioctl(vol->mixer_fd, MIXER_READ(vol->master_channel), &levels.value);
    return (levels.left + levels.right) >> 1;
#else
    long aleft = 0;
    long aright = 0;

    if (vol->master_element != NULL)
    {
        if ( ! vol->alsamixer_mapping)
        {
            snd_mixer_selem_get_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, &aleft);
            snd_mixer_selem_get_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, &aright);
        }
        else
        {
            aleft = get_normalized_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT);
            aright = get_normalized_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT);
        }
    }
    return (aleft + aright) >> 1;
#endif
}

#ifndef DISABLE_ALSA
static int set_normalized_volume(snd_mixer_elem_t *elem,
                                 snd_mixer_selem_channel_id_t channel,
                                 int vol,
                                 int dir)
{
    long min, max, value;
    double min_norm, volume;
    int err;

    volume = vol / 100.0;

    err = snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
    if (err < 0 || min >= max) {
        err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        if (err < 0)
            return err;

        value = lrint_dir(volume * (max - min), dir) + min;
        return snd_mixer_selem_set_playback_volume(elem, channel, value);
    }

    if (use_linear_dB_scale(min, max)) {
        value = lrint_dir(volume * (max - min), dir) + min;
        return snd_mixer_selem_set_playback_dB(elem, channel, value, dir);
    }

    if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
        min_norm = exp10((min - max) / 6000.0);
        volume = volume * (1 - min_norm) + min_norm;
    }
    value = lrint_dir(6000.0 * log10(volume), dir) + max;

    return snd_mixer_selem_set_playback_dB(elem, channel, value, dir);
}
#endif

/* Set the volume to the sound system.
 * This implementation sets the Front Left and Front Right channels to the specified value. */
static void asound_set_volume(VolumeALSAPlugin * vol, int volume)
{
    int dir = volume - asound_get_volume(vol);

    /* Volume is set to the correct value already */
    if (dir == 0)
        return;

#ifdef DISABLE_ALSA
    StereoVolume levels;

    levels.left = levels.right = volume;
    ioctl(vol->mixer_fd, MIXER_WRITE(vol->master_channel), &levels.value);
#else
    if (vol->master_element != NULL)
    {
        if ( ! vol->alsamixer_mapping)
        {
            snd_mixer_selem_set_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
            snd_mixer_selem_set_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);
        }
        else
        {
            set_normalized_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, volume, dir);
            set_normalized_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume, dir);
        }
    }
#endif
}

/*** Graphics ***/

static void volumealsa_lookup_current_icon(VolumeALSAPlugin * vol, gboolean mute, int level)
{
    /* Change icon according to mute / volume */
    const char* icon_panel="audio-volume-muted-panel";
    const char* icon_fallback=ICONS_MUTE;
    if (mute)
    {
         icon_panel = "audio-volume-muted-panel";
         icon_fallback=ICONS_MUTE;
    }
    else if (level >= 66)
    {
         icon_panel = "audio-volume-high-panel";
         icon_fallback=ICONS_VOLUME_HIGH;
    }
    else if (level >= 33)
    {
         icon_panel = "audio-volume-medium-panel";
         icon_fallback=ICONS_VOLUME_MEDIUM;
    }
    else if (level > 0)
    {
         icon_panel = "audio-volume-low-panel";
         icon_fallback=ICONS_VOLUME_LOW;
    }

    vol->icon_panel = icon_panel;
    vol->icon_fallback = icon_fallback;
}

static void volumealsa_update_current_icon(VolumeALSAPlugin * vol, gboolean mute, int level)
{
    /* Find suitable icon */
    volumealsa_lookup_current_icon(vol, mute, level);

    /* Change icon, fallback to default icon if theme doesn't exsit */
    lxpanel_image_change_icon(vol->tray_icon, vol->icon_panel, vol->icon_fallback);

    /* Display current level in tooltip. */
    char * tooltip = g_strdup_printf("%s %d", _("Volume control"), level);
    gtk_widget_set_tooltip_text(vol->plugin, tooltip);
    g_free(tooltip);
}

/*
 * Here we just update volume's vertical scale and mute check button.
 * The rest will be updated by signal handelrs.
 */
static void volumealsa_update_display(VolumeALSAPlugin * vol)
{
    /* Mute. */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), asound_is_muted(vol));
    gtk_widget_set_sensitive(vol->mute_check, (asound_has_mute(vol)));

    /* Volume. */
    if (vol->volume_scale != NULL)
    {
        gtk_range_set_value(GTK_RANGE(vol->volume_scale), asound_get_volume(vol));
    }
}

struct mixer_desc
{
    char * cmd;
    char * exec;
    gboolean needs_pa;
    gboolean needs_term;
};

const struct mixer_desc mixers[] = {
    /* those with needs_pa should be first! */
    { "gnome-sound-applet", "gnome-sound-applet", TRUE, FALSE },
    { "pavucontrol", "pavucontrol", TRUE, FALSE },
#ifdef DISABLE_ALSA
    { "xfce4-mixer", "xfce4-mixer", FALSE, FALSE },
    { "aumix", "aumix", FALSE, TRUE },
#else
    { "gnome-alsamixer", "gnome-alsamixer", FALSE, FALSE },
    { "alsamixergui", "alsamixergui", FALSE, FALSE },
    { "alsamixer", "alsamixer", FALSE, TRUE },
#endif
    { NULL }
};

static void volume_run_mixer(VolumeALSAPlugin * vol)
{
    char *path = NULL;
    const gchar *command_line = NULL;
    GAppInfoCreateFlags flags = G_APP_INFO_CREATE_NONE;
    int i;

    /* check if command line was configured */
    if (config_setting_lookup_string(vol->settings, "MixerCommand", &command_line))
        if (config_setting_lookup_int(vol->settings, "MixerCommandTerm", &i) && i)
            flags = G_APP_INFO_CREATE_NEEDS_TERMINAL;

    /* if command isn't set in settings then let guess it */
    if (command_line == NULL)
    {
        i = 0;
        path = g_find_program_in_path("pulseaudio");
        /* Assume that when pulseaudio is installed, it's launching every time */
        if (path)
            g_free(path);
        /* Fallback to alsamixer when PA is not running, or when no PA utility is find */
        else while (mixers[i].cmd && mixers[i].needs_pa)
            i++;
        for (; mixers[i].cmd; i++)
        {
            if ((path = g_find_program_in_path(mixers[i].exec)))
            {
                command_line = mixers[i].cmd;
                if (mixers[i].needs_term)
                    flags = G_APP_INFO_CREATE_NEEDS_TERMINAL;
                g_free(path);
                break;
            }
        }
    }

    if (command_line)
    {
        fm_launch_command_simple(NULL, NULL, flags, command_line, NULL);
    }
    else
    {
        fm_show_error(NULL, NULL,
                      _("Error, you need to install an application to configure"
                        " the sound (pavucontrol, alsamixer ...)"));
    }
}

static void _check_click(VolumeALSAPlugin * vol, int button, GdkModifierType mod)
{
    if (vol->slider_click == button && vol->slider_click_mods == mod)
    {
        /* Left-click.  Show or hide the popup window. */
        if (vol->show_popup)
        {
            gtk_widget_hide(vol->popup_window);
            vol->show_popup = FALSE;
        }
        else
        {
            gtk_widget_show_all(vol->popup_window);
            vol->show_popup = TRUE;
        }
    }
    if (vol->mute_click == button && vol->mute_click_mods == mod)
    {
        /* Middle-click.  Toggle the mute status. */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), ! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vol->mute_check)));
    }
    if (vol->mixer_click == button && vol->mixer_click_mods == mod)
    {
        volume_run_mixer(vol);
    }
}

/* Handler for "button-press-event" signal on main widget. */
static gboolean volumealsa_button_press_event(GtkWidget * widget, GdkEventButton * event, LXPanel * panel)
{
    VolumeALSAPlugin * vol = lxpanel_plugin_get_data(widget);

    if (event->button == 1)
    {
        _check_click(vol, 1,
                     event->state & gtk_accelerator_get_default_mod_mask());
    }

    return FALSE;
}

static gboolean volumealsa_button_release_event(GtkWidget * widget, GdkEventButton * event, VolumeALSAPlugin * vol)
{
    if (event->button != 1)
    {
        _check_click(vol, event->button,
                     event->state & gtk_accelerator_get_default_mod_mask());
    }
    return FALSE;
}

/* Handler for "focus-out" signal on popup window. */
static gboolean volumealsa_popup_focus_out(GtkWidget * widget, GdkEvent * event, VolumeALSAPlugin * vol)
{
    /* Hide the widget. */
    gtk_widget_hide(vol->popup_window);
    vol->show_popup = FALSE;
    return FALSE;
}

/* Handler for "map" signal on popup window. */
static void volumealsa_popup_map(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    lxpanel_plugin_adjust_popup_position(widget, vol->plugin);
}

/* Handler for "value_changed" signal on popup window vertical scale. */
static void volumealsa_popup_scale_changed(GtkRange * range, VolumeALSAPlugin * vol)
{
    int level = gtk_range_get_value(GTK_RANGE(vol->volume_scale));
    gboolean mute = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vol->mute_check));

    /* Reflect the value of the control to the sound system. */
    asound_set_volume(vol, level);

    /*
     * Redraw the controls.
     * Scale and check button do not need to be updated, as these are always
     * in sync with user's actions.
     */
    volumealsa_update_current_icon(vol, mute, level);
}

/* Handler for "scroll-event" signal on popup window vertical scale. */
static void volumealsa_popup_scale_scrolled(GtkScale * scale, GdkEventScroll * evt, VolumeALSAPlugin * vol)
{
    /* Get the state of the vertical scale. */
    gdouble val = gtk_range_get_value(GTK_RANGE(vol->volume_scale));

    /* Dispatch on scroll direction to update the value. */
    if ((evt->direction == GDK_SCROLL_UP) || (evt->direction == GDK_SCROLL_LEFT))
        val += 2;
    else
        val -= 2;

    /* Reset the state of the vertical scale.  This provokes a "value_changed" event. */
    gtk_range_set_value(GTK_RANGE(vol->volume_scale), CLAMP((int)val, 0, 100));
}

/* Handler for "toggled" signal on popup window mute checkbox. */
static void volumealsa_popup_mute_toggled(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    int level = gtk_range_get_value(GTK_RANGE(vol->volume_scale));
    gboolean mute = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vol->mute_check));

    /* Reflect the mute toggle to the sound system. */
#ifdef DISABLE_ALSA
    if (mute)
    {
        vol->vol_before_mute = level;
        asound_set_volume(vol, 0);
    }
    else
    {
        asound_set_volume(vol, vol->vol_before_mute);
    }
#else
    if (vol->master_element != NULL)
    {
        int chn;
        for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++)
            snd_mixer_selem_set_playback_switch(vol->master_element, chn, ((mute) ? 0 : 1));
    }
#endif

    /*
     * Redraw the controls.
     * Scale and check button do not need to be updated, as these are always
     * in sync with user's actions.
     */
    volumealsa_update_current_icon(vol, mute, level);
}

/* Hotkeys handlers */
static void volume_up(const char *keystring, gpointer user_data)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *)user_data;
    int val = (int)gtk_range_get_value(GTK_RANGE(vol->volume_scale)) + 2;
    gtk_range_set_value(GTK_RANGE(vol->volume_scale), CLAMP(val, 0, 100));
}

static void volume_down(const char *keystring, gpointer user_data)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *)user_data;
    int val = (int)gtk_range_get_value(GTK_RANGE(vol->volume_scale)) - 2;
    gtk_range_set_value(GTK_RANGE(vol->volume_scale), CLAMP(val, 0, 100));
}

static void volume_mute(const char *keystring, gpointer user_data)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *)user_data;
    gboolean muted = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vol->mute_check));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), !muted);
}

/* Build the window that appears when the top level widget is clicked. */
static void volumealsa_build_popup_window(GtkWidget *p)
{
    VolumeALSAPlugin * vol = lxpanel_plugin_get_data(p);

    gdouble dpi = gdk_screen_get_resolution(gdk_screen_get_default());
    gint scaling_factor = (int) ceil(dpi / 96);

    /* Create a new window. */
    vol->popup_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(vol->popup_window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(vol->popup_window), 5);
    gtk_window_set_default_size(GTK_WINDOW(vol->popup_window), 80, scaling_factor*140);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(vol->popup_window), GDK_WINDOW_TYPE_HINT_DIALOG);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(vol->popup_window), "focus-out-event", G_CALLBACK(volumealsa_popup_focus_out), vol);
    g_signal_connect(G_OBJECT(vol->popup_window), "map", G_CALLBACK(volumealsa_popup_map), vol);

    /* Create a scrolled window as the child of the top level window. */
    GtkWidget * scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER(scrolledwindow), 0);
    gtk_widget_show(scrolledwindow);
    gtk_container_add(GTK_CONTAINER(vol->popup_window), scrolledwindow);
    gtk_widget_set_can_focus(scrolledwindow, FALSE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_NONE);

    /* Create a viewport as the child of the scrolled window. */
    GtkWidget * viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
    gtk_widget_show(viewport);

    /* Create a frame as the child of the viewport. */
    GtkWidget * frame = gtk_frame_new(_("Volume"));
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(viewport), frame);

    /* Create a vertical box as the child of the frame. */
    GtkWidget * box = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), box);

    /* Create a vertical scale as the child of the vertical box. */
    vol->volume_scale = gtk_vscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 100, 0, 0, 0)));
    gtk_scale_set_draw_value(GTK_SCALE(vol->volume_scale), FALSE);
    gtk_range_set_inverted(GTK_RANGE(vol->volume_scale), TRUE);
    gtk_box_pack_start(GTK_BOX(box), vol->volume_scale, TRUE, TRUE, 0);

    /* Value-changed and scroll-event signals. */
    vol->volume_scale_handler = g_signal_connect(vol->volume_scale, "value-changed", G_CALLBACK(volumealsa_popup_scale_changed), vol);
    g_signal_connect(vol->volume_scale, "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol);

    /* Create a check button as the child of the vertical box. */
    vol->mute_check = gtk_check_button_new_with_label(_("Mute"));
    gtk_box_pack_end(GTK_BOX(box), vol->mute_check, FALSE, FALSE, 0);
    vol->mute_check_handler = g_signal_connect(vol->mute_check, "toggled", G_CALLBACK(volumealsa_popup_mute_toggled), vol);
}

/* Plugin constructor. */
static GtkWidget *volumealsa_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    VolumeALSAPlugin * vol = g_new0(VolumeALSAPlugin, 1);
    GtkWidget *p;
    const char *tmp_str;

#ifndef DISABLE_ALSA
    /* Read config necessary for proper initialization of ALSA. */
    config_setting_lookup_int(settings, "UseAlsamixerVolumeMapping", &vol->alsamixer_mapping);
    if (config_setting_lookup_string(settings, "MasterChannel", &tmp_str))
        vol->master_channel = g_strdup(tmp_str);
    if (!config_setting_lookup_int(settings, "CardNumber", &vol->used_device))
        vol->used_device = -1;
#else
    vol->master_channel = SOUND_MIXER_VOLUME;
    if (config_setting_lookup_string(settings, "MasterChannel", &tmp_str))
    {
        if (strcmp(tmp_str, "PCM") == 0)
            vol->master_channel = SOUND_MIXER_PCM;
        else if (strcmp(tmp_str, "Headphone") == 0)
            vol->master_channel = SOUND_MIXER_PHONEOUT;
    }
#endif
    if (config_setting_lookup_string(settings, "MuteButton", &tmp_str))
        vol->mute_click = panel_config_click_parse(tmp_str, &vol->mute_click_mods);
    else
        vol->mute_click = 2; /* middle-click default */
    if (config_setting_lookup_string(settings, "SliderButton", &tmp_str))
        vol->slider_click = panel_config_click_parse(tmp_str, &vol->slider_click_mods);
    else
        vol->slider_click = 1; /* left-click default */
    if (config_setting_lookup_string(settings, "MixerButton", &tmp_str))
        vol->mixer_click = panel_config_click_parse(tmp_str, &vol->mixer_click_mods);
    if (config_setting_lookup_string(settings, "VolumeUpKey", &tmp_str))
        lxpanel_apply_hotkey(&vol->hotkey_up, tmp_str, volume_up, vol, FALSE);
    if (config_setting_lookup_string(settings, "VolumeDownKey", &tmp_str))
        lxpanel_apply_hotkey(&vol->hotkey_down, tmp_str, volume_down, vol, FALSE);
    if (config_setting_lookup_string(settings, "VolumeMuteKey", &tmp_str))
        lxpanel_apply_hotkey(&vol->hotkey_mute, tmp_str, volume_mute, vol, FALSE);

    /* Initialize ALSA.  If that fails, present nothing. */
    if ( ! asound_initialize(vol))
    {
        volumealsa_destructor(vol);
        return NULL;
    }

    /* Allocate top level widget and set into Plugin widget pointer. */
    vol->panel = panel;
    vol->plugin = p = gtk_event_box_new();
    vol->settings = settings;
    lxpanel_plugin_set_data(p, vol, volumealsa_destructor);
    gtk_widget_set_tooltip_text(p, _("Volume control"));

    /* Allocate icon as a child of top level. */
    vol->tray_icon = lxpanel_image_new_for_icon(panel, "audio-volume-muted-panel",
                                                -1, ICONS_MUTE);
    gtk_container_add(GTK_CONTAINER(p), vol->tray_icon);

    /* Initialize window to appear when icon clicked. */
    volumealsa_build_popup_window(p);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(p), "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol );
    g_signal_connect(G_OBJECT(p), "button-release-event", G_CALLBACK(volumealsa_button_release_event), vol );

    /* Update the display, show the widget, and return. */
    volumealsa_update_display(vol);
    volumealsa_update_current_icon(vol, asound_is_muted(vol), asound_get_volume(vol));
    gtk_widget_show_all(p);
    return p;
}

/* Plugin destructor. */
static void volumealsa_destructor(gpointer user_data)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *) user_data;

    lxpanel_apply_hotkey(&vol->hotkey_up, NULL, NULL, NULL, FALSE);
    lxpanel_apply_hotkey(&vol->hotkey_down, NULL, NULL, NULL, FALSE);
    lxpanel_apply_hotkey(&vol->hotkey_mute, NULL, NULL, NULL, FALSE);

    asound_deinitialize(vol);

    /* If the dialog box is open, dismiss it. */
    if (vol->popup_window != NULL)
        gtk_widget_destroy(vol->popup_window);

#ifndef DISABLE_ALSA
    if (vol->restart_idle)
        g_source_remove(vol->restart_idle);

    g_free(vol->master_channel);
#endif

    /* Deallocate all memory. */
    g_free(vol);
}

#ifndef DISABLE_ALSA
static GtkListStore *alsa_make_channels_list(VolumeALSAPlugin *vol, int *active)
{
    GtkListStore *list;
    GtkTreeIter iter;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    const char *name;
    int i;

    snd_mixer_selem_id_alloca(&sid);
    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING); /* desc, value */
    for (elem = snd_mixer_first_elem(vol->mixer), i = 0; elem != NULL;
         elem = snd_mixer_elem_next(elem), i++)
    {
        if (snd_mixer_selem_is_active(elem) &&
            snd_mixer_selem_has_playback_volume(elem) &&
            !snd_mixer_selem_has_capture_volume(elem) &&
            !snd_mixer_selem_has_capture_switch(elem))
        {
            snd_mixer_selem_get_id(elem, sid);
            name = snd_mixer_selem_id_get_name(sid);
            gtk_list_store_insert_with_values(list, &iter, i, 0, _(name),
                                                              1, name, -1);
            if (elem == vol->master_element)
                *active = i;
        }
    }
    return list;
}

static void card_selector_changed(GtkComboBox *card_selector, VolumeALSAPlugin *vol)
{
    GtkTreeModel *model = gtk_combo_box_get_model(card_selector);
    GtkTreeIter iter;
    int old_card = vol->used_device;
    int i = gtk_combo_box_get_active(card_selector);
    char *old_channel;

    gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
    gtk_tree_model_get(model, &iter, 1, &vol->used_device, -1);
    asound_deinitialize(vol);
    if (!asound_initialize(vol))
    {
        /* could not change card with the same master channel, try default */
        old_channel = vol->master_channel;
        vol->master_channel = NULL;
        asound_deinitialize(vol);
        if (!asound_initialize(vol))
        {
            g_warning("could not set card to %d", vol->used_device);
            vol->master_channel = old_channel;
            vol->used_device = old_card;
            //FIXME: reset the selector back
            /* schedule to restart with old settings */
            if (vol->restart_idle == 0)
                vol->restart_idle = g_timeout_add_seconds(1, asound_restart, vol);
            return;
        }
        g_free(old_channel);
        config_group_set_string(vol->settings, "MasterChannel", NULL);
    }
    /* remember and apply selection */
    volumealsa_update_display(vol);
    config_group_set_int(vol->settings, "CardNumber", vol->used_device);
    /* rebuild channel selection list */
    i = -1;
    model = GTK_TREE_MODEL(alsa_make_channels_list(vol, &i));
    gtk_combo_box_set_model(GTK_COMBO_BOX(vol->channel_selector), model);
    gtk_combo_box_set_active(GTK_COMBO_BOX(vol->channel_selector), i);
    g_object_unref(model);
}
#endif

static void channel_selector_changed(GtkComboBox *channel_selector, VolumeALSAPlugin *vol)
{
    GtkTreeModel *model = gtk_combo_box_get_model(channel_selector);
    GtkTreeIter iter;
#ifdef DISABLE_ALSA
    int ch; /* channel index */
#else
    char *ch; /* channel name */
#endif
    int i = gtk_combo_box_get_active(channel_selector);

    gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
    gtk_tree_model_get(model, &iter, 1, &ch, -1);
#ifdef DISABLE_ALSA
    config_group_set_int(vol->settings, "MasterChannel", ch);
#else
    config_group_set_string(vol->settings, "MasterChannel", ch);
    asound_find_element(vol, (const char **)&ch, 1); //FIXME: is error possible?
    /* Set the playback volume range as we wish it. */
    if (!vol->alsamixer_mapping)
        snd_mixer_selem_set_playback_volume_range(vol->master_element, 0, 100);
    /* g_debug("MasterChannel changed: %s", ch); */
    g_free(vol->master_channel);
#endif
    vol->master_channel = ch; /* just take it instead of alloc + free */
    volumealsa_update_display(vol);
}

static void mixer_selector_changed(GtkComboBox *mixer_selector, VolumeALSAPlugin *vol)
{
    GtkWidget *mixer_entry = gtk_bin_get_child(GTK_BIN(mixer_selector));
    const char *cmd, *set;
    GtkTreeModel *model;
    GtkTreeIter iter;
    int i;

    i = gtk_combo_box_get_active(mixer_selector);
    if (i < 0)
        /* it was just editing */
        return;
    if (!config_setting_lookup_string(vol->settings, "MixerCommand", &set))
        set = NULL;
    cmd = gtk_entry_get_text((GtkEntry *)mixer_entry);
    if (set)
    {
        if (strcmp(set, cmd) == 0)
            /* not changed */
            return;
    }
    else if (gtk_combo_box_get_active(mixer_selector) == 0)
        /* it's left at default */
        return;
    model = gtk_combo_box_get_model(mixer_selector);
    gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
    gtk_tree_model_get(model, &iter, 1, &i, -1);
    /* g_debug("new choice: %s needs_term=%d", cmd, i); */
    config_group_set_string(vol->settings, "MixerCommand", cmd);
    config_group_set_int(vol->settings, "MixerCommandTerm", i);
}

struct mixer_selector_check_data
{
    GtkComboBox *mixer_selector;
    const char *text;
    int needs_term;
};

static gboolean mixer_selector_check(GtkTreeModel *model, GtkTreePath *path,
                                     GtkTreeIter *iter, gpointer user_data)
{
    struct mixer_selector_check_data *data = user_data;
    char *cmd;

    gtk_tree_model_get(model, iter, 0, &cmd, 1, &data->needs_term, -1);
    if (cmd && strcmp(cmd, data->text) == 0)
    {
        int *indices = gtk_tree_path_get_indices(path);
        gtk_combo_box_set_active(data->mixer_selector, indices[0]);
        g_free(cmd);
        return TRUE;
    }
    g_free(cmd);
    return FALSE;
}

static gboolean mixer_selector_focus_out(GtkWidget *mixer_entry,
                                         GdkEvent *evt, VolumeALSAPlugin *vol)
{
    struct mixer_selector_check_data data;
    GtkTreeModel *model;

    data.mixer_selector = GTK_COMBO_BOX(gtk_widget_get_parent(mixer_entry));
    data.text = gtk_entry_get_text((GtkEntry *)mixer_entry);
    data.needs_term = 0;
    model = gtk_combo_box_get_model(data.mixer_selector);

    /* check if current value is one of model choices */
    if (gtk_combo_box_get_active(data.mixer_selector) < 0)
        gtk_tree_model_foreach(model, &mixer_selector_check, &data);
    /* check executable and remember selection */
    if (gtk_combo_box_get_active(data.mixer_selector) < 0)
    {
        /* check only user input since predefined choices were tested already */
        char *exec, *path;

        /* g_debug("user entered mixer: %s", data.text); */
        exec = strchr(data.text, ' ');
        if (exec)
            exec = g_strndup(data.text, exec - data.text);
        path = g_find_program_in_path(exec ? exec : data.text);
        g_free(exec);
        g_free(path);
        if (path == NULL)
        {
            /* invalid executable requested, ignore it then */
            g_warning("%s cannot be executed, ignoring it", data.text);
            return FALSE;
        }
    }
    config_group_set_string(vol->settings, "MixerCommand", data.text);
    config_group_set_int(vol->settings, "MixerCommandTerm", data.needs_term);
    return FALSE;
}

static gboolean mixer_selector_key_press(GtkWidget *mixer_entry,
                                         GdkEventKey *evt, VolumeALSAPlugin *vol)
{
    if (evt->keyval == GDK_KEY_Return)
        /* loose focus on Enter press */
        gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(mixer_entry)), NULL);
    return FALSE;
}

static gboolean mute_button_changed(GtkWidget *btn, char *click, VolumeALSAPlugin *vol)
{
    int n;
    GdkModifierType mods;

    n = panel_config_click_parse(click, &mods);
    if (n == 0 || ((n != vol->mixer_click || mods != vol->mixer_click_mods) &&
                   (n != vol->slider_click || mods != vol->slider_click_mods)))
    {
        config_group_set_string(vol->settings, "MuteButton", click);
        vol->mute_click = n;
        vol->mute_click_mods = mods;
        return TRUE;
    }
    //FIXME: show a message?
    return FALSE;
}

static gboolean mixer_button_changed(GtkWidget *btn, char *click, VolumeALSAPlugin *vol)
{
    int n;
    GdkModifierType mods;

    n = panel_config_click_parse(click, &mods);
    if (n == 0 || ((n != vol->mute_click || mods != vol->mute_click_mods) &&
                   (n != vol->slider_click || mods != vol->slider_click_mods)))
    {
        config_group_set_string(vol->settings, "MixerButton", click);
        vol->mixer_click = n;
        vol->mixer_click_mods = mods;
        return TRUE;
    }
    //FIXME: show a message?
    return FALSE;
}

static gboolean volume_button_changed(GtkWidget *btn, char *click, VolumeALSAPlugin *vol)
{
    int n;
    GdkModifierType mods;

    n = panel_config_click_parse(click, &mods);
    if (n == 0 || ((n != vol->mixer_click || mods != vol->mixer_click_mods) &&
                   (n != vol->mute_click || mods != vol->mute_click_mods)))
    {
        config_group_set_string(vol->settings, "SliderButton", click);
        vol->slider_click = n;
        vol->slider_click_mods = mods;
        return TRUE;
    }
    //FIXME: show a message?
    return FALSE;
}

static gboolean up_key_changed(GtkWidget *btn, char *click, VolumeALSAPlugin *vol)
{
    gboolean res;

    res = lxpanel_apply_hotkey(&vol->hotkey_up, click, &volume_up, vol, TRUE);
    if (res)
        config_group_set_string(vol->settings, "VolumeUpKey", click);
    return res;
}

static gboolean down_key_changed(GtkWidget *btn, char *click, VolumeALSAPlugin *vol)
{
    gboolean res;

    res = lxpanel_apply_hotkey(&vol->hotkey_down, click, &volume_down, vol, TRUE);
    if (res)
        config_group_set_string(vol->settings, "VolumeDownKey", click);
    return res;
}

static gboolean mute_key_changed(GtkWidget *btn, char *click, VolumeALSAPlugin *vol)
{
    gboolean res;

    res = lxpanel_apply_hotkey(&vol->hotkey_mute, click, &volume_mute, vol, TRUE);
    if (res)
        config_group_set_string(vol->settings, "VolumeMuteKey", click);
    return res;
}

#if THING_THAT_NEVER_HAPPEN
/* Just to have these translated */
N_("Line"), N_("LineOut"), N_("Front"), N_("Surround"), N_("Center"), N_("Speaker+LO");
#endif

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *volumealsa_configure(LXPanel *panel, GtkWidget *p)
{
    VolumeALSAPlugin *vol = lxpanel_plugin_get_data(p);
    const char *tmp_str;
    char *path;
    GtkListStore *list;
    GtkCellRenderer *column;
#ifndef DISABLE_ALSA
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    snd_hctl_t *hctl;
    GtkWidget *card_selector;
#endif
    GtkWidget *mute_button;
    GtkWidget *volume_button;
    GtkWidget *mixer_button;
    GtkWidget *up_key;
    GtkWidget *down_key;
    GtkWidget *mute_key;
    GtkWidget *mixer_selector;
    GtkWidget *mixer_entry;
    GtkTreeIter iter;
    int active = 0;
    int i = 0;
    int j = -1;

#ifndef DISABLE_ALSA
    snd_mixer_selem_id_alloca(&sid);
    /* setup card selector */
    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT); /* desc, num */
    if (snd_hctl_open(&hctl, "default", 0) == 0)
    {
        /* check if "default" isn't a HW, so include it in the list */
        if (snd_ctl_type(snd_hctl_ctl(hctl)) != 0)
        {
            gtk_list_store_insert_with_values(list, &iter, i++, 0, _("default"),
                                                                1, j, -1);
            if (vol->used_device < 0)
                active = 0;
        }
        snd_hctl_close(hctl);
    }
    while (snd_card_next(&j) == 0 && j >= 0)
    {
        char *name = NULL;
        snd_mixer_t *mixer;
        char id[16];

        if (snd_card_get_name(j, &name) == 0)
        {
            /* test if there any available channel */
            snprintf(id, sizeof(id), "hw:%d", j);
            snd_mixer_open(&mixer, 0);
            snd_mixer_attach(mixer, id);
            snd_mixer_selem_register(mixer, NULL, NULL);
            snd_mixer_load(mixer);
            for (elem = snd_mixer_first_elem(mixer); elem != NULL;
                 elem = snd_mixer_elem_next(elem))
            {
                if (snd_mixer_selem_is_active(elem) &&
                    snd_mixer_selem_has_playback_volume(elem) &&
                    !snd_mixer_selem_has_capture_volume(elem) &&
                    !snd_mixer_selem_has_capture_switch(elem))
                    break;
            }
            snd_mixer_close(mixer);
            if (elem != NULL)
            {
                g_debug("found soundcard: %s", name);
                gtk_list_store_insert_with_values(list, &iter, i++, 0, name,
                                                                    1, j, -1);
                if (vol->used_device == j)
                    active = i;
            }
            else
                g_debug("no elements in soundcard %s", name);
            free(name);
        }
    }
    card_selector = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list));
    g_object_unref(list);
    /* gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(card_selector), 1); */
    column = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(card_selector), column, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(card_selector), column,
                                   "text", 0, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(card_selector), active);
    g_signal_connect(card_selector, "changed",
                     G_CALLBACK(card_selector_changed), vol);
    g_signal_connect(card_selector, "scroll-event", G_CALLBACK(gtk_true), NULL);
#endif

    /* setup channel selector */
#ifdef DISABLE_ALSA
    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT); /* desc, index */
    gtk_list_store_insert_with_values(list, &iter, 0, 0, _("Master"),
                                                      1, SOUND_MIXER_VOLUME, -1);
    if (vol->master_channel == SOUND_MIXER_VOLUME)
        active = 0;
    gtk_list_store_insert_with_values(list, &iter, 1, 0, _("PCM"),
                                                      1, SOUND_MIXER_PCM, -1);
    if (vol->master_channel == SOUND_MIXER_VOLUME)
        active = 1;
    gtk_list_store_insert_with_values(list, &iter, 2, 0, _("Headphone"),
                                                      1, SOUND_MIXER_PHONEOUT, -1);
    if (vol->master_channel == SOUND_MIXER_VOLUME)
        active = 2;
#else
    list = alsa_make_channels_list(vol, &active);
#endif
    vol->channel_selector = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list));
    g_object_unref(list);
    /* gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(vol->channel_selector), 1); */
    column = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(vol->channel_selector), column, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(vol->channel_selector), column,
                                   "text", 0, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(vol->channel_selector), active);
    g_signal_connect(vol->channel_selector, "changed",
                     G_CALLBACK(channel_selector_changed), vol);
    g_signal_connect(vol->channel_selector, "scroll-event", G_CALLBACK(gtk_true), NULL);

    /* setup buttons */
    if (!config_setting_lookup_string(vol->settings, "SliderButton", &tmp_str))
        tmp_str = "1";
    volume_button = panel_config_click_button_new(_("Click for Volume Slider"), tmp_str);
    g_signal_connect(volume_button, "changed", G_CALLBACK(volume_button_changed), vol);
    if (!config_setting_lookup_string(vol->settings, "MuteButton", &tmp_str))
        tmp_str = "2";
    mute_button = panel_config_click_button_new(_("Click for Toggle Mute"), tmp_str);
    g_signal_connect(mute_button, "changed", G_CALLBACK(mute_button_changed), vol);
    if (!config_setting_lookup_string(vol->settings, "MixerButton", &tmp_str))
        tmp_str = NULL;
    mixer_button = panel_config_click_button_new(_("Click for Open Mixer"), tmp_str);
    g_signal_connect(mixer_button, "changed", G_CALLBACK(mixer_button_changed), vol);

    /* setup hotkeys */
    up_key = panel_config_hotkey_button_new(_("Hotkey for Volume Up"), vol->hotkey_up);
    g_signal_connect(up_key, "changed", G_CALLBACK(up_key_changed), vol);
    down_key = panel_config_hotkey_button_new(_("Hotkey for Volume Down"), vol->hotkey_down);
    g_signal_connect(down_key, "changed", G_CALLBACK(down_key_changed), vol);
    mute_key = panel_config_hotkey_button_new(_("Hotkey for Volume Mute"), vol->hotkey_mute);
    g_signal_connect(mute_key, "changed", G_CALLBACK(mute_key_changed), vol);

    /* setup mixer selector */
    if (!config_setting_lookup_string(vol->settings, "MixerCommand", &tmp_str))
        tmp_str = NULL;
    active = -1;
    i = j = 0;
    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT); /* line, needs_term */
    path = g_find_program_in_path("pulseaudio");
    if (path)
        g_free(path);
    else while (mixers[i].cmd && mixers[i].needs_pa)
        i++;
    for (; mixers[i].cmd; i++)
    {
        path = g_find_program_in_path(mixers[i].exec);
        if (path)
        {
            if (tmp_str && active < 0 && strcmp(tmp_str, mixers[i].cmd) == 0)
            {
                active = j;
                tmp_str = NULL;
            }
            gtk_list_store_insert_with_values(list, &iter, j++, 0, mixers[i].cmd,
                                                                1, (int)mixers[i].needs_term,
                                                                -1);
            g_free(path);
        }
    }
    if (tmp_str)
    {
        active = j;
        /* FIXME: support "needs terminal" for custom MixerCommand */
        gtk_list_store_insert_with_values(list, &iter, j, 0, tmp_str, 1, 0, -1);
    }
    if (active < 0)
        active = 0;
#if GTK_CHECK_VERSION(2, 24, 0)
    mixer_selector = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(list));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(mixer_selector), 0);
#else
    mixer_selector = gtk_combo_box_entry_new_with_model(GTK_TREE_MODEL(list), 0);
#endif
    g_object_unref(list);
    /* gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(mixer_selector), 1); */
    gtk_combo_box_set_active(GTK_COMBO_BOX(mixer_selector), active);
    mixer_entry = gtk_bin_get_child(GTK_BIN(mixer_selector));
    g_signal_connect(mixer_entry, "key-press-event",
                     G_CALLBACK(mixer_selector_key_press), vol);
    g_signal_connect(mixer_selector, "changed",
                     G_CALLBACK(mixer_selector_changed), vol);
    g_signal_connect(mixer_entry, "focus-out-event",
                     G_CALLBACK(mixer_selector_focus_out), vol);
    g_signal_connect(mixer_selector, "scroll-event", G_CALLBACK(gtk_true), NULL);

    return lxpanel_generic_config_dlg(_("Volume Control"), panel, NULL, p,
#ifndef DISABLE_ALSA
                                      _("Audio Card"), NULL, CONF_TYPE_TRIM,
                                      "", card_selector, CONF_TYPE_EXTERNAL,
#endif
                                      _("Channel to Operate"), NULL, CONF_TYPE_TRIM,
                                      "", vol->channel_selector, CONF_TYPE_EXTERNAL,
                                      "", volume_button, CONF_TYPE_EXTERNAL,
                                      "", mute_button, CONF_TYPE_EXTERNAL,
                                      "", mixer_button, CONF_TYPE_EXTERNAL,
                                      "", up_key, CONF_TYPE_EXTERNAL,
                                      "", down_key, CONF_TYPE_EXTERNAL,
                                      "", mute_key, CONF_TYPE_EXTERNAL,
                                      _("Command to Open Mixer"), NULL, CONF_TYPE_TRIM,
                                      "", mixer_selector, CONF_TYPE_EXTERNAL,
                                      NULL);
}

/* Callback when panel configuration changes. */
static void volumealsa_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    /* Do a full redraw. */
    volumealsa_update_display(lxpanel_plugin_get_data(p));
}

static gboolean volumealsa_update_context_menu(GtkWidget *plugin, GtkMenu *menu)
{
    GtkWidget *img = gtk_image_new_from_stock("gtk-directory", GTK_ICON_SIZE_MENU);
    GtkWidget *menu_item = gtk_image_menu_item_new_with_label(_("Launch Mixer"));
    //FIXME: precheck and disable if MixerCommand not set
    gtk_image_menu_item_set_image((GtkImageMenuItem *)menu_item, img);
    g_signal_connect_swapped(menu_item, "activate", G_CALLBACK(volume_run_mixer),
                             lxpanel_plugin_get_data(plugin));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    return FALSE;
}

#ifndef DISABLE_ALSA
static LXPanelPluginInit _volumealsa_init = {
    .name = N_("Volume Control"),
    .description = N_("Display and control volume"),

    .superseded = TRUE,
    .new_instance = volumealsa_constructor,
    .config = volumealsa_configure,
    .reconfigure = volumealsa_panel_configuration_changed,
    .update_context_menu = volumealsa_update_context_menu,
    .button_press_event = volumealsa_button_press_event
};

static void volumealsa_init(void)
{
    lxpanel_register_plugin_type("volumealsa", &_volumealsa_init);
}
#endif

FM_DEFINE_MODULE(lxpanel_gtk, volume)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Volume Control"),
    .description = N_("Display and control volume"),
#ifndef DISABLE_ALSA
    .init = volumealsa_init,
#endif

    .new_instance = volumealsa_constructor,
    .config = volumealsa_configure,
    .reconfigure = volumealsa_panel_configuration_changed,
    .update_context_menu = volumealsa_update_context_menu,
    .button_press_event = volumealsa_button_press_event
};

/* vim: set sw=4 et sts=4 : */
