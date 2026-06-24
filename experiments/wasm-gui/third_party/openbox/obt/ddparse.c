/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/ddparse.c for the Openbox window manager
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

#include "obt/ddparse.h"
#include "obt/link.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

typedef struct _ObtDDParse ObtDDParse;

/* Parses the value and adds it to the group's key_hash, with the given
   key
   Return TRUE if it is added to the hash table, and FALSE if not.
*/
typedef gboolean (*ObtDDParseValueFunc)(gchar *key, const gchar *val,
                                        ObtDDParse *parse, gboolean *error);


enum {
    DE_TYPE             = 1 << 0,
    DE_TYPE_APPLICATION = 1 << 1,
    DE_TYPE_LINK        = 1 << 2,
    DE_NAME             = 1 << 3,
    DE_EXEC             = 1 << 4,
    DE_URL              = 1 << 5
};

struct _ObtDDParse {
    gchar *filename;
    gulong lineno;
    gulong flags;
    ObtDDParseGroup *group;
    /* the key is a group name, the value is a ObtDDParseGroup */
    GHashTable *group_hash;
};

struct _ObtDDParseGroup {
    gchar *name;
    gboolean seen;
    ObtDDParseValueFunc value_func;
    /* the key is a string (a key inside the group in the .desktop).
       the value is an ObtDDParseValue */
    GHashTable *key_hash;
};

/* Displays a warning message including the file name and line number, and
   sets the boolean @error to true if it points to a non-NULL address.
*/
static void parse_error(const gchar *m, const ObtDDParse *const parse,
                        gboolean *error)
{
    if (!parse->filename)
        g_warning("%s at line %lu of input", m, parse->lineno);
    else
        g_warning("%s at line %lu of file %s",
                  m, parse->lineno, parse->filename);
    if (error) *error = TRUE;
}

static void parse_value_free(ObtDDParseValue *v)
{
    switch (v->type) {
    case OBT_DDPARSE_EXEC:
    case OBT_DDPARSE_STRING:
    case OBT_DDPARSE_LOCALESTRING:
        g_free(v->value.string); break;
    case OBT_DDPARSE_STRINGS:
    case OBT_DDPARSE_LOCALESTRINGS:
        g_strfreev(v->value.strings.a);
        v->value.strings.n = 0;
        break;
    case OBT_DDPARSE_BOOLEAN:
    case OBT_DDPARSE_NUMERIC:
    case OBT_DDPARSE_ENUM_TYPE:
    case OBT_DDPARSE_ENVIRONMENTS:
        break;
    default:
        g_assert_not_reached();
    }
    g_slice_free(ObtDDParseValue, v);
}

static ObtDDParseGroup* parse_group_new(gchar *name, ObtDDParseValueFunc f)
{
    ObtDDParseGroup *g = g_slice_new(ObtDDParseGroup);
    g->name = name;
    g->value_func = f;
    g->seen = FALSE;
    g->key_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        g_free,
                                        (GDestroyNotify)parse_value_free);
    return g;
}

static void parse_group_free(ObtDDParseGroup *g)
{
    g_free(g->name);
    g_hash_table_destroy(g->key_hash);
    g_slice_free(ObtDDParseGroup, g);
}

/*! Reads an input string, strips out invalid stuff, and parses
    backslash-stuff.
 */
