/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2010 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2009-2010 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2015      Danila Poyarkov <dannotemail@gmail.com>
 * Copyright (c) 2017      Gregor Santner <gsantner@mailbox.org>
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>

#include <garcon/garcon-environment.h>
#include <garcon/garcon-menu-element.h>
#include <garcon/garcon-menu-item.h>
#include <garcon/garcon-menu-item-action.h>
#include <garcon/garcon-private.h>



#ifndef G_KEY_FILE_DESKTOP_KEY_KEYWORDS
#define G_KEY_FILE_DESKTOP_KEY_KEYWORDS "Keywords"
#endif

#ifndef HAVE_REALPATH
#define realpath(path, resolved_path) NULL
#endif


/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
  PROP_DESKTOP_ID,
  PROP_REQUIRES_TERMINAL,
  PROP_NO_DISPLAY,
  PROP_STARTUP_NOTIFICATION,
  PROP_NAME,
  PROP_GENERIC_NAME,
  PROP_COMMENT,
  PROP_ICON_NAME,
  PROP_COMMAND,
  PROP_TRY_EXEC,
  PROP_HIDDEN,
  PROP_PREFERS_NON_DEFAULT_GPU,
  PROP_PATH,
};

/* Signal identifiers */
enum
{
  CHANGED,
  LAST_SIGNAL,
};



static void         garcon_menu_item_element_init                    (GarconMenuElementIface *iface);
static void         garcon_menu_item_finalize                        (GObject                *object);
static void         garcon_menu_item_get_property                    (GObject                *object,
                                                                      guint                   prop_id,
                                                                      GValue                 *value,
                                                                      GParamSpec             *pspec);
static void         garcon_menu_item_set_property                    (GObject                *object,
                                                                      guint                   prop_id,
                                                                      const GValue           *value,
                                                                      GParamSpec             *pspec);
