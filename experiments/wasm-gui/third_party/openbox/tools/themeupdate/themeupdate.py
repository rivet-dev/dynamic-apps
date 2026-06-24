#! /usr/bin/python

# themeupdate.py for the Openbox window manager
# This utility is for updating old themes from Blackbox, Fluxbox, and Openbox2
# to Openbox3
#
# Copyright (c) 2003-2007        Dana Jansens
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# See the COPYING file for a copy of the GNU General Public License.

import sys

data = []
valid = True

def out(str):
    sys.stderr.write(str)
    sys.stderr.flush()

def read_bool():
    while True:
        inp = sys.stdin.readline(1).strip()
        if inp == 'y' or inp == '': return True
        if inp == 'n': return False

def getkeyval(line):
    key = line[:line.find(':')].strip()
    value = line[line.find(':') + 1:].strip()
    if not (key and value):
        key = value = None
    return key, value

def find_key(data, keysubstr, exact = False):
    i = 0
    n = len(data)
    while i < n:
        l = data[i]
        key, value = getkeyval(l)
        if key and value:
            if (exact and key == keysubstr) or \
               (not exact and key.find(keysubstr) != -1):
                return i, key, value
        i += 1
    return -1, None, None

def simple_replace(data):
    pairs = {}
    pairs['.picColor'] = '.imageColor'
    pairs['menu.frame'] = 'menu.items'
    pairs['menu.hilite'] = 'menu.selected'
    pairs['borderColor'] = 'border.color'
    pairs['imageColor'] = 'image.color'
    pairs['textColor'] = 'text.color'
    pairs['interlaceColor'] = 'interlace.color'

    for k in pairs.keys():
        while 1:
            i, key, nul = find_key(data, k);
            if i >= 0:
                newl = data[i].replace(k, pairs[k])
                out('Updating "' + key +
                    '" to "' + key.replace(k, pairs[k]) + '"\n')
                data[i] = newl
            else:
                break

    pairs = {}
    pairs['window.focus.font'] = 'window.active.label.text.font'
    pairs['window.unfocus.font'] = 'window.inactive.label.text.font'
    pairs['window.justify'] = 'window.label.justify'
    pairs['menu.frame.disableColor'] = 'menu.disabled.textColor'
    pairs['window.label.focus.font'] = 'window.active.label.text.font'
    pairs['window.label.unfocus.font'] = 'window.inactive.label.text.font'
    pairs['window.label.justify'] = 'window.label.text.justify'
    pairs['menu.title.font'] = 'menu.title.text.font'
    pairs['menu.title.justify'] = 'menu.title.text.justify'
    pairs['menuOverlap'] = 'menu.overlap'
    pairs['handleWidth'] = 'window.handle.width'
    pairs['borderWidth'] = 'border.width'
    pairs['bevelWidth'] = 'padding.width'
    pairs['frameWidth'] = 'window.client.padding.width'
    pairs['window.frame.focusColor'] = 'window.active.client.color'
    pairs['window.frame.unfocusColor'] = 'window.inactive.client.color'
    pairs['window.title.focus'] = 'window.active.title.bg'
    pairs['window.title.unfocus'] = 'window.inactive.title.bg'
    pairs['window.label.focus'] = 'window.active.label.bg'
    pairs['window.label.unfocus'] = 'window.inactive.label.bg'
    pairs['window.handle.focus'] = 'window.active.handle.bg'
    pairs['window.handle.unfocus'] = 'window.inactive.handle.bg'
    pairs['window.grip.focus'] = 'window.active.grip.bg'
    pairs['window.grip.unfocus'] = 'window.inactive.grip.bg'
    pairs['menu.items'] = 'menu.items.bg'
    pairs['menu.title'] = 'menu.title.bg'
    pairs['menu.selected'] = 'menu.items.active.bg'
    pairs['window.title.focus'] = 'window.active.title.bg'
    pairs['window.label.focus'] = 'window.active.label.bg'
    pairs['window.title.unfocus'] = 'window.inactive.title.bg'
    pairs['window.label.unfocus'] = 'window.inactive.label.bg'
    pairs['window.button.disabled.focus'] = 'window.active.button.disabled.bg'
    pairs['window.button.disabled.unfocus'] = \
        'window.inactive.button.disabled.bg'
    pairs['window.button.pressed.focus'] = 'window.active.button.pressed.bg'
    pairs['window.button.pressed.unfocus'] = \
        'window.inactive.button.pressed.bg'
    pairs['window.button.toggled.focus'] = 'window.active.button.toggled.bg'
    pairs['window.button.toggled.unfocus'] = \
        'window.inactive.button.toggled.bg'
    pairs['window.button.focus'] = 'window.active.button.unpressed.bg'
    pairs['window.button.unfocus'] = 'window.inactive.button.unpressed.bg'
    pairs['window.button.hover.focus'] = 'window.active.button.hover.bg'
    pairs['window.button.hover.unfocus'] = 'window.inactive.button.hover.bg'

    for k in pairs.keys():
        while 1:
            i, key, nul = find_key(data, k, True);
            if i >= 0:
                newl = data[i].replace(k, pairs[k])
                out('Updating "' + key +
                    '" to "' + key.replace(k, pairs[k]) + '"\n')
                data[i] = newl
            else:
                break

    pairs = {}
    pairs['window.title.focus'] = 'window.active.title'
    pairs['window.title.unfocus'] = 'window.inactive.title'
    pairs['window.label.focus'] = 'window.active.label'
    pairs['window.label.unfocus'] = 'window.inactive.label'
    pairs['window.handle.focus'] = 'window.active.handle'
    pairs['window.handle.unfocus'] = 'window.inactive.handle'
    pairs['window.grip.focus'] = 'window.active.grip'
    pairs['window.grip.unfocus'] = 'window.inactive.grip'
    pairs['menu.selected'] = 'menu.items.active'
    pairs['window.title.focus'] = 'window.active.title'
    pairs['window.label.focus'] = 'window.active.label'
    pairs['window.title.unfocus'] = 'window.inactive.title'
    pairs['window.label.unfocus'] = 'window.inactive.label'
    pairs['window.button.disabled.focus'] = 'window.active.button.disabled'
    pairs['window.button.disabled.unfocus'] = \
        'window.inactive.button.disabled'
    pairs['window.button.pressed.focus'] = 'window.active.button.pressed'
    pairs['window.button.pressed.unfocus'] = \
        'window.inactive.button.pressed'
    pairs['window.button.toggled.focus'] = 'window.active.button.toggled'
    pairs['window.button.toggled.unfocus'] = \
        'window.inactive.button.toggled'
    pairs['window.button.focus'] = 'window.active.button'
    pairs['window.button.unfocus'] = 'window.inactive.button'
    pairs['window.button.hover.focus'] = 'window.active.button.hover'
    pairs['window.button.hover.unfocus'] = 'window.inactive.button.hover'
    pairs['window.label.unfocus'] = 'window.inactive.label'
    pairs['window.label.focus'] = 'window.active.label'
    pairs['window.button.focus'] = 'window.active.button.unpressed'
    pairs['menu.disabled'] = 'menu.items.disabled'
    pairs['menu.selected'] = 'menu.items.active'
    pairs['window.button.unfocus'] = 'window.inactive.button.unpressed'
    pairs['window.button.pressed.focus'] = 'window.active.button.pressed'
    pairs['window.button.pressed.unfocus'] = 'window.inactive.button.pressed'
    pairs['window.button.disabled.focus'] = 'window.active.button.disabled'
    pairs['window.button.disabled.unfocus'] = 'window.inactive.button.disabled'
    pairs['window.button.hover.focus'] = 'window.active.button.hover'
    pairs['window.button.hover.unfocus'] = 'window.inactive.button.hover'
    pairs['window.button.toggled.focus'] = 'window.active.button.toggled'
    pairs['window.button.toggled.unfocus'] = 'window.inactive.button.toggled'

    for k in pairs.keys():
        while 1:
            i, key, nul = find_key(data, k);
            if i >= 0:
                newl = data[i].replace(k, pairs[k])
                out('Updating "' + key +
                    '" to "' + key.replace(k, pairs[k]) + '"\n')
                data[i] = newl
            else:
                break

