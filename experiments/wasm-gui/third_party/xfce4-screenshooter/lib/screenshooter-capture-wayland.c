/*  $Id$
 *
 *  Copyright © 2024 André Miranda <andreldm@xfce.org>
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

#include "screenshooter-capture-wayland.h"

#include <sys/mman.h>

#include <gdk/gdkwayland.h>
#include <protocols/wlr-screencopy-unstable-v1-client.h>
#include <libxfce4util/libxfce4util.h>

#include "screenshooter-global.h"
#include "screenshooter-utils.h"



/* Internals */


typedef struct {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct zwlr_screencopy_manager_v1 *screencopy_manager;
}
ClientData;

typedef struct {
  ClientData *client_data;
  GdkMonitor *monitor;
  struct zwlr_screencopy_frame_v1 *frame;
  struct wl_buffer *buffer;
  struct wl_shm_pool *pool;
  unsigned char *shm_data;
  int width;
  int height;
  int stride;
  int size;
  uint32_t format;
  gboolean capture_done;
  gboolean capture_failed;
}
OutputData;



static void handle_global (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
static void handle_global_remove (void *data, struct wl_registry *reg, uint32_t name);
static void handle_frame_ready (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
static void handle_frame_failed (void *data, struct zwlr_screencopy_frame_v1 *frame);
static void handle_frame_flags (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags);
static void handle_frame_buffer (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t fmt, uint32_t w, uint32_t h, uint32_t str);
static void screenshooter_free_client_data (ClientData *client_data);
static void screenshooter_free_output_data (gpointer data);
static gboolean screenshooter_initialize_client_data (ClientData *client_data);
static GdkPixbuf *screenshooter_compose_screenshot (GList *outputs);



void
screenshooter_free_client_data (ClientData *client_data)
{
  if (client_data->compositor != NULL)
    wl_compositor_destroy (client_data->compositor);
  if (client_data->shm != NULL)
    wl_shm_destroy (client_data->shm);
  if (client_data->screencopy_manager != NULL)
    zwlr_screencopy_manager_v1_destroy (client_data->screencopy_manager);

  wl_registry_destroy (client_data->registry);
  /* do not destroy client_data->display because it is owned by gdk */
}



void
screenshooter_free_output_data (gpointer data)
{
  OutputData *output = data;

  if (output->shm_data != NULL)
    munmap (output->shm_data, output->size);
  if (output->buffer != NULL)
    wl_buffer_destroy (output->buffer);
  if (output->frame != NULL)
    zwlr_screencopy_frame_v1_destroy (output->frame);

  g_free (output);
}



/* global registry functions */



static void
handle_global (void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
  ClientData *client_data = data;

  if (g_strcmp0 (interface, wl_compositor_interface.name) == 0)
    client_data->compositor = wl_registry_bind (wl_registry, name, &wl_compositor_interface, 1);
  else if (g_strcmp0 (interface, wl_shm_interface.name) == 0)
    client_data->shm = wl_registry_bind (wl_registry, name, &wl_shm_interface, 1);
  else if (g_strcmp0 (interface, zwlr_screencopy_manager_v1_interface.name) == 0)
    client_data->screencopy_manager = wl_registry_bind (wl_registry, name, &zwlr_screencopy_manager_v1_interface, 1);
}



static void
handle_global_remove (void *data, struct wl_registry *reg, uint32_t name)
{
  /* do nothing */
}



const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove
};



/* screencopy frame functions */



static void
handle_frame_ready (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
  OutputData *output = data;

  TRACE ("buffer copy ready");
  output->capture_done = TRUE;
}



static void
handle_frame_failed (void *data, struct zwlr_screencopy_frame_v1 *frame)
{
  OutputData *output = data;

  screenshooter_error (_("Failed to capture screencopy frame"));
  output->capture_failed = TRUE;
}



static void
handle_frame_flags (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags)
{
  TRACE ("buffer flags received");
}



static void
handle_frame_buffer (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
  int fd;
  char template[] = "/tmp/screenshooter-buffer-XXXXXX";
  OutputData *output = data;

  output->format = format;
  output->width = width;
  output->height = height;
  output->stride = stride;
  output->size = stride * height;

  fd = mkstemp (template);
  if (fd == -1)
    {
      screenshooter_error (_("Failed to create file descriptor"));
      g_abort ();
    }
  ftruncate (fd, output->size);
  unlink (template);

  output->shm_data = mmap (NULL, output->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (output->shm_data == MAP_FAILED)
    {
      screenshooter_error (_("Failed to map memory"));
      close (fd);
      g_abort ();
    }

  output->pool = wl_shm_create_pool (output->client_data->shm, fd, output->size);
  output->buffer = wl_shm_pool_create_buffer (output->pool, 0, width, height, stride, format);
  close (fd);
  wl_shm_pool_destroy (output->pool);

  zwlr_screencopy_frame_v1_copy (frame, output->buffer);
  /* wait for flags and ready events */
}



const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .ready = handle_frame_ready,
    .failed = handle_frame_failed,
    .flags = handle_frame_flags,
    .buffer = handle_frame_buffer
};



/* Wayland client initialization */



gboolean
screenshooter_initialize_client_data (ClientData *client_data)
{
  client_data->display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
  client_data->registry = wl_display_get_registry (client_data->display);

  wl_registry_add_listener (client_data->registry, &registry_listener, client_data);
  wl_display_roundtrip (client_data->display);

  if (client_data->compositor == NULL)
    {
      screenshooter_error (_("Required Wayland interfaces are missing"));
      return FALSE;
    }
  if (client_data->shm == NULL)
    {
      screenshooter_error (_("Compositor is missing wl_shm"));
      return FALSE;
    }
  if (client_data->screencopy_manager == NULL)
    {
      screenshooter_error (_("Compositor does not support wlr-screencopy-unstable-v1"));
      return FALSE;
    }

  return TRUE;
}



/* Pixbuf manipulation functions */



static GdkPixbuf
*screenshooter_convert_buffer_to_pixbuf (OutputData *output)
{
  guint8 *data = output->shm_data;

  if (output->format == WL_SHM_FORMAT_ARGB8888 || output->format == WL_SHM_FORMAT_XRGB8888)
    {
      for (int y = 0; y < output->height; y++)
        {
          for (int x = 0; x < output->width; x++)
            {
              gint offset = y * output->stride + x * 4;
              guint32 *px = (guint32 *)(gpointer)(data + offset);
              guint8 red = (*px >> 16) & 0xFF;
              guint8 green = (*px >> 8) & 0xFF;
              guint8 blue = *px & 0xFF;
              guint8 alpha = (*px >> 24) & 0xFF;
              data[offset + 0] = red;
              data[offset + 1] = green;
              data[offset + 2] = blue;
              data[offset + 3] = alpha;
            }
        }
    }
  else if (output->format == WL_SHM_FORMAT_ABGR8888 || output->format == WL_SHM_FORMAT_XBGR8888)
    {
      for (int y = 0; y < output->height; y++)
        {
          for (int x = 0; x < output->width; x++)
            {
              gint offset = y * output->stride + x * 4;
              guint32 *px = (guint32 *)(gpointer)(data + offset);
              guint8 red = *px & 0xFF;
              guint8 green = (*px >> 8) & 0xFF;
              guint8 blue = (*px >> 16) & 0xFF;
              guint8 alpha = (*px >> 24) & 0xFF;
              data[offset + 0] = red;
              data[offset + 1] = green; 
              data[offset + 2] = blue;
              data[offset + 3] = alpha;
            }
        }
    }
  else
    {
      screenshooter_error (_("Unsupported pixel format: %d"), output->format);
      return NULL;
    }

  return gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, TRUE, 8, output->width, output->height, output->stride, NULL, NULL);
}



