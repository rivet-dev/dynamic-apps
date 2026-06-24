/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   frame.c for the Openbox window manager
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

#include "frame.h"
#include "client.h"
#include "openbox.h"
#include "grab.h"
#include "debug.h"
#include "config.h"
#include "framerender.h"
#include "focus_cycle.h"
#include "focus_cycle_indicator.h"
#include "moveresize.h"
#include "screen.h"
#include "obrender/theme.h"
#include "obt/display.h"
#include "obt/xqueue.h"
#include "obt/prop.h"

#define FRAME_EVENTMASK (EnterWindowMask | LeaveWindowMask | \
                         ButtonPressMask | ButtonReleaseMask | \
                         SubstructureRedirectMask | FocusChangeMask)
#define ELEMENT_EVENTMASK (ButtonPressMask | ButtonReleaseMask | \
                           ButtonMotionMask | PointerMotionMask | \
                           EnterWindowMask | LeaveWindowMask)

#define FRAME_ANIMATE_ICONIFY_TIME 150000 /* .15 seconds */
#define FRAME_ANIMATE_ICONIFY_STEP_TIME (1000 / 60) /* 60 Hz */

#define FRAME_HANDLE_Y(f) (f->size.top + f->client->area.height + f->cbwidth_b)

static void flash_done(gpointer data);
static gboolean flash_timeout(gpointer data);

static void layout_title(ObFrame *self);
static void set_theme_statics(ObFrame *self);
static void free_theme_statics(ObFrame *self);
static gboolean frame_animate_iconify(gpointer self);
static void frame_adjust_cursors(ObFrame *self);

static Window createWindow(Window parent, Visual *visual,
                           gulong mask, XSetWindowAttributes *attrib)
{
    return XCreateWindow(obt_display, parent, 0, 0, 1, 1, 0,
                         (visual ? 32 : RrDepth(ob_rr_inst)), InputOutput,
                         (visual ? visual : RrVisual(ob_rr_inst)),
                         mask, attrib);

}

static Visual *check_32bit_client(ObClient *c)
{
    XWindowAttributes wattrib;
    Status ret;

    /* we're already running at 32 bit depth, yay. we don't need to use their
       visual */
    if (RrDepth(ob_rr_inst) == 32)
        return NULL;

    ret = XGetWindowAttributes(obt_display, c->window, &wattrib);
    g_assert(ret != BadDrawable);
    g_assert(ret != BadWindow);

    if (wattrib.depth == 32)
        return wattrib.visual;
    return NULL;
}

ObFrame *frame_new(ObClient *client)
{
    XSetWindowAttributes attrib;
    gulong mask;
    ObFrame *self;
    Visual *visual;

    self = g_slice_new0(ObFrame);
    self->client = client;

    visual = check_32bit_client(client);

    /* create the non-visible decor windows */

    mask = 0;
    if (visual) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        /* create a colormap with the visual */
        self->colormap = attrib.colormap =
            XCreateColormap(obt_display, obt_root(ob_screen),
                            visual, AllocNone);
        attrib.background_pixel = BlackPixel(obt_display, ob_screen);
        attrib.border_pixel = BlackPixel(obt_display, ob_screen);
    }
    self->window = createWindow(obt_root(ob_screen), visual,
                                mask, &attrib);

    /* create the visible decor windows */

    mask = 0;
    if (visual) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        attrib.colormap = RrColormap(ob_rr_inst);
    }

    self->backback = createWindow(self->window, NULL, mask, &attrib);
    self->backfront = createWindow(self->backback, NULL, mask, &attrib);

    mask |= CWEventMask;
    attrib.event_mask = ELEMENT_EVENTMASK;
    self->innerleft = createWindow(self->window, NULL, mask, &attrib);
    self->innertop = createWindow(self->window, NULL, mask, &attrib);
    self->innerright = createWindow(self->window, NULL, mask, &attrib);
    self->innerbottom = createWindow(self->window, NULL, mask, &attrib);

    self->innerblb = createWindow(self->innerbottom, NULL, mask, &attrib);
    self->innerbrb = createWindow(self->innerbottom, NULL, mask, &attrib);
    self->innerbll = createWindow(self->innerleft, NULL, mask, &attrib);
    self->innerbrr = createWindow(self->innerright, NULL, mask, &attrib);

    self->title = createWindow(self->window, NULL, mask, &attrib);
    self->titleleft = createWindow(self->window, NULL, mask, &attrib);
    self->titletop = createWindow(self->window, NULL, mask, &attrib);
    self->titletopleft = createWindow(self->window, NULL, mask, &attrib);
    self->titletopright = createWindow(self->window, NULL, mask, &attrib);
    self->titleright = createWindow(self->window, NULL, mask, &attrib);
    self->titlebottom = createWindow(self->window, NULL, mask, &attrib);

    self->topresize = createWindow(self->title, NULL, mask, &attrib);
    self->tltresize = createWindow(self->title, NULL, mask, &attrib);
    self->tllresize = createWindow(self->title, NULL, mask, &attrib);
    self->trtresize = createWindow(self->title, NULL, mask, &attrib);
    self->trrresize = createWindow(self->title, NULL, mask, &attrib);

    self->left = createWindow(self->window, NULL, mask, &attrib);
    self->right = createWindow(self->window, NULL, mask, &attrib);

    self->label = createWindow(self->title, NULL, mask, &attrib);
    self->max = createWindow(self->title, NULL, mask, &attrib);
    self->close = createWindow(self->title, NULL, mask, &attrib);
    self->desk = createWindow(self->title, NULL, mask, &attrib);
    self->shade = createWindow(self->title, NULL, mask, &attrib);
    self->icon = createWindow(self->title, NULL, mask, &attrib);
    self->iconify = createWindow(self->title, NULL, mask, &attrib);

    self->handle = createWindow(self->window, NULL, mask, &attrib);
    self->lgrip = createWindow(self->handle, NULL, mask, &attrib);
    self->rgrip = createWindow(self->handle, NULL, mask, &attrib);

    self->handleleft = createWindow(self->handle, NULL, mask, &attrib);
    self->handleright = createWindow(self->handle, NULL, mask, &attrib);

    self->handletop = createWindow(self->window, NULL, mask, &attrib);
    self->handlebottom = createWindow(self->window, NULL, mask, &attrib);
    self->lgripleft = createWindow(self->window, NULL, mask, &attrib);
    self->lgriptop = createWindow(self->window, NULL, mask, &attrib);
    self->lgripbottom = createWindow(self->window, NULL, mask, &attrib);
    self->rgripright = createWindow(self->window, NULL, mask, &attrib);
    self->rgriptop = createWindow(self->window, NULL, mask, &attrib);
    self->rgripbottom = createWindow(self->window, NULL, mask, &attrib);

    self->focused = FALSE;

    /* the other stuff is shown based on decor settings */
    XMapWindow(obt_display, self->label);
    XMapWindow(obt_display, self->backback);
    XMapWindow(obt_display, self->backfront);

    self->max_press = self->close_press = self->desk_press =
        self->iconify_press = self->shade_press = FALSE;
    self->max_hover = self->close_hover = self->desk_hover =
        self->iconify_hover = self->shade_hover = FALSE;

    /* make sure the size will be different the first time, so the extent hints
       will be set */
    STRUT_SET(self->oldsize, -1, -1, -1, -1);

    set_theme_statics(self);

    return self;
}

static void set_theme_statics(ObFrame *self)
{
    /* set colors/appearance/sizes for stuff that doesn't change */
    XResizeWindow(obt_display, self->max,
                  ob_rr_theme->button_size, ob_rr_theme->button_size);
    XResizeWindow(obt_display, self->iconify,
                  ob_rr_theme->button_size, ob_rr_theme->button_size);
    XResizeWindow(obt_display, self->icon,
                  ob_rr_theme->button_size + 2, ob_rr_theme->button_size + 2);
    XResizeWindow(obt_display, self->close,
                  ob_rr_theme->button_size, ob_rr_theme->button_size);
    XResizeWindow(obt_display, self->desk,
                  ob_rr_theme->button_size, ob_rr_theme->button_size);
    XResizeWindow(obt_display, self->shade,
                  ob_rr_theme->button_size, ob_rr_theme->button_size);
    XResizeWindow(obt_display, self->tltresize,
                  ob_rr_theme->grip_width, ob_rr_theme->paddingy + 1);
    XResizeWindow(obt_display, self->trtresize,
                  ob_rr_theme->grip_width, ob_rr_theme->paddingy + 1);
    XResizeWindow(obt_display, self->tllresize,
                  ob_rr_theme->paddingx + 1, ob_rr_theme->title_height);
    XResizeWindow(obt_display, self->trrresize,
                  ob_rr_theme->paddingx + 1, ob_rr_theme->title_height);
}

