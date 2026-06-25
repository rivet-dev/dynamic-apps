/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include <exo/exo.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-user.h>
#include <thunar/thunar-util.h>



/* the interval in which the user/group cache is flushed (in seconds) */
#define THUNAR_USER_MANAGER_FLUSH_INTERVAL (10 * 60)




static void         thunar_group_finalize   (GObject          *object);
static ThunarGroup *thunar_group_new        (guint32           id);



struct _ThunarGroupClass
{
  GObjectClass __parent__;
};

struct _ThunarGroup
{
  GObject __parent__;

  guint32 id;
  gchar  *name;
};



G_DEFINE_TYPE (ThunarGroup, thunar_group, G_TYPE_OBJECT)



static void
thunar_group_class_init (ThunarGroupClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_group_finalize;
}



static void
thunar_group_init (ThunarGroup *group)
{
}



static void
thunar_group_finalize (GObject *object)
{
  ThunarGroup *group = THUNAR_GROUP (object);

  /* release the group's name */
  g_free (group->name);

  (*G_OBJECT_CLASS (thunar_group_parent_class)->finalize) (object);
}



static ThunarGroup*
thunar_group_new (guint32 id)
{
  ThunarGroup *group;

  group = g_object_new (THUNAR_TYPE_GROUP, NULL);
  group->id = id;

  return group;
}



/**
 * thunar_group_get_id:
 * @group : a #ThunarGroup.
 *
 * Returns the unique id of the given @group.
 *
 * Return value: the unique id of @group.
 **/
guint32
thunar_group_get_id (ThunarGroup *group)
{
  g_return_val_if_fail (THUNAR_IS_GROUP (group), 0);
  return group->id;
}



/**
 * thunar_group_get_name:
 * @group : a #ThunarGroup.
 *
 * Returns the name of the @group. If the system is
 * unable to determine the name of @group, it'll
 * return the group id as string.
 *
 * Return value: the name of @group.
 **/
const gchar*
thunar_group_get_name (ThunarGroup *group)
{
  struct group *grp;

  g_return_val_if_fail (THUNAR_IS_GROUP (group), NULL);

  /* determine the name on-demand */
  if (G_UNLIKELY (group->name == NULL))
    {
      grp = getgrgid (group->id);
      if (G_LIKELY (grp != NULL))
        group->name = g_strdup (grp->gr_name);
      else
        group->name = g_strdup_printf ("%u", (guint) group->id);
    }

  return group->name;
}



static void        thunar_user_finalize          (GObject         *object);
static void        thunar_user_load              (ThunarUser      *user);
static ThunarUser *thunar_user_new               (guint32          id);
static ThunarGroup*thunar_user_get_primary_group (ThunarUser      *user);


struct _ThunarUserClass
{
  GObjectClass __parent__;
};

struct _ThunarUser
{
  GObject __parent__;

  GList       *groups;
  ThunarGroup *primary_group;
  guint32      id;
  gchar       *name;
  gchar       *real_name;
};



static guint32 thunar_user_effective_uid;



G_DEFINE_TYPE (ThunarUser, thunar_user, G_TYPE_OBJECT)



static void
thunar_user_class_init (ThunarUserClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the current process' effective user id, we do
   * this only once to avoid the syscall overhead on every
   * is_me() invocation.
   */
  thunar_user_effective_uid = geteuid ();

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_user_finalize;
}



static void
thunar_user_init (ThunarUser *user)
{
}



static void
thunar_user_finalize (GObject *object)
{
  ThunarUser *user = THUNAR_USER (object);

  /* unref the associated groups */
  g_list_free_full (user->groups, g_object_unref);

  /* drop the reference on the primary group */
  if (G_LIKELY (user->primary_group != NULL))
    g_object_unref (G_OBJECT (user->primary_group));

  /* release the names */
  g_free (user->real_name);
  g_free (user->name);

  (*G_OBJECT_CLASS (thunar_user_parent_class)->finalize) (object);
}



