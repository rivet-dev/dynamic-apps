/*
 *  xfconf
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "xfconf-locking-utils.h"

/* group cache stuff */

static time_t etc_group_mtime = 0;
static GHashTable *group_cache = NULL;

static void
xfconf_ensure_group_cache(void)
{
    gboolean needs_rebuild = FALSE;
    struct stat st;
    struct group *gr;
    GHashTable *members;
        
    if(!stat("/etc/group", &st)) {
        if(st.st_mtime > etc_group_mtime) {
            etc_group_mtime = st.st_mtime;
            needs_rebuild = TRUE;
        }
    } else
        needs_rebuild = TRUE;        
    
    if(!needs_rebuild && group_cache)
        return;
    
    if(group_cache)
        g_hash_table_destroy(group_cache);
    
    group_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        (GDestroyNotify)g_free,
                                        (GDestroyNotify)g_hash_table_destroy);
    
    for(setgrent(), gr = getgrent(); gr; gr = getgrent()) {
        gint i;
        
        members = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        (GDestroyNotify)g_free, NULL);
        
        for(i = 0; gr->gr_mem[i]; ++i) {
            g_hash_table_replace(members, g_strdup(gr->gr_mem[i]),
                                 GINT_TO_POINTER(1));
        }
        
        g_hash_table_replace(group_cache, g_strdup(gr->gr_name), members);
    }
}

static gboolean
xfconf_user_is_in_group(const gchar *user,
                        const gchar *group)
{
    GHashTable *members;
    
    xfconf_ensure_group_cache();
    
    members = g_hash_table_lookup(group_cache, group);
    
    if(G_UNLIKELY(!members))
        return FALSE;
    
    return g_hash_table_lookup(members, user) ? TRUE : FALSE;
}

gboolean
xfconf_user_is_in_list(const gchar *list)
{
    gboolean ret = FALSE;
    const gchar *user_name = g_get_user_name();
    gchar **tokens;
    gint i;
    
    tokens = g_strsplit(list, ";", -1);
    
    for(i = 0; tokens[i]; ++i) {
        if(!*tokens[i])
            continue;
        else if(*tokens[i] == '@') {
            if(xfconf_user_is_in_group(user_name, tokens[i]+1)) {
                ret = TRUE;
                break;
            }
        } else {
            if(!strcmp(user_name, tokens[i])) {
                ret = TRUE;
                break;
            }
        }
    }
    
    g_strfreev(tokens);
    
    return ret;
}
