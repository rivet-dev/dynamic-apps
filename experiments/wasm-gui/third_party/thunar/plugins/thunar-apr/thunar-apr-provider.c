/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-apr/thunar-apr-desktop-page.h>
#include <thunar-apr/thunar-apr-image-page.h>
#include <thunar-apr/thunar-apr-provider.h>



static void   thunar_apr_provider_property_page_provider_init (ThunarxPropertyPageProviderIface *iface);
static GList *thunar_apr_provider_get_pages                   (ThunarxPropertyPageProvider      *property_page_provider,
                                                               GList                            *files);



struct _ThunarAprProviderClass
{
  GObjectClass __parent__;
};

struct _ThunarAprProvider
{
  GObject __parent__;
};



THUNARX_DEFINE_TYPE_WITH_CODE (ThunarAprProvider,
                               thunar_apr_provider,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_PROPERTY_PAGE_PROVIDER,
                                                            thunar_apr_provider_property_page_provider_init));



static void
thunar_apr_provider_class_init (ThunarAprProviderClass *klass)
{
}



static void
thunar_apr_provider_property_page_provider_init (ThunarxPropertyPageProviderIface *iface)
{
  iface->get_pages = thunar_apr_provider_get_pages;
}



static void
thunar_apr_provider_init (ThunarAprProvider *apr_provider)
{
}



static GList*
thunar_apr_provider_get_pages (ThunarxPropertyPageProvider *property_page_provider,
                               GList                       *files)
{
  GSList *formats;
  GSList *lp;
  gchar **mime_types;
  gchar  *scheme;
  GList  *pages = NULL;
  gint    n;

  /* we can handle only property pages for a single file */
  if (G_UNLIKELY (files == NULL || files->next != NULL))
    return NULL;

  /* determine the URI scheme of the file (works only for local files) */
  scheme = thunarx_file_info_get_uri_scheme (files->data);
  if (G_LIKELY (strcmp (scheme, "file") == 0))
    {
      /* ThunarAprDesktopPage case */
      if (G_LIKELY (pages == NULL))
        {
          /* check if we have a .desktop file here */
          if (thunarx_file_info_has_mime_type (files->data, "application/x-desktop"))
            pages = g_list_append (pages, g_object_new (THUNAR_APR_TYPE_DESKTOP_PAGE, "file", files->data, NULL));
        }

      /* ThunarAprImagePage case */
      if (G_LIKELY (pages == NULL))
        {
          /* determine the supported GdkPixbuf formats */
          formats = gdk_pixbuf_get_formats ();
          for (lp = formats; lp != NULL && pages == NULL; lp = lp->next)
            {
              /* check if any of the mime types of this format matches */
              mime_types = gdk_pixbuf_format_get_mime_types (lp->data);
              for (n = 0; mime_types[n] != NULL && pages == NULL; ++n)
                if (thunarx_file_info_has_mime_type (files->data, mime_types[n]))
                  pages = g_list_append (pages, g_object_new (THUNAR_APR_TYPE_IMAGE_PAGE, "file", files->data, NULL));
              g_strfreev (mime_types);
            }
          g_slist_free (formats);
        }
    }
  g_free (scheme);

  return pages;
}

