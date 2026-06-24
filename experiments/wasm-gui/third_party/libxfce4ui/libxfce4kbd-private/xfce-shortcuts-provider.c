/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include <libxfce4kbd-private/xfce-shortcuts-provider.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_NAME,
};



typedef struct _XfceShortcutsProviderContext XfceShortcutsProviderContext;



static void xfce_shortcuts_provider_constructed      (GObject                    *object);
static void xfce_shortcuts_provider_finalize         (GObject                    *object);
static void xfce_shortcuts_provider_get_property     (GObject                    *object,
                                                      guint                       prop_id,
                                                      GValue                     *value,
                                                      GParamSpec                 *pspec);
static void xfce_shortcuts_provider_set_property     (GObject                    *object,
                                                      guint                       prop_id,
                                                      const GValue               *value,
                                                      GParamSpec                 *pspec);
static void xfce_shortcuts_provider_register         (XfceShortcutsProvider      *provider);
static void xfce_shortcuts_provider_property_changed (XfconfChannel              *channel,
                                                      gchar                      *property,
                                                      GValue                     *value,
                                                      XfceShortcutsProvider      *provider);



struct _XfceShortcutsProviderPrivate
{
  XfconfChannel *channel;
  gchar         *name;
  gchar         *default_base_property;
  gchar         *custom_base_property;
};

struct _XfceShortcutsProviderContext
{
  XfceShortcutsProvider *provider;
  GList                 *list;
  const gchar           *base_property;
  GHashTable            *properties;
};



G_DEFINE_TYPE_WITH_PRIVATE (XfceShortcutsProvider, xfce_shortcuts_provider, G_TYPE_OBJECT)



static void
xfce_shortcuts_provider_class_init (XfceShortcutsProviderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = xfce_shortcuts_provider_constructed;
  gobject_class->finalize = xfce_shortcuts_provider_finalize;
  gobject_class->get_property = xfce_shortcuts_provider_get_property;
  gobject_class->set_property = xfce_shortcuts_provider_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "name",
                                                        "name",
                                                        NULL,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_STRINGS));

  g_signal_new ("shortcut-removed",
                XFCE_TYPE_SHORTCUTS_PROVIDER,
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);

  g_signal_new ("shortcut-added",
                XFCE_TYPE_SHORTCUTS_PROVIDER,
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);
}



static void
xfce_shortcuts_provider_init (XfceShortcutsProvider *provider)
{
  provider->priv = xfce_shortcuts_provider_get_instance_private (provider);

  provider->priv->channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  g_signal_connect (provider->priv->channel, "property-changed",
                    G_CALLBACK (xfce_shortcuts_provider_property_changed), provider);
}



static void
xfce_shortcuts_provider_constructed (GObject *object)
{
  XfceShortcutsProvider *provider = XFCE_SHORTCUTS_PROVIDER (object);

  provider->priv->default_base_property = g_strdup_printf ("/%s/default", provider->priv->name);
  provider->priv->custom_base_property = g_strdup_printf ("/%s/custom", provider->priv->name);

  xfce_shortcuts_provider_register (provider);

  if (!xfce_shortcuts_provider_is_custom (provider))
    xfce_shortcuts_provider_reset_to_defaults (provider);
}



static void
xfce_shortcuts_provider_finalize (GObject *object)
{
  XfceShortcutsProvider *provider = XFCE_SHORTCUTS_PROVIDER (object);

  g_free (provider->priv->name);
  g_free (provider->priv->custom_base_property);
  g_free (provider->priv->default_base_property);

  g_object_unref (provider->priv->channel);

  (*G_OBJECT_CLASS (xfce_shortcuts_provider_parent_class)->finalize) (object);
}



