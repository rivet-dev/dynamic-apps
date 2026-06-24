/* $Id$ */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

/**
 * SECTION: xfce-rc
 * @title: Resource Config File Support
 * @short_description: functions for reading and writing resource config files.
 *
 * Provides support for parsing INI-style resource config files like used by for
 * example KDE and some Xfce components (like xfwm4, who uses rc files for the
 * themes).
 *
 * The parser itself is optimized for high-performance using memory and string chunks
 * to reduce the time spent looking for heap memory (a nice side effect of this is the
 * reduced heap corruption). But due to this fact, an #XfceRc object might consume quite
 * a lot of memory after some time of usage. Therefore you should close an #XfceRc object
 * as soon as possible after loading configuration data from the object.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif



#include <libxfce4util/libxfce4util.h>
#include <libxfce4util/xfce-private.h>
#include <libxfce4util/xfce-rc-private.h>
#include <libxfce4util/libxfce4util-alias.h>

static XfceRc *
xfce_rc_copy (const XfceRc *rc)
{
  return g_slice_dup (XfceRc, rc);
}

static void
xfce_rc_free (XfceRc *rc)
{
  if (G_LIKELY (rc != NULL))
    {
      g_slice_free (XfceRc, rc);
    }
}

G_DEFINE_BOXED_TYPE (XfceRc, xfce_rc,
  xfce_rc_copy, xfce_rc_free)

/* called by _xfce_rc_{simple,config}_new */
void
_xfce_rc_init (XfceRc *rc)
{
#ifdef HAVE_SETLOCALE
  const gchar *language;
  gchar *locale;
#endif

  _xfce_return_if_fail (rc != NULL);

#ifdef HAVE_SETLOCALE
  language = g_getenv ("LANGUAGE");
  if (language != NULL)
    rc->languages = g_strsplit (language, ":", -1);

  locale = setlocale (LC_MESSAGES, NULL);
  if (locale != NULL
      && strcmp (locale, "C") != 0
      && strcmp (locale, "POSIX") != 0)
    {
      rc->locale = g_strdup (locale);
    }
#endif
}



/**
 * xfce_rc_simple_open:
 * @filename : name of the filename to open.
 * @readonly : whether to open @filename readonly.
 *
 * Parses the resource config file specified by @filename.
 *
 * If @readonly is %TRUE parsing is generally faster, because only untranslated
 * entries and entries that match the current locale will be loaded. Also if
 * you pass %TRUE for @readonly, #xfce_rc_simple_open will fail if @filename
 * does not reference a regular file.
 *
 * It is no error if @readonly is %FALSE and the file referenced by @filename
 * does not exists. In this case you'll start with a fresh config, which contains
 * only the default group and no entries.
 *
 * Return value: (transfer full): the newly created #XfceRc object, or %NULL on error.
 *
 * Since: 4.2
 **/
XfceRc*
xfce_rc_simple_open (const gchar *filename,
                     gboolean     readonly)
{
  XfceRcSimple *simple;
  gboolean      exists;

  exists = g_file_test (filename, G_FILE_TEST_IS_REGULAR);

  if (!exists && readonly)
    return NULL;

  simple = _xfce_rc_simple_new (NULL, filename, readonly);

  if (exists && !_xfce_rc_simple_parse (simple))
    {
      xfce_rc_close (XFCE_RC (simple));
      return NULL;
    }

  return XFCE_RC (simple);
}



/**
 * xfce_rc_config_open:
 * @type     : The resource type being opened
 * @resource : The resource name to open
 * @readonly : whether to open @resource readonly.
 *
 * If @readonly is %TRUE parsing is generally faster, because only untranslated
 * entries and entries that match the current locale will be loaded. Also if
 * you pass %TRUE for @readonly, #xfce_rc_config will fail if @resource
 * does not reference a regular file.
 *
 * It is no error if @readonly is %FALSE and the file referenced by @resource
 * does not exists. In this case you'll start with a fresh config, which contains
 * only the default group and no entries.
 *
 * Return value: (transfer full): the newly created #XfceRc object, or %NULL on error.
 *
 * Since: 4.2
 **/