def replace_colors(data):
    i = 0
    n = len(data)
    while i < n:
        l = data[i]
        key, value = getkeyval(l)
        if key and value:
            if key.find('.color') != -1:
                if key.find('client.color') == -1 \
                       and key.find('image.color') == -1 \
                       and key.find('bg.color') == -1 \
                       and key.find('border.color') == -1 \
                       and key.find('interlace.color') == -1 \
                       and key.find('text.color') == -1:
                    newl = data[i].replace('.color', '.bg.color')
                    out('Updating "' + key +
                        '" to "' + key.replace('.color', '.bg.color') + '"\n')
                    data[i] = newl
            if key.find('.border.color') != -1 \
               and key.find('bg.border.color') == -1:
                newl = data[i].replace('.border.color', '.bg.border.color')
                out('Updating "' + key +
                    '" to "' + key.replace('.border.color',
                                           '.bg.border.color') + '"\n')
                data[i] = newl
            if key.find('.interlace.color') != -1 \
               and key.find('bg.interlace.color') == -1:
                newl = data[i].replace('.interlace.color',
                                       '.bg.interlace.color')
                out('Updating "' + key +
                    '" to "' + key.replace('.interlace.color',
                                           '.bg.interlace.color') + '"\n')
                data[i] = newl
        i += 1

