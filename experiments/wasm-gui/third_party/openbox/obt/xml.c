/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/xml.c for the Openbox window manager
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

#include "obt/xml.h"
#include "obt/paths.h"

#include <libxml/xinclude.h>
#include <glib.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

struct Callback {
    gchar *tag;
    ObtXmlCallback func;
    gpointer data;
};

struct _ObtXmlInst {
    gint ref;
    ObtPaths *xdg_paths;
    GHashTable *callbacks;
    xmlDocPtr doc;
    xmlNodePtr root;
    gchar *path;
    gchar *last_error_file;
    gint last_error_line;
    gchar *last_error_message;
};

static void obt_xml_save_last_error(ObtXmlInst* inst);

static void destfunc(struct Callback *c)
{
    g_free(c->tag);
    g_slice_free(struct Callback, c);
}

ObtXmlInst* obt_xml_instance_new(void)
{
    ObtXmlInst *i = g_slice_new(ObtXmlInst);
    i->ref = 1;
    i->xdg_paths = obt_paths_new();
    i->callbacks = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                         (GDestroyNotify)destfunc);
    i->doc = NULL;
    i->root = NULL;
    i->path = NULL;
    i->last_error_file = NULL;
    i->last_error_line = -1;
    i->last_error_message = NULL;
    return i;
}

void obt_xml_instance_ref(ObtXmlInst *i)
{
    ++i->ref;
}

void obt_xml_instance_unref(ObtXmlInst *i)
{
    if (i && --i->ref == 0) {
        obt_paths_unref(i->xdg_paths);
        g_hash_table_destroy(i->callbacks);
        g_free(i->last_error_file);
        g_free(i->last_error_message);
        g_slice_free(ObtXmlInst, i);
    }
}

xmlDocPtr obt_xml_doc(ObtXmlInst *i)
{
    g_assert(i->doc); /* a doc is open? */
    return i->doc;
}

xmlNodePtr obt_xml_root(ObtXmlInst *i)
{
    g_assert(i->doc); /* a doc is open? */
    return i->root;
}

void obt_xml_register(ObtXmlInst *i, const gchar *tag,
                      ObtXmlCallback func, gpointer data)
{
    struct Callback *c;

    if (g_hash_table_lookup(i->callbacks, tag)) {
        g_error("Tag '%s' already registered", tag);
        return;
    }

    c = g_slice_new(struct Callback);
    c->tag = g_strdup(tag);
    c->func = func;
    c->data = data;
    g_hash_table_insert(i->callbacks, c->tag, c);
}

void obt_xml_unregister(ObtXmlInst *i, const gchar *tag)
{
    g_hash_table_remove(i->callbacks, tag);
}

static gboolean load_file(ObtXmlInst *i,
                          const gchar *domain,
                          const gchar *filename,
                          const gchar *root_node,
                          GSList *paths)
{
    GSList *it;
    gboolean r = FALSE;

    g_assert(i->doc == NULL); /* another doc isn't open already? */

    xmlResetLastError();

    for (it = paths; !r && it; it = g_slist_next(it)) {
        gchar *path;
        struct stat s;

        if (!domain && !filename) /* given a full path to the file */
            path = g_strdup(it->data);
        else
            path = g_build_filename(it->data, domain, filename, NULL);

        if (stat(path, &s) >= 0) {
            /* XML_PARSE_BLANKS is needed apparently, or the tree can end up
               with extra nodes in it. */
            i->doc = xmlReadFile(path, NULL, (XML_PARSE_NOBLANKS |
                                              XML_PARSE_RECOVER));
            xmlXIncludeProcessFlags(i->doc, (XML_PARSE_NOBLANKS |
                                             XML_PARSE_RECOVER));
            if (i->doc) {
                i->root = xmlDocGetRootElement(i->doc);
                if (!i->root) {
                    xmlFreeDoc(i->doc);
                    i->doc = NULL;
                    g_message("%s is an empty XML document", path);
                }
                else if (xmlStrcmp(i->root->name,
                                   (const xmlChar*)root_node)) {
                    xmlFreeDoc(i->doc);
                    i->doc = NULL;
                    i->root = NULL;
                    g_message("XML document %s is of wrong type. Root "
                              "node is not '%s'", path, root_node);
                }
                else {
                    i->path = g_strdup(path);
                    r = TRUE; /* ok! */
                }
            }
        }

        g_free(path);
    }

    obt_xml_save_last_error(i);

    return r;
}

gboolean obt_xml_load_file(ObtXmlInst *i,
                           const gchar *path,
                           const gchar *root_node)
{
    GSList *paths;
    gboolean r;

    paths = g_slist_append(NULL, g_strdup(path));

    r = load_file(i, NULL, NULL, root_node, paths);

    while (paths) {
        g_free(paths->data);
        paths = g_slist_delete_link(paths, paths);
    }
    return r;
}

