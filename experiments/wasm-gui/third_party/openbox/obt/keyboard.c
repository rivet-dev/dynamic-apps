/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/keyboard.c for the Openbox window manager
   Copyright (c) 2007        Dana Jansens

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

#include "obt/display.h"
#include "obt/keyboard.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

struct _ObtIC
{
    guint ref;
    XIC xic;
    Window client;
    Window focus;
};

/* These masks are constants and the modifier keys are bound to them as
   anyone sees fit:
        ShiftMask (1<<0), LockMask (1<<1), ControlMask (1<<2), Mod1Mask (1<<3),
        Mod2Mask (1<<4), Mod3Mask (1<<5), Mod4Mask (1<<6), Mod5Mask (1<<7)
*/
#define NUM_MASKS 8
#define ALL_MASKS 0xff /* an or'ing of all 8 keyboard masks */

/* Get the bitflag for the n'th modifier mask */
#define nth_mask(n) (1 << n)

static void set_modkey_mask(guchar mask, KeySym sym);
static void xim_init(void);
void obt_keyboard_shutdown();
void obt_keyboard_context_renew(ObtIC *ic);

static XModifierKeymap *modmap;
static KeySym *keymap;
static gint min_keycode, max_keycode, keysyms_per_keycode;
/*! This is a bitmask of the different masks for each modifier key */
static guchar modkeys_keys[OBT_KEYBOARD_NUM_MODKEYS];

static gboolean alt_l = FALSE;
static gboolean meta_l = FALSE;
static gboolean super_l = FALSE;
static gboolean hyper_l = FALSE;

static gboolean started = FALSE;

static XIM xim = NULL;
static XIMStyle xim_style = 0;
static GSList *xic_all = NULL;

void obt_keyboard_reload(void)
{
    gint i, j, k;

    if (started) obt_keyboard_shutdown(); /* free stuff */
    started = TRUE;

    xim_init();

    /* reset the keys to not be bound to any masks */
    for (i = 0; i < OBT_KEYBOARD_NUM_MODKEYS; ++i)
        modkeys_keys[i] = 0;

    modmap = XGetModifierMapping(obt_display);
    /* note: modmap->max_keypermod can be 0 when there is no valid key layout
       available */

    XDisplayKeycodes(obt_display, &min_keycode, &max_keycode);
    keymap = XGetKeyboardMapping(obt_display, min_keycode,
                                 max_keycode - min_keycode + 1,
                                 &keysyms_per_keycode);

    alt_l = meta_l = super_l = hyper_l = FALSE;

    /* go through each of the modifier masks (eg ShiftMask, CapsMask...) */
    for (i = 0; i < NUM_MASKS; ++i) {
        /* go through each keycode that is bound to the mask */
        for (j = 0; j < modmap->max_keypermod; ++j) {
            KeySym sym;
            /* get a keycode that is bound to the mask (i) */
            KeyCode keycode = modmap->modifiermap[i*modmap->max_keypermod + j];
            if (keycode) {
                /* go through each keysym bound to the given keycode */
                for (k = 0; k < keysyms_per_keycode; ++k) {
                    sym = keymap[(keycode-min_keycode) * keysyms_per_keycode +
                                 k];
                    if (sym != NoSymbol) {
                        /* bind the key to the mask (e.g. Alt_L => Mod1Mask) */
                        set_modkey_mask(nth_mask(i), sym);
                    }
                }
            }
        }
    }

    /* CapsLock, Shift, and Control are special and hard-coded */
    modkeys_keys[OBT_KEYBOARD_MODKEY_CAPSLOCK] = LockMask;
    modkeys_keys[OBT_KEYBOARD_MODKEY_SHIFT] = ShiftMask;
    modkeys_keys[OBT_KEYBOARD_MODKEY_CONTROL] = ControlMask;
}

void obt_keyboard_shutdown(void)
{
    GSList *it;

    XFreeModifiermap(modmap);
    modmap = NULL;
    XFree(keymap);
    keymap = NULL;
    for (it = xic_all; it; it = g_slist_next(it)) {
        ObtIC* ic = it->data;
        if (ic->xic) {
            XDestroyIC(ic->xic);
            ic->xic = NULL;
        }
    }
    if (xim) XCloseIM(xim);
    xim = NULL;
    xim_style = 0;
    started = FALSE;
}

