/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/display.c for the Openbox window manager
   Copyright (c) 2007        Dana Jansens

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

#include "obt/xqueue.h"
#include "obt/display.h"

#define MINSZ 16

static XEvent *q = NULL;
static gulong qsz = 0;
static gulong qstart; /* the first event in the queue */
static gulong qend; /* the last event in the queue */
static gulong qnum = 0;

static inline void shrink(void) {
    if (qsz > MINSZ && qnum < qsz / 4) {
        const gulong newsz = qsz/2;
        gulong i;

        if (qnum == 0) {
            qstart = 0;
            qend = -1;
        }

        /* all in the shinking part, move it to pos 0 */
        else if (qstart >= newsz && qend >= newsz) {
            for (i = 0; i < qnum; ++i)
                q[i] = q[qstart+i];
            qstart = 0;
            qend = qnum - 1;
        }

        /* it wraps around to 0 right now, move the part between newsz and qsz
           to be before newsz */
        else if (qstart >= newsz) {
            const gulong n = qsz - qstart;
            for (i = 0; i < n; ++i)
                q[newsz-n+i] = q[qstart+i];
            qstart = newsz-n;
        }

        /* it needs to wrap around to 0, move the stuff after newsz to pos 0 */
        else if (qend >= newsz) {
            const gulong n = qend + 1 - newsz;
            for (i = 0; i < n; ++i)
                q[i] = q[newsz+i];
            qend = n - 1;
        }

        q = g_renew(XEvent, q, newsz);
        qsz = newsz;
    }
}

static inline void grow(void) {
    if (qnum == qsz) {
        const gulong newsz = qsz*2;
        gulong i;
 
        q = g_renew(XEvent, q, newsz);

        g_assert(qnum > 0);

        if (qend < qstart) { /* it wraps around to 0 right now */
            for (i = 0; i <= qend; ++i)
                q[qsz+i] = q[i];
            qend = qsz + qend;
        }

        qsz = newsz;
    }
}

/* Grab all pending X events */
static gboolean read_events(gboolean block)
{
    gint sth, n;

    n = XEventsQueued(obt_display, QueuedAfterFlush) > 0;
    sth = FALSE;

    while ((block && !sth) || n > 0) {
        XEvent e;

        if (XNextEvent(obt_display, &e) != Success)
            return FALSE;

        grow(); /* make sure there is room */

        ++qnum;
        qend = (qend + 1) % qsz; /* move the end */
        q[qend] = e; /* stick the event at the end */

        --n;
        sth = TRUE;
    }

    return sth; /* return if we read anything */
}

static void pop(const gulong p)
{
    /* remove the event */
    --qnum;
    if (qnum == 0) {
        qstart = 0;
        qend = -1;
    }
    else if (p == qstart)
        qstart = (qstart + 1) % qsz;
    else {
        gulong pi;

        /* is it cheaper to move the start or the end ? */
        if ((p >= qstart && p < qstart + qnum/2) ||
            (p < qstart && p < (qstart + qnum/2) % qsz))
        {
            /* move the start */
            pi = p;
            while (pi != qstart) {
                const gulong pi_next = (pi == 0 ? qsz-1 : pi-1);

                q[pi] = q[pi_next];
                pi = pi_next;
            }
            qstart = (qstart + 1) % qsz;
        }
        else {
            /* move the end */
            pi = p;
            while (pi != qend) {
                const gulong pi_next = (pi + 1) % qsz;

                q[pi] = q[pi_next];
                pi = pi_next;
            }
            qend = (qend == 0 ? qsz-1 : qend-1);
        }
    }

    shrink(); /* shrink the q if too little in it */
}

void xqueue_init(void)
{
    if (q != NULL) return;
    qsz = MINSZ;
    q = g_new(XEvent, qsz);
    qstart = 0;
    qend = -1;
}

void xqueue_destroy(void)
{
    if (q == NULL) return;
    g_free(q);
    q = NULL;
    qsz = 0;
}

gboolean xqueue_match_window(XEvent *e, gpointer data)
{
    const Window w = *(Window*)data;
    return e->xany.window == w;
}

gboolean xqueue_match_type(XEvent *e, gpointer data)
{
    return e->type == GPOINTER_TO_INT(data);
}

gboolean xqueue_match_window_type(XEvent *e, gpointer data)
{
    const ObtXQueueWindowType x = *(ObtXQueueWindowType*)data;
    return e->xany.window == x.window && e->type == x.type;
}

gboolean xqueue_match_window_message(XEvent *e, gpointer data)
{
    const ObtXQueueWindowMessage x = *(ObtXQueueWindowMessage*)data;
    return e->xany.window == x.window && e->type == ClientMessage &&
        e->xclient.message_type == x.message;
}

gboolean xqueue_peek(XEvent *event_return)
{
    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(event_return != NULL, FALSE);

    if (!qnum) read_events(TRUE);
    if (!qnum) return FALSE;
    *event_return = q[qstart]; /* get the head */
    return TRUE;
}

