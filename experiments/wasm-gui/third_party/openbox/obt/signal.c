/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/signal.c for the Openbox window manager
   Copyright (c) 2010        Dana Jansens

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

#include "signal.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

typedef struct _ObtSignalCallback ObtSignalCallback;

struct _ObtSignalCallback
{
    ObtSignalHandler func;
    gpointer data;
};

static gboolean signal_prepare(GSource *source, gint *timeout);
static gboolean signal_check(GSource *source);
static gboolean signal_occurred(GSource *source, GSourceFunc callback,
                               gpointer data);
static void sighandler(gint sig);

/* this should be more than the number of possible signals on any
   architecture... */
#define NUM_SIGNALS 99

/* a set of all possible signals */
static sigset_t all_signals_set;

/* keep track of what signals have a signal handler installed, and remember
   the action we replaced when installing it for when we clean up */
static struct {
    guint installed; /* a ref count */
    struct sigaction oldact;
} all_signals[NUM_SIGNALS];

/* signals which cause a core dump, these can't be used for callbacks */
static const gint core_signals[] =
{
    SIGABRT,
    SIGSEGV,
    SIGFPE,
    SIGILL,
    SIGQUIT,
    SIGTRAP,
    SIGSYS,
    SIGBUS,
    SIGXCPU,
    SIGXFSZ
};
#define NUM_CORE_SIGNALS (gint)(sizeof(core_signals) / sizeof(core_signals[0]))

static GSourceFuncs source_funcs = {
    signal_prepare,
    signal_check,
    signal_occurred,
    NULL
};
static GSource *gsource = NULL;
static guint listeners = 0; /* a ref count for the signal listener */
static gboolean signal_fired;
guint signals_fired[NUM_SIGNALS];
GSList *callbacks[NUM_SIGNALS];

void obt_signal_listen(void)
{
    if (!listeners) {
        guint i;
        struct sigaction action;
        sigset_t sigset;

        /* initialize the all_signals_set */
        sigfillset(&all_signals_set);

        sigemptyset(&sigset);
        action.sa_handler = sighandler;
        action.sa_mask = sigset;
        action.sa_flags = SA_NOCLDSTOP;

        /* always grab all the signals that cause core dumps */
        for (i = 0; i < NUM_CORE_SIGNALS; ++i) {
            /* SIGABRT is curiously not grabbed here!! that's because when we
               get one of the core_signals, we use abort() to dump the core.
               And having the abort() only go back to our signal handler again
               is less than optimal */
            if (core_signals[i] != SIGABRT) {
                sigaction(core_signals[i], &action,
                          &all_signals[core_signals[i]].oldact);
                all_signals[core_signals[i]].installed++;
            }
        }

        gsource = g_source_new(&source_funcs, sizeof(GSource));
        g_source_set_priority(gsource, G_PRIORITY_HIGH);

        g_source_attach(gsource, NULL);
    }

    ++listeners;
}

void obt_signal_stop(void)
{
    --listeners;

    if (!listeners) {
        gint i;
        GSList *it, *next;

        g_source_unref(gsource);
        gsource = NULL;

        /* remove user defined signal handlers */
        for (i = 0; i < NUM_SIGNALS; ++i)
            for (it = callbacks[i]; it; it = next) {
                ObtSignalCallback *cb = it->data;
                next = g_slist_next(it);
                obt_signal_remove_callback(i, cb->func);
            }

        /* release all the signals that cause core dumps */
        for (i = 0; i < NUM_CORE_SIGNALS; ++i) {
            if (all_signals[core_signals[i]].installed) {
                sigaction(core_signals[i],
                          &all_signals[core_signals[i]].oldact, NULL);
                all_signals[core_signals[i]].installed--;
            }
        }

#ifdef DEBUG
        for (i = 0; i < NUM_SIGNALS; ++i)
            g_assert(all_signals[i].installed == 0);
#endif
    }
}

void obt_signal_add_callback(gint sig, ObtSignalHandler func, gpointer data)
{
    ObtSignalCallback *cb;
    gint i;

    g_return_if_fail(func != NULL);
    g_return_if_fail(sig >= 0 && sig <= NUM_SIGNALS);
    for (i = 0; i < NUM_CORE_SIGNALS; ++i)
        g_return_if_fail(sig != core_signals[i]);

    cb = g_slice_new(ObtSignalCallback);
    cb->func = func;
    cb->data = data;
    callbacks[sig] = g_slist_prepend(callbacks[sig], cb);

    /* install the signal handler */
    if (!all_signals[sig].installed) {
        struct sigaction action;
        sigset_t sigset;

        sigemptyset(&sigset);
        action.sa_handler = sighandler;
        action.sa_mask = sigset;
        action.sa_flags = SA_NOCLDSTOP;

        sigaction(sig, &action, &all_signals[sig].oldact);
    }

    all_signals[sig].installed++;
}

void obt_signal_remove_callback(gint sig, ObtSignalHandler func)
{
    GSList *it;
    gint i;

    g_return_if_fail(func != NULL);
    g_return_if_fail(sig >= 0 && sig <= NUM_SIGNALS);
    for (i = 0; i < NUM_CORE_SIGNALS; ++i)
        g_return_if_fail(sig != core_signals[i]);

    for (it = callbacks[sig]; it; it = g_slist_next(it)) {
        ObtSignalCallback *cb = it->data;
        if (cb->func == func) {
            g_assert(all_signals[sig].installed > 0);

            callbacks[sig] = g_slist_delete_link(callbacks[sig], it);
            g_slice_free(ObtSignalCallback, cb);

            /* uninstall the signal handler */
            all_signals[sig].installed--;
            if (!all_signals[sig].installed)
                sigaction(sig, &all_signals[sig].oldact, NULL);
            break;
        }
    }
}

static gboolean signal_prepare(GSource *source, gint *timeout)
{
    *timeout = -1;
    return signal_fired;
}

static gboolean signal_check(GSource *source)
{
    return signal_fired;
}

static gboolean signal_occurred(GSource *source, GSourceFunc callback,
                                gpointer data)
{
    guint i;
    sigset_t oldset;
    guint fired[NUM_SIGNALS];

    /* block signals so that we can do this without the data changing
       on us */
    sigprocmask(SIG_SETMASK, &all_signals_set, &oldset);

    /* make a copy of the signals that fired */
    for (i = 0; i < NUM_SIGNALS; ++i) {
        fired[i] = signals_fired[i];
        signals_fired[i] = 0;
    }
    signal_fired = FALSE;

    sigprocmask(SIG_SETMASK, &oldset, NULL);

    /* call the signal callbacks for the signals */
    for (i = 0; i < NUM_SIGNALS; ++i) {
        while (fired[i]) {
            GSList *it;
            for (it = callbacks[i]; it; it = g_slist_next(it)) {
                const ObtSignalCallback *cb = it->data;
                cb->func(i, cb->data);
            }
            --fired[i];
        }
    }

    return TRUE; /* repeat */
}

static void sighandler(gint sig)
{
    guint i;

    g_return_if_fail(sig < NUM_SIGNALS);

    for (i = 0; i < NUM_CORE_SIGNALS; ++i)
        if (sig == core_signals[i]) {
            /* XXX special case for signals that default to core dump.
               but throw some helpful output here... */

            fprintf(stderr, "How are you gentlemen? All your base are"
                    " belong to us. (Openbox received signal %d)\n", sig);

            /* die with a core dump */
            abort();
        }

    signal_fired = TRUE;
    ++signals_fired[sig];

    /* i don't think we want to modify the GMainContext inside a signal
       handler, so use a GSource instead of an idle func to call back
       to the application */
}
