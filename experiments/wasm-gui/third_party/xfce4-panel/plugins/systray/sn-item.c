/*
 *  Copyright (c) 2017 Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>
#ifdef HAVE_DBUSMENU
#include <libdbusmenu-gtk/dbusmenu-gtk.h>
#endif

#include <common/panel-debug.h>
#include "sn-item.h"



static void                  sn_item_finalize                        (GObject                 *object);

static void                  sn_item_get_property                    (GObject                 *object,
                                                                      guint                    prop_id,
                                                                      GValue                  *value,
                                                                      GParamSpec              *pspec);

static void                  sn_item_set_property                    (GObject                 *object,
                                                                      guint                    prop_id,
                                                                      const GValue            *value,
                                                                      GParamSpec              *pspec);

static void                  sn_item_signal_received                 (GDBusProxy              *proxy,
                                                                      gchar                   *sender_name,
                                                                      gchar                   *signal_name,
                                                                      GVariant                *parameters,
                                                                      gpointer                 user_data);

static void                  sn_item_get_all_properties_result       (GObject                 *source_object,
                                                                      GAsyncResult            *res,
                                                                      gpointer                 user_data);



struct _SnItemClass
{
  GObjectClass         __parent__;
};

struct _SnItem
{
  GObject              __parent__;

  gboolean             started;
  gboolean             initialized;
  gboolean             exposed;

  GCancellable        *cancellable;
  GDBusProxy          *item_proxy;
  GDBusProxy          *properties_proxy;

  gchar               *bus_name;
  gchar               *object_path;
  gchar               *key;

  gchar               *id;
  gchar               *title;
  gchar               *tooltip_title;
  gchar               *tooltip_subtitle;
  gchar               *icon_desc;
  gchar               *attention_desc;

  gchar               *status;
  gchar               *icon_name;
  gchar               *attention_icon_name;
  gchar               *overlay_icon_name;
  GdkPixbuf           *icon_pixbuf;
  GdkPixbuf           *attention_icon_pixbuf;
  GdkPixbuf           *overlay_icon_pixbuf;
  gchar               *icon_theme_path;

  gboolean             item_is_menu;
  gchar               *menu_object_path;
  GtkWidget           *cached_menu;
};

G_DEFINE_TYPE (SnItem, sn_item, G_TYPE_OBJECT)



enum
{
  PROP_0,
  PROP_BUS_NAME,
  PROP_OBJECT_PATH,
  PROP_KEY,
  PROP_EXPOSED
};

enum
{
  EXPOSE,
  SEAL,
  FINISH,
  TOOLTIP_CHANGED,
  ICON_CHANGED,
  MENU_CHANGED,
  LAST_SIGNAL
};

static guint sn_item_signals[LAST_SIGNAL] = { 0, };



typedef struct
{
  GDBusConnection     *connection;
  guint                handler;
}
SubscriptionContext;



#define free_error_and_return_if_cancelled(error) \
if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) \
  { \
    g_error_free (error); \
    return; \
  } \
if (error != NULL) \
  {  \
    panel_debug (PANEL_DEBUG_SYSTRAY, "%s: Fatal error for item '%s': (domain '%s', code %d) %s", \
                 G_STRLOC, XFCE_IS_SN_ITEM (item) ? item->id : "", \
                 g_quark_to_string (error->domain), error->code, error->message); \
    g_error_free (error); \
  }



#define finish_and_return_if_true(condition) \
if (condition) \
  { \
    if (G_IS_OBJECT (item)) \
      { \
        panel_debug (PANEL_DEBUG_SYSTRAY, "%s: Finishing on error for item '%s'", \
                     G_STRLOC, item->id); \
        g_signal_emit (item, sn_item_signals[FINISH], 0); \
      } \
    return; \
  }



static void
sn_item_class_init (SnItemClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = sn_item_finalize;
  object_class->get_property = sn_item_get_property;
  object_class->set_property = sn_item_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BUS_NAME,
                                   g_param_spec_string ("bus-name", NULL, NULL, NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path", NULL, NULL, NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_KEY,
                                   g_param_spec_string ("key", NULL, NULL, NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_EXPOSED,
                                   g_param_spec_boolean ("exposed", NULL, NULL, FALSE,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  sn_item_signals[EXPOSE] =
    g_signal_new (g_intern_static_string ("expose"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  sn_item_signals[SEAL] =
    g_signal_new (g_intern_static_string ("seal"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  sn_item_signals[FINISH] =
    g_signal_new (g_intern_static_string ("finish"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  sn_item_signals[TOOLTIP_CHANGED] =
    g_signal_new (g_intern_static_string ("tooltip-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  sn_item_signals[ICON_CHANGED] =
    g_signal_new (g_intern_static_string ("icon-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  sn_item_signals[MENU_CHANGED] =
    g_signal_new (g_intern_static_string ("menu-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
sn_item_init (SnItem *item)
{
  item->started = FALSE;
  item->initialized = FALSE;
  item->exposed = TRUE;

  item->cancellable = g_cancellable_new ();
  item->item_proxy = NULL;
  item->properties_proxy = NULL;

  item->bus_name = NULL;
  item->object_path = NULL;
  item->key = NULL;

  item->id = NULL;
  item->title = NULL;
  item->tooltip_title = NULL;
  item->tooltip_subtitle = NULL;
  item->icon_desc = NULL;
  item->attention_desc = NULL;

  item->status = NULL;
  item->icon_name = NULL;
  item->attention_icon_name = NULL;
  item->overlay_icon_name = NULL;
  item->icon_pixbuf = NULL;
  item->attention_icon_pixbuf = NULL;
  item->overlay_icon_pixbuf = NULL;
  item->icon_theme_path = NULL;

  /* Ubuntu indicators don't support activate action and
     don't provide this option so it's enabled by default. */
  item->item_is_menu = TRUE;
  item->menu_object_path = NULL;
  item->cached_menu = NULL;
}



