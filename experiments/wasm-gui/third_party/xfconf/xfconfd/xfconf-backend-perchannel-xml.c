/*
 *  xfconf
 *
 *  Copyright (c) 2007-2009 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfconf-backend-perchannel-xml.h"
#include "xfconf-backend.h"
#include "xfconf-locking-utils.h"
#include "common/xfconf-gvaluefuncs.h"
#include "xfconf/xfconf-types.h"
#include "common/xfconf-common-private.h"

#define FILE_VERSION_MAJOR  "1"
#define FILE_VERSION_MINOR  "0"

#define PROP_NAME_IS_VALID(name) ( (name) && (name)[0] == '/' && (name)[1] != 0 && !strstr((name), "//") )

#define CONFIG_DIR_STEM  "xfce4/xfconf/" XFCONF_BACKEND_PERCHANNEL_XML_TYPE_ID "/"
#define CONFIG_FILE_FMT  CONFIG_DIR_STEM "%s.xml"
#define CACHE_TIMEOUT    (20*60*1000)  /* 20 minutes */
#define WRITE_TIMEOUT    (5)  /* 5 seconds */
#define MAX_PROP_PATH    (4096)

struct _XfconfBackendPerchannelXml
{
    GObject parent;

    gchar *config_save_path;

    GHashTable *channels;

    guint save_id;

    XfconfPropertyChangedFunc prop_changed_func;
    gpointer prop_changed_data;
};

typedef struct _XfconfBackendPerchannelXmlClass
{
    GObjectClass parent;
} XfconfBackendPerchannelXmlClass;

typedef struct
{
    GNode *properties;
    gboolean locked;
    gboolean dirty;
} XfconfChannel;

typedef struct
{
    gchar *name;
    GValue value;
    GValue system_value;
    gboolean locked;
} XfconfProperty;

typedef enum
{
    ELEM_NONE = 0,
    ELEM_CHANNEL,
    ELEM_PROPERTY,
    ELEM_VALUE,
} XmlParserElem;

/* FIXME: due to the hierarchical nature of the file, i need to use a
 * stack for list_property and list_value because  more than one array
 * property can be open at once.  the current xml file writer always
 * puts the <value> elements right after the opening <property>, but it's
 * possible someone could edit the file so that's not the case anymore. */
typedef struct
{
    XfconfBackendPerchannelXml *xbpx;
    XfconfChannel *channel;
    gboolean is_system_file;
    XmlParserElem cur_elem;
    gchar cur_path[MAX_PROP_PATH];
    gchar *list_property;
    GValue *list_value;
} XmlParserState;

static void xfconf_backend_perchannel_xml_finalize(GObject *obj);

static void xfconf_backend_perchannel_xml_backend_init(XfconfBackendInterface *iface);

static gboolean xfconf_backend_perchannel_xml_initialize(XfconfBackend *backend,
                                                         GError **error);
static gboolean xfconf_backend_perchannel_xml_set(XfconfBackend *backend,
                                                  const gchar *channel_name,
                                                  const gchar *property,
                                                  const GValue *value,
                                                  GError **error);
static gboolean xfconf_backend_perchannel_xml_get(XfconfBackend *backend,
                                                  const gchar *channel_name,
                                                  const gchar *property,
                                                  GValue *value,
                                                  GError **error);
static gboolean xfconf_backend_perchannel_xml_get_all(XfconfBackend *backend,
                                                      const gchar *channel_name,
                                                      const gchar *property_base,
                                                      GHashTable *properties,
                                                      GError **error);
static gboolean xfconf_backend_perchannel_xml_exists(XfconfBackend *backend,
                                                     const gchar *channel_name,
                                                     const gchar *property,
                                                     gboolean *exists,
                                                     GError **error);
static gboolean xfconf_backend_perchannel_xml_reset(XfconfBackend *backend,
                                                    const gchar *channel_name,
                                                    const gchar *property,
                                                    gboolean recursive,
                                                    GError **error);
static gboolean xfconf_backend_perchannel_xml_list_channels(XfconfBackend *backend,
                                                            GSList **channels,
                                                            GError **error);
static gboolean xfconf_backend_perchannel_xml_is_property_locked(XfconfBackend *backend,
                                                                 const gchar *channel_name,
                                                                 const gchar *property,
                                                                 gboolean *locked,
                                                                 GError **error);
static gboolean xfconf_backend_perchannel_xml_flush(XfconfBackend *backend,
                                                    GError **error);
static void xfconf_backend_perchannel_xml_register_property_changed_func(XfconfBackend *backend,
                                                                         XfconfPropertyChangedFunc func,
                                                                         gpointer user_data);

static void xfconf_backend_perchannel_xml_schedule_save(XfconfBackendPerchannelXml *xbpx,
                                                        XfconfChannel *channel);

static XfconfChannel *xfconf_backend_perchannel_xml_create_channel(XfconfBackendPerchannelXml *xbpx,
                                                                   const gchar *channel_name);
static XfconfChannel *xfconf_backend_perchannel_xml_load_channel(XfconfBackendPerchannelXml *xbpx,
                                                                 const gchar *channel_name,
                                                                 GError **error);
static gboolean xfconf_backend_perchannel_xml_flush_channel(XfconfBackendPerchannelXml *xbpx,
                                                            const gchar *channel_name,
                                                            GError **error);

static GNode *xfconf_proptree_add_property(GNode *proptree,
                                           const gchar *name,
                                           const GValue *value,
                                           const GValue *system_value,
                                           gboolean locked);
static XfconfProperty *xfconf_proptree_lookup(GNode *proptree,
                                              const gchar *name);
static GNode *xfconf_proptree_lookup_node(GNode *proptree,
                                          const gchar *name);
static gboolean xfconf_proptree_reset(GNode *proptree,
                                      const gchar *name);
static void xfconf_proptree_destroy(GNode *proptree);
static gchar *xfconf_proptree_build_propname(GNode *prop_node,
                                             gchar *buf,
                                             gsize buflen);

static void xfconf_channel_destroy(XfconfChannel *channel);
static void xfconf_property_free(XfconfProperty *property);


G_DEFINE_TYPE_WITH_CODE(XfconfBackendPerchannelXml, xfconf_backend_perchannel_xml, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(XFCONF_TYPE_BACKEND,
                                              xfconf_backend_perchannel_xml_backend_init))


static void
xfconf_backend_perchannel_xml_class_init(XfconfBackendPerchannelXmlClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;

    object_class->finalize = xfconf_backend_perchannel_xml_finalize;
}

static void
xfconf_backend_perchannel_xml_init(XfconfBackendPerchannelXml *instance)
{
    instance->channels = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               (GDestroyNotify)g_free,
                                                (GDestroyNotify)xfconf_channel_destroy);
}

static void
xfconf_backend_perchannel_xml_finalize(GObject *obj)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(obj);

    if(xbpx->save_id) {
        g_source_remove(xbpx->save_id);
        xbpx->save_id = 0;
        xfconf_backend_perchannel_xml_flush(XFCONF_BACKEND(xbpx), NULL);
    }

    g_hash_table_destroy(xbpx->channels);

    g_free(xbpx->config_save_path);

    G_OBJECT_CLASS(xfconf_backend_perchannel_xml_parent_class)->finalize(obj);
}