static void free_theme_statics(ObFrame *self)
{
}

void frame_free(ObFrame *self)
{
    free_theme_statics(self);

    XDestroyWindow(obt_display, self->window);
    if (self->colormap)
        XFreeColormap(obt_display, self->colormap);

    g_slice_free(ObFrame, self);
}

void frame_show(ObFrame *self)
{
    if (!self->visible) {
        self->visible = TRUE;
        framerender_frame(self);
        /* Grab the server to make sure that the frame window is mapped before
           the client gets its MapNotify, i.e. to make sure the client is
           _visible_ when it gets MapNotify. */
        grab_server(TRUE);
        XMapWindow(obt_display, self->client->window);
        XMapWindow(obt_display, self->window);
        grab_server(FALSE);
    }
}

void frame_hide(ObFrame *self)
{
    if (self->visible) {
        self->visible = FALSE;
        if (!frame_iconify_animating(self))
            XUnmapWindow(obt_display, self->window);
        /* we unmap the client itself so that we can get MapRequest
           events, and because the ICCCM tells us to! */
        XUnmapWindow(obt_display, self->client->window);
        self->client->ignore_unmaps += 1;
    }
}

void frame_adjust_theme(ObFrame *self)
{
    free_theme_statics(self);
    set_theme_statics(self);
}

#ifdef SHAPE
void frame_adjust_shape_kind(ObFrame *self, int kind)
{
    gint num;
    XRectangle xrect[2];
    gboolean shaped;

    shaped = (kind == ShapeBounding && self->client->shaped);
#ifdef ShapeInput
    shaped |= (kind == ShapeInput && self->client->shaped_input);
#endif

    if (!shaped) {
        /* clear the shape on the frame window */
        XShapeCombineMask(obt_display, self->window, kind,
                          self->size.left,
                          self->size.top,
                          None, ShapeSet);
    } else {
        /* make the frame's shape match the clients */
        XShapeCombineShape(obt_display, self->window, kind,
                           self->size.left,
                           self->size.top,
                           self->client->window,
                           kind, ShapeSet);

        num = 0;
        if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
            xrect[0].x = 0;
            xrect[0].y = 0;
            xrect[0].width = self->area.width;
            xrect[0].height = self->size.top;
            ++num;
        }

        if (self->decorations & OB_FRAME_DECOR_HANDLE &&
            ob_rr_theme->handle_height > 0)
        {
            xrect[1].x = 0;
            xrect[1].y = FRAME_HANDLE_Y(self);
            xrect[1].width = self->area.width;
            xrect[1].height = ob_rr_theme->handle_height +
                self->bwidth * 2;
            ++num;
        }

        XShapeCombineRectangles(obt_display, self->window,
                                ShapeBounding, 0, 0, xrect, num,
                                ShapeUnion, Unsorted);
    }
}
#endif

void frame_adjust_shape(ObFrame *self)
{
#ifdef SHAPE
  frame_adjust_shape_kind(self, ShapeBounding);
#ifdef ShapeInput
  frame_adjust_shape_kind(self, ShapeInput);
#endif
#endif
}

