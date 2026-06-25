/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundatoin; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundatoin, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <migrate/migrate-default.h>
#include <libxfce4panel/xfce-panel-macros.h>



typedef struct
{
  XfconfChannel *channel;
  GSList        *path;
  GPtrArray     *array;
}
ConfigParser;



static GType
migrate_default_gtype_from_string (const gchar *type)
{
  if (strcmp (type, "string") == 0)
    return G_TYPE_STRING;
  else if (strcmp (type, "uint") == 0)
    return G_TYPE_UINT;
  else if (strcmp (type, "int") == 0)
    return G_TYPE_INT;
  else if (strcmp (type, "double") == 0)
    return G_TYPE_DOUBLE;
  else if (strcmp (type, "bool") == 0)
    return G_TYPE_BOOLEAN;
  else if (strcmp (type, "array") == 0)
    return G_TYPE_BOXED;
  else if (strcmp (type, "empty") == 0)
    return G_TYPE_NONE;

  return G_TYPE_INVALID;
}



static void
migrate_default_set_value (GValue      *value,
                           const gchar *string)
{
  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_STRING:
      g_value_set_string (value, string);
      break;

    case G_TYPE_UINT:
      g_value_set_uint (value, strtol (string, NULL, 0));
      break;

    case G_TYPE_INT:
      g_value_set_int (value, strtoul (string, NULL, 0));
      break;

    case G_TYPE_DOUBLE:
      g_value_set_double (value, g_ascii_strtod (string, NULL));
      break;

    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, strcmp (string, "true") == 0);
      break;

    }
}



static gchar *
migrate_default_property_path (ConfigParser *parser)
{
  GSList  *li;
  GString *path;

  path = g_string_new (NULL);

  for (li = parser->path; li != NULL; li = li->next)
    {
      g_string_append_c (path, '/');
      g_string_append (path, (const gchar *) li->data);
    }

  return g_string_free (path, FALSE);
}



static void
migrate_default_start_element_handler (GMarkupParseContext  *context,
                                       const gchar          *element_name,
                                       const gchar         **attribute_names,
                                       const gchar         **attribute_values,
                                       gpointer              user_data,
                                       GError              **error)
{
  ConfigParser *parser = user_data;
  guint         i;
  const gchar  *channel_name;
  const gchar  *prop_name, *prop_value, *prop_type;
  GType         type;
  gchar        *prop_path;
  GValue        value = { 0, };
  const gchar  *value_value, *value_type;
  GValue       *value2;

  if (strcmp (element_name, "channel") == 0)
    {
      channel_name = NULL;

      if (G_LIKELY (attribute_names != NULL))
        {
          for (i = 0; attribute_names[i] != NULL; i++)
            {
              if (strcmp (attribute_names[i], "name") == 0)
                {
                  /* instead of `channel_name = attribute_values[i];`,
                   * this is an xfce4-panel workaround to make it work
                   * with the custom channel names */
                  channel_name = XFCE_PANEL_CHANNEL_NAME;
                }
            }
        }

      if (channel_name != NULL)
        {
          /* open the xfconf channel */
          parser->channel = xfconf_channel_get (channel_name);
        }
      else
        {
          g_set_error_literal (error, G_MARKUP_ERROR_INVALID_CONTENT, G_MARKUP_ERROR,
                               "The channel element has no name attribute");
        }
    }
  else if (strcmp (element_name, "property") == 0)
    {
      prop_name = NULL;
      prop_value = NULL;
      prop_type = NULL;

      /* check if we need to flush an array */
      if (parser->array != NULL)
        {
          prop_path = migrate_default_property_path (parser);
          xfconf_channel_set_arrayv (parser->channel, prop_path, parser->array);
          g_free (prop_path);

          xfconf_array_free (parser->array);
          parser->array = NULL;
        }

      if (G_LIKELY (attribute_names != NULL))
        {
          for (i = 0; attribute_names[i] != NULL; i++)
            {
              if (strcmp (attribute_names[i], "name") == 0)
                prop_name = attribute_values[i];
              else if (strcmp (attribute_names[i], "value") == 0)
                prop_value = attribute_values[i];
              else if (strcmp (attribute_names[i], "type") == 0)
                prop_type = attribute_values[i];
            }
        }

      if (prop_name != NULL && prop_type != NULL)
        {
          type = migrate_default_gtype_from_string (prop_type);

          parser->path = g_slist_append (parser->path, g_strdup (prop_name));

          if (type == G_TYPE_INVALID)
            {
              g_set_error (error, G_MARKUP_ERROR_INVALID_CONTENT, G_MARKUP_ERROR,
                           "Property \"%s\" has invalid type \"%s\"",
                           prop_name, prop_type);
            }
          if (type == G_TYPE_BOXED)
            {
              parser->array = g_ptr_array_new ();
            }
          else if (type != G_TYPE_NONE && prop_value != NULL)
            {
              g_value_init (&value, type);
              migrate_default_set_value (&value, prop_value);

              prop_path = migrate_default_property_path (parser);
              xfconf_channel_set_property (parser->channel, prop_path, &value);
              g_free (prop_path);

              g_value_unset (&value);
            }
        }
      else
        {
          g_set_error (error, G_MARKUP_ERROR_INVALID_CONTENT, G_MARKUP_ERROR,
                       "Found property without name (%s), type (%s) or value (%s)",
                       prop_name, prop_type, prop_value);
        }
    }
  else if (strcmp (element_name, "value") == 0)
    {
      if (parser->array != NULL)
        {
          value_type = NULL;
          value_value = NULL;

          if (G_LIKELY (attribute_names != NULL))
            {
              for (i = 0; attribute_names[i] != NULL; i++)
                {
                  if (strcmp (attribute_names[i], "type") == 0)
                    value_type = attribute_values[i];
                  else if (strcmp (attribute_names[i], "value") == 0)
                    value_value = attribute_values[i];
                }
            }

          if (value_type != NULL && value_value != NULL)
            {
              type = migrate_default_gtype_from_string (value_type);

              if (type != G_TYPE_INVALID && type != G_TYPE_NONE && type != G_TYPE_BOXED)
                {
                  value2 = g_new0 (GValue, 1);
                  g_value_init (value2, type);

                  migrate_default_set_value (value2, value_value);

                  g_ptr_array_add (parser->array, value2);
                }
              else
                {
                  g_set_error (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                               "Found value has unknown type \"%s\"", value_type);
                }
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                           "Found value element with missing type (%s) or value (%s)",
                           value_type, value_value);
            }
        }
      else
        {
          g_set_error_literal (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                               "Found value element without an array property");
        }
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                   "Unknown start element \"%s\"", element_name);
    }
}