static void
thunar_user_load (ThunarUser *user)
{
  ThunarUserManager *manager;
  struct passwd     *pw;
  const gchar       *s;
  gchar             *name;
  gchar             *t;

  g_return_if_fail (user->name == NULL);

  pw = getpwuid (user->id);
  if (G_LIKELY (pw != NULL))
    {
      manager = thunar_user_manager_get_default ();

      /* query name and primary group */
      user->name = g_strdup (pw->pw_name);
      user->primary_group = thunar_user_manager_get_group_by_id (manager, pw->pw_gid);

      /* try to figure out the real name */
      s = strchr (pw->pw_gecos, ',');
      if (s != NULL)
        user->real_name = g_strndup (pw->pw_gecos, s - pw->pw_gecos);
      else if (pw->pw_gecos[0] != '\0')
        user->real_name = g_strdup (pw->pw_gecos);

      /* substitute '&' in the real_name with the account name */
      if (G_LIKELY (user->real_name != NULL && strchr (user->real_name, '&') != NULL))
        {
          /* generate a version of the username with the first char upper'd */
          name = g_strdup (user->name);
          name[0] = g_ascii_toupper (name[0]);

          /* replace all occurances of '&' */
          t = xfce_str_replace (user->real_name, "&", name);
          g_free (user->real_name);
          user->real_name = t;

          /* clean up */
          g_free (name);
        }

      g_object_unref (G_OBJECT (manager));
    }
  else
    {
      user->name = g_strdup_printf ("%u", (guint) user->id);
    }
}



static ThunarUser*
thunar_user_new (guint32 id)
{
  ThunarUser *user;

  user = g_object_new (THUNAR_TYPE_USER, NULL);
  user->id = id;

  return user;
}


static ThunarGroup*
thunar_user_get_primary_group (ThunarUser *user)
{
  g_return_val_if_fail (THUNAR_IS_USER (user), NULL);

  /* load the user data on-demand */
  if (G_UNLIKELY (user->name == NULL))
    thunar_user_load (user);

  return user->primary_group;
}



/**
 * thunar_user_get_groups:
 * @user : a #ThunarUser.
 *
 * Returns all #ThunarGroup<!---->s that @user
 * belongs to. The returned list and the #ThunarGroup<!---->s
 * contained within the list are owned by @user and must not be
 * freed or altered by the caller.
 *
 * Note that if @user has a primary group, this group will
 * also be contained in the returned list.
 *
 * Return value: the groups that @user belongs to.
 **/
GList*
thunar_user_get_groups (ThunarUser *user)
{
  ThunarUserManager *manager;
  ThunarGroup       *primary_group;
  ThunarGroup       *group;
  gid_t              gidset[NGROUPS_MAX];
  gint               gidsetlen;
  gint               n;

  g_return_val_if_fail (THUNAR_IS_USER (user), NULL);

  /* load the groups on-demand */
  if (G_UNLIKELY (user->groups == NULL))
    {
      primary_group = thunar_user_get_primary_group (user);

      /* we can only determine the groups list for the current
       * process owner in a portable fashion, and in fact, we
       * only need the list for the current user.
       */
      if (thunar_user_is_me (user))
        {
          manager = thunar_user_manager_get_default ();

          /* add all supplementary groups */
          gidsetlen = getgroups (G_N_ELEMENTS (gidset), gidset);
          for (n = 0; n < gidsetlen; ++n)
            if (primary_group == NULL || thunar_group_get_id (primary_group) != gidset[n])
              {
                group = thunar_user_manager_get_group_by_id (manager, gidset[n]);
                if (G_LIKELY (group != NULL))
                  user->groups = g_list_append (user->groups, group);
              }

          g_object_unref (G_OBJECT (manager));
        }

      /* prepend the primary group (if any) */
      if (G_LIKELY (primary_group != NULL))
        {
          user->groups = g_list_prepend (user->groups, primary_group);
          g_object_ref (G_OBJECT (primary_group));
        }
    }

  return user->groups;
}



/**
 * thunar_user_get_name:
 * @user : a #ThunarUser.
 *
 * Returns the name of @user. If the system is
 * unable to determine the account name of @user,
 * it'll return the user id as string.
 *
 * Return value: the name of @user.
 **/
