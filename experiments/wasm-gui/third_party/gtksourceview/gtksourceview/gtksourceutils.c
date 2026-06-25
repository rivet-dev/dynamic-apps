/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2005 - Paolo Borelli
 * Copyright (C) 2007 - Gustavo Giráldez
 * Copyright (C) 2007 - Paolo Maggi
 * Copyright (C) 2013, 2017 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:utils
 * @title: GtkSourceUtils
 * @short_description: Utility functions
 *
 * Utility functions.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourceutils.h"
#include "gtksourceutils-private.h"
#include <string.h>
#include <errno.h>
#include <math.h>
#include <glib/gi18n-lib.h>

/**
 * gtk_source_utils_unescape_search_text:
 * @text: the text to unescape.
 *
 * Use this function before gtk_source_search_settings_set_search_text(), to
 * unescape the following sequences of characters: `\n`, `\r`, `\t` and `\\`.
 * The purpose is to easily write those characters in a search entry.
 *
 * Note that unescaping the search text is not needed for regular expression
 * searches.
 *
 * See also: gtk_source_utils_escape_search_text().
 *
 * Returns: the unescaped @text.
 * Since: 3.10
 */
gchar *
gtk_source_utils_unescape_search_text (const gchar *text)
{
	GString *str;
	gint length;
	gboolean drop_prev = FALSE;
	const gchar *cur;
	const gchar *end;
	const gchar *prev;

	if (text == NULL)
	{
		return NULL;
	}

	length = strlen (text);

	str = g_string_new ("");

	cur = text;
	end = text + length;
	prev = NULL;

	while (cur != end)
	{
		const gchar *next;
		next = g_utf8_next_char (cur);

		if (prev && (*prev == '\\'))
		{
			switch (*cur)
			{
				case 'n':
					str = g_string_append (str, "\n");
					break;
				case 'r':
					str = g_string_append (str, "\r");
					break;
				case 't':
					str = g_string_append (str, "\t");
					break;
				case '\\':
					str = g_string_append (str, "\\");
					drop_prev = TRUE;
					break;
				default:
					str = g_string_append (str, "\\");
					str = g_string_append_len (str, cur, next - cur);
					break;
			}
		}
		else if (*cur != '\\')
		{
			str = g_string_append_len (str, cur, next - cur);
		}
		else if ((next == end) && (*cur == '\\'))
		{
			str = g_string_append (str, "\\");
		}

		if (!drop_prev)
		{
			prev = cur;
		}
		else
		{
			prev = NULL;
			drop_prev = FALSE;
		}

		cur = next;
	}

	return g_string_free (str, FALSE);
}

/**
 * gtk_source_utils_escape_search_text:
 * @text: the text to escape.
 *
 * Use this function to escape the following characters: `\n`, `\r`, `\t` and `\`.
 *
 * For a regular expression search, use g_regex_escape_string() instead.
 *
 * One possible use case is to take the #GtkTextBuffer's selection and put it in a
 * search entry. The selection can contain tabulations, newlines, etc. So it's
 * better to escape those special characters to better fit in the search entry.
 *
 * See also: gtk_source_utils_unescape_search_text().
 *
 * <warning>
 * Warning: the escape and unescape functions are not reciprocal! For example,
 * escape (unescape (\)) = \\. So avoid cycles such as: search entry -> unescape
 * -> search settings -> escape -> search entry. The original search entry text
 * may be modified.
 * </warning>
 *
 * Returns: the escaped @text.
 * Since: 3.10
 */
gchar *
gtk_source_utils_escape_search_text (const gchar* text)
{
	GString *str;
	gint length;
	const gchar *p;
	const gchar *end;

	if (text == NULL)
	{
		return NULL;
	}

	length = strlen (text);

	str = g_string_new ("");

	p = text;
	end = text + length;

	while (p != end)
	{
		const gchar *next = g_utf8_next_char (p);

		switch (*p)
		{
			case '\n':
				g_string_append (str, "\\n");
				break;
			case '\r':
				g_string_append (str, "\\r");
				break;
			case '\t':
				g_string_append (str, "\\t");
				break;
			case '\\':
				g_string_append (str, "\\\\");
				break;
			default:
				g_string_append_len (str, p, next - p);
				break;
		}

		p = next;
	}

	return g_string_free (str, FALSE);
}

