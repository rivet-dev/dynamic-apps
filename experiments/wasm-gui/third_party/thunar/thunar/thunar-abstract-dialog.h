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

#ifndef __THUNAR_ABSTRACT_DIALOG_H__
#define __THUNAR_ABSTRACT_DIALOG_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarAbstractDialogClass ThunarAbstractDialogClass;
typedef struct _ThunarAbstractDialog      ThunarAbstractDialog;

#define THUNAR_TYPE_ABSTRACT_DIALOG             (thunar_abstract_dialog_get_type ())
#define THUNAR_ABSTRACT_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ABSTRACT_DIALOG, ThunarAbstractDialog))
#define THUNAR_ABSTRACT_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ABSTRACT_DIALOG, ThunarAbstractDialogClass))
#define THUNAR_IS_ABSTRACT_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ABSTRACT_DIALOG))
#define THUNAR_IS_ABSTRACT_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_ABSTRACT_DIALOG))
#define THUNAR_ABSTRACT_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ABSTRACT_DIALOG, ThunarAbstractDialog))

struct _ThunarAbstractDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarAbstractDialog
{
  GtkDialog __parent__;
};

GType thunar_abstract_dialog_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_ABSTRACT_DIALOG_H__ */
