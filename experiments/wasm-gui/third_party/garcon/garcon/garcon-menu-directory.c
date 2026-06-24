/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <locale.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h>

#include <garcon/garcon-environment.h>
#include <garcon/garcon-menu-directory.h>
#include <garcon/garcon-private.h>



/**
 * SECTION: garcon-menu-directory
 * @title: GarconMenuDirectory
 * @short_description: Garcon element for .directory files.
 * @include: garcon/garcon.h
 *
 * Element that represents a .directory file in the menu configurations.
 * Each menu (except for the root menu) has a #GarconMenuDirectory,
 * see garcon_menu_get_directory().
 **/



/* Desktop entry keys */
#if 0
static const gchar *desktop_entry_keys[] =
{
  "Name",
  "Comment",
  "Icon",
  "Categories",
  "OnlyShowIn",
  "NotShowIn",
  "NoDisplay",
  "Hidden",
  NULL
};
#endif



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
  PROP_NAME,
  PROP_COMMENT,
  PROP_NO_DISPLAY,
  PROP_ICON_NAME,
};



static void garcon_menu_directory_finalize     (GObject                  *object);
static void garcon_menu_directory_get_property (GObject                  *object,
                                                guint                     prop_id,
                                                GValue                   *value,
                                                GParamSpec               *pspec);
static void garcon_menu_directory_set_property (GObject                  *object,
                                                guint                     prop_id,
                                                const GValue             *value,
                                                GParamSpec               *pspec);



struct _GarconMenuDirectoryPrivate
{
  /* Directory file */
  GFile  *file;

  /* Directory name */
  gchar  *name;

  /* Directory description (comment) */
  gchar  *comment;

  /* Icon */
  gchar  *icon_name;

  /* Environments in which the menu should be displayed only */
  gchar **only_show_in;

  /* Environments in which the menu should be hidden */
  gchar **not_show_in;

  /* Whether the menu should be ignored completely */
  guint   hidden : 1;

  /* Whether the menu should be hidden */
  guint   no_display : 1;
};



/* TODO, maybe implement the GarconMenuElement interface */
G_DEFINE_TYPE_WITH_PRIVATE (GarconMenuDirectory, garcon_menu_directory, G_TYPE_OBJECT)



static void
garcon_menu_directory_class_init (GarconMenuDirectoryClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_directory_finalize;
  gobject_class->get_property = garcon_menu_directory_get_property;
  gobject_class->set_property = garcon_menu_directory_set_property;

  /**
   * GarconMenuDirectory:filename:
   *
   * The @GFile of an %GarconMenuDirectory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "File",
                                                        "File",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT_ONLY));

  /**
   * GarconMenuDirectory:name:
   *
   * Name of the directory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Directory name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuDirectory:comment:
   *
   * Directory description (comment).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMENT,
                                   g_param_spec_string ("comment",
                                                        "Description",
                                                        "Directory description",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuDirectory:icon-name:
   *
   * Icon associated with this directory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "Icon",
                                                        "Directory icon",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuDirectory:no-display:
   *
   * Whether this menu item is hidden in menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NO_DISPLAY,
                                   g_param_spec_boolean ("no-display",
                                                         "No Display",
                                                         "Visibility state of the related menu",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

}



static void
garcon_menu_directory_init (GarconMenuDirectory *directory)
{
  directory->priv = garcon_menu_directory_get_instance_private (directory);
  directory->priv->file = NULL;
  directory->priv->name = NULL;
  directory->priv->icon_name = NULL;
  directory->priv->only_show_in = NULL;
  directory->priv->not_show_in = NULL;
  directory->priv->hidden = FALSE;
  directory->priv->no_display = FALSE;
}



static void
garcon_menu_directory_finalize (GObject *object)
{
  GarconMenuDirectory *directory = GARCON_MENU_DIRECTORY (object);

  /* Free name */
  g_free (directory->priv->name);

  /* Free comment */
  g_free (directory->priv->comment);

  /* Free icon_name */
  g_free (directory->priv->icon_name);

  /* Free environment lists */
  g_strfreev (directory->priv->only_show_in);
  g_strfreev (directory->priv->not_show_in);

  /* Free file */
  if (directory->priv->file != NULL)
    g_object_unref (directory->priv->file);

  (*G_OBJECT_CLASS (garcon_menu_directory_parent_class)->finalize) (object);
}