static void
xfce_shortcuts_provider_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  XfceShortcutsProvider *provider = XFCE_SHORTCUTS_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, provider->priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_shortcuts_provider_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  XfceShortcutsProvider *provider = XFCE_SHORTCUTS_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_free (provider->priv->name);
      provider->priv->name = g_strdup (g_value_get_string (value));
      g_object_notify (object, "name");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_shortcuts_provider_register (XfceShortcutsProvider *provider)
{
  gchar       **provider_names;
  gchar       **names;
  gboolean      already_registered = FALSE;
  gint          i;
  const gchar  *name;

  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));

  name = xfce_shortcuts_provider_get_name (provider);
  if (G_UNLIKELY (name == NULL))
    return;

  provider_names = xfconf_channel_get_string_list (provider->priv->channel, "/providers");
  if (provider_names != NULL)
    for (i = 0; !already_registered && provider_names[i] != NULL; i++)
      already_registered = g_str_equal (provider_names[i], name);

  if (G_UNLIKELY (!already_registered))
    {
      names = g_new0 (gchar *, (provider_names != NULL ? g_strv_length (provider_names) : 0) + 2);
      i = 0;

      if (provider_names != NULL)
        for (; provider_names[i] != NULL; i++)
          names[i] = provider_names[i];

      names[i++] = (gchar *) name;
      names[i] = NULL;

      xfconf_channel_set_string_list (provider->priv->channel, "/providers",
                                      (const gchar * const *) names);

      g_free (names);
    }

  g_strfreev (provider_names);
}



static void
xfce_shortcuts_provider_property_changed (XfconfChannel         *channel,
                                          gchar                 *property,
                                          GValue                *value,
                                          XfceShortcutsProvider *provider)
{
  const gchar *shortcut;
  gchar       *override_property;

  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));

  DBG ("property = %s", property);

  if (!g_str_has_prefix (property, provider->priv->custom_base_property))
    return;

  override_property = g_strconcat (provider->priv->custom_base_property, "/override", NULL);

  if (G_UNLIKELY (g_utf8_collate (property, override_property) == 0))
    {
      g_free (override_property);
      return;
    }
  g_free (override_property);

  if (g_str_has_suffix (property, "/startup-notify"))
    return;

  shortcut = property + strlen (provider->priv->custom_base_property) + strlen ("/");

  if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
    g_signal_emit_by_name (provider, "shortcut-added", shortcut);
  else
    g_signal_emit_by_name (provider, "shortcut-removed", shortcut);
}



XfceShortcutsProvider *
xfce_shortcuts_provider_new (const gchar *name)
{
  return g_object_new (XFCE_TYPE_SHORTCUTS_PROVIDER, "name", name, NULL);
}



GList *
xfce_shortcuts_provider_get_providers (void)
{
  GList         *providers = NULL;
  XfconfChannel *channel;
  gchar        **names;
  gint           i;

  channel = xfconf_channel_get ("xfce4-keyboard-shortcuts");
  names = xfconf_channel_get_string_list (channel, "/providers");

  if (G_LIKELY (names != NULL))
    {
      for (i = 0; names[i] != NULL; ++i)
        providers = g_list_append (providers, xfce_shortcuts_provider_new (names[i]));
      g_strfreev (names);
    }

  return providers;
}



void
xfce_shortcuts_provider_free_providers (GList *providers)
{
  GList *iter;

  for (iter = g_list_first (providers); iter != NULL; iter = g_list_next (iter))
    g_object_unref (iter->data);

  g_list_free (providers);
}



const gchar *
xfce_shortcuts_provider_get_name (XfceShortcutsProvider *provider)
{
  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider), NULL);
  return provider->priv->name;
}



gboolean
xfce_shortcuts_provider_is_custom (XfceShortcutsProvider *provider)
{
  gchar   *property;
  gboolean override;

  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), FALSE);

  property = g_strconcat (provider->priv->custom_base_property, "/override", NULL);
  override = xfconf_channel_get_bool (provider->priv->channel, property, FALSE);
  g_free (property);

  return override;
}



void
xfce_shortcuts_provider_reset_to_defaults (XfceShortcutsProvider *provider)
{
  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));

  DBG ("property = %s", provider->priv->custom_base_property);

  xfconf_channel_reset_property (provider->priv->channel, provider->priv->custom_base_property, TRUE);
  xfce_shortcuts_provider_clone_defaults (provider);
}