XfceRc*
xfce_rc_config_open (XfceResourceType type,
                     const gchar     *resource,
                     gboolean         readonly)
{
  XfceRcConfig *config;

  config = _xfce_rc_config_new (type, resource, readonly);

  return XFCE_RC (config);
}



/**
 * xfce_rc_close:
 * @rc : an #XfceRc object.
 *
 * Destructs @rc.
 *
 * If @rc was opened read-write and contains dirty (modified) entries, these
 * will be flushed to permanent storage first.
 *
 * Since: 4.2
 **/
void
xfce_rc_close (XfceRc *rc)
{
  g_return_if_fail (rc != NULL);
  g_return_if_fail (rc->close != NULL);

  if (rc->flush != NULL)
    (*rc->flush) (rc);

  (*rc->close) (rc);

  if (rc->locale != NULL)
    g_free (rc->locale);

  if (rc->languages != NULL)
    g_strfreev (rc->languages);

  g_free (rc);
}



/**
 * xfce_rc_flush:
 * @rc : an #XfceRc object.
 *
 * Flushes all changes that currently reside only in memory back to permanent
 * storage. Dirty configuration entries are written in the most specific file
 * available.
 *
 * Since: 4.2
 **/
void
xfce_rc_flush (XfceRc *rc)
{
  g_return_if_fail (rc != NULL);

  if (rc->flush != NULL)
    (*rc->flush) (rc);
}



/**
 * xfce_rc_rollback:
 * @rc : an #XfceRc object.
 *
 * Mark @rc as "clean", i.e. don't write dirty entries at destruction time. If
 * you then call #xfce_rc_write_entry again, the dirty flag is set again and
 * dirty entries will be written at a subsequent #xfce_rc_flush call.
 *
 * Since: 4.2
 **/
void
xfce_rc_rollback (XfceRc *rc)
{
  g_return_if_fail (rc != NULL);

  if (rc->rollback != NULL)
    (*rc->rollback) (rc);
}



/**
 * xfce_rc_is_dirty:
 * @rc : an #XfceRc object.
 *
 * Checks whether @rc has any dirty (modified) entries.
 *
 * Return value: %TRUE if @rc has any dirty (modified) entries.
 *
 * Since: 4.2
 **/
gboolean
xfce_rc_is_dirty (const XfceRc *rc)
{
  g_return_val_if_fail (rc != NULL, FALSE);

  if (rc->is_dirty != NULL)
    return (*rc->is_dirty) (rc);
  else
    return FALSE;
}



/**
 * xfce_rc_is_readonly:
 * @rc : an #XfceRc object.
 *
 * Returns the read-only status of @rc.
 *
 * Return value: the read-only status.
 *
 * Since: 4.2
 **/
gboolean
xfce_rc_is_readonly (const XfceRc *rc)
{
  g_return_val_if_fail (rc != NULL, FALSE);

  if (rc->is_readonly != NULL)
    return (*rc->is_readonly) (rc);
  else
    return TRUE;
}



/**
 * xfce_rc_get_locale:
 * @rc : an #XfceRc object.
 *
 * Returns current locale used by @rc to lookup translated entries.
 *
 * Return value: a string representing the current locale.
 *
 * Since: 4.2
 **/
const gchar*
xfce_rc_get_locale (const XfceRc *rc)
{
  g_return_val_if_fail (rc != NULL, NULL);

  if (rc->locale == NULL)
    return "C";
  else
    return rc->locale;
}



/**
 * xfce_rc_get_groups:
 * @rc : an #XfceRc object.
 *
 * Returns the names of all known groups in @rc.
 *
 * Since the default groups (the "NULL group") name is %NULL, it will not be
 * returned with this functions. But it does not matter at all, since the
 * default group is known to always exist.
 *
 * Return value: (transfer full): a NULL-terminated string array will the names of all groups in
 *               @rc. Should be freed using g_strfreev() when no longer needed.
 *
 * Since: 4.2
 **/
gchar**
xfce_rc_get_groups (const XfceRc *rc)
{
  g_return_val_if_fail (rc != NULL, NULL);
  g_return_val_if_fail (rc->get_groups != NULL, NULL);

  return (*rc->get_groups) (rc);
}



