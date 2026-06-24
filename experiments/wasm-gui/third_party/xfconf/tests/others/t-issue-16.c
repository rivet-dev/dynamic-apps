/*
 *  xfconf
 *
 *  Copyright (c) 2020 cryptogopher
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
main(int argc, char **argv)
{
  XfconfChannel *channel;

  if(!xfconf_tests_start())
    return 1;

  /* Generate at least one asynchronous set_property call to trigger
   * asynchronous callback execution. Because removal of the channel directly
   * follows and callback will only be invoked after control returns to main
   * loop - callback will be executed with dangling pointer to
   * XfconfCacheOldItem. */
  channel = xfconf_channel_new(TEST_CHANNEL_NAME);
  xfconf_channel_reset_property (channel, test_int_property, FALSE);
  TEST_OPERATION(xfconf_channel_set_int(channel, test_int_property, test_int));
  g_object_unref(G_OBJECT(channel));

  while (g_main_context_pending(NULL))
    g_main_context_iteration(NULL, FALSE);

  xfconf_tests_end();

  return 0;
}
