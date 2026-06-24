/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/paths.c for the Openbox window manager
   Copyright (c) 2003-2007   Dana Jansens

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

#include "obt/bsearch.h"
#include "obt/paths.h"
#include "obt/util.h"

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_GRP_H
#  include <grp.h>
#endif
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif

struct _ObtPaths
{
    gint   ref;
    gchar  *config_home;
    gchar  *data_home;
    gchar  *cache_home;
    GSList *config_dirs;
    GSList *data_dirs;
    GSList *autostart_dirs;
    GSList *exec_dirs;

    uid_t   uid;
    gid_t  *gid;
    guint   n_gid;
};

static gint slist_path_cmp(const gchar *a, const gchar *b)
{
    return strcmp(a, b);
}

typedef GSList* (*GSListFunc) (gpointer list, gconstpointer data);

static GSList* slist_path_add(GSList *list, gpointer data, GSListFunc func)
{
    g_assert(func);

    if (!data)
        return list;

    if (!g_slist_find_custom(list, data, (GCompareFunc) slist_path_cmp))
        list = func(list, data);
    else
        g_free(data);

    return list;
}

static GSList* split_paths(const gchar *paths)
{
    GSList *list = NULL;
    gchar **spl, **it;

    if (!paths)
        return NULL;
    spl = g_strsplit(paths, ":", -1);
    for (it = spl; *it; ++it) {
        if ((*it)[0]) /* skip empty strings */
            list = slist_path_add(list, *it, (GSListFunc) g_slist_append);
    }
    g_free(spl);
    return list;
}

int gid_cmp(const void *va, const void *vb)
{
    const gid_t a = *(const gid_t*)va, b = *(const gid_t*)vb;
    return a>b ? 1 : (a == b ? 0 : -1);
}

static void find_uid_gid(uid_t *u, gid_t **g, guint *n)
{
    struct passwd *pw;
    const gchar *name;
    struct group *gr;

    *u = getuid();
    pw = getpwuid(*u);
    name = pw->pw_name;

    *g = g_new(gid_t, *n=1);
    (*g)[0] = getgid();

    while ((gr = getgrent())) {
        if (gr->gr_gid != (*g)[0]) { /* skip the main group */
            gchar **c;
            for (c = gr->gr_mem; *c; ++c)
                if (strcmp(*c, name) == 0) {
                    *g = g_renew(gid_t, *g, ++(*n)); /* save the group */
                    (*g)[*n-1] = gr->gr_gid;
                    break;
                }
        }
    }
    endgrent();

    qsort(*g, *n, sizeof(gid_t), gid_cmp);
}

ObtPaths* obt_paths_new(void)
{
    ObtPaths *p;
    const gchar *path;
    GSList *it;

    p = g_slice_new0(ObtPaths);
    p->ref = 1;

    find_uid_gid(&p->uid, &p->gid, &p->n_gid);

    path = g_getenv("XDG_CONFIG_HOME");
    if (path && path[0] != '\0') /* not unset or empty */
        p->config_home = g_build_filename(path, NULL);
    else
        p->config_home = g_build_filename(g_get_home_dir(), ".config", NULL);

    path = g_getenv("XDG_DATA_HOME");
    if (path && path[0] != '\0') /* not unset or empty */
        p->data_home = g_build_filename(path, NULL);
    else
        p->data_home = g_build_filename(g_get_home_dir(), ".local",
                                        "share", NULL);

    path = g_getenv("XDG_CACHE_HOME");
    if (path && path[0] != '\0') /* not unset or empty */
        p->cache_home = g_build_filename(path, NULL);
    else
        p->cache_home = g_build_filename(g_get_home_dir(), ".cache", NULL);

    path = g_getenv("XDG_CONFIG_DIRS");
    if (path && path[0] != '\0') /* not unset or empty */
        p->config_dirs = split_paths(path);
    else {
        p->config_dirs = slist_path_add(p->config_dirs,
                                        g_strdup(CONFIGDIR),
                                        (GSListFunc) g_slist_append);
        p->config_dirs = slist_path_add(p->config_dirs,
                                        g_build_filename
                                        (G_DIR_SEPARATOR_S,
                                         "etc", "xdg", NULL),
                                        (GSListFunc) g_slist_append);
    }
    p->config_dirs = slist_path_add(p->config_dirs,
                                    g_strdup(p->config_home),
                                    (GSListFunc) g_slist_prepend);

    for (it = p->config_dirs; it; it = g_slist_next(it)) {
        gchar *const s = g_strdup_printf("%s/autostart", (gchar*)it->data);
        p->autostart_dirs = g_slist_append(p->autostart_dirs, s);
    }

    path = g_getenv("XDG_DATA_DIRS");
    if (path && path[0] != '\0') /* not unset or empty */
        p->data_dirs = split_paths(path);
    else {
        p->data_dirs = slist_path_add(p->data_dirs,
                                      g_strdup(DATADIR),
                                      (GSListFunc) g_slist_append);
        p->data_dirs = slist_path_add(p->data_dirs,
                                      g_build_filename
                                      (G_DIR_SEPARATOR_S,
                                       "usr", "local", "share", NULL),
                                      (GSListFunc) g_slist_append);
        p->data_dirs = slist_path_add(p->data_dirs,
                                      g_build_filename
                                      (G_DIR_SEPARATOR_S,
                                       "usr", "share", NULL),
                                      (GSListFunc) g_slist_append);
    }
    p->data_dirs = slist_path_add(p->data_dirs,
                                  g_strdup(p->data_home),
                                  (GSListFunc) g_slist_prepend);

    path = g_getenv("PATH");
    if (path && path[0] != '\0') /* not unset or empty */
        p->exec_dirs = split_paths(path);
    else
        p->exec_dirs = NULL;

    return p;
}