static void
sn_item_finalize (GObject *object)
{
  SnItem *item = XFCE_SN_ITEM (object);

  g_cancellable_cancel (item->cancellable);
  g_object_unref (item->cancellable);

  if (item->properties_proxy != NULL)
    g_object_unref (item->properties_proxy);

  if (item->item_proxy != NULL)
    g_object_unref (item->item_proxy);

  g_free (item->bus_name);
  g_free (item->object_path);
  g_free (item->key);

  g_free (item->id);
  g_free (item->title);
  g_free (item->tooltip_title);
  g_free (item->tooltip_subtitle);
  g_free (item->icon_desc);
  g_free (item->attention_desc);

  g_free (item->status);
  g_free (item->icon_name);
  g_free (item->attention_icon_name);
  g_free (item->overlay_icon_name);
  g_free (item->icon_theme_path);

  if (item->icon_pixbuf != NULL)
    g_object_unref (item->icon_pixbuf);
  if (item->attention_icon_pixbuf != NULL)
    g_object_unref (item->attention_icon_pixbuf);
  if (item->overlay_icon_pixbuf != NULL)
    g_object_unref (item->overlay_icon_pixbuf);

  g_free (item->menu_object_path);
  if (item->cached_menu != NULL)
    gtk_widget_destroy (item->cached_menu);

  G_OBJECT_CLASS (sn_item_parent_class)->finalize (object);
}



static void
sn_item_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  SnItem *item = XFCE_SN_ITEM (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, item->key);
      break;

    case PROP_EXPOSED:
      g_value_set_boolean (value, item->exposed);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
