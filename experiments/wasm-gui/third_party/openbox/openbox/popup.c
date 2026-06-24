/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   popup.c for the Openbox window manager
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

#include "popup.h"

#include "openbox.h"
#include "frame.h"
#include "client.h"
#include "stacking.h"
#include "event.h"
#include "screen.h"
#include "obrender/render.h"
#include "obrender/theme.h"

ObPopup *popup_new(void)
{
    XSetWindowAttributes attrib;
    ObPopup *self = g_slice_new0(ObPopup);

    self->obwin.type = OB_WINDOW_CLASS_INTERNAL;
    self->gravity = NorthWestGravity;
    self->x = self->y = self->textw = self->h = 0;
    self->a_bg = RrAppearanceCopy(ob_rr_theme->osd_bg);
    self->a_text = RrAppearanceCopy(ob_rr_theme->osd_hilite_label);
    self->iconwm = self->iconhm = 1;

    attrib.override_redirect = True;
    self->bg = XCreateWindow(obt_display, obt_root(ob_screen),
                             0, 0, 1, 1, 0, RrDepth(ob_rr_inst),
                             InputOutput, RrVisual(ob_rr_inst),
                             CWOverrideRedirect, &attrib);

    self->text = XCreateWindow(obt_display, self->bg,
                               0, 0, 1, 1, 0, RrDepth(ob_rr_inst),
                               InputOutput, RrVisual(ob_rr_inst), 0, NULL);

    XSetWindowBorderWidth(obt_display, self->bg, ob_rr_theme->obwidth);
    XSetWindowBorder(obt_display, self->bg,
                     RrColorPixel(ob_rr_theme->osd_border_color));

    XMapWindow(obt_display, self->text);

    stacking_add(INTERNAL_AS_WINDOW(self));
    window_add(&self->bg, INTERNAL_AS_WINDOW(self));
    return self;
}

void popup_free(ObPopup *self)
{
    if (self) {
        popup_hide(self); /* make sure it's not showing or is being delayed and
                             will be shown */
        XDestroyWindow(obt_display, self->bg);
        XDestroyWindow(obt_display, self->text);
        RrAppearanceFree(self->a_bg);
        RrAppearanceFree(self->a_text);
        window_remove(self->bg);
        stacking_remove(self);
        g_slice_free(ObPopup, self);
    }
}

void popup_position(ObPopup *self, gint gravity, gint x, gint y)
{
    self->gravity = gravity;
    self->x = x;
    self->y = y;
}

void popup_text_width(ObPopup *self, gint w)
{
    self->textw = w;
}

void popup_min_width(ObPopup *self, gint minw)
{
    self->minw = minw;
}

void popup_max_width(ObPopup *self, gint maxw)
{
    self->maxw = maxw;
}

void popup_height(ObPopup *self, gint h)
{
    gint texth;

    /* don't let the height be smaller than the text */
    texth = RrMinHeight(self->a_text) + ob_rr_theme->paddingy * 2;
    self->h = MAX(h, texth);
}

void popup_text_width_to_string(ObPopup *self, gchar *text)
{
    if (text[0] != '\0') {
        self->a_text->texture[0].data.text.string = text;
        self->textw = RrMinWidth(self->a_text);
    } else
        self->textw = 0;
}

void popup_height_to_string(ObPopup *self, gchar *text)
{
    self->h = RrMinHeight(self->a_text) + ob_rr_theme->paddingy * 2;
}

void popup_text_width_to_strings(ObPopup *self, gchar **strings, gint num)
{
    gint i, maxw;

    maxw = 0;
    for (i = 0; i < num; ++i) {
        popup_text_width_to_string(self, strings[i]);
        maxw = MAX(maxw, self->textw);
    }
    self->textw = maxw;
}

void popup_set_text_align(ObPopup *self, RrJustify align)
{
    self->a_text->texture[0].data.text.justify = align;
}

static gboolean popup_show_timeout(gpointer data)
{
    ObPopup *self = data;

    XMapWindow(obt_display, self->bg);
    stacking_raise(INTERNAL_AS_WINDOW(self));
    self->mapped = TRUE;
    self->delay_mapped = FALSE;
    self->delay_timer = 0;

    return FALSE; /* don't repeat */
}

