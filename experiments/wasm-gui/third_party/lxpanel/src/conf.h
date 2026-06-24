/*
 * Copyright (c) 2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* This is a config file parser with API similar to one used by libconfig
   for convenience but contents of the file is the own config format
   therefore it is much more restricted than libconfig is.
   Strings cannot be numeric and are not quoted (similarly to INI file format).
   Groups cannot be inside other group but only inside an anonymous list.
   That anonymous list is the only list type which is supported and there
   can be only one anonymous member in any group. */

#ifndef __CONF_H__
#define __CONF_H__ 1

#include <glib.h>
#include <stdio.h>

G_BEGIN_DECLS

typedef struct _config_setting_t config_setting_t;
typedef struct _PanelConf PanelConf;

typedef enum
{
    PANEL_CONF_TYPE_GROUP,
    PANEL_CONF_TYPE_INT,
    PANEL_CONF_TYPE_STRING,
    PANEL_CONF_TYPE_LIST
} PanelConfType;

typedef void (*PanelConfSaveHook)(const config_setting_t * setting, FILE * f, gpointer user_data);

PanelConf *config_new(void);
void config_destroy(PanelConf * config);
gboolean config_read_file(PanelConf * config, const char * filename);
gboolean config_write_file(PanelConf * config, const char * filename);
char * config_setting_to_string(const config_setting_t * setting);

config_setting_t * config_root_setting(const PanelConf * config);
config_setting_t * config_setting_get_member(const config_setting_t * setting, const char * name);
config_setting_t * config_setting_get_elem(const config_setting_t * setting, unsigned int index);
const char * config_setting_get_name(const config_setting_t * setting);
config_setting_t * config_setting_get_parent(const config_setting_t * setting);

int config_setting_get_int(const config_setting_t * setting);
const char * config_setting_get_string(const config_setting_t * setting);

gboolean config_setting_lookup_int(const config_setting_t * setting,
                                   const char * name, int * value);
gboolean config_setting_lookup_string(const config_setting_t * setting,
                                      const char * name, const char ** value);

config_setting_t * config_setting_add(config_setting_t * parent, const char * name, PanelConfType type);

gboolean config_setting_move_member(config_setting_t * setting, config_setting_t * parent, const char * name);
gboolean config_setting_move_elem(config_setting_t * setting, config_setting_t * parent, int index);

gboolean config_setting_set_int(config_setting_t * setting, int value);
gboolean config_setting_set_string(config_setting_t * setting, const char * value);
gboolean config_setting_remove(config_setting_t * parent, const char * name);
gboolean config_setting_remove_elem(config_setting_t * parent, unsigned int index);
gboolean config_setting_destroy(config_setting_t * setting);

#define config_group_set_int(_group,_name,_value) \
        config_setting_set_int(config_setting_add(_group, _name, \
                                                  PANEL_CONF_TYPE_INT), \
                               _value)
#define config_group_set_string(_group,_name,_value) \
        config_setting_set_string(config_setting_add(_group, _name, \
                                                     PANEL_CONF_TYPE_STRING), \
                               _value)
#define config_group_add_subgroup(_group,_name) \
        config_setting_add(config_setting_add(_group, "", PANEL_CONF_TYPE_LIST), \
                           _name, PANEL_CONF_TYPE_GROUP)

PanelConfType config_setting_type(const config_setting_t * setting);
#define config_setting_is_group(_s) config_setting_type(_s) == PANEL_CONF_TYPE_GROUP
#define config_setting_is_list(_s) config_setting_type(_s) == PANEL_CONF_TYPE_LIST
#define config_setting_is_scalar(_s) \
        (config_setting_type(_s) == PANEL_CONF_TYPE_INT || \
         config_setting_type(_s) == PANEL_CONF_TYPE_STRING)

void config_setting_set_save_hook(config_setting_t * setting, PanelConfSaveHook hook, gpointer user_data);

G_END_DECLS

#endif /* __CONF_H__ */
