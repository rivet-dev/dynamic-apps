/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_PROPERTIES_DIALOG_H__
#define __THUNAR_PROPERTIES_DIALOG_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

/**
 * ThunarPropertiesDialogFlags:
 * @THUNAR_PROPERTIES_DIALOG_NONE: No flags set.
 * @THUNAR_PROPERTIES_DIALOG_SHOW_HIGHLIGHT: SHow the file highlight tab
 *
 * Flags used when creating a #ThunarPropertiesDialog
 */
typedef enum
{
  THUNAR_PROPERTIES_DIALOG_NONE           = 0,
  THUNAR_PROPERTIES_DIALOG_SHOW_HIGHLIGHT = (1 << 0),
} ThunarPropertiesDialogFlags;

typedef struct _ThunarPropertiesDialogClass ThunarPropertiesDialogClass;
typedef struct _ThunarPropertiesDialog      ThunarPropertiesDialog;

#define THUNAR_TYPE_PROPERTIES_DIALOG             (thunar_properties_dialog_get_type ())
#define THUNAR_PROPERTIES_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROPERTIES_DIALOG, ThunarPropertiesDialog))
#define THUNAR_PROPERTIES_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROPERTIES_DIALOG, ThunarPropertiesDialogClass))
#define THUNAR_IS_PROPERTIES_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROPERTIES_DIALOG))
#define THUNAR_IS_PROPERTIES_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROPERTIES_DIALOG))
#define THUNAR_PROPERTIES_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROPERTIES_DIALOG, ThunarPropertiesDialog))

GType       thunar_properties_dialog_get_type (void) G_GNUC_CONST;

GtkWidget  *thunar_properties_dialog_new       (GtkWindow                  *parent,
                                                ThunarPropertiesDialogFlags flags);

void        thunar_properties_dialog_set_files (ThunarPropertiesDialog *dialog,
                                                GList                  *files);

void        thunar_properties_dialog_set_file  (ThunarPropertiesDialog *dialog,
                                                ThunarFile             *file);

G_END_DECLS;

#endif /* !__THUNAR_PROPERTIES_DIALOG_H__ */
