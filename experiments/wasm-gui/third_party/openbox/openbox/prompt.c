/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   prompt.c for the Openbox window manager
   Copyright (c) 2008        Dana Jansens

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

#include "prompt.h"
#include "openbox.h"
#include "screen.h"
#include "client.h"
#include "group.h"
#include "event.h"
#include "obt/display.h"
#include "obt/keyboard.h"
#include "obt/prop.h"
#include "gettext.h"

static GList *prompt_list = NULL;

/* we construct these */
static RrAppearance *prompt_a_bg;
static RrAppearance *prompt_a_button;
static RrAppearance *prompt_a_focus;
static RrAppearance *prompt_a_press;
/* we change the max width which would screw with others */
static RrAppearance *prompt_a_msg;

/* sizing stuff */
#define OUTSIDE_MARGIN 4
#define MSG_BUTTON_SEPARATION 4
#define BUTTON_SEPARATION 4
#define BUTTON_VMARGIN 4
#define BUTTON_HMARGIN 12
#define MAX_WIDTH 400

static void prompt_layout(ObPrompt *self);
static void render_all(ObPrompt *self);
static void render_button(ObPrompt *self, ObPromptElement *e);
static void prompt_resize(ObPrompt *self, gint w, gint h);
static void prompt_run_callback(ObPrompt *self, gint result);

void prompt_startup(gboolean reconfig)
{
    /* note: this is not a copy, don't free it */
    prompt_a_bg = ob_rr_theme->osd_bg;

    prompt_a_button = RrAppearanceCopy(ob_rr_theme->osd_unpressed_button);
    prompt_a_focus = RrAppearanceCopy(ob_rr_theme->osd_focused_button);
    prompt_a_press = RrAppearanceCopy(ob_rr_theme->osd_pressed_button);    

    prompt_a_msg = RrAppearanceCopy(ob_rr_theme->osd_hilite_label);
    prompt_a_msg->texture[0].data.text.flow = TRUE;

    if (reconfig) {
        GList *it;
        for (it = prompt_list; it; it = g_list_next(it)) {
            ObPrompt *p = it->data;
            prompt_layout(p);
            render_all(p);
        }
    }
}

void prompt_shutdown(gboolean reconfig)
{
    GList *it, *next;

    if (!reconfig) {
        for (it = prompt_list; it; it = next) {
            ObPrompt *p = it->data;
            next = it->next;
            if (p->cleanup) p->cleanup(p, p->data);
        }

        g_assert(prompt_list == NULL);
    }

    RrAppearanceFree(prompt_a_button);
    RrAppearanceFree(prompt_a_focus);
    RrAppearanceFree(prompt_a_press);
    RrAppearanceFree(prompt_a_msg);
}

