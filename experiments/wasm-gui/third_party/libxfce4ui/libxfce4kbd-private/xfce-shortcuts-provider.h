/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __XFCE_SHORTCUTS_PROVIDER_H__
#define __XFCE_SHORTCUTS_PROVIDER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _XfceShortcut                 XfceShortcut;

typedef struct _XfceShortcutsProviderPrivate XfceShortcutsProviderPrivate;
typedef struct _XfceShortcutsProviderClass   XfceShortcutsProviderClass;
typedef struct _XfceShortcutsProvider        XfceShortcutsProvider;

#define XFCE_TYPE_SHORTCUTS_PROVIDER            (xfce_shortcuts_provider_get_type ())
#define XFCE_SHORTCUTS_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SHORTCUTS_PROVIDER, XfceShortcutsProvider))
#define XFCE_SHORTCUTS_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SHORTCUTS_PROVIDER, XfceShortcutsProviderClass))
#define XFCE_IS_SHORTCUTS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SHORTCUTS_PROVIDER))
#define XFCE_IS_SHORTCUTS_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SHORTCUTS_PROVIDER)
#define XFCE_SHORTCUTS_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SHORTCUTS_PROVIDER, XfceShortcutsProviderClass))

GType xfce_shortcuts_provider_get_type (void) G_GNUC_CONST;

XfceShortcutsProvider  *xfce_shortcuts_provider_new               (const gchar           *name) G_GNUC_MALLOC;
GList                  *xfce_shortcuts_provider_get_providers     (void) G_GNUC_MALLOC;
void                    xfce_shortcuts_provider_free_providers    (GList                 *providers);
const gchar            *xfce_shortcuts_provider_get_name          (XfceShortcutsProvider *provider);
gboolean                xfce_shortcuts_provider_is_custom         (XfceShortcutsProvider *provider);
void                    xfce_shortcuts_provider_reset_to_defaults (XfceShortcutsProvider *provider);
void                    xfce_shortcuts_provider_clone_defaults    (XfceShortcutsProvider *provider);
GList                  *xfce_shortcuts_provider_get_shortcuts     (XfceShortcutsProvider *provider);
XfceShortcut           *xfce_shortcuts_provider_get_shortcut      (XfceShortcutsProvider *provider,
                                                                   const gchar           *shortcut);
gboolean                xfce_shortcuts_provider_has_shortcut      (XfceShortcutsProvider *provider,
                                                                   const gchar           *shortcut);
void                    xfce_shortcuts_provider_set_shortcut      (XfceShortcutsProvider *provider,
                                                                   const gchar           *shortcut,
                                                                   const gchar           *command,
                                                                   gboolean               snotify);
void                    xfce_shortcuts_provider_reset_shortcut    (XfceShortcutsProvider *provider,
                                                                   const gchar           *shortcut);

void                    xfce_shortcuts_free                       (GList                 *shortcuts);
void                    xfce_shortcut_free                        (XfceShortcut          *shortcut);



struct _XfceShortcutsProviderClass
{
  GObjectClass __parent__;
};

struct _XfceShortcutsProvider
{
  GObject __parent__;

  XfceShortcutsProviderPrivate *priv;
};

struct _XfceShortcut
{
  gchar *property_name;
  gchar *shortcut;
  gchar *command;
  guint  snotify : 1;
};

G_END_DECLS

#endif /* !__XFCE_SHORTCUTS_PROVIDER_H__ */
