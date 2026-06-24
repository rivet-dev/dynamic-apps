/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   menuframe.c for the Openbox window manager
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

#include "menuframe.h"
#include "client.h"
#include "menu.h"
#include "screen.h"
#include "actions.h"
#include "event.h"
#include "grab.h"
#include "openbox.h"
#include "config.h"
#include "obt/prop.h"
#include "obt/keyboard.h"
#include "obrender/theme.h"

#define PADDING 2
#define MAX_MENU_WIDTH 400

#define ITEM_HEIGHT (ob_rr_theme->menu_font_height + 2*PADDING)

#define FRAME_EVENTMASK (ButtonPressMask |ButtonMotionMask | EnterWindowMask |\
                         LeaveWindowMask)
#define ENTRY_EVENTMASK (EnterWindowMask | LeaveWindowMask | \
                         ButtonPressMask | ButtonReleaseMask | \
                         PointerMotionMask)

GList *menu_frame_visible;
GHashTable *menu_frame_map;

static RrAppearance *a_sep;
static guint submenu_show_timer = 0;
static guint submenu_hide_timer = 0;

static ObMenuEntryFrame* menu_entry_frame_new(ObMenuEntry *entry,
                                              ObMenuFrame *frame);
static void menu_entry_frame_free(ObMenuEntryFrame *self);
static void menu_frame_update(ObMenuFrame *self);
static gboolean submenu_show_timeout(gpointer data);
static void menu_frame_hide(ObMenuFrame *self);

static gboolean submenu_hide_timeout(gpointer data);

static Window createWindow(Window parent, gulong mask,
                           XSetWindowAttributes *attrib)
{
    return XCreateWindow(obt_display, parent, 0, 0, 1, 1, 0,
                         RrDepth(ob_rr_inst), InputOutput,
                         RrVisual(ob_rr_inst), mask, attrib);
}

static void client_dest(ObClient *client, gpointer data)
{
    GList *it;

    /* menus can be associated with a client, so null those refs since
       we are disappearing now */
    for (it = menu_frame_visible; it; it = g_list_next(it)) {
        ObMenuFrame *f = it->data;
        if (f->client == client)
            f->client = NULL;
    }
}

void menu_frame_startup(gboolean reconfig)
{
    gint i;

    a_sep = RrAppearanceCopy(ob_rr_theme->a_clear);
    RrAppearanceAddTextures(a_sep, ob_rr_theme->menu_sep_width);
    for (i = 0; i < ob_rr_theme->menu_sep_width; ++i) {
        a_sep->texture[i].type = RR_TEXTURE_LINE_ART;
        a_sep->texture[i].data.lineart.color =
            ob_rr_theme->menu_sep_color;
    }

    if (reconfig) return;

    client_add_destroy_notify(client_dest, NULL);
    menu_frame_map = g_hash_table_new(g_int_hash, g_int_equal);
}

void menu_frame_shutdown(gboolean reconfig)
{
    RrAppearanceFree(a_sep);

    if (reconfig) return;

    client_remove_destroy_notify(client_dest);
    g_hash_table_destroy(menu_frame_map);
}

ObMenuFrame* menu_frame_new(ObMenu *menu, guint show_from, ObClient *client)
{
    ObMenuFrame *self;
    XSetWindowAttributes attr;

    self = g_slice_new0(ObMenuFrame);
    self->obwin.type = OB_WINDOW_CLASS_MENUFRAME;
    self->menu = menu;
    self->selected = NULL;
    self->client = client;
    self->direction_right = TRUE;
    self->show_from = show_from;

    attr.event_mask = FRAME_EVENTMASK;
    self->window = createWindow(obt_root(ob_screen),
                                CWEventMask, &attr);

    /* make it a popup menu type window */
    OBT_PROP_SET32(self->window, NET_WM_WINDOW_TYPE, ATOM,
                   OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_POPUP_MENU));

    XSetWindowBorderWidth(obt_display, self->window, ob_rr_theme->mbwidth);
    XSetWindowBorder(obt_display, self->window,
                     RrColorPixel(ob_rr_theme->menu_border_color));

    self->a_items = RrAppearanceCopy(ob_rr_theme->a_menu);

    window_add(&self->window, MENUFRAME_AS_WINDOW(self));
    stacking_add(MENUFRAME_AS_WINDOW(self));

    return self;
}

void menu_frame_free(ObMenuFrame *self)
{
    if (self) {
        while (self->entries) {
            menu_entry_frame_free(self->entries->data);
            self->entries = g_list_delete_link(self->entries, self->entries);
        }

        stacking_remove(MENUFRAME_AS_WINDOW(self));
        window_remove(self->window);

        RrAppearanceFree(self->a_items);

        XDestroyWindow(obt_display, self->window);

        g_slice_free(ObMenuFrame, self);
    }
}

ObtIC* menu_frame_ic(ObMenuFrame *self)
{
    /* menus are always used through a grab right now, so they can always use
       the grab input context */
    return grab_input_context();
}

static ObMenuEntryFrame* menu_entry_frame_new(ObMenuEntry *entry,
                                              ObMenuFrame *frame)
{
    ObMenuEntryFrame *self;
    XSetWindowAttributes attr;

    self = g_slice_new0(ObMenuEntryFrame);
    self->entry = entry;
    self->frame = frame;

    menu_entry_ref(entry);

    attr.event_mask = ENTRY_EVENTMASK;
    self->window = createWindow(self->frame->window, CWEventMask, &attr);
    self->text = createWindow(self->window, 0, NULL);
    g_hash_table_insert(menu_frame_map, &self->window, self);
    g_hash_table_insert(menu_frame_map, &self->text, self);
    if ((entry->type == OB_MENU_ENTRY_TYPE_NORMAL) ||
        (entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)) {
        self->icon = createWindow(self->window, 0, NULL);
        g_hash_table_insert(menu_frame_map, &self->icon, self);
    }
    if (entry->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
        self->bullet = createWindow(self->window, 0, NULL);
        g_hash_table_insert(menu_frame_map, &self->bullet, self);
    }

    XMapWindow(obt_display, self->window);
    XMapWindow(obt_display, self->text);

    window_add(&self->window, MENUFRAME_AS_WINDOW(self->frame));

    return self;
}