static const gchar *garcon_menu_item_get_element_name                (GarconMenuElement      *element);
static const gchar *garcon_menu_item_get_element_comment             (GarconMenuElement      *element);
static const gchar *garcon_menu_item_get_element_icon_name           (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_visible             (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_show_in_environment (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_no_display          (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_equal               (GarconMenuElement      *element,
                                                                      GarconMenuElement      *other);
static gboolean     garcon_menu_item_lists_equal                     (GList                  *list1,
                                                                      GList                  *list2);



static guint item_signals[LAST_SIGNAL];



struct _GarconMenuItemPrivate
{
  /* Source file of the menu item */
  GFile      *file;

  /* Desktop file id */
  gchar      *desktop_id;

  /* List of categories */
  GList      *categories;

  /* List of keywords */
  GList      *keywords;

  /* Whether this application requires a terminal to be started in */
  guint       requires_terminal : 1;

  /* Whether this menu item should be hidden */
  guint       no_display : 1;

  /* Whether this application supports startup notification */
  guint       supports_startup_notification : 1;

  /* Name to be displayed for the menu item */
  gchar      *name;

  /* Generic name of the menu item */
  gchar      *generic_name;

  /* Comment/description of the item */
  gchar      *comment;

  /* Command to be executed when the menu item is clicked */
  gchar      *command;

  /* TryExec value */
  gchar      *try_exec;

  /* Menu item icon name */
  gchar      *icon_name;

  /* Environments in which the menu item should be displayed only */
  gchar     **only_show_in;

  /* Environments in which the menu item should be hidden */
  gchar     **not_show_in;

  /* Working directory */
  gchar      *path;

  /* List of application actions of type GarconMenuItemAction */
  GList      *actions;

  /* Hidden value */
  guint       hidden : 1;

  /* Whether prefers non-default GPU */
  guint       prefers_non_default_gpu : 1;

  /* Counter keeping the number of menus which use this item. This works
   * like a reference counter and should be increased / decreased by GarconMenu
   * items whenever the item is added to or removed from the menu. */
  guint       num_allocated;
};



G_DEFINE_TYPE_WITH_CODE (GarconMenuItem, garcon_menu_item, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GarconMenuItem)
                         G_IMPLEMENT_INTERFACE (GARCON_TYPE_MENU_ELEMENT,
                                                garcon_menu_item_element_init))



static void
garcon_menu_item_class_init (GarconMenuItemClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_item_finalize;
  gobject_class->get_property = garcon_menu_item_get_property;
  gobject_class->set_property = garcon_menu_item_set_property;

  /**
   * GarconMenuItem:file:
   *
   * The #GFile from which the %GarconMenuItem was loaded.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT_ONLY));

  /**
   * GarconMenuItem:desktop-id:
   *
   * The desktop-file id of this application.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DESKTOP_ID,
                                   g_param_spec_string ("desktop-id",
                                                        "Desktop-File Id",
                                                        "Desktop-File Id of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:requires-terminal:
   *
   * Whether this application requires a terinal to be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REQUIRES_TERMINAL,
                                   g_param_spec_boolean ("requires-terminal",
                                                         "Requires a terminal",
                                                         "Whether this application requires a terminal",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:no-display:
   *
   * Whether this menu item is hidden in menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NO_DISPLAY,
                                   g_param_spec_boolean ("no-display",
                                                         "No Display",
                                                         "Visibility state of the menu item",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:startup-notification:
   *
   * Whether this application supports startup notification.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_STARTUP_NOTIFICATION,
                                   g_param_spec_boolean ("supports-startup-notification",
                                                         "Startup notification",
                                                         "Startup notification support",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:name:
   *
   * Name of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:generic-name:
   *
   * GenericName of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_GENERIC_NAME,
                                   g_param_spec_string ("generic-name",
                                                        "Generic name",
                                                        "Generic name of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:comment:
   *
   * Comment/description for the application. To be displayed e.g. in tooltips of
   * GtkMenuItems.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMENT,
                                   g_param_spec_string ("comment",
                                                        "Comment",
                                                        "Comment/description for the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:command:
   *
   * Command to be executed when the menu item is clicked.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "Command",
                                                        "Application command",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:try-exec:
   *
   * Path to an executable file on disk used to determine if the program
   * is actually installed. If the path is not an absolute path, the file
   * is looked up in the $PATH environment variable. If the file is not
   * present or if it is not executable, the entry may be ignored (not be
   * used in menus, for example).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TRY_EXEC,
                                   g_param_spec_string ("try-exec",
                                                        "TryExec",
                                                        "Command to check if application is installed",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:icon-name:
   *
   * Name of the icon to be displayed for this menu item.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "Icon name",
                                                        "Name of the application icon",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

 /**
   * GarconMenuItem:hidden:
   *
   * It means the user deleted (at his level) something that was present
   * (at an upper level, e.g. in the system dirs). It's strictly equivalent
   * to the .desktop file not existing at all, as far as that user is concerned.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN,
                                   g_param_spec_boolean ("hidden",
                                                         "Hidden",
                                                         "Whether the application has been deleted",
                                                          FALSE,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

 /**
   * GarconMenuItem:prefers-non-default-gpu:
   *
   * If true, the application prefers to be run on a more powerful discrete GPU
   * if available.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PREFERS_NON_DEFAULT_GPU,
                                   g_param_spec_boolean ("prefers-non-default-gpu",
                                                         "Prefers non-default GPU",
                                                         "Whether the application prefers to be run on a discrete GPU",
                                                          FALSE,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

 /**
   * GarconMenuItem:path:
   *
   * Working directory the application should be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        "Path",
                                                        "Working directory path",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem::changed:
   * @item : a #GarconMenuItem.
   *
   * Emitted when #GarconMenuItem has been reloaded.
   **/
  item_signals[CHANGED] =
    g_signal_new (g_intern_static_string ("changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GarconMenuItemClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
garcon_menu_item_element_init (GarconMenuElementIface *iface)
{
  iface->get_name = garcon_menu_item_get_element_name;
  iface->get_comment = garcon_menu_item_get_element_comment;
  iface->get_icon_name = garcon_menu_item_get_element_icon_name;
  iface->get_visible = garcon_menu_item_get_element_visible;
  iface->get_show_in_environment = garcon_menu_item_get_element_show_in_environment;
  iface->get_no_display = garcon_menu_item_get_element_no_display;
  iface->equal = garcon_menu_item_get_element_equal;
}



static void
garcon_menu_item_init (GarconMenuItem *item)
{
  item->priv = garcon_menu_item_get_instance_private (item);
}



static void
garcon_menu_item_finalize (GObject *object)
{
  GarconMenuItem *item = GARCON_MENU_ITEM (object);

  g_free (item->priv->desktop_id);
  g_free (item->priv->name);
  g_free (item->priv->generic_name);
  g_free (item->priv->comment);
  g_free (item->priv->command);
  g_free (item->priv->try_exec);
  g_free (item->priv->icon_name);
  g_free (item->priv->path);

  g_strfreev (item->priv->only_show_in);
  g_strfreev (item->priv->not_show_in);

  _garcon_g_list_free_full (item->priv->categories, g_free);
  _garcon_g_list_free_full (item->priv->keywords, g_free);
  _garcon_g_list_free_full (item->priv->actions, garcon_menu_item_action_unref);

  if (item->priv->file != NULL)
    g_object_unref (G_OBJECT (item->priv->file));

  (*G_OBJECT_CLASS (garcon_menu_item_parent_class)->finalize) (object);
}



static void
garcon_menu_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GarconMenuItem *item = GARCON_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, item->priv->file);
      break;

    case PROP_DESKTOP_ID:
      g_value_set_string (value, garcon_menu_item_get_desktop_id (item));
      break;

    case PROP_COMMENT:
      g_value_set_string (value, garcon_menu_item_get_comment (item));
      break;

    case PROP_REQUIRES_TERMINAL:
      g_value_set_boolean (value, garcon_menu_item_requires_terminal (item));
      break;

    case PROP_NO_DISPLAY:
      g_value_set_boolean (value, garcon_menu_item_get_no_display (item));
      break;

    case PROP_STARTUP_NOTIFICATION:
      g_value_set_boolean (value, garcon_menu_item_supports_startup_notification (item));
      break;

    case PROP_NAME:
      g_value_set_string (value, garcon_menu_item_get_name (item));
      break;

    case PROP_GENERIC_NAME:
      g_value_set_string (value, garcon_menu_item_get_generic_name (item));
      break;

    case PROP_COMMAND:
      g_value_set_string (value, garcon_menu_item_get_command (item));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, garcon_menu_item_get_icon_name (item));
      break;

    case PROP_TRY_EXEC:
      g_value_set_string (value, garcon_menu_item_get_try_exec (item));
      break;

    case PROP_HIDDEN:
      g_value_set_boolean (value, garcon_menu_item_get_hidden (item));
      break;

    case PROP_PREFERS_NON_DEFAULT_GPU:
      g_value_set_boolean (value, garcon_menu_item_get_prefers_non_default_gpu (item));
      break;

    case PROP_PATH:
      g_value_set_string (value, garcon_menu_item_get_path (item));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GarconMenuItem *item = GARCON_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      item->priv->file = g_value_dup_object (value);
      break;

    case PROP_DESKTOP_ID:
      garcon_menu_item_set_desktop_id (item, g_value_get_string (value));
      break;

    case PROP_REQUIRES_TERMINAL:
      garcon_menu_item_set_requires_terminal (item, g_value_get_boolean (value));
      break;

    case PROP_NO_DISPLAY:
      garcon_menu_item_set_no_display (item, g_value_get_boolean (value));
      break;

    case PROP_STARTUP_NOTIFICATION:
      garcon_menu_item_set_supports_startup_notification (item, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      garcon_menu_item_set_name (item, g_value_get_string (value));
      break;

    case PROP_GENERIC_NAME:
      garcon_menu_item_set_generic_name (item, g_value_get_string (value));
      break;

    case PROP_COMMENT:
      garcon_menu_item_set_comment (item, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      garcon_menu_item_set_command (item, g_value_get_string (value));
      break;

    case PROP_TRY_EXEC:
      garcon_menu_item_set_try_exec (item, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      garcon_menu_item_set_icon_name (item, g_value_get_string (value));
      break;

    case PROP_HIDDEN:
      garcon_menu_item_set_hidden (item, g_value_get_boolean (value));
      break;

    case PROP_PREFERS_NON_DEFAULT_GPU:
      garcon_menu_item_set_prefers_non_default_gpu (item, g_value_get_boolean (value));
      break;

    case PROP_PATH:
      garcon_menu_item_set_path (item, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
garcon_menu_item_get_element_visible (GarconMenuElement *element)
{
  GarconMenuItem  *item;
  const gchar     *try_exec;
  gchar          **mt;
  gboolean         result = TRUE;
  gchar           *command;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);

  item = GARCON_MENU_ITEM (element);

  if (garcon_menu_item_get_hidden (item)
      || garcon_menu_item_get_no_display (item)
      || !garcon_menu_item_get_show_in_environment (item))
    return FALSE;

  /* Check the TryExec field */
  try_exec = garcon_menu_item_get_try_exec (item);
  if (try_exec != NULL && g_shell_parse_argv (try_exec, NULL, &mt, NULL))
    {
      /* Check if we have an absolute path to an existing file */
      result = g_file_test (mt[0], G_FILE_TEST_EXISTS);

      /* Else, we may have a program in $PATH */
      if (!result)
        {
          command = g_find_program_in_path (mt[0]);
          result = (command != NULL);
          g_free (command);
        }

      /* Cleanup */
      g_strfreev (mt);
    }

  return result;
}



static gboolean
garcon_menu_item_get_element_show_in_environment (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);
  return garcon_menu_item_get_show_in_environment (GARCON_MENU_ITEM (element));
}



static gboolean
garcon_menu_item_get_element_no_display (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);
  return garcon_menu_item_get_no_display (GARCON_MENU_ITEM (element));
}



static gboolean
garcon_menu_item_get_element_equal (GarconMenuElement *element,
                                    GarconMenuElement *other)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (other), FALSE);

  return g_file_equal (GARCON_MENU_ITEM (element)->priv->file,
                       GARCON_MENU_ITEM (other)->priv->file);
}



static const gchar*
garcon_menu_item_get_element_name (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), NULL);
  return GARCON_MENU_ITEM (element)->priv->name;
}



static const gchar*
garcon_menu_item_get_element_comment (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), NULL);
  return GARCON_MENU_ITEM (element)->priv->comment;
}



