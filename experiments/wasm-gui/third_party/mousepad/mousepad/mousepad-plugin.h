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

#ifndef __MOUSEPAD_PLUGIN_H__
#define __MOUSEPAD_PLUGIN_H__

#include <mousepad/mousepad-plugin-provider.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_PLUGIN (mousepad_plugin_get_type ())
G_DECLARE_DERIVABLE_TYPE (MousepadPlugin, mousepad_plugin, MOUSEPAD, PLUGIN, GObject)

struct _MousepadPluginClass
{
  GObjectClass __parent__;

  /* virtual functions */
  void (*enable)  (MousepadPlugin *plugin);
  void (*disable) (MousepadPlugin *plugin);

  /* reserved space for the addition of future virtual functions */
  gpointer padding[12];
};

G_END_DECLS

#endif /* !__MOUSEPAD_PLUGIN_H__ */