/**
 * xfce_rc_get_entries:
 * @rc    : an #XfceRc object.
 * @group : the name of the group to get entries from.
 *
 * Returns the names of all entries in @group if any.
 *
 * %NULL is a valid input value for @group. #xfce_rc_get_entries will
 * then return all entries in the so called "NULL group". Though this
 * "NULL group" should only be used for backward compatibility with old
 * applications. You should not use it in newly written code.
 *
 * Return value: (transfer full): a NULL-terminated string array with all entries in @group. Has to
 *               be freed using g_strfreev() if no longer needed. If the specified
 *               @group does not exists, %NULL is returned. If the @group has no entries,
 *               an empty string array is returned.
 *
 * Since: 4.2
 **/
gchar**
xfce_rc_get_entries (const XfceRc *rc, const gchar *group)
{
  g_return_val_if_fail (rc != NULL, NULL);
  g_return_val_if_fail (rc->get_entries != NULL, NULL);

  return (*rc->get_entries) (rc, group);
}



/**
 * xfce_rc_delete_group:
 * @rc     : an #XfceRc object.
 * @group  : name of the group to delete.
 * @global : whether to delete the group globally.
 *
 * If @rc is a simple config object and @group exists, it is deleted. All entries
 * within @group will be deleted. For simple config objects, @global is ignored.
 *
 * If @rc is a complex config object and @group exists, it will be deleted will
 * all entries. If @global is %TRUE, the entry will be marked as deleted globally,
 * therefore all calls to #xfce_rc_read_entry and related functions will return
 * the fallback values. If @global is %FALSE, the @group will be deleted in the
 * per-user config file, and further calls to #xfce_rc_read_entry will most
 * probably return the system-wide config entries.
 *
 * Since: 4.2
 **/
void
xfce_rc_delete_group (XfceRc *rc, const gchar *group, gboolean global)
{
  g_return_if_fail (rc != NULL);

  if (rc->delete_group != NULL)
    (*rc->delete_group) (rc, group, global);
}



/**
 * xfce_rc_get_group:
 * @rc : an #XfceRc object.
 *
 * Returns the name of the group in which we are searching for keys and
 * from which we are retrieving entries. If the currently active group is
 * the default group (the so called "NULL group"), %NULL will be returned.
 *
 * Return value: the name of the current group.
 *
 * Since: 4.2
 **/
const gchar*
xfce_rc_get_group (const XfceRc *rc)
{
  g_return_val_if_fail (rc != NULL, NULL);
  g_return_val_if_fail (rc->get_group != NULL, NULL);

  return (*rc->get_group) (rc);
}



/**
 * xfce_rc_has_group:
 * @rc    : an #XfceRc object.
 * @group : the group to search for.
 *
 * Returns %TRUE if the specified @group is known about.
 *
 * Return value: %TRUE if the @group exists.
 *
 * Since: 4.2
 **/
gboolean
xfce_rc_has_group (const XfceRc *rc, const gchar *group)
{
  g_return_val_if_fail (rc != NULL, FALSE);
  g_return_val_if_fail (rc->has_group != NULL, FALSE);

  return (*rc->has_group) (rc, group);
}



/**
 * xfce_rc_set_group:
 * @rc    : an #XfceRc object.
 * @group : the name of the new group or %NULL to to switch back to the default group.
 *
 * Specifies the group in which keys will be read and written. Subsequent calls
 * to #xfce_rc_read_entry and #xfce_rc_write_entry will be applied only in the
 * active group.
 *
 * If @group references a group that does not exists, it will be created for
 * you. But note, that empty groups will not be synced to permanent storage.
 *
 * Since: 4.2
 **/
void
xfce_rc_set_group (XfceRc *rc, const gchar *group)
{
  g_return_if_fail (rc != NULL);
  g_return_if_fail (rc->set_group != NULL);

  (*rc->set_group) (rc, group);
}



/**
 * xfce_rc_delete_entry:
 * @rc     : an #XfceRc object.
 * @key    : the key to delete.
 * @global : whether to delete @key globally.
 *
 * Similar to #xfce_rc_delete_group, but works on an entry in the current
 * group.
 *
 * Since: 4.2
 **/
