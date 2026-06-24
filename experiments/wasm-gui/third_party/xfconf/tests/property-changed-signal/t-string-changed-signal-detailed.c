/*
 *  xfconf
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#include <string.h>

typedef struct
{
    GMainLoop *mloop;
    gboolean got_signal;
} SignalTestData;

static void
test_signal_changed(XfconfChannel *channel,
                    const gchar *property,
                    const GValue *value,
                    gpointer user_data)
{
    SignalTestData *std = user_data;
    if(!strcmp(property, test_string_property))
        std->got_signal = TRUE;
    else
        std->got_signal = FALSE;
    g_main_loop_quit(std->mloop);
}

static gboolean
test_watchdog(gpointer data)
{
    SignalTestData *std = data;
    g_main_loop_quit(std->mloop);
    return FALSE;
}

int
main(int argc,
     char **argv)
{
    XfconfChannel *channel;
    SignalTestData std = { NULL, FALSE };
    gchar detailed_signal[512];
    
    std.mloop = g_main_loop_new(NULL, FALSE);

    if(!xfconf_tests_start())
        return 1;
    
    channel = xfconf_channel_new(TEST_CHANNEL_NAME);
    xfconf_channel_reset_property (channel, test_string_property, FALSE);
    xfconf_channel_reset_property (channel, test_int_property, FALSE);
    
    g_snprintf(detailed_signal, sizeof(detailed_signal),
               "property-changed::%s", test_string_property);
    g_signal_connect(G_OBJECT(channel), detailed_signal,
                     G_CALLBACK(test_signal_changed), &std);
    
    TEST_OPERATION(xfconf_channel_set_string(channel, test_string_property, test_string));
    TEST_OPERATION(xfconf_channel_set_int(channel, test_int_property, test_int));
    
    g_timeout_add(2000, test_watchdog, &std);
    g_main_loop_run(std.mloop);

    g_main_loop_unref(std.mloop);
    g_object_unref(G_OBJECT(channel));
    
    xfconf_tests_end();
    
    return std.got_signal ? 0 : 1;
}
