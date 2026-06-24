/*
 * Copyright (C) 2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2014 Henry Gebhardt <hsggebhardt@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "conf.h"
#include "private.h"

#include <string.h>
#include <stdlib.h>

struct _config_setting_t
{
    config_setting_t *next;
    config_setting_t *parent;
    PanelConfType type;
    PanelConfSaveHook hook;
    gpointer hook_data;
    char *name;
    union {
        gint num; /* for integer or boolean */
        gchar *str; /* for string */
        config_setting_t *first; /* for group or list */
    };
};

struct _PanelConf
{
    config_setting_t *root;
};

static config_setting_t *_config_setting_t_new(config_setting_t *parent, int index,
                                               const char *name, PanelConfType type)
{
    config_setting_t *s;
    s = g_slice_new0(config_setting_t);
    s->type = type;
    s->name = g_strdup(name);
    if (parent == NULL || (parent->type != PANEL_CONF_TYPE_GROUP && parent->type != PANEL_CONF_TYPE_LIST))
        return s;
    s->parent = parent;
    if (parent->first == NULL || index == 0)
    {
        s->next = parent->first;
        parent->first = s;
    }
    else
    {
        for (parent = parent->first; parent->next && index != 1; parent = parent->next)
            index--;
        /* FIXME: check if index is out of range? */
        s->next = parent->next;
        parent->next = s;
    }
    return s;
}

/* frees data, not removes from parent */
static void _config_setting_t_free(config_setting_t *setting)
{
    g_free(setting->name);
    switch (setting->type)
    {
    case PANEL_CONF_TYPE_STRING:
        g_free(setting->str);
        break;
    case PANEL_CONF_TYPE_GROUP:
    case PANEL_CONF_TYPE_LIST:
        while (setting->first)
        {
            config_setting_t *s = setting->first;
            setting->first = s->next;
            _config_setting_t_free(s);
        }
        break;
    case PANEL_CONF_TYPE_INT:
        break;
    }
    g_slice_free(config_setting_t, setting);
}

/* the same as above but removes from parent */
static void _config_setting_t_remove(config_setting_t *setting)
{
    g_return_if_fail(setting->parent);
    g_return_if_fail(setting->parent->type == PANEL_CONF_TYPE_GROUP || setting->parent->type == PANEL_CONF_TYPE_LIST);
    /* remove from parent */
    if (setting->parent->first == setting)
        setting->parent->first = setting->next;
    else
    {
        config_setting_t *s = setting->parent->first;
        while (s->next != NULL && s->next != setting)
            s = s->next;
        g_assert(s->next != NULL);
        s->next = setting->next;
    }
    /* free the data */
    _config_setting_t_free(setting);
}

static config_setting_t * _config_setting_get_member(const config_setting_t * setting, const char * name)
{
    config_setting_t *s;
    for (s = setting->first; s; s = s->next)
        if (g_strcmp0(s->name, name) == 0)
            break;
    return s;
}

/* returns either new or existing setting struct, NULL on error or conflict */
static config_setting_t * _config_setting_try_add(config_setting_t * parent,
                                                  const char * name,
                                                  PanelConfType type)
{
    config_setting_t *s;
    if (parent == NULL)
        return NULL;
    if (name[0] == '\0')
        return NULL;
    if (parent->type == PANEL_CONF_TYPE_GROUP &&
        (s = _config_setting_get_member(parent, name)))
        return (s->type == type) ? s : NULL;
    return _config_setting_t_new(parent, -1, name, type);
}

PanelConf *config_new(void)
{
    PanelConf *c = g_slice_new(PanelConf);
    c->root = _config_setting_t_new(NULL, -1, NULL, PANEL_CONF_TYPE_GROUP);
    return c;
}

void config_destroy(PanelConf * config)
{
    _config_setting_t_free(config->root);
    g_slice_free(PanelConf, config);
}