void frame_adjust_area(ObFrame *self, gboolean moved,
                       gboolean resized, gboolean fake)
{
    if (resized) {
        /* do this before changing the frame's status like max_horz max_vert */
        frame_adjust_cursors(self);

        self->functions = self->client->functions;
        self->decorations = self->client->decorations;
        self->max_horz = self->client->max_horz;
        self->max_vert = self->client->max_vert;
        self->shaded = self->client->shaded;

        if (self->decorations & OB_FRAME_DECOR_BORDER)
            self->bwidth = self->client->undecorated ?
                ob_rr_theme->ubwidth : ob_rr_theme->fbwidth;
        else
            self->bwidth = 0;

        if (self->decorations & OB_FRAME_DECOR_BORDER &&
            !self->client->undecorated)
        {
            self->cbwidth_l = self->cbwidth_r = ob_rr_theme->cbwidthx;
            self->cbwidth_t = self->cbwidth_b = ob_rr_theme->cbwidthy;
        } else
            self->cbwidth_l = self->cbwidth_t =
                self->cbwidth_r = self->cbwidth_b = 0;

        if (self->max_horz) {
            self->cbwidth_l = self->cbwidth_r = 0;
            self->width = self->client->area.width;
            if (self->max_vert)
                self->cbwidth_b = 0;
        } else
            self->width = self->client->area.width +
                self->cbwidth_l + self->cbwidth_r;

        /* some elements are sized based of the width, so don't let them have
           negative values */
        self->width = MAX(self->width,
                          (ob_rr_theme->grip_width + self->bwidth) * 2 + 1);

        STRUT_SET(self->size,
                  self->cbwidth_l + (!self->max_horz ? self->bwidth : 0),
                  self->cbwidth_t +
                  (!self->max_horz || !self->max_vert ? self->bwidth : 0),
                  self->cbwidth_r + (!self->max_horz ? self->bwidth : 0),
                  self->cbwidth_b +
                  (!self->max_horz || !self->max_vert ? self->bwidth : 0));

        if (self->decorations & OB_FRAME_DECOR_TITLEBAR)
            self->size.top += ob_rr_theme->title_height + self->bwidth;
        else if (self->max_horz && self->max_vert) {
            /* A maximized and undecorated window needs a border on the
               top of the window to let the user still undecorate/unmaximize the
               window via the client menu. */
            self->size.top += self->bwidth;
        }

        if (self->decorations & OB_FRAME_DECOR_HANDLE &&
            ob_rr_theme->handle_height > 0)
        {
            self->size.bottom += ob_rr_theme->handle_height + self->bwidth;
        }

        /* position/size and map/unmap all the windows */

        if (!fake) {
            gint innercornerheight =
                ob_rr_theme->grip_width - self->size.bottom;

            if (self->cbwidth_l) {
                XMoveResizeWindow(obt_display, self->innerleft,
                                  self->size.left - self->cbwidth_l,
                                  self->size.top,
                                  self->cbwidth_l, self->client->area.height);

                XMapWindow(obt_display, self->innerleft);
            } else
                XUnmapWindow(obt_display, self->innerleft);

            if (self->cbwidth_l && innercornerheight > 0) {
                XMoveResizeWindow(obt_display, self->innerbll,
                                  0,
                                  self->client->area.height - 
                                  (ob_rr_theme->grip_width -
                                   self->size.bottom),
                                  self->cbwidth_l,
                                  ob_rr_theme->grip_width - self->size.bottom);

                XMapWindow(obt_display, self->innerbll);
            } else
                XUnmapWindow(obt_display, self->innerbll);

            if (self->cbwidth_r) {
                XMoveResizeWindow(obt_display, self->innerright,
                                  self->size.left + self->client->area.width,
                                  self->size.top,
                                  self->cbwidth_r, self->client->area.height);

                XMapWindow(obt_display, self->innerright);
            } else
                XUnmapWindow(obt_display, self->innerright);

            if (self->cbwidth_r && innercornerheight > 0) {
                XMoveResizeWindow(obt_display, self->innerbrr,
                                  0,
                                  self->client->area.height - 
                                  (ob_rr_theme->grip_width -
                                   self->size.bottom),
                                  self->cbwidth_r,
                                  ob_rr_theme->grip_width - self->size.bottom);

                XMapWindow(obt_display, self->innerbrr);
            } else
                XUnmapWindow(obt_display, self->innerbrr);

            if (self->cbwidth_t) {
                XMoveResizeWindow(obt_display, self->innertop,
                                  self->size.left - self->cbwidth_l,
                                  self->size.top - self->cbwidth_t,
                                  self->client->area.width +
                                  self->cbwidth_l + self->cbwidth_r,
                                  self->cbwidth_t);

                XMapWindow(obt_display, self->innertop);
            } else
                XUnmapWindow(obt_display, self->innertop);

            if (self->cbwidth_b) {
                XMoveResizeWindow(obt_display, self->innerbottom,
                                  self->size.left - self->cbwidth_l,
                                  self->size.top + self->client->area.height,
                                  self->client->area.width +
                                  self->cbwidth_l + self->cbwidth_r,
                                  self->cbwidth_b);

                XMoveResizeWindow(obt_display, self->innerblb,
                                  0, 0,
                                  ob_rr_theme->grip_width + self->bwidth,
                                  self->cbwidth_b);
                XMoveResizeWindow(obt_display, self->innerbrb,
                                  self->client->area.width +
                                  self->cbwidth_l + self->cbwidth_r -
                                  (ob_rr_theme->grip_width + self->bwidth),
                                  0,
                                  ob_rr_theme->grip_width + self->bwidth,
                                  self->cbwidth_b);

                XMapWindow(obt_display, self->innerbottom);
                XMapWindow(obt_display, self->innerblb);
                XMapWindow(obt_display, self->innerbrb);
            } else {
                XUnmapWindow(obt_display, self->innerbottom);
                XUnmapWindow(obt_display, self->innerblb);
                XUnmapWindow(obt_display, self->innerbrb);
            }

            if (self->bwidth) {
                gint titlesides;

                /* height of titleleft and titleright */
                titlesides = (!self->max_horz ? ob_rr_theme->grip_width : 0);

                XMoveResizeWindow(obt_display, self->titletop,
                                  ob_rr_theme->grip_width + self->bwidth, 0,
                                  /* width + bwidth*2 - bwidth*2 - grips*2 */
                                  self->width - ob_rr_theme->grip_width * 2,
                                  self->bwidth);
                XMoveResizeWindow(obt_display, self->titletopleft,
                                  0, 0,
                                  ob_rr_theme->grip_width + self->bwidth,
                                  self->bwidth);
                XMoveResizeWindow(obt_display, self->titletopright,
                                  self->client->area.width +
                                  self->size.left + self->size.right -
                                  ob_rr_theme->grip_width - self->bwidth,
                                  0,
                                  ob_rr_theme->grip_width + self->bwidth,
                                  self->bwidth);

                if (titlesides > 0) {
                    XMoveResizeWindow(obt_display, self->titleleft,
                                      0, self->bwidth,
                                      self->bwidth,
                                      titlesides);
                    XMoveResizeWindow(obt_display, self->titleright,
                                      self->client->area.width +
                                      self->size.left + self->size.right -
                                      self->bwidth,
                                      self->bwidth,
                                      self->bwidth,
                                      titlesides);

                    XMapWindow(obt_display, self->titleleft);
                    XMapWindow(obt_display, self->titleright);
                } else {
                    XUnmapWindow(obt_display, self->titleleft);
                    XUnmapWindow(obt_display, self->titleright);
                }

                XMapWindow(obt_display, self->titletop);
                XMapWindow(obt_display, self->titletopleft);
                XMapWindow(obt_display, self->titletopright);

                if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
                    XMoveResizeWindow(obt_display, self->titlebottom,
                                      (self->max_horz ? 0 : self->bwidth),
                                      ob_rr_theme->title_height + self->bwidth,
                                      self->width,
                                      self->bwidth);

                    XMapWindow(obt_display, self->titlebottom);
                } else
                    XUnmapWindow(obt_display, self->titlebottom);
            } else {
                XUnmapWindow(obt_display, self->titlebottom);

                XUnmapWindow(obt_display, self->titletop);
                XUnmapWindow(obt_display, self->titletopleft);
                XUnmapWindow(obt_display, self->titletopright);
                XUnmapWindow(obt_display, self->titleleft);
                XUnmapWindow(obt_display, self->titleright);
            }

            if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
                XMoveResizeWindow(obt_display, self->title,
                                  (self->max_horz ? 0 : self->bwidth),
                                  self->bwidth,
                                  self->width, ob_rr_theme->title_height);

                XMapWindow(obt_display, self->title);

                if (self->decorations & OB_FRAME_DECOR_GRIPS) {
                    XMoveResizeWindow(obt_display, self->topresize,
                                      ob_rr_theme->grip_width,
                                      0,
                                      self->width - ob_rr_theme->grip_width *2,
                                      ob_rr_theme->paddingy + 1);

                    XMoveWindow(obt_display, self->tltresize, 0, 0);
                    XMoveWindow(obt_display, self->tllresize, 0, 0);
                    XMoveWindow(obt_display, self->trtresize,
                                self->width - ob_rr_theme->grip_width, 0);
                    XMoveWindow(obt_display, self->trrresize,
                                self->width - ob_rr_theme->paddingx - 1, 0);

                    XMapWindow(obt_display, self->topresize);
                    XMapWindow(obt_display, self->tltresize);
                    XMapWindow(obt_display, self->tllresize);
                    XMapWindow(obt_display, self->trtresize);
                    XMapWindow(obt_display, self->trrresize);
                } else {
                    XUnmapWindow(obt_display, self->topresize);
                    XUnmapWindow(obt_display, self->tltresize);
                    XUnmapWindow(obt_display, self->tllresize);
                    XUnmapWindow(obt_display, self->trtresize);
                    XUnmapWindow(obt_display, self->trrresize);
                }
            } else
                XUnmapWindow(obt_display, self->title);
        }

        if ((self->decorations & OB_FRAME_DECOR_TITLEBAR))
            /* layout the title bar elements */
            layout_title(self);

        if (!fake) {
            gint sidebwidth = self->max_horz ? 0 : self->bwidth;

            if (self->bwidth && self->size.bottom) {
                XMoveResizeWindow(obt_display, self->handlebottom,
                                  ob_rr_theme->grip_width +
                                  self->bwidth + sidebwidth,
                                  self->size.top + self->client->area.height +
                                  self->size.bottom - self->bwidth,
                                  self->width - (ob_rr_theme->grip_width +
                                                 sidebwidth) * 2,
                                  self->bwidth);


                if (sidebwidth) {
                    XMoveResizeWindow(obt_display, self->lgripleft,
                                      0,
                                      self->size.top +
                                      self->client->area.height +
                                      self->size.bottom -
                                      (!self->max_horz ?
                                       ob_rr_theme->grip_width :
                                       self->size.bottom - self->cbwidth_b),
                                      self->bwidth,
                                      (!self->max_horz ?
                                       ob_rr_theme->grip_width :
                                       self->size.bottom - self->cbwidth_b));
                    XMoveResizeWindow(obt_display, self->rgripright,
                                  self->size.left +
                                      self->client->area.width +
                                      self->size.right - self->bwidth,
                                      self->size.top +
                                      self->client->area.height +
                                      self->size.bottom -
                                      (!self->max_horz ?
                                       ob_rr_theme->grip_width :
                                       self->size.bottom - self->cbwidth_b),
                                      self->bwidth,
                                      (!self->max_horz ?
                                       ob_rr_theme->grip_width :
                                       self->size.bottom - self->cbwidth_b));

                    XMapWindow(obt_display, self->lgripleft);
                    XMapWindow(obt_display, self->rgripright);
                } else {
                    XUnmapWindow(obt_display, self->lgripleft);
                    XUnmapWindow(obt_display, self->rgripright);
                }

                XMoveResizeWindow(obt_display, self->lgripbottom,
                                  sidebwidth,
                                  self->size.top + self->client->area.height +
                                  self->size.bottom - self->bwidth,
                                  ob_rr_theme->grip_width + self->bwidth,
                                  self->bwidth);
                XMoveResizeWindow(obt_display, self->rgripbottom,
                                  self->size.left + self->client->area.width +
                                  self->size.right - self->bwidth - sidebwidth-
                                  ob_rr_theme->grip_width,
                                  self->size.top + self->client->area.height +
                                  self->size.bottom - self->bwidth,
                                  ob_rr_theme->grip_width + self->bwidth,
                                  self->bwidth);

                XMapWindow(obt_display, self->handlebottom);
                XMapWindow(obt_display, self->lgripbottom);
                XMapWindow(obt_display, self->rgripbottom);

                if (self->decorations & OB_FRAME_DECOR_HANDLE &&
                    ob_rr_theme->handle_height > 0)
                {
                    XMoveResizeWindow(obt_display, self->handletop,
                                      ob_rr_theme->grip_width +
                                      self->bwidth + sidebwidth,
                                      FRAME_HANDLE_Y(self),
                                      self->width - (ob_rr_theme->grip_width +
                                                     sidebwidth) * 2,
                                      self->bwidth);
                    XMapWindow(obt_display, self->handletop);

                    if (self->decorations & OB_FRAME_DECOR_GRIPS) {
                        XMoveResizeWindow(obt_display, self->handleleft,
                                          ob_rr_theme->grip_width,
                                          0,
                                          self->bwidth,
                                          ob_rr_theme->handle_height);
                        XMoveResizeWindow(obt_display, self->handleright,
                                          self->width -
                                          ob_rr_theme->grip_width -
                                          self->bwidth,
                                          0,
                                          self->bwidth,
                                          ob_rr_theme->handle_height);

                        XMoveResizeWindow(obt_display, self->lgriptop,
                                          sidebwidth,
                                          FRAME_HANDLE_Y(self),
                                          ob_rr_theme->grip_width +
                                          self->bwidth,
                                          self->bwidth);
                        XMoveResizeWindow(obt_display, self->rgriptop,
                                          self->size.left +
                                          self->client->area.width +
                                          self->size.right - self->bwidth -
                                          sidebwidth - ob_rr_theme->grip_width,
                                          FRAME_HANDLE_Y(self),
                                          ob_rr_theme->grip_width +
                                          self->bwidth,
                                          self->bwidth);

                        XMapWindow(obt_display, self->handleleft);
                        XMapWindow(obt_display, self->handleright);
                        XMapWindow(obt_display, self->lgriptop);
                        XMapWindow(obt_display, self->rgriptop);
                    } else {
                        XUnmapWindow(obt_display, self->handleleft);
                        XUnmapWindow(obt_display, self->handleright);
                        XUnmapWindow(obt_display, self->lgriptop);
                        XUnmapWindow(obt_display, self->rgriptop);
                    }
                } else {
                    XUnmapWindow(obt_display, self->handleleft);
                    XUnmapWindow(obt_display, self->handleright);
                    XUnmapWindow(obt_display, self->lgriptop);
                    XUnmapWindow(obt_display, self->rgriptop);

                    XUnmapWindow(obt_display, self->handletop);
                }
            } else {
                XUnmapWindow(obt_display, self->handleleft);
                XUnmapWindow(obt_display, self->handleright);
                XUnmapWindow(obt_display, self->lgriptop);
                XUnmapWindow(obt_display, self->rgriptop);

                XUnmapWindow(obt_display, self->handletop);

                XUnmapWindow(obt_display, self->handlebottom);
                XUnmapWindow(obt_display, self->lgripleft);
                XUnmapWindow(obt_display, self->rgripright);
                XUnmapWindow(obt_display, self->lgripbottom);
                XUnmapWindow(obt_display, self->rgripbottom);
            }

            if (self->decorations & OB_FRAME_DECOR_HANDLE &&
                ob_rr_theme->handle_height > 0)
            {
                XMoveResizeWindow(obt_display, self->handle,
                                  sidebwidth,
                                  FRAME_HANDLE_Y(self) + self->bwidth,
                                  self->width, ob_rr_theme->handle_height);
                XMapWindow(obt_display, self->handle);

                if (self->decorations & OB_FRAME_DECOR_GRIPS) {
                    XMoveResizeWindow(obt_display, self->lgrip,
                                      0, 0,
                                      ob_rr_theme->grip_width,
                                      ob_rr_theme->handle_height);
                    XMoveResizeWindow(obt_display, self->rgrip,
                                      self->width - ob_rr_theme->grip_width,
                                      0,
                                      ob_rr_theme->grip_width,
                                      ob_rr_theme->handle_height);

                    XMapWindow(obt_display, self->lgrip);
                    XMapWindow(obt_display, self->rgrip);
                } else {
                    XUnmapWindow(obt_display, self->lgrip);
                    XUnmapWindow(obt_display, self->rgrip);
                }
            } else {
                XUnmapWindow(obt_display, self->lgrip);
                XUnmapWindow(obt_display, self->rgrip);

                XUnmapWindow(obt_display, self->handle);
            }

            if (self->bwidth && !self->max_horz &&
                (self->client->area.height + self->size.top +
                 self->size.bottom) > ob_rr_theme->grip_width * 2)
            {
                XMoveResizeWindow(obt_display, self->left,
                                  0,
                                  self->bwidth + ob_rr_theme->grip_width,
                                  self->bwidth,
                                  self->client->area.height +
                                  self->size.top + self->size.bottom -
                                  ob_rr_theme->grip_width * 2);

                XMapWindow(obt_display, self->left);
            } else
                XUnmapWindow(obt_display, self->left);

            if (self->bwidth && !self->max_horz &&
                (self->client->area.height + self->size.top +
                 self->size.bottom) > ob_rr_theme->grip_width * 2)
            {
                XMoveResizeWindow(obt_display, self->right,
                                  self->client->area.width + self->cbwidth_l +
                                  self->cbwidth_r + self->bwidth,
                                  self->bwidth + ob_rr_theme->grip_width,
                                  self->bwidth,
                                  self->client->area.height +
                                  self->size.top + self->size.bottom -
                                  ob_rr_theme->grip_width * 2);

                XMapWindow(obt_display, self->right);
            } else
                XUnmapWindow(obt_display, self->right);

            XMoveResizeWindow(obt_display, self->backback,
                              self->size.left, self->size.top,
                              self->client->area.width,
                              self->client->area.height);
        }
    }

    /* shading can change without being moved or resized */
    RECT_SET_SIZE(self->area,
                  self->client->area.width +
                  self->size.left + self->size.right,
                  (self->client->shaded ?
                   ob_rr_theme->title_height + self->bwidth * 2:
                   self->client->area.height +
                   self->size.top + self->size.bottom));

    if ((moved || resized) && !fake) {
        /* find the new coordinates, done after setting the frame.size, for
           frame_client_gravity. */
        self->area.x = self->client->area.x;
        self->area.y = self->client->area.y;
        frame_client_gravity(self, &self->area.x, &self->area.y);
    }

    if (!fake) {
        if (!frame_iconify_animating(self))
            /* move and resize the top level frame.
               shading can change without being moved or resized.

               but don't do this during an iconify animation. it will be
               reflected afterwards.
            */
            XMoveResizeWindow(obt_display, self->window,
                              self->area.x,
                              self->area.y,
                              self->area.width,
                              self->area.height);

        /* when the client has StaticGravity, it likes to move around.
           also this correctly positions the client when it maps.
           this also needs to be run when the frame's decorations sizes change!
        */
        XMoveWindow(obt_display, self->client->window,
                    self->size.left, self->size.top);

        if (resized) {
            self->need_render = TRUE;
            framerender_frame(self);
            frame_adjust_shape(self);
        }

        if (!STRUT_EQUAL(self->size, self->oldsize)) {
            gulong vals[4];
            vals[0] = self->size.left;
            vals[1] = self->size.right;
            vals[2] = self->size.top;
            vals[3] = self->size.bottom;
            OBT_PROP_SETA32(self->client->window, NET_FRAME_EXTENTS,
                            CARDINAL, vals, 4);
            OBT_PROP_SETA32(self->client->window, KDE_NET_WM_FRAME_STRUT,
                            CARDINAL, vals, 4);
            self->oldsize = self->size;
        }

        /* if this occurs while we are focus cycling, the indicator needs to
           match the changes */
        if (focus_cycle_target == self->client)
            focus_cycle_update_indicator(self->client);
    }
    if (resized && (self->decorations & OB_FRAME_DECOR_TITLEBAR) &&
        self->label_width)
    {
        XResizeWindow(obt_display, self->label, self->label_width,
                      ob_rr_theme->label_height);
    }
}

