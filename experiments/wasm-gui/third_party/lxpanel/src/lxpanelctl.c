/*
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006 Jim Huang <jserv.tw@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

/* Remote controller of lxpanel */

#include "lxpanelctl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static Display* dpy;

static const char usage[] =
        "\nlxpanelctl - LXPanel Controller\n"
        "Usage: lxpanelctl <command>\n\n"
        "Available commands:\n"
        "menu\t\t\tshow system menu\n"
        "run\t\t\tshow run dialog\n"
        "config\t\t\tshow configuration dialog\n"
        "restart\t\t\trestart lxpanel\n"
        "exit\t\t\texit lxpanel\n"
        "command <plugin> <cmd>\tsend a command to a plugin\n\n";

static int get_cmd( const char* cmd )
{
    if( ! strcmp( cmd, "menu") )
        return LXPANEL_CMD_SYS_MENU;
    else if( ! strcmp( cmd, "run") )
        return LXPANEL_CMD_RUN;
    else if( ! strcmp( cmd, "config") )
        return LXPANEL_CMD_CONFIG;
    else if( ! strcmp( cmd, "restart") )
        return LXPANEL_CMD_RESTART;
    else if( ! strcmp( cmd, "exit") )
        return LXPANEL_CMD_EXIT;
    else if( ! strcmp( cmd, "command") )
        return LXPANEL_CMD_COMMAND;
    return -1;
}

/* format: either "<edge>" or "<num>:<edge>" */
static int parse_id(const char *arg, int *mon)
{
    char *end;
    long lmon = strtoul(arg, &end, 10);

    if (*end == ':')
        arg = (const char *)end + 1;
    else
        lmon = 0;
    *mon = lmon;
    if (strcmp(arg, "top") == 0)
        return EDGE_TOP;
    if (strcmp(arg, "bottom") == 0)
        return EDGE_BOTTOM;
    if (strcmp(arg, "left") == 0)
        return EDGE_LEFT;
    if (strcmp(arg, "right") == 0)
        return EDGE_RIGHT;
    return EDGE_NONE;
}

int main( int argc, char** argv )
{
    char *display_name = (char *)getenv("DISPLAY");
    XEvent ev;
    Window root;
    Atom cmd_atom;
    int cmd;
    /* int restart; */
    /* target of message, it's XClientMessageEvent::b[1]
     * valid only if XClientMessageEvent::b[0] == LXPANEL_CMD_COMMAND */
    uint8_t target;

    if( argc < 2 )
    {
        printf( usage );
        return 1;
    }

    /*
    if( restart = !strcmp( argv[1], "restart" ) )
        argv[1] = "exit";
    */

    if( ( cmd = get_cmd( argv[1] ) ) == -1 )
        return 1;

    if (cmd == LXPANEL_CMD_COMMAND && argc < 4)
    {
        printf( usage );
        return 1;
    }

    dpy = XOpenDisplay(display_name);
    if (dpy == NULL) {
        printf("Cant connect to display: %s\n", display_name);
        exit(1);
    }
    root = DefaultRootWindow(dpy);
    cmd_atom = XInternAtom(dpy, "_LXPANEL_CMD", False);
    memset(&ev, '\0', sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = root;
    ev.xclient.message_type = cmd_atom;
    ev.xclient.format = 8;

    ev.xclient.data.b[0] = cmd;

    if (cmd == LXPANEL_CMD_COMMAND)
    {
        int i = 2;
        if (argc > 4 && strncmp(argv[2], "--panel=", 8) == 0)
        {
            int monitor;
            int edge = parse_id(argv[2] + 8, &monitor);
            /* edge: EDGE_NONE ..., monitor: 0 - none, 1...8 - selected */
            target = ((edge & 0x7) << 4) + (monitor & 0xf);
            i++;
        }
        else
            target = (EDGE_NONE << 4) + 0; /* edge: none, monitor: none */
        ev.xclient.data.b[1] = target;
        snprintf(&ev.xclient.data.b[2], 18, "%s\t%s", argv[i], argv[i+1]);
    }

    XSendEvent(dpy, root, False,
               SubstructureRedirectMask|SubstructureNotifyMask, &ev);
    XSync(dpy, False);
    XCloseDisplay(dpy);

/*
    if( restart ) {
        system( PACKAGE_BIN_DIR "/lxpanel &" );
    }
*/
    return 0;
}