static gboolean
_xfce_shortcuts_provider_clone_default (const gchar           *property,
                                        const GValue          *value,
                                        XfceShortcutsProvider *provider)
{
  const gchar *shortcut;
  gchar       *custom_property;

  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider), TRUE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), TRUE);

  if (G_UNLIKELY (!G_IS_VALUE (value)))
    return FALSE;

  shortcut = property + strlen (provider->priv->default_base_property) + strlen ("/");

  DBG ("shortcut = %s, command = %s", shortcut, g_value_get_string (value));

  custom_property = g_strconcat (provider->priv->custom_base_property, "/", shortcut, NULL);
  xfconf_channel_set_property (provider->priv->channel, custom_property, value);
  g_free (custom_property);

  return FALSE;
}



void
xfce_shortcuts_provider_clone_defaults (XfceShortcutsProvider *provider)
{
  GHashTable *properties;
  gchar      *property;

  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));

  /* Get default command shortcuts */
  properties = xfconf_channel_get_properties (provider->priv->channel, provider->priv->default_base_property);

  if (G_LIKELY (properties != NULL))
    {
      /* Copy from /commands/default to /commands/custom property by property */
      g_hash_table_foreach (properties,
                            (GHFunc) (void (*)(void)) _xfce_shortcuts_provider_clone_default,
                            provider);

      g_hash_table_destroy (properties);
    }

  DBG ("adding override property");

  /* Add the override property */
  property = g_strconcat (provider->priv->custom_base_property, "/override", NULL);
  xfconf_channel_set_bool (provider->priv->channel, property, TRUE);
  g_free (property);
}



static gboolean
_xfce_shortcuts_provider_get_shortcut (const gchar                  *property,
                                       const GValue                 *value,
                                       XfceShortcutsProviderContext *context)
{
  XfceShortcut *sc;
  const gchar  *shortcut;
  const gchar  *command;
  const GValue *snotify;
  gchar        *snotify_prop;

  g_return_val_if_fail (context != NULL, TRUE);
  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (context->provider), TRUE);

  if (G_VALUE_TYPE (value) != G_TYPE_STRING)
    return FALSE;

  if (!g_str_has_prefix (property, context->provider->priv->custom_base_property))
    return FALSE;

  shortcut = property + strlen (context->provider->priv->custom_base_property) + strlen ("/");

  command = g_value_get_string (value);

  if (G_LIKELY (shortcut != NULL
      && command != NULL
      && g_utf8_strlen (shortcut, -1) > 0
      && g_utf8_strlen (command, -1) > 0))
    {
      sc = g_slice_new0 (XfceShortcut);

      sc->property_name = g_strdup (property);
      sc->shortcut = g_strdup (shortcut);
      sc->command = g_strdup (command);

      /* Lookup startup notify in the hash table */
      snotify_prop = g_strconcat (property, "/startup-notify", NULL);
      snotify = g_hash_table_lookup (context->properties, snotify_prop);
      if (snotify != NULL)
        sc->snotify = g_value_get_boolean (snotify);
      else
        sc->snotify = FALSE;
      g_free (snotify_prop);
      context->list = g_list_append (context->list, sc);
    }

  return FALSE;
}



GList *
xfce_shortcuts_provider_get_shortcuts (XfceShortcutsProvider *provider)
{
  XfceShortcutsProviderContext context;
  GHashTable                  *properties;

  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider), NULL);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), NULL);

  properties = xfconf_channel_get_properties (provider->priv->channel, provider->priv->custom_base_property);

  context.provider = provider;
  context.list = NULL;
  context.properties = properties;

  if (G_LIKELY (properties != NULL))
    g_hash_table_foreach (properties,
                          (GHFunc) (void (*)(void)) _xfce_shortcuts_provider_get_shortcut,
                          &context);

  return context.list;
}



