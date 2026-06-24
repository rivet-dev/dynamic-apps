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

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "contributors.h"
#include "about-dialog-ui.h"

#ifdef HAVE_GLIBTOP
#include "system-info.h"
#endif

#define MARGIN 20
#define DOCS_URL "https://docs.xfce.org/xfce/"
#define DOCS_URL_SLUG "/start"



typedef struct
{
  const gchar *name;
  const gchar *icon_name;
  const gchar *display_name;
  const gchar *description;
}
AboutModules;



static gboolean     opt_version = FALSE;
static GOptionEntry opt_entries[] =
{
  { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
  { NULL }
};


#ifdef HAVE_GLIBTOP
static void
xfce_about_system (GtkBuilder *builder)
{
  GObject *label;
  GObject *vendor_info;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  g_autofree char *device_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *gpu_text = NULL;
  g_autofree char *gtk_text = NULL;
  g_autofree char *kernel_text = NULL;
  g_autofree char *memory_text = NULL;
  g_autofree char *os_name_text = NULL;
  g_autofree char *os_type_text = NULL;
  guint num_gpus = 0;

  label = gtk_builder_get_object (builder, "device");
  device_text = get_system_info (DEVICE_NAME);
  gtk_label_set_text (GTK_LABEL (label), device_text ? device_text : "");

  label = gtk_builder_get_object (builder, "osname");
  os_name_text = get_os_name ();
  gtk_label_set_text (GTK_LABEL (label), os_name_text ? os_name_text : "");

  label = gtk_builder_get_object (builder, "ostype");
  os_type_text = get_os_type ();
  gtk_label_set_text (GTK_LABEL (label), os_type_text ? os_type_text : "");

  label = gtk_builder_get_object (builder, "vendor-info");
  vendor_info = gtk_builder_get_object (builder, "vendor-info-label");
#ifdef VENDOR_INFO
  gtk_label_set_text (GTK_LABEL (label), VENDOR_INFO);
  gtk_widget_show (GTK_WIDGET (vendor_info));
#else
  gtk_widget_hide (GTK_WIDGET (vendor_info));
  gtk_widget_hide (GTK_WIDGET (label));
#endif

  label = gtk_builder_get_object (builder, "xfce-version");
  gtk_label_set_text (GTK_LABEL (label), xfce_version_string ());

  label = gtk_builder_get_object (builder, "gtk-version");
  gtk_text = g_strdup_printf ("%d.%d.%d", gtk_get_major_version (), gtk_get_minor_version (), gtk_get_micro_version ());
  gtk_label_set_text (GTK_LABEL (label), gtk_text);

  label = gtk_builder_get_object (builder, "kernel-version");
  kernel_text = get_system_info (KERNEL);
  gtk_label_set_text (GTK_LABEL (label), kernel_text ? kernel_text : "");

  label = gtk_builder_get_object (builder, "cpu");
  info = glibtop_get_sysinfo ();
  cpu_text = get_cpu_info (info);
  gtk_label_set_markup (GTK_LABEL (label), cpu_text ? cpu_text : "");

  label = gtk_builder_get_object (builder, "memory");
  glibtop_get_mem (&mem);
  memory_text = g_format_size_full (mem.total, G_FORMAT_SIZE_IEC_UNITS);
  gtk_label_set_text (GTK_LABEL (label), memory_text ? memory_text : "");

  gpu_text = get_gpu_info (&num_gpus);
  if (num_gpus > 1)
    {
      label = gtk_builder_get_object (builder, "gpu-label");
      gtk_label_set_text (GTK_LABEL (label), _("GPUs"));
    }
  label = gtk_builder_get_object (builder, "gpu");
  gtk_label_set_markup (GTK_LABEL (label), gpu_text ? gpu_text : "");
}
#endif



static void
xfce_about_about (GtkWidget *vbox)
{
  guint                i;
  AboutModules        *info;
  static AboutModules  xfce_about_info[] =
    {
      { "xfwm4",
        "org.xfce.xfwm4",
        N_("Window Manager"),
        N_("Handles the placement of windows on the screen.")
      },
      { "xfce4-panel",
        "org.xfce.panel",
        N_("Panel"),
        N_("Provides a home for window buttons, launchers, app menu and more.")
      },
      { "xfdesktop",
        "org.xfce.xfdesktop",
        N_("Desktop Manager"),
        N_("Sets desktop backgrounds, handles icons and more.")
      },
      { "thunar",
        "org.xfce.thunar",
        N_("File Manager"),
        N_("Manages your files in a modern, easy-to-use and fast way.")
      },
      { "thunar-volman",
        "org.xfce.volman",
        N_("Volume Manager"),
        N_("Manages removable drives and media for Thunar.")
      },
      { "xfce4-session",
        "org.xfce.session",
        N_("Session Manager"),
        N_("Saves and restores your session, handles startup, autostart and shutdown.")
      },
      { "xfce4-settings",
        "org.xfce.settings.manager",
        N_("Settings Manager"),
        N_("Configures appearance, display, keyboard and mouse settings.")
      },
      { "xfce4-appfinder",
        "org.xfce.appfinder",
        N_("Application Finder"),
        N_("Quickly finds and launches applications installed on your system.")
      },
      { "xfconf",
        "org.xfce.settings.editor",
        N_("Settings Daemon"),
        N_("Stores your settings in a D-Bus-based configuration system.")
      },
      { "garcon",
        "org.xfce.garcon",
        N_("A Menu Library"),
        N_("Implements a freedesktop.org compliant menu based on GLib and GIO.")
      },
      { "tumbler",
        "org.xfce.tumbler",
        N_("Thumbnails Service"),
        N_("Implements the thumbnail management D-Bus specification.")
      }
    };

  g_return_if_fail (GTK_IS_BOX (vbox));

  for (i = 0; i < G_N_ELEMENTS (xfce_about_info); i++)
    {
      GtkWidget     *grid;
      GtkWidget     *image;
      GtkWidget     *component;
      GtkWidget     *description;
      const gchar   *format;
      gchar         *str;

      info = xfce_about_info + i;
      format = "<b>%s</b> (<a href='%s%s%s'>%s</a>)";
      str = g_markup_printf_escaped (format, _(info->display_name), DOCS_URL,
                                     info->name, DOCS_URL_SLUG, info->name);

      component = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (component), str);
      gtk_label_set_xalign (GTK_LABEL (component), 0.0);
      gtk_label_set_line_wrap (GTK_LABEL (component), TRUE);
      gtk_widget_set_valign (GTK_WIDGET (component), GTK_ALIGN_END);

      description = gtk_label_new (_(info->description));
      gtk_label_set_xalign (GTK_LABEL (description), 0.0);
      gtk_label_set_line_wrap (GTK_LABEL (description), TRUE);
      gtk_widget_set_valign (GTK_WIDGET (description), GTK_ALIGN_START);

      image = gtk_image_new_from_icon_name (info->icon_name, GTK_ICON_SIZE_DND);

      grid = gtk_grid_new ();
      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
      gtk_grid_attach (GTK_GRID (grid), image, 0, 0, 1, 2);
      gtk_grid_attach (GTK_GRID (grid), component, 1, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), description, 1, 1, 1, 1);
      gtk_widget_show_all (grid);

      gtk_container_add (GTK_CONTAINER (vbox), grid);

      g_free (str);
    }
}



