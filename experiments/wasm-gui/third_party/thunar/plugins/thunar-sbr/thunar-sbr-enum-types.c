/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <thunar-sbr/thunar-sbr-enum-types.h>



static GType case_renamer_mode_type;
static GType insert_mode_type;
static GType number_mode_type;
static GType offset_mode_type;
static GType text_mode_type;
static GType date_mode_type;



GType
thunar_sbr_case_renamer_mode_get_type (void)
{
  return case_renamer_mode_type;
}



GType
thunar_sbr_insert_mode_get_type (void)
{
  return insert_mode_type;
}



GType
thunar_sbr_number_mode_get_type (void)
{
  return number_mode_type;
}



GType
thunar_sbr_offset_mode_get_type (void)
{
  return offset_mode_type;
}



GType
thunar_sbr_text_mode_get_type (void)
{
  return text_mode_type;
}



GType
thunar_sbr_date_mode_get_type (void)
{
  return date_mode_type;
}



void
thunar_sbr_register_enum_types (ThunarxProviderPlugin *plugin)
{
  static const GEnumValue case_renamer_mode_values[] =
  {
    { THUNAR_SBR_CASE_RENAMER_MODE_LOWER,       "THUNAR_SBR_CASE_RENAMER_MODE_LOWER",       N_ ("lowercase"),                 },
    { THUNAR_SBR_CASE_RENAMER_MODE_UPPER,       "THUNAR_SBR_CASE_RENAMER_MODE_UPPER",       N_ ("UPPERCASE"),                 },
    { THUNAR_SBR_CASE_RENAMER_MODE_TITLE,       "THUNAR_SBR_CASE_RENAMER_MODE_TITLE",       N_ ("Title Case"),                 },
    { THUNAR_SBR_CASE_RENAMER_MODE_FIRST_UPPER, "THUNAR_SBR_CASE_RENAMER_MODE_FIRST_UPPER", N_ ("First character uppercase"), },
    { 0,                                        NULL,                                       NULL,                             },
  };

  static const GEnumValue insert_mode_values[] =
  {
    { THUNAR_SBR_INSERT_MODE_INSERT,    "THUNAR_SBR_INSERT_MODE_INSERT",    N_ ("Insert"),    },
    { THUNAR_SBR_INSERT_MODE_OVERWRITE, "THUNAR_SBR_INSERT_MODE_OVERWRITE", N_ ("Overwrite"), },
    { 0,                                NULL,                               NULL,             },
  };

  static const GEnumValue number_mode_values[] =
  {
    { THUNAR_SBR_NUMBER_MODE_123,              "THUNAR_SBR_NUMBER_MODE_123",             N_ ("1, 2, 3, ..."),             },
    { THUNAR_SBR_NUMBER_MODE_010203,           "THUNAR_SBR_NUMBER_MODE_010203",          N_ ("01, 02, 03, ..."),          },
    { THUNAR_SBR_NUMBER_MODE_001002003,        "THUNAR_SBR_NUMBER_MODE_001002003",       N_ ("001, 002, 003, ..."),       },
    { THUNAR_SBR_NUMBER_MODE_000100020003,     "THUNAR_SBR_NUMBER_MODE_000100020003",    N_ ("0001, 0002, 0003, ..."),    },
    { THUNAR_SBR_NUMBER_MODE_000010000200003,  "THUNAR_SBR_NUMBER_MODE_000010000200003", N_ ("00001, 00002, 00003, ..."), },
    { THUNAR_SBR_NUMBER_MODE_ABC,              "THUNAR_SBR_NUMBER_MODE_ABC",             N_ ("a, b, c, d, ..."),          },
    { 0,                                        NULL,                                    NULL,                            },
  };

  static const GEnumValue offset_mode_values[] =
  {
    { THUNAR_SBR_OFFSET_MODE_LEFT,  "THUNAR_SBR_OFFSET_MODE_LEFT",  N_ ("From the front (left)"),  },
    { THUNAR_SBR_OFFSET_MODE_RIGHT, "THUNAR_SBR_OFFSET_MODE_RIGHT", N_ ("From the back (right)"), },
    { 0,                            NULL,                           NULL,              },
  };

  static const GEnumValue text_mode_values[] =
  {
    { THUNAR_SBR_TEXT_MODE_OTN, "THUNAR_SBR_TEXT_MODE_OTN", N_ ("Old Name - Text - Number"), },
    { THUNAR_SBR_TEXT_MODE_NTO, "THUNAR_SBR_TEXT_MODE_NTO", N_ ("Number - Text - Old Name"), },
    { THUNAR_SBR_TEXT_MODE_TN,  "THUNAR_SBR_TEXT_MODE_TN",  N_ ("Text - Number"),            },
    { THUNAR_SBR_TEXT_MODE_NT,  "THUNAR_SBR_TEXT_MODE_NT",  N_ ("Number - Text"),            },
    { 0,                        NULL,                       NULL,                            },
  };

  static const GEnumValue date_mode_values[] =
  {
    { THUNAR_SBR_DATE_MODE_NOW,    "THUNAR_SBR_DATE_MODE_NOW",   N_ ("Current"),            },
    { THUNAR_SBR_DATE_MODE_ATIME,  "THUNAR_SBR_DATE_MODE_ATIME", N_ ("Date Accessed"),      },
    { THUNAR_SBR_DATE_MODE_MTIME,  "THUNAR_SBR_DATE_MODE_MTIME", N_ ("Date Modified"),      },
#ifdef HAVE_EXIF
    { THUNAR_SBR_DATE_MODE_TAKEN,  "THUNAR_SBR_DATE_MODE_TAKEN", N_ ("Date Picture Taken"), },
#endif
    { 0,                           NULL,                         NULL,                      },
  };

  case_renamer_mode_type = thunarx_provider_plugin_register_enum (plugin, "ThunarSbrCaseRenamerMode", case_renamer_mode_values);
  insert_mode_type = thunarx_provider_plugin_register_enum (plugin, "ThunarSbrInsertMode", insert_mode_values);
  number_mode_type = thunarx_provider_plugin_register_enum (plugin, "ThunarSbrNumberMode", number_mode_values);
  offset_mode_type = thunarx_provider_plugin_register_enum (plugin, "ThunarSbrOffsetMode", offset_mode_values);
  text_mode_type = thunarx_provider_plugin_register_enum (plugin, "ThunarSbrTextMode", text_mode_values);
  date_mode_type = thunarx_provider_plugin_register_enum (plugin, "ThunarSbrDateMode", date_mode_values);
}