static void menu_entry_frame_free(ObMenuEntryFrame *self)
{
    if (self) {
        window_remove(self->window);

        XDestroyWindow(obt_display, self->text);
        XDestroyWindow(obt_display, self->window);
        g_hash_table_remove(menu_frame_map, &self->text);
        g_hash_table_remove(menu_frame_map, &self->window);
        if ((self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL) ||
            (self->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)) {
            XDestroyWindow(obt_display, self->icon);
            g_hash_table_remove(menu_frame_map, &self->icon);
        }
        if (self->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
            XDestroyWindow(obt_display, self->bullet);
            g_hash_table_remove(menu_frame_map, &self->bullet);
        }

        menu_entry_unref(self->entry);
        g_slice_free(ObMenuEntryFrame, self);
    }
}

void menu_frame_move(ObMenuFrame *self, gint x, gint y)
{
    RECT_SET_POINT(self->area, x, y);
    self->monitor = screen_find_monitor_point(x, y);
    XMoveWindow(obt_display, self->window, self->area.x, self->area.y);
}

static void menu_frame_place_topmenu(ObMenuFrame *self, const GravityPoint *pos,
                                     gint *x, gint *y, gint monitor,
                                     gboolean user_positioned)
{
    gint dx, dy;

    screen_apply_gravity_point(x, y, self->area.width, self->area.height,
                               pos, screen_physical_area_monitor(monitor));

    if (user_positioned)
        return;

    if (config_menu_middle) {
        gint myx;

        myx = *x;
        *y -= self->area.height / 2;

        /* try to the right of the cursor */
        menu_frame_move_on_screen(self, myx, *y, &dx, &dy);
        self->direction_right = TRUE;
        if (dx != 0) {
            /* try to the left of the cursor */
            myx = *x - self->area.width;
            menu_frame_move_on_screen(self, myx, *y, &dx, &dy);
            self->direction_right = FALSE;
        }
        if (dx != 0) {
            /* if didnt fit on either side so just use what it says */
            myx = *x;
            menu_frame_move_on_screen(self, myx, *y, &dx, &dy);
            self->direction_right = TRUE;
        }
        *x = myx + dx;
        *y += dy;
    } else {
        gint myx, myy;

        myx = *x;
        myy = *y;

        /* try to the bottom right of the cursor */
        menu_frame_move_on_screen(self, myx, myy, &dx, &dy);
        self->direction_right = TRUE;
        if (dx != 0 || dy != 0) {
            /* try to the bottom left of the cursor */
            myx = *x - self->area.width;
            myy = *y;
            menu_frame_move_on_screen(self, myx, myy, &dx, &dy);
            self->direction_right = FALSE;
        }
        if (dx != 0 || dy != 0) {
            /* try to the top right of the cursor */
            myx = *x;
            myy = *y - self->area.height;
            menu_frame_move_on_screen(self, myx, myy, &dx, &dy);
            self->direction_right = TRUE;
        }
        if (dx != 0 || dy != 0) {
            /* try to the top left of the cursor */
            myx = *x - self->area.width;
            myy = *y - self->area.height;
            menu_frame_move_on_screen(self, myx, myy, &dx, &dy);
            self->direction_right = FALSE;
        }
        if (dx != 0 || dy != 0) {
            /* if didnt fit on either side so just use what it says */
            myx = *x;
            myy = *y;
            menu_frame_move_on_screen(self, myx, myy, &dx, &dy);
            self->direction_right = TRUE;
        }
        *x = myx + dx;
        *y = myy + dy;
    }
}

static void menu_frame_place_submenu(ObMenuFrame *self, gint *x, gint *y)
{
    gint overlapx, overlapy;
    gint bwidth;

    overlapx = ob_rr_theme->menu_overlap_x;
    overlapy = ob_rr_theme->menu_overlap_y;
    bwidth = ob_rr_theme->mbwidth;

    if (self->direction_right)
        *x = self->parent->area.x + self->parent->area.width -
            overlapx - bwidth;
    else
        *x = self->parent->area.x - self->area.width + overlapx + bwidth;

    *y = self->parent->area.y + self->parent_entry->area.y;
    if (config_menu_middle)
        *y -= (self->area.height - (bwidth * 2) - ITEM_HEIGHT) / 2;
    else
        *y += overlapy;
}

void menu_frame_move_on_screen(ObMenuFrame *self, gint x, gint y,
                               gint *dx, gint *dy)
{
    const Rect *a = NULL;
    Rect search = self->area;
    gint pos, half, monitor;

    *dx = *dy = 0;
    RECT_SET_POINT(search, x, y);

    if (self->parent)
        monitor = self->parent->monitor;
    else
        monitor = screen_find_monitor(&search);

    a = screen_physical_area_monitor(monitor);

    half = g_list_length(self->entries) / 2;
    pos = g_list_index(self->entries, self->selected);

    /* if in the bottom half then check this stuff first, will keep the bottom
       edge of the menu visible */
    if (pos > half) {
        *dx = MAX(*dx, a->x - x);
        *dy = MAX(*dy, a->y - y);
    }
    *dx = MIN(*dx, (a->x + a->width) - (x + self->area.width));
    *dy = MIN(*dy, (a->y + a->height) - (y + self->area.height));
    /* if in the top half then check this stuff last, will keep the top
       edge of the menu visible */
    if (pos <= half) {
        *dx = MAX(*dx, a->x - x);
        *dy = MAX(*dy, a->y - y);
    }
}