gboolean config_read_file(PanelConf * config, const char * filename)
{
    FILE *f = fopen(filename, "r");
    size_t size;
    char *buff, *c, *name, *end, *p;
    config_setting_t *s, *parent;

    if (f == NULL)
        return FALSE;
    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    rewind(f);
    buff = g_malloc(size + 1);
    size = fread(buff, 1, size, f);
    fclose(f);
    buff[size] = '\0';
    name = NULL;
    parent = config->root;
    for (c = buff; *c; )
    {
        switch(*c)
        {
        case '#':
_skip_all:
            while (*c && *c != '\n')
                c++;
            if (!*c)
                break;
            /* continue with EOL */
        case '\n':
            name = NULL;
            c++;
            break;
        case ' ':
        case '\t':
            if (name)
                *c = '\0';
            c++;
            break;
        case '=': /* scalar value follows */
            if (name)
                *c++ = '\0';
            else
            {
                g_warning("config: invalid scalar definition");
                goto _skip_all;
            }
            while (*c == ' ' || *c == '\t')
                c++; /* skip spaces after '=' */
            if (name == NULL || *c == '\0' || *c == '\n') /* invalid statement */
                break;
            size = strtol(c, &end, 10);
            while (*end == ' ' || *end == '\t')
                end++; /* skip trailing spaces */
            if (*end == '\0' || *end == '\n')
            {
                s = _config_setting_try_add(parent, name, PANEL_CONF_TYPE_INT);
                if (s)
                {
                    s->num = (int)size;
                    /* g_debug("config loader: got new int %s: %d", name, s->num); */
                }
                else
                    g_warning("config: duplicate setting '%s' conflicts, ignored", name);
            }
            else if (c[0] == '"')
            {
                c++;
                for (end = p = c; *end && *end != '\n' && *end != '"'; p++, end++)
                {
                    if (*end == '\\' && end[1] != '\0' && end[1] != '\n')
                    {
                        end++; /* skip quoted char */
                        if (*end == 'n') /* \n */
                            *end = '\n';
                    }
                    if (p != end)
                        *p = *end; /* move char skipping '\\' */
                }
                if (*end == '"')
                {
                    end++;
                    goto _make_string;
                }
                else /* incomplete string */
                    g_warning("config: unfinished string setting '%s', ignored", name);
            }
            else
            {
                for (end = c; *end && *end != '\n'; )
                    end++;
                p = end;
_make_string:
                s = _config_setting_try_add(parent, name, PANEL_CONF_TYPE_STRING);
                if (s)
                {
                    g_free(s->str);
                    s->str = g_strndup(c, p - c);
                    /* g_debug("config loader: got new string %s: %s", name, s->str); */
                }
                else
                    g_warning("config: duplicate setting '%s' conflicts, ignored", name);
            }
            c = end;
            break;
        case '{':
            parent = config_setting_add(parent, "", PANEL_CONF_TYPE_LIST);
            if (name)
            {
                *c = '\0';
                s = config_setting_add(parent, name, PANEL_CONF_TYPE_GROUP);
            }
            else
                s = NULL;
            c++;
            if (s)
            {
                parent = s;
                /* g_debug("config loader: group '%s' added", name); */
            }
            else
                g_warning("config: invalid group '%s' in config file ignored", name);
            name = NULL;
            break;
        case '}':
            c++;
            if (parent->parent)
                parent = parent->parent; /* go up, to anonymous list */
            if (parent->type == PANEL_CONF_TYPE_LIST)
                parent = parent->parent; /* go to upper group */
            name = NULL;
            break;
        default:
            if (name == NULL)
                name = c;
            c++;
        }
    }
    g_free(buff);
    return TRUE;
}

#define SETTING_INDENT "  "

static void _config_write_setting(const config_setting_t *setting, GString *buf,
                                  GString *out, FILE *f)
{
    gint indent = buf->len;
    config_setting_t *s;

    switch (setting->type)
    {
    case PANEL_CONF_TYPE_INT:
        g_string_append_printf(buf, "%s=%d\n", setting->name, setting->num);
        break;
    case PANEL_CONF_TYPE_STRING:
        if (!setting->str) /* don't save NULL strings */
            return;
        if (setting->str[0])
        {
            char *end;
            if (strtol(setting->str, &end, 10)) end = end;
            if (*end == '\0') /* numeric string, quote it */
            {
                g_string_append_printf(buf, "%s=\"%s\"\n", setting->name, setting->str);
                break;
            }
        }
        g_string_append_printf(buf, "%s=%s\n", setting->name, setting->str);
        break;
    case PANEL_CONF_TYPE_GROUP:
        if (!out && setting->hook) /* plugin does not support settings */
        {
            lxpanel_put_line(f, "%s%s {", buf->str, setting->name);
            setting->hook(setting, f, setting->hook_data);
            lxpanel_put_line(f, "%s}", buf->str);
            /* old settings ways are kinda weird... */
        }
        else
        {
            if (out)
            {
                g_string_append(out, buf->str);
                g_string_append(out, setting->name);
                g_string_append(out, " {\n");
            }
            else
                fprintf(f, "%s%s {\n", buf->str, setting->name);
            g_string_append(buf, SETTING_INDENT);
            for (s = setting->first; s; s = s->next)
                _config_write_setting(s, buf, out, f);
            g_string_truncate(buf, indent);
            if (out)
            {
                g_string_append(out, buf->str);
                g_string_append(out, "}\n");
            }
            else
                fprintf(f, "%s}\n", buf->str);
        }
        return;
    case PANEL_CONF_TYPE_LIST:
        if (setting->name[0] != '\0')
        {
            g_warning("only anonymous lists are supported in panel config, got \"%s\"",
                      setting->name);
            return;
        }
        for (s = setting->first; s; s = s->next)
            _config_write_setting(s, buf, out, f);
        return;
    }
    if (out)
        g_string_append(out, buf->str);
    else
        fputs(buf->str, f);
    g_string_truncate(buf, indent);
}

