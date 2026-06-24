/*
 * Copyright (C) 2019 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <gdk/gdkx.h>
#include <sys/utsname.h>

#ifdef HAVE_EPOXY
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <X11/Xlib.h>
#endif

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include "system-info.h"



typedef struct
{
  gboolean is_default;
  gint memory_size_mib;  /* Unit: mebibyte (MiB), zero if unknown */
  char *name;            /* NULL if unknown */
  char *pci_id;          /* "VID:DID", lowercase, VID (DID) is vendor (device) ID, NULL if unknown */
} GPUInfo;

typedef struct
{
  char *regex;
  char *replacement;
} ReplaceStrings;



/* A list of some 64-bit CPU architectures.
 * Source: http://en.wikipedia.org/wiki/Uname */
static const gchar *const arch_64[] = {
  "aarch64",
  "amd64",
  "ia64",
  "loongarch64",
  "ppc64",
  "sparc64",
  "x86_64",
  NULL
};



static char *
prettify_info (const char *info)
{
  g_autofree char *escaped = NULL;
  g_autofree gchar *pretty = NULL;
  guint   i;
  static const ReplaceStrings rs[] = {
    { "Mesa DRI ", ""},
    { "Intel[(]R[)]", "Intel<sup>\302\256</sup>"},
    { "Core[(]TM[)]", "Core<sup>\342\204\242</sup>"},
    { "Atom[(]TM[)]", "Atom<sup>\342\204\242</sup>"},
    { "Gallium .* on (AMD .*)", "\\1"},
    { "(AMD .*) [(][^)]*[)]", "\\1"},
    { "(AMD [A-Z])(.*)", "\\1\\L\\2\\E"},
    { "AMD", "AMD<sup>\302\256</sup>"},
    { "GeForce ", "GeForce<sup>\302\256</sup> "},
    { "GeForce[(]R[)]", "GeForce<sup>\302\256</sup>"},
    { "Radeon ", "Radeon<sup>\342\204\242</sup> "},
    { "Radeon[(]TM[)]", "Radeon<sup>\342\204\242</sup>"},
    { "Graphics Controller", "Graphics"},
  };

  if (*info == '\0')
    return NULL;

  escaped = g_markup_escape_text (info, -1);
  pretty = g_strdup (g_strstrip (escaped));

  for (i = 0; i < G_N_ELEMENTS (rs); i++)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GRegex) re = NULL;
      g_autofree gchar *new = NULL;

      re = g_regex_new (rs[i].regex, 0, 0, &error);
      if (re == NULL)
        {
          g_warning ("Error building regex: %s", error->message);
          continue;
        }

      new = g_regex_replace (re,
                             pretty,
                             -1,
                             0,
                             rs[i].replacement,
                             0,
                             &error);

      if (error != NULL)
        {
          g_warning ("Error replacing %s: %s", rs[i].regex, error->message);
          continue;
        }

      g_free (pretty);
      pretty = g_steal_pointer (&new);
    }

  return g_steal_pointer (&pretty);
}



static char *
remove_duplicate_whitespace (const char *old)
{
  g_autofree gchar *new = NULL;
  g_autoptr(GRegex) re = NULL;
  g_autoptr(GError) error = NULL;

  if (old == NULL)
    return NULL;

  re = g_regex_new ("[ \t\n\r]+", G_REGEX_MULTILINE, 0, &error);
  if (re == NULL)
    {
      g_warning ("Error building regex: %s", error->message);
      return g_strdup (old);
    }
  new = g_regex_replace (re,
                         old,
                         -1,
                         0,
                         " ",
                         0,
                         &error);
  if (new == NULL)
    {
      g_warning ("Error replacing string: %s", error->message);
      return g_strdup (old);
    }

  return g_steal_pointer (&new);
}



static char *
info_cleanup (const char *input)
{
  g_autofree char *pretty;

  pretty = prettify_info (input);
  return remove_duplicate_whitespace (pretty);
}



char *
get_cpu_info (const glibtop_sysinfo *info)
{
  g_autoptr(GHashTable) counts = NULL;
  g_autoptr(GString) cpu = NULL;
  GHashTableIter iter;
  gpointer       key, value;
  guint          i;
  int            j;

  counts = g_hash_table_new (g_str_hash, g_str_equal);

  /* count duplicates */
  for (i = 0; i != info->ncpu; ++i)
    {
      const char * const keys[] = { "model name", "cpu", "Model Name", "Processor" };
      char *model;
      int  *count;

      model = NULL;

      for (j = 0; model == NULL && j != G_N_ELEMENTS (keys); ++j)
        {
          model = g_hash_table_lookup (info->cpuinfo[i].values,
                                       keys[j]);
        }

      if (model == NULL)
          continue;

      count = g_hash_table_lookup (counts, model);
      if (count == NULL)
        g_hash_table_insert (counts, model, GINT_TO_POINTER (1));
      else
        g_hash_table_replace (counts, model, GINT_TO_POINTER (GPOINTER_TO_INT (count) + 1));
    }

  cpu = g_string_new (NULL);
  g_hash_table_iter_init (&iter, counts);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_autofree char *cleanedup = NULL;
      int count;

      count = GPOINTER_TO_INT (value);
      cleanedup = info_cleanup ((const char *) key);
      if (count > 1)
        g_string_append_printf (cpu, "%s \303\227 %d ", cleanedup, count);
      else
        g_string_append_printf (cpu, "%s ", cleanedup);
    }

  return g_strdup (cpu->str);
}




