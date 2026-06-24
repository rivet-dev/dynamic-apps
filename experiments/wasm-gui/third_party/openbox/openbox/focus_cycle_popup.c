/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus_cycle_popup.c for the Openbox window manager
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

#include "focus_cycle_popup.h"
#include "focus_cycle.h"
#include "popup.h"
#include "client.h"
#include "screen.h"
#include "focus.h"
#include "openbox.h"
#include "config.h"
#include "window.h"
#include "event.h"
#include "obrender/render.h"

#include <X11/Xlib.h>
#include <glib.h>

/* Size of the icons, which can appear inside or outside of a hilite box */
#define ICON_SIZE (gint)config_theme_window_list_icon_size
/* Size of the hilite box around a window's icon */
#define HILITE_SIZE (ICON_SIZE + 2*HILITE_OFFSET)
/* Width of the outer ring around the hilite box */
#define HILITE_WIDTH 2
/* Space between the outer ring around the hilite box and the icon inside it */
#define HILITE_MARGIN 1
/* Total distance from the edge of the hilite box to the icon inside it */
#define HILITE_OFFSET (HILITE_WIDTH + HILITE_MARGIN)
/* Margin area around the outside of the dialog */
#define OUTSIDE_BORDER 3
/* Margin area around the text */
#define TEXT_BORDER 2
/* Scroll the list-mode list when the cursor gets within this many rows of the
   top or bottom */
#define SCROLL_MARGIN 4

typedef struct _ObFocusCyclePopup       ObFocusCyclePopup;
typedef struct _ObFocusCyclePopupTarget ObFocusCyclePopupTarget;

struct _ObFocusCyclePopupTarget
{
    ObClient *client;
    RrImage *icon;
    gchar *text;
    Window iconwin;
    /* This is used when the popup is in list mode */
    Window textwin;
};

struct _ObFocusCyclePopup
{
    ObWindow obwin;
    Window bg;

    /* This is used when the popup is in icon mode */
    Window icon_mode_text;

    Window list_mode_up;
    Window list_mode_down;

    GList *targets;
    gint n_targets;

    const ObFocusCyclePopupTarget *last_target;

    gint maxtextw;

    /* How are the list is scrolled, in scroll mode */
    gint scroll;

    RrAppearance *a_bg;
    RrAppearance *a_text;
    RrAppearance *a_hilite_text;
    RrAppearance *a_icon;
    RrAppearance *a_arrow;

    gboolean mapped;
    ObFocusCyclePopupMode mode;
};

/*! This popup shows all possible windows */
static ObFocusCyclePopup popup;
/*! This popup shows a single window */
static ObIconPopup *single_popup;

static gchar   *popup_get_name (ObClient *c);
static gboolean popup_setup    (ObFocusCyclePopup *p,
                                gboolean create_targets,
                                gboolean refresh_targets,
                                gboolean linear);
static void     popup_render   (ObFocusCyclePopup *p,
                                const ObClient *c);

static Window create_window(Window parent, guint bwidth, gulong mask,
                            XSetWindowAttributes *attr)
{
    return XCreateWindow(obt_display, parent, 0, 0, 1, 1, bwidth,
                         RrDepth(ob_rr_inst), InputOutput,
                         RrVisual(ob_rr_inst), mask, attr);
}