static void menu_entry_frame_render(ObMenuEntryFrame *self)
{
    RrAppearance *item_a, *text_a;
    gint th; /* temp */
    ObMenu *sub;
    ObMenuFrame *frame = self->frame;

    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        item_a = (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
                  !self->entry->data.normal.enabled ?
                  /* disabled */
                  (self == self->frame->selected ?
                   ob_rr_theme->a_menu_disabled_selected :
                   ob_rr_theme->a_menu_disabled) :
                  /* enabled */
                  (self == self->frame->selected ?
                   ob_rr_theme->a_menu_selected :
                   ob_rr_theme->a_menu_normal));
        th = ITEM_HEIGHT;
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        if (self->entry->data.separator.label) {
            item_a = ob_rr_theme->a_menu_title;
            th = ob_rr_theme->menu_title_height;
        } else {
            item_a = ob_rr_theme->a_menu_normal;
            th = ob_rr_theme->menu_sep_width +
                2*ob_rr_theme->menu_sep_paddingy;
        }
        break;
    default:
        g_assert_not_reached();
    }

    RECT_SET_SIZE(self->area, self->frame->inner_w, th);
    XResizeWindow(obt_display, self->window,
                  self->area.width, self->area.height);
    item_a->surface.parent = self->frame->a_items;
    item_a->surface.parentx = self->area.x;
    item_a->surface.parenty = self->area.y;
    RrPaint(item_a, self->window, self->area.width, self->area.height);

    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
        text_a = (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
                  !self->entry->data.normal.enabled ?
                  /* disabled */
                  (self == self->frame->selected ?
                   ob_rr_theme->a_menu_text_disabled_selected :
                   ob_rr_theme->a_menu_text_disabled) :
                  /* enabled */
                  (self == self->frame->selected ?
                   ob_rr_theme->a_menu_text_selected :
                   ob_rr_theme->a_menu_text_normal));
        text_a->texture[0].data.text.string = self->entry->data.normal.label;
        if (self->entry->data.normal.shortcut &&
            (self->frame->menu->show_all_shortcuts ||
             self->entry->data.normal.shortcut_always_show ||
             self->entry->data.normal.shortcut_position > 0))
        {
            text_a->texture[0].data.text.shortcut = TRUE;
            text_a->texture[0].data.text.shortcut_pos =
                self->entry->data.normal.shortcut_position;
        } else
            text_a->texture[0].data.text.shortcut = FALSE;
        break;
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        text_a = (self == self->frame->selected ?
                  ob_rr_theme->a_menu_text_selected :
                  ob_rr_theme->a_menu_text_normal);
        sub = self->entry->data.submenu.submenu;
        text_a->texture[0].data.text.string = sub ? sub->title : "";
        if (sub && sub->shortcut && (self->frame->menu->show_all_shortcuts ||
                              sub->shortcut_always_show ||
                              sub->shortcut_position > 0))
        {
            text_a->texture[0].data.text.shortcut = TRUE;
            text_a->texture[0].data.text.shortcut_pos = sub->shortcut_position;
        } else
            text_a->texture[0].data.text.shortcut = FALSE;
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        if (self->entry->data.separator.label != NULL) {
            text_a = ob_rr_theme->a_menu_text_title;
            text_a->texture[0].data.text.string =
                self->entry->data.separator.label;
        }
        else
            text_a = ob_rr_theme->a_menu_text_normal;
        break;
    default:
        g_assert_not_reached();
    }

    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
        XMoveResizeWindow(obt_display, self->text,
                          self->frame->text_x, PADDING,
                          self->frame->text_w,
                          ITEM_HEIGHT - 2*PADDING);
        text_a->surface.parent = item_a;
        text_a->surface.parentx = self->frame->text_x;
        text_a->surface.parenty = PADDING;
        RrPaint(text_a, self->text, self->frame->text_w,
                ITEM_HEIGHT - 2*PADDING);
        break;
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        XMoveResizeWindow(obt_display, self->text,
                          self->frame->text_x, PADDING,
                          self->frame->text_w - ITEM_HEIGHT,
                          ITEM_HEIGHT - 2*PADDING);
        text_a->surface.parent = item_a;
        text_a->surface.parentx = self->frame->text_x;
        text_a->surface.parenty = PADDING;
        RrPaint(text_a, self->text, self->frame->text_w - ITEM_HEIGHT,
                ITEM_HEIGHT - 2*PADDING);
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        if (self->entry->data.separator.label != NULL) {
            /* labeled separator */
            XMoveResizeWindow(obt_display, self->text,
                              ob_rr_theme->paddingx, ob_rr_theme->paddingy,
                              self->area.width - 2*ob_rr_theme->paddingx,
                              ob_rr_theme->menu_title_height -
                              2*ob_rr_theme->paddingy);
            text_a->surface.parent = item_a;
            text_a->surface.parentx = ob_rr_theme->paddingx;
            text_a->surface.parenty = ob_rr_theme->paddingy;
            RrPaint(text_a, self->text,
                    self->area.width - 2*ob_rr_theme->paddingx,
                    ob_rr_theme->menu_title_height -
                    2*ob_rr_theme->paddingy);
        } else {
            gint i;

            /* unlabeled separator */
            XMoveResizeWindow(obt_display, self->text, 0, 0,
                              self->area.width,
                              ob_rr_theme->menu_sep_width +
                              2*ob_rr_theme->menu_sep_paddingy);

            a_sep->surface.parent = item_a;
            a_sep->surface.parentx = 0;
            a_sep->surface.parenty = 0;
            for (i = 0; i < ob_rr_theme->menu_sep_width; ++i) {
                a_sep->texture[i].data.lineart.x1 =
                    ob_rr_theme->menu_sep_paddingx;
                a_sep->texture[i].data.lineart.y1 =
                    ob_rr_theme->menu_sep_paddingy + i;
                a_sep->texture[i].data.lineart.x2 =
                    self->area.width - ob_rr_theme->menu_sep_paddingx - 1;
                a_sep->texture[i].data.lineart.y2 =
                    ob_rr_theme->menu_sep_paddingy + i;
            }

            RrPaint(a_sep, self->text, self->area.width,
                    ob_rr_theme->menu_sep_width +
                    2*ob_rr_theme->menu_sep_paddingy);
        }
        break;
    default:
        g_assert_not_reached();
    }

    if (((self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL) ||
         (self->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)) &&
        self->entry->data.normal.icon)
    {
        RrAppearance *clear;

        XMoveResizeWindow(obt_display, self->icon,
                          PADDING, frame->item_margin.top,
                          ITEM_HEIGHT - frame->item_margin.top
                          - frame->item_margin.bottom,
                          ITEM_HEIGHT - frame->item_margin.top
                          - frame->item_margin.bottom);

        clear = ob_rr_theme->a_clear_tex;
        RrAppearanceClearTextures(clear);
        clear->texture[0].type = RR_TEXTURE_IMAGE;
        clear->texture[0].data.image.image =
            self->entry->data.normal.icon;
        clear->texture[0].data.image.alpha =
            self->entry->data.normal.icon_alpha;
        clear->surface.parent = item_a;
        clear->surface.parentx = PADDING;
        clear->surface.parenty = frame->item_margin.top;
        RrPaint(clear, self->icon,
                ITEM_HEIGHT - frame->item_margin.top
                - frame->item_margin.bottom,
                ITEM_HEIGHT - frame->item_margin.top
                - frame->item_margin.bottom);
        XMapWindow(obt_display, self->icon);
    } else if (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
               self->entry->data.normal.mask)
    {
        RrColor *c;
        RrAppearance *clear;

        XMoveResizeWindow(obt_display, self->icon,
                          PADDING, frame->item_margin.top,
                          ITEM_HEIGHT - frame->item_margin.top
                          - frame->item_margin.bottom,
                          ITEM_HEIGHT - frame->item_margin.top
                          - frame->item_margin.bottom);

        clear = ob_rr_theme->a_clear_tex;
        RrAppearanceClearTextures(clear);
        clear->texture[0].type = RR_TEXTURE_MASK;
        clear->texture[0].data.mask.mask =
            self->entry->data.normal.mask;

        c = (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
             !self->entry->data.normal.enabled ?
             /* disabled */
             (self == self->frame->selected ?
              self->entry->data.normal.mask_disabled_selected_color :
              self->entry->data.normal.mask_disabled_color) :
             /* enabled */
             (self == self->frame->selected ?
              self->entry->data.normal.mask_selected_color :
              self->entry->data.normal.mask_normal_color));
        clear->texture[0].data.mask.color = c;

        clear->surface.parent = item_a;
        clear->surface.parentx = PADDING;
        clear->surface.parenty = frame->item_margin.top;
        RrPaint(clear, self->icon,
                ITEM_HEIGHT - frame->item_margin.top
                - frame->item_margin.bottom,
                ITEM_HEIGHT - frame->item_margin.top
                - frame->item_margin.bottom);
        XMapWindow(obt_display, self->icon);
    } else
        XUnmapWindow(obt_display, self->icon);

    if (self->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
        RrAppearance *bullet_a;
        XMoveResizeWindow(obt_display, self->bullet,
                          self->frame->text_x + self->frame->text_w -
                          ITEM_HEIGHT + PADDING, PADDING,
                          ITEM_HEIGHT - 2*PADDING,
                          ITEM_HEIGHT - 2*PADDING);
        bullet_a = (self == self->frame->selected ?
                    ob_rr_theme->a_menu_bullet_selected :
                    ob_rr_theme->a_menu_bullet_normal);
        bullet_a->surface.parent = item_a;
        bullet_a->surface.parentx =
            self->frame->text_x + self->frame->text_w - ITEM_HEIGHT + PADDING;
        bullet_a->surface.parenty = PADDING;
        RrPaint(bullet_a, self->bullet,
                ITEM_HEIGHT - 2*PADDING,
                ITEM_HEIGHT - 2*PADDING);
        XMapWindow(obt_display, self->bullet);
    } else
        XUnmapWindow(obt_display, self->bullet);

    XFlush(obt_display);
}