static void frame_adjust_cursors(ObFrame *self)
{
    if ((self->functions & OB_CLIENT_FUNC_RESIZE) !=
        (self->client->functions & OB_CLIENT_FUNC_RESIZE) ||
        self->max_horz != self->client->max_horz ||
        self->max_vert != self->client->max_vert ||
        self->shaded != self->client->shaded)
    {
        gboolean r = (self->client->functions & OB_CLIENT_FUNC_RESIZE) &&
            !(self->client->max_horz && self->client->max_vert);
        gboolean topbot = !self->client->max_vert;
        gboolean sh = self->client->shaded;
        XSetWindowAttributes a;

        /* these ones turn off when max vert, and some when shaded */
        a.cursor = ob_cursor(r && topbot && !sh ?
                             OB_CURSOR_NORTH : OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->topresize, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->titletop, CWCursor, &a);
        a.cursor = ob_cursor(r && topbot ? OB_CURSOR_SOUTH : OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->handle, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->handletop, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->handlebottom, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerbottom, CWCursor, &a);

        /* these ones change when shaded */
        a.cursor = ob_cursor(r ? (sh ? OB_CURSOR_WEST : OB_CURSOR_NORTHWEST) :
                             OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->titleleft, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->tltresize, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->tllresize, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->titletopleft, CWCursor, &a);
        a.cursor = ob_cursor(r ? (sh ? OB_CURSOR_EAST : OB_CURSOR_NORTHEAST) :
                             OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->titleright, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->trtresize, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->trrresize, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->titletopright, CWCursor,&a);

        /* these ones are pretty static */
        a.cursor = ob_cursor(r ? OB_CURSOR_WEST : OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->left, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerleft, CWCursor, &a);
        a.cursor = ob_cursor(r ? OB_CURSOR_EAST : OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->right, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerright, CWCursor, &a);
        a.cursor = ob_cursor(r ? OB_CURSOR_SOUTHWEST : OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->lgrip, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->handleleft, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->lgripleft, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->lgriptop, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->lgripbottom, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerbll, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerblb, CWCursor, &a);
        a.cursor = ob_cursor(r ? OB_CURSOR_SOUTHEAST : OB_CURSOR_NONE);
        XChangeWindowAttributes(obt_display, self->rgrip, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->handleright, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->rgripright, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->rgriptop, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->rgripbottom, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerbrr, CWCursor, &a);
        XChangeWindowAttributes(obt_display, self->innerbrb, CWCursor, &a);
    }
}