void
xfce_rc_delete_entry (XfceRc *rc, const gchar *key, gboolean global)
{
  g_return_if_fail (rc != NULL);
  g_return_if_fail (key != NULL);

  if (rc->delete_entry != NULL)
    (*rc->delete_entry) (rc, key, global);
}



/**
 * xfce_rc_has_entry:
 * @rc  : an #XfceRc object.
 * @key : the key to search for.
 *
 * Checks whether the @key has an entry in the current group.
 *
 * Return value: %TRUE if the @key is available, else %FALSE.
 *
 * Since: 4.2
 **/
gboolean
xfce_rc_has_entry (const XfceRc *rc,
                   const gchar  *key)
{
  g_return_val_if_fail (rc != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  if (G_LIKELY (rc->has_entry != NULL))
    return (*rc->has_entry) (rc, key);
  else
    return (*rc->read_entry) (rc, key, FALSE) != NULL;
}



/**
 * xfce_rc_read_entry:
 * @rc       : an #XfceRc object.
 * @key      : the key to search for.
 * @fallback : a default value returned if the @key was not found.
 *
 * Reads the value of an entry specified by @key in the current group.
 *
 * Return value: the value for this @key, or @fallback if @key was not found.
 *
 * Since: 4.2
 **/
const gchar*
xfce_rc_read_entry (const XfceRc *rc,
                    const gchar  *key,
                    const gchar  *fallback)
{
  const gchar *value;

  g_return_val_if_fail (rc != NULL, fallback);
  g_return_val_if_fail (rc->read_entry != NULL, fallback);
  g_return_val_if_fail (key != NULL, fallback);

  value = (*rc->read_entry) (rc, key, TRUE);
  if (value == NULL)
    value = fallback;
  return value;
}



/**
 * xfce_rc_read_entry_untranslated:
 * @rc       : an #XfceRc object.
 * @key      : the key to search for.
 * @fallback : a default value returned if the @key was not found.
 *
 * Reads the value of an entry specified by @key in the current group. The
 * untranslated entry is returned. You normally do not need this.
 *
 * Return value: the untranslated value for this @key, or @fallback if @key was not
 *               found.
 *
 * Since: 4.2
 **/
const gchar*
xfce_rc_read_entry_untranslated (const XfceRc *rc,
                                 const gchar  *key,
                                 const gchar  *fallback)
{
  const gchar *value;

  g_return_val_if_fail (rc != NULL, fallback);
  g_return_val_if_fail (rc->read_entry != NULL, fallback);
  g_return_val_if_fail (key != NULL, fallback);

  value = (*rc->read_entry) (rc, key, FALSE);
  if (value == NULL)
    value = fallback;
  return value;
}



/**
 * xfce_rc_read_bool_entry:
 * @rc       : an #XfceRc object.
 * @key      : the key to search for.
 * @fallback : a default value returned if the @key was not found.
 *
 * Reads the value of an entry specified by @key in the current group and interpret
 * it as a boolean value. Currently "on", "true" and "yes" are accepted as true,
 * everything else is false.
 *
 * Return value: the value for this @key.
 *
 * Since: 4.2
 **/
gboolean
xfce_rc_read_bool_entry (const XfceRc *rc,
                         const gchar  *key,
                         gboolean      fallback)
{
  const gchar *value;

  value = xfce_rc_read_entry (rc, key, NULL);
  if (value != NULL)
    {
      return g_ascii_strcasecmp (value, "true") == 0
          || g_ascii_strcasecmp (value, "on") == 0
          || g_ascii_strcasecmp (value, "yes") == 0;
    }

  return fallback;
}



/**
 * xfce_rc_read_int_entry:
 * @rc       : an #XfceRc object.
 * @key      : the key to search for.
 * @fallback : a default value returned if the @key was not found.
 *
 * Reads the value of an entry specified by @key in the current group
 * and interprets it as an integer value.
 *
 * Return value: the value for this @key.
 *
 * Since: 4.2
 **/
gint
xfce_rc_read_int_entry (const XfceRc *rc,
                        const gchar  *key,
                        gint          fallback)
{
  const gchar *value;
  gchar       *endptr;
  long         result;

  value = xfce_rc_read_entry (rc, key, NULL);
  if (value != NULL)
    {
      errno = 0;
      result = strtol (value, &endptr, 10);

      if (errno != 0)
        return fallback;
      else
        return result;
    }

  return fallback;
}



/**
 * xfce_rc_read_list_entry:
 * @rc        : an #XfceRc object.
 * @key       : the key to search for.
 * @delimiter : a string which specifies the places at which to split the string.
 *              The delimiter is not included in any of the resulting strings.
 *              If NULL, "," is used.
 *
 * Reads a list of strings in the entry specified by key in the current group.
 * The returned list has to be freed using g_strfreev() when no longer needed.
 *
 * This does not support delimiter escaping. If you need this feature, use
 * g_key_file_get_string_list() instead.
 *
 * Return value: (transfer full): the list or NULL if the entry does not exist.
 *
 * Since: 4.2
 **/
gchar**
xfce_rc_read_list_entry (const XfceRc *rc,
                         const gchar  *key,
                         const gchar  *delimiter)
{
  const gchar *value;
  gchar      **result = NULL;

  if (delimiter == NULL)
    delimiter = ",";

  value = xfce_rc_read_entry (rc, key, NULL);
  if (value != NULL)
    result = g_strsplit (value, delimiter, -1);

  return result;
}



/**
 * xfce_rc_write_entry:
 * @rc    : an #XfceRc object.
 * @key   : the key to write.
 * @value : the value to write.
 *
 * Writes a @key/@value pair. This has no effect if the resource config
 * was opened readonly, else the value will be written to permanent storage
 * on the next call to #xfce_rc_flush or when @rc is destroyed using
 * #xfce_rc_close.
 *
 * If @rc was opened using #xfce_rc_config_open, the value will be
 * written to the most specific config file.
 *
 * Since: 4.2
 **/
void
xfce_rc_write_entry (XfceRc      *rc,
                     const gchar *key,
                     const gchar *value)
{
  g_return_if_fail (rc != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  if (rc->write_entry != NULL)
    (*rc->write_entry) (rc, key, value);
}



/**
 * xfce_rc_write_bool_entry:
 * @rc    : an #XfceRc object.
 * @key   : the key to write.
 * @value : the value to write.
 *
 * Wrapper for #xfce_rc_write_entry, that stores a boolean @value.
 *
 * Since: 4.2
 **/
void
xfce_rc_write_bool_entry (XfceRc      *rc,
                          const gchar *key,
                          gboolean     value)
{
  xfce_rc_write_entry (rc, key, value ? "true" : "false");
}



/**
 * xfce_rc_write_int_entry:
 * @rc    : an #XfceRc object.
 * @key   : the key to write.
 * @value : the value to write.
 *
 * Wrapper for #xfce_rc_write_entry, that stores an integer @value.
 *
 * Since: 4.2
 **/
void
xfce_rc_write_int_entry (XfceRc      *rc,
                         const gchar *key,
                         gint         value)
{
  gchar buffer[32];

  g_snprintf (buffer, 32, "%d", value);
  xfce_rc_write_entry (rc, key, buffer);
}



/**
 * xfce_rc_write_list_entry:
 * @rc        : an #XfceRc object.
 * @key       : the key to write.
 * @value     : a %NULL terminated list of strings to store in the entry specified by key.
 * @separator : the list separator. Defaults to "," if %NULL.
 *
 * Wrapper for #xfce_rc_write_entry, that stores a string list @value.
 *
 * This does not support delimiter escaping. If you need this feature, use
 * g_key_file_set_string_list() instead.
 *
 * Since: 4.2
 **/
void
xfce_rc_write_list_entry (XfceRc      *rc,
                          const gchar *key,
                          gchar      **value,
                          const gchar *separator)
{
  gchar *list;

  g_return_if_fail (value != NULL);

  if (separator == NULL)
    separator = ",";

  list = g_strjoinv (separator, value);
  xfce_rc_write_entry (rc, key, list);
  g_free (list);
}



#define __XFCE_RC_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