void obt_paths_ref(ObtPaths *p)
{
    ++p->ref;
}

void obt_paths_unref(ObtPaths *p)
{
    if (p && --p->ref == 0) {
        GSList *it;

        for (it = p->config_dirs; it; it = g_slist_next(it))
            g_free(it->data);
        g_slist_free(p->config_dirs);
        for (it = p->data_dirs; it; it = g_slist_next(it))
            g_free(it->data);
        g_slist_free(p->data_dirs);
        for (it = p->autostart_dirs; it; it = g_slist_next(it))
            g_free(it->data);
        g_slist_free(p->autostart_dirs);
        for (it = p->exec_dirs; it; it = g_slist_next(it))
            g_free(it->data);
        g_slist_free(p->exec_dirs);
        g_free(p->config_home);
        g_free(p->data_home);
        g_free(p->cache_home);
        g_free(p->gid);

        g_slice_free(ObtPaths, p);
    }
}

gchar *obt_paths_expand_tilde(const gchar *f)
{
    gchar *ret;
    GRegex *regex;

    if (!f)
        return NULL;

    regex = g_regex_new("(?:^|(?<=[ \\t]))~(?=[/ \\t$])", G_REGEX_MULTILINE | G_REGEX_RAW, 0, NULL);
    ret = g_regex_replace_literal(regex, f, -1, 0, g_get_home_dir(), 0, NULL);
    g_regex_unref(regex);

    return ret;
}

gboolean obt_paths_mkdir(const gchar *path, gint mode)
{
    gboolean ret = TRUE;

    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(path[0] != '\0', FALSE);

    if (!g_file_test(path, G_FILE_TEST_IS_DIR))
        if (mkdir(path, mode) == -1)
            ret = FALSE;

    return ret;
}

gboolean obt_paths_mkdir_path(const gchar *path, gint mode)
{
    gboolean ret = TRUE;

    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(path[0] == '/', FALSE);

    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
        gchar *c, *e;

        c = g_strdup(path);
        e = c;
        while ((e = strchr(e + 1, '/'))) {
            *e = '\0';
            if (!(ret = obt_paths_mkdir(c, mode)))
                goto parse_mkdir_path_end;
            *e = '/';
        }
        ret = obt_paths_mkdir(c, mode);

    parse_mkdir_path_end:
        g_free(c);
    }

    return ret;
}

const gchar* obt_paths_config_home(ObtPaths *p)
{
    return p->config_home;
}

const gchar* obt_paths_data_home(ObtPaths *p)
{
    return p->data_home;
}

const gchar* obt_paths_cache_home(ObtPaths *p)
{
    return p->cache_home;
}

GSList* obt_paths_config_dirs(ObtPaths *p)
{
    return p->config_dirs;
}

GSList* obt_paths_data_dirs(ObtPaths *p)
{
    return p->data_dirs;
}

GSList* obt_paths_autostart_dirs(ObtPaths *p)
{
    return p->autostart_dirs;
}

static inline gboolean try_exec(const ObtPaths *const p,
                                const gchar *const path)
{
    struct stat st;
    BSEARCH_SETUP();

    if (stat(path, &st) != 0)
        return FALSE;

    if (!S_ISREG(st.st_mode))
        return FALSE;
    if (st.st_uid == p->uid)
        return st.st_mode & S_IXUSR;
    BSEARCH(guint, p->gid, 0, p->n_gid, st.st_gid);
    if (BSEARCH_FOUND())
        return st.st_mode & S_IXGRP;
    return st.st_mode & S_IXOTH;
}

gboolean obt_paths_try_exec(ObtPaths *p, const gchar *path)
{
    if (path[0] == '/') {
        return try_exec(p, path);
    }
    else {
        GSList *it;

        for (it = p->exec_dirs; it; it = g_slist_next(it)) {
            gchar *f = g_build_filename(it->data, path, NULL);
            gboolean e = try_exec(p, f);
            g_free(f);
            if (e) return TRUE;
        }
    }

    return FALSE;
}
