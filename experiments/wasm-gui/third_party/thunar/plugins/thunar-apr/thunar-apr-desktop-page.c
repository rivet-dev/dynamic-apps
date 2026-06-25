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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <thunar-apr/thunar-apr-desktop-page.h>

/* use g_access() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(filename, mode) (access ((filename), (mode)))
#endif



static void     thunar_apr_desktop_page_finalize         (GObject                    *object);
static void     thunar_apr_desktop_page_file_changed     (ThunarAprAbstractPage      *abstract_page,
                                                          ThunarxFileInfo            *file);
static void     thunar_apr_desktop_page_save             (ThunarAprDesktopPage       *desktop_page,
                                                          GtkWidget                  *widget);
static void     thunar_apr_desktop_page_save_widget      (ThunarAprDesktopPage       *desktop_page,
                                                          GtkWidget                  *widget,
                                                          GKeyFile                   *key_file);
static void     thunar_apr_desktop_page_activated        (GtkWidget                  *entry,
                                                          ThunarAprDesktopPage       *desktop_page);
static gboolean thunar_apr_desktop_page_focus_out_event  (GtkWidget                  *entry,
                                                          GdkEventFocus              *event,
                                                          ThunarAprDesktopPage       *desktop_page);
static void     thunar_apr_desktop_page_toggled          (GtkWidget                  *button,
                                                          ThunarAprDesktopPage       *desktop_page);
static void     thunar_apr_desktop_page_program_toggled  (GtkWidget                  *button,
                                                          ThunarAprDesktopPage       *desktop_page);
static void     thunar_apr_desktop_page_trusted_toggled  (GtkWidget                  *button,
                                                          ThunarAprDesktopPage       *desktop_page);
static gboolean is_executable                            (GFile    *gfile,
                                                          GError  **error);
static gboolean set_executable                           (GFile    *gfile,
                                                          gboolean  executable,
                                                          GError  **error);



struct _ThunarAprDesktopPageClass
{
  ThunarAprAbstractPageClass __parent__;
};

struct _ThunarAprDesktopPage
{
  ThunarAprAbstractPage __parent__;

  GtkWidget            *description_entry;
  GtkWidget            *command_entry;
  GtkWidget            *path_entry;
  GtkWidget            *url_entry;
  GtkWidget            *comment_entry;
  GtkWidget            *snotify_button;
  GtkWidget            *terminal_button;
  GtkWidget            *program_button;
  GtkWidget            *trusted_button;

  /* the values of the entries remember when
   * the file was saved last time to avoid a
   * race condition between saving the file
   * and the FAM notification which is received
   * with some delay and would otherwise over-
   * ride the (possibly changed) values of the
   * entries.
   */
  gchar                *description_text;
  gchar                *command_text;
  gchar                *path_text;
  gchar                *url_text;
  gchar                *comment_text;
};



THUNARX_DEFINE_TYPE (ThunarAprDesktopPage,
                     thunar_apr_desktop_page,
                     THUNAR_APR_TYPE_ABSTRACT_PAGE);



static gboolean
is_executable (GFile   *gfile,
               GError **error)
{
  GError    *error_local = NULL;
  GFileInfo *info;
  gboolean   executable;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (gfile),               FALSE);

  info = g_file_query_info (gfile,
                            G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error_local);
  if (error_local != NULL)
    {
      g_propagate_error (error, error_local);
      return FALSE;
    }

  executable = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
  g_object_unref (info);

  return executable;
}



/* copied from exo-die-utils.c */
static gboolean
set_executable (GFile    *gfile,
                gboolean  executable,
                GError  **error)
{
  GError *error_local = NULL;
  guint32 mode = 0111, mask = 0111;
  guint32 old_mode, new_mode;
  GFileInfo *info;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (gfile),               FALSE);


  info = g_file_query_info (gfile,
                            G_FILE_ATTRIBUTE_UNIX_MODE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error_local);

  if (error_local != NULL)
    {
      g_propagate_error (error, error_local);
      return FALSE;
    }

  old_mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
  new_mode = executable ? ((old_mode & ~mask) | mode) : (old_mode & ~mask);

  if (old_mode != new_mode) {
    g_file_set_attribute_uint32 (gfile,
                                 G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error_local);
  }
  g_object_unref (info);

  if (error_local != NULL)
    {
      g_propagate_error (error, error_local);
      return FALSE;
    }

  return TRUE;
}