static gchar* parse_value_string(const gchar *in,
                                 gboolean locale,
                                 gboolean semicolonterminate,
                                 gulong *len,
                                 const ObtDDParse *const parse,
                                 gboolean *error)
{
    gint bytes;
    gboolean backslash;
    gchar *out, *o;
    const gchar *end, *i;

    /* find the end/size of the string */
    backslash = FALSE;
    for (end = in; *end; ++end) {
        if (semicolonterminate) {
            if (backslash) backslash = FALSE;
            else if (*end == '\\') backslash = TRUE;
            else if (*end == ';') break;
        }
    }
    bytes = end - in;

    g_return_val_if_fail(in != NULL, NULL);

    if (locale && !g_utf8_validate(in, bytes, &end)) {
        parse_error("Invalid bytes in localestring", parse, error);
        bytes = end - in;
    }

    out = g_new(char, bytes + 1);
    if (len) *len = 0;
    i = in; o = out;
    backslash = FALSE;
    while (i < end) {
        const gchar *next;

        /* find the next character in the string */
        if (!locale) next = i+1;
        else if (!(next = g_utf8_find_next_char(i, end))) next = end;

        if (backslash) {
            switch(*i) {
            case 's': *o++ = ' '; break;
            case 'n': *o++ = '\n'; break;
            case 't': *o++ = '\t'; break;
            case 'r': *o++ = '\r'; break;
            case ';': *o++ = ';'; break;
            case '\\': *o++ = '\\'; break;
            default:
                parse_error((locale ?
                             "Invalid escape sequence in localestring" :
                             "Invalid escape sequence in string"),
                            parse, error);
            }
            backslash = FALSE;
        }
        else if (*i == '\\')
            backslash = TRUE;
        else if ((guchar)*i >= 127 || (guchar)*i < 32) {
            /* avoid ascii control characters */
            parse_error("Found control character in string", parse, error);
            break;
        }
        else {
            const gulong s = next-i;
            memcpy(o, i, s);
            o += s;
            if (len) *len += s;
        }
        i = next;
    }
    *o = '\0';
    return out;
}


/*! Reads a list of input strings, strips out invalid stuff, and parses
    backslash-stuff.
 */
static gchar** parse_value_strings(const gchar *in,
                                   gboolean locale,
                                   gulong *nstrings,
                                   const ObtDDParse *const parse,
                                   gboolean *error)
{
    gchar **out;
    const gchar *i;

    out = g_new(gchar*, 1);
    out[0] = NULL;
    *nstrings = 0;

    i = in;
    while (TRUE) {
        gchar *a;
        gulong len;

        a = parse_value_string(i, locale, TRUE, &len, parse, error);
        i += len;

        if (len) {
            (*nstrings)++;
            out = g_renew(gchar*, out, *nstrings+1);
            out[*nstrings-1] = a;
            out[*nstrings] = NULL;
        }

        if (!*i) break; /* no more strings */
        ++i;
    }
    return out;
}

static guint parse_value_environments(const gchar *in,
                                      const ObtDDParse *const parse,
                                      gboolean *error)
{
    const gchar *s;
    guint mask = 0;

    s = in;
    while (*s) {
        switch (*(s++)) {
        case 'G':
            if (strcmp(s, "NOME") == 0) {
                mask |= OBT_LINK_ENV_GNOME;
                s += 4;
            }
            break;
        case 'K':
            if (strcmp(s, "DE") == 0) {
                mask |= OBT_LINK_ENV_KDE;
                s += 2;
            }
            break;
        case 'L':
            if (strcmp(s, "XDE") == 0) {
                mask |= OBT_LINK_ENV_LXDE;
                s += 3;
            }
            break;
        case 'R':
            if (strcmp(s, "OX") == 0) {
                mask |= OBT_LINK_ENV_ROX;
                s += 2;
            }
            break;
        case 'X':
            if (strcmp(s, "FCE") == 0) {
                mask |= OBT_LINK_ENV_XFCE;
                s += 3;
            }
            break;
        case 'O':
            switch (*(s++)) {
            case 'l':
                if (strcmp(s, "d") == 0) {
                    mask |= OBT_LINK_ENV_OLD;
                    s += 1;
                }
                break;
            case 'P':
                if (strcmp(s, "ENBOX") == 0) {
                    mask |= OBT_LINK_ENV_OPENBOX;
                    s += 5;
                }
                break;
            }
        }
        /* find the next string, or the end of the sequence */
        while (*s && *s != ';') ++s;
    }
    return mask;
}

static gboolean parse_value_boolean(const gchar *in,
                                    const ObtDDParse *const parse,
                                    gboolean *error)
{
    if (strcmp(in, "true") == 0)
        return TRUE;
    else if (strcmp(in, "false") != 0)
        parse_error("Invalid boolean value", parse, error);
    return FALSE;
}