ObPrompt* prompt_new(const gchar *msg, const gchar *title,
                     const ObPromptAnswer *answers, gint n_answers,
                     gint default_result, gint cancel_result,
                     ObPromptCallback func, ObPromptCleanup cleanup,
                     gpointer data)
{
    ObPrompt *self;
    XSetWindowAttributes attrib;
    gint i;

    attrib.override_redirect = FALSE;

    self = g_slice_new0(ObPrompt);
    self->ref = 1;
    self->func = func;
    self->cleanup = cleanup;
    self->data = data;
    self->default_result = default_result;
    self->cancel_result = cancel_result;
    self->super.type = OB_WINDOW_CLASS_PROMPT;
    self->super.window = XCreateWindow(obt_display, obt_root(ob_screen),
                                       0, 0, 1, 1, 0,
                                       CopyFromParent, InputOutput,
                                       CopyFromParent,
                                       CWOverrideRedirect,
                                       &attrib);
    self->ic = obt_keyboard_context_new(self->super.window,
                                        self->super.window);

    /* make it a dialog type window */
    OBT_PROP_SET32(self->super.window, NET_WM_WINDOW_TYPE, ATOM,
                   OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DIALOG));

    /* set the window's title */
    if (title)
        OBT_PROP_SETS(self->super.window, NET_WM_NAME, title);

    /* listen for key presses on the window */
    self->event_mask = KeyPressMask;

    /* set up the text message widow */
    self->msg.text = g_strdup(msg);
    self->msg.window = XCreateWindow(obt_display, self->super.window,
                                     0, 0, 1, 1, 0,
                                     CopyFromParent, InputOutput,
                                     CopyFromParent, 0, NULL);
    XMapWindow(obt_display, self->msg.window);

    /* set up the buttons from the answers */

    self->n_buttons = n_answers;
    if (!self->n_buttons)
        self->n_buttons = 1;

    self->button = g_new0(ObPromptElement, self->n_buttons);

    if (n_answers == 0) {
        g_assert(self->n_buttons == 1); /* should be set to this above.. */
        self->button[0].text = g_strdup(_("OK"));
    }
    else {
        g_assert(self->n_buttons > 0);
        for (i = 0; i < self->n_buttons; ++i) {
            self->button[i].text = g_strdup(answers[i].text);
            self->button[i].result = answers[i].result;
        }
    }

    for (i = 0; i < self->n_buttons; ++i) {
        self->button[i].window = XCreateWindow(obt_display, self->super.window,
                                               0, 0, 1, 1, 0,
                                               CopyFromParent, InputOutput,
                                               CopyFromParent, 0, NULL);
        XMapWindow(obt_display, self->button[i].window);
        window_add(&self->button[i].window, PROMPT_AS_WINDOW(self));

        /* listen for button presses on the buttons */
        XSelectInput(obt_display, self->button[i].window,
                     ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
    }

    prompt_list = g_list_prepend(prompt_list, self);

    return self;
}

void prompt_ref(ObPrompt *self)
{
    ++self->ref;
}

void prompt_unref(ObPrompt *self)
{
    if (self && --self->ref == 0) {
        gint i;

        if (self->mapped)
            prompt_hide(self);

        prompt_list = g_list_remove(prompt_list, self);

        obt_keyboard_context_unref(self->ic);

        for (i = 0; i < self->n_buttons; ++i) {
            window_remove(self->button[i].window);
            XDestroyWindow(obt_display, self->button[i].window);
        }

        XDestroyWindow(obt_display, self->msg.window);
        XDestroyWindow(obt_display, self->super.window);
        g_slice_free(ObPrompt, self);
    }
}

