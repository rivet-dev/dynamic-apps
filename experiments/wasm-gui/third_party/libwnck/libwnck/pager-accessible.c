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

#include <config.h>

#include <libwnck/libwnck.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <atk/atk.h>
#include "pager-accessible.h"
#include "pager-accessible-factory.h"
#include "workspace-accessible.h"
#include "private.h"

typedef struct _WnckPagerAccessiblePriv WnckPagerAccessiblePriv;
struct _WnckPagerAccessiblePriv
{
  GSList *children;
};

static void        wnck_pager_accessible_class_init       (WnckPagerAccessibleClass *klass);
static const char* wnck_pager_accessible_get_name         (AtkObject                *obj);
static const char* wnck_pager_accessible_get_description  (AtkObject                *obj);
static int         wnck_pager_accessible_get_n_children   (AtkObject                *obj);
static AtkObject*  wnck_pager_accessible_ref_child        (AtkObject                *obj,
                                                           int                       i);
static void        atk_selection_interface_init           (AtkSelectionIface        *iface);
static gboolean    wnck_pager_add_selection               (AtkSelection             *selection,
                                                           int                       i);
static gboolean    wnck_pager_is_child_selected           (AtkSelection             *selection,
                                                           int                       i);
static AtkObject*  wnck_pager_ref_selection               (AtkSelection             *selection,
                                                           int                       i);
static int         wnck_pager_selection_count             (AtkSelection             *selection);
static void        wnck_pager_accessible_update_workspace (AtkObject                *aobj_ws,
                                                           WnckPager                *pager,
                                                           int                       i);
static void        wnck_pager_accessible_finalize         (GObject                  *gobject);

static WnckPagerAccessiblePriv* get_private_data          (GObject                  *gobject);

static void* parent_class;
static GQuark quark_private_data = 0;

GType
wnck_pager_accessible_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo tinfo =
        {
          sizeof (WnckPagerAccessibleClass),
          (GBaseInitFunc) NULL, /* base init */
          (GBaseFinalizeFunc) NULL, /* base finalize */
          (GClassInitFunc) wnck_pager_accessible_class_init, /* class init */
          (GClassFinalizeFunc) NULL, /* class finalize */
          NULL, /* class data */
          sizeof (WnckPagerAccessible), /* instance size */
          0, /* nb preallocs */
          NULL, /* instance init */
          NULL /* value table */
        };

      const GInterfaceInfo atk_selection_info =
        {
          (GInterfaceInitFunc) atk_selection_interface_init,
          (GInterfaceFinalizeFunc) NULL,
          NULL
        };

      /*
       * Figure out the size of the class and instance
       * we are deriving from
       */
      AtkObjectFactory *factory;
      GType derived_type;
      GTypeQuery query;
      GType derived_atk_type;

      derived_type = g_type_parent (WNCK_TYPE_PAGER);

      factory = atk_registry_get_factory (atk_get_default_registry (),
                                          derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      g_type_query (derived_atk_type, &query);
      tinfo.class_size = query.class_size;
      tinfo.instance_size = query.instance_size;

      type = g_type_register_static (derived_atk_type,
                                     "WnckPagerAccessible", &tinfo, 0);

      g_type_add_interface_static (type, ATK_TYPE_SELECTION, &atk_selection_info);
    }

  return type;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->add_selection = wnck_pager_add_selection;
  iface->ref_selection = wnck_pager_ref_selection;
  iface->get_selection_count = wnck_pager_selection_count;
  iface->is_child_selected = wnck_pager_is_child_selected;
}

static void
wnck_pager_accessible_class_init (WnckPagerAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  class->get_name = wnck_pager_accessible_get_name;
  class->get_description = wnck_pager_accessible_get_description;
  class->get_n_children = wnck_pager_accessible_get_n_children;
  class->ref_child = wnck_pager_accessible_ref_child;

  obj_class->finalize = wnck_pager_accessible_finalize;
  quark_private_data = g_quark_from_static_string ("wnck-pager-accessible-private-data");
}


