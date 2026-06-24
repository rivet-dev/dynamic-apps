/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   group.c for the Openbox window manager
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

#include "group.h"
#include "client.h"

static GHashTable *group_map;

static guint window_hash(Window *w) { return *w; }
static gboolean window_comp(Window *w1, Window *w2) { return *w1 == *w2; }

void group_startup(gboolean reconfig)
{
    if (reconfig) return;

    group_map = g_hash_table_new((GHashFunc)window_hash,
                                 (GEqualFunc)window_comp);
}

void group_shutdown(gboolean reconfig)
{
    if (reconfig) return;

    g_hash_table_destroy(group_map);
}

ObGroup *group_add(Window leader, ObClient *client)
{
    ObGroup *self;

    self = g_hash_table_lookup(group_map, &leader);
    if (self == NULL) {
        self = g_slice_new(ObGroup);
        self->leader = leader;
        self->members = NULL;
        g_hash_table_insert(group_map, &self->leader, self);
    }

    self->members = g_slist_append(self->members, client);

    return self;
}

void group_remove(ObGroup *self, ObClient *client)
{
    self->members = g_slist_remove(self->members, client);
    if (self->members == NULL) {
        g_hash_table_remove(group_map, &self->leader);
        g_slice_free(ObGroup, self);
    }
}
