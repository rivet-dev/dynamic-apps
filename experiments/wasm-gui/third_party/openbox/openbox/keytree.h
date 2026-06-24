/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   keytree.h for the Openbox window manager
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

#ifndef __plugin_keyboard_tree_h
#define __plugin_keyboard_tree_h

#include <glib.h>

typedef struct KeyBindingTree {
    guint state;
    guint key;
    GList *keylist;
    GSList *actions; /* list of Action pointers */
    gboolean chroot;

    /* the level up in the tree */
    struct KeyBindingTree *parent;
    /* the next binding in the tree at the same level */
    struct KeyBindingTree *next_sibling;
    /* the first child of this binding (next binding in a chained sequence).*/
    struct KeyBindingTree *first_child;
} KeyBindingTree;

void tree_destroy(KeyBindingTree *tree);
KeyBindingTree *tree_build(GList *keylist);
void tree_assimilate(KeyBindingTree *node);
KeyBindingTree *tree_find(KeyBindingTree *search, gboolean *conflict);
gboolean tree_chroot(KeyBindingTree *tree, GList *keylist);

#endif