static void prompt_layout(ObPrompt *self)
{
    gint l, r, t, b;
    gint i;
    gint allbuttonsw, allbuttonsh, buttonx;
    gint w, h;
    gint maxw;

    RrMargins(prompt_a_bg, &l, &t, &r, &b);
    l += OUTSIDE_MARGIN;
    t += OUTSIDE_MARGIN;
    r += OUTSIDE_MARGIN;
    b += OUTSIDE_MARGIN;

    {
        const Rect *area = screen_physical_area_all_monitors();
        maxw = MIN(MAX_WIDTH, area->width*4/5);
    }

    /* find the button sizes and how much space we need for them */
    allbuttonsw = allbuttonsh = 0;
    for (i = 0; i < self->n_buttons; ++i) {
        gint bw, bh;

        prompt_a_button->texture[0].data.text.string = self->button[i].text;
        prompt_a_focus->texture[0].data.text.string = self->button[i].text;
        prompt_a_press->texture[0].data.text.string = self->button[i].text;
        RrMinSize(prompt_a_button, &bw, &bh);
        self->button[i].width = bw;
        self->button[i].height = bh;
        RrMinSize(prompt_a_focus, &bw, &bh);
        self->button[i].width = MAX(self->button[i].width, bw);
        self->button[i].height = MAX(self->button[i].height, bh);
        RrMinSize(prompt_a_press, &bw, &bh);
        self->button[i].width = MAX(self->button[i].width, bw);
        self->button[i].height = MAX(self->button[i].height, bh);

        self->button[i].width += BUTTON_HMARGIN * 2;
        self->button[i].height += BUTTON_VMARGIN * 2;

        allbuttonsw += self->button[i].width + (i > 0 ? BUTTON_SEPARATION : 0);
        allbuttonsh = MAX(allbuttonsh, self->button[i].height);
    }

    self->msg_wbound = MAX(allbuttonsw, maxw);

    /* measure the text message area */
    prompt_a_msg->texture[0].data.text.string = self->msg.text;
    prompt_a_msg->texture[0].data.text.maxwidth = self->msg_wbound;
    RrMinSize(prompt_a_msg, &self->msg.width, &self->msg.height);

    /* width and height inside the outer margins */
    w = MAX(self->msg.width, allbuttonsw);
    h = self->msg.height + MSG_BUTTON_SEPARATION + allbuttonsh;

    /* position the text message */
    self->msg.x = l + (w - self->msg.width) / 2;
    self->msg.y = t;

    /* position the button buttons on the right of the dialog */
    buttonx = l + w;
    for (i = self->n_buttons - 1; i >= 0; --i) {
        self->button[i].x = buttonx - self->button[i].width;
        buttonx -= self->button[i].width + BUTTON_SEPARATION;
        self->button[i].y = t + h - allbuttonsh;
        self->button[i].y += (allbuttonsh - self->button[i].height) / 2;
    }

    /* size and position the toplevel window */
    prompt_resize(self, w + l + r, h + t + b);

    /* move and resize the internal windows */
    XMoveResizeWindow(obt_display, self->msg.window,
                      self->msg.x, self->msg.y,
                      self->msg.width, self->msg.height);
    for (i = 0; i < self->n_buttons; ++i)
        XMoveResizeWindow(obt_display, self->button[i].window,
                          self->button[i].x, self->button[i].y,
                          self->button[i].width, self->button[i].height);
}

static void prompt_resize(ObPrompt *self, gint w, gint h)
{
    XConfigureRequestEvent req;
    XSizeHints hints;

    self->width = w;
    self->height = h;

    /* the user can't resize the prompt */
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = hints.max_width = w;
    hints.min_height = hints.max_height = h;
    XSetWMNormalHints(obt_display, self->super.window, &hints);

    if (self->mapped) {
        /* send a configure request like a normal client would */
        req.type = ConfigureRequest;
        req.display = obt_display;
        req.parent = obt_root(ob_screen);
        req.window = self->super.window;
        req.width = w;
        req.height = h;
        req.value_mask = CWWidth | CWHeight;
        XSendEvent(req.display, req.window, FALSE, StructureNotifyMask,
                   (XEvent*)&req);
    }
    else
        XResizeWindow(obt_display, self->super.window, w, h);
}

static void setup_button_focus_tex(ObPromptElement *e, RrAppearance *a,
                                   gboolean on)
{
    gint i, l, r, t, b;

    for (i = 1; i < 5; ++i)
        a->texture[i].type = on ? RR_TEXTURE_LINE_ART : RR_TEXTURE_NONE;

    if (!on) return;

    RrMargins(a, &l, &t, &r, &b);
    l += MIN(BUTTON_HMARGIN, BUTTON_VMARGIN) / 2;
    r += MIN(BUTTON_HMARGIN, BUTTON_VMARGIN) / 2;
    t += MIN(BUTTON_HMARGIN, BUTTON_VMARGIN) / 2;
    b += MIN(BUTTON_HMARGIN, BUTTON_VMARGIN) / 2;

    /* top line */
    a->texture[1].data.lineart.x1 = l;
    a->texture[1].data.lineart.x2 = e->width - r - 1;
    a->texture[1].data.lineart.y1 = t;
    a->texture[1].data.lineart.y2 = t;

    /* bottom line */
    a->texture[2].data.lineart.x1 = l;
    a->texture[2].data.lineart.x2 = e->width - r - 1;
    a->texture[2].data.lineart.y1 = e->height - b - 1;
    a->texture[2].data.lineart.y2 = e->height - b - 1;

    /* left line */
    a->texture[3].data.lineart.x1 = l;
    a->texture[3].data.lineart.x2 = l;
    a->texture[3].data.lineart.y1 = t;
    a->texture[3].data.lineart.y2 = e->height - b - 1;

    /* right line */
    a->texture[4].data.lineart.x1 = e->width - r - 1;
    a->texture[4].data.lineart.x2 = e->width - r - 1;
    a->texture[4].data.lineart.y1 = t;
    a->texture[4].data.lineart.y2 = e->height - b - 1;
}