static const gchar*
garcon_menu_item_get_element_icon_name (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), NULL);
  return GARCON_MENU_ITEM (element)->priv->icon_name;
}



static gboolean
garcon_menu_item_lists_equal (GList *list1,
                              GList *list2)
{
  gboolean  element_missing = FALSE;
  GList    *lp;

  if (g_list_length (list1) != g_list_length (list2))
    return FALSE;

  for (lp = list1; !element_missing && lp != NULL; lp = lp->next)
    {
      if (g_list_find_custom (list2, lp->data, (GCompareFunc) g_strcmp0) == NULL)
        element_missing = TRUE;
    }

  return !element_missing;
}



static gchar *
garcon_menu_item_url_exec (XfceRc *rc)
{
  GString     *string;
  const gchar *url;
  gchar       *url_exec = NULL;

  /* Support Type=Link items */
  url = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_URL, NULL);
  if (url != NULL)
    {
#if GLIB_CHECK_VERSION (2, 68, 0)
      string = g_string_new (url);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_string_replace (string, "%", "%%", 0);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
      string = g_string_sized_new (2 * strlen (url));
      for (const gchar *p = url; *p != '\0'; p++)
        if (*p == '%')
          string = g_string_append (string, "%%");
        else
          string = g_string_append_c (string, *p);
      string = g_string_append_c (string, '\0');
#endif
      url_exec = g_strdup_printf ("exo-open '%s'", string->str);
      g_string_free (string, TRUE);
    }

  return url_exec;
}


/**
 * garcon_menu_item_new: (constructor)
 * @file: a #GFile
 *
 * Returns (transfer full): a new #GarconMenuItem
 */