void xim_init(void)
{
    GSList *it;
    gchar *aname, *aclass;

    aname = g_strdup(g_get_prgname());
    if (!aname) aname = g_strdup("obt");

    aclass = g_strdup(aname);
    if (g_ascii_islower(aclass[0]))
        aclass[0] = g_ascii_toupper(aclass[0]);

    xim = XOpenIM(obt_display, NULL, aname, aclass);

    if (!xim)
        g_message("Failed to open an Input Method");
    else {
        XIMStyles *xim_styles = NULL;
        char *r;

        /* get the input method styles */
        r = XGetIMValues(xim, XNQueryInputStyle, &xim_styles, NULL);
        if (r || !xim_styles)
            g_message("Input Method does not support any styles");
        if (xim_styles) {
            int i;

            /* find a style that doesnt need preedit or status */
            for (i = 0; i < xim_styles->count_styles; ++i) {
                if (xim_styles->supported_styles[i] == 
                    (XIMPreeditNothing | XIMStatusNothing))
                {
                    xim_style = xim_styles->supported_styles[i];
                    break;
                }
            }
            XFree(xim_styles);
        }

        if (!xim_style) {
            g_message("Input Method does not support a usable style");

            XCloseIM(xim);
            xim = NULL;
        }
    }

    /* any existing contexts need to be recreated for the new input method */
    for (it = xic_all; it; it = g_slist_next(it))
        obt_keyboard_context_renew(it->data);

    g_free(aclass);
    g_free(aname);
}

guint obt_keyboard_keyevent_to_modmask(XEvent *e)
{
    gint i, masknum;

    g_return_val_if_fail(e->type == KeyPress || e->type == KeyRelease,
                         OBT_KEYBOARD_MODKEY_NONE);

    for (masknum = 0; masknum < NUM_MASKS; ++masknum)
        for (i = 0; i < modmap->max_keypermod; ++i) {
            KeyCode c = modmap->modifiermap[masknum*modmap->max_keypermod + i];
            if (c == e->xkey.keycode)
                return 1<<masknum;
        }
    return 0;
}

guint obt_keyboard_only_modmasks(guint mask)
{
    mask &= ALL_MASKS;
    /* strip off these lock keys. they shouldn't affect key bindings */
    mask &= ~LockMask; /* use the LockMask, not what capslock is bound to,
                          because you could bind it to something else and it
                          should work as that modifier then. i think capslock
                          is weird in xkb. */
    mask &= ~obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_NUMLOCK);
    mask &= ~obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SCROLLLOCK);
    return mask;
}

guint obt_keyboard_modkey_to_modmask(ObtModkeysKey key)
{
    return modkeys_keys[key];
}

static void set_modkey_mask(guchar mask, KeySym sym)
{
    /* find what key this is, and bind it to the mask */

    if (sym == XK_Num_Lock)
        modkeys_keys[OBT_KEYBOARD_MODKEY_NUMLOCK] |= mask;
    else if (sym == XK_Scroll_Lock)
        modkeys_keys[OBT_KEYBOARD_MODKEY_SCROLLLOCK] |= mask;

    else if (sym == XK_Super_L && super_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_SUPER] |= mask;
    else if (sym == XK_Super_L && !super_l)
        /* left takes precident over right, so erase any masks the right
           key may have set */
        modkeys_keys[OBT_KEYBOARD_MODKEY_SUPER] = mask, super_l = TRUE;
    else if (sym == XK_Super_R && !super_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_SUPER] |= mask;

    else if (sym == XK_Hyper_L && hyper_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_HYPER] |= mask;
    else if (sym == XK_Hyper_L && !hyper_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_HYPER] = mask, hyper_l = TRUE;
    else if (sym == XK_Hyper_R && !hyper_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_HYPER] |= mask;

    else if (sym == XK_Alt_L && alt_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_ALT] |= mask;
    else if (sym == XK_Alt_L && !alt_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_ALT] = mask, alt_l = TRUE;
    else if (sym == XK_Alt_R && !alt_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_ALT] |= mask;

    else if (sym == XK_Meta_L && meta_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_META] |= mask;
    else if (sym == XK_Meta_L && !meta_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_META] = mask, meta_l = TRUE;
    else if (sym == XK_Meta_R && !meta_l)
        modkeys_keys[OBT_KEYBOARD_MODKEY_META] |= mask;

    /* CapsLock, Shift, and Control are special and hard-coded */
}

KeyCode* obt_keyboard_keysym_to_keycode(KeySym sym)
{
    KeyCode *ret;
    gint i, j, n;

    ret = g_new(KeyCode, 1);
    n = 0;
    ret[n] = 0;

    /* go through each keycode and look for the keysym */
    for (i = min_keycode; i <= max_keycode; ++i)
        for (j = 0; j < keysyms_per_keycode; ++j)
            if (sym == keymap[(i-min_keycode) * keysyms_per_keycode + j]) {
                ret = g_renew(KeyCode, ret, ++n + 1);
                ret[n-1] = i;
                ret[n] = 0;
            }
    return ret;
}