void focus_cycle_popup_startup(gboolean reconfig)
{
    XSetWindowAttributes attrib;
    RrPixel32 *p;

    single_popup = icon_popup_new();

    popup.obwin.type = OB_WINDOW_CLASS_INTERNAL;
    popup.a_bg = RrAppearanceCopy(ob_rr_theme->osd_bg);
    popup.a_hilite_text = RrAppearanceCopy(ob_rr_theme->osd_hilite_label);
    popup.a_text = RrAppearanceCopy(ob_rr_theme->osd_unhilite_label);
    popup.a_icon = RrAppearanceCopy(ob_rr_theme->a_clear);
    popup.a_arrow = RrAppearanceCopy(ob_rr_theme->a_clear_tex);

    popup.a_hilite_text->surface.parent = popup.a_bg;
    popup.a_text->surface.parent = popup.a_bg;
    popup.a_icon->surface.parent = popup.a_bg;

    popup.a_text->texture[0].data.text.justify = RR_JUSTIFY_LEFT;
    popup.a_hilite_text->texture[0].data.text.justify = RR_JUSTIFY_LEFT;

    /* 2 textures. texture[0] is the icon.  texture[1] is the hilight, and
       may or may not be used */
    RrAppearanceAddTextures(popup.a_icon, 2);

    RrAppearanceClearTextures(popup.a_icon);
    popup.a_icon->texture[0].type = RR_TEXTURE_IMAGE;

    RrAppearanceClearTextures(popup.a_arrow);
    popup.a_arrow->texture[0].type = RR_TEXTURE_MASK;
    popup.a_arrow->texture[0].data.mask.color =
        ob_rr_theme->osd_text_active_color;

    attrib.override_redirect = True;
    attrib.border_pixel=RrColorPixel(ob_rr_theme->osd_border_color);
    popup.bg = create_window(obt_root(ob_screen), ob_rr_theme->obwidth,
                             CWOverrideRedirect | CWBorderPixel, &attrib);

    /* create the text window used for the icon-mode popup */
    popup.icon_mode_text = create_window(popup.bg, 0, 0, NULL);

    /* create the windows for the up and down arrows */
    popup.list_mode_up = create_window(popup.bg, 0, 0, NULL);
    popup.list_mode_down = create_window(popup.bg, 0, 0, NULL);

    popup.targets = NULL;
    popup.n_targets = 0;
    popup.last_target = NULL;

    /* set up the hilite texture for the icon */
    popup.a_icon->texture[1].data.rgba.width = HILITE_SIZE;
    popup.a_icon->texture[1].data.rgba.height = HILITE_SIZE;
    popup.a_icon->texture[1].data.rgba.alpha = 0xff;
    p = g_new(RrPixel32, HILITE_SIZE * HILITE_SIZE);
    popup.a_icon->texture[1].data.rgba.data = p;

    /* create the hilite under the target icon */
    {
        RrPixel32 color;
        RrColor *tc;
        gint x, y, o;

        tc = ob_rr_theme->osd_text_active_color;
        color = ((tc->r & 0xff) << RrDefaultRedOffset) +
            ((tc->g & 0xff) << RrDefaultGreenOffset) +
            ((tc->b & 0xff) << RrDefaultBlueOffset);

        o = 0;
        for (x = 0; x < HILITE_SIZE; x++)
            for (y = 0; y < HILITE_SIZE; y++) {
                guchar a;

                if (x < HILITE_WIDTH ||
                    x >= HILITE_SIZE - HILITE_WIDTH ||
                    y < HILITE_WIDTH ||
                    y >= HILITE_SIZE - HILITE_WIDTH)
                {
                    /* the border of the target */
                    a = 0x88;
                } else {
                    /* the background of the target */
                    a = 0x22;
                }

                p[o++] = color + (a << RrDefaultAlphaOffset);
            }
    }

    stacking_add(INTERNAL_AS_WINDOW(&popup));
    window_add(&popup.bg, INTERNAL_AS_WINDOW(&popup));
}

void focus_cycle_popup_shutdown(gboolean reconfig)
{
    icon_popup_free(single_popup);

    window_remove(popup.bg);
    stacking_remove(INTERNAL_AS_WINDOW(&popup));

    while(popup.targets) {
        ObFocusCyclePopupTarget *t = popup.targets->data;

        RrImageUnref(t->icon);
        g_free(t->text);
        XDestroyWindow(obt_display, t->iconwin);
        XDestroyWindow(obt_display, t->textwin);
        g_slice_free(ObFocusCyclePopupTarget, t);

        popup.targets = g_list_delete_link(popup.targets, popup.targets);
    }

    g_free(popup.a_icon->texture[1].data.rgba.data);
    popup.a_icon->texture[1].data.rgba.data = NULL;

    XDestroyWindow(obt_display, popup.list_mode_up);
    XDestroyWindow(obt_display, popup.list_mode_down);
    XDestroyWindow(obt_display, popup.icon_mode_text);
    XDestroyWindow(obt_display, popup.bg);

    RrAppearanceFree(popup.a_arrow);
    RrAppearanceFree(popup.a_icon);
    RrAppearanceFree(popup.a_hilite_text);
    RrAppearanceFree(popup.a_text);
    RrAppearanceFree(popup.a_bg);
}

