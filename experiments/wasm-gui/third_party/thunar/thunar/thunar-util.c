/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef G_PLATFORM_WIN32
#include <direct.h>
#include <glib/gwin32.h>
#endif

#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-folder.h>
#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-icon-renderer.h>

#include <glib.h>
#include <glib/gstdio.h>

#define BORDER_RADIUS 8

enum
{
  THUNAR_CELL_TOP_RIGHT,
  THUNAR_CELL_BOTTOM_RIGHT,
  THUNAR_CELL_TOP_LEFT,
  THUNAR_CELL_BOTTOM_LEFT,
};

enum
{
  DRAW_ON_LEFT,
  DRAW_ON_RIGHT,
  DRAW_ON_BOTTOM,
  DRAW_ON_TOP,
  DRAW_ON_ALL_SIDES,
} DrawOnSide;

const char *SEARCH_PREFIX = "Search: ";

/**
 * thunar_util_strrchr_offset:
 * @str:    haystack
 * @offset: pointer offset in @str
 * @c:      search needle
 *
 * Return the last occurrence of the character @c in
 * the string @str starting at @offset.
 *
 * There are also Glib functions for this like g_strrstr_len
 * and g_utf8_strrchr, but these work internally the same
 * as this function (tho, less efficient).
 *
 * Return value: pointer in @str or NULL.
 **/
static inline gchar *
thunar_util_strrchr_offset (const gchar *str,
                            const gchar *offset,
                            gchar        c)
{
  const gchar *p;

  for (p = offset; p > str; p--)
    if (*p == c)
      return (gchar *) p;

  return NULL;
}



/**
 * thunar_util_str_get_extension
 * @filename : an UTF-8 filename
 *
 * Returns a pointer to the extension in @filename.
 *
 * This is an improved version of g_utf8_strrchr with
 * improvements to recognize compound extensions like
 * ".tar.gz" and ".desktop.in.in".
 *
 * Return value: pointer to the extension in @filename
 *               or NULL.
**/
gchar *
thunar_util_str_get_extension (const gchar *filename)
{
  static const gchar *compressed[] = { "gz", "bz2", "lzma", "lrz", "rpm", "lzo", "xz", "z" };
  gchar              *dot;
  gchar              *ext;
  guint               i;
  gchar              *dot2;
  gsize               len;
  gboolean            is_in;

  /* check if there is an possible extension part in the name */
  dot = strrchr (filename, '.');
  if (dot == NULL
      || dot == filename
      || dot[1] == '\0')
    return NULL;

  /* skip the . */
  ext = dot + 1;

  /* check if this looks like a compression mime-type */
  for (i = 0; i < G_N_ELEMENTS (compressed); i++)
    {
      if (strcasecmp (ext, compressed[i]) == 0)
        {
          /* look for a possible container part (tar, psd, epsf) */
          dot2 = thunar_util_strrchr_offset (filename, dot - 1, '.');
          if (dot2 != NULL
              && dot2 != filename)
            {
              /* check the 2nd part range, keep it between 2 and 5 chars */
              len = dot - dot2 - 1;
              if (len >= 2 && len <= 5)
                dot = dot2;
            }

          /* that's it for compression types */
          return dot;
        }
    }

  /* for coders, .in are quite common, so check for those too
   * with a max of 3 rounds (2x .in and the possibly final extension) */
  if (strcasecmp (ext, "in") == 0)
    {
      for (i = 0, is_in = TRUE; is_in && i < 3; i++)
        {
          dot2 = thunar_util_strrchr_offset (filename, dot - 1, '.');
          /* the extension before .in could be long. check that it's at least 2 chars */
          len = dot - dot2 - 1;
          if (dot2 == NULL
              || dot2 == filename
              || len < 2)
            break;

          /* continue if another .in was found */
          is_in = dot - dot2 == 3 && strncasecmp (dot2, ".in", 3) == 0;

          dot = dot2;
        }
    }

  return dot;
}