gunichar obt_keyboard_keypress_to_unichar(ObtIC *ic, XEvent *ev)
{
    gunichar unikey = 0;
    KeySym sym;
    Status status;
    gchar *buf, fixbuf[4]; /* 4 is enough for a utf8 char */
    gint len, bufsz;
    gboolean got_string = FALSE;

    g_return_val_if_fail(ev->type == KeyPress, 0);

    if (!ic)
        g_warning("Using obt_keyboard_keypress_to_unichar() without an "
                  "Input Context.  No i18n support!");

    if (ic && ic->xic) {
        buf = fixbuf;
        bufsz = sizeof(fixbuf);

#ifdef X_HAVE_UTF8_STRING
        len = Xutf8LookupString(ic->xic, &ev->xkey, buf, bufsz, &sym, &status);
#else
        len = XmbLookupString(ic->xic, &ev->xkey, buf, bufsz, &sym, &status);
#endif

        if (status == XBufferOverflow) {
            buf = g_new(char, len);
            bufsz = len;

#ifdef X_HAVE_UTF8_STRING
            len = Xutf8LookupString(ic->xic, &ev->xkey, buf, bufsz, &sym,
                                    &status);
#else
            len = XmbLookupString(ic->xic, &ev->xkey, buf, bufsz, &sym,
                                  &status);
#endif
        }

        if ((status == XLookupChars || status == XLookupBoth)) {
            if ((guchar)buf[0] >= 32) { /* not an ascii control character */
#ifndef X_HAVE_UTF8_STRING
                /* convert to utf8 */
                gchar *buf2 = buf;
                buf = g_locale_to_utf8(buf2, len, NULL, NULL, NULL);
                g_free(buf2);
#endif

                got_string = TRUE;
            }
        }
        else if (status == XLookupKeySym)
            /* this key doesn't have a text representation, it is a command
               key of some sort */;
        else
            g_message("Bad keycode lookup. Keysym 0x%x Status: %s\n",
                      (guint) sym,
                      (status == XBufferOverflow ? "BufferOverflow" :
                       status == XLookupNone ? "XLookupNone" :
                       status == XLookupKeySym ? "XLookupKeySym" :
                       "Unknown status"));
    }
    else {
        buf = fixbuf;
        bufsz = sizeof(fixbuf);
        len = XLookupString(&ev->xkey, buf, bufsz, &sym, NULL);
        if ((guchar)buf[0] >= 32) /* not an ascii control character */
            got_string = TRUE;
    }

    if (got_string) {
        gunichar u = g_utf8_get_char_validated(buf, len);
        if (u && u != (gunichar)-1 && u != (gunichar)-2)
            unikey = u;
    }

    if (buf != fixbuf) g_free(buf);

    return unikey;
}

KeySym obt_keyboard_keypress_to_keysym(XEvent *ev)
{
    KeySym sym;

    g_return_val_if_fail(ev->type == KeyPress, None);

    sym = None;
    XLookupString(&ev->xkey, NULL, 0, &sym, NULL);
    return sym;
}

void obt_keyboard_context_renew(ObtIC *ic)
{
    if (xim) {
        ic->xic = XCreateIC(xim,
                            XNInputStyle, xim_style,
                            XNClientWindow, ic->client,
                            XNFocusWindow, ic->focus,
                            NULL);
        if (!ic->xic)
            g_message("Error creating Input Context for window 0x%x 0x%x\n",
                      (guint)ic->client, (guint)ic->focus);
    }
}

ObtIC* obt_keyboard_context_new(Window client, Window focus)
{
    ObtIC *ic;

    g_return_val_if_fail(client != None && focus != None, NULL);

    ic = g_slice_new(ObtIC);
    ic->ref = 1;
    ic->client = client;
    ic->focus = focus;
    ic->xic = NULL;

    obt_keyboard_context_renew(ic);
    xic_all = g_slist_prepend(xic_all, ic);

    return ic;
}

void obt_keyboard_context_ref(ObtIC *ic)
{
    ++ic->ref;
}

void obt_keyboard_context_unref(ObtIC *ic)
{
    if (--ic->ref < 1) {
        xic_all = g_slist_remove(xic_all, ic);
        if (ic->xic)
            XDestroyIC(ic->xic);
        g_slice_free(ObtIC, ic);
    }
}
