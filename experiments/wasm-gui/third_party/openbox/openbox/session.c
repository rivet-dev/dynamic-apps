/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   session.c for the Openbox window manager
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

/* This session code is largely inspired by metacity code. */

#include "session.h"

struct _ObClient;

GList *session_saved_state = NULL;
gint session_desktop = -1;
gint session_num_desktops = 0;
gboolean session_desktop_layout_present = FALSE;
ObDesktopLayout session_desktop_layout;
GSList *session_desktop_names = NULL;

#ifndef USE_SM
void session_startup(gint argc, gchar **argv) {}
void session_shutdown(gboolean permanent) {}
GList* session_state_find(struct _ObClient *c) { return NULL; }
void session_request_logout(gboolean silent) {}
gboolean session_connected(void) { return FALSE; }
#else

#include "debug.h"
#include "openbox.h"
#include "client.h"
#include "focus.h"
#include "gettext.h"
#include "obt/xml.h"
#include "obt/paths.h"

#include <time.h>
#include <errno.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#  include <sys/types.h>
#  include <unistd.h>
#endif

#include <X11/SM/SMlib.h>

#define SM_ERR_LEN 1024

static SmcConn  sm_conn;
static gint     sm_argc;
static gchar  **sm_argv;

/* Data saved from the first level save yourself */
typedef struct {
    ObClient *focus_client;
    gint      desktop;
} ObSMSaveData;

static gboolean session_connect();

static void session_load_file(const gchar *path);
static gboolean session_save_to_file(const ObSMSaveData *savedata);

static void session_setup_program();
static void session_setup_user();
static void session_setup_restart_style(gboolean restart);
static void session_setup_pid();
static void session_setup_priority();
static void session_setup_clone_command();
static void session_setup_restart_command();

static void sm_save_yourself(SmcConn conn, SmPointer data, gint save_type,
                             Bool shutdown, gint interact_style, Bool fast);
static void sm_die(SmcConn conn, SmPointer data);
static void sm_save_complete(SmcConn conn, SmPointer data);
static void sm_shutdown_cancelled(SmcConn conn, SmPointer data);

static gboolean session_state_cmp(ObSessionState *s, ObClient *c);
static void session_state_free(ObSessionState *state);

void session_startup(gint argc, gchar **argv)
{
    gchar *dir;
    ObtPaths *p;

    if (!ob_sm_use) return;

    sm_argc = argc;
    sm_argv = argv;

    p = obt_paths_new();
    dir = g_build_filename(obt_paths_cache_home(p),
                           "openbox", "sessions", NULL);
    obt_paths_unref(p), p = NULL;

    if (!obt_paths_mkdir_path(dir, 0700)) {
        g_message(_("Unable to make directory \"%s\": %s"),
                  dir, g_strerror(errno));
    }

    if (ob_sm_save_file != NULL) {
        if (ob_sm_restore) {
            ob_debug_type(OB_DEBUG_SM, "Loading from session file %s",
                          ob_sm_save_file);
            session_load_file(ob_sm_save_file);
        }
    } else {
        gchar *filename;

        /* this algo is from metacity */
        filename = g_strdup_printf("%u-%u-%u.obs",
                                   (guint)time(NULL),
                                   (guint)getpid(),
                                   g_random_int());
        ob_sm_save_file = g_build_filename(dir, filename, NULL);
        g_free(filename);
    }

    if (session_connect()) {
        session_setup_program();
        session_setup_user();
        session_setup_restart_style(TRUE);
        session_setup_pid();
        session_setup_priority();
        session_setup_clone_command();
    }

    g_free(dir);
}

void session_shutdown(gboolean permanent)
{
    if (!ob_sm_use) return;

    if (sm_conn) {
        /* if permanent is true then we will change our session state so that
           the SM won't run us again */
        if (permanent)
            session_setup_restart_style(FALSE);

        SmcCloseConnection(sm_conn, 0, NULL);

        while (session_saved_state) {
            session_state_free(session_saved_state->data);
            session_saved_state = g_list_delete_link(session_saved_state,
                                                     session_saved_state);
        }
    }
}