gboolean config_write_file(PanelConf * config, const char * filename)
{
    FILE *f = fopen(filename, "w");
    GString *str;
    if (f == NULL)
        return FALSE;
    fputs("# lxpanel <profile> config file. Manually editing is not recommended.\n"
          "# Use preference dialog in lxpanel to adjust config when you can.\n\n", f);
    str = g_string_sized_new(128);
    _config_write_setting(config_setting_get_member(config->root, ""), str, NULL, f);
    /* FIXME: handle errors */
    fclose(f);
    g_string_free(str, TRUE);
    return TRUE;
}

/* it is used for old plugins only */
char * config_setting_to_string(const config_setting_t * setting)
{
    GString *val, *buf;
    g_return_val_if_fail(setting, NULL);
    val = g_string_sized_new(128);
    buf = g_string_sized_new(128);
    _config_write_setting(setting, val, buf, NULL);
    g_string_free(val, TRUE);
    return g_string_free(buf, FALSE);
}

config_setting_t * config_root_setting(const PanelConf * config)
{
    return config->root;
}

config_setting_t * config_setting_get_member(const config_setting_t * setting, const char * name)
{
    g_return_val_if_fail(name && setting, NULL);
    g_return_val_if_fail(setting->type == PANEL_CONF_TYPE_GROUP, NULL);
    return _config_setting_get_member(setting, name);
}

config_setting_t * config_setting_get_elem(const config_setting_t * setting, unsigned int index)
{
    config_setting_t *s;
    g_return_val_if_fail(setting, NULL);
    g_return_val_if_fail(setting->type == PANEL_CONF_TYPE_LIST || setting->type == PANEL_CONF_TYPE_GROUP, NULL);
    for (s = setting->first; s && index > 0; s = s->next)
        index--;
    return s;
}

const char * config_setting_get_name(const config_setting_t * setting)
{
    return setting->name;
}

config_setting_t * config_setting_get_parent(const config_setting_t * setting)
{
    return setting->parent;
}

int config_setting_get_int(const config_setting_t * setting)
{
    if (!setting || setting->type != PANEL_CONF_TYPE_INT)
        return 0;
    return setting->num;
}

const char * config_setting_get_string(const config_setting_t * setting)
{
    if (!setting || setting->type != PANEL_CONF_TYPE_STRING)
        return NULL;
    return setting->str;
}

gboolean config_setting_lookup_int(const config_setting_t * setting,
                                   const char * name, int * value)
{
    config_setting_t *sub;

    g_return_val_if_fail(name && setting && value, FALSE);
    g_return_val_if_fail(setting->type == PANEL_CONF_TYPE_GROUP, FALSE);
    sub = _config_setting_get_member(setting, name);
    if (!sub || sub->type != PANEL_CONF_TYPE_INT)
        return FALSE;
    *value = sub->num;
    return TRUE;
}

gboolean config_setting_lookup_string(const config_setting_t * setting,
                                      const char * name, const char ** value)
{
    config_setting_t *sub;

    g_return_val_if_fail(name && setting && value, FALSE);
    g_return_val_if_fail(setting->type == PANEL_CONF_TYPE_GROUP, FALSE);
    sub = _config_setting_get_member(setting, name);
    if (!sub || sub->type != PANEL_CONF_TYPE_STRING)
        return FALSE;
    *value = sub->str;
    return TRUE;
}

/* returns either new or existing setting struct, NULL on args error,
   removes old setting on conflict */
config_setting_t * config_setting_add(config_setting_t * parent, const char * name, PanelConfType type)
{
    config_setting_t *s;
    if (parent == NULL || (parent->type != PANEL_CONF_TYPE_GROUP && parent->type != PANEL_CONF_TYPE_LIST))
        return NULL;
    if (type == PANEL_CONF_TYPE_LIST)
    {
        if (!name || name[0])
            /* only anonymous lists are supported */
            return NULL;
    }
    else if (name == NULL || name[0] == '\0')
        /* other types should be not anonymous */
        return NULL;
    if (parent->type == PANEL_CONF_TYPE_GROUP &&
        (s = _config_setting_get_member(parent, name)))
    {
        if (s->type == type)
            return s;
        _config_setting_t_remove(s);
    }
    return _config_setting_t_new(parent, -1, name, type);
}


