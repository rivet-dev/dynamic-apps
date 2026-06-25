/*
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Author(s):
 * 	Behdad Esfahbod
 */

#ifndef vte_vteunistr_h_included
#define vte_vteunistr_h_included

#include <glib.h>

G_BEGIN_DECLS

/**
 * vteunistr:
 *
 * vteunistr is a gunichar-compatible way to store strings.  A string
 * consisting of a single unichar c is represented as the same value
 * as c itself.  In that sense, gunichars can be readily used as
 * vteunistrs.  Longer strings can be built by appending a unichar
 * to an already existing string.
 *
 * vteunistr is essentially just a gunicode-compatible quark value.
 * It can be used to store strings (of a base followed by combining
 * characters) where the code was designed to only allow one character.
 *
 * Strings are internalized efficiently and never freed.  No memory
 * management of vteunistr values is needed.
 **/
typedef guint32 vteunistr;

/**
 * _vte_unistr_append_unichar:
 * @s: a #vteunistr
 * @c: Unicode character to append to @s
 *
 * Creates a vteunistr value for the string @s followed by the
 * character @c.
 *
 * Returns: the new #vteunistr value
 **/
vteunistr
_vte_unistr_append_unichar (vteunistr s, gunichar c);

/**
 * _vte_unistr_append_unistr:
 * @s: a #vteunistr
 * @t: another #vteunistr to append to @s
 *
 * Creates a vteunistr value for the string @s followed by the
 * string @t.
 *
 * Returns: the new #vteunistr value
 **/
vteunistr
_vte_unistr_append_unistr (vteunistr s, vteunistr t);

gunichar
_vte_unistr_get_base (vteunistr s);

/**
 * _vte_unistr_append_to_string:
 * @s: a #vteunistr
 * @c: Unicode character to replace the base character of @s.
 *
 * Creates a vteunistr value where the base character from @s is
 * replaced by @c, while the combining characters from @s are carried over.
 *
 * Returns: the new #vteunistr value
 */
vteunistr
_vte_unistr_replace_base (vteunistr s, gunichar c);

/**
 * _vte_unistr_append_to_string:
 * @s: a #vteunistr
 * @gs: a #GString to append @s to
 *
 * Appends @s to @gs.  This is how one converts a #vteunistr to a
 * traditional string.
 **/
void
_vte_unistr_append_to_string (vteunistr s, GString *gs);

/**
 * _vte_unistr_append_to_gunichars:
 * @s: a #vteunistr
 * @a: a #GArray of #gunichar items to append @s to
 *
 * Appends @s to @a.
 **/
void
_vte_unistr_append_to_gunichars (vteunistr s, GArray *a);

/**
 * _vte_unistr_strlen:
 * @s: a #vteunistr
 *
 * Counts the number of character in @s.
 *
 * Returns: length of @s in characters.
 **/
int
_vte_unistr_strlen (vteunistr s);

G_END_DECLS

#endif
