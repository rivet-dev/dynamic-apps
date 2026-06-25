/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



/* the timeout until the sweeper is run (in seconds) */
#define THUNAR_ICON_FACTORY_SWEEP_TIMEOUT (30)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ICON_THEME,
  PROP_THUMBNAIL_MODE,
  PROP_THUMBNAIL_DRAW_FRAMES,
  PROP_THUMBNAIL_SIZE,
  PROP_THUMBNAIL_MAX_FILE_SIZE,
};



typedef struct _ThunarIconKey ThunarIconKey;



static void       thunar_icon_factory_dispose               (GObject                  *object);
static void       thunar_icon_factory_finalize              (GObject                  *object);
static void       thunar_icon_factory_get_property          (GObject                  *object,
                                                             guint                     prop_id,
                                                             GValue                   *value,
                                                             GParamSpec               *pspec);
static void       thunar_icon_factory_set_property          (GObject                  *object,
                                                             guint                     prop_id,
                                                             const GValue             *value,
                                                             GParamSpec               *pspec);
static gboolean   thunar_icon_factory_changed               (GSignalInvocationHint    *ihint,
                                                             guint                     n_param_values,
                                                             const GValue             *param_values,
                                                             gpointer                  user_data);
static gboolean   thunar_icon_factory_sweep_timer           (gpointer                  user_data);
static void       thunar_icon_factory_sweep_timer_destroy   (gpointer                  user_data);
static GdkPixbuf *thunar_icon_factory_load_from_file        (ThunarIconFactory        *factory,
                                                             const gchar              *path,
                                                             gint                      size);
static GdkPixbuf *thunar_icon_factory_lookup_icon           (ThunarIconFactory        *factory,
                                                             const gchar              *name,
                                                             gint                      size,
                                                             gboolean                  wants_default);
static guint      thunar_icon_key_hash                      (gconstpointer             data);
static gboolean   thunar_icon_key_equal                     (gconstpointer             a,
                                                             gconstpointer             b);
static void       thunar_icon_key_free                      (gpointer                  data);
static GdkPixbuf *thunar_icon_factory_load_fallback         (ThunarIconFactory        *factory,
                                                             gint                      size);



struct _ThunarIconFactoryClass
{
  GObjectClass __parent__;
};

struct _ThunarIconFactory
{
  GObject __parent__;

  ThunarPreferences   *preferences;

  GHashTable          *icon_cache;

  GtkIconTheme        *icon_theme;

  ThunarThumbnailMode  thumbnail_mode;

  gboolean             thumbnail_draw_frames;

  ThunarThumbnailSize  thumbnail_size;

  /* maximum file size (in bytes) allowed to be thumbnailed */
  guint64              thumbnail_max_file_size;

  guint                sweep_timer_id;

  gulong               changed_hook_id;

  /* stamp that gets bumped when the theme changes */
  guint                theme_stamp;
};

struct _ThunarIconKey
{
  gchar *name;
  gint   size;
};

typedef struct
{
  ThunarFileIconState   icon_state;
  ThunarFileThumbState  thumb_state;
  gint                  icon_size;
  guint                 stamp;
  GdkPixbuf            *icon;
}
ThunarIconStore;



static GQuark thunar_icon_factory_quark = 0;
static GQuark thunar_icon_factory_store_quark = 0;



G_DEFINE_TYPE (ThunarIconFactory, thunar_icon_factory, G_TYPE_OBJECT)