static gfloat parse_value_numeric(const gchar *in,
                                  const ObtDDParse *const parse,
                                  gboolean *error)
{
    gfloat out = 0;
    if (sscanf(in, "%f", &out) == 0)
        parse_error("Invalid numeric value", parse, error);
    return out;
}

static gboolean parse_file_line(FILE *f, gchar **buf,
                                gulong *size, gulong *read,
                                ObtDDParse *parse, gboolean *error)
{
    const gulong BUFMUL = 80;
    size_t ret;
    gulong i, null;

    if (*size == 0) {
        g_assert(*read == 0);
        *size = BUFMUL;
        *buf = g_new(char, *size);
    }

    /* remove everything up to a null zero already in the buffer and shift
       the rest to the front */
    null = *size;
    for (i = 0; i < *read; ++i) {
        if (null < *size)
            (*buf)[i-null-1] = (*buf)[i];
        else if ((*buf)[i] == '\0')
            null = i;
    }
    if (null < *size)
        *read -= null + 1;

    /* is there already a newline in the buffer? */
    for (i = 0; i < *read; ++i)
        if ((*buf)[i] == '\n') {
            /* turn it into a null zero and done */
            (*buf)[i] = '\0';
            return TRUE;
        }

    /* we need to read some more to find a newline */
    while (TRUE) {
        gulong eol;
        gchar *newread;

        newread = *buf + *read;
        ret = fread(newread, sizeof(char), *size-*read, f);
        if (ret < *size - *read && !feof(f)) {
            parse_error("Error reading", parse, error);
            return FALSE;
        }
        *read += ret;

        /* strip out null zeros in the input and look for an endofline */
        null = 0;
        eol = *size;
        for (i = newread-*buf; i < *read; ++i) {
            if (null > 0)
                (*buf)[i] = (*buf)[i+null];
            if ((*buf)[i] == '\0') {
                ++null;
                --(*read);
                --i; /* try again */
            }
            else if ((*buf)[i] == '\n' && eol == *size) {
                eol = i;
                /* turn it into a null zero */
                (*buf)[i] = '\0';
            }
        }

        if (eol != *size)
            /* found an endofline, done */
            break;
        else if (feof(f) && *read < *size) {
            /* found the endoffile, done (if there is space) */
            if (*read > 0) {
                /* stick a null zero on if there is test on the last line */
                (*buf)[(*read)++] = '\0';
            }
            break;
        }
        else {
            /* read more */
            size += BUFMUL;
            *buf = g_renew(char, *buf, *size);
        }
    }
    return *read > 0;
}

static void parse_group(const gchar *buf, gulong len,
                        ObtDDParse *parse, gboolean *error)
{
    ObtDDParseGroup *g;
    gchar *group;
    gulong i;

    /* get the group name */
    group = g_strndup(buf+1, len-2);
    for (i = 0; i < len-2; ++i)
        if ((guchar)group[i] < 32 || (guchar)group[i] >= 127) {
            /* valid ASCII only */
            parse_error("Invalid character found", parse, NULL);
            group[i] = '\0'; /* stopping before this character */
            break;
        }

    /* make sure it's a new group */
    g = g_hash_table_lookup(parse->group_hash, group);
    if (g && g->seen) {
        parse_error("Duplicate group found", parse, error);
        g_free(group);
        return;
    }
    /* if it's the first group, make sure it's named Desktop Entry */
    else if (!parse->group && strcmp(group, "Desktop Entry") != 0)
    {
        parse_error("Incorrect group found, "
                    "expected [Desktop Entry]",
                    parse, error);
        g_free(group);
        return;
    }
    else {
        if (!g) {
            g = parse_group_new(group, NULL);
            g_hash_table_insert(parse->group_hash, g->name, g);
        }
        else
            g_free(group);

        g->seen = TRUE;
        parse->group = g;
        g_print("Found group %s\n", g->name);
    }
}