static void
garcon_menu_directory_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GarconMenuDirectory *directory = GARCON_MENU_DIRECTORY (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, directory->priv->file);
      break;

    case PROP_NAME:
      g_value_set_string (value, garcon_menu_directory_get_name (directory));
      break;

    case PROP_COMMENT:
      g_value_set_string (value, garcon_menu_directory_get_comment (directory));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, garcon_menu_directory_get_icon_name (directory));
      break;

    case PROP_NO_DISPLAY:
      g_value_set_boolean (value, garcon_menu_directory_get_no_display (directory));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_directory_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GarconMenuDirectory *directory = GARCON_MENU_DIRECTORY (object);

  switch (prop_id)
    {
    case PROP_FILE:
      directory->priv->file = g_value_dup_object (value);
      break;

    case PROP_NAME:
      garcon_menu_directory_set_name (directory, g_value_get_string (value));
      break;

    case PROP_COMMENT:
      garcon_menu_directory_set_comment (directory, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      garcon_menu_directory_set_icon_name (directory, g_value_get_string (value));
      break;

    case PROP_NO_DISPLAY:
      garcon_menu_directory_set_no_display (directory, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * garcon_menu_directory_new: (method)
 * @file : a #GFile
 *
 * Create a new #GarconMenuDirectory for @file. You most likely never
 * use this, but retrieve the info from garcon_menu_get_directory().
 *
 * Returns: a #GarconMenuDirectory.
 **/
GarconMenuDirectory *
garcon_menu_directory_new (GFile *file)
{
  GarconMenuDirectory *directory = NULL;
  XfceRc              *rc;
  const gchar         *name;
  const gchar         *comment;
  const gchar         *icon_name;
  gboolean             no_display;
  gchar               *filename;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (g_file_is_native (file), NULL);

 /* Open the rc file */
  filename = g_file_get_path (file);
  rc = xfce_rc_simple_open (filename, TRUE);
  g_free (filename);
  if (G_UNLIKELY (rc == NULL))
    return NULL;

  xfce_rc_set_group (rc, G_KEY_FILE_DESKTOP_GROUP);

  /* Parse name, exec command and icon name */
  name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);

  /* If there is no name we must bail out now or segfault later */
  if (G_UNLIKELY (name == NULL))
    return NULL;

  comment = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL);
  icon_name = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
  no_display = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, FALSE);

  /* Allocate a new directory instance */
  directory = g_object_new (GARCON_TYPE_MENU_DIRECTORY,
                            "file", file,
                            "name", name,
                            "comment", comment,
                            "icon-name", icon_name,
                            "no-display", no_display,
                            NULL);

  /* Set rest of the private data directly */
  directory->priv->only_show_in = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, ";");
  directory->priv->not_show_in = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, ";");
  directory->priv->hidden = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_HIDDEN, FALSE);

  /* Cleanup */
  xfce_rc_close (rc);

  return directory;
}



/**
 * garcon_menu_directory_get_file:
 * @directory: a #GarconMenuDirectory
 *
 * Get the #GFile for @directory. The returned object should be
 * unreffed with g_object_unref() when no longer needed.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
garcon_menu_directory_get_file (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), NULL);
  return g_object_ref (directory->priv->file);
}



/**
 * garcon_menu_directory_get_name:
 * @directory : a #GarconMenuDirectory
 *
 * Get the name of @directory.
 *
 * Returns: a the name for @directory.
 */
const gchar*
garcon_menu_directory_get_name (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->name;
}



/**
 * garcon_menu_directory_set_name:
 * @directory : a #GarconMenuDirectory
 * @name : the new name for @directory.
 *
 * Set the name of @directory.
 */
void
garcon_menu_directory_set_name (GarconMenuDirectory *directory,
                                const gchar         *name)
{
  g_return_if_fail (GARCON_IS_MENU_DIRECTORY (directory));
  g_return_if_fail (g_utf8_validate (name, -1, NULL));

  if (g_strcmp0 (directory->priv->name, name) == 0)
    return;

  /* Free old name */
  g_free (directory->priv->name);

  /* Set the new filename */
  directory->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "name");
}



/**
 * garcon_menu_directory_get_comment:
 * @directory : a #GarconMenuDirectory
 *
 * Get the comment of @directory.
 *
 * Returns: a the description for @directory.
 */
const gchar*
garcon_menu_directory_get_comment (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->comment;
}



/**
 * garcon_menu_directory_set_comment:
 * @directory : a #GarconMenuDirectory
 * @comment : the new description for @directory.
 *
 * Set the comment of @directory.
 */
void
garcon_menu_directory_set_comment (GarconMenuDirectory *directory,
                                   const gchar         *comment)
{
  g_return_if_fail (GARCON_IS_MENU_DIRECTORY (directory));
  g_return_if_fail (comment == NULL || g_utf8_validate (comment, -1, NULL));

  if (g_strcmp0 (directory->priv->comment, comment) == 0)
    return;

  /* Free old name */
  g_free (directory->priv->comment);

  /* Set the new filename */
  directory->priv->comment = g_strdup (comment);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "comment");
}



/**
 * garcon_menu_directory_get_icon_name:
 * @directory : a #GarconMenuDirectory
 *
 * Get the icon name of @directory.
 *
 * Returns: a the icon-name key for @directory.
 */
const gchar*
garcon_menu_directory_get_icon_name (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->icon_name;
}