static void
free_gpu_info (gpointer p)
{
  GPUInfo *i = p;
  g_free (i->name);
  g_free (i->pci_id);
  g_free (i);
}

static GList*
append_gpu_info (GList *gpus, GPUInfo *gpu)
{
  gboolean merged = FALSE;

  if (gpu->pci_id)
    {
      GList *l;

      /* Merge the GPU info with GPUs already present in the list */
      for (l = gpus; l;)
        {
          GPUInfo *i = l->data;
          gboolean remove = FALSE;

          if (!i->pci_id)
            {
              /* Remove all GPUs without a PCI ID */
              remove = TRUE;
            }
          else if (strcmp (i->pci_id, gpu->pci_id) == 0)
            {
              i->is_default |= gpu->is_default;
              if (i->memory_size_mib == 0 && gpu->memory_size_mib != 0)
                i->memory_size_mib = gpu->memory_size_mib;
              if (!i->name && gpu->name)
                i->name = g_strdup (gpu->name);
              merged = TRUE;
            }

          if (remove)
            {
              GList *next = l->next;
              gpus = g_list_remove_link (gpus, l);
              g_list_free_1 (l);
              free_gpu_info (i);
              l = next;
            }
          else
            l = l->next;
        }
    }

  if (!merged)
    gpus = g_list_append (gpus, gpu);
  else
    free_gpu_info (gpu);

  return gpus;
}



#ifdef HAVE_GUDEV

static GList*
get_drm_cards (GList *gpus)
{
  const gchar *const subsystems[] = { "drm", NULL };
  GUdevClient *client;
  GList *devices, *l;

  client = g_udev_client_new (subsystems);
  devices = g_udev_client_query_by_subsystem (client, "drm");
  for (l = devices; l != NULL; l = l->next)
    {
      GUdevDevice *d = l->data;
      const char *path = g_udev_device_get_device_file (d);

      if (path && g_str_has_prefix (path, "/dev/dri/render"))
        {
          GUdevDevice *parent;
          const char *property;
          char *cleanedup_property;

          parent = g_udev_device_get_parent (d);

          property = g_udev_device_get_property (parent, "ID_MODEL_FROM_DATABASE");
          if (property && (cleanedup_property = info_cleanup (property)) != NULL)
            {
              GPUInfo *const gpu = g_new0 (GPUInfo, 1);

              gpu->name = cleanedup_property;
              g_strstrip (gpu->name);

              /* Example GPU names before cleanup:
               *   GM107 [GeForce GTX 750]
               *   Navi 14 [Radeon RX 5500/5500M / Pro 5500M]
               * After cleanup:
               *   GeForce GTX 750
               *   Radeon RX 5500/5500M / Pro 5500M
               */
              if (g_regex_match_simple ("^[^\\[]+\\[[^\\]]{4,}\\]$", gpu->name, 0, 0))
                {
                  char *s;
                  gpu->name[strlen (gpu->name) - 1] = '\0';  /* Remove trailing ] */
                  if (G_LIKELY ((s = strrchr (gpu->name, '[')) != NULL))
                    {
                      s = g_strdup (s+1);
                      g_free (gpu->name);
                      gpu->name = s;
                    }
                }

              property = g_udev_device_get_property (parent, "PCI_ID");
              if (property)
                {
                   gpu->pci_id = g_ascii_strdown (property, -1);
                   g_strstrip (gpu->pci_id);
                }

              gpus = append_gpu_info (gpus, gpu);
          }

          g_object_unref (parent);
        }

      g_object_unref (d);
    }

  g_list_free (devices);
  g_object_unref (client);

  return gpus;
}

#endif



/**
 * Returns a string containing all detected GPUs.
 * If the machine has multiple GPUs, they are separated by the newline '\n' character.
 *
 * @num_gpus: where to store the number of detected GPUs, or %NULL.
 */