/*! this code is taken from the menu_frame_render. if that changes, this won't
  work.. */
static gint menu_entry_frame_get_height(ObMenuEntryFrame *self,
                                        gboolean first_entry,
                                        gboolean last_entry)
{
    ObMenuEntryType t;
    gint h = 0;

    h += 2*PADDING;

    if (self)
        t = self->entry->type;
    else
        /* this is the More... entry, it's NORMAL type */
        t = OB_MENU_ENTRY_TYPE_NORMAL;

    switch (t) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        h += ob_rr_theme->menu_font_height;
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        if (self->entry->data.separator.label != NULL) {
            h += ob_rr_theme->menu_title_height +
                (ob_rr_theme->mbwidth - PADDING) * 2;

            /* if the first entry is a labeled separator, then make its border
               overlap with the menu's outside border */
            if (first_entry)
                h -= ob_rr_theme->mbwidth;
            /* if the last entry is a labeled separator, then make its border
               overlap with the menu's outside border */
            if (last_entry)
                h -= ob_rr_theme->mbwidth;
        } else {
            h += ob_rr_theme->menu_sep_width +
                2*ob_rr_theme->menu_sep_paddingy - PADDING * 2;
        }
        break;
    }

    return h;
}

void menu_frame_render(ObMenuFrame *self)
{
    gint w = 0, h = 0;
    gint tw, th; /* temps */
    GList *it;
    gboolean has_icon = FALSE;
    ObMenu *sub;
    ObMenuEntryFrame *e;

    /* find text dimensions */

    STRUT_SET(self->item_margin, 0, 0, 0, 0);

    if (self->entries) {
        gint l, t, r, b;

        e = self->entries->data;
        ob_rr_theme->a_menu_text_normal->texture[0].data.text.string = "";
        tw = RrMinWidth(ob_rr_theme->a_menu_text_normal);
        tw += 2*PADDING;

        th = ITEM_HEIGHT;

        RrMargins(ob_rr_theme->a_menu_normal, &l, &t, &r, &b);
        STRUT_SET(self->item_margin,
                  MAX(self->item_margin.left, l),
                  MAX(self->item_margin.top, t),
                  MAX(self->item_margin.right, r),
                  MAX(self->item_margin.bottom, b));
        RrMargins(ob_rr_theme->a_menu_selected, &l, &t, &r, &b);
        STRUT_SET(self->item_margin,
                  MAX(self->item_margin.left, l),
                  MAX(self->item_margin.top, t),
                  MAX(self->item_margin.right, r),
                  MAX(self->item_margin.bottom, b));
        RrMargins(ob_rr_theme->a_menu_disabled, &l, &t, &r, &b);
        STRUT_SET(self->item_margin,
                  MAX(self->item_margin.left, l),
                  MAX(self->item_margin.top, t),
                  MAX(self->item_margin.right, r),
                  MAX(self->item_margin.bottom, b));
        RrMargins(ob_rr_theme->a_menu_disabled_selected, &l, &t, &r, &b);
        STRUT_SET(self->item_margin,
                  MAX(self->item_margin.left, l),
                  MAX(self->item_margin.top, t),
                  MAX(self->item_margin.right, r),
                  MAX(self->item_margin.bottom, b));
    }

    /* render the entries */

    for (it = self->entries; it; it = g_list_next(it)) {
        RrAppearance *text_a;
        e = it->data;

        /* if the first entry is a labeled separator, then make its border
           overlap with the menu's outside border */
        if (it == self->entries &&
            e->entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR &&
            e->entry->data.separator.label)
        {
            h -= ob_rr_theme->mbwidth;
        }

        if (e->entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR &&
            e->entry->data.separator.label)
        {
            e->border = ob_rr_theme->mbwidth;
        }

        RECT_SET_POINT(e->area, 0, h+e->border);
        XMoveWindow(obt_display, e->window,
                    e->area.x-e->border, e->area.y-e->border);
        XSetWindowBorderWidth(obt_display, e->window, e->border);
        XSetWindowBorder(obt_display, e->window,
                         RrColorPixel(ob_rr_theme->menu_border_color));

        text_a = (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
                  !e->entry->data.normal.enabled ?
                  /* disabled */
                  (e == self->selected ?
                   ob_rr_theme->a_menu_text_disabled_selected :
                   ob_rr_theme->a_menu_text_disabled) :
                  /* enabled */
                  (e == self->selected ?
                   ob_rr_theme->a_menu_text_selected : 
                   ob_rr_theme->a_menu_text_normal));
        switch (e->entry->type) {
        case OB_MENU_ENTRY_TYPE_NORMAL:
            text_a->texture[0].data.text.string = e->entry->data.normal.label;
            tw = RrMinWidth(text_a);
            tw = MIN(tw, MAX_MENU_WIDTH);
            th = ob_rr_theme->menu_font_height;

            if (e->entry->data.normal.icon ||
                e->entry->data.normal.mask)
                has_icon = TRUE;
            break;
        case OB_MENU_ENTRY_TYPE_SUBMENU:
            sub = e->entry->data.submenu.submenu;
            text_a->texture[0].data.text.string = sub ? sub->title : "";
            tw = RrMinWidth(text_a);
            tw = MIN(tw, MAX_MENU_WIDTH);
            th = ob_rr_theme->menu_font_height;

            if (e->entry->data.normal.icon ||
                e->entry->data.normal.mask)
                has_icon = TRUE;

            tw += ITEM_HEIGHT - PADDING;
            break;
        case OB_MENU_ENTRY_TYPE_SEPARATOR:
            if (e->entry->data.separator.label != NULL) {
                ob_rr_theme->a_menu_text_title->texture[0].data.text.string =
                    e->entry->data.separator.label;
                tw = RrMinWidth(ob_rr_theme->a_menu_text_title) +
                    2*ob_rr_theme->paddingx;
                tw = MIN(tw, MAX_MENU_WIDTH);
                th = ob_rr_theme->menu_title_height +
                    (ob_rr_theme->mbwidth - PADDING) *2;
            } else {
                tw = 0;
                th = ob_rr_theme->menu_sep_width +
                    2*ob_rr_theme->menu_sep_paddingy - 2*PADDING;
            }
            break;
        default:
            g_assert_not_reached();
        }
        tw += 2*PADDING;
        th += 2*PADDING;
        w = MAX(w, tw);
        h += th;
    }

    /* if the last entry is a labeled separator, then make its border
       overlap with the menu's outside border */
    it = g_list_last(self->entries);
    e = it ? it->data : NULL;
    if (e && e->entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR &&
        e->entry->data.separator.label)
    {
        h -= ob_rr_theme->mbwidth;
    }

    self->text_x = PADDING;
    self->text_w = w;

    if (self->entries) {
        if (has_icon) {
            w += ITEM_HEIGHT + PADDING;
            self->text_x += ITEM_HEIGHT + PADDING;
        }
    }

    if (!w) w = 10;
    if (!h) h = 3;

    XResizeWindow(obt_display, self->window, w, h);

    self->inner_w = w;

    RrPaint(self->a_items, self->window, w, h);

    for (it = self->entries; it; it = g_list_next(it))
        menu_entry_frame_render(it->data);

    w += ob_rr_theme->mbwidth * 2;
    h += ob_rr_theme->mbwidth * 2;

    RECT_SET_SIZE(self->area, w, h);

    XFlush(obt_display);
}

