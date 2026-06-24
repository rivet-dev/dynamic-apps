/*
 * Copyright (c) 2009 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <exo/exo.h>



static void
test_noop_one (void)
{
  g_assert (exo_noop_one () == 1);
}



static void
test_noop_zero (void)
{
  g_assert (exo_noop_zero () == 0);
}



static void
test_noop_null (void)
{
  g_assert (exo_noop_null () == NULL);
}



static void
test_noop_true (void)
{
  g_assert (exo_noop_true () == TRUE);
}



static void
test_noop_false (void)
{
  g_assert (exo_noop_false () == FALSE);
}



gint
main (gint    argc,
      gchar **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/noop/test-noop-one", test_noop_one);
  g_test_add_func ("/noop/test-noop-zero", test_noop_zero);
  g_test_add_func ("/noop/test-noop-null", test_noop_null);
  g_test_add_func ("/noop/test-noop-true", test_noop_true);
  g_test_add_func ("/noop/test-noop-false", test_noop_false);

  return g_test_run ();
}