static void
xfconf_backend_perchannel_xml_backend_init(XfconfBackendInterface *iface)
{
    iface->initialize = xfconf_backend_perchannel_xml_initialize;
    iface->set = xfconf_backend_perchannel_xml_set;
    iface->get = xfconf_backend_perchannel_xml_get;
    iface->get_all = xfconf_backend_perchannel_xml_get_all;
    iface->exists = xfconf_backend_perchannel_xml_exists;
    iface->reset = xfconf_backend_perchannel_xml_reset;
    iface->list_channels = xfconf_backend_perchannel_xml_list_channels;
    iface->is_property_locked = xfconf_backend_perchannel_xml_is_property_locked;
    iface->flush = xfconf_backend_perchannel_xml_flush;
    iface->register_property_changed_func = xfconf_backend_perchannel_xml_register_property_changed_func;
}

static gboolean
xfconf_backend_perchannel_xml_initialize(XfconfBackend *backend,
                                         GError **error)
{
    XfconfBackendPerchannelXml *backend_px = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    gchar *path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                              CONFIG_DIR_STEM,
                                              TRUE);

    if(!path || !g_file_test(path, G_FILE_TEST_IS_DIR)) {
        if(error) {
            g_set_error(error, XFCONF_ERROR,
                        XFCONF_ERROR_WRITE_FAILURE,
                        _("Unable to create configuration directory \"%s\""),
                        path);
        }
        g_free(path);
        return FALSE;
    }

    backend_px->config_save_path = path;

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_set(XfconfBackend *backend,
                                  const gchar *channel_name,
                                  const gchar *property,
                                  const GValue *value,
                                  GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);
    XfconfProperty *cur_prop;

    if(!channel) {
        channel = xfconf_backend_perchannel_xml_load_channel(xbpx, channel_name,
#ifdef XFCONF_ENABLE_CHECKS
                                                             error);
#else
                                                             NULL);
#endif
        if(!channel) {
#ifdef XFCONF_ENABLE_CHECKS
            g_clear_error(error);
#endif
            channel = xfconf_backend_perchannel_xml_create_channel(xbpx,
                                                                   channel_name);
        }
    }

    cur_prop = xfconf_proptree_lookup(channel->properties, property);
    if(cur_prop) {
        if(cur_prop->locked) {
            if(error) {
                g_set_error(error, XFCONF_ERROR,
                            XFCONF_ERROR_PERMISSION_DENIED,
                            _("Permission denied while modifying property \"%s\" on channel \"%s\""),
                            property, channel_name);
            }
            return FALSE;
        }

        if(_xfconf_gvalue_is_equal(G_VALUE_TYPE(&cur_prop->value)
                                   ? &cur_prop->value
                                   : &cur_prop->system_value, value))
        {
            return TRUE;
        }

        if(G_VALUE_TYPE(&cur_prop->value))
            g_value_unset(&cur_prop->value);
        g_value_copy(value, g_value_init(&cur_prop->value,
                                         G_VALUE_TYPE(value)));

        if(xbpx->prop_changed_func)
            xbpx->prop_changed_func(backend, channel_name, property, xbpx->prop_changed_data);
    } else {
        xfconf_proptree_add_property(channel->properties, property, value,
                                     NULL, FALSE);
        if(xbpx->prop_changed_func)
            xbpx->prop_changed_func(backend, channel_name, property, xbpx->prop_changed_data);
    }

    xfconf_backend_perchannel_xml_schedule_save(xbpx, channel);

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_get(XfconfBackend *backend,
                                  const gchar *channel_name,
                                  const gchar *property,
                                  GValue *value,
                                  GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);
    XfconfProperty *cur_prop;
    GValue *value_to_get = NULL;

    TRACE("entering");

    if(!channel) {
        channel = xfconf_backend_perchannel_xml_load_channel(xbpx, channel_name,
                                                             error);
        if(!channel)
            return FALSE;
    }

    cur_prop = xfconf_proptree_lookup(channel->properties, property);
    if(cur_prop) {
        if(G_VALUE_TYPE(&cur_prop->value))
            value_to_get = &cur_prop->value;
        else if(G_VALUE_TYPE(&cur_prop->system_value))
            value_to_get = &cur_prop->system_value;
    }

    if(!value_to_get) {
        if(error) {
            g_set_error(error, XFCONF_ERROR,
                        XFCONF_ERROR_PROPERTY_NOT_FOUND,
                        _("Property \"%s\" does not exist on channel \"%s\""),
                        property, channel_name);
        }
        return FALSE;
    }

    g_value_copy(value_to_get, g_value_init(value, G_VALUE_TYPE(value_to_get)));

    return TRUE;
}

static void
xfconf_proptree_node_to_hash_table(GNode *node,
                                   GHashTable *props_hash,
                                   gchar cur_path[MAX_PROP_PATH])
{
    XfconfProperty *prop = node->data;
    GValue *value_to_get = NULL;

    if(G_VALUE_TYPE(&prop->value))
        value_to_get = &prop->value;
    else if(G_VALUE_TYPE(&prop->system_value))
        value_to_get = &prop->system_value;

    if(value_to_get) {
        GValue *value = g_new0(GValue, 1);
        gchar *fullprop;

        g_value_copy(value_to_get, g_value_init(value,
                                                G_VALUE_TYPE(value_to_get)));
        fullprop = g_strconcat(cur_path, "/", prop->name, NULL);
        g_hash_table_insert(props_hash, fullprop, value);
    }

    if(node->children) {
        GNode *cur;
        gchar *p;

        if(prop->name[0] != '/') {
            g_strlcat(cur_path, "/", MAX_PROP_PATH);
            g_strlcat(cur_path, prop->name, MAX_PROP_PATH);
        }

        for(cur = g_node_first_child(node);
            cur;
            cur = g_node_next_sibling(cur))
        {
            xfconf_proptree_node_to_hash_table(cur, props_hash, cur_path);
        }

        p = strrchr(cur_path, '/');
        if(p)
            *p = 0;
    }
}

static gboolean
xfconf_backend_perchannel_xml_get_all(XfconfBackend *backend,
                                      const gchar *channel_name,
                                      const gchar *property_base,
                                      GHashTable *properties,
                                      GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);
    GNode *props_tree;
    gchar cur_path[MAX_PROP_PATH], *p;

    if(!channel) {
        channel = xfconf_backend_perchannel_xml_load_channel(xbpx, channel_name,
                                                             error);
        if(!channel)
            return FALSE;
    }

    if(property_base[0] && property_base[1]) {
        /* it's not "" or "/" */
        props_tree = xfconf_proptree_lookup_node(channel->properties,
                                                 property_base);
        if(!props_tree) {
            if(error) {
                g_set_error(error, XFCONF_ERROR, XFCONF_ERROR_PROPERTY_NOT_FOUND,
                             _("Property \"%s\" does not exist on channel \"%s\""),
                             property_base, channel_name);
            }
            return FALSE;
        }

        g_strlcpy(cur_path, property_base, sizeof(cur_path));
        p = g_strrstr(cur_path, "/");  /* guaranteed to succeed */
        *p = 0;
    } else {
        props_tree = channel->properties;
        cur_path[0] = 0;
    }

    xfconf_proptree_node_to_hash_table(props_tree, properties, cur_path);

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_exists(XfconfBackend *backend,
                                     const gchar *channel_name,
                                     const gchar *property,
                                     gboolean *exists,
                                     GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);
    XfconfProperty *prop;

    if(!channel) {
        channel = xfconf_backend_perchannel_xml_load_channel(xbpx, channel_name,
#ifdef XFCONF_ENABLE_CHECKS
                                                             error);
#else
                                                             NULL);