static void parse_key_value(const gchar *buf, gulong len,
                            ObtDDParse *parse, gboolean *error)
{
    gulong i, keyend, valstart, eq;
    char *key;

    /* find the end of the key */
    for (i = 0; i < len; ++i)
        if (!(((guchar)buf[i] >= 'A' && (guchar)buf[i] <= 'Z') ||
              ((guchar)buf[i] >= 'a' && (guchar)buf[i] <= 'z') ||
              ((guchar)buf[i] >= '0' && (guchar)buf[i] <= '9') ||
              ((guchar)buf[i] == '-'))) {
            /* not part of the key */
            break;
        }
    keyend = i;

    if (keyend < 1) {
        parse_error("Empty key", parse, error);
        return;
    }
    /* find the = character */
    for (i = keyend; i < len; ++i) {
        if (buf[i] == '=') {
            eq = i;
            break;
        }
        else if (buf[i] != ' ') {
            parse_error("Invalid character in key name", parse, error);
            return ;
        }
    }
    if (i == len) {
        parse_error("Key without value found", parse, error);
        return;
    }
    /* find the start of the value */
    for (i = eq+1; i < len; ++i)
        if (buf[i] != ' ') {
            valstart = i;
            break;
        }
    if (i == len) {
        parse_error("Empty value found", parse, error);
        return;
    }

    key = g_strndup(buf, keyend);
    if (g_hash_table_lookup(parse->group->key_hash, key)) {
        parse_error("Duplicate key found", parse, error);
        g_free(key);
        return;
    }
    g_print("Found key/value %s=%s.\n", key, buf+valstart);
    if (parse->group->value_func)
        if (!parse->group->value_func(key, buf+valstart, parse, error)) {
            parse_error("Unknown key", parse, error);
            g_free(key);
        }
}

static gboolean parse_file(FILE *f, ObtDDParse *parse)
{
    gchar *buf = NULL;
    gulong bytes = 0, read = 0;
    gboolean error = FALSE;

    while (!error && parse_file_line(f, &buf, &bytes, &read, parse, &error)) {
        gulong len = strlen(buf);
        if (buf[0] == '#' || buf[0] == '\0')
            ; /* ignore comment lines */
        else if (buf[0] == '[' && buf[len-1] == ']')
            parse_group(buf, len, parse, &error);
        else if (!parse->group)
            /* just ignore keys outside of groups */
            parse_error("Key found before group", parse, NULL);
        else
            /* ignore errors in key-value pairs and continue */
            parse_key_value(buf, len, parse, NULL);
        ++parse->lineno;
    }

    if (buf) g_free(buf);
    return !error;
}

