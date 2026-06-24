/* vim: set sw=2 et: */
/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <libwnck/libwnck.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <unistd.h>
#include "workspace-accessible.h"
#include "private.h"
static void        wnck_workspace_accessible_class_init          (WnckWorkspaceAccessibleClass *klass);
static const char* wnck_workspace_accessible_get_name            (AtkObject                    *obj);
static const char* wnck_workspace_accessible_get_description     (AtkObject                    *obj);
static int         wnck_workspace_accessible_get_index_in_parent (AtkObject                    *obj);
static void        atk_component_interface_init                  (AtkComponentIface            *iface);
static void        wnck_workspace_accessible_get_extents         (AtkComponent                 *component,
                                                                  int                          *x,
                                                                  int                          *y,
                                                                  int                          *width,
                                                                  int                          *height,
                                                                  AtkCoordType                  coords);
static void        wnck_workspace_accessible_get_position        (AtkComponent                 *component,
                                                                  int                          *x,
                                                                  int                          *y,
                                                                  AtkCoordType                  coords);
static gboolean    wnck_workspace_accessible_contains            (AtkComponent                 *component,
                                                                  int                           x,
                                                                  int                           y,
                                                                  AtkCoordType                  coords);
static void        wnck_workspace_accessible_get_size            (AtkComponent                 *component,
                                                                  int                          *width,
                                                                  int                          *height);

GType
wnck_workspace_accessible_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      const GTypeInfo tinfo =
      {
        sizeof (WnckWorkspaceAccessibleClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) wnck_workspace_accessible_class_init,
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (WnckWorkspaceAccessible), /* instance size*/
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };

      const GInterfaceInfo atk_component_info =
      {
        (GInterfaceInitFunc) atk_component_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (ATK_TYPE_GOBJECT_ACCESSIBLE, "WnckWorkspaceAccessible", &tinfo, 0);
      g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
  }
  return type;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_extents = wnck_workspace_accessible_get_extents;
  iface->get_size = wnck_workspace_accessible_get_size;
  iface->get_position = wnck_workspace_accessible_get_position;
  iface->contains = wnck_workspace_accessible_contains;
}

static void
wnck_workspace_accessible_get_extents (AtkComponent *component,
                                       int          *x,
                                       int          *y,
                                       int          *width,
                                       int          *height,
                                       AtkCoordType  coords)
{
  AtkGObjectAccessible *atk_gobj;
  WnckPager *pager;
  GdkRectangle rect;
  GtkWidget *widget;
  AtkObject *parent;
  GObject *g_obj;
  int px, py;

  g_return_if_fail (WNCK_IS_WORKSPACE_ACCESSIBLE (component));

  atk_gobj = ATK_GOBJECT_ACCESSIBLE (component);
  g_obj = atk_gobject_accessible_get_object (atk_gobj);
  if (g_obj == NULL)
    return;

  g_return_if_fail (WNCK_IS_WORKSPACE (g_obj));

  parent = atk_object_get_parent (ATK_OBJECT(component));
#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
#else
  widget = GTK_ACCESSIBLE (parent)->widget;
#endif

  if (widget == NULL)
    {
      /*
       *State is defunct
       */
      return;
    }

  g_return_if_fail (WNCK_IS_PAGER (widget));
  pager = WNCK_PAGER (widget);

  g_return_if_fail (WNCK_IS_PAGER (pager));

  atk_component_get_position (ATK_COMPONENT (parent), &px,&py, coords);

  _wnck_pager_get_workspace_rect (pager, WNCK_WORKSPACE_ACCESSIBLE (component)->index, &rect);

  *x = rect.x + px;
  *y = rect.y + py;
  *height = rect.height;
  *width = rect.width;
}

static void
wnck_workspace_accessible_get_size (AtkComponent *component,
                                    int          *width,
                                    int          *height)
{
  AtkCoordType coords = ATK_XY_SCREEN;
  int x, y;

  /* FIXME: Value for initialization of coords picked randomly to please gcc */

  wnck_workspace_accessible_get_extents (component, &x, &y, width, height, coords);
}

static void
wnck_workspace_accessible_get_position (AtkComponent *component,
		                     int         *x,
			      	     int         *y,
			     	     AtkCoordType  coords)
{
  int width, height;
  wnck_workspace_accessible_get_extents (component, x, y, &width, &height, coords);
}

static gboolean
wnck_workspace_accessible_contains (AtkComponent *component,
                                    int           x,
                                    int           y,
                                    AtkCoordType  coords)
{
  int lx, ly, width, height;

  wnck_workspace_accessible_get_extents (component, &lx, &ly, &width, &height, coords);

  /*
   * Check if the specified co-ordinates fall within the workspace.
   */
  if ( (x > lx) && ((lx + width) >= x)  && (y > ly) && ((ly + height) >= ly) )
    return TRUE;
  else
    return FALSE;
}

static void
wnck_workspace_accessible_class_init (WnckWorkspaceAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_name = wnck_workspace_accessible_get_name;
  class->get_description = wnck_workspace_accessible_get_description;
  class->get_index_in_parent = wnck_workspace_accessible_get_index_in_parent;
}

AtkObject*
wnck_workspace_accessible_new (GObject *obj)
{
  GObject *object;
  AtkObject *atk_object;

  g_return_val_if_fail (WNCK_IS_WORKSPACE (obj), NULL);

  object = g_object_new (WNCK_WORKSPACE_TYPE_ACCESSIBLE, NULL);
  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, obj);

  g_return_val_if_fail (ATK_IS_OBJECT (atk_object), NULL);

  WNCK_WORKSPACE_ACCESSIBLE (atk_object)->index =
    wnck_workspace_get_number (WNCK_WORKSPACE (obj));

  return atk_object;
}

static const char*
wnck_workspace_accessible_get_name (AtkObject *obj)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE_ACCESSIBLE (obj), NULL);

  if (obj->name != NULL)
    {
      return obj->name;
    }
  else
    return NULL;
}

static const char*
wnck_workspace_accessible_get_description (AtkObject *obj)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE_ACCESSIBLE (obj), NULL);

  if (obj->description != NULL)
    {
      return obj->description;
    }
  else
    return NULL;
}

static gint
wnck_workspace_accessible_get_index_in_parent (AtkObject *obj)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE_ACCESSIBLE (obj), -1);

  return WNCK_WORKSPACE_ACCESSIBLE (obj)->index;
}
