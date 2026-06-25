/*
 * Copyright (C) 2006-2007 Jasper Huijsmans <jasper@xfce.org>
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */



#ifndef __LIBXFCE4PANEL_ENUMS_H__
#define __LIBXFCE4PANEL_ENUMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION: enums
 * @title: Standard Enumerations
 * @short_description: Standard enumerations used by the Xfce Panel.
 * @include: libxfce4panel/libxfce4panel.h
 *
 * Currently only contains the definition of #XfceScreenPosition.
 **/

/**
 * XfcePanelPluginMode
 * @XFCE_PANEL_PLUGIN_MODE_HORIZONTAL : Horizontal panel and plugins
 * @XFCE_PANEL_PLUGIN_MODE_VERTICAL   : Vertical rotated panel and plugins
 * @XFCE_PANEL_PLUGIN_MODE_DESKBAR    : Vertical panel with horizontal plugins
 *
 * Orientation of the plugin in the panel.
 *
 * Since: 4.10
 **/
typedef enum /*<enum,prefix=XFCE_PANEL_PLUGIN_MODE >*/
{
  XFCE_PANEL_PLUGIN_MODE_HORIZONTAL,
  XFCE_PANEL_PLUGIN_MODE_VERTICAL,
  XFCE_PANEL_PLUGIN_MODE_DESKBAR
}
XfcePanelPluginMode;

/**
 * XfceScreenPosition
 * @XFCE_SCREEN_POSITION_NONE       : No position has been set.
 * @XFCE_SCREEN_POSITION_NW_H       : North West Horizontal
 * @XFCE_SCREEN_POSITION_N          : North
 * @XFCE_SCREEN_POSITION_NE_H       : North East Horizontal
 * @XFCE_SCREEN_POSITION_NW_V       : North West Vertical
 * @XFCE_SCREEN_POSITION_W          : West
 * @XFCE_SCREEN_POSITION_SW_V       : South West Vertical
 * @XFCE_SCREEN_POSITION_NE_V       : North East Vertical
 * @XFCE_SCREEN_POSITION_E          : East
 * @XFCE_SCREEN_POSITION_SE_V       : South East Vertical
 * @XFCE_SCREEN_POSITION_SW_H       : South West Horizontal
 * @XFCE_SCREEN_POSITION_S          : South
 * @XFCE_SCREEN_POSITION_SE_H       : South East Horizontal
 * @XFCE_SCREEN_POSITION_FLOATING_H : Floating Horizontal
 * @XFCE_SCREEN_POSITION_FLOATING_V : Floating Vertical
 *
 * There are three screen positions for each side of the screen:
 * LEFT/TOP, CENTER and RIGHT/BOTTOM. The XfceScreenPosition is expressed
 * as navigational direction, with possible addition of H or V to denote
 * horizontal and vertical orientation. Additionally there are two floating
 * positions, horizontal and vertical.
 **/
typedef enum /*<enum,prefix=XFCE_SCREEN_POSITION >*/
{
    XFCE_SCREEN_POSITION_NONE,

    /* top */
    XFCE_SCREEN_POSITION_NW_H,          /* North West Horizontal */
    XFCE_SCREEN_POSITION_N,             /* North                 */
    XFCE_SCREEN_POSITION_NE_H,          /* North East Horizontal */

    /* left */
    XFCE_SCREEN_POSITION_NW_V,          /* North West Vertical   */
    XFCE_SCREEN_POSITION_W,             /* West                  */
    XFCE_SCREEN_POSITION_SW_V,          /* South West Vertical   */

    /* right */
    XFCE_SCREEN_POSITION_NE_V,          /* North East Vertical   */
    XFCE_SCREEN_POSITION_E,             /* East                  */
    XFCE_SCREEN_POSITION_SE_V,          /* South East Vertical   */

    /* bottom */
    XFCE_SCREEN_POSITION_SW_H,          /* South West Horizontal */
    XFCE_SCREEN_POSITION_S,             /* South                 */
    XFCE_SCREEN_POSITION_SE_H,          /* South East Horizontal */

    /* floating */
    XFCE_SCREEN_POSITION_FLOATING_H,    /* Floating Horizontal */
    XFCE_SCREEN_POSITION_FLOATING_V     /* Floating Vertical */
}
XfceScreenPosition;

/**
 * xfce_screen_position_is_horizontal:
 * @position : the #XfceScreenPosition
 *
 * Whether the current #XfceScreenPosition is horizontal.
 *
 * Returns: %TRUE if horizontal, %FALSE otherwise
 **/
#define xfce_screen_position_is_horizontal(position)   \
    (position <= XFCE_SCREEN_POSITION_NE_H ||          \
     (position >= XFCE_SCREEN_POSITION_SW_H &&         \
      position <= XFCE_SCREEN_POSITION_FLOATING_H))

/**
 * xfce_screen_position_get_orientation:
 * @position : the #XfceScreenPosition
 *
 * Converts the current #XfceScreenPosition into a #GtkOrientation.
 *
 * Returns: the #GtkOrientation corresponding to @position.
 **/
#define xfce_screen_position_get_orientation(position) \
    (xfce_screen_position_is_horizontal (position) ? \
        GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL)

/**
 * xfce_screen_position_is_floating:
 * @position : the #XfceScreenPosition
 *
 * Whether the current #XfceScreenPosition is floating on the screen.
 *
 * Returns: %TRUE if floating, %FALSE otherwise.
 **/
#define xfce_screen_position_is_floating(position) \
    (position >= XFCE_SCREEN_POSITION_FLOATING_H || \
     position == XFCE_SCREEN_POSITION_NONE)

/**
 * xfce_screen_position_is_top:
 * @position : the #XfceScreenPosition
 *
 * Whether the current #XfceScreenPosition is above of the center of
 * the screen.
 *
 * Returns: %TRUE if on the top of the screen, %FALSE otherwise
 **/
#define xfce_screen_position_is_top(position) \
    (position >= XFCE_SCREEN_POSITION_NW_H && \
     position <= XFCE_SCREEN_POSITION_NE_H)

/**
 * xfce_screen_position_is_left:
 * @position : the #XfceScreenPosition
 *
 * Whether the current #XfceScreenPosition is left of the center of
 * the screen.
 *
 * Returns: %TRUE if on the left of the screen, %FALSE otherwise
 **/
#define xfce_screen_position_is_left(position) \
    (position >= XFCE_SCREEN_POSITION_NW_V && \
     position <= XFCE_SCREEN_POSITION_SW_V)

/**
 * xfce_screen_position_is_right:
 * @position : the #XfceScreenPosition
 *
 * Whether the current #XfceScreenPosition is right of the center of
 * the screen.
 *
 * Returns: %TRUE if on the right of the screen, %FALSE otherwise
 **/
#define xfce_screen_position_is_right(position) \
    (position >= XFCE_SCREEN_POSITION_NE_V && \
     position <= XFCE_SCREEN_POSITION_SE_V)

/**
 * xfce_screen_position_is_bottom:
 * @position : the #XfceScreenPosition
 *
 * Whether the current #XfceScreenPosition is below of the center of
 * the screen.
 *
 * Returns: %TRUE if on the bottom of the screen, %FALSE otherwise
 **/
#define xfce_screen_position_is_bottom(position) \
    (position >= XFCE_SCREEN_POSITION_SW_H && \
     position <= XFCE_SCREEN_POSITION_SE_H)

G_END_DECLS

#endif /* !__LIBXFCE4PANEL_ENUMS_H__ */