#define GSV_DATA_SUBDIR "gtksourceview-" GSV_API_VERSION_S

gchar **
_gtk_source_utils_get_default_dirs (const gchar *basename)
{
	const gchar * const *system_dirs;
	GPtrArray *dirs;

	dirs = g_ptr_array_new ();

	/* User dir */
	g_ptr_array_add (dirs, g_build_filename (g_get_user_data_dir (),
						 GSV_DATA_SUBDIR,
						 basename,
						 NULL));

	/* System dirs */
	for (system_dirs = g_get_system_data_dirs ();
	     system_dirs != NULL && *system_dirs != NULL;
	     system_dirs++)
	{
		g_ptr_array_add (dirs, g_build_filename (*system_dirs,
							 GSV_DATA_SUBDIR,
							 basename,
							 NULL));
	}

	g_ptr_array_add (dirs, NULL);

	return (gchar **) g_ptr_array_free (dirs, FALSE);
}

static GSList *
build_file_listing (const gchar *item,
		    GSList      *filenames,
		    const gchar *suffix,
		    gboolean     only_dirs)
{
	GDir *dir;
	const gchar *name;

	if (!only_dirs && g_file_test (item, G_FILE_TEST_IS_REGULAR))
	{
		filenames = g_slist_prepend (filenames, g_strdup(item));
		return filenames;

	}
	dir = g_dir_open (item, 0, NULL);

	if (dir == NULL)
	{
		return filenames;
	}

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		gchar *full_path = g_build_filename (item, name, NULL);

		if (!g_file_test (full_path, G_FILE_TEST_IS_DIR) &&
		    g_str_has_suffix (name, suffix))
		{
			filenames = g_slist_prepend (filenames, full_path);
		}
		else
		{
			g_free (full_path);
		}
	}

	g_dir_close (dir);

	return filenames;
}

GSList *
_gtk_source_utils_get_file_list (gchar       **path,
				 const gchar  *suffix,
				 gboolean      only_dirs)
{
	GSList *files = NULL;

	for ( ; path && *path; ++path)
	{
		files = build_file_listing (*path, files, suffix, only_dirs);
	}

	return g_slist_reverse (files);
}

/* Wrapper around strtoull for easier use: tries
 * to convert @str to a number and return -1 if it is not.
 * Used to check if references in subpattern contexts
 * (e.g. \%{1@start} or \%{blah@start}) are named or numbers.
 */
gint
_gtk_source_utils_string_to_int (const gchar *str)
{
	guint64 number;
	gchar *end_str;

	if (str == NULL || *str == '\0')
	{
		return -1;
	}

	errno = 0;
	number = g_ascii_strtoull (str, &end_str, 10);

	if (errno != 0 || number > G_MAXINT || *end_str != '\0')
	{
		return -1;
	}

	return number;
}

#define FONT_FAMILY  "font-family"
#define FONT_VARIANT "font-variant"
#define FONT_STRETCH "font-stretch"
#define FONT_WEIGHT  "font-weight"
#define FONT_STYLE   "font-style"
#define FONT_SIZE    "font-size"

