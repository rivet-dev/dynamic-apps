/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   menu.c for the Openbox window manager
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

#include "debug.h"
#include "menu.h"
#include "openbox.h"
#include "stacking.h"
#include "grab.h"
#include "client.h"
#include "config.h"
#include "actions.h"
#include "screen.h"
#include "menuframe.h"
#include "keyboard.h"
#include "geom.h"
#include "misc.h"
#include "client_menu.h"
#include "client_list_menu.h"
#include "client_list_combined_menu.h"
#include "gettext.h"
#include "obt/xml.h"
#include "obt/paths.h"

typedef struct _ObMenuParseState ObMenuParseState;

struct _ObMenuParseState
{
    ObMenu *parent;
    ObMenu *pipe_creator;
};

static GHashTable *menu_hash = NULL;
static ObtXmlInst *menu_parse_inst;
static ObMenuParseState menu_parse_state;
static gboolean menu_can_hide = FALSE;
static guint menu_timeout_id = 0;

static void menu_destroy_hash_value(ObMenu *self);
static void parse_menu_item(xmlNodePtr node, gpointer data);
static void parse_menu_separator(xmlNodePtr node, gpointer data);
static void parse_menu(xmlNodePtr node, gpointer data);
static gunichar parse_shortcut(const gchar *label, gboolean allow_shortcut,
                               gchar **strippedlabel, guint *position,
                               gboolean *always_show);

void menu_startup(gboolean reconfig)
{
    gboolean loaded = FALSE;
    GSList *it;

    menu_hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                      (GDestroyNotify)menu_destroy_hash_value);

    client_list_menu_startup(reconfig);
    client_list_combined_menu_startup(reconfig);
    client_menu_startup();

    menu_parse_inst = obt_xml_instance_new();

    menu_parse_state.parent = NULL;
    menu_parse_state.pipe_creator = NULL;
    obt_xml_register(menu_parse_inst, "menu", parse_menu, &menu_parse_state);
    obt_xml_register(menu_parse_inst, "item", parse_menu_item,
                     &menu_parse_state);
    obt_xml_register(menu_parse_inst, "separator",
                       parse_menu_separator, &menu_parse_state);

    for (it = config_menu_files; it; it = g_slist_next(it)) {
        if (obt_xml_load_config_file(menu_parse_inst,
                                     "openbox",
                                     it->data,
                                     "openbox_menu"))
        {
            loaded = TRUE;
            obt_xml_tree_from_root(menu_parse_inst);
            obt_xml_close(menu_parse_inst);
        }
        else if (obt_xml_load_file(menu_parse_inst,
                                   it->data,
                                   "openbox_menu"))
        {
            loaded = TRUE;
            obt_xml_tree_from_root(menu_parse_inst);
            obt_xml_close(menu_parse_inst);
        }
        else
            g_message(_("Unable to find a valid menu file \"%s\""),
                      (const gchar*)it->data);
    }
    if (!loaded) {
        if (obt_xml_load_config_file(menu_parse_inst,
                                     "openbox",
                                     "menu.xml",
                                     "openbox_menu"))
        {
            obt_xml_tree_from_root(menu_parse_inst);
            obt_xml_close(menu_parse_inst);
        } else
            g_message(_("Unable to find a valid menu file \"%s\""),
                      "menu.xml");
    }

    g_assert(menu_parse_state.parent == NULL);
}

void menu_shutdown(gboolean reconfig)
{
    obt_xml_instance_unref(menu_parse_inst);
    menu_parse_inst = NULL;

    menu_frame_hide_all();

    client_list_combined_menu_shutdown(reconfig);
    client_list_menu_shutdown(reconfig);

    g_hash_table_destroy(menu_hash);
    menu_hash = NULL;
}

static gboolean menu_pipe_submenu(gpointer key, gpointer val, gpointer data)
{
    ObMenu *menu = val;
    return menu->pipe_creator != NULL;
}

static void clear_cache(gpointer key, gpointer val, gpointer data)
{
    ObMenu *menu = val;
    if (menu->execute)
        menu_clear_entries(menu);
}

void menu_clear_pipe_caches(void)
{
    /* delete any pipe menus' submenus */
    g_hash_table_foreach_remove(menu_hash, menu_pipe_submenu, NULL);
    /* empty the top level pipe menus */
    g_hash_table_foreach(menu_hash, clear_cache, NULL);
}