#endif
        if(!channel) {
#ifdef XFCONF_ENABLE_CHECKS
            g_clear_error(error);
#endif

            *exists = FALSE;
            return TRUE;
        }
    }

    prop = xfconf_proptree_lookup(channel->properties, property);
    *exists = (prop && (G_VALUE_TYPE(&prop->value)
                        || G_VALUE_TYPE(&prop->system_value))
               ? TRUE : FALSE);

    return TRUE;
}

typedef struct
{
    XfconfBackendPerchannelXml *xbpx;
    const gchar *channel_name;
} PropChangeData;

static gboolean
nodes_do_prop_reset(GNode *node,
                    gpointer data)
{
    PropChangeData *pdata = data;
    XfconfProperty *prop = node->data;
    gchar prop_fullname[MAX_PROP_PATH];


    /* we don't signal if |value| isn't set but |system_value| is,
     * because we're not actually changing anything by definition */
    if(G_VALUE_TYPE(&prop->value)) {
        g_value_unset(&prop->value);
        if(pdata->xbpx->prop_changed_func) {
            pdata->xbpx->prop_changed_func(XFCONF_BACKEND(pdata->xbpx),
                                           pdata->channel_name,
                                           xfconf_proptree_build_propname(node,
                                                                          prop_fullname,
                                                                          sizeof(prop_fullname)),
                                           pdata->xbpx->prop_changed_data);
        }
    }

    return FALSE;
}

static gboolean
nodes_clean_up(GNode *node,
               gpointer data)
{
    XfconfProperty *prop = node->data;

    /* clean up dangling nodes in tree without system defaults */
    if(!node->children
       && !G_VALUE_TYPE(&prop->value)
       && !G_VALUE_TYPE(&prop->system_value)
       && !prop->locked) {
        g_node_unlink(node);
        xfconf_proptree_destroy(node);
    }

    return FALSE;
}

static gboolean
do_reset_channel(XfconfBackend *backend,
                 const gchar *channel_name,
                 GNode *properties,
                 GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    gchar *filename;
    PropChangeData pdata;

    pdata.xbpx = xbpx;
    pdata.channel_name = channel_name;
    g_node_traverse(properties, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                    nodes_do_prop_reset, &pdata);

    /* we could probably prune the existing proptree, or even just leave
     * it as-is, but it's easier to just kill it.  it'll get reloaded later
     * from the system file (if any) if needed. */
    g_hash_table_remove(xbpx->channels, channel_name);

    /* regardless of whether or not we have a system file, we don't need
     * the user file anymore */
    filename = g_strdup_printf("%s/%s.xml", xbpx->config_save_path, channel_name);
    if(unlink(filename)) {
        if(error) {
            g_set_error(error, XFCONF_ERROR,
                        XFCONF_ERROR_WRITE_FAILURE,
                        _("Unable to remove channel \"%s\": %s"),
                        channel_name, strerror(errno));
        }
        g_free(filename);
        return FALSE;
    }
    g_free(filename);

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_reset(XfconfBackend *backend,
                                    const gchar *channel_name,
                                    const gchar *property,
                                    gboolean recursive,
                                    GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);

    if(!channel) {
        channel = xfconf_backend_perchannel_xml_load_channel(xbpx, channel_name,
                                                             error);
        if(!channel)
            return FALSE;
    }

    if(!recursive) {
        if(!xfconf_proptree_reset(channel->properties, property)) {
            if(error) {
                g_set_error(error, XFCONF_ERROR,
                            XFCONF_ERROR_PROPERTY_NOT_FOUND,
                            _("Property \"%s\" does not exist on channel \"%s\""),
                            property, channel_name);
            }
            return FALSE;
        }

        if(xbpx->prop_changed_func)  /* FIXME: this could fire spuriously */
            xbpx->prop_changed_func(backend, channel_name, property, xbpx->prop_changed_data);
    } else {
        GNode *top;
        
        if(property[0] && property[1]) {
            PropChangeData pdata;

            /* it's not "" or "/" */
            top = xfconf_proptree_lookup_node(channel->properties, property);
            if(!top) {
                if(error) {
                    g_set_error(error, XFCONF_ERROR,
                                XFCONF_ERROR_PROPERTY_NOT_FOUND,
                                _("Property \"%s\" does not exist on channel \"%s\""),
                                property, channel_name);
                }
                return FALSE;
            }

            pdata.xbpx = xbpx;
            pdata.channel_name = channel_name;
            g_node_traverse(top, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                            nodes_do_prop_reset, &pdata);

            /* clean up dangling nodes in tree without system defaults */
            g_node_traverse(top, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                            nodes_clean_up, NULL);
        } else {
            /* remove the entire channel */
            return do_reset_channel(backend, channel_name,
                                    channel->properties, error);
        }
    }

    xfconf_backend_perchannel_xml_schedule_save(xbpx, channel);

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_list_channels(XfconfBackend *backend,
                                            GSList **channels,
                                            GError **error)
{
    gchar **dirs;
    gint i;
    GDir *dir;
    const gchar *name;

    dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG, CONFIG_DIR_STEM);
    for(i = 0; dirs[i]; ++i) {
        dir = g_dir_open(dirs[i], 0, 0);
        if(!dir)
            continue;

        while((name = g_dir_read_name(dir))) {
            if(g_str_has_suffix(name, ".xml")) {
                /* FIXME: maybe validate the files' contents a bit? */
                /* FIXME: maybe temp use a hash table or gtree to avoid search */
                gchar *channel_name = g_strndup(name, strlen(name) - 4);
                if(!g_slist_find_custom(*channels, channel_name,
                                        (GCompareFunc)strcmp))
                {
                    *channels = g_slist_prepend(*channels, channel_name);
                } else
                    g_free(channel_name);
            }
        }

        g_dir_close(dir);
    }
    g_strfreev(dirs);

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_is_property_locked(XfconfBackend *backend,
                                                 const gchar *channel_name,
                                                 const gchar *property,
                                                 gboolean *locked,
                                                 GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);
    XfconfProperty *prop = NULL;

    if(!channel) {
        channel = xfconf_backend_perchannel_xml_load_channel(xbpx, channel_name,
                                                             error);
        if(!channel)
            return FALSE;
    }

    if(!channel->locked)
        prop = xfconf_proptree_lookup(channel->properties, property);
    *locked = (channel->locked || (prop ? prop->locked : FALSE));
    return TRUE;
}