gboolean session_connected(void)
{
    return !!sm_conn;
}

/*! Connect to the session manager and set up our callback functions */
static gboolean session_connect(void)
{
    SmcCallbacks cb;
    gchar *oldid;
    gchar sm_err[SM_ERR_LEN];

    /* set up our callback functions */
    cb.save_yourself.callback = sm_save_yourself;
    cb.save_yourself.client_data = NULL;
    cb.die.callback = sm_die;
    cb.die.client_data = NULL;
    cb.save_complete.callback = sm_save_complete;
    cb.save_complete.client_data = NULL;
    cb.shutdown_cancelled.callback = sm_shutdown_cancelled;
    cb.shutdown_cancelled.client_data = NULL;

    /* connect to the server */
    oldid = ob_sm_id;
    ob_debug_type(OB_DEBUG_SM, "Connecting to SM with id: %s",
                  oldid ? oldid : "(null)");
    sm_conn = SmcOpenConnection(NULL, NULL, 1, 0,
                                SmcSaveYourselfProcMask |
                                SmcDieProcMask |
                                SmcSaveCompleteProcMask |
                                SmcShutdownCancelledProcMask,
                                &cb, oldid, &ob_sm_id,
                                SM_ERR_LEN-1, sm_err);
    g_free(oldid);
    ob_debug_type(OB_DEBUG_SM, "Connected to SM with id: %s", ob_sm_id);
    if (sm_conn == NULL)
        ob_debug("Failed to connect to session manager: %s", sm_err);
    return sm_conn != NULL;
}

static void session_setup_program(void)
{
    SmPropValue vals = {
        .value = sm_argv[0],
        .length = strlen(sm_argv[0]) + 1
    };
    SmProp prop = {
        .name = g_strdup(SmProgram),
        .type = g_strdup(SmARRAY8),
        .num_vals = 1,
        .vals = &vals
    };
    SmProp *list = &prop;
    ob_debug_type(OB_DEBUG_SM, "Setting program: %s", sm_argv[0]);
    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
}

static void session_setup_user(void)
{
    char *user = g_strdup(g_get_user_name());

    SmPropValue vals = {
        .value = user,
        .length = strlen(user) + 1
    };
    SmProp prop = {
        .name = g_strdup(SmUserID),
        .type = g_strdup(SmARRAY8),
        .num_vals = 1,
        .vals = &vals
    };
    SmProp *list = &prop;
    ob_debug_type(OB_DEBUG_SM, "Setting user: %s", user);
    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
    g_free(user);
}

static void session_setup_restart_style(gboolean restart)
{
    gchar restart_hint = restart ? SmRestartImmediately : SmRestartIfRunning;

    SmPropValue vals = {
        .value = &restart_hint,
        .length = 1
    };
    SmProp prop = {
        .name = g_strdup(SmRestartStyleHint),
        .type = g_strdup(SmCARD8),
        .num_vals = 1,
        .vals = &vals
    };
    SmProp *list = &prop;
    ob_debug_type(OB_DEBUG_SM, "Setting restart: %d", restart);
    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
}

static void session_setup_pid(void)
{
    gchar *pid = g_strdup_printf("%ld", (glong) getpid());

    SmPropValue vals = {
        .value = pid,
        .length = strlen(pid) + 1
    };
    SmProp prop = {
        .name = g_strdup(SmProcessID),
        .type = g_strdup(SmARRAY8),
        .num_vals = 1,
        .vals = &vals
    };
    SmProp *list = &prop;
    ob_debug_type(OB_DEBUG_SM, "Setting pid: %s", pid);
    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
    g_free(pid);
}

/*! This is a gnome-session-manager extension */
static void session_setup_priority(void)
{
    gchar priority = 20; /* 20 is a lower prioity to run before other apps */

    SmPropValue vals = {
        .value = &priority,
        .length = 1
    };
    SmProp prop = {
        .name = g_strdup("_GSM_Priority"),
        .type = g_strdup(SmCARD8),
        .num_vals = 1,
        .vals = &vals
    };
    SmProp *list = &prop;
    ob_debug_type(OB_DEBUG_SM, "Setting priority: %d", priority);
    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
}

