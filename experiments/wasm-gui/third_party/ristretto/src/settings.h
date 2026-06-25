/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __RISTRETTO_SETTINGS_H__
#define __RISTRETTO_SETTINGS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
    SORT_TYPE_NAME = 0,
    SORT_TYPE_TYPE,
    SORT_TYPE_DATE,
    SORT_TYPE_COUNT,
} RsttoSortType;



#define RSTTO_TYPE_SETTINGS rstto_settings_get_type ()
G_DECLARE_FINAL_TYPE (RsttoSettings, rstto_settings, RSTTO, SETTINGS, GObject)

typedef struct _RsttoSettingsPrivate RsttoSettingsPrivate;

struct _RsttoSettings
{
    GObject parent;
    RsttoSettingsPrivate *priv;
};



gboolean rstto_settings_get_xfconf_disabled (void);

void rstto_settings_set_xfconf_disabled (gboolean disabled);

RsttoSettings *rstto_settings_new (void);

void
rstto_settings_set_navbar_position (RsttoSettings *settings,
                                    guint pos);
guint
rstto_settings_get_navbar_position (RsttoSettings *settings);

void
rstto_settings_set_uint_property (RsttoSettings *settings,
                                  const gchar *property_name,
                                  guint value);
guint
rstto_settings_get_uint_property (RsttoSettings *settings,
                                  const gchar *property_name);
void
rstto_settings_set_int_property (RsttoSettings *settings,
                                 const gchar *property_name,
                                 gint value);
gint
rstto_settings_get_int_property (RsttoSettings *settings,
                                 const gchar *property_name);
void
rstto_settings_set_string_property (RsttoSettings *settings,
                                    const gchar *property_name,
                                    const gchar *value);
gchar *
rstto_settings_get_string_property (RsttoSettings *settings,
                                    const gchar *property_name);
void
rstto_settings_set_boolean_property (RsttoSettings *settings,
                                     const gchar *property_name,
                                     gboolean value);
gboolean
rstto_settings_get_boolean_property (RsttoSettings *settings,
                                     const gchar *property_name);

G_END_DECLS

#endif /* __RISTRETTO_SETTINGS_H__ */