void
thunar_util_load_bookmarks (GFile               *bookmarks_file,
                            ThunarBookmarksFunc  foreach_func,
                            gpointer             user_data)
{
  gchar       *bookmarks_path;
  gchar        line[1024];
  const gchar *name;
  gchar       *space;
  FILE        *fp;
  gint         row_num = 1;
  GFile       *file;

  _thunar_return_if_fail (G_IS_FILE (bookmarks_file));
  _thunar_return_if_fail (g_file_is_native (bookmarks_file));
  _thunar_return_if_fail (foreach_func != NULL);

  /* determine the path to the GTK+ bookmarks file */
  bookmarks_path = g_file_get_path (bookmarks_file);

  /* append the GTK+ bookmarks (if any) */
  fp = fopen (bookmarks_path, "r");
  g_free (bookmarks_path);

  if (G_UNLIKELY (fp == NULL))
    {
      bookmarks_path = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);
      fp = fopen(bookmarks_path, "r");
      g_free(bookmarks_path);
    }

  if (G_LIKELY (fp != NULL))
    {
      while (fgets (line, sizeof (line), fp) != NULL)
        {
          /* remove trailing spaces */
          g_strchomp (line);

          /* skip over empty lines */
          if (*line == '\0' || *line == ' ')
            continue;

          /* check if there is a custom name in the line */
          name = NULL;
          space = strchr (line, ' ');
          if (space != NULL)
            {
              /* break line */
              *space++ = '\0';

              /* get the custom name */
              if (G_LIKELY (*space != '\0'))
                name = space;
            }

          file = g_file_new_for_uri (line);

          /* callback */
          foreach_func (file, name, row_num++, user_data);

          g_object_unref (G_OBJECT (file));
        }

      fclose (fp);
    }

}



/**
 * thunar_util_expand_filename:
 * @filename          : a local filename.
 * @working_directory : #GFile of the current working directory.
 * @error             : return location for errors or %NULL.
 *
 * Takes a user-typed @filename and expands a tilde at the
 * beginning of the @filename. It also resolves paths prefixed with
 * '.' using the current working directory.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the expanded @filename or %NULL on error.
 **/
gchar *
thunar_util_expand_filename (const gchar  *filename,
                             GFile        *working_directory,
                             GError      **error)
{
  struct passwd *passwd;
  const gchar   *replacement;
  const gchar   *remainder;
  const gchar   *slash;
  gchar         *username;
  gchar         *pwd;
  gchar         *variable;
  gchar         *result = NULL;

  g_return_val_if_fail (filename != NULL, NULL);

  /* check if we have a valid (non-empty!) filename */
  if (G_UNLIKELY (*filename == '\0'))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid path"));
      return NULL;
    }

  /* check if we start with a '~' */
  if (*filename == '~')
    {
      /* examine the remainder of the filename */
      remainder = filename + 1;

      /* if we have only the slash, then we want the home dir */
      if (G_UNLIKELY (*remainder == '\0'))
        return g_strdup (xfce_get_homedir ());

      /* lookup the slash */
      for (slash = remainder; *slash != '\0' && *slash != G_DIR_SEPARATOR; ++slash);

      /* check if a username was given after the '~' */
      if (G_LIKELY (slash == remainder))
        {
          /* replace the tilde with the home dir */
          replacement = xfce_get_homedir ();
        }
      else
        {
          /* lookup the pwd entry for the username */
          username = g_strndup (remainder, slash - remainder);
          passwd = getpwnam (username);
          g_free (username);

          /* check if we have a valid entry */
          if (G_UNLIKELY (passwd == NULL))
            {
              username = g_strndup (remainder, slash - remainder);
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Unknown user \"%s\""), username);
              g_free (username);
              return NULL;
            }

          /* use the homedir of the specified user */
          replacement = passwd->pw_dir;
        }

      /* generate the filename */
      return g_build_filename (replacement, slash, NULL);
    }
  else if (*filename == '$')
    {
      /* examine the remainder of the variable and filename */
      remainder = filename + 1;

      /* lookup the slash at the end of the variable */
      for (slash = remainder; *slash != '\0' && *slash != G_DIR_SEPARATOR; ++slash);

      /* get the variable for replacement */
      variable = g_strndup (remainder, slash - remainder);
      replacement = g_getenv (variable);
      g_free (variable);

      if (replacement == NULL)
        return NULL;

      /* generate the filename */
      return g_build_filename (replacement, slash, NULL);
    }
  else if (*filename == '.')
    {
      /* examine the remainder of the filename */
      remainder = filename + 1;

      /* transform working directory into a filename string */
      if (G_LIKELY (working_directory != NULL))
        {
          pwd = g_file_get_path (working_directory);

          /* if we only have the slash then we want the working directory only */
          if (G_UNLIKELY (*remainder == '\0'))
            return pwd;

          /* concatenate working directory and remainder */
          result = g_build_filename (pwd, remainder, G_DIR_SEPARATOR_S, NULL);

          /* free the working directory string */
          g_free (pwd);
        }
      else
        result = g_strdup (filename);

      /* return the resulting path string */
      return result;
    }

  return g_strdup (filename);
}