static void session_setup_clone_command(void)
{
    gint i;

    SmPropValue *vals = g_new(SmPropValue, sm_argc);
    SmProp prop = {
        .name = g_strdup(SmCloneCommand),
        .type = g_strdup(SmLISTofARRAY8),
        .num_vals = sm_argc,
        .vals = vals
    };
    SmProp *list = &prop;

    ob_debug_type(OB_DEBUG_SM, "Setting clone command: (%d)", sm_argc);
    for (i = 0; i < sm_argc; ++i) {
        vals[i].value = sm_argv[i];
        vals[i].length = strlen(sm_argv[i]) + 1;
        ob_debug_type(OB_DEBUG_SM, "    %s", vals[i].value);
    }

    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
    g_free(vals);
}

static void session_setup_restart_command(void)
{
    gint i;

    SmPropValue *vals = g_new(SmPropValue, sm_argc + 4);
    SmProp prop = {
        .name = g_strdup(SmRestartCommand),
        .type = g_strdup(SmLISTofARRAY8),
        .num_vals = sm_argc + 4,
        .vals = vals
    };
    SmProp *list = &prop;

    ob_debug_type(OB_DEBUG_SM, "Setting restart command: (%d)", sm_argc+4);
    for (i = 0; i < sm_argc; ++i) {
        vals[i].value = sm_argv[i];
        vals[i].length = strlen(sm_argv[i]) + 1;
        ob_debug_type(OB_DEBUG_SM, "    %s", vals[i].value);
    }

    vals[i].value = g_strdup("--sm-client-id");
    vals[i].length = strlen("--sm-client-id") + 1;
    vals[i+1].value = ob_sm_id;
    vals[i+1].length = strlen(ob_sm_id) + 1;
    ob_debug_type(OB_DEBUG_SM, "    %s", vals[i].value);
    ob_debug_type(OB_DEBUG_SM, "    %s", vals[i+1].value);

    vals[i+2].value = g_strdup("--sm-save-file");
    vals[i+2].length = strlen("--sm-save-file") + 1;
    vals[i+3].value = ob_sm_save_file;
    vals[i+3].length = strlen(ob_sm_save_file) + 1;
    ob_debug_type(OB_DEBUG_SM, "    %s", vals[i+2].value);
    ob_debug_type(OB_DEBUG_SM, "    %s", vals[i+3].value);

    SmcSetProperties(sm_conn, 1, &list);
    g_free(prop.name);
    g_free(prop.type);
    g_free(vals[i].value);
    g_free(vals[i+2].value);
    g_free(vals);
}

static ObSMSaveData *sm_save_get_data(void)
{
    ObSMSaveData *savedata = g_slice_new0(ObSMSaveData);
    /* save the active desktop and client.
       we don't bother to preemptively save the other desktop state like
       number and names of desktops, cuz those shouldn't be changing during
       the save.. */
    savedata->focus_client = focus_client;
    savedata->desktop = screen_desktop;
    return savedata;
}

static void sm_save_yourself_2(SmcConn conn, SmPointer data)
{
    gboolean success;
    ObSMSaveData *savedata = data;

    /* save the current state */
    ob_debug_type(OB_DEBUG_SM, "Session save phase 2 requested");
    ob_debug_type(OB_DEBUG_SM,
                  "  Saving session to file '%s'", ob_sm_save_file);
    if (savedata == NULL)
        savedata = sm_save_get_data();
    success = session_save_to_file(savedata);
    g_slice_free(ObSMSaveData, savedata);

    /* tell the session manager how to restore this state */
    if (success) session_setup_restart_command();

    ob_debug_type(OB_DEBUG_SM, "Saving is done (success = %d)", success);
    SmcSaveYourselfDone(conn, success);
}

