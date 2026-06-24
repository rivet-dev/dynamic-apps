/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   debug.c for the Openbox window manager
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

#include "debug.h"
#include "prompt.h"
#include "openbox.h"
#include "gettext.h"
#include "obt/paths.h"

#include <glib.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

static gboolean  enabled_types[OB_DEBUG_TYPE_NUM] = {FALSE};
static FILE     *log_file = NULL;
static guint     rr_handler_id = 0;
static guint     obt_handler_id = 0;
static guint     ob_handler_id = 0;
static guint     ob_handler_prompt_id = 0;
static GList    *prompt_queue = NULL;
static gboolean  allow_prompts = TRUE;

static void log_handler(const gchar *log_domain, GLogLevelFlags log_level,
                        const gchar *message, gpointer user_data);
static void prompt_handler(const gchar *log_domain, GLogLevelFlags log_level,
                           const gchar *message, gpointer user_data);

void ob_debug_startup(void)
{
    ObtPaths *p = obt_paths_new();
    gchar *dir = g_build_filename(obt_paths_cache_home(p),
                                  "openbox", NULL);

    /* log messages to a log file!  fancy, no? */
    if (!obt_paths_mkdir_path(dir, 0777))
        g_message(_("Unable to make directory '%s': %s"),
                  dir, g_strerror(errno));
    else {
        gchar *name = g_build_filename(obt_paths_cache_home(p),
                                       "openbox", "openbox.log", NULL);
        /* unlink it before opening to remove competition */
        unlink(name);
        log_file = fopen(name, "w");
        g_free(name);
    }

    rr_handler_id =
        g_log_set_handler("ObRender", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL |
                          G_LOG_FLAG_RECURSION, log_handler, NULL);
    obt_handler_id =
        g_log_set_handler("Obt", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL |
                          G_LOG_FLAG_RECURSION, log_handler, NULL);
    ob_handler_id =
        g_log_set_handler("Openbox", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL |
                          G_LOG_FLAG_RECURSION, log_handler, NULL);
    ob_handler_prompt_id =
        g_log_set_handler("Openbox", G_LOG_LEVEL_MASK & ~G_LOG_LEVEL_DEBUG,
                          prompt_handler, NULL);

    obt_paths_unref(p);
    g_free(dir);
}

void ob_debug_shutdown(void)
{
    g_log_remove_handler("ObRender", rr_handler_id);
    g_log_remove_handler("Obt", obt_handler_id);
    g_log_remove_handler("Openbox", ob_handler_id);
    g_log_remove_handler("Openbox", ob_handler_prompt_id);

    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void ob_debug_enable(ObDebugType type, gboolean enable)
{
    g_assert(type < OB_DEBUG_TYPE_NUM);
    enabled_types[type] = enable;
}

static inline void log_print(FILE *out, const gchar* log_domain,
                             const gchar *level, const gchar *message)
{
    fprintf(out, "%s", log_domain);
    fprintf(out, "-");
    fprintf(out, "%s", level);
    fprintf(out, ": ");
    fprintf(out, "%s", message);
    fprintf(out, "\n");
    fflush(out);
}

static void log_handler(const gchar *log_domain, GLogLevelFlags log_level,
                        const gchar *message, gpointer data)
{
    FILE *out;
    const gchar *level;

    switch (log_level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_DEBUG:    level = "Debug";    out = stdout; break;
    case G_LOG_LEVEL_INFO:     level = "Info";     out = stdout; break;
    case G_LOG_LEVEL_MESSAGE:  level = "Message";  out = stdout; break;
    case G_LOG_LEVEL_WARNING:  level = "Warning";  out = stderr; break;
    case G_LOG_LEVEL_CRITICAL: level = "Critical"; out = stderr; break;
    case G_LOG_LEVEL_ERROR:    level = "Error";    out = stderr; break;
    default:                   g_assert_not_reached(); /* invalid level.. */
    }

    log_print(out, log_domain, level, message);
    if (log_file) log_print(log_file, log_domain, level, message);
}

static void prompt_handler(const gchar *log_domain, GLogLevelFlags log_level,
                           const gchar *message, gpointer data)
{
    if (ob_state() == OB_STATE_RUNNING && allow_prompts)
        prompt_queue = g_list_prepend(prompt_queue, g_strdup(message));
    else
        log_handler(log_domain, log_level, message, data);
}

static inline void log_argv(ObDebugType type,
                            const gchar *format, va_list args)
{
    const gchar *prefix;
    gchar *message;

    g_assert(type < OB_DEBUG_TYPE_NUM);
    if (!enabled_types[type]) return;

    switch (type) {
    case OB_DEBUG_FOCUS:    prefix = "(FOCUS) ";           break;
    case OB_DEBUG_APP_BUGS: prefix = "(APPLICATION BUG) "; break;
    case OB_DEBUG_SM:       prefix = "(SESSION) ";         break;
    default:                prefix = NULL;                 break;
    }

    message = g_strdup_vprintf(format, args);
    if (prefix) {
        gchar *a = message;
        message = g_strconcat(prefix, message, NULL);
        g_free(a);
    }

    g_debug("%s", message);
    g_free(message);
}

void ob_debug(const gchar *a, ...)
{
    va_list vl;

    va_start(vl, a);
    log_argv(OB_DEBUG_NORMAL, a, vl);
    va_end(vl);
}

void ob_debug_type(ObDebugType type, const gchar *a, ...)
{
    va_list vl;

    va_start(vl, a);
    log_argv(type, a, vl);
    va_end(vl);
}

void ob_debug_show_prompts(void)
{
    if (prompt_queue) {
        allow_prompts = FALSE; /* avoid recursive prompts */
        while (prompt_queue) {
            prompt_show_message(prompt_queue->data, "Openbox", _("Close"));
            g_free(prompt_queue->data);
            prompt_queue = g_list_delete_link(prompt_queue, prompt_queue);
        }
        allow_prompts = TRUE;
    }
}
