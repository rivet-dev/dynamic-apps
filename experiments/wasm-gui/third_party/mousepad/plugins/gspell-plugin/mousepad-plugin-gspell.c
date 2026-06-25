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

#include <gspell-plugin/gspell-plugin.h>

#include <gmodule.h>


G_MODULE_EXPORT void                mousepad_plugin_initialize (MousepadPluginProvider *provider);
G_MODULE_EXPORT MousepadPluginData *mousepad_plugin_get_data   (void);



static MousepadPluginData plugin_data;



void
mousepad_plugin_initialize (MousepadPluginProvider *provider)
{
  static GType types[2] = { G_TYPE_INVALID, G_TYPE_INVALID };

  /* register the types provided by this plugin */
  gspell_plugin_register (provider);

  /* set up plugin data */
  types[0] = GSPELL_TYPE_PLUGIN;
  plugin_data.types = types;
  plugin_data.destroyable = FALSE;
  plugin_data.label = _("Spell Checking");
  plugin_data.tooltip =
    _("The default language for new documents is set here. It can then be changed per"
      " document via the context menu, where there are also spelling correction suggestions"
      " for underlined words.");
  plugin_data.category = _("Editor");
  plugin_data.accel = "<Control>K";
}



MousepadPluginData *
mousepad_plugin_get_data (void)
{
  return &plugin_data;
}
