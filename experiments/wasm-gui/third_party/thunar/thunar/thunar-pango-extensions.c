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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <thunar/thunar-pango-extensions.h>



static PangoAttrList *thunar_pango_attr_list_wrap (PangoAttribute *attribute, ...) G_GNUC_MALLOC;



static PangoAttrList*
thunar_pango_attr_list_wrap (PangoAttribute *attribute, ...)
{
  PangoAttrList *attr_list;
  va_list        args;

  /* allocate a new attribute list */
  attr_list = pango_attr_list_new ();

  /* add all specified attributes */
  va_start (args, attribute);
  while (attribute != NULL)
    {
      attribute->start_index = 0;
      attribute->end_index = -1;
      pango_attr_list_insert (attr_list, attribute);
      attribute = va_arg (args, PangoAttribute *);
    }
  va_end (args);

  return attr_list;
}



/**
 * thunar_pango_attr_list_big:
 *
 * Returns a #PangoAttrList for rendering big text.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for rendering big text.
 **/
PangoAttrList*
thunar_pango_attr_list_big (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_scale_new (PANGO_SCALE_LARGE), NULL);
  return attr_list;
}



/**
 * thunar_pango_attr_list_big_bold:
 *
 * Returns a #PangoAttrList for rendering big bold text.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for rendering big bold text.
 **/
PangoAttrList*
thunar_pango_attr_list_big_bold (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_scale_new (PANGO_SCALE_LARGE), pango_attr_weight_new (PANGO_WEIGHT_BOLD), NULL);
  return attr_list;
}



/**
 * thunar_pango_attr_list_bold:
 *
 * Returns a #PangoAttrList for rendering bold text.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for rendering bold text.
 **/
PangoAttrList*
thunar_pango_attr_list_bold (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_weight_new (PANGO_WEIGHT_BOLD), NULL);
  return attr_list;
}



/**
 * thunar_pango_attr_disable_hyphens:
 *
 * Returns a #PangoAttrList for not inserting hyphens at intra-word line breaks.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for not inserting hyphens at intra-word line
 *               breaks.
 **/
#if PANGO_VERSION_CHECK (1, 44, 0)
PangoAttrList*
thunar_pango_attr_disable_hyphens (void)
{
  static PangoAttrList *attr_list = NULL;

  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_insert_hyphens_new (FALSE), NULL);

  return attr_list;
}
#endif



/**
 * thunar_pango_attr_list_italic:
 *
 * Returns a #PangoAttrList for rendering italic text.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for rendering italic text.
 **/
PangoAttrList*
thunar_pango_attr_list_italic (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_style_new (PANGO_STYLE_ITALIC), NULL);
  return attr_list;
}



/**
 * thunar_pango_attr_list_small_italic:
 *
 * Returns a #PangoAttrList for rendering small italic text.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for rendering small italic text.
 **/
PangoAttrList*
thunar_pango_attr_list_small_italic (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_scale_new (PANGO_SCALE_SMALL), pango_attr_style_new (PANGO_STYLE_ITALIC), NULL);
  return attr_list;
}



/**
 * thunar_pango_attr_list_small:
 *
 * Returns a #PangoAttrList for rendering small text.
 * The returned list is owned by the callee and must
 * not be freed or modified by the caller.
 *
 * Return value: a #PangoAttrList for rendering small text.
 **/
PangoAttrList*
thunar_pango_attr_list_small (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_scale_new (PANGO_SCALE_SMALL), NULL);
  return attr_list;
}



/**
 * thunar_pango_attr_list_underline_single:
 *
 * Returns a #PangoAttrList for underlining text using a single line.
 * The returned list is owned by the callee and must not be freed
 * or modified by the caller.
 *
 * Return value: a #PangoAttrList for underlining text using a single line.
 **/
PangoAttrList*
thunar_pango_attr_list_underline_single (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = thunar_pango_attr_list_wrap (pango_attr_underline_new (PANGO_UNDERLINE_SINGLE), NULL);
  return attr_list;
}
