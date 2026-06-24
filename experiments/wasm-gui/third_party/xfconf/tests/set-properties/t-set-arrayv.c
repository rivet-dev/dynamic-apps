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

int
main(int argc,
     char **argv)
{
    XfconfChannel *channel;
    
    if(!xfconf_tests_start())
        return 1;
    
    channel = xfconf_channel_new(TEST_CHANNEL_NAME);
    
    {
        GPtrArray *arr = g_ptr_array_sized_new(3);
        GValue *val;
        
        val = g_new0(GValue, 1);
        g_value_init(val, G_TYPE_BOOLEAN);
        g_value_set_boolean(val, TRUE);
        g_ptr_array_add(arr, val);
        
        val = g_new0(GValue, 1);
        g_value_init(val, G_TYPE_INT64);
        g_value_set_int64(val, 5000000000LL);
        g_ptr_array_add(arr, val);
        
        val = g_new0(GValue, 1);
        g_value_init(val, G_TYPE_STRING);
        g_value_set_static_string(val, "test string");
        g_ptr_array_add(arr, val);
        
        TEST_OPERATION(xfconf_channel_set_arrayv(channel, test_array_property,
                                                 arr));
        
        xfconf_array_free(arr);
    }
    
    g_object_unref(G_OBJECT(channel));
    
    xfconf_tests_end();
    
    return 0;
}
