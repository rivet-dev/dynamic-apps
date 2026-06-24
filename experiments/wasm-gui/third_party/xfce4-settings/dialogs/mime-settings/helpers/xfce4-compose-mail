#!/usr/bin/env python3
#
# xfce4-compose-mail - Python script to parse mailto:-URIs and invoke the
#                      various included MailReaders with the appropriate
#                      parameters.
#
# Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
# Copyright (c) 2013 Seth Vidal - Red Hat, Inc. <skvidal@fedoraproject.org>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License ONLY.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301 USA

import sys
import os
import urllib.parse
import shutil

# check number of arguments
if len(sys.argv) < 3:
    print("Usage: %s <style> <binary> <mailto>" % (sys.argv[0]), file=sys.stderr)
    sys.exit(1)

# determine arguments
style = sys.argv[1]
binary = sys.argv[2]
raw_mailto = sys.argv[3]

def parse_url(mailto):
    url = urllib.parse.urlparse(mailto)
    res = {}
    to = []
    query = None

    if url.query or '?' not in url.path:
        if url.path:
            to.append(urllib.parse.unquote(url.path))

    else:
        (thisto, ques, query) = url.path.partition('?')
        if thisto.strip():
            to.append(urllib.parse.unquote(thisto))

    if not query:
        query = url.query

    q_dict = urllib.parse.parse_qs(query)
    to.extend(q_dict.get('to', []))
    res['to'] = to
    if 'to' in q_dict:
        del(q_dict['to'])

    res.update(q_dict)
    return res

# mailto:-components
mailto = parse_url(raw_mailto)
to = mailto.get('to', [])
cc = mailto.get('cc', [])
bcc = mailto.get('bcc', [])
subject = ' '.join(mailto.get('subject', []))
body = '\n'.join(mailto.get('body', []))
attachments = mailto.get('attach',[])
attachments += mailto.get('attachment',[])

# check if binary is valid
binary_path = shutil.which(binary)
if not binary_path:
    print("%s: command not found or not executable: '%s'." %
            (sys.argv[0], binary), file=sys.stderr)
    sys.exit(1)

# start with only the binary name
args = [binary]

# add style-specific parameters
if style == 'mozilla':
    # similar to mozilla-remote, but with --compose
    command = "to='" + ','.join(to) + "'"
    command += ",cc='" + ','.join(cc) + "'"
    command += ",bcc='" + ','.join(bcc) + "'"
    command += ",attachment='" + ','.join(attachments) + "'"
    if subject:
         command += ",subject=" + urllib.parse.quote(subject, errors='replace')
    if body:
         command += ",body=" + urllib.parse.quote(body, errors='replace')

    args.append('-compose')
    args.append(command)

elif style == 'mozilla-remote':
    command = 'xfeDoCommand(composeMessage'
    command += ",to='" + ','.join(to) + "'"
    command += ",cc='" + ','.join(cc) + "'"
    command += ",bcc='" + ','.join(bcc) + "'"
    command += ",attachment='" + ','.join(attachments) + "'"
    if subject:
         command += ",subject=" + urllib.parse.quote(subject, errors='replace')
    if body:
         command += ",body=" + urllib.parse.quote(body, errors='replace')
    command += ')'

    args.append('-remote')
    args.append(command)

elif style in ['evolution', 'kmail', 'geary']:
     # these take a mailto uri and parse it themselves
     # just hand back what we were handed originally
     args.append(raw_mailto)

elif style == 'balsa':
    args.append('--compose')
    args.append(raw_mailto)

elif style == 'sylpheed':
    args.append('--compose')
    args.append(raw_mailto)
    # attachments must be passed separately
    if attachments and not args[0] == 'claws-mail':
        args.append('--attach')
        for item in attachments:
            args.append(item)

elif style == 'mutt':
    if subject:
        args.append('-s')
        args.append(subject)
    for item in cc:
        args.append('-c')
        args.append(item)
    for item in bcc:
        args.append('-b')
        args.append(item)
    if attachments:
        args.append('-a')
        for item in attachments:
            args.append(item)
    args.append('--')
    for item in to:
        args.append(item)
    if not to:
        args.append('')
    if body:
        args.append('mailto:?body=' + urllib.parse.quote(body, errors='replace'))

else:
   print("%s: Unsupported style '%s'." % (sys.argv[0], style), file=sys.stderr)
   sys.exit(1)

# try to execute the generated command
try:
    os.execv(binary_path, args)
except OSError as e:
    print('{}: Error: {}'.format(sys.argv[0], e), file=sys.stderr)
    print('Command was: {} {}'.format(binary_path, args), file=sys.stderr)

# something went wrong (this process was supposed to be replaced)
sys.exit(1)
