/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2007-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <garcon/garcon-menu-element.h>
#include <garcon/garcon-menu-separator.h>



static void         garcon_menu_separator_element_init                    (GarconMenuElementIface   *iface);
static void         garcon_menu_separator_finalize                        (GObject                  *object);

static const gchar *garcon_menu_separator_get_element_name                (GarconMenuElement        *element);
static const gchar *garcon_menu_separator_get_element_comment             (GarconMenuElement        *element);
static const gchar *garcon_menu_separator_get_element_icon_name           (GarconMenuElement        *element);
static gboolean     garcon_menu_separator_get_element_visible             (GarconMenuElement        *element);
static gboolean     garcon_menu_separator_get_element_show_in_environment (GarconMenuElement        *element);
static gboolean     garcon_menu_separator_get_element_no_display          (GarconMenuElement        *element);
static gboolean     garcon_menu_separator_get_element_equal               (GarconMenuElement        *element,
                                                                           GarconMenuElement        *other);



G_DEFINE_TYPE_WITH_CODE (GarconMenuSeparator, garcon_menu_separator, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GARCON_TYPE_MENU_ELEMENT, garcon_menu_separator_element_init))



static void
garcon_menu_separator_class_init (GarconMenuSeparatorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_separator_finalize;
}



static void
garcon_menu_separator_element_init (GarconMenuElementIface *iface)
{
  iface->get_name = garcon_menu_separator_get_element_name;
  iface->get_comment = garcon_menu_separator_get_element_comment;
  iface->get_icon_name = garcon_menu_separator_get_element_icon_name;
  iface->get_visible = garcon_menu_separator_get_element_visible;
  iface->get_show_in_environment = garcon_menu_separator_get_element_show_in_environment;
  iface->get_no_display = garcon_menu_separator_get_element_no_display;
  iface->equal = garcon_menu_separator_get_element_equal;
}



static void
garcon_menu_separator_init (GarconMenuSeparator *separator)
{
}



static void
garcon_menu_separator_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (garcon_menu_separator_parent_class)->finalize) (object);
}



/**
 * garcon_menu_separator_get_default: (constructor)
 *
 * Returns the default #GarconMenuSeparator. The returned object
 * should be unreffed with g_object_unref() when no longer needed.
 *
 * Returns: (transfer full): the default #GarconMenuSeparator.
 */
GarconMenuSeparator*
garcon_menu_separator_get_default (void)
{
  static GarconMenuSeparator *separator = NULL;

  if (G_UNLIKELY (separator == NULL))
    {
      /* Create a new separator */
      separator = g_object_new (GARCON_TYPE_MENU_SEPARATOR, NULL);
      g_object_add_weak_pointer (G_OBJECT (separator), (gpointer *) &separator);
    }
  else
    {
      /* Take a reference */
      g_object_ref (G_OBJECT (separator));
    }

  return separator;
}



static const gchar*
garcon_menu_separator_get_element_name (GarconMenuElement *element)
{
  return NULL;
}



static const gchar*
garcon_menu_separator_get_element_comment (GarconMenuElement *element)
{
  return NULL;
}



static const gchar*
garcon_menu_separator_get_element_icon_name (GarconMenuElement *element)
{
  return NULL;
}



static gboolean
garcon_menu_separator_get_element_visible (GarconMenuElement *element)
{
  return TRUE;
}



static gboolean
garcon_menu_separator_get_element_show_in_environment (GarconMenuElement *element)
{
  return TRUE;
}



static gboolean
garcon_menu_separator_get_element_no_display (GarconMenuElement *element)
{
  return FALSE;
}



static gboolean
garcon_menu_separator_get_element_equal (GarconMenuElement *element,
                                         GarconMenuElement *other)
{
  /* FIXME this is inherently broken as the separator is a singleton class */
  return FALSE;
}