static void popup_target_free(ObFocusCyclePopupTarget *t)
{
    RrImageUnref(t->icon);
    g_free(t->text);
    XDestroyWindow(obt_display, t->iconwin);
    XDestroyWindow(obt_display, t->textwin);
    g_slice_free(ObFocusCyclePopupTarget, t);
}

static gboolean popup_setup(ObFocusCyclePopup *p, gboolean create_targets,
                            gboolean refresh_targets, gboolean linear)
{
    gint maxwidth, n;
    GList *it;
    GList *rtargets; /* old targets for refresh */
    GList *rtlast;
    gboolean change;

    if (refresh_targets) {
        rtargets = p->targets;
        rtlast = g_list_last(rtargets);
        p->targets = NULL;
        p->n_targets = 0;
        change = FALSE;
    }
    else {
        rtargets = rtlast = NULL;
        change = TRUE;
    }

    g_assert(p->targets == NULL);
    g_assert(p->n_targets == 0);

    /* make its width to be the width of all the possible titles */

    /* build a list of all the valid focus targets and measure their strings,
       and count them */
    maxwidth = 0;
    n = 0;
    for (it = g_list_last(linear ? client_list : focus_order);
         it;
         it = g_list_previous(it))
    {
        ObClient *ft = it->data;

        if (focus_cycle_valid(ft)) {
            GList *rit;

            /* reuse the target if possible during refresh */
            for (rit = rtlast; rit; rit = g_list_previous(rit)) {
                ObFocusCyclePopupTarget *t = rit->data;
                if (t->client == ft) {
                    if (rit == rtlast)
                        rtlast = g_list_previous(rit);
                    rtargets = g_list_remove_link(rtargets, rit);

                    p->targets = g_list_concat(rit, p->targets);
                    ++n;

                    if (rit != rtlast)
                        change = TRUE; /* order changed */
                    break;
                }
            }

            if (!rit) {
                gchar *text = popup_get_name(ft);

                /* measure */
                p->a_text->texture[0].data.text.string = text;
                maxwidth = MAX(maxwidth, RrMinWidth(p->a_text));

                if (!create_targets) {
                    g_free(text);
                } else {
                    ObFocusCyclePopupTarget *t =
                        g_slice_new(ObFocusCyclePopupTarget);

                    t->client = ft;
                    t->text = text;
                    t->icon = client_icon(t->client);
                    RrImageRef(t->icon); /* own the icon so it won't go away */
                    t->iconwin = create_window(p->bg, 0, 0, NULL);
                    t->textwin = create_window(p->bg, 0, 0, NULL);

                    p->targets = g_list_prepend(p->targets, t);
                    ++n;

                    change = TRUE; /* added a window */
                }
            }
        }
    }

    if (rtargets) {
        change = TRUE; /* removed a window */

        while (rtargets) {
            popup_target_free(rtargets->data);
            rtargets = g_list_delete_link(rtargets, rtargets);
        }
    }

    p->n_targets = n;
    if (refresh_targets)
        /* don't shrink when refreshing */
        p->maxtextw = MAX(p->maxtextw, maxwidth);
    else
        p->maxtextw = maxwidth;

    return change;
}

static void popup_cleanup(void)
{
    while(popup.targets) {
        popup_target_free(popup.targets->data);
        popup.targets = g_list_delete_link(popup.targets, popup.targets);
    }
    popup.n_targets = 0;
    popup.last_target = NULL;
}