XfceShortcut *
xfce_shortcuts_provider_get_shortcut (XfceShortcutsProvider *provider,
                                      const gchar           *shortcut)
{
  XfceShortcut *sc = NULL;
  gchar        *base_property;
  gchar        *property;
  gchar        *command;
  gchar        *property2;
  gboolean      snotify;

  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider), NULL);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), NULL);

  if (G_LIKELY (xfce_shortcuts_provider_is_custom (provider)))
    base_property = provider->priv->custom_base_property;
  else
    base_property = provider->priv->default_base_property;

  property = g_strconcat (base_property, "/", shortcut, NULL);
  command = xfconf_channel_get_string (provider->priv->channel, property, NULL);

  if (G_LIKELY (command != NULL))
    {
      property2 = g_strconcat (property, "/startup-notify", NULL);
      snotify = xfconf_channel_get_bool (provider->priv->channel, property2, FALSE);
      g_free (property2);

      sc = g_slice_new0 (XfceShortcut);
      sc->command = command;
      sc->property_name = g_strdup (property);
      sc->shortcut = g_strdup (shortcut);
      sc->snotify = snotify;
    }

  g_free (property);

  return sc;
}



gboolean
xfce_shortcuts_provider_has_shortcut (XfceShortcutsProvider *provider,
                                      const gchar           *shortcut)
{
  gboolean has_property;
  gchar   *base_property;
  gchar   *property;

  g_return_val_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), FALSE);

  if (G_LIKELY (xfce_shortcuts_provider_is_custom (provider)))
    base_property = provider->priv->custom_base_property;
  else
    base_property = provider->priv->default_base_property;

  property = g_strconcat (base_property, "/", shortcut, NULL);
  has_property = xfconf_channel_has_property (provider->priv->channel, property);
  g_free (property);

  if (!has_property && g_strrstr (shortcut, "<Primary>"))
    {
      /* We want to match a shortcut with <Primary>. Older versions of
       * GTK+ used <Control> and this might be stored in Xfconf. We need
       * to check for this too. */

      gchar       *with_control_shortcut;

      with_control_shortcut = xfce_str_replace (shortcut, "Primary", "Control");

      DBG ("Looking for old GTK+ shortcut %s", with_control_shortcut);

      property =
        g_strconcat (base_property, "/", with_control_shortcut, NULL);
      has_property = xfconf_channel_has_property (provider->priv->channel, property);
      g_free (property);

      g_free (with_control_shortcut);
    }

  return has_property;
}



void
xfce_shortcuts_provider_set_shortcut (XfceShortcutsProvider *provider,
                                      const gchar           *shortcut,
                                      const gchar           *command,
                                      gboolean               snotify)
{
  gchar *property;
  gchar *property2;

  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));
  g_return_if_fail (shortcut != NULL && command != NULL);

  /* Only allow custom shortcuts to be changed */
  if (G_UNLIKELY (!xfce_shortcuts_provider_is_custom (provider)))
    return;

  property = g_strconcat (provider->priv->custom_base_property, "/", shortcut, NULL);

  if (xfconf_channel_has_property (provider->priv->channel, property))
    xfconf_channel_reset_property (provider->priv->channel, property, TRUE);

  if (snotify)
    {
      property2 = g_strconcat (property, "/startup-notify", NULL);
      xfconf_channel_set_bool (provider->priv->channel, property2, snotify);
      g_free (property2);
    }

  xfconf_channel_set_string (provider->priv->channel, property, command);

  g_free (property);

}



void
xfce_shortcuts_provider_reset_shortcut (XfceShortcutsProvider *provider,
                                        const gchar           *shortcut)
{
  gchar *property;

  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));
  g_return_if_fail (shortcut != NULL);

  property = g_strconcat (provider->priv->custom_base_property, "/", shortcut, NULL);

  DBG ("property = %s", property);

  xfconf_channel_reset_property (provider->priv->channel, property, TRUE);
  g_free (property);
}



void
xfce_shortcuts_free (GList *shortcuts)
{
  g_list_foreach (shortcuts, (GFunc) (void (*)(void)) xfce_shortcut_free, NULL);
  g_list_free (shortcuts);
}



void
xfce_shortcut_free (XfceShortcut *shortcut)
{
  if (G_UNLIKELY (shortcut == NULL))
    return;

  g_free (shortcut->property_name);
  g_free (shortcut->shortcut);
  g_free (shortcut->command);
  g_slice_free (XfceShortcut, shortcut);
}
