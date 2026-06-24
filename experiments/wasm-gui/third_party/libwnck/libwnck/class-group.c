/* class group object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2003 Ximian, Inc.
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 * Copyright (C) 2006-2007 Vincent Untz
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

#undef WNCK_DISABLE_DEPRECATED

#include <string.h>
#include "class-group.h"
#include "window.h"
#include "private.h"

/**
 * SECTION:class-group
 * @short_description: an object representing a group of windows of the same
 * class.
 * @see_also: wnck_window_get_class_group()
 * @stability: Unstable
 *
 * The #WnckClassGroup is a group of #WnckWindow that are all in the same
 * class. It can be used to represent windows by classes, group windows by
 * classes or to manipulate all windows of a particular class.
 *
 * The class of a window is defined by the WM_CLASS property of this window.
 * More information about the WM_CLASS property is available in the <ulink
 * url="http://tronche.com/gui/x/icccm/sec-4.html&num;s-4.1.2.5">WM_CLASS Property</ulink>
 * section (section 4.1.2.5) of the <ulink
 * url="http://tronche.com/gui/x/icccm/">ICCCM</ulink>.
 *
 * The #WnckClassGroup objects are always owned by libwnck and must not be
 * referenced or unreferenced.
 */

/* Private part of the WnckClassGroup structure */
struct _WnckClassGroupPrivate {
  char *res_class;
  char *name;
  GList *windows;

  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
};

G_DEFINE_TYPE (WnckClassGroup, wnck_class_group, G_TYPE_OBJECT);
#define WNCK_CLASS_GROUP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_CLASS_GROUP, WnckClassGroupPrivate))

/* Hash table that maps res_class strings -> WnckClassGroup instances */
static GHashTable *class_group_hash = NULL;



static void wnck_class_group_class_init  (WnckClassGroupClass *class);
static void wnck_class_group_init        (WnckClassGroup      *class_group);
static void wnck_class_group_finalize    (GObject             *object);

enum {
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

void
_wnck_class_group_shutdown_all (void)
{
  if (class_group_hash != NULL)
    {
      g_hash_table_destroy (class_group_hash);
      class_group_hash = NULL;
    }
}

static void
wnck_class_group_class_init (WnckClassGroupClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (WnckClassGroupPrivate));

  gobject_class->finalize = wnck_class_group_finalize;

  /**
   * WnckClassGroup::name-changed:
   * @class_group: the #WnckClassGroup which emitted the signal.
   *
   * Emitted when the name of @class_group changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckClassGroupClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * WnckClassGroup::icon-changed:
   * @class_group: the #WnckClassGroup which emitted the signal.
   *
   * Emitted when the icon of @class_group changes.
   */
  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckClassGroupClass, icon_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
wnck_class_group_init (WnckClassGroup *class_group)
{
  class_group->priv = WNCK_CLASS_GROUP_GET_PRIVATE (class_group);
}

static void
wnck_class_group_finalize (GObject *object)
{
  WnckClassGroup *class_group;

  class_group = WNCK_CLASS_GROUP (object);

  if (class_group->priv->res_class)
    g_free (class_group->priv->res_class);
  class_group->priv->res_class = NULL;

  if (class_group->priv->name)
    g_free (class_group->priv->name);
  class_group->priv->name = NULL;

  g_list_free (class_group->priv->windows);
  class_group->priv->windows = NULL;

  if (class_group->priv->icon)
    g_object_unref (class_group->priv->icon);
  class_group->priv->icon = NULL;

  if (class_group->priv->mini_icon)
    g_object_unref (class_group->priv->mini_icon);
  class_group->priv->mini_icon = NULL;

  G_OBJECT_CLASS (wnck_class_group_parent_class)->finalize (object);
}

/**
 * wnck_class_group_get:
 * @id: identifier name of the sought resource class.
 *
 * Gets the #WnckClassGroup corresponding to @id.
 *
 * Return value: (transfer none): the #WnckClassGroup corresponding to
 * @id, or %NULL if there is no #WnckClassGroup with the specified
 * @id. The returned #WnckClassGroup is owned by libwnck and must not be
 * referenced or unreferenced.
 *
 * Since: 2.2
 **/
WnckClassGroup *
wnck_class_group_get (const char *id)
{
  if (!class_group_hash)
    return NULL;
  else
    return g_hash_table_lookup (class_group_hash, id ? id : "");
}

/**
 * _wnck_class_group_create:
 * @res_class: name of the resource class for the group.
 *
 * Creates a new WnckClassGroup with the specified resource class name.  If
 * @res_class is #NULL, then windows without a resource class name will get
 * grouped under this class group.
 *
 * Return value: a newly-created #WnckClassGroup, or an existing one that
 * matches the @res_class.
 **/
WnckClassGroup *
_wnck_class_group_create (const char *res_class)
{
  WnckClassGroup *class_group;

  if (class_group_hash == NULL)
    class_group_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, g_object_unref);