gchar *
_gtk_source_utils_pango_font_description_to_css (const PangoFontDescription *font_desc)
{
	PangoFontMask mask;
	GString *str;

#define ADD_KEYVAL(key,fmt) \
	g_string_append(str,key":"fmt";")
#define ADD_KEYVAL_PRINTF(key,fmt,...) \
	g_string_append_printf(str,key":"fmt";", __VA_ARGS__)

	g_return_val_if_fail (font_desc, NULL);

	str = g_string_new (NULL);

	mask = pango_font_description_get_set_fields (font_desc);

	if ((mask & PANGO_FONT_MASK_FAMILY) != 0)
	{
		const gchar *family;

		family = pango_font_description_get_family (font_desc);
		ADD_KEYVAL_PRINTF (FONT_FAMILY, "\"%s\"", family);
	}

	if ((mask & PANGO_FONT_MASK_STYLE) != 0)
	{
		PangoStyle style;

		style = pango_font_description_get_style (font_desc);

		switch (style)
		{
			case PANGO_STYLE_NORMAL:
				ADD_KEYVAL (FONT_STYLE, "normal");
				break;

			case PANGO_STYLE_OBLIQUE:
				ADD_KEYVAL (FONT_STYLE, "oblique");
				break;

			case PANGO_STYLE_ITALIC:
				ADD_KEYVAL (FONT_STYLE, "italic");
				break;

			default:
				break;
		}
	}

	if ((mask & PANGO_FONT_MASK_VARIANT) != 0)
	{
		PangoVariant variant;

		variant = pango_font_description_get_variant (font_desc);

		switch (variant)
		{
			case PANGO_VARIANT_NORMAL:
				ADD_KEYVAL (FONT_VARIANT, "normal");
				break;

			case PANGO_VARIANT_SMALL_CAPS:
				ADD_KEYVAL (FONT_VARIANT, "small-caps");
				break;

#if PANGO_VERSION_CHECK(1, 49, 3)
			case PANGO_VARIANT_ALL_SMALL_CAPS:
				ADD_KEYVAL (FONT_VARIANT, "all-small-caps");
				break;

			case PANGO_VARIANT_PETITE_CAPS:
				ADD_KEYVAL (FONT_VARIANT, "petite-caps");
				break;

			case PANGO_VARIANT_ALL_PETITE_CAPS:
				ADD_KEYVAL (FONT_VARIANT, "all-petite-caps");
				break;

			case PANGO_VARIANT_UNICASE:
				ADD_KEYVAL (FONT_VARIANT, "unicase");
				break;

			case PANGO_VARIANT_TITLE_CAPS:
				ADD_KEYVAL (FONT_VARIANT, "titling-caps");
				break;
#endif

			default:
				break;
		}
	}

	if ((mask & PANGO_FONT_MASK_WEIGHT))
	{
		gint weight;

		weight = pango_font_description_get_weight (font_desc);

		/*
		 * WORKAROUND:
		 *
		 * font-weight with numbers does not appear to be working as expected
		 * right now. So for the common (bold/normal), let's just use the string
		 * and let gtk warn for the other values, which shouldn't really be
		 * used for this.
		 */

		switch (weight)
		{
			case PANGO_WEIGHT_SEMILIGHT:
			/*
			 * 350 is not actually a valid css font-weight, so we will just round
			 * up to 400.
			 */
			case PANGO_WEIGHT_NORMAL:
				ADD_KEYVAL (FONT_WEIGHT, "normal");
				break;

			case PANGO_WEIGHT_BOLD:
				ADD_KEYVAL (FONT_WEIGHT, "bold");
				break;

			case PANGO_WEIGHT_THIN:
			case PANGO_WEIGHT_ULTRALIGHT:
			case PANGO_WEIGHT_LIGHT:
			case PANGO_WEIGHT_BOOK:
			case PANGO_WEIGHT_MEDIUM:
			case PANGO_WEIGHT_SEMIBOLD:
			case PANGO_WEIGHT_ULTRABOLD:
			case PANGO_WEIGHT_HEAVY:
			case PANGO_WEIGHT_ULTRAHEAVY:
			default:
				/* round to nearest hundred */
				weight = round (weight / 100.0) * 100;
				ADD_KEYVAL_PRINTF ("font-weight", "%d", weight);
				break;
		}
	}

	if ((mask & PANGO_FONT_MASK_STRETCH))
	{
		switch (pango_font_description_get_stretch (font_desc))
		{
			case PANGO_STRETCH_ULTRA_CONDENSED:
				ADD_KEYVAL (FONT_STRETCH, "ultra-condensed");
				break;

			case PANGO_STRETCH_EXTRA_CONDENSED:
				ADD_KEYVAL (FONT_STRETCH, "extra-condensed");
				break;

			case PANGO_STRETCH_CONDENSED:
				ADD_KEYVAL (FONT_STRETCH, "condensed");
				break;

			case PANGO_STRETCH_SEMI_CONDENSED:
				ADD_KEYVAL (FONT_STRETCH, "semi-condensed");
				break;

			case PANGO_STRETCH_NORMAL:
				ADD_KEYVAL (FONT_STRETCH, "normal");
				break;

			case PANGO_STRETCH_SEMI_EXPANDED:
				ADD_KEYVAL (FONT_STRETCH, "semi-expanded");
				break;

			case PANGO_STRETCH_EXPANDED:
				ADD_KEYVAL (FONT_STRETCH, "expanded");
				break;

			case PANGO_STRETCH_EXTRA_EXPANDED:
				ADD_KEYVAL (FONT_STRETCH, "extra-expanded");
				break;

			case PANGO_STRETCH_ULTRA_EXPANDED:
				ADD_KEYVAL (FONT_STRETCH, "ultra-expanded");
				break;

			default:
				break;
		}
	}

	if ((mask & PANGO_FONT_MASK_SIZE))
	{
		gint font_size;

		font_size = pango_font_description_get_size (font_desc) / PANGO_SCALE;
		ADD_KEYVAL_PRINTF ("font-size", "%dpt", font_size);
	}

	return g_string_free (str, FALSE);

#undef ADD_KEYVAL
#undef ADD_KEYVAL_PRINTF
}