static void sm_save_yourself(SmcConn conn, SmPointer data, gint save_type,
                             Bool shutdown, gint interact_style, Bool fast)
{
    ObSMSaveData *savedata = NULL;
    gchar *vendor;

#ifdef DEBUG
    {
        const gchar *sname =
            (save_type == SmSaveLocal ? "SmSaveLocal" :
             (save_type == SmSaveGlobal ? "SmSaveGlobal" :
              (save_type == SmSaveBoth ? "SmSaveBoth" : "INVALID!!")));
        ob_debug_type(OB_DEBUG_SM, "Session save requested, type %s", sname);
    }
#endif

    if (save_type == SmSaveGlobal) {
        /* we have no data to save.  we only store state to get back to where
           we were, we don't keep open writable files or anything */
        SmcSaveYourselfDone(conn, TRUE);
        return;
    }

    vendor = SmcVendor(sm_conn);
    ob_debug_type(OB_DEBUG_SM, "Session manager's vendor: %s", vendor);

    if (!strcmp(vendor, "KDE")) {
        /* ksmserver guarantees that phase 1 will complete before allowing any
           clients interaction, so we can save this sanely here before they
           get messed up from interaction */
        savedata = sm_save_get_data();
    }
    free(vendor);

    if (!SmcRequestSaveYourselfPhase2(conn, sm_save_yourself_2, savedata)) {
        ob_debug_type(OB_DEBUG_SM, "Requst for phase 2 failed");
        g_slice_free(ObSMSaveData, savedata);
        SmcSaveYourselfDone(conn, FALSE);
    }
}

static void sm_die(SmcConn conn, SmPointer data)
{
    ob_debug_type(OB_DEBUG_SM, "Die requested");
    ob_exit(0);
}

static void sm_save_complete(SmcConn conn, SmPointer data)
{
    ob_debug_type(OB_DEBUG_SM, "Save complete");
}

static void sm_shutdown_cancelled(SmcConn conn, SmPointer data)
{
    ob_debug_type(OB_DEBUG_SM, "Shutdown cancelled");
}