void menu_pipe_execute(ObMenu *self)
{
    gchar *output;
    GError *err = NULL;

    if (!self->execute)
        return;
    if (self->entries) /* the entries are already created and cached */
        return;

    if (!g_spawn_command_line_sync(self->execute, &output, NULL, NULL, &err)) {
        g_message(_("Failed to execute command for pipe-menu \"%s\": %s"),
                  self->execute, err->message);
        g_error_free(err);
        return;
    }

    if (obt_xml_load_mem(menu_parse_inst, output, strlen(output),
                         "openbox_pipe_menu"))
    {
        menu_parse_state.pipe_creator = self;
        menu_parse_state.parent = self;
        obt_xml_tree_from_root(menu_parse_inst);
        obt_xml_close(menu_parse_inst);
    } else {
        g_message(_("Invalid output from pipe-menu \"%s\""), self->execute);
    }

    g_free(output);
}

static ObMenu* menu_from_name(gchar *name)
{
    ObMenu *self = NULL;

    g_assert(name != NULL);

    if (!(self = g_hash_table_lookup(menu_hash, name)))
        g_message(_("Attempted to access menu \"%s\" but it does not exist"),
                  name);
    return self;
}

#define VALID_SHORTCUT(c) (((c) >= '0' && (c) <= '9') || \
                           ((c) >= 'A' && (c) <= 'Z') || \
                           ((c) >= 'a' && (c) <= 'z'))

static gunichar parse_shortcut(const gchar *label, gboolean allow_shortcut,
                               gchar **strippedlabel, guint *position,
                               gboolean *always_show)
{
    gunichar shortcut = 0;

    *position = 0;
    *always_show = FALSE;

    g_assert(strippedlabel != NULL);

    if (label == NULL) {
        *strippedlabel = NULL;
    } else {
        gchar *i;
        gboolean escape;

        *strippedlabel = g_strdup(label);

        /* if allow_shortcut is false, then you can't use the '_', instead you
           have to just use the first valid character
        */

        /* allow __ to escape an underscore */
        i = *strippedlabel;
        do {
            escape = FALSE;
            i = strchr(i, '_');
            if (i && *(i+1) == '_') {
                gchar *j;

                /* remove the escape '_' from the string */
                for (j = i; *j != '\0'; ++j)
                    *j = *(j+1);

                ++i;
                escape = TRUE;
            }
        } while (escape);

        if (allow_shortcut && i != NULL) {
            /* there is an underscore in the string */

            /* you have to use a printable ascii character for shortcuts
               don't allow space either, so you can have like "a _ b"
            */
            if (VALID_SHORTCUT(*(i+1))) {
                shortcut = g_unichar_tolower(g_utf8_get_char(i+1));
                *position = i - *strippedlabel;
                *always_show = TRUE;

                /* remove the '_' from the string */
                for (; *i != '\0'; ++i)
                    *i = *(i+1);
            } else if (*(i+1) == '\0') {
                /* no default shortcut if the '_' is the last character
                   (eg. "Exit_") for menu entries that you don't want
                   to be executed by mistake
                */
                    *i = '\0';
            }
        } else {
            /* there is no underscore, so find the first valid character to use
               instead */

            for (i = *strippedlabel; *i != '\0'; ++i)
                if (VALID_SHORTCUT(*i)) {
                    *position = i - *strippedlabel;
                    shortcut = g_unichar_tolower(g_utf8_get_char(i));
                    break;
                }
        }
    }
    return shortcut;
}

static void parse_menu_item(xmlNodePtr node, gpointer data)
{
    ObMenuParseState *state = data;
    gchar *label;
    gchar *icon;
    ObMenuEntry *e;

    if (state->parent) {
        /* Don't try to extract "icon" attribute if icons in user-defined
           menus are not enabled. */

        if (obt_xml_attr_string_unstripped(node, "label", &label)) {
            xmlNodePtr c;
            GSList *acts = NULL;

            c = obt_xml_find_node(node->children, "action");
            while (c) {
                ObActionsAct *action = actions_parse(c);
                if (action)
                    acts = g_slist_append(acts, action);
                c = obt_xml_find_node(c->next, "action");
            }
            e = menu_add_normal(state->parent, -1, label, acts, TRUE);
            
            if (config_menu_show_icons &&
                obt_xml_attr_string(node, "icon", &icon))
            {
                e->data.normal.icon = RrImageNewFromName(ob_rr_icons, icon);

                if (e->data.normal.icon)
                    e->data.normal.icon_alpha = 0xff;

                g_free(icon);
            }
            g_free(label);
        }
    }
}

static void parse_menu_separator(xmlNodePtr node, gpointer data)
{
    ObMenuParseState *state = data;

    if (state->parent) {
        gchar *label;

        if (!obt_xml_attr_string_unstripped(node, "label", &label))
            label = NULL;

        menu_add_separator(state->parent, -1, label);
        g_free(label);
    }
}