const gchar*
thunar_user_get_name (ThunarUser *user)
{
  g_return_val_if_fail (THUNAR_IS_USER (user), 0);

  /* load the user's data on-demand */
  if (G_UNLIKELY (user->name == NULL))
    thunar_user_load (user);

  return user->name;
}



/**
 * thunar_user_get_real_name:
 * @user : a #ThunarUser.
 *
 * Returns the real name of @user or %NULL if the
 * real name for @user is not known to the underlying
 * system.
 *
 * Return value: the real name for @user or %NULL.
 **/
const gchar*
thunar_user_get_real_name (ThunarUser *user)
{
  g_return_val_if_fail (THUNAR_IS_USER (user), 0);

  /* load the user's data on-demand */
  if (G_UNLIKELY (user->name == NULL))
    thunar_user_load (user);

  return user->real_name;
}



/**
 * thunar_user_is_me:
 * @user : a #ThunarUser.
 *
 * Checks whether the owner of the current process is
 * described by @user.
 *
 * Return value: %TRUE if @user is the owner of the current
 *               process, else %FALSE.
 **/
gboolean
thunar_user_is_me (ThunarUser *user)
{
  g_return_val_if_fail (THUNAR_IS_USER (user), FALSE);
  return (user->id == thunar_user_effective_uid);
}




static void     thunar_user_manager_finalize            (GObject                *object);
static gboolean thunar_user_manager_flush_timer         (gpointer                user_data);
static void     thunar_user_manager_flush_timer_destroy (gpointer                user_data);



struct _ThunarUserManagerClass
{
  GObjectClass __parent__;
};

struct _ThunarUserManager
{
  GObject __parent__;

  GHashTable *groups;
  GHashTable *users;

  guint       flush_timer_id;
};



G_DEFINE_TYPE (ThunarUserManager, thunar_user_manager, G_TYPE_OBJECT)



static void
thunar_user_manager_class_init (ThunarUserManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_user_manager_finalize;
}



static void
thunar_user_manager_init (ThunarUserManager *manager)
{
  manager->groups = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  manager->users = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  /* keep the groups file in memory if possible */
#ifdef HAVE_SETGROUPENT
  setgroupent (TRUE);
#endif

  /* keep the passwd file in memory if possible */
#ifdef HAVE_SETPASSENT
  setpassent (TRUE);
#endif

  /* start the flush timer */
  manager->flush_timer_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, THUNAR_USER_MANAGER_FLUSH_INTERVAL,
                                                        thunar_user_manager_flush_timer, manager,
                                                        thunar_user_manager_flush_timer_destroy);
}



static void
thunar_user_manager_finalize (GObject *object)
{
  ThunarUserManager *manager = THUNAR_USER_MANAGER (object);

  /* stop the flush timer */
  if (G_LIKELY (manager->flush_timer_id != 0))
    g_source_remove (manager->flush_timer_id);

  /* destroy the hash tables */
  g_hash_table_destroy (manager->groups);
  g_hash_table_destroy (manager->users);

  /* unload the groups file */
  endgrent ();

  /* unload the passwd file */
  endpwent ();

  (*G_OBJECT_CLASS (thunar_user_manager_parent_class)->finalize) (object);
}



static gboolean
thunar_user_manager_flush_timer (gpointer user_data)
{
  ThunarUserManager *manager = THUNAR_USER_MANAGER (user_data);
  guint              size = 0;

THUNAR_THREADS_ENTER

  /* drop all cached groups */
  size += g_hash_table_foreach_remove (manager->groups,
                                       (GHRFunc) (void (*)(void)) gtk_true,
                                       NULL);

  /* drop all cached users */
  size += g_hash_table_foreach_remove (manager->users,
                                       (GHRFunc) (void (*)(void)) gtk_true,
                                       NULL);

  /* reload groups and passwd files if we had cached entities */
  if (G_LIKELY (size > 0))
    {
      endgrent ();
      endpwent ();

#ifdef HAVE_SETGROUPENT
      setgroupent (TRUE);
#endif

#ifdef HAVE_SETPASSENT
      setpassent (TRUE);
#endif
    }

THUNAR_THREADS_LEAVE

  return TRUE;
}