  g_return_val_if_fail (g_hash_table_lookup (class_group_hash, res_class ? res_class : "") == NULL,
			NULL);

  class_group = g_object_new (WNCK_TYPE_CLASS_GROUP, NULL);

  class_group->priv->res_class = g_strdup (res_class ? res_class : "");

  g_hash_table_insert (class_group_hash,
                       class_group->priv->res_class, class_group);
  /* Hash now owns one ref, caller gets none */

  return class_group;
}

/**
 * _wnck_class_group_destroy:
 * @class_group: a #WnckClassGroup.
 *
 * Destroys the specified @class_group.
 **/
void
_wnck_class_group_destroy (WnckClassGroup *class_group)
{
  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));

  g_hash_table_remove (class_group_hash, class_group->priv->res_class);

  /* Removing from hash also removes the only ref WnckClassGroup had */
}

static const char *
get_name_from_applications (WnckClassGroup *class_group)
{
  const char *first_name;
  GList *l;

  /* Try to get the name from the group leaders.  If all have the same name, we
   * can use that.
   */

  first_name = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      WnckWindow *w;
      WnckApplication *app;

      w = WNCK_WINDOW (l->data);
      app = wnck_window_get_application (w);

      if (!first_name)
	{
	  if (app)
	    first_name = wnck_application_get_name (app);
	}
      else
	{
	  if (!app || strcmp (first_name, wnck_application_get_name (app)) != 0)
	    break;
	}
    }

  if (!l)
    {
      /* All names are the same, so use one of them */
      return first_name;
    }
  else
    return NULL;
}

static const char *
get_name_from_windows (WnckClassGroup *class_group)
{
  const char *first_name;
  GList *l;

  /* Try to get the name from windows, following the same rationale as
   * get_name_from_applications()
   */

  first_name = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      WnckWindow *window;

      window = WNCK_WINDOW (l->data);

      if (!first_name)
	first_name = wnck_window_get_name (window);
      else
	if (strcmp (first_name, wnck_window_get_name (window)) != 0)
	  break;
    }

  if (!l)
    {
      /* All names are the same, so use one of them */
      return first_name;
    }
  else
    return NULL;
}


/* Gets a sensible name for the class group from the application group leaders
 * or from individual windows.
 */
static void
set_name (WnckClassGroup *class_group)
{
  const char *new_name;

  if (class_group->priv->name)
    {
      g_free (class_group->priv->name);
      class_group->priv->name = NULL;
    }

  new_name = get_name_from_applications (class_group);

  if (!new_name)
    {
      new_name = get_name_from_windows (class_group);

      if (!new_name)
	new_name = class_group->priv->res_class;
    }

  g_assert (new_name != NULL);

  if (!class_group->priv->name ||
      strcmp (class_group->priv->name, new_name) != 0)
    {
      g_free (class_group->priv->name);
      class_group->priv->name = g_strdup (new_name);

      g_signal_emit (G_OBJECT (class_group), signals[NAME_CHANGED], 0);
    }
}

/* Walks the list of applications, trying to get an icon from them */
static void
get_icons_from_applications (WnckClassGroup *class_group, GdkPixbuf **icon, GdkPixbuf **mini_icon)
{
  GList *l;

  *icon = NULL;
  *mini_icon = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      WnckWindow *window;
      WnckApplication *app;

      window = WNCK_WINDOW (l->data);
      app = wnck_window_get_application (window);
      if (app)
	{
	  *icon = wnck_application_get_icon (app);
	  *mini_icon = wnck_application_get_mini_icon (app);

	  if (*icon && *mini_icon)
	    return;
	  else
	    {
	      *icon = NULL;
	      *mini_icon = NULL;
	    }
	}
    }
}

/* Walks the list of windows, trying to get an icon from them */
static void
get_icons_from_windows (WnckClassGroup *class_group, GdkPixbuf **icon, GdkPixbuf **mini_icon)
{
  GList *l;

  *icon = NULL;
  *mini_icon = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      WnckWindow *window;

      window = WNCK_WINDOW (l->data);

      *icon = wnck_window_get_icon (window);
      *mini_icon = wnck_window_get_mini_icon (window);

      if (*icon && *mini_icon)
	return;
      else
	{
	  *icon = NULL;
	  *mini_icon = NULL;
	}
    }
}

/* Gets a sensible icon and mini_icon for the class group from the application
 * group leaders or from individual windows.
 */
static void
set_icon (WnckClassGroup *class_group)
{
  GdkPixbuf *icon, *mini_icon;
  gboolean icons_reffed = FALSE;

  get_icons_from_applications (class_group, &icon, &mini_icon);

  if (!icon || !mini_icon)
    get_icons_from_windows (class_group, &icon, &mini_icon);

  if (!icon || !mini_icon)
    {
      _wnck_get_fallback_icons (&icon,
                                _wnck_get_default_icon_size (),
                                _wnck_get_default_icon_size (),
                                &mini_icon,
                                _wnck_get_default_mini_icon_size (),
                                _wnck_get_default_mini_icon_size ());
      icons_reffed = TRUE;
    }

  g_assert (icon && mini_icon);

  if (class_group->priv->icon)
    g_object_unref (class_group->priv->icon);

  if (class_group->priv->mini_icon)
    g_object_unref (class_group->priv->mini_icon);

  class_group->priv->icon = icon;
  class_group->priv->mini_icon = mini_icon;

  if (!icons_reffed)
    {
      g_object_ref (class_group->priv->icon);
      g_object_ref (class_group->priv->mini_icon);
    }

  g_signal_emit (G_OBJECT (class_group), signals[ICON_CHANGED], 0);
}

