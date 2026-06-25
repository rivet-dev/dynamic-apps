#!/usr/bin/env python
#
# Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA
#

# --------------------------------------------------------------- #
# This example demonstrates how to integrate Thunar's Bulk Rename #
# functionality into Nautilus. You'll need the nautilus-python    #
# bindings to actually try this example.                          #
#                                                                 #
# Just copy this file to                                          #
#                                                                 #
#   ~/.nautilus/python-extensions/                                #
#                                                                 #
# creating the directory if it does not already exist, and re-    #
# start Nautilus. Afterwards you'll see an action labeled "Bulk   #
# Rename" in the file context menu, which can be used to rename   #
# the selected files.                                             #
#                                                                 #
# Thunar must be compiled with D-BUS support for this to work.    #
# --------------------------------------------------------------- #


import gtk
import nautilus

import dbus
import dbus.service
if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
  import dbus.glib

class NautilusBulkRename(nautilus.MenuProvider):
  def __init__(self):
    bus = dbus.SessionBus()
    thunar_object = bus.get_object('org.xfce.Thunar', '/org/xfce/FileManager')
    self.thunar = dbus.Interface(thunar_object, 'org.xfce.Thunar')

  def menu_activate_cb(self, item, files):
    uris = []
    for file in files:
      uris.append(file.get_uri())
    self.thunar.BulkRename(files[0].get_parent_uri(), uris, False, item.get_data('display'))

  def get_file_items(self, window, files):
    if len(files) == 0:
      return

    item = nautilus.MenuItem('NautilusBulkRename::bulk-rename', 'Bulk Rename', 'Rename all selected files at once')
    item.set_data('display', window.get_screen().make_display_name())
    item.connect('activate', self.menu_activate_cb, files)
    return item,