static void menu_frame_update(ObMenuFrame *self)
{
    GList *mit, *fit;
    const Rect *a;
    gint h;

    menu_pipe_execute(self->menu);
    menu_find_submenus(self->menu);

    self->selected = NULL;

    /* start at show_from */
    mit = g_list_nth(self->menu->entries, self->show_from);

    /* go through the menu's and frame's entries and connect the frame entries
       to the menu entries */
    for (fit = self->entries; mit && fit;
         mit = g_list_next(mit), fit = g_list_next(fit))
    {
        ObMenuEntryFrame *f = fit->data;
        f->entry = mit->data;
    }

    /* if there are more menu entries than in the frame, add them */
    while (mit) {
        ObMenuEntryFrame *e = menu_entry_frame_new(mit->data, self);
        self->entries = g_list_append(self->entries, e);
        mit = g_list_next(mit);
    }

    /* if there are more frame entries than menu entries then get rid of
       them */
    while (fit) {
        GList *n = g_list_next(fit);
        menu_entry_frame_free(fit->data);
        self->entries = g_list_delete_link(self->entries, fit);
        fit = n;
    }

    /* * make the menu fit on the screen */

    /* calculate the height of the menu */
    h = 0;
    for (fit = self->entries; fit; fit = g_list_next(fit))
        h += menu_entry_frame_get_height(fit->data,
                                         fit == self->entries,
                                         g_list_next(fit) == NULL);
    /* add the border at the top and bottom */
    h += ob_rr_theme->mbwidth * 2;

    a = screen_physical_area_monitor(self->monitor);

    if (h > a->height) {
        GList *flast, *tmp;
        gboolean last_entry = TRUE;

        /* take the height of our More... entry into account */
        h += menu_entry_frame_get_height(NULL, FALSE, TRUE);

        /* start at the end of the entries */
        flast = g_list_last(self->entries);

        /* pull out all the entries from the frame that don't
           fit on the screen, leaving at least 1 though */
        while (h > a->height && g_list_previous(flast) != NULL) {
            /* update the height, without this entry */
            h -= menu_entry_frame_get_height(flast->data, FALSE, last_entry);

            /* destroy the entry we're not displaying */
            tmp = flast;
            flast = g_list_previous(flast);
            menu_entry_frame_free(tmp->data);
            self->entries = g_list_delete_link(self->entries, tmp);

            /* only the first one that we see is the last entry in the menu */
            last_entry = FALSE;
        };

        {
            ObMenuEntry *more_entry;
            ObMenuEntryFrame *more_frame;
            /* make the More... menu entry frame which will display in this
               frame.
               if self->menu->more_menu is NULL that means that this is already
               More... menu, so just use ourself.
            */
            more_entry = menu_get_more((self->menu->more_menu ?
                                        self->menu->more_menu :
                                        self->menu),
                                       /* continue where we left off */
                                       self->show_from +
                                       g_list_length(self->entries));
            more_frame = menu_entry_frame_new(more_entry, self);
            /* make it get deleted when the menu frame goes away */
            menu_entry_unref(more_entry);

            /* add our More... entry to the frame */
            self->entries = g_list_append(self->entries, more_frame);
        }
    }

    menu_frame_render(self);
}