static void
migrate_default_end_element_handler (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     gpointer              user_data,
                                     GError              **error)
{
  ConfigParser *parser = user_data;
  GSList       *li;
  gchar        *prop_path;

  if (strcmp (element_name, "channel") == 0)
    {
      if (G_LIKELY (parser->channel != NULL))
        parser->channel = NULL;

     if (parser->path != NULL)
       {
         g_set_error_literal (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                              "Property path still contains items");
       }
    }
  else if (strcmp (element_name, "property") == 0)
    {
      if (parser->array != NULL)
        {
          prop_path = migrate_default_property_path (parser);
          xfconf_channel_set_arrayv (parser->channel, prop_path, parser->array);
          g_free (prop_path);

          xfconf_array_free (parser->array);
          parser->array = NULL;
        }

      li = g_slist_last (parser->path);
      if (li != NULL)
        {
          g_free (li->data);
          parser->path = g_slist_delete_link (parser->path, li);
        }
      else
        {
          g_set_error_literal (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                              "Element could no be popped from the path");
        }
    }
  else if (strcmp (element_name, "value") == 0)
    {
      /* nothing to do */
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR_UNKNOWN_ELEMENT, G_MARKUP_ERROR,
                   "Unknown end element \"%s\"", element_name);
    }
}



static GMarkupParser markup_parser =
{
  migrate_default_start_element_handler,
  migrate_default_end_element_handler,
  NULL,
  NULL,
  NULL
};



gboolean
migrate_default (const gchar    *filename,
                 GError        **error)
{
  gsize                length;
  gchar               *contents;
  GMarkupParseContext *context;
  ConfigParser        *parser;
  gboolean             succeed = FALSE;

  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!g_file_get_contents (filename, &contents, &length, error))
    return FALSE;

  parser = g_slice_new0 (ConfigParser);
  parser->path = NULL;

  context = g_markup_parse_context_new (&markup_parser, 0, parser, NULL);

  if (g_markup_parse_context_parse (context, contents, length, error))
    {
      /* check if the entire file is parsed */
      if (g_markup_parse_context_end_parse (context, error))
        succeed = TRUE;
    }

  g_free (contents);
  g_markup_parse_context_free (context);
  g_slice_free (ConfigParser, parser);

  return succeed;
}