GdkPixbuf
*screenshooter_compose_screenshot (GList *outputs)
{
  GdkRectangle geometry;
  GdkPixbuf *dest = NULL;
  int max_width = 0, max_height = 0;

  /* find the maximum dimensions */
  for (GList *iter = outputs; iter != NULL; iter = iter->next)
    {
      OutputData *output = (OutputData *) iter->data;
      gdk_monitor_get_geometry (output->monitor, &geometry);
      max_width = MAX (max_width, geometry.x + geometry.width);
      max_height = MAX (max_height, geometry.y + geometry.height);
    }

  /* create a new destination pixbuf */
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, max_width, max_height);
  if (dest == NULL)
    {
      g_warning ("Failed to create destination pixbuf.");
      return NULL;
    }

  /* fill with transparency */
  gdk_pixbuf_fill (dest, 0x00000000);

  /* composite each screenshot onto the destination pixbuf */
  for (GList *iter = outputs; iter != NULL; iter = iter->next)
    {
      OutputData *output = (OutputData *) iter->data;
      GdkPixbuf *pixbuf = screenshooter_convert_buffer_to_pixbuf (output);
      gdk_monitor_get_geometry (output->monitor, &geometry);
      gdk_pixbuf_composite (pixbuf, dest,
                            geometry.x, geometry.y,
                            geometry.width, geometry.height,
                            geometry.x, geometry.y,
                            1.0, 1.0,
                            GDK_INTERP_BILINEAR,
                            255);
      g_object_unref (pixbuf);
    }

  return dest;
}



