/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <libxfce4panel/xfce-panel-macros.h>



static void
panel_properties_store_value (XfconfChannel *channel,
                              const gchar   *xfconf_property,
                              GType          xfconf_property_type,
                              GObject       *object,
                              const gchar   *object_property)
{
  GValue       value = { 0, };
  GdkRGBA     *rgba;
#ifndef NDEBUG
  GParamSpec *pspec;
#endif

  panel_return_if_fail (G_IS_OBJECT (object));
  panel_return_if_fail (XFCONF_IS_CHANNEL (channel));

#ifndef NDEBUG
  /* check if the types match */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), object_property);
  panel_assert (pspec != NULL);
  if (G_PARAM_SPEC_VALUE_TYPE (pspec) != xfconf_property_type)
    {
      g_critical ("Object and Xfconf properties don't match! %s::%s. %s != %s",
                  G_OBJECT_TYPE_NAME (object), xfconf_property,
                  g_type_name (xfconf_property_type),
                  g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
    }
#endif

  /* write the property to the xfconf channel */
  g_value_init (&value, xfconf_property_type);
  g_object_get_property (G_OBJECT (object), object_property, &value);

  if (G_LIKELY (xfconf_property_type != GDK_TYPE_RGBA))
    {
        xfconf_channel_set_property (channel, xfconf_property, &value);
    }
  else
    {
      /* work around xfconf's lack of storing colors (bug #7117) and
       * do the same as xfconf_g_property_bind_gdkcolor() does */
      rgba = g_value_get_boxed (&value);
      xfconf_channel_set_array (channel, xfconf_property,
                                G_TYPE_DOUBLE, &rgba->red,
                                G_TYPE_DOUBLE, &rgba->green,
                                G_TYPE_DOUBLE, &rgba->blue,
                                G_TYPE_DOUBLE, &rgba->alpha,
                                G_TYPE_INVALID);
    }

  g_value_unset (&value);
}



XfconfChannel *
panel_properties_get_channel (GObject *object_for_weak_ref)
{
  GError        *error = NULL;
  XfconfChannel *channel;

  panel_return_val_if_fail (G_IS_OBJECT (object_for_weak_ref), NULL);

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialize Xfconf: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  channel = xfconf_channel_get (XFCE_PANEL_CHANNEL_NAME);
  g_object_weak_ref (object_for_weak_ref, (GWeakNotify) xfconf_shutdown, NULL);

  return channel;
}



void
panel_properties_bind (XfconfChannel       *channel,
                       GObject             *object,
                       const gchar         *property_base,
                       const PanelProperty *properties,
                       gboolean             save_properties)
{
  const PanelProperty *prop;
  gchar               *property;

  panel_return_if_fail (channel == NULL || XFCONF_IS_CHANNEL (channel));
  panel_return_if_fail (G_IS_OBJECT (object));
  panel_return_if_fail (property_base != NULL && *property_base == '/');
  panel_return_if_fail (properties != NULL);

  if (G_LIKELY (channel == NULL))
    channel = panel_properties_get_channel (object);
  panel_return_if_fail (XFCONF_IS_CHANNEL (channel));

  /* walk the properties array */
  for (prop = properties; prop->property != NULL; prop++)
    {
      property = g_strconcat (property_base, "/", prop->property, NULL);

      if (save_properties)
        panel_properties_store_value (channel, property, prop->type, object, prop->property);

      if (G_LIKELY (prop->type != GDK_TYPE_RGBA))
        xfconf_g_property_bind (channel, property, prop->type, object, prop->property);
      else
        xfconf_g_property_bind_gdkrgba (channel, property, object, prop->property);

      g_free (property);
    }
}



void
panel_properties_unbind (GObject *object)
{
  xfconf_g_property_unbind_all (object);
}
