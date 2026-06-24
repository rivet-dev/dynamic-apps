/*
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2010 Marty Jack <martyj19@comcast.net>
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

#ifndef _LXPANELCTL_H
#define _LXPANELCTL_H

/* Commands controlling lxpanel.
 * These are the parameter of a _LXPANEL_CMD ClientMessage to the root window.
 * Endianness alert:  Note that the parameter is in b[0], not l[0]. */
typedef enum {
    LXPANEL_CMD_NONE,
    LXPANEL_CMD_SYS_MENU,
    LXPANEL_CMD_RUN,
    LXPANEL_CMD_CONFIG,
    LXPANEL_CMD_RESTART,
    LXPANEL_CMD_EXIT,
    LXPANEL_CMD_COMMAND
} PanelControlCommand;

/* this enum was in private.h but it is used by LXPANEL_CMD_COMMAND now */
enum { EDGE_NONE=0, EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM };

#endif