void popup_delay_show(ObPopup *self, gulong msec, gchar *text)
{
    gint l, t, r, b;
    gint x, y, w, h;
    guint m;
    gint emptyx, emptyy; /* empty space between elements */
    gint textx, texty, textw, texth;
    gint iconx, icony, iconw, iconh;
    const Rect *area;
    Rect mon;
    gboolean hasicon = self->hasicon;

    /* when there is no icon and the text is not parent relative, then
       fill the whole dialog with the text appearance, don't use the bg at all
    */
    if (hasicon || self->a_text->surface.grad == RR_SURFACE_PARENTREL)
        RrMargins(self->a_bg, &l, &t, &r, &b);
    else
        l = t = r = b = 0;

    /* set up the textures */
    self->a_text->texture[0].data.text.string = text;

    /* measure the text out */
    if (text[0] != '\0') {
        RrMinSize(self->a_text, &textw, &texth);
    } else {
        textw = 0;
        texth = RrMinHeight(self->a_text);
    }

    /* get the height, which is also used for the icon width */
    emptyy = t + b + ob_rr_theme->paddingy * 2;
    if (self->h)
        texth = self->h - emptyy;
    h = texth * self->iconhm + emptyy;

    if (self->textw)
        textw = self->textw;

    iconx = textx = l + ob_rr_theme->paddingx;

    emptyx = l + r + ob_rr_theme->paddingx * 2;
    if (hasicon) {
        iconw = texth * self->iconwm;
        iconh = texth * self->iconhm;
        textx += iconw + ob_rr_theme->paddingx;
        if (textw)
            emptyx += ob_rr_theme->paddingx; /* between the icon and text */
        icony = (h - iconh - emptyy) / 2 + t + ob_rr_theme->paddingy;
    } else
        iconw = 0;

    texty = (h - texth - emptyy) / 2 + t + ob_rr_theme->paddingy;

    /* when there is no icon, then fill the whole dialog with the text
       appearance
    */
    if (!hasicon)
    {
        textx = texty = 0;
        texth += emptyy;
        textw += emptyx;
        emptyx = emptyy = 0;
    }

    w = textw + emptyx + iconw;
    /* cap it at maxw/minw */
    if (self->maxw) w = MIN(w, self->maxw);
    if (self->minw) w = MAX(w, self->minw);
    textw = w - emptyx - iconw;

    /* sanity checks to avoid crashes! */
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (texth < 1) texth = 1;

    /* set up the x coord */
    x = self->x;
    switch (self->gravity) {
    case NorthGravity: case CenterGravity: case SouthGravity:
        x -= w / 2;
        break;
    case NorthEastGravity: case EastGravity: case SouthEastGravity:
        x -= w;
        break;
    }

    /* set up the y coord */
    y = self->y;
    switch (self->gravity) {
    case WestGravity: case CenterGravity: case EastGravity:
        y -= h / 2;
        break;
    case SouthWestGravity: case SouthGravity: case SouthEastGravity:
        y -= h;
        break;
    }

    /* If the popup belongs to a client (eg, the moveresize popup), get
     * the monitor for that client, otherwise do other stuff */
    if (self->client) {
        m = client_monitor(self->client);
    } else {
        /* Find the monitor which contains the biggest part of the popup.
         * If the popup is completely off screen, limit it to the intersection
         * of all monitors and then try again. If it's still off screen, put it
         * on monitor 0. */
        RECT_SET(mon, x, y, w, h);
        m = screen_find_monitor(&mon);
    }
    area = screen_physical_area_monitor(m);

    x = MAX(MIN(x, area->x+area->width-w), area->x);
    y = MAX(MIN(y, area->y+area->height-h), area->y);

    if (m == screen_num_monitors) {
        RECT_SET(mon, x, y, w, h);
        m = screen_find_monitor(&mon);
        if (m == screen_num_monitors)
            m = 0;
        area = screen_physical_area_monitor(m);

        x = MAX(MIN(x, area->x+area->width-w), area->x);
        y = MAX(MIN(y, area->y+area->height-h), area->y);
    }

    /* set the windows/appearances up */
    XMoveResizeWindow(obt_display, self->bg, x, y, w, h);
    /* when there is no icon and the text is not parent relative, then
       fill the whole dialog with the text appearance, don't use the bg at all
    */
    if (hasicon || self->a_text->surface.grad == RR_SURFACE_PARENTREL)
        RrPaint(self->a_bg, self->bg, w, h);

    if (textw) {
        self->a_text->surface.parent = self->a_bg;
        self->a_text->surface.parentx = textx;
        self->a_text->surface.parenty = texty;
        XMoveResizeWindow(obt_display, self->text, textx, texty, textw, texth);
        RrPaint(self->a_text, self->text, textw, texth);
    }

    if (hasicon)
        self->draw_icon(iconx, icony, iconw, iconh, self->draw_icon_data);

    /* do the actual showing */
    if (!self->mapped) {
        if (msec) {
            /* don't kill previous show timers */
            if (!self->delay_mapped) {
                self->delay_timer =
                    g_timeout_add(msec, popup_show_timeout, self);
                self->delay_mapped = TRUE;
            }
        } else {
            popup_show_timeout(self);
        }
    }
}