static gboolean session_save_to_file(const ObSMSaveData *savedata)
{
    FILE *f;
    GList *it;
    gboolean success = TRUE;

    f = fopen(ob_sm_save_file, "w");
    if (!f) {
        success = FALSE;
        g_message(_("Unable to save the session to \"%s\": %s"),
                  ob_sm_save_file, g_strerror(errno));
    } else {
        fprintf(f, "<?xml version=\"1.0\"?>\n\n");
        fprintf(f, "<openbox_session>\n\n");

        fprintf(f, "<desktop>%d</desktop>\n", savedata->desktop);

        fprintf(f, "<numdesktops>%d</numdesktops>\n", screen_num_desktops);

        fprintf(f, "<desktoplayout>\n");
        fprintf(f, "  <orientation>%d</orientation>\n",
                screen_desktop_layout.orientation);
        fprintf(f, "  <startcorner>%d</startcorner>\n",
                screen_desktop_layout.start_corner);
        fprintf(f, "  <columns>%d</columns>\n",
                screen_desktop_layout.columns);
        fprintf(f, "  <rows>%d</rows>\n",
                screen_desktop_layout.rows);
        fprintf(f, "</desktoplayout>\n");

        if (screen_desktop_names) {
            gint i;
            gchar *t;

            fprintf(f, "<desktopnames>\n");
            for (i = 0; screen_desktop_names[i]; ++i){
                t = g_markup_escape_text(screen_desktop_names[i], -1);
                fprintf(f, "  <name>%s</name>\n", t);
                g_free(t);
            }
            fprintf(f, "</desktopnames>\n");
        }

        /* they are ordered top to bottom in stacking order */
        for (it = stacking_list; it; it = g_list_next(it)) {
            gint prex, prey, prew, preh;
            ObClient *c;
            gchar *t;

            if (WINDOW_IS_CLIENT(it->data))
                c = WINDOW_AS_CLIENT(it->data);
            else
                continue;

            if (!client_normal(c))
                continue;

            if (!c->sm_client_id) {
                ob_debug_type(OB_DEBUG_SM, "Client %s does not have a "
                              "session id set",
                              c->title);
                if (!c->wm_command) {
                    ob_debug_type(OB_DEBUG_SM, "Client %s does not have an "
                                  "oldskool wm_command set either. We won't "
                                  "be saving its data",
                                  c->title);
                    continue;
                }
            }

            ob_debug_type(OB_DEBUG_SM, "Saving state for client %s",
                          c->title);

            prex = c->area.x;
            prey = c->area.y;
            prew = c->area.width;
            preh = c->area.height;
            if (c->fullscreen) {
                prex = c->pre_fullscreen_area.x;
                prey = c->pre_fullscreen_area.x;
                prew = c->pre_fullscreen_area.width;
                preh = c->pre_fullscreen_area.height;
            }
            if (c->max_horz) {
                prex = c->pre_max_area.x;
                prew = c->pre_max_area.width;
            }
            if (c->max_vert) {
                prey = c->pre_max_area.y;
                preh = c->pre_max_area.height;
            }

            if (c->sm_client_id)
                fprintf(f, "<window id=\"%s\">\n", c->sm_client_id);
            else {
                t = g_markup_escape_text(c->wm_command, -1);
                fprintf(f, "<window command=\"%s\">\n", t);
                g_free(t);
            }

            t = g_markup_escape_text(c->name, -1);
            fprintf(f, "\t<name>%s</name>\n", t);
            g_free(t);

            t = g_markup_escape_text(c->class, -1);
            fprintf(f, "\t<class>%s</class>\n", t);
            g_free(t);

            t = g_markup_escape_text(c->role, -1);
            fprintf(f, "\t<role>%s</role>\n", t);
            g_free(t);

            fprintf(f, "\t<windowtype>%d</windowtype>\n", c->type);

            fprintf(f, "\t<desktop>%d</desktop>\n", c->desktop);
            fprintf(f, "\t<x>%d</x>\n", prex);
            fprintf(f, "\t<y>%d</y>\n", prey);
            fprintf(f, "\t<width>%d</width>\n", prew);
            fprintf(f, "\t<height>%d</height>\n", preh);
            if (c->shaded)
                fprintf(f, "\t<shaded />\n");
            if (c->iconic)
                fprintf(f, "\t<iconic />\n");
            if (c->skip_pager)
                fprintf(f, "\t<skip_pager />\n");
            if (c->skip_taskbar)
                fprintf(f, "\t<skip_taskbar />\n");
            if (c->fullscreen)
                fprintf(f, "\t<fullscreen />\n");
            if (c->above)
                fprintf(f, "\t<above />\n");
            if (c->below)
                fprintf(f, "\t<below />\n");
            if (c->max_horz)
                fprintf(f, "\t<max_horz />\n");
            if (c->max_vert)
                fprintf(f, "\t<max_vert />\n");
            if (c->undecorated)
                fprintf(f, "\t<undecorated />\n");
            if (savedata->focus_client == c)
                fprintf(f, "\t<focused />\n");
            fprintf(f, "</window>\n\n");
        }

        fprintf(f, "</openbox_session>\n");

        if (fflush(f)) {
            success = FALSE;
            g_message(_("Error while saving the session to \"%s\": %s"),
                      ob_sm_save_file, g_strerror(errno));
        }
        fclose(f);
    }

    return success;
}

static void session_state_free(ObSessionState *state)
{
    if (state) {
        g_free(state->id);
        g_free(state->command);
        g_free(state->name);
        g_free(state->class);
        g_free(state->role);

        g_slice_free(ObSessionState, state);
    }
}