static void
thunar_icon_factory_class_init (ThunarIconFactoryClass *klass)
{
  GObjectClass *gobject_class;

  thunar_icon_factory_store_quark = g_quark_from_static_string ("thunar-icon-factory-store");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_icon_factory_dispose;
  gobject_class->finalize = thunar_icon_factory_finalize;
  gobject_class->get_property = thunar_icon_factory_get_property;
  gobject_class->set_property = thunar_icon_factory_set_property;

  /**
   * ThunarIconFactory:icon-theme:
   *
   * The #GtkIconTheme on which the given #ThunarIconFactory instance operates
   * on.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_THEME,
                                   g_param_spec_object ("icon-theme",
                                                        "icon-theme",
                                                        "icon-theme",
                                                        GTK_TYPE_ICON_THEME,
                                                        EXO_PARAM_READABLE));

  /**
   * ThunarIconFactory:thumbnail-mode:
   *
   * Whether this #ThunarIconFactory will try to generate and load thumbnails
   * when loading icons for #ThunarFile<!---->s.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_THUMBNAIL_MODE,
                                   g_param_spec_enum ("thumbnail-mode",
                                                      "thumbnail-mode",
                                                      "thumbnail-mode",
                                                      THUNAR_TYPE_THUMBNAIL_MODE,
                                                      THUNAR_THUMBNAIL_MODE_ONLY_LOCAL,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarIconFactory:thumbnail-draw-frames:
   * 
   * Whether to draw black frames around thumbnails.
   * This looks neat, but will delay the first draw a bit.
   * May have an impact on older systems, on folders with many pictures.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_THUMBNAIL_DRAW_FRAMES,
                                   g_param_spec_boolean ("thumbnail-draw-frames",
                                                         "thumbnail-draw-frames",
                                                         "thumbnail-draw-frames",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarIconFactory:thumbnail-size:
   *
   * Size of the thumbnails to load
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_THUMBNAIL_SIZE,
                                   g_param_spec_enum ("thumbnail-size",
                                                      "thumbnail-size",
                                                      "thumbnail-size",
                                                      THUNAR_TYPE_THUMBNAIL_SIZE,
                                                      THUNAR_THUMBNAIL_SIZE_NORMAL,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarIconFactory:thumbnail-max-file-size:
   *
   * Maximum file size (in bytes) allowed to be thumbnailed
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_THUMBNAIL_MAX_FILE_SIZE,
                                   g_param_spec_uint64 ("thumbnail-max-file-size",
                                                        "thumbnail-max-file-size",
                                                        "thumbnail-max-file-size",
                                                        0, G_MAXUINT64, 0,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_icon_factory_init (ThunarIconFactory *factory)
{
  factory->thumbnail_mode = THUNAR_THUMBNAIL_MODE_ONLY_LOCAL;
  factory->thumbnail_size = THUNAR_THUMBNAIL_SIZE_NORMAL;

  /* connect emission hook for the "changed" signal on the GtkIconTheme class. We use the emission
   * hook way here, because that way we can make sure that the icon cache is definetly cleared
   * before any other part of the application gets notified about the icon theme change.
   */
  factory->changed_hook_id = g_signal_add_emission_hook (g_signal_lookup ("changed", GTK_TYPE_ICON_THEME),
                                                         0, thunar_icon_factory_changed, factory, NULL);

  /* allocate the hash table for the icon cache */
  factory->icon_cache = g_hash_table_new_full (thunar_icon_key_hash, thunar_icon_key_equal,
                                               thunar_icon_key_free, g_object_unref);
}



static void
thunar_icon_factory_dispose (GObject *object)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  _thunar_return_if_fail (THUNAR_IS_ICON_FACTORY (factory));

  if (G_UNLIKELY (factory->sweep_timer_id != 0))
    g_source_remove (factory->sweep_timer_id);

  (*G_OBJECT_CLASS (thunar_icon_factory_parent_class)->dispose) (object);
}



static void
thunar_icon_factory_finalize (GObject *object)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  _thunar_return_if_fail (THUNAR_IS_ICON_FACTORY (factory));

  /* clear the icon cache hash table */
  g_hash_table_destroy (factory->icon_cache);

  /* remove the "changed" emission hook from the GtkIconTheme class */
  g_signal_remove_emission_hook (g_signal_lookup ("changed", GTK_TYPE_ICON_THEME), factory->changed_hook_id);

  /* disconnect from the associated icon theme (if any) */
  if (G_LIKELY (factory->icon_theme != NULL))
    {
      g_object_set_qdata (G_OBJECT (factory->icon_theme), thunar_icon_factory_quark, NULL);
      g_object_unref (G_OBJECT (factory->icon_theme));
    }

  /* disconnect from the preferences */
  g_object_unref (G_OBJECT (factory->preferences));

  (*G_OBJECT_CLASS (thunar_icon_factory_parent_class)->finalize) (object);
}