void popup_hide(ObPopup *self)
{
    if (self->mapped) {
        gulong ignore_start;

        /* kill enter events cause by this unmapping */
        ignore_start = event_start_ignore_all_enters();

        XUnmapWindow(obt_display, self->bg);
        self->mapped = FALSE;

        event_end_ignore_all_enters(ignore_start);
    } else if (self->delay_mapped) {
        g_source_remove(self->delay_timer);
        self->delay_timer = 0;
        self->delay_mapped = FALSE;
    }
}

static void icon_popup_draw_icon(gint x, gint y, gint w, gint h, gpointer data)
{
    ObIconPopup *self = data;

    self->a_icon->surface.parent = self->popup->a_bg;
    self->a_icon->surface.parentx = x;
    self->a_icon->surface.parenty = y;
    XMoveResizeWindow(obt_display, self->icon, x, y, w, h);
    RrPaint(self->a_icon, self->icon, w, h);
}

ObIconPopup *icon_popup_new(void)
{
    ObIconPopup *self;

    self = g_slice_new0(ObIconPopup);
    self->popup = popup_new();
    self->a_icon = RrAppearanceCopy(ob_rr_theme->a_clear_tex);
    self->icon = XCreateWindow(obt_display, self->popup->bg,
                               0, 0, 1, 1, 0,
                               RrDepth(ob_rr_inst), InputOutput,
                               RrVisual(ob_rr_inst), 0, NULL);
    XMapWindow(obt_display, self->icon);

    self->popup->hasicon = TRUE;
    self->popup->draw_icon = icon_popup_draw_icon;
    self->popup->draw_icon_data = self;

    return self;
}

void icon_popup_free(ObIconPopup *self)
{
    if (self) {
        XDestroyWindow(obt_display, self->icon);
        RrAppearanceFree(self->a_icon);
        popup_free(self->popup);
        g_slice_free(ObIconPopup, self);
    }
}

void icon_popup_delay_show(ObIconPopup *self, gulong msec,
                           gchar *text, RrImage *icon)
{
    if (icon) {
        RrAppearanceClearTextures(self->a_icon);
        self->a_icon->texture[0].type = RR_TEXTURE_IMAGE;
        self->a_icon->texture[0].data.image.alpha = 0xff;
        self->a_icon->texture[0].data.image.image = icon;
    } else {
        RrAppearanceClearTextures(self->a_icon);
        self->a_icon->texture[0].type = RR_TEXTURE_NONE;
    }

    popup_delay_show(self->popup, msec, text);
}

void icon_popup_icon_size_multiplier(ObIconPopup *self, guint wm, guint hm)
{
    /* cap them at 1 */
    self->popup->iconwm = MAX(1, wm);
    self->popup->iconhm = MAX(1, hm);
}

