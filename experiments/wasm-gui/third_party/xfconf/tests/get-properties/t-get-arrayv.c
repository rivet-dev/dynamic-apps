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
        GPtrArray *arr = xfconf_channel_get_arrayv(channel, test_array_property);
        GValue *val;
        
        if(!arr) {
            g_critical("Test failed: returned array is NULL");
            xfconf_tests_end();
            return 1;
        }
        
        if(arr->len != 3) {
            g_critical("Test failed: array len should be %d, is %d", 3, arr->len);
            xfconf_tests_end();
            return 1;
        }
        
        val = g_ptr_array_index(arr, 0);
        if(G_VALUE_TYPE(val) != G_TYPE_BOOLEAN) {
            g_critical("Test failed: array elem 0 is not G_TYPE_BOOLEAN");
            xfconf_tests_end();
            return 1;
        }
        if(g_value_get_boolean(val) != TRUE) {
            g_critical("Test failed: array elem 0 value is not TRUE");
            xfconf_tests_end();
            return 1;
        }
        
        val = g_ptr_array_index(arr, 1);
        if(G_VALUE_TYPE(val) != G_TYPE_INT64) {
            g_critical("Test failed: array elem 1 is not G_TYPE_INT64");
            xfconf_tests_end();
            return 1;
        }
        if(g_value_get_int64(val) != 5000000000LL) {
            g_critical("Test failed: array elem 1 value is not 5000000000");
            xfconf_tests_end();
            return 1;
        }
        
        val = g_ptr_array_index(arr, 2);
        if(G_VALUE_TYPE(val) != G_TYPE_STRING) {
            g_critical("Test failed: array elem 2 is not G_TYPE_STRING");
            xfconf_tests_end();
            return 1;
        }
        if(strcmp(g_value_get_string(val), "test string")) {
            g_critical("Test failed: array elem 2 value should be \"test string\", but it's \"%s\"",
                       g_value_get_string(val));
            xfconf_tests_end();
            return 1;
        }
        
        xfconf_array_free(arr);
    }
    
    g_object_unref(G_OBJECT(channel));
    
    xfconf_tests_end();
    
    return 0;
}