/**
 * _wnck_class_group_add_window:
 * @class_group: a #WnckClassGroup.
 * @window: a #WnckWindow.
 *
 * Adds a window to @class_group.  You should only do this if the resource
 * class of the window matches the @class_group<!-- -->'s.
 **/
void
_wnck_class_group_add_window (WnckClassGroup *class_group,
                              WnckWindow     *window)
{

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_class_group (window) == NULL);

  class_group->priv->windows = g_list_prepend (class_group->priv->windows,
                                               window);
  _wnck_window_set_class_group (window, class_group);

  set_name (class_group);
  set_icon (class_group);

  /* FIXME: should we monitor class group changes on the window?  The ICCCM says
   * that clients should never change WM_CLASS unless the window is withdrawn.
   */
}

/**
 * _wnck_class_group_remove_window:
 * @class_group: a #WnckClassGroup.
 * @window: a #WnckWindow.
 *
 * Removes a window from the list of windows that are grouped under the
 * specified @class_group.
 **/
void
_wnck_class_group_remove_window (WnckClassGroup *class_group,
				 WnckWindow     *window)
{
  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_class_group (window) == class_group);

  class_group->priv->windows = g_list_remove (class_group->priv->windows,
                                              window);
  _wnck_window_set_class_group (window, NULL);

  set_name (class_group);
  set_icon (class_group);
}

/**
 * wnck_class_group_get_windows:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the list of #WnckWindow that are grouped in @class_group.
 *
 * Return value: (element-type WnckWindow) (transfer none): the list of
 * #WnckWindow grouped in @class_group, or %NULL if the group contains no
 * window. The list should not be modified nor freed, as it is owned by
 * @class_group.
 *
 * Since: 2.2
 **/
GList *
wnck_class_group_get_windows (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->windows;
}

/**
 * wnck_class_group_get_id:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the identifier name for @class_group. This is the resource class for
 * @class_group.
 *
 * Return value: the identifier name of @class_group, or an
 * empty string if the group has no identifier name.
 *
 * Since: 3.2
 **/
const char *
wnck_class_group_get_id (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->res_class;
}

/**
 * wnck_class_group_get_res_class:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the resource class name for @class_group.
 *
 * Return value: the resource class name of @class_group, or an
 * empty string if the group has no resource class name.
 *
 * Since: 2.2
 * Deprecated:3.2: Use wnck_class_group_get_id() instead.
 **/
const char *
wnck_class_group_get_res_class (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->res_class;
}

/**
 * wnck_class_group_get_name:
 * @class_group: a #WnckClassGroup.
 *
 * Gets an human-readable name for @class_group. Since there is no way to
 * properly find this name, a suboptimal heuristic is used to find it. The name
 * is the name of all #WnckApplication for each #WnckWindow in @class_group if
 * they all have the same name. If all #WnckApplication don't have the same
 * name, the name is the name of all #WnckWindow in @class_group if they all
 * have the same name. If all #WnckWindow don't have the same name, the
 * resource class name is used.
 *
 * Return value: an human-readable name for @class_group.
 *
 * Since: 2.2
 **/
const char *
wnck_class_group_get_name (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->name;
}

/**
 * wnck_class_group_get_icon:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the icon to be used for @class_group. Since there is no way to
 * properly find the icon, a suboptimal heuristic is used to find it. The icon
 * is the first icon found by looking at all the #WnckApplication for each
 * #WnckWindow in @class_group, then at all the #WnckWindow in @class_group. If
 * no icon was found, a fallback icon is used.
 *
 * Return value: (transfer none): the icon for @class_group. The caller should
 * reference the returned <classname>GdkPixbuf</classname> if it needs to keep
 * the icon around.
 *
 * Since: 2.2
 **/
GdkPixbuf *
wnck_class_group_get_icon (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->icon;
}

/**
 * wnck_class_group_get_mini_icon:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the mini-icon to be used for @class_group. Since there is no way to
 * properly find the mini-icon, the same suboptimal heuristic as the one for
 * wnck_class_group_get_icon() is used to find it.
 *
 * Return value: (transfer none): the mini-icon for @class_group. The caller
 * should reference the returned <classname>GdkPixbuf</classname> if it needs
 * to keep the mini-icon around.
 *
 * Since: 2.2
 **/
GdkPixbuf *
wnck_class_group_get_mini_icon (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->mini_icon;
}