static void
xfce_about_credits_translators (GtkTextBuffer *buffer, GtkTextIter *end)
{
  gtk_text_buffer_get_end_iter (buffer, end);
  gtk_text_buffer_insert (buffer, end,
      _("Please see <https://www.xfce.org/about/credits>"), -1);
  gtk_text_buffer_insert (buffer, end, "\n\n", -1);
}



static void
xfce_about_credits (GtkTextBuffer *buffer)
{
  guint                   i;
  GtkTextTag             *title;
  GtkTextTag             *indent;
  GtkTextIter             end;
  const ContributorGroup *group;
  const ContributorInfo  *user;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  title = gtk_text_buffer_create_tag (buffer, "title",
                                      "pixels-below-lines", 6,
                                      "scale", 1.1,
                                      "weight", PANGO_WEIGHT_BOLD, NULL);

  indent = gtk_text_buffer_create_tag (buffer, "indent",
                                       "left-margin", MARGIN,
                                       "indent", -MARGIN, NULL);

  gtk_text_buffer_get_end_iter (buffer, &end);

  for (i = 0; i < G_N_ELEMENTS (xfce_contributors); i++)
    {
      group = xfce_contributors + i;

      gtk_text_buffer_insert_with_tags (buffer, &end, _(group->name), -1, title, NULL);
      gtk_text_buffer_insert (buffer, &end, "\n", -1);

      if (group->contributors != NULL)
        {
          for (user = group->contributors; user->name != NULL; user++)
            {
              gtk_text_buffer_insert_with_tags (buffer, &end, user->name, -1, indent, NULL);
              if (g_strcmp0 (user->email, "") != 0)
                {
                  gtk_text_buffer_insert (buffer, &end, " <", -1);
                  gtk_text_buffer_insert (buffer, &end, user->email, -1);
                  gtk_text_buffer_insert (buffer, &end, ">\n", -1);
                }
              else
                gtk_text_buffer_insert (buffer, &end, "\n", -1);
            }
        }
      else
        {
          /* add the translators */
          xfce_about_credits_translators (buffer, &end);
        }

      gtk_text_buffer_insert (buffer, &end, "\n", -1);
    }

  gtk_text_buffer_insert (buffer, &end,
      _("If you know of anyone missing from this list; don't hesitate and "
        "file a bug on <https://gitlab.xfce.org/xfce/libxfce4ui/-/issues> ."), -1);
  gtk_text_buffer_insert (buffer, &end, "\n\n", -1);
  gtk_text_buffer_insert_with_tags (buffer, &end,
      _("Thanks to all who helped making this software available!"), -1, title, NULL);

  gtk_text_buffer_insert (buffer, &end, "\n", -1);
}