GarconMenuItem *
garcon_menu_item_new (GFile *file)
{
  GarconMenuItem       *item = NULL;
  GarconMenuItemAction *action = NULL;
  XfceRc               *rc;
  GList                *categories = NULL;
  GList                *keywords = NULL;
  gchar                *filename;
  gboolean              terminal;
  gboolean              no_display;
  gboolean              startup_notify;
  gboolean              hidden;
  gboolean              prefers_non_default_gpu;
  const gchar          *path;
  const gchar          *name;
  const gchar          *generic_name;
  const gchar          *comment;
  const gchar          *exec;
  const gchar          *try_exec;
  const gchar          *icon;
  gchar                *action_group;
  gchar               **mt;
  gchar               **str_list;
  gchar                *url_exec = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (g_file_is_native (file), NULL);

  /* Use absolute paths for symlinks. See bug #1 */
  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) == G_FILE_TYPE_SYMBOLIC_LINK
      && (filename = realpath (g_file_peek_path (file), NULL)) != NULL)
    {
      file = g_file_new_for_path (filename);
    }
  else
    {
      filename = g_file_get_path (file);
      g_object_ref (file);
    }

  /* Open the rc file */
  rc = xfce_rc_simple_open (filename, TRUE);
  g_free (filename);
  if (G_UNLIKELY (rc == NULL))
    {
      g_object_unref (file);
      return NULL;
    }

  xfce_rc_set_group (rc, G_KEY_FILE_DESKTOP_GROUP);

  /* Parse name and exec command */
  name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
  exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);

  /* Support Type=Link items */
  if (G_UNLIKELY (exec == NULL))
    exec = url_exec = garcon_menu_item_url_exec (rc);

  /* Validate Name and Exec fields */
  if (G_LIKELY (exec != NULL && name != NULL))
    {
      /* Determine other application properties */
      generic_name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME, NULL);
      comment = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL);
      try_exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL);
      icon = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
      path = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_PATH, NULL);
      terminal = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_TERMINAL, FALSE);
      no_display = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, FALSE);
      startup_notify = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, FALSE)
                       || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE);
      hidden = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_HIDDEN, FALSE);
      prefers_non_default_gpu = xfce_rc_read_bool_entry (rc, "PrefersNonDefaultGPU", FALSE);

      /* Allocate a new menu item instance */
      item = g_object_new (GARCON_TYPE_MENU_ITEM,
                           "file", file,
                           "command", exec,
                           "try-exec", try_exec,
                           "name", name,
                           "generic-name", generic_name,
                           "comment", comment,
                           "icon-name", icon,
                           "requires-terminal", terminal,
                           "no-display", no_display,
                           "supports-startup-notification", startup_notify,
                           "path", path,
                           "hidden", hidden,
                           "prefers-non-default-gpu", prefers_non_default_gpu,
                           NULL);

      /* Determine the categories this application should be shown in */
      str_list = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_CATEGORIES, ";");
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              /* Try to steal the values */
              if (**mt != '\0')
                categories = g_list_prepend (categories, *mt);
              else
                g_free (*mt);
            }

          /* Cleanup */
          g_free (str_list);

          /* Assign categories list to the menu item */
          garcon_menu_item_set_categories (item, categories);
        }

      /* Determine the keywords this application should be shown in */
      str_list = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_KEYWORDS, ";");
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              /* Try to steal the values */
              if (**mt != '\0')
                keywords = g_list_prepend (keywords, *mt);
              else
                g_free (*mt);
            }

          /* Cleanup */
          g_free (str_list);

          /* Assign keywords list to the menu item */
          garcon_menu_item_set_keywords (item, keywords);
        }

      /* Set the rest of the private data directly */
      item->priv->only_show_in = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, ";");
      item->priv->not_show_in = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, ";");

      /* Determine this application actions */
      str_list = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_ACTIONS, ";");
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              if (**mt != '\0')
                {
                  /* Set current desktop action group */
                  action_group = g_strdup_printf ("Desktop Action %s", *mt);
                  xfce_rc_set_group (rc, action_group);

                  /* Parse name and exec command */
                  name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
                  exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
                  icon = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);

                  /* Validate Name and Exec fields, icon is optional */
                  if (G_LIKELY (exec != NULL && name != NULL))
                    {
                      /* Allocate a new action instance */
                      action = g_object_new (GARCON_TYPE_MENU_ITEM_ACTION,
                                             "name", name,
                                             "command", exec,
                                             "icon-name", icon,
                                             NULL);

                      garcon_menu_item_set_action (item, *mt, action);
                      garcon_menu_item_action_unref (action);
                    }

                  g_free (action_group);
                }
            }

          /* Cleanup */
          g_strfreev (str_list);
        }

      else
        {
          str_list = xfce_rc_read_list_entry (rc, "X-Ayatana-Desktop-Shortcuts", ";");
          if (G_LIKELY (str_list != NULL))
            {
              for (mt = str_list; *mt != NULL; ++mt)
                {
                  if (**mt != '\0')
                    {
                      action_group = g_strdup_printf ("%s Shortcut Group", *mt);
                      xfce_rc_set_group (rc, action_group);

                      name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
                      exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
                      icon = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);

                      /* Validate Name and Exec fields, icon is optional */
                      if (G_LIKELY (exec != NULL && name != NULL))
                        {
                          action = g_object_new (GARCON_TYPE_MENU_ITEM_ACTION,
                                                 "name", name,
                                                 "command", exec,
                                                 "icon-name", icon,
                                                 NULL);

                          garcon_menu_item_set_action (item, *mt, action);
                          garcon_menu_item_action_unref (action);
                        }

                      g_free (action_group);
                    }
                }

              g_strfreev (str_list);
            }
        }
    }

  /* Cleanup */
  xfce_rc_close (rc);
  g_free (url_exec);
  g_object_unref (file);

  return item;
}


/**
 * garcon_menu_item_new_for_path: (constructor)
 * @filename: (type filename): name of a file
 *
 * Returns: (transfer full): a new #GarconMenuItem
 */
GarconMenuItem *
garcon_menu_item_new_for_path (const gchar *filename)
{
  GFile          *file;
  GarconMenuItem *item;

  g_return_val_if_fail (filename != NULL, NULL);

  file = g_file_new_for_path (filename);
  item = garcon_menu_item_new (file);
  g_object_unref (G_OBJECT (file));

  return item;
}


/**
 * garcon_menu_item_new_for_uri: (constructor)
 * @uri: a given URI
 *
 * Returns: (transfer full): a new #GarconMenuItem
 */
GarconMenuItem *
garcon_menu_item_new_for_uri (const gchar *uri)
{
  GFile          *file;
  GarconMenuItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  file = g_file_new_for_uri (uri);
  item = garcon_menu_item_new (file);
  g_object_unref (G_OBJECT (file));

  return item;
}