static gboolean menu_frame_is_visible(ObMenuFrame *self)
{
    return !!(g_list_find(menu_frame_visible, self));
}

static gboolean menu_frame_show(ObMenuFrame *self)
{
    GList *it;

    /* determine if the underlying menu is already visible */
    for (it = menu_frame_visible; it; it = g_list_next(it)) {
        ObMenuFrame *f = it->data;
        if (f->menu == self->menu)
            break;
    }
    if (!it) {
        if (self->menu->update_func)
            if (!self->menu->update_func(self, self->menu->data))
                return FALSE;
    }

    if (menu_frame_visible == NULL) {
        /* no menus shown yet */

        /* grab the pointer in such a way as to pass through "owner events"
           so that we can get enter/leave notifies in the menu. */
        if (!grab_pointer(TRUE, FALSE, OB_CURSOR_POINTER))
            return FALSE;
        if (!grab_keyboard()) {
            ungrab_pointer();
            return FALSE;
        }
    }

    menu_frame_update(self);

    menu_frame_visible = g_list_prepend(menu_frame_visible, self);

    if (self->menu->show_func)
        self->menu->show_func(self, self->menu->data);

    return TRUE;
}

gboolean menu_frame_show_topmenu(ObMenuFrame *self, const GravityPoint *pos,
                                 gint monitor, gboolean mouse,
                                 gboolean user_positioned)
{
    gint px, py;
    gint x, y;

    if (menu_frame_is_visible(self))
        return TRUE;
    if (!menu_frame_show(self))
        return FALSE;

    if (self->menu->place_func) {
        x = pos->x.pos;
        y = pos->y.pos;
        self->menu->place_func(self, &x, &y, mouse, self->menu->data);
    } else {
        menu_frame_place_topmenu(self, pos, &x, &y, monitor,
                                 user_positioned);
    }

    menu_frame_move(self, x, y);

    XMapWindow(obt_display, self->window);

    if (screen_pointer_pos(&px, &py)) {
        ObMenuEntryFrame *e = menu_entry_frame_under(px, py);
        if (e && e->frame == self)
            e->ignore_enters++;
    }

    return TRUE;
}