/**
 * thunar_util_humanize_file_time:
 * @file_time         : a #guint64 timestamp.
 * @date_style        : the #ThunarDateFormat used to humanize the @file_time.
 * @date_custom_style : custom style to apply, if @date_style is set to custom
 *
 * Returns a human readable date representation of the specified
 * @file_time. The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * Return value: a human readable date representation of @file_time
 *               according to the @date_format.
 **/
gchar*
thunar_util_humanize_file_time (guint64          file_time,
                                ThunarDateStyle  date_style,
                                const gchar     *date_custom_style)
{
  const gchar *date_format;
  gchar       *time_str;
  GDateTime   *dt_file;
  GDate        dfile;
  GDate        dnow;
  gint         diff;

  /* check if the file_time is valid */
  if (G_UNLIKELY (file_time == 0))
    return g_strdup (_("Unknown"));

  if ((dt_file = g_date_time_new_from_unix_local (file_time)) == NULL)
    return g_strdup (_("Unknown"));

  /* check which style to use to format the time */
  if (date_style == THUNAR_DATE_STYLE_SIMPLE
   || date_style == THUNAR_DATE_STYLE_SHORT
   || date_style == THUNAR_DATE_STYLE_CUSTOM_SIMPLE)
    {
      /* setup the dates for the time values */
      g_date_set_time_t (&dfile, (time_t) file_time);
      g_date_set_time_t (&dnow, time (NULL));

      /* determine the difference in days */
      diff = g_date_get_julian (&dnow) - g_date_get_julian (&dfile);
      if (diff == 0)
        {
          if (date_style == THUNAR_DATE_STYLE_SIMPLE || date_style == THUNAR_DATE_STYLE_CUSTOM_SIMPLE)
            {
              /* TRANSLATORS: file was modified less than one day ago */
              time_str = g_strdup (_("Today"));
            }
          else /* if (date_style == THUNAR_DATE_STYLE_SHORT) */
            {
              /* TRANSLATORS: file was modified less than one day ago */
              time_str = g_date_time_format (dt_file, _("Today at %X"));
            }
        }
      else if (diff == 1)
        {
          if (date_style == THUNAR_DATE_STYLE_SIMPLE || date_style == THUNAR_DATE_STYLE_CUSTOM_SIMPLE)
            {
              /* TRANSLATORS: file was modified less than two days ago */
              time_str = g_strdup (_("Yesterday"));
            }
          else /* if (date_style == THUNAR_DATE_STYLE_SHORT) */
            {
              /* TRANSLATORS: file was modified less than two days ago */
              time_str = g_date_time_format (dt_file, _("Yesterday at %X"));
            }
        }
      else
        {
          if (date_style == THUNAR_DATE_STYLE_CUSTOM_SIMPLE)
            {
              if (date_custom_style == NULL)
                time_str = g_strdup ("");
              else
              /* use custom date formatting */
                time_str = g_date_time_format (dt_file, date_custom_style);
            }
          else
            {
              if (diff > 1 && diff < 7)
                {
                  /* Days from last week */
                  date_format = (date_style == THUNAR_DATE_STYLE_SIMPLE) ? "%A" : _("%A at %X");
                }
              else
                {
                  /* Any other date */
                  date_format = (date_style == THUNAR_DATE_STYLE_SIMPLE) ? "%x" : _("%x at %X");
                }

              /* format the date string accordingly */
              time_str = g_date_time_format (dt_file, date_format);
            }
        }
    }
  else if (date_style == THUNAR_DATE_STYLE_LONG)
    {
      /* use long, date(1)-like format string */
      time_str = g_date_time_format (dt_file, "%c");
    }
  else if (date_style == THUNAR_DATE_STYLE_YYYYMMDD)
    {
      time_str = g_date_time_format (dt_file, "%Y-%m-%d %H:%M:%S");
    }
  else if (date_style == THUNAR_DATE_STYLE_MMDDYYYY)
    {
      time_str = g_date_time_format (dt_file, "%m-%d-%Y %H:%M:%S");
    }
  else if (date_style == THUNAR_DATE_STYLE_DDMMYYYY)
    {
      time_str = g_date_time_format (dt_file, "%d-%m-%Y %H:%M:%S");
    }
  else /* if (date_style == THUNAR_DATE_STYLE_CUSTOM) */
    {
      if (date_custom_style == NULL)
        time_str = g_strdup ("");
      else
        /* use custom date formatting */
        time_str = g_date_time_format (dt_file, date_custom_style);
    }

  g_date_time_unref (dt_file);

  return time_str;
}