gboolean
garcon_menu_item_reload (GarconMenuItem  *item,
                         gboolean        *affects_the_outside,
                         GError         **error)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return garcon_menu_item_reload_from_file (item, item->priv->file, affects_the_outside, error);
}



gboolean
garcon_menu_item_reload_from_file (GarconMenuItem  *item,
                                   GFile           *file,
                                   gboolean        *affects_the_outside,
                                   GError         **error)
{
  XfceRc               *rc;
  GarconMenuItemAction *action = NULL;
  gboolean              boolean;
  GList                *categories = NULL;
  GList                *old_categories = NULL;
  GList                *keywords = NULL;
  GList                *old_keywords = NULL;
  GList                *lp;
  gchar               **mt;
  gchar               **str_list;
  const gchar          *string;
  const gchar          *name;
  const gchar          *exec;
  const gchar          *icon;
  gchar                *filename;
  gchar                *action_group;
  gchar                *url_exec = NULL;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_file_is_native (file), FALSE);

  /* Open the rc file */
  filename = g_file_get_path (file);
  rc = xfce_rc_simple_open (filename, TRUE);
  g_free (filename);
  if (G_UNLIKELY (rc == NULL))
    return FALSE;

  xfce_rc_set_group (rc, G_KEY_FILE_DESKTOP_GROUP);

  /* Check if there is a name and exec key */
  name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
  exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);

  /* Support Type=Link items */
  if (G_UNLIKELY (exec == NULL))
    exec = url_exec = garcon_menu_item_url_exec (rc);

  if (G_UNLIKELY (name == NULL || exec == NULL))
    {
      g_set_error_literal (error, G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_KEY_NOT_FOUND,
                           "Either the name or exec key was not defined.");
      xfce_rc_close (rc);

      return FALSE;
    }

  /* Queue property notifications */
  g_object_freeze_notify (G_OBJECT (item));

  /* Set the new file if needed */
  if (!g_file_equal (file, item->priv->file))
    {
      if (G_LIKELY (item->priv->file != NULL))
        g_object_unref (G_OBJECT (item->priv->file));
      item->priv->file = G_FILE (g_object_ref (G_OBJECT (file)));

      g_object_notify (G_OBJECT (item), "file");
    }

  /* Update properties */
  garcon_menu_item_set_name (item, name);

  garcon_menu_item_set_command (item, exec);

  string = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME, NULL);
  garcon_menu_item_set_generic_name (item, string);

  string = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL);
  garcon_menu_item_set_comment (item, string);

  string = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL);
  garcon_menu_item_set_try_exec (item, string);

  string = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
  garcon_menu_item_set_icon_name (item, string);

  string = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_PATH, NULL);
  garcon_menu_item_set_path (item, string);

  boolean = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_TERMINAL, FALSE);
  garcon_menu_item_set_requires_terminal (item, boolean);

  boolean = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, FALSE);
  garcon_menu_item_set_no_display (item, boolean);

  boolean = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, FALSE)
            || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE);
  garcon_menu_item_set_supports_startup_notification (item, boolean);

  boolean = xfce_rc_read_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_HIDDEN, FALSE);
  garcon_menu_item_set_hidden (item, boolean);

  boolean = xfce_rc_read_bool_entry (rc, "PrefersNonDefaultGPU", FALSE);
  garcon_menu_item_set_prefers_non_default_gpu (item, boolean);

  if (affects_the_outside != NULL)
    {
      /* create a deep copy the old categories list */
      old_categories = g_list_copy (item->priv->categories);
      for (lp = old_categories; lp != NULL; lp = lp->next)
        lp->data = g_strdup (lp->data);
    }

  /* Determine the categories this application should be shown in */
  str_list = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_CATEGORIES, ";");
  if (G_LIKELY (str_list != NULL))
    {
      for (mt = str_list; *mt != NULL; ++mt)
        {
          /* Try to steal the values */
          if (**mt != '\0')
            categories = g_list_prepend (categories, *mt);
          else
            g_free (*mt);
        }

      /* Cleanup */
      g_free (str_list);

      /* Assign categories list to the menu item */
      garcon_menu_item_set_categories (item, categories);
    }
  else
    {
      /* Assign empty categories list to the menu item */
      garcon_menu_item_set_categories (item, NULL);
    }

  if (affects_the_outside != NULL)
    {
      if (!garcon_menu_item_lists_equal (old_categories, categories))
        *affects_the_outside = TRUE;

      _garcon_g_list_free_full (old_categories, g_free);
    }


  if (affects_the_outside != NULL)
    {
      /* create a deep copy the old keywords list */
      old_keywords = g_list_copy (item->priv->keywords);
      for (lp = old_keywords; lp != NULL; lp = lp->next)
        lp->data = g_strdup (lp->data);
    }

  /* Determine the keywords this application should be shown in */
  str_list = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_KEYWORDS, ";");
  if (G_LIKELY (str_list != NULL))
    {
      for (mt = str_list; *mt != NULL; ++mt)
        {
          /* Try to steal the values */
          if (**mt != '\0')
            keywords = g_list_prepend (keywords, *mt);
          else
            g_free (*mt);
        }

      /* Cleanup */
      g_free (str_list);

      /* Assign keywords list to the menu item */
      garcon_menu_item_set_keywords (item, keywords);
    }
  else
    {
      /* Assign empty keywords list to the menu item */
      garcon_menu_item_set_keywords (item, NULL);
    }

  if (affects_the_outside != NULL)
    {
      if (!garcon_menu_item_lists_equal (old_keywords, keywords))
        *affects_the_outside = TRUE;

      _garcon_g_list_free_full (old_keywords, g_free);
    }

  /* Set the rest of the private data directly */
  g_strfreev (item->priv->only_show_in);
  g_strfreev (item->priv->not_show_in);
  item->priv->only_show_in = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, ";");
  item->priv->not_show_in = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, ";");

  /* Update application actions */
  _garcon_g_list_free_full (item->priv->actions, garcon_menu_item_action_unref);
  item->priv->actions = NULL;

  str_list = xfce_rc_read_list_entry (rc, G_KEY_FILE_DESKTOP_KEY_ACTIONS, ";");
  if (G_LIKELY (str_list != NULL))
    {
      for (mt = str_list; *mt != NULL; ++mt)
        {
          if (**mt != '\0')
            {
              /* Set current desktop action group */
              action_group = g_strdup_printf ("Desktop Action %s", *mt);
              xfce_rc_set_group (rc, action_group);

              /* Parse name and exec command */
              name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
              exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
              icon = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);

              /* Validate Name and Exec fields, icon is optional */
              if (G_LIKELY (exec != NULL && name != NULL))
                {
                  /* Allocate a new action instance */
                  action = g_object_new (GARCON_TYPE_MENU_ITEM_ACTION,
                                         "name", name,
                                         "command", exec,
                                         "icon-name", icon,
                                         NULL);

                  garcon_menu_item_set_action (item, *mt, action);
                  garcon_menu_item_action_unref (action);
                }
              g_free (action_group);
            }
          else
            g_free (*mt);
        }

      /* Cleanup */
      g_free (str_list);
    }

  else
    {
      str_list = xfce_rc_read_list_entry (rc, "X-Ayatana-Desktop-Shortcuts", ";");
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              if (**mt != '\0')
                {
                  action_group = g_strdup_printf ("%s Shortcut Group", *mt);
                  xfce_rc_set_group (rc, action_group);

                  name = xfce_rc_read_entry (rc, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
                  exec = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
                  icon = xfce_rc_read_entry_untranslated (rc, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);

                  /* Validate Name and Exec fields, icon is optional */
                  if (G_LIKELY (exec != NULL && name != NULL))
                    {
                      action = g_object_new (GARCON_TYPE_MENU_ITEM_ACTION,
                                             "name", name,
                                             "command", exec,
                                             "icon-name", icon,
                                             NULL);

                      garcon_menu_item_set_action (item, *mt, action);
                      garcon_menu_item_action_unref (action);
                    }

                  g_free (action_group);
                }
              else
                g_free (*mt);
            }

          g_free (str_list);
        }
    }

  /* Flush property notifications */
  g_object_thaw_notify (G_OBJECT (item));

  /* Emit signal to everybody knows we reloaded the file */
  g_signal_emit (G_OBJECT (item), item_signals[CHANGED], 0);

  xfce_rc_close (rc);
  g_free (url_exec);

  return TRUE;
}