sn_item_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  SnItem *item = XFCE_SN_ITEM (object);

  switch (prop_id)
    {
    case PROP_BUS_NAME:
      g_free (item->bus_name);
      item->bus_name = g_value_dup_string (value);
      break;

    case PROP_OBJECT_PATH:
      g_free (item->object_path);
      item->object_path = g_value_dup_string (value);
      break;

    case PROP_KEY:
      g_free (item->key);
      item->key = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
sn_item_subscription_context_unsubscribe (gpointer  data,
                                          GObject  *where_the_object_was)
{
  SubscriptionContext *context = data;

  g_dbus_connection_signal_unsubscribe (context->connection, context->handler);
  g_free (context);
}



static void
sn_item_name_owner_changed (GDBusConnection *connection,
                            const gchar     *sender_name,
                            const gchar     *object_path,
                            const gchar     *interface_name,
                            const gchar     *signal_name,
                            GVariant        *parameters,
                            gpointer         user_data)
{
  SnItem   *item = user_data;
  gchar    *new_owner;
  gboolean  finish;

  g_variant_get (parameters, "(sss)", NULL, NULL, &new_owner);
  finish = new_owner == NULL || strlen (new_owner) == 0;
  g_free (new_owner);

  finish_and_return_if_true (finish);
}



static void
sn_item_properties_callback (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
  SnItem *item = user_data;
  GError *error = NULL;

  item->properties_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  free_error_and_return_if_cancelled (error);
  finish_and_return_if_true (item->properties_proxy == NULL);

  g_signal_connect (item->item_proxy, "g-signal",
                    G_CALLBACK (sn_item_signal_received), item);
  sn_item_invalidate (item, FALSE);
}



static void
sn_item_item_callback (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  SnItem              *item = user_data;
  GError              *error = NULL;
  SubscriptionContext *context;

  item->item_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  free_error_and_return_if_cancelled (error);
  finish_and_return_if_true (item->item_proxy == NULL);

  context = g_new0 (SubscriptionContext, 1);
  context->connection = g_dbus_proxy_get_connection (item->item_proxy);
  context->handler =
    g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (item->item_proxy),
                                        "org.freedesktop.DBus",
                                        "org.freedesktop.DBus",
                                        "NameOwnerChanged",
                                        "/org/freedesktop/DBus",
                                        g_dbus_proxy_get_name (item->item_proxy),
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        sn_item_name_owner_changed,
                                        item, NULL);
  g_object_weak_ref (G_OBJECT (item->item_proxy),
                     sn_item_subscription_context_unsubscribe, context);

  g_dbus_proxy_new (g_dbus_proxy_get_connection (item->item_proxy),
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    item->bus_name,
                    item->object_path,
                    "org.freedesktop.DBus.Properties",
                    item->cancellable,
                    sn_item_properties_callback,
                    item);
}



static gboolean
sn_item_start_failed (gpointer user_data)
{
  SnItem *item = user_data;

  /* start is failed, emit the signal in next loop iteration */
  panel_debug (PANEL_DEBUG_SYSTRAY, "%s: Finishing on error for item '%s'",
               G_STRLOC, XFCE_IS_SN_ITEM (item) ? item->id : "");
  g_signal_emit (G_OBJECT (item), sn_item_signals[FINISH], 0);

  return G_SOURCE_REMOVE;
}



void
sn_item_start (SnItem *item)
{
  g_return_if_fail (XFCE_IS_SN_ITEM (item));
  g_return_if_fail (!item->started);

  if (!g_dbus_is_name (item->bus_name))
    {
      g_idle_add (sn_item_start_failed, item);
      return;
    }

  item->started = TRUE;
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            item->bus_name,
                            item->object_path,
                            "org.kde.StatusNotifierItem",
                            item->cancellable,
                            sn_item_item_callback,
                            item);
}