static void
thunar_icon_factory_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  switch (prop_id)
    {
    case PROP_ICON_THEME:
      g_value_set_object (value, factory->icon_theme);
      break;

    case PROP_THUMBNAIL_MODE:
      g_value_set_enum (value, factory->thumbnail_mode);
      break;

    case PROP_THUMBNAIL_DRAW_FRAMES:
      g_value_set_boolean (value, factory->thumbnail_draw_frames);
      break;

    case PROP_THUMBNAIL_SIZE:
      g_value_set_enum (value, factory->thumbnail_size);
      break;

    case PROP_THUMBNAIL_MAX_FILE_SIZE:
      g_value_set_uint64 (value, factory->thumbnail_max_file_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_icon_factory_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  switch (prop_id)
    {
    case PROP_THUMBNAIL_MODE:
      factory->thumbnail_mode = g_value_get_enum (value);
      break;

    case PROP_THUMBNAIL_DRAW_FRAMES:
      factory->thumbnail_draw_frames = g_value_get_boolean (value);
      break;

    case PROP_THUMBNAIL_SIZE:
      factory->thumbnail_size = g_value_get_enum (value);
      break;

    case PROP_THUMBNAIL_MAX_FILE_SIZE:
      factory->thumbnail_max_file_size = g_value_get_uint64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_icon_factory_changed (GSignalInvocationHint *ihint,
                             guint                  n_param_values,
                             const GValue          *param_values,
                             gpointer               user_data)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (user_data);

  /* drop all items from the icon cache */
  g_hash_table_remove_all (factory->icon_cache);

  /* bump the stamp so all file icons are reloaded */
  factory->theme_stamp++;

  /* keep the emission hook alive */
  return TRUE;
}



static gboolean
thunar_icon_check_sweep (ThunarIconKey *key,
                         GdkPixbuf     *pixbuf)
{
  return (G_OBJECT (pixbuf)->ref_count == 1);
}



static gboolean
thunar_icon_factory_sweep_timer (gpointer user_data)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (user_data);

  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), FALSE);

THUNAR_THREADS_ENTER

  /* ditch all icons whose ref_count is 1 */
  g_hash_table_foreach_remove (factory->icon_cache,
                               (GHRFunc) (void (*)(void)) thunar_icon_check_sweep,
                               factory);

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_icon_factory_sweep_timer_destroy (gpointer user_data)
{
  THUNAR_ICON_FACTORY (user_data)->sweep_timer_id = 0;
}



static inline gboolean
thumbnail_needs_frame (const GdkPixbuf *thumbnail,
                       gint             width,
                       gint             height,
                       gint             size)
{
  const guchar *pixels;
  gint          rowstride;
  gint          n;
  guchar        alpha_threshold = 255u;   /* An alpha value of 255 means full opacity, 0 is fully transparent. */

  /* don't add frames to small thumbnails */
  if (size < THUNAR_ICON_SIZE_64 )
    return FALSE;

  /* always add a frame to thumbnails w/o alpha channel */
  if (G_LIKELY (!gdk_pixbuf_get_has_alpha (thumbnail)))
    return TRUE;

  /* get a pointer to the thumbnail data */
  pixels = gdk_pixbuf_get_pixels (thumbnail);

  /* Data is stored in 4 channels (red, green, blue, alpha). We are only interested in channel 4 (alpha) */

  /* check if we have a transparent pixel on the first row */
  for (n = width * 4; n > 0; n -= 4)
    if (pixels[n - 1] < alpha_threshold)
      return FALSE;

  /* determine the rowstride */
  rowstride = gdk_pixbuf_get_rowstride (thumbnail);

  /* skip the first row */
  pixels += rowstride;

  /* check if we have a transparent pixel in the first or last column */
  for (n = height - 2; n > 0; --n, pixels += rowstride)
    if (pixels[3] < alpha_threshold || pixels[width * 4 - 1] < alpha_threshold)
      return FALSE;

  /* check if we have a transparent pixel on the last row */
  for (n = width * 4; n > 0; n -= 4)
    if (pixels[n - 1] < alpha_threshold)
      return FALSE;

  return TRUE;
}



static GdkPixbuf*
thunar_icon_factory_get_thumbnail_frame (void)
{
  GInputStream *stream;
  static GdkPixbuf *frame = NULL;

  if (G_LIKELY (frame != NULL))
     return frame;

  stream = g_resources_open_stream ("/org/xfce/thunar/thumbnail-frame.png", 0, NULL);
  if (G_UNLIKELY (stream != NULL)) {
    frame = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
    g_object_unref (stream);
  }

  return frame;
}



