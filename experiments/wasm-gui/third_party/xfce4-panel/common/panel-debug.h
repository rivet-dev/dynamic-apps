/*
 * Copyright (C) 2010 Nick Schermer <nick@xfce.org>
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

#ifndef __PANEL_DEBUG_H__
#define __PANEL_DEBUG_H__

#define PANEL_DEBUG_BOOL(bool) ((bool) ? "true" : "false")

typedef enum
{
  PANEL_DEBUG_YES              = 1 << 0, /* always enabled if PANEL_DEBUG is not %NULL */

  /* external plugin proxy modes */
  PANEL_DEBUG_GDB              = 1 << 1, /* run external plugins in gdb */
  PANEL_DEBUG_VALGRIND         = 1 << 2, /* run external plugins in valgrind */

  /* filter domains */
  PANEL_DEBUG_APPLICATION      = 1 << 3,
  PANEL_DEBUG_APPLICATIONSMENU = 1 << 4,
  PANEL_DEBUG_BASE_WINDOW      = 1 << 5,
  PANEL_DEBUG_DISPLAY_LAYOUT   = 1 << 6,
  PANEL_DEBUG_EXTERNAL         = 1 << 7,
  PANEL_DEBUG_MAIN             = 1 << 8,
  PANEL_DEBUG_MODULE           = 1 << 9,
  PANEL_DEBUG_MODULE_FACTORY   = 1 << 10,
  PANEL_DEBUG_POSITIONING      = 1 << 11,
  PANEL_DEBUG_STRUTS           = 1 << 12,
  PANEL_DEBUG_SYSTRAY          = 1 << 13,
  PANEL_DEBUG_TASKLIST         = 1 << 14,
  PANEL_DEBUG_PAGER            = 1 << 15,
  PANEL_DEBUG_ITEMBAR          = 1 << 16,
  PANEL_DEBUG_CLOCK            = 1 << 17,
}
PanelDebugFlag;

gboolean panel_debug_has_domain   (PanelDebugFlag  domain);

void     panel_debug              (PanelDebugFlag  domain,
                                   const gchar    *message,
                                   ...) G_GNUC_PRINTF (2, 3);

void     panel_debug_filtered     (PanelDebugFlag  domain,
                                   const gchar    *message,
                                   ...) G_GNUC_PRINTF (2, 3);

#endif /* !__PANEL_DEBUG_H__ */
