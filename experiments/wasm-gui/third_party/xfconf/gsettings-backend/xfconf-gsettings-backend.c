/*
 * Copyright (C) 2018 - Ali Abdallah <ali@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <glib.h>

#include "xfconf/xfconf.h"
#include "xfconf/xfconf-private.h"

#include "common/xfconf-gvaluefuncs.h"

#include "xfconf-gsettings-backend.h"

struct _XfconfGsettingsBackend
{
  GSettingsBackend __parent__;

  XfconfChannel    *channel;

  GHashTable       *changed_prop;
  GHashTable       *subscribed_prop;

  gint              nhandled_tree_node;
};

G_DEFINE_TYPE (XfconfGsettingsBackend, xfconf_gsettings_backend, G_TYPE_SETTINGS_BACKEND);

static void
xfconf_gsettings_backend_property_changed_cb (XfconfGsettingsBackend *self,
                                              const gchar            *property,
                                              const GValue           *value)
{
  gpointer origin_tag;
  gboolean found;

  /* Look if this property has been changed by calling the write method below */
  found = g_hash_table_lookup_extended (self->changed_prop, property, NULL, &origin_tag);

  if (found) {
    /* Emit the changed signal */
    g_debug ("Emitting property changed signal '%s'\n", property);
    g_settings_backend_changed ((GSettingsBackend*)self, property, origin_tag);
    g_hash_table_remove (self->changed_prop, property);
  } else {
    /* Check to see if the client subscribed on that property */
    GList *keys;
    GList *l;

    keys = g_hash_table_get_keys (self->subscribed_prop);
    for (l = keys; l; l=l->next) {
      if (g_str_has_prefix (property, (gchar*)l->data)) {
        found = TRUE;
        g_debug ("Emitting property changed signal '%s'\n", property);
        g_settings_backend_changed ((GSettingsBackend*)self, property, NULL);
        break;
      }
    }
    g_list_free(keys);
  }

  if (!found)
    g_warning ("Changed property '%s' not expected!", property);
}

static GVariant *
xfconf_gsettings_backend_read (GSettingsBackend   *backend,
                               const gchar        *key,
                               const GVariantType *expected_type,
                               gboolean            default_value)
{
  XfconfGsettingsBackend *self = XFCONF_GSETTINGS_BACKEND(backend);
  GValue value = G_VALUE_INIT;
  GVariant *variant;
  gboolean found;

  /* The GSettings will take care of handling the default value */
  if (default_value)
    return NULL;

  found = xfconf_channel_get_property (self->channel,
                                       key,
                                       &value);
  if (!found)
    return NULL;

  variant = xfconf_gvalue_to_gvariant (&value);

  /* Set it as a string type */
  if (!g_variant_is_of_type (variant, expected_type)) {
    GError *error = NULL;
    gchar *type_str;

    type_str = g_variant_type_dup_string (expected_type);

    g_variant_unref(variant);
    variant = g_variant_parse (expected_type, g_value_get_string(&value), NULL, NULL, &error);

    if (error) {
      g_critical ("Failed to handle property '%s' with expected type '%s' => %s",
                   key, type_str, error->message);
      g_error_free (error);
      return (NULL);
    }
    g_free(type_str);
  }

  g_value_unset (&value);
  return variant;
}

static void
xfconf_gsettings_backend_reset (GSettingsBackend   *backend,
                                const gchar        *key,
                                gpointer            origin_tag)
{
  XfconfGsettingsBackend *self;

  self = XFCONF_GSETTINGS_BACKEND(backend);

  g_hash_table_replace (self->changed_prop, g_strdup(key), origin_tag);

  xfconf_channel_reset_property (self->channel, key, TRUE);
}

static gboolean
xfconf_gsettings_backend_get_writable (GSettingsBackend *backend,
                                       const gchar      *key)
{
  XfconfGsettingsBackend *self;

  self = XFCONF_GSETTINGS_BACKEND(backend);

  return !xfconf_channel_is_property_locked(self->channel, key);
}

static gboolean
xfconf_gsettings_backend_write_full (GSettingsBackend *backend,
                                     const gchar      *key,
                                     GVariant         *variant,
                                     gboolean          add_sig_changed,
                                     gpointer          origin_tag)
{
  XfconfGsettingsBackend *self;
  GValue *value;
  gchar *var_str;
  gboolean ret_val;

  self = XFCONF_GSETTINGS_BACKEND(backend);

  g_debug("Writing property %s variant %s\n", key, g_variant_get_type_string(variant));

  value = xfconf_gvariant_to_gvalue (variant);

  if (value == NULL) {
    value = g_new0 (GValue, 1);
    var_str = g_variant_print(variant, FALSE);

    g_value_init(value, G_TYPE_STRING);
    g_value_set_string(value, var_str);
  }

  if (add_sig_changed)
    g_hash_table_replace (self->changed_prop, g_strdup(key), origin_tag);

  ret_val = xfconf_channel_set_property (self->channel, key, value);
  if (ret_val == FALSE && add_sig_changed)
    g_hash_table_remove (self->changed_prop, key);

  g_value_unset (value);
  g_free (value);

  return ret_val;
}

