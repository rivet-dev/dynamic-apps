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

# --------------------------------------------------------------------------- #
# Simple example of how to communicate with Thunar (and any other Xfce file   #
# file manager) using the org.xfce.FileManager D-BUS interface.               #
#                                                                             #
# Thunar must be compiled with D-BUS support for this to work.                #
# --------------------------------------------------------------------------- #

import gtk
import dbus
import dbus.service
if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
  import dbus.glib

# acquire a reference to the FileManager object
bus = dbus.SessionBus()
xfce_file_manager_object = bus.get_object('org.xfce.FileManager', '/org/xfce/FileManager')
xfce_file_manager = dbus.Interface(xfce_file_manager_object, 'org.xfce.FileManager')

# You can now invoke methods on the xfce_file_manager object,
# for example, to open a new file manager window for /tmp, you
# would use the following method:
#
# xfce_file_manager.DisplayFolder('/tmp', '', '')
#
# or, if you also want to pre-select a file in the folder, you
# can use:
#
# xfce_file_manager.DisplayFolderAndSelect('/tmp', 'file.txt', '', '')
#
# else if you want to display the file managers preferences
# dialog, you'd use
#
# xfce_file_manager.DisplayPreferencesDialog('', '')
#
# and to open the file properties dialog of a given file, use
#
# xfce_file_manager.DisplayFileProperties('/path/to/file', '', '')
#
# and last but not least, to launch a given file (or open
# a folder), use
#
# xfce_file_manager.Launch('/path/to/file', '', '')
#
# See the thunar-dbus-service-infos.xml file for the exact
# interface definition.
#

# We just popup a new window for the root directory here to
# demonstrate that it works. ;-)
xfce_file_manager.Launch('/', '', '')