/**
 * garcon_menu_item_get_file:
 * @item: A #GarconMenuItem
 *
 * Get the #GFile for @item. The returned object should be
 * unreffed with g_object_unref() when no longer needed.
 *
 * Returns: (transfer full): a #GFile.
 */
GFile *
garcon_menu_item_get_file (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return g_object_ref (item->priv->file);
}


gchar *
garcon_menu_item_get_uri (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return g_file_get_uri (item->priv->file);
}



const gchar *
garcon_menu_item_get_desktop_id (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->desktop_id;
}



void
garcon_menu_item_set_desktop_id (GarconMenuItem *item,
                                 const gchar    *desktop_id)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (desktop_id != NULL);

  /* Abort if old and new desktop_id are equal */
  if (g_strcmp0 (item->priv->desktop_id, desktop_id) == 0)
    return;

  /* Assign the new desktop_id */
  g_free (item->priv->desktop_id);
  item->priv->desktop_id = g_strdup (desktop_id);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "desktop-id");
}


/**
 * garcon_menu_item_get_categories:
 * @item: a #GarconMenuItem
 *
 * Returns list of categories
 *
 * Returns: (element-type utf8) (transfer none):
 */
GList*
garcon_menu_item_get_categories (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->categories;
}


/**
 * garcon_menu_item_set_categories:
 * @item: a #GarconMenuItem
 * @categories: (element-type utf8): list of categories
 */
void
garcon_menu_item_set_categories (GarconMenuItem *item,
                                 GList          *categories)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if lists are equal */
  if (G_UNLIKELY (item->priv->categories == categories))
    return;

  /* Free old list */
  _garcon_g_list_free_full (item->priv->categories, g_free);

  /* Assign new list */
  item->priv->categories = categories;
}


/**
 * garcon_menu_item_get_keywords:
 * @item: a #GarconMenuItem
 *
 * Returns: (element-type utf8) (transfer none):
 */
GList*
garcon_menu_item_get_keywords (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->keywords;
}


/**
 * garcon_menu_item_set_keywords:
 * @item: a #GarconMenuItem
 * @keywords: (element-type utf8): list of keywords
 */
void
garcon_menu_item_set_keywords (GarconMenuItem *item,
                               GList          *keywords)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if lists are equal */
  if (G_UNLIKELY (item->priv->keywords == keywords))
    return;

  /* Free old list */
  _garcon_g_list_free_full (item->priv->keywords, g_free);

  /* Assign new list */
  item->priv->keywords = keywords;
}


const gchar*
garcon_menu_item_get_command (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->command;
}


void
garcon_menu_item_set_command (GarconMenuItem *item,
                              const gchar    *command)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (command != NULL);

  /* Abort if old and new command are equal */
  if (g_strcmp0 (item->priv->command, command) == 0)
    return;

  /* Assign new command */
  g_free (item->priv->command);
  item->priv->command = g_strdup (command);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "command");
}



const gchar*
garcon_menu_item_get_try_exec (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->try_exec;
}



