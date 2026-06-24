/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/link.c for the Openbox window manager
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

#include "obt/link.h"
#include "obt/ddparse.h"
#include "obt/paths.h"
#include <glib.h>

struct _ObtLink {
    guint ref;

    ObtLinkType type;
    gchar *name; /*!< Specific name for the object (eg Firefox) */
    gboolean display; /*<! When false, do not display this link in menus or
                           launchers, etc */
    gboolean deleted; /*<! When true, the Link could exist but is deleted
                           for the current user */
    gchar *generic; /*!< Generic name for the object (eg Web Browser) */
    gchar *comment; /*!< Comment/description to display for the object */
    gchar *icon; /*!< Name/path for an icon for the object */
    guint env_required; /*!< The environments that must be present to use this
                          link. */
    guint env_restricted; /*!< The environments that must _not_ be present to
                            use this link. */

    union _ObtLinkData {
        struct _ObtLinkApp {
            gchar *exec; /*!< Executable to run for the app */
            gchar *wdir; /*!< Working dir to run the app in */
            gboolean term; /*!< Run the app in a terminal or not */
            ObtLinkAppOpen open;

            gchar **mime; /*!< Mime types the app can open */

            GQuark *categories; /*!< Array of quarks representing the
                                  application's categories */
            gulong  n_categories; /*!< Number of categories for the app */

            ObtLinkAppStartup startup;
            gchar *startup_wmclass;
        } app;
        struct _ObtLinkLink {
            gchar *addr;
        } url;
        struct _ObtLinkDir {
        } dir;
    } d;
};

ObtLink* obt_link_from_ddfile(const gchar *ddname, GSList *paths,
                              ObtPaths *p)
{
    ObtLink *link;
    GHashTable *groups, *keys;
    ObtDDParseGroup *g;
    ObtDDParseValue *v;

    /* parse the file, and get a hash table of the groups */
    groups = obt_ddparse_file(ddname, paths);
    if (!groups) return NULL; /* parsing failed */
    /* grab the Desktop Entry group */
    g = g_hash_table_lookup(groups, "Desktop Entry");
    g_assert(g != NULL);
    /* grab the keys that appeared in the Desktop Entry group */
    keys = obt_ddparse_group_keys(g);

    /* build the ObtLink (we steal all strings from the parser) */
    link = g_slice_new0(ObtLink);
    link->ref = 1;
    link->display = TRUE;

    v = g_hash_table_lookup(keys, "Type");
    g_assert(v);
    link->type = v->value.enumerable;

    if ((v = g_hash_table_lookup(keys, "Hidden")))
        link->deleted = v->value.boolean;

    if ((v = g_hash_table_lookup(keys, "NoDisplay")))
        link->display = !v->value.boolean;

    if ((v = g_hash_table_lookup(keys, "GenericName")))
        link->generic = v->value.string, v->value.string = NULL;

    if ((v = g_hash_table_lookup(keys, "Comment")))
        link->comment = v->value.string, v->value.string = NULL;

    if ((v = g_hash_table_lookup(keys, "Icon")))
        link->icon = v->value.string, v->value.string = NULL;

    if ((v = g_hash_table_lookup(keys, "OnlyShowIn")))
        link->env_required = v->value.environments;
    else
        link->env_required = 0;

    if ((v = g_hash_table_lookup(keys, "NotShowIn")))
        link->env_restricted = v->value.environments;
    else
        link->env_restricted = 0;

    /* type-specific keys */

    if (link->type == OBT_LINK_TYPE_APPLICATION) {
        gchar *c;
        gboolean percent;

        v = g_hash_table_lookup(keys, "Exec");
        g_assert(v);
        link->d.app.exec = v->value.string;
        v->value.string = NULL;

        /* parse link->d.app.exec to determine link->d.app.open */
        percent = FALSE;
        for (c = link->d.app.exec; *c; ++c) {
            if (percent) {
                switch (*c) {
                case 'f': link->d.app.open = OBT_LINK_APP_SINGLE_LOCAL; break;
                case 'F': link->d.app.open = OBT_LINK_APP_MULTI_LOCAL; break;
                case 'u': link->d.app.open = OBT_LINK_APP_SINGLE_URL; break;
                case 'U': link->d.app.open = OBT_LINK_APP_MULTI_URL; break;
                default: percent = FALSE;
                }
                if (percent) break; /* found f/F/u/U */
            }
            else if (*c == '%') percent = TRUE;
        }

        if ((v = g_hash_table_lookup(keys, "TryExec"))) {
            /* XXX spawn a thread to check TryExec? */
            link->display = link->display &&
                obt_paths_try_exec(p, v->value.string);
        }

        if ((v = g_hash_table_lookup(keys, "Path"))) {
            /* steal the string */
            link->d.app.wdir = v->value.string;
            v->value.string = NULL;
        }

        if ((v = g_hash_table_lookup(keys, "Terminal")))
            link->d.app.term = v->value.boolean;

        if ((v = g_hash_table_lookup(keys, "StartupNotify")))
            link->d.app.startup = v->value.boolean ?
                OBT_LINK_APP_STARTUP_PROTOCOL_SUPPORT :
                OBT_LINK_APP_STARTUP_NO_SUPPORT;
        else {
            link->d.app.startup = OBT_LINK_APP_STARTUP_LEGACY_SUPPORT;
            if ((v = g_hash_table_lookup(keys, "StartupWMClass"))) {
                /* steal the string */
                link->d.app.startup_wmclass = v->value.string;
                v->value.string = NULL;
            }
        }

        if ((v = g_hash_table_lookup(keys, "Categories"))) {
            gulong i;
            gchar *end;

            link->d.app.categories = g_new(GQuark, v->value.strings.n);
            link->d.app.n_categories = v->value.strings.n;

            for (i = 0; i < v->value.strings.n; ++i) {
                link->d.app.categories[i] =
                    g_quark_from_string(v->value.strings.a[i]);
                c = end = end+1; /* next */
            }
        }

        if ((v = g_hash_table_lookup(keys, "MimeType"))) {
            /* steal the string array */
            link->d.app.mime = v->value.strings.a;
            v->value.strings.a = NULL;
            v->value.strings.n = 0;
        }
    }
    else if (link->type == OBT_LINK_TYPE_URL) {
        v = g_hash_table_lookup(keys, "URL");
        g_assert(v);
        link->d.url.addr = v->value.string;
        v->value.string = NULL;
    }

    /* destroy the parsing info */
    g_hash_table_destroy(groups);

    return link;
}

void obt_link_ref(ObtLink *dd)
{
    ++dd->ref;
}

void obt_link_unref(ObtLink *dd)
{
    if (--dd->ref < 1) {
        g_free(dd->name);
        g_free(dd->generic);
        g_free(dd->comment);
        g_free(dd->icon);
        if (dd->type == OBT_LINK_TYPE_APPLICATION) {
            g_free(dd->d.app.exec);
            g_free(dd->d.app.wdir);
            g_strfreev(dd->d.app.mime);
            g_free(dd->d.app.categories);
            g_free(dd->d.app.startup_wmclass);
        }
        else if (dd->type == OBT_LINK_TYPE_URL)
            g_free(dd->d.url.addr);
        g_slice_free(ObtLink, dd);
    }
}

const GQuark* obt_link_app_categories(ObtLink *e, gulong *n)
{
    g_return_val_if_fail(e != NULL, NULL);
    g_return_val_if_fail(e->type == OBT_LINK_TYPE_APPLICATION, NULL);
    g_return_val_if_fail(n != NULL, NULL);

    *n = e->d.app.n_categories;
    return e->d.app.categories;
}