static void
xfconf_backend_perchannel_xml_flush_get_dirty(gpointer key,
                                              gpointer value,
                                              gpointer user_data)
{
    XfconfChannel *channel = value;
    GSList **dirty = user_data;
    if(channel->dirty)
        *dirty = g_slist_prepend(*dirty, key);
}

static gboolean
xfconf_backend_perchannel_xml_flush(XfconfBackend *backend,
                                    GError **error)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);
    GSList *dirty = NULL, *l;

    g_hash_table_foreach(xbpx->channels, xfconf_backend_perchannel_xml_flush_get_dirty, &dirty);

    for(l = dirty; l; l = l->next)
        xfconf_backend_perchannel_xml_flush_channel(xbpx, l->data, error);
    g_slist_free(dirty);

    TRACE("exiting, flushed all channels");

    return TRUE;
}

static void
xfconf_backend_perchannel_xml_register_property_changed_func(XfconfBackend *backend,
                                                             XfconfPropertyChangedFunc func,
                                                             gpointer user_data)
{
    XfconfBackendPerchannelXml *xbpx = XFCONF_BACKEND_PERCHANNEL_XML(backend);

    xbpx->prop_changed_func = func;
    xbpx->prop_changed_data = user_data;
}



static GNode *
xfconf_proptree_lookup_node(GNode *proptree,
                            const gchar *name)
{
    GNode *found_node = NULL;
    gchar **parts;
    GNode *parent, *node;
    gint i;

    g_return_val_if_fail(PROP_NAME_IS_VALID(name), NULL);

    parts = g_strsplit(name+1, "/", -1);
    parent = proptree;

    for(i = 0; parts[i]; ++i) {
        for(node = g_node_first_child(parent);
            node;
            node = g_node_next_sibling(node))
        {
            if(!strcmp(((XfconfProperty *)node->data)->name, parts[i])) {
                if(!parts[i+1])
                    found_node = node;
                else
                    parent = node;
                break;
            }
        }

        if(found_node || !node)
            break;
    }

    g_strfreev(parts);

    return found_node;
}

static XfconfProperty *
xfconf_proptree_lookup(GNode *proptree,
                       const gchar *name)
{
    GNode *node;
    XfconfProperty *prop = NULL;

    node = xfconf_proptree_lookup_node(proptree, name);
    if(node)
        prop = node->data;

    return prop;
}

/* here we assume the entry does not already exist */
static GNode *
xfconf_proptree_add_property(GNode *proptree,
                             const gchar *name,
                             const GValue *value,
                             const GValue *system_value,
                             gboolean locked)
{
    GNode *parent = NULL;
    gchar tmp[MAX_PROP_PATH];
    gchar *p;
    XfconfProperty *prop;

    g_return_val_if_fail(PROP_NAME_IS_VALID(name), NULL);

    g_strlcpy(tmp, name, MAX_PROP_PATH);
    p = g_strrstr(tmp, "/");
    if(p == tmp)
        parent = proptree;
    else {
        *p = 0;
        parent = xfconf_proptree_lookup_node(proptree, tmp);
        if(!parent)
            parent = xfconf_proptree_add_property(proptree, tmp, NULL, NULL, FALSE);
    }

    prop = g_slice_new0(XfconfProperty);
    prop->name = g_strdup(strrchr(name, '/')+1);
    if(value) {
        g_value_init(&prop->value, G_VALUE_TYPE(value));
        g_value_copy(value, &prop->value);
    }
    prop->locked = locked;

    return g_node_append_data(parent, prop);
}

static gboolean
xfconf_proptree_reset(GNode *proptree,
                      const gchar *name)
{
    GNode *node = xfconf_proptree_lookup_node(proptree, name);

    if(node) {
        XfconfProperty *prop = node->data;

        if(G_IS_VALUE(&prop->value)) {
            if(node->children || G_VALUE_TYPE(&prop->system_value)) {
                /* don't remove the children; just blank out the value */
                DBG("unsetting value at \"%s\"", prop->name);
                g_value_unset(&prop->value);
            } else {
                GNode *parent = node->parent;

                g_node_unlink(node);
                xfconf_proptree_destroy(node);

                /* remove parents without values until we find the root node or 
                 * a parent with a value or any children */
                while(parent) {
                    prop = parent->data;
                    if(!G_IS_VALUE(&prop->value)
                       && !G_IS_VALUE(&prop->system_value)
                       && !parent->children && strcmp(prop->name, "/"))
                    {
                        GNode *tmp = parent;
                        parent = parent->parent;

                        DBG("unlinking node at \"%s\"", prop->name);

                        g_node_unlink(tmp);
                        xfconf_proptree_destroy(tmp);
                    } else
                        parent = NULL;
                }
            }

            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
proptree_free_node_data(GNode *node,
                        gpointer data)
{
    xfconf_property_free((XfconfProperty *)node->data);
    return FALSE;
}

static void
xfconf_proptree_destroy(GNode *proptree)
{
    if(G_LIKELY(proptree)) {
        g_node_traverse(proptree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                        proptree_free_node_data, NULL);
        g_node_destroy(proptree);
    }
}

static gchar *
xfconf_proptree_build_propname(GNode *prop_node,
                               gchar *buf,
                               gsize buflen)
{
    GSList *components = NULL, *lp;
    GNode *cur;

    for(cur = prop_node; cur; cur = cur->parent) {
        XfconfProperty *prop = cur->data;
        if(prop->name[0] == '/')
            break;
        components = g_slist_prepend(components, prop->name);
    }

    /* FIXME: optimise */
    buf[0] = 0;
    for(lp = components; lp; lp = lp->next) {
        g_strlcat(buf, "/", buflen);
        g_strlcat(buf, (gchar *)lp->data, buflen);
    }

    g_slist_free(components);

    return buf;
}



static void
xfconf_channel_destroy(XfconfChannel *channel)
{
    xfconf_proptree_destroy(channel->properties);
    g_slice_free(XfconfChannel, channel);
}

static void
xfconf_property_free(XfconfProperty *property)
{
    g_free(property->name);
    if(G_VALUE_TYPE(&property->value))
        g_value_unset(&property->value);
    if(G_VALUE_TYPE(&property->system_value))
        g_value_unset(&property->system_value);
    g_slice_free(XfconfProperty, property);
}

static gboolean
xfconf_backend_perchannel_xml_save_timeout(gpointer data)
{
    XFCONF_BACKEND_PERCHANNEL_XML(data)->save_id = 0;
    xfconf_backend_perchannel_xml_flush(XFCONF_BACKEND(data), NULL);

    return FALSE;
}

static void
xfconf_backend_perchannel_xml_schedule_save(XfconfBackendPerchannelXml *xbpx,
                                            XfconfChannel *channel)
{
    channel->dirty = TRUE;

    if(xbpx->save_id == 0) {
        xbpx->save_id = g_timeout_add_seconds(WRITE_TIMEOUT,
                                              xfconf_backend_perchannel_xml_save_timeout,
                                              xbpx);
    }
}

static XfconfChannel *
xfconf_backend_perchannel_xml_create_channel(XfconfBackendPerchannelXml *xbpx,
                                             const gchar *channel_name)
{
    XfconfChannel *channel;
    XfconfProperty *prop;

    channel = g_hash_table_lookup(xbpx->channels, channel_name);
    if(channel) {
        g_warning("Attempt to create channel when one already exists.");
        return channel;
    }

    channel = g_slice_new0(XfconfChannel);
    prop = g_slice_new0(XfconfProperty);
    prop->name = g_strdup("/");
    channel->properties = g_node_new(prop);
    g_hash_table_insert(xbpx->channels, g_strdup(channel_name), channel);

    return channel;
}


static gboolean
xfconf_xml_handle_channel(XmlParserState *state,
                          const gchar **attribute_names,
                          const gchar **attribute_values,
                          GError **error)
{
    const gchar *name = NULL, *version = NULL;
    const gchar *locked = NULL, *unlocked = NULL;
    gsize maj_ver_len;
    gint i;
    gchar *p;

    for(i = 0; attribute_names[i]; ++i) {
        if(!strcmp(attribute_names[i], "name"))
            name = attribute_values[i];
        else if(!strcmp(attribute_names[i], "version"))
            version = attribute_values[i];
        else if(!strcmp(attribute_names[i], "locked"))
            locked = attribute_values[i];
        else if(!strcmp(attribute_names[i], "unlocked"))
            unlocked = attribute_values[i];
        else {
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Unknown attribute in <channel>: %s",
                            attribute_names[i]);
            }
            return FALSE;
        }
    }

    if(!name || !*name || !version || !*version) {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR,
                        G_MARKUP_ERROR_EMPTY,
                        "Element <channel> requires both name and version attributes");
        }
        return FALSE;
    }

    /* compare versions */
    p = strstr(version, ".");
    maj_ver_len = p ? (gsize)(p - version) : strlen(version);
    if(maj_ver_len != strlen(FILE_VERSION_MAJOR)
       || strncmp(version, FILE_VERSION_MAJOR, maj_ver_len))
    {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR,
                        G_MARKUP_ERROR_INVALID_CONTENT,
                        "On-disk file version %s is not compatible with our file version %s.%s",
                        version, FILE_VERSION_MAJOR,
                        FILE_VERSION_MINOR);
        }
        return FALSE;
    }

    if((locked && *locked) || (unlocked && *unlocked)) {
        gboolean locked_state = FALSE;

        if(!state->is_system_file) {
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Attribute \"locked\" not allowed in <channel> for non-system files");
            }
            return FALSE;
        }

        if(unlocked && *unlocked)
            locked_state = !xfconf_user_is_in_list(unlocked);
        else if(locked && *locked)
            locked_state = xfconf_user_is_in_list(locked);

        /* Policy:
         *   + If the channel was locked by a previous file, and this file
         *     maintains that lock, we merge the properties in the new file
         *     with the properties in the old (the properties in the new
         *     file "win" if there are conflicts).
         *   + If the channel was not locked by a previous file, but this
         *     file is locked, we discard all previous properties and start
         *     anew with this file.
         *   + If the channel was locked by a previous file, but this file
         *     does not maintain that lock, we don't use the properties from
         *     this file.
         *   + If the channel was not locked by a previous file, and this
         *     file doesn't lock it either, we merge (new file "wins").
         */

        if(state->channel->locked && !locked_state) {
            if(error) {
                g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY,
                            "File is not locked, but a previous file is");
            }
            return FALSE;
        } else if(!state->channel->locked && locked_state) {
            XfconfProperty *prop;

            xfconf_proptree_destroy(state->channel->properties);

            prop = g_slice_new0(XfconfProperty);
            prop->name = g_strdup("/");
            state->channel->properties = g_node_new(prop);

            state->channel->locked = TRUE;
        }
    }

    state->cur_elem = ELEM_CHANNEL;

    return TRUE;
}

