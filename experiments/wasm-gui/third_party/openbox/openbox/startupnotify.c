/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   startupnotify.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "startupnotify.h"
#include "gettext.h"
#include "event.h"
#include "obt/xqueue.h"

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#ifndef USE_LIBSN

void sn_startup(gboolean reconfig) {}
void sn_shutdown(gboolean reconfig) {}
gboolean sn_app_starting() { return FALSE; }
Time sn_app_started(const gchar *id, const gchar *wmclass, const gchar *name)
{
    return CurrentTime;
}
gboolean sn_get_desktop(gchar *id, guint *desktop) { return FALSE; }
void sn_setup_spawn_environment(const gchar *program, const gchar *name,
                                const gchar *icon_name, const gchar *wmclass,
                                gint desktop) {}
void sn_spawn_cancel() {}

#else

#include "openbox.h"
#include "screen.h"

#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>

static SnDisplay *sn_display;
static SnMonitorContext *sn_context;
static SnLauncherContext *sn_launcher;
static GSList *sn_waits; /* list of SnStartupSequences we're waiting on */

static SnStartupSequence* sequence_find(const gchar *id);

static void sn_handler(const XEvent *e, gpointer data);
static void sn_event_func(SnMonitorEvent *event, gpointer data);

void sn_startup(gboolean reconfig)
{
    if (reconfig) return;

    sn_display = sn_display_new(obt_display, NULL, NULL);
    sn_context = sn_monitor_context_new(sn_display, ob_screen,
                                        sn_event_func, NULL, NULL);
    sn_launcher = sn_launcher_context_new(sn_display, ob_screen);

    xqueue_add_callback(sn_handler, NULL);
}

void sn_shutdown(gboolean reconfig)
{
    GSList *it;

    if (reconfig) return;

    xqueue_remove_callback(sn_handler, NULL);

    for (it = sn_waits; it; it = g_slist_next(it))
        sn_startup_sequence_unref((SnStartupSequence*)it->data);
    g_slist_free(sn_waits);
    sn_waits = NULL;

    screen_set_root_cursor();

    sn_launcher_context_unref(sn_launcher);
    sn_monitor_context_unref(sn_context);
    sn_display_unref(sn_display);
}

static SnStartupSequence* sequence_find(const gchar *id)
{
    SnStartupSequence*ret = NULL;
    GSList *it;

    for (it = sn_waits; it; it = g_slist_next(it)) {
        SnStartupSequence *seq = it->data;
        if (!strcmp(id, sn_startup_sequence_get_id(seq))) {
            ret = seq;
            break;
        }
    }
    return ret;
}

gboolean sn_app_starting(void)
{
    return sn_waits != NULL;
}

static gboolean sn_wait_timeout(gpointer data)
{
    SnStartupSequence *seq = data;
    sn_waits = g_slist_remove(sn_waits, seq);
    screen_set_root_cursor();
    return FALSE; /* don't repeat */
}

static void sn_handler(const XEvent *e, gpointer data)
{
    XEvent ec;
    ec = *e;
    sn_display_process_event(sn_display, &ec);
}

static void sn_event_func(SnMonitorEvent *ev, gpointer data)
{
    SnStartupSequence *seq;
    gboolean change = FALSE;

    if (!(seq = sn_monitor_event_get_startup_sequence(ev)))
        return;

    switch (sn_monitor_event_get_type(ev)) {
    case SN_MONITOR_EVENT_INITIATED:
        sn_startup_sequence_ref(seq);
        sn_waits = g_slist_prepend(sn_waits, seq);
        /* 20 second timeout for apps to start if the launcher doesn't
           have a timeout */
        g_timeout_add_full(G_PRIORITY_DEFAULT,
                           20 * 1000, sn_wait_timeout, seq,
                           (GDestroyNotify)sn_startup_sequence_unref);
        change = TRUE;
        break;
    case SN_MONITOR_EVENT_CHANGED:
        /* XXX feedback changed? */
        change = TRUE;
        break;
    case SN_MONITOR_EVENT_COMPLETED:
    case SN_MONITOR_EVENT_CANCELED:
        if ((seq = sequence_find(sn_startup_sequence_get_id(seq)))) {
            sn_waits = g_slist_remove(sn_waits, seq);
            g_source_remove_by_user_data(seq);
            change = TRUE;
        }
        break;
    };

    if (change)
        screen_set_root_cursor();
}

