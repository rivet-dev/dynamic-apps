/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_UCA_CHOOSER_H__
#define __THUNAR_UCA_CHOOSER_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _ThunarUcaChooserClass ThunarUcaChooserClass;
typedef struct _ThunarUcaChooser      ThunarUcaChooser;

#define THUNAR_UCA_TYPE_CHOOSER             (thunar_uca_chooser_get_type ())
#define THUNAR_UCA_CHOOSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_UCA_TYPE_CHOOSER, ThunarUcaChooser))
#define THUNAR_UCA_CHOOSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_UCA_TYPE_CHOOSER, ThunarUcaChooserClass))
#define THUNAR_UCA_IS_CHOOSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_UCA_TYPE_CHOOSER))
#define THUNAR_UCA_IS_CHOOSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_UCA_TYPE_CHOOSER))
#define THUNAR_UCA_CHOOSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_UCA_TYPE_CHOOSER, ThunarUcaChooserClass))

GType      thunar_uca_chooser_get_type      (void) G_GNUC_CONST;
void       thunar_uca_chooser_register_type (ThunarxProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__THUNAR_UCA_CHOOSER_H__ */
