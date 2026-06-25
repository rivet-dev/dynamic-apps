/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_BROWSER_H__
#define __THUNAR_BROWSER_H__

#include <thunar/thunar-file.h>
#include <thunar/thunar-device.h>

G_BEGIN_DECLS

#define THUNAR_TYPE_BROWSER           (thunar_browser_get_type ())
#define THUNAR_BROWSER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_BROWSER, ThunarBrowser))
#define THUNAR_IS_BROWSER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_BROWSER))
#define THUNAR_BROWSER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_BROWSER, ThunarBrowserIface))

typedef struct _ThunarBrowser      ThunarBrowser;
typedef struct _ThunarBrowserIface ThunarBrowserIface;

typedef void (*ThunarBrowserPokeFileFunc)     (ThunarBrowser *browser,
                                               ThunarFile    *file,
                                               ThunarFile    *target_file,
                                               GError        *error,
                                               gpointer       user_data);

typedef void (*ThunarBrowserPokeDeviceFunc)   (ThunarBrowser *browser,
                                               ThunarDevice  *volume,
                                               ThunarFile    *mount_point,
                                               GError        *error,
                                               gpointer       user_data,
                                               gboolean       cancelled);

typedef void (*ThunarBrowserPokeLocationFunc) (ThunarBrowser *browser,
                                               GFile         *location,
                                               ThunarFile    *file,
                                               ThunarFile    *target_file,
                                               GError        *error,
                                               gpointer       user_data);

struct _ThunarBrowserIface
{
  GTypeInterface __parent__;

  /* signals */

  /* virtual methods */
};

GType thunar_browser_get_type      (void) G_GNUC_CONST;

void  thunar_browser_poke_file     (ThunarBrowser                 *browser,
                                    ThunarFile                    *file,
                                    gpointer                       widget,
                                    ThunarBrowserPokeFileFunc      func,
                                    gpointer                       user_data);
void  thunar_browser_poke_device   (ThunarBrowser                 *browser,
                                    ThunarDevice                  *device,
                                    gpointer                       widget,
                                    ThunarBrowserPokeDeviceFunc    func,
                                    gpointer                       user_data);
void  thunar_browser_poke_location (ThunarBrowser                 *browser,
                                    GFile                         *location,
                                    gpointer                       widget,
                                    ThunarBrowserPokeLocationFunc  func,
                                    gpointer                       user_data);

G_END_DECLS

#endif /* !__THUNAR_BROWSER_H__ */