def remove(data):
    invalid = []
    invalid.append('toolbar')
    for inv in invalid:
        while 1:
            i, key, nul = find_key(data, inv)
            if i >= 0:
                out(key + ' is no longer supported.\nRemove (Y/n)? ')
                if read_bool():
                    out('Removing "' + key + '"\n')
                    data.pop(i)
            else:
                break
    invalid.append('rootCommand')
    invalid.append('menu.bullet')
    invalid.append('menu.bullet.image.color')
    invalid.append('menu.bullet.selected.image.color')
    invalid.append('menu.frame.justify')
    for inv in invalid:
        while 1:
            i, key, nul = find_key(data, inv, True)
            if i >= 0:
                out(key + ' is no longer supported.\nRemove (Y/n)? ')
                if read_bool():
                    out('Removing "' + key + '"\n')
                    data.pop(i)
            else:
                break

def pressed(data):
    i, nul, nul = find_key(data, 'window.button.pressed', True)
    if i >= 0:
        out('The window.button.pressed option has been replaced by ' +
            'window.button.pressed.focus and ' +
            'window.button.pressed.unfocus.\nUpdate (Y/n)? ')
        if read_bool():
            l = data[i]
            out('Removing "window.button.pressed"\n')
            data.pop(i)
            out('Adding "window.button.pressed.unfocus"\n')
            data.insert(i, l.replace('window.button.pressed',
                                             'window.button.pressed.unfocus'))
            out('Adding "window.button.pressed.focus"\n')
            data.insert(i, l.replace('window.button.pressed',
                                     'window.button.pressed.focus'))

def x_fonts(data):
    i, nul, nul = find_key(data, 'window.font')
    if i >= 0:
        out('You appear to specify fonts using the old X fonts ' +
            'syntax.\nShall I remove all fonts from the theme (Y/n)? ')
        if not read_bool():
            return
    else: return
    while 1:
        i, key, nul = find_key(data, '.font')
        if i < 0:
            break
        out('Removing "' + key + '"\n')
        data.pop(i)

