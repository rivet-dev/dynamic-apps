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
test_str_elide_underscores (void)
{
  gchar *res;

  res = exo_str_elide_underscores ("this_is_a_sample");
  g_assert_cmpstr (res, ==, "thisisasample");
  g_free (res);

  res = exo_str_elide_underscores ("m__nemonic");
  g_assert_cmpstr (res, ==, "m_nemonic");
  g_free (res);
}



static void
test_str_is_equal (void)
{
  const gchar *p = "cde";

  /* comparison that should return FALSE */
  g_assert (!exo_str_is_equal ("a", NULL));
  g_assert (!exo_str_is_equal (NULL, "b"));
  g_assert (!exo_str_is_equal ("a", "abcde"));
  g_assert (!exo_str_is_equal (p, "a"));

  /* comparison that should return TRUE */
  g_assert (exo_str_is_equal (NULL, NULL));
  g_assert (exo_str_is_equal ("test", "test"));
  g_assert (exo_str_is_equal (p, p));
}



static void
test_str_is_empty (void)
{
  const gchar *p;

  p = NULL;
  g_assert (exo_str_is_empty (p));

  p = "";
  g_assert (exo_str_is_empty (p));

  p = "a";
  g_assert (!exo_str_is_empty (p));
}



static void
test_str_replace (void)
{
  gchar       *res;
  const gchar *test = "You should eat fruits every day.";

  res = exo_str_replace (test, "fruits", "pizza");
  g_assert_cmpstr (res, ==, "You should eat pizza every day.");
  g_free (res);

  res = exo_str_replace (test, " fruits every day", NULL);
  g_assert_cmpstr (res, ==, "You should eat.");
  g_free (res);

  res = exo_str_replace (NULL, NULL, NULL);
  g_assert (res == NULL);

  res = exo_str_replace (test, NULL, NULL);
  g_assert_cmpstr (res, ==, test);
  g_free (res);
}



static void
test_strdup_strftime (void)
{
  /* TODO */
}



static void
test_strndupv (void)
{
  gchar **res, **input;
  guint   i;

  input = g_strsplit ("v,w,x,y,z", ",", -1);

  res = exo_strndupv (input, 2);
  g_assert_cmpuint (g_strv_length (res), ==, 2);
  for (i = 0; i < g_strv_length (res); i++)
    g_assert_cmpstr (res[i], ==, input[i]);
  g_strfreev (res);

  res = exo_strndupv (input, 500);
  g_assert_cmpuint (g_strv_length (res), ==, g_strv_length (input));
  for (i = 0; i < g_strv_length (res); i++)
    g_assert_cmpstr (res[i], ==, input[i]);
  g_strfreev (res);

  res = exo_strndupv (input, 0);
  g_assert (res == NULL);

  res = exo_strndupv (NULL, 2);
  g_assert (res == NULL);

  g_strfreev (input);
}



gint
main (gint    argc,
      gchar **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/string/test-str-elide-underscores", test_str_elide_underscores);
  g_test_add_func ("/string/test-str-is-equal", test_str_is_equal);
  g_test_add_func ("/string/test-str-is-empty", test_str_is_empty);
  g_test_add_func ("/string/test-str-replace", test_str_replace);
  g_test_add_func ("/string/test-strdup-strftime", test_strdup_strftime);
  g_test_add_func ("/string/test-strndupv", test_strndupv);

  return g_test_run ();
}
