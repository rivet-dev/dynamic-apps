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

#ifndef __THUNAR_SENDTO_MODEL_H__
#define __THUNAR_SENDTO_MODEL_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarSendtoModelClass ThunarSendtoModelClass;
typedef struct _ThunarSendtoModel      ThunarSendtoModel;

#define THUNAR_TYPE_SENDTO_MODEL            (thunar_sendto_model_get_type ())
#define THUNAR_SENDTO_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SENDTO_MODEL, ThunarSendtoModel))
#define THUNAR_SENDTO_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SENDTO_MODEL, ThunarSendtoModelClass))
#define THUNAR_IS_SENDTO_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SENDTO_MODEL))
#define THUNAR_IS_SENDTO_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SENDTO_MODEL))
#define THUNAR_SENDTO_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_SENDTO_MODEL))

GType              thunar_sendto_model_get_type     (void) G_GNUC_CONST;

ThunarSendtoModel *thunar_sendto_model_get_default  (void) G_GNUC_WARN_UNUSED_RESULT;

GList             *thunar_sendto_model_get_matching (ThunarSendtoModel *sendto_model,
                                                     GList             *files) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_SENDTO_MODEL_H__ */
