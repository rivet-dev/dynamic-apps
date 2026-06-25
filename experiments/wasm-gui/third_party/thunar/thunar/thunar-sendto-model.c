/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#include <thunar/thunar-private.h>
#include <thunar/thunar-sendto-model.h>



static void thunar_sendto_model_finalize   (GObject                *object);
static void thunar_sendto_model_load       (ThunarSendtoModel      *sendto_model);
static void thunar_sendto_model_event      (GFileMonitor           *monitor,
                                            GFile                  *file,
                                            GFile                  *other_file,
                                            GFileMonitorEvent       event_type,
                                            gpointer                user_data);



struct _ThunarSendtoModelClass
{
  GObjectClass __parent__;
};

struct _ThunarSendtoModel
{
  GObject __parent__;
  GList  *monitors;
  GList  *handlers;
  guint   loaded : 1;
};



G_DEFINE_TYPE (ThunarSendtoModel, thunar_sendto_model, G_TYPE_OBJECT)



static void
thunar_sendto_model_class_init (ThunarSendtoModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_sendto_model_finalize;
}



static void
thunar_sendto_model_init (ThunarSendtoModel *sendto_model)
{
  sendto_model->monitors = NULL;
}



static void
thunar_sendto_model_finalize (GObject *object)
{
  ThunarSendtoModel *sendto_model = THUNAR_SENDTO_MODEL (object);
  GList             *lp;

  /* release the handlers */
  g_list_free_full (sendto_model->handlers, g_object_unref);

  /* disconnect all monitors */
  for (lp = sendto_model->monitors; lp != NULL; lp = lp->next)
    {
      g_file_monitor_cancel (lp->data);
      g_object_unref (lp->data);
    }
  g_list_free (sendto_model->monitors);

  (*G_OBJECT_CLASS (thunar_sendto_model_parent_class)->finalize) (object);
}



static gint
g_app_info_compare (gpointer a,
                    gpointer b)
{
  return g_utf8_collate (g_app_info_get_name (b),
                         g_app_info_get_name (a));
}



static void
thunar_sendto_model_load (ThunarSendtoModel *sendto_model)
{
#ifdef HAVE_GIO_UNIX
  GDesktopAppInfo *app_info = NULL;
#endif
  gchar          **specs;
  gchar           *path;
  guint            n;
  GKeyFile        *key_file;
  gchar          **mime_types;

  /* lookup all sendto .desktop files */
  specs = xfce_resource_match (XFCE_RESOURCE_DATA, "Thunar/sendto/*.desktop", TRUE);
  for (n = 0; specs[n] != NULL; ++n)
    {
      /* lookup the absolute path to the .desktop file */
      path = xfce_resource_lookup (XFCE_RESOURCE_DATA, specs[n]);
      if (G_LIKELY (path != NULL))
        {
          /* try to load the .desktop file */
          key_file = g_key_file_new ();
          if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL))
            {
              g_key_file_free (key_file);
              continue;
            }

#ifdef HAVE_GIO_UNIX
          app_info = g_desktop_app_info_new_from_keyfile (key_file);

          if (G_LIKELY (app_info != NULL))
            {
              /* add to our handler list, sorted by their desktop-ids (reverse order) */
              sendto_model->handlers = g_list_insert_sorted (sendto_model->handlers,
                                                             G_APP_INFO (app_info),
                                                             (GCompareFunc) (void (*)(void)) g_app_info_compare);

              /* attach the mime-types to the object */
              mime_types = g_key_file_get_string_list (key_file,
                                                       G_KEY_FILE_DESKTOP_GROUP,
                                                       G_KEY_FILE_DESKTOP_KEY_MIME_TYPE,
                                                       NULL, NULL);
              if (mime_types != NULL)
                g_object_set_data_full (G_OBJECT (app_info), "mime-types", mime_types, (GDestroyNotify) g_strfreev);
            }
#else
          /* FIXME try to create the app info ourselves in a platform independent way */
#endif

          g_key_file_free (key_file);
        }

      /* cleanup */
      g_free (specs[n]);
      g_free (path);
    }
  g_free (specs);
}



static void
thunar_sendto_model_event (GFileMonitor     *monitor,
                           GFile            *file,
                           GFile            *other_file,
                           GFileMonitorEvent event_type,
                           gpointer          user_data)
{
  ThunarSendtoModel *sendto_model = THUNAR_SENDTO_MODEL (user_data);

  /* release the previously loaded handlers */
  if (G_LIKELY (sendto_model->handlers != NULL))
    {
      g_list_free_full (sendto_model->handlers, g_object_unref);
      sendto_model->handlers = NULL;
    }

  /* reload the handlers for the model */
  thunar_sendto_model_load (sendto_model);
}



