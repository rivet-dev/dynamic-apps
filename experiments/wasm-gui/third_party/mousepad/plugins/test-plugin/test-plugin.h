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

#ifndef __TEST_PLUGIN_H__
#define __TEST_PLUGIN_H__

#include <mousepad/mousepad-plugin.h>

G_BEGIN_DECLS

#define TEST_TYPE_PLUGIN (test_plugin_get_type ())
G_DECLARE_FINAL_TYPE (TestPlugin, test_plugin, TEST, PLUGIN, MousepadPlugin)

void test_plugin_register (MousepadPluginProvider *plugin);

G_END_DECLS

#endif /* !__TEST_PLUGIN_H__ */