static void remove_from_parent(config_setting_t * setting)
{
    config_setting_t *s;

    if (setting->parent->first == setting) {
        setting->parent->first = setting->next;
        goto _isolate_setting;
    }

    for (s = setting->parent->first; s->next; s = s->next)
        if (s->next == setting)
            break;
    g_assert(s->next);
    s->next = setting->next;

_isolate_setting:
    setting->next = NULL;
    setting->parent = NULL;
}

static void append_to_parent(config_setting_t * setting, config_setting_t * parent)
{
    config_setting_t *s;

    setting->parent = parent;
    if (parent->first == NULL) {
        parent->first = setting;
        return;
    }

    s = parent->first;
    while (s->next)
        s = s->next;
    s->next = setting;
}

static void insert_after(config_setting_t * setting, config_setting_t * parent,
        config_setting_t * prev)
{
    setting->parent = parent;
    if (prev == NULL) {
        setting->next = parent->first;
        parent->first = setting;
    } else {
        setting->next = prev->next;
        prev->next = setting;
    }
}

gboolean config_setting_move_member(config_setting_t * setting, config_setting_t * parent, const char * name)
{
    config_setting_t *s;

    g_return_val_if_fail(setting && setting->parent, FALSE);
    if (parent == NULL || name == NULL || parent->type != PANEL_CONF_TYPE_GROUP)
        return FALSE;
    s = _config_setting_get_member(parent, name);
    if (s) /* we cannot rename/move to this name, it exists already */
        return (s == setting);
    if (setting->parent == parent) /* it's just renaming thing */
        goto _rename;
    remove_from_parent(setting); /* remove from old parent */
    append_to_parent(setting, parent); /* add to new parent */
    /* rename if need */
    if (g_strcmp0(setting->name, name) != 0)
    {
_rename:
        g_free(setting->name);
        setting->name = g_strdup(name);
    }
    return TRUE;
}

gboolean config_setting_move_elem(config_setting_t * setting, config_setting_t * parent, int index)
{
    config_setting_t *prev = NULL;

    g_return_val_if_fail(setting && setting->parent, FALSE);
    if (parent == NULL || parent->type != PANEL_CONF_TYPE_LIST)
        return FALSE;
    if (setting->type != PANEL_CONF_TYPE_GROUP) /* we support only list of groups now */
        return FALSE;
    /* let check the place */
    if (index != 0)
    {
        prev = parent->first;
        if (prev)
            for ( ; index != 1 && prev->next; prev = prev->next)
                index--;
        if (index > 1) /* too few elements yet */
        {
_out_of_range:
            g_warning("config_setting_move_elem: index out of range");
            return FALSE;
        }
        if (prev && prev->next == setting) /* it is already there */
            return TRUE;
        if (prev == setting) /* special case: we moving it +1, swapping with next */
        {
            if (prev->next == NULL)
                goto _out_of_range;
            prev = prev->next;
        }
    }
    else if (parent->first == setting) /* it is already there */
        return TRUE;
    remove_from_parent(setting); /* remove from old parent */
    /* add to new parent */
    if (index == 0)
        g_assert(prev == NULL);
    insert_after(setting, parent, prev);
    /* don't rename  */
    return TRUE;
}

gboolean config_setting_set_int(config_setting_t * setting, int value)
{
    if (!setting || setting->type != PANEL_CONF_TYPE_INT)
        return FALSE;
    setting->num = value;
    return TRUE;
}

gboolean config_setting_set_string(config_setting_t * setting, const char * value)
{
    if (!setting || setting->type != PANEL_CONF_TYPE_STRING)
        return FALSE;
    g_free(setting->str);
    setting->str = g_strdup(value);
    return TRUE;
}

gboolean config_setting_remove(config_setting_t * parent, const char * name)
{
    config_setting_t *s = config_setting_get_member(parent, name);
    if (s == NULL)
        return FALSE;
    _config_setting_t_remove(s);
    return TRUE;
}

gboolean config_setting_remove_elem(config_setting_t * parent, unsigned int index)
{
    config_setting_t *s = config_setting_get_elem(parent, index);
    if (s == NULL)
        return FALSE;
    _config_setting_t_remove(s);
    return TRUE;
}

gboolean config_setting_destroy(config_setting_t * setting)
{
    if (setting == NULL || setting->parent == NULL)
        return FALSE;
    _config_setting_t_remove(setting);
    return TRUE;
}

PanelConfType config_setting_type(const config_setting_t * setting)
{
    return setting->type;
}

void config_setting_set_save_hook(config_setting_t * setting, PanelConfSaveHook hook,
                                  gpointer user_data)
{
    setting->hook = hook;
    setting->hook_data = user_data;
}