/**
 * thunar_sendto_model_get_default:
 *
 * Returns a reference to the default #ThunarSendtoModel
 * instance. The caller is responsible to free the returned
 * reference using g_object_unref() when no longer needed.
 *
 * Return value: reference to the default sendto model.
 **/
ThunarSendtoModel*
thunar_sendto_model_get_default (void)
{
  static ThunarSendtoModel *sendto_model = NULL;

  if (G_UNLIKELY (sendto_model == NULL))
    {
      sendto_model = g_object_new (THUNAR_TYPE_SENDTO_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (sendto_model), (gpointer) &sendto_model);
    }
  else
    {
      g_object_ref (G_OBJECT (sendto_model));
    }

  return sendto_model;
}



/**
 * thunar_sendto_model_get_matching:
 * @sendto_model : a #ThunarSendtoModel.
 * @files        : a #GList of #ThunarFile<!---->s.
 *
 * Returns the list of #GAppInfo<!---->s for the "Send To" targets that
 * support the specified @files.
 *
 * The returned list is owned by the caller and must be freed when no
 * longer needed, using:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: a #GList of supported #GAppInfo<!---->s as
 *               "Send To" targets for the specified @files.
 **/
GList*
thunar_sendto_model_get_matching (ThunarSendtoModel *sendto_model,
                                  GList             *files)
{
  GFileMonitor *monitor;
  GFile        *file;
  gchar       **datadirs;
  gchar        *dir;
  GList        *handlers = NULL;
  GList        *hp;
  GList        *fp;
  guint         n;
  const gchar **mime_types;
  const gchar  *content_type;

  _thunar_return_val_if_fail (THUNAR_IS_SENDTO_MODEL (sendto_model), NULL);

  /* no files, no sendto actions */
  if (G_UNLIKELY (files == NULL))
    return NULL;

  /* connect to the monitor on-demand */
  if (G_UNLIKELY (sendto_model->monitors == NULL))
    {
      /* watch all possible sendto directories */
      datadirs = xfce_resource_dirs (XFCE_RESOURCE_DATA);
      for (n = 0; datadirs[n] != NULL; ++n)
        {
          /* determine the path to the sendto directory */
          dir = g_build_filename (datadirs[n], "Thunar", "sendto", NULL);
          file = g_file_new_for_path (dir);

          /* watch the directory for changes */
          monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
          if (G_LIKELY (monitor != NULL))
            {
              g_signal_connect (monitor, "changed", G_CALLBACK (thunar_sendto_model_event), sendto_model);
              sendto_model->monitors = g_list_prepend (sendto_model->monitors, monitor);
            }

          g_object_unref (file);
          g_free (dir);
        }
      g_strfreev (datadirs);

      /* load the model */
      thunar_sendto_model_load (sendto_model);
    }

  /* test all handlers */
  for (hp = sendto_model->handlers; hp != NULL; hp = hp->next)
    {
      /* FIXME Ignore GAppInfos which don't support multiple file arguments */

      /* ignore the handler if it doesn't support URIs, but we don't have a local file */
      if (!g_app_info_supports_uris (hp->data))
        {
          /* check if we have any non-local files */
          for (fp = files; fp != NULL; fp = fp->next)
            if (!thunar_file_is_local (fp->data))
              break;

          /* check if the test failed */
          if (G_UNLIKELY (fp != NULL))
            continue;
        }

      /* check if we need to test mime types for this handler */
      mime_types = g_object_get_data (G_OBJECT (hp->data), "mime-types");
      if (mime_types != NULL)
        {
          /* each file must match atleast one of the specified mime types */
          for (fp = files; fp != NULL; fp = fp->next)
            {
              /* each file must be supported by one of the mime types */
              for (n = 0; mime_types[n] != NULL; ++n)
                {
                  content_type = thunar_file_get_content_type (fp->data);
                  if (g_content_type_equals (content_type, mime_types[n]))
                    break;
                }

              /* check if all mime types failed */
              if (mime_types[n] == NULL)
                break;
            }

          /* check if the test failed */
          if (G_UNLIKELY (fp != NULL))
            continue;
        }

      /* the handler is supported */
      handlers = g_list_prepend (handlers, g_object_ref (G_OBJECT (hp->data)));
    }

  return handlers;
}