/*
 * _gtk_source_utils_dgettext:
 *
 * Try to translate string from given domain. It returns
 * duplicated string which must be freed with g_free().
 */
gchar *
_gtk_source_utils_dgettext (const gchar *domain,
			    const gchar *string)
{
	const gchar *translated;
	gchar *tmp;

	g_return_val_if_fail (string != NULL, NULL);

	if (domain == NULL)
	{
		return g_strdup (_(string));
	}

	translated = dgettext (domain, string);

	if (g_strcmp0 (translated, string) == 0)
	{
		return g_strdup (_(string));
	}

	if (g_utf8_validate (translated, -1, NULL))
	{
		return g_strdup (translated);
	}

	tmp = g_locale_to_utf8 (translated, -1, NULL, NULL, NULL);
	return tmp != NULL ? tmp : g_strdup (string);
}

/*
 * _gtk_source_utils_int_to_string:
 * @value: the integer to convert to a string
 * @outstr: (out): a location for a pointer to the result string
 *
 * The following implementation uses an internal cache to speed up the
 * conversion of integers to strings by comparing the value to the
 * previous value that was calculated.
 *
 * If we detect a simple increment, we can alter the previous string directly
 * and then carry the number to each of the previous chars sequentually. If we
 * still have a carry bit at the end of the loop, we need to move the whole
 * string over 1 place to take account for the new "1" at the start.
 *
 * This function is not thread-safe, as the resulting string is stored in
 * static data.
 *
 * Returns: the number of characters in the resulting string
 */
gint
_gtk_source_utils_int_to_string (guint         value,
                                 const gchar **outstr)
{
	static struct{
		guint value;
		guint len;
		gchar str[12];
	} fi;

	*outstr = fi.str;

	if (value == fi.value)
	{
		return fi.len;
	}

	if G_LIKELY (value == fi.value + 1)
	{
		guint carry = 1;
		gint i;

		for (i = fi.len - 1; i >= 0; i--)
		{
			fi.str[i] += carry;
			carry = fi.str[i] == ':';

			if (carry)
			{
				fi.str[i] = '0';
			}
			else
			{
				break;
			}
		}

		if G_UNLIKELY (carry)
		{
			for (i = fi.len; i > 0; i--)
			{
				fi.str[i] = fi.str[i-1];
			}

			fi.len++;
			fi.str[0] = '1';
			fi.str[fi.len] = 0;
		}

		fi.value++;

		return fi.len;
	}

#ifdef G_OS_WIN32
	fi.len = g_snprintf (fi.str, sizeof fi.str - 1, "%u", value);
#else
	/* Use snprintf() directly when possible to reduce overhead */
	fi.len = snprintf (fi.str, sizeof fi.str - 1, "%u", value);
#endif
	fi.str[fi.len] = 0;
	fi.value = value;

	return fi.len;
}

