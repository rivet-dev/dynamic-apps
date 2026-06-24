/* $Id$ */
/*-
 * Copyright (c) 2004-2005 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>


G_GNUC_NORETURN static void
usage (gboolean error)
{
  FILE *fp = error ? stderr : stdout;

  fprintf (fp,
           "Usage: xfce4-kiosk-query [OPTION...] <module> <capability>...\n"
           "\n"
           "Options:\n"
           "  -h          Print this help screen\n"
           "  -v          Show version information\n"
           "\n"
           "Queries the given capabilities of <module> for the current user\n"
           "and reports whether the user has the capabilities or not. This\n"
           "tool is mainly meant for system administrators to test their\n"
           "Kiosk setup.\n");

  exit (error ? EXIT_FAILURE : EXIT_SUCCESS);
}


int
main (int argc, char **argv)
{
  const gchar *module;
  XfceKiosk   *kiosk;
  int          ch;
  int          n;

  while ((ch = getopt (argc, argv, "hv")) != -1)
    {
      switch (ch)
        {
        case 'v':
          printf ("xfce4-kiosk-query %s (Xfce %s)\n\n"
                  "Copyright (c) 2003-2024\n"
                  "        The Xfce development team. All rights reserved.\n"
                  "Written for Xfce by Benedikt Meurer <benny@xfce.org>.\n\n"
                  "Please report bugs to <%s>.\n",
                  PACKAGE_VERSION, xfce_version_string (), PACKAGE_BUGREPORT);
          return EXIT_SUCCESS;

        case 'h':
        default:
          usage (ch != 'h');
        }
    }

  argc -= optind;
  argv += optind;

  if (argc < 2)
    usage (TRUE);

  module = argv[0];

  printf ("MODULE \"%s\"\n", module);

  kiosk = xfce_kiosk_new (module);

  for (n = 1; n < argc; ++n)
    {
      if (xfce_kiosk_query (kiosk, argv[n]))
        printf ("  CAP \"%s\" = ALLOWED\n", argv[n]);
      else
        printf ("  CAP \"%s\" = DENIED\n", argv[n]);
    }

  return EXIT_SUCCESS;
}


