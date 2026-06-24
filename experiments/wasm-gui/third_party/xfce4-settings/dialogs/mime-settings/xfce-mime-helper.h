/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_MIME_HELPER_H__
#define __XFCE_MIME_HELPER_H__

#include <exo/exo.h>

G_BEGIN_DECLS

typedef enum /*< enum,prefix=XFCE_MIME_HELPER >*/
{
  XFCE_MIME_HELPER_WEBBROWSER,        /*< nick=WebBrowser >*/
  XFCE_MIME_HELPER_MAILREADER,        /*< nick=MailReader >*/
  XFCE_MIME_HELPER_FILEMANAGER,       /*< nick=FileManager >*/
  XFCE_MIME_HELPER_TERMINALEMULATOR,  /*< nick=TerminalEmulator >*/
  XFCE_MIME_HELPER_N_CATEGORIES,      /*< skip >*/
} XfceMimeHelperCategory;

typedef struct _XfceMimeHelperClass XfceMimeHelperClass;
typedef struct _XfceMimeHelper      XfceMimeHelper;

#define XFCE_MIME_TYPE_HELPER            (xfce_mime_helper_get_type ())
#define XFCE_MIME_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_MIME_TYPE_HELPER, XfceMimeHelper))
#define XFCE_MIME_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_MIME_TYPE_HELPER, XfceMimeHelperClass))
#define XFCE_MIME_IS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_MIME_TYPE_HELPER))
#define XFCE_MIME_IS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_MIME_TYPE_HELPER))
#define XFCE_MIME_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_MIME_TYPE_HELPER, XfceMimeHelperClass))

GType              xfce_mime_helper_get_type      (void) G_GNUC_CONST;
XfceMimeHelperCategory  xfce_mime_helper_get_category  (const XfceMimeHelper   *helper);
const gchar       *xfce_mime_helper_get_id        (const XfceMimeHelper   *helper);
const gchar       *xfce_mime_helper_get_name      (const XfceMimeHelper   *helper);
const gchar       *xfce_mime_helper_get_icon      (const XfceMimeHelper   *helper);
const gchar       *xfce_mime_helper_get_command   (const XfceMimeHelper   *helper);
gboolean           xfce_mime_helper_execute       (XfceMimeHelper         *helper,
                                             GdkScreen         *screen,
                                             const gchar       *parameter,
                                             GError           **error);


#define XFCE_MIME_TYPE_HELPER_DATABASE             (xfce_mime_helper_database_get_type ())
#define XFCE_MIME_HELPER_DATABASE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_MIME_TYPE_HELPER_DATABASE, XfceMimeHelperDatabase))
#define XFCE_MIME_HELPER_DATABASE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_MIME_TYPE_HELPER_DATABASE, XfceMimeHelperDatabaseClass))
#define XFCE_MIME_IS_HELPER_DATABASE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_MIME_TYPE_HELPER_DATABASE))
#define XFCE_MIME_IS_HELPER_DATABASE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_MIME_TYPE_HELPER_DATABASE))
#define XFCE_MIME_HELPER_DATABASE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_MIME_TYPE_HELPER_DATABASE, XfceMimeHelperDatabaseClass))

typedef struct _XfceMimeHelperDatabaseClass XfceMimeHelperDatabaseClass;
typedef struct _XfceMimeHelperDatabase      XfceMimeHelperDatabase;

GType               xfce_mime_helper_database_get_type        (void) G_GNUC_CONST;
XfceMimeHelperDatabase  *xfce_mime_helper_database_get             (void);
XfceMimeHelper          *xfce_mime_helper_database_get_default     (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category);
gboolean            xfce_mime_helper_database_set_default     (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category,
                                                         XfceMimeHelper         *helper,
                                                         GError           **error);
gboolean            xfce_mime_helper_database_clear_default   (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category,
                                                         GError           **error);
GList              *xfce_mime_helper_database_get_all         (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category);
XfceMimeHelper          *xfce_mime_helper_database_get_custom      (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category);
void                xfce_mime_helper_database_set_custom      (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category,
                                                         const gchar       *command);
gboolean            xfce_mime_helper_database_get_dismissed   (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category);
gboolean            xfce_mime_helper_database_set_dismissed   (XfceMimeHelperDatabase *database,
                                                         XfceMimeHelperCategory  category,
                                                         gboolean           dismissed);

G_END_DECLS

#endif /* !__XFCE_MIME_HELPER_H__ */
