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

#include <test-plugin/test-plugin.h>

#include <gmodule.h>


G_MODULE_EXPORT void                mousepad_plugin_initialize (MousepadPluginProvider *provider);
G_MODULE_EXPORT MousepadPluginData *mousepad_plugin_get_data   (void);



static MousepadPluginData plugin_data;



void
mousepad_plugin_initialize (MousepadPluginProvider *provider)
{
  static GType types[2] = { G_TYPE_INVALID, G_TYPE_INVALID };

  /* register the types provided by this plugin */
  test_plugin_register (provider);

  /* set up plugin data */
  types[0] = TEST_TYPE_PLUGIN;
  plugin_data.types = types;
  plugin_data.destroyable = TRUE;
  plugin_data.label = "Mousepad Test";
  plugin_data.tooltip = NULL;
  plugin_data.category = "Technical";
  plugin_data.accel = NULL;
}



MousepadPluginData *
mousepad_plugin_get_data (void)
{
  return &plugin_data;
}