static GdkPixbuf*
thunar_icon_factory_load_from_file (ThunarIconFactory *factory,
                                    const gchar       *path,
                                    gint               size)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *frame;
  GdkPixbuf *tmp;
  gboolean   needs_frame;
  gint       max_width;
  gint       max_height;
  gint       width;
  gint       height;

  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);

  /* try to load the image from the file */
  pixbuf = gdk_pixbuf_new_from_file (path, NULL);
  if (G_LIKELY (pixbuf != NULL))
    {
      /* determine the dimensions of the pixbuf */
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);

      needs_frame = FALSE;
      if (factory->thumbnail_draw_frames)
        {
          /* check if we want to add a frame to the image (we really don't
           * want to do this for icons displayed in the details view).
           * */
          needs_frame = (strstr (path, G_DIR_SEPARATOR_S ".cache/thumbnails" G_DIR_SEPARATOR_S) != NULL)
                && (size >= 32) && thumbnail_needs_frame (pixbuf, width, height, size);
        }

      /* be sure to make framed thumbnails fit into the size */
      if (G_LIKELY (needs_frame))
        {
          max_width = size - (3 + 6);
          max_height = size - (3 + 6);
        }
      else
        {
          max_width = size;
          max_height = size;
        }

      /* scale down the icon (if required) */
      if (G_LIKELY (width > max_width || height > max_height))
        {
          /* scale down to the required size */
          tmp = exo_gdk_pixbuf_scale_down (pixbuf, TRUE, MAX (1, max_height), MAX (1, max_height));
          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = tmp;
        }

      /* add a frame around thumbnail (large) images */
      if (G_LIKELY (needs_frame))
        {
          /* add a frame to the thumbnail */
          frame = thunar_icon_factory_get_thumbnail_frame ();
          tmp = exo_gdk_pixbuf_frame (pixbuf, frame, 4, 3, 5, 6);
          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = tmp;
        }
    }

  return pixbuf;
}



static GdkPixbuf*
thunar_icon_factory_lookup_icon (ThunarIconFactory *factory,
                                 const gchar       *name,
                                 gint               size,
                                 gboolean           wants_default)
{
  ThunarIconKey  lookup_key;
  ThunarIconKey *key;
  GtkIconInfo   *icon_info;
  GdkPixbuf     *pixbuf = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  _thunar_return_val_if_fail (name != NULL && *name != '\0', NULL);
  _thunar_return_val_if_fail (size > 0, NULL);

  /* prepare the lookup key */
  lookup_key.name = (gchar *) name;
  lookup_key.size = size;

  /* check if we already have a cached version of the icon */
  if (!g_hash_table_lookup_extended (factory->icon_cache, &lookup_key, NULL, (gpointer) &pixbuf))
    {
      /* check if we have to load a file instead of a themed icon */
      if (G_UNLIKELY (g_path_is_absolute (name)))
        {
          /* load the file directly */
          pixbuf = thunar_icon_factory_load_from_file (factory, name, size);
        }
      else
        {
          /* FIXME: is there a better approach? */
          if (g_strcmp0 (name, "inode-directory") == 0)
            name = "folder";

          /* check if the icon theme contains an icon of that name */
          icon_info = gtk_icon_theme_lookup_icon (factory->icon_theme, name, size, GTK_ICON_LOOKUP_FORCE_SIZE);
          if (G_LIKELY (icon_info != NULL))
            {
              /* try to load the pixbuf from the icon info */
              pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

              /* cleanup */
              g_object_unref (icon_info);
            }
        }

      /* use fallback icon if no pixbuf could be loaded */
      if (G_UNLIKELY (pixbuf == NULL))
        {
          /* check if we are allowed to return the fallback icon */
          if (!wants_default)
            return NULL;
          else
            return thunar_icon_factory_load_fallback (factory, size);
        }

      /* generate a key for the new cached icon */
      key = g_slice_new (ThunarIconKey);
      key->size = size;
      key->name = g_strdup (name);

      /* insert the new icon into the cache */
      g_hash_table_insert (factory->icon_cache, key, pixbuf);
    }

  /* schedule the sweeper */
  if (G_UNLIKELY (factory->sweep_timer_id == 0))
    {
      factory->sweep_timer_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, THUNAR_ICON_FACTORY_SWEEP_TIMEOUT,
                                                            thunar_icon_factory_sweep_timer, factory,
                                                            thunar_icon_factory_sweep_timer_destroy);
    }

  return GDK_PIXBUF (g_object_ref (G_OBJECT (pixbuf)));
}



