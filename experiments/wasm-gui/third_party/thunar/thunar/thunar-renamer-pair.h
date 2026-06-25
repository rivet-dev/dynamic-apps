/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_RENAMER_PAIR_H__
#define __THUNAR_RENAMER_PAIR_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarRenamerPair ThunarRenamerPair;

#define THUNAR_TYPE_RENAMER_PAIR (thunar_renamer_pair_get_type ())

struct _ThunarRenamerPair
{
  ThunarFile *file;
  gchar      *name;
};

GType              thunar_renamer_pair_get_type           (void)                                 G_GNUC_CONST;

ThunarRenamerPair *thunar_renamer_pair_new                (ThunarFile        *file,
                                                           const gchar       *name)              G_GNUC_MALLOC;
ThunarRenamerPair *thunar_renamer_pair_copy               (ThunarRenamerPair *renamer_pair)      G_GNUC_MALLOC;

void               thunar_renamer_pair_free               (gpointer           data);

GList             *thunar_renamer_pair_list_copy          (GList             *renamer_pair_list) G_GNUC_MALLOC;
void               thunar_renamer_pair_list_free          (GList             *renamer_pair_list);
int                thunar_renamer_pair_compare_ascending  (const void        *pair_a,
                                                           const void        *pair_b);
int                thunar_renamer_pair_compare_descending (const void        *pair_a,
                                                           const void        *pair_b);

G_END_DECLS;

#endif /* !__THUNAR_RENAMER_PAIR_H__ */