static gboolean
wnck_pager_add_selection (AtkSelection *selection,
		          int           i)
{
  WnckPager *pager;
  WnckWorkspace *wspace;
  GtkWidget *widget;
  int n_spaces;

#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
#else
  widget = GTK_ACCESSIBLE (selection)->widget;
#endif

  if (widget == NULL)
    {
      /*
       *State is defunct
       */
      return FALSE;
    }

  pager = WNCK_PAGER (widget);
  n_spaces = _wnck_pager_get_n_workspaces (pager);

  if (i < 0 || i >= n_spaces)
    return FALSE;

  /*
   * Activate the following worksapce as current workspace
   */
  wspace = _wnck_pager_get_workspace (pager, i);
  /* FIXME: Is gtk_get_current_event_time() good enough here?  I have no idea */
  _wnck_pager_activate_workspace (wspace, gtk_get_current_event_time ());

  return TRUE;
}

/*
 * Returns the AtkObject of the selected WorkSpace
 */
static AtkObject*
wnck_pager_ref_selection (AtkSelection *selection,
		          int           i)
{
  WnckPager *pager;
  GtkWidget *widget;
  WnckWorkspace *active_wspace;
  AtkObject *accessible;
  int wsno;

  g_return_val_if_fail (i == 0, NULL);

#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
#else
  widget = GTK_ACCESSIBLE (selection)->widget;
#endif
  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return NULL;
    }
  pager = WNCK_PAGER (widget);

  active_wspace = WNCK_WORKSPACE (_wnck_pager_get_active_workspace (pager));
  wsno = wnck_workspace_get_number (active_wspace);

  accessible = ATK_OBJECT (wnck_pager_accessible_ref_child (ATK_OBJECT (selection), wsno));

  return accessible;
}

/*
 * Returns the no.of child selected, it should be either 1 or 0
 * b'coz only one child can be selected at a time.
 */
static int
wnck_pager_selection_count (AtkSelection *selection)
{
  GtkWidget *widget;

#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
#else
  widget = GTK_ACCESSIBLE (selection)->widget;
#endif
  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return 0;
    }
  else
    {
      return 1;
    }
}

/*
 *Checks whether the WorkSpace specified by i is selected,
 *and returns TRUE on selection.
 */
static gboolean
wnck_pager_is_child_selected (AtkSelection *selection,
		              int           i)
{
  WnckPager *pager;
  GtkWidget *widget;
  WnckWorkspace *active_wspace;
  int wsno;

#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
#else
  widget = GTK_ACCESSIBLE (selection)->widget;
#endif
  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return FALSE;
    }

  pager = WNCK_PAGER (widget);
  active_wspace = _wnck_pager_get_active_workspace (pager);

  wsno = wnck_workspace_get_number (active_wspace);

  return (wsno == i);
}

AtkObject*
wnck_pager_accessible_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *aobj_pager;
  GtkAccessible *gtk_accessible;

  object = g_object_new (WNCK_PAGER_TYPE_ACCESSIBLE, NULL);

  aobj_pager = ATK_OBJECT (object);

  gtk_accessible = GTK_ACCESSIBLE (aobj_pager);
#if GTK_CHECK_VERSION(2,21,3)
  gtk_accessible_set_widget (gtk_accessible, widget);
#else
  gtk_accessible->widget = widget;
#endif

  atk_object_initialize (aobj_pager, widget);
  aobj_pager->role = ATK_ROLE_PANEL;

  return aobj_pager;
}