static guint
thunar_icon_key_hash (gconstpointer data)
{
  const ThunarIconKey *key = data;
  const gchar         *p;
  guint                h;

  h = (guint) key->size << 5;

  for (p = key->name; *p != '\0'; ++p)
    h = (h << 5) - h + *p;

  return h;
}



static gboolean
thunar_icon_key_equal (gconstpointer a,
                       gconstpointer b)
{
  const ThunarIconKey *a_key = a;
  const ThunarIconKey *b_key = b;

  /* compare sizes first */
  if (a_key->size != b_key->size)
    return FALSE;

  /* do a full string comparison on the names */
  return (g_strcmp0 (a_key->name, b_key->name) == 0);
}



static void
thunar_icon_key_free (gpointer data)
{
  ThunarIconKey *key = data;

  g_free (key->name);
  g_slice_free (ThunarIconKey, key);
}



static void
thunar_icon_store_free (gpointer data)
{
  ThunarIconStore *store = data;

  if (store->icon != NULL)
    g_object_unref (store->icon);
  g_slice_free (ThunarIconStore, store);
}



static GdkPixbuf*
thunar_icon_factory_load_fallback (ThunarIconFactory *factory,
                                   gint               size)
{
  return thunar_icon_factory_lookup_icon (factory, "text-x-generic", size, FALSE);
}



/**
 * thunar_icon_factory_get_default:
 *
 * Returns the #ThunarIconFactory that operates on the default #GtkIconTheme.
 * The default #ThunarIconFactory instance will be around for the time the
 * programs runs, starting with the first call to this function.
 *
 * The caller is responsible to free the returned object using
 * g_object_unref() when no longer needed.
 *
 * Return value: the #ThunarIconFactory for the default icon theme.
 **/
ThunarIconFactory*
thunar_icon_factory_get_default (void)
{
  static ThunarIconFactory *factory = NULL;

  if (G_UNLIKELY (factory == NULL))
    {
      factory = thunar_icon_factory_get_for_icon_theme (gtk_icon_theme_get_default ());
      g_object_add_weak_pointer (G_OBJECT (factory), (gpointer) &factory);
    }
  else
    {
      g_object_ref (G_OBJECT (factory));
    }

  return factory;
}



/**
 * thunar_icon_factory_get_for_icon_theme:
 * @icon_theme : a #GtkIconTheme instance.
 *
 * Determines the proper #ThunarIconFactory to be used with the specified
 * @icon_theme and returns it.
 *
 * You need to explicitly free the returned #ThunarIconFactory object
 * using g_object_unref() when you are done with it.
 *
 * Return value: the #ThunarIconFactory for @icon_theme.
 **/