void
garcon_menu_item_set_try_exec (GarconMenuItem *item,
                               const gchar    *try_exec)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new try_exec are equal */
  if (g_strcmp0 (item->priv->try_exec, try_exec) == 0)
    return;

  /* Assign new try_exec */
  g_free (item->priv->try_exec);
  item->priv->try_exec = g_strdup (try_exec);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "try-exec");
}



const gchar*
garcon_menu_item_get_name (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->name;
}



void
garcon_menu_item_set_name (GarconMenuItem *item,
                           const gchar    *name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (g_utf8_validate (name, -1, NULL));

  /* Abort if old and new name are equal */
  if (g_strcmp0 (item->priv->name, name) == 0)
    return;

  /* Assign new name */
  g_free (item->priv->name);
  item->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "name");
}



const gchar*
garcon_menu_item_get_generic_name (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->generic_name;
}



void
garcon_menu_item_set_generic_name (GarconMenuItem *item,
                                   const gchar    *generic_name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (generic_name == NULL || g_utf8_validate (generic_name, -1, NULL));

  /* Abort if old and new generic name are equal */
  if (g_strcmp0 (item->priv->generic_name, generic_name) == 0)
    return;

  /* Assign new generic_name */
  g_free (item->priv->generic_name);
  item->priv->generic_name = g_strdup (generic_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "generic-name");
}



const gchar*
garcon_menu_item_get_comment (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->comment;
}



void
garcon_menu_item_set_comment (GarconMenuItem *item,
                              const gchar    *comment)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (comment == NULL || g_utf8_validate (comment, -1, NULL));

  /* Abort if old and new comment are equal */
  if (g_strcmp0 (item->priv->comment, comment) == 0)
    return;

  /* Assign new comment */
  g_free (item->priv->comment);
  item->priv->comment = g_strdup (comment);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "comment");
}



const gchar*
garcon_menu_item_get_icon_name (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->icon_name;
}



void
garcon_menu_item_set_icon_name (GarconMenuItem *item,
                                const gchar    *icon_name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new icon name are equal */
  if (g_strcmp0 (item->priv->icon_name, icon_name) == 0)
    return;

  /* Assign new icon name */
  g_free (item->priv->icon_name);
  item->priv->icon_name = g_strdup (icon_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "icon-name");
}



const gchar*
garcon_menu_item_get_path (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->path;
}



void
garcon_menu_item_set_path (GarconMenuItem *item,
                           const gchar    *path)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new path are equal */
  if (g_strcmp0 (item->priv->path, path) == 0)
    return;

  /* Assign new path */
  g_free (item->priv->path);
  item->priv->path = g_strdup (path);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "path");
}



gboolean
garcon_menu_item_get_hidden (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), TRUE);
  return item->priv->hidden;
}



void
garcon_menu_item_set_hidden (GarconMenuItem *item,
                             gboolean        hidden)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->hidden == hidden)
    return;

  /* Assign new value */
  item->priv->hidden = !!hidden;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "hidden");
}



gboolean
garcon_menu_item_get_prefers_non_default_gpu (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), TRUE);
  return item->priv->prefers_non_default_gpu;
}



void
garcon_menu_item_set_prefers_non_default_gpu (GarconMenuItem *item,
                                              gboolean        prefers_non_default_gpu)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->prefers_non_default_gpu == prefers_non_default_gpu)
    return;

  /* Assign new value */
  item->priv->prefers_non_default_gpu = !!prefers_non_default_gpu;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "prefers-non-default-gpu");
}



gboolean
garcon_menu_item_requires_terminal (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->requires_terminal;
}



void
garcon_menu_item_set_requires_terminal (GarconMenuItem *item,
                                        gboolean        requires_terminal)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->requires_terminal == requires_terminal)
    return;

  /* Assign new value */
  item->priv->requires_terminal = !!requires_terminal;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "requires-terminal");
}



gboolean
garcon_menu_item_get_no_display (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->no_display;
}



void
garcon_menu_item_set_no_display (GarconMenuItem *item,
                                 gboolean        no_display)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->no_display == no_display)
    return;

  /* Assign new value */
  item->priv->no_display = !!no_display;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "no-display");
}



gboolean
garcon_menu_item_supports_startup_notification (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->supports_startup_notification;
}



void
garcon_menu_item_set_supports_startup_notification (GarconMenuItem *item,
                                                    gboolean        supports_startup_notification)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->supports_startup_notification == supports_startup_notification)
    return;

  /* Assign new value */
  item->priv->supports_startup_notification = !!supports_startup_notification;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "supports-startup-notification");
}



gboolean
garcon_menu_item_has_category (GarconMenuItem *item,
                               const gchar    *category)
{
  GList   *iter;
  gboolean found = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (category != NULL, FALSE);

  for (iter = item->priv->categories; !found && iter != NULL; iter = g_list_next (iter))
    if (g_strcmp0 (iter->data, category) == 0)
      found = TRUE;

  return found;
}



gboolean
garcon_menu_item_has_keyword (GarconMenuItem *item,
                              const gchar    *keyword)
{
  GList   *iter;
  gboolean found = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (keyword != NULL, FALSE);

  for (iter = item->priv->keywords; !found && iter != NULL; iter = g_list_next (iter))
    if (g_strcmp0 (iter->data, keyword) == 0)
      found = TRUE;

  return found;
}


/**
 * garcon_menu_item_get_actions:
 * @item: a #GarconMenuItem
 *
 * Returns: (element-type GarconMenuItemAction) (transfer container):
 */