def xft_fonts(data):
    i, nul, nul = find_key(data, '.xft.')
    if i >= 0:
        out('You appear to specify fonts using the old Xft fonts ' +
            'syntax.\nShall I update these to the new syntax (Y/n)? ')
        if not read_bool():
            return
    else: return
    fonts = {}
    fonts['window'] = 'window.label.focus.font'
    fonts['menu.items'] = 'menu.items.font'
    fonts['menu.title'] = 'menu.title.font'
    for f in fonts.keys():
        li, nul, flags = find_key(data, f + '.xft.flags')
        if li < 0:
            li, nul, flags = find_key(data, '*.xft.flags')
        else:
            out('Removing ' + f + '.xft.flags\n')
            data.pop(li)
        oi, nul, offset = find_key(data, f + '.xft.shadow.offset')
        if oi < 0:
            oi, nul, offset = find_key(data, '*.xft.shadow.offset')
        else:
            out('Removing ' + f + '.xft.shadow.offset\n')
            data.pop(oi)
        ti, nul, tint = find_key(data, f + '.xft.shadow.tint')
        if ti < 0:
            ti, nul, tint = find_key(data, '*.xft.shadow.tint')
        else:
            out('Removing ' + f + '.xft.shadow.tint\n')
            data.pop(ti)
        fi, nul, face = find_key(data, f + '.xft.font')
        if fi < 0:
            fi, nul, face = find_key(data, '*.xft.font')
            if fi >= 0: fi = len(data) - 1
        else:
            out('Removing ' + f + '.xft.font\n')
            data.pop(fi)

        if fi >= 0:
            s = face
            if li >= 0:
                if flags.find('bold'):
                    s = s + ':bold'
                if flags.find('shadow'):
                    s = s + ':shadow=y'
            if oi >= 0:
                s = s + ':shadowoffset=' + offset
            if ti >= 0:
                s = s + ':shadowtint=' + tint
        out('Adding ' + fonts[f] + '\n')
        data.insert(fi, fonts[f] + ': ' + s)

    for stars in ('*.xft.flags', '*.xft.shadow.offset' ,
                  '*.xft.shadow.tint', '*.xft.font'):
        i, key, nul = find_key(data, stars)
        if i >= 0:
            out('Removing ' + key + '\n')
            data.pop(i)

def pixelsize(data):
    fonts = ('window.label.focus.font',
             'menu.items.font',
             'menu.title.font')
    for f in fonts:
        i, key, value = find_key(data, f, True)
        if value:
            if value.find('pixelsize') == -1:
                out('*** ERROR *** The ' + key + ' font size is not being '
                    'specified by pixelsize. It is recommended that you use '
                    'pixelsize instead of pointsize for specifying theme '
                    'fonts. e.g. "sans:pixelsize=12"\n')
                global valid
                valid = False

def warn_missing(data):
    need = ('window.active.button.hover',  'window.inactive.button.hover',
            'menu.overlap')
    for n in need:
        i, nul, nul = find_key(data, n)
        if i < 0:
            out('The ' + n + ' value was not found in the theme, but it '
                'can optionally be set.\n')

def err_missing(data):
    need = ('window.active.button.disabled',
            'window.inactive.button.disabled',
            'window.active.client.color',
            'window.inactive.client.color')
    for n in need:
        i, nul, nul = find_key(data, n)
        if i < 0:
            out('*** ERROR *** The ' + n + ' value was not found in the '
                'theme, but it is required to be set.\n')
            global valid
            valid = False


def usage():
    out('Usage: themupdate.py /path/to/themerc > newthemerc\n\n')
    sys.exit()

try:
    file = open(sys.argv[1])
except IndexError:
    usage()
except IOError:
    out('Unable to open file "' + sys.argv[1] + '"\n\n')
    usage()

data = file.readlines()
for i in range(len(data)):
    data[i] = data[i].strip()

simple_replace(data)
replace_colors(data)
remove(data)
pressed(data)
x_fonts(data)
xft_fonts(data)
pixelsize(data)
warn_missing(data)
err_missing(data)

for l in data:
    print l

sys.exit(not valid)