void
sn_item_invalidate (SnItem   *item,
                    gboolean  force_update)
{
  g_return_if_fail (XFCE_IS_SN_ITEM (item));

  /* leave if the properties proxy has not yet been created */
  if (item->properties_proxy == NULL)
    return;

  if (force_update)
    {
      /* force menu update (see issue #567) */
      if (item->menu_object_path != NULL)
        {
          g_free (item->menu_object_path);
          item->menu_object_path = NULL;
        }
    }

  g_dbus_proxy_call (item->properties_proxy,
                     "GetAll",
                     g_variant_new ("(s)", "org.kde.StatusNotifierItem"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     item->cancellable,
                     sn_item_get_all_properties_result,
                     item);
}



static void
sn_item_signal_received (GDBusProxy *proxy,
                         gchar      *sender_name,
                         gchar      *signal_name,
                         GVariant   *parameters,
                         gpointer    user_data)
{
  sn_item_invalidate (user_data, FALSE);
}



static gboolean
sn_item_pixbuf_equals (GdkPixbuf *p1,
                       GdkPixbuf *p2)
{
  guchar *p1p, *p2p;
  guint   p1l, p2l;
  guint   i;

  if (p1 == p2 || (p1 == NULL && p2 == NULL))
    return TRUE;

  if ((p1 == NULL) != (p2 == NULL))
    return FALSE;

  p1p = gdk_pixbuf_get_pixels_with_length (p1, &p1l);
  p2p = gdk_pixbuf_get_pixels_with_length (p2, &p2l);

  if (p1l != p2l)
    return FALSE;

  for (i = 0; i < p1l; i++)
    if (p1p[i] != p2p[i])
      return FALSE;

  return TRUE;
}



static void
sn_item_free (guchar   *pixels,
              gpointer  data)
{
  g_free (pixels);
}



static GdkPixbuf *
sn_item_extract_pixbuf (GVariant *variant)
{
  GVariantIter  *iter;
  gint           width, height;
  gint           lwidth = 0, lheight = 0;
  GVariant      *array_value;
  gconstpointer  data;
  guchar        *array = NULL;
  gsize          size;
  guchar         alpha;
  gint           i;

  if (variant == NULL)
    return NULL;

  g_variant_get (variant, "a(iiay)", &iter);

  if (iter == NULL)
    return NULL;

  while (g_variant_iter_loop (iter, "(ii@ay)", &width, &height, &array_value))
    {
      if (width > 0 && height > 0 && array_value != NULL &&
          width * height > lwidth * lheight)
        {
          size = g_variant_get_size (array_value);
          /* sanity check */
          if (size == (gsize)(4 * width * height))
            {
              /* find the largest image */
              data = g_variant_get_data (array_value);
              if (data != NULL)
                {
                  if (array != NULL)
                    g_free (array);
#if GLIB_CHECK_VERSION(2, 68, 0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                  array = g_memdup2 (data, size);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
                  array = g_memdup (data, size);
#endif
                  lwidth = width;
                  lheight = height;
                }
            }
        }
    }

  g_variant_iter_free (iter);

  if (array != NULL)
    {
      /* argb to rgba */
      for (i = 0; i < 4 * lwidth * lheight; i += 4)
        {
          alpha = array[i];
          array[i] = array[i + 1];
          array[i + 1] = array[i + 2];
          array[i + 2] = array[i + 3];
          array[i + 3] = alpha;
        }

      return gdk_pixbuf_new_from_data (array, GDK_COLORSPACE_RGB,
                                       TRUE, 8, lwidth, lheight, 4 * lwidth,
                                       sn_item_free, NULL);
    }

  return NULL;
}



static void
sn_item_get_all_properties_result (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  SnItem       *item = user_data;
  GError       *error = NULL;
  GVariant     *properties;
  GVariantIter *iter = NULL;
  const gchar  *name;
  GVariant     *value;

  const gchar  *cstr_val1;
  gchar        *str_val1;
  gchar        *str_val2;
  gboolean      bool_val1;
  GdkPixbuf    *pb_val1;

  gboolean      update_exposed = FALSE;
  gboolean      update_tooltip = FALSE;
  gboolean      update_icon = FALSE;
  gboolean      update_menu = FALSE;

  properties = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (properties == NULL)
    {
      free_error_and_return_if_cancelled (error);
      return;
    }

  #define string_empty_null(s) ((s) != NULL ? (s) : "")

  #define update_new_string(val, entry, update_what) \
  if (g_strcmp0 (string_empty_null (val), string_empty_null (item->entry))) \
    { \
      g_free (item->entry); \
      item->entry = \
        val != NULL && strlen (val != NULL ? val : "") > 0 \
        ? g_strdup (val) : NULL; \
      update_what = TRUE; \
    }

  #define update_new_pixbuf(val, entry, update_what) \
  if (!sn_item_pixbuf_equals (val, item->entry)) \
    { \
      if (item->entry != NULL) \
        g_object_unref (item->entry); \
      item->entry = val; \
      update_what = TRUE; \
    } \
  else if (val != NULL) \
    { \
      g_object_unref (val); \
    }

  if (g_variant_check_format_string (properties, "(a{sv})", FALSE) == FALSE)
    {
      g_warning ("Could not parse properties for StatusNotifierItem.");
      return;
    }
  g_variant_get (properties, "(a{sv})", &iter);

  while (g_variant_iter_loop (iter, "{&sv}", &name, &value)) {
    if (!g_strcmp0 (name, "Id"))
      {
        if (item->id == NULL)
          item->id = g_variant_dup_string (value, NULL);
      }
    else if (!g_strcmp0 (name, "Status"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, status, update_icon);
        bool_val1 = g_strcmp0 (item->status, "Passive") != 0;
        if (bool_val1 != item->exposed)
          {
            item->exposed = bool_val1;
            update_exposed = TRUE;
          }
      }
    else if (!g_strcmp0 (name, "Title"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, title, update_tooltip);
      }
    else if (!g_strcmp0 (name, "ToolTip"))
      {
        cstr_val1 = g_variant_get_type_string (value);
        if (!g_strcmp0 (cstr_val1, "(sa(iiay)ss)"))
          {
            g_variant_get (value, "(sa(iiay)ss)", NULL, NULL, &str_val1, &str_val2);
            update_new_string (str_val1, tooltip_title, update_tooltip);
            update_new_string (str_val2, tooltip_subtitle, update_tooltip);
            g_free (str_val1);
            g_free (str_val2);
          }
        else if (!g_strcmp0 (cstr_val1, "s"))
          {
            cstr_val1 = g_variant_get_string (value, NULL);
            update_new_string (cstr_val1, tooltip_title, update_tooltip);
            update_new_string (NULL, tooltip_subtitle, update_tooltip);
          }
        else
          {
            update_new_string (NULL, tooltip_title, update_tooltip);
            update_new_string (NULL, tooltip_subtitle, update_tooltip);
          }
      }
    else if (!g_strcmp0 (name, "ItemIsMenu"))
      {
        bool_val1 = g_variant_get_boolean (value);
        if (bool_val1 != item->item_is_menu)
          {
            item->item_is_menu = bool_val1;
            update_menu = TRUE;
          }
      }
    else if (!g_strcmp0 (name, "Menu"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, menu_object_path, update_menu);
      }
    else if (!g_strcmp0 (name, "IconThemePath"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, icon_theme_path, update_icon);
      }
    else if (!g_strcmp0 (name, "IconName"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, icon_name, update_icon);
      }
    else if (!g_strcmp0 (name, "IconPixmap"))
      {
        pb_val1 = sn_item_extract_pixbuf (value);
        update_new_pixbuf (pb_val1, icon_pixbuf, update_icon);
      }
    else if (!g_strcmp0 (name, "IconAccessibleDesc"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, icon_desc, update_tooltip);
      }
    else if (!g_strcmp0 (name, "AttentionIconName"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, attention_icon_name, update_icon);
      }
    else if (!g_strcmp0 (name, "AttentionIconPixmap"))
      {
        pb_val1 = sn_item_extract_pixbuf (value);
        update_new_pixbuf (pb_val1, attention_icon_pixbuf, update_icon);
      }
    else if (!g_strcmp0 (name, "AttentionAccessibleDesc"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, attention_desc, update_tooltip);
      }
    else if (!g_strcmp0 (name, "OverlayIconName"))
      {
        cstr_val1 = g_variant_get_string (value, NULL);
        update_new_string (cstr_val1, overlay_icon_name, update_icon);
      }
    else if (!g_strcmp0 (name, "OverlayIconPixmap"))
      {
        pb_val1 = sn_item_extract_pixbuf (value);
        update_new_pixbuf (pb_val1, overlay_icon_pixbuf, update_icon);
      }
  }

  g_variant_iter_free (iter);
  g_variant_unref (properties);

  #undef update_new_pixbuf
  #undef update_new_string
  #undef string_empty_null

  if (!item->initialized)
    {
      if (item->id != NULL)
        {
          item->initialized = TRUE;
          update_exposed = TRUE;
          update_tooltip = FALSE;
          update_icon = TRUE;
          update_menu = FALSE;
        }
      else
        return;
    }

  if (update_exposed)
    g_signal_emit (G_OBJECT (item), sn_item_signals[item->exposed ? EXPOSE : SEAL], 0);

  if (item->exposed)
    {
      if (update_tooltip)
        g_signal_emit (G_OBJECT (item), sn_item_signals[TOOLTIP_CHANGED], 0);
      if (update_icon)
        {
          /* we prioritize the attention icon if it exists afterwards, but it may
           * exist here without the corresponding status, which is authentic */
          if (g_strcmp0 (item->status, "NeedsAttention") != 0)
            {
              g_clear_object (&item->attention_icon_pixbuf);
              g_free (item->attention_icon_name);
              item->attention_icon_name = NULL;
            }
          g_signal_emit (G_OBJECT (item), sn_item_signals[ICON_CHANGED], 0);
        }
      if (update_menu)
        {
          if (item->cached_menu != NULL)
            g_object_unref (item->cached_menu);
          item->cached_menu = NULL;
          g_signal_emit (G_OBJECT (item), sn_item_signals[MENU_CHANGED], 0);
        }
    }
}



const gchar *
sn_item_get_name (SnItem *item)
{
  g_return_val_if_fail (XFCE_IS_SN_ITEM (item), NULL);
  g_return_val_if_fail (item->initialized, NULL);

  return item->id;
}



void
sn_item_get_icon (SnItem       *item,
                  const gchar **theme_path,
                  const gchar **icon_name,
                  GdkPixbuf   **icon_pixbuf,
                  const gchar **overlay_icon_name,
                  GdkPixbuf   **overlay_icon_pixbuf)
{
  g_return_if_fail (XFCE_IS_SN_ITEM (item));
  g_return_if_fail (item->initialized);

  if (icon_name != NULL)
    {
      *icon_name = item->attention_icon_name != NULL
                   ? item->attention_icon_name
                   : item->icon_name;
    }

  if (icon_pixbuf != NULL)
    {
      *icon_pixbuf = item->attention_icon_pixbuf != NULL
                   ? item->attention_icon_pixbuf
                   : item->icon_pixbuf;
    }

  if (overlay_icon_name != NULL)
    *overlay_icon_name = item->overlay_icon_name;

  if (overlay_icon_pixbuf != NULL)
    *overlay_icon_pixbuf = item->overlay_icon_pixbuf;

  if (theme_path != NULL)
    *theme_path = item->icon_theme_path;
}



void
sn_item_get_tooltip (SnItem       *item,
                     const gchar **title,
                     const gchar **subtitle)
{
  gchar *stub;

  g_return_if_fail (XFCE_IS_SN_ITEM (item));
  g_return_if_fail (item->initialized);

  if (title == NULL)
    title = (gpointer)&stub;
  if (subtitle == NULL)
    subtitle = (gpointer)&stub;

  #define sn_subtitle(subtitle) (g_strcmp0 (subtitle, *title) ? subtitle : NULL)

  if (item->tooltip_title != NULL && item->tooltip_subtitle != NULL)
    {
      *title = item->tooltip_title;
      *subtitle = sn_subtitle (item->tooltip_subtitle);
    }
  else if (item->attention_desc != NULL)
    {
      /* try to use attention_desc as subtitle */
      if (item->tooltip_title != NULL)
        {
          *title = item->tooltip_title;
          *subtitle = sn_subtitle (item->attention_desc);
        }
      else if (item->title != NULL)
        {
          *title = item->title;
          *subtitle = sn_subtitle (item->attention_desc);
        }
      else
        {
          *title = item->attention_desc;
          *subtitle = NULL;
        }
    }
  else if (item->icon_desc != NULL)
    {
      /* try to use icon_desc as subtitle */
      if (item->tooltip_title != NULL)
        {
          *title = item->tooltip_title;
          *subtitle = sn_subtitle (item->icon_desc);
        }
      else if (item->title != NULL)
        {
          *title = item->title;
          *subtitle = sn_subtitle (item->icon_desc);
        }
      else
        {
          *title = item->icon_desc;
          *subtitle = NULL;
        }
    }
  else if (item->tooltip_title != NULL )
    {
      *title = item->tooltip_title;
      *subtitle = NULL;
    }
  else if (item->title != NULL )
    {
      *title = item->title;
      *subtitle = NULL;
    }
  else
    {
      *title = NULL;
      *subtitle = NULL;
    }

  #undef sn_subtitle
}



gboolean
sn_item_is_menu_only (SnItem *item)
{
  g_return_val_if_fail (XFCE_IS_SN_ITEM (item), FALSE);
  g_return_val_if_fail (item->initialized, FALSE);

  return item->item_is_menu;
}



GtkWidget *
sn_item_get_menu (SnItem *item)
{
  #ifdef HAVE_DBUSMENU
  DbusmenuGtkMenu   *menu;
  DbusmenuGtkClient *client;

  g_return_val_if_fail (XFCE_IS_SN_ITEM (item), NULL);
  g_return_val_if_fail (item->initialized, NULL);

  if (item->cached_menu == NULL && item->menu_object_path != NULL)
    {
      menu = dbusmenu_gtkmenu_new (item->bus_name, item->menu_object_path);
      if (menu != NULL)
        {
          client = dbusmenu_gtkmenu_get_client (menu);
          dbusmenu_gtkclient_set_accel_group (client, gtk_accel_group_new ());
          g_object_ref_sink (menu);
          item->cached_menu = GTK_WIDGET (menu);
        }
    }

  return item->cached_menu;
  #else
  return NULL;
  #endif
}



void
sn_item_activate (SnItem *item,
                  gint    x_root,
                  gint    y_root)
{
  g_return_if_fail (XFCE_IS_SN_ITEM (item));
  g_return_if_fail (item->initialized);
  g_return_if_fail (item->item_proxy != NULL);

  g_dbus_proxy_call (item->item_proxy, "Activate",
                     g_variant_new ("(ii)", x_root, y_root),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}



void
sn_item_secondary_activate (SnItem *item,
                            gint    x_root,
                            gint    y_root)
{
  g_return_if_fail (XFCE_IS_SN_ITEM (item));
  g_return_if_fail (item->initialized);
  g_return_if_fail (item->item_proxy != NULL);

  g_dbus_proxy_call (item->item_proxy, "SecondaryActivate",
                     g_variant_new ("(ii)", x_root, y_root),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}



void
sn_item_scroll (SnItem *item,
                gint    delta_x,
                gint    delta_y)
{
  g_return_if_fail (XFCE_IS_SN_ITEM (item));
  g_return_if_fail (item->initialized);
  g_return_if_fail (item->item_proxy != NULL);

  if (delta_x != 0)
    {
      g_dbus_proxy_call (item->item_proxy, "Scroll",
                         g_variant_new ("(is)", delta_x, "horizontal"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1, NULL, NULL, NULL);
    }

  if (delta_y != 0)
    {
      g_dbus_proxy_call (item->item_proxy, "Scroll",
                         g_variant_new ("(is)", delta_y, "vertical"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1, NULL, NULL, NULL);
    }
}