void frame_adjust_client_area(ObFrame *self)
{
    /* adjust the window which is there to prevent flashing on unmap */
    XMoveResizeWindow(obt_display, self->backfront, 0, 0,
                      self->client->area.width,
                      self->client->area.height);
}

void frame_adjust_state(ObFrame *self)
{
    self->need_render = TRUE;
    framerender_frame(self);
}

void frame_adjust_focus(ObFrame *self, gboolean hilite)
{
    ob_debug_type(OB_DEBUG_FOCUS,
                  "Frame for 0x%x has focus: %d",
                  self->client->window, hilite);
    self->focused = hilite;
    self->need_render = TRUE;
    framerender_frame(self);
    XFlush(obt_display);
}

void frame_adjust_title(ObFrame *self)
{
    self->need_render = TRUE;
    framerender_frame(self);
}

void frame_adjust_icon(ObFrame *self)
{
    self->need_render = TRUE;
    framerender_frame(self);
}

void frame_grab_client(ObFrame *self)
{
    /* DO NOT map the client window here. we used to do that, but it is bogus.
       we need to set up the client's dimensions and everything before we
       send a mapnotify or we create race conditions.
    */

    /* reparent the client to the frame */
    XReparentWindow(obt_display, self->client->window, self->window, 0, 0);

    /*
      When reparenting the client window, it is usually not mapped yet, since
      this occurs from a MapRequest. However, in the case where Openbox is
      starting up, the window is already mapped, so we'll see an unmap event
      for it.
    */
    if (ob_state() == OB_STATE_STARTING)
        ++self->client->ignore_unmaps;

    /* select the event mask on the client's parent (to receive config/map
       req's) the ButtonPress is to catch clicks on the client border */
    XSelectInput(obt_display, self->window, FRAME_EVENTMASK);

    /* set all the windows for the frame in the window_map */
    window_add(&self->window, CLIENT_AS_WINDOW(self->client));
    window_add(&self->backback, CLIENT_AS_WINDOW(self->client));
    window_add(&self->backfront, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerleft, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innertop, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerright, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerbottom, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerblb, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerbll, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerbrb, CLIENT_AS_WINDOW(self->client));
    window_add(&self->innerbrr, CLIENT_AS_WINDOW(self->client));
    window_add(&self->title, CLIENT_AS_WINDOW(self->client));
    window_add(&self->label, CLIENT_AS_WINDOW(self->client));
    window_add(&self->max, CLIENT_AS_WINDOW(self->client));
    window_add(&self->close, CLIENT_AS_WINDOW(self->client));
    window_add(&self->desk, CLIENT_AS_WINDOW(self->client));
    window_add(&self->shade, CLIENT_AS_WINDOW(self->client));
    window_add(&self->icon, CLIENT_AS_WINDOW(self->client));
    window_add(&self->iconify, CLIENT_AS_WINDOW(self->client));
    window_add(&self->handle, CLIENT_AS_WINDOW(self->client));
    window_add(&self->lgrip, CLIENT_AS_WINDOW(self->client));
    window_add(&self->rgrip, CLIENT_AS_WINDOW(self->client));
    window_add(&self->topresize, CLIENT_AS_WINDOW(self->client));
    window_add(&self->tltresize, CLIENT_AS_WINDOW(self->client));
    window_add(&self->tllresize, CLIENT_AS_WINDOW(self->client));
    window_add(&self->trtresize, CLIENT_AS_WINDOW(self->client));
    window_add(&self->trrresize, CLIENT_AS_WINDOW(self->client));
    window_add(&self->left, CLIENT_AS_WINDOW(self->client));
    window_add(&self->right, CLIENT_AS_WINDOW(self->client));
    window_add(&self->titleleft, CLIENT_AS_WINDOW(self->client));
    window_add(&self->titletop, CLIENT_AS_WINDOW(self->client));
    window_add(&self->titletopleft, CLIENT_AS_WINDOW(self->client));
    window_add(&self->titletopright, CLIENT_AS_WINDOW(self->client));
    window_add(&self->titleright, CLIENT_AS_WINDOW(self->client));
    window_add(&self->titlebottom, CLIENT_AS_WINDOW(self->client));
    window_add(&self->handleleft, CLIENT_AS_WINDOW(self->client));
    window_add(&self->handletop, CLIENT_AS_WINDOW(self->client));
    window_add(&self->handleright, CLIENT_AS_WINDOW(self->client));
    window_add(&self->handlebottom, CLIENT_AS_WINDOW(self->client));
    window_add(&self->lgripleft, CLIENT_AS_WINDOW(self->client));
    window_add(&self->lgriptop, CLIENT_AS_WINDOW(self->client));
    window_add(&self->lgripbottom, CLIENT_AS_WINDOW(self->client));
    window_add(&self->rgripright, CLIENT_AS_WINDOW(self->client));
    window_add(&self->rgriptop, CLIENT_AS_WINDOW(self->client));
    window_add(&self->rgripbottom, CLIENT_AS_WINDOW(self->client));
}

static gboolean find_reparent(XEvent *e, gpointer data)
{
    const ObFrame *self = data;

    /* Find ReparentNotify events for the window that aren't being reparented into the
       frame, thus the client reparenting itself off the frame. */
    return e->type == ReparentNotify && e->xreparent.window == self->client->window &&
        e->xreparent.parent != self->window;
}

void frame_release_client(ObFrame *self)
{
    /* if there was any animation going on, kill it */
    if (self->iconify_animation_timer)
        g_source_remove(self->iconify_animation_timer);

    /* check if the app has already reparented its window away */
    if (!xqueue_exists_local(find_reparent, self)) {
        /* according to the ICCCM - if the client doesn't reparent itself,
           then we will reparent the window to root for them */
        XReparentWindow(obt_display, self->client->window, obt_root(ob_screen),
                        self->client->area.x, self->client->area.y);
    }

    /* remove all the windows for the frame from the window_map */
    window_remove(self->window);
    window_remove(self->backback);
    window_remove(self->backfront);
    window_remove(self->innerleft);
    window_remove(self->innertop);
    window_remove(self->innerright);
    window_remove(self->innerbottom);
    window_remove(self->innerblb);
    window_remove(self->innerbll);
    window_remove(self->innerbrb);
    window_remove(self->innerbrr);
    window_remove(self->title);
    window_remove(self->label);
    window_remove(self->max);
    window_remove(self->close);
    window_remove(self->desk);
    window_remove(self->shade);
    window_remove(self->icon);
    window_remove(self->iconify);
    window_remove(self->handle);
    window_remove(self->lgrip);
    window_remove(self->rgrip);
    window_remove(self->topresize);
    window_remove(self->tltresize);
    window_remove(self->tllresize);
    window_remove(self->trtresize);
    window_remove(self->trrresize);
    window_remove(self->left);
    window_remove(self->right);
    window_remove(self->titleleft);
    window_remove(self->titletop);
    window_remove(self->titletopleft);
    window_remove(self->titletopright);
    window_remove(self->titleright);
    window_remove(self->titlebottom);
    window_remove(self->handleleft);
    window_remove(self->handletop);
    window_remove(self->handleright);
    window_remove(self->handlebottom);
    window_remove(self->lgripleft);
    window_remove(self->lgriptop);
    window_remove(self->lgripbottom);
    window_remove(self->rgripright);
    window_remove(self->rgriptop);
    window_remove(self->rgripbottom);

    if (self->flash_timer) g_source_remove(self->flash_timer);
}