static gchar *popup_get_name(ObClient *c)
{
    ObClient *p;
    gchar *title;
    const gchar *desk = NULL;
    gchar *ret;

    /* find our highest direct parent */
    p = client_search_top_direct_parent(c);

    if (c->desktop != DESKTOP_ALL && c->desktop != screen_desktop)
        desk = screen_desktop_names[c->desktop];

    title = c->iconic ? c->icon_title : c->title;

    /* use the transient's parent's title/icon if we don't have one */
    if (p != c && title[0] == '\0')
        title = p->iconic ? p->icon_title : p->title;

    if (desk)
        ret = g_strdup_printf("%s [%s]", title, desk);
    else
        ret = g_strdup(title);

    return ret;
}

static void popup_render(ObFocusCyclePopup *p, const ObClient *c)
{
    gint ml, mt, mr, mb;
    gint l, t, r, b;
    gint x, y, w, h;
    const Rect *screen_area = NULL;
    gint i;
    GList *it;
    const ObFocusCyclePopupTarget *newtarget;
    ObFocusCyclePopupMode mode = p->mode;
    gint icons_per_row;
    gint icon_rows;
    gint textw, texth;
    gint selected_pos;
    gint last_scroll;

    /* vars for icon mode */
    gint icon_mode_textx;
    gint icon_mode_texty;
    gint icons_center_x;

    /* vars for list mode */
    gint list_mode_icon_column_w = HILITE_SIZE + OUTSIDE_BORDER;
    gint up_arrow_x, down_arrow_x;
    gint up_arrow_y, down_arrow_y;
    gboolean showing_arrows = FALSE;

    g_assert(mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS ||
             mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST);

    screen_area = screen_physical_area_primary(FALSE);

    /* get the outside margins */
    RrMargins(p->a_bg, &ml, &mt, &mr, &mb);

    /* get our outside borders */
    l = ml + OUTSIDE_BORDER;
    r = mr + OUTSIDE_BORDER;
    t = mt + OUTSIDE_BORDER;
    b = mb + OUTSIDE_BORDER;

    /* get the width from the text and keep it within limits */
    w = l + r + p->maxtextw;
    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
        /* when in list mode, there are icons down the side */
        w += list_mode_icon_column_w;
    w = MIN(w, MAX(screen_area->width/3, POPUP_WIDTH)); /* max width */
    w = MAX(w, POPUP_WIDTH); /* min width */

    /* get the text height */
    texth = RrMinHeight(p->a_hilite_text);
    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
        texth = MAX(MAX(texth, RrMinHeight(p->a_text)), HILITE_SIZE);
    else
        texth += TEXT_BORDER * 2;

    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS) {
        /* how many icons will fit in that row? make the width fit that */
        w -= l + r;
        icons_per_row = (w + HILITE_SIZE - 1) / HILITE_SIZE;
        w = icons_per_row * HILITE_SIZE + l + r;

        /* how many rows do we need? */
        icon_rows = (p->n_targets-1) / icons_per_row + 1;
    } else {
        /* in list mode, there is one column of icons.. */
        icons_per_row = 1;
        /* maximum is 80% of the screen height */
        icon_rows = MIN(p->n_targets,
                        (4*screen_area->height/5) /* 80% of the screen */
                        /
                        MAX(HILITE_SIZE, texth)); /* height of each row */
        /* but make sure there is always one */
        icon_rows = MAX(icon_rows, 1);
    }

    /* get the text width */
    textw = w - l - r;
    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
        /* leave space on the side for the icons */
        textw -= list_mode_icon_column_w;

    if (!p->mapped)
        /* reset the scrolling when the dialog is first shown */
        p->scroll = 0;

    /* find the height of the dialog */
    h = t + b + (icon_rows * MAX(HILITE_SIZE, texth));
    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS)
        /* in icon mode the text sits below the icons, so make some space */
        h += OUTSIDE_BORDER + texth;

    /* find the focused target */
    newtarget = NULL;
    for (i = 0, it = p->targets; it; ++i, it = g_list_next(it)) {
        const ObFocusCyclePopupTarget *target = it->data;
        if (target->client == c) {
            /* save the target */
            newtarget = target;
            break;
        }
    }
    selected_pos = i;
    g_assert(newtarget != NULL);

    /* scroll the list if needed */
    last_scroll = p->scroll;
    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST) {
        const gint top = p->scroll + SCROLL_MARGIN;
        const gint bottom = p->scroll + icon_rows - SCROLL_MARGIN;
        const gint min_scroll = 0;
        const gint max_scroll = p->n_targets - icon_rows;

        if (top - selected_pos >= 0) {
            p->scroll -= top - selected_pos + 1;
            p->scroll = MAX(p->scroll, min_scroll);
        } else if (selected_pos - bottom >= 0) {
            p->scroll += selected_pos - bottom + 1;
            p->scroll = MIN(p->scroll, max_scroll);
        }
    }

    /* show the scroll arrows when appropriate */
    if (p->scroll && mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST) {
        XMapWindow(obt_display, p->list_mode_up);
        showing_arrows = TRUE;
    } else
        XUnmapWindow(obt_display, p->list_mode_up);

    if (p->scroll < p->n_targets - icon_rows &&
        mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
    {
        XMapWindow(obt_display, p->list_mode_down);
        showing_arrows = TRUE;
    } else
        XUnmapWindow(obt_display, p->list_mode_down);

    /* make space for the arrows */
    if (showing_arrows)
        h += ob_rr_theme->up_arrow_mask->height + OUTSIDE_BORDER
            + ob_rr_theme->down_arrow_mask->height + OUTSIDE_BORDER;

    /* center the icons if there is less than one row */
    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS && icon_rows == 1)
        icons_center_x = (w - p->n_targets * HILITE_SIZE) / 2;
    else
        icons_center_x = 0;

    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS) {
        /* get the position of the text */
        icon_mode_textx = l;
        icon_mode_texty = h - texth - b;
    }

    /* find the position for the popup (include the outer borders) */
    x = screen_area->x + (screen_area->width -
                          (w + ob_rr_theme->obwidth * 2)) / 2;
    y = screen_area->y + (screen_area->height -
                          (h + ob_rr_theme->obwidth * 2)) / 2;

    if (!p->mapped) {
        /* position the background but don't draw it */
        XMoveResizeWindow(obt_display, p->bg, x, y, w, h);

        if (mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS) {
            /* position the text */
            XMoveResizeWindow(obt_display, p->icon_mode_text,
                              icon_mode_textx, icon_mode_texty, textw, texth);
            XMapWindow(obt_display, popup.icon_mode_text);
        } else {
            XUnmapWindow(obt_display, popup.icon_mode_text);

            up_arrow_x = (w - ob_rr_theme->up_arrow_mask->width) / 2;
            up_arrow_y = t;

            down_arrow_x = (w - ob_rr_theme->down_arrow_mask->width) / 2;
            down_arrow_y = h - b - ob_rr_theme->down_arrow_mask->height;

            /* position the arrows */
            XMoveResizeWindow(obt_display, p->list_mode_up,
                              up_arrow_x, up_arrow_y,
                              ob_rr_theme->up_arrow_mask->width,
                              ob_rr_theme->up_arrow_mask->height);
            XMoveResizeWindow(obt_display, p->list_mode_down,
                              down_arrow_x, down_arrow_y,
                              ob_rr_theme->down_arrow_mask->width,
                              ob_rr_theme->down_arrow_mask->height);
        }
    }

    /* * * draw everything * * */

    /* draw the background */
    if (!p->mapped)
        RrPaint(p->a_bg, p->bg, w, h);

    /* draw the scroll arrows */
    if (!p->mapped && mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST) {
        p->a_arrow->texture[0].data.mask.mask =
            ob_rr_theme->up_arrow_mask;
        p->a_arrow->surface.parent = p->a_bg;
        p->a_arrow->surface.parentx = up_arrow_x;
        p->a_arrow->surface.parenty = up_arrow_y;
        RrPaint(p->a_arrow, p->list_mode_up, 
                ob_rr_theme->up_arrow_mask->width,
                ob_rr_theme->up_arrow_mask->height);

        p->a_arrow->texture[0].data.mask.mask =
            ob_rr_theme->down_arrow_mask;
        p->a_arrow->surface.parent = p->a_bg;
        p->a_arrow->surface.parentx = down_arrow_x;
        p->a_arrow->surface.parenty = down_arrow_y;
        RrPaint(p->a_arrow, p->list_mode_down, 
                ob_rr_theme->down_arrow_mask->width,
                ob_rr_theme->down_arrow_mask->height);
    }

    /* draw the icons and text */
    for (i = 0, it = p->targets; it; ++i, it = g_list_next(it)) {
        const ObFocusCyclePopupTarget *target = it->data;

        /* have to redraw the targetted icon and last targetted icon
         * to update the hilite */
        if (!p->mapped || newtarget == target || p->last_target == target ||
            last_scroll != p->scroll)
        {
            /* row and column start from 0 */
            const gint row = i / icons_per_row - p->scroll;
            const gint col = i % icons_per_row;
            gint iconx, icony;
            gint list_mode_textx, list_mode_texty;
            RrAppearance *text;

            /* find the coordinates for the icon */
            iconx = icons_center_x + l + (col * HILITE_SIZE);
            icony = t + (showing_arrows ? ob_rr_theme->up_arrow_mask->height
                                          + OUTSIDE_BORDER
                         : 0)
                + (row * MAX(texth, HILITE_SIZE))
                + MAX(texth - HILITE_SIZE, 0) / 2;

            /* find the dimensions of the text box */
            list_mode_textx = iconx + HILITE_SIZE + TEXT_BORDER;
            list_mode_texty = icony;

            /* position the icon */
            XMoveResizeWindow(obt_display, target->iconwin,
                              iconx, icony, HILITE_SIZE, HILITE_SIZE);

            /* position the text */
            if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
                XMoveResizeWindow(obt_display, target->textwin,
                                  list_mode_textx, list_mode_texty,
                                  textw, texth);

            /* show/hide the right windows */
            if (row >= 0 && row < icon_rows) {
                XMapWindow(obt_display, target->iconwin);
                if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
                    XMapWindow(obt_display, target->textwin);
                else
                    XUnmapWindow(obt_display, target->textwin);
            } else {
                XUnmapWindow(obt_display, target->textwin);
                if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST)
                    XUnmapWindow(obt_display, target->iconwin);
                else
                    XMapWindow(obt_display, target->iconwin);
            }

            /* get the icon from the client */
            p->a_icon->texture[0].data.image.twidth = ICON_SIZE;
            p->a_icon->texture[0].data.image.theight = ICON_SIZE;
            p->a_icon->texture[0].data.image.tx = HILITE_OFFSET;
            p->a_icon->texture[0].data.image.ty = HILITE_OFFSET;
            p->a_icon->texture[0].data.image.alpha =
                target->client->iconic ? OB_ICONIC_ALPHA : 0xff;
            p->a_icon->texture[0].data.image.image = target->icon;

            /* Draw the hilite? */
            p->a_icon->texture[1].type = (target == newtarget) ?
                RR_TEXTURE_RGBA : RR_TEXTURE_NONE;

            /* draw the icon */
            p->a_icon->surface.parentx = iconx;
            p->a_icon->surface.parenty = icony;
            RrPaint(p->a_icon, target->iconwin, HILITE_SIZE, HILITE_SIZE);

            /* draw the text */
            if (mode == OB_FOCUS_CYCLE_POPUP_MODE_LIST ||
                target == newtarget)
            {
                text = (target == newtarget) ? p->a_hilite_text : p->a_text;
                text->texture[0].data.text.string = target->text;
                text->surface.parentx =
                    mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS ?
                    icon_mode_textx : list_mode_textx;
                text->surface.parenty =
                    mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS ?
                    icon_mode_texty : list_mode_texty;
                RrPaint(text,
                        (mode == OB_FOCUS_CYCLE_POPUP_MODE_ICONS ?
                         p->icon_mode_text : target->textwin),
                        textw, texth);
            }
        }
    }

    p->last_target = newtarget;

    XFlush(obt_display);
}

