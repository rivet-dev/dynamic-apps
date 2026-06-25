/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_SBR_ENUM_TYPES_H__
#define __THUNAR_SBR_ENUM_TYPES_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

#define THUNAR_SBR_TYPE_CASE_RENAMER_MODE (thunar_sbr_case_renamer_mode_get_type ())

/**
 * ThunarSbrCaseRenamerMode:
 * @THUNAR_SBR_CASE_RENAMER_MODE_LOWER       : convert to lower case.
 * @THUNAR_SBR_CASE_RENAMER_MODE_UPPER       : convert to upper case.
 * @THUNAR_SBR_CASE_RENAMER_MODE_TITLE       : convert to title case.
 * @THUNAR_SBR_CASE_RENAMER_MODE_FIRST_UPPER : convert to First character uppercase.
 *
 * The operation mode for the #ThunarSbrCaseRenamer.
 **/
typedef enum
{
  THUNAR_SBR_CASE_RENAMER_MODE_LOWER,
  THUNAR_SBR_CASE_RENAMER_MODE_UPPER,
  THUNAR_SBR_CASE_RENAMER_MODE_TITLE,
  THUNAR_SBR_CASE_RENAMER_MODE_FIRST_UPPER,
} ThunarSbrCaseRenamerMode;

GType thunar_sbr_case_renamer_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_SBR_TYPE_INSERT_MODE (thunar_sbr_insert_mode_get_type ())

/**
 * ThunarSbrInsertMode:
 * @THUNAR_SBR_INSERT_MODE_INSERT    : insert characters.
 * @THUNAR_SBR_INSERT_MODE_OVERWRITE : overwrite existing characters.
 *
 * The operation mode for the #ThunarSbrInsertRenamer.
 **/
typedef enum
{
  THUNAR_SBR_INSERT_MODE_INSERT,
  THUNAR_SBR_INSERT_MODE_OVERWRITE,
} ThunarSbrInsertMode;

GType thunar_sbr_insert_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_SBR_TYPE_NUMBER_MODE (thunar_sbr_number_mode_get_type ())

/**
 * ThunarSbrNumberMode:
 * @THUNAR_SBR_NUMBER_MODE_123          : 1, 2, 3, ...
 * @THUNAR_SBR_NUMBER_MODE_010203       : 01, 02, 03, ...
 * @THUNAR_SBR_NUMBER_MODE_001002003    : 001, 002, 003, ...
 * @THUNAR_SBR_NUMBER_MODE_000100020003 : 0001, 0002, 0003, ...
 * @THUNAR_SBR_NUMBER_MODE_000100020003 : 00001, 00002, 00003, ...
 * @THUNAR_SBR_NUMBER_MODE_ABC          : a, b, c, ...
 *
 * The numbering mode for the #ThunarSbrNumberRenamer.
 **/
typedef enum
{
  THUNAR_SBR_NUMBER_MODE_123,
  THUNAR_SBR_NUMBER_MODE_010203,
  THUNAR_SBR_NUMBER_MODE_001002003,
  THUNAR_SBR_NUMBER_MODE_000100020003,
  THUNAR_SBR_NUMBER_MODE_000010000200003,
  THUNAR_SBR_NUMBER_MODE_ABC,
} ThunarSbrNumberMode;

GType thunar_sbr_number_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_SBR_TYPE_OFFSET_MODE (thunar_sbr_offset_mode_get_type ())

/**
 * ThunarSbrOffsetMode:
 * @THUNAR_SBR_OFFSET_MODE_LEFT  : offset starting from the left.
 * @THUNAR_SBR_OFFSET_MODE_RIGHT : offset starting from the right.
 *
 * The offset mode for the #ThunarSbrInsertRenamer and the
 * #ThunarSbrRemoveRenamer.
 **/
typedef enum
{
  THUNAR_SBR_OFFSET_MODE_LEFT,
  THUNAR_SBR_OFFSET_MODE_RIGHT,
} ThunarSbrOffsetMode;

GType thunar_sbr_offset_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_SBR_TYPE_TEXT_MODE (thunar_sbr_text_mode_get_type ())

/**
 * ThunarSbrTextMode:
 * @THUNAR_SBR_TEXT_MODE_OTN : Old Name - Text - Number
 * @THUNAR_SBR_TEXT_MODE_NTO : Number - Text - Old Name
 * @THUNAR_SBR_TEXT_MODE_TN  : Text - Number
 * @THUNAR_SBR_TEXT_MODE_NT  : Number - Text
 *
 * The text mode for the #ThunarSbrNumberRenamer.
 **/
typedef enum
{
  THUNAR_SBR_TEXT_MODE_OTN,
  THUNAR_SBR_TEXT_MODE_NTO,
  THUNAR_SBR_TEXT_MODE_TN,
  THUNAR_SBR_TEXT_MODE_NT,
} ThunarSbrTextMode;

GType thunar_sbr_text_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_SBR_TYPE_DATE_MODE (thunar_sbr_date_mode_get_type ())

/**
 * ThunarSbrDateMode:
 * @THUNAR_SBR_DATE_MODE_NOW   : Current Time
 * @THUNAR_SBR_DATE_MODE_ATIME : Access Time
 * @THUNAR_SBR_DATE_MODE_MTIME : Modification Time
 * @THUNAR_SBR_DATE_MODE_TAKEN : Picture Taken Time
 *
 * The date mode for the #ThunarSbrDateRenamer.
 **/
typedef enum
{
  THUNAR_SBR_DATE_MODE_NOW,
  THUNAR_SBR_DATE_MODE_ATIME,
  THUNAR_SBR_DATE_MODE_MTIME,
#ifdef HAVE_EXIF
  THUNAR_SBR_DATE_MODE_TAKEN,
#endif
} ThunarSbrDateMode;

GType thunar_sbr_date_mode_get_type (void) G_GNUC_CONST;


void thunar_sbr_register_enum_types (ThunarxProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__THUNAR_SBR_ENUM_TYPES_H__ */