static void render_button(ObPrompt *self, ObPromptElement *e)
{
    RrAppearance *a;

    if (e->hover && e->pressed)       a = prompt_a_press;
    else if (self->focus == e)        a = prompt_a_focus;
    else                              a = prompt_a_button;

    a->surface.parent = prompt_a_bg;
    a->surface.parentx = e->x;
    a->surface.parenty = e->y;

    /* draw the keyfocus line */
    if (self->focus == e)
        setup_button_focus_tex(e, a, TRUE);

    a->texture[0].data.text.string = e->text;
    RrPaint(a, e->window, e->width, e->height);

    /* turn off the keyfocus line so that it doesn't affect size calculations
     */
    if (self->focus == e)
        setup_button_focus_tex(e, a, FALSE);
}

static void render_all(ObPrompt *self)
{
    gint i;

    RrPaint(prompt_a_bg, self->super.window, self->width, self->height);

    prompt_a_msg->surface.parent = prompt_a_bg;
    prompt_a_msg->surface.parentx = self->msg.x;
    prompt_a_msg->surface.parenty = self->msg.y;

    prompt_a_msg->texture[0].data.text.string = self->msg.text;
    prompt_a_msg->texture[0].data.text.maxwidth = self->msg_wbound;
    RrPaint(prompt_a_msg, self->msg.window, self->msg.width, self->msg.height);

    for (i = 0; i < self->n_buttons; ++i)
        render_button(self, &self->button[i]);
}

void prompt_show(ObPrompt *self, ObClient *parent, gboolean modal)
{
    gint i;

    if (self->mapped) {
        /* activate the prompt */
        OBT_PROP_MSG(ob_screen, self->super.window, NET_ACTIVE_WINDOW,
                     1, /* from an application.. */
                     event_time(),
                     0,
                     0, 0);
        return;
    }

    /* set the focused button (if not found then the first button is used) */
    self->focus = &self->button[0];
    for (i = 0; i < self->n_buttons; ++i)
        if (self->button[i].result == self->default_result) {
            self->focus = &self->button[i];
            break;
        }

    if (parent) {
        Atom states[1];
        gint nstates;
        Window p;
        XWMHints h;

        if (parent->group) {
            /* make it transient for the window's group */
            h.flags = WindowGroupHint;
            h.window_group = parent->group->leader;
            p = obt_root(ob_screen);
        }
        else {
            /* make it transient for the window directly */
            h.flags = 0;
            p = parent->window;
        }

        XSetWMHints(obt_display, self->super.window, &h);
        OBT_PROP_SET32(self->super.window, WM_TRANSIENT_FOR, WINDOW, p);

        states[0] = OBT_PROP_ATOM(NET_WM_STATE_MODAL);
        nstates = (modal ? 1 : 0);
        OBT_PROP_SETA32(self->super.window, NET_WM_STATE, ATOM,
                        states, nstates);
    }
    else
        OBT_PROP_ERASE(self->super.window, WM_TRANSIENT_FOR);

    /* set up the dialog and render it */
    prompt_layout(self);
    render_all(self);

    client_manage(self->super.window, self);

    self->mapped = TRUE;
}

void prompt_hide(ObPrompt *self)
{
    XUnmapWindow(obt_display, self->super.window);
    self->mapped = FALSE;
}