/**
 * thunar_util_parse_parent:
 * @parent        : a #GtkWidget, a #GdkScreen or %NULL.
 * @window_return : return location for the toplevel #GtkWindow or
 *                  %NULL.
 *
 * Determines the screen for the @parent and returns that #GdkScreen.
 * If @window_return is not %NULL, the pointer to the #GtkWindow is
 * placed into it, or %NULL if the window could not be determined.
 *
 * Return value: the #GdkScreen for the @parent.
 **/
GdkScreen*
thunar_util_parse_parent (gpointer    parent,
                          GtkWindow **window_return)
{
  GdkScreen *screen;
  GtkWidget *window = NULL;

  _thunar_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent), NULL);

  /* determine the proper parent */
  if (parent == NULL)
    {
      /* just use the default screen then */
      screen = gdk_screen_get_default ();
    }
  else if (GDK_IS_SCREEN (parent))
    {
      /* yep, that's a screen */
      screen = GDK_SCREEN (parent);
    }
  else
    {
      /* parent is a widget, so let's determine the toplevel window */
      window = gtk_widget_get_toplevel (GTK_WIDGET (parent));
      if (window != NULL
          && gtk_widget_is_toplevel (window))
        {
          /* make sure the toplevel window is shown */
          gtk_widget_show_now (window);
        }
      else
        {
          /* no toplevel, not usable then */
          window = NULL;
        }

      /* determine the screen for the widget */
      screen = gtk_widget_get_screen (GTK_WIDGET (parent));
    }

  /* check if we should return the window */
  if (G_LIKELY (window_return != NULL))
    *window_return = (GtkWindow *) window;

  return screen;
}



/**
 * thunar_util_time_from_rfc3339:
 * @date_string : an RFC 3339 encoded date string.
 *
 * Decodes the @date_string, which must be in the special RFC 3339
 * format <literal>YYYY-MM-DDThh:mm:ss</literal>. This method is
 * used to decode deletion dates of files in the trash. See the
 * Trash Specification for details.
 *
 * Return value: the time value matching the @date_string or
 *               %0 if the @date_string could not be parsed.
 **/
time_t
thunar_util_time_from_rfc3339 (const gchar *date_string)
{
  struct tm tm;

#ifdef HAVE_STRPTIME
  /* using strptime() its easy to parse the date string */
  if (G_UNLIKELY (strptime (date_string, "%FT%T", &tm) == NULL))
    return 0;
#else
  gulong val;

  /* be sure to start with a clean tm */
  memset (&tm, 0, sizeof (tm));

  /* parsing by hand is also doable for RFC 3339 dates */
  val = strtoul (date_string, (gchar **) &date_string, 10);
  if (G_UNLIKELY (*date_string != '-'))
    return 0;

  /* YYYY-MM-DD */
  tm.tm_year = val - 1900;
  date_string++;
  tm.tm_mon = strtoul (date_string, (gchar **) &date_string, 10) - 1;

  if (G_UNLIKELY (*date_string++ != '-'))
    return 0;

  tm.tm_mday = strtoul (date_string, (gchar **) &date_string, 10);

  if (G_UNLIKELY (*date_string++ != 'T'))
    return 0;

  val = strtoul (date_string, (gchar **) &date_string, 10);
  if (G_UNLIKELY (*date_string != ':'))
    return 0;

  /* hh:mm:ss */
  tm.tm_hour = val;
  date_string++;
  tm.tm_min = strtoul (date_string, (gchar **) &date_string, 10);

  if (G_UNLIKELY (*date_string++ != ':'))
    return 0;

  tm.tm_sec = strtoul (date_string, (gchar **) &date_string, 10);
#endif /* !HAVE_STRPTIME */

  /* translate tm to time_t */
  return mktime (&tm);
}



