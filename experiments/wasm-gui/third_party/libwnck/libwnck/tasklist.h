/* tasklist object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003, 2005-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_TASKLIST_H
#define WNCK_TASKLIST_H

#include <gtk/gtk.h>
#include <libwnck/screen.h>

G_BEGIN_DECLS

#define WNCK_TYPE_TASKLIST              (wnck_tasklist_get_type ())
#define WNCK_TASKLIST(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_TASKLIST, WnckTasklist))
#define WNCK_TASKLIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_TASKLIST, WnckTasklistClass))
#define WNCK_IS_TASKLIST(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_TASKLIST))
#define WNCK_IS_TASKLIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_TASKLIST))
#define WNCK_TASKLIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_TASKLIST, WnckTasklistClass))

typedef struct _WnckTasklist        WnckTasklist;
typedef struct _WnckTasklistClass   WnckTasklistClass;
typedef struct _WnckTasklistPrivate WnckTasklistPrivate;

/**
 * WnckTasklist:
 *
 * The #WnckTasklist struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckTasklist
{
  GtkContainer parent_instance;

  WnckTasklistPrivate *priv;
};

struct _WnckTasklistClass
{
  GtkContainerClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * WnckTasklistGroupingType:
 * @WNCK_TASKLIST_NEVER_GROUP: never group multiple #WnckWindow of the same
 * #WnckApplication.
 * @WNCK_TASKLIST_AUTO_GROUP: group multiple #WnckWindow of the same
 * #WnckApplication for some #WnckApplication, when there is not enough place
 * to have a good-looking list of all #WnckWindow.
 * @WNCK_TASKLIST_ALWAYS_GROUP: always group multiple #WnckWindow of the same
 * #WnckApplication, for all #WnckApplication.
 *
 * Type defining the policy of the #WnckTasklist for grouping multiple
 * #WnckWindow of the same #WnckApplication.
 */
typedef enum {
  WNCK_TASKLIST_NEVER_GROUP,
  WNCK_TASKLIST_AUTO_GROUP,
  WNCK_TASKLIST_ALWAYS_GROUP
} WnckTasklistGroupingType;

GType wnck_tasklist_get_type (void) G_GNUC_CONST;

GtkWidget *wnck_tasklist_new (void);
const int *wnck_tasklist_get_size_hint_list (WnckTasklist  *tasklist,
					      int           *n_elements);

void wnck_tasklist_set_grouping (WnckTasklist             *tasklist,
				 WnckTasklistGroupingType  grouping);
void wnck_tasklist_set_switch_workspace_on_unminimize (WnckTasklist  *tasklist,
						       gboolean       switch_workspace_on_unminimize);
void wnck_tasklist_set_middle_click_close (WnckTasklist  *tasklist,
					   gboolean       middle_click_close);
void wnck_tasklist_set_grouping_limit (WnckTasklist *tasklist,
				       gint          limit);
void wnck_tasklist_set_include_all_workspaces (WnckTasklist *tasklist,
					       gboolean      include_all_workspaces);
void wnck_tasklist_set_button_relief (WnckTasklist *tasklist,
                                      GtkReliefStyle relief);
void wnck_tasklist_set_orientation (WnckTasklist *tasklist,
                                    GtkOrientation orient);

/**
 * WnckLoadIconFunction:
 * @icon_name: an icon name as in the Icon field in a .desktop file for the
 * icon to load.
 * @size: the desired icon size.
 * @flags: not defined to do anything yet.
 * @data: data passed to the function, set when the #WnckLoadIconFunction has
 * been set for the #WnckTasklist.
 *
 * Specifies the type of function passed to wnck_tasklist_set_icon_loader().
 *
 * Returns: it should return a <classname>GdkPixbuf</classname> of @icon_name
 * at size @size, or %NULL if no icon for @icon_name at size @size could be
 * loaded.
 *
 * Since: 2.2
 */
typedef GdkPixbuf* (*WnckLoadIconFunction) (const char   *icon_name,
                                            int           size,
                                            unsigned int  flags,
                                            void         *data);

void wnck_tasklist_set_icon_loader (WnckTasklist         *tasklist,
                                    WnckLoadIconFunction  load_icon_func,
                                    void                 *data,
                                    GDestroyNotify        free_data_func);

G_END_DECLS

#endif /* WNCK_TASKLIST_H */