static void
thunar_user_manager_flush_timer_destroy (gpointer user_data)
{
  THUNAR_USER_MANAGER (user_data)->flush_timer_id = 0;
}



/**
 * thunar_user_manager_get_default:
 *
 * Returns the default #ThunarUserManager instance, which is shared
 * by all modules using the user module. Call g_object_unref() on the
 * returned object when you are done with it.
 *
 * Return value: the default #ThunarUserManager instance.
 **/
ThunarUserManager*
thunar_user_manager_get_default (void)
{
  static ThunarUserManager *manager = NULL;

  if (G_UNLIKELY (manager == NULL))
    {
      manager = g_object_new (THUNAR_TYPE_USER_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
    }
  else
    {
      g_object_ref (G_OBJECT (manager));
    }

  return manager;
}



/**
 * thunar_user_manager_get_group_by_id:
 * @manager : a #ThunarUserManager.
 * @id      : the group id.
 *
 * Looks up the #ThunarGroup corresponding to @id in @manager. Returns
 * %NULL if @manager is unable to determine the #ThunarGroup for @id,
 * else a pointer to the corresponding #ThunarGroup. The caller is
 * responsible for freeing the returned object using g_object_unref().
 *
 * Return value: the #ThunarGroup corresponding to @id or %NULL.
 **/
ThunarGroup*
thunar_user_manager_get_group_by_id (ThunarUserManager *manager,
                                     guint32            id)
{
  ThunarGroup *group;

  g_return_val_if_fail (THUNAR_IS_USER_MANAGER (manager), NULL);

  /* lookup/load the group corresponding to id */
  group = g_hash_table_lookup (manager->groups, GINT_TO_POINTER (id));
  if (group == NULL)
    {
      group = thunar_group_new (id);
      g_hash_table_insert (manager->groups, GINT_TO_POINTER (id), group);
    }

  /* take a reference for the caller */
  g_object_ref (G_OBJECT (group));

  return group;
}



/**
 * thunar_user_manager_get_user_by_id:
 * @manager : a #ThunarUserManager.
 * @id      : the user id.
 *
 * Looks up the #ThunarUser corresponding to @id in @manager. Returns
 * %NULL if @manager is unable to determine the #ThunarUser for @id,
 * else a pointer to the corresponding #ThunarUser. The caller is
 * responsible for freeing the returned object using g_object_unref().
 *
 * Return value: the #ThunarUser corresponding to @id or %NULL.
 **/
ThunarUser*
thunar_user_manager_get_user_by_id (ThunarUserManager *manager,
                                    guint32            id)
{
  ThunarUser *user;

  g_return_val_if_fail (THUNAR_IS_USER_MANAGER (manager), NULL);

  /* lookup/load the user corresponding to id */
  user = g_hash_table_lookup (manager->users, GINT_TO_POINTER (id));
  if (user == NULL)
    {
      user = thunar_user_new (id);
      g_hash_table_insert (manager->users, GINT_TO_POINTER (id), user);
    }

  /* take a reference for the caller */
  g_object_ref (G_OBJECT (user));

  return user;
}



/**
 * thunar_user_manager_get_all_groups:
 * @manager : a #ThunarUserManager.
 *
 * Returns the list of all #ThunarGroup<!---->s in the system
 * that are known to the @manager.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: the list of all groups known to the @manager.
 **/
GList*
thunar_user_manager_get_all_groups (ThunarUserManager *manager)
{
  ThunarGroup  *group;
  struct group *grp;
  GList        *groups = NULL;

  g_return_val_if_fail (THUNAR_IS_USER_MANAGER (manager), NULL);

  /* make sure we reload the groups list */
  endgrent ();

  /* iterate through all groups in the system */
  for (;;)
    {
      /* lookup the next group */
      grp = getgrent ();
      if (G_UNLIKELY (grp == NULL))
        break;

      /* lookup our version of the group */
      group = thunar_user_manager_get_group_by_id (manager, grp->gr_gid);
      if (G_LIKELY (group != NULL))
        groups = g_list_append (groups, group);
    }

  return groups;
}
