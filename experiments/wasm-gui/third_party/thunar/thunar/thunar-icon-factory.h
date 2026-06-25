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

#ifndef __THUNAR_ICON_FACTORY_H__
#define __THUNAR_ICON_FACTORY_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarIconFactoryClass ThunarIconFactoryClass;
typedef struct _ThunarIconFactory      ThunarIconFactory;

#define THUNAR_TYPE_ICON_FACTORY            (thunar_icon_factory_get_type ())
#define THUNAR_ICON_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ICON_FACTORY, ThunarIconFactory))
#define THUNAR_ICON_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ICON_FACTORY, ThunarIconFactoryClass))
#define THUNAR_IS_ICON_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ICON_FACTORY))
#define THUNAR_IS_ICON_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_ICON_FACTORY))
#define THUNAR_ICON_FACTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ICON_FACTORY, ThunarIconFactoryClass))



GType                  thunar_icon_factory_get_type           (void) G_GNUC_CONST;

ThunarIconFactory     *thunar_icon_factory_get_default        (void);
ThunarIconFactory     *thunar_icon_factory_get_for_icon_theme (GtkIconTheme             *icon_theme);

gboolean               thunar_icon_factory_get_show_thumbnail (const ThunarIconFactory  *factory,
                                                               const ThunarFile         *file);

GdkPixbuf             *thunar_icon_factory_load_icon          (ThunarIconFactory        *factory,
                                                               const gchar              *name,
                                                               gint                      size,
                                                               gboolean                  wants_default);

GdkPixbuf             *thunar_icon_factory_load_file_icon     (ThunarIconFactory        *factory,
                                                               ThunarFile               *file,
                                                               ThunarFileIconState       icon_state,
                                                               gint                      icon_size);

void                   thunar_icon_factory_clear_pixmap_cache (ThunarFile               *file);

G_END_DECLS;

#endif /* !__THUNAR_ICON_FACTORY_H__ */