static gboolean parse_desktop_entry_value(gchar *key, const gchar *val,
                                          ObtDDParse *parse, gboolean *error)
{
    ObtDDParseValue v, *pv;

    switch (key[0]) {
    case 'C':
        switch (key[1]) {
        case 'a': /* Categories */
            if (strcmp(key+2, "tegories")) return FALSE;
            v.type = OBT_DDPARSE_STRINGS; break;
        case 'o': /* Comment */
            if (strcmp(key+2, "mment")) return FALSE;
            v.type = OBT_DDPARSE_LOCALESTRING; break;
        default:
            return FALSE;
        }
        break;
    case 'E': /* Exec */
        if (strcmp(key+1, "xec")) return FALSE;
        v.type = OBT_DDPARSE_EXEC; parse->flags |= DE_EXEC; break;
    case 'G': /* GenericName */
        if (strcmp(key+1, "enericName")) return FALSE;
        v.type = OBT_DDPARSE_LOCALESTRING; break;
    case 'I': /* Icon */
        if (strcmp(key+1, "con")) return FALSE;
        v.type = OBT_DDPARSE_LOCALESTRING; break;
    case 'H': /* Hidden */
        if (strcmp(key+1, "idden")) return FALSE;
        v.type = OBT_DDPARSE_BOOLEAN; break;
    case 'M': /* MimeType */
        if (strcmp(key+1, "imeType")) return FALSE;
        v.type = OBT_DDPARSE_STRINGS; break;
    case 'N':
        switch (key[1]) {
        case 'a': /* Name */
            if (strcmp(key+2, "me")) return FALSE;
            v.type = OBT_DDPARSE_LOCALESTRING; parse->flags |= DE_NAME; break;
        case 'o':
            switch (key[2]) {
            case 'D': /* NoDisplay */
                if (strcmp(key+3, "isplay")) return FALSE;
                v.type = OBT_DDPARSE_BOOLEAN; break;
            case 't': /* NotShowIn */
                if (strcmp(key+3, "ShowIn")) return FALSE;
                v.type = OBT_DDPARSE_STRINGS; break;
            default:
                return FALSE;
            }
            break;
        default:
            return FALSE;
        }
        break;
    case 'P': /* Path */
        if (strcmp(key+1, "ath")) return FALSE;
        v.type = OBT_DDPARSE_STRING; break;
    case 'S': /* Path */
        if (key[1] == 't' && key[2] == 'a' && key[3] == 'r' &&
            key[4] == 't' && key[5] == 'u' && key[6] == 'p')
            switch (key[7]) {
            case 'N': /* StartupNotify */
                if (strcmp(key+8, "otify")) return FALSE;
                v.type = OBT_DDPARSE_BOOLEAN; break;
            case 'W': /* StartupWMClass */
                if (strcmp(key+8, "MClass")) return FALSE;
                v.type = OBT_DDPARSE_STRING; break;
            default:
                return FALSE;
            }
        else
            return FALSE;
        break;
    case 'T':
        switch (key[1]) {
        case 'e': /* Terminal */
            if (strcmp(key+2, "rminal")) return FALSE;
            v.type = OBT_DDPARSE_BOOLEAN; break;
        case 'r': /* TryExec */
            if (strcmp(key+2, "yExec")) return FALSE;
            v.type = OBT_DDPARSE_STRING; break;
        case 'y': /* Type */
            if (strcmp(key+2, "pe")) return FALSE;
            v.type = OBT_DDPARSE_ENUM_TYPE; parse->flags |= DE_TYPE; break;
        default:
            return FALSE;
        }
        break;
    case 'U': /* URL */
        if (strcmp(key+1, "RL")) return FALSE;
        v.type = OBT_DDPARSE_STRING; parse->flags |= DE_URL; break;
    case 'V': /* MimeType */
        if (strcmp(key+1, "ersion")) return FALSE;
        v.type = OBT_DDPARSE_STRING; break;
    default:
        return FALSE;
    }

    /* parse the value */
    switch (v.type) {
    case OBT_DDPARSE_EXEC: {
        gchar *c, *m;
        gboolean percent;
        gboolean found;

        v.value.string = parse_value_string(val, FALSE, FALSE, NULL,
                                            parse, error);
        g_assert(v.value.string);

        /* an exec string can only contain one of the file/url-opening %'s */
        percent = found = FALSE;
        for (c = v.value.string; *c; ++c) {
            if (percent) {
                switch (*c) {
                case 'f':
                case 'F':
                case 'u':
                case 'U':
                    if (found) {
                        m = g_strdup_printf("Malformed Exec key, "
                                            "extraneous %%%c", *c);
                        parse_error(m, parse, error);
                        g_free(m);
                    }
                    found = TRUE;
                    break;
                case 'd':
                case 'D':
                case 'n':
                case 'N':
                case 'v':
                case 'm':
                    m = g_strdup_printf("Malformed Exec key, "
                                        "uses deprecated %%%c", *c);
                    parse_error(m, parse, NULL); /* just a warning */
                    g_free(m);
                    break;
                case 'i':
                case 'c':
                case 'k':
                case '%':
                    break;
                default:
                    m = g_strdup_printf("Malformed Exec key, "
                                        "uses unknown %%%c", *c);
                    parse_error(m, parse, NULL); /* just a warning */
                    g_free(m);
                }
                percent = FALSE;
            }
            else if (*c == '%') percent = TRUE;
        }
        break;
    }
    case OBT_DDPARSE_STRING:
        v.value.string = parse_value_string(val, FALSE, FALSE, NULL,
                                            parse, error);
        g_assert(v.value.string);
        break;
    case OBT_DDPARSE_LOCALESTRING:
        v.value.string = parse_value_string(val, TRUE, FALSE, NULL,
                                            parse, error);
        g_assert(v.value.string);
        break;
    case OBT_DDPARSE_STRINGS:
        v.value.strings.a = parse_value_strings(val, FALSE, &v.value.strings.n,
                                                parse, error);
        g_assert(v.value.strings.a);
        g_assert(v.value.strings.n);
        break;
    case OBT_DDPARSE_LOCALESTRINGS:
        v.value.strings.a = parse_value_strings(val, TRUE, &v.value.strings.n,
                                                parse, error);
        g_assert(v.value.strings.a);
        g_assert(v.value.strings.n);
        break;
    case OBT_DDPARSE_BOOLEAN:
        v.value.boolean = parse_value_boolean(val, parse, error);
        break;
    case OBT_DDPARSE_NUMERIC:
        v.value.numeric = parse_value_numeric(val, parse, error);
        break;
    case OBT_DDPARSE_ENUM_TYPE:
        if (val[0] == 'A' && strcmp(val+1, "pplication") == 0) {
            v.value.enumerable = OBT_LINK_TYPE_APPLICATION;
            parse->flags |= DE_TYPE_APPLICATION;
        }
        else if (val[0] == 'L' && strcmp(val+1, "ink") == 0) {
            v.value.enumerable = OBT_LINK_TYPE_URL;
            parse->flags |= DE_TYPE_LINK;
        }
        else if (val[0] == 'D' && strcmp(val+1, "irectory") == 0)
            v.value.enumerable = OBT_LINK_TYPE_DIRECTORY;
        else {
            parse_error("Unknown Type", parse, error);
            return FALSE;
        }
        break;
    case OBT_DDPARSE_ENVIRONMENTS:
        v.value.environments = parse_value_environments(val, parse, error);
        break;
    default:
        g_assert_not_reached();
    }

    pv = g_slice_new(ObtDDParseValue);
    *pv = v;
    g_hash_table_insert(parse->group->key_hash, key, pv);
    return TRUE;
}

