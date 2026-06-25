/*
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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-plugin.h>
#include <mousepad/mousepad-settings.h>



/* GObject virtual functions */
static void mousepad_plugin_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec);
static void mousepad_plugin_get_property (GObject      *object,
                                          guint         prop_id,
                                          GValue       *value,
                                          GParamSpec   *pspec);
static void mousepad_plugin_constructed  (GObject      *object);



typedef struct _MousepadPluginPrivate
{
  GObject __parent__;

  /* the plugin provider */
  MousepadPluginProvider *provider;

  /* the plugin state */
  gboolean enabled;
} MousepadPluginPrivate;

/* MousepadPlugin properties */
enum
{
  PROP_PROVIDER = 1,
  N_PROPERTIES
};



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MousepadPlugin, mousepad_plugin, G_TYPE_OBJECT)



static void
mousepad_plugin_class_init (MousepadPluginClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = mousepad_plugin_set_property;
  gobject_class->get_property = mousepad_plugin_get_property;
  gobject_class->constructed = mousepad_plugin_constructed;

  g_object_class_install_property (gobject_class, PROP_PROVIDER,
    g_param_spec_object ("provider", "Provider", "The plugin provider",
                         MOUSEPAD_TYPE_PLUGIN_PROVIDER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}



static void
mousepad_plugin_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MousepadPluginPrivate *priv = mousepad_plugin_get_instance_private (MOUSEPAD_PLUGIN (object));

  switch (prop_id)
    {
    case PROP_PROVIDER:
      priv->provider = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_plugin_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MousepadPluginPrivate *priv = mousepad_plugin_get_instance_private (MOUSEPAD_PLUGIN (object));

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_take_object (value, priv->provider);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_plugin_init (MousepadPlugin *plugin)
{
  MousepadPluginPrivate *priv = mousepad_plugin_get_instance_private (plugin);

  priv->provider = NULL;
  priv->enabled = TRUE;
}



static void
mousepad_plugin_state_changed (MousepadPlugin *plugin)
{
  MousepadPluginPrivate  *priv = mousepad_plugin_get_instance_private (plugin);
  gchar                 **plugins;
  gboolean                contained;

  /* get the list of enabled plugins */
  plugins = MOUSEPAD_SETTING_GET_STRV (ENABLED_PLUGINS);

  /* check if this plugin is in the list */
  contained = g_strv_contains ((const gchar *const *) plugins,
                               G_TYPE_MODULE (priv->provider)->name);

  if (! priv->enabled && contained)
    {
      priv->enabled = TRUE;
      MOUSEPAD_PLUGIN_GET_CLASS (plugin)->enable (plugin);
    }
  else if (priv->enabled && ! contained)
    {
      priv->enabled = FALSE;
      MOUSEPAD_PLUGIN_GET_CLASS (plugin)->disable (plugin);
    }

  /* cleanup */
  g_strfreev (plugins);
}



static void
mousepad_plugin_constructed (GObject *object)
{
  MousepadPluginPrivate *priv = mousepad_plugin_get_instance_private (MOUSEPAD_PLUGIN (object));

  /* if the plugin isn't destroyed when disabled, bind to gsettings to keep its state in sync */
  if (! mousepad_plugin_provider_is_destroyable (priv->provider))
    MOUSEPAD_SETTING_CONNECT_OBJECT (ENABLED_PLUGINS, mousepad_plugin_state_changed,
                                     object, G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (mousepad_plugin_parent_class)->constructed (object);
}