static gboolean
xfconf_xml_handle_property(XmlParserState *state,
                           const gchar **attribute_names,
                           const gchar **attribute_values,
                           GError **error)
{
    gint i;
    const gchar *name = NULL, *type = NULL, *value = NULL;
    const gchar *locked = NULL, *unlocked = NULL;
    gchar fullpath[MAX_PROP_PATH];
    XfconfProperty *prop = NULL;
    GType value_type;
    GValue *value_to_set = NULL;

    for(i = 0; attribute_names[i]; ++i) {
        if(!strcmp(attribute_names[i], "name"))
            name = attribute_values[i];
        else if(!strcmp(attribute_names[i], "type"))
            type = attribute_values[i];
        else if(!strcmp(attribute_names[i], "value"))
            value = attribute_values[i];
        else if(!strcmp(attribute_names[i], "locked"))
            locked = attribute_values[i];
        else if(!strcmp(attribute_names[i], "unlocked"))
            unlocked = attribute_values[i];
        else {
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Unknown attribute in <property>: %s",
                            attribute_names[i]);
            }
            return FALSE;
        }
    }

    if(!name || !*name || !type || !*type) {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY,
                        "Element <property> requires both name and type attributes");
        }
        return FALSE;
    }

    /* FIXME: name validation! */
    g_strlcpy(fullpath, state->cur_path, MAX_PROP_PATH);
    g_strlcat(fullpath, "/", MAX_PROP_PATH);
    g_strlcat(fullpath, name, MAX_PROP_PATH);

    /* Policy:
     *   + If the channel is already locked and we're here, we can
     *     assume we're in system file.  Then we overwrite any existing
     *     property with the property specified here.
     *   + If the channel isn't locked, but this property is locked and
     *     we're in a system file, then we overwrite any existing
     *     property and mark the property as locked.
     *   + If the channel isn't locked, but the property is locked and
     *     we're NOT in a system file, then we ignore this property.
     *   + If the channel isn't locked and the property isn't locked,
     *     we overwrite any existing property.
     *
     * XXX: What do we do if the channel is locked, the property was set
     *      in a previous file, and the property here is of type 'empty'?
     *      For now I'll clear out the parent property.
     */

    if(state->channel->locked && !state->is_system_file) {
        /* the channel loader should never even try to parse this file */
        g_assert_not_reached();
    }

    prop = xfconf_proptree_lookup(state->channel->properties, fullpath);

    if(state->channel->locked) {
        /* we must still be in a system file, otherwise we'd never get here */
        if(prop) {
            /* when the channel is locked and we're in a system file, a new
             * property will always "win", even if it's "empty" */
            if(G_VALUE_TYPE(&prop->value))  /* shouldn't be set, but... */
                g_value_unset(&prop->value);
            if(G_VALUE_TYPE(&prop->system_value))
                g_value_unset(&prop->system_value);
        } else {
            GNode *pnode = xfconf_proptree_add_property(state->channel->properties,
                                                        fullpath, NULL, NULL,
                                                        TRUE);
            prop = pnode->data;
        }
    } else {
        if(prop && prop->locked && !state->is_system_file) {
            /* not system file, prop already locked, pass on this one */
            state->cur_elem = ELEM_PROPERTY;
            g_strlcpy(state->cur_path, fullpath, MAX_PROP_PATH);
            return TRUE;
        }

        if(prop) {
            /* new prop wins, regardless of previous state */
            if(G_VALUE_TYPE(&prop->value))
                g_value_unset(&prop->value);
            if(state->is_system_file) {
                /* we only clear this if we're in a system file.  if we're
                 * not, we want to remember the system value for reset
                 * purposes. */
                if(G_VALUE_TYPE(&prop->system_value))
                    g_value_unset(&prop->system_value);
            }
        } else {
            GNode *pnode = xfconf_proptree_add_property(state->channel->properties,
                                                        fullpath, NULL, NULL,
                                                        FALSE);
            prop = pnode->data;
        }
    }

    /* Policy:
     *   + There's no way to unlock a property in a locked channel with
     *     an attribute on a <property> element.
     */

    if((locked && *locked) || (unlocked && *unlocked)) {
        if(!state->is_system_file) {
            /* can't lock properties from a non-locked channel */
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Attribute \"locked\" not allowed in <property> for non-system files");
            }

            xfconf_proptree_reset(state->channel->properties, fullpath);
            return FALSE;
        }

        if(state->channel->locked) {
            /* if the channel is locked, don't even bother with this */
            prop->locked = TRUE;
        } else {
            /* not locked already, but we have a lock/unlock directive */
            if(unlocked && *unlocked)
                prop->locked = !xfconf_user_is_in_list(unlocked);
            else if(locked && *locked)
                prop->locked = xfconf_user_is_in_list(locked);
        }
    }

    /* At this point, we're definitely going to overwrite |prop| with the
     * data we get in this element... since we cleared it out above anyway.
     * This might not be the best design choice, but it makes the code
     * slightly simpler, and I don't think it matters in practice, anyway. */

    /* parse types and values */
    value_type = _xfconf_gtype_from_string(type);
    if(G_TYPE_INVALID == value_type) {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR,
                        G_MARKUP_ERROR_INVALID_CONTENT,
                        _("Invalid type for <property>: \"%s\""), type);
        }
        return FALSE;
    }

    if(state->is_system_file)
        value_to_set = &prop->system_value;
    else
        value_to_set = &prop->value;

    if(G_TYPE_NONE != value_type) {
        g_value_init(value_to_set, value_type);
        if(!_xfconf_gvalue_from_string(value_to_set, value)) {
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_INVALID_CONTENT,
                            _("Unable to parse value of type \"%s\" from \"%s\""),
                            g_type_name(value_type), value);
            }
            return FALSE;
        }

        if(G_TYPE_PTR_ARRAY == value_type) {
            /* FIXME: use stacks here */
            state->list_property = g_strdup(fullpath);
            state->list_value = value_to_set;
        }

        if(prop)
            DBG("property '%s' has value type %s", fullpath, G_VALUE_TYPE_NAME(value_to_set));
    } else
        DBG("empty property (branch)");

    g_strlcpy(state->cur_path, fullpath, MAX_PROP_PATH);
    state->cur_elem = ELEM_PROPERTY;

    return TRUE;
}