static void
thunar_apr_desktop_page_class_init (ThunarAprDesktopPageClass *klass)
{
  ThunarAprAbstractPageClass *thunarapr_abstract_page_class;
  GObjectClass               *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_apr_desktop_page_finalize;

  thunarapr_abstract_page_class = THUNAR_APR_ABSTRACT_PAGE_CLASS (klass);
  thunarapr_abstract_page_class->file_changed = thunar_apr_desktop_page_file_changed;
}




static void
thunar_apr_desktop_page_init (ThunarAprDesktopPage *desktop_page)
{
  AtkRelationSet *relations;
  PangoAttribute *attribute;
  PangoAttrList  *attr_list;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *grid;
  GtkWidget      *label;
  GtkWidget      *spacer;
  guint           row = 0;
  GFile          *gfile;
  gboolean        metadata_supported;

  gtk_container_set_border_width (GTK_CONTAINER (desktop_page), 12);

  /* allocate shared bold Pango attributes */
  attr_list = pango_attr_list_new ();
  attribute = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attribute->start_index = 0;
  attribute->end_index = -1;
  pango_attr_list_insert (attr_list, attribute);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_add (GTK_CONTAINER (desktop_page), grid);
  gtk_widget_show (grid);

  label = gtk_label_new (_("Description:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  desktop_page->description_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (desktop_page->description_entry, _("The generic name of the entry, for example \"Web Browser\" "
                                                                  "in case of Firefox."));
  g_signal_connect (G_OBJECT (desktop_page->description_entry), "activate", G_CALLBACK (thunar_apr_desktop_page_activated), desktop_page);
  g_signal_connect (G_OBJECT (desktop_page->description_entry), "focus-out-event", G_CALLBACK (thunar_apr_desktop_page_focus_out_event), desktop_page);
  gtk_widget_set_hexpand (desktop_page->description_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->description_entry, 1, row, 1, 1);
  gtk_widget_show (desktop_page->description_entry);

  g_object_bind_property (G_OBJECT (desktop_page->description_entry), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (desktop_page->description_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  label = gtk_label_new (_("Command:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  desktop_page->command_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (desktop_page->command_entry, _("The program to execute, possibly with arguments."));
  g_signal_connect (G_OBJECT (desktop_page->command_entry), "activate", G_CALLBACK (thunar_apr_desktop_page_activated), desktop_page);
  g_signal_connect (G_OBJECT (desktop_page->command_entry), "focus-out-event", G_CALLBACK (thunar_apr_desktop_page_focus_out_event), desktop_page);
  gtk_widget_set_hexpand (desktop_page->command_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->command_entry, 1, row, 1, 1);
  gtk_widget_show (desktop_page->command_entry);

  g_object_bind_property (G_OBJECT (desktop_page->command_entry), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (desktop_page->command_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  label = gtk_label_new (_("Working Directory:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  desktop_page->path_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (desktop_page->path_entry, _("The working directory for the program."));
  g_signal_connect (G_OBJECT (desktop_page->path_entry), "activate", G_CALLBACK (thunar_apr_desktop_page_activated), desktop_page);
  g_signal_connect (G_OBJECT (desktop_page->path_entry), "focus-out-event", G_CALLBACK (thunar_apr_desktop_page_focus_out_event), desktop_page);
  gtk_widget_set_hexpand (desktop_page->path_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->path_entry, 1, row, 1, 1);
  gtk_widget_show (desktop_page->path_entry);

  g_object_bind_property (G_OBJECT (desktop_page->path_entry), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (desktop_page->path_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  label = gtk_label_new (_("URL:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  desktop_page->url_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (desktop_page->url_entry, _("The URL to access."));
  g_signal_connect (G_OBJECT (desktop_page->url_entry), "activate", G_CALLBACK (thunar_apr_desktop_page_activated), desktop_page);
  g_signal_connect (G_OBJECT (desktop_page->url_entry), "focus-out-event", G_CALLBACK (thunar_apr_desktop_page_focus_out_event), desktop_page);
  gtk_widget_set_hexpand (desktop_page->url_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->url_entry, 1, row, 1, 1);
  gtk_widget_show (desktop_page->url_entry);

  g_object_bind_property (G_OBJECT (desktop_page->url_entry), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (desktop_page->url_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  label = gtk_label_new (_("Comment:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  desktop_page->comment_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (desktop_page->comment_entry, _("Tooltip for the entry, for example \"View sites on the Internet\" "
                                                              "in case of Firefox. Should not be redundant with the name or the "
                                                              "description."));
  g_signal_connect (G_OBJECT (desktop_page->comment_entry), "activate", G_CALLBACK (thunar_apr_desktop_page_activated), desktop_page);
  g_signal_connect (G_OBJECT (desktop_page->comment_entry), "focus-out-event", G_CALLBACK (thunar_apr_desktop_page_focus_out_event), desktop_page);
  gtk_widget_set_hexpand (desktop_page->comment_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->comment_entry, 1, row, 1, 1);
  gtk_widget_show (desktop_page->comment_entry);

  g_object_bind_property (G_OBJECT (desktop_page->comment_entry), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (desktop_page->comment_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  /* Nothing here on purpose */

  row++;

  /* add spacing between the entries and the options */
  spacer = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
  gtk_grid_attach (GTK_GRID (grid), spacer, 0, row, 2, 1);
  gtk_widget_show (spacer);

  row++;

  label = gtk_label_new (_("Options:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  desktop_page->snotify_button = gtk_check_button_new_with_mnemonic (_("Use _startup notification"));
  gtk_widget_set_tooltip_text (desktop_page->snotify_button, _("Select this option to enable startup notification when the command "
                                                               "is run from the file manager or the menu. Not every application supports "
                                                               "startup notification."));
  g_signal_connect (G_OBJECT (desktop_page->snotify_button), "toggled", G_CALLBACK (thunar_apr_desktop_page_toggled), desktop_page);
  gtk_widget_set_hexpand (desktop_page->snotify_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->snotify_button, 1, row, 1, 1);
  gtk_widget_show (desktop_page->snotify_button);

  g_object_bind_property (G_OBJECT (desktop_page->snotify_button), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the buttons */
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  object = gtk_widget_get_accessible (desktop_page->snotify_button);
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  desktop_page->terminal_button = gtk_check_button_new_with_mnemonic (_("Run in _terminal"));
  gtk_widget_set_tooltip_text (desktop_page->terminal_button, _("Select this option to run the command in a terminal window."));
  g_signal_connect (G_OBJECT (desktop_page->terminal_button), "toggled", G_CALLBACK (thunar_apr_desktop_page_toggled), desktop_page);
  gtk_widget_set_hexpand (desktop_page->terminal_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->terminal_button, 1, row, 1, 1);
  gtk_widget_show (desktop_page->terminal_button);

  /* don't bind visibility with label */

  /* set Atk label relation for the buttons */
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  object = gtk_widget_get_accessible (desktop_page->terminal_button);
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  row++;

  label = gtk_label_new (_("Security:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* same function as in thunar-permission-chooser.c */
  desktop_page->program_button = gtk_check_button_new_with_mnemonic (_("Allow this file to _run as a .desktop file"));
  gtk_widget_set_tooltip_text (desktop_page->program_button, _("Select this to enable executable permission bit(+x). Thunar will not launch the .desktop file if not set."));
  g_signal_connect (G_OBJECT (desktop_page->program_button), "toggled",
                    G_CALLBACK (thunar_apr_desktop_page_program_toggled), desktop_page);
  gtk_widget_set_hexpand (desktop_page->program_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), desktop_page->program_button, 1, row, 1, 1);
  gtk_widget_show (desktop_page->program_button);

  g_object_bind_property (G_OBJECT (desktop_page->program_button), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);
  xfce_gtk_label_set_a11y_relation (GTK_LABEL (label), desktop_page->program_button);

  row++;

  gfile = g_file_new_for_uri ("file:///");
  metadata_supported = xfce_g_file_metadata_is_supported (gfile);
  g_object_unref (gfile);
  if (metadata_supported)
    {
      desktop_page->trusted_button = gtk_check_button_new_with_mnemonic (_("Set this file as trusted"));
      gtk_widget_set_tooltip_text (desktop_page->trusted_button, _("Select this option to trust this .desktop file. This will generate a checksum of the file and store it via gvfs. The additional check will protect from malicious launchers which e.g. pretend to be a picture, having the executable flag pre-set"));
      g_signal_connect (G_OBJECT (desktop_page->trusted_button), "toggled",
                        G_CALLBACK (thunar_apr_desktop_page_trusted_toggled), desktop_page);
      gtk_widget_set_hexpand (desktop_page->trusted_button, TRUE);
      gtk_grid_attach (GTK_GRID (grid), desktop_page->trusted_button, 1, row, 1, 1);
      gtk_widget_show (desktop_page->trusted_button);

      g_object_bind_property (G_OBJECT (desktop_page->trusted_button), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);
      xfce_gtk_label_set_a11y_relation (GTK_LABEL (label), desktop_page->trusted_button);

      row++;
    }
  else
    {
      g_info ("metadata not supported");
      desktop_page->trusted_button = NULL;
    }

  /* release shared bold Pango attributes */
  pango_attr_list_unref (attr_list);
}



static void
thunar_apr_desktop_page_finalize (GObject *object)
{
  ThunarAprDesktopPage *desktop_page = THUNAR_APR_DESKTOP_PAGE (object);

  /* release the saved texts */
  g_free (desktop_page->description_text);
  g_free (desktop_page->command_text);
  g_free (desktop_page->path_text);
  g_free (desktop_page->url_text);
  g_free (desktop_page->comment_text);

  (*G_OBJECT_CLASS (thunar_apr_desktop_page_parent_class)->finalize) (object);
}



static void
thunar_apr_desktop_page_file_changed (ThunarAprAbstractPage *abstract_page,
                                      ThunarxFileInfo       *file)
{
  ThunarAprDesktopPage *desktop_page = THUNAR_APR_DESKTOP_PAGE (abstract_page);
  GKeyFile             *key_file;
  GFile                *gfile;
  gboolean              writable;
  gboolean              enabled;
  gboolean              executable;
  gboolean              trusted;
  GError               *error = NULL;
  gchar                *filename;
  gchar                *value;
  gchar                *type;
  gchar                *uri;

  /* allocate the key file memory */
  key_file = g_key_file_new ();

  /* determine the local path to the file */
  uri = thunarx_file_info_get_uri (file);
  filename = g_filename_from_uri (uri, NULL, NULL);
  g_free (uri);

  /* try to load the file contents */
  if (filename != NULL && g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL))
    {
      /* determine the type of the .desktop file (default to "Application") */
      type = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "Type", NULL);
      if (G_UNLIKELY (type == NULL))
        type = g_strdup ("Application");

      /* change page title depending on the type */
      if (strcmp (type, "Application") == 0)
        thunarx_property_page_set_label (THUNARX_PROPERTY_PAGE (desktop_page), _("Launcher"));
      else if (strcmp (type, "Link") == 0)
        thunarx_property_page_set_label (THUNARX_PROPERTY_PAGE (desktop_page), _("Link"));
      else
        thunarx_property_page_set_label (THUNARX_PROPERTY_PAGE (desktop_page), type);

      /* update the "Description" entry */
      value = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "GenericName", NULL, NULL);
      if (g_strcmp0 (value, desktop_page->description_text) != 0)
        {
          /* update the entry */
          gtk_entry_set_text (GTK_ENTRY (desktop_page->description_entry), (value != NULL) ? value : "");

          /* update the saved value */
          g_free (desktop_page->description_text);
          desktop_page->description_text = value;
        }
      else
        {
          g_free (value);
        }

      /* update the "Comment" entry */
      value = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "Comment", NULL, NULL);
      if (g_strcmp0 (value, desktop_page->comment_text) != 0)
        {
          /* update the entry */
          gtk_entry_set_text (GTK_ENTRY (desktop_page->comment_entry), (value != NULL) ? value : "");

          /* update the saved value */
          g_free (desktop_page->comment_text);
          desktop_page->comment_text = value;
        }
      else
        {
          g_free (value);
        }

      /* update the type dependant entries */
      if (strcmp (type, "Application") == 0)
        {
          /* update the "Command" entry but but ignore escape sequences */
          value = g_key_file_get_value (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
          if (g_strcmp0 (value, desktop_page->command_text) != 0)
            {
              /* update the entry */
              gtk_entry_set_text (GTK_ENTRY (desktop_page->command_entry), (value != NULL) ? value : "");

              /* update the saved value */
              g_free (desktop_page->command_text);
              desktop_page->command_text = value;
            }
          else
            {
              g_free (value);
            }

          /* update the "Path" entry */
          value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "Path", NULL);
          if (g_strcmp0 (value, desktop_page->path_text) != 0)
            {
              /* update the entry */
              gtk_entry_set_text (GTK_ENTRY (desktop_page->path_entry), (value != NULL) ? value : "");

              /* update the saved value */
              g_free (desktop_page->path_text);
              desktop_page->path_text = value;
            }
          else
            {
              g_free (value);
            }

          /* update the "Use startup notification" button */
          enabled = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, "StartupNotify", &error);
          g_signal_handlers_block_by_func (G_OBJECT (desktop_page->snotify_button), thunar_apr_desktop_page_toggled, desktop_page);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_page->snotify_button), (error == NULL && enabled));
          g_signal_handlers_unblock_by_func (G_OBJECT (desktop_page->snotify_button), thunar_apr_desktop_page_toggled, desktop_page);
          g_clear_error (&error);

          /* update the "Run in terminal" button */
          enabled = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, "Terminal", &error);
          g_signal_handlers_block_by_func (G_OBJECT (desktop_page->terminal_button), thunar_apr_desktop_page_toggled, desktop_page);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_page->terminal_button), (error == NULL && enabled));
          g_signal_handlers_unblock_by_func (G_OBJECT (desktop_page->terminal_button), thunar_apr_desktop_page_toggled, desktop_page);
          g_clear_error (&error);

          /* update visibility of the specific widgets */
          gtk_widget_show (desktop_page->command_entry);
          gtk_widget_show (desktop_page->path_entry);
          gtk_widget_hide (desktop_page->url_entry);
          gtk_widget_show (desktop_page->snotify_button);
          gtk_widget_show (desktop_page->terminal_button);
        }
      else if (strcmp (type, "Link") == 0)
        {
          /* update the "URL" entry */
          value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "URL", NULL);
          if (g_strcmp0 (value, desktop_page->url_text) != 0)
            {
              /* update the entry */
              gtk_entry_set_text (GTK_ENTRY (desktop_page->url_entry), (value != NULL) ? value : "");

              /* update the saved value */
              g_free (desktop_page->url_text);
              desktop_page->url_text = value;
            }
          else
            {
              g_free (value);
            }

          /* update visibility of the specific widgets */
          gtk_widget_hide (desktop_page->command_entry);
          gtk_widget_hide (desktop_page->path_entry);
          gtk_widget_show (desktop_page->url_entry);
          gtk_widget_hide (desktop_page->snotify_button);
          gtk_widget_hide (desktop_page->terminal_button);
        }
      else
        {
          /* hide the specific widgets */
          gtk_widget_hide (desktop_page->command_entry);
          gtk_widget_hide (desktop_page->path_entry);
          gtk_widget_hide (desktop_page->url_entry);
          gtk_widget_hide (desktop_page->snotify_button);
          gtk_widget_hide (desktop_page->terminal_button);
        }

      /* update flags */
      gfile = thunarx_file_info_get_location (abstract_page->file);

      executable = is_executable (gfile, &error);
      if (error != NULL)
        {
          g_warning ("Failed to initialize program_button : %s", error->message);
          g_clear_error (&error);
        }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_page->program_button), executable);

      if (desktop_page->trusted_button != NULL)
        {
          trusted = xfce_g_file_is_trusted (gfile, NULL, &error);
          if (error != NULL)
            {
              g_warning ("Failed to initialize trusted_button : %s", error->message);
              g_clear_error (&error);
            }
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_page->trusted_button), trusted);
        }

      g_object_unref (gfile);

      /* show security */
      gtk_widget_show (desktop_page->program_button);
      if (desktop_page->trusted_button != NULL)
        gtk_widget_show (desktop_page->trusted_button);

      /* check if the file is writable... */
      writable = (g_access (filename, W_OK) == 0);

      /* ...and update the editability of the entries */
      gtk_editable_set_editable (GTK_EDITABLE (desktop_page->description_entry), writable);
      gtk_editable_set_editable (GTK_EDITABLE (desktop_page->command_entry), writable);
      gtk_editable_set_editable (GTK_EDITABLE (desktop_page->path_entry), writable);
      gtk_editable_set_editable (GTK_EDITABLE (desktop_page->url_entry), writable);
      gtk_editable_set_editable (GTK_EDITABLE (desktop_page->comment_entry), writable);
      gtk_widget_set_sensitive (desktop_page->snotify_button, writable);
      gtk_widget_set_sensitive (desktop_page->terminal_button, writable);
      gtk_widget_set_sensitive (desktop_page->program_button, writable);
      if (desktop_page->trusted_button != NULL)
        gtk_widget_set_sensitive (desktop_page->trusted_button, writable);

      /* cleanup */
      g_free (type);
    }
  else
    {
      /* reset page title */
      thunarx_property_page_set_label (THUNARX_PROPERTY_PAGE (desktop_page), _("Unknown"));

      /* hide all widgets */
      gtk_widget_hide (desktop_page->description_entry);
      gtk_widget_hide (desktop_page->command_entry);
      gtk_widget_hide (desktop_page->path_entry);
      gtk_widget_hide (desktop_page->url_entry);
      gtk_widget_hide (desktop_page->comment_entry);
      gtk_widget_hide (desktop_page->snotify_button);
      gtk_widget_hide (desktop_page->terminal_button);
      gtk_widget_hide (desktop_page->program_button);
      if (desktop_page->trusted_button != NULL)
        gtk_widget_hide (desktop_page->trusted_button);
    }

  /* cleanup */
  g_key_file_free (key_file);
  g_free (filename);
}



static void
thunar_apr_desktop_page_save (ThunarAprDesktopPage *desktop_page,
                              GtkWidget            *widget)
{
  GtkWidget *toplevel;
  GtkWidget *message;
  GKeyFile  *key_file;
  GError    *error = NULL;
  gchar     *filename;
  gchar     *data;
  gchar     *uri;
  gsize      data_length;
  FILE      *fp;
  GFile     *gfile;
  gboolean   trusted;

  /* verify that we still have a valid file */
  if (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file == NULL)
    return;

  /* determine the local path to the file */
  uri = thunarx_file_info_get_uri (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file);
  filename = g_filename_from_uri (uri, NULL, NULL);
  g_free (uri);

  /* verify that we have a valid local path */
  if (G_UNLIKELY (filename == NULL))
    return;

  /* allocate the key file resources */
  key_file = g_key_file_new ();

  /* try to parse the key file */
  if (g_key_file_load_from_file (key_file, filename, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    {
      /* save the widget changes to the key file */
      thunar_apr_desktop_page_save_widget (desktop_page, widget, key_file);

      /* give empty desktop files a type */
      if (!g_key_file_has_key (key_file, G_KEY_FILE_DESKTOP_GROUP,
                               G_KEY_FILE_DESKTOP_KEY_TYPE, NULL))
        {
          g_key_file_set_string (key_file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_TYPE,
                                 "Application");
        }

      /* determine the content of the key file */
      data = g_key_file_to_data (key_file, &data_length, &error);
      if (G_LIKELY (data_length > 0))
        {
          trusted = FALSE;
          if (desktop_page->trusted_button != NULL)
            trusted  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (desktop_page->trusted_button));

          /* try to save the key file content to disk */
          fp = fopen (filename, "w");
          if (G_LIKELY (fp != NULL))
            {
              if (fwrite (data, data_length, 1, fp) != 1)
                error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
              fclose (fp);
            }
          else
            {
              error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
            }

          /* Update safety flag checksum */
          if (trusted && error == NULL)
            {
              gfile = thunarx_file_info_get_location (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file);
              xfce_g_file_set_trusted (gfile, trusted, NULL, &error);
              g_object_unref (gfile);
            }
        }

      /* cleanup */
      g_free (data);
    }

  /* check if we succeed */
  if (G_UNLIKELY (error != NULL))
    {
      /* display an error dialog to the user */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (desktop_page));
      message = gtk_message_dialog_new ((GtkWindow *) toplevel,
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Failed to save \"%s\"."), filename);
      gtk_window_set_title (GTK_WINDOW (message), _("Error"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }

  /* cleanup */
  g_key_file_free (key_file);
  g_free (filename);
}



static void
thunar_apr_desktop_page_set_string (GKeyFile    *key_file,
                                    const gchar *key,
                                    const gchar *value)
{
  if (xfce_str_is_empty (value))
    {
      g_key_file_remove_key (key_file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             key, NULL);
    }
  else
    {
      /* escape special characters but not for the "Exec" property */
      if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_EXEC) == 0)
        g_key_file_set_value (key_file,
                              G_KEY_FILE_DESKTOP_GROUP,
                              key, value);
      else
        g_key_file_set_string (key_file,
                               G_KEY_FILE_DESKTOP_GROUP,
                               key, value);
    }
}



static void
thunar_apr_desktop_page_save_widget (ThunarAprDesktopPage *desktop_page,
                                     GtkWidget            *widget,
                                     GKeyFile             *key_file)
{
  const gchar * const *locale;
  gchar               *key;

  if (widget == desktop_page->description_entry)
    {
      /* update the saved description text */
      g_free (desktop_page->description_text);
      desktop_page->description_text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

      /* save the new description (localized if required) */
      for (locale = g_get_language_names (); *locale != NULL; ++locale)
        {
          key = g_strdup_printf (G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME "[%s]", *locale);
          if (g_key_file_has_key (key_file, G_KEY_FILE_DESKTOP_GROUP, key, NULL))
            {
              thunar_apr_desktop_page_set_string (key_file, key, desktop_page->description_text);
              g_free (key);
              break;
            }
          g_free (key);
        }

      /* fallback to unlocalized description */
      if (G_UNLIKELY (*locale == NULL))
        {
          thunar_apr_desktop_page_set_string (key_file,
                                              G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME,
                                              desktop_page->description_text);
        }
    }
  else if (widget == desktop_page->command_entry)
    {
      /* update the saved command */
      g_free (desktop_page->command_text);
      desktop_page->command_text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

      /* save the unlocalized command */
      thunar_apr_desktop_page_set_string (key_file,
                                          G_KEY_FILE_DESKTOP_KEY_EXEC,
                                          desktop_page->command_text);
    }
  else if (widget == desktop_page->path_entry)
    {
      /* update the saved command */
      g_free (desktop_page->path_text);
      desktop_page->path_text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

      /* save the unlocalized command */
      thunar_apr_desktop_page_set_string (key_file,
                                          G_KEY_FILE_DESKTOP_KEY_PATH,
                                          desktop_page->path_text);
    }
  else if (widget == desktop_page->url_entry)
    {
      /* update the saved URL */
      g_free (desktop_page->url_text);
      desktop_page->url_text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

      /* save the unlocalized url */
      thunar_apr_desktop_page_set_string (key_file,
                                          G_KEY_FILE_DESKTOP_KEY_URL,
                                          desktop_page->url_text);
    }
  else if (widget == desktop_page->comment_entry)
    {
      /* update the saved comment text */
      g_free (desktop_page->comment_text);
      desktop_page->comment_text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

      /* save the new comment (localized if required) */
      for (locale = g_get_language_names (); *locale != NULL; ++locale)
        {
          key = g_strdup_printf (G_KEY_FILE_DESKTOP_KEY_COMMENT "[%s]", *locale);
          if (g_key_file_has_key (key_file, G_KEY_FILE_DESKTOP_GROUP, key, NULL))
            {
              thunar_apr_desktop_page_set_string (key_file, key, desktop_page->comment_text);
              g_free (key);
              break;
            }
          g_free (key);
        }

      /* fallback to unlocalized comment */
      if (G_UNLIKELY (*locale == NULL))
        thunar_apr_desktop_page_set_string (key_file,
                                            G_KEY_FILE_DESKTOP_KEY_COMMENT,
                                            desktop_page->comment_text);
    }
  else if (widget == desktop_page->snotify_button)
    {
      g_key_file_set_boolean (key_file,
                              G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
    }
  else if (widget == desktop_page->terminal_button)
    {
      g_key_file_set_boolean (key_file,
                              G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
    }
  else
    {
      g_assert_not_reached ();
    }
}



static void
thunar_apr_desktop_page_activated (GtkWidget            *entry,
                                   ThunarAprDesktopPage *desktop_page)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (THUNAR_APR_IS_DESKTOP_PAGE (desktop_page));

  /* check if the entry is editable, and if so, save the file */
  if (gtk_editable_get_editable (GTK_EDITABLE (entry)))
    thunar_apr_desktop_page_save (desktop_page, entry);
}



static gboolean
thunar_apr_desktop_page_focus_out_event (GtkWidget            *entry,
                                         GdkEventFocus        *event,
                                         ThunarAprDesktopPage *desktop_page)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);
  g_return_val_if_fail (THUNAR_APR_IS_DESKTOP_PAGE (desktop_page), FALSE);

  /* check if the entry is editable, and if so, save the file */
  if (gtk_editable_get_editable (GTK_EDITABLE (entry)))
    thunar_apr_desktop_page_save (desktop_page, entry);
  return FALSE;
}



static void
thunar_apr_desktop_page_toggled (GtkWidget            *button,
                                 ThunarAprDesktopPage *desktop_page)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  g_return_if_fail (THUNAR_APR_IS_DESKTOP_PAGE (desktop_page));

  /* save the file */
  thunar_apr_desktop_page_save (desktop_page, button);
}



/**
 * Allowed state:
 *
 * +-----+-----+
 * |EXE  |SAFE |
 * +=====+=====+
 * |TRUE |TRUE | Allowed to launch
 * +-----+-----+
 * |TRUE |FALSE| Ask before launch
 * +-----+-----+
 * |FALSE|FALSE| Not recognized as .desktop
 * +-----+-----+
 **/
static void
thunar_apr_desktop_page_program_toggled (GtkWidget            *button,
                                         ThunarAprDesktopPage *desktop_page)
{
  GError  *error = NULL;
  GFile   *gfile;
  gboolean executable;
  gboolean trusted;

  g_return_if_fail (button == desktop_page->program_button);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  g_return_if_fail (THUNAR_APR_IS_DESKTOP_PAGE (desktop_page));
  g_return_if_fail (THUNARX_IS_FILE_INFO (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file));

  gfile = thunarx_file_info_get_location (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file);

  executable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (desktop_page->program_button));
  set_executable (gfile, executable, &error);

  g_object_unref (gfile);

  if (error != NULL)
    {
      g_warning ("Error while setting execution flag : %s", error->message);
      g_error_free (error);
      return;
    }

  trusted  = (desktop_page->trusted_button != NULL) ? gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (desktop_page->trusted_button)) : FALSE;
  /* if the executable flag is unset, that will as well unset the trusted flag */
  if (!executable && trusted)
    if (desktop_page->trusted_button != NULL)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_page->trusted_button), FALSE);
}

static void
thunar_apr_desktop_page_trusted_toggled (GtkWidget            *button,
                                         ThunarAprDesktopPage *desktop_page)
{
  GError  *error = NULL;
  GFile   *gfile;
  gboolean executable;
  gboolean trusted;

  g_return_if_fail (button == desktop_page->trusted_button);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  g_return_if_fail (THUNAR_APR_IS_DESKTOP_PAGE (desktop_page));
  g_return_if_fail (THUNARX_IS_FILE_INFO (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file));

  gfile = thunarx_file_info_get_location (THUNAR_APR_ABSTRACT_PAGE (desktop_page)->file);

  trusted  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (desktop_page->trusted_button));
  xfce_g_file_set_trusted (gfile, trusted, NULL, &error);

  g_object_unref (gfile);

  if (error != NULL)
    {
      g_warning ("Error while setting safety flag : %s", error->message);
      g_error_free (error);
      return;
    }

  executable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (desktop_page->program_button));
  /* setting the trusted flag automatically sets the execute flag */
  if (!executable && trusted)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_page->program_button), TRUE);
}
