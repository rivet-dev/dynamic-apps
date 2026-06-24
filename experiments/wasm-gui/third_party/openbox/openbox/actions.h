/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   actions.h for the Openbox window manager
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

#include "misc.h"
#include "frame.h"
#include "obt/xml.h"
#include "obt/keyboard.h"

#include <glib.h>
#include <X11/Xlib.h>

typedef struct _ObActionsDefinition   ObActionsDefinition;
typedef struct _ObActionsAct          ObActionsAct;
typedef struct _ObActionsData         ObActionsData;
typedef struct _ObActionsAnyData      ObActionsAnyData;
typedef struct _ObActionsGlobalData   ObActionsGlobalData;
typedef struct _ObActionsClientData   ObActionsClientData;
typedef struct _ObActionsSelectorData ObActionsSelectorData;

typedef void     (*ObActionsDataFreeFunc)(gpointer options);
typedef gboolean (*ObActionsRunFunc)(ObActionsData *data,
                                     gpointer options);
typedef gpointer (*ObActionsDataSetupFunc)(xmlNodePtr node);
typedef void     (*ObActionsShutdownFunc)(void);

/* functions for interactive actions */
/* return TRUE if the action is going to be interactive, or false to change
   your mind and make it not */
typedef gboolean (*ObActionsIPreFunc)(guint initial_state, gpointer options);
typedef void     (*ObActionsIPostFunc)(gpointer options);
typedef gboolean (*ObActionsIInputFunc)(guint initial_state,
                                        XEvent *e,
                                        ObtIC *ic,
                                        gpointer options,
                                        gboolean *used);
typedef void     (*ObActionsICancelFunc)(gpointer options);
typedef gpointer (*ObActionsIDataSetupFunc)(xmlNodePtr node,
                                            ObActionsIPreFunc *pre,
                                            ObActionsIInputFunc *input,
                                            ObActionsICancelFunc *cancel,
                                            ObActionsIPostFunc *post);

struct _ObActionsData {
    ObUserAction uact;
    guint state;
    gint x;
    gint y;
    gint button;

    struct _ObClient *client;
    ObFrameContext context;
};

void actions_startup(gboolean reconfigure);
void actions_shutdown(gboolean reconfigure);

/*! Use this if the actions created from this name may be interactive */
gboolean actions_register_i(const gchar *name,
                            ObActionsIDataSetupFunc setup,
                            ObActionsDataFreeFunc free,
                            ObActionsRunFunc run);

gboolean actions_register(const gchar *name,
                          ObActionsDataSetupFunc setup,
                          ObActionsDataFreeFunc free,
                          ObActionsRunFunc run);

gboolean actions_set_shutdown(const gchar *name,
                              ObActionsShutdownFunc shutdown);
gboolean actions_set_modifies_focused_window(const gchar *name,
                                             gboolean modifies);
gboolean actions_set_can_stop(const gchar *name,
                              gboolean modifies);

ObActionsAct* actions_parse(xmlNodePtr node);
ObActionsAct* actions_parse_string(const gchar *name);

gboolean actions_act_is_interactive(ObActionsAct *act);

void actions_act_ref(ObActionsAct *act);
void actions_act_unref(ObActionsAct *act);

/*! When this is true, an XAllowEvents with ReplayPointer will be called
  if an action is going to maybe try moving windows around on screen (or
  map/unmap windows)
*/
void actions_set_need_pointer_replay_before_move(gboolean replay);
/*! Returns if a ReplayPointer is still needed.  If it was called while running
  actions then this will be false */
gboolean actions_get_need_pointer_replay_before_move(void);

/*! Pass in a GSList of ObActionsAct's to be run. */
void actions_run_acts(GSList *acts,
                      ObUserAction uact,
                      guint state,
                      gint x,
                      gint y,
                      gint button,
                      ObFrameContext con,
                      struct _ObClient *client);

gboolean actions_interactive_act_running(void);
void actions_interactive_cancel_act(void);

gboolean actions_interactive_input_event(XEvent *e);

/*! Function for actions to call when they are moving a client around */
void actions_client_move(ObActionsData *data, gboolean start);
