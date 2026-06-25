/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-application.h>

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif



gint
main (gint argc, gchar **argv)
{
  MousepadApplication *application;
  gint                 status;

  /* bind the text domain to the locale directory */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  /* set the package textdomain */
  textdomain (GETTEXT_PACKAGE);

#ifdef G_ENABLE_DEBUG
  /* crash when something went wrong */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* create the application */
  application = g_object_new (MOUSEPAD_TYPE_APPLICATION,
                              "application-id", MOUSEPAD_ID,
                              "flags", G_APPLICATION_HANDLES_COMMAND_LINE
                                       | G_APPLICATION_HANDLES_OPEN,
                              NULL);

  /* run the application */
  status = g_application_run (G_APPLICATION (application), argc, argv);

  /* cleanup */
  g_object_unref (application);

  return status;
}