gchar *
thunar_util_change_working_directory (const gchar *new_directory)
{
  gchar *old_directory;

  _thunar_return_val_if_fail (new_directory != NULL && *new_directory != '\0', NULL);

  /* try to determine the current working directory */
  old_directory = g_get_current_dir();

  /* try switching to the new working directory */
  if (g_chdir (new_directory) != 0)
    {
      /* switching failed, we don't need to return the old directory */
      g_free (old_directory);
      old_directory = NULL;
    }

  return old_directory;
}



void
thunar_setup_display_cb (gpointer data)
{
  g_setenv ("DISPLAY", (char *) data, TRUE);
}



/**
 * thunar_util_next_new_file_name
 * @dir : the directory to search for a free filename
 * @file_name : the filename which will be used as the basis/default
 * @ThunarNextFileNameMode: To decide if the naming should follow "file copy","file link" or "new file" syntax
 *
 * Returns a filename that is like @file_name with the possible addition of
 * a number to differentiate it from other similarly named files. In other words
 * it searches @dir for incrementally named files starting from @file_name
 * and returns the first available increment.
 *
 * e.g. in a folder with the following files:
 * - file
 * - empty
 * - file_copy
 *
 * Calling this functions with the above folder and @file_name equal to 'file' the returned
 * filename will be 'file 1' for the mode "new file".
 *
 * The caller is responsible to free the returned string using g_free() when no longer needed.
 *
 * Return value: pointer to the new filename.
**/
gchar*
thunar_util_next_new_file_name (ThunarFile            *dir,
                                const gchar           *file_name,
                                ThunarNextFileNameMode name_mode)
{
  ThunarFolder   *folder          = thunar_folder_get_for_file (dir);
  unsigned long   file_name_size  = strlen (file_name);
  unsigned        count           = 0;
  gboolean        found_duplicate = FALSE;
  gchar          *extension       = NULL;
  gchar          *new_name        = g_strdup (file_name);

  /* get file extension if file is not a directory */
  extension = thunar_util_str_get_extension (file_name);

  /* if the file has an extension don't include it in the search */
  if (extension != NULL)
    file_name_size -= strlen (extension);

  /* loop through the directory until new_name is unique */
  while (TRUE)
    {
      found_duplicate = FALSE;
      for (GList *files = thunar_folder_get_files (folder); files != NULL; files = files->next)
        {
          ThunarFile  *file = files->data;
          gchar       *name = g_file_get_basename (thunar_file_get_file (file));

          if (g_strcmp0 (new_name, name) == 0)
            {
              found_duplicate = TRUE;
              g_free (name);
              break;
            }
          g_free (name);
        }

      if (!found_duplicate)
        break;
      g_free (new_name);
      if (name_mode == THUNAR_NEXT_FILE_NAME_MODE_NEW)
        new_name = g_strdup_printf (_("%.*s %u%s"), (int) file_name_size, file_name, ++count, extension ? extension : "");
      else if (name_mode == THUNAR_NEXT_FILE_NAME_MODE_COPY)
        new_name = g_strdup_printf (_("%.*s (copy %u)%s"), (int) file_name_size, file_name, ++count, extension ? extension : "");
      else if (name_mode == THUNAR_NEXT_FILE_NAME_MODE_LINK)
        {
          if (count == 0)
            {
              new_name = g_strdup_printf (_("link to %.*s%s"), (int) file_name_size, file_name, extension ? extension : "");
              ++count;
            }
          else
            {
              new_name = g_strdup_printf (_("link %u to %.*s%s"), ++count, (int) file_name_size, file_name, extension ? extension : "");
            }
        }
      else
        g_assert("should not be reached");
    }
  g_object_unref (G_OBJECT (folder));

  return new_name;
}