static gboolean
xfconf_xml_handle_value(XmlParserState *state,
                        const gchar **attribute_names,
                        const gchar **attribute_values,
                        GError **error)
{
    gint i;
    const gchar *type = NULL, *value = NULL;
    GPtrArray *arr;
    GValue *val;
    GType value_type = G_TYPE_INVALID;

    for(i = 0; attribute_names[i]; ++i) {
        if(!strcmp(attribute_names[i], "type"))
            type = attribute_values[i];
        else if(!strcmp(attribute_names[i], "value"))
            value = attribute_values[i];
        else {
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Unknown attribute in <value>: %s",
                            attribute_names[i]);
            }
            return FALSE;
        }
    }

    value_type = _xfconf_gtype_from_string(type);
    if(G_TYPE_PTR_ARRAY == value_type) {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR,
                        G_MARKUP_ERROR_INVALID_CONTENT,
                        _("The type attribute of <value> cannot be an array"));
        }
        return FALSE;
    } else if(G_TYPE_INVALID == value_type
              || G_TYPE_NONE == value_type)
    {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR,
                        G_MARKUP_ERROR_INVALID_CONTENT,
                        _("Invalid type for <value>: \"%s\""), type);
        }
        return FALSE;
    }

    val = g_new0(GValue, 1);
    g_value_init(val, value_type);
    if(!_xfconf_gvalue_from_string(val, value)) {
        if(error) {
            g_set_error(error, G_MARKUP_ERROR,
                        G_MARKUP_ERROR_INVALID_CONTENT,
                        _("Unable to parse value of type \"%s\" from \"%s\""),
                        g_type_name(value_type), value);
        }
        g_value_unset(val);
        g_free(val);
        return FALSE;
    }

    arr = g_value_get_boxed(state->list_value);
    g_ptr_array_add(arr, val);

    state->cur_elem = ELEM_VALUE;

    return TRUE;
}

static void
xfconf_backend_perchannel_xml_start_elem(GMarkupParseContext *context,
                                         const gchar *element_name,
                                         const gchar **attribute_names,
                                         const gchar **attribute_values,
                                         gpointer user_data,
                                         GError **error)
{
    XmlParserState *state = user_data;

    switch(state->cur_elem) {
        case ELEM_NONE:
            if(strcmp(element_name, "channel")) {
                if(error) {
                    g_set_error(error, G_MARKUP_ERROR,
                                G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                                "Element <%s> not valid at top level",
                                element_name);
                }
                return;
            }
            
            xfconf_xml_handle_channel(state, attribute_names,
                                      attribute_values, error);
            break;

        case ELEM_CHANNEL:
        case ELEM_PROPERTY:
            if(!strcmp(element_name, "property")) {
                xfconf_xml_handle_property(state, attribute_names,
                                           attribute_values, error);
            } else if(ELEM_PROPERTY == state->cur_elem
                      && state->list_property  /* FIXME: use stack */
                      && state->list_value  /* FIXME: use stack */
                      && !strcmp(element_name, "value"))
            {
                xfconf_xml_handle_value(state, attribute_names,
                                        attribute_values, error);
            } else {
                if(error) {
                    g_set_error(error, G_MARKUP_ERROR,
                                G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                                "Element <%s> not allowed inside <%s>",
                                element_name, ELEM_CHANNEL == state->cur_elem
                                              ? "channel" : "property");
                }
                return;
            }
            break;

        case ELEM_VALUE:
            if(error) {
                g_set_error(error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                            "No other elements are allowed inside a <value> element.");
            }
            DBG("failed: got %s in <value>", element_name);
            return;
    }
}

static void
xfconf_backend_perchannel_xml_end_elem(GMarkupParseContext *context,
                                       const gchar *element_name,
                                       gpointer user_data,
                                       GError **error)
{
    XmlParserState *state = user_data;
    gchar *p;

    switch(state->cur_elem) {
        case ELEM_CHANNEL:
            state->cur_elem = ELEM_NONE;
            state->cur_path[0] = 0;
            break;

        case ELEM_PROPERTY:
            /* FIXME: use stacks here */
            g_free(state->list_property);
            state->list_property = NULL;
            state->list_value = NULL;

            p = g_strrstr(state->cur_path, "/");

            if(p) {
                *p = 0;
                if(!*(state->cur_path))
                    state->cur_elem = ELEM_CHANNEL;
                else
                    state->cur_elem = ELEM_PROPERTY;
            } else {
                g_warning("Missing '/' char: I don't think this should happen");
                state->cur_elem = ELEM_CHANNEL;
            }
            break;

        case ELEM_VALUE:
            state->cur_elem = ELEM_PROPERTY;
            break;

        case ELEM_NONE:
            /* this really can't happen */
            break;
    }
}

