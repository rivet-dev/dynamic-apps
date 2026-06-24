/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   prompt.h for the Openbox window manager
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

#ifndef ob__prompt_h
#define ob__prompt_h

#include "window.h"
#include "geom.h"
#include "obrender/render.h"
#include "obt/keyboard.h"
#include <glib.h>
#include <X11/Xlib.h>

typedef struct _ObPrompt       ObPrompt;
typedef struct _ObPromptElement ObPromptElement;
typedef struct _ObPromptAnswer ObPromptAnswer;

typedef gboolean (*ObPromptCallback)(ObPrompt *p, gint result, gpointer data);
typedef void (*ObPromptCleanup)(ObPrompt *p, gpointer data);

struct _ObPromptElement {
    gchar *text;
    Window window;

    gint x, y, width, height;
    gboolean pressed;
    gboolean hover;
    gint result;
};

struct _ObPrompt
{
    ObInternalWindow super;
    gint ref;

    ObtIC *ic;
    guint event_mask;

    /* keep a copy of this because we re-render things that may need it
       (i.e. the buttons) */
    RrAppearance *a_bg;

    gboolean mapped;
    gint width, height;
    gint msg_wbound;

    ObPromptElement msg;

    /* one for each answer */
    ObPromptElement *button;
    gint n_buttons;

    /* points to the button with the focus */
    ObPromptElement *focus;
    /* the default button to have selected */
    gint default_result;
    /* the cancel result if the dialog is closed */
    gint cancel_result;

    ObPromptCallback func;
    ObPromptCleanup cleanup;
    gpointer data;
};

struct _ObPromptAnswer {
    const gchar *text;
    gint result;
};

void prompt_startup(gboolean reconfig);
void prompt_shutdown(gboolean reconfig);

/*! Create a new prompt
  @param answers A number of ObPromptAnswers which define the buttons which
                 will appear in the dialog from left to right, and the result
                 returned when they are selected.
  @param n_answers The number of answers
  @param default_result The result for the answer button selected by default
  @param cancel_result The result that is given if the dialog is closed instead
         of having a button presssed
  @param func The callback function which is called when the dialog is closed
         or a button is pressed
  @param cleanup The cleanup function which is called if the prompt system
         is shutting down, and someone is still holding a reference to the
         prompt.  This callback should cause the prompt's refcount to go to
         zero so it can be freed, and free any other memory associated with
         the prompt.  The cleanup function is also called if the prompt's
         callback function returns TRUE.
  @param data User defined data which will be passed to the callback
*/
ObPrompt* prompt_new(const gchar *msg, const gchar *title,
                     const ObPromptAnswer *answers, gint n_answers,
                     gint default_result, gint cancel_result,
                     ObPromptCallback func, ObPromptCleanup cleanup,
                     gpointer data);
void prompt_ref(ObPrompt *self);
void prompt_unref(ObPrompt *self);

/*! Show the prompt.  It will be centered within the given area rectangle */
void prompt_show(ObPrompt *self, struct _ObClient *parent, gboolean modal);
void prompt_hide(ObPrompt *self);

gboolean prompt_key_event(ObPrompt *self, XEvent *e);
gboolean prompt_mouse_event(ObPrompt *self, XEvent *e);
void prompt_cancel(ObPrompt *self);

ObPrompt* prompt_show_message(const gchar *msg, const gchar *title,
                              const gchar *answer);

#endif