GList *
garcon_menu_item_get_actions (GarconMenuItem *item)
{
  GList                *action_names = NULL;
  GList                *iter;
  GarconMenuItemAction *action;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);

  for (iter = item->priv->actions; iter != NULL ; iter = g_list_next (iter))
    {
      action = GARCON_MENU_ITEM_ACTION (iter->data);
      action_names = g_list_prepend (action_names, (gchar*)garcon_menu_item_action_get_name (action));
    }
  action_names = g_list_reverse (action_names);

  return action_names;
}


/**
 * garcon_menu_item_get_action:
 * @item: a #GarconMenuItem
 * @action_name:
 *
 * Returns: (nullable) (transfer none): a #GarconMenuItemAction
 */
GarconMenuItemAction *
garcon_menu_item_get_action (GarconMenuItem *item,
                             const gchar    *action_name)
{
  GList                *iter;
  GarconMenuItemAction *action;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  g_return_val_if_fail (action_name != NULL, NULL);

  for (iter = item->priv->actions; iter != NULL; iter = g_list_next (iter))
    {
      action = GARCON_MENU_ITEM_ACTION (iter->data);
      if (g_strcmp0 (garcon_menu_item_action_get_name (action), action_name) == 0)
        return (action);
    }

  return NULL;
}




void
garcon_menu_item_set_action (GarconMenuItem       *item,
                             const gchar          *action_name,
                             GarconMenuItemAction *action)
{
  GList                *iter;
  GarconMenuItemAction *old_action;
  gboolean             found = FALSE;

  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (GARCON_IS_MENU_ITEM_ACTION (action));

  /* If action name is found in list, then insert new action into the list and
   * remove old action */
  for (iter = item->priv->actions; !found && iter != NULL; iter = g_list_next (iter))
    {
      old_action = GARCON_MENU_ITEM_ACTION (iter->data);
      if (g_strcmp0 (garcon_menu_item_action_get_name (old_action), action_name) == 0)
        {
           /* Release reference on action currently stored at action name */
           garcon_menu_item_action_unref (old_action);

           /* Replace action in list at action name and grab a reference */
           iter->data = action;
           garcon_menu_item_action_ref (action);

           /* Set flag that action was found */
           found = TRUE;
        }
    }

  /* If action name was not found in list, then simply add it to list */
  if (found == FALSE)
    {
      /* Add action to list and grab a reference */
      item->priv->actions=g_list_append (item->priv->actions, action);
      garcon_menu_item_action_ref (action);
    }
}



gboolean
garcon_menu_item_has_action (GarconMenuItem  *item,
                             const gchar     *action_name)
{
  GList                *iter;
  GarconMenuItemAction *action;
  gboolean             found = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (action_name != NULL, FALSE);

  for (iter = item->priv->actions; !found && iter != NULL; iter = g_list_next (iter))
    {
      action = GARCON_MENU_ITEM_ACTION (iter->data);
      if (g_strcmp0 (garcon_menu_item_action_get_name (action), action_name) == 0)
        found = TRUE;
    }

  return found;
}



gboolean
garcon_menu_item_get_show_in_environment (GarconMenuItem *item)
{
  const gchar *env;
  guint        i, j;
  gboolean     show = TRUE;
  gchar**      path = NULL;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = garcon_get_environment ();

  /* If no environment has been set, the menu is displayed no matter what
   * OnlyShowIn or NotShowIn contain */
  if (G_UNLIKELY (env == NULL))
    return TRUE;

  /* According to the spec there is either a OnlyShowIn or a NotShowIn list
   * The environment can be multiple Desktop Names separated by a colons */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      /* Check if your environemnt is in OnlyShowIn list */
      show = FALSE;
      path = g_strsplit(env, ":", 0);
      for (j = 0; path[j] != NULL; j++)
        for (i = 0; !show && item->priv->only_show_in[i] != NULL; i++)
          if (g_strcmp0 (item->priv->only_show_in[i], path[j]) == 0)
            show = TRUE;
      g_strfreev(path);
    }
  else if (G_UNLIKELY (item->priv->not_show_in != NULL))
    {
      /* Check if your environemnt is in NotShowIn list */
      show = TRUE;
      path = g_strsplit(env, ":", 0);
      for (j = 0; path[j] != NULL; j++)
        for (i = 0; show && item->priv->not_show_in[i] != NULL; i++)
          if (g_strcmp0 (item->priv->not_show_in[i], path[j]) == 0)
            show = FALSE;
      g_strfreev(path);
    }

  return show;
}



gboolean
garcon_menu_item_only_show_in_environment (GarconMenuItem *item)
{
  const gchar *env;
  guint        i, j;
  gboolean     show = FALSE;
  gchar**      path = NULL;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = garcon_get_environment ();

  /* If no environment has been set, the contents of OnlyShowIn don't matter */
  if (G_LIKELY (env == NULL))
    return FALSE;

  /* Check if we have an OnlyShowIn list */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      /* Check if your environemnt is in OnlyShowIn list */
      show = FALSE;
      path = g_strsplit(env, ":", 0);
      for (j= 0; path[j] != NULL; j++)
        for (i = 0; !show && item->priv->only_show_in[i] != NULL; i++)
          if (g_strcmp0 (item->priv->only_show_in[i], path[j]) == 0)
            show = TRUE;
    }

  return show;
}



void
garcon_menu_item_ref (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Increment the allocation counter */
  garcon_menu_item_increment_allocated (item);

  /* Grab a reference on the object */
  g_object_ref (G_OBJECT (item));
}



void
garcon_menu_item_unref (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  garcon_menu_item_decrement_allocated (item);

  /* Decrement the reference counter */
  g_object_unref (G_OBJECT (item));
}



gint
garcon_menu_item_get_allocated (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->num_allocated;
}



void
garcon_menu_item_increment_allocated (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  item->priv->num_allocated++;
}



void
garcon_menu_item_decrement_allocated (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  if (item->priv->num_allocated > 0)
    item->priv->num_allocated--;
}