/* is there anything present between us and the label? */
static gboolean is_button_present(ObFrame *self, const gchar *lc, gint dir) {
    for (; *lc != '\0' && lc >= config_title_layout; lc += dir) {
        if (*lc == ' ') continue; /* it was invalid */
        if (*lc == 'N' && self->decorations & OB_FRAME_DECOR_ICON)
            return TRUE;
        if (*lc == 'D' && self->decorations & OB_FRAME_DECOR_ALLDESKTOPS)
            return TRUE;
        if (*lc == 'S' && self->decorations & OB_FRAME_DECOR_SHADE)
            return TRUE;
        if (*lc == 'I' && self->decorations & OB_FRAME_DECOR_ICONIFY)
            return TRUE;
        if (*lc == 'M' && self->decorations & OB_FRAME_DECOR_MAXIMIZE)
            return TRUE;
        if (*lc == 'C' && self->decorations & OB_FRAME_DECOR_CLOSE)
            return TRUE;
        if (*lc == 'L') return FALSE;
    }
    return FALSE;
}

static void place_button(ObFrame *self, const char *lc, gint bwidth,
                         gint left, gint i,
                         gint *x, gint *button_on, gint *button_x)
{
  if (!(*button_on = is_button_present(self, lc, i)))
    return;

  self->label_width -= bwidth;
  if (i > 0)
    *button_x = *x;
  *x += i * bwidth;
  if (i < 0) {
    if (self->label_x <= left || *x > self->label_x) {
      *button_x = *x;
    } else {
      /* the button would have been drawn on top of another button */
      *button_on = FALSE;
      self->label_width += bwidth;
    }
  }
}

static void layout_title(ObFrame *self)
{
    gchar *lc;
    gint i;

    const gint bwidth = ob_rr_theme->button_size + ob_rr_theme->paddingx + 1;
    /* position of the leftmost button */
    const gint left = ob_rr_theme->paddingx + 1;
    /* position of the rightmost button */
    const gint right = self->width;

    /* turn them all off */
    self->icon_on = self->desk_on = self->shade_on = self->iconify_on =
        self->max_on = self->close_on = self->label_on = FALSE;
    self->label_width = self->width - (ob_rr_theme->paddingx + 1) * 2;
    self->leftmost = self->rightmost = OB_FRAME_CONTEXT_NONE;

    /* figure out what's being shown, find each element's position, and the
       width of the label

       do the ones before the label, then after the label,
       i will be +1 the first time through when working to the left,
       and -1 the second time through when working to the right */
    for (i = 1; i >= -1; i-=2) {
        gint x;
        ObFrameContext *firstcon;

        if (i > 0) {
            x = left;
            lc = config_title_layout;
            firstcon = &self->leftmost;
        } else {
            x = right;
            lc = config_title_layout + strlen(config_title_layout)-1;
            firstcon = &self->rightmost;
        }

        /* stop at the end of the string (or the label, which calls break) */
        for (; *lc != '\0' && lc >= config_title_layout; lc+=i) {
            if (*lc == 'L') {
                if (i > 0) {
                    self->label_on = TRUE;
                    self->label_x = x;
                }
                break; /* break the for loop, do other side of label */
            } else if (*lc == 'N') {
                if (firstcon) *firstcon = OB_FRAME_CONTEXT_ICON;
                /* icon is bigger than buttons */
                place_button(self, lc, bwidth + 2, left, i, &x, &self->icon_on, &self->icon_x);
            } else if (*lc == 'D') {
                if (firstcon) *firstcon = OB_FRAME_CONTEXT_ALLDESKTOPS;
                place_button(self, lc, bwidth, left, i, &x, &self->desk_on, &self->desk_x);
            } else if (*lc == 'S') {
                if (firstcon) *firstcon = OB_FRAME_CONTEXT_SHADE;
                place_button(self, lc, bwidth, left, i, &x, &self->shade_on, &self->shade_x);
            } else if (*lc == 'I') {
                if (firstcon) *firstcon = OB_FRAME_CONTEXT_ICONIFY;
                place_button(self, lc, bwidth, left, i, &x, &self->iconify_on, &self->iconify_x);
            } else if (*lc == 'M') {
                if (firstcon) *firstcon = OB_FRAME_CONTEXT_MAXIMIZE;
                place_button(self, lc, bwidth, left, i, &x, &self->max_on, &self->max_x);
            } else if (*lc == 'C') {
                if (firstcon) *firstcon = OB_FRAME_CONTEXT_CLOSE;
                place_button(self, lc, bwidth, left, i, &x, &self->close_on, &self->close_x);
            } else
                continue; /* don't set firstcon */
            firstcon = NULL;
        }
    }

    /* position and map the elements */
    if (self->icon_on) {
        XMapWindow(obt_display, self->icon);
        XMoveWindow(obt_display, self->icon, self->icon_x,
                    ob_rr_theme->paddingy);
    } else
        XUnmapWindow(obt_display, self->icon);

    if (self->desk_on) {
        XMapWindow(obt_display, self->desk);
        XMoveWindow(obt_display, self->desk, self->desk_x,
                    ob_rr_theme->paddingy + 1);
    } else
        XUnmapWindow(obt_display, self->desk);

    if (self->shade_on) {
        XMapWindow(obt_display, self->shade);
        XMoveWindow(obt_display, self->shade, self->shade_x,
                    ob_rr_theme->paddingy + 1);
    } else
        XUnmapWindow(obt_display, self->shade);

    if (self->iconify_on) {
        XMapWindow(obt_display, self->iconify);
        XMoveWindow(obt_display, self->iconify, self->iconify_x,
                    ob_rr_theme->paddingy + 1);
    } else
        XUnmapWindow(obt_display, self->iconify);

    if (self->max_on) {
        XMapWindow(obt_display, self->max);
        XMoveWindow(obt_display, self->max, self->max_x,
                    ob_rr_theme->paddingy + 1);
    } else
        XUnmapWindow(obt_display, self->max);

    if (self->close_on) {
        XMapWindow(obt_display, self->close);
        XMoveWindow(obt_display, self->close, self->close_x,
                    ob_rr_theme->paddingy + 1);
    } else
        XUnmapWindow(obt_display, self->close);

    if (self->label_on && self->label_width > 0) {
        XMapWindow(obt_display, self->label);
        XMoveWindow(obt_display, self->label, self->label_x,
                    ob_rr_theme->paddingy);
    } else
        XUnmapWindow(obt_display, self->label);
}

gboolean frame_next_context_from_string(gchar *names, ObFrameContext *cx)
{
    gchar *p, *n;

    if (!*names) /* empty string */
        return FALSE;

    /* find the first space */
    for (p = names; *p; p = g_utf8_next_char(p)) {
        const gunichar c = g_utf8_get_char(p);
        if (g_unichar_isspace(c)) break;
    }

    if (p == names) {
        /* leading spaces in the string */
        n = g_utf8_next_char(names);
        if (!frame_next_context_from_string(n, cx))
            return FALSE;
    } else {
        n = p;
        if (*p) {
            /* delete the space with null zero(s) */
            while (n < g_utf8_next_char(p))
                *(n++) = '\0';
        }

        *cx = frame_context_from_string(names);

        /* find the next non-space */
        for (; *n; n = g_utf8_next_char(n)) {
            const gunichar c = g_utf8_get_char(n);
            if (!g_unichar_isspace(c)) break;
        }
    }

    /* delete everything we just read (copy everything at n to the start of
       the string */
    for (p = names; *n; ++p, ++n)
        *p = *n;
    *p = *n;

    return TRUE;
}