void focus_cycle_popup_show(ObClient *c, ObFocusCyclePopupMode mode,
                            gboolean linear)
{
    g_assert(c != NULL);

    if (mode == OB_FOCUS_CYCLE_POPUP_MODE_NONE) {
        focus_cycle_popup_hide();
        return;
    }

    /* do this stuff only when the dialog is first showing */
    if (!popup.mapped) {
        popup_setup(&popup, TRUE, FALSE, linear);
        /* this is fixed once the dialog is shown */
        popup.mode = mode;
    }
    g_assert(popup.targets != NULL);

    popup_render(&popup, c);

    if (!popup.mapped) {
        /* show the dialog */
        XMapWindow(obt_display, popup.bg);
        XFlush(obt_display);
        popup.mapped = TRUE;
        screen_hide_desktop_popup();
    }
}

void focus_cycle_popup_hide(void)
{
    gulong ignore_start;

    ignore_start = event_start_ignore_all_enters();

    XUnmapWindow(obt_display, popup.bg);
    XFlush(obt_display);

    event_end_ignore_all_enters(ignore_start);

    popup.mapped = FALSE;

    popup_cleanup();
}

void focus_cycle_popup_single_show(struct _ObClient *c)
{
    gchar *text;

    g_assert(c != NULL);

    /* do this stuff only when the dialog is first showing */
    if (!single_popup->popup->mapped) {
        const Rect *a;

        /* position the popup */
        a = screen_physical_area_primary(FALSE);
        icon_popup_position(single_popup, CenterGravity,
                            a->x + a->width / 2, a->y + a->height / 2);
        icon_popup_height(single_popup, POPUP_HEIGHT);
        icon_popup_min_width(single_popup, POPUP_WIDTH);
        icon_popup_max_width(single_popup, MAX(a->width/3, POPUP_WIDTH));
        icon_popup_text_width(single_popup, popup.maxtextw);
    }

    text = popup_get_name(c);
    icon_popup_show(single_popup, text, client_icon(c));
    g_free(text);
    screen_hide_desktop_popup();
}