/*! Stop hiding an open submenu.
    @child The OnMenuFrame of the submenu to be hidden
*/
static void remove_submenu_hide_timeout(ObMenuFrame *child)
{
    if (submenu_hide_timer) g_source_remove(submenu_hide_timer);
}

gboolean menu_frame_show_submenu(ObMenuFrame *self, ObMenuFrame *parent,
                                 ObMenuEntryFrame *parent_entry)
{
    gint x, y, dx, dy;
    gint px, py;

    if (menu_frame_is_visible(self))
        return TRUE;

    self->monitor = parent->monitor;
    self->parent = parent;
    self->parent_entry = parent_entry;

    /* set up parent's child to be us */
    if ((parent->child) != self) {
        if (parent->child)
            menu_frame_hide(parent->child);
        parent->child = self;
        parent->child_entry = parent_entry;
    }

    if (!menu_frame_show(self)) {
        parent->child = NULL;
        parent->child_entry = NULL;
        return FALSE;
    }

    menu_frame_place_submenu(self, &x, &y);
    menu_frame_move_on_screen(self, x, y, &dx, &dy);

    if (dx != 0) {
        /*try the other side */
        self->direction_right = !self->direction_right;
        menu_frame_place_submenu(self, &x, &y);
        menu_frame_move_on_screen(self, x, y, &dx, &dy);
    }
    menu_frame_move(self, x + dx, y + dy);

    XMapWindow(obt_display, self->window);

    if (screen_pointer_pos(&px, &py)) {
        ObMenuEntryFrame *e = menu_entry_frame_under(px, py);
        if (e && e->frame == self)
            e->ignore_enters++;
    }

    return TRUE;
}

static void menu_frame_hide(ObMenuFrame *self)
{
    ObMenu *const menu = self->menu;
    GList *it = g_list_find(menu_frame_visible, self);
    gulong ignore_start;

    if (!it)
        return;

    if (menu->hide_func)
        menu->hide_func(self, menu->data);

    if (self->child)
        menu_frame_hide(self->child);

    if (self->parent) {
        remove_submenu_hide_timeout(self);

        self->parent->child = NULL;
        self->parent->child_entry = NULL;
    }
    self->parent = NULL;
    self->parent_entry = NULL;

    menu_frame_visible = g_list_delete_link(menu_frame_visible, it);

    if (menu_frame_visible == NULL) {
        /* last menu shown */
        ungrab_pointer();
        ungrab_keyboard();
    }

    ignore_start = event_start_ignore_all_enters();
    XUnmapWindow(obt_display, self->window);
    event_end_ignore_all_enters(ignore_start);

    menu_frame_free(self);

    if (menu->cleanup_func)
        menu->cleanup_func(menu, menu->data);
}

void menu_frame_hide_all(void)
{
    GList *it;

    if (config_submenu_show_delay && submenu_show_timer)
        /* remove any submenu open requests */
        g_source_remove(submenu_show_timer);
    if ((it = g_list_last(menu_frame_visible)))
        menu_frame_hide(it->data);
}

ObMenuFrame* menu_frame_under(gint x, gint y)
{
    ObMenuFrame *ret = NULL;
    GList *it;

    for (it = menu_frame_visible; it; it = g_list_next(it)) {
        ObMenuFrame *f = it->data;

        if (RECT_CONTAINS(f->area, x, y)) {
            ret = f;
            break;
        }
    }
    return ret;
}

ObMenuEntryFrame* menu_entry_frame_under(gint x, gint y)
{
    ObMenuFrame *frame;
    ObMenuEntryFrame *ret = NULL;
    GList *it;

    if ((frame = menu_frame_under(x, y))) {
        x -= ob_rr_theme->mbwidth + frame->area.x;
        y -= ob_rr_theme->mbwidth + frame->area.y;

        for (it = frame->entries; it; it = g_list_next(it)) {
            ObMenuEntryFrame *e = it->data;

            if (RECT_CONTAINS(e->area, x, y)) {
                ret = e;
                break;
            }
        }
    }
    return ret;
}

static gboolean submenu_show_timeout(gpointer data)
{
    g_assert(menu_frame_visible);
    menu_entry_frame_show_submenu((ObMenuEntryFrame*)data);
    return FALSE;
}

static void submenu_show_dest(gpointer data)
{
    submenu_show_timer = 0;
}

static gboolean submenu_hide_timeout(gpointer data)
{
    g_assert(menu_frame_visible);
    menu_frame_hide((ObMenuFrame*)data);
    return FALSE;
}

static void submenu_hide_dest(gpointer data)
{
    submenu_hide_timer = 0;
}