ThunarIconFactory*
thunar_icon_factory_get_for_icon_theme (GtkIconTheme *icon_theme)
{
  ThunarIconFactory *factory;

  _thunar_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* generate the quark on-demand */
  if (G_UNLIKELY (thunar_icon_factory_quark == 0))
    thunar_icon_factory_quark = g_quark_from_static_string ("thunar-icon-factory");

  /* check if the given icon theme already knows about an icon factory */
  factory = g_object_get_qdata (G_OBJECT (icon_theme), thunar_icon_factory_quark);
  if (G_UNLIKELY (factory == NULL))
    {
      /* allocate a new factory and connect it to the icon theme */
      factory = g_object_new (THUNAR_TYPE_ICON_FACTORY, NULL);
      factory->icon_theme = GTK_ICON_THEME (g_object_ref (G_OBJECT (icon_theme)));
      g_object_set_qdata (G_OBJECT (factory->icon_theme), thunar_icon_factory_quark, factory);

      /* connect the properties of the global preference */
      factory->preferences = thunar_preferences_get ();
      g_object_bind_property (G_OBJECT (factory->preferences), "misc-thumbnail-mode",
                              G_OBJECT (factory),              "thumbnail-mode",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (G_OBJECT (factory->preferences), "misc-thumbnail-max-file-size",
                              G_OBJECT (factory),              "thumbnail-max-file-size",
                              G_BINDING_SYNC_CREATE);
    }
  else
    {
      g_object_ref (G_OBJECT (factory));
    }

  return factory;
}



/**
 * thunar_icon_factory_get_show_thumbnail:
 * @factory       : a #ThunarIconFactory instance.
 * @file          : a #ThunarFile.
 *
 * Return value: if a Thumbnail show be shown for @file.
 **/
gboolean
thunar_icon_factory_get_show_thumbnail (const ThunarIconFactory *factory,
                                        const ThunarFile        *file)
{
  GFilesystemPreviewType preview;

  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), THUNAR_THUMBNAIL_MODE_NEVER);
  _thunar_return_val_if_fail (file == NULL || THUNAR_IS_FILE (file), THUNAR_THUMBNAIL_MODE_NEVER);

  if (file == NULL
      || factory->thumbnail_mode == THUNAR_THUMBNAIL_MODE_NEVER)
    return FALSE;

  /* always create thumbs for local files */
  if (thunar_file_is_local (file))
    return TRUE;

  preview = thunar_file_get_preview_type (file);

  /* file system says to never thumbnail anything */
  if (preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
    return FALSE;

  /* only if the setting is local and the fs reports to be local */
  if (factory->thumbnail_mode == THUNAR_THUMBNAIL_MODE_ONLY_LOCAL)
    return preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL;

  /* THUNAR_THUMBNAIL_MODE_ALWAYS */
  return TRUE;
}



/**
 * thunar_icon_factory_load_icon:
 * @factory       : a #ThunarIconFactory instance.
 * @name          : name of the icon to load.
 * @size          : desired icon size.
 * @wants_default : %TRUE to return the fallback icon if no icon of @name
 *                  is found in the @factory.
 *
 * Looks up the icon named @name in the icon theme associated with @factory
 * and returns a pixbuf for the icon at the given @size. This function will
 * return a default fallback icon if the requested icon could not be found
 * and @wants_default is %TRUE, else %NULL will be returned in that case.
 *
 * Call g_object_unref() on the returned pixbuf when you are
 * done with it.
 *
 * Return value: the pixbuf for the icon named @name at @size.
 **/
GdkPixbuf*
thunar_icon_factory_load_icon (ThunarIconFactory        *factory,
                               const gchar              *name,
                               gint                      size,
                               gboolean                  wants_default)
{
  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  _thunar_return_val_if_fail (size > 0, NULL);

  /* cannot happen unless there's no XSETTINGS manager
   * for the default screen, but just in case...
   */
  if (G_UNLIKELY (xfce_str_is_empty (name)))
    {
      /* check if the caller will happly accept the fallback icon */
      if (G_LIKELY (wants_default))
        return thunar_icon_factory_load_fallback (factory, size);
      else
        return NULL;
    }

  /* lookup the icon */
  return thunar_icon_factory_lookup_icon (factory, name, size, wants_default);
}



/**
 * thunar_icon_factory_load_file_icon:
 * @factory    : a #ThunarIconFactory instance.
 * @file       : a #ThunarFile.
 * @icon_state : the desired icon state.
 * @icon_size  : the desired icon size.
 *
 * The caller is responsible to free the returned object using
 * g_object_unref() when no longer needed.
 *
 * Return value: the #GdkPixbuf icon.
 **/
