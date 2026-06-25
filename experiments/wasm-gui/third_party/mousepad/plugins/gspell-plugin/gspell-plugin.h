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

#ifndef __GSPELL_PLUGIN_H__
#define __GSPELL_PLUGIN_H__

#include <mousepad/mousepad-plugin.h>

G_BEGIN_DECLS

#define GSPELL_TYPE_PLUGIN (gspell_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GspellPlugin, gspell_plugin, GSPELL, PLUGIN, MousepadPlugin)

void gspell_plugin_register (MousepadPluginProvider *plugin);

G_END_DECLS

#endif /* !__GSPELL_PLUGIN_H__ */
