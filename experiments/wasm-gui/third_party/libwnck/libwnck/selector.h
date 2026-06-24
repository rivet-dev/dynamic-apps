/* selector */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_SELECTOR_H
#define WNCK_SELECTOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define WNCK_TYPE_SELECTOR              (wnck_selector_get_type ())
#define WNCK_SELECTOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_SELECTOR, WnckSelector))
#define WNCK_SELECTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_SELECTOR, WnckSelectorClass))
#define WNCK_IS_SELECTOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_SELECTOR))
#define WNCK_IS_SELECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_SELECTOR))
#define WNCK_SELECTOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_SELECTOR, WnckSelectorClass))
typedef struct _WnckSelector WnckSelector;
typedef struct _WnckSelectorClass WnckSelectorClass;
typedef struct _WnckSelectorPrivate WnckSelectorPrivate;

/**
 * WnckSelector:
 *
 * The #WnckSelector struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckSelector
{
  GtkMenuBar parent_instance;
  WnckSelectorPrivate *priv;
};

struct _WnckSelectorClass 
{
  GtkMenuBarClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GtkWidget *wnck_selector_new      (void);
GType      wnck_selector_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* WNCK_SELECTOR_H */