gboolean obt_xml_load_config_file(ObtXmlInst *i,
                                  const gchar *domain,
                                  const gchar *filename,
                                  const gchar *root_node)
{
    GSList *it, *paths = NULL;
    gboolean r;

    for (it = obt_paths_config_dirs(i->xdg_paths); it; it = g_slist_next(it))
        paths = g_slist_append(paths, g_strdup(it->data));

    r = load_file(i, domain, filename, root_node, paths);

    while (paths) {
        g_free(paths->data);
        paths = g_slist_delete_link(paths, paths);
    }
    return r;
}

gboolean obt_xml_load_data_file(ObtXmlInst *i,
                                const gchar *domain,
                                const gchar *filename,
                                const gchar *root_node)
{
    GSList *it, *paths = NULL;
    gboolean r;

    for (it = obt_paths_data_dirs(i->xdg_paths); it; it = g_slist_next(it))
        paths = g_slist_append(paths, g_strdup(it->data));

    r = load_file(i, domain, filename, root_node, paths);

    while (paths) {
        g_free(paths->data);
        paths = g_slist_delete_link(paths, paths);
    }
    return r;
}

gboolean obt_xml_load_theme_file(ObtXmlInst *i,
                                 const gchar *theme,
                                 const gchar *domain,
                                 const gchar *filename,
                                 const gchar *root_node)
{
    GSList *it, *paths = NULL;
    gboolean r;

    /* use ~/.themes for backwards compatibility */
    paths = g_slist_append
        (paths, g_build_filename(g_get_home_dir(), ".themes", theme, NULL));

    for (it = obt_paths_data_dirs(i->xdg_paths); it; it = g_slist_next(it))
        paths = g_slist_append
            (paths, g_build_filename(it->data, "themes", theme, NULL));

    r = load_file(i, domain, filename, root_node, paths);

    while (paths) {
        g_free(paths->data);
        paths = g_slist_delete_link(paths, paths);
    }
    return r;
}


gboolean obt_xml_load_mem(ObtXmlInst *i,
                          gpointer data, guint len, const gchar *root_node)
{
    gboolean r = FALSE;

    g_assert(i->doc == NULL); /* another doc isn't open already? */

    xmlResetLastError();

    i->doc = xmlParseMemory(data, len);
    if (i->doc) {
        i->root = xmlDocGetRootElement(i->doc);
        if (!i->root) {
            xmlFreeDoc(i->doc);
            i->doc = NULL;
            g_message("Given memory is an empty document");
        }
        else if (xmlStrcmp(i->root->name, (const xmlChar*)root_node)) {
            xmlFreeDoc(i->doc);
            i->doc = NULL;
            i->root = NULL;
            g_message("XML Document in given memory is of wrong "
                      "type. Root node is not '%s'\n", root_node);
        }
        else
            r = TRUE; /* ok ! */
    }

    obt_xml_save_last_error(i);

    return r;
}

static void obt_xml_save_last_error(ObtXmlInst* inst)
{
    xmlErrorPtr error = xmlGetLastError();
    if (error) {
        inst->last_error_file = g_strdup(error->file);
        inst->last_error_line = error->line;
        inst->last_error_message = g_strdup(error->message);
        xmlResetError(error);
    }
}

gboolean obt_xml_last_error(ObtXmlInst *inst)
{
    return inst->last_error_file &&
        inst->last_error_line >= 0 &&
        inst->last_error_message;
}

gchar* obt_xml_last_error_file(ObtXmlInst *inst)
{
    if (!obt_xml_last_error(inst))
        return NULL;
    return inst->last_error_file;
}

gint obt_xml_last_error_line(ObtXmlInst *inst)
{
    if (!obt_xml_last_error(inst))
        return -1;
    return inst->last_error_line;
}

gchar* obt_xml_last_error_message(ObtXmlInst *inst)
{
    if (!obt_xml_last_error(inst))
        return NULL;
    return inst->last_error_message;
}

gboolean obt_xml_save_file(ObtXmlInst *inst,
                           const gchar *path,
                           gboolean pretty)
{
    return xmlSaveFormatFile(path, inst->doc, pretty) != -1;
}

void obt_xml_close(ObtXmlInst *i)
{
    if (i && i->doc) {
        xmlFreeDoc(i->doc);
        g_free(i->path);
        i->doc = NULL;
        i->root = NULL;
        i->path = NULL;
    }
}

void obt_xml_tree(ObtXmlInst *i, xmlNodePtr node)
{
    g_assert(i->doc); /* a doc is open? */

    while (node) {
        if (node->name) {
            struct Callback *c = g_hash_table_lookup(i->callbacks, node->name);
            if (c) c->func(node, c->data);
        }
        node = node->next;
    }
}

