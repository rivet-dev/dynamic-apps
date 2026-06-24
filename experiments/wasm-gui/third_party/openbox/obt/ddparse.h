/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/ddparse.h for the Openbox window manager
   Copyright (c) 2009        Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include <glib.h>

typedef struct _ObtDDParseGroup ObtDDParseGroup;

typedef enum {
    OBT_DDPARSE_EXEC,
    OBT_DDPARSE_STRING,
    OBT_DDPARSE_LOCALESTRING,
    OBT_DDPARSE_STRINGS,
    OBT_DDPARSE_LOCALESTRINGS,
    OBT_DDPARSE_BOOLEAN,
    OBT_DDPARSE_NUMERIC,
    OBT_DDPARSE_ENUM_TYPE,
    OBT_DDPARSE_ENVIRONMENTS,
    OBT_DDPARSE_NUM_VALUE_TYPES
} ObtDDParseValueType;

typedef struct _ObtDDParseValue {
    ObtDDParseValueType type;
    union _ObtDDParseValueValue {
        gchar *string;
        struct _ObtDDParseValueStrings {
            gchar **a;
            gulong n;
        } strings;
        gboolean boolean;
        gfloat numeric;
        guint enumerable;
        guint environments; /*!< A mask of flags from ObtLinkEnvMask */
    } value;
} ObtDDParseValue;

/* Returns a hash table where the keys are groups, and the values are
   ObtDDParseGroups */
GHashTable* obt_ddparse_file(const gchar *name, GSList *paths);

/* Returns a hash table where the keys are "keys" in the .desktop file,
   and the values are "values" in the .desktop file, for the group @g. */
GHashTable* obt_ddparse_group_keys(ObtDDParseGroup *g);