Time sn_app_started(const gchar *id, const gchar *wmclass, const gchar *name)
{
    GSList *it;
    Time t = CurrentTime;

    if (!id && !wmclass)
        return t;

    for (it = sn_waits; it; it = g_slist_next(it)) {
        SnStartupSequence *seq = it->data;
        gboolean found = FALSE;
        const gchar *seqid, *seqclass, *seqbin;
        seqid = sn_startup_sequence_get_id(seq);
        seqclass = sn_startup_sequence_get_wmclass(seq);
        seqbin = sn_startup_sequence_get_binary_name(seq);

        if (id && seqid) {
            /* if the app has a startup id, then look for that for highest
               accuracy */
            if (!strcmp(seqid, id))
                found = TRUE;
        }
        else if (seqclass) {
            /* seqclass = "a string to match against the "resource name" or
               "resource class" hints.  These are WM_CLASS[0] and WM_CLASS[1]"
               - from the startup-notification spec
            */
            found = (seqclass && !strcmp(seqclass, wmclass)) ||
                (seqclass && !strcmp(seqclass, name));
        }
        else if (seqbin) {
            /* Check the binary name against the class and name hints
               as well, to help apps that don't have the class set
               correctly */
            found = (seqbin && !g_ascii_strcasecmp(seqbin, wmclass)) ||
                (seqbin && !g_ascii_strcasecmp(seqbin, name));
        }

        if (found) {
            sn_startup_sequence_complete(seq);
            t = sn_startup_sequence_get_timestamp(seq);
            break;
        }
    }
    return t;
}

gboolean sn_get_desktop(gchar *id, guint *desktop)
{
    SnStartupSequence *seq;

    if (id && (seq = sequence_find(id))) {
        gint desk = sn_startup_sequence_get_workspace(seq);
        if (desk != -1) {
            *desktop = desk;
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean sn_launch_wait_timeout(gpointer data)
{
    SnLauncherContext *sn = data;
    sn_launcher_context_complete(sn);
    return FALSE; /* don't repeat */
}

void sn_setup_spawn_environment(const gchar *program, const gchar *name,
                                const gchar *icon_name, const gchar *wmclass,
                                gint desktop)
{
    gchar *desc;
    const char *id;

    desc = g_strdup_printf(_("Running %s"), program);

    if (sn_launcher_context_get_initiated(sn_launcher)) {
        sn_launcher_context_unref(sn_launcher);
        sn_launcher = sn_launcher_context_new(sn_display, ob_screen);
    }

    sn_launcher_context_set_name(sn_launcher, name ? name : program);
    sn_launcher_context_set_description(sn_launcher, desc);
    sn_launcher_context_set_icon_name(sn_launcher, icon_name ?
                                      icon_name : program);
    sn_launcher_context_set_binary_name(sn_launcher, program);
    if (wmclass) sn_launcher_context_set_wmclass(sn_launcher, wmclass);
    if (desktop >= 0 && (unsigned) desktop < screen_num_desktops)
        sn_launcher_context_set_workspace(sn_launcher, (signed) desktop);
    sn_launcher_context_initiate(sn_launcher, "openbox", program,
                                 event_time());
    id = sn_launcher_context_get_startup_id(sn_launcher);

    /* 20 second timeout for apps to start */
    sn_launcher_context_ref(sn_launcher);
    g_timeout_add_full(G_PRIORITY_DEFAULT,
                       20 * 1000, sn_launch_wait_timeout, sn_launcher,
                       (GDestroyNotify)sn_launcher_context_unref);

    g_setenv("DESKTOP_STARTUP_ID", id, TRUE);

    g_free(desc);
}

void sn_spawn_cancel(void)
{
    sn_launcher_context_complete(sn_launcher);
}

#endif