void obt_xml_tree_from_root(ObtXmlInst *i)
{
    obt_xml_tree(i, i->root->children);
}

gchar *obt_xml_node_string_unstripped(xmlNodePtr node)
{
    xmlChar *c = xmlNodeGetContent(node);
    gchar *s;
    s = g_strdup(c ? (gchar*)c : "");
    xmlFree(c);
    return s;
}

gchar *obt_xml_node_string(xmlNodePtr node)
{
    gchar* result = obt_xml_node_string_unstripped(node);
    g_strstrip(result); /* strip leading/trailing whitespace */
    return result;
}

gint obt_xml_node_int(xmlNodePtr node)
{
    xmlChar *c = xmlNodeGetContent(node);
    gint i;
    if (c) g_strstrip((char*)c); /* strip leading/trailing whitespace */
    i = c ? atoi((gchar*)c) : 0;
    xmlFree(c);
    return i;
}

gboolean obt_xml_node_bool(xmlNodePtr node)
{
    xmlChar *c = xmlNodeGetContent(node);
    gboolean b = FALSE;
    if (c) g_strstrip((char*)c); /* strip leading/trailing whitespace */
    if (c && !xmlStrcasecmp(c, (const xmlChar*) "true"))
        b = TRUE;
    else if (c && !xmlStrcasecmp(c, (const xmlChar*) "yes"))
        b = TRUE;
    else if (c && !xmlStrcasecmp(c, (const xmlChar*) "on"))
        b = TRUE;
    xmlFree(c);
    return b;
}

gboolean obt_xml_node_contains(xmlNodePtr node, const gchar *val)
{
    xmlChar *c = xmlNodeGetContent(node);
    gboolean r;
    if (c) g_strstrip((char*)c); /* strip leading/trailing whitespace */
    r = !xmlStrcasecmp(c, (const xmlChar*) val);
    xmlFree(c);
    return r;
}

xmlNodePtr obt_xml_find_node(xmlNodePtr node, const gchar *tag)
{
    while (node) {
        if (!xmlStrcmp(node->name, (const xmlChar*) tag))
            return node;
        node = node->next;
    }
    return NULL;
}

gboolean obt_xml_attr_bool(xmlNodePtr node, const gchar *name,
                           gboolean *value)
{
    xmlChar *c = xmlGetProp(node, (const xmlChar*) name);
    gboolean r = FALSE;
    if (c) {
        g_strstrip((char*)c); /* strip leading/trailing whitespace */
        if (!xmlStrcasecmp(c, (const xmlChar*) "true"))
            *value = TRUE, r = TRUE;
        else if (!xmlStrcasecmp(c, (const xmlChar*) "yes"))
            *value = TRUE, r = TRUE;
        else if (!xmlStrcasecmp(c, (const xmlChar*) "on"))
            *value = TRUE, r = TRUE;
        else if (!xmlStrcasecmp(c, (const xmlChar*) "false"))
            *value = FALSE, r = TRUE;
        else if (!xmlStrcasecmp(c, (const xmlChar*) "no"))
            *value = FALSE, r = TRUE;
        else if (!xmlStrcasecmp(c, (const xmlChar*) "off"))
            *value = FALSE, r = TRUE;
    }
    xmlFree(c);
    return r;
}

gboolean obt_xml_attr_int(xmlNodePtr node, const gchar *name, gint *value)
{
    xmlChar *c = xmlGetProp(node, (const xmlChar*) name);
    gboolean r = FALSE;
    if (c) {
        g_strstrip((char*)c); /* strip leading/trailing whitespace */
        *value = atoi((gchar*)c);
        r = TRUE;
    }
    xmlFree(c);
    return r;
}

gboolean obt_xml_attr_string_unstripped(xmlNodePtr node, const gchar *name,
                                        gchar **value)
{
    xmlChar *c = xmlGetProp(node, (const xmlChar*) name);
    gboolean r = FALSE;
    if (c) {
        *value = g_strdup((gchar*)c);
        r = TRUE;
    }
    xmlFree(c);
    return r;
}

gboolean obt_xml_attr_string(xmlNodePtr node, const gchar *name,
                             gchar **value)
{
    gboolean result = obt_xml_attr_string_unstripped(node, name, value);
    if (result)
        g_strstrip(*value); /* strip leading/trailing whitespace */
    return result;
}

gboolean obt_xml_attr_contains(xmlNodePtr node, const gchar *name,
                               const gchar *val)
{
    xmlChar *c = xmlGetProp(node, (const xmlChar*) name);
    gboolean r = FALSE;
    if (c) {
        g_strstrip((char*)c); /* strip leading/trailing whitespace */
        r = !xmlStrcasecmp(c, (const xmlChar*) val);
    }
    xmlFree(c);
    return r;
}