static void parse_menu(xmlNodePtr node, gpointer data)
{
    ObMenuParseState *state = data;
    gchar *name = NULL, *title = NULL, *script = NULL;
    ObMenu *menu;
    ObMenuEntry *e;
    gchar *icon;

    if (!obt_xml_attr_string(node, "id", &name))
        goto parse_menu_fail;

    if (!g_hash_table_lookup(menu_hash, name)) {
        if (!obt_xml_attr_string_unstripped(node, "label", &title))
            goto parse_menu_fail;

        if ((menu = menu_new(name, title, TRUE, NULL))) {
            menu->pipe_creator = state->pipe_creator;
            if (obt_xml_attr_string(node, "execute", &script)) {
                menu->execute = obt_paths_expand_tilde(script);
            } else {
                ObMenu *old;

                old = state->parent;
                state->parent = menu;
                obt_xml_tree(menu_parse_inst, node->children);
                state->parent = old;
            }
        }
    }

    if (state->parent) {
        e = menu_add_submenu(state->parent, -1, name);

        if (config_menu_show_icons &&
            obt_xml_attr_string(node, "icon", &icon))
        {
            e->data.submenu.icon = RrImageNewFromName(ob_rr_icons, icon);

            if (e->data.submenu.icon)
                e->data.submenu.icon_alpha = 0xff;

            g_free(icon);
        }
    }

parse_menu_fail:
    g_free(name);
    g_free(title);
    g_free(script);
}

ObMenu* menu_new(const gchar *name, const gchar *title,
                 gboolean allow_shortcut_selection, gpointer data)
{
    ObMenu *self;

    self = g_slice_new0(ObMenu);
    self->name = g_strdup(name);
    self->data = data;

    self->shortcut = parse_shortcut(title, allow_shortcut_selection,
                                    &self->title, &self->shortcut_position,
                                    &self->shortcut_always_show);
    self->collate_key = g_utf8_collate_key(self->title, -1);

    g_hash_table_replace(menu_hash, self->name, self);

    /* Each menu has a single more_menu.  When the menu spills past what
       can fit on the screen, a new menu frame entry is created from this
       more_menu, and a new menu frame for the submenu is created for this
       menu, also pointing to the more_menu.

       This can be done multiple times using the same more_menu.

       more_menu->more_menu will always be NULL, since there is only 1 for
       each menu. */
    self->more_menu = g_slice_new0(ObMenu);
    self->more_menu->name = _("More...");
    self->more_menu->title = _("More...");
    self->more_menu->collate_key = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
    self->more_menu->data = data;
    self->more_menu->shortcut = g_unichar_tolower(g_utf8_get_char("M"));

    return self;
}

static void menu_destroy_hash_value(ObMenu *self)
{
    /* make sure its not visible */
    {
        GList *it;
        ObMenuFrame *f;

        for (it = menu_frame_visible; it; it = g_list_next(it)) {
            f = it->data;
            if (f->menu == self)
                menu_frame_hide_all();
        }
    }

    if (self->destroy_func)
        self->destroy_func(self, self->data);

    menu_clear_entries(self);
    g_free(self->name);
    g_free(self->title);
    g_free(self->collate_key);
    g_free(self->execute);
    g_slice_free(ObMenu, self->more_menu);

    g_slice_free(ObMenu, self);
}

void menu_free(ObMenu *menu)
{
    if (menu)
        g_hash_table_remove(menu_hash, menu->name);
}

static gboolean menu_hide_delay_func(gpointer data)
{
    menu_can_hide = TRUE;
    menu_timeout_id = 0;

    return FALSE; /* no repeat */
}

void menu_show(gchar *name, const GravityPoint *pos, gint monitor,
               gboolean mouse, gboolean user_positioned, ObClient *client)
{
    ObMenu *self;
    ObMenuFrame *frame;

    if (!(self = menu_from_name(name)) ||
        grab_on_keyboard() || grab_on_pointer()) return;

    /* if the requested menu is already the top visible menu, then don't
       bother */
    if (menu_frame_visible) {
        frame = menu_frame_visible->data;
        if (frame->menu == self)
            return;
    }

    menu_frame_hide_all();

    /* clear the pipe menus when showing a new menu */
    menu_clear_pipe_caches();

    frame = menu_frame_new(self, 0, client);
    if (!menu_frame_show_topmenu(frame, pos, monitor, mouse, user_positioned))
        menu_frame_free(frame);
    else {
        if (!mouse) {
            /* select the first entry if it's not a submenu and we opened
             * the menu with the keyboard, and skip all headers */
            GList *it = frame->entries;
            while (it) {
                ObMenuEntryFrame *e = it->data;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL) {
                    menu_frame_select(frame, e, FALSE);
                    break;
                } else if (e->entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR)
                    it = g_list_next(it);
                else
                    break;
            }
        }

        /* reset the hide timer */
        if (!mouse)
            menu_can_hide = TRUE;
        else {
            menu_can_hide = FALSE;
            if (menu_timeout_id) g_source_remove(menu_timeout_id);
            menu_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                 config_menu_hide_delay,
                                                 menu_hide_delay_func,
                                                 NULL, NULL);
        }
    }
}