GHashTable* obt_ddparse_file(const gchar *name, GSList *paths)
{
    ObtDDParse parse;
    ObtDDParseGroup *desktop_entry;
    GSList *it;
    FILE *f;
    gboolean success;

    parse.filename = NULL;
    parse.lineno = 0;
    parse.group = NULL;
    parse.group_hash = g_hash_table_new_full(g_str_hash,
                                             g_str_equal,
                                             NULL,
                                             (GDestroyNotify)parse_group_free);

    /* set up the groups (there's only one right now) */
    desktop_entry = parse_group_new(g_strdup("Desktop Entry"),
                                    parse_desktop_entry_value);
    g_hash_table_insert(parse.group_hash, desktop_entry->name, desktop_entry);

    success = FALSE;
    for (it = paths; it && !success; it = g_slist_next(it)) {
        gchar *path = g_strdup_printf("%s/%s", (char*)it->data, name);
        if ((f = fopen(path, "r"))) {
            parse.filename = path;
            parse.lineno = 1;
            parse.flags = 0;
            if ((success = parse_file(f, &parse))) {
                /* check that required keys exist */

                if (!(parse.flags & DE_TYPE)) {
                    g_warning("Missing Type key in %s", path);
                    success = FALSE;
                }
                if (!(parse.flags & DE_NAME)) {
                    g_warning("Missing Name key in %s", path);
                    success = FALSE;
                }
                if (parse.flags & DE_TYPE_APPLICATION &&
                    !(parse.flags & DE_EXEC))
                {
                    g_warning("Missing Exec key for Application in %s",
                              path);
                    success = FALSE;
                }
                else if (parse.flags & DE_TYPE_LINK && !(parse.flags & DE_URL))
                {
                    g_warning("Missing URL key for Link in %s", path);
                    success = FALSE;
                }
            }
            fclose(f);
        }
        g_free(path);
    }
    if (!success) {
        g_hash_table_destroy(parse.group_hash);
        parse.group_hash = NULL;
    }
    return parse.group_hash;
}

GHashTable* obt_ddparse_group_keys(ObtDDParseGroup *g)
{
    return g->key_hash;
}