/* Public */



GdkPixbuf
*screenshooter_capture_screenshot_wayland (gint     region,
                                           gint     delay,
                                           gboolean show_mouse,
                                           gboolean show_border)
{
  int n_monitors;
  gboolean failure = FALSE;
  GList *outputs = NULL;
  ClientData client_data = {};
  GdkPixbuf *screenshot = NULL;

  if (region != FULLSCREEN)
    {
      screenshooter_error (_("The selected mode is not supported in Wayland"));
      return NULL;
    }

  /* only fullscreen is supported for now */
  TRACE ("Get the screenshot of the entire screen");

  /* initializate the wayland client  */
  if (!screenshooter_initialize_client_data (&client_data))
    {
      screenshooter_free_client_data (&client_data);
      return NULL;
    }

  /* collect outputs (monitors)  */
  n_monitors = gdk_display_get_n_monitors (gdk_display_get_default ());
  for (int i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor = gdk_display_get_monitor (gdk_display_get_default (), i);
      struct wl_output *wl_output = gdk_wayland_monitor_get_wl_output (monitor);
      if (wl_output == NULL)
        {
          g_warning ("No output available for monitor %d", i);
          continue;
        }

      OutputData *output = g_new0 (OutputData, 1);
      outputs = g_list_append (outputs, output);
      output->monitor = monitor;
      output->client_data = &client_data;
      output->frame = zwlr_screencopy_manager_v1_capture_output (client_data.screencopy_manager, show_mouse, wl_output);
      zwlr_screencopy_frame_v1_add_listener (output->frame, &frame_listener, output);
    }

  /* wait for the capture of all outputs */
  while (TRUE)
    {
      gboolean done = TRUE;

      for (GList *elem = outputs; elem; elem = elem->next)
        {
          OutputData *output = elem->data;
          if (!output->capture_done && !output->capture_failed)
            done = FALSE;
          if (output->capture_failed)
            failure = TRUE;
        }

      if (done)
        break;

      wl_display_dispatch (client_data.display);
    }

  /* check the result */
  if (failure)
    screenshooter_error (_("Failed to capture"));
  else
    screenshot = screenshooter_compose_screenshot (outputs);

  screenshooter_free_client_data (&client_data);
  g_list_free_full (outputs, screenshooter_free_output_data);

  return screenshot;
}