/**
 * thunar_util_is_a_search_query
 * @string : the string to check
 *
 * Return value: a boolean that is TRUE if @string starts with 'Search: '.
**/
gboolean
thunar_util_is_a_search_query (const gchar *string)
{
  return strncmp (string, SEARCH_PREFIX, strlen (SEARCH_PREFIX)) == 0;
}



/**
 * thunar_util_strjoin_list:
 * @data      : list of strings which needs to be appended.
 * @separator : text which needs to be added as a seperator
 *
 * Joins a number of strings together to form one long string, with the optional separator
 * inserted between each of them. It skips all the NULL values passed in the list. The returned
 * string should be freed with g_free().
 * Similar to g_strjoin().
 *
 * The caller is responsible to free the returned text using g_free() when it is no longer needed
 *
 * Return value: the concatenated string
 **/
gchar*
thunar_util_strjoin_list (GList       *string_list,
                          const gchar *separator)
{
  GList *lp;
  gchar *joined_string = NULL;
  gchar *temp_string;

  for (lp = string_list; lp != NULL; lp = lp->next)
    {
      if (xfce_str_is_empty ((gchar *) lp->data))
        continue;

      if (joined_string == NULL)
        {
          joined_string = g_strdup (lp->data);
        }
      else
        {
          temp_string = g_strjoin (separator, joined_string, lp->data, NULL);
          g_free (joined_string);
          joined_string = temp_string;
        }
    }

  if (joined_string == NULL)
    return g_strdup ("");
  else
    return joined_string;
}



static void
thunar_util_determine_corner_properties (GtkWidget       *widget,
                                         GtkCellRenderer *cell,
                                         gint             cell_height,
                                         gint             cell_width,
                                         gdouble         *radius,
                                         gboolean        *side)
{
  GtkTextDirection text_direction;

  /* only have rounded corners for icon view. */
  if (G_LIKELY (EXO_IS_ICON_VIEW (widget)))
    {
      if (exo_icon_view_get_orientation (EXO_ICON_VIEW (widget)) == GTK_ORIENTATION_HORIZONTAL)
        {
          /* Compact View */
          /* determine the radius proportional to either height or width (depens on the view) */
          *radius = cell_height * (BORDER_RADIUS / 100.0);

          text_direction = gtk_widget_get_direction (widget);

          /* decide which side to draw the rounded corners */
          if (THUNAR_IS_TEXT_RENDERER (cell))
            *side = text_direction == GTK_TEXT_DIR_LTR ? DRAW_ON_RIGHT : DRAW_ON_LEFT;
          else
            *side = text_direction == GTK_TEXT_DIR_LTR ? DRAW_ON_LEFT : DRAW_ON_RIGHT;
        }
      else
        {
          /* Icon View */
          *radius = cell_width * (BORDER_RADIUS / 100.0);

          /* text direction has no effect on this view */
          /* decide which side to draw the rounded corners */
          if (THUNAR_IS_TEXT_RENDERER (cell))
            *side = DRAW_ON_BOTTOM;
          else
            *side = DRAW_ON_TOP;
        }
    }
}



