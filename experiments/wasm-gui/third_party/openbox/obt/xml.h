/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/xml.h for the Openbox window manager
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

#ifndef __obt_xml_h
#define __obt_xml_h

#include <libxml/parser.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _ObtXmlInst ObtXmlInst;

typedef void (*ObtXmlCallback)(xmlNodePtr node, gpointer data);

ObtXmlInst* obt_xml_instance_new(void);
void obt_xml_instance_ref(ObtXmlInst *inst);
void obt_xml_instance_unref(ObtXmlInst *inst);

gboolean obt_xml_load_file(ObtXmlInst *inst,
                           const gchar *path,
                           const gchar *root_node);
gboolean obt_xml_load_config_file(ObtXmlInst *inst,
                                  const gchar *domain,
                                  const gchar *filename,
                                  const gchar *root_node);
gboolean obt_xml_load_data_file(ObtXmlInst *inst,
                                const gchar *domain,
                                const gchar *filename,
                                const gchar *root_node);
gboolean obt_xml_load_theme_file(ObtXmlInst *inst,
                                 const gchar *theme,
                                 const gchar *domain,
                                 const gchar *filename,
                                 const gchar *root_node);
gboolean obt_xml_load_mem(ObtXmlInst *inst,
                          gpointer data, guint len, const gchar *root_node);

/* Returns true if an error is present. */
gboolean obt_xml_last_error(ObtXmlInst *inst);
gchar* obt_xml_last_error_file(ObtXmlInst *inst);
gint obt_xml_last_error_line(ObtXmlInst *inst);
gchar* obt_xml_last_error_message(ObtXmlInst *inst);

gboolean obt_xml_save_file(ObtXmlInst *inst,
                           const gchar *path,
                           gboolean pretty);

xmlDocPtr obt_xml_doc(ObtXmlInst *inst);
xmlNodePtr obt_xml_root(ObtXmlInst *inst);

void obt_xml_close(ObtXmlInst *inst);

void obt_xml_register(ObtXmlInst *inst, const gchar *tag,
                      ObtXmlCallback func, gpointer data);
void obt_xml_unregister(ObtXmlInst *inst, const gchar *tag);
void obt_xml_tree(ObtXmlInst *i, xmlNodePtr node);
void obt_xml_tree_from_root(ObtXmlInst *i);


/* helpers */

xmlNodePtr obt_xml_find_node   (xmlNodePtr node, const gchar *name);

gboolean obt_xml_node_contains (xmlNodePtr node, const gchar *val);
gchar   *obt_xml_node_string_unstripped(xmlNodePtr node);
gchar   *obt_xml_node_string   (xmlNodePtr node);
gint     obt_xml_node_int      (xmlNodePtr node);
gboolean obt_xml_node_bool     (xmlNodePtr node);

gboolean obt_xml_attr_contains (xmlNodePtr node, const gchar *name,
                                const gchar *val);
gboolean obt_xml_attr_string_unstripped(xmlNodePtr node, const gchar *name,
                                        gchar **value);
gboolean obt_xml_attr_string   (xmlNodePtr node, const gchar *name,
                                gchar **value);
gboolean obt_xml_attr_int      (xmlNodePtr node, const gchar *name,
                                gint *value);
gboolean obt_xml_attr_bool     (xmlNodePtr node, const gchar *name,
                                gboolean *value);

G_END_DECLS

#endif
