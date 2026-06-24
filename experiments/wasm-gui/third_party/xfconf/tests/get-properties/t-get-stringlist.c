/*
 *  xfconf
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "tests-common.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

int
main(int argc,
     char **argv)
{
    XfconfChannel *channel;
    
    if(!xfconf_tests_start())
        return 1;
    
    channel = xfconf_channel_new(TEST_CHANNEL_NAME);
    
    {
        gchar **strlist = xfconf_channel_get_string_list(channel,
                                                         test_strlist_property);
        gint i;
        
        if(!strlist) {
            g_critical("Test failed: xfconf_channel_get_string_list() returned NULL");
            xfconf_tests_end();
            return 1;
        }
        
        for(i = 0; strlist[i] && test_strlist[i]; ++i) {
            if(strcmp(strlist[i], test_strlist[i])) {
                g_critical("Test failed: string list values don't match (%s != %s)",
                           strlist[i], test_strlist[i]);
                xfconf_tests_end();
                return 1;
            }
        }
        
        if(strlist[i] || test_strlist[i]) {
            g_critical("Test failed: in string list, element %d should be NULL (0x%p, 0x%p)",
                       i, strlist[i], test_strlist[i]);
            xfconf_tests_end();
            return 1;
        }
        
        g_strfreev(strlist);
    }
    
    g_object_unref(G_OBJECT(channel));
    
    xfconf_tests_end();
    
    return 0;
}
