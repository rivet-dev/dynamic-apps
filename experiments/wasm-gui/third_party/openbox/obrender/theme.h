/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   theme.h for the Openbox window manager
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

#ifndef __theme_h
#define __theme_h

#include "render.h"

G_BEGIN_DECLS

typedef struct _RrTheme RrTheme;

struct _RrTheme {
    const RrInstance *inst;

    /* style settings - fonts */
    RrFont *win_font_focused;
    RrFont *win_font_unfocused;
    RrFont *menu_title_font;
    RrFont *menu_font;
    RrFont *osd_font_hilite;
    RrFont *osd_font_unhilite;

    /* style settings - geometry */
    gint paddingx;
    gint paddingy;
    gint handle_height;
    gint fbwidth; /*!< frame border width */
    gint mbwidth; /*!< menu border width */
    gint obwidth; /*!< osd border width */
    gint ubwidth; /*!< undecorated frame border width */
    gint cbwidthx;
    gint cbwidthy;
    gint menu_overlap_x;
    gint menu_overlap_y;
    gint menu_sep_width;
    gint menu_sep_paddingx;
    gint menu_sep_paddingy;
    /* these ones are calculated, not set directly by the theme file */
    gint win_font_height;
    gint menu_title_font_height;
    gint menu_font_height;
    gint label_height;
    gint title_height;
    gint button_size;
    gint grip_width;
    gint menu_title_label_height;
    gint menu_title_height;

    /* style settings - colors */
    RrColor *menu_border_color;
    RrColor *osd_border_color;
    RrColor *frame_focused_border_color;
    RrColor *frame_undecorated_focused_border_color;
    RrColor *frame_unfocused_border_color;
    RrColor *frame_undecorated_unfocused_border_color;
    RrColor *title_separator_focused_color;
    RrColor *title_separator_unfocused_color;
    RrColor *cb_focused_color;
    RrColor *cb_unfocused_color;
    RrColor *title_focused_color;
    RrColor *title_unfocused_color;
    RrColor *titlebut_focused_disabled_color;
    RrColor *titlebut_unfocused_disabled_color;
    RrColor *titlebut_focused_hover_color;
    RrColor *titlebut_unfocused_hover_color;
    RrColor *titlebut_focused_hover_toggled_color;
    RrColor *titlebut_unfocused_hover_toggled_color;
    RrColor *titlebut_focused_pressed_toggled_color;
    RrColor *titlebut_unfocused_pressed_toggled_color;
    RrColor *titlebut_focused_unpressed_toggled_color;
    RrColor *titlebut_unfocused_unpressed_toggled_color;
    RrColor *titlebut_focused_pressed_color;
    RrColor *titlebut_unfocused_pressed_color;
    RrColor *titlebut_focused_unpressed_color;
    RrColor *titlebut_unfocused_unpressed_color;
    RrColor *menu_title_color;
    RrColor *menu_sep_color;
    RrColor *menu_color;
    RrColor *menu_bullet_color;
    RrColor *menu_bullet_selected_color;
    RrColor *menu_selected_color;
    RrColor *menu_disabled_color;
    RrColor *menu_disabled_selected_color;
    RrColor *title_focused_shadow_color;
    gchar    title_focused_shadow_alpha;
    RrColor *title_unfocused_shadow_color;
    gchar    title_unfocused_shadow_alpha;
    RrColor *osd_text_active_color;
    RrColor *osd_text_inactive_color;
    RrColor *osd_text_active_shadow_color;
    RrColor *osd_text_inactive_shadow_color;
    gchar    osd_text_active_shadow_alpha;
    gchar    osd_text_inactive_shadow_alpha;
    RrColor *osd_pressed_color;
    RrColor *osd_unpressed_color;
    RrColor *osd_focused_color;
    RrColor *osd_pressed_lineart;
    RrColor *osd_focused_lineart;
    RrColor *menu_title_shadow_color;
    RrColor *menu_text_shadow_color;

    /* style settings - pics */
    RrPixel32 *def_win_icon; /* RGBA */
    gint       def_win_icon_w;
    gint       def_win_icon_h;

    /* style settings - masks */
    RrPixmapMask *menu_bullet_mask; /* submenu pointer */
#if 0
    RrPixmapMask *menu_toggle_mask; /* menu boolean */
#endif

    RrPixmapMask *down_arrow_mask;
    RrPixmapMask *up_arrow_mask;

    /* buttons */
    RrButton *btn_max;
    RrButton *btn_close;
    RrButton *btn_desk;
    RrButton *btn_shade;
    RrButton *btn_iconify;

    /* global appearances */
    RrAppearance *a_focused_grip;
    RrAppearance *a_unfocused_grip;
    RrAppearance *a_focused_title;
    RrAppearance *a_unfocused_title;
    RrAppearance *a_focused_label;
    RrAppearance *a_unfocused_label;
    /* always parentrelative, so no focused/unfocused */
    RrAppearance *a_icon;
    RrAppearance *a_focused_handle;
    RrAppearance *a_unfocused_handle;
    RrAppearance *a_menu_text_title;
    RrAppearance *a_menu_title;
    RrAppearance *a_menu;
    RrAppearance *a_menu_normal;
    RrAppearance *a_menu_selected;
    RrAppearance *a_menu_disabled;
    RrAppearance *a_menu_disabled_selected;
    RrAppearance *a_menu_text_normal;
    RrAppearance *a_menu_text_disabled;
    RrAppearance *a_menu_text_disabled_selected;
    RrAppearance *a_menu_text_selected;
    RrAppearance *a_menu_bullet_normal;
    RrAppearance *a_menu_bullet_selected;
    RrAppearance *a_clear;     /* clear with no texture */
    RrAppearance *a_clear_tex; /* clear with a texture */

    RrAppearance *osd_bg; /* can never be parent relative */
    RrAppearance *osd_hilite_bg; /* can never be parent relative */
    RrAppearance *osd_hilite_label; /* can be parent relative */
    RrAppearance *osd_unhilite_bg; /* can never be parent relative */
    RrAppearance *osd_unhilite_label; /* can be parent relative */
    RrAppearance *osd_pressed_button;
    RrAppearance *osd_unpressed_button;
    RrAppearance *osd_focused_button;

    gchar *name;
};

/*! The font values are all optional. If a NULL is used for any of them, then
  the default font will be used. */
RrTheme* RrThemeNew(const RrInstance *inst, const gchar *theme,
                    gboolean allow_fallback,
                    RrFont *active_window_font, RrFont *inactive_window_font,
                    RrFont *menu_title_font, RrFont *menu_item_font,
                    RrFont *active_osd_font, RrFont *inactive_osd_font);
void RrThemeFree(RrTheme *theme);

G_END_DECLS

#endif