void focus_cycle_popup_single_hide(void)
{
    icon_popup_hide(single_popup);
}

gboolean focus_cycle_popup_is_showing(ObClient *c)
{
    if (popup.mapped) {
        GList *it;

        for (it = popup.targets; it; it = g_list_next(it)) {
            ObFocusCyclePopupTarget *t = it->data;
            if (t->client == c)
                return TRUE;
        }
    }
    return FALSE;
}

static ObClient* popup_revert(ObClient *target)
{
    GList *it, *itt;

    for (it = popup.targets; it; it = g_list_next(it)) {
        ObFocusCyclePopupTarget *t = it->data;
        if (t->client == target) {
            /* move to a previous window if possible */
            for (itt = it->prev; itt; itt = g_list_previous(itt)) {
                ObFocusCyclePopupTarget *t2 = itt->data;
                if (focus_cycle_valid(t2->client))
                    return t2->client;
            }

            /* otherwise move to a following window if possible */
            for (itt = it->next; itt; itt = g_list_next(itt)) {
                ObFocusCyclePopupTarget *t2 = itt->data;
                if (focus_cycle_valid(t2->client))
                    return t2->client;
            }

            /* otherwise, we can't go anywhere there is nowhere valid to go */
            return NULL;
        }
    }
    return NULL;
}

ObClient* focus_cycle_popup_refresh(ObClient *target,
                                    gboolean redraw,
                                    gboolean linear)
{
    if (!popup.mapped) return NULL;

    if (!focus_cycle_valid(target))
        target = popup_revert(target);

    redraw = popup_setup(&popup, TRUE, TRUE, linear) && redraw;

    if (!target && popup.targets)
        target = ((ObFocusCyclePopupTarget*)popup.targets->data)->client;

    if (target && redraw) {
        popup.mapped = FALSE;
        popup_render(&popup, target);
        XFlush(obt_display);
        popup.mapped = TRUE;
    }

    return target;
}
