#!/usr/bin/python

# @file tests/test-gir.py Unit test: Test GI repository
#
# @Copyright (C) 2010 Canonical Ltd.
# Author: Martin Pitt <martin.pitt@ubuntu.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA  02111-1307, USA.

import os

# use our local typelib
os.environ['GI_TYPELIB_PATH'] = 'libnotify:' + os.environ.get('GI_TYPELIB_PATH', '')

from gi.repository import Notify

assert Notify.is_initted() == False
Notify.init('test')
assert Notify.is_initted() == True

print 'server info:', Notify.get_server_info()
print 'server capabilities:', Notify.get_server_caps()

n = Notify.Notification.new('title', None, None)
n.show()
n = Notify.Notification.new('title', 'text', None)
n.show()
n = Notify.Notification.new('title', 'text', 'gtk-ok')
n.show()

n.update('New Title', None, None)
n.show()
n.update('Newer Title', 'New Body', None)
n.show()

def callback():
    print 'Do it! Callback'

n = Notify.Notification.new('Action', 'Here we go!', 'gtk-alert')
n.add_action('doit', 'Do it!', callback, None, None)
n.show()

