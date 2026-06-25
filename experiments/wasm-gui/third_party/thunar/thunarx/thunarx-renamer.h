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

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_RENAMER_H__
#define __THUNARX_RENAMER_H__

#include <gtk/gtk.h>

#include <thunarx/thunarx-file-info.h>

G_BEGIN_DECLS

typedef struct _ThunarxRenamerPrivate ThunarxRenamerPrivate;
typedef struct _ThunarxRenamerClass   ThunarxRenamerClass;
typedef struct _ThunarxRenamer        ThunarxRenamer;

#define THUNARX_TYPE_RENAMER            (thunarx_renamer_get_type ())
#define THUNARX_RENAMER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_RENAMER, ThunarxRenamer))
#define THUNARX_RENAMER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_RENAMER, ThunarxRenamerClass))
#define THUNARX_IS_RENAMER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_RENAMER))
#define THUNARX_IS_RENAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNARX_TYPE_RENAMER))
#define THUNARX_RENAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNARX_TYPE_RENAMER, ThunarxRenamerClass))

/**
 * ThunarxRenamerClass:
 * @process:        see thunarx_renamer_process().
 * @load:           see thunarx_renamer_load().
 * @save:           see thunarx_renamer_save().
 * @get_menu_items: see thunarx_renamer_get_menu_items().
 * @changed:        see thunarx_renamer_changed().
 *
 * Abstract base class with virtual methods implemented by extensions
 * that provide additional bulk renamers for the integrated bulk
 * rename module in Thunar.
 *
 */

struct _ThunarxRenamerClass
{
  /*< private >*/
  GtkVBoxClass __parent__;

  /*< public >*/

  /* virtual methods */
  gchar *(*process)        (ThunarxRenamer  *renamer,
                            ThunarxFileInfo *file,
                            const gchar     *text,
                            guint            index);

  void   (*load)           (ThunarxRenamer  *renamer,
                            GHashTable      *settings);
  void   (*save)           (ThunarxRenamer  *renamer,
                            GHashTable      *settings);

  GList *(*get_menu_items) (ThunarxRenamer  *renamer,
                            GtkWindow       *window,
                            GList           *files);

  /*< private >*/
  void (*reserved0) (void);
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);

  /*< public >*/

  /* signals */
  void (*changed) (ThunarxRenamer *renamer);

  /*< private >*/
  void (*reserved6) (void);
  void (*reserved7) (void);
  void (*reserved8) (void);
  void (*reserved9) (void);
};

struct _ThunarxRenamer
{
  /*< private >*/
  GtkVBox                __parent__;
  ThunarxRenamerPrivate *priv;
};

GType        thunarx_renamer_get_type       (void) G_GNUC_CONST;

const gchar *thunarx_renamer_get_help_url   (ThunarxRenamer   *renamer);
void         thunarx_renamer_set_help_url   (ThunarxRenamer   *renamer,
                                             const gchar      *help_url);

const gchar *thunarx_renamer_get_name       (ThunarxRenamer   *renamer);
void         thunarx_renamer_set_name       (ThunarxRenamer   *renamer,
                                             const gchar      *name);

gchar       *thunarx_renamer_process        (ThunarxRenamer   *renamer,
                                             ThunarxFileInfo  *file,
                                             const gchar      *text,
                                             guint             index) G_GNUC_MALLOC;

void         thunarx_renamer_load           (ThunarxRenamer   *renamer,
                                             GHashTable       *settings);
void         thunarx_renamer_save           (ThunarxRenamer   *renamer,
                                             GHashTable       *settings);

GList       *thunarx_renamer_get_menu_items (ThunarxRenamer   *renamer,
                                             GtkWindow        *window,
                                             GList            *files) G_GNUC_MALLOC;

void         thunarx_renamer_changed        (ThunarxRenamer   *renamer);

G_END_DECLS

#endif /* !__THUNARX_RENAMER_H__ */