gboolean menu_hide_delay_reached(void)
{
    return menu_can_hide;
}

void menu_hide_delay_reset(void)
{
    if (menu_timeout_id) g_source_remove(menu_timeout_id);
    menu_hide_delay_func(NULL);
}

static ObMenuEntry* menu_entry_new(ObMenu *menu, ObMenuEntryType type, gint id)
{
    ObMenuEntry *self;

    g_assert(menu);

    self = g_slice_new0(ObMenuEntry);
    self->ref = 1;
    self->type = type;
    self->menu = menu;
    self->id = id;

    switch (type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
        self->data.normal.enabled = TRUE;
        break;
    case OB_MENU_ENTRY_TYPE_SUBMENU:
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        break;
    }

    return self;
}

void menu_entry_ref(ObMenuEntry *self)
{
    ++self->ref;
}

void menu_entry_unref(ObMenuEntry *self)
{
    if (self && --self->ref == 0) {
        switch (self->type) {
        case OB_MENU_ENTRY_TYPE_NORMAL:
            RrImageUnref(self->data.normal.icon);
            g_free(self->data.normal.label);
            g_free(self->data.normal.collate_key);
            while (self->data.normal.actions) {
                actions_act_unref(self->data.normal.actions->data);
                self->data.normal.actions =
                    g_slist_delete_link(self->data.normal.actions,
                                        self->data.normal.actions);
            }
            break;
        case OB_MENU_ENTRY_TYPE_SUBMENU:
            RrImageUnref(self->data.submenu.icon);
            g_free(self->data.submenu.name);
            break;
        case OB_MENU_ENTRY_TYPE_SEPARATOR:
            g_free(self->data.separator.label);
            break;
        }

        g_slice_free(ObMenuEntry, self);
    }
}

void menu_clear_entries(ObMenu *self)
{
#ifdef DEBUG
    /* assert that the menu isn't visible */
    {
        GList *it;
        ObMenuFrame *f;

        for (it = menu_frame_visible; it; it = g_list_next(it)) {
            f = it->data;
            g_assert(f->menu != self);
        }
    }
#endif

    while (self->entries) {
        menu_entry_unref(self->entries->data);
        self->entries = g_list_delete_link(self->entries, self->entries);
    }
    self->more_menu->entries = self->entries; /* keep it in sync */
}

void menu_entry_remove(ObMenuEntry *self)
{
    self->menu->entries = g_list_remove(self->menu->entries, self);
    menu_entry_unref(self);
}

ObMenuEntry* menu_add_normal(ObMenu *self, gint id, const gchar *label,
                             GSList *actions, gboolean allow_shortcut)
{
    ObMenuEntry *e;

    e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_NORMAL, id);
    e->data.normal.actions = actions;

    menu_entry_set_label(e, label, allow_shortcut);

    self->entries = g_list_append(self->entries, e);
    self->more_menu->entries = self->entries; /* keep it in sync */
    return e;
}

ObMenuEntry* menu_get_more(ObMenu *self, guint show_from)
{
    ObMenuEntry *e;
    e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_SUBMENU, -1);
    /* points to itself */
    e->data.submenu.name = g_strdup(self->name);
    e->data.submenu.submenu = self;
    e->data.submenu.show_from = show_from;
    return e;
}

ObMenuEntry* menu_add_submenu(ObMenu *self, gint id, const gchar *submenu)
{
    ObMenuEntry *e;

    e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_SUBMENU, id);
    e->data.submenu.name = g_strdup(submenu);

    self->entries = g_list_append(self->entries, e);
    self->more_menu->entries = self->entries; /* keep it in sync */
    return e;
}

ObMenuEntry* menu_add_separator(ObMenu *self, gint id, const gchar *label)
{
    ObMenuEntry *e;

    e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_SEPARATOR, id);

    menu_entry_set_label(e, label, FALSE);

    self->entries = g_list_append(self->entries, e);
    self->more_menu->entries = self->entries; /* keep it in sync */
    return e;
}

void menu_set_show_func(ObMenu *self, ObMenuShowFunc func)
{
    self->show_func = func;
}