ObFrameContext frame_context_from_string(const gchar *name)
{
    if (!g_ascii_strcasecmp("Desktop", name))
        return OB_FRAME_CONTEXT_DESKTOP;
    else if (!g_ascii_strcasecmp("Root", name))
        return OB_FRAME_CONTEXT_ROOT;
    else if (!g_ascii_strcasecmp("Client", name))
        return OB_FRAME_CONTEXT_CLIENT;
    else if (!g_ascii_strcasecmp("Titlebar", name))
        return OB_FRAME_CONTEXT_TITLEBAR;
    else if (!g_ascii_strcasecmp("Frame", name))
        return OB_FRAME_CONTEXT_FRAME;
    else if (!g_ascii_strcasecmp("TLCorner", name))
        return OB_FRAME_CONTEXT_TLCORNER;
    else if (!g_ascii_strcasecmp("TRCorner", name))
        return OB_FRAME_CONTEXT_TRCORNER;
    else if (!g_ascii_strcasecmp("BLCorner", name))
        return OB_FRAME_CONTEXT_BLCORNER;
    else if (!g_ascii_strcasecmp("BRCorner", name))
        return OB_FRAME_CONTEXT_BRCORNER;
    else if (!g_ascii_strcasecmp("Top", name))
        return OB_FRAME_CONTEXT_TOP;
    else if (!g_ascii_strcasecmp("Bottom", name))
        return OB_FRAME_CONTEXT_BOTTOM;
    else if (!g_ascii_strcasecmp("Left", name))
        return OB_FRAME_CONTEXT_LEFT;
    else if (!g_ascii_strcasecmp("Right", name))
        return OB_FRAME_CONTEXT_RIGHT;
    else if (!g_ascii_strcasecmp("Maximize", name))
        return OB_FRAME_CONTEXT_MAXIMIZE;
    else if (!g_ascii_strcasecmp("AllDesktops", name))
        return OB_FRAME_CONTEXT_ALLDESKTOPS;
    else if (!g_ascii_strcasecmp("Shade", name))
        return OB_FRAME_CONTEXT_SHADE;
    else if (!g_ascii_strcasecmp("Iconify", name))
        return OB_FRAME_CONTEXT_ICONIFY;
    else if (!g_ascii_strcasecmp("Icon", name))
        return OB_FRAME_CONTEXT_ICON;
    else if (!g_ascii_strcasecmp("Close", name))
        return OB_FRAME_CONTEXT_CLOSE;
    else if (!g_ascii_strcasecmp("MoveResize", name))
        return OB_FRAME_CONTEXT_MOVE_RESIZE;
    else if (!g_ascii_strcasecmp("Dock", name))
        return OB_FRAME_CONTEXT_DOCK;

    return OB_FRAME_CONTEXT_NONE;
}

ObFrameContext frame_context(ObClient *client, Window win, gint x, gint y)
{
    ObFrame *self;
    ObWindow *obwin;

    if (moveresize_in_progress)
        return OB_FRAME_CONTEXT_MOVE_RESIZE;

    if (win == obt_root(ob_screen))
        return OB_FRAME_CONTEXT_ROOT;
    if ((obwin = window_find(win))) {
        if (WINDOW_IS_DOCK(obwin)) {
          return OB_FRAME_CONTEXT_DOCK;
        }
    }
    if (client == NULL) return OB_FRAME_CONTEXT_NONE;
    if (win == client->window) {
        /* conceptually, this is the desktop, as far as users are
           concerned */
        if (client->type == OB_CLIENT_TYPE_DESKTOP)
            return OB_FRAME_CONTEXT_DESKTOP;
        return OB_FRAME_CONTEXT_CLIENT;
    }

    self = client->frame;

    /* when the user clicks in the corners of the titlebar and the client
       is fully maximized, then treat it like they clicked in the
       button that is there */
    if (self->max_horz && self->max_vert &&
        (win == self->title || win == self->titletop ||
         win == self->titleleft || win == self->titletopleft ||
         win == self->titleright || win == self->titletopright))
    {
        /* get the mouse coords in reference to the whole frame */
        gint fx = x;
        gint fy = y;

        /* these windows are down a border width from the top of the frame */
        if (win == self->title ||
            win == self->titleleft || win == self->titleright)
            fy += self->bwidth;

        /* title is a border width in from the edge */
        if (win == self->title)
            fx += self->bwidth;
        /* titletop is a bit to the right */
        else if (win == self->titletop)
            fx += ob_rr_theme->grip_width + self->bwidth;
        /* titletopright is way to the right edge */
        else if (win == self->titletopright)
            fx += self->area.width - (ob_rr_theme->grip_width + self->bwidth);
        /* titleright is even more way to the right edge */
        else if (win == self->titleright)
            fx += self->area.width - self->bwidth;

        /* figure out if we're over the area that should be considered a
           button */
        if (fy < self->bwidth + ob_rr_theme->paddingy + 1 +
            ob_rr_theme->button_size)
        {
            if (fx < (self->bwidth + ob_rr_theme->paddingx + 1 +
                      ob_rr_theme->button_size))
            {
                if (self->leftmost != OB_FRAME_CONTEXT_NONE)
                    return self->leftmost;
            }
            else if (fx >= (self->area.width -
                            (self->bwidth + ob_rr_theme->paddingx + 1 +
                             ob_rr_theme->button_size)))
            {
                if (self->rightmost != OB_FRAME_CONTEXT_NONE)
                    return self->rightmost;
            }
        }

        /* there is no resizing maximized windows so make them the titlebar
           context */
        return OB_FRAME_CONTEXT_TITLEBAR;
    }
    else if (self->max_vert &&
             (win == self->titletop || win == self->topresize))
        /* can't resize vertically when max vert */
        return OB_FRAME_CONTEXT_TITLEBAR;
    else if (self->shaded &&
             (win == self->titletop || win == self->topresize))
        /* can't resize vertically when shaded */
        return OB_FRAME_CONTEXT_TITLEBAR;

    if (win == self->window)            return OB_FRAME_CONTEXT_FRAME;
    if (win == self->label)             return OB_FRAME_CONTEXT_TITLEBAR;
    if (win == self->handle)            return OB_FRAME_CONTEXT_BOTTOM;
    if (win == self->handletop)         return OB_FRAME_CONTEXT_BOTTOM;
    if (win == self->handlebottom)      return OB_FRAME_CONTEXT_BOTTOM;
    if (win == self->handleleft)        return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->lgrip)             return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->lgripleft)         return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->lgriptop)          return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->lgripbottom)       return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->handleright)       return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->rgrip)             return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->rgripright)        return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->rgriptop)          return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->rgripbottom)       return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->title)             return OB_FRAME_CONTEXT_TITLEBAR;
    if (win == self->titlebottom)       return OB_FRAME_CONTEXT_TITLEBAR;
    if (win == self->titleleft)         return OB_FRAME_CONTEXT_TLCORNER;
    if (win == self->titletopleft)      return OB_FRAME_CONTEXT_TLCORNER;
    if (win == self->titleright)        return OB_FRAME_CONTEXT_TRCORNER;
    if (win == self->titletopright)     return OB_FRAME_CONTEXT_TRCORNER;
    if (win == self->titletop)          return OB_FRAME_CONTEXT_TOP;
    if (win == self->topresize)         return OB_FRAME_CONTEXT_TOP;
    if (win == self->tltresize)         return OB_FRAME_CONTEXT_TLCORNER;
    if (win == self->tllresize)         return OB_FRAME_CONTEXT_TLCORNER;
    if (win == self->trtresize)         return OB_FRAME_CONTEXT_TRCORNER;
    if (win == self->trrresize)         return OB_FRAME_CONTEXT_TRCORNER;
    if (win == self->left)              return OB_FRAME_CONTEXT_LEFT;
    if (win == self->right)             return OB_FRAME_CONTEXT_RIGHT;
    if (win == self->innertop)          return OB_FRAME_CONTEXT_TITLEBAR;
    if (win == self->innerleft)         return OB_FRAME_CONTEXT_LEFT;
    if (win == self->innerbottom)       return OB_FRAME_CONTEXT_BOTTOM;
    if (win == self->innerright)        return OB_FRAME_CONTEXT_RIGHT;
    if (win == self->innerbll)          return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->innerblb)          return OB_FRAME_CONTEXT_BLCORNER;
    if (win == self->innerbrr)          return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->innerbrb)          return OB_FRAME_CONTEXT_BRCORNER;
    if (win == self->max)               return OB_FRAME_CONTEXT_MAXIMIZE;
    if (win == self->iconify)           return OB_FRAME_CONTEXT_ICONIFY;
    if (win == self->close)             return OB_FRAME_CONTEXT_CLOSE;
    if (win == self->icon)              return OB_FRAME_CONTEXT_ICON;
    if (win == self->desk)              return OB_FRAME_CONTEXT_ALLDESKTOPS;
    if (win == self->shade)             return OB_FRAME_CONTEXT_SHADE;

    return OB_FRAME_CONTEXT_NONE;
}

void frame_client_gravity(ObFrame *self, gint *x, gint *y)
{
    /* horizontal */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case SouthWestGravity:
    case WestGravity:
        break;

    case NorthGravity:
    case SouthGravity:
    case CenterGravity:
        /* the middle of the client will be the middle of the frame */
        *x -= (self->size.right - self->size.left) / 2;
        break;

    case NorthEastGravity:
    case SouthEastGravity:
    case EastGravity:
        /* the right side of the client will be the right side of the frame */
        *x -= self->size.right + self->size.left -
            self->client->border_width * 2;
        break;

    case ForgetGravity:
    case StaticGravity:
        /* the client's position won't move */
        *x -= self->size.left - self->client->border_width;
        break;
    }

    /* vertical */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case NorthEastGravity:
    case NorthGravity:
        break;

    case CenterGravity:
    case EastGravity:
    case WestGravity:
        /* the middle of the client will be the middle of the frame */
        *y -= (self->size.bottom - self->size.top) / 2;
        break;

    case SouthWestGravity:
    case SouthEastGravity:
    case SouthGravity:
        /* the bottom of the client will be the bottom of the frame */
        *y -= self->size.bottom + self->size.top -
            self->client->border_width * 2;
        break;

    case ForgetGravity:
    case StaticGravity:
        /* the client's position won't move */
        *y -= self->size.top - self->client->border_width;
        break;
    }
}