GdkPixbuf*
thunar_icon_factory_load_file_icon (ThunarIconFactory  *factory,
                                    ThunarFile         *file,
                                    ThunarFileIconState icon_state,
                                    gint                icon_size)
{
  GInputStream    *stream;
  GtkIconInfo     *icon_info;
  const gchar     *thumbnail_path;
  GdkPixbuf       *icon = NULL;
  GIcon           *gicon;
  const gchar     *icon_name;
  const gchar     *custom_icon;
  ThunarIconStore *store;

  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (icon_size > 0, NULL);

  /* check if we have a stored icon on the file and it is still valid */
  store = g_object_get_qdata (G_OBJECT (file), thunar_icon_factory_store_quark);
  if (store != NULL
      && store->icon_state == icon_state
      && store->icon_size == icon_size
      && store->stamp == factory->theme_stamp
      && store->thumb_state == thunar_file_get_thumb_state (file))
    {
      return g_object_ref (store->icon);
    }

  /* check if we have a custom icon for this file */
  custom_icon = thunar_file_get_custom_icon (file);
  if (custom_icon != NULL)
    {
      /* try to load the icon */
      icon = thunar_icon_factory_lookup_icon (factory, custom_icon, icon_size, FALSE);
      if (G_LIKELY (icon != NULL))
        return icon;
    }

  /* check if thumbnails are enabled and we can display a thumbnail for the item */
  if (thunar_icon_factory_get_show_thumbnail (factory, file))
    {
      /* determine the preview icon first */
      gicon = thunar_file_get_preview_icon (file);

      /* check if we have a preview icon */
      if (gicon != NULL)
        {
          if (G_IS_THEMED_ICON (gicon))
            {
              /* we have a themed preview icon, look it up using the icon theme */
              icon_info =
                gtk_icon_theme_lookup_by_gicon (factory->icon_theme,
                                                gicon, icon_size,
                                                GTK_ICON_LOOKUP_USE_BUILTIN
                                                | GTK_ICON_LOOKUP_FORCE_SIZE);

              /* check if the lookup succeeded */
              if (icon_info != NULL)
                {
                  /* try to load the pixbuf from the icon info */
                  icon = gtk_icon_info_load_icon (icon_info, NULL);
                  g_object_unref (icon_info);
                }
            }
          else if (G_IS_LOADABLE_ICON (gicon))
            {
              /* we have a loadable icon, try to open it for reading */
              stream = g_loadable_icon_load (G_LOADABLE_ICON (gicon), icon_size,
                                             NULL, NULL, NULL);

              /* check if we have a valid input stream */
              if (stream != NULL)
                {
                  /* load the pixbuf from the stream */
                  icon = gdk_pixbuf_new_from_stream_at_scale (stream, icon_size,
                                                              icon_size, TRUE,
                                                              NULL, NULL);

                  /* destroy the stream */
                  g_object_unref (stream);
                }
            }

          /* return the icon if we have one */
          if (icon != NULL)
            return icon;
        }
      else
        {
          if (thunar_file_get_size (file) < factory->thumbnail_max_file_size || factory->thumbnail_max_file_size == 0) /* 0 = unlimited */
            {
              /* we have no preview icon but the thumbnail should be ready. determine
               * the filename of the thumbnail */
              thumbnail_path = thunar_file_get_thumbnail_path (file, factory->thumbnail_size);

              /* check if we have a valid path */
              if (thumbnail_path != NULL)
                {
                  /* try to load the thumbnail */
                  icon = thunar_icon_factory_load_from_file (factory, thumbnail_path, icon_size);
                }
            }
        }
    }

  /* lookup the icon name for the icon in the given state and load the icon */
  if (G_LIKELY (icon == NULL))
    {
      icon_name = thunar_file_get_icon_name (file, icon_state, factory->icon_theme);
      icon = thunar_icon_factory_load_icon (factory, icon_name, icon_size, TRUE);
    }

  if (G_LIKELY (icon != NULL))
    {
      store = g_slice_new (ThunarIconStore);
      store->icon_size = icon_size;
      store->icon_state = icon_state;
      store->stamp = factory->theme_stamp;
      store->thumb_state = thunar_file_get_thumb_state (file);
      store->icon = g_object_ref (icon);

      g_object_set_qdata_full (G_OBJECT (file), thunar_icon_factory_store_quark,
                               store, thunar_icon_store_free);
    }

  return icon;
}



/**
 * thunar_icon_factory_clear_pixmap_cache:
 * @file : a #ThunarFile.
 *
 * Unset the pixmap cache on a file to force a reload on the next request.
 **/
void
thunar_icon_factory_clear_pixmap_cache (ThunarFile *file)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* unset the data */
  if (thunar_icon_factory_store_quark != 0)
    g_object_set_qdata (G_OBJECT (file), thunar_icon_factory_store_quark, NULL);
}