#if 0
static inline gboolean
check_is_whitespace(const gchar *str,
                    gsize str_len)
{
    gint i;

    for(i = 0; i < str_len; ++i) {
        if(str[i] != ' ' && str[i] != '\t' && str[i] != '\r' && str[i] != '\n'
           && str[i] != 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
xfconf_backend_perchannel_xml_text_elem(GMarkupParseContext *context,
                                        const gchar *text,
                                        gsize text_len,
                                        gpointer user_data,
                                        GError **error)
{
    XmlParserState *state = user_data;

    if(ELEM_VALUE != state->cur_elem) {
        /* check to make sure it's not just whitespace */
        if(check_is_whitespace(text, text_len))
            return;

        if(error) {
            g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                        "Content only allowed in <value> elements");
        }
        return;
    }

    if(!state->cur_text)
        state->cur_text = g_strndup(text, text_len);
    else {
        gint cur_len = strlen(state->cur_text);
        state->cur_text = g_realloc(state->cur_text, cur_len + text_len + 1);
        memcpy(state->cur_text + cur_len, text, text_len);
        state->cur_text[cur_len + text_len] = 0;
    }
}
#endif

static gboolean
xfconf_backend_perchannel_xml_merge_file(XfconfBackendPerchannelXml *xbpx,
                                         const gchar *filename,
                                         gboolean is_system_file,
                                         XfconfChannel *channel,
                                         GError **error)
{
    gboolean ret = FALSE;
    GMappedFile *mmap_file;
    gchar *file_contents;
    gsize length;
    GMarkupParseContext *context;
    XmlParserState *state;
    GError *error2 = NULL;
    GMarkupParser parser = {
        xfconf_backend_perchannel_xml_start_elem,
        xfconf_backend_perchannel_xml_end_elem,
        /* xfconf_backend_perchannel_xml_text_elem, */
        NULL,
    };

    TRACE("entering (%s)", filename);

    /* we first try to load a mapped file, if this fails (no mmap
     * implementation is a possible cause) we fall back to normal file
     * loading */
    mmap_file = g_mapped_file_new(filename, FALSE, NULL);
    if(G_LIKELY(mmap_file != NULL)) {
        file_contents = g_mapped_file_get_contents(mmap_file);
        length = g_mapped_file_get_length(mmap_file);
        DBG("successfully loaded mapped file");
    } else if(!g_file_get_contents(filename, &file_contents, &length, error)) {
        return FALSE;
    }

    state = g_slice_new0(XmlParserState);
    state->channel = channel;
    state->xbpx = xbpx;
    state->cur_elem = ELEM_NONE;
    state->is_system_file = is_system_file;

    DBG("got file(size=%"G_GSIZE_FORMAT"): %s", length, file_contents);

    context = g_markup_parse_context_new(&parser, 0, state, NULL);
    if(g_markup_parse_context_parse(context, file_contents, length, &error2)
       && g_markup_parse_context_end_parse(context, &error2)) {
        ret = TRUE;
    } else {
        g_warning("Error parsing xfconf config file \"%s\": %s", filename,
                  error2 ? error2->message : "(?)");
        if(error)
          *error = error2;
        else
          g_error_free(error2);
    }

    TRACE("exiting");

    g_slice_free(XmlParserState, state);

    if(context)
        g_markup_parse_context_free(context);

    if(mmap_file)
        g_mapped_file_unref(mmap_file);
    else
        g_free(file_contents);

    return ret;
}

static XfconfChannel *
xfconf_backend_perchannel_xml_load_channel(XfconfBackendPerchannelXml *xbpx,
                                           const gchar *channel_name,
                                           GError **error)
{
    XfconfChannel *channel = NULL;
    gchar *filename_stem, **filenames, *user_file;
    gint i, length;
    XfconfProperty *prop;

    TRACE("entering");

    filename_stem = g_strdup_printf(CONFIG_FILE_FMT, channel_name);
    filenames = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG, filename_stem);
    user_file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                            filename_stem, FALSE);
    g_free(filename_stem);

    if((!filenames || !filenames[0]) && !user_file) {
        if(error) {
            g_set_error(error, XFCONF_ERROR,
                        XFCONF_ERROR_CHANNEL_NOT_FOUND,
                        _("Channel \"%s\" does not exist"), channel_name);
        }
        goto out;
    }

    channel = g_slice_new0(XfconfChannel);
    prop = g_slice_new0(XfconfProperty);
    prop->name = g_strdup("/");
    channel->properties = g_node_new(prop);

    /* read in system files, we do this in reversed order to properly 
     * follow the xdg spec, see bug #6079 for more information */
    length = filenames ? g_strv_length(filenames) : 0;
    for(i = length - 1; i >= 0; --i) {
        if(!g_strcmp0(user_file, filenames[i]))
            continue;
        xfconf_backend_perchannel_xml_merge_file(xbpx, filenames[i], TRUE,
                                                 channel, NULL);
    }

    if(!channel->locked && user_file) {
        /* read in user file */
        xfconf_backend_perchannel_xml_merge_file(xbpx, user_file, FALSE,
                                                 channel, NULL);
    }

    g_hash_table_insert(xbpx->channels, g_strdup(channel_name), channel);

out:
    g_strfreev(filenames);
    g_free(user_file);

    return channel;
}