static gboolean session_state_cmp(ObSessionState *s, ObClient *c)
{
    ob_debug_type(OB_DEBUG_SM, "Comparing client against saved state: ");
    ob_debug_type(OB_DEBUG_SM, "  client id: %s ", c->sm_client_id);
    ob_debug_type(OB_DEBUG_SM, "  client name: %s ", c->name);
    ob_debug_type(OB_DEBUG_SM, "  client class: %s ", c->class);
    ob_debug_type(OB_DEBUG_SM, "  client role: %s ", c->role);
    ob_debug_type(OB_DEBUG_SM, "  client type: %d ", c->type);
    ob_debug_type(OB_DEBUG_SM, "  client command: %s ",
                  c->wm_command ? c->wm_command : "(null)");
    ob_debug_type(OB_DEBUG_SM, "  state id: %s ", s->id);
    ob_debug_type(OB_DEBUG_SM, "  state name: %s ", s->name);
    ob_debug_type(OB_DEBUG_SM, "  state class: %s ", s->class);
    ob_debug_type(OB_DEBUG_SM, "  state role: %s ", s->role);
    ob_debug_type(OB_DEBUG_SM, "  state type: %d ", s->type);
    ob_debug_type(OB_DEBUG_SM, "  state command: %s ",
                  s->command ? s->command : "(null)");

    if ((c->sm_client_id && s->id && !strcmp(c->sm_client_id, s->id)) ||
        (c->wm_command && s->command && !strcmp(c->wm_command, s->command)))
    {
        return (!strcmp(s->name, c->name) &&
                !strcmp(s->class, c->class) &&
                !strcmp(s->role, c->role) &&
                /* the check for type is to catch broken clients, like
                   firefox, which open a different window on startup
                   with the same info as the one we saved. only do this
                   check for old windows that dont use xsmp, others should
                   know better ! */
                (!s->command || c->type == s->type));
    }
    return FALSE;
}

GList* session_state_find(ObClient *c)
{
    GList *it;

    for (it = session_saved_state; it; it = g_list_next(it)) {
        ObSessionState *s = it->data;
        if (!s->matched && session_state_cmp(s, c)) {
            s->matched = TRUE;
            break;
        }
    }
    return it;
}