static void pager_popup_draw_icon(gint px, gint py, gint w, gint h,
                                  gpointer data)
{
    ObPagerPopup *self = data;
    gint x, y;
    guint rown, n;
    guint horz_inc;
    guint vert_inc;
    guint r, c;
    gint eachw, eachh;
    const guint cols = screen_desktop_layout.columns;
    const guint rows = screen_desktop_layout.rows;
    const gint linewidth = ob_rr_theme->obwidth;

    eachw = (w - ((cols + 1) * linewidth)) / cols;
    eachh = (h - ((rows + 1) * linewidth)) / rows;
    /* make them squares */
    eachw = eachh = MIN(eachw, eachh);

    /* center */
    px += (w - (cols * (eachw + linewidth) + linewidth)) / 2;
    py += (h - (rows * (eachh + linewidth) + linewidth)) / 2;

    if (eachw <= 0 || eachh <= 0)
        return;

    switch (screen_desktop_layout.orientation) {
    case OB_ORIENTATION_HORZ:
        switch (screen_desktop_layout.start_corner) {
        case OB_CORNER_TOPLEFT:
            n = 0;
            horz_inc = 1;
            vert_inc = cols;
            break;
        case OB_CORNER_TOPRIGHT:
            n = cols - 1;
            horz_inc = -1;
            vert_inc = cols;
            break;
        case OB_CORNER_BOTTOMRIGHT:
            n = rows * cols - 1;
            horz_inc = -1;
            vert_inc = -screen_desktop_layout.columns;
            break;
        case OB_CORNER_BOTTOMLEFT:
            n = (rows - 1) * cols;
            horz_inc = 1;
            vert_inc = -cols;
            break;
        default:
            g_assert_not_reached();
        }
        break;
    case OB_ORIENTATION_VERT:
        switch (screen_desktop_layout.start_corner) {
        case OB_CORNER_TOPLEFT:
            n = 0;
            horz_inc = rows;
            vert_inc = 1;
            break;
        case OB_CORNER_TOPRIGHT:
            n = rows * (cols - 1);
            horz_inc = -rows;
            vert_inc = 1;
            break;
        case OB_CORNER_BOTTOMRIGHT:
            n = rows * cols - 1;
            horz_inc = -rows;
            vert_inc = -1;
            break;
        case OB_CORNER_BOTTOMLEFT:
            n = rows - 1;
            horz_inc = rows;
            vert_inc = -1;
            break;
        default:
            g_assert_not_reached();
        }
        break;
    default:
        g_assert_not_reached();
    }

    rown = n;
    for (r = 0, y = 0; r < rows; ++r, y += eachh + linewidth)
    {
        for (c = 0, x = 0; c < cols; ++c, x += eachw + linewidth)
        {
            RrAppearance *a;

            if (n < self->desks) {
                a = (n == self->curdesk ? self->hilight : self->unhilight);

                a->surface.parent = self->popup->a_bg;
                a->surface.parentx = x + px;
                a->surface.parenty = y + py;
                XMoveResizeWindow(obt_display, self->wins[n],
                                  x + px, y + py, eachw, eachh);
                RrPaint(a, self->wins[n], eachw, eachh);
            }
            n += horz_inc;
        }
        n = rown += vert_inc;
    }
}

ObPagerPopup *pager_popup_new(void)
{
    ObPagerPopup *self;

    self = g_slice_new(ObPagerPopup);
    self->popup = popup_new();

    self->desks = 0;
    self->wins = g_new(Window, self->desks);
    self->hilight = RrAppearanceCopy(ob_rr_theme->osd_hilite_bg);
    self->unhilight = RrAppearanceCopy(ob_rr_theme->osd_unhilite_bg);

    self->popup->hasicon = TRUE;
    self->popup->draw_icon = pager_popup_draw_icon;
    self->popup->draw_icon_data = self;

    return self;
}

void pager_popup_free(ObPagerPopup *self)
{
    if (self) {
        guint i;

        for (i = 0; i < self->desks; ++i)
            XDestroyWindow(obt_display, self->wins[i]);
        g_free(self->wins);
        RrAppearanceFree(self->hilight);
        RrAppearanceFree(self->unhilight);
        popup_free(self->popup);
        g_slice_free(ObPagerPopup, self);
    }
}

void pager_popup_delay_show(ObPagerPopup *self, gulong msec,
                            gchar *text, guint desk)
{
    guint i;

    if (screen_num_desktops < self->desks)
        for (i = screen_num_desktops; i < self->desks; ++i)
            XDestroyWindow(obt_display, self->wins[i]);

    if (screen_num_desktops != self->desks)
        self->wins = g_renew(Window, self->wins, screen_num_desktops);

    if (screen_num_desktops > self->desks)
        for (i = self->desks; i < screen_num_desktops; ++i) {
            XSetWindowAttributes attr;

            attr.border_pixel =
                RrColorPixel(ob_rr_theme->osd_border_color);
            self->wins[i] = XCreateWindow(obt_display, self->popup->bg,
                                          0, 0, 1, 1, ob_rr_theme->obwidth,
                                          RrDepth(ob_rr_inst), InputOutput,
                                          RrVisual(ob_rr_inst), CWBorderPixel,
                                          &attr);
            XMapWindow(obt_display, self->wins[i]);
        }

    self->desks = screen_num_desktops;
    self->curdesk = desk;

    popup_delay_show(self->popup, msec, text);
}

void pager_popup_icon_size_multiplier(ObPagerPopup *self, guint wm, guint hm)
{
    /* cap them at 1 */
    self->popup->iconwm = MAX(1, wm);
    self->popup->iconhm = MAX(1, hm);
}