void frame_frame_gravity(ObFrame *self, gint *x, gint *y)
{
    /* horizontal */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
        break;
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
        /* the middle of the client will be the middle of the frame */
        *x += (self->size.right - self->size.left) / 2;
        break;
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
        /* the right side of the client will be the right side of the frame */
        *x += self->size.right + self->size.left -
            self->client->border_width * 2;
        break;
    case StaticGravity:
    case ForgetGravity:
        /* the client's position won't move */
        *x += self->size.left - self->client->border_width;
        break;
    }

    /* vertical */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
        break;
    case WestGravity:
    case CenterGravity:
    case EastGravity:
        /* the middle of the client will be the middle of the frame */
        *y += (self->size.bottom - self->size.top) / 2;
        break;
    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
        /* the bottom of the client will be the bottom of the frame */
        *y += self->size.bottom + self->size.top -
            self->client->border_width * 2;
        break;
    case StaticGravity:
    case ForgetGravity:
        /* the client's position won't move */
        *y += self->size.top - self->client->border_width;
        break;
    }
}

void frame_rect_to_frame(ObFrame *self, Rect *r)
{
    r->width += self->size.left + self->size.right;
    r->height += self->size.top + self->size.bottom;
    frame_client_gravity(self, &r->x, &r->y);
}

void frame_rect_to_client(ObFrame *self, Rect *r)
{
    r->width -= self->size.left + self->size.right;
    r->height -= self->size.top + self->size.bottom;
    frame_frame_gravity(self, &r->x, &r->y);
}

static void flash_done(gpointer data)
{
    ObFrame *self = data;

    self->flash_timer = 0;
}

static gboolean flash_timeout(gpointer data)
{
    ObFrame *self = data;
    GTimeVal now;

    g_get_current_time(&now);
    if (now.tv_sec > self->flash_end.tv_sec ||
        (now.tv_sec == self->flash_end.tv_sec &&
         now.tv_usec >= self->flash_end.tv_usec))
        self->flashing = FALSE;

    if (!self->flashing) {
        if (self->focused != self->flash_on)
            frame_adjust_focus(self, self->focused);

        return FALSE; /* we are done */
    }

    self->flash_on = !self->flash_on;
    if (!self->focused) {
        frame_adjust_focus(self, self->flash_on);
        self->focused = FALSE;
    }

    return TRUE; /* go again */
}

void frame_flash_start(ObFrame *self)
{
    self->flash_on = self->focused;

    if (!self->flashing)
        self->flash_timer = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                               600, flash_timeout, self,
                                               flash_done);
    g_get_current_time(&self->flash_end);
    g_time_val_add(&self->flash_end, G_USEC_PER_SEC * 5);

    self->flashing = TRUE;
}

void frame_flash_stop(ObFrame *self)
{
    self->flashing = FALSE;
}

static gulong frame_animate_iconify_time_left(ObFrame *self,
                                              const GTimeVal *now)
{
    glong sec, usec;
    sec = self->iconify_animation_end.tv_sec - now->tv_sec;
    usec = self->iconify_animation_end.tv_usec - now->tv_usec;
    if (usec < 0) {
        usec += G_USEC_PER_SEC;
        sec--;
    }
    /* no negative values */
    return MAX(sec * G_USEC_PER_SEC + usec, 0);
}

static gboolean frame_animate_iconify(gpointer p)
{
    ObFrame *self = p;
    gint x, y, w, h;
    gint iconx, icony, iconw;
    GTimeVal now;
    gulong time;
    gboolean iconifying;

    if (self->client->icon_geometry.width == 0) {
        /* there is no icon geometry set so just go straight down */
        const Rect *a;

        a = screen_physical_area_monitor(screen_find_monitor(&self->area));
        iconx = self->area.x + self->area.width / 2 + 32;
        icony = a->y + a->width;
        iconw = 64;
    } else {
        iconx = self->client->icon_geometry.x;
        icony = self->client->icon_geometry.y;
        iconw = self->client->icon_geometry.width;
    }

    iconifying = self->iconify_animation_going > 0;

    /* how far do we have left to go ? */
    g_get_current_time(&now);
    time = frame_animate_iconify_time_left(self, &now);

    if ((time > 0 && iconifying) || (time == 0 && !iconifying)) {
        /* start where the frame is supposed to be */
        x = self->area.x;
        y = self->area.y;
        w = self->area.width;
        h = self->area.height;
    } else {
        /* start at the icon */
        x = iconx;
        y = icony;
        w = iconw;
        h = self->size.top; /* just the titlebar */
    }

    if (time > 0) {
        glong dx, dy, dw;
        glong elapsed;

        dx = self->area.x - iconx;
        dy = self->area.y - icony;
        dw = self->area.width - self->bwidth * 2 - iconw;
         /* if restoring, we move in the opposite direction */
        if (!iconifying) { dx = -dx; dy = -dy; dw = -dw; }

        elapsed = FRAME_ANIMATE_ICONIFY_TIME - time;
        x = x - (dx * elapsed) / FRAME_ANIMATE_ICONIFY_TIME;
        y = y - (dy * elapsed) / FRAME_ANIMATE_ICONIFY_TIME;
        w = w - (dw * elapsed) / FRAME_ANIMATE_ICONIFY_TIME;
        h = self->size.top; /* just the titlebar */
    }

    XMoveResizeWindow(obt_display, self->window, x, y, w, h);
    XFlush(obt_display);

    return time > 0; /* repeat until we're out of time */
}

void frame_end_iconify_animation(gpointer data)
{
    ObFrame *self = data;
    /* see if there is an animation going */
    if (self->iconify_animation_going == 0) return;

    if (!self->visible)
        XUnmapWindow(obt_display, self->window);
    else {
        /* Send a ConfigureNotify when the animation is done, this fixes
           KDE's pager showing the window in the wrong place.  since the
           window is mapped at a different location and is then moved, we
           need to send the synthetic configurenotify, since apps may have
           read the position when the client mapped, apparently. */
        client_reconfigure(self->client, TRUE);
    }

    /* we're not animating any more ! */
    self->iconify_animation_going = 0;
    self->iconify_animation_timer = 0;

    XMoveResizeWindow(obt_display, self->window,
                      self->area.x, self->area.y,
                      self->area.width, self->area.height);
    /* we delay re-rendering until after we're done animating */
    framerender_frame(self);
    XFlush(obt_display);
}

void frame_begin_iconify_animation(ObFrame *self, gboolean iconifying)
{
    gulong time;
    gboolean new_anim = FALSE;
    gboolean set_end = TRUE;
    GTimeVal now;

    /* if there is no titlebar, just don't animate for now
       XXX it would be nice tho.. */
    if (!(self->decorations & OB_FRAME_DECOR_TITLEBAR))
        return;

    /* get the current time */
    g_get_current_time(&now);

    /* get how long until the end */
    time = FRAME_ANIMATE_ICONIFY_TIME;
    if (self->iconify_animation_going) {
        if (!!iconifying != (self->iconify_animation_going > 0)) {
            /* animation was already going on in the opposite direction */
            time = time - frame_animate_iconify_time_left(self, &now);
        } else
            /* animation was already going in the same direction */
            set_end = FALSE;
    } else
        new_anim = TRUE;
    self->iconify_animation_going = iconifying ? 1 : -1;

    /* set the ending time */
    if (set_end) {
        self->iconify_animation_end.tv_sec = now.tv_sec;
        self->iconify_animation_end.tv_usec = now.tv_usec;
        g_time_val_add(&self->iconify_animation_end, time);
    }

    if (new_anim) {
        if (self->iconify_animation_timer)
            g_source_remove(self->iconify_animation_timer);
        self->iconify_animation_timer =
            g_timeout_add_full(G_PRIORITY_DEFAULT,
                               FRAME_ANIMATE_ICONIFY_STEP_TIME,
                               frame_animate_iconify, self,
                               frame_end_iconify_animation);
                               

        /* do the first step */
        frame_animate_iconify(self);

        /* show it during the animation even if it is not "visible" */
        if (!self->visible)
            XMapWindow(obt_display, self->window);
    }
}