static void session_load_file(const gchar *path)
{
    ObtXmlInst *i;
    xmlNodePtr node, n, m;
    GList *it, *inext;

    i = obt_xml_instance_new();

    if (!obt_xml_load_file(i, path, "openbox_session")) {
        ob_debug_type(OB_DEBUG_SM, "ERROR: session file is missing root node");
        obt_xml_instance_unref(i);
        return;
    }
    node = obt_xml_root(i);

    if ((n = obt_xml_find_node(node->children, "desktop")))
        session_desktop = obt_xml_node_int(n);

    if ((n = obt_xml_find_node(node->children, "numdesktops")))
        session_num_desktops = obt_xml_node_int(n);

    if ((n = obt_xml_find_node(node->children, "desktoplayout"))) {
        /* make sure they are all there for it to be valid */
        if ((m = obt_xml_find_node(n->children, "orientation")))
            session_desktop_layout.orientation = obt_xml_node_int(m);
        if (m && (m = obt_xml_find_node(n->children, "startcorner")))
            session_desktop_layout.start_corner = obt_xml_node_int(m);
        if (m && (m = obt_xml_find_node(n->children, "columns")))
            session_desktop_layout.columns = obt_xml_node_int(m);
        if (m && (m = obt_xml_find_node(n->children, "rows")))
            session_desktop_layout.rows = obt_xml_node_int(m);
        session_desktop_layout_present = m != NULL;
    }

    if ((n = obt_xml_find_node(node->children, "desktopnames"))) {
        for (m = obt_xml_find_node(n->children, "name"); m;
             m = obt_xml_find_node(m->next, "name"))
        {
            session_desktop_names = g_slist_append(session_desktop_names,
                                                   obt_xml_node_string(m));
        }
    }

    ob_debug_type(OB_DEBUG_SM, "loading windows");
    for (node = obt_xml_find_node(node->children, "window"); node != NULL;
         node = obt_xml_find_node(node->next, "window"))
    {
        ObSessionState *state;

        state = g_slice_new0(ObSessionState);

        if (!obt_xml_attr_string(node, "id", &state->id))
            if (!obt_xml_attr_string(node, "command", &state->command))
            goto session_load_bail;
        if (!(n = obt_xml_find_node(node->children, "name")))
            goto session_load_bail;
        state->name = obt_xml_node_string(n);
        if (!(n = obt_xml_find_node(node->children, "class")))
            goto session_load_bail;
        state->class = obt_xml_node_string(n);
        if (!(n = obt_xml_find_node(node->children, "role")))
            goto session_load_bail;
        state->role = obt_xml_node_string(n);
        if (!(n = obt_xml_find_node(node->children, "windowtype")))
            goto session_load_bail;
        state->type = obt_xml_node_int(n);
        if (!(n = obt_xml_find_node(node->children, "desktop")))
            goto session_load_bail;
        state->desktop = obt_xml_node_int(n);
        if (!(n = obt_xml_find_node(node->children, "x")))
            goto session_load_bail;
        state->x = obt_xml_node_int(n);
        if (!(n = obt_xml_find_node(node->children, "y")))
            goto session_load_bail;
        state->y = obt_xml_node_int(n);
        if (!(n = obt_xml_find_node(node->children, "width")))
            goto session_load_bail;
        state->w = obt_xml_node_int(n);
        if (!(n = obt_xml_find_node(node->children, "height")))
            goto session_load_bail;
        state->h = obt_xml_node_int(n);

        state->shaded =
            obt_xml_find_node(node->children, "shaded") != NULL;
        state->iconic =
            obt_xml_find_node(node->children, "iconic") != NULL;
        state->skip_pager =
            obt_xml_find_node(node->children, "skip_pager") != NULL;
        state->skip_taskbar =
            obt_xml_find_node(node->children, "skip_taskbar") != NULL;
        state->fullscreen =
            obt_xml_find_node(node->children, "fullscreen") != NULL;
        state->above =
            obt_xml_find_node(node->children, "above") != NULL;
        state->below =
            obt_xml_find_node(node->children, "below") != NULL;
        state->max_horz =
            obt_xml_find_node(node->children, "max_horz") != NULL;
        state->max_vert =
            obt_xml_find_node(node->children, "max_vert") != NULL;
        state->undecorated =
            obt_xml_find_node(node->children, "undecorated") != NULL;
        state->focused =
            obt_xml_find_node(node->children, "focused") != NULL;

        /* save this. they are in the file in stacking order, so preserve
           that order here */
        session_saved_state = g_list_append(session_saved_state, state);
        ob_debug_type(OB_DEBUG_SM, "loaded %s", state->name);
        continue;

    session_load_bail:
        ob_debug_type(OB_DEBUG_SM, "loading FAILED");
        session_state_free(state);
    }

    /* Remove any duplicates.  This means that if two windows (or more) are
       saved with the same session state, we won't restore a session for any
       of them because we don't know what window to put what on. AHEM FIREFOX.

       This is going to be an O(2^n) kind of operation unfortunately.
    */
    for (it = session_saved_state; it; it = inext) {
        GList *jt, *jnext;
        gboolean founddup = FALSE;
        ObSessionState *s1 = it->data;

        inext = g_list_next(it);

        for (jt = g_list_next(it); jt; jt = jnext) {
            ObSessionState *s2 = jt->data;
            gboolean match;

            jnext = g_list_next(jt);

            if (s1->id && s2->id)
                match = strcmp(s1->id, s2->id) == 0;
            else if (s1->command && s2->command)
                match = strcmp(s1->command, s2->command) == 0;
            else
                match = FALSE;

            if (match &&
                !strcmp(s1->name, s2->name) &&
                !strcmp(s1->class, s2->class) &&
                !strcmp(s1->role, s2->role))
            {
                ob_debug_type(OB_DEBUG_SM, "removing duplicate %s", s2->name);
                session_state_free(s2);
                session_saved_state =
                    g_list_delete_link(session_saved_state, jt);
                founddup = TRUE;
            }
        }

        if (founddup) {
            ob_debug_type(OB_DEBUG_SM, "removing duplicate %s", s1->name);
            session_state_free(s1);
            session_saved_state = g_list_delete_link(session_saved_state, it);
        }
    }

    obt_xml_instance_unref(i);
}

void session_request_logout(gboolean silent)
{
    if (sm_conn) {
        SmcRequestSaveYourself(sm_conn,
                               SmSaveGlobal,
                               TRUE, /* logout */
                               (silent ?
                                SmInteractStyleNone : SmInteractStyleAny),
                               TRUE,  /* if false, with GSM, it shows the old
                                         logout prompt */
                               TRUE); /* global */
    }
    else
        g_message(_("Not connected to a session manager"));
}

#endif