static gboolean
xfconf_format_xml_tag(GString *elem_str,
                      GValue *value,
                      gboolean is_array_value,
                      gchar spaces[MAX_PROP_PATH],
                      gboolean *is_array)
{
    gchar *tmp;
    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

    switch(G_VALUE_TYPE(value)) {
        case G_TYPE_STRING:
            tmp = g_markup_escape_text(g_value_get_string(value), -1);
            g_string_append_printf(elem_str, " type=\"string\" value=\"%s\"",
                                   tmp);
            g_free(tmp);
            break;

        case G_TYPE_UCHAR:
            g_string_append_printf(elem_str, " type=\"uchar\" value=\"%hhu\"",
                                   g_value_get_uchar(value));
            break;

        case G_TYPE_CHAR:
            g_string_append_printf(elem_str, " type=\"char\" value=\"%hhd\"",
                                   g_value_get_uchar(value));
            break;

        case G_TYPE_UINT:
            g_string_append_printf(elem_str, " type=\"uint\" value=\"%u\"",
                                   g_value_get_uint(value));
            break;

        case G_TYPE_INT:
            g_string_append_printf(elem_str, " type=\"int\" value=\"%d\"",
                                   g_value_get_int(value));
            break;

        case G_TYPE_UINT64:
            g_string_append_printf(elem_str, " type=\"uint64\" value=\"%" G_GUINT64_FORMAT "\"",
                                   g_value_get_uint64(value));
            break;

        case G_TYPE_INT64:
            g_string_append_printf(elem_str, " type=\"int64\" value=\"%" G_GINT64_FORMAT "\"",
                                   g_value_get_int64(value));
            break;

        case G_TYPE_FLOAT:
            g_string_append_printf(elem_str, " type=\"float\" value=\"%s\"",
                                   g_ascii_dtostr(buf, sizeof(buf), g_value_get_float(value)));
            break;

        case G_TYPE_DOUBLE:
            g_string_append_printf(elem_str, " type=\"double\" value=\"%s\"",
                                   g_ascii_dtostr(buf, sizeof(buf), g_value_get_double(value)));
            break;

        case G_TYPE_BOOLEAN:
            g_string_append_printf(elem_str, " type=\"bool\" value=\"%s\"",
                                   g_value_get_boolean(value) ? "true" : "false");
            break;

        default:
            if(G_VALUE_TYPE(value) == G_TYPE_STRV) {
                gchar **strlist;
                gint i;

                /* we shouldn't get here anymore, i think */
                g_critical("Got G_TYPE_STRV.  Shouldn't happen anymore, right?");

                if(is_array_value)
                    return FALSE;

                g_string_append(elem_str, " type=\"array\">\n");

                strlist = g_value_get_boxed(value);
                for(i = 0; strlist[i]; ++i) {
                    gchar *value_str = g_markup_escape_text(strlist[i], -1);
                    g_string_append_printf(elem_str,
                                           "%s  <value type=\"string\" value=\"%s\"/>\n",
                                           spaces, value_str);
                    g_free(value_str);
                }

                *is_array = TRUE;
            } else if(G_TYPE_PTR_ARRAY == G_VALUE_TYPE(value)) {
                GPtrArray *arr;
                guint i;
                
                if(is_array_value)
                    return FALSE;

                g_string_append(elem_str, " type=\"array\">\n");

                arr = g_value_get_boxed(value);
                for(i = 0; i < arr->len; ++i) {
                    GValue *value1 = g_ptr_array_index(arr, i);
                    gboolean dummy;

                    g_string_append_printf(elem_str, "%s  <value", spaces);
                    if(!xfconf_format_xml_tag(elem_str, value1, TRUE, spaces,
                                              &dummy))
                    {
                        return FALSE;
                    }
                    g_string_append(elem_str, "/>\n");
                }

                *is_array = TRUE;
            } else {
                if(is_array_value)
                    return FALSE;

                if(G_VALUE_TYPE(value) != G_TYPE_INVALID) {
                    g_warning("Unknown value type %d (\"%s\"), treating as branch",
                              (int)G_VALUE_TYPE(value), G_VALUE_TYPE_NAME(value));
                    g_value_unset(value);
                }

                is_array = FALSE;
                g_string_append(elem_str, " type=\"empty\"");
            }
            break;
    }

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_write_node(XfconfBackendPerchannelXml *xbpx,
                                         FILE *fp,
                                         GNode *node,
                                         gint depth,
                                         GError **error)
{
    XfconfProperty *prop = node->data;
    GValue *value = &prop->value;
    GString *elem_str;
    gchar spaces[MAX_PROP_PATH];
    GNode *child;
    gchar *escaped_name;
    gboolean is_array = FALSE;
    
    if(depth * 2 > (gint) sizeof(spaces) + 1)
        depth = sizeof(spaces) / 2 - 1;

    memset(spaces, ' ', depth * 2);
    spaces[depth * 2] = 0;

    escaped_name = g_markup_escape_text(prop->name, strlen(prop->name));

    elem_str = g_string_sized_new(128);
    g_string_append_printf(elem_str, "%s<property name=\"%s\"", spaces,
                           escaped_name);

    g_free (escaped_name);

    if(!xfconf_format_xml_tag(elem_str, value, FALSE, spaces, &is_array)) {
        /* _flush_channel() will handle |error| */
        g_string_free(elem_str, TRUE);
        return FALSE;
    }

    child = g_node_first_child(node);
    if(!is_array) {
        if(child)
            g_string_append(elem_str, ">\n");
        else
            g_string_append(elem_str, "/>\n");
    }

    if(fputs(elem_str->str, fp) == EOF) {
        /* _flush_channel() will handle |error| */
        g_string_free(elem_str, TRUE);
        return FALSE;
    }
    g_string_free(elem_str, TRUE);

    for(; child; child = g_node_next_sibling(child)) {
        if(!xfconf_backend_perchannel_xml_write_node(xbpx, fp, child,
                                                     depth + 1, error))
        {
            /* _flush_channel() will handle |error| */
            return FALSE;
        }
    }

    if(is_array || g_node_first_child(node)) {
        if(fputs(spaces, fp) == EOF || fputs("</property>\n", fp) == EOF) {
            /* _flush_channel() will handle |error| */
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
xfconf_backend_perchannel_xml_flush_channel(XfconfBackendPerchannelXml *xbpx,
                                            const gchar *channel_name,
                                            GError **error)
{
    gboolean ret = FALSE;
    XfconfChannel *channel = g_hash_table_lookup(xbpx->channels, channel_name);
    GNode *child;
    gchar *filename = NULL, *filename_tmp = NULL;
    FILE *fp = NULL;

    DBG("Flushed dirty channel \"%s\"", channel_name);

    if(!channel) {
        if(error) {
            g_set_error(error, XFCONF_ERROR,
                        XFCONF_ERROR_CHANNEL_NOT_FOUND,
                        _("Channel \"%s\" does not exist"), channel_name);
        }
        return FALSE;
    }

    filename = g_strdup_printf("%s/%s.xml", xbpx->config_save_path, channel_name);
    filename_tmp = g_strconcat(filename, ".new", NULL);

    /* ensure the config directory exists */
    if (g_mkdir_with_parents(xbpx->config_save_path, 0755) != 0)
        goto out;

    fp = fopen(filename_tmp, "w");
    if(!fp)
        goto out;

    if(fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n", fp) == EOF
       || fprintf(fp, "<channel name=\"%s\" version=\"%s.%s\">\n", channel_name,
                  FILE_VERSION_MAJOR, FILE_VERSION_MINOR) < 0)
    {
        goto out;
    }

    for(child = g_node_first_child(channel->properties);
        child;
        child = g_node_next_sibling(child))
    {
        if(!xfconf_backend_perchannel_xml_write_node(xbpx, fp, child, 1, error))
            goto out;
    }

    if(fputs("</channel>\n", fp) == EOF)
        goto out;

    if(fflush(fp))
        goto out;

#if defined(HAVE_FDATASYNC)
    if(fdatasync(fileno(fp)))
        goto out;
#elif defined(HAVE_FSYNC)
    if(fsync(fileno(fp)))
        goto out;
#else
    sync();
#endif

    if(fclose(fp)) {
        fp = NULL;
        goto out;
    }
    fp = NULL;

    if(rename(filename_tmp, filename))
        goto out;

    ret = TRUE;

out:
    if(!ret && error && !*error) {
        g_set_error(error, XFCONF_ERROR,
                    XFCONF_ERROR_WRITE_FAILURE,
                    _("Unable to write channel \"%s\": %s"),
                    channel_name, strerror(errno));
    }

    if(fp)
        fclose(fp);

    g_free(filename);
    g_free(filename_tmp);

    channel->dirty = FALSE;

    return ret;
}