void menu_frame_select(ObMenuFrame *self, ObMenuEntryFrame *entry,
                       gboolean immediate)
{
    ObMenuEntryFrame *old = self->selected;
    ObMenuFrame *oldchild = self->child;
    ObMenuEntryFrame *oldchild_entry = self->child_entry;

    /* if the user selected a separator, ignore it and reselect what we had
       selected before */
    if (entry && entry->entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR)
        entry = old;

    if (old == entry &&
        (!old || old->entry->type != OB_MENU_ENTRY_TYPE_SUBMENU))
        return;

    /* if the user left this menu but we have a submenu open, move the
       selection back to that submenu */
    if (!entry && oldchild_entry)
        entry = oldchild_entry;

    if (config_submenu_show_delay && submenu_show_timer)
        /* remove any submenu open requests */
        g_source_remove(submenu_show_timer);

    self->selected = entry;

    if (old)
        menu_entry_frame_render(old);

    if (oldchild_entry) {
        /* There is an open submenu */
        if (oldchild_entry == self->selected) {
            /* The open submenu has been reselected, so stop hiding the
               submenu */
            remove_submenu_hide_timeout(oldchild);
        }
        else if (oldchild_entry == old) {
            /* The open submenu was selected and is no longer, so hide the
               submenu */
            if (immediate || config_submenu_hide_delay == 0)
                menu_frame_hide(oldchild);
            else if (config_submenu_hide_delay > 0) {
                if (submenu_hide_timer) g_source_remove(submenu_hide_timer);
                submenu_hide_timer =
                    g_timeout_add_full(G_PRIORITY_DEFAULT,
                                       config_submenu_hide_delay,
                                       submenu_hide_timeout, oldchild, submenu_hide_dest);
            }
        }
    }

    if (self->selected) {
        menu_entry_frame_render(self->selected);

        if (self->selected->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
            /* only show if the submenu isn't already showing */
            if (oldchild_entry != self->selected) {
                if (immediate || config_submenu_show_delay == 0)
                    menu_entry_frame_show_submenu(self->selected);
                else if (config_submenu_show_delay > 0) {
                    if (submenu_show_timer)
                        g_source_remove(submenu_show_timer);
                    submenu_show_timer =
                        g_timeout_add_full(G_PRIORITY_DEFAULT,
                                           config_submenu_show_delay,
                                           submenu_show_timeout,
                                           self->selected, submenu_show_dest);
                }
            }
            /* hide the grandchildren of this menu. and move the cursor to
               the current menu */
            else if (immediate && self->child && self->child->child) {
                menu_frame_hide(self->child->child);
                menu_frame_select(self->child, NULL, TRUE);
            }
        }
    }
}

void menu_entry_frame_show_submenu(ObMenuEntryFrame *self)
{
    ObMenuFrame *f;

    if (!self->entry->data.submenu.submenu) return;

    f = menu_frame_new(self->entry->data.submenu.submenu,
                       self->entry->data.submenu.show_from,
                       self->frame->client);
    /* pass our direction on to our child */
    f->direction_right = self->frame->direction_right;

    if (!menu_frame_show_submenu(f, self->frame, self))
        menu_frame_free(f);
}

void menu_entry_frame_execute(ObMenuEntryFrame *self, guint state)
{
    if (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
        self->entry->data.normal.enabled)
    {
        /* grab all this shizzle, cuz when the menu gets hidden, 'self'
           gets freed */
        ObMenuEntry *entry = self->entry;
        ObMenuExecuteFunc func = self->frame->menu->execute_func;
        gpointer data = self->frame->menu->data;
        GSList *acts = self->entry->data.normal.actions;
        ObClient *client = self->frame->client;
        ObMenuFrame *frame = self->frame;
        guint mods = obt_keyboard_only_modmasks(state);

        /* release grabs before executing the shit */
        if (!(mods & ControlMask)) {
            event_cancel_all_key_grabs();
            frame = NULL;
        }

        if (func)
            func(entry, frame, client, state, data);
        else
            actions_run_acts(acts, OB_USER_ACTION_MENU_SELECTION,
                             state, -1, -1, 0, OB_FRAME_CONTEXT_NONE, client);
    }
}

void menu_frame_select_previous(ObMenuFrame *self)
{
    GList *it = NULL, *start;

    if (self->entries) {
        start = it = g_list_find(self->entries, self->selected);
        while (TRUE) {
            ObMenuEntryFrame *e;

            it = it ? g_list_previous(it) : g_list_last(self->entries);
            if (it == start)
                break;

            if (it) {
                e = it->data;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
                    break;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL)
                    break;
            }
        }
    }
    menu_frame_select(self, it ? it->data : NULL, FALSE);
}

void menu_frame_select_next(ObMenuFrame *self)
{
    GList *it = NULL, *start;

    if (self->entries) {
        start = it = g_list_find(self->entries, self->selected);
        while (TRUE) {
            ObMenuEntryFrame *e;

            it = it ? g_list_next(it) : self->entries;
            if (it == start)
                break;

            if (it) {
                e = it->data;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
                    break;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL)
                    break;
            }
        }
    }
    menu_frame_select(self, it ? it->data : NULL, FALSE);
}

void menu_frame_select_first(ObMenuFrame *self)
{
    GList *it = NULL;

    if (self->entries) {
        for (it = self->entries; it; it = g_list_next(it)) {
            ObMenuEntryFrame *e = it->data;
            if (e->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
                break;
            if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL)
                break;
        }
    }
    menu_frame_select(self, it ? it->data : NULL, FALSE);
}

void menu_frame_select_last(ObMenuFrame *self)
{
    GList *it = NULL;

    if (self->entries) {
        for (it = g_list_last(self->entries); it; it = g_list_previous(it)) {
            ObMenuEntryFrame *e = it->data;
            if (e->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
                break;
            if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL)
                break;
        }
    }
    menu_frame_select(self, it ? it->data : NULL, FALSE);
}