gboolean prompt_key_event(ObPrompt *self, XEvent *e)
{
    gboolean shift;
    guint shift_mask, mods;
    KeySym sym;

    if (e->type != KeyPress) return FALSE;

    shift_mask = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SHIFT);
    mods = obt_keyboard_only_modmasks(e->xkey.state);
    shift = !!(mods & shift_mask);

    /* only accept shift */
    if (mods != 0 && mods != shift_mask)
        return FALSE;

    sym = obt_keyboard_keypress_to_keysym(e);

    if (sym == XK_Escape)
        prompt_cancel(self);
    else if (sym == XK_Return || sym == XK_KP_Enter || sym == XK_space)
        prompt_run_callback(self, self->focus->result);
    else if (sym == XK_Tab || sym == XK_Left || sym == XK_Right) {
        gint i;
        gboolean left;
        ObPromptElement *oldfocus;

        left = (sym == XK_Left) || ((sym == XK_Tab) && shift);
        oldfocus = self->focus;

        for (i = 0; i < self->n_buttons; ++i)
            if (self->focus == &self->button[i]) break;
        i += (left ? -1 : 1);
        if (i < 0) i = self->n_buttons - 1;
        else if (i >= self->n_buttons) i = 0;
        self->focus = &self->button[i];

        if (oldfocus != self->focus) render_button(self, oldfocus);
        render_button(self, self->focus);
    }
    return TRUE;
}

gboolean prompt_mouse_event(ObPrompt *self, XEvent *e)
{
    gint i;
    ObPromptElement *but;

    if (e->type != ButtonPress && e->type != ButtonRelease &&
        e->type != MotionNotify) return FALSE;

    /* find the button */
    but = NULL;
    for (i = 0; i < self->n_buttons; ++i)
        if (self->button[i].window ==
            (e->type == MotionNotify ? e->xmotion.window : e->xbutton.window))
        {
            but = &self->button[i];
            break;
        }
    if (!but) return FALSE;

    if (e->type == ButtonPress) {
        ObPromptElement *oldfocus;

        oldfocus = self->focus;

        but->pressed = but->hover = TRUE;
        self->focus = but;

        if (oldfocus != but) render_button(self, oldfocus);
        render_button(self, but);
    }
    else if (e->type == ButtonRelease) {
        if (but->hover)
            prompt_run_callback(self, but->result);
        but->pressed = FALSE;
    }
    else if (e->type == MotionNotify) {
        if (but->pressed) {
            gboolean hover;

            hover = (e->xmotion.x >= 0 && e->xmotion.y >= 0 &&
                     e->xmotion.x < but->width && e->xmotion.y < but->height);

            if (hover != but->hover) {
                but->hover = hover;
                render_button(self, but);
            }
        }
    }
    return TRUE;
}

void prompt_cancel(ObPrompt *self)
{
    prompt_run_callback(self, self->cancel_result);
}

static gboolean prompt_show_message_cb(ObPrompt *p, int res, gpointer data)
{
    return TRUE; /* call the cleanup func */
}

static void prompt_show_message_cleanup(ObPrompt *p, gpointer data)
{
    prompt_unref(p);
}

ObPrompt* prompt_show_message(const gchar *msg, const gchar *title,
                              const gchar *answer)
{
    ObPrompt *p;
    ObPromptAnswer ans[] = {
        { answer, 0 }
    };

    p = prompt_new(msg, title, ans, 1, 0, 0,
                   prompt_show_message_cb, prompt_show_message_cleanup, NULL);
    prompt_show(p, NULL, FALSE);
    return p;
}

static void prompt_run_callback(ObPrompt *self, gint result)
{
    prompt_ref(self);
    if (self->func) {
        gboolean clean = self->func(self, result, self->data);
        if (clean && self->cleanup)
            self->cleanup(self, self->data);
    }
    prompt_hide(self);
    prompt_unref(self);
}
