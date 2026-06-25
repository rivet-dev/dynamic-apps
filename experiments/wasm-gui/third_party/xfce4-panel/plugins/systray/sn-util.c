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

#include "sn-util.h"



static void                  sn_weak_handler_destroy_data            (gpointer                 data,
                                                                      GObject                 *where_the_object_was);

static void                  sn_weak_handler_destroy_instance        (gpointer                 data,
                                                                      GObject                 *where_the_object_was);



typedef struct
{
  gpointer instance;
  gpointer data;
  gulong   handler;
}
WeakHandler;



static void
sn_weak_handler_destroy_data (gpointer  data,
                              GObject  *where_the_object_was)
{
  WeakHandler *weak_handler = data;

  g_signal_handler_disconnect (weak_handler->instance,
                               weak_handler->handler);
  g_object_weak_unref (G_OBJECT (weak_handler->instance),
                       sn_weak_handler_destroy_instance,
                       weak_handler);

  g_free (data);
}



static void
sn_weak_handler_destroy_instance (gpointer  data,
                                  GObject  *where_the_object_was)
{
  WeakHandler *weak_handler = data;

  g_object_weak_unref (G_OBJECT (weak_handler->data),
                       sn_weak_handler_destroy_data,
                       weak_handler);

  g_free (data);
}



static gulong
sn_signal_connect_weak_internal (gpointer      instance,
                                 const gchar  *detailed_signal,
                                 GCallback     c_handler,
                                 gpointer      data,
                                 GConnectFlags connect_flags)
{
  gulong       handler;
  WeakHandler *weak_handler;

  g_return_val_if_fail (G_IS_OBJECT (data), 0);

  handler = g_signal_connect_data (instance, detailed_signal,
                                   c_handler, data,
                                   NULL, connect_flags);

  if (handler != 0 && instance != data)
    {
      weak_handler = g_new0 (WeakHandler, 1);
      weak_handler->instance = instance;
      weak_handler->data = data;
      weak_handler->handler = handler;

      g_object_weak_ref (G_OBJECT (data),
                         sn_weak_handler_destroy_data,
                         weak_handler);
      g_object_weak_ref (G_OBJECT (instance),
                         sn_weak_handler_destroy_instance,
                         weak_handler);
    }

  return handler;
}



gulong
sn_signal_connect_weak (gpointer     instance,
                        const gchar *detailed_signal,
                        GCallback    c_handler,
                        gpointer     data)
{
  return sn_signal_connect_weak_internal (instance, detailed_signal,
                                          c_handler, data, 0);
}



gulong
sn_signal_connect_weak_swapped (gpointer     instance,
                                const gchar *detailed_signal,
                                GCallback    c_handler,
                                gpointer     data)
{
  return sn_signal_connect_weak_internal (instance, detailed_signal,
                                          c_handler, data, G_CONNECT_SWAPPED);
}



static void
sn_container_has_children_callback (GtkWidget *widget,
                                    gpointer   user_data)
{
  gboolean *has_children = user_data;
  *has_children = TRUE;
}



gboolean
sn_container_has_children (GtkWidget *widget)
{
  gboolean has_children = FALSE;

  if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_foreach (GTK_CONTAINER (widget),
                             sn_container_has_children_callback,
                             &has_children);
    }

  return has_children;
}