char*
get_gpu_info (guint *num_gpus)
{
  GList *gpus = NULL;
  gchar *result = NULL;
#ifdef HAVE_EPOXY
  Display *dpy;
#endif

  if (num_gpus)
    *num_gpus = 0;

#ifdef HAVE_EPOXY
  dpy = XOpenDisplay (NULL);
  if (dpy)
  {
    XVisualInfo *visual_info;

    int attrib[] = {
      GLX_RGBA,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      None
    };

    visual_info = glXChooseVisual (dpy, DefaultScreen (dpy), attrib);
    if (visual_info)
    {
      const int screen = DefaultScreen (dpy);
      const Window root_win = RootWindow (dpy, screen);
      XSetWindowAttributes win_attr = {};
      Window win;
      GLXContext ctx;

      win_attr.colormap = XCreateColormap (dpy, root_win, visual_info->visual, AllocNone);
      win = XCreateWindow (dpy, root_win,
                           0, 0, /* x, y */
                           1, 1, /* width, height */
                           0, visual_info->depth, InputOutput,
                           visual_info->visual, CWColormap, &win_attr);

      ctx = glXCreateContext (dpy, visual_info, NULL, True);
      XFree (visual_info);
      visual_info = NULL;

      if (ctx)
      {
	if (glXMakeCurrent (dpy, win, ctx))
        {
          GPUInfo *gpu = g_new0 (GPUInfo, 1);
          gchar *renderer, *cleanedup_renderer;

          gpu->is_default = TRUE;

          renderer = g_strdup ((const gchar*) glGetString (GL_RENDERER));
          if (renderer && (cleanedup_renderer = info_cleanup (renderer)) != NULL)
          {
            gsize length;
            gchar *renderer_lc;
            gboolean strip = TRUE;

            g_free (renderer);
            renderer = cleanedup_renderer;
            length = strlen (renderer);

            /* Return full renderer string in the following cases: */
            renderer_lc = g_ascii_strdown (renderer, length);
            strip = strip && !g_str_has_prefix (renderer_lc, "llvmpipe");
            strip = strip && !g_str_has_prefix (renderer_lc, "softpipe");
            strip = strip && !g_str_has_prefix (renderer_lc, "swr");
            strip = strip && !g_str_has_prefix (renderer_lc, "zink");
            g_free (renderer_lc);
            renderer_lc = NULL;

            if (strip)
            {
              /* End the renderer string before the first parenthesis */
              const gchar *bracket = strchr (renderer, '(');
              if (bracket > renderer)
              {
                length = (gsize) (bracket-renderer);
                for(; length > 0 && g_ascii_isspace (renderer[length-1]); length--);
              }
            }

            gpu->name = g_strndup (renderer, length);
          }
          g_free (renderer);

          if (epoxy_has_glx_extension (dpy, 0, "GLX_MESA_query_renderer"))
          {
            unsigned mem_mib = 0, vendor = 0, device = 0;
            if (glXQueryCurrentRendererIntegerMESA (GLX_RENDERER_VIDEO_MEMORY_MESA, &mem_mib) && mem_mib > 0)
                gpu->memory_size_mib = mem_mib;

            if (glXQueryCurrentRendererIntegerMESA (GLX_RENDERER_VENDOR_ID_MESA, &vendor) &&
                glXQueryCurrentRendererIntegerMESA (GLX_RENDERER_DEVICE_ID_MESA, &device))
              gpu->pci_id = g_strdup_printf ("%04x:%04x", vendor, device);
          }

          if (gpu->memory_size_mib == 0 && epoxy_has_gl_extension ("GL_NVX_gpu_memory_info"))
          {
            GLint mem_kib = 0;
            glGetIntegerv (GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &mem_kib);
            if (mem_kib > 0)
                gpu->memory_size_mib = mem_kib >> 10;
          }

          /* Deallocate ctx immediately when calling glXDestroyContext() */
          glXMakeCurrent (dpy, None, NULL);

          gpus = append_gpu_info (gpus, gpu);
        }
        glXDestroyContext (dpy, ctx);
      }

      XDestroyWindow (dpy, win);
    }

    XCloseDisplay (dpy);
  }
#endif

#ifdef HAVE_GUDEV
  gpus = get_drm_cards (gpus);
#endif

  if (gpus)
    {
      GList *gpu;
      for (gpu = gpus; gpu; gpu = gpu->next)
        {
          GPUInfo *info = (GPUInfo*) gpu->data;
          char *s;

          if (result)
            {
              s = g_strconcat (result, "\n", NULL);
              g_free (result);
              result = s;
            }

          s = g_strconcat (result ? result : "", info->name ? info->name : _("Unknown"), NULL);
          g_free (result);
          result = s;

          if (info->memory_size_mib != 0)
            {
              char *mem = g_format_size_full ((guint64) info->memory_size_mib << 20, G_FORMAT_SIZE_IEC_UNITS);
              s = g_strconcat (result, " (", mem, ")", NULL);
              g_free (mem);
              g_free (result);
              result = s;
            }
        }

      if (num_gpus)
        *num_gpus = g_list_length (gpus);

      g_list_free_full (gpus, free_gpu_info);
      gpus = NULL;
    }

  if (!result)
    result = g_strdup (_("Unknown"));

  return result;
}



