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

# ------------------------------------------------------------ #
# Simple example of how to communicate with Thunar using the   #
# org.xfce.Thunar D-BUS interface.                             #
#                                                              #
# Thunar must be compiled with D-BUS support for this to work. #
# ------------------------------------------------------------ #

import gtk
import dbus
import dbus.service
if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
  import dbus.glib

# acquire a reference to the Thunar object
bus = dbus.SessionBus()
thunar_object = bus.get_object('org.xfce.Thunar', '/org/xfce/FileManager')
thunar = dbus.Interface(thunar_object, 'org.xfce.Thunar')

# You can now invoke methods on the thunar object, for
# example, to terminate a running Thunar instance (just
# like Thunar -q), you can use:
#
# thunar.Terminate()
#
# or, if you want to open the bulk rename dialog in the
# standalone version with an empty file list and /tmp
# as default folder for the "Add Files" dialog, use:
#
# thunar.BulkRename('/tmp', [], True, '', '')
#
# See the thunar-dbus-service-infos.xml file for the exact
# interface definition.
#

# We just popup the bulk rename dialog to
# demonstrate that it works. ;-)
thunar.BulkRename('/tmp', [], True, '', '')