static void
xfce_about_copyright (GtkTextBuffer *buffer)
{
  GtkTextIter end;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_end_iter (buffer, &end);

  gtk_text_buffer_insert (buffer, &end,
      _("Xfce 4 is copyright Olivier Fourdan (fourdan@xfce.org). The different "
        "components are copyrighted by their respective authors."), -1);

  gtk_text_buffer_insert (buffer, &end, "\n\n", -1);
  gtk_text_buffer_insert (buffer, &end,
      _("The libxfce4ui, libxfce4util and exo packages are "
        "distributed under the terms of the GNU Library General Public License as "
        "published by the Free Software Foundation; either version 2 of the License, or "
        "(at your option) any later version."), -1);

  gtk_text_buffer_insert (buffer, &end, "\n\n", -1);
  gtk_text_buffer_insert (buffer, &end,
      _("The packages thunar, xfce4-appfinder, xfce4-panel, xfce4-session, "
        "xfce4-settings, xfconf, xfdesktop and xfwm4 are "
        "distributed under the terms of the GNU General Public License as "
        "published by the Free Software Foundation; either version 2 of the "
        "License, or (at your option) any later version."), -1);

  gtk_text_buffer_insert (buffer, &end, "\n", -1);
}



#ifdef VENDOR_INFO
static void
xfce_about_vendor (GtkBuilder *builder)
{
  gchar   *contents;
  gchar   *filename;
  GObject *object;
  gsize    length;

  g_return_if_fail (GTK_IS_BUILDER (builder));

  filename = g_build_filename (DATADIR, "vendorinfo", NULL);
  if (g_file_get_contents (filename, &contents, &length, NULL))
    {
      if (length > 0)
        {
          if (g_utf8_validate(contents, length, NULL))
            {
              object = gtk_builder_get_object (builder, "vendor-buffer");
              gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object), contents, length);

              object = gtk_builder_get_object (builder, "vendor-label");
              gtk_label_set_text (GTK_LABEL (object), VENDOR_INFO);

              object = gtk_builder_get_object (builder, "vendor-tab");
              gtk_widget_show (GTK_WIDGET (object));
            }
          else
            {
              g_critical ("\"%s\" is not UTF-8 valid", filename);
            }
        }

      g_free (contents);
    }
  else
    {
      g_message ("No vendor information found in \"%s\".", filename);
    }

  g_free (filename);
}
#endif



