/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   client_list_menu.c for the Openbox window manager
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

#include "openbox.h"
#include "menu.h"
#include "menuframe.h"
#include "screen.h"
#include "client.h"
#include "client_list_combined_menu.h"
#include "focus.h"
#include "config.h"
#include "gettext.h"

#include <glib.h>

#define MENU_NAME "client-list-combined-menu"

static ObMenu *combined_menu;

#define SEPARATOR -1
#define ADD_DESKTOP -2
#define REMOVE_DESKTOP -3

static void self_cleanup(ObMenu *menu, gpointer data)
{
    menu_clear_entries(menu);
}

static gboolean self_update(ObMenuFrame *frame, gpointer data)
{
    ObMenu *menu = frame->menu;
    ObMenuEntry *e;
    GList *it;
    guint desktop;

    menu_clear_entries(menu);

    for (desktop = 0; desktop < screen_num_desktops; desktop++) {
        gboolean empty = TRUE;
        gboolean onlyiconic = TRUE;

        menu_add_separator(menu, SEPARATOR, screen_desktop_names[desktop]);
        for (it = focus_order; it; it = g_list_next(it)) {
            ObClient *c = it->data;
            if (focus_valid_target(c, desktop,
                                   TRUE, TRUE,
                                   FALSE, TRUE, FALSE, FALSE, FALSE))
            {
                empty = FALSE;

                if (c->iconic) {
                    gchar *title = g_strdup_printf("(%s)", c->icon_title);
                    e = menu_add_normal(menu, desktop, title, NULL, FALSE);
                    g_free(title);
                } else {
                    onlyiconic = FALSE;
                    e = menu_add_normal(menu, desktop, c->title, NULL, FALSE);
                }

                if (config_menu_show_icons) {
                    e->data.normal.icon = client_icon(c);
                    RrImageRef(e->data.normal.icon);
                    e->data.normal.icon_alpha =
                        c->iconic ? OB_ICONIC_ALPHA : 0xff;
                }

                e->data.normal.data = c;
            }
        }

        if (empty || onlyiconic) {
            /* no entries or only iconified windows, so add a
             * way to go to this desktop without uniconifying a window */
            if (!empty)
                menu_add_separator(menu, SEPARATOR, NULL);

            e = menu_add_normal(menu, desktop, _("Go there..."), NULL, TRUE);
            if (desktop == screen_desktop)
                e->data.normal.enabled = FALSE;
        }
    }

    if (config_menu_manage_desktops) {
        menu_add_separator(menu, SEPARATOR, _("Manage desktops"));
        menu_add_normal(menu, ADD_DESKTOP, _("_Add new desktop"), NULL, TRUE);
        menu_add_normal(menu, REMOVE_DESKTOP, _("_Remove last desktop"),
                        NULL, TRUE);
    }

    return TRUE; /* always show the menu */
}

static void menu_execute(ObMenuEntry *self, ObMenuFrame *f,
                         ObClient *c, guint state, gpointer data)
{
    if (self->id == ADD_DESKTOP) {
        screen_add_desktop(FALSE);
        menu_frame_hide_all();
    }
    else if (self->id == REMOVE_DESKTOP) {
        screen_remove_desktop(FALSE);
        menu_frame_hide_all();
    }
    else {
        ObClient *t = self->data.normal.data;
        if (t) { /* it's set to NULL if its destroyed */
            gboolean here = state & ShiftMask;

            client_activate(t, TRUE, here, TRUE, TRUE, TRUE);
            /* if the window is omnipresent then we need to go to its
               desktop */
            if (!here && t->desktop == DESKTOP_ALL)
                screen_set_desktop(self->id, FALSE);
        }
        else
            screen_set_desktop(self->id, TRUE);
    }
}

static void client_dest(ObClient *client, gpointer data)
{
    /* This concise function removes all references to a closed
     * client in the client_list_menu, so we don't have to check
     * in client.c */
    GList *eit;
    for (eit = combined_menu->entries; eit; eit = g_list_next(eit)) {
        ObMenuEntry *meit = eit->data;
        if (meit->type == OB_MENU_ENTRY_TYPE_NORMAL &&
            meit->data.normal.data == client)
        {
            meit->data.normal.data = NULL;
        }
    }
}

void client_list_combined_menu_startup(gboolean reconfig)
{
    if (!reconfig)
        client_add_destroy_notify(client_dest, NULL);

    combined_menu = menu_new(MENU_NAME, _("Windows"), TRUE, NULL);
    menu_set_update_func(combined_menu, self_update);
    menu_set_cleanup_func(combined_menu, self_cleanup);
    menu_set_execute_func(combined_menu, menu_execute);
}

void client_list_combined_menu_shutdown(gboolean reconfig)
{
    if (!reconfig)
        client_remove_destroy_notify(client_dest);
}