/**
 * garcon_menu_directory_set_icon_name:
 * @directory : a #GarconMenuDirectory
 * @icon_name      : the new icon name for @directory.
 *
 * Set the icon name of @directory.
 */
void
garcon_menu_directory_set_icon_name (GarconMenuDirectory *directory,
                                     const gchar         *icon_name)
{
  g_return_if_fail (GARCON_IS_MENU_DIRECTORY (directory));

  if (g_strcmp0 (directory->priv->icon_name, icon_name) == 0)
    return;

  /* Free old name */
  g_free (directory->priv->icon_name);

  /* Set the new filename */
  directory->priv->icon_name = g_strdup (icon_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "icon-name");
}



/**
 * garcon_menu_directory_get_no_display:
 * @directory : a #GarconMenuDirectory
 *
 * Whether @directory should be displayed.
 * For applications you want to call garcon_menu_directory_get_visible().
 *
 * Returns: a the no-display key for @directory.
 */
gboolean
garcon_menu_directory_get_no_display (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), FALSE);
  return directory->priv->no_display;
}



/**
 * garcon_menu_directory_set_no_display:
 * @directory : a #GarconMenuDirectory
 * @no_display : whether @directory should be displayed.
 *
 * Set the NoDisplay key of @directory.
 */
void
garcon_menu_directory_set_no_display (GarconMenuDirectory *directory,
                                      gboolean             no_display)
{
  g_return_if_fail (GARCON_IS_MENU_DIRECTORY (directory));

  /* Abort if old and new value are equal */
  if (directory->priv->no_display == no_display)
    return;

  /* Assign new value */
  directory->priv->no_display = no_display;

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "no-display");
}



/**
 * garcon_menu_directory_get_hidden:
 * @directory : a #GarconMenuDirectory
 *
 * Whether @directory should be hidden.
 * For applications you want to call garcon_menu_directory_get_visible().
 *
 * Returns: a the hidden key for @directory.
 */
gboolean
garcon_menu_directory_get_hidden (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), FALSE);
  return directory->priv->hidden;
}



/**
 * garcon_menu_directory_get_show_in_environment:
 * @directory : a #GarconMenuDirectory
 *
 * Whether @directory is visible in the current environment
 * which has been set by garcon_set_environment().
 * For applications you want to call garcon_menu_directory_get_visible().
 *
 * Returns: %TRUE is visible in environment, else %FALSE.
 */
gboolean
garcon_menu_directory_get_show_in_environment (GarconMenuDirectory *directory)
{
  const gchar *env;
  guint        i;
  gboolean     show = TRUE;

  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), FALSE);

  /* Determine current environment */
  env = garcon_get_environment ();

  /* If no environment has been set, the menu is displayed no matter what
   * OnlyShowIn or NotShowIn contain */
  if (G_UNLIKELY (env == NULL))
    return TRUE;

  /* According to the spec there is either a OnlyShowIn or a NotShowIn list */
  if (G_UNLIKELY (directory->priv->only_show_in != NULL))
    {
      /* Check if your environemnt is in OnlyShowIn list */
      for (i = 0, show = FALSE; !show && directory->priv->only_show_in[i] != NULL; i++)
        if (g_strcmp0 (directory->priv->only_show_in[i], env) == 0)
          show = TRUE;
    }
  else if (G_UNLIKELY (directory->priv->not_show_in != NULL))
    {
      /* Check if your environemnt is in NotShowIn list */
      for (i = 0, show = TRUE; show && directory->priv->not_show_in[i] != NULL; i++)
        if (g_strcmp0 (directory->priv->not_show_in[i], env) == 0)
          show = FALSE;
    }

  return show;
}



/**
 * garcon_menu_directory_get_visible:
 * @directory : a #GarconMenuDirectory
 *
 * Check which runs the following checks:
 * garcon_menu_directory_get_show_in_environment(),
 * garcon_menu_directory_get_hidden() and
 * garcon_menu_directory_get_no_display().
 *
 * Returns: if visible %TRUE, else %FALSE.
 **/
gboolean
garcon_menu_directory_get_visible (GarconMenuDirectory *directory)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), FALSE);

  if (!garcon_menu_directory_get_show_in_environment (directory))
    return FALSE;
  else if (garcon_menu_directory_get_hidden (directory))
    return FALSE;
  else if (garcon_menu_directory_get_no_display (directory))
    return FALSE;

  return TRUE;
}


/**
 * garcon_menu_directory_equal:
 * @directory : a #GarconMenuDirectory
 * @other : a #GarconMenuDirectory
 *
 * Checks if both directories point to the same file.
 *
 * Returns: if files are equal %TRUE, else %FALSE.
 **/
gboolean
garcon_menu_directory_equal (GarconMenuDirectory *directory,
                             GarconMenuDirectory *other)
{
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (directory), FALSE);
  g_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (other), FALSE);
  return g_file_equal (directory->priv->file, other->priv->file);
}