/*
 * The goal of this function is to be like gtk_text_view_scroll_to_iter() but
 * without any of the scrolling animation. We use it from the source map where
 * the updates are so fast the scrolling animation makes it feel very delayed.
 *
 * Many parts of this function were taken from gtk_text_view_scroll_to_iter ()
 * https://developer.gnome.org/gtk3/stable/GtkTextView.html#gtk-text-view-scroll-to-iter
 */
void
_gtk_source_view_jump_to_iter (GtkTextView       *text_view,
                               const GtkTextIter *iter,
                               double             within_margin,
                               gboolean           use_align,
                               double             xalign,
                               double             yalign)
{
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  GdkRectangle rect;
  GdkRectangle screen;
  int xvalue = 0;
  int yvalue = 0;
  int scroll_dest;
  int screen_bottom;
  int screen_right;
  int screen_xoffset;
  int screen_yoffset;
  int current_x_scroll;
  int current_y_scroll;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (within_margin >= 0.0 && within_margin <= 0.5);
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (text_view));
  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (text_view));

  gtk_text_view_get_iter_location (text_view, iter, &rect);
  gtk_text_view_get_visible_rect (text_view, &screen);

  current_x_scroll = screen.x;
  current_y_scroll = screen.y;

  screen_xoffset = screen.width * within_margin;
  screen_yoffset = screen.height * within_margin;

  screen.x += screen_xoffset;
  screen.y += screen_yoffset;
  screen.width -= screen_xoffset * 2;
  screen.height -= screen_yoffset * 2;


  /* paranoia check */
  if (screen.width < 1)
    screen.width = 1;
  if (screen.height < 1)
    screen.height = 1;

  /* The -1 here ensures that we leave enough space to draw the cursor
   * when this function is used for horizontal scrolling.
   */
  screen_right = screen.x + screen.width - 1;
  screen_bottom = screen.y + screen.height;


  /* The alignment affects the point in the target character that we
   * choose to align. If we're doing right/bottom alignment, we align
   * the right/bottom edge of the character the mark is at; if we're
   * doing left/top we align the left/top edge of the character; if
   * we're doing center alignment we align the center of the
   * character.
   */

  /* Vertical alignment */
  scroll_dest = current_y_scroll;
  if (use_align)
    {
      scroll_dest = rect.y + (rect.height * yalign) - (screen.height * yalign);

      /* if scroll_dest < screen.y, we move a negative increment (up),
       * else a positive increment (down)
       */
      yvalue = scroll_dest - screen.y + screen_yoffset;
    }
  else
    {
      /* move minimum to get onscreen */
      if (rect.y < screen.y)
        {
          scroll_dest = rect.y;
          yvalue = scroll_dest - screen.y - screen_yoffset;
        }
      else if ((rect.y + rect.height) > screen_bottom)
        {
          scroll_dest = rect.y + rect.height;
          yvalue = scroll_dest - screen_bottom + screen_yoffset;
        }
    }
  yvalue += current_y_scroll;

  /* Horizontal alignment */
  scroll_dest = current_x_scroll;
  if (use_align)
    {
      scroll_dest = rect.x + (rect.width * xalign) - (screen.width * xalign);

      /* if scroll_dest < screen.y, we move a negative increment (left),
       * else a positive increment (right)
       */
      xvalue = scroll_dest - screen.x + screen_xoffset;
    }
  else
    {
      /* move minimum to get onscreen */
      if (rect.x < screen.x)
        {
          scroll_dest = rect.x;
          xvalue = scroll_dest - screen.x - screen_xoffset;
        }
      else if ((rect.x + rect.width) > screen_right)
        {
          scroll_dest = rect.x + rect.width;
          xvalue = scroll_dest - screen_right + screen_xoffset;
        }
    }
  xvalue += current_x_scroll;

  gtk_adjustment_set_value (hadj, xvalue);
  gtk_adjustment_set_value (vadj, yvalue);
}
