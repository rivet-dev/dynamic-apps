/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings-store.h>



#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
/* Needed to use keyfile GSettings backend */
# define G_SETTINGS_ENABLE_BACKEND
# include <gio/gsettingsbackend.h>
#endif



struct _MousepadSettingsStore
{
  GObject parent;

  GSettingsBackend *backend;
  GList            *roots;
  GHashTable       *keys;
};



typedef struct
{
  const gchar *name;
  GSettings   *settings;
}
MousepadSettingKey;



static void mousepad_settings_store_finalize (GObject *object);



G_DEFINE_TYPE (MousepadSettingsStore, mousepad_settings_store, G_TYPE_OBJECT)



static MousepadSettingKey *
mousepad_setting_key_new (const gchar *key_name,
                          GSettings   *settings)
{
  MousepadSettingKey *key;

  key = g_slice_new0 (MousepadSettingKey);
  key->name = g_intern_string (key_name);
  key->settings = g_object_ref (settings);

  return key;
}



static void
mousepad_setting_key_free (gpointer data)
{
  MousepadSettingKey *key = data;

  if (G_LIKELY (key != NULL))
    {
      g_object_unref (key->settings);
      g_slice_free (MousepadSettingKey, key);
    }
}



static void
mousepad_settings_store_update_env (void)
{
  const gchar *old_value = g_getenv ("GSETTINGS_SCHEMA_DIR");
  gchar       *new_value = NULL;

  /* append to path */
  if (old_value != NULL)
    {
      gchar **paths = g_strsplit (old_value, G_SEARCHPATH_SEPARATOR_S, 0);
      gsize   len = g_strv_length (paths) + 1;

      paths = g_renew (gchar *, paths, len + 1);
      paths[len - 1] = g_strdup (MOUSEPAD_GSETTINGS_SCHEMA_DIR);
      paths[len] = NULL;
      new_value = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, paths);
      g_strfreev (paths);
    }

  /* set new path */
  if (new_value == NULL)
    new_value = g_strdup (MOUSEPAD_GSETTINGS_SCHEMA_DIR);

  g_setenv ("GSETTINGS_SCHEMA_DIR", new_value, TRUE);
  g_free (new_value);
}



static void
mousepad_settings_store_class_init (MousepadSettingsStoreClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_settings_store_finalize;

  mousepad_settings_store_update_env ();
}



static void
mousepad_settings_store_finalize (GObject *object)
{
  MousepadSettingsStore *self = MOUSEPAD_SETTINGS_STORE (object);

  g_return_if_fail (MOUSEPAD_IS_SETTINGS_STORE (object));

  if (self->backend != NULL)
    g_object_unref (self->backend);

  g_list_free_full (self->roots, g_object_unref);
  g_hash_table_destroy (self->keys);

  G_OBJECT_CLASS (mousepad_settings_store_parent_class)->finalize (object);
}



static void
mousepad_settings_store_add_key (MousepadSettingsStore *self,
                                 const gchar           *setting,
                                 const gchar           *key_name,
                                 GSettings             *settings)
{
  MousepadSettingKey *key;

  key = mousepad_setting_key_new (key_name, settings);

  g_hash_table_insert (self->keys, (gpointer) g_intern_string (setting), key);
}



static void
mousepad_settings_store_add_settings (MousepadSettingsStore *self,
                                      const gchar           *schema_id,
                                      GSettingsSchemaSource *source,
                                      GSettings             *settings)
{
  GSettingsSchema  *schema;
  GSettings        *child_settings;
  gchar           **keys, **key, **children, **child;
  gchar            *setting, *child_schema_id;
  const gchar      *prefix;

  /* loop through keys in schema and store mapping of their setting name to GSettings */
  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
  keys = g_settings_schema_list_keys (schema);
  prefix = schema_id + MOUSEPAD_ID_LEN + 1;
  for (key = keys; key && *key; key++)
    {
      setting = g_strdup_printf ("%s.%s", prefix, *key);
      mousepad_settings_store_add_key (self, setting, *key, settings);
      g_free (setting);
    }
  g_strfreev (keys);

  /* loop through child schemas and add them too */
  children = g_settings_schema_list_children (schema);
  for (child = children; child && *child; child++)
    {
      child_settings = g_settings_get_child (settings, *child);
      child_schema_id = g_strdup_printf ("%s.%s", schema_id, *child);
      mousepad_settings_store_add_settings (self, child_schema_id, source, child_settings);
      g_object_unref (child_settings);
      g_free (child_schema_id);
    }
  g_strfreev (children);
  g_settings_schema_unref (schema);
}



static void
mousepad_settings_store_init (MousepadSettingsStore *self)
{
#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
  gchar *conf_file;

  conf_file = g_build_filename (g_get_user_config_dir (), MOUSEPAD_SETTINGS_RELPATH, NULL);
  self->backend = g_keyfile_settings_backend_new (conf_file, "/", NULL);
  g_free (conf_file);
#else
  self->backend = NULL;
#endif

  self->roots = NULL;
  self->keys = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, mousepad_setting_key_free);

  mousepad_settings_store_add_root (self, MOUSEPAD_ID);
}



MousepadSettingsStore *
mousepad_settings_store_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SETTINGS_STORE, NULL);
}



void
mousepad_settings_store_add_root (MousepadSettingsStore *self,
                                  const gchar           *schema_id)
{
  GSettingsSchemaSource *source;
  GSettingsSchema       *schema;
  GSettings             *root;

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  /* exit silently if no schema is found: plugins may have settings or not */
  if (schema == NULL)
    return;

  root = g_settings_new_full (schema, self->backend, NULL);
  g_settings_schema_unref (schema);

  self->roots = g_list_prepend (self->roots, root);

  mousepad_settings_store_add_settings (self, schema_id, source, root);
}



const gchar *
mousepad_settings_store_lookup_key_name (MousepadSettingsStore *self,
                                         const gchar           *setting)
{
  const gchar *key_name = NULL;

  if (! mousepad_settings_store_lookup (self, setting, &key_name, NULL))
    return NULL;

  return key_name;
}



GSettings *
mousepad_settings_store_lookup_settings (MousepadSettingsStore *self,
                                         const gchar           *setting)
{
  GSettings *settings = NULL;

  if (! mousepad_settings_store_lookup (self, setting, NULL, &settings))
    return NULL;

  return settings;
}



gboolean
mousepad_settings_store_lookup (MousepadSettingsStore *self,
                                const gchar           *setting,
                                const gchar          **key_name,
                                GSettings            **settings)
{
  MousepadSettingKey *key;

  g_return_val_if_fail (MOUSEPAD_IS_SETTINGS_STORE (self), FALSE);
  g_return_val_if_fail (setting != NULL, FALSE);

  if (key_name == NULL && settings == NULL)
    return g_hash_table_contains (self->keys, setting);

  key = g_hash_table_lookup (self->keys, setting);

  if (G_UNLIKELY (key == NULL))
    return FALSE;

  if (key_name != NULL)
    *key_name = key->name;

  if (settings != NULL)
    *settings = key->settings;

  return TRUE;
}