static void
thunar_util_draw_rounded_corners (cairo_t            *cr,
                                  const GdkRectangle *background_area,
                                  gdouble             radius,
                                  gint                side)
{
  gdouble *corner_radius;
  gdouble  draw_round_corners_on_left[4]      = { 0,      0,      radius, radius };
  gdouble  draw_round_corners_on_right[4]     = { radius, radius, 0,      0      };
  gdouble  draw_round_corners_on_top[4]       = { radius, 0,      0,      radius };
  gdouble  draw_round_corners_on_bottom[4]    = { 0,      radius, radius, 0      };
  gdouble  draw_round_corners_on_all_sides[4] = { radius, radius, radius, radius };
  gdouble  degrees = G_PI / 180.0;

  switch (side)
    {
    case DRAW_ON_LEFT:
      corner_radius = draw_round_corners_on_left;
      break;
    case DRAW_ON_RIGHT:
      corner_radius = draw_round_corners_on_right;
      break;
    case DRAW_ON_BOTTOM:
      corner_radius = draw_round_corners_on_bottom;
      break;
    case DRAW_ON_TOP:
      corner_radius = draw_round_corners_on_top;
      break;
    case DRAW_ON_ALL_SIDES:
      corner_radius = draw_round_corners_on_all_sides;
      break;
    default:
      corner_radius = draw_round_corners_on_all_sides;
      g_warn_if_reached ();
    }

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             background_area->x + background_area->width - corner_radius[THUNAR_CELL_TOP_RIGHT], /* cairo x coord */
             background_area->y + corner_radius[THUNAR_CELL_TOP_RIGHT],                          /* cairo y coord */
             corner_radius[THUNAR_CELL_TOP_RIGHT], -90 * degrees, 0 * degrees);                  /* radius, angle1, angle2 resp. */
  cairo_arc (cr,
             background_area->x + background_area->width - corner_radius[THUNAR_CELL_BOTTOM_RIGHT],
             background_area->y + background_area->height - corner_radius[THUNAR_CELL_BOTTOM_RIGHT],
             corner_radius[THUNAR_CELL_BOTTOM_RIGHT], 0 * degrees, 90 * degrees);
  cairo_arc (cr,
             background_area->x + corner_radius[THUNAR_CELL_TOP_LEFT],
             background_area->y + background_area->height - corner_radius[THUNAR_CELL_TOP_LEFT],
             corner_radius[THUNAR_CELL_TOP_LEFT], 90 * degrees, 180 * degrees);
  cairo_arc (cr,
             background_area->x + corner_radius[THUNAR_CELL_BOTTOM_LEFT],
             background_area->y + corner_radius[THUNAR_CELL_BOTTOM_LEFT],
             corner_radius[THUNAR_CELL_BOTTOM_LEFT], 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
  cairo_clip (cr);
}



void
thunar_util_clip_view_background (GtkCellRenderer      *cell,
                                  cairo_t              *cr,
                                  const GdkRectangle   *background_area,
                                  GtkWidget            *widget,
                                  GtkCellRendererState  flags)
{
  GtkStyleContext  *context;
  GdkRGBA          *color = NULL;
  GdkRGBA           highlight_color_rgba;
  gboolean          color_selected = (flags & GTK_CELL_RENDERER_SELECTED) != 0;
  gboolean          rounded_corners;
  gchar            *highlight_color;
  gdouble           radius = 0.0;
  gint              side = DRAW_ON_ALL_SIDES;

  g_object_get (G_OBJECT (cell), 
                "highlight-color", &highlight_color,
                "rounded-corners", &rounded_corners,
                NULL);

  cairo_save (cr);

  if (G_LIKELY (rounded_corners))
    {
      /* determine radius & the side to draw the rounded corners */
      thunar_util_determine_corner_properties (widget, cell,
                                               background_area->height, background_area->width,
                                               &radius, &side);

      thunar_util_draw_rounded_corners (cr, background_area, radius, side);
    }

  if (G_UNLIKELY (highlight_color != NULL))
    {
      gdk_rgba_parse (&highlight_color_rgba, highlight_color);
      color = gdk_rgba_copy (&highlight_color_rgba);
    }

  /**
   * If the item is selected then paint the background area with the theme's selected item's color.
   * To distinguish between highlighted & non highlighted files, the background area of icon renderer
   * is left untouched if it already has a highlight color
   **/
  if (G_UNLIKELY (color_selected && !(THUNAR_IS_ICON_RENDERER (cell) && highlight_color != NULL)))
    {
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get (context, GTK_STATE_FLAG_SELECTED, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color, NULL);
    }

  if (G_LIKELY (color != NULL))
    {
      gdk_cairo_set_source_rgba (cr, color);
      gdk_rgba_free (color);
      cairo_paint (cr);
    }

  g_free (highlight_color);
  cairo_restore (cr);
}
