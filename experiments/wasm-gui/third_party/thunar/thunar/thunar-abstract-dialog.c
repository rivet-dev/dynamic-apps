/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-abstract-dialog.h>



static void thunar_abstract_dialog_close      (GtkDialog                 *dialog);



G_DEFINE_ABSTRACT_TYPE (ThunarAbstractDialog, thunar_abstract_dialog, GTK_TYPE_DIALOG)



static void
thunar_abstract_dialog_class_init (ThunarAbstractDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkBindingSet  *binding_set;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->close = thunar_abstract_dialog_close;

  /* connect additional key bindings to the GtkDialog::close action signal */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_w, GDK_CONTROL_MASK, "close", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_W, GDK_CONTROL_MASK, "close", 0);
}



static void
thunar_abstract_dialog_init (ThunarAbstractDialog *dialog)
{
}



static void
thunar_abstract_dialog_close (GtkDialog *dialog)
{
  GdkEvent *event;

  /* verify that dialog is realized */
  if (G_LIKELY (gtk_widget_get_realized (GTK_WIDGET (dialog))))
    {
      /* send a delete event to the dialog */
      event = gdk_event_new (GDK_DELETE);
      event->any.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (dialog)));
      event->any.send_event = TRUE;
      gtk_main_do_event (event);
      gdk_event_free (event);
    }
}