static void
wnck_pager_accessible_finalize (GObject *gobject)
{
  WnckPagerAccessiblePriv *pager_accessible_priv;
  GSList *children;

  pager_accessible_priv = get_private_data (gobject);

  if (pager_accessible_priv)
    {
      if (pager_accessible_priv->children)
        {
          children = pager_accessible_priv->children;
          g_slist_foreach (children,
                           (GFunc) g_object_unref, NULL);

          g_slist_free (children);
        }

      g_free (pager_accessible_priv);
      g_object_set_qdata (gobject,
                          quark_private_data,
                          NULL);
    }

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static const char*
wnck_pager_accessible_get_name (AtkObject *obj)
{
  g_return_val_if_fail (WNCK_PAGER_IS_ACCESSIBLE (obj), NULL);

  if (obj->name == NULL)
    obj->name = g_strdup (_("Workspace Switcher"));

  return obj->name;
}

static const char*
wnck_pager_accessible_get_description (AtkObject *obj)
{
  g_return_val_if_fail (WNCK_PAGER_IS_ACCESSIBLE (obj), NULL);

  if (obj->description == NULL)
    obj->description = g_strdup (_("Tool to switch between workspaces"));

  return obj->description;
}

/*
 * Number of workspaces is returned as n_children
 */
static int
wnck_pager_accessible_get_n_children (AtkObject* obj)
{
  GtkAccessible *accessible;
  GtkWidget *widget;
  WnckPager *pager;

  g_return_val_if_fail (WNCK_PAGER_IS_ACCESSIBLE (obj), 0);

  accessible = GTK_ACCESSIBLE (obj);
#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (accessible);
#else
  widget = accessible->widget;
#endif

  if (widget == NULL)
    /* State is defunct */
    return 0;

  pager = WNCK_PAGER (widget);

  return _wnck_pager_get_n_workspaces (pager);
}

/*
 * Will return appropriate static AtkObject for the workspaces
 */
static AtkObject*
wnck_pager_accessible_ref_child (AtkObject *obj,
                                 int        i)
{
  GtkAccessible *accessible;
  GtkWidget *widget;
  WnckPager *pager;
  int n_spaces = 0;
  int len;
  WnckPagerAccessiblePriv *pager_accessible_priv;
  AtkObject *ret;

  g_return_val_if_fail (WNCK_PAGER_IS_ACCESSIBLE (obj), NULL);
  g_return_val_if_fail (ATK_IS_OBJECT (obj), NULL);

  accessible = GTK_ACCESSIBLE (obj);
#if GTK_CHECK_VERSION(2,21,0)
  widget = gtk_accessible_get_widget (accessible);
#else
  widget = accessible->widget;
#endif

  if (widget == NULL)
    /* State is defunct */
    return NULL;

  pager = WNCK_PAGER (widget);
  pager_accessible_priv = get_private_data (G_OBJECT (obj));

  len = g_slist_length (pager_accessible_priv->children);
  n_spaces = _wnck_pager_get_n_workspaces (pager);

  if (i < 0 || i >= n_spaces)
    return NULL;

  /* We are really inefficient about this due to all the appending,
   * and never shrink the list either.
   */
  while (n_spaces > len)
    {
      AtkRegistry *default_registry;
      AtkObjectFactory *factory;
      WnckWorkspace *wspace;
      WnckWorkspaceAccessible *space_accessible;

      default_registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (default_registry,
                                          WNCK_TYPE_WORKSPACE);

      wspace = _wnck_pager_get_workspace (pager, len);
      space_accessible = WNCK_WORKSPACE_ACCESSIBLE (atk_object_factory_create_accessible (factory,
                                                                                          G_OBJECT (wspace)));
      atk_object_set_parent (ATK_OBJECT (space_accessible), obj);

      pager_accessible_priv->children = g_slist_append (pager_accessible_priv->children,
                                                   space_accessible);

      ++len;
    }

  ret = g_slist_nth_data (pager_accessible_priv->children, i);
  g_object_ref (G_OBJECT (ret));
  wnck_pager_accessible_update_workspace (ret, pager, i);

  return ret;
}

static void
wnck_pager_accessible_update_workspace (AtkObject *aobj_ws,
                                        WnckPager *pager,
                                        int        i)
{
  g_free (aobj_ws->name);
  aobj_ws->name = g_strdup (_wnck_pager_get_workspace_name (pager, i));

  g_free (aobj_ws->description);
  aobj_ws->description = g_strdup_printf (_("Click this to switch to workspace %s"),
                                          aobj_ws->name);
  aobj_ws->role = ATK_ROLE_UNKNOWN;
}

static WnckPagerAccessiblePriv*
get_private_data (GObject *gobject)
{
  WnckPagerAccessiblePriv *private_data;

  private_data = g_object_get_qdata (gobject,
                                     quark_private_data);
  if (!private_data)
    {
      private_data = g_new0 (WnckPagerAccessiblePriv, 1);
      g_object_set_qdata (gobject,
                          quark_private_data,
                          private_data);
    }
  return private_data;
}


