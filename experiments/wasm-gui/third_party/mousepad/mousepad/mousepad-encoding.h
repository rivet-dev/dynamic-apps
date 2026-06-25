/*
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

#ifndef __MOUSEPAD_ENCODINGS_H__
#define __MOUSEPAD_ENCODINGS_H__

G_BEGIN_DECLS

/* button labels */
#define MOUSEPAD_ENCODING_LABEL_DEFAULT _("Default")
#define MOUSEPAD_ENCODING_LABEL_SYSTEM  _("System")
#define MOUSEPAD_ENCODING_LABEL_HISTORY _("History")

typedef enum
{
  MOUSEPAD_ENCODING_NONE,

  MOUSEPAD_ENCODING_ASCII,

  MOUSEPAD_ENCODING_ISO_8859_1,
  MOUSEPAD_ENCODING_ISO_8859_2,
  MOUSEPAD_ENCODING_ISO_8859_3,
  MOUSEPAD_ENCODING_ISO_8859_4,
  MOUSEPAD_ENCODING_ISO_8859_5,
  MOUSEPAD_ENCODING_ISO_8859_6,
  MOUSEPAD_ENCODING_ISO_8859_7,
  MOUSEPAD_ENCODING_ISO_8859_8,
  MOUSEPAD_ENCODING_ISO_8859_8_I,
  MOUSEPAD_ENCODING_ISO_8859_9,
  MOUSEPAD_ENCODING_ISO_8859_10,
  MOUSEPAD_ENCODING_ISO_8859_13,
  MOUSEPAD_ENCODING_ISO_8859_14,
  MOUSEPAD_ENCODING_ISO_8859_15,
  MOUSEPAD_ENCODING_ISO_8859_16,

  MOUSEPAD_ENCODING_UTF_7,
  MOUSEPAD_ENCODING_UTF_8,
  MOUSEPAD_ENCODING_UTF_16LE,
  MOUSEPAD_ENCODING_UTF_16BE,
  MOUSEPAD_ENCODING_UCS_2LE,
  MOUSEPAD_ENCODING_UCS_2BE,
  MOUSEPAD_ENCODING_UTF_32LE,
  MOUSEPAD_ENCODING_UTF_32BE,

  MOUSEPAD_ENCODING_ARMSCII_8,
  MOUSEPAD_ENCODING_BIG5,
  MOUSEPAD_ENCODING_BIG5_HKSCS,
  MOUSEPAD_ENCODING_CP_866,

  MOUSEPAD_ENCODING_EUC_JP,
  MOUSEPAD_ENCODING_EUC_KR,
  MOUSEPAD_ENCODING_EUC_TW,

  MOUSEPAD_ENCODING_GB18030,
  MOUSEPAD_ENCODING_GB2312,
  MOUSEPAD_ENCODING_GBK,
  MOUSEPAD_ENCODING_GEOSTD8,
  MOUSEPAD_ENCODING_HZ,

  MOUSEPAD_ENCODING_IBM_850,
  MOUSEPAD_ENCODING_IBM_852,
  MOUSEPAD_ENCODING_IBM_855,
  MOUSEPAD_ENCODING_IBM_857,
  MOUSEPAD_ENCODING_IBM_862,
  MOUSEPAD_ENCODING_IBM_864,

  MOUSEPAD_ENCODING_ISO_2022_JP,
  MOUSEPAD_ENCODING_ISO_2022_KR,
  MOUSEPAD_ENCODING_ISO_IR_111,
  MOUSEPAD_ENCODING_JOHAB,
  MOUSEPAD_ENCODING_KOI8_R,
  MOUSEPAD_ENCODING_KOI8_U,

  MOUSEPAD_ENCODING_SHIFT_JIS,
  MOUSEPAD_ENCODING_TCVN,
  MOUSEPAD_ENCODING_TIS_620,
  MOUSEPAD_ENCODING_UHC,
  MOUSEPAD_ENCODING_VISCII,

  MOUSEPAD_ENCODING_WINDOWS_1250,
  MOUSEPAD_ENCODING_WINDOWS_1251,
  MOUSEPAD_ENCODING_WINDOWS_1252,
  MOUSEPAD_ENCODING_WINDOWS_1253,
  MOUSEPAD_ENCODING_WINDOWS_1254,
  MOUSEPAD_ENCODING_WINDOWS_1255,
  MOUSEPAD_ENCODING_WINDOWS_1256,
  MOUSEPAD_ENCODING_WINDOWS_1257,
  MOUSEPAD_ENCODING_WINDOWS_1258,

  MOUSEPAD_N_ENCODINGS
}
MousepadEncoding;



const gchar      *mousepad_encoding_get_charset (MousepadEncoding   encoding);

const gchar      *mousepad_encoding_get_name    (MousepadEncoding   encoding);

MousepadEncoding  mousepad_encoding_find        (const gchar       *charset);

MousepadEncoding  mousepad_encoding_get_default (void);

MousepadEncoding  mousepad_encoding_get_system  (void);

MousepadEncoding  mousepad_encoding_read_bom    (const gchar       *contents,
                                                 gsize              length,
                                                 gsize             *bom_length);

void              mousepad_encoding_write_bom   (MousepadEncoding  *encoding,
                                                 gsize             *length,
                                                 gchar            **contents);

G_END_DECLS

#endif /* !__MOUSEPAD_ENCODINGS_H__ */