/**
 * @string: A string which might be quoted with ' or ".
 *
 * Unquotes the given string if both leading and trailing quotes are present,
 * and if the quotes are of the same type (single or double).
 * The returned string has to be freed with g_free() when no longer needed. 
 *
 * Return value: A newly-allocated, unquoted string.
 */
static gchar *
unquote_string (const gchar *string)
{
  gint size = 0;

  if ((g_str_has_prefix (string, "\"") && g_str_has_suffix (string, "\""))
      || (g_str_has_prefix (string, "\'") && g_str_has_suffix (string, "\'")))
    {
      string += 1;
      size -= 1;
    }

  size += strlen (string);

  return g_strndup (string, size);
}



static GHashTable*
get_os_info (void)
{
  GHashTable *hashtable;
  g_autofree gchar *buffer = NULL;

  hashtable = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
      g_auto(GStrv) lines = NULL;
      gint i;

      lines = g_strsplit (buffer, "\n", -1);

      for (i = 0; lines[i] != NULL; i++)
        {
          gchar *delimiter;

          /* Initialize the hash table if needed */
          if (!hashtable)
            hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

          delimiter = strstr (lines[i], "=");

          if (delimiter != NULL)
            {
              gchar *key, *value;

              key = g_strndup (lines[i], delimiter - lines[i]);

              /* Jump the '=' */
              delimiter += strlen ("=");

              value = unquote_string (delimiter);

              g_hash_table_insert (hashtable, key, value);
            }
        }
    }

  return hashtable;
}



char *
get_os_type (void)
{
  const guint userspace_num_bits = 8 * GLIB_SIZEOF_VOID_P;
  guint kernel_num_bits = userspace_num_bits;
  struct utsname buffer;

  if (uname (&buffer) == 0)
  {
    const gchar *const *a64;
    for (a64 = arch_64; *a64; a64++)
    {
      if (strcmp (buffer.machine, *a64) == 0)
      {
        kernel_num_bits = 64;
        break;
      }
    }
  }

  if (kernel_num_bits == 64 && userspace_num_bits == 32)
    /* translators: This is the type of architecture for the OS */
    return g_strdup (_("64-bit (32-bit userspace)"));
  else
    /* translators: This is the type of architecture for the OS */
    return g_strdup ((userspace_num_bits == 64) ? _("64-bit") : _("32-bit"));
}



char *
get_system_info (guint infotype)
{
  gchar *result = NULL;
  struct utsname buffer;

  if (uname (&buffer) != 0) {
    result = g_strdup (_("Unknown"));
    return result;
  }

  switch (infotype)
    {
      case OS_NAME:
        result = g_strdup_printf ("%s %s", buffer.sysname, buffer.release);
        break;
      case DEVICE_NAME:
        result = g_strdup (buffer.nodename);
        break;
      case KERNEL:
        result = g_strdup (buffer.release);
        break;
    }

  return result;
}



char *
get_os_name (void)
{
  GHashTable *os_info;
  gchar *name, *version_id, *pretty_name, *build_id;
  gchar *result = NULL;
  g_autofree gchar *name_version = NULL;

  os_info = get_os_info ();

  /* If we're not on Linux we return the OS info from uname */
  if (!os_info) {
    result = get_system_info (OS_NAME);
    return result;
  }

  name = g_hash_table_lookup (os_info, "NAME");
  version_id = g_hash_table_lookup (os_info, "VERSION_ID");
  pretty_name = g_hash_table_lookup (os_info, "PRETTY_NAME");
  build_id = g_hash_table_lookup (os_info, "BUILD_ID");

  if (pretty_name)
    name_version = g_strdup (pretty_name);
  else if (name && version_id)
    name_version = g_strdup_printf ("%s %s", name, version_id);
  else
    name_version = g_strdup (_("Unknown"));

  if (build_id)
    {
      /* translators: This is the name of the OS, followed by the build ID, for
       * example:
       * "Fedora 25 (Workstation Edition); Build ID: xyz" or
       * "Ubuntu 16.04 LTS; Build ID: jki" */
      result = g_strdup_printf (_("%s; Build ID: %s"), name_version, build_id);
    }
  else
    {
      result = g_strdup (name_version);
    }

  g_clear_pointer (&os_info, g_hash_table_destroy);

  return result;
}