static void
xfce_about_license (GtkBuilder *builder,
                    GObject    *buffer)
{
  GObject         *dialog;
  GObject         *object;
  static gboolean  initial = TRUE;

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  object = gtk_builder_get_object (builder, "license-textview");
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (object), GTK_TEXT_BUFFER (buffer));

  dialog = gtk_builder_get_object (builder, "license-dialog");
  if (initial)
    {
      g_signal_connect (G_OBJECT (dialog), "delete-event",
           G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      object = gtk_builder_get_object (builder, "license-close-button");
      g_signal_connect_swapped (G_OBJECT (object), "clicked",
          G_CALLBACK (gtk_widget_hide), dialog);

      object = gtk_builder_get_object (builder, "window");
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (object));

      initial = FALSE;
    }

  gtk_widget_show (GTK_WIDGET (dialog));
}



static void
xfce_about_license_gpl (GtkBuilder *builder)
{
  xfce_about_license (builder,
      gtk_builder_get_object (builder, "gpl-buffer"));
}



static void
xfce_about_license_lgpl (GtkBuilder *builder)
{
  xfce_about_license (builder,
      gtk_builder_get_object (builder, "lgpl-buffer"));
}



static void
xfce_about_license_bsd (GtkBuilder *builder)
{
  xfce_about_license (builder,
      gtk_builder_get_object (builder, "bsd-buffer"));
}



static void
xfce_about_help (GtkWidget *button,
                 GtkWidget *dialog)
{
  g_return_if_fail (GTK_IS_BUTTON (button));
  xfce_dialog_show_help (GTK_WINDOW (dialog),
                         NULL, NULL, NULL);
}



gint
main (gint    argc,
      gchar **argv)
{
  GtkBuilder *builder;
  GError     *error = NULL;
  GObject    *dialog;
  GObject    *object;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  if (G_UNLIKELY (!gtk_init_with_args (&argc, &argv, NULL, opt_entries, PACKAGE, &error)))
    {
      if (G_LIKELY (error != NULL))
        {
          g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
          g_print (_("Type '%s --help' for usage information."), G_LOG_DOMAIN);
          g_print ("\n");

          g_error_free (error);
        }
      else
        g_error (_("Unable to initialize GTK+."));

      return EXIT_FAILURE;
    }

  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2008-2024");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, xfce_about_dialog_ui,
                                    xfce_about_dialog_ui_length, &error))
    {
      xfce_dialog_show_error (NULL, error, _("Failed to load interface"));
      g_error_free (error);
      g_object_unref (G_OBJECT (builder));

      return EXIT_FAILURE;
    }

  dialog = gtk_builder_get_object (builder, "window");
  g_signal_connect_swapped (G_OBJECT (dialog), "delete-event",
      G_CALLBACK (gtk_main_quit), NULL);

#ifdef HAVE_GLIBTOP
  xfce_about_system (builder);
#else
  // Hide system tab
  gtk_notebook_remove_page (GTK_NOTEBOOK (gtk_builder_get_object (builder, "notebook")), 0);
#endif

  object = gtk_builder_get_object (builder, "components-box");
  xfce_about_about (GTK_WIDGET (object));

  object = gtk_builder_get_object (builder, "credits-buffer");
  xfce_about_credits (GTK_TEXT_BUFFER (object));

  object = gtk_builder_get_object (builder, "copyright-buffer");
  xfce_about_copyright (GTK_TEXT_BUFFER (object));

#ifdef VENDOR_INFO
  xfce_about_vendor (builder);
#endif

  object = gtk_builder_get_object (builder, "gpl-button");
  g_signal_connect_swapped (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_about_license_gpl), builder);

  object = gtk_builder_get_object (builder, "lgpl-button");
  g_signal_connect_swapped (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_about_license_lgpl), builder);

  object = gtk_builder_get_object (builder, "bsd-button");
  g_signal_connect_swapped (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_about_license_bsd), builder);

  object = gtk_builder_get_object (builder, "help-button");
  g_signal_connect (G_OBJECT (object), "clicked",
      G_CALLBACK (xfce_about_help), dialog);

  object = gtk_builder_get_object (builder, "close-button");
  g_signal_connect_swapped (G_OBJECT (object), "clicked",
      G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (GTK_WIDGET (dialog));

  gtk_main ();

  g_object_unref (G_OBJECT (builder));

  return EXIT_SUCCESS;
}
