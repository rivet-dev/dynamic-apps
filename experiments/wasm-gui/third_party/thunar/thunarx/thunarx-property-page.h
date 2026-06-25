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

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_PROPERTY_PAGE_H__
#define __THUNARX_PROPERTY_PAGE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ThunarxPropertyPagePrivate ThunarxPropertyPagePrivate;
typedef struct _ThunarxPropertyPageClass   ThunarxPropertyPageClass;
typedef struct _ThunarxPropertyPage        ThunarxPropertyPage;

#define THUNARX_TYPE_PROPERTY_PAGE            (thunarx_property_page_get_type ())
#define THUNARX_PROPERTY_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_PROPERTY_PAGE, ThunarxPropertyPage))
#define THUNARX_PROPERTY_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_PROPERTY_PAGE, ThunarxPropertyPageClass))
#define THUNARX_IS_PROPERTY_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_PROPERTY_PAGE))
#define THUNARX_IS_PROPERTY_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNARX_TYPE_PROPERTY_PAGE))
#define THUNARX_PROPERTY_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNARX_TYPE_PROPERTY_PAGE))

struct _ThunarxPropertyPageClass
{
  GtkBinClass __parent__;

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
};

struct _ThunarxPropertyPage
{
  GtkBin __parent__;

  /*< private >*/
  ThunarxPropertyPagePrivate *priv;
};

GType        thunarx_property_page_get_type              (void) G_GNUC_CONST;

GtkWidget   *thunarx_property_page_new                   (const gchar         *label) G_GNUC_MALLOC;
GtkWidget   *thunarx_property_page_new_with_label_widget (GtkWidget           *label_widget) G_GNUC_MALLOC;

const gchar *thunarx_property_page_get_label             (ThunarxPropertyPage *property_page);
void         thunarx_property_page_set_label             (ThunarxPropertyPage *property_page,
                                                          const gchar         *label);

GtkWidget   *thunarx_property_page_get_label_widget      (ThunarxPropertyPage *property_page);
void         thunarx_property_page_set_label_widget      (ThunarxPropertyPage *property_page,
                                                          GtkWidget           *label_widget);

G_END_DECLS

#endif /* !__THUNARX_PROPERTY_PAGE_H__ */
