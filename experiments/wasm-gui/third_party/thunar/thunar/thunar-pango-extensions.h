/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_PANGO_EXTENSIONS_H__
#define __THUNAR_PANGO_EXTENSIONS_H__

#include <pango/pango.h>

G_BEGIN_DECLS;

PangoAttrList *thunar_pango_attr_list_big               (void) G_GNUC_CONST;
PangoAttrList *thunar_pango_attr_list_big_bold          (void) G_GNUC_CONST;
PangoAttrList *thunar_pango_attr_list_bold              (void) G_GNUC_CONST;
#if PANGO_VERSION_CHECK (1, 44, 0)
PangoAttrList *thunar_pango_attr_disable_hyphens        (void) G_GNUC_CONST;
#endif
PangoAttrList *thunar_pango_attr_list_italic            (void) G_GNUC_CONST;
PangoAttrList *thunar_pango_attr_list_small_italic      (void) G_GNUC_CONST;
PangoAttrList *thunar_pango_attr_list_small             (void) G_GNUC_CONST;
PangoAttrList *thunar_pango_attr_list_underline_single  (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_PANGO_EXTENSIONS_H__ */