void menu_set_hide_func(ObMenu *self, ObMenuHideFunc func)
{
    self->hide_func = func;
}

void menu_set_update_func(ObMenu *self, ObMenuUpdateFunc func)
{
    self->update_func = func;
}

void menu_set_execute_func(ObMenu *self, ObMenuExecuteFunc func)
{
    self->execute_func = func;
    self->more_menu->execute_func = func; /* keep it in sync */
}

void menu_set_cleanup_func(ObMenu *self, ObMenuCleanupFunc func)
{
    self->cleanup_func = func;
}

void menu_set_destroy_func(ObMenu *self, ObMenuDestroyFunc func)
{
    self->destroy_func = func;
}

void menu_set_place_func(ObMenu *self, ObMenuPlaceFunc func)
{
    self->place_func = func;
}

ObMenuEntry* menu_find_entry_id(ObMenu *self, gint id)
{
    ObMenuEntry *ret = NULL;
    GList *it;

    for (it = self->entries; it; it = g_list_next(it)) {
        ObMenuEntry *e = it->data;

        if (e->id == id) {
            ret = e;
            break;
        }
    }
    return ret;
}

void menu_find_submenus(ObMenu *self)
{
    GList *it;

    for (it = self->entries; it; it = g_list_next(it)) {
        ObMenuEntry *e = it->data;

        if (e->type == OB_MENU_ENTRY_TYPE_SUBMENU)
            e->data.submenu.submenu = menu_from_name(e->data.submenu.name);
    }
}

void menu_entry_set_label(ObMenuEntry *self, const gchar *label,
                          gboolean allow_shortcut)
{
    switch (self->type) {
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        g_free(self->data.separator.label);
        self->data.separator.label = g_strdup(label);
        break;
    case OB_MENU_ENTRY_TYPE_NORMAL:
        g_free(self->data.normal.label);
        g_free(self->data.normal.collate_key);
        self->data.normal.shortcut =
            parse_shortcut(label, allow_shortcut, &self->data.normal.label,
                           &self->data.normal.shortcut_position,
                           &self->data.normal.shortcut_always_show);
        self->data.normal.collate_key =
            g_utf8_collate_key(self->data.normal.label, -1);
        break;
    default:
        g_assert_not_reached();
    }
}

void menu_show_all_shortcuts(ObMenu *self, gboolean show)
{
    self->show_all_shortcuts = show;
}

static int sort_func(const void *a, const void *b) {
    const ObMenuEntry *e[2] = {*(ObMenuEntry**)a, *(ObMenuEntry**)b};
    const gchar *k[2];
    gint i;

    for (i = 0; i < 2; ++i) {
        if (e[i]->type == OB_MENU_ENTRY_TYPE_NORMAL)
            k[i] = e[i]->data.normal.collate_key;
        else {
            g_assert(e[i]->type == OB_MENU_ENTRY_TYPE_SUBMENU);
            if (e[i]->data.submenu.submenu)
                k[i] = e[i]->data.submenu.submenu->collate_key;
            else
                return -1; /* arbitrary really.. the submenu doesn't exist. */
        }
    }
    return strcmp(k[0], k[1]);
}

/*!
  @param start The first entry in the range to sort.
  @param end The last entry in the range to sort.
*/
static void sort_range(ObMenu *self, GList *start, GList *end, guint len)
{
    ObMenuEntry **ar;
    GList *it;
    guint i;
    if (!len) return;

    ar = g_slice_alloc(sizeof(ObMenuEntry*) * len);
    for (i = 0, it = start; it != g_list_next(end); ++i, it = g_list_next(it))
        ar[i] = it->data;
    qsort(ar, len, sizeof(ObMenuEntry*), sort_func);
    for (i = 0, it = start; it != g_list_next(end); ++i, it = g_list_next(it))
        it->data = ar[i];
    g_slice_free1(sizeof(ObMenuEntry*) * len, ar);
}

void menu_sort_entries(ObMenu *self)
{
    GList *it, *start, *end, *last;
    guint len;

    /* need the submenus to know their labels for sorting */
    menu_find_submenus(self);

    start = self->entries;
    len = 0;
    for (it = self->entries; it; it = g_list_next(it)) {
        ObMenuEntry *e = it->data;
        if (e->type == OB_MENU_ENTRY_TYPE_SEPARATOR) {
            end = g_list_previous(it);
            sort_range(self, start, end, len);

            it = g_list_next(it); /* skip over the separator */
            start = it;
            len = 0;
        }
        else
            len += 1;
        last = it;
    }
    sort_range(self, start, last, len);
}