gboolean xqueue_peek_local(XEvent *event_return)
{
    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(event_return != NULL, FALSE);

    if (!qnum) read_events(FALSE);
    if (!qnum) return FALSE;
    *event_return = q[qstart]; /* get the head */
    return TRUE;
}

gboolean xqueue_next(XEvent *event_return)
{
    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(event_return != NULL, FALSE);

    if (!qnum) read_events(TRUE);
    if (qnum) {
        *event_return = q[qstart]; /* get the head */
        pop(qstart);
        return TRUE;
    }

    return FALSE;
}

gboolean xqueue_next_local(XEvent *event_return)
{
    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(event_return != NULL, FALSE);

    if (!qnum) read_events(FALSE);
    if (qnum) {
        *event_return = q[qstart]; /* get the head */
        pop(qstart);
        return TRUE;
    }

    return FALSE;
}

gboolean xqueue_exists(xqueue_match_func match, gpointer data)
{
    gulong i, checked;

    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(match != NULL, FALSE);

    checked = 0;
    while (TRUE) {
        for (i = checked; i < qnum; ++i, ++checked) {
            const gulong p = (qstart + i) % qsz;
            if (match(&q[p], data))
                return TRUE;
        }
        if (!read_events(TRUE)) break; /* error */
    }
    return FALSE;
}

gboolean xqueue_exists_local(xqueue_match_func match, gpointer data)
{
    gulong i, checked;

    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(match != NULL, FALSE);

    checked = 0;
    while (TRUE) {
        for (i = checked; i < qnum; ++i, ++checked) {
            const gulong p = (qstart + i) % qsz;
            if (match(&q[p], data))
                return TRUE;
        }
        if (!read_events(FALSE)) break;
    }
    return FALSE;
}

gboolean xqueue_remove_local(XEvent *event_return,
                             xqueue_match_func match, gpointer data)
{
    gulong i, checked;

    g_return_val_if_fail(q != NULL, FALSE);
    g_return_val_if_fail(event_return != NULL, FALSE);
    g_return_val_if_fail(match != NULL, FALSE);

    checked = 0;
    while (TRUE) {
        for (i = checked; i < qnum; ++i, ++checked) {
            const gulong p = (qstart + i) % qsz;
            if (match(&q[p], data)) {
                *event_return = q[p];
                pop(p);
                return TRUE;
            }
        }
        if (!read_events(FALSE)) break;
    }
    return FALSE;
}

gboolean xqueue_pending_local(void)
{
    g_return_val_if_fail(q != NULL, FALSE);
    
    if (!qnum) read_events(FALSE);
    return qnum != 0;
}

typedef struct _ObtXQueueCB {
    ObtXQueueFunc func;
    gpointer data;
} ObtXQueueCB;

static ObtXQueueCB *callbacks = NULL;
static guint n_callbacks = 0;

static gboolean event_read(GSource *source, GSourceFunc callback,
                           gpointer data)
{
    XEvent ev;

    while (xqueue_next_local(&ev)) {
        guint i;
        for (i = 0; i < n_callbacks; ++i)
            callbacks[i].func(&ev, callbacks[i].data);
    }

    return TRUE; /* repeat */
}

static gboolean x_source_prepare(GSource *source, gint *timeout)
{
    *timeout = -1;
    return XPending(obt_display);
}

static gboolean x_source_check(GSource *source)
{
    return XPending(obt_display);
}

struct x_source {
    GSource source;

    GPollFD pfd;
};

static GSourceFuncs x_source_funcs = {
    x_source_prepare,
    x_source_check,
    event_read,
    NULL
};

void xqueue_listen(void)
{
    GSource *source = g_source_new(&x_source_funcs, sizeof(struct x_source));
    struct x_source *x_source = (struct x_source *)source;
    GPollFD *pfd = &x_source->pfd;

    *pfd = (GPollFD){ ConnectionNumber(obt_display), G_IO_IN, G_IO_IN };
    g_source_add_poll(source, pfd);
    g_source_attach(source, NULL);
}

void xqueue_add_callback(ObtXQueueFunc f, gpointer data)
{
    guint i;

    g_return_if_fail(f != NULL);

    for (i = 0; i < n_callbacks; ++i)
        if (callbacks[i].func == f && callbacks[i].data == data)
            return;

    callbacks = g_renew(ObtXQueueCB, callbacks, n_callbacks + 1);
    callbacks[n_callbacks].func = f;
    callbacks[n_callbacks].data = data;
    ++n_callbacks;
}

void xqueue_remove_callback(ObtXQueueFunc f, gpointer data)
{
    guint i;

    g_return_if_fail(f != NULL);

    for (i = 0; i < n_callbacks; ++i) {
        if (callbacks[i].func == f && callbacks[i].data == data) {
            /* remove it */
            for (; i < n_callbacks - 1; ++i)
                callbacks[i] = callbacks[i+1];
            callbacks = g_renew(ObtXQueueCB, callbacks, n_callbacks - 1);
            --n_callbacks;
            break;
        }
    }
}