static gboolean
xfconf_gsettings_backend_write (GSettingsBackend *backend,
                                const gchar      *key,
                                GVariant         *variant,
                                gpointer          origin_tag)
{
  return xfconf_gsettings_backend_write_full (backend, key, variant, TRUE, origin_tag);
}

static gboolean
xfconf_gsettings_backend_tree_traverse (const gchar            *key,
                                        GVariant               *variant,
                                        XfconfGsettingsBackend *self)
{

  if (!xfconf_gsettings_backend_write_full ((GSettingsBackend*)self, key, variant, FALSE, NULL))
    return TRUE;

  self->nhandled_tree_node++;

  return FALSE;
}

static gboolean
xfconf_gsettings_backend_write_tree (GSettingsBackend *backend,
                                     GTree            *tree,
                                     gpointer          origin_tag)
{
  XfconfGsettingsBackend *self;

  self = XFCONF_GSETTINGS_BACKEND(backend);

  self->nhandled_tree_node = 0;

  g_tree_foreach (tree, (GTraverseFunc) xfconf_gsettings_backend_tree_traverse, self);

  /* If we manage to handle all Tree nodes, send the changed signal */
  if (self->nhandled_tree_node == g_tree_nnodes (tree)) {
    g_settings_backend_changed_tree (backend, tree, origin_tag);
    return TRUE;
  }

  return FALSE;
}

static void
xfconf_gsettings_backend_subscribe (GSettingsBackend *backend,
                                    const char       *name)
{
  XfconfGsettingsBackend *self;

  self = XFCONF_GSETTINGS_BACKEND(backend);

  g_debug ("Subscribe on property '%s'\n", name);

  g_hash_table_replace (self->subscribed_prop, g_strdup(name), g_strdup(name));
}

static void
xfconf_gsettings_backend_unsubscribe (GSettingsBackend *backend,
                                      const char       *name)
{
  XfconfGsettingsBackend *self;

  self = XFCONF_GSETTINGS_BACKEND(backend);

  g_debug ("Unsubscribe from property '%s'\n", name);

  g_hash_table_remove (self->subscribed_prop, name);
}


static void
xfconf_gsettings_backend_sync (GSettingsBackend *backend)
{
  GDBusConnection *bus;
  GError *error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (error != NULL) {
    g_critical ("Failed to get bus connection '%s'", error->message);
    g_error_free (error);
    return;
  }

  g_dbus_connection_flush_sync (bus, NULL, NULL);
}

static gboolean
xfconf_gsettings_backend_has_prefix (gconstpointer v1,
                                     gconstpointer v2)
{
  return g_str_has_prefix ((const gchar*)v1, (const gchar*)v2);
}

static void
xfconf_gsettings_backend_finalize (XfconfGsettingsBackend *self)
{
  g_object_unref (self->channel);

  g_hash_table_destroy (self->changed_prop);

  g_hash_table_destroy (self->subscribed_prop);

  xfconf_shutdown();

  G_OBJECT_CLASS(xfconf_gsettings_backend_parent_class)->finalize((GObject*)self);
}

static void
xfconf_gsettings_backend_init (XfconfGsettingsBackend *self)
{
  GError *error = NULL;

  if (!xfconf_init (&error)) {
    g_critical ("Failed to get connection to xfconfd: %s", error->message);
    g_error_free (error);
    return;
  }

  self->nhandled_tree_node = 0;

  self->channel = xfconf_channel_new ("gsettings");

  self->changed_prop = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, NULL);

  self->subscribed_prop = g_hash_table_new_full (g_str_hash, xfconf_gsettings_backend_has_prefix,
                                                 g_free, g_free);

  g_signal_connect_swapped (self->channel, "property-changed",
                            G_CALLBACK (xfconf_gsettings_backend_property_changed_cb), self);
}

static void
xfconf_gsettings_backend_class_init (XfconfGsettingsBackendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GSettingsBackendClass *gsettings_class = G_SETTINGS_BACKEND_CLASS (klass);

  gsettings_class->read = xfconf_gsettings_backend_read;
  gsettings_class->reset = xfconf_gsettings_backend_reset;
  gsettings_class->get_writable = xfconf_gsettings_backend_get_writable;
  gsettings_class->write_tree = xfconf_gsettings_backend_write_tree;
  gsettings_class->write = xfconf_gsettings_backend_write;
  gsettings_class->subscribe = xfconf_gsettings_backend_subscribe;
  gsettings_class->unsubscribe = xfconf_gsettings_backend_unsubscribe;
  gsettings_class->sync = xfconf_gsettings_backend_sync;

  object_class->finalize = (void (*) (GObject *object)) xfconf_gsettings_backend_finalize;
}

XfconfGsettingsBackend* xfconf_gsettings_backend_new (void)
{
  XfconfGsettingsBackend *xfconf_gsettings = g_object_new (XFCONF_GSETTINGS_BACKEND_TYPE, NULL);
  return xfconf_gsettings;
}
